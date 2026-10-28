import importlib.util
from pathlib import Path
import struct
import unittest

spec = importlib.util.spec_from_file_location('tp_modbus', Path(__file__).resolve().parents[1] / 'tools' / 'tp_modbus.py')
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)

class Port:
    def __init__(self, reply): self.reply, self.sent, self.timeout = bytearray(reply), b'', 0
    def reset_input_buffer(self): pass
    def write(self, data): self.sent = data; return len(data)
    def flush(self): pass
    def read(self, n):
        data = bytes(self.reply[:n]); del self.reply[:n]; return data

class Tests(unittest.TestCase):
    def test_status_and_crc(self):
        p = Port(m.frame(bytes.fromhex('01030400010002')))
        self.assertEqual(m.Client(p, 1).read(10, 2), [1, 2])
        self.assertEqual(p.sent, bytes.fromhex('0103000a0002e409'))

    def test_schedule_and_command(self):
        p = Port(m.frame(bytes.fromhex('011000010005')))
        m.Client(p, 1).schedule(2, 3600, 60, -300)
        self.assertEqual(p.sent[1:-2], bytes.fromhex('10000100050a000200000e10003cfed4'))
        p = Port(m.frame(bytes.fromhex('010600000002')))
        m.Client(p, 1).command(2)
        self.assertEqual(p.sent, m.frame(bytes.fromhex('010600000002')))

    def test_config_round_trip_and_full_write(self):
        values = [1., -2., .5, .01, 10., 1., 0., 0., 0., 1., 10., 100.]
        raw = b''.join(struct.pack('>f', v) for v in values) + struct.pack('>3H', 0, 3600, 1)
        p = Port(m.frame(bytes([1, 3, len(raw)]) + raw))
        config = m.Client(p, 1).get_config()
        self.assertAlmostEqual(config['min_signal'], .01)
        self.assertEqual(config['period_seconds'], 3600)
        p = Port(m.frame(bytes.fromhex('01100014001b')))
        m.Client(p, 1).set_config(config)
        self.assertEqual(p.sent, m.frame(bytes.fromhex('01100014001b36') + raw))
        config['kp'] = float('nan')
        with self.assertRaises(ValueError): m.Client(p, 1).set_config(config)
        del config['kp']
        with self.assertRaises(ValueError): m.Client(p, 1).set_config(config)

    def test_log_query(self):
        words = [3, 0, 7, 2, 0, 1, 0, 0, 0, 42, 0x3f80, 0]
        response = m.frame(bytes.fromhex('011000320003')) + m.frame(bytes([1, 3, 24]) + struct.pack('>12H', *words))
        p = Port(response)
        event = m.Client(p, 1).log()
        self.assertEqual(event, {'sequence': 7, 'type': 2, 'detail': 0, 'utc': 42, 'value_bits': 0x3f800000})

    def test_exception_corruption_and_timeout(self):
        for reply in (m.frame(bytes.fromhex('018302')), bytes.fromhex('01030200000000'), b''):
            with self.assertRaises(m.ModbusError): m.Client(Port(reply), 1, .001).read(10, 1)
        with self.assertRaises(ValueError): m.Client(Port(b''), 1).schedule(2, 1, 1440, 0)

if __name__ == '__main__': unittest.main()
