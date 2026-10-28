import pathlib
import queue
import tkinter as tk
import struct
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools'))
from tp_gui import App, read_status, read_calibration_status, read_clock_status, read_event, parse_config, CONFIG_LABELS


def words(value):
    return struct.unpack('>HH', struct.pack('>f', value))


class FakeClient:
    def __init__(self, valid):
        self.calls = []
        self.valid = valid

    def maintenance_status(self):
        return None

    def read(self, address, count):
        self.calls.append((address, count))
        if address == 10:
            return [1, 0, 3, 0, *words(1.25), self.valid, 3, 1, 0]
        return [self.valid, *words(25.5)]


class StatusTest(unittest.TestCase):
    def test_valid_data_and_register_ranges(self):
        client = FakeClient(1)
        text = read_status(client)
        self.assertEqual(client.calls, [(10, 10), (128, 3)])
        self.assertIn('运行', text)
        self.assertIn('1.250 mg/L', text)
        self.assertIn('25.50 °C', text)
        self.assertIn('等待下一步：是', text)
        self.assertIn('升级维护：未知', text)

    def test_invalid_values_are_not_displayed(self):
        text = read_status(FakeClient(0))
        self.assertIn('无有效结果', text)
        self.assertIn('无有效温度', text)
        self.assertNotIn('1.250', text)
        self.assertNotIn('25.50', text)


class CalibrationStatusTest(unittest.TestCase):
    def test_running_and_passing(self):
        class Client:
            def auto_calibration_status(self):
                return {'state': 'high', 'error_code': 0}
            def auto_standard_status(self):
                return {'state': 'complete', 'error': 0, 'passed': True, 'actual_mg_l': 5.0}
        text = read_calibration_status(Client())
        self.assertIn('高点采样', text)
        self.assertIn('通过', text)
        self.assertIn('5.000 mg/L', text)

    def test_unavailable_results_are_not_shown_as_failures(self):
        class Client:
            def auto_calibration_status(self):
                return {'state': 'unavailable', 'error_code': 0}
            def auto_standard_status(self):
                return {'state': 'unavailable', 'error': 0, 'passed': None, 'actual_mg_l': None}
        text = read_calibration_status(Client())
        self.assertIn('未配置', text)
        self.assertIn('无结果', text)
        self.assertNotIn('不通过', text)


class ClockStatusTest(unittest.TestCase):
    def test_verified_utc_time(self):
        class Client:
            def clock_status(self):
                return {'state': 'verified', 'utc_seconds': 1700000000}
        text = read_clock_status(Client())
        self.assertIn('校时回读成功', text)
        self.assertIn('2023-11-14 22:13:20 UTC', text)

    def test_pending_clock_is_not_reported_as_valid(self):
        class Client:
            def clock_status(self):
                return {'state': 'pending', 'utc_seconds': None}
        text = read_clock_status(Client())
        self.assertIn('校时中', text)
        self.assertIn('无有效时间', text)


class EventTest(unittest.TestCase):
    def test_latest_event_including_missing_timestamp(self):
        class Client:
            def log(self, sequence):
                return {'sequence': 42, 'type': 2, 'detail': 3, 'utc': None,
                        'value_bits': 0x3f800000}
        text = read_event(Client())
        for item in ('序号：42', '类型：2', '详情：3', '无时间', '0x3F800000'):
            self.assertIn(item, text)

    def test_historical_measurement_is_decoded(self):
        class Client:
            def log(self, sequence):
                self.sequence = sequence
                return {'sequence': sequence, 'type': 2, 'detail': 1, 'utc': 1700000000,
                        'value_bits': 0x3f800000}
        client = Client()
        text = read_event(client, 17)
        self.assertEqual(client.sequence, 17)
        self.assertIn('序号：17', text)
        self.assertIn('浓度：1.000 mg/L', text)

    def test_other_events_do_not_claim_measurements(self):
        class Client:
            def log(self, sequence):
                return {'sequence': 1, 'type': 3, 'detail': 1, 'utc': None,
                        'value_bits': 0x3f800000}
        self.assertNotIn('浓度：', read_event(Client()))

    def test_query_failure_is_propagated(self):
        class Client:
            def log(self, sequence):
                raise ValueError('日志为空')
        with self.assertRaisesRegex(ValueError, '日志为空'):
            read_event(Client())


class ConfigInputTest(unittest.TestCase):
    def test_converts_complete_snapshot(self):
        values = {name: '1.25' for name in CONFIG_LABELS}
        values.update(period_seconds='60', slave_address='1')
        result = parse_config(values)
        self.assertEqual(set(result), set(CONFIG_LABELS))
        self.assertEqual(result['kp'], 1.25)
        self.assertEqual(result['period_seconds'], 60)
        self.assertEqual(result['slave_address'], 1)

    def test_rejects_invalid_integer_and_float(self):
        values = {name: '1' for name in CONFIG_LABELS}
        values['period_seconds'] = '1.5'
        with self.assertRaises(ValueError):
            parse_config(values)
        values['period_seconds'] = '60'
        values['kp'] = 'abc'
        with self.assertRaises(ValueError):
            parse_config(values)


class ConnectionChangeTest(unittest.TestCase):
    def setUp(self):
        class Root:
            def after(self, *_): pass
        self.tk = tk.Tcl()
        self.app = App.__new__(App)
        self.app.root = Root()
        self.app.events = queue.Queue()
        self.app.busy = True
        self.app.connection_generation = 0
        for name in ('status', 'cal_status', 'clock_status', 'event_status', 'message'):
            setattr(self.app, name, tk.StringVar(master=self.tk, value='old'))

    def test_old_device_reply_does_not_replace_current_status(self):
        self.app.connection_changed()
        self.app.events.put((0, 'COM3', 1, 'status', 'old device data', None))
        self.app.poll()
        self.assertIn('重新读取', self.app.status.get())
        self.assertIn('丢弃', self.app.message.get())
        self.assertFalse(self.app.busy)

    def test_old_device_command_outcome_identifies_target(self):
        self.app.connection_changed()
        self.app.events.put((0, 'COM3', 1, 'command', None, 'timeout'))
        self.app.poll()
        self.assertIn('COM3 地址 1', self.app.message.get())
        self.assertIn('timeout', self.app.message.get())


if __name__ == '__main__':
    unittest.main()
