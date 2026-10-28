#!/usr/bin/env python3
"""Check a linked F407 OTA slot ELF and its raw binary; does not authenticate it."""
import argparse
from pathlib import Path
import re
import struct
import subprocess

BASES = (0x08020000, 0x08080000)
CAPACITY = 0x60000


def check(elf: Path, image: Path, slot: int, objdump: str) -> None:
    base = BASES[slot]
    data = image.read_bytes()
    if not 8 <= len(data) <= CAPACITY:
        raise ValueError("binary size outside image slot")
    sp, entry = struct.unpack_from("<II", data)
    if sp & 7 or not 0x20000000 < sp <= 0x20020000:
        raise ValueError("invalid initial stack")
    if not entry & 1 or not base <= (entry & ~1) <= base + len(data) - 2:
        raise ValueError("reset entry outside binary or not Thumb")
    text = subprocess.check_output([objdump, "-h", str(elf)], text=True)
    lines = text.splitlines()
    sections = []
    for i, line in enumerate(lines):
        m = re.match(r"\s*\d+\s+(\S+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+", line)
        if not m or i + 1 >= len(lines):
            continue
        flags = set(lines[i + 1].strip().split(", "))
        if {"CONTENTS", "ALLOC", "LOAD"} <= flags:
            name = m.group(1)
            size, vma, lma = (int(m.group(j), 16) for j in (2, 3, 4))
            if not base <= lma or lma + size > base + CAPACITY:
                raise ValueError(f"load section {name} outside slot")
            sections.append((name, size, vma, lma))
    if not sections or not any(n == ".isr_vector" and v == base and l == base and s >= 8 for n, s, v, l in sections):
        raise ValueError("vector table missing or at wrong slot")
    if max(l + s for _, s, _, l in sections) != base + len(data):
        raise ValueError("ELF load span does not match binary size")
    print(f"slot {slot}: base=0x{base:08x} bytes={len(data)} stack=0x{sp:08x} entry=0x{entry:08x}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("slot", type=int, choices=(0, 1))
    parser.add_argument("elf", type=Path)
    parser.add_argument("binary", type=Path)
    parser.add_argument("--objdump", default="arm-none-eabi-objdump")
    args = parser.parse_args()
    check(args.elf, args.binary, args.slot, args.objdump)


if __name__ == "__main__":
    main()
