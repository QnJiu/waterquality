import hashlib
import pathlib
import struct
import subprocess
import sys
import tempfile
import unittest

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, utils

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools'))
import sign_manifest as sm


class ManifestTests(unittest.TestCase):
    def test_signature_and_fields(self):
        key = ec.generate_private_key(ec.SECP256R1())
        image = b'abc'
        data = sm.create_manifest(image, 0x12345678, 2, 1, key)
        self.assertEqual(len(data), 116)
        self.assertEqual(struct.unpack('<4sIIIB3x32s', data[:52]),
                         (b'TPU1', 0x12345678, 2, 3, 1, hashlib.sha256(image).digest()))
        sig = utils.encode_dss_signature(int.from_bytes(data[52:84], 'big'), int.from_bytes(data[84:], 'big'))
        key.public_key().verify(sig, data[:52], ec.ECDSA(hashes.SHA256()))

    def test_invalid_parameters(self):
        key = ec.generate_private_key(ec.SECP256R1())
        for image, product, version, bank in ((b'', 1, 1, 0), (b'x', 0, 1, 0),
                                               (b'x', 1, 0, 0), (b'x', 1, 1, 2)):
            with self.assertRaises(ValueError):
                sm.create_manifest(image, product, version, bank, key)
        with self.assertRaises(ValueError):
            sm.create_manifest(b'x', 1, 1, 0, ec.generate_private_key(ec.SECP384R1()))

    def test_cli_refuses_overwrite(self):
        with tempfile.TemporaryDirectory() as temp:
            p = pathlib.Path(temp)
            key = ec.generate_private_key(ec.SECP256R1())
            (p / 'key').write_bytes(key.private_bytes(serialization.Encoding.PEM,
                serialization.PrivateFormat.PKCS8, serialization.NoEncryption()))
            (p / 'image').write_bytes(b'abc')
            cmd = [sys.executable, str(pathlib.Path(sm.__file__)), '--image', str(p / 'image'),
                   '--key', str(p / 'key'), '--product', '0x12345678', '--version', '2',
                   '--bank', '1', '--output', str(p / 'manifest')]
            result = subprocess.run(cmd, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            manifest = (p / 'manifest').read_bytes()
            self.assertIn(hashlib.sha256(manifest[:52]).hexdigest(), result.stdout)
            self.assertNotEqual(subprocess.run(cmd, capture_output=True).returncode, 0)
            self.assertEqual((p / 'manifest').read_bytes(), manifest)


if __name__ == '__main__':
    unittest.main()
