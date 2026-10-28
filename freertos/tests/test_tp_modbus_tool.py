import importlib.util
import contextlib
import io
import sys
from unittest.mock import patch
from types import SimpleNamespace
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

    def test_maintenance_status_and_old_device(self):
        for value, expected in [(0, False), (1, True), (2, None)]:
            reply = m.frame(bytes([1, 3, 2, 0, value]))
            self.assertIs(m.Client(Port(reply), 1).maintenance_status(), expected)
        old_reply = m.frame(bytes([1, 0x83, 2]))
        self.assertIsNone(m.Client(Port(old_reply), 1).maintenance_status())
        other_error = m.frame(bytes([1, 0x83, 4]))
        with self.assertRaises(m.ModbusError):
            m.Client(Port(other_error), 1).maintenance_status()

    def test_schedule_and_command(self):
        p = Port(m.frame(bytes.fromhex('011000010005')))
        m.Client(p, 1).schedule(2, 3600, 60, -300)
        self.assertEqual(p.sent[1:-2], bytes.fromhex('10000100050a000200000e10003cfed4'))
        p = Port(m.frame(bytes.fromhex('010600000002')))
        m.Client(p, 1).command(2)
        self.assertEqual(p.sent, m.frame(bytes.fromhex('010600000002')))

    def test_clock_setting_and_status(self):
        p = Port(m.frame(bytes.fromhex('0110007b0002')))
        m.Client(p, 1).set_clock(1700000000)
        self.assertEqual(p.sent, m.frame(bytes.fromhex('0110007b0002046553f100')))
        for invalid in (True, 946684799, 4102444800):
            with self.assertRaises(ValueError): m.Client(p, 1).set_clock(invalid)
        replies = m.frame(bytes.fromhex('01030c00030000000000016553f100'))
        p = Port(replies)
        self.assertEqual(m.Client(p, 1).clock_status(), {'state': 'verified', 'utc_seconds': 1700000000})
        self.assertEqual(p.sent, m.frame(bytes.fromhex('0103007a0006')))
        replies = m.frame(bytes.fromhex('01030c000200000000000000000000'))
        self.assertIsNone(m.Client(Port(replies), 1).clock_status()['utc_seconds'])

    def test_auto_calibration_commands_and_status(self):
        for code in (6, 7, 8, 9, 10, 11):
            payload = bytes([1, 6, 0, 0, 0, code])
            p = Port(m.frame(payload))
            m.Client(p, 1).command(code)
            self.assertEqual(p.sent, m.frame(payload))
        p = Port(m.frame(bytes.fromhex('01030400040000')))
        self.assertEqual(m.Client(p, 1).auto_calibration_status(),
                         {'state': 'complete', 'state_code': 4, 'error_code': 0})
        self.assertEqual(p.sent, m.frame(bytes.fromhex('0103005a0002')))
        p = Port(m.frame(bytes.fromhex('01030400630004')))
        self.assertEqual(m.Client(p, 1).auto_calibration_status()['state'], 'unknown')
        with self.assertRaises(ValueError): m.Client(Port(b''), 1).command(12)

    def test_auto_standard_status(self):
        p = Port(m.frame(bytes.fromhex('01030a0003000000013f800000')))
        self.assertEqual(m.Client(p, 1).auto_standard_status(),
                         {'state': 'complete', 'error': 0, 'passed': True, 'actual_mg_l': 1.0})
        self.assertEqual(p.sent, m.frame(bytes.fromhex('010300690005')))
        p = Port(m.frame(bytes.fromhex('01030a00040002000000000000')))
        self.assertEqual(m.Client(p, 1).auto_standard_status()['actual_mg_l'], None)

    def test_temperature_cli(self):
        for response, expected in (('010306000141a00000', '20.0'),
                                   ('010306000000000000', 'null')):
            class SerialPort(Port):
                def __init__(self, *args, **kwargs):
                    super().__init__(m.frame(bytes.fromhex(response)))
                def __enter__(self): return self
                def __exit__(self, *args): pass
            text = io.StringIO()
            with patch.dict(sys.modules, {'serial': SimpleNamespace(Serial=SerialPort)}), contextlib.redirect_stdout(text):
                self.assertEqual(m.main(['--port', 'COM3', 'temperature']), 0)
            self.assertIn('"temperature_c": ' + expected, text.getvalue())

    def test_readings_cli(self):
        class SerialPort(Port):
            instance = None
            def __init__(self, *args, **kwargs):
                super().__init__(m.frame(bytes([1, 3, 14]) + struct.pack('>Hfff', 5, 1.25, 99.0, 3.5)))
                SerialPort.instance = self
            def __enter__(self): return self
            def __exit__(self, *args): pass
        text = io.StringIO()
        with patch.dict(sys.modules, {'serial': SimpleNamespace(Serial=SerialPort)}), contextlib.redirect_stdout(text):
            self.assertEqual(m.main(['--port', 'COM3', 'readings']), 0)
        self.assertEqual(__import__('json').loads(text.getvalue()),
                         {'blank': 1.25, 'reacted': None, 'turbidity': 3.5})
        self.assertEqual(SerialPort.instance.sent, m.frame(bytes.fromhex('010300840007')))

    def test_auto_calibration_cli(self):
        class SerialPort(Port):
            def __init__(self, *args, **kwargs):
                super().__init__(m.frame(bytes.fromhex('01030400030000')))
            def __enter__(self): return self
            def __exit__(self, *args): pass
        text = io.StringIO()
        with patch.dict(sys.modules, {'serial': SimpleNamespace(Serial=SerialPort)}), contextlib.redirect_stdout(text):
            self.assertEqual(m.main(['--port', 'COM3', 'auto-cal-status']), 0)
        self.assertIn('"state": "high"', text.getvalue())

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

    def test_calibration_request(self):
        values = [1, 1, 0, 0, 1, .1, 0, 5, .01]
        p = Port(m.frame(bytes.fromhex('011000460012')))
        m.Client(p, 1).calibrate(values)
        self.assertEqual(p.sent, m.frame(bytes.fromhex('01100046001224') + struct.pack('>9f', *values)))
        with self.assertRaises(ValueError): m.Client(p, 1).calibrate([float('nan')]*9)

    def test_standard_check(self):
        values = [1., .1, 0., 1., .01]
        response = m.frame(bytes.fromhex('0110005c000a')) + m.frame(bytes.fromhex('01030600013f800000'))
        p = Port(response)
        self.assertEqual(m.Client(p, 1).check_standard(values), {'passed': True, 'actual_mg_l': 1.0})
        self.assertEqual(p.sent, m.frame(bytes.fromhex('010300660003')))
        with self.assertRaises(ValueError):
            m.Client(p, 1).check_standard([float('nan')]*5)

    def test_log_stats(self):
        words = [1, 1, 2, 0, 3, 0xffff, 0xffff, 0, 4]
        p = Port(m.frame(bytes([1, 3, 18]) + struct.pack('>9H', *words)))
        self.assertEqual(m.Client(p, 1).log_stats(),
                         {'available': True, 'queued': 65538, 'written': 3,
                          'dropped': 0xffffffff, 'io_errors': 4})
        self.assertEqual(p.sent, m.frame(bytes.fromhex('0103006e0009')))
        p = Port(m.frame(bytes([1, 3, 18]) + struct.pack('>9H', 0, *([1] * 8))))
        self.assertEqual(m.Client(p, 1).log_stats(),
                         {'available': False, 'queued': None, 'written': None,
                          'dropped': None, 'io_errors': None})
        p = Port(m.frame(bytes([1, 3, 18]) + struct.pack('>9H', 2, *([0] * 8))))
        with self.assertRaises(m.ModbusError): m.Client(p, 1).log_stats()

    def test_log_stats_cli(self):
        class SerialPort(Port):
            def __init__(self, *args, **kwargs):
                super().__init__(m.frame(bytes([1, 3, 18]) + struct.pack('>9H', 1, 0, 5, 0, 4, 0, 1, 0, 0)))
            def __enter__(self): return self
            def __exit__(self, *args): pass
        text = io.StringIO()
        with patch.dict(sys.modules, {'serial': SimpleNamespace(Serial=SerialPort)}), contextlib.redirect_stdout(text):
            self.assertEqual(m.main(['--port', 'COM3', 'log-stats']), 0)
        self.assertEqual(__import__('json').loads(text.getvalue()),
                         {'available': True, 'queued': 5, 'written': 4, 'dropped': 1, 'io_errors': 0})

    def test_log_query(self):
        words = [3, 0, 7, 2, 0, 1, 0, 0, 0, 42, 0x3f80, 0]
        response = m.frame(bytes.fromhex('011000320003')) + m.frame(bytes([1, 3, 24]) + struct.pack('>12H', *words))
        p = Port(response)
        event = m.Client(p, 1).log()
        self.assertEqual(event, {'sequence': 7, 'type': 2, 'detail': 0, 'utc': 42, 'value_bits': 0x3f800000})

    def test_read_schedule(self):
        p = Port(m.frame(bytes([1, 3, 10]) + struct.pack('>5H', 2, 1, 2, 90, 65506)))
        self.assertEqual(m.Client(p, 1).get_schedule(), (2, 65538, 90, -30))
        self.assertEqual(p.sent, m.frame(bytes.fromhex('010300010005')))
        p = Port(m.frame(bytes([1, 3, 10]) + struct.pack('>5H', 3, 0, 0, 0, 0)))
        with self.assertRaises(m.ModbusError):
            m.Client(p, 1).get_schedule()

    def test_cli_uncertain_write_and_read_failures(self):
        class SerialPort(Port):
            def __init__(self, *args, **kwargs): super().__init__(b'')
            def __enter__(self): return self
            def __exit__(self, *args): pass
        with patch.dict(sys.modules, {'serial': SimpleNamespace(Serial=SerialPort)}):
            for action in (['command', 'start'], ['status']):
                text = io.StringIO()
                with contextlib.redirect_stderr(text):
                    self.assertEqual(m.main(['--port', 'COM3', '--timeout', '0.001'] + action), 1)
                self.assertEqual('outcome may be unknown' in text.getvalue(), action[0] == 'command')
            text = io.StringIO()
            with contextlib.redirect_stderr(text):
                self.assertEqual(m.main(['--port', 'COM3', 'schedule', 'daily', '0', '0', '0']), 1)
            self.assertNotIn('outcome may be unknown', text.getvalue())

    def test_exception_corruption_and_timeout(self):
        for reply in (m.frame(bytes.fromhex('018302')), bytes.fromhex('01030200000000'), b''):
            with self.assertRaises(m.ModbusError): m.Client(Port(reply), 1, .001).read(10, 1)
        with self.assertRaises(ValueError): m.Client(Port(b''), 1).schedule(2, 1, 1440, 0)

if __name__ == '__main__': unittest.main()
