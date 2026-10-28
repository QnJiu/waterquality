"""Read the teaching controller's status over Modbus RTU (MicroPython)."""
import struct


def crc16(data):
    crc = 0xffff
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0xa001 if crc & 1 else 0)
    return crc


def status_request(address):
    if not 1 <= address <= 247:
        raise ValueError("invalid Modbus address")
    payload = struct.pack(">BBHH", address, 3, 10, 7)
    return payload + struct.pack("<H", crc16(payload))


def parse_status(frame, address):
    if len(frame) < 5 or crc16(frame[:-2]) != struct.unpack("<H", frame[-2:])[0]:
        raise ValueError("invalid Modbus frame or CRC")
    if frame[0] != address:
        raise ValueError("wrong Modbus address")
    if frame[1] == 0x83 and len(frame) == 5:
        raise ValueError("Modbus exception %d" % frame[2])
    if len(frame) != 19 or frame[1] != 3 or frame[2] != 14:
        raise ValueError("unexpected Modbus response")
    state, fault, step, start_result, concentration, valid = struct.unpack(">HHHHfH", frame[3:-2])
    return {"state": state, "fault": fault, "step": step,
            "start_result": start_result, "result_valid": bool(valid),
            "concentration_mg_l": concentration if valid else None}


def read_status(uart, direction, address, clock, timeout_ms=1000):
    """One request on a dedicated RS485 bus; direction high enables transmit."""
    if timeout_ms <= 0:
        raise ValueError("invalid timeout")
    # Discard any late reply before a new request. Only this client may use the bus.
    if uart.any():
        uart.read(256)
        if uart.any():
            raise OSError("UART input not idle")
    clock.sleep_ms(5)  # RTU inter-frame silence at 115200 baud
    direction.value(1)
    try:
        request = status_request(address)
        if uart.write(request) != len(request):
            raise OSError("short UART write")
        uart.flush()  # wait for the final stop bit before turning the bus around
    finally:
        direction.value(0)
    deadline = clock.ticks_add(clock.ticks_ms(), timeout_ms)
    reply = bytearray()
    while clock.ticks_diff(deadline, clock.ticks_ms()) > 0:
        remaining = (5 if len(reply) >= 2 and reply[1] == 0x83 else 19) - len(reply)
        if remaining <= 0:
            return parse_status(reply, address)
        part = uart.read(remaining)
        if part:
            reply.extend(part)
        else:
            clock.sleep_ms(1)
    raise OSError("Modbus response timeout")
