# Test Plan: 3.1 Operator Categories

**Spec Ref:** Section 3.1 of jz-hdl-specification.md

## 1. Objective

Verify that all operator categories are correctly recognized, parsed, and produce the specified result widths: unary arithmetic (input width, must parenthesize), binary arithmetic add/sub (input width), multiply/divide/modulus (see S3.2), bitwise (input width), logical (1-bit), comparison (1-bit), shift (LHS width), ternary (operand width), and concatenation (sum of widths). Also verify that category-level constraints (operand width requirements, parenthesization) are enforced.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected Result Width |
|---|-----------|-------|-----------------------|
| 1 | Unary minus | `(-a)` where a is 8-bit | 8 |
| 2 | Unary plus | `(+a)` where a is 8-bit | 8 |
| 3 | Binary add | `a + b` (both 8-bit) | 8 |
| 4 | Binary sub | `a - b` (both 8-bit) | 8 |
| 5 | Multiply | `a * b` (both 8-bit) | 16 (2N) |
| 6 | Divide (guarded) | `IF (b != 0) { q <= a / b; }` (both 8-bit) | 8 |
| 7 | Modulus (guarded) | `IF (b != 0) { r <= a % b; }` (both 8-bit) | 8 |
| 8 | Bitwise AND | `a & b` (both 8-bit) | 8 |
| 9 | Bitwise OR | `a \| b` (both 8-bit) | 8 |
| 10 | Bitwise XOR | `a ^ b` (both 8-bit) | 8 |
| 11 | Bitwise NOT | `~a` (8-bit) | 8 |
| 12 | Logical AND | `a && b` (both 1-bit) | 1 |
| 13 | Logical OR | `a \|\| b` (both 1-bit) | 1 |
| 14 | Logical NOT | `!a` (1-bit) | 1 |
| 15 | Comparison == | `a == b` (both 8-bit) | 1 |
| 16 | Comparison < | `a < b` (both 8-bit) | 1 |
| 17 | Left shift | `a << b` (a=8-bit, b=3-bit) | 8 (LHS width) |
| 18 | Right shift | `a >> b` | 8 |
| 19 | Arith right shift | `a >>> b` | 8 |
| 20 | Ternary | `c ? a : b` (c=1-bit, a,b=8-bit) | 8 |
| 21 | Concatenation | `{a, b}` (a=8-bit, b=4-bit) | 12 |

### 2.2 Error Cases

| # | Test Case | Description | Expected Rule ID |
|---|-----------|-------------|------------------|
| 1 | Logical AND on multi-bit | `8'd1 && 8'd2` -- operands must be 1-bit | LOGICAL_WIDTH_NOT_1 |
| 2 | Logical NOT on multi-bit | `!8'd1` -- operand must be 1-bit | LOGICAL_WIDTH_NOT_1 |
| 3 | Logical OR on multi-bit | `8'd1 \|\| 8'd2` -- operands must be 1-bit | LOGICAL_WIDTH_NOT_1 |
| 4 | Binary op width mismatch | `a + b` where a is 8-bit and b is 4-bit | TYPE_BINOP_WIDTH_MISMATCH |
| 5 | Unary minus without parens | `-flag` instead of `(-flag)` | UNARY_ARITH_MISSING_PARENS |
| 6 | Ternary cond multi-bit | `8'd1 ? a : b` -- cond must be 1-bit | TERNARY_COND_WIDTH_NOT_1 |
| 7 | Ternary branch mismatch | `sel ? 8'd1 : 4'd0` -- branches must match | TERNARY_BRANCH_WIDTH_MISMATCH |
| 8 | Empty concatenation | `{}` -- at least one element required | CONCAT_EMPTY |
| 9 | Division by constant zero | `a / 8'd0` | DIV_CONST_ZERO |
| 10 | Unguarded runtime division | `q <= n / d;` without IF guard | DIV_UNGUARDED_RUNTIME_ZERO |
| 11 | GND/VCC in arithmetic expr | `a + GND` -- special drivers not allowed in expressions | SPECIAL_DRIVER_IN_EXPRESSION |
| 12 | GND/VCC in concatenation | `{a, VCC}` -- special drivers not allowed in concat | SPECIAL_DRIVER_IN_CONCAT |
| 13 | GND/VCC sliced | `GND[0]` -- special drivers may not be sliced | SPECIAL_DRIVER_SLICED |
| 14 | GND/VCC as index expr | `a[GND]` -- special drivers not allowed in index | SPECIAL_DRIVER_IN_INDEX |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | 1-bit all operators | Each operator with 1-bit operands |
| 2 | Max width (256-bit) | Operators on wide signals |
| 3 | Concatenation of many | `{a, b, c, d, e}` -- sum of 5 widths |
| 4 | Nested expressions | `(a + b) & (c + d)` -- verify intermediate widths |

## 3. Input/Output Matrix

| # | Scenario | Construct | Expected | Rule ID | Severity |
|---|----------|-----------|----------|---------|----------|
| 1 | Happy: binary add | `a + b` (8-bit each) | Result width = 8 | -- | -- |
| 2 | Happy: multiply | `a * b` (8-bit each) | Result width = 16 | -- | -- |
| 3 | Happy: comparison | `a == b` | Result width = 1 | -- | -- |
| 4 | Happy: concatenation | `{a, b}` (8+4) | Result width = 12 | -- | -- |
| 5 | Happy: all categories | All operator categories used correctly | empty `.out` | -- | -- |
| 6 | Error: logical multi-bit | `8'd1 && 8'd2` | Error | LOGICAL_WIDTH_NOT_1 | error |
| 7 | Error: width mismatch | `a[8] + b[4]` | Error | TYPE_BINOP_WIDTH_MISMATCH | error |
| 8 | Error: unary no parens | `-flag` | Error | UNARY_ARITH_MISSING_PARENS | error |
| 9 | Error: ternary cond wide | `8'd1 ? a : b` | Error | TERNARY_COND_WIDTH_NOT_1 | error |
| 10 | Error: ternary branch mismatch | `sel ? 8'd1 : 4'd0` | Error | TERNARY_BRANCH_WIDTH_MISMATCH | error |
| 11 | Error: empty concat | `{}` | Error | CONCAT_EMPTY | error |
| 12 | Error: div by const 0 | `a / 8'd0` | Error | DIV_CONST_ZERO | error |
| 13 | Error: unguarded div | `q <= n / d` (no guard) | Warning | DIV_UNGUARDED_RUNTIME_ZERO | warning |
| 14 | Error: GND in expression | `a + GND` | Error | SPECIAL_DRIVER_IN_EXPRESSION | error |
| 15 | Error: VCC in concat | `{a, VCC}` | Error | SPECIAL_DRIVER_IN_CONCAT | error |
| 16 | Error: GND sliced | `GND[0]` | Error | SPECIAL_DRIVER_SLICED | error |
| 17 | Error: GND as index | `a[GND]` | Error | SPECIAL_DRIVER_IN_INDEX | error |

## 4. Existing Validation Tests

| Test File | Rule Tested |
|-----------|-------------|
| `3_1_HAPPY_PATH-operator_categories_ok.jz` | -- (happy-path) |
| `3_1_LOGICAL_WIDTH_NOT_1-logical_ops_multibit.jz` | LOGICAL_WIDTH_NOT_1 |
| `3_1_TYPE_BINOP_WIDTH_MISMATCH-width_mismatch.jz` | TYPE_BINOP_WIDTH_MISMATCH |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| LOGICAL_WIDTH_NOT_1 | error | Logical `&&`, `\|\|`, `!` require 1-bit operands | `3_1_LOGICAL_WIDTH_NOT_1-logical_ops_multibit.jz` |
| TYPE_BINOP_WIDTH_MISMATCH | error | Binary operator requires equal operand widths | `3_1_TYPE_BINOP_WIDTH_MISMATCH-width_mismatch.jz` |
| UNARY_ARITH_MISSING_PARENS | error | S3.2/S8.1 Unary arithmetic `-flag`/`+flag` used without required parentheses | 3_2_UNARY_ARITH_MISSING_PARENS-unparenthesized_unary.jz, 3_4_UNARY_ARITH_MISSING_PARENS-negate_without_parens.jz |
| TERNARY_COND_WIDTH_NOT_1 | error | S3.2/S8.1 Ternary `?:` condition must be 1 bit wide; use a comparison or reduction operator | 3_2_TERNARY_COND_WIDTH_NOT_1-multibit_condition.jz |
| TERNARY_BRANCH_WIDTH_MISMATCH | error | S3.2/S8.1 Ternary true/false branches have mismatched widths | 2_3_TERNARY_BRANCH_WIDTH_MISMATCH-ternary_arm_widths.jz, 3_2_TERNARY_BRANCH_WIDTH_MISMATCH-branch_width_mismatch.jz, 3_4_TERNARY_BRANCH_WIDTH_MISMATCH-concat_width_mismatch.jz |
| CONCAT_EMPTY | error | S3.2/S8.1 Empty concatenation `{}` is not allowed | 3_2_CONCAT_EMPTY-empty_concatenation.jz |
| DIV_CONST_ZERO | error | S3.2 Division/modulus by compile-time constant zero divisor | 3_2_DIV_CONST_ZERO-constant_zero_divisor.jz |
| DIV_UNGUARDED_RUNTIME_ZERO | warning | S3.2 Divisor may be zero at runtime; guard with IF (divisor != 0) or use a compile-time constant | 3_2_DIV_UNGUARDED_RUNTIME_ZERO-unguarded_division.jz |
| SPECIAL_DRIVER_IN_EXPRESSION | error | S2.3 GND/VCC may not appear in arithmetic/logical expressions | 2_4_SPECIAL_DRIVER_IN_EXPRESSION-gnd_vcc_in_expr.jz |
| SPECIAL_DRIVER_IN_CONCAT | error | S2.3 GND/VCC may not appear in concatenations | 2_4_SPECIAL_DRIVER_IN_CONCAT-gnd_vcc_in_concat.jz |
| SPECIAL_DRIVER_SLICED | error | S2.3 GND/VCC may not be sliced or indexed | 1_3_SPECIAL_DRIVER_SLICED-vcc_gnd_sliced.jz, 2_4_SPECIAL_DRIVER_SLICED-gnd_vcc_sliced.jz |
### 5.2 Rules Not Tested Here (covered by other Section 3 or Section 2 tests)

| Rule ID | Severity | Reason |
|---------|----------|--------|
| SPECIAL_DRIVER_IN_INDEX | error | Dead code: test exists (`2_4_SPECIAL_DRIVER_IN_INDEX-gnd_vcc_in_index.jz`) but rule is dead code |
