import importlib.util
import json
import pathlib
import sys
import types
import unittest
from unittest.mock import patch

folder = pathlib.Path(__file__).resolve().parents[1] / 'examples' / 'esp32_cloud'


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, folder / path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


cloud = load('cloud', 'cloud.py')


class CloudTest(unittest.TestCase):
    def setUp(self):
        self.status = {'state': 2, 'fault': 0, 'step': 15, 'start_result': 0,
                       'result_valid': True, 'concentration_mg_l': 0.3}

    def test_property_message(self):
        class Client:
            def publish(self, topic, payload, qos):
                self.topic, self.message, self.qos = topic, json.loads(payload), qos
        client = Client()
        cloud.publish_status(client, '/sys/p/d/thing/event/property/post', self.status, 7)
        self.assertEqual(client.topic, b'/sys/p/d/thing/event/property/post')
        self.assertEqual(client.qos, 0)
        self.assertEqual(client.message['id'], '7')
        self.assertEqual(client.message['method'], 'thing.event.property.post')
        self.assertEqual(client.message['params']['Concentration'], 0.3)
        self.assertEqual(client.message['params']['ResultValid'], 1)

    def test_invalid_or_nonfinite_concentration(self):
        self.status['result_valid'] = False
        self.status['concentration_mg_l'] = None
        self.assertNotIn('Concentration', json.loads(cloud.property_message(self.status, 1))['params'])
        self.status['result_valid'] = True
        self.status['concentration_mg_l'] = float('nan')
        with self.assertRaises(ValueError): cloud.property_message(self.status, 1)

    def test_ota_notice_and_stage(self):
        import tempfile
        import os
        notice_module = load('ota_notice', 'ota_notice.py')
        notice = notice_module.UpgradeNotice('/ota/device/upgrade/p/d')
        topic = b'/ota/device/upgrade/p/d'
        payload = json.dumps({'data': {'url': 'https://example.com/app.tpb', 'version': '2'}}).encode()
        notice.callback(b'other', payload)
        notice.callback(topic, b'invalid json')
        notice.callback(topic, json.dumps({'data': {'url': 'http://bad', 'version': '2'}}).encode())
        self.assertIsNone(notice.pending)
        notice.callback(topic, payload)
        self.assertEqual(notice.pending, ('https://example.com/app.tpb', '2'))
        calls = []
        class UART:
            def __init__(self, *args, **kwargs): calls.append(('uart', args, kwargs))
            def deinit(self): calls.append('deinit')
        https = types.SimpleNamespace(download=lambda url, target, limit, ca: (calls.append((url, limit, ca)), target.write(b'package')))
        ota = types.SimpleNamespace(Client=lambda uart, clock: uart, MAX_IMAGE_SIZE=100,
                                    upload_bundle=lambda client, source: {'bank': 1, 'version': 2, 'written': len(source.read())})
        config = types.SimpleNamespace(OTA_CA_FILE='ca.pem', OTA_UART_ID=1, OTA_UART_TX=25, OTA_UART_RX=26)
        old = os.getcwd()
        with tempfile.TemporaryDirectory() as tmp, patch.dict(sys.modules, {'ota_https': https, 'ota_client': ota}):
            try:
                os.chdir(tmp)
                result = notice_module.stage_bundle(notice.pending[0], config, UART, lambda pin: pin, None)
            finally:
                os.chdir(old)
        self.assertEqual(result['written'], 7)
        self.assertEqual(calls[0], ('https://example.com/app.tpb', 216, 'ca.pem'))
        self.assertEqual(calls[-1], 'deinit')
        notice.pending = None
        notice.completed_version = '2'
        notice.callback(topic, payload)
        self.assertIsNone(notice.pending)

    def test_tls_setup_and_failed_connection_close(self):
        calls = []
        class Socket:
            def close(self): calls.append('close')
        class Client:
            def __init__(self, *args, **kwargs):
                self.sock = Socket()
                calls.append(('client', args, kwargs))
            def connect(self, timeout):
                calls.append(('connect', timeout))
                raise OSError('failure')
        class Context:
            def __init__(self, protocol): calls.append(('protocol', protocol))
            def load_verify_locations(self, cafile): calls.append(('ca', cafile))
        cfg = types.SimpleNamespace(CA_FILE='ca.pem', MQTT_CLIENT_ID='client',
                                    MQTT_HOST='iot.example', MQTT_USER='user', MQTT_PASSWORD='pass')
        ssl = types.SimpleNamespace(SSLContext=Context, PROTOCOL_TLS_CLIENT=4, CERT_REQUIRED=2)
        mqtt = types.ModuleType('umqtt.simple')
        mqtt.MQTTClient, mqtt.MQTTException = Client, type('MQTTException', (Exception,), {})
        modules = {'network': types.ModuleType('network'),
                   'ntptime': types.SimpleNamespace(settime=lambda: calls.append('ntp')),
                   'ssl': ssl, 'machine': types.SimpleNamespace(UART=object, Pin=object),
                   'umqtt': types.ModuleType('umqtt'), 'umqtt.simple': mqtt,
                   'config': cfg, 'modbus_status': load('modbus_status', 'modbus_status.py'),
                   'cloud': cloud, 'ota_notice': load('ota_notice', 'ota_notice.py')}
        with patch.dict(sys.modules, modules):
            main = load('esp32_cloud_main', 'main.py')
            with self.assertRaises(OSError): main.connect_mqtt()
        self.assertIn('ntp', calls)
        self.assertIn(('ca', 'ca.pem'), calls)
        self.assertIn(('connect', 10), calls)
        self.assertEqual(calls[-1], 'close')
        args, kwargs = next((entry[1], entry[2]) for entry in calls if isinstance(entry, tuple) and entry[0] == 'client')
        self.assertEqual(args, (b'client', 'iot.example'))
        self.assertEqual(kwargs['port'], 8883)
        self.assertEqual(kwargs['ssl'].verify_mode, ssl.CERT_REQUIRED)


if __name__ == '__main__': unittest.main()
