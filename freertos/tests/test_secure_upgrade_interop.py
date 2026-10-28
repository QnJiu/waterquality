import ctypes
import hashlib
import os
import pathlib
import sys
import unittest

from cryptography.hazmat.primitives.asymmetric import ec
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools'))
import sign_manifest as signing
import upgrade_client as u


class Transport:
    def __init__(self, lib):
        self.lib = lib
        self.response = b''
    def write(self, data):
        incoming = ctypes.create_string_buffer(data)
        outgoing = ctypes.create_string_buffer(120)
        n = self.lib.sim_secure_exchange(incoming, len(data), outgoing, len(outgoing))
        if n < 0:
            raise OSError('frame failed')
        self.response = outgoing.raw[:n]
    def read(self, timeout):
        result, self.response = self.response, b''
        return result


class SecureUpgradeTests(unittest.TestCase):
    def setUp(self):
        path = os.environ.get('TP_SECURE_SIM_LIB')
        if not path:
            self.skipTest('set TP_SECURE_SIM_LIB')
        self.lib = ctypes.CDLL(path)
        self.lib.sim_secure_init.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t]
        self.lib.sim_secure_exchange.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p, ctypes.c_size_t]
        self.lib.sim_secure_confirm.argtypes = [ctypes.c_uint8]
        key = ec.generate_private_key(ec.SECP256R1())
        point = key.public_key().public_numbers()
        public = point.x.to_bytes(32, 'big') + point.y.to_bytes(32, 'big')
        old_image = b'factory image'
        old_manifest = signing.create_manifest(old_image, 0x12345678, 1, 0, key)
        self.image = bytes(range(256)) * 3
        self.manifest = signing.create_manifest(self.image, 0x12345678, 2, 1, key)
        self.manifest_id = hashlib.sha256(self.manifest[:52]).digest()
        self.assertEqual(self.lib.sim_secure_init(public, old_manifest, old_image, len(old_image)), 0)

    def upload(self, image=None, manifest=None):
        return u.upload(u.Client(Transport(self.lib), 1), manifest or self.manifest,
                        self.manifest_id, self.image if image is None else image)

    def test_upgrade_reboot_and_confirm(self):
        state = self.upload()
        self.assertEqual((state['phase'], state['written']), (3, len(self.image)))
        self.assertEqual((self.lib.sim_secure_erases(), self.lib.sim_secure_writes()), (1, 3))
        self.assertEqual(self.lib.sim_secure_reboot(), 0)
        self.assertEqual(self.lib.sim_secure_select(), 1)
        self.assertEqual(self.lib.sim_secure_confirm(1), u.OK)
        self.assertEqual(self.lib.sim_secure_reboot(), 0)
        self.assertEqual(self.lib.sim_secure_select(), 1)

    def test_unconfirmed_reverts(self):
        self.upload()
        self.assertEqual(self.lib.sim_secure_reboot(), 0)
        self.assertEqual(self.lib.sim_secure_select(), 1)
        self.assertEqual(self.lib.sim_secure_reboot(), 0)
        self.assertEqual(self.lib.sim_secure_select(), 0)

    def test_tampered_manifest_and_image(self):
        bad = bytearray(self.manifest); bad[-1] ^= 1
        with self.assertRaisesRegex(u.ProtocolError, 'begin failed'):
            self.upload(manifest=bytes(bad))
        self.assertEqual(self.lib.sim_secure_erases(), 0)
        bad_image = bytearray(self.image); bad_image[0] ^= 1
        with self.assertRaisesRegex(u.ProtocolError, 'finish failed'):
            self.upload(image=bytes(bad_image))
        self.assertEqual(self.lib.sim_secure_select(), 0)

    def test_corrupt_staged_content_reverts(self):
        self.upload()
        self.lib.sim_secure_corrupt_manifest(1)
        self.assertEqual(self.lib.sim_secure_reboot(), 0)
        self.assertEqual(self.lib.sim_secure_select(), 0)

    def test_corrupt_staged_image_reverts(self):
        self.upload()
        self.lib.sim_secure_corrupt_image(1)
        self.assertEqual(self.lib.sim_secure_reboot(), 0)
        self.assertEqual(self.lib.sim_secure_select(), 0)
    def test_reauthenticate_after_interrupted_download(self):
        client = u.Client(Transport(self.lib), 1)
        self.assertEqual(client.command(u.BEGIN, self.manifest)[0], u.OK)
        self.assertEqual(client.command(u.WRITE, bytes(4) + self.image[:256])[0], u.OK)
        self.assertEqual(self.lib.sim_secure_reboot(), 0)
        state = self.upload()
        self.assertEqual(state['phase'], 3)
        self.assertEqual((self.lib.sim_secure_erases(), self.lib.sim_secure_writes()), (1, 3))
        self.assertEqual(self.lib.sim_secure_select(), 1)


if __name__ == '__main__':
    unittest.main()
