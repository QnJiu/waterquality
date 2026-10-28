#!/usr/bin/env python3
"""Check a signed F407 factory image and create the boot identity source."""
import argparse
import hashlib
import pathlib
import struct
import sys

from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, utils


def identity_source(public_key, manifest, image):
    if not isinstance(public_key, ec.EllipticCurvePublicKey) or not isinstance(public_key.curve, ec.SECP256R1):
        raise ValueError('public key must be P-256')
    if len(manifest) != 116:
        raise ValueError('manifest must be 116 bytes')
    magic, product, version, size, bank, digest = struct.unpack('<4sIIIB3x32s', manifest[:52])
    if magic != b'TPU1' or not product or not version or bank not in (0, 1) or manifest[17:20] != b'\0\0\0':
        raise ValueError('invalid manifest fields')
    if size != len(image) or size < 8 or size > 384 * 1024 or hashlib.sha256(image).digest() != digest:
        raise ValueError('image size or hash does not match manifest')
    signature = utils.encode_dss_signature(int.from_bytes(manifest[52:84], 'big'),
                                           int.from_bytes(manifest[84:], 'big'))
    public_key.verify(signature, manifest[:52], ec.ECDSA(hashes.SHA256()))
    sp, entry = struct.unpack_from('<II', image)
    base = (0x08020000, 0x08080000)[bank]
    addr = entry & ~1
    if not (0x20000000 < sp <= 0x20020000 and sp % 8 == 0 and entry & 1 and base <= addr <= base + size - 2):
        raise ValueError('invalid F407 image vector')
    point = public_key.public_numbers()
    raw = point.x.to_bytes(32, 'big') + point.y.to_bytes(32, 'big')
    values = ','.join('0x%02x' % byte for byte in raw)
    return ('#include "tp_f407_boot_identity.h"\n'
            'const uint32_t tp_f407_boot_product_id = %du;\n'
            'const uint8_t tp_f407_boot_public_key[64] = {%s};\n' % (product, values))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('public-key', 'manifest', 'image', 'output'):
        parser.add_argument('--' + name, type=pathlib.Path, required=True)
    args = parser.parse_args()
    try:
        key = serialization.load_pem_public_key(args.public_key.read_bytes())
        source = identity_source(key, args.manifest.read_bytes(), args.image.read_bytes())
        with args.output.open('x', encoding='ascii', newline='\n') as output:
            output.write(source)
        print('Checked image and manifest; created ' + str(args.output))
        return 0
    except (ValueError, TypeError, OSError, InvalidSignature) as exc:
        print('Factory preparation failed: %s' % exc, file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
