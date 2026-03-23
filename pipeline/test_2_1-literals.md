# Test Plan: 2.1 Literals

**Specification Reference:** Section 2.1 of jz-hdl-specification.md

## 1. Objective

Verify that the lexer and parser correctly handle sized literals with syntax `<width>'<base><value>`, enforce base-specific digit rules (binary: 0/1/x/z; decimal: 0-9; hex: 0-9/A-F), compute intrinsic bit-width correctly, apply extension rules (zero/x/z extension based on MSB), detect overflow, reject unsized literals, and enforce x/z usage restrictions.

## 2. Instrumentation Strategy

- **Span: `lexer.parse_literal`** — Trace literal tokenization; attributes: `declared_width`, `base`, `value_str`, `intrinsic_width`.
- **Span: `sem.literal_validate`** — Semantic validation; attributes: `has_x`, `has_z`, `overflow`, `context` (reset/init/expr).
- **Event: `literal.overflow`** — Intrinsic width exceeds declared width.
- **Event: `literal.unsized`** — Missing width prefix.
- **Event: `literal.invalid_digit`** — Digit not valid for base.
- **Coverage Hook:** Ensure all bases (b/d/h) and all extension types (0-ext, x-ext, z-ext) are tested.

## 3. Test Scenarios

### 3.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Binary exact width | `8'b1100_0011` | 8-bit, value 0xC3 |
| 2 | Binary with underscores | `8'b1100_0011` | Underscores ignored |
| 3 | Binary zero-extend | `8'b101` | Extended to `8'b0000_0101` |
| 4 | Binary x-extend | `4'bx` | Extended to `4'bxxxx` |
| 5 | Binary z-extend | `8'bz` | Extended to `8'bzzzz_zzzz` |
| 6 | Decimal | `8'd255` | 8-bit, value 255 |
| 7 | Hex | `8'hFF` | 8-bit, value 255 |
| 8 | Hex zero-extend | `8'hF` | Extended to `8'h0F` |
| 9 | CONST width | `WIDTH'hAB` with CONST WIDTH=8 | 8-bit hex |
| 10 | Decimal zero | `8'd0` | 8-bit, all zeros |
| 11 | Hex with mixed case | `8'hAb` | Valid, case-insensitive digits |
| 12 | 32-bit hex zeros | `32'h00` | 32-bit, all zeros |

### 3.2 Boundary/Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Width = 1 | `1'b0` | Valid, 1-bit |
| 2 | Large width | `256'd0` | Valid, 256-bit |
| 3 | Binary exact intrinsic = declared | `4'b1010` | Valid, no extension needed |
| 4 | Hex intrinsic width edge | `25'h1FFFFFF` | Intrinsic 25 bits, fits exactly |
| 5 | Decimal max for width | `8'd255` | Intrinsic 8 bits, fits in 8 |
| 6 | Hex leading zeros | `8'h00FF` | Intrinsic 8 bits (leading zeros don't count) |
| 7 | Single x in binary | `8'bx` | Extended to `8'bxxxx_xxxx` |

### 3.3 Negative Testing

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Unsized literal | `'hFF` | Error: LIT_UNSIZED |
| 2 | Bare integer | `42` in expression | Error: LIT_BARE_INTEGER |
| 3 | Width zero | `0'b0` | Error: LIT_WIDTH_ZERO |
| 4 | Binary overflow | `4'b10101` | Error: LIT_OVERFLOW (intrinsic 5 > declared 4) |
| 5 | Hex overflow | `25'h3FFFFFF` | Error: LIT_OVERFLOW (intrinsic 26 > declared 25) |
| 6 | Decimal overflow | `4'd16` | Error: LIT_OVERFLOW (intrinsic 5 > declared 4) |
| 7 | Decimal with x | `8'd10x` | Error: LIT_DECIMAL_HAS_XZ |
| 8 | Hex with z | `8'hz0` | Error: LIT_HEX_HAS_XZ |
| 9 | Hex with x | `8'hFx` | Error: LIT_HEX_HAS_XZ |
| 10 | Invalid digit for base | `8'b012` | Error: LIT_INVALID_DIGIT_FOR_BASE |
| 11 | x in register reset | `REGISTER { r [8] = 8'bxxxx_xxxx; }` | Error: LIT_RESET_HAS_X |
| 12 | z in register reset | `REGISTER { r [8] = 8'bzzzz_zzzz; }` | Error: LIT_RESET_HAS_Z |
| 13 | Underscore at start | `8'b_1100` | Error: invalid literal |
| 14 | Underscore at end | `8'b1100_` | Error: invalid literal |
| 15 | Undefined CONST in width | `UNDEF'hFF` | Error: undefined CONST |

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | `'hFF` | Error: unsized | LIT_UNSIZED | No width prefix |
| 2 | `42` in expr | Error: bare int | LIT_BARE_INTEGER | Must use sized literal |
| 3 | `0'b0` | Error: zero width | LIT_WIDTH_ZERO | Width must be positive |
| 4 | `4'b10101` | Error: overflow | LIT_OVERFLOW | 5 bits > 4-bit width |
| 5 | `8'd10x` | Error: x in decimal | LIT_DECIMAL_HAS_XZ | x/z not in decimal |
| 6 | `8'hFx` | Error: x in hex | LIT_HEX_HAS_XZ | x/z not in hex |
| 7 | `8'b012` | Error: invalid digit | LIT_INVALID_DIGIT_FOR_BASE | '2' invalid in binary |
| 8 | `8'bxxxx_xxxx` in reg reset | Error: x in reset | LIT_RESET_HAS_X | Reset must be 0/1 |
| 9 | `8'b1100_0011` | Valid: 8-bit binary | — | Happy path |
| 10 | `4'bx` | Valid: extended to `4'bxxxx` | — | x-extension |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `lexer.c` | Tokenizes literals | Direct unit test with source strings; no mocks needed |
| `sem_literal.c` | Semantic validation of literal context | Feed literal AST nodes with context |
| `const_eval.c` | Evaluates CONST expressions in width | Mock known CONST values |
| `parser_expressions.c` | Parses literal into AST | Feed token stream |
| `diagnostic.c` | Error collection | Capture and verify rule IDs |
| `rules.c` | Rule ID registry | No mock needed |

## 6. Rules Matrix

### 6.1 Rules Tested

| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| LIT_UNSIZED | Unsized literal not permitted | Neg 1 |
| LIT_BARE_INTEGER | Bare integer in runtime expression | Neg 2 |
| LIT_WIDTH_ZERO | Width is zero | Neg 3 |
| LIT_OVERFLOW | Intrinsic width exceeds declared width | Neg 4, 5, 6 |
| LIT_DECIMAL_HAS_XZ | x or z in decimal literal | Neg 7 |
| LIT_HEX_HAS_XZ | x or z in hex literal | Neg 8, 9 |
| LIT_INVALID_DIGIT_FOR_BASE | Invalid digit for base | Neg 10 |
| LIT_RESET_HAS_X | x in register reset literal | Neg 11 |
| LIT_RESET_HAS_Z | z in register reset literal | Neg 12 |

### 6.2 Rules Missing

| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| LIT_UNDERSCORE_POSITION | S2.1 "cannot be first or last character" | No explicit rule for underscore at start/end of value |
| LIT_MEM_INIT_HAS_XZ | S2.1 "MEM initialization literals" | May need separate rule for x/z in MEM init vs register reset |
