import errno
import importlib.util
from pathlib import Path
import unittest

path = Path(__file__).resolve().parents[1] / "examples" / "esp32_gateway" / "main.py"
spec = importlib.util.spec_from_file_location("tp_gateway", path)
gateway = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gateway)


class Socket:
    def __init__(self):
        self.incoming = [b"request", None]
        self.sent = b""
        self.block_send = True

    def recv(self, n):
        data = self.incoming.pop(0) if self.incoming else None
        if data is None:
            raise OSError(errno.EAGAIN)
        assert len(data) <= n
        return data

    def send(self, data):
        if self.block_send:
            self.block_send = False
            raise OSError(errno.EAGAIN)
        self.sent += data[:2]
        return min(2, len(data))


class Uart:
    def __init__(self):
        self.incoming = b"response"
        self.sent = b""

    def any(self):
        return len(self.incoming)

    def read(self, n):
        data, self.incoming = self.incoming[:n], self.incoming[n:]
        return data

    def write(self, data):
        self.sent += data[:2]
        return min(2, len(data))


class BridgeTest(unittest.TestCase):
    def test_partial_writes_and_nonblocking_socket(self):
        sock, uart = Socket(), Uart()
        bridge = gateway.Bridge(sock, uart)
        for _ in range(12):
            self.assertTrue(bridge.step())
        self.assertEqual(uart.sent, b"request")
        self.assertEqual(sock.sent, b"response")
        self.assertEqual(bridge.to_uart, b"")
        self.assertEqual(bridge.to_socket, b"")
        sock.incoming.append(b"")
        self.assertFalse(bridge.step())

    def test_zero_byte_send_closes_connection(self):
        sock, uart = Socket(), Uart()
        sock.send = lambda data: 0
        bridge = gateway.Bridge(sock, uart)
        self.assertTrue(bridge.step())
        self.assertFalse(bridge.step())

    def test_socket_error_propagates(self):
        sock, uart = Socket(), Uart()
        def failed(_):
            raise OSError(errno.ECONNRESET)
        sock.recv = failed
        with self.assertRaises(OSError):
            gateway.Bridge(sock, uart).step()


if __name__ == "__main__":
    unittest.main()
