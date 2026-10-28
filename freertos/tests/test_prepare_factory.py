import pathlib
import struct
import subprocess
import sys
import tempfile
import unittest

from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import ec

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools'))
import prepare_factory
import sign_manifest


class FactoryPreparationTests(unittest.TestCase):
    def setUp(self):
        self.key = ec.generate_private_key(ec.SECP256R1())
        self.image = struct.pack('<II', 0x20020000, 0x08020009) + b'firmware'
        self.manifest = sign_manifest.create_manifest(self.image, 42, 1, 0, self.key)

    def test_checks_signature_image_and_vector(self):
        source = prepare_factory.identity_source(self.key.public_key(), self.manifest, self.image)
        self.assertIn('tp_f407_boot_product_id = 42u', source)
        with self.assertRaises(ValueError):
            prepare_factory.identity_source(self.key.public_key(), self.manifest, self.image + b'x')
        for manifest, key in ((self.manifest[:-1] + bytes([self.manifest[-1] ^ 1]), self.key.public_key()),
                              (self.manifest, ec.generate_private_key(ec.SECP256R1()).public_key())):
            with self.assertRaises(InvalidSignature):
                prepare_factory.identity_source(key, manifest, self.image)
        bad_image = struct.pack('<II', 0x20020000, 0x08080009) + b'firmware'
        bad_manifest = sign_manifest.create_manifest(bad_image, 42, 1, 0, self.key)
        with self.assertRaises(ValueError):
            prepare_factory.identity_source(self.key.public_key(), bad_manifest, bad_image)

    def test_cli_never_overwrites(self):
        with tempfile.TemporaryDirectory() as directory:
            p = pathlib.Path(directory)
            (p / 'public.pem').write_bytes(self.key.public_key().public_bytes(
                serialization.Encoding.PEM, serialization.PublicFormat.SubjectPublicKeyInfo))
            (p / 'manifest').write_bytes(self.manifest)
            (p / 'image').write_bytes(self.image)
            output = p / 'identity.c'
            cmd = [sys.executable, prepare_factory.__file__, '--public-key', str(p / 'public.pem'),
                   '--manifest', str(p / 'manifest'), '--image', str(p / 'image'), '--output', str(output)]
            result = subprocess.run(cmd, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            original = output.read_bytes()
            self.assertNotEqual(subprocess.run(cmd, capture_output=True).returncode, 0)
            self.assertEqual(output.read_bytes(), original)


if __name__ == '__main__':
    unittest.main()
