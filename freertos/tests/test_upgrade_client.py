import hashlib
import io
import pathlib
import struct
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import upgrade_client as u


class FakeClient:
    def __init__(self, image, manifest_id):
        self.image = image
        self.state = {"phase": 2, "bank": 1, "version": 2, "size": len(image),
                      "written": 0, "id": manifest_id}
        self.storage = bytearray(len(image))
        self.lose_write = True
        self.lose_finish = True
        self.writes = []

    def command(self, cmd, payload=b""):
        if cmd == u.BEGIN:
            return u.OK, dict(self.state)
        if cmd == u.STATUS:
            return u.OK, dict(self.state)
        if cmd == u.WRITE:
            offset = struct.unpack_from("<I", payload)[0]
            data = payload[4:]
            self.writes.append(offset)
            if offset != self.state["written"]:
                return u.INVALID, dict(self.state)
            self.storage[offset:offset+len(data)] = data
            self.state["written"] += len(data)
            if self.lose_write:
                self.lose_write = False
                raise TimeoutError()
            return u.OK, dict(self.state)
        if cmd == u.FINISH:
            assert self.storage == self.image
            self.state["phase"] = 3
            if self.lose_finish:
                self.lose_finish = False
                raise TimeoutError()
            return u.OK, dict(self.state)
        raise AssertionError(cmd)


class Tests(unittest.TestCase):
    def test_manifest_and_image_preflight(self):
        image = b"firmware image"
        body = struct.pack("<4sIIIB3x32s", b"TPU1", 1, 2, len(image), 1,
                           hashlib.sha256(image).digest())
        manifest = body + bytes(64)
        identity = hashlib.sha256(body).digest()
        self.assertEqual(u.check_files(manifest, image), identity)
        self.assertEqual(u.check_files(manifest, image, identity), identity)
        invalid = [manifest[:-1], b"bad!" + manifest[4:],
                   manifest[:17] + b"\x01" + manifest[18:]]
        for bad in invalid:
            with self.subTest(manifest=bad):
                with self.assertRaises(ValueError):
                    u.check_files(bad, image)
        for bad_image in (image[:-1], b"X" + image[1:]):
            with self.assertRaises(ValueError):
                u.check_files(manifest, bad_image)
        with self.assertRaises(ValueError):
            u.check_files(manifest, image, bytes(32))

    def test_https_download_and_redirect_policy(self):
        for url in ("http://example.com/file", "file:///etc/passwd",
                    "https://user:password@example.com/file"):
            with self.subTest(url=url), self.assertRaises(ValueError):
                u.download(url, 10, 1)
        handler = u.HttpsRedirect()
        request = u.urllib.request.Request("https://example.com/file")
        with self.assertRaises(ValueError):
            handler.redirect_request(request, None, 302, "Found", {}, "http://example.com/file")
        opener = unittest.mock.Mock()
        with patch.object(u.urllib.request, "build_opener", return_value=opener):
            opener.open.return_value = io.BytesIO(b"12345")
            self.assertEqual(u.download("https://example.com/file", 5, 2), b"12345")
            opener.open.assert_called_with("https://example.com/file", timeout=2)
            opener.open.return_value = io.BytesIO(b"123456")
            with self.assertRaises(ValueError):
                u.download("https://example.com/file", 5, 2)

    def test_remote_files_checked_before_device_connection(self):
        image = b"firmware"
        body = struct.pack("<4sIIIB3x32s", b"TPU1", 1, 2, len(image), 1,
                           hashlib.sha256(image).digest())
        manifest = body + bytes(64)
        argv = ["upgrade_client", "--tcp", "host:1234", "--manifest-url",
                "https://example.com/manifest", "--image-url", "https://example.com/image"]
        with patch.object(sys, "argv", argv), patch.object(u, "download", side_effect=[manifest, b"wrong"]), \
             patch.object(u, "TcpTransport") as transport, patch.object(sys, "stderr", new_callable=io.StringIO):
            self.assertEqual(u.main(), 1)
            transport.assert_not_called()
        state = {"bank": 1, "version": 2, "written": len(image)}
        with patch.object(sys, "argv", argv), patch.object(u, "download", side_effect=[manifest, image]) as fetch, \
             patch.object(u, "TcpTransport") as transport, patch.object(u, "upload", return_value=state) as upload, \
             patch.object(sys, "stdout", new_callable=io.StringIO):
            self.assertEqual(u.main(), 0)
            self.assertEqual(fetch.call_args_list[0].args[1], 116)
            self.assertEqual(fetch.call_args_list[1].args[1], u.MAX_IMAGE_SIZE)
            self.assertEqual(upload.call_args.args[2], hashlib.sha256(body).digest())
            transport.return_value.close.assert_called_once()

    def test_frames_and_recovery(self):
        self.assertEqual(u.crc16(b"123456789"), 0x4B37)
        self.assertEqual(u.encode(u.BEGIN, 0x7D7E, b"ok").hex(), "7e01027d5e7d5d02006f6ba9b47e")
        decoder = u.Decoder()
        frame = u.encode(0x84, 0x7D7E, b"\x7e\x7d")
        parsed = [x for b in frame if (x := decoder.feed(b)) is not None]
        self.assertEqual(parsed, [(0x84, 0x7D7E, b"\x7e\x7d")])
        damaged = bytearray(frame); damaged[-2] ^= 1
        self.assertFalse([x for b in damaged if (x := decoder.feed(b)) is not None])
        self.assertEqual([x for b in frame if (x := decoder.feed(b)) is not None], parsed)
        for b in bytes([u.FLAG]) + b"a" * 521 + bytes([u.FLAG]):
            self.assertIsNone(decoder.feed(b))
        self.assertEqual([x for b in frame if (x := decoder.feed(b)) is not None], parsed)

    def test_reply_validation(self):
        data = struct.pack("<BBBBIII", u.OK, 1, 2, 1, 9, 100, 10) + bytes(32)
        result, state = u.parse_reply(data)
        self.assertEqual((result, state["written"], state["version"]), (u.OK, 10, 9))
        for bad in (b"", b"\0\1", b"\0\0x", bytes([9, 0])):
            with self.assertRaises(u.ProtocolError):
                u.parse_reply(bad)

    def test_lost_replies_and_resume(self):
        image = bytes(range(256)) * 3
        client = FakeClient(image, bytes(range(32)))
        state = u.upload(client, b"manifest", bytes(range(32)), image)
        self.assertEqual(state["phase"], 3)
        self.assertEqual(client.writes, [0, 256, 512])
        self.assertEqual(client.storage, image)

    def test_mismatched_identity_does_not_write(self):
        client = FakeClient(b"data", bytes(32))
        with self.assertRaises(u.ProtocolError):
            u.upload(client, b"manifest", bytes([1]) * 32, b"data")
        self.assertFalse(client.writes)


if __name__ == "__main__":
    unittest.main()
