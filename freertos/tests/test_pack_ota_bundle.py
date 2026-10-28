import hashlib
import pathlib
import struct
import subprocess
import sys
import tempfile
import unittest

root = pathlib.Path(__file__).resolve().parents[1]


class BundleToolTest(unittest.TestCase):
    def test_cli_validates_and_does_not_overwrite(self):
        with tempfile.TemporaryDirectory() as folder:
            folder = pathlib.Path(folder)
            image = b'example image'
            manifest = struct.pack('<4sIIIB3x32s', b'TPU1', 1, 2, len(image), 1,
                                   hashlib.sha256(image).digest()) + bytes(64)
            (folder / 'manifest').write_bytes(manifest)
            (folder / 'image').write_bytes(image)
            args = [sys.executable, str(root / 'tools' / 'pack_ota_bundle.py'),
                    '--manifest', str(folder / 'manifest'), '--image', str(folder / 'image'),
                    '--output', str(folder / 'bundle')]
            first = subprocess.run(args, capture_output=True)
            self.assertEqual(first.returncode, 0, first.stderr)
            self.assertEqual((folder / 'bundle').read_bytes(), manifest + image)
            self.assertNotEqual(subprocess.run(args, capture_output=True).returncode, 0)
            (folder / 'image').write_bytes(b'wrong')
            args[-1] = str(folder / 'bad')
            self.assertNotEqual(subprocess.run(args, capture_output=True).returncode, 0)
            self.assertFalse((folder / 'bad').exists())


if __name__ == '__main__': unittest.main()
