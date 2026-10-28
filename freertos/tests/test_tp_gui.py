import pathlib
import struct
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools'))
from tp_gui import read_status


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


if __name__ == '__main__':
    unittest.main()
