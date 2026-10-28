"""MicroPython sender for the existing signed F407 upgrade protocol."""
import hashlib
import struct

FLAG, ESC = 0x7E, 0x7D
STATUS, BEGIN, RESUME, WRITE, FINISH = range(1, 6)
OK, BUSY, INVALID, IO_ERROR, TIMEOUT, LOCK_BUSY = range(6)
MAX_RAW = 520
MAX_IMAGE_SIZE = 384 * 1024  # F407 reference slot


class FinishUnknown(TimeoutError):
    """The final request was sent, but its result could not be observed."""


class ProtocolError(Exception):
    pass


def crc16(data):
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0xA001 if crc & 1 else 0)
    return crc


def encode(command, seq, payload=b""):
    if not 0 <= command <= 255 or not 0 <= seq <= 65535 or len(payload) > 512:
        raise ValueError("invalid command, sequence, or payload length")
    raw = struct.pack("<BBHH", 1, command, seq, len(payload)) + payload
    raw += struct.pack("<H", crc16(raw))
    result = bytearray([FLAG])
    for byte in raw:
        if byte in (FLAG, ESC):
            result.extend((ESC, byte ^ 0x20))
        else:
            result.append(byte)
    result.append(FLAG)
    return bytes(result)


class Decoder:
    def __init__(self):
        self.started = False
        self.raw = bytearray()
        self.escape = False
        self.discard = False

    def feed(self, byte):
        if byte == FLAG:
            raw = bytes(self.raw)
            good = self.started and raw and not self.escape and not self.discard
            self.started, self.escape, self.discard = True, False, False
            self.raw.clear()
            if not good or len(raw) < 8:
                return None
            version, cmd, seq, length = struct.unpack_from("<BBHH", raw)
            if version != 1 or length > 512 or len(raw) != length + 8:
                return None
            if crc16(raw[:-2]) != struct.unpack_from("<H", raw, len(raw)-2)[0]:
                return None
            return cmd, seq, raw[6:-2]
        if not self.started or self.discard:
            return None
        if self.escape:
            self.escape = False
            if byte not in (FLAG ^ 0x20, ESC ^ 0x20):
                self.discard = True
                return None
            byte ^= 0x20
        elif byte == ESC:
            self.escape = True
            return None
        if len(self.raw) >= MAX_RAW:
            self.discard = True
        else:
            self.raw.append(byte)
        return None


def parse_reply(data):
    if len(data) < 2 or data[0] > LOCK_BUSY or data[1] not in (0, 1):
        raise ProtocolError("invalid response header")
    if not data[1]:
        if len(data) != 2:
            raise ProtocolError("invalid response length")
        return data[0], None
    if len(data) != 48:
        raise ProtocolError("invalid state length")
    phase, bank, version, size, written = struct.unpack_from("<BBIII", data, 2)
    if phase not in (1, 2, 3) or bank > 1 or not version or not size or written > size:
        raise ProtocolError("invalid download state")
    return data[0], {"phase": phase, "bank": bank, "version": version,
                     "size": size, "written": written, "id": data[16:48]}


class Client:
    def __init__(self, uart, clock, timeout_ms=10000):
        if timeout_ms <= 0:
            raise ValueError("invalid timeout")
        self.uart, self.clock, self.timeout_ms = uart, clock, timeout_ms
        self.sequence, self.decoder = 0, Decoder()

    def command(self, cmd, payload=b""):
        for attempt in range(21):
            seq = self.sequence
            self.sequence = (seq + 1) & 0xffff
            request = encode(cmd, seq, payload)
            if self.uart.write(request) != len(request):
                raise OSError("short UART write")
            deadline = self.clock.ticks_add(self.clock.ticks_ms(), self.timeout_ms)
            while self.clock.ticks_diff(deadline, self.clock.ticks_ms()) > 0:
                data = self.uart.read(64)
                if not data:
                    self.clock.sleep_ms(1)
                    continue
                for byte in data:
                    reply = self.decoder.feed(byte)
                    if reply and reply[0] == cmd | 0x80 and reply[1] == seq:
                        result = parse_reply(reply[2])
                        if result != (LOCK_BUSY, None) or attempt == 20:
                            return result
                        break
                else:
                    continue
                break
            else:
                raise TimeoutError("upgrade response timeout")
            self.clock.sleep_ms(100)


def check_files(manifest, image):
    """Validate TPU1 and hash the file without loading the image into RAM."""
    if len(manifest) != 116:
        raise ValueError("invalid manifest length")
    magic, product, version, size, bank, digest = struct.unpack("<4sIIIB3x32s", manifest[:52])
    if (magic != b"TPU1" or not product or not version or not size or
            size > MAX_IMAGE_SIZE or bank not in (0, 1) or manifest[17:20] != bytes(3)):
        raise ValueError("invalid manifest fields")
    image.seek(0)
    hasher, length = hashlib.sha256(), 0
    while True:
        block = image.read(1024)
        if not block:
            break
        length += len(block)
        if length > size:
            raise ValueError("image longer than manifest")
        hasher.update(block)
    if length != size or hasher.digest() != digest:
        raise ValueError("image does not match manifest")
    return hashlib.sha256(manifest[:52]).digest(), size


def upload(client, manifest, image, retries=3):
    manifest_id, size = check_files(manifest, image)

    def checked(state):
        if state is None or state["id"] != manifest_id or state["size"] != size:
            raise ProtocolError("download state does not match manifest")
        return state

    def status():
        result, state = client.command(STATUS)
        if result != OK:
            raise ProtocolError("status failed: %d" % result)
        return checked(state)

    # BEGIN is safe to repeat for the same signed manifest; this also
    # handles a request lost before the device opened a session.
    for attempt in range(retries + 1):
        try:
            result, state = client.command(BEGIN, manifest)
            break
        except TimeoutError:
            if attempt == retries:
                raise
    if result not in (OK, BUSY):
        raise ProtocolError("begin failed: %d" % result)
    state = checked(state) if state else status()
    if state["phase"] == 1:
        result, state = client.command(RESUME)
        if result != OK:
            raise ProtocolError("resume failed: %d" % result)
        state = checked(state)
    while state["phase"] == 2 and state["written"] < size:
        offset = state["written"]
        image.seek(offset)
        block = image.read(min(256, size - offset))
        if not block:
            raise ProtocolError("image unexpectedly truncated")
        for attempt in range(retries + 1):
            try:
                result, new_state = client.command(WRITE, struct.pack("<I", offset) + block)
                if result != OK:
                    raise ProtocolError("write failed: %d" % result)
                state = checked(new_state)
            except TimeoutError:
                state = status()
            if state["written"] == offset + len(block) and state["phase"] == 2:
                break
            if state["written"] != offset or state["phase"] != 2:
                raise ProtocolError("unexpected download progress")
            if attempt == retries:
                raise ProtocolError("write retry limit reached")
    if state["written"] != size or state["phase"] not in (2, 3):
        raise ProtocolError("download not ready to finish")
    finish_uncertain = False
    for attempt in range(retries + 1):
        try:
            result, state = client.command(FINISH)
        except TimeoutError:
            finish_uncertain = True
            if attempt == retries:
                raise FinishUnknown("finish response lost; check the confirmed device version")
            continue
        except (OSError, ProtocolError):
            # A transport error or malformed reply can occur after FINISH was
            # accepted, so neither proves that staging failed.
            raise FinishUnknown("finish communication failed; check the confirmed device version")
        if result != OK:
            if finish_uncertain:
                raise FinishUnknown("earlier finish response lost; check the confirmed device version")
            raise ProtocolError("finish failed: %d" % result)
        try:
            state = checked(state)
            if state["phase"] != 3 or state["written"] != size:
                raise ProtocolError("unexpected final state")
        except ProtocolError:
            raise FinishUnknown("finish state unavailable; check the confirmed device version")
        return state


class BundleImage:
    """Expose the image following a 116-byte signed manifest as a file."""
    def __init__(self, source):
        self.source = source
    def seek(self, offset):
        if offset < 0:
            raise ValueError("invalid image offset")
        return self.source.seek(116 + offset)
    def read(self, size):
        return self.source.read(size)


def upload_bundle(client, source):
    source.seek(0)
    manifest = source.read(116)
    return upload(client, manifest, BundleImage(source))
