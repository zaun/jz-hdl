# Test Plan: 3.2 Operator Definitions

**Spec Ref:** Section 3.2 of jz-hdl-specification.md

## 1. Objective

Verify detailed semantics of each operator: unary arithmetic parenthesization requirement, binary arithmetic carry behavior, multiplication 2N result width, division/modulus unsigned semantics and division-by-zero handling (constant and runtime), bitwise operations, logical 1-bit requirement, comparison result, shift fill behavior (logical vs arithmetic), ternary condition width and branch width matching, concatenation MSB/LSB ordering, and x-dependency/observability rules.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Parenthesized unary neg | `(-a)` -- valid |
| 2 | Multiply full product | `a * b` (8-bit) -> 16-bit result |
| 3 | Division guarded | `IF (d != 0) { q <= n / d; }` -- valid |
| 4 | Modulus guarded | `IF (d != 0) { r <= n % d; }` -- valid |
| 5 | Bitwise NOT | `~8'hFF` = `8'h00` |
| 6 | Logical shift left | `8'b0000_0001 << 3'd4` = `8'b0001_0000` |
| 7 | Logical shift right | `8'b1000_0000 >> 3'd1` = `8'b0100_0000` (zero-fill) |
| 8 | Arithmetic shift right | `8'b1000_0001 >>> 3'd1` = `8'b1100_0000` (MSB replicated) |
| 9 | Ternary with 1-bit cond | `en ? a : b` -- valid |
| 10 | Concatenation MSB ordering | `{8'hAA, 8'hBB}` = `16'hAABB` |
| 11 | Guard via `>` condition | `IF (d > 8'd0) { q <= n / d; }` -- valid guard |
| 12 | Guard via `== N` (N!=0) | `IF (d == 8'd5) { q <= n / d; }` -- valid guard |
| 13 | Nested guard | Outer IF proves nonzero, inner IF uses division -- valid |

### 2.2 Error Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Unary not parenthesized | `-flag` without parens -- Error: UNARY_ARITH_MISSING_PARENS |
| 2 | Division by constant zero | `a / 8'd0` -- Error: DIV_CONST_ZERO |
| 3 | Ternary cond multi-bit | `8'd1 ? a : b` -- Error: TERNARY_COND_WIDTH_NOT_1 |
| 4 | Ternary branch width mismatch | `sel ? 8'd1 : 4'd0` -- Error: TERNARY_BRANCH_WIDTH_MISMATCH |
| 5 | Empty concatenation | `{}` -- Error: CONCAT_EMPTY |
| 6 | Logical op multi-bit | `8'd1 && 8'd2` -- Error: LOGICAL_WIDTH_NOT_1 |
| 7 | x-bits to observable sink | `reg <= 8'bxxxx_0000;` -- Error: OBS_X_TO_OBSERVABLE_SINK |
| 8 | x in non-CASE context | Binary literal with x used in arithmetic -- Error: OBS_X_TO_OBSERVABLE_SINK |
| 9 | Unguarded runtime division | `q <= n / d;` without IF guard -- Warning: DIV_UNGUARDED_RUNTIME_ZERO |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Multiply 1-bit | `1'b1 * 1'b1` = 2-bit result |
| 2 | Shift by 0 | `a << 3'd0` = `a` (no shift) |
| 3 | Shift by max | `8'd1 << 4'd8` -- shift equals width, result = 0 |
| 4 | Division max/1 | `8'd255 / 8'd1` = `8'd255` |
| 5 | Modulus N % N | `8'd10 % 8'd10` = `8'd0` |
| 6 | Single-element concat | `{a}` -- valid, width = a's width |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|-----------------|---------|-------|
| 1 | `-flag` (no parens) | Error | UNARY_ARITH_MISSING_PARENS | Must parenthesize |
| 2 | `a / 8'd0` | Error | DIV_CONST_ZERO | Constant zero divisor |
| 3 | `q <= n / d` (unguarded) | Warning | DIV_UNGUARDED_RUNTIME_ZERO | No nonzero proof |
| 4 | `8'd1 ? a : b` | Error | TERNARY_COND_WIDTH_NOT_1 | Cond must be 1-bit |
| 5 | `sel ? 8'd1 : 4'd0` | Error | TERNARY_BRANCH_WIDTH_MISMATCH | Branch widths must match |
| 6 | `{}` | Error | CONCAT_EMPTY | Empty concatenation not allowed |
| 7 | `8'd1 && 8'd2` | Error | LOGICAL_WIDTH_NOT_1 | Logical requires 1-bit |
| 8 | `reg <= 8'bxxxx_0000` | Error | OBS_X_TO_OBSERVABLE_SINK | x-bits to observable sink |
| 9 | `8'b1 * 8'b1` | 16-bit result | -- | 2N width |
| 10 | `{8'hAA, 8'hBB}` | `16'hAABB` | -- | MSB-first ordering |

## 4. Existing Validation Tests

| Test File | Rule Tested |
|-----------|-------------|
| `3_2_UNARY_ARITH_MISSING_PARENS-unparenthesized_unary.jz` | UNARY_ARITH_MISSING_PARENS |
| `3_2_CONCAT_EMPTY-empty_concatenation.jz` | CONCAT_EMPTY |
| `3_2_DIV_CONST_ZERO-constant_zero_divisor.jz` | DIV_CONST_ZERO |
| `3_2_DIV_UNGUARDED_RUNTIME_ZERO-unguarded_division.jz` | DIV_UNGUARDED_RUNTIME_ZERO |
| `3_2_LOGICAL_WIDTH_NOT_1-multibit_logical_operands.jz` | LOGICAL_WIDTH_NOT_1 |
| `3_2_TERNARY_BRANCH_WIDTH_MISMATCH-branch_width_mismatch.jz` | TERNARY_BRANCH_WIDTH_MISMATCH |
| `3_2_TERNARY_COND_WIDTH_NOT_1-multibit_condition.jz` | TERNARY_COND_WIDTH_NOT_1 |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| UNARY_ARITH_MISSING_PARENS | error | Unary `-`/`+` without required parentheses | Error 1 |
| LOGICAL_WIDTH_NOT_1 | error | Logical `&&`, `\|\|`, `!` require 1-bit operands | Error 6 |
| TERNARY_COND_WIDTH_NOT_1 | error | Ternary `?:` condition must be 1 bit wide | Error 3 |
| TERNARY_BRANCH_WIDTH_MISMATCH | error | Ternary true/false branches have mismatched widths | Error 4 |
| CONCAT_EMPTY | error | Empty concatenation `{}` is not allowed | Error 5 |
| DIV_CONST_ZERO | error | Division/modulus by compile-time constant zero | Error 2 |
| DIV_UNGUARDED_RUNTIME_ZERO | warning | Divisor may be zero at runtime without guard | Error 9 |
| OBS_X_TO_OBSERVABLE_SINK | error | x-bits drive REGISTER, MEM, or output | Error 7, 8 |

### 5.2 Rules Not Tested

| Rule ID | Severity | Spec Reference | Gap Description |
|---------|----------|----------------|-----------------|
| -- | -- | -- | All relevant rules for S3.2 are covered |
