#!/usr/bin/env python3
"""Combine an existing signed TPU1 manifest and image into one OTA file."""
import argparse
from pathlib import Path
from upgrade_client import check_files, MAX_IMAGE_SIZE


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', required=True, type=Path)
    parser.add_argument('--image', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    manifest, image = args.manifest.read_bytes(), args.image.read_bytes()
    if len(image) > MAX_IMAGE_SIZE:
        raise ValueError('image exceeds reference slot size')
    check_files(manifest, image)
    with args.output.open('xb') as target:
        target.write(manifest)
        target.write(image)
    print('Created', args.output, '(%d bytes)' % (len(manifest) + len(image)))


if __name__ == '__main__':
    main()
