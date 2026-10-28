#!/usr/bin/env python3
"""Sequential client for the TP upgrade wire protocol (Python 3)."""
import argparse
import pathlib
import socket
import struct
import sys
import time

FLAG, ESC = 0x7E, 0x7D
STATUS, BEGIN, RESUME, WRITE, FINISH = range(1, 6)
OK, BUSY, INVALID, IO_ERROR, TIMEOUT = range(5)
MAX_RAW = 520


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
    if len(data) < 2 or data[0] > TIMEOUT or data[1] not in (0, 1):
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
    def __init__(self, transport, timeout):
        self.transport, self.timeout = transport, timeout
        self.sequence = 0
        self.decoder = Decoder()

    def command(self, cmd, payload=b""):
        seq = self.sequence
        self.sequence = (seq + 1) & 0xFFFF
        self.transport.write(encode(cmd, seq, payload))
        deadline = time.monotonic() + self.timeout
        while time.monotonic() < deadline:
            chunk = self.transport.read(deadline - time.monotonic())
            for byte in chunk:
                frame = self.decoder.feed(byte)
                if frame and frame[0] == (cmd | 0x80) and frame[1] == seq:
                    return parse_reply(frame[2])
        raise TimeoutError("response timeout")


def match_state(state, manifest_id, image):
    if state is None or state["id"] != manifest_id or state["size"] != len(image):
        raise ProtocolError("device download state does not match the supplied manifest ID and image size")
    return state


def status(client, manifest_id, image):
    result, state = client.command(STATUS)
    if result != OK:
        raise ProtocolError("status failed: %d" % result)
    return match_state(state, manifest_id, image)


def upload(client, manifest, manifest_id, image, retries=3):
    if not manifest or len(manifest) > 512 or len(manifest_id) != 32 or not image:
        raise ProtocolError("invalid manifest, ID, or empty image")
    try:
        result, state = client.command(BEGIN, manifest)
    except TimeoutError:
        result, state = BUSY, None
    if result not in (OK, BUSY):
        raise ProtocolError("begin failed: %d" % result)
    state = match_state(state, manifest_id, image) if state else status(client, manifest_id, image)
    if state["phase"] == 1:
        result, state = client.command(RESUME)
        if result != OK:
            raise ProtocolError("resume failed: %d" % result)
        state = match_state(state, manifest_id, image)
    while state["phase"] == 2 and state["written"] < len(image):
        offset = state["written"]
        block = image[offset:offset+256]
        for attempt in range(retries + 1):
            try:
                result, new_state = client.command(WRITE, struct.pack("<I", offset) + block)
                if result != OK:
                    raise ProtocolError("write failed: %d" % result)
                state = match_state(new_state, manifest_id, image)
            except TimeoutError:
                state = status(client, manifest_id, image)
            if state["written"] == offset + len(block) and state["phase"] == 2:
                break
            if state["written"] != offset or state["phase"] != 2:
                raise ProtocolError("unexpected download progress")
            if attempt == retries:
                raise ProtocolError("write retry limit reached")
    if state["written"] != len(image) or state["phase"] not in (2, 3):
        raise ProtocolError("download not ready to finish")
    for attempt in range(retries + 1):
        try:
            result, state = client.command(FINISH)
        except TimeoutError:
            if attempt == retries:
                raise
            continue
        if result != OK:
            raise ProtocolError("finish failed: %d" % result)
        state = match_state(state, manifest_id, image)
        if state["phase"] != 3 or state["written"] != len(image):
            raise ProtocolError("unexpected final state")
        return state


class TcpTransport:
    def __init__(self, host, port, timeout):
        self.socket = socket.create_connection((host, port), timeout)

    def write(self, data):
        self.socket.sendall(data)

    def read(self, timeout):
        self.socket.settimeout(max(timeout, 0.001))
        try:
            data = self.socket.recv(1024)
        except socket.timeout:
            return b""
        if not data:
            raise ConnectionError("connection closed")
        return data

    def close(self):
        self.socket.close()


class SerialTransport:
    def __init__(self, port, baud, timeout):
        import serial
        self.port = serial.Serial(port, baudrate=baud, timeout=timeout, write_timeout=timeout)

    def write(self, data):
        if self.port.write(data) != len(data):
            raise IOError("incomplete serial write")

    def read(self, timeout):
        self.port.timeout = max(timeout, 0.001)
        return self.port.read(1)

    def close(self):
        self.port.close()


def main():
    parser = argparse.ArgumentParser(description="Send an authenticated manifest and image using TP upgrade wire protocol")
    target = parser.add_mutually_exclusive_group(required=True)
    target.add_argument("--tcp", metavar="HOST:PORT")
    target.add_argument("--serial", metavar="PORT")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=10)
    parser.add_argument("--manifest", required=True, type=pathlib.Path)
    parser.add_argument("--manifest-id", required=True, help="32-byte authenticated manifest ID in hex")
    parser.add_argument("--image", required=True, type=pathlib.Path)
    args = parser.parse_args()
    transport = None
    try:
        if args.timeout <= 0 or args.baud <= 0:
            raise ValueError("timeout and baud must be positive")
        manifest_id = bytes.fromhex(args.manifest_id)
        manifest, image = args.manifest.read_bytes(), args.image.read_bytes()
        if len(manifest_id) != 32 or not manifest or len(manifest) > 512 or not image:
            raise ValueError("manifest ID must be 32 bytes, manifest 1..512 bytes, and image nonempty")
        if args.tcp:
            host, port = args.tcp.rsplit(":", 1)
            transport = TcpTransport(host, int(port), args.timeout)
        else:
            transport = SerialTransport(args.serial, args.baud, args.timeout)
        state = upload(Client(transport, args.timeout), manifest, manifest_id, image)
        print("Staged bank %d, version %d, %d bytes" % (state["bank"], state["version"], state["written"]))
        return 0
    except (ValueError, OSError, ImportError, ProtocolError, TimeoutError) as exc:
        print("Upgrade failed: %s" % exc, file=sys.stderr)
        return 1
    finally:
        if transport:
            transport.close()


if __name__ == "__main__":
    sys.exit(main())
