#!/usr/bin/env python3
"""Reports how much of the 32 KiB ROM a build uses, from the linker map.

The ROM image is always padded to 32 KiB, so the unused part is measured
as the trailing filler (0xFF) after the last area the map lists.

  python3 tools/romusage.py build/kakuro.gb build/kakuro.map
"""
import re
import sys
from pathlib import Path

ROM_SIZE = 32 * 1024


def main() -> int:
    rom = Path(sys.argv[1]).read_bytes()
    if len(rom) != ROM_SIZE:
        print(f"ROM is {len(rom)} bytes, expected {ROM_SIZE}")
        return 1
    end = 0
    text = Path(sys.argv[2]).read_text(errors="replace") if len(sys.argv) > 2 else ""
    for m in re.finditer(r"^(_\w+)\s+([0-9A-F]{8})\s+([0-9A-F]{8})", text, re.M):
        start, size = int(m.group(2), 16), int(m.group(3), 16)
        if start < ROM_SIZE and size:
            end = max(end, start + size)
    if not end:
        end = len(rom.rstrip(b"\xff"))
    print(f"ROM: {end} of {ROM_SIZE} bytes used ({ROM_SIZE - end} free)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
