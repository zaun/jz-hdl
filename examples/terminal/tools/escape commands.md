# CSI (Control Sequence Introducer) Commands

## Screen Clearing

### `CSI n J`

| n Value | Action |
|----------|--------|
| 0 (default) | Clear from cursor to end of screen |
| 1 | Clear from cursor to beginning of screen |
| 2 | Clear entire screen |
| 3 | Clear entire screen and delete all lines saved in the scrollback buffer |

---

## Cursor Positioning

### `CSI n ; m H`

Moves the cursor to **row `n`**, **column `m`**.

- Values are **1-based**
- Defaults to **1;1** (top-left corner) if omitted

Examples:

- `CSI ;5H` → same as `CSI 1;5H`
- `CSI 17;H` → same as `CSI 17;1H`
- `CSI 17H` → same as `CSI 17;1H`

---

## Relative Cursor Movement

| Sequence | Direction | Description |
|-----------|------------|--------------|
| `CSI n A` | Up | Move cursor up `n` cells (default 1) |
| `CSI n B` | Down | Move cursor down `n` cells |
| `CSI n C` | Forward | Move cursor right `n` cells |
| `CSI n D` | Back | Move cursor left `n` cells |

If the cursor is at the screen edge, movement has no effect.

---

## Line Movement

| Sequence | Description |
|------------|--------------|
| `CSI n E` | Move to beginning of line `n` lines down (default 1) |
| `CSI n F` | Move to beginning of line `n` lines up (default 1) |

---

## Horizontal Positioning

### `CSI n G`

Move cursor to column `n` (default 1).

---

## Text Attributes

### `CSI n m`

Sets character colors and styles (SGR — Select Graphic Rendition).

---

## Line Erasing

### `CSI n K`

Erases part of the current line (cursor position does not change).

| n Value | Action |
|----------|--------|
| 0 (default) | Clear from cursor to end of line |
| 1 | Clear from cursor to beginning of line |
| 2 | Clear entire line |

---

## Scrolling

| Sequence | Description |
|------------|--------------|
| `CSI n S` | Scroll whole page up `n` lines (default 1). New lines added at bottom. |
| `CSI n T` | Scroll whole page down `n` lines (default 1). New lines added at top. |

---

## Cursor Save / Restore

| Sequence | Description |
|------------|--------------|
| `CSI s` | Save cursor position/state (SCO mode) |
| `CSI u` | Restore cursor position/state |

---

Here is your updated **Markdown reference**, with a new section added documenting the color support formally under SGR:

---

## SGR — Select Graphic Rendition (`CSI n m`)

Your terminal currently supports the standard 16-color ANSI palette:

### Foreground Colors

#### Standard FG (30–37)

| Code | Color |
|------|--------|
| 30   | Black |
| 31   | Red |
| 32   | Green |
| 33   | Yellow |
| 34   | Blue |
| 35   | Magenta |
| 36   | Cyan |
| 37   | White |

#### Bright FG (90–97)

| Code | Color |
|------|--------|
| 90   | Gray (Bright Black) |
| 91   | Bright Red |
| 92   | Bright Green |
| 93   | Bright Yellow |
| 94   | Bright Blue |
| 95   | Bright Magenta |
| 96   | Bright Cyan |
| 97   | Bright White |

---

### Background Colors

#### Standard BG (40–47)

| Code | Color |
|------|--------|
| 40   | Black |
| 41   | Red |
| 42   | Green |
| 43   | Yellow |
| 44   | Blue |
| 45   | Magenta |
| 46   | Cyan |
| 47   | White |

#### Bright BG (100–107)

| Code | Color |
|------|--------|
| 100  | Gray (Bright Black) |
| 101  | Bright Red |
| 102  | Bright Green |
| 103  | Bright Yellow |
| 104  | Bright Blue |
| 105  | Bright Magenta |
| 106  | Bright Cyan |
| 107  | Bright White |

---

## Text Attributes

| Code | Meaning |
|------|----------|
| 0    | Reset all attributes |
| 1    | Bold (maps FG to bright variant) |
| 4    | Underline |
| 5    | Slow Blink |
| 6    | Fast Blink |
| 7    | Inverse (Swap FG/BG) |
| 8    | Conceal (FG = BG) |
| 9    | Strikethrough |

---

## Attribute Reset Codes

| Code | Meaning |
|------|----------|
| 22   | Reset bold |
| 24   | Reset underline |
| 25   | Reset blink |
| 27   | Reset inverse |
| 28   | Reset conceal |
| 29   | Reset strikethrough |

---

## Combined Usage

Foreground and background colors may be combined:

```
CSI <fg> ; <bg> m
```

Example:

```
CSI 31;47m
```

Red foreground on white background.

Reset attributes:

```
CSI 0m
```

---

## 256-Color Mode

### `CSI 38;5;N m` — Set 256-color Foreground

### `CSI 48;5;N m` — Set 256-color Background

Color index ranges:
- 0-7: Standard ANSI colors
- 8-15: Bright ANSI colors
- 16-231: 6×6×6 color cube (R×36 + G×6 + B + 16)
- 232-255: Grayscale ramp (24 shades)

---

## Truecolor Mode

### `CSI 38;2;R;G;B m` — Set Truecolor Foreground

### `CSI 48;2;R;G;B m` — Set Truecolor Background

R, G, B values are 0-255. Mapped to the nearest 256-color palette entry
using 6-level quantization per channel into the 6×6×6 color cube.

---

## Default Color Codes

| Code | Meaning |
|------|----------|
| 39   | Default foreground color (index 7) |
| 49   | Default background color (index 0) |

---

## Supported Color Capability Summary

- 256-color palette via BSRAM lookup
- 16 standard/bright ANSI colors (indices 0-15)
- 6×6×6 color cube (indices 16-231)
- 24-step grayscale ramp (indices 232-255)
- 256-color mode: `CSI 38;5;N m` / `CSI 48;5;N m`
- Truecolor mode: `CSI 38;2;R;G;B m` / `CSI 48;2;R;G;B m` (quantized to 256-color)
- Total combinations: 256 × 256 = 65,536 FG/BG pairings

---

## Cursor Position Report

### `CSI 6n`

Reports cursor position by transmitting:

```
ESC[n;mR
```

Where:
- `n` = row
- `m` = column

--

## Display size report

### `CSI 18 t`

Reports display size by transmitting:

```
CSI 8 ; rows ; cols t
```

---

## Terminal Version Report

### `CSI >q`

Terminal responds with:

```
ESC P>|Simple Terminal 1.0 ESC \
```

---

## Alt Screen Buffer

### `CSI ?1049h` — Switch to Alt Screen

- Saves main screen cursor position and attributes
- Activates alternate screen buffer (separate SDRAM region)
- Clears the alt screen
- Resets cursor to (0, 0)
- Scrolling is disabled in alt screen mode

### `CSI ?1049l` — Switch Back to Main Screen

- Deactivates alt screen buffer
- Restores saved cursor position and attributes
- Main screen content is preserved

Used by full-screen applications (vi, less, tmux, etc.)

---

# Character Set Modes

## `ESC (B` — Default Mapping (Unix / Standard)

| Code | Behavior |
|-------|----------|
| CR (0x0D) | Move cursor to column 0 |
| LF (0x0A) | If Newline Mode OFF → move down same column<br>If Newline Mode ON → move down and column 0 |
| DEL (0x7F) | If column > 0 → move back one column and replace with space |
| TAB (0x09) | Move to next tab stop (8, 16, 24, ...) |
| ESC ESC | Draw `←` glyph |
| All ESC sequences | Processed |

---

## `ESC (U` — Raw Mapping (PC-ANSI / BBS)

| Code | Behavior |
|-------|----------|
| CR, LF, TAB | Same behavior as Default Mapping |
| DEL (0x7F) | Draw House glyph `⌂` (do not delete) |
| Other control codes (0x01–0x1F) | Display CP437 glyphs (smilies, hearts, etc.) |
| ESC ESC | Draw `←` glyph |
| All ESC sequences | Processed |

---

# Mode Toggles

## Newline Mode

| Sequence      | Action |
|---------------|----------|
| `CSI 4 h`     | Insert Mode ON (characters insert, shift right) |
| `CSI 4 l`     | Insert Mode OFF (overwrite mode) |
| `CSI 20 h`    | Newline Mode ON (LF performs CR+LF) |
| `CSI 20 l`    | Newline Mode OFF (LF moves down only) |
| `CSI ?1 h`    | Application cursor keys ON |
| `CSI ?1 l`    | Application cursor keys OFF |
| `CSI ?3 h`    | 132 column mode
| `CSI ?3 l`    | 80 column mode
| `CSI ?90;n h` | n column mode n < 10 > 142 are ignored
| `CSI ?7 h`    | Auto Wrap ON — When text reaches the last column, the next printable character wraps to the beginning of the next line (scrolling if needed). |
| `CSI ?7 l`    | Auto Wrap OFF — When text reaches the last column, additional printable characters overwrite the last column without wrapping. |
| `CSI ?12 h`   | Blink cursor |
| `CSI ?12 l`   | No blink cursor |
| `CSI ?25 h`   | Show cursor |
| `CSI ?25 l`   | Hide cursor |

---

# Test Patters

### Request

```
ESC P TEST1 ESC \
```

- Clear entire screen buffer
- Reset scrollback
- Move cursor to 1;1
- Disable wrap temporarily while drawing
- Draw full-screen ruler grid

# EDID Data Request

### Request

```
ESC P EDID ESC \
```

### Responses

| Sequence | Description |
|------------|--------------|
| `ESC P1 EDID=DATA ESC\` | Monitor Name (FC field) |
| `ESC P2 EDID=DATA ESC\` | Monitor Serial Number (FF field) |
| `ESC P3 EDID=DATA ESC\` | Unspecified Text (FE field) |
| `ESC P4 EDID=DATA ESC\` | Physical Screen Size |

---

# Mouse Tracking

## Enable Mouse Tracking

```
CSI ?1000h
CSI ?1006h
```

When recieved these should be echoed back to the user over the UART.

## Disable Mouse Tracking (Return to Normal Scrolling)

```
CSI ?1000l
CSI ?1006l
```

When recieved these should be echoed back to the user over the UART.

# Unicode Support

~2,613 glyphs across 32 font banks. Single-width glyphs use 8x16 bitmaps.
Wide glyphs (Hiragana, Katakana, CJK punctuation, Kanji, Emoji) use 16x16
bitmaps split into left/right halves stored in separate banks.

Font data is LZSS-compressed into 9 BSRAM ROM banks and written to SDRAM
at startup (65,536 bytes decompressed, 32 banks x 128 x 16).

A unified lookup ROM (char_lut, 512 entries x 24-bit) maps Unicode
codepoints to internal slots via binary search. Kanji (U+4E00-9FFF) map
to slots 0-228 (internal U+4000+), emoji (BMP + non-BMP) map to slots
229+ (internal U+5000+). The UTF-8 decoder handles 2/3/4-byte sequences.

### Single-width banks (8x16)

  ┌──────┬─────────────┬──────────────────┬───────────────────────────────────┐
  │ Bank │    Range    │ Non-zero entries │            Content                │
  ├──────┼─────────────┼──────────────────┼───────────────────────────────────┤
  │ 0    │ U+0000-007F │ 1083             │ ASCII                             │
  ├──────┼─────────────┼──────────────────┼───────────────────────────────────┤
  │ 1    │ U+0080-00FF │ 891              │ Latin-1 Supplement                │
  ├──────┼─────────────┼──────────────────┼───────────────────────────────────┤
  │ 2    │ U+0100-017F │ 1378             │ Latin Extended-A                  │
  ├──────┼─────────────┼──────────────────┼───────────────────────────────────┤
  │ 3    │ U+0180-01FF │ 1322             │ Latin Extended-B lo               │
  ├──────┼─────────────┼──────────────────┼───────────────────────────────────┤
  │ 4    │ U+0200-027F │ 829              │ Latin Extended-B hi               │
  ├──────┼─────────────┼──────────────────┼───────────────────────────────────┤
  │ 5    │ U+0300-037F │ 97               │ Combining Diacriticals + Greek    │
  ├──────┼─────────────┼──────────────────┼───────────────────────────────────┤
  │ 6    │ U+0380-03FF │ 1076             │ Greek and Coptic                  │
  ├──────┼─────────────┼──────────────────┼───────────────────────────────────┤
  │ 7    │ U+0400-047F │ 1088             │ Cyrillic                          │
  ├──────┼─────────────┼──────────────────┼───────────────────────────────────┤
  │ 8    │ U+0480-04FF │ 1147             │ Cyrillic Extended                 │
  ├──────┼─────────────┼──────────────────┼───────────────────────────────────┤
  │ 9    │ U+0500-057F │ 1197             │ Cyrillic Supplement + Armenian    │
  ├──────┼─────────────┼──────────────────┼───────────────────────────────────┤
  │ 10   │ U+2000-207F │ 230              │ General Punctuation               │
  ├──────┼─────────────┼──────────────────┼───────────────────────────────────┤
  │ 11   │ U+2080-20FF │ 212              │ Super/Sub/Currency                │
  ├──────┼─────────────┼──────────────────┼───────────────────────────────────┤
  │ 12   │ U+2100-217F │ 275              │ Number Forms                      │
  ├──────┼─────────────┼──────────────────┼───────────────────────────────────┤
  │ 13   │ U+2180-21FF │ 99               │ Arrows                            │
  ├──────┼─────────────┼──────────────────┼───────────────────────────────────┤
  │ 14   │ U+2200-227F │ 220              │ Math Operators lo                 │
  ├──────┼─────────────┼──────────────────┼───────────────────────────────────┤
  │ 15   │ U+2280-22FF │ 46               │ Math Operators hi                 │
  ├──────┼─────────────┼──────────────────┼───────────────────────────────────┤
  │ 16   │ U+2500-257F │ 1438             │ Box Drawing                       │
  ├──────┼─────────────┼──────────────────┼───────────────────────────────────┤
  │ 17   │ U+2580-25FF │ 402              │ Block Elements                    │
  ├──────┼─────────────┼──────────────────┼───────────────────────────────────┤
  │ 22   │ U+1E00-1E7F │ 1413             │ Latin Extended Additional lo      │
  ├──────┼─────────────┼──────────────────┼───────────────────────────────────┤
  │ 23   │ U+1E80-1EFF │ 1389             │ Latin Extended Additional hi      │
  ├──────┼─────────────┼──────────────────┼───────────────────────────────────┤
  │ 24   │ U+2800-287F │ 704              │ Braille Patterns lo               │
  ├──────┼─────────────┼──────────────────┼───────────────────────────────────┤
  │ 25   │ U+2880-28FF │ 832              │ Braille Patterns hi               │
  └──────┴─────────────┴──────────────────┴───────────────────────────────────┘

### Wide-character banks (16x16, left + right halves)

  ┌──────┬─────────────┬──────────────────┬───────────────────────────────────┐
  │ Bank │    Range    │ Non-zero entries │            Content                │
  ├──────┼─────────────┼──────────────────┼───────────────────────────────────┤
  │ 18   │ U+3000-307F │ 1224             │ CJK Punctuation + Hiragana (left) │
  ├──────┼─────────────┼──────────────────┼───────────────────────────────────┤
  │ 19   │ U+3080-30FF │ 1082             │ Hiragana end + Katakana (left)    │
  ├──────┼─────────────┼──────────────────┼───────────────────────────────────┤
  │ 20   │ U+3000-307F │ 999              │ CJK Punctuation + Hiragana (right)│
  ├──────┼─────────────┼──────────────────┼───────────────────────────────────┤
  │ 21   │ U+3080-30FF │ 989              │ Hiragana end + Katakana (right)   │
  ├──────┼─────────────┼──────────────────┼───────────────────────────────────┤
  │ 26   │ U+4000-407F │ 1250             │ Kanji lo (left)                   │
  ├──────┼─────────────┼──────────────────┼───────────────────────────────────┤
  │ 27   │ U+4080-40FF │ 998              │ Kanji hi (left)                   │
  ├──────┼─────────────┼──────────────────┼───────────────────────────────────┤
  │ 28   │ U+4000-407F │ 1201             │ Kanji lo (right)                  │
  ├──────┼─────────────┼──────────────────┼───────────────────────────────────┤
  │ 29   │ U+4080-40FF │ 987              │ Kanji hi (right)                  │
  ├──────┼─────────────┼──────────────────┼───────────────────────────────────┤
  │ 30   │ U+5000-507F │ (varies)         │ Emoji (left)                      │
  ├──────┼─────────────┼──────────────────┼───────────────────────────────────┤
  │ 31   │ U+5000-507F │ (varies)         │ Emoji (right)                     │
  └──────┴─────────────┴──────────────────┴───────────────────────────────────┘

Wide characters occupy 2 terminal columns. The left half is stored with the
original Unicode code point, the right half with bit 15 set (code | 0x8000).
The font cache maps bit-15 codes to the right-half banks (20-21, 28-29, 31).
  