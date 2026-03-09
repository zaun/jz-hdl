#!/usr/bin/env python3
"""Convert a raw binary file to a hex file (one 32-bit word per line).

Usage: bin2hex.py input.bin output.hex [total_words]

The binary is read as little-endian 32-bit words (RV32I native byte order).
Pads with zeros to total_words (default 1024).
"""

import sys
import struct

def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} input.bin output.hex [total_words]")
        sys.exit(1)

    infile = sys.argv[1]
    outfile = sys.argv[2]
    total_words = int(sys.argv[3]) if len(sys.argv) > 3 else 1024

    with open(infile, "rb") as f:
        data = f.read()

    # Pad to word boundary
    while len(data) % 4 != 0:
        data += b'\x00'

    num_words = len(data) // 4
    if num_words > total_words:
        print(f"ERROR: binary has {num_words} words, exceeds ROM size {total_words}")
        sys.exit(1)

    words = struct.unpack(f"<{num_words}I", data)

    with open(outfile, "w") as f:
        for w in words:
            f.write(f"{w:08X}\n")
        # Pad remaining with zeros
        for _ in range(total_words - num_words):
            f.write("00000000\n")

    print(f"{infile} -> {outfile}: {num_words} words used / {total_words} total")

if __name__ == "__main__":
    main()
