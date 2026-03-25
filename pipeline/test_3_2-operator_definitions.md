# Test Plan: 3.2 Operator Definitions

**Spec Ref:** Section 3.2 of jz-hdl-specification.md

## 1. Objective

Verify detailed semantics of each operator: unary arithmetic parenthesization requirement, binary arithmetic carry behavior, multiplication 2N result width, division/modulus unsigned semantics and division-by-zero handling (constant and runtime), bitwise operations, logical 1-bit requirement, comparison result, shift fill behavior (logical vs arithmetic), ternary condition width and branch width matching, concatenation MSB/LSB ordering, and x-dependency/observability rules. This section is the primary home for all S3 diagnostic rules.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Parenthesized unary neg | `(-a)` -- valid |
| 2 | Parenthesized unary plus | `(+a)` -- valid |
| 3 | Multiply full product | `a * b` (8-bit) -> 16-bit result |
| 4 | Division guarded by `!= 0` | `IF (d != 0) { q <= n / d; }` -- valid |
| 5 | Modulus guarded by `!= 0` | `IF (d != 0) { r <= n % d; }` -- valid |
| 6 | Bitwise NOT | `~8'hFF` = `8'h00` |
| 7 | Logical shift left | `8'b0000_0001 << 3'd4` = `8'b0001_0000` |
| 8 | Logical shift right | `8'b1000_0000 >> 3'd1` = `8'b0100_0000` (zero-fill) |
| 9 | Arithmetic shift right | `8'b1000_0001 >>> 3'd1` = `8'b1100_0000` (MSB replicated) |
| 10 | Ternary with 1-bit cond | `en ? a : b` -- valid |
| 11 | Concatenation MSB ordering | `{8'hAA, 8'hBB}` = `16'hAABB` |
| 12 | Guard via `>` condition | `IF (d > 8'd0) { q <= n / d; }` -- valid guard |
| 13 | Guard via `>= N` (N>=1) | `IF (d >= 8'd1) { q <= n / d; }` -- valid guard |
| 14 | Guard via `== N` (N!=0) | `IF (d == 8'd5) { q <= n / d; }` -- valid guard |
| 15 | Guard via `!= N` ELSE (N!=0) | `IF (d != 8'd5) { ... } ELSE { q <= n / d; }` -- ELSE proves d==5 |
| 16 | Guard via `< N` ELSE (N>=1) | `IF (d < 8'd1) { ... } ELSE { q <= n / d; }` -- ELSE proves d>=1 |
| 17 | Nested guard | Outer IF proves nonzero, inner IF uses division -- valid |
| 18 | Logical ops on 1-bit | `a && b`, `a \|\| b`, `!a` where a,b are 1-bit -- valid |
| 19 | Single-element concat | `{a}` -- valid, width = a's width |
| 20 | Comparison all operators | `==`, `!=`, `<`, `>`, `<=`, `>=` with equal-width operands |

### 2.2 Error Cases

| # | Test Case | Description | Expected Rule ID | Severity |
|---|-----------|-------------|------------------|----------|
| 1 | Unary not parenthesized | `-flag` without parens | UNARY_ARITH_MISSING_PARENS | error |
| 2 | Unary plus not parenthesized | `+flag` without parens | UNARY_ARITH_MISSING_PARENS | error |
| 3 | Division by constant zero | `a / 8'd0` | DIV_CONST_ZERO | error |
| 4 | Modulus by constant zero | `a % 8'd0` | DIV_CONST_ZERO | error |
| 5 | Unguarded runtime division | `q <= n / d;` without IF guard | DIV_UNGUARDED_RUNTIME_ZERO | warning |
| 6 | Unguarded runtime modulus | `r <= n % d;` without IF guard | DIV_UNGUARDED_RUNTIME_ZERO | warning |
| 7 | Ternary cond multi-bit | `8'd1 ? a : b` | TERNARY_COND_WIDTH_NOT_1 | error |
| 8 | Ternary branch width mismatch | `sel ? 8'd1 : 4'd0` | TERNARY_BRANCH_WIDTH_MISMATCH | error |
| 9 | Empty concatenation | `{}` | CONCAT_EMPTY | error |
| 10 | Logical AND multi-bit | `8'd1 && 8'd2` | LOGICAL_WIDTH_NOT_1 | error |
| 11 | Logical OR multi-bit | `8'd1 \|\| 8'd2` | LOGICAL_WIDTH_NOT_1 | error |
| 12 | Logical NOT multi-bit | `!8'd1` | LOGICAL_WIDTH_NOT_1 | error |
| 13 | x-bits to observable sink | `reg <= 8'bxxxx_0000;` | OBS_X_TO_OBSERVABLE_SINK | error |
| 14 | x in non-CASE context | Binary literal with x used in arithmetic | OBS_X_TO_OBSERVABLE_SINK | error |
| 15 | GND/VCC in arithmetic expr | `a + GND` -- special drivers forbidden in expressions | SPECIAL_DRIVER_IN_EXPRESSION | error |
| 16 | GND/VCC in concatenation | `{a, VCC}` -- special drivers forbidden in concat | SPECIAL_DRIVER_IN_CONCAT | error |
| 17 | GND/VCC sliced | `GND[0]` -- special drivers may not be sliced/indexed | SPECIAL_DRIVER_SLICED | error |
| 18 | GND/VCC as index expression | `a[GND]` -- special drivers forbidden as index | SPECIAL_DRIVER_IN_INDEX | error |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Multiply 1-bit | `1'b1 * 1'b1` = 2-bit result |
| 2 | Shift by 0 | `a << 3'd0` = `a` (no shift) |
| 3 | Shift by max | `8'd1 << 4'd8` -- shift equals width, result = 0 |
| 4 | Division max/1 | `8'd255 / 8'd1` = `8'd255` |
| 5 | Modulus N % N | `8'd10 % 8'd10` = `8'd0` |
| 6 | Guard on ELSE branch | `IF (d == 8'd0) { ... } ELSE { q <= n / d; }` -- ELSE proves nonzero |
| 7 | Guard composition through nesting | Outer guard active inside inner IF/ELSE/SELECT |
| 8 | Literal on either side of guard | `IF (8'd0 != d)` -- literal may appear on either side |

## 3. Input/Output Matrix

| # | Scenario | Construct | Expected | Rule ID | Severity |
|---|----------|-----------|----------|---------|----------|
| 1 | Happy: all operators | Valid operator usage | empty `.out` | -- | -- |
| 2 | Error: unary no parens | `-flag` | Error | UNARY_ARITH_MISSING_PARENS | error |
| 3 | Error: div const zero | `a / 8'd0` | Error | DIV_CONST_ZERO | error |
| 4 | Error: mod const zero | `a % 8'd0` | Error | DIV_CONST_ZERO | error |
| 5 | Error: unguarded div | `q <= n / d` (no guard) | Warning | DIV_UNGUARDED_RUNTIME_ZERO | warning |
| 6 | Error: ternary cond wide | `8'd1 ? a : b` | Error | TERNARY_COND_WIDTH_NOT_1 | error |
| 7 | Error: ternary branch mismatch | `sel ? 8'd1 : 4'd0` | Error | TERNARY_BRANCH_WIDTH_MISMATCH | error |
| 8 | Error: empty concat | `{}` | Error | CONCAT_EMPTY | error |
| 9 | Error: logical multi-bit | `8'd1 && 8'd2` | Error | LOGICAL_WIDTH_NOT_1 | error |
| 10 | Error: x to sink | `reg <= 8'bxxxx_0000` | Error | OBS_X_TO_OBSERVABLE_SINK | error |
| 11 | Happy: guarded div | `IF (d != 0) { q <= n / d; }` | No diagnostic | -- | -- |
| 12 | Happy: multiply 2N | `a * b` (8-bit) | 16-bit result | -- | -- |
| 13 | Happy: concat ordering | `{8'hAA, 8'hBB}` | `16'hAABB` | -- | -- |
| 14 | Error: GND in expression | `a + GND` | Error | SPECIAL_DRIVER_IN_EXPRESSION | error |
| 15 | Error: VCC in concat | `{a, VCC}` | Error | SPECIAL_DRIVER_IN_CONCAT | error |
| 16 | Error: GND sliced | `GND[0]` | Error | SPECIAL_DRIVER_SLICED | error |
| 17 | Error: GND as index | `a[GND]` | Error | SPECIAL_DRIVER_IN_INDEX | error |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| `3_2_OPERATOR_SEMANTICS-operator_semantics_ok.jz` | -- | Happy path: valid operator usage accepted |
| `3_2_UNARY_ARITH_MISSING_PARENS-unparenthesized_unary.jz` | UNARY_ARITH_MISSING_PARENS | Unary arithmetic operator used without required parentheses |
| `3_2_LOGICAL_WIDTH_NOT_1-multibit_logical_operands.jz` | LOGICAL_WIDTH_NOT_1 | Logical operator used with operands wider than 1 bit |
| `3_2_TERNARY_COND_WIDTH_NOT_1-multibit_condition.jz` | TERNARY_COND_WIDTH_NOT_1 | Ternary condition expression is wider than 1 bit |
| `3_2_TERNARY_BRANCH_WIDTH_MISMATCH-branch_width_mismatch.jz` | TERNARY_BRANCH_WIDTH_MISMATCH | Ternary true/false branches have mismatched widths |
| `3_2_CONCAT_EMPTY-empty_concatenation.jz` | CONCAT_EMPTY | Empty concatenation `{}` is not allowed |
| `3_2_DIV_CONST_ZERO-constant_zero_divisor.jz` | DIV_CONST_ZERO | Division or modulus by compile-time constant zero |
| `3_2_DIV_UNGUARDED_RUNTIME_ZERO-unguarded_division.jz` | DIV_UNGUARDED_RUNTIME_ZERO | Divisor may be zero at runtime without a nonzero guard |
| `3_2_OBS_X_TO_OBSERVABLE_SINK-x_bits_in_expressions.jz` | OBS_X_TO_OBSERVABLE_SINK | x-bits reaching observable sink outside CASE pattern |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| UNARY_ARITH_MISSING_PARENS | error | Unary `-`/`+` without required parentheses | `3_2_UNARY_ARITH_MISSING_PARENS-unparenthesized_unary.jz` |
| LOGICAL_WIDTH_NOT_1 | error | Logical `&&`, `\|\|`, `!` require 1-bit operands | `3_2_LOGICAL_WIDTH_NOT_1-multibit_logical_operands.jz` |
| TERNARY_COND_WIDTH_NOT_1 | error | Ternary `?:` condition must be 1 bit wide | `3_2_TERNARY_COND_WIDTH_NOT_1-multibit_condition.jz` |
| TERNARY_BRANCH_WIDTH_MISMATCH | error | Ternary true/false branches have mismatched widths | `3_2_TERNARY_BRANCH_WIDTH_MISMATCH-branch_width_mismatch.jz` |
| CONCAT_EMPTY | error | Empty concatenation `{}` is not allowed | `3_2_CONCAT_EMPTY-empty_concatenation.jz` |
| DIV_CONST_ZERO | error | Division/modulus by compile-time constant zero | `3_2_DIV_CONST_ZERO-constant_zero_divisor.jz` |
| DIV_UNGUARDED_RUNTIME_ZERO | warning | Divisor may be zero at runtime without a nonzero guard | `3_2_DIV_UNGUARDED_RUNTIME_ZERO-unguarded_division.jz` |
| OBS_X_TO_OBSERVABLE_SINK | error | x-bits reaching observable sink outside CASE pattern | `3_2_OBS_X_TO_OBSERVABLE_SINK-x_bits_in_expressions.jz` |
| SPECIAL_DRIVER_IN_EXPRESSION | error | S2.3 GND/VCC may not appear in arithmetic/logical expressions | 2_4_SPECIAL_DRIVER_IN_EXPRESSION-gnd_vcc_in_expr.jz |
| SPECIAL_DRIVER_IN_CONCAT | error | S2.3 GND/VCC may not appear in concatenations | 2_4_SPECIAL_DRIVER_IN_CONCAT-gnd_vcc_in_concat.jz |
| SPECIAL_DRIVER_SLICED | error | S2.3 GND/VCC may not be sliced or indexed | 1_3_SPECIAL_DRIVER_SLICED-vcc_gnd_sliced.jz, 2_4_SPECIAL_DRIVER_SLICED-gnd_vcc_sliced.jz |
| SPECIAL_DRIVER_IN_INDEX | error | S2.3 GND/VCC may not appear in slice/index expressions | 2_4_SPECIAL_DRIVER_IN_INDEX-gnd_vcc_in_index.jz |
| TYPE_BINOP_WIDTH_MISMATCH | error | S2.2/3.2/8.1 Binary operator requires equal operand widths but receives mismatched widths | 2_2_TYPE_BINOP_WIDTH_MISMATCH-width_mismatch.jz, 2_3_TYPE_BINOP_WIDTH_MISMATCH-mismatched_operand_widths.jz, 3_1_TYPE_BINOP_WIDTH_MISMATCH-width_mismatch.jz |

### 5.2 Rules Not Tested Here (covered by Section 2.4 tests)

All rules for this section are tested.
