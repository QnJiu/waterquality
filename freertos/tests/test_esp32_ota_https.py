import importlib.util
import io
import pathlib
import unittest
from unittest.mock import patch

path = pathlib.Path(__file__).resolve().parents[1] / 'examples' / 'esp32_gateway' / 'ota_https.py'
spec = importlib.util.spec_from_file_location('ota_https', path)
https = importlib.util.module_from_spec(spec)
spec.loader.exec_module(https)


class Stream:
    def __init__(self, data): self.data, self.sent, self.closed = data, b'', False
    def settimeout(self, value): self.timeout = value
    def connect(self, address): self.address = address
    def write(self, data):
        self.sent += data[:7]
        return min(len(data), 7)
    def read(self, n):
        data, self.data = self.data[:n], self.data[n:]
        return data
    def close(self): self.closed = True


class Context:
    def __init__(self, stream): self.stream = stream
    def load_verify_locations(self, cafile): self.ca = cafile
    def wrap_socket(self, sock, server_hostname):
        self.hostname = server_hostname
        return self.stream


class Tests(unittest.TestCase):
    def fetch(self, raw, limit=20):
        stream = Stream(raw)
        context = Context(stream)
        target = io.BytesIO()
        with patch.object(https.socket, 'socket', return_value=stream), \
             patch.object(https.socket, 'getaddrinfo', return_value=[(0, 0, 0, '', ('server', 443))]), \
             patch.object(https.ssl, 'SSLContext', return_value=context):
            result = https.download('https://example.com/image?x=1', target, limit, 'ca.pem')
        return result, target.getvalue(), context, stream

    def test_success_with_verified_tls_and_partial_writes(self):
        n, body, ctx, stream = self.fetch(b'HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhello')
        self.assertEqual((n, body), (5, b'hello'))
        self.assertEqual(ctx.verify_mode, https.ssl.CERT_REQUIRED)
        self.assertEqual((ctx.ca, ctx.hostname), ('ca.pem', 'example.com'))
        self.assertIn(b'GET /image?x=1 HTTP/1.1\r\n', stream.sent)
        self.assertTrue(stream.closed)

    def test_refuse_unsafe_urls(self):
        for url in ('http://server/a', 'https://user@server/a', 'https://server/a\r\nInjected: x',
                    'https://server:8443/a', 'https://server/a#fragment',
                    'https://server/a b', 'https://server/a\tHTTP/1.1', 'https://server/a\x00'):
            with self.subTest(url=url), self.assertRaises(ValueError): https.split_url(url)

    def test_query_without_path(self):
        self.assertEqual(https.split_url('https://example.com?token=abc'),
                         ('example.com', '/?token=abc'))
        self.assertEqual(https.split_url('https://example.com'), ('example.com', '/'))

    def test_bad_response(self):
        cases = [b'HTTP/1.1 302 Found\r\n\r\n',
                 b'HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n',
                 b'HTTP/1.1 200 OK\r\nContent-Length: 21\r\n\r\n',
                 b'HTTP/1.1 200 OK\r\nContent-Length: 3\r\nContent-Length: 3\r\n\r\n',
                 b'HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nabc']
        for response in cases:
            with self.subTest(response=response), self.assertRaises((ValueError, OSError)):
                self.fetch(response)


if __name__ == '__main__': unittest.main()
