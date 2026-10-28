import ctypes
import os
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import upgrade_client as u


class CTransport:
    def __init__(self, library):
        self.lib = library
        self.response = b""
        self.drop = {u.WRITE, u.FINISH}
        self.decoder = u.Decoder()

    def write(self, data):
        command = None
        for byte in data:
            parsed = self.decoder.feed(byte)
            if parsed:
                command = parsed[0]
        incoming = ctypes.create_string_buffer(data)
        outgoing = ctypes.create_string_buffer(120)
        n = self.lib.sim_exchange(incoming, len(data), outgoing, len(outgoing))
        if n < 0:
            raise OSError("simulator frame error")
        self.response = outgoing.raw[:n]
        if command in self.drop:
            self.drop.remove(command)
            self.response = b""

    def read(self, timeout):
        result, self.response = self.response, b""
        return result


class Tests(unittest.TestCase):
    def setUp(self):
        path = os.environ.get("TP_UP_SIM_LIB")
        if not path:
            self.skipTest("set TP_UP_SIM_LIB to the built upgrade simulator library")
        self.lib = ctypes.CDLL(path)
        self.lib.sim_exchange.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p, ctypes.c_size_t]
        self.lib.sim_exchange.restype = ctypes.c_int
        self.lib.sim_reset_transport.restype = None
        self.assertEqual(self.lib.sim_init(), 0)

    def test_python_client_against_c_upgrade_core(self):
        lib = self.lib
        client = u.Client(CTransport(lib), 0.02)
        image = bytes(range(256)) * 3
        state = u.upload(client, b"ok", bytes([1]) + bytes(31), image)
        self.assertEqual((state["phase"], state["written"]), (3, len(image)))
        self.assertEqual(lib.sim_writes(), 3)
        self.assertEqual(lib.sim_boot_phase(), 1)  # TP_BOOT_READY

    def test_reconnect_after_partial_download(self):
        lib = self.lib
        transport = CTransport(lib); transport.drop.clear()
        client = u.Client(transport, 0.02)
        image = bytes(range(256)) * 3
        self.assertEqual(client.command(u.BEGIN, b"ok")[0], u.OK)
        self.assertEqual(client.command(u.WRITE, bytes(4) + image[:256])[0], u.OK)
        lib.sim_reset_transport()
        result = u.upload(u.Client(CTransport(lib), 0.02), b"ok", bytes([1]) + bytes(31), image)
        self.assertEqual(result["written"], len(image))
        self.assertEqual(lib.sim_writes(), 3)
        self.assertEqual(lib.sim_boot_phase(), 1)

    def test_rejects_untrusted_manifest(self):
        with self.assertRaisesRegex(u.ProtocolError, "begin failed"):
            u.upload(u.Client(CTransport(self.lib), 0.02), b"bad", bytes([1]) + bytes(31), bytes(range(256)) * 3)
        self.assertEqual(self.lib.sim_writes(), 0)
        self.assertEqual(self.lib.sim_boot_phase(), 0)

    def test_rejects_corrupted_image(self):
        image = bytearray(bytes(range(256)) * 3)
        image[100] ^= 1
        transport = CTransport(self.lib)
        transport.drop.clear()
        with self.assertRaisesRegex(u.ProtocolError, "finish failed"):
            u.upload(u.Client(transport, 0.02), b"ok", bytes([1]) + bytes(31), image)
        self.assertEqual(self.lib.sim_writes(), 3)
        self.assertEqual(self.lib.sim_boot_phase(), 0)


if __name__ == "__main__":
    unittest.main()
