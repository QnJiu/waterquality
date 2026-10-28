#!/usr/bin/env python3
"""Create a TPU1 P-256 signed manifest for the reference upgrade protocol."""
import argparse
import getpass
import hashlib
import pathlib
import struct
import sys

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, utils


def create_manifest(image, product, version, bank, key):
    if not image or len(image) > 0xffffffff:
        raise ValueError("image size must be 1..4294967295 bytes")
    if not 1 <= product <= 0xffffffff or not 1 <= version <= 0xffffffff or bank not in (0, 1):
        raise ValueError("product and version must be nonzero uint32, bank must be 0 or 1")
    if not isinstance(key, ec.EllipticCurvePrivateKey) or not isinstance(key.curve, ec.SECP256R1):
        raise ValueError("private key must be a P-256 EC key")
    body = struct.pack('<4sIIIB3x32s', b'TPU1', product, version, len(image), bank,
                       hashlib.sha256(image).digest())
    der = key.sign(body, ec.ECDSA(hashes.SHA256()))
    r, s = utils.decode_dss_signature(der)
    return body + r.to_bytes(32, 'big') + s.to_bytes(32, 'big')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image', type=pathlib.Path, required=True)
    parser.add_argument('--key', type=pathlib.Path, required=True, help='PEM P-256 private key')
    parser.add_argument('--encrypted-key', action='store_true', help='prompt for PEM password')
    parser.add_argument('--product', type=lambda x: int(x, 0), required=True)
    parser.add_argument('--version', type=lambda x: int(x, 0), required=True)
    parser.add_argument('--bank', type=int, choices=(0, 1), required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True, help='new manifest file; never overwritten')
    args = parser.parse_args()
    try:
        password = getpass.getpass('Private key password: ').encode() if args.encrypted_key else None
        key = serialization.load_pem_private_key(args.key.read_bytes(), password=password)
        manifest = create_manifest(args.image.read_bytes(), args.product, args.version, args.bank, key)
        with args.output.open('xb') as output:
            output.write(manifest)
        print('Manifest ID: ' + hashlib.sha256(manifest[:52]).hexdigest())
        return 0
    except (ValueError, TypeError, OSError) as exc:
        print('Manifest creation failed: %s' % exc, file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
