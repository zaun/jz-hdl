# Test Plan: 3.4 Operator Examples

**Spec Ref:** Section 3.4 of jz-hdl-specification.md

## 1. Objective

Verify that the canonical operator examples from the specification compile correctly and produce expected results: unary negation with parentheses, multi-bit negation via subtraction, arithmetic vs logical right shift, overflow prevention via concatenation, ternary with concatenation, and tri-state driver patterns.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Unary negation | `flag = (-flag);` | Two's complement of flag |
| 2 | Multi-bit negate | `negated = 8'h00 - input_val;` | Two's complement via subtraction |
| 3 | Logical right shift | `8'b1000_0001 >> 1'b1` | `8'b0100_0000` (zero-fill) |
| 4 | Arithmetic right shift | `8'b1000_0001 >>> 1'b1` | `8'b1100_0000` (MSB replicated) |
| 5 | Carry capture | `{carry, sum} = {1'b0, a} + {1'b0, b};` | 9-bit result with carry |
| 6 | Ternary + concat | `result = sel ? {a, b} : {c, d};` | Selected concatenation |
| 7 | Tri-state driver | `inout_port = enable ? data_out : 8'bzzzz_zzzz;` | Conditional drive/release |

### 2.2 Error Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Missing parens on negate | `-flag` (no parens) -- Error: UNARY_ARITH_MISSING_PARENS |
| 2 | Ternary width mismatch | `sel ? {a, b} : c` where concat width != c width -- Error: TERNARY_BRANCH_WIDTH_MISMATCH |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Negate zero | `(-8'd0)` = `8'd0` |
| 2 | Negate max | `(-8'd255)` = `8'd1` (two's complement) |
| 3 | Shift by zero | `a >> 3'd0` = `a` |
| 4 | Carry = 0 case | `{carry, sum} = {1'b0, 8'd100} + {1'b0, 8'd50}` -> carry=0 |
| 5 | Carry = 1 case | `{carry, sum} = {1'b0, 8'd200} + {1'b0, 8'd200}` -> carry=1 |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|-----------------|---------|-------|
| 1 | `(-8'd5)` | `8'd251` (two's complement) | -- | Parenthesized, valid |
| 2 | `8'b1000_0001 >> 1'b1` | `8'b0100_0000` | -- | Logical shift: zero-fill |
| 3 | `8'b1000_0001 >>> 1'b1` | `8'b1100_0000` | -- | Arith shift: MSB replicate |
| 4 | `{1'b0, 8'd200} + {1'b0, 8'd200}` | `9'b1_1001_0000` | -- | Carry captured |
| 5 | `-flag` (no parens) | Error | UNARY_ARITH_MISSING_PARENS | Must parenthesize |
| 6 | `sel ? {a,b} : c` (width mismatch) | Error | TERNARY_BRANCH_WIDTH_MISMATCH | Branch widths must match |

## 4. Existing Validation Tests

| Test File | Rule Tested |
|-----------|-------------|
| `3_4_OPERATOR_EXAMPLES-spec_examples_ok.jz` | — (happy-path) |
| `3_4_TERNARY_BRANCH_WIDTH_MISMATCH-concat_width_mismatch.jz` | TERNARY_BRANCH_WIDTH_MISMATCH |
| `3_4_UNARY_ARITH_MISSING_PARENS-negate_without_parens.jz` | UNARY_ARITH_MISSING_PARENS |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| UNARY_ARITH_MISSING_PARENS | error | Unary `-`/`+` without required parentheses | `3_4_UNARY_ARITH_MISSING_PARENS-negate_without_parens.jz` |
| TERNARY_BRANCH_WIDTH_MISMATCH | error | Ternary true/false branches have mismatched widths | `3_4_TERNARY_BRANCH_WIDTH_MISMATCH-concat_width_mismatch.jz` |
| LOGICAL_WIDTH_NOT_1 | error | S3.2/S8.1 Logical `&&`, `||`, `!` require 1-bit operands; did you mean bitwise `&`, `|`, `~`? | 3_1_LOGICAL_WIDTH_NOT_1-logical_ops_multibit.jz, 3_2_LOGICAL_WIDTH_NOT_1-multibit_logical_operands.jz |
| CONCAT_EMPTY | error | S3.2/S8.1 Empty concatenation `{}` is not allowed | 3_2_CONCAT_EMPTY-empty_concatenation.jz |
| DIV_CONST_ZERO | error | S3.2 Division/modulus by compile-time constant zero divisor | 3_2_DIV_CONST_ZERO-constant_zero_divisor.jz |
| DIV_UNGUARDED_RUNTIME_ZERO | warning | S3.2 Divisor may be zero at runtime; guard with IF (divisor != 0) or use a compile-time constant | 3_2_DIV_UNGUARDED_RUNTIME_ZERO-unguarded_division.jz |
| TERNARY_COND_WIDTH_NOT_1 | error | S3.2/S8.1 Ternary `?:` condition must be 1 bit wide; use a comparison or reduction operator | 3_2_TERNARY_COND_WIDTH_NOT_1-multibit_condition.jz |
| TYPE_BINOP_WIDTH_MISMATCH | error | S2.2/3.2/8.1 Binary operator requires equal operand widths but receives mismatched widths | 2_2_TYPE_BINOP_WIDTH_MISMATCH-width_mismatch.jz, 2_3_TYPE_BINOP_WIDTH_MISMATCH-mismatched_operand_widths.jz, 3_1_TYPE_BINOP_WIDTH_MISMATCH-width_mismatch.jz |
| SPECIAL_DRIVER_IN_EXPRESSION | error | S2.3 GND/VCC may not appear in arithmetic/logical expressions | 2_4_SPECIAL_DRIVER_IN_EXPRESSION-gnd_vcc_in_expr.jz |
| SPECIAL_DRIVER_IN_CONCAT | error | S2.3 GND/VCC may not appear in concatenations | 2_4_SPECIAL_DRIVER_IN_CONCAT-gnd_vcc_in_concat.jz |
| SPECIAL_DRIVER_SLICED | error | S2.3 GND/VCC may not be sliced or indexed | 1_3_SPECIAL_DRIVER_SLICED-vcc_gnd_sliced.jz, 2_4_SPECIAL_DRIVER_SLICED-gnd_vcc_sliced.jz |
### 5.2 Rules Not Tested Here (covered by S3.1/S3.2 tests)

| Rule ID | Severity | Reason |
|---------|----------|--------|
| SPECIAL_DRIVER_IN_INDEX | error | Dead code: test exists (`2_4_SPECIAL_DRIVER_IN_INDEX-gnd_vcc_in_index.jz`) but rule is dead code |
