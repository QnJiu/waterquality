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

    def test_lock_busy_retries_without_executing_command(self):
        class BusyTransport(CTransport):
            def __init__(self, lib):
                super().__init__(lib)
                self.drop.clear()
                self.busy = {u.BEGIN: 2, u.WRITE: 1, u.FINISH: 1}

            def write(self, data):
                decoder = u.Decoder()
                frame = None
                for byte in data:
                    frame = decoder.feed(byte) or frame
                command, sequence, _ = frame
                if self.busy.get(command, 0):
                    self.busy[command] -= 1
                    self.response = u.encode(command | 0x80, sequence, bytes((u.LOCK_BUSY, 0)))
                else:
                    super().write(data)

        image = bytes(range(256)) * 3
        transport = BusyTransport(self.lib)
        state = u.upload(u.Client(transport, 0.02, busy_retries=3, busy_delay=0),
                         b"ok", bytes([1]) + bytes(31), image)
        self.assertEqual(state["written"], len(image))
        self.assertEqual(self.lib.sim_writes(), 3)
        self.assertEqual(self.lib.sim_boot_phase(), 1)

    def test_lock_busy_retry_limit(self):
        class BusyTransport:
            count = 0
            def write(self, data):
                self.count += 1
                decoder = u.Decoder()
                for byte in data:
                    frame = decoder.feed(byte)
                    if frame:
                        self.response = u.encode(frame[0] | 0x80, frame[1], bytes((u.LOCK_BUSY, 0)))
            def read(self, timeout):
                response, self.response = self.response, b""
                return response
        transport = BusyTransport()
        self.assertEqual(u.Client(transport, 0.02, busy_retries=2, busy_delay=0).command(u.STATUS),
                         (u.LOCK_BUSY, None))
        self.assertEqual(transport.count, 3)

    def test_application_busy_without_state_is_not_retried(self):
        class BusyTransport:
            count = 0
            def write(self, data):
                self.count += 1
                decoder = u.Decoder()
                for byte in data:
                    frame = decoder.feed(byte)
                    if frame:
                        self.response = u.encode(frame[0] | 0x80, frame[1], bytes((u.BUSY, 0)))
            def read(self, timeout):
                response, self.response = self.response, b""
                return response
        transport = BusyTransport()
        self.assertEqual(u.Client(transport, 0.02, busy_retries=2, busy_delay=0).command(u.BEGIN, b"ok"),
                         (u.BUSY, None))
        self.assertEqual(transport.count, 1)

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

    def test_reconnect_after_verified_before_staging(self):
        lib = self.lib
        transport = CTransport(lib); transport.drop.clear()
        client = u.Client(transport, 0.02)
        image = bytes(range(256)) * 3
        self.assertEqual(client.command(u.BEGIN, b"ok")[0], u.OK)
        for offset in range(0, len(image), 256):
            self.assertEqual(client.command(u.WRITE, offset.to_bytes(4, "little") + image[offset:offset+256])[0], u.OK)
        self.assertEqual(lib.sim_verify_download(), u.OK)
        self.assertEqual(lib.sim_boot_phase(), 0)
        lib.sim_reset_transport()
        state = u.upload(u.Client(CTransport(lib), 0.02), b"ok", bytes([1]) + bytes(31), image)
        self.assertEqual((state["phase"], state["written"]), (3, len(image)))
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
