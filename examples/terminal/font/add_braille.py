#!/usr/bin/env python3
"""Add Braille pattern glyphs (U+2800-28FF) to font.json.

All 256 Braille patterns are generated algorithmically from the dot encoding.
Each Unicode code point U+2800+N encodes 8 dots in bits 0-7 of N:

  Dot layout:        Bit mapping:
  [1] [4]            bit 0  bit 3
  [2] [5]            bit 1  bit 4
  [3] [6]            bit 2  bit 5
  [7] [8]            bit 6  bit 7

Each dot renders as a 2x2 pixel block in the 8x16 glyph:
  Left column: pixels 2-3    Right column: pixels 5-6
  Row groups: dots 1,4 at rows 2-3; dots 2,5 at rows 5-6;
              dots 3,6 at rows 8-9; dots 7,8 at rows 11-12
"""

import json
import os
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
FONT_PATH = os.path.join(SCRIPT_DIR, "font.json")

# Dot positions: (bit_index, row_start, row_end)
# Left column dots (pixels 2-3)
LEFT_DOTS = [
    (0, 2, 3),   # dot 1
    (1, 5, 6),   # dot 2
    (2, 8, 9),   # dot 3
    (6, 11, 12), # dot 7
]

# Right column dots (pixels 5-6)
RIGHT_DOTS = [
    (3, 2, 3),   # dot 4
    (4, 5, 6),   # dot 5
    (5, 8, 9),   # dot 6
    (7, 11, 12), # dot 8
]

def make_braille_bitmap(code_offset):
    """Generate 8x16 bitmap for Braille pattern U+2800+code_offset."""
    rows = ["00000000"] * 16

    for bit, r_start, r_end in LEFT_DOTS:
        if code_offset & (1 << bit):
            for r in range(r_start, r_end + 1):
                # Set pixels 2-3 (bits 5-4 in MSB-first)
                row = list(rows[r])
                row[2] = '1'
                row[3] = '1'
                rows[r] = ''.join(row)

    for bit, r_start, r_end in RIGHT_DOTS:
        if code_offset & (1 << bit):
            for r in range(r_start, r_end + 1):
                # Set pixels 5-6 (bits 2-1 in MSB-first)
                row = list(rows[r])
                row[5] = '1'
                row[6] = '1'
                rows[r] = ''.join(row)

    return rows


def main():
    with open(FONT_PATH) as f:
        glyphs = json.load(f)

    existing = {g["code"] for g in glyphs}
    added = []

    for offset in range(256):
        code = 0x2800 + offset
        if code in existing:
            continue
        char = chr(code)
        bitmap = make_braille_bitmap(offset)
        added.append({"code": code, "char": char, "bitmap": bitmap})

    if not added:
        print("All Braille glyphs already present.")
        return

    # Merge and sort
    merged = glyphs + added
    merged.sort(key=lambda g: g["code"])

    # Validate
    for g in added:
        assert len(g["bitmap"]) == 16
        for row in g["bitmap"]:
            assert len(row) == 8 and all(c in "01" for c in row)

    with open(FONT_PATH, "w") as f:
        json.dump(merged, f, indent=2, ensure_ascii=False)

    print(f"Added {len(added)} Braille glyphs (U+2800-28FF)")
    print(f"Total glyphs in font.json: {len(merged)}")


if __name__ == "__main__":
    main()
