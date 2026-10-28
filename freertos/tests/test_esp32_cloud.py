import importlib.util
import pathlib
import struct
import unittest

path = pathlib.Path(__file__).resolve().parents[1] / 'examples' / 'esp32_cloud' / 'modbus_status.py'
spec = importlib.util.spec_from_file_location('modbus_status', path)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


def response(payload):
    return payload + struct.pack('<H', mod.crc16(payload))


class Clock:
    now = 0
    def ticks_ms(self): return self.now
    def ticks_add(self, a, b): return a + b
    def ticks_diff(self, a, b): return a - b
    def sleep_ms(self, ms): self.now += ms


class Direction:
    def __init__(self): self.levels = []
    def value(self, v): self.levels.append(v)


class UART:
    def __init__(self, reply):
        self.reply, self.started, self.written = reply, False, b''
    def any(self): return 0
    def write(self, data): self.written = data; return len(data)
    def flush(self): self.started = True
    def read(self, n):
        if not self.started: return None
        part, self.reply = self.reply[:min(n, 3)], self.reply[min(n, 3):]
        return part


class StatusTest(unittest.TestCase):
    def test_read_status_and_direction(self):
        payload = struct.pack('>BBBHHHHfH', 1, 3, 14, 2, 0, 15, 0, 0.301, 1)
        uart, direction = UART(response(payload)), Direction()
        status = mod.read_status(uart, direction, 1, Clock())
        self.assertEqual(uart.written.hex(), '0103000a0007240a')
        self.assertEqual(direction.levels, [1, 0])
        self.assertEqual(status['state'], 2)
        self.assertTrue(status['result_valid'])
        self.assertAlmostEqual(status['concentration_mg_l'], 0.301, places=5)

    def test_invalid_result_is_not_reported(self):
        payload = struct.pack('>BBBHHHHfH', 1, 3, 14, 3, 2, 0, 0, 12.0, 0)
        self.assertIsNone(mod.parse_status(response(payload), 1)['concentration_mg_l'])

    def test_exception_crc_and_timeout(self):
        with self.assertRaisesRegex(ValueError, 'exception 2'):
            mod.parse_status(response(bytes([1, 0x83, 2])), 1)
        with self.assertRaisesRegex(ValueError, 'CRC'):
            mod.parse_status(b'\x01\x83\x02\x00\x00', 1)
        direction = Direction()
        with self.assertRaisesRegex(OSError, 'timeout'):
            mod.read_status(UART(b''), direction, 1, Clock(), 10)
        self.assertEqual(direction.levels, [1, 0])

    def test_short_write_releases_bus(self):
        uart, direction = UART(b''), Direction()
        uart.write = lambda data: 1
        with self.assertRaisesRegex(OSError, 'short UART'):
            mod.read_status(uart, direction, 1, Clock())
        self.assertEqual(direction.levels, [1, 0])


if __name__ == '__main__': unittest.main()
