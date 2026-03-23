# Test Plan: 5.5 Intrinsic Operators

**Specification Reference:** Section 5.5 (5.5.1-5.5.28) of jz-hdl-specification.md

## 1. Objective

Verify all 28 intrinsic operators: their result widths, operand constraints, compile-time vs hardware-elaborating behavior, and correct error reporting for invalid arguments, out-of-range indices, width mismatches, and context violations.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | `uadd(a,b)` | 8-bit + 8-bit | Result width 9 (MAX+1), unsigned add with carry |
| 2 | `sadd(a,b)` | 8-bit + 8-bit | Result width 9, signed add with carry |
| 3 | `umul(a,b)` | 8-bit x 8-bit | Result width 16 (2xMAX), unsigned full product |
| 4 | `smul(a,b)` | 8-bit x 8-bit | Result width 16, signed full product |
| 5 | `clog2(256)` | Compile-time constant | Result 8 |
| 6 | `gbit(src,idx)` | 8-bit, 3-bit idx | Result width 1 |
| 7 | `sbit(src,idx,val)` | 8-bit, 3-bit, 1-bit | Result width 8 |
| 8 | `gslice(src,idx,W)` | 16-bit, 4-bit, W=4 | Result width 4 |
| 9 | `sslice(src,idx,W,val)` | 16-bit, 4-bit, W=4, 4-bit | Result width 16 |
| 10 | `widthof(sig)` | Any signal | Compile-time integer |
| 11 | `lit(W,V)` | Width, value | W bits |
| 12 | `oh2b(src)` | 8-bit one-hot | Result width 3 (clog2(8)) |
| 13 | `b2oh(idx,W)` | 3-bit, W=8 | Result width 8 |
| 14 | `prienc(src)` | 8-bit | Result width 3 (clog2(8)) |
| 15 | `lzc(src)` | 8-bit | Result width 4 (clog2(8)+1) |
| 16 | `usub(a,b)` | 8-bit - 8-bit | Result width 9 |
| 17 | `ssub(a,b)` | 8-bit - 8-bit | Result width 9 |
| 18 | `abs(a)` | 8-bit signed | Result width 8 |
| 19 | `umin(a,b)` | 8-bit, 8-bit | Result width 8 |
| 20 | `umax(a,b)` | 8-bit, 8-bit | Result width 8 |
| 21 | `smin(a,b)` | 8-bit, 8-bit | Result width 8 |
| 22 | `smax(a,b)` | 8-bit, 8-bit | Result width 8 |
| 23 | `popcount(src)` | 8-bit | Result width 4 (clog2(8)+1) |
| 24 | `reverse(src)` | 8-bit | Result width 8 |
| 25 | `bswap(src)` | 16-bit | Result width 16 |
| 26 | `reduce_and(src)` | 8-bit | Result width 1 |
| 27 | `reduce_or(src)` | 8-bit | Result width 1 |
| 28 | `reduce_xor(src)` | 8-bit | Result width 1 |

### 2.2 Error Cases

| # | Test Case | Input | Expected | Rule ID |
|---|-----------|-------|----------|---------|
| 1 | clog2 zero argument | `clog2(0)` | Error | CLOG2_NONPOSITIVE_ARG |
| 2 | clog2 runtime expression | `clog2(wire_val)` | Error | CLOG2_INVALID_CONTEXT |
| 3 | widthof runtime expression | `widthof(a + b)` | Error | WIDTHOF_INVALID_CONTEXT |
| 4 | widthof invalid target | `widthof(non_signal)` | Error | WIDTHOF_INVALID_TARGET |
| 5 | widthof invalid syntax | `widthof(a[3:0])` | Error | WIDTHOF_INVALID_SYNTAX |
| 6 | widthof unresolvable width | `widthof(unresolved)` | Error | WIDTHOF_WIDTH_NOT_RESOLVABLE |
| 7 | Function result truncated | Intrinsic result assigned to narrower target | Error | FUNC_RESULT_TRUNCATED_SILENTLY |
| 8 | lit width invalid | `lit(0, 5)` | Error | LIT_WIDTH_INVALID |
| 9 | lit value invalid | `lit(4, -1)` | Error | LIT_VALUE_INVALID |
| 10 | lit value overflow | `lit(4, 20)` | Error | LIT_VALUE_OVERFLOW |
| 11 | lit invalid context | `lit()` in non-constant context | Error | LIT_INVALID_CONTEXT |
| 12 | sbit set not 1-bit | `sbit(src, idx, 8'd5)` | Error | SBIT_SET_WIDTH_NOT_1 |
| 13 | gbit index out of range | `gbit(8-bit, 8)` | Error | GBIT_INDEX_OUT_OF_RANGE |
| 14 | sbit index out of range | `sbit(8-bit, 8, val)` | Error | SBIT_INDEX_OUT_OF_RANGE |
| 15 | gslice index out of range | `gslice(8-bit, 8, 4)` | Error | GSLICE_INDEX_OUT_OF_RANGE |
| 16 | gslice zero width | `gslice(src, idx, 0)` | Error | GSLICE_WIDTH_INVALID |
| 17 | sslice index out of range | `sslice(8-bit, 8, 4, val)` | Error | SSLICE_INDEX_OUT_OF_RANGE |
| 18 | sslice zero width | `sslice(src, idx, 0, val)` | Error | SSLICE_WIDTH_INVALID |
| 19 | sslice value width mismatch | `sslice(src, idx, 4, 8-bit)` | Error | SSLICE_VALUE_WIDTH_MISMATCH |
| 20 | oh2b input too narrow | `oh2b(1-bit)` | Error | OH2B_INPUT_TOO_NARROW |
| 21 | b2oh width invalid | `b2oh(idx, 1)` | Error | B2OH_WIDTH_INVALID |
| 22 | prienc input too narrow | `prienc(1-bit)` | Error | PRIENC_INPUT_TOO_NARROW |
| 23 | bswap non-byte-aligned | `bswap(7-bit)` | Error | BSWAP_WIDTH_NOT_BYTE_ALIGNED |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | clog2(1) | Compile-time | Result = 0 |
| 2 | popcount all-ones | `popcount(8'hFF)` | Result = 8 |
| 3 | popcount all-zeros | `popcount(8'h00)` | Result = 0 |
| 4 | reverse 1-bit | `reverse(1'b1)` | Result = `1'b1` |
| 5 | bswap 3 bytes | `bswap(24-bit)` | Valid, byte swap across 3 bytes |
| 6 | uadd max values | `uadd(8'hFF, 8'hFF)` | Result = 9'd510 |
| 7 | Different operand widths | `uadd(4'd5, 8'd10)` | Result width MAX(4,8)+1 = 9 |
| 8 | reduce_and all-ones | `reduce_and(8'hFF)` | Result = `1'b1` |
| 9 | reduce_and not all-ones | `reduce_and(8'hFE)` | Result = `1'b0` |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| 1 | `clog2(0)` | Compile error | CLOG2_NONPOSITIVE_ARG | error | Argument must be positive |
| 2 | `clog2(wire_val)` | Compile error | CLOG2_INVALID_CONTEXT | error | Compile-time only |
| 3 | `widthof(a + b)` | Compile error | WIDTHOF_INVALID_CONTEXT | error | Compile-time only |
| 4 | `lit(0, 5)` | Compile error | LIT_WIDTH_INVALID | error | Width must be positive |
| 5 | `lit(4, 20)` | Compile error | LIT_VALUE_OVERFLOW | error | Value exceeds width |
| 6 | `sbit(src, idx, 8'd5)` | Compile error | SBIT_SET_WIDTH_NOT_1 | error | Set value must be 1 bit |
| 7 | `gbit(8-bit, const 8)` | Compile error | GBIT_INDEX_OUT_OF_RANGE | error | Index >= source width |
| 8 | `gslice(src, idx, 0)` | Compile error | GSLICE_WIDTH_INVALID | error | Width must be positive |
| 9 | `sslice(src, idx, 4, 8-bit)` | Compile error | SSLICE_VALUE_WIDTH_MISMATCH | error | Value width must match W |
| 10 | `oh2b(1-bit)` | Compile error | OH2B_INPUT_TOO_NARROW | error | Source must be >= 2 bits |
| 11 | `b2oh(idx, 1)` | Compile error | B2OH_WIDTH_INVALID | error | Width must be >= 2 |
| 12 | `prienc(1-bit)` | Compile error | PRIENC_INPUT_TOO_NARROW | error | Source must be >= 2 bits |
| 13 | `bswap(7-bit)` | Compile error | BSWAP_WIDTH_NOT_BYTE_ALIGNED | error | Width must be multiple of 8 |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 5_5_CLOG2_NONPOSITIVE_ARG-zero_argument.jz | CLOG2_NONPOSITIVE_ARG | clog2() argument is zero |
| 5_5_CLOG2_INVALID_CONTEXT-runtime_expression.jz | CLOG2_INVALID_CONTEXT | clog2() used with runtime expression |
| 5_5_WIDTHOF_INVALID_CONTEXT-runtime_expression.jz | WIDTHOF_INVALID_CONTEXT | widthof() used with runtime expression |
| 5_5_LIT_WIDTH_INVALID-non_positive_width.jz | LIT_WIDTH_INVALID | lit() with non-positive width |
| 5_5_LIT_VALUE_OVERFLOW-value_exceeds_width.jz | LIT_VALUE_OVERFLOW | lit() value exceeds declared width |
| 5_5_SBIT_SET_WIDTH_NOT_1-non_unit_set_value.jz | SBIT_SET_WIDTH_NOT_1 | sbit() third argument not 1 bit |
| 5_5_GBIT_INDEX_OUT_OF_RANGE-const_index_exceeds_width.jz | GBIT_INDEX_OUT_OF_RANGE | gbit() index exceeds source width |
| 5_5_SBIT_INDEX_OUT_OF_RANGE-const_index_exceeds_width.jz | SBIT_INDEX_OUT_OF_RANGE | sbit() index exceeds source width |
| 5_5_GSLICE_INDEX_OUT_OF_RANGE-const_index_exceeds_width.jz | GSLICE_INDEX_OUT_OF_RANGE | gslice() index exceeds source width |
| 5_5_GSLICE_WIDTH_INVALID-zero_width_param.jz | GSLICE_WIDTH_INVALID | gslice() width parameter is zero |
| 5_5_SSLICE_INDEX_OUT_OF_RANGE-const_index_exceeds_width.jz | SSLICE_INDEX_OUT_OF_RANGE | sslice() index exceeds source width |
| 5_5_SSLICE_WIDTH_INVALID-zero_width_param.jz | SSLICE_WIDTH_INVALID | sslice() width parameter is zero |
| 5_5_SSLICE_VALUE_WIDTH_MISMATCH-value_width_mismatch.jz | SSLICE_VALUE_WIDTH_MISMATCH | sslice() value width does not match width parameter |
| 5_5_OH2B_INPUT_TOO_NARROW-single_bit_source.jz | OH2B_INPUT_TOO_NARROW | oh2b() source is only 1 bit |
| 5_5_B2OH_WIDTH_INVALID-width_below_two.jz | B2OH_WIDTH_INVALID | b2oh() width is below 2 |
| 5_5_PRIENC_INPUT_TOO_NARROW-single_bit_source.jz | PRIENC_INPUT_TOO_NARROW | prienc() source is only 1 bit |
| 5_5_BSWAP_WIDTH_NOT_BYTE_ALIGNED-non_byte_width.jz | BSWAP_WIDTH_NOT_BYTE_ALIGNED | bswap() source width not a multiple of 8 |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| CLOG2_NONPOSITIVE_ARG | error | S5.5.5 Argument to clog2() <= 0 | 5_5_CLOG2_NONPOSITIVE_ARG-zero_argument.jz |
| CLOG2_INVALID_CONTEXT | error | S5.5.5 clog2() used outside compile-time context | 5_5_CLOG2_INVALID_CONTEXT-runtime_expression.jz |
| WIDTHOF_INVALID_CONTEXT | error | S5.5.10 widthof() used outside compile-time context | 5_5_WIDTHOF_INVALID_CONTEXT-runtime_expression.jz |
| LIT_WIDTH_INVALID | error | S5.5.11 lit() width must be a positive integer | 5_5_LIT_WIDTH_INVALID-non_positive_width.jz |
| LIT_VALUE_OVERFLOW | error | S5.5.11 lit() value exceeds declared width | 5_5_LIT_VALUE_OVERFLOW-value_exceeds_width.jz |
| SBIT_SET_WIDTH_NOT_1 | error | S5.5.7 sbit() third argument must be width-1 | 5_5_SBIT_SET_WIDTH_NOT_1-non_unit_set_value.jz |
| GBIT_INDEX_OUT_OF_RANGE | error | S5.5.6 gbit() index out of range | 5_5_GBIT_INDEX_OUT_OF_RANGE-const_index_exceeds_width.jz |
| SBIT_INDEX_OUT_OF_RANGE | error | S5.5.7 sbit() index out of range | 5_5_SBIT_INDEX_OUT_OF_RANGE-const_index_exceeds_width.jz |
| GSLICE_INDEX_OUT_OF_RANGE | error | S5.5.8 gslice() index out of range | 5_5_GSLICE_INDEX_OUT_OF_RANGE-const_index_exceeds_width.jz |
| GSLICE_WIDTH_INVALID | error | S5.5.8 gslice() width must be positive | 5_5_GSLICE_WIDTH_INVALID-zero_width_param.jz |
| SSLICE_INDEX_OUT_OF_RANGE | error | S5.5.9 sslice() index out of range | 5_5_SSLICE_INDEX_OUT_OF_RANGE-const_index_exceeds_width.jz |
| SSLICE_WIDTH_INVALID | error | S5.5.9 sslice() width must be positive | 5_5_SSLICE_WIDTH_INVALID-zero_width_param.jz |
| SSLICE_VALUE_WIDTH_MISMATCH | error | S5.5.9 sslice() value width does not match width parameter | 5_5_SSLICE_VALUE_WIDTH_MISMATCH-value_width_mismatch.jz |
| OH2B_INPUT_TOO_NARROW | error | S5.5.12 oh2b() source must be >= 2 bits | 5_5_OH2B_INPUT_TOO_NARROW-single_bit_source.jz |
| B2OH_WIDTH_INVALID | error | S5.5.13 b2oh() width must be >= 2 | 5_5_B2OH_WIDTH_INVALID-width_below_two.jz |
| PRIENC_INPUT_TOO_NARROW | error | S5.5.14 prienc() source must be >= 2 bits | 5_5_PRIENC_INPUT_TOO_NARROW-single_bit_source.jz |
| BSWAP_WIDTH_NOT_BYTE_ALIGNED | error | S5.5.26 bswap() source width must be multiple of 8 | 5_5_BSWAP_WIDTH_NOT_BYTE_ALIGNED-non_byte_width.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| WIDTHOF_INVALID_TARGET | error | No dedicated validation test for widthof() on non-signal target |
| WIDTHOF_INVALID_SYNTAX | error | No dedicated validation test for widthof() with slice/concat argument |
| WIDTHOF_WIDTH_NOT_RESOLVABLE | error | No dedicated validation test for unresolvable widthof() target |
| FUNC_RESULT_TRUNCATED_SILENTLY | error | No dedicated validation test for intrinsic result truncation |
| LIT_VALUE_INVALID | error | No dedicated validation test for lit() with negative value |
| LIT_INVALID_CONTEXT | error | No dedicated validation test for lit() in non-constant context |
