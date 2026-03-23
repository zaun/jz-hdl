# Test Plan: 2.1 Literals

**Spec Ref:** Section 2.1 of jz-hdl-specification.md

## 1. Objective

Verify sized literal syntax (`<width>'<base><value>`), base-specific digit rules (binary: 0/1/x/z; decimal: 0-9; hex: 0-9/A-F/x/z), overflow detection (intrinsic width exceeds declared width), extension rules (zero/x/z extension based on MSB), underscore formatting rules, and rejection of unsized and bare integer literals.

## 2. Test Scenarios

### 2.1 Happy Path

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

### 2.2 Error Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Unsized literal | `'hFF` | Error: LIT_UNSIZED |
| 2 | Bare integer | `42` in expression | Error: LIT_BARE_INTEGER |
| 3 | Width zero | `0'b0` | Error: LIT_WIDTH_NOT_POSITIVE |
| 4 | Binary overflow | `4'b10101` | Error: LIT_OVERFLOW (intrinsic 5 > declared 4) |
| 5 | Hex overflow | `25'h3FFFFFF` | Error: LIT_OVERFLOW (intrinsic 26 > declared 25) |
| 6 | Decimal overflow | `4'd16` | Error: LIT_OVERFLOW (intrinsic 5 > declared 4) |
| 7 | Decimal with x | `8'd10x` | Error: LIT_DECIMAL_HAS_XZ |
| 8 | Invalid digit for base | `8'b012` | Error: LIT_INVALID_DIGIT_FOR_BASE |
| 9 | Underscore at start | `8'b_1100` | Error: LIT_UNDERSCORE_AT_EDGES |
| 10 | Underscore at end | `8'b1100_` | Error: LIT_UNDERSCORE_AT_EDGES |
| 11 | Undefined CONST in width | `UNDEF'hFF` | Error: LIT_UNDEFINED_CONST_WIDTH |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Width = 1 | `1'b0` | Valid, 1-bit |
| 2 | Large width | `256'd0` | Valid, 256-bit |
| 3 | Binary exact intrinsic = declared | `4'b1010` | Valid, no extension needed |
| 4 | Hex intrinsic width edge | `25'h1FFFFFF` | Intrinsic 25 bits, fits exactly |
| 5 | Decimal max for width | `8'd255` | Intrinsic 8 bits, fits in 8 |
| 6 | Hex leading zeros | `8'h00FF` | Intrinsic 8 bits (leading zeros don't count) |
| 7 | Single x in binary | `8'bx` | Extended to `8'bxxxx_xxxx` |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| 1 | `'hFF` | Error: unsized | LIT_UNSIZED | error | No width prefix |
| 2 | `42` in expr | Error: bare int | LIT_BARE_INTEGER | error | Must use sized literal |
| 3 | `0'b0` | Error: zero width | LIT_WIDTH_NOT_POSITIVE | error | Width must be positive |
| 4 | `4'b10101` | Error: overflow | LIT_OVERFLOW | error | 5 bits > 4-bit width |
| 5 | `8'd10x` | Error: x in decimal | LIT_DECIMAL_HAS_XZ | error | x/z not in decimal |
| 6 | `8'b012` | Error: invalid digit | LIT_INVALID_DIGIT_FOR_BASE | error | '2' invalid in binary |
| 7 | `8'b_1100` | Error: underscore edge | LIT_UNDERSCORE_AT_EDGES | error | Underscore at start of value |
| 8 | `UNDEF'hFF` | Error: undefined const | LIT_UNDEFINED_CONST_WIDTH | error | Width uses undefined CONST |
| 9 | `8'b1100_0011` | Valid: 8-bit binary | -- | -- | Happy path |
| 10 | `4'bx` | Valid: extended to `4'bxxxx` | -- | -- | x-extension |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 2_1_LIT_BARE_INTEGER-bare_int_in_expression.jz | LIT_BARE_INTEGER | Bare integer used in runtime expression |
| 2_1_LIT_DECIMAL_HAS_XZ-xz_in_decimal.jz | LIT_DECIMAL_HAS_XZ | x or z digit in decimal literal |
| 2_1_LIT_INVALID_DIGIT_FOR_BASE-invalid_digits.jz | LIT_INVALID_DIGIT_FOR_BASE | Digit not valid for base |
| 2_1_LIT_OVERFLOW-intrinsic_exceeds_declared.jz | LIT_OVERFLOW | Intrinsic width exceeds declared width |
| 2_1_LIT_UNDEFINED_CONST_WIDTH-undefined_const.jz | LIT_UNDEFINED_CONST_WIDTH | Width uses undefined CONST name |
| 2_1_LIT_UNDERSCORE_AT_EDGES-underscore_position.jz | LIT_UNDERSCORE_AT_EDGES | Underscore at first or last position |
| 2_1_LIT_UNSIZED-missing_width_prefix.jz | LIT_UNSIZED | Unsized literal missing width prefix |
| 2_1_LIT_WIDTH_NOT_POSITIVE-zero_width.jz | LIT_WIDTH_NOT_POSITIVE | Literal with zero or negative width |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| LIT_UNSIZED | error | Unsized literal not permitted | Error 1; 2_1_LIT_UNSIZED-missing_width_prefix.jz |
| LIT_BARE_INTEGER | error | Bare integer in runtime expression | Error 2; 2_1_LIT_BARE_INTEGER-bare_int_in_expression.jz |
| LIT_UNDERSCORE_AT_EDGES | error | Underscore at first or last character of value | Error 9, 10; 2_1_LIT_UNDERSCORE_AT_EDGES-underscore_position.jz |
| LIT_UNDEFINED_CONST_WIDTH | error | Width uses undefined CONST name | Error 11; 2_1_LIT_UNDEFINED_CONST_WIDTH-undefined_const.jz |
| LIT_WIDTH_NOT_POSITIVE | error | Width is non-positive or non-integer | Error 3; 2_1_LIT_WIDTH_NOT_POSITIVE-zero_width.jz |
| LIT_OVERFLOW | error | Intrinsic width exceeds declared width | Error 4, 5, 6; 2_1_LIT_OVERFLOW-intrinsic_exceeds_declared.jz |
| LIT_DECIMAL_HAS_XZ | error | x or z digit in decimal literal | Error 7; 2_1_LIT_DECIMAL_HAS_XZ-xz_in_decimal.jz |
| LIT_INVALID_DIGIT_FOR_BASE | error | Digit not valid for base | Error 8; 2_1_LIT_INVALID_DIGIT_FOR_BASE-invalid_digits.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| -- | -- | All rules from this section have validation tests |
