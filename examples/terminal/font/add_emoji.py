#!/usr/bin/env python3
"""Add common emoji glyphs to font.json + generate emoji_lut.mem.

Emoji are 16x16 wide characters stored in internal range U+5000-507F.
The terminal hardware uses a unified lookup ROM (shared with Kanji)
to remap emoji codepoints to internal slots at receive time.

This script:
1. Defines 128 popular emoji as 16x16 monochrome bitmaps
2. Assigns each an internal slot (U+5000 + index)
3. Adds the internally-coded glyphs to font.json
4. Generates emoji_lut.mem (128 entries of 24-bit Unicode codepoints, sorted)
   for the unified binary search ROM in terminal.jz
"""

import json
import os
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
FONT_PATH = os.path.join(SCRIPT_DIR, "font.json")
OUT_DIR = os.path.join(SCRIPT_DIR, "..", "out")


def W(hex_str):
    """Convert space-separated 4-digit hex row values to 16-char binary strings."""
    parts = hex_str.split()
    assert len(parts) == 16, f"Expected 16 hex values, got {len(parts)}"
    result = []
    for h in parts:
        val = int(h, 16)
        result.append(format(val, '016b'))
    return result


# ============================================================================
# Emoji definitions: (unicode_codepoint, name, hex_bitmap)
# Each bitmap is 16 rows of 16-bit hex values (space-separated)
#
# Includes ~14 BMP emoji (U+2600-2B50 range) and ~114 non-BMP emoji
# (U+1F300-1F9FF range), sorted by popularity.
# ============================================================================

EMOJI = [
    # === BMP Emoji (U+2000-2FFF range) ===

    # U+203C Double Exclamation Mark
    (0x203C, "double_exclamation", W("0000 0000 1818 1818 1818 1818 1818 1818 1008 0000 1818 1818 0000 0000 0000 0000")),

    # U+2600 Sun
    (0x2600, "sun", W("0000 0100 0280 2184 1188 0990 0FF0 0FF0 0FF0 0FF0 0990 1188 2184 0280 0100 0000")),

    # U+2615 Hot Beverage (coffee)
    (0x2615, "coffee", W("0000 0000 0480 0240 0480 0000 1FF0 3FF8 3FF8 3FF8 3FF8 3FF8 1FF0 0000 0000 0000")),

    # U+2639 Frowning Face
    (0x2639, "frowning", W("0000 0000 07E0 1FF8 3FFC 3FFC 7FFE 6DB6 7FFE 7FFE 7E7E 7DBE 7FFE 3FFC 1FF8 07E0")),

    # U+263A Smiling Face
    (0x263A, "smiling", W("0000 0000 07E0 1FF8 3FFC 3FFC 7FFE 6DB6 7FFE 7FFE 7DBE 7E7E 7FFE 3FFC 1FF8 07E0")),

    # U+2665 Heart Suit (solid)
    (0x2665, "heart_suit", W("0000 0000 0000 1C38 3E7C 7FFE 7FFE 7FFE 7FFE 3FFC 1FF8 0FF0 07E0 03C0 0180 0000")),

    # U+2705 Check Mark
    (0x2705, "check", W("0000 0000 0000 0006 000E 001C 0038 0070 40E0 61C0 3380 1F00 0E00 0400 0000 0000")),

    # U+2728 Sparkles
    (0x2728, "sparkles", W("0000 0100 0100 0100 0920 0540 0280 7C3E 0280 0540 0920 0100 0100 0100 0000 0000")),

    # U+2757 Exclamation Mark
    (0x2757, "exclamation", W("0000 0000 03C0 07E0 07E0 07E0 07E0 07E0 03C0 03C0 0180 0000 03C0 03C0 0180 0000")),

    # U+2763 Heart Exclamation
    (0x2763, "heart_exclamation", W("0000 1C38 3E7C 7FFE 7FFE 3FFC 1FF8 0FF0 07E0 03C0 0180 0000 03C0 03C0 0180 0000")),

    # U+2764 Red Heart
    (0x2764, "red_heart", W("0000 0000 0000 1C38 3E7C 7FFE 7FFE 7FFE 7FFE 3FFC 1FF8 0FF0 07E0 03C0 0180 0000")),

    # U+26A1 Zap (lightning)
    (0x26A1, "zap", W("0000 0000 00F0 01E0 03C0 0780 0F00 1FFC 01F0 03E0 07C0 0F80 1F00 1E00 0000 0000")),

    # U+2B50 Star
    (0x2B50, "star", W("0000 0000 0180 0180 03C0 03C0 07E0 FFFF 7FFE 0FF0 0FF0 1FF8 1818 3018 200C 0000")),

    # U+270C Victory Hand
    (0x270C, "victory", W("0000 0000 0000 0C60 0C60 0C60 0C60 0C60 0C60 0FE0 07C0 07C0 07C0 03C0 0380 0000")),

    # === Non-BMP Emoji (U+1F300+ range) ===

    # U+1F300 Cyclone
    (0x1F300, "cyclone", W("0000 0000 07F0 1FFC 3C0E 3006 7186 7386 73C6 71E6 700E 3C0E 1FFC 07F0 0000 0000")),

    # U+1F308 Rainbow
    (0x1F308, "rainbow", W("0000 0000 0000 07E0 1FF8 3FFC 7FFE 7FFE 6006 4002 4002 0000 0000 0000 0000 0000")),

    # U+1F30D Globe (Europe-Africa)
    (0x1F30D, "globe", W("0000 07E0 1FF8 3FFC 7FFE 7FFE 7FFE 7FFE 7FFE 7FFE 7FFE 3FFC 1FF8 07E0 0000 0000")),

    # U+1F31F Glowing Star
    (0x1F31F, "glowing_star", W("0000 0180 0180 0180 2184 1188 0990 07E0 0FF0 0990 1188 2184 0180 0180 0180 0000")),

    # U+1F339 Rose
    (0x1F339, "rose", W("0000 0180 03C0 07E0 0FF0 1FF8 1FF8 0FF0 07E0 0180 0180 0180 03C0 0180 0180 0000")),

    # U+1F346 Eggplant
    (0x1F346, "eggplant", W("0000 0060 00C0 0180 03C0 07E0 0FF0 0FF0 1FF8 1FF8 1FF8 0FF0 0FF0 07E0 03C0 0000")),

    # U+1F351 Peach
    (0x1F351, "peach", W("0000 0180 0180 03C0 0000 0FF0 1FF8 3FFC 7FFE 7FFE 7FFE 7FFE 3FFC 1FF8 0FF0 0000")),

    # U+1F37A Beer Mug
    (0x1F37A, "beer", W("0000 0000 3C00 7E00 7F80 7FC0 7FC0 7FC0 7FC0 7FC0 7FC0 7F80 7E00 3C00 0000 0000")),

    # U+1F37B Clinking Beer Mugs
    (0x1F37B, "beers", W("0000 0000 1E78 3FFC 3FFC 3FFC 3FFC 3FFC 3FFC 3FFC 3FFC 1FF8 0FF0 0000 0000 0000")),

    # U+1F381 Wrapped Gift
    (0x1F381, "gift", W("0000 0000 0180 07E0 1FF8 3FFC 1FF8 3FFC 2184 2184 2184 2184 2184 3FFC 0000 0000")),

    # U+1F382 Birthday Cake
    (0x1F382, "cake", W("0000 0000 0180 03C0 0180 0000 1FF8 3FFC 3FFC 2184 3FFC 3FFC 3FFC 1FF8 0000 0000")),

    # U+1F383 Jack-O-Lantern
    (0x1F383, "pumpkin", W("0000 0180 03C0 07E0 1FF8 3FFC 7FFE 6C36 7FFE 7FFE 73CE 7C3E 3FFC 1FF8 0000 0000")),

    # U+1F384 Christmas Tree
    (0x1F384, "xmas_tree", W("0000 0180 0180 03C0 07E0 03C0 07E0 0FF0 07E0 0FF0 1FF8 3FFC 0180 0180 03C0 0000")),

    # U+1F389 Party Popper
    (0x1F389, "party", W("0000 0002 0004 0008 0110 0220 0440 0880 1100 2200 4400 2200 1100 0880 0000 0000")),

    # U+1F38A Confetti Ball
    (0x1F38A, "confetti", W("0000 2412 1248 0180 03C0 07E0 0FF0 1FF8 3FFC 3FFC 3FFC 1FF8 0FF0 07E0 0000 0000")),

    # U+1F3B5 Musical Note
    (0x1F3B5, "music_note", W("0000 0000 0180 01C0 01E0 01F0 0180 0180 0180 0180 0180 0D80 0F80 0700 0000 0000")),

    # U+1F3B6 Musical Notes
    (0x1F3B6, "music_notes", W("0000 0000 0C30 0C38 0C3C 0C3C 0C30 0C30 0C30 6C30 EC30 CC30 6000 0000 0000 0000")),

    # U+1F3C6 Trophy
    (0x1F3C6, "trophy", W("0000 0000 7FFE 3FFC 3FFC 3FFC 1FF8 0FF0 07E0 03C0 0180 0180 07E0 07E0 0000 0000")),

    # U+1F3E0 House
    (0x1F3E0, "house", W("0000 0000 0180 03C0 07E0 0FF0 1FF8 3FFC 7FFE 0FF0 0FF0 0FF0 0FF0 0FF0 0000 0000")),

    # U+1F40D Snake
    (0x1F40D, "snake", W("0000 0000 0700 0F80 0880 0080 00C0 00F0 007C 003E 201F 300E 1C00 0F80 07C0 0000")),

    # U+1F40E Horse
    (0x1F40E, "horse", W("0000 0000 0C00 1E00 3F00 3F80 1FC0 0FC0 07E0 03E0 0370 0330 0618 0618 0000 0000")),

    # U+1F414 Chicken
    (0x1F414, "chicken", W("0000 0600 0F00 0F00 0600 0000 1FC0 3FE0 7FF0 7FF0 7FF0 3FE0 1FC0 0060 0040 0000")),

    # U+1F418 Elephant
    (0x1F418, "elephant", W("0000 0000 3C00 7F00 FF80 FFC0 FFC0 FFE0 FFE0 C0E0 C060 C060 E060 7FE0 3FC0 0000")),

    # U+1F41B Bug
    (0x1F41B, "bug", W("0000 0000 0000 0660 0FF0 1FF8 3FFC 7FFE 7FFE 3FFC 1FF8 0FF0 0660 0000 0000 0000")),

    # U+1F41D Honeybee
    (0x1F41D, "bee", W("0000 0000 0660 0660 0FF0 1FF8 1008 1FF8 1008 1FF8 0FF0 0660 0240 0420 0000 0000")),

    # U+1F420 Tropical Fish
    (0x1F420, "fish", W("0000 0000 0000 0000 03C0 0FF0 1FF8 7FFE 1FF8 0FF0 03C0 0000 0000 0000 0000 0000")),

    # U+1F425 Baby Chick
    (0x1F425, "chick", W("0000 0000 07E0 0FF0 0FF0 0FF0 07E0 3FFC 7FFE 7FFE 7FFE 3FFC 0000 0000 0000 0000")),

    # U+1F431 Cat Face
    (0x1F431, "cat", W("0000 2004 3C3C 3E7C 7FFE 7FFE 6DB6 7FFE 7FFE 7E7E 7DBE 3FFC 1FF8 0000 0000 0000")),

    # U+1F435 Monkey Face
    (0x1F435, "monkey", W("0000 0000 07E0 1FF8 3FFC 7FFE 7FFE 6DB6 7FFE 7FFE 73CE 3FFC 1FF8 07E0 0000 0000")),

    # U+1F436 Dog Face
    (0x1F436, "dog", W("0000 0000 1818 3C3C 7E7E 7FFE 7FFE 6DB6 7FFE 7FFE 7E7E 3FFC 1FF8 07E0 0000 0000")),

    # U+1F440 Eyes
    (0x1F440, "eyes", W("0000 0000 0000 0000 1C38 3E7C 7FFE 7FFE 7FFE 3E7C 1C38 0000 0000 0000 0000 0000")),

    # U+1F44B Waving Hand
    (0x1F44B, "wave", W("0000 0D80 1240 1240 1240 1240 1FE0 3FF0 3FF0 3FF0 3FF0 1FF0 1FE0 0FC0 0000 0000")),

    # U+1F44C OK Hand
    (0x1F44C, "ok_hand", W("0000 0000 07E0 0C30 0810 0810 0C30 07E0 0FF0 1FF8 1FF8 0FF0 0FF0 07E0 0000 0000")),

    # U+1F44D Thumbs Up
    (0x1F44D, "thumbsup", W("0000 0000 0180 03C0 07E0 07E0 0FF0 1FF8 1FF8 1FF8 1FF0 0FF0 07E0 03C0 0000 0000")),

    # U+1F44E Thumbs Down
    (0x1F44E, "thumbsdown", W("0000 0000 03C0 07E0 0FF0 1FF0 1FF8 1FF8 1FF8 0FF0 07E0 07E0 03C0 0180 0000 0000")),

    # U+1F44F Clapping Hands
    (0x1F44F, "clap", W("0000 0000 0240 07E0 0FF0 0FF0 1FF8 1FF8 1FF8 1FF8 0FF0 0FF0 07E0 03C0 0000 0000")),

    # U+1F450 Open Hands
    (0x1F450, "open_hands", W("0000 0000 1818 3C3C 3C3C 3C3C 1FF8 0FF0 0FF0 0FF0 0FF0 07E0 07E0 03C0 0000 0000")),

    # U+1F451 Crown
    (0x1F451, "crown", W("0000 0000 0000 2184 2184 3FFC 7FFE 7FFE 7FFE 7FFE 7FFE 7FFE 7FFE 0000 0000 0000")),

    # U+1F480 Skull
    (0x1F480, "skull", W("0000 0000 07E0 1FF8 3FFC 7FFE 6DB6 7FFE 6DB6 3FFC 1FF8 0FF0 0660 0FF0 0000 0000")),

    # U+1F494 Broken Heart
    (0x1F494, "broken_heart", W("0000 0000 0000 1C38 3E7C 7FFE 7F9E 3F0C 1C18 0F30 07E0 03C0 0180 0000 0000 0000")),

    # U+1F495 Two Hearts
    (0x1F495, "two_hearts", W("0000 0000 0E1C 1F3E 1F3E 0FFC 07F8 03F0 01E0 7C00 FE00 FE00 7C00 3800 1000 0000")),

    # U+1F496 Sparkling Heart
    (0x1F496, "sparkling_heart", W("0000 2004 0000 1C38 3E7C 7FFE 7FFE 7FFE 3FFC 1FF8 0FF0 07E0 03C0 0180 0000 2004")),

    # U+1F498 Heart with Arrow
    (0x1F498, "heart_arrow", W("0000 0000 0003 1C3B 3E7F 7FFE 7FFC 7FF8 3FFC 1FF8 0FF0 07E0 03C0 0180 0000 0000")),

    # U+1F499 Blue Heart
    (0x1F499, "blue_heart", W("0000 0000 0000 1C38 3E7C 7FFE 7FFE 7FFE 7FFE 3FFC 1FF8 0FF0 07E0 03C0 0180 0000")),

    # U+1F49A Green Heart
    (0x1F49A, "green_heart", W("0000 0000 0000 1C38 3E7C 7FFE 7FFE 7FFE 7FFE 3FFC 1FF8 0FF0 07E0 03C0 0180 0000")),

    # U+1F49B Yellow Heart
    (0x1F49B, "yellow_heart", W("0000 0000 0000 1C38 3E7C 7FFE 7FFE 7FFE 7FFE 3FFC 1FF8 0FF0 07E0 03C0 0180 0000")),

    # U+1F49C Purple Heart
    (0x1F49C, "purple_heart", W("0000 0000 0000 1C38 3E7C 7FFE 7FFE 7FFE 7FFE 3FFC 1FF8 0FF0 07E0 03C0 0180 0000")),

    # U+1F49D Heart with Ribbon
    (0x1F49D, "heart_ribbon", W("0000 0000 0000 1C38 3E7C 7FFE 7FFE 7FFE 7FFE 3FFC 1FF8 0FF0 07E0 03C0 0180 0000")),

    # U+1F4A1 Light Bulb
    (0x1F4A1, "bulb", W("0000 0000 03C0 0FF0 1FF8 1FF8 1FF8 1FF8 0FF0 07E0 07E0 03C0 03C0 03C0 0180 0000")),

    # U+1F4A3 Bomb
    (0x1F4A3, "bomb", W("0000 0060 00C0 0180 0180 07E0 1FF8 3FFC 7FFE 7FFE 7FFE 7FFE 3FFC 1FF8 07E0 0000")),

    # U+1F4A5 Collision (boom)
    (0x1F4A5, "boom", W("0000 0100 2108 1290 0C60 0180 0180 7FFE 0180 0180 0C60 1290 2108 0100 0000 0000")),

    # U+1F4A9 Pile of Poo
    (0x1F4A9, "poo", W("0000 0000 0180 03C0 03C0 07E0 0FF0 0FF0 1FF8 3FFC 6DB6 7FFE 73CE 3FFC 1FF8 0000")),

    # U+1F4AA Flexed Biceps
    (0x1F4AA, "muscle", W("0000 0000 0780 0FC0 1FE0 3FE0 3FC0 1FC0 0FE0 07E0 03C0 03C0 0180 0000 0000 0000")),

    # U+1F4AF Hundred Points
    (0x1F4AF, "hundred", W("0000 0000 6186 F3CF F3CF 6186 0000 7FFE 0000 6186 F3CF F3CF 6186 0000 0000 0000")),

    # U+1F4B0 Money Bag
    (0x1F4B0, "money_bag", W("0000 0180 07E0 0FF0 0FF0 1FF8 3FFC 3E7C 3FFC 3E7C 3FFC 1FF8 0FF0 07E0 0000 0000")),

    # U+1F4D6 Open Book
    (0x1F4D6, "book", W("0000 0000 7FFE 7FFE 6006 6186 6006 6186 6006 6186 6006 7FFE 7FFE 0000 0000 0000")),

    # U+1F4E7 E-Mail
    (0x1F4E7, "email", W("0000 0000 0000 7FFE 7FFE 7186 6306 4002 6006 7186 7986 7FFE 7FFE 0000 0000 0000")),

    # U+1F525 Fire
    (0x1F525, "fire", W("0000 0000 0080 0180 0180 03C0 07E0 0FF0 1FF8 1FF8 3FFC 3FFC 1FF8 0FF0 03C0 0000")),

    # U+1F550 Clock (1 o'clock)
    (0x1F550, "clock", W("0000 07E0 1FF8 3FFC 71CE 6186 6186 61FE 6006 7006 3FFC 1FF8 07E0 0000 0000 0000")),

    # U+1F595 Middle Finger
    (0x1F595, "middle_finger", W("0000 0000 0180 0180 0180 0180 03C0 07E0 07E0 07E0 07E0 07E0 03C0 0000 0000 0000")),

    # U+1F596 Vulcan Salute
    (0x1F596, "vulcan", W("0000 0000 6C30 6C30 6C30 6C30 6C30 7FF0 3FE0 1FC0 0FC0 07C0 03C0 0000 0000 0000")),

    # U+1F5A4 Black Heart
    (0x1F5A4, "black_heart", W("0000 0000 0000 1C38 3E7C 7FFE 7FFE 7FFE 7FFE 3FFC 1FF8 0FF0 07E0 03C0 0180 0000")),

    # U+1F600 Grinning Face
    (0x1F600, "grinning", W("0000 0000 07E0 1FF8 3FFC 7FFE 6DB6 7FFE 7FFE 7C3E 7FFE 3FFC 1FF8 07E0 0000 0000")),

    # U+1F601 Beaming Face
    (0x1F601, "beaming", W("0000 0000 07E0 1FF8 3FFC 7FFE 6DB6 7FFE 7FFE 7C3E 7FFE 3FFC 1FF8 07E0 0000 0000")),

    # U+1F602 Face with Tears of Joy
    (0x1F602, "joy", W("0000 0000 07E0 1FF8 3FFC 7FFE 6DB6 7FFE 7FFE 7C3E 7FFE 3FFC 1FF8 07E0 0000 0000")),

    # U+1F603 Smiling with Open Mouth
    (0x1F603, "smiley", W("0000 0000 07E0 1FF8 3FFC 7FFE 6DB6 7FFE 7FFE 7C3E 7FFE 3FFC 1FF8 07E0 0000 0000")),

    # U+1F604 Smiling with Open Mouth + Smiling Eyes
    (0x1F604, "smile", W("0000 0000 07E0 1FF8 3FFC 7FFE 7FFE 6DB6 7FFE 7C3E 7FFE 3FFC 1FF8 07E0 0000 0000")),

    # U+1F605 Smiling with Sweat
    (0x1F605, "sweat_smile", W("0000 0000 07E0 1FF8 3FFC 7FFE 6DB6 7FFE 7FFE 7C3E 7FFE 3FFC 1FF8 07E0 0060 0000")),

    # U+1F606 Squinting
    (0x1F606, "laughing", W("0000 0000 07E0 1FF8 3FFC 7FFE 7FFE 7FFE 7FFE 7C3E 7FFE 3FFC 1FF8 07E0 0000 0000")),

    # U+1F607 Smiling with Halo
    (0x1F607, "halo", W("03C0 07E0 07E0 1FF8 3FFC 7FFE 6DB6 7FFE 7FFE 7DBE 7E7E 3FFC 1FF8 07E0 0000 0000")),

    # U+1F608 Smiling with Horns
    (0x1F608, "imp", W("6006 3C3C 0FF0 1FF8 3FFC 7FFE 6DB6 7FFE 7FFE 7DBE 7E7E 3FFC 1FF8 07E0 0000 0000")),

    # U+1F609 Winking Face
    (0x1F609, "wink", W("0000 0000 07E0 1FF8 3FFC 7FFE 6DB6 7FFE 7FFE 7DBE 7E7E 3FFC 1FF8 07E0 0000 0000")),

    # U+1F60A Smiling with Smiling Eyes
    (0x1F60A, "blush", W("0000 0000 07E0 1FF8 3FFC 7FFE 7FFE 6DB6 7FFE 7DBE 7E7E 3FFC 1FF8 07E0 0000 0000")),

    # U+1F60B Face Savoring Food
    (0x1F60B, "yum", W("0000 0000 07E0 1FF8 3FFC 7FFE 6DB6 7FFE 7FFE 7C3E 7FFE 3FFC 1FF8 07E0 0000 0000")),

    # U+1F60C Relieved Face
    (0x1F60C, "relieved", W("0000 0000 07E0 1FF8 3FFC 7FFE 7FFE 7FFE 7FFE 7DBE 7E7E 3FFC 1FF8 07E0 0000 0000")),

    # U+1F60D Heart Eyes
    (0x1F60D, "heart_eyes", W("0000 0000 07E0 1FF8 3FFC 7FFE 6DB6 6DB6 7FFE 7C3E 7FFE 3FFC 1FF8 07E0 0000 0000")),

    # U+1F60E Sunglasses
    (0x1F60E, "sunglasses", W("0000 0000 07E0 1FF8 3FFC 7FFE 7FFE 7FFE 7FFE 7DBE 7E7E 3FFC 1FF8 07E0 0000 0000")),

    # U+1F60F Smirking
    (0x1F60F, "smirk", W("0000 0000 07E0 1FF8 3FFC 7FFE 6DB6 7FFE 7FFE 7FBE 7E7E 3FFC 1FF8 07E0 0000 0000")),

    # U+1F610 Neutral Face
    (0x1F610, "neutral", W("0000 0000 07E0 1FF8 3FFC 7FFE 6DB6 7FFE 7FFE 7FFE 7E7E 3FFC 1FF8 07E0 0000 0000")),

    # U+1F611 Expressionless
    (0x1F611, "expressionless", W("0000 0000 07E0 1FF8 3FFC 7FFE 7FFE 6DB6 7FFE 7FFE 7E7E 3FFC 1FF8 07E0 0000 0000")),

    # U+1F612 Unamused
    (0x1F612, "unamused", W("0000 0000 07E0 1FF8 3FFC 7FFE 6DB6 7FFE 7FFE 7FBE 7E7E 3FFC 1FF8 07E0 0000 0000")),

    # U+1F614 Pensive
    (0x1F614, "pensive", W("0000 0000 07E0 1FF8 3FFC 7FFE 7FFE 6DB6 7FFE 7E7E 7DBE 3FFC 1FF8 07E0 0000 0000")),

    # U+1F618 Kissing with Heart
    (0x1F618, "kiss_heart", W("0000 0000 07E0 1FF8 3FFC 7FFE 6DB6 7FFE 7FFE 7F7E 7FFE 3FFC 1FF8 07E0 0E00 0000")),

    # U+1F61B Tongue Out
    (0x1F61B, "tongue", W("0000 0000 07E0 1FF8 3FFC 7FFE 6DB6 7FFE 7FFE 7C3E 7FFE 3FFC 1FF8 07E0 0180 0180")),

    # U+1F61C Winking Tongue
    (0x1F61C, "wink_tongue", W("0000 0000 07E0 1FF8 3FFC 7FFE 6FF6 7FFE 7FFE 7C3E 7FFE 3FFC 1FF8 07E0 0180 0180")),

    # U+1F61D Squinting Tongue
    (0x1F61D, "squint_tongue", W("0000 0000 07E0 1FF8 3FFC 7FFE 7FFE 7FFE 7FFE 7C3E 7FFE 3FFC 1FF8 07E0 0180 0180")),

    # U+1F620 Angry Face
    (0x1F620, "angry", W("0000 0000 07E0 1FF8 3FFC 7FFE 7FFE 6DB6 7FFE 7E7E 7DBE 3FFC 1FF8 07E0 0000 0000")),

    # U+1F621 Pouting Face
    (0x1F621, "rage", W("0000 0000 07E0 1FF8 3FFC 7FFE 7FFE 6DB6 7FFE 7E7E 7DBE 3FFC 1FF8 07E0 0000 0000")),

    # U+1F622 Crying Face
    (0x1F622, "cry", W("0000 0000 07E0 1FF8 3FFC 7FFE 6DB6 7FFE 7FFE 7E7E 7DBE 3FFC 1FF8 07E0 0060 0000")),

    # U+1F623 Persevering
    (0x1F623, "persevere", W("0000 0000 07E0 1FF8 3FFC 7FFE 7FFE 7FFE 7FFE 7E7E 7DBE 3FFC 1FF8 07E0 0000 0000")),

    # U+1F624 Triumph
    (0x1F624, "triumph", W("0000 0000 07E0 1FF8 3FFC 7FFE 7FFE 6DB6 7FFE 7FFE 7DBE 3FFC 1FF8 07E0 0000 0000")),

    # U+1F625 Disappointed + Relieved
    (0x1F625, "disappointed_relieved", W("0000 0000 07E0 1FF8 3FFC 7FFE 6DB6 7FFE 7FFE 7E7E 7DBE 3FFC 1FF8 07E0 0060 0000")),

    # U+1F628 Fearful
    (0x1F628, "fearful", W("0000 0000 07E0 1FF8 3FFC 7FFE 6DB6 7FFE 7FFE 7E7E 7FFE 3FFC 1FF8 07E0 0000 0000")),

    # U+1F629 Weary
    (0x1F629, "weary", W("0000 0000 07E0 1FF8 3FFC 7FFE 7FFE 7FFE 7FFE 7E7E 7DBE 3FFC 1FF8 07E0 0000 0000")),

    # U+1F62A Sleepy
    (0x1F62A, "sleepy", W("0000 0000 07E0 1FF8 3FFC 7FFE 7FFE 6DB6 7FFE 7F7E 7FFE 3FFC 1FF8 07E0 0000 0000")),

    # U+1F62B Tired
    (0x1F62B, "tired", W("0000 0000 07E0 1FF8 3FFC 7FFE 7FFE 7FFE 7FFE 7C3E 7FFE 3FFC 1FF8 07E0 0000 0000")),

    # U+1F62D Loudly Crying
    (0x1F62D, "sob", W("0000 0000 07E0 1FF8 3FFC 7FFE 6DB6 7FFE 7FFE 7C3E 7FFE 3FFC 1FF8 07E0 0660 0000")),

    # U+1F62E Open Mouth
    (0x1F62E, "open_mouth", W("0000 0000 07E0 1FF8 3FFC 7FFE 6DB6 7FFE 7FFE 7E7E 7E7E 3FFC 1FF8 07E0 0000 0000")),

    # U+1F631 Screaming
    (0x1F631, "scream", W("0000 0000 07E0 1FF8 3FFC 7FFE 6DB6 7FFE 7FFE 7E7E 7E7E 7E7E 3FFC 1FF8 07E0 0000")),

    # U+1F633 Flushed
    (0x1F633, "flushed", W("0000 0000 07E0 1FF8 3FFC 7FFE 63C6 7FFE 7FFE 7E7E 7DBE 3FFC 1FF8 07E0 0000 0000")),

    # U+1F634 Sleeping
    (0x1F634, "sleeping", W("0000 1FF8 0000 07E0 1FF8 3FFC 7FFE 7FFE 7FFE 7FFE 7F7E 7FFE 3FFC 1FF8 07E0 0000")),

    # U+1F637 Medical Mask
    (0x1F637, "mask", W("0000 0000 07E0 1FF8 3FFC 7FFE 6DB6 7FFE 7FFE 7FFE 7FFE 3FFC 1FF8 07E0 0000 0000")),

    # U+1F642 Slightly Smiling
    (0x1F642, "slight_smile", W("0000 0000 07E0 1FF8 3FFC 7FFE 6DB6 7FFE 7FFE 7DBE 7E7E 3FFC 1FF8 07E0 0000 0000")),

    # U+1F643 Upside-Down Face
    (0x1F643, "upside_down", W("0000 0000 07E0 1FF8 3FFC 7E7E 7DBE 7FFE 7FFE 6DB6 7FFE 3FFC 1FF8 07E0 0000 0000")),

    # U+1F644 Rolling Eyes
    (0x1F644, "rolling_eyes", W("0000 0000 07E0 1FF8 3FFC 7FFE 6FF6 7FFE 7FFE 7FFE 7E7E 3FFC 1FF8 07E0 0000 0000")),

    # U+1F648 See-No-Evil Monkey
    (0x1F648, "see_no_evil", W("0000 0000 0000 07E0 1FF8 3FFC 7FFE 7FFE 7FFE 3FFC 7FFE 73CE 3FFC 1FF8 07E0 0000")),

    # U+1F649 Hear-No-Evil Monkey
    (0x1F649, "hear_no_evil", W("0000 0000 0000 07E0 1FF8 3FFC 7FFE 7FFE 7FFE 3FFC 6DB6 7FFE 3FFC 1FF8 07E0 0000")),

    # U+1F64A Speak-No-Evil Monkey
    (0x1F64A, "speak_no_evil", W("0000 0000 0000 07E0 1FF8 3FFC 6DB6 7FFE 7FFE 3FFC 7FFE 7FFE 3FFC 1FF8 07E0 0000")),

    # U+1F64F Folded Hands (prayer)
    (0x1F64F, "pray", W("0000 0180 03C0 07E0 07E0 0FF0 0FF0 0FF0 0FF0 0FF0 07E0 07E0 03C0 0180 0000 0000")),

    # U+1F680 Rocket
    (0x1F680, "rocket", W("0000 0180 03C0 07E0 0FF0 0FF0 1FF8 1FF8 1FF8 1FF8 0FF0 0FF0 1998 3C3C 1818 0000")),

    # U+1F6A8 Police Car Light
    (0x1F6A8, "rotating_light", W("0000 0180 03C0 07E0 0FF0 0FF0 0FF0 1FF8 3FFC 3FFC 3FFC 3FFC 3FFC 1FF8 0000 0000")),

    # U+1F6AB No Entry
    (0x1F6AB, "no_entry", W("0000 07E0 1FF8 3FFC 7FFE 7FFE 7FFE 7FFE 7FFE 7FFE 7FFE 3FFC 1FF8 07E0 0000 0000")),

    # U+1F910 Zipper-Mouth
    (0x1F910, "zipper_mouth", W("0000 0000 07E0 1FF8 3FFC 7FFE 6DB6 7FFE 7FFE 7FFE 7FFE 3FFC 1FF8 07E0 0000 0000")),

    # U+1F911 Money-Mouth
    (0x1F911, "money_mouth", W("0000 0000 07E0 1FF8 3FFC 7FFE 6DB6 7FFE 7FFE 7E7E 7E7E 3FFC 1FF8 07E0 0000 0000")),

    # U+1F913 Nerd Face
    (0x1F913, "nerd", W("0000 0000 07E0 1FF8 3FFC 7FFE 6DB6 7FFE 7FFE 7DBE 7E7E 3FFC 1FF8 07E0 0000 0000")),

    # U+1F914 Thinking Face
    (0x1F914, "thinking", W("0000 0000 07E0 1FF8 3FFC 7FFE 6DB6 7FFE 7FFE 7FBE 7E7E 3FFC 1FF8 07E0 0600 0000")),

    # U+1F917 Hugging Face
    (0x1F917, "hugging", W("0000 0000 07E0 1FF8 3FFC 7FFE 6DB6 7FFE 7FFE 7DBE 7E7E 3FFC 1FF8 07E0 0000 0000")),

    # U+1F918 Metal (horns)
    (0x1F918, "metal", W("0000 0000 6C00 6C00 6C00 6C00 6FE0 7FF0 3FF0 1FE0 0FE0 07E0 03C0 0000 0000 0000")),

    # U+1F919 Call Me Hand
    (0x1F919, "call_me", W("0000 0000 E00E E00E E00E E00E FFE0 7FF0 3FF0 1FE0 0FE0 07E0 03C0 0000 0000 0000")),

    # U+1F91A Raised Back of Hand
    (0x1F91A, "raised_back", W("0000 0000 0180 03C0 07E0 07E0 0FF0 0FF0 0FF0 0FF0 0FF0 07E0 07E0 03C0 0000 0000")),

    # U+1F91E Crossed Fingers
    (0x1F91E, "crossed_fingers", W("0000 0000 0360 07E0 07E0 07E0 07E0 0FC0 0FC0 0FC0 07C0 07C0 03C0 0000 0000 0000")),

    # U+1F923 Rolling on Floor Laughing
    (0x1F923, "rofl", W("0000 0000 07E0 1FF8 3FFC 7FFE 7FFE 7FFE 7FFE 7C3E 7FFE 3FFC 1FF8 07E0 0000 0000")),

    # U+1F926 Facepalm
    (0x1F926, "facepalm", W("0000 0000 7FE0 7FE0 7FF0 7FF8 3FFC 1FF8 0FF0 0FF0 0FF0 07E0 07E0 03C0 0000 0000")),

    # U+1F929 Star-Struck
    (0x1F929, "star_struck", W("0000 0000 07E0 1FF8 3FFC 7FFE 6DB6 6DB6 7FFE 7C3E 7FFE 3FFC 1FF8 07E0 0000 0000")),

    # U+1F92A Zany Face
    (0x1F92A, "zany", W("0000 0000 07E0 1FF8 3FFC 7FFE 6FF6 6DB6 7FFE 7C3E 7FFE 3FFC 1FF8 07E0 0000 0000")),

    # U+1F92B Shushing Face
    (0x1F92B, "shushing", W("0000 0000 07E0 1FF8 3FFC 7FFE 6DB6 7FFE 7FFE 7F7E 7FFE 3FFC 1FF8 07E0 0000 0000")),

    # U+1F92D Hand Over Mouth
    (0x1F92D, "hand_over_mouth", W("0000 0000 07E0 1FF8 3FFC 7FFE 6DB6 7FFE 7FFE 7FFE 7FFE 3FFC 1FF8 07E0 0000 0000")),

    # U+1F970 Smiling with Hearts
    (0x1F970, "smiling_hearts", W("0000 0000 47E2 1FF8 3FFC 7FFE 6DB6 7FFE 7FFE 7DBE 7E7E 3FFC 1FF8 07E0 0000 0000")),

    # U+1F971 Yawning Face
    (0x1F971, "yawning", W("0000 0000 07E0 1FF8 3FFC 7FFE 7FFE 6DB6 7FFE 7E7E 7E7E 3FFC 1FF8 07E0 0000 0000")),

    # U+1F973 Partying Face
    (0x1F973, "partying", W("0600 0300 07E0 1FF8 3FFC 7FFE 6DB6 7FFE 7FFE 7C3E 7FFE 3FFC 1FF8 07E0 0000 0000")),

    # U+1F975 Hot Face
    (0x1F975, "hot_face", W("0000 0000 07E0 1FF8 3FFC 7FFE 6DB6 7FFE 7FFE 7C3E 7FFE 3FFC 1FF8 07E0 0000 0000")),

    # U+1F976 Cold Face
    (0x1F976, "cold_face", W("0000 0000 07E0 1FF8 3FFC 7FFE 6DB6 7FFE 7FFE 7E7E 7FFE 3FFC 1FF8 07E0 0000 0000")),

    # U+1F979 Face Holding Back Tears
    (0x1F979, "holding_back_tears", W("0000 0000 07E0 1FF8 3FFC 7FFE 6DB6 7FFE 7FFE 7FFE 7DBE 3FFC 1FF8 07E0 0000 0000")),

    # U+1F97A Pleading Face
    (0x1F97A, "pleading", W("0000 0000 07E0 1FF8 3FFC 7FFE 63C6 7FFE 7FFE 7E7E 7DBE 3FFC 1FF8 07E0 0000 0000")),

    # U+1F9E1 Orange Heart
    (0x1F9E1, "orange_heart", W("0000 0000 0000 1C38 3E7C 7FFE 7FFE 7FFE 7FFE 3FFC 1FF8 0FF0 07E0 03C0 0180 0000")),
]


def main():
    with open(FONT_PATH) as f:
        glyphs = json.load(f)

    existing_codes = {g["code"] for g in glyphs}

    # Sort emoji by Unicode codepoint for lookup ROM
    emoji_sorted = sorted(EMOJI, key=lambda e: e[0])

    if len(emoji_sorted) > 128:
        print(f"Warning: {len(emoji_sorted)} emoji exceeds 128-slot limit, truncating", file=sys.stderr)
        emoji_sorted = emoji_sorted[:128]

    # Deduplicate
    seen = set()
    emoji_dedup = []
    for e in emoji_sorted:
        if e[0] not in seen:
            seen.add(e[0])
            emoji_dedup.append(e)
    emoji_sorted = emoji_dedup

    print(f"Processing {len(emoji_sorted)} emoji...")

    # Assign internal slots: U+5000 + index
    added = []
    unicode_to_slot = {}

    for idx, (unicode_cp, name, bitmap) in enumerate(emoji_sorted):
        internal_code = 0x5000 + idx
        unicode_to_slot[unicode_cp] = idx

        if internal_code in existing_codes:
            continue

        # Validate bitmap
        assert len(bitmap) == 16, f"U+{unicode_cp:04X} ({name}) has {len(bitmap)} rows"
        for row in bitmap:
            assert len(row) == 16 and all(c in "01" for c in row), \
                f"U+{unicode_cp:04X} ({name}) bad wide row"

        added.append({
            "code": internal_code,
            "char": name,
            "bitmap": bitmap,
            "_unicode": unicode_cp  # metadata for reference
        })

    if not added:
        print("All emoji glyphs already present.")
    else:
        # Remove metadata before saving
        for g in added:
            del g["_unicode"]

        # Merge and sort
        merged = glyphs + added
        merged.sort(key=lambda g: g["code"])

        with open(FONT_PATH, "w") as f:
            json.dump(merged, f, indent=2, ensure_ascii=False)

        print(f"Added {len(added)} emoji glyphs (U+5000-50{len(emoji_sorted)-1:02X})")
        print(f"Total glyphs in font.json: {len(merged)}")

    # Generate emoji_lut.mem (128 entries, 24-bit Unicode codepoints sorted)
    os.makedirs(OUT_DIR, exist_ok=True)
    lut_path = os.path.join(OUT_DIR, "emoji_lut.mem")
    with open(lut_path, "w") as f:
        for idx in range(128):
            if idx < len(emoji_sorted):
                unicode_cp = emoji_sorted[idx][0]
                f.write(f"{unicode_cp:024b}\n")
            else:
                f.write("111111111111111111111111\n")  # sentinel
    print(f"Generated {lut_path} ({len(emoji_sorted)} entries + padding)")

    # Also generate a human-readable mapping file
    map_path = os.path.join(OUT_DIR, "emoji_map.txt")
    with open(map_path, "w") as f:
        f.write("# Emoji lookup table: slot -> Unicode -> internal -> name\n")
        f.write(f"# {len(emoji_sorted)} entries\n\n")
        for idx, (unicode_cp, name, _) in enumerate(emoji_sorted):
            internal = 0x5000 + idx
            f.write(f"slot {idx:3d}: U+{unicode_cp:06X} -> U+{internal:04X}  {name}\n")
    print(f"Generated {map_path}")


if __name__ == "__main__":
    main()
