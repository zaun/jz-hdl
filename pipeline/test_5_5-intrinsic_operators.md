# Test Plan: 5.5 Intrinsic Operators

**Specification Reference:** Section 5.5 (5.5.1–5.5.28) of jz-hdl-specification.md

## 1. Objective

Verify all 28 intrinsic operators: their result widths, operand constraints, compile-time vs hardware-elaborating behavior, and correct mathematical results.

## 2. Instrumentation Strategy

- **Span: `sem.intrinsic`** — Trace intrinsic validation; attributes: `name`, `operand_widths`, `result_width`, `is_compile_time`.
- **Coverage Hook:** Ensure every intrinsic is called with valid and invalid inputs.

## 3. Test Scenarios

### 3.1 Happy Path — All 28 Intrinsics

| # | Intrinsic | Input | Expected Result Width | Notes |
|---|-----------|-------|--------------------|-------|
| 1 | `uadd(a,b)` | 8-bit + 8-bit | 9 (MAX+1) | Unsigned add with carry |
| 2 | `sadd(a,b)` | 8-bit + 8-bit | 9 | Signed add with carry |
| 3 | `umul(a,b)` | 8-bit × 8-bit | 16 (2×MAX) | Unsigned full product |
| 4 | `smul(a,b)` | 8-bit × 8-bit | 16 | Signed full product |
| 5 | `clog2(256)` | Compile-time | 8 | Ceiling log2 |
| 6 | `gbit(src,idx)` | 8-bit, 3-bit idx | 1 | Get single bit |
| 7 | `sbit(src,idx,val)` | 8-bit, 3-bit, 1-bit | 8 | Set/clear one bit |
| 8 | `gslice(src,idx,W)` | 16-bit, 4-bit, W=4 | 4 | Dynamic multi-bit extract |
| 9 | `sslice(src,idx,W,val)` | 16-bit, 4-bit, W=4, 4-bit | 16 | Dynamic multi-bit overwrite |
| 10 | `widthof(sig)` | Any signal | Compile-time integer | Width query |
| 11 | `lit(W,V)` | Width, value | W bits | Compile-time literal |
| 12 | `oh2b(src)` | 8-bit one-hot | 3 (clog2(8)) | One-hot to binary |
| 13 | `b2oh(idx,W)` | 3-bit, W=8 | 8 | Binary to one-hot |
| 14 | `prienc(src)` | 8-bit | 3 (clog2(8)) | Priority encoder |
| 15 | `lzc(src)` | 8-bit | 4 (clog2(8)+1) | Leading zero count |
| 16 | `usub(a,b)` | 8-bit - 8-bit | 9 | Unsigned widening subtract |
| 17 | `ssub(a,b)` | 8-bit - 8-bit | 9 | Signed widening subtract |
| 18 | `abs(a)` | 8-bit signed | 8 | Absolute value |
| 19 | `umin(a,b)` | 8-bit, 8-bit | 8 | Unsigned minimum |
| 20 | `umax(a,b)` | 8-bit, 8-bit | 8 | Unsigned maximum |
| 21 | `smin(a,b)` | 8-bit, 8-bit | 8 | Signed minimum |
| 22 | `smax(a,b)` | 8-bit, 8-bit | 8 | Signed maximum |
| 23 | `popcount(src)` | 8-bit | 4 (clog2(8)+1) | Population count |
| 24 | `reverse(src)` | 8-bit | 8 | Bit reversal |
| 25 | `bswap(src)` | 16-bit | 16 | Byte swap |
| 26 | `reduce_and(src)` | 8-bit | 1 | AND reduction |
| 27 | `reduce_or(src)` | 8-bit | 1 | OR reduction |
| 28 | `reduce_xor(src)` | 8-bit | 1 | XOR reduction |

### 3.2 Boundary/Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | clog2(1) | Result = 0 |
| 2 | clog2(0) | Error or special case |
| 3 | gbit out of range | Index >= source width → result = 0 |
| 4 | popcount all-ones | `popcount(8'hFF)` = 8 |
| 5 | popcount all-zeros | `popcount(8'h00)` = 0 |
| 6 | reverse 1-bit | `reverse(1'b1)` = `1'b1` |
| 7 | bswap odd bytes | `bswap(24-bit)` — 3 bytes |
| 8 | uadd max values | `uadd(8'hFF, 8'hFF)` = 9'd510 |
| 9 | Different operand widths | `uadd(4'd5, 8'd10)` — MAX(4,8)+1 = 9 |

### 3.3 Negative Testing

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Wrong argument count | `uadd(a)` — missing operand |
| 2 | clog2 on runtime signal | `clog2(wire_val)` — Error: compile-time only |
| 3 | widthof on literal | `widthof(8'd5)` — Error: must be signal |
| 4 | bswap non-byte-multiple | `bswap(7-bit)` — Error |
| 5 | lit overflow | `lit(4, 20)` — value doesn't fit in 4 bits |
| 6 | sbit val not 1-bit | `sbit(src, idx, 8'd5)` — Error |

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | `uadd(8'd200, 8'd100)` | 9'd300 | — | Unsigned widening add |
| 2 | `clog2(wire)` | Error | INTRINSIC_COMPILE_TIME_ONLY | Not compile-time |
| 3 | `bswap(7-bit)` | Error | INTRINSIC_BSWAP_NOT_BYTE | Must be byte-multiple |
| 4 | `popcount(8'b1010_1010)` | 4'd4 | — | Count set bits |
| 5 | `reduce_and(8'hFF)` | 1'b1 | — | All bits set |
| 6 | `reduce_and(8'hFE)` | 1'b0 | — | Not all bits set |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `parser_expressions.c` | Parses intrinsic calls | Token stream |
| `driver_operators.c` | Intrinsic semantic validation | Unit test per intrinsic |
| `const_eval.c` | Compile-time intrinsic evaluation | Unit test for clog2, widthof, lit |
| `ir_build_expr.c` | Hardware elaboration of intrinsics | IR verification |
| `sim_eval.c` | Runtime evaluation | Simulation comparison |

## 6. Rules Matrix

### 6.1 Rules Tested

| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| INTRINSIC_ARG_COUNT | Wrong argument count | Neg 1 |
| INTRINSIC_COMPILE_TIME_ONLY | Runtime signal in compile-time intrinsic | Neg 2 |
| INTRINSIC_OPERAND_TYPE | Invalid operand type | Neg 3, 6 |
| INTRINSIC_BSWAP_NOT_BYTE | bswap on non-byte-multiple | Neg 4 |

### 6.2 Rules Missing

| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| INTRINSIC_UNDEFINED | S5.5 | Unknown intrinsic name |
| INTRINSIC_LIT_OVERFLOW | S5.5.11 | lit() value doesn't fit in width |
| INTRINSIC_CLOG2_ZERO | S5.5.5 | clog2(0) behavior |
| INTRINSIC_WIDTH_MISMATCH | S5.5 various | Operand widths don't match for binary intrinsics |
