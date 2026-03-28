# Test Plan: 5.5 Intrinsic Operators

**Specification Reference:** Section 5.5 (5.5.1-5.5.28) of jz-hdl-specification.md

## 1. Objective

Verify all 28 intrinsic operators: their result widths, operand constraints, compile-time vs hardware-elaborating behavior, and correct error reporting for invalid arguments, out-of-range indices, width mismatches, and context violations. Covers arithmetic (uadd, sadd, umul, smul, usub, ssub, abs), min/max (umin, umax, smin, smax), bit manipulation (gbit, sbit, gslice, sslice, reverse, bswap, popcount), encoding (oh2b, b2oh, prienc, lzc), reductions (reduce_and, reduce_or, reduce_xor), and compile-time (clog2, widthof, lit).

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
| 15 | `lzc(src)` | 8-bit | Result width 4 (clog2(9)) |
| 16 | `usub(a,b)` | 8-bit - 8-bit | Result width 9 |
| 17 | `ssub(a,b)` | 8-bit - 8-bit | Result width 9 |
| 18 | `abs(a)` | 8-bit signed | Result width 9 (W+1) |
| 19 | `umin(a,b)` | 8-bit, 8-bit | Result width 8 |
| 20 | `umax(a,b)` | 8-bit, 8-bit | Result width 8 |
| 21 | `smin(a,b)` | 8-bit, 8-bit | Result width 8 |
| 22 | `smax(a,b)` | 8-bit, 8-bit | Result width 8 |
| 23 | `popcount(src)` | 8-bit | Result width 4 (clog2(9)) |
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
| 3 | widthof runtime expression | `widthof(sig)` in runtime context | Error | WIDTHOF_INVALID_CONTEXT |
| 4 | widthof invalid target | `widthof(non_signal)` | Error | WIDTHOF_INVALID_TARGET |
| 5 | widthof invalid syntax | `widthof(a[3:0])` | Error | WIDTHOF_INVALID_SYNTAX |
| 6 | widthof unresolvable width | `widthof(unresolved)` | Error | WIDTHOF_WIDTH_NOT_RESOLVABLE |
| 7 | Function result truncated | Intrinsic result assigned to narrower target | Error | FUNC_RESULT_TRUNCATED_SILENTLY |
| 8 | lit width invalid | `lit(0, 5)` | Error | LIT_WIDTH_INVALID |
| 9 | lit value invalid | `lit(4, -1)` | Error | LIT_VALUE_INVALID |
| 10 | lit value overflow | `lit(4, 20)` | Error | LIT_VALUE_OVERFLOW |
| 11 | lit invalid context | `lit()` in CONST block | Error | LIT_INVALID_CONTEXT |
| 12 | sbit set not 1-bit | `sbit(src, idx, 8'd5)` | Error | SBIT_SET_WIDTH_NOT_1 |
| 13 | gbit index out of range | `gbit(8-bit, const 8)` | Error | GBIT_INDEX_OUT_OF_RANGE |
| 14 | sbit index out of range | `sbit(8-bit, const 8, val)` | Error | SBIT_INDEX_OUT_OF_RANGE |
| 15 | gslice index out of range | `gslice(8-bit, const 8, 4)` | Error | GSLICE_INDEX_OUT_OF_RANGE |
| 16 | gslice zero width | `gslice(src, idx, 0)` | Error | GSLICE_WIDTH_INVALID |
| 17 | sslice index out of range | `sslice(8-bit, const 8, 4, val)` | Error | SSLICE_INDEX_OUT_OF_RANGE |
| 18 | sslice zero width | `sslice(src, idx, 0, val)` | Error | SSLICE_WIDTH_INVALID |
| 19 | sslice value width mismatch | `sslice(src, idx, 4, 8-bit)` | Error | SSLICE_VALUE_WIDTH_MISMATCH |
| 20 | oh2b input too narrow | `oh2b(1-bit)` | Error | OH2B_INPUT_TOO_NARROW |
| 21 | b2oh width invalid | `b2oh(idx, 1)` | Error | B2OH_WIDTH_INVALID |
| 22 | prienc input too narrow | `prienc(1-bit)` | Error | PRIENC_INPUT_TOO_NARROW |
| 23 | bswap non-byte-aligned | `bswap(7-bit)` | Error | BSWAP_WIDTH_NOT_BYTE_ALIGNED |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | clog2(1) | Compile-time | Result = 1 |
| 2 | popcount all-ones | `popcount(8'hFF)` | Result = 8 |
| 3 | popcount all-zeros | `popcount(8'h00)` | Result = 0 |
| 4 | reverse 1-bit | `reverse(1'b1)` | Result = `1'b1` |
| 5 | bswap 3 bytes | `bswap(24-bit)` | Valid, byte swap across 3 bytes |
| 6 | uadd max values | `uadd(8'hFF, 8'hFF)` | Result = 9'd510 |
| 7 | Different operand widths | `uadd(4'd5, 8'd10)` | Result width MAX(4,8)+1 = 9 |
| 8 | reduce_and all-ones | `reduce_and(8'hFF)` | Result = `1'b1` |
| 9 | reduce_and not all-ones | `reduce_and(8'hFE)` | Result = `1'b0` |
| 10 | Nested intrinsics | `uadd(gbit(a, i), gbit(b, j))` | Valid, nesting allowed |
| 11 | lit with CONST width | `lit(MY_WIDTH, 0)` | Valid, compile-time width |
| 12 | widthof in CONST init | `CONST { W = widthof(sig); }` | Valid, compile-time |
| 13 | lzc all-zeros | `lzc(8'h00)` | Result = 8 |
| 14 | abs most-negative value | `abs(8'h80)` | Result MSB = 1 (overflow) |

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1 | clog2 with zero argument | `clog2(0)` | CLOG2_NONPOSITIVE_ARG | error |
| 2 | clog2 with runtime wire | `clog2(wire_val)` in width bracket | CLOG2_INVALID_CONTEXT | error |
| 3 | widthof in runtime expression | `a = widthof(b) ? x : y;` | WIDTHOF_INVALID_CONTEXT | error |
| 4 | widthof on non-signal | `widthof(CONST_NAME)` on a const | WIDTHOF_INVALID_TARGET | error |
| 5 | widthof with slice arg | `widthof(sig[3:0])` | WIDTHOF_INVALID_SYNTAX | error |
| 6 | widthof unresolvable | `widthof(sig)` where sig width is unresolved | WIDTHOF_WIDTH_NOT_RESOLVABLE | error |
| 7 | Intrinsic result truncated | `8-bit <= uadd(8-bit, 8-bit)` (9 into 8) | FUNC_RESULT_TRUNCATED_SILENTLY | error |
| 8 | lit with zero width | `lit(0, 5)` | LIT_WIDTH_INVALID | error |
| 9 | lit with negative value | `lit(4, -1)` | LIT_VALUE_INVALID | error |
| 10 | lit value exceeds width | `lit(4, 16)` (16 >= 2^4) | LIT_VALUE_OVERFLOW | error |
| 11 | lit in CONST block | `CONST { X = lit(4, 3); }` | LIT_INVALID_CONTEXT | error |
| 12 | sbit set value not 1-bit | `sbit(src, idx, 8'd5)` | SBIT_SET_WIDTH_NOT_1 | error |
| 13 | gbit static index >= W | `gbit(8-bit, const 8)` | GBIT_INDEX_OUT_OF_RANGE | error |
| 14 | sbit static index >= W | `sbit(8-bit, const 8, 1'b0)` | SBIT_INDEX_OUT_OF_RANGE | error |
| 15 | gslice static index >= W | `gslice(8-bit, const 8, 4)` | GSLICE_INDEX_OUT_OF_RANGE | error |
| 16 | gslice width = 0 | `gslice(src, idx, 0)` | GSLICE_WIDTH_INVALID | error |
| 17 | sslice static index >= W | `sslice(8-bit, const 8, 4, val)` | SSLICE_INDEX_OUT_OF_RANGE | error |
| 18 | sslice width = 0 | `sslice(src, idx, 0, val)` | SSLICE_WIDTH_INVALID | error |
| 19 | sslice value width != W param | `sslice(src, idx, 4, 8-bit)` | SSLICE_VALUE_WIDTH_MISMATCH | error |
| 20 | oh2b 1-bit source | `oh2b(1-bit)` | OH2B_INPUT_TOO_NARROW | error |
| 21 | b2oh width < 2 | `b2oh(idx, 1)` | B2OH_WIDTH_INVALID | error |
| 22 | prienc 1-bit source | `prienc(1-bit)` | PRIENC_INPUT_TOO_NARROW | error |
| 23 | bswap non-multiple-of-8 | `bswap(7-bit)` | BSWAP_WIDTH_NOT_BYTE_ALIGNED | error |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 5_5_HAPPY_PATH-intrinsic_operators_ok.jz | -- | Valid intrinsic operator usage accepted |
| 5_5_B2OH_WIDTH_INVALID-width_below_two.jz | B2OH_WIDTH_INVALID | b2oh() width is below 2 |
| 5_5_BSWAP_WIDTH_NOT_BYTE_ALIGNED-non_byte_width.jz | BSWAP_WIDTH_NOT_BYTE_ALIGNED | bswap() source width not a multiple of 8 |
| 5_5_CLOG2_INVALID_CONTEXT-runtime_expression.jz | CLOG2_INVALID_CONTEXT | clog2() used with runtime expression |
| 5_5_CLOG2_NONPOSITIVE_ARG-zero_argument.jz | CLOG2_NONPOSITIVE_ARG | clog2() argument is zero |
| 5_5_FUNC_RESULT_TRUNCATED_SILENTLY-intrinsic_truncation.jz | FUNC_RESULT_TRUNCATED_SILENTLY | Intrinsic result assigned to narrower target |
| 5_5_GBIT_INDEX_OUT_OF_RANGE-const_index_exceeds_width.jz | GBIT_INDEX_OUT_OF_RANGE | gbit() index exceeds source width |
| 5_5_GSLICE_INDEX_OUT_OF_RANGE-const_index_exceeds_width.jz | GSLICE_INDEX_OUT_OF_RANGE | gslice() index exceeds source width |
| 5_5_GSLICE_WIDTH_INVALID-zero_width_param.jz | GSLICE_WIDTH_INVALID | gslice() width parameter is zero |
| 5_5_LIT_INVALID_CONTEXT-non_constant_context.jz | LIT_INVALID_CONTEXT | lit() used in non-constant context |
| 5_5_LIT_VALUE_INVALID-negative_value.jz | LIT_VALUE_INVALID | lit() value is negative |
| 5_5_LIT_VALUE_OVERFLOW-value_exceeds_width.jz | LIT_VALUE_OVERFLOW | lit() value exceeds declared width |
| 5_5_LIT_WIDTH_INVALID-non_positive_width.jz | LIT_WIDTH_INVALID | lit() with non-positive width |
| 5_5_OH2B_INPUT_TOO_NARROW-single_bit_source.jz | OH2B_INPUT_TOO_NARROW | oh2b() source is only 1 bit |
| 5_5_PRIENC_INPUT_TOO_NARROW-single_bit_source.jz | PRIENC_INPUT_TOO_NARROW | prienc() source is only 1 bit |
| 5_5_SBIT_INDEX_OUT_OF_RANGE-const_index_exceeds_width.jz | SBIT_INDEX_OUT_OF_RANGE | sbit() index exceeds source width |
| 5_5_SBIT_SET_WIDTH_NOT_1-non_unit_set_value.jz | SBIT_SET_WIDTH_NOT_1 | sbit() third argument not 1 bit |
| 5_5_SSLICE_INDEX_OUT_OF_RANGE-const_index_exceeds_width.jz | SSLICE_INDEX_OUT_OF_RANGE | sslice() index exceeds source width |
| 5_5_SSLICE_VALUE_WIDTH_MISMATCH-value_width_mismatch.jz | SSLICE_VALUE_WIDTH_MISMATCH | sslice() value width does not match width parameter |
| 5_5_SSLICE_WIDTH_INVALID-zero_width_param.jz | SSLICE_WIDTH_INVALID | sslice() width parameter is zero |
| 5_5_WIDTHOF_INVALID_CONTEXT-runtime_expression.jz | WIDTHOF_INVALID_CONTEXT | widthof() used with runtime expression |
| 5_5_WIDTHOF_INVALID_SYNTAX-slice_argument.jz | WIDTHOF_INVALID_SYNTAX | widthof() called with slice/concat argument |
| 5_5_WIDTHOF_INVALID_TARGET-non_signal_target.jz | WIDTHOF_INVALID_TARGET | widthof() called on non-signal target |
| 5_5_WIDTHOF_WIDTH_NOT_RESOLVABLE-unresolvable_width.jz | WIDTHOF_WIDTH_NOT_RESOLVABLE | widthof() target width cannot be resolved |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| B2OH_WIDTH_INVALID | error | S5.5.13 b2oh() width below 2 | 5_5_B2OH_WIDTH_INVALID-width_below_two.jz |
| BSWAP_WIDTH_NOT_BYTE_ALIGNED | error | S5.5.26 bswap() width not multiple of 8 | 5_5_BSWAP_WIDTH_NOT_BYTE_ALIGNED-non_byte_width.jz |
| CLOG2_INVALID_CONTEXT | error | S5.5.5 clog2() in runtime context | 5_5_CLOG2_INVALID_CONTEXT-runtime_expression.jz |
| CLOG2_NONPOSITIVE_ARG | error | S5.5.5 clog2() argument <= 0 | 5_5_CLOG2_NONPOSITIVE_ARG-zero_argument.jz |
| GSLICE_WIDTH_INVALID | error | S5.5.8 gslice() width parameter is zero | 5_5_GSLICE_WIDTH_INVALID-zero_width_param.jz |
| LIT_INVALID_CONTEXT | error | S5.5.11 lit() in compile-time-only context | 5_5_LIT_INVALID_CONTEXT-non_constant_context.jz |
| LIT_VALUE_INVALID | error | S5.5.11 lit() value is negative | 5_5_LIT_VALUE_INVALID-negative_value.jz |
| LIT_VALUE_OVERFLOW | error | S5.5.11 lit() value >= 2^width | 5_5_LIT_VALUE_OVERFLOW-value_exceeds_width.jz |
| LIT_WIDTH_INVALID | error | S5.5.11 lit() width <= 0 | 5_5_LIT_WIDTH_INVALID-non_positive_width.jz |
| OH2B_INPUT_TOO_NARROW | error | S5.5.12 oh2b() source < 2 bits | 5_5_OH2B_INPUT_TOO_NARROW-single_bit_source.jz |
| PRIENC_INPUT_TOO_NARROW | error | S5.5.14 prienc() source < 2 bits | 5_5_PRIENC_INPUT_TOO_NARROW-single_bit_source.jz |
| SBIT_SET_WIDTH_NOT_1 | error | S5.5.7 sbit() set value not 1-bit | 5_5_SBIT_SET_WIDTH_NOT_1-non_unit_set_value.jz |
| SSLICE_VALUE_WIDTH_MISMATCH | error | S5.5.9 sslice() value width != width param | 5_5_SSLICE_VALUE_WIDTH_MISMATCH-value_width_mismatch.jz |
| SSLICE_WIDTH_INVALID | error | S5.5.9 sslice() width parameter is zero | 5_5_SSLICE_WIDTH_INVALID-zero_width_param.jz |
| WIDTHOF_INVALID_CONTEXT | error | S5.5.10 widthof() in runtime context | 5_5_WIDTHOF_INVALID_CONTEXT-runtime_expression.jz |

### 5.2 Rules Not Tested (in this section)

| Rule ID | Severity | Reason |
|---------|----------|--------|
| WIDTHOF_INVALID_TARGET | error | Bug: test exists (`5_5_WIDTHOF_INVALID_TARGET-non_signal_target.jz`) but rule has a known compiler bug |
| WIDTHOF_INVALID_SYNTAX | error | Bug: test exists (`5_5_WIDTHOF_INVALID_SYNTAX-slice_argument.jz`) but rule has a known compiler bug |
| WIDTHOF_WIDTH_NOT_RESOLVABLE | error | Bug: test exists (`5_5_WIDTHOF_WIDTH_NOT_RESOLVABLE-unresolvable_width.jz`) but rule has a known compiler bug |
| FUNC_RESULT_TRUNCATED_SILENTLY | error | Bug: test exists (`5_5_FUNC_RESULT_TRUNCATED_SILENTLY-intrinsic_truncation.jz`) but rule has a known compiler bug |
| GBIT_INDEX_OUT_OF_RANGE | error | Bug: test exists (`5_5_GBIT_INDEX_OUT_OF_RANGE-const_index_exceeds_width.jz`) but rule has a known compiler bug |
| SBIT_INDEX_OUT_OF_RANGE | error | Bug: test exists (`5_5_SBIT_INDEX_OUT_OF_RANGE-const_index_exceeds_width.jz`) but rule has a known compiler bug |
| GSLICE_INDEX_OUT_OF_RANGE | error | Bug: test exists (`5_5_GSLICE_INDEX_OUT_OF_RANGE-const_index_exceeds_width.jz`) but rule has a known compiler bug |
| SSLICE_INDEX_OUT_OF_RANGE | error | Bug: test exists (`5_5_SSLICE_INDEX_OUT_OF_RANGE-const_index_exceeds_width.jz`) but rule has a known compiler bug |
