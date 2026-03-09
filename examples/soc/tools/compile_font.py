#!/usr/bin/env python3
"""Compile a JSON font file to Verilog $readmemh format.

Reads glyph bitmaps from JSON (source of truth) and outputs a hex file
for $readmemh initialization of the font ROM.

Usage:
    python3 compile_font.py <input.json> <output.hex>
"""

import json
import sys


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input.json> <output.hex>", file=sys.stderr)
        sys.exit(1)

    json_path = sys.argv[1]
    hex_path = sys.argv[2]

    with open(json_path, "r") as f:
        glyphs = json.load(f)

    # Build lookup by code
    by_code = {g["code"]: g for g in glyphs}

    # Determine glyph dimensions from first glyph
    first = glyphs[0]
    glyph_h = len(first["bitmap"])
    glyph_w = len(first["bitmap"][0])
    num_glyphs = 256
    hex_digits = (glyph_w + 3) // 4

    total = num_glyphs * glyph_h
    with open(hex_path, "w") as f:
        for char_idx in range(num_glyphs):
            glyph = by_code.get(char_idx)
            for row in range(glyph_h):
                if glyph and row < len(glyph["bitmap"]):
                    val = int(glyph["bitmap"][row], 2)
                else:
                    val = 0
                f.write(f"{val:0{hex_digits}X}\n")

    print(f"  font: {json_path} -> {hex_path} ({total} entries, {glyph_w}x{glyph_h})")


if __name__ == "__main__":
    main()
