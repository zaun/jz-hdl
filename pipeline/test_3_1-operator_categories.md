# Test Plan: 3.1 Operator Categories

**Specification Reference:** Section 3.1 of jz-hdl-specification.md

## 1. Objective

Verify that all operator categories are correctly recognized, parsed, and produce the specified result widths: unary arithmetic (input width), binary arithmetic add/sub (input width), multiply/divide/modulus (see S3.2), bitwise (input width), logical (1-bit), comparison (1-bit), shift (LHS width), ternary (operand width), and concatenation (sum of widths).

## 2. Instrumentation Strategy

- **Span: `parser.expression`** — Trace expression parsing; attributes: `operator`, `category`, `operand_count`.
- **Span: `sem.result_width`** — Width computation per operator; attributes: `operator`, `lhs_width`, `rhs_width`, `result_width`.
- **Coverage Hook:** Ensure every operator from every category is exercised at least once.

## 3. Test Scenarios

### 3.1 Happy Path

| # | Test Case | Input | Expected Result Width |
|---|-----------|-------|--------------------|
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

### 3.2 Boundary/Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | 1-bit all operators | Each operator with 1-bit operands |
| 2 | Max width (256-bit) | Operators on wide signals |
| 3 | Concatenation of many | `{a, b, c, d, e}` — sum of 5 widths |
| 4 | Nested expressions | `(a + b) & (c + d)` — verify intermediate widths |

### 3.3 Negative Testing

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Logical AND on multi-bit | `8'd1 && 8'd2` — operands must be 1-bit |
| 2 | Logical NOT on multi-bit | `!8'd1` — operand must be 1-bit |
| 3 | Unknown operator | Verify lexer rejects invalid operators |

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | `a + b` (8-bit each) | Result width = 8 | — | Add/sub: input width |
| 2 | `a * b` (8-bit each) | Result width = 16 | — | Multiply: 2N |
| 3 | `a == b` | Result width = 1 | — | Comparison: 1-bit |
| 4 | `{a, b}` (8+4) | Result width = 12 | — | Concat: sum |
| 5 | `8'd1 && 8'd2` | Error: logical on multi-bit | WIDTH_BINOP_MISMATCH | Logical requires 1-bit |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `parser_expressions.c` | Parses operator expressions into AST | Feed token streams |
| `driver_operators.c` | Validates operator semantics and widths | Unit test with expression AST |
| `driver_width.c` | Width computation | Verify computed widths |
| `driver_expr.c` | Expression tree evaluation | Verify result widths |

## 6. Rules Matrix

### 6.1 Rules Tested

| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| WIDTH_BINOP_MISMATCH | Operand width mismatch | Neg 1, 2 |

### 6.2 Rules Missing

| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| LOGICAL_OP_MULTI_BIT | S3.1 "Operands must be width-1" | Logical ops require 1-bit input; may need explicit rule vs generic width mismatch |
