#!/usr/bin/env python3
"""Compile font.json into font bank .mem files and compressed ROM.

Bank files (font_b0.mem .. font_b29.mem):
  30 banks of 2048 x 8-bit entries for the font_ram module (BSRAM-based).

Compressed ROM (font_rom0.mem .. font_rom7.mem + font_size.txt):
  Two-layer compressed byte stream for SDRAM decompression at startup.
  Flat byte order: bank 0 offset 0 row 0..15, bank 0 offset 1 row 0..15, ...

  Layer 1 - ZeroGlyph encoding (applied first to raw font data):
    Byte 0x00 + count N (1-255) = N consecutive all-zero 16-byte glyphs
    Byte 0x00 + 0x00 = escape, next 16 bytes are a literal glyph starting with 0x00
    Any byte 0x01-0xFF = start of a literal 16-byte glyph (read 15 more bytes)

  Layer 2 - LZSS with lazy matching (applied to ZeroGlyph output):
    Flag byte: 8 bits, MSB first. 1=literal, 0=reference.
    Literal: 1 raw byte.
    Reference: 2 bytes {offset[11:4], offset[3:0] | (length-3)[3:0]}
    Sliding window: 4096 bytes, match length 3-18.
    Lazy: if pos+1 has a longer match, emit literal and take the longer match.

Glyph index = {bank[4:0], char_code[6:0]}
Address within bank = {glyph_idx[6:0], row[3:0]} = (code & 0x7F) * 16 + row

Each .mem line is an 8-digit binary string (one byte in 0/1 format).
"""

import json
import sys
import os

NUM_BANKS = 32
ENTRIES_PER_BANK = 2048

# LZSS parameters
WINDOW_SIZE = 4096      # 12-bit offset
MAX_MATCH = 18          # 4-bit length + 3
MIN_MATCH = 3

# Map each 128-aligned Unicode range to a bank number
# key = range_base >> 7, value = bank number
RANGE_TO_BANK = {
    0x0000 >> 7:  0,   # U+0000-007F  ASCII
    0x0080 >> 7:  1,   # U+0080-00FF  Latin-1
    0x0100 >> 7:  2,   # U+0100-017F  Latin Ext-A
    0x0180 >> 7:  3,   # U+0180-01FF  Latin Ext-B lo
    0x0200 >> 7:  4,   # U+0200-027F  Latin Ext-B hi
    0x0300 >> 7:  5,   # U+0300-037F  Combining + Greek start
    0x0380 >> 7:  6,   # U+0380-03FF  Greek main
    0x0400 >> 7:  7,   # U+0400-047F  Cyrillic lo
    0x0480 >> 7:  8,   # U+0480-04FF  Cyrillic hi
    0x0500 >> 7:  9,   # U+0500-057F  Cyrillic Supplement
    0x2000 >> 7: 10,   # U+2000-207F  General Punctuation
    0x2080 >> 7: 11,   # U+2080-20FF  Super/Sub + Currency
    0x2100 >> 7: 12,   # U+2100-217F  Number Forms lo
    0x2180 >> 7: 13,   # U+2180-21FF  Number Forms hi + Arrows
    0x2200 >> 7: 14,   # U+2200-227F  Math Operators lo
    0x2280 >> 7: 15,   # U+2280-22FF  Math Operators hi
    0x2500 >> 7: 16,   # U+2500-257F  Box Drawing
    0x2580 >> 7: 17,   # U+2580-25FF  Block Elements
    # Latin Extended Additional
    0x1E00 >> 7: 22,   # U+1E00-1E7F  Latin Ext Additional lo
    0x1E80 >> 7: 23,   # U+1E80-1EFF  Latin Ext Additional hi
    # Braille
    0x2800 >> 7: 24,   # U+2800-287F  Braille lo
    0x2880 >> 7: 25,   # U+2880-28FF  Braille hi
    # Wide char banks (left halves)
    0x3000 >> 7: 18,   # U+3000-307F  CJK Punctuation + Hiragana start (left)
    0x3080 >> 7: 19,   # U+3080-30FF  Hiragana end + Katakana (left)
    # Kanji internal (left halves, remapped from CJK U+4E00-9FFF)
    0x4000 >> 7: 26,   # U+4000-407F  Kanji left lo
    0x4080 >> 7: 27,   # U+4080-40FF  Kanji left hi
    # Emoji internal (left halves, remapped from various emoji codepoints)
    0x5000 >> 7: 30,   # U+5000-507F  Emoji left
}

# Right-half banks for wide characters (maps same ranges to different banks)
RANGE_TO_BANK_RIGHT = {
    0x3000 >> 7: 20,   # U+3000-307F  CJK Punctuation + Hiragana start (right)
    0x3080 >> 7: 21,   # U+3080-30FF  Hiragana end + Katakana (right)
    # Kanji internal (right halves; at runtime, terminal sets bit 15 → U+C0xx)
    0x4000 >> 7: 28,   # U+4000-407F  Kanji right lo
    0x4080 >> 7: 29,   # U+4080-40FF  Kanji right hi
    # Emoji internal (right halves; at runtime, terminal sets bit 15 → U+D0xx)
    0x5000 >> 7: 31,   # U+5000-507F  Emoji right
}

def unicode_to_bank_and_offset(code):
    """Map Unicode code point to (bank, offset) or None if unmapped."""
    range_key = code >> 7
    bank = RANGE_TO_BANK.get(range_key)
    if bank is None:
        return None
    offset = code & 0x7F
    return (bank, offset)


def lzss_compress(data):
    """LZSS compress a byte array.

    Returns compressed byte array.
    Format: groups of (flag_byte, up to 8 tokens).
    Flag bit 1 = literal byte, 0 = (offset, length) reference pair.
    Reference: 2 bytes, offset[11:4], offset[3:0]|len_minus3[3:0].
    """
    out = bytearray()
    pos = 0
    n = len(data)

    while pos < n:
        flag_byte = 0
        tokens = bytearray()
        for bit in range(8):
            if pos >= n:
                # Pad remaining bits as literals of 0x00
                flag_byte |= (1 << (7 - bit))
                tokens.append(0x00)
                continue

            # Search for best match in sliding window
            best_offset = 0
            best_length = 0

            search_start = max(0, pos - WINDOW_SIZE)
            max_len = min(MAX_MATCH, n - pos)

            for i in range(search_start, pos):
                length = 0
                while length < max_len and data[i + length] == data[pos + length]:
                    length += 1
                    # Allow matching into the lookahead (RLE-style)
                    if i + length >= pos:
                        # For overlapping matches, the source wraps
                        break
                if length > best_length:
                    best_length = length
                    best_offset = pos - i

            if best_length >= MIN_MATCH:
                # Reference: 0-bit
                # Encode offset (1-4096) and length (3-18)
                off = best_offset - 1  # 0-4095
                ln = best_length - MIN_MATCH  # 0-15
                tokens.append((off >> 4) & 0xFF)
                tokens.append(((off & 0x0F) << 4) | (ln & 0x0F))
                pos += best_length
            else:
                # Literal: 1-bit
                flag_byte |= (1 << (7 - bit))
                tokens.append(data[pos])
                pos += 1

        out.append(flag_byte)
        out.extend(tokens)

    return bytes(out)


def lzss_decompress(data):
    """LZSS decompress for verification."""
    out = bytearray()
    pos = 0
    n = len(data)

    while pos < n:
        flag_byte = data[pos]
        pos += 1

        for bit in range(8):
            if pos >= n:
                break

            if flag_byte & (1 << (7 - bit)):
                # Literal
                out.append(data[pos])
                pos += 1
            else:
                # Reference
                if pos + 1 >= n:
                    break
                b0 = data[pos]
                b1 = data[pos + 1]
                pos += 2
                offset = (b0 << 4) | (b1 >> 4)
                offset += 1  # 1-4096
                length = (b1 & 0x0F) + MIN_MATCH

                for _ in range(length):
                    out.append(out[-offset])

    return bytes(out)


def _find_best_match(data, pos, n):
    """Find best match at pos in sliding window."""
    best_offset = 0
    best_length = 0
    search_start = max(0, pos - WINDOW_SIZE)
    max_len = min(MAX_MATCH, n - pos)
    for i in range(search_start, pos):
        length = 0
        while length < max_len and data[i + length] == data[pos + length]:
            length += 1
            if i + length >= pos:
                break
        if length > best_length:
            best_length = length
            best_offset = pos - i
    return best_offset, best_length


def lzss_compress_lazy(data):
    """LZSS compress with lazy matching.

    At each position, if a match is found, check if pos+1 has a longer match.
    If so, emit a literal for the current byte and use the longer match instead.
    Same format as lzss_compress.
    """
    out = bytearray()
    pos = 0
    n = len(data)

    while pos < n:
        flag_byte = 0
        tokens = bytearray()
        for bit in range(8):
            if pos >= n:
                flag_byte |= (1 << (7 - bit))
                tokens.append(0x00)
                continue

            best_offset, best_length = _find_best_match(data, pos, n)

            if best_length >= MIN_MATCH:
                # Lazy: check if next position has a better match
                if pos + 1 < n:
                    next_offset, next_length = _find_best_match(data, pos + 1, n)
                    if next_length > best_length + 1:
                        # Better to emit literal now, take longer match next
                        flag_byte |= (1 << (7 - bit))
                        tokens.append(data[pos])
                        pos += 1
                        continue

                off = best_offset - 1
                ln = best_length - MIN_MATCH
                tokens.append((off >> 4) & 0xFF)
                tokens.append(((off & 0x0F) << 4) | (ln & 0x0F))
                pos += best_length
            else:
                flag_byte |= (1 << (7 - bit))
                tokens.append(data[pos])
                pos += 1

        out.append(flag_byte)
        out.extend(tokens)

    return bytes(out)


def zero_glyph_compress(data, glyph_size=16):
    """Replace all-zero glyphs with a single marker token.

    Format: 0x00 marker + count byte = N consecutive zero glyphs (N*16 zero bytes).
    Non-zero glyphs pass through as-is (16 raw bytes).
    A single zero glyph costs 2 bytes instead of 16 (87.5% savings per empty glyph).
    """
    out = bytearray()
    pos = 0
    n = len(data)
    while pos + glyph_size <= n:
        # Check if current glyph is all zeros
        if data[pos:pos + glyph_size] == b'\x00' * glyph_size:
            # Count consecutive zero glyphs (max 255)
            count = 0
            while (pos + glyph_size <= n
                   and count < 255
                   and data[pos:pos + glyph_size] == b'\x00' * glyph_size):
                count += 1
                pos += glyph_size
            out.append(0x00)
            out.append(count)
        else:
            # Check if first byte is 0x00 — need escape
            if data[pos] == 0x00:
                # Emit escape: 0x00 0x00 means "zero zero-glyphs" = next glyph is literal
                out.append(0x00)
                out.append(0x00)
            out.extend(data[pos:pos + glyph_size])
            pos += glyph_size
    # Trailing partial glyph (shouldn't happen with aligned data)
    if pos < n:
        out.extend(data[pos:])
    return bytes(out)


def zero_glyph_decompress(data, glyph_size=16):
    """Decompress zero-glyph encoded data."""
    out = bytearray()
    pos = 0
    n = len(data)
    while pos < n:
        if data[pos] == 0x00:
            pos += 1
            if pos >= n:
                break
            count = data[pos]
            pos += 1
            if count == 0:
                # Escape: next glyph_size bytes are a literal glyph starting with 0x00
                if pos + glyph_size <= n:
                    out.extend(data[pos:pos + glyph_size])
                    pos += glyph_size
            else:
                # N zero glyphs
                out.extend(b'\x00' * (count * glyph_size))
        else:
            # Literal glyph (first byte is non-zero, so no ambiguity)
            out.extend(data[pos:pos + glyph_size])
            pos += glyph_size
    return bytes(out)


def next_power_of_2(n):
    """Return the smallest power of 2 >= n."""
    if n <= 1:
        return 1
    p = 1
    while p < n:
        p <<= 1
    return p


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    out_dir = os.path.join(script_dir, "..", "out")
    os.makedirs(out_dir, exist_ok=True)
    font_path = os.path.join(script_dir, "font.json")

    with open(font_path) as f:
        glyphs = json.load(f)

    # Build banks
    banks = [["00000000"] * ENTRIES_PER_BANK for _ in range(NUM_BANKS)]
    mapped = 0
    skipped = 0

    for g in glyphs:
        code = g["code"]
        rows = g["bitmap"]
        if len(rows) != 16:
            print(f"Error: char U+{code:04X} has {len(rows)} rows (expected 16)", file=sys.stderr)
            sys.exit(1)

        row_width = len(rows[0])
        if row_width == 16:
            # Wide glyph: split into left half (banks 18-19) and right half (banks 20-21)
            range_key = code >> 7
            left_bank = RANGE_TO_BANK.get(range_key)
            right_bank = RANGE_TO_BANK_RIGHT.get(range_key)
            if left_bank is None or right_bank is None:
                print(f"Warning: wide char U+{code:04X} has no bank mapping, skipping", file=sys.stderr)
                skipped += 1
                continue

            offset = code & 0x7F
            base = offset * 16

            for row in range(16):
                bits = rows[row]
                if len(bits) != 16 or not all(b in "01" for b in bits):
                    print(f"Error: U+{code:04X} row {row} invalid wide bitmap '{bits}'", file=sys.stderr)
                    sys.exit(1)
                left_byte = int(bits[0:8], 2)
                right_byte = int(bits[8:16], 2)
                banks[left_bank][base + row] = f"{left_byte:08b}"
                banks[right_bank][base + row] = f"{right_byte:08b}"
            mapped += 1
        elif row_width == 8:
            # Normal narrow glyph
            result = unicode_to_bank_and_offset(code)
            if result is None:
                print(f"Warning: char U+{code:04X} has no bank mapping, skipping", file=sys.stderr)
                skipped += 1
                continue

            bank, offset = result
            base = offset * 16

            for row in range(16):
                bits = rows[row]
                if len(bits) != 8 or not all(b in "01" for b in bits):
                    print(f"Error: U+{code:04X} row {row} invalid bitmap '{bits}'", file=sys.stderr)
                    sys.exit(1)
                banks[bank][base + row] = bits
            mapped += 1
        else:
            print(f"Error: U+{code:04X} row width {row_width} (expected 8 or 16)", file=sys.stderr)
            sys.exit(1)

    # Write bank .mem files
    for i in range(NUM_BANKS):
        name = f"font_b{i}.mem"
        out_path = os.path.join(out_dir, name)
        with open(out_path, "w") as f:
            for entry in banks[i]:
                f.write(entry + "\n")

    # Build flat byte array for compression
    # Order: bank 0 offset 0 rows 0-15, bank 0 offset 1 rows 0-15, ...
    # This is glyph_idx order: glyph_idx = bank * 128 + offset
    flat = bytearray()
    for bank in range(NUM_BANKS):
        for offset in range(128):
            base = offset * 16
            for row in range(16):
                flat.append(int(banks[bank][base + row], 2))

    total_bytes = len(flat)
    assert total_bytes == NUM_BANKS * 128 * 16  # 36864

    # Two-layer compression: ZeroGlyph encoding then LZSS with lazy matching
    zg_encoded = zero_glyph_compress(flat)
    compressed = lzss_compress_lazy(zg_encoded)

    # Verify round-trip (LZSS decompress then ZeroGlyph decompress)
    lzss_decompressed = lzss_decompress(compressed)
    decompressed = zero_glyph_decompress(lzss_decompressed[:len(zg_encoded)])
    if decompressed != flat:
        print("ERROR: ZeroGlyph+LZSS round-trip verification failed!", file=sys.stderr)
        for i in range(min(len(decompressed), total_bytes)):
            if decompressed[i] != flat[i]:
                print(f"  First mismatch at byte {i}: expected 0x{flat[i]:02X}, got 0x{decompressed[i]:02X}", file=sys.stderr)
                break
        sys.exit(1)

    # Build unified char_lut.mem (kanji + emoji, sorted by codepoint, 24-bit)
    kanji_lut_path = os.path.join(out_dir, "kanji_lut.mem")
    emoji_lut_path = os.path.join(out_dir, "emoji_lut.mem")
    lut_entries = []  # list of (unicode_cp_24bit,)
    if os.path.exists(kanji_lut_path):
        with open(kanji_lut_path) as f:
            for line in f:
                val = int(line.strip(), 2)
                if val != 0xFFFF:
                    lut_entries.append(val)
    kanji_count = len(lut_entries)
    if os.path.exists(emoji_lut_path):
        with open(emoji_lut_path) as f:
            for line in f:
                val = int(line.strip(), 2)
                if val != 0xFFFFFF:
                    lut_entries.append(val)
    # Sort all entries by codepoint
    lut_entries.sort()
    # Write unified char_lut.mem (512 entries, 24-bit binary)
    char_lut_path = os.path.join(out_dir, "char_lut.mem")
    with open(char_lut_path, "w") as f:
        for i in range(512):
            if i < len(lut_entries):
                f.write(f"{lut_entries[i]:024b}\n")
            else:
                f.write("111111111111111111111111\n")
    print(f"Generated unified char_lut.mem: {len(lut_entries)} entries ({kanji_count} kanji + {len(lut_entries) - kanji_count} emoji)")

    comp_size = len(compressed)
    BANK_SIZE = 2048
    NUM_ROM_BANKS = (comp_size + BANK_SIZE - 1) // BANK_SIZE  # ceil division
    rom_size = NUM_ROM_BANKS * BANK_SIZE

    # Write compressed .mem file (padded to bank boundary)
    comp_path = os.path.join(out_dir, "font_compressed.mem")
    with open(comp_path, "w") as f:
        for i in range(rom_size):
            if i < comp_size:
                f.write(f"{compressed[i]:08b}\n")
            else:
                f.write("00000000\n")

    # Write ROM bank files (N banks of 2048 bytes each for BSRAM)
    for b in range(NUM_ROM_BANKS):
        bank_path = os.path.join(out_dir, f"font_rom{b}.mem")
        with open(bank_path, "w") as f:
            for i in range(BANK_SIZE):
                idx = b * BANK_SIZE + i
                if idx < comp_size:
                    f.write(f"{compressed[idx]:08b}\n")
                else:
                    f.write("00000000\n")

    # Write size file
    size_path = os.path.join(out_dir, "font_size.txt")
    with open(size_path, "w") as f:
        f.write(f"{comp_size}\n")
        f.write(f"{rom_size}\n")
        f.write(f"{total_bytes}\n")

    # Summary
    bsram_blocks = (rom_size + 2047) // 2048  # 2KB per BSRAM block
    print(f"Compiled {mapped} glyphs into {NUM_BANKS} banks ({skipped} skipped)")
    for i in range(NUM_BANKS):
        nonzero = sum(1 for e in banks[i] if e != "00000000")
        print(f"  Bank {i:2d} (font_b{i}.mem): {nonzero} non-zero entries")
    compression_ratio = total_bytes / comp_size
    savings_pct = (1 - comp_size / total_bytes) * 100
    print(f"\nZeroGlyph + LZSS lazy compression:")
    print(f"  Uncompressed:  {total_bytes:,} bytes")
    print(f"  ZeroGlyph:     {len(zg_encoded):,} bytes")
    print(f"  Compressed:    {comp_size:,} bytes")
    print(f"  Ratio:         {compression_ratio:.2f}:1 ({savings_pct:.1f}% smaller)")
    print(f"  ROM size:      {rom_size:,} bytes ({NUM_ROM_BANKS} banks)")
    print(f"  BSRAM blocks:  {bsram_blocks} (for compressed ROM)")

if __name__ == "__main__":
    main()
