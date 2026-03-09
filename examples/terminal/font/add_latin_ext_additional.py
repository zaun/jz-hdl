#!/usr/bin/env python3
"""Add Latin Extended Additional (U+1E00-1EFF) to font.json.

These characters are composed from base Latin letters + diacritical marks.
Uses existing glyphs from font.json as base characters, then overlays
diacritics (dot below, hook above, circumflex+acute, etc.)

Primary coverage: Vietnamese, Welsh, medieval/scholarly Latin.
"""

import json
import os
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
FONT_PATH = os.path.join(SCRIPT_DIR, "font.json")

BLANK = ["00000000"] * 16


def load_font():
    with open(FONT_PATH) as f:
        return json.load(f)


def build_lookup(glyphs):
    return {g["code"]: g["bitmap"] for g in glyphs}


def overlay(base, mark):
    """Overlay mark bitmap onto base bitmap (OR operation)."""
    result = []
    for i in range(16):
        b = int(base[i], 2)
        m = int(mark[i], 2)
        result.append(format(b | m, '08b'))
    return result


def shift_right(bitmap, n):
    """Shift bitmap right by n pixels."""
    result = []
    for row in bitmap:
        val = int(row, 2) >> n
        result.append(format(val & 0xFF, '08b'))
    return result


def shift_left(bitmap, n):
    """Shift bitmap left by n pixels."""
    result = []
    for row in bitmap:
        val = (int(row, 2) << n) & 0xFF
        result.append(format(val, '08b'))
    return result


# Diacritic marks as 8x16 bitmaps (positioned for uppercase/lowercase)

# Dot below - for uppercase (row 13-14)
DOT_BELOW_UC = (["00000000"] * 13 +
                ["00011000",
                 "00011000"] +
                ["00000000"])

# Dot below - for lowercase (row 13-14)
DOT_BELOW_LC = DOT_BELOW_UC

# Hook above - for uppercase (rows 0-1)
HOOK_ABOVE_UC = (["00011000",
                  "00000100",
                  "00011000"] +
                 ["00000000"] * 13)

# Hook above - for lowercase (rows 0-2)
HOOK_ABOVE_LC = HOOK_ABOVE_UC

# Acute above (rows 0-1)
ACUTE_ABOVE = (["00001100",
                "00011000"] +
               ["00000000"] * 14)

# Grave above (rows 0-1)
GRAVE_ABOVE = (["00110000",
                "00011000"] +
               ["00000000"] * 14)

# Tilde above (rows 0-1)
TILDE_ABOVE = (["00110100",
                "00101100"] +
               ["00000000"] * 14)

# Circumflex above (row 0-1)
CIRCUMFLEX_ABOVE = (["00010000",
                     "00101000"] +
                    ["00000000"] * 14)

# Breve above (rows 0-1)
BREVE_ABOVE = (["00100100",
                "00011000"] +
               ["00000000"] * 14)

# Horn (small extension at upper-right, rows 1-3)
HORN = (["00000110",
         "00000010",
         "00000100"] +
        ["00000000"] * 13)

# Cedilla below (rows 13-15)
CEDILLA_BELOW = (["00000000"] * 13 +
                 ["00010000",
                  "00001000",
                  "00011000"])

# Ring above (row 0-1)
RING_ABOVE = (["00011000",
               "00100100",
               "00011000"] +
              ["00000000"] * 13)

# Macron below (row 14)
MACRON_BELOW = (["00000000"] * 14 +
                ["00111100"] +
                ["00000000"])

# Circumflex below (rows 13-14)
CIRCUMFLEX_BELOW = (["00000000"] * 13 +
                    ["00101000",
                     "00010000"] +
                    ["00000000"])

# Line below (row 15)
LINE_BELOW = (["00000000"] * 15 +
              ["01111110"])

# Diaeresis above (row 0)
DIAERESIS_ABOVE = (["00100100"] +
                   ["00000000"] * 15)

# Stroke through (row 7-8)
STROKE_THROUGH = (["00000000"] * 7 +
                  ["01111110"] +
                  ["00000000"] * 8)

# Double acute (rows 0-1)
DOUBLE_ACUTE = (["00010010",
                 "00101100"] +
                ["00000000"] * 14)

# Double grave (rows 0-1)
DOUBLE_GRAVE = (["01001000",
                 "00110100"] +
                ["00000000"] * 14)

# Caron above (rows 0-1)
CARON_ABOVE = (["00101000",
                "00010000"] +
               ["00000000"] * 14)


def make_composed_glyph(base_bmp, *marks):
    """Compose base bitmap with one or more diacritic marks."""
    result = list(base_bmp)
    for mark in marks:
        result = overlay(result, mark)
    return result


def make_latin_ext_additional(E):
    """Generate Latin Extended Additional glyphs.
    E = existing bitmap lookup by code point."""
    new_glyphs = []

    def add(code, char, bitmap):
        new_glyphs.append({"code": code, "char": char, "bitmap": bitmap})

    def compose(code, char, base_code, *marks):
        if base_code in E:
            bmp = make_composed_glyph(E[base_code], *marks)
            add(code, char, bmp)
        else:
            add(code, char, BLANK)

    # Vietnamese vowels with various marks
    # A with diacritics
    compose(0x1EA0, "Ạ", 0x0041, DOT_BELOW_UC)
    compose(0x1EA1, "ạ", 0x0061, DOT_BELOW_LC)
    compose(0x1EA2, "Ả", 0x0041, HOOK_ABOVE_UC)
    compose(0x1EA3, "ả", 0x0061, HOOK_ABOVE_LC)
    compose(0x1EA4, "Ấ", 0x00C2, ACUTE_ABOVE)    # Â + acute
    compose(0x1EA5, "ấ", 0x00E2, ACUTE_ABOVE)
    compose(0x1EA6, "Ầ", 0x00C2, GRAVE_ABOVE)     # Â + grave
    compose(0x1EA7, "ầ", 0x00E2, GRAVE_ABOVE)
    compose(0x1EA8, "Ẩ", 0x00C2, HOOK_ABOVE_UC)   # Â + hook
    compose(0x1EA9, "ẩ", 0x00E2, HOOK_ABOVE_LC)
    compose(0x1EAA, "Ẫ", 0x00C2, TILDE_ABOVE)     # Â + tilde
    compose(0x1EAB, "ẫ", 0x00E2, TILDE_ABOVE)
    compose(0x1EAC, "Ậ", 0x00C2, DOT_BELOW_UC)    # Â + dot below
    compose(0x1EAD, "ậ", 0x00E2, DOT_BELOW_LC)
    compose(0x1EAE, "Ắ", 0x0102, ACUTE_ABOVE)     # Ă + acute
    compose(0x1EAF, "ắ", 0x0103, ACUTE_ABOVE)
    compose(0x1EB0, "Ằ", 0x0102, GRAVE_ABOVE)     # Ă + grave
    compose(0x1EB1, "ằ", 0x0103, GRAVE_ABOVE)
    compose(0x1EB2, "Ẳ", 0x0102, HOOK_ABOVE_UC)   # Ă + hook
    compose(0x1EB3, "ẳ", 0x0103, HOOK_ABOVE_LC)
    compose(0x1EB4, "Ẵ", 0x0102, TILDE_ABOVE)     # Ă + tilde
    compose(0x1EB5, "ẵ", 0x0103, TILDE_ABOVE)
    compose(0x1EB6, "Ặ", 0x0102, DOT_BELOW_UC)    # Ă + dot below
    compose(0x1EB7, "ặ", 0x0103, DOT_BELOW_LC)

    # E with diacritics
    compose(0x1EB8, "Ẹ", 0x0045, DOT_BELOW_UC)
    compose(0x1EB9, "ẹ", 0x0065, DOT_BELOW_LC)
    compose(0x1EBA, "Ẻ", 0x0045, HOOK_ABOVE_UC)
    compose(0x1EBB, "ẻ", 0x0065, HOOK_ABOVE_LC)
    compose(0x1EBC, "Ẽ", 0x0045, TILDE_ABOVE)
    compose(0x1EBD, "ẽ", 0x0065, TILDE_ABOVE)
    compose(0x1EBE, "Ế", 0x00CA, ACUTE_ABOVE)     # Ê + acute
    compose(0x1EBF, "ế", 0x00EA, ACUTE_ABOVE)
    compose(0x1EC0, "Ề", 0x00CA, GRAVE_ABOVE)     # Ê + grave
    compose(0x1EC1, "ề", 0x00EA, GRAVE_ABOVE)
    compose(0x1EC2, "Ể", 0x00CA, HOOK_ABOVE_UC)   # Ê + hook
    compose(0x1EC3, "ể", 0x00EA, HOOK_ABOVE_LC)
    compose(0x1EC4, "Ễ", 0x00CA, TILDE_ABOVE)     # Ê + tilde
    compose(0x1EC5, "ễ", 0x00EA, TILDE_ABOVE)
    compose(0x1EC6, "Ệ", 0x00CA, DOT_BELOW_UC)    # Ê + dot below
    compose(0x1EC7, "ệ", 0x00EA, DOT_BELOW_LC)

    # I with diacritics
    compose(0x1EC8, "Ỉ", 0x0049, HOOK_ABOVE_UC)
    compose(0x1EC9, "ỉ", 0x0069, HOOK_ABOVE_LC)
    compose(0x1ECA, "Ị", 0x0049, DOT_BELOW_UC)
    compose(0x1ECB, "ị", 0x0069, DOT_BELOW_LC)

    # O with diacritics
    compose(0x1ECC, "Ọ", 0x004F, DOT_BELOW_UC)
    compose(0x1ECD, "ọ", 0x006F, DOT_BELOW_LC)
    compose(0x1ECE, "Ỏ", 0x004F, HOOK_ABOVE_UC)
    compose(0x1ECF, "ỏ", 0x006F, HOOK_ABOVE_LC)
    compose(0x1ED0, "Ố", 0x00D4, ACUTE_ABOVE)     # Ô + acute
    compose(0x1ED1, "ố", 0x00F4, ACUTE_ABOVE)
    compose(0x1ED2, "Ồ", 0x00D4, GRAVE_ABOVE)     # Ô + grave
    compose(0x1ED3, "ồ", 0x00F4, GRAVE_ABOVE)
    compose(0x1ED4, "Ổ", 0x00D4, HOOK_ABOVE_UC)   # Ô + hook
    compose(0x1ED5, "ổ", 0x00F4, HOOK_ABOVE_LC)
    compose(0x1ED6, "Ỗ", 0x00D4, TILDE_ABOVE)     # Ô + tilde
    compose(0x1ED7, "ỗ", 0x00F4, TILDE_ABOVE)
    compose(0x1ED8, "Ộ", 0x00D4, DOT_BELOW_UC)    # Ô + dot below
    compose(0x1ED9, "ộ", 0x00F4, DOT_BELOW_LC)

    # O with horn + diacritics (Vietnamese Ơ)
    compose(0x1EDA, "Ớ", 0x004F, HORN, ACUTE_ABOVE)
    compose(0x1EDB, "ớ", 0x006F, HORN, ACUTE_ABOVE)
    compose(0x1EDC, "Ờ", 0x004F, HORN, GRAVE_ABOVE)
    compose(0x1EDD, "ờ", 0x006F, HORN, GRAVE_ABOVE)
    compose(0x1EDE, "Ở", 0x004F, HORN, HOOK_ABOVE_UC)
    compose(0x1EDF, "ở", 0x006F, HORN, HOOK_ABOVE_LC)
    compose(0x1EE0, "Ỡ", 0x004F, HORN, TILDE_ABOVE)
    compose(0x1EE1, "ỡ", 0x006F, HORN, TILDE_ABOVE)
    compose(0x1EE2, "Ợ", 0x004F, HORN, DOT_BELOW_UC)
    compose(0x1EE3, "ợ", 0x006F, HORN, DOT_BELOW_LC)

    # U with diacritics
    compose(0x1EE4, "Ụ", 0x0055, DOT_BELOW_UC)
    compose(0x1EE5, "ụ", 0x0075, DOT_BELOW_LC)
    compose(0x1EE6, "Ủ", 0x0055, HOOK_ABOVE_UC)
    compose(0x1EE7, "ủ", 0x0075, HOOK_ABOVE_LC)

    # U with horn + diacritics (Vietnamese Ư)
    compose(0x1EE8, "Ứ", 0x0055, HORN, ACUTE_ABOVE)
    compose(0x1EE9, "ứ", 0x0075, HORN, ACUTE_ABOVE)
    compose(0x1EEA, "Ừ", 0x0055, HORN, GRAVE_ABOVE)
    compose(0x1EEB, "ừ", 0x0075, HORN, GRAVE_ABOVE)
    compose(0x1EEC, "Ử", 0x0055, HORN, HOOK_ABOVE_UC)
    compose(0x1EED, "ử", 0x0075, HORN, HOOK_ABOVE_LC)
    compose(0x1EEE, "Ữ", 0x0055, HORN, TILDE_ABOVE)
    compose(0x1EEF, "ữ", 0x0075, HORN, TILDE_ABOVE)
    compose(0x1EF0, "Ự", 0x0055, HORN, DOT_BELOW_UC)
    compose(0x1EF1, "ự", 0x0075, HORN, DOT_BELOW_LC)

    # Y with diacritics
    compose(0x1EF2, "Ỳ", 0x0059, GRAVE_ABOVE)
    compose(0x1EF3, "ỳ", 0x0079, GRAVE_ABOVE)
    compose(0x1EF4, "Ỵ", 0x0059, DOT_BELOW_UC)
    compose(0x1EF5, "ỵ", 0x0079, DOT_BELOW_LC)
    compose(0x1EF6, "Ỷ", 0x0059, HOOK_ABOVE_UC)
    compose(0x1EF7, "ỷ", 0x0079, HOOK_ABOVE_LC)
    compose(0x1EF8, "Ỹ", 0x0059, TILDE_ABOVE)
    compose(0x1EF9, "ỹ", 0x0079, TILDE_ABOVE)

    # ================================================================
    # Lower range: U+1E00-1E9F  (scholarly/medieval Latin)
    # ================================================================

    # A with ring below
    compose(0x1E00, "Ḁ", 0x0041, (["00000000"] * 13 + ["00011000", "00100100", "00011000"]))
    compose(0x1E01, "ḁ", 0x0061, (["00000000"] * 13 + ["00011000", "00100100", "00011000"]))

    # B with dot above
    compose(0x1E02, "Ḃ", 0x0042, (["00011000"] + ["00000000"] * 15))
    compose(0x1E03, "ḃ", 0x0062, (["00011000"] + ["00000000"] * 15))

    # B with dot below
    compose(0x1E04, "Ḅ", 0x0042, DOT_BELOW_UC)
    compose(0x1E05, "ḅ", 0x0062, DOT_BELOW_LC)

    # B with line below
    compose(0x1E06, "Ḇ", 0x0042, LINE_BELOW)
    compose(0x1E07, "ḇ", 0x0062, LINE_BELOW)

    # C with cedilla + acute
    compose(0x1E08, "Ḉ", 0x00C7, ACUTE_ABOVE)
    compose(0x1E09, "ḉ", 0x00E7, ACUTE_ABOVE)

    # D with dot above
    compose(0x1E0A, "Ḋ", 0x0044, (["00011000"] + ["00000000"] * 15))
    compose(0x1E0B, "ḋ", 0x0064, (["00011000"] + ["00000000"] * 15))

    # D with dot below
    compose(0x1E0C, "Ḍ", 0x0044, DOT_BELOW_UC)
    compose(0x1E0D, "ḍ", 0x0064, DOT_BELOW_LC)

    # D with line below
    compose(0x1E0E, "Ḏ", 0x0044, LINE_BELOW)
    compose(0x1E0F, "ḏ", 0x0064, LINE_BELOW)

    # D with cedilla
    compose(0x1E10, "Ḑ", 0x0044, CEDILLA_BELOW)
    compose(0x1E11, "ḑ", 0x0064, CEDILLA_BELOW)

    # D with circumflex below
    compose(0x1E12, "Ḓ", 0x0044, CIRCUMFLEX_BELOW)
    compose(0x1E13, "ḓ", 0x0064, CIRCUMFLEX_BELOW)

    # E with macron + grave
    compose(0x1E14, "Ḕ", 0x0112, GRAVE_ABOVE) if 0x0112 in E else None
    compose(0x1E15, "ḕ", 0x0113, GRAVE_ABOVE) if 0x0113 in E else None

    # E with macron + acute
    compose(0x1E16, "Ḗ", 0x0112, ACUTE_ABOVE) if 0x0112 in E else None
    compose(0x1E17, "ḗ", 0x0113, ACUTE_ABOVE) if 0x0113 in E else None

    # E with circumflex below
    compose(0x1E18, "Ḙ", 0x0045, CIRCUMFLEX_BELOW)
    compose(0x1E19, "ḙ", 0x0065, CIRCUMFLEX_BELOW)

    # E with tilde below
    compose(0x1E1A, "Ḛ", 0x0045, (["00000000"] * 14 + ["00110100", "00101100"]))
    compose(0x1E1B, "ḛ", 0x0065, (["00000000"] * 14 + ["00110100", "00101100"]))

    # E with cedilla + breve
    compose(0x1E1C, "Ḝ", 0x00C8, BREVE_ABOVE) if 0x00C8 in E else None  # fallback
    compose(0x1E1D, "ḝ", 0x00E8, BREVE_ABOVE) if 0x00E8 in E else None

    # F with dot above
    compose(0x1E1E, "Ḟ", 0x0046, (["00011000"] + ["00000000"] * 15))
    compose(0x1E1F, "ḟ", 0x0066, (["00011000"] + ["00000000"] * 15))

    # G with macron
    compose(0x1E20, "Ḡ", 0x0047, (["00111100"] + ["00000000"] * 15))
    compose(0x1E21, "ḡ", 0x0067, (["00111100"] + ["00000000"] * 15))

    # H with dot above
    compose(0x1E22, "Ḣ", 0x0048, (["00011000"] + ["00000000"] * 15))
    compose(0x1E23, "ḣ", 0x0068, (["00011000"] + ["00000000"] * 15))

    # H with dot below
    compose(0x1E24, "Ḥ", 0x0048, DOT_BELOW_UC)
    compose(0x1E25, "ḥ", 0x0068, DOT_BELOW_LC)

    # H with diaeresis
    compose(0x1E26, "Ḧ", 0x0048, DIAERESIS_ABOVE)
    compose(0x1E27, "ḧ", 0x0068, DIAERESIS_ABOVE)

    # H with cedilla
    compose(0x1E28, "Ḩ", 0x0048, CEDILLA_BELOW)
    compose(0x1E29, "ḩ", 0x0068, CEDILLA_BELOW)

    # H with breve below
    compose(0x1E2A, "Ḫ", 0x0048, (["00000000"] * 14 + ["00100100", "00011000"]))
    compose(0x1E2B, "ḫ", 0x0068, (["00000000"] * 14 + ["00100100", "00011000"]))

    # I with tilde below
    compose(0x1E2C, "Ḭ", 0x0049, (["00000000"] * 14 + ["00110100", "00101100"]))
    compose(0x1E2D, "ḭ", 0x0069, (["00000000"] * 14 + ["00110100", "00101100"]))

    # I with diaeresis + acute
    compose(0x1E2E, "Ḯ", 0x00CF, ACUTE_ABOVE) if 0x00CF in E else None
    compose(0x1E2F, "ḯ", 0x00EF, ACUTE_ABOVE) if 0x00EF in E else None

    # K with acute
    compose(0x1E30, "Ḱ", 0x004B, ACUTE_ABOVE)
    compose(0x1E31, "ḱ", 0x006B, ACUTE_ABOVE)

    # K with dot below
    compose(0x1E32, "Ḳ", 0x004B, DOT_BELOW_UC)
    compose(0x1E33, "ḳ", 0x006B, DOT_BELOW_LC)

    # K with line below
    compose(0x1E34, "Ḵ", 0x004B, LINE_BELOW)
    compose(0x1E35, "ḵ", 0x006B, LINE_BELOW)

    # L with dot below
    compose(0x1E36, "Ḷ", 0x004C, DOT_BELOW_UC)
    compose(0x1E37, "ḷ", 0x006C, DOT_BELOW_LC)

    # L with dot below + macron
    compose(0x1E38, "Ḹ", 0x004C, DOT_BELOW_UC, (["00111100"] + ["00000000"] * 15))
    compose(0x1E39, "ḹ", 0x006C, DOT_BELOW_LC, (["00111100"] + ["00000000"] * 15))

    # L with line below
    compose(0x1E3A, "Ḻ", 0x004C, LINE_BELOW)
    compose(0x1E3B, "ḻ", 0x006C, LINE_BELOW)

    # L with circumflex below
    compose(0x1E3C, "Ḽ", 0x004C, CIRCUMFLEX_BELOW)
    compose(0x1E3D, "ḽ", 0x006C, CIRCUMFLEX_BELOW)

    # M with acute
    compose(0x1E3E, "Ḿ", 0x004D, ACUTE_ABOVE)
    compose(0x1E3F, "ḿ", 0x006D, ACUTE_ABOVE)

    # M with dot above
    compose(0x1E40, "Ṁ", 0x004D, (["00011000"] + ["00000000"] * 15))
    compose(0x1E41, "ṁ", 0x006D, (["00011000"] + ["00000000"] * 15))

    # M with dot below
    compose(0x1E42, "Ṃ", 0x004D, DOT_BELOW_UC)
    compose(0x1E43, "ṃ", 0x006D, DOT_BELOW_LC)

    # N with dot above
    compose(0x1E44, "Ṅ", 0x004E, (["00011000"] + ["00000000"] * 15))
    compose(0x1E45, "ṅ", 0x006E, (["00011000"] + ["00000000"] * 15))

    # N with dot below
    compose(0x1E46, "Ṇ", 0x004E, DOT_BELOW_UC)
    compose(0x1E47, "ṇ", 0x006E, DOT_BELOW_LC)

    # N with line below
    compose(0x1E48, "Ṉ", 0x004E, LINE_BELOW)
    compose(0x1E49, "ṉ", 0x006E, LINE_BELOW)

    # N with circumflex below
    compose(0x1E4A, "Ṋ", 0x004E, CIRCUMFLEX_BELOW)
    compose(0x1E4B, "ṋ", 0x006E, CIRCUMFLEX_BELOW)

    # O with tilde + acute
    compose(0x1E4C, "Ṍ", 0x00D5, ACUTE_ABOVE) if 0x00D5 in E else None
    compose(0x1E4D, "ṍ", 0x00F5, ACUTE_ABOVE) if 0x00F5 in E else None

    # O with tilde + diaeresis
    compose(0x1E4E, "Ṏ", 0x00D5, DIAERESIS_ABOVE) if 0x00D5 in E else None
    compose(0x1E4F, "ṏ", 0x00F5, DIAERESIS_ABOVE) if 0x00F5 in E else None

    # O with macron + grave
    compose(0x1E50, "Ṑ", 0x004F, (["00111100"] + ["00000000"] * 15), GRAVE_ABOVE)
    compose(0x1E51, "ṑ", 0x006F, (["00111100"] + ["00000000"] * 15), GRAVE_ABOVE)

    # O with macron + acute
    compose(0x1E52, "Ṓ", 0x004F, (["00111100"] + ["00000000"] * 15), ACUTE_ABOVE)
    compose(0x1E53, "ṓ", 0x006F, (["00111100"] + ["00000000"] * 15), ACUTE_ABOVE)

    # P with acute
    compose(0x1E54, "Ṕ", 0x0050, ACUTE_ABOVE)
    compose(0x1E55, "ṕ", 0x0070, ACUTE_ABOVE)

    # P with dot above
    compose(0x1E56, "Ṗ", 0x0050, (["00011000"] + ["00000000"] * 15))
    compose(0x1E57, "ṗ", 0x0070, (["00011000"] + ["00000000"] * 15))

    # R with dot above
    compose(0x1E58, "Ṙ", 0x0052, (["00011000"] + ["00000000"] * 15))
    compose(0x1E59, "ṙ", 0x0072, (["00011000"] + ["00000000"] * 15))

    # R with dot below
    compose(0x1E5A, "Ṛ", 0x0052, DOT_BELOW_UC)
    compose(0x1E5B, "ṛ", 0x0072, DOT_BELOW_LC)

    # R with dot below + macron
    compose(0x1E5C, "Ṝ", 0x0052, DOT_BELOW_UC, (["00111100"] + ["00000000"] * 15))
    compose(0x1E5D, "ṝ", 0x0072, DOT_BELOW_LC, (["00111100"] + ["00000000"] * 15))

    # R with line below
    compose(0x1E5E, "Ṟ", 0x0052, LINE_BELOW)
    compose(0x1E5F, "ṟ", 0x0072, LINE_BELOW)

    # S with dot above
    compose(0x1E60, "Ṡ", 0x0053, (["00011000"] + ["00000000"] * 15))
    compose(0x1E61, "ṡ", 0x0073, (["00011000"] + ["00000000"] * 15))

    # S with dot below
    compose(0x1E62, "Ṣ", 0x0053, DOT_BELOW_UC)
    compose(0x1E63, "ṣ", 0x0073, DOT_BELOW_LC)

    # S with acute + dot above
    compose(0x1E64, "Ṥ", 0x0053, ACUTE_ABOVE, (["00000000", "00011000"] + ["00000000"] * 14))
    compose(0x1E65, "ṥ", 0x0073, ACUTE_ABOVE, (["00000000", "00011000"] + ["00000000"] * 14))

    # S with caron + dot above
    compose(0x1E66, "Ṧ", 0x0053, CARON_ABOVE, (["00000000", "00000000", "00011000"] + ["00000000"] * 13))
    compose(0x1E67, "ṧ", 0x0073, CARON_ABOVE, (["00000000", "00000000", "00011000"] + ["00000000"] * 13))

    # S with dot below + dot above
    compose(0x1E68, "Ṩ", 0x0053, DOT_BELOW_UC, (["00011000"] + ["00000000"] * 15))
    compose(0x1E69, "ṩ", 0x0073, DOT_BELOW_LC, (["00011000"] + ["00000000"] * 15))

    # T with dot above
    compose(0x1E6A, "Ṫ", 0x0054, (["00011000"] + ["00000000"] * 15))
    compose(0x1E6B, "ṫ", 0x0074, (["00011000"] + ["00000000"] * 15))

    # T with dot below
    compose(0x1E6C, "Ṭ", 0x0054, DOT_BELOW_UC)
    compose(0x1E6D, "ṭ", 0x0074, DOT_BELOW_LC)

    # T with line below
    compose(0x1E6E, "Ṯ", 0x0054, LINE_BELOW)
    compose(0x1E6F, "ṯ", 0x0074, LINE_BELOW)

    # T with circumflex below
    compose(0x1E70, "Ṱ", 0x0054, CIRCUMFLEX_BELOW)
    compose(0x1E71, "ṱ", 0x0074, CIRCUMFLEX_BELOW)

    # U with diaeresis below
    compose(0x1E72, "Ṳ", 0x0055, (["00000000"] * 14 + ["00100100", "00000000"]))
    compose(0x1E73, "ṳ", 0x0075, (["00000000"] * 14 + ["00100100", "00000000"]))

    # U with tilde below
    compose(0x1E74, "Ṵ", 0x0055, (["00000000"] * 14 + ["00110100", "00101100"]))
    compose(0x1E75, "ṵ", 0x0075, (["00000000"] * 14 + ["00110100", "00101100"]))

    # U with circumflex below
    compose(0x1E76, "Ṷ", 0x0055, CIRCUMFLEX_BELOW)
    compose(0x1E77, "ṷ", 0x0075, CIRCUMFLEX_BELOW)

    # U with tilde + acute
    compose(0x1E78, "Ṹ", 0x0055, TILDE_ABOVE, ACUTE_ABOVE)
    compose(0x1E79, "ṹ", 0x0075, TILDE_ABOVE, ACUTE_ABOVE)

    # U with macron + diaeresis
    compose(0x1E7A, "Ṻ", 0x0055, (["00111100"] + ["00000000"] * 15), DIAERESIS_ABOVE)
    compose(0x1E7B, "ṻ", 0x0075, (["00111100"] + ["00000000"] * 15), DIAERESIS_ABOVE)

    # V with tilde
    compose(0x1E7C, "Ṽ", 0x0056, TILDE_ABOVE)
    compose(0x1E7D, "ṽ", 0x0076, TILDE_ABOVE)

    # V with dot below
    compose(0x1E7E, "Ṿ", 0x0056, DOT_BELOW_UC)
    compose(0x1E7F, "ṿ", 0x0076, DOT_BELOW_LC)

    # W with grave
    compose(0x1E80, "Ẁ", 0x0057, GRAVE_ABOVE)
    compose(0x1E81, "ẁ", 0x0077, GRAVE_ABOVE)

    # W with acute
    compose(0x1E82, "Ẃ", 0x0057, ACUTE_ABOVE)
    compose(0x1E83, "ẃ", 0x0077, ACUTE_ABOVE)

    # W with diaeresis
    compose(0x1E84, "Ẅ", 0x0057, DIAERESIS_ABOVE)
    compose(0x1E85, "ẅ", 0x0077, DIAERESIS_ABOVE)

    # W with dot above
    compose(0x1E86, "Ẇ", 0x0057, (["00011000"] + ["00000000"] * 15))
    compose(0x1E87, "ẇ", 0x0077, (["00011000"] + ["00000000"] * 15))

    # W with dot below
    compose(0x1E88, "Ẉ", 0x0057, DOT_BELOW_UC)
    compose(0x1E89, "ẉ", 0x0077, DOT_BELOW_LC)

    # X with dot above
    compose(0x1E8A, "Ẋ", 0x0058, (["00011000"] + ["00000000"] * 15))
    compose(0x1E8B, "ẋ", 0x0078, (["00011000"] + ["00000000"] * 15))

    # X with diaeresis
    compose(0x1E8C, "Ẍ", 0x0058, DIAERESIS_ABOVE)
    compose(0x1E8D, "ẍ", 0x0078, DIAERESIS_ABOVE)

    # Y with dot above
    compose(0x1E8E, "Ẏ", 0x0059, (["00011000"] + ["00000000"] * 15))
    compose(0x1E8F, "ẏ", 0x0079, (["00011000"] + ["00000000"] * 15))

    # Z with circumflex
    compose(0x1E90, "Ẑ", 0x005A, CIRCUMFLEX_ABOVE)
    compose(0x1E91, "ẑ", 0x007A, CIRCUMFLEX_ABOVE)

    # Z with dot below
    compose(0x1E92, "Ẓ", 0x005A, DOT_BELOW_UC)
    compose(0x1E93, "ẓ", 0x007A, DOT_BELOW_LC)

    # Z with line below
    compose(0x1E94, "Ẕ", 0x005A, LINE_BELOW)
    compose(0x1E95, "ẕ", 0x007A, LINE_BELOW)

    # h with line below
    compose(0x1E96, "ẖ", 0x0068, LINE_BELOW)

    # t with diaeresis
    compose(0x1E97, "ẗ", 0x0074, DIAERESIS_ABOVE)

    # w with ring above
    compose(0x1E98, "ẘ", 0x0077, RING_ABOVE)

    # y with ring above
    compose(0x1E99, "ẙ", 0x0079, RING_ABOVE)

    # a with right half ring (ayin)
    compose(0x1E9A, "ẚ", 0x0061, (["00000000", "00000010", "00000100"] + ["00000000"] * 13))

    # Long s with dot above
    compose(0x1E9B, "ẛ", 0x0066, (["00011000"] + ["00000000"] * 15))  # approx with f

    # Fill remaining range U+1E9C-1E9F with blanks (rare characters)
    for code in range(0x1E9C, 0x1EA0):
        if code not in [g["code"] for g in new_glyphs]:
            add(code, chr(code), BLANK)

    # Fill any gaps in U+1EFA-1EFF
    compose(0x1EFA, "Ỻ", 0x004C, STROKE_THROUGH)  # Long L
    compose(0x1EFB, "ỻ", 0x006C, STROKE_THROUGH)
    compose(0x1EFC, "Ỽ", 0x0056, TILDE_ABOVE)      # Middle-Welsh V
    compose(0x1EFD, "ỽ", 0x0076, TILDE_ABOVE)
    compose(0x1EFE, "Ỿ", 0x0059, (["00000000"] * 7 + ["01111110"] + ["00000000"] * 8))
    compose(0x1EFF, "ỿ", 0x0079, (["00000000"] * 7 + ["01111110"] + ["00000000"] * 8))

    # Filter out None entries (from conditional compose calls)
    new_glyphs_clean = [g for g in new_glyphs if g is not None]
    return new_glyphs_clean


def main():
    glyphs = load_font()
    E = build_lookup(glyphs)
    existing_codes = {g["code"] for g in glyphs}

    new_glyphs = make_latin_ext_additional(E)

    # Filter already-existing
    added = [g for g in new_glyphs if g["code"] not in existing_codes]

    if not added:
        print("All Latin Extended Additional glyphs already present.")
        return

    # Fill remaining gaps in U+1E00-1EFF with blank glyphs
    all_new_codes = {g["code"] for g in added}
    for code in range(0x1E00, 0x1F00):
        if code not in existing_codes and code not in all_new_codes:
            added.append({"code": code, "char": chr(code), "bitmap": BLANK})

    # Merge and sort
    merged = glyphs + added
    merged.sort(key=lambda g: g["code"])

    # Validate
    errors = 0
    for g in added:
        if len(g["bitmap"]) != 16:
            print(f"Error: U+{g['code']:04X} has {len(g['bitmap'])} rows", file=sys.stderr)
            errors += 1
            continue
        for row in g["bitmap"]:
            if len(row) != 8 or not all(c in "01" for c in row):
                print(f"Error: U+{g['code']:04X} bad row: {row!r}", file=sys.stderr)
                errors += 1
                break

    if errors:
        print(f"{errors} validation errors!", file=sys.stderr)
        sys.exit(1)

    with open(FONT_PATH, "w") as f:
        json.dump(merged, f, indent=2, ensure_ascii=False)

    print(f"Added {len(added)} Latin Extended Additional glyphs (U+1E00-1EFF)")
    print(f"Total glyphs in font.json: {len(merged)}")


if __name__ == "__main__":
    main()
