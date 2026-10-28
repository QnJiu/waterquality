import pathlib
import struct
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools'))
from tp_gui import read_status, read_calibration_status, read_clock_status, read_latest_event, parse_config, CONFIG_LABELS


def words(value):
    return struct.unpack('>HH', struct.pack('>f', value))


class FakeClient:
    def __init__(self, valid):
        self.calls = []
        self.valid = valid

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
            def log(self):
                return {'sequence': 42, 'type': 2, 'detail': 3, 'utc': None,
                        'value_bits': 0x3f800000}
        text = read_latest_event(Client())
        for item in ('序号：42', '类型：2', '详情：3', '无时间', '0x3F800000'):
            self.assertIn(item, text)

    def test_query_failure_is_propagated(self):
        class Client:
            def log(self):
                raise ValueError('日志为空')
        with self.assertRaisesRegex(ValueError, '日志为空'):
            read_latest_event(Client())


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


if __name__ == '__main__':
    unittest.main()
