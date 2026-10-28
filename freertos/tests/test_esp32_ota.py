import hashlib
import importlib.util
import io
import pathlib
import struct
import sys
import unittest

root = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('esp_ota', root / 'examples' / 'esp32_gateway' / 'ota_client.py')
ota = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ota)
sys.path.insert(0, str(root / 'tests'))
from test_upgrade_client import FakeClient


def manifest_for(image):
    body = struct.pack('<4sIIIB3x32s', b'TPU1', 1, 2, len(image), 1,
                       hashlib.sha256(image).digest())
    return body + bytes(64), hashlib.sha256(body).digest()


class Clock:
    now = 0
    def ticks_ms(self): return self.now
    def ticks_add(self, a, b): return a + b
    def ticks_diff(self, a, b): return a - b
    def sleep_ms(self, ms): self.now += ms


class EchoUART:
    def __init__(self): self.pending = b''
    def write(self, packet):
        decoder = ota.Decoder()
        frames = [frame for byte in packet if (frame := decoder.feed(byte))]
        cmd, seq, _ = frames[0]
        self.pending = ota.encode(cmd | 0x80, seq, b'\0\0')
        return len(packet)
    def read(self, n):
        result, self.pending = self.pending[:3], self.pending[3:]
        return result


class Tests(unittest.TestCase):
    def test_serial_framing_matches_pc_client(self):
        self.assertEqual(ota.Client(EchoUART(), Clock()).command(ota.STATUS), (0, None))
        self.assertEqual(ota.encode(ota.BEGIN, 0x7d7e, b'ok').hex(), '7e01027d5e7d5d02006f6ba9b47e')

    def test_resume_after_lost_replies(self):
        image = bytes(range(256)) * 3
        manifest, ident = manifest_for(image)
        client = FakeClient(image, ident)
        state = ota.upload(client, manifest, io.BytesIO(image))
        self.assertEqual(state['phase'], 3)
        self.assertEqual(client.writes, [0, 256, 512])
        self.assertEqual(client.storage, image)

    def test_begin_retries_after_request_or_reply_is_lost(self):
        class LostBegin(FakeClient):
            def __init__(self, image, ident):
                super().__init__(image, ident)
                self.attempts = 0
            def command(self, cmd, payload=b''):
                if cmd == ota.BEGIN:
                    self.attempts += 1
                    if self.attempts <= 2:
                        raise TimeoutError()
                return super().command(cmd, payload)

        image = b'firmware'
        manifest, ident = manifest_for(image)
        client = LostBegin(image, ident)
        self.assertEqual(ota.upload(client, manifest, io.BytesIO(image))['phase'], 3)
        self.assertEqual(client.attempts, 3)
        client = LostBegin(image, ident)
        with self.assertRaises(TimeoutError):
            ota.upload(client, manifest, io.BytesIO(image), retries=1)
        self.assertEqual(client.attempts, 2)

    def test_finish_timeout_reports_unknown_outcome(self):
        class LostFinish(FakeClient):
            def command(self, cmd, payload=b''):
                if cmd == ota.FINISH:
                    raise TimeoutError()
                return super().command(cmd, payload)
        image = b'firmware'
        manifest, ident = manifest_for(image)
        with self.assertRaises(ota.FinishUnknown):
            ota.upload(LostFinish(image, ident), manifest, io.BytesIO(image), retries=1)

    def test_finish_io_error_reports_unknown_outcome(self):
        class BrokenFinish(FakeClient):
            def command(self, cmd, payload=b''):
                if cmd == ota.FINISH:
                    self.state['phase'] = 3  # device may have received it
                    raise OSError('UART read failed')
                return super().command(cmd, payload)
        image = b'firmware'
        manifest, ident = manifest_for(image)
        client = BrokenFinish(image, ident)
        with self.assertRaises(ota.FinishUnknown):
            ota.upload(client, manifest, io.BytesIO(image))
        self.assertEqual(client.state['phase'], 3)

    def test_failed_finish_retry_after_lost_reply_is_unknown(self):
        class Retry(FakeClient):
            def __init__(self, image, ident):
                super().__init__(image, ident)
                self.attempts = 0
            def command(self, cmd, payload=b""):
                if cmd == ota.FINISH:
                    self.attempts += 1
                    if self.attempts == 1:
                        self.state["phase"] = 3
                        raise TimeoutError()
                    return ota.INVALID, None
                return super().command(cmd, payload)
        image = b'firmware'
        manifest, ident = manifest_for(image)
        client = Retry(image, ident)
        with self.assertRaises(ota.FinishUnknown):
            ota.upload(client, manifest, io.BytesIO(image))
        self.assertEqual(client.attempts, 2)

    def test_invalid_finish_reply_keeps_outcome_unknown(self):
        class BadReply(FakeClient):
            def __init__(self, image, ident, reply):
                super().__init__(image, ident)
                self.reply = reply
            def command(self, cmd, payload=b''):
                if cmd == ota.FINISH:
                    self.state['phase'] = 3  # FINISH may have already succeeded
                    if self.reply == 'malformed':
                        raise ota.ProtocolError('invalid response header')
                    if self.reply == 'missing':
                        return ota.OK, None
                    return ota.OK, dict(self.state, phase=2)
                return super().command(cmd, payload)
        image = b'firmware'
        manifest, ident = manifest_for(image)
        for reply in ('malformed', 'missing', 'wrong_phase'):
            with self.subTest(reply=reply):
                client = BadReply(image, ident, reply)
                with self.assertRaises(ota.FinishUnknown):
                    ota.upload(client, manifest, io.BytesIO(image))
                self.assertEqual(client.state['phase'], 3)

    def test_reject_bad_image_before_any_command(self):
        image = b'firmware'
        manifest, ident = manifest_for(image)
        client = FakeClient(image, ident)
        with self.assertRaises(ValueError):
            ota.upload(client, manifest, io.BytesIO(b'bad data'))
        self.assertFalse(client.writes)
        self.assertEqual(client.state['written'], 0)

    def test_single_bundle_upload_and_tamper_rejected(self):
        image = bytes(range(256)) * 3
        manifest, ident = manifest_for(image)
        client = FakeClient(image, ident)
        result = ota.upload_bundle(client, io.BytesIO(manifest + image))
        self.assertEqual(result['phase'], 3)
        self.assertEqual(client.storage, image)
        for bad in (manifest + image[:-1], manifest + image + b'x', b'x' + manifest[1:] + image):
            with self.subTest(bad_length=len(bad)), self.assertRaises(ValueError):
                ota.upload_bundle(FakeClient(image, ident), io.BytesIO(bad))

    def test_no_reply_times_out(self):
        class UART:
            def write(self, data): return len(data)
            def read(self, n): return None
        with self.assertRaises(TimeoutError):
            ota.Client(UART(), Clock(), timeout_ms=3).command(ota.STATUS)


if __name__ == '__main__': unittest.main()
