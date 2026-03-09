#!/usr/bin/env python3
"""Generate palette.mem — 256-entry xterm color palette (24-bit RGB binary values).

Output: one line per entry, 24 binary digits (0/1), no prefix.
Used by pixel_generator.jz BSRAM palette lookup.
"""

import os
import sys

# Standard ANSI colors (0-7) — matches the hardcoded values in pixel_generator.jz
STANDARD = [
    (0x00, 0x00, 0x00),  # 0  Black
    (0xAA, 0x00, 0x00),  # 1  Red
    (0x00, 0xAA, 0x00),  # 2  Green
    (0xAA, 0x55, 0x00),  # 3  Yellow (brown)
    (0x00, 0x00, 0xAA),  # 4  Blue
    (0xAA, 0x00, 0xAA),  # 5  Magenta
    (0x00, 0xAA, 0xAA),  # 6  Cyan
    (0xAA, 0xAA, 0xAA),  # 7  White
]

# Bright ANSI colors (8-15)
BRIGHT = [
    (0x55, 0x55, 0x55),  # 8   Bright Black (Gray)
    (0xFF, 0x55, 0x55),  # 9   Bright Red
    (0x55, 0xFF, 0x55),  # 10  Bright Green
    (0xFF, 0xFF, 0x55),  # 11  Bright Yellow
    (0x55, 0x55, 0xFF),  # 12  Bright Blue
    (0xFF, 0x55, 0xFF),  # 13  Bright Magenta
    (0x55, 0xFF, 0xFF),  # 14  Bright Cyan
    (0xFF, 0xFF, 0xFF),  # 15  Bright White
]


def generate_palette():
    palette = []

    # 0-7: Standard colors
    palette.extend(STANDARD)

    # 8-15: Bright colors
    palette.extend(BRIGHT)

    # 16-231: 6x6x6 color cube
    levels = [0, 51, 102, 153, 204, 255]  # 0x00, 0x33, 0x66, 0x99, 0xCC, 0xFF
    for r in range(6):
        for g in range(6):
            for b in range(6):
                palette.append((levels[r], levels[g], levels[b]))

    # 232-255: Grayscale ramp (24 entries)
    for i in range(24):
        v = 8 + i * 10
        palette.append((v, v, v))

    return palette


def main():
    palette = generate_palette()
    assert len(palette) == 256, f"Expected 256 entries, got {len(palette)}"

    # Output directory
    out_dir = os.path.join(os.path.dirname(__file__), '..', 'out')
    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir, 'palette.mem')

    with open(out_path, 'w') as f:
        for r, g, b in palette:
            f.write(f"{r:08b}{g:08b}{b:08b}\n")

    print(f"Generated {out_path} ({len(palette)} entries)")


if __name__ == '__main__':
    main()
