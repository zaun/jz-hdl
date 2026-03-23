# Test Plan: 3.1 Operator Categories

**Spec Ref:** Section 3.1 of jz-hdl-specification.md

## 1. Objective

Verify that all operator categories are correctly recognized, parsed, and produce the specified result widths: unary arithmetic (input width), binary arithmetic add/sub (input width), multiply/divide/modulus (see S3.2), bitwise (input width), logical (1-bit), comparison (1-bit), shift (LHS width), ternary (operand width), and concatenation (sum of widths).

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected Result Width |
|---|-----------|-------|-----------------------|
| 1 | Unary minus | `(-a)` where a is 8-bit | 8 |
| 2 | Unary plus | `(+a)` where a is 8-bit | 8 |
| 3 | Binary add | `a + b` (both 8-bit) | 8 |
| 4 | Binary sub | `a - b` (both 8-bit) | 8 |
| 5 | Multiply | `a * b` (both 8-bit) | 16 (2N) |
| 6 | Divide | `a / b` (both 8-bit) | 8 |
| 7 | Modulus | `a % b` (both 8-bit) | 8 |
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
| 20 | Ternary | `c ? a : b` (a,b=8-bit) | 8 |
| 21 | Concatenation | `{a, b}` (a=8-bit, b=4-bit) | 12 |

### 2.2 Error Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Logical AND on multi-bit | `8'd1 && 8'd2` -- operands must be 1-bit |
| 2 | Logical NOT on multi-bit | `!8'd1` -- operand must be 1-bit |
| 3 | Binary op width mismatch | `a + b` where a is 8-bit and b is 4-bit |
| 4 | Unknown operator | Verify lexer rejects invalid operators |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | 1-bit all operators | Each operator with 1-bit operands |
| 2 | Max width (256-bit) | Operators on wide signals |
| 3 | Concatenation of many | `{a, b, c, d, e}` -- sum of 5 widths |
| 4 | Nested expressions | `(a + b) & (c + d)` -- verify intermediate widths |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|-----------------|---------|-------|
| 1 | `a + b` (8-bit each) | Result width = 8 | -- | Add/sub: input width |
| 2 | `a * b` (8-bit each) | Result width = 16 | -- | Multiply: 2N |
| 3 | `a == b` | Result width = 1 | -- | Comparison: 1-bit |
| 4 | `{a, b}` (8+4) | Result width = 12 | -- | Concat: sum |
| 5 | `8'd1 && 8'd2` | Error: logical on multi-bit | LOGICAL_WIDTH_NOT_1 | Logical requires 1-bit |
| 6 | `a + b` (mismatched widths) | Error: width mismatch | TYPE_BINOP_WIDTH_MISMATCH | Binary op requires equal widths |

## 4. Existing Validation Tests

| Test File | Rule Tested |
|-----------|-------------|
| `3_1_LOGICAL_WIDTH_NOT_1-logical_ops_multibit.jz` | LOGICAL_WIDTH_NOT_1 |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| LOGICAL_WIDTH_NOT_1 | error | Logical `&&`, `\|\|`, `!` require 1-bit operands | Error 1, 2 |
| TYPE_BINOP_WIDTH_MISMATCH | error | Binary operator requires equal operand widths | Error 3 |

### 5.2 Rules Not Tested

| Rule ID | Severity | Spec Reference | Gap Description |
|---------|----------|----------------|-----------------|
| -- | -- | -- | All relevant rules for S3.1 are covered |
