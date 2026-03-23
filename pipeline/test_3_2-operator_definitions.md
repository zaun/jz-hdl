# Test Plan: 3.2 Operator Definitions

**Specification Reference:** Section 3.2 of jz-hdl-specification.md

## 1. Objective

Verify detailed semantics of each operator: unary arithmetic parenthesization requirement, binary arithmetic carry behavior, multiplication 2N result width, division/modulus unsigned semantics and division-by-zero handling (constant and runtime), bitwise operations, logical 1-bit requirement, comparison result, shift fill behavior (logical vs arithmetic), ternary condition width, concatenation MSB/LSB ordering, and x-dependency/observability rules.

## 2. Instrumentation Strategy

- **Span: `sem.operator_validate`** — Per-operator validation; attributes: `operator`, `operand_widths`, `result_width`, `has_x_dep`.
- **Span: `sem.div_guard_check`** — Division guard proof; attributes: `divisor_is_const`, `guard_condition`, `proven_nonzero`.
- **Event: `div.const_zero`** — Constant zero divisor detected.
- **Event: `div.unguarded_runtime`** — Runtime divisor without nonzero guard.
- **Event: `unary.not_parenthesized`** — Unary arithmetic without parentheses.

## 3. Test Scenarios

### 3.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Parenthesized unary neg | `(-a)` — valid |
| 2 | Multiply full product | `a * b` (8-bit) → 16-bit result |
| 3 | Division guarded | `IF (d != 0) { q <= n / d; }` — valid |
| 4 | Modulus guarded | `IF (d != 0) { r <= n % d; }` — valid |
| 5 | Bitwise NOT | `~8'hFF` = `8'h00` |
| 6 | Logical shift left | `8'b0000_0001 << 3'd4` = `8'b0001_0000` |
| 7 | Logical shift right | `8'b1000_0000 >> 3'd1` = `8'b0100_0000` (zero-fill) |
| 8 | Arithmetic shift right | `8'b1000_0001 >>> 3'd1` = `8'b1100_0000` (MSB replicated) |
| 9 | Ternary with 1-bit cond | `en ? a : b` — valid |
| 10 | Concatenation MSB ordering | `{8'hAA, 8'hBB}` = `16'hAABB` |
| 11 | Guard via `>` condition | `IF (d > 8'd0) { q <= n / d; }` — valid guard |
| 12 | Guard via `== N` (N≠0) | `IF (d == 8'd5) { q <= n / d; }` — valid guard |
| 13 | Nested guard | Outer IF proves nonzero, inner IF uses division — valid |

### 3.2 Boundary/Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Multiply 1-bit | `1'b1 * 1'b1` = 2-bit result |
| 2 | Shift by 0 | `a << 3'd0` = `a` (no shift) |
| 3 | Shift by max | `8'd1 << 4'd8` — shift equals width, result = 0 |
| 4 | Division max/1 | `8'd255 / 8'd1` = `8'd255` |
| 5 | Modulus N % N | `8'd10 % 8'd10` = `8'd0` |
| 6 | Empty concatenation | Should not be valid |
| 7 | Single-element concat | `{a}` — valid, width = a's width |

### 3.3 Negative Testing

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Unary not parenthesized | `-flag` without parens — Error |
| 2 | Division by constant zero | `a / 8'd0` — Error: DIV_CONST_ZERO |
| 3 | Unguarded runtime division | `q <= n / d;` without IF guard — Warning |
| 4 | Ternary cond multi-bit | `8'd1 ? a : b` — cond must be 1-bit |
| 5 | x-bits to observable sink | `reg <= 8'bxxxx_0000;` — Error |
| 6 | x in non-CASE context | Binary literal with x used in arithmetic — observability error |

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | `-flag` (no parens) | Error | UNARY_NOT_PARENTHESIZED | Must parenthesize |
| 2 | `a / 8'd0` | Error | EXPR_DIVISION_BY_ZERO / DIV_CONST_ZERO | Constant zero divisor |
| 3 | `q <= n / d` (unguarded) | Warning | DIV_UNGUARDED_RUNTIME_ZERO | No nonzero proof |
| 4 | `8'd1 ? a : b` | Error | WIDTH_TERNARY_MISMATCH or similar | Cond must be 1-bit |
| 5 | `8'b1 * 8'b1` | 16-bit result | — | 2N width |
| 6 | `{8'hAA, 8'hBB}` | `16'hAABB` | — | MSB-first ordering |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `parser_expressions.c` | Parses all operator expressions | Token stream input |
| `driver_operators.c` | Operator-specific semantic validation | Expression AST input |
| `driver_expr.c` | Expression evaluation and x-tracking | Crafted expression trees |
| `ir_div_guard.c` | Division guard proof engine | Test guard conditions |
| `driver_width.c` | Width computation per operator | Verify result widths |
| `sim_eval.c` | Runtime operator evaluation | Simulation comparison |

## 6. Rules Matrix

### 6.1 Rules Tested

| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| UNARY_NOT_PARENTHESIZED | Unary `-`/`+` without parentheses | Neg 1 |
| EXPR_DIVISION_BY_ZERO | Division by constant zero | Neg 2 |
| DIV_CONST_ZERO | Compile-time constant zero divisor | Neg 2 |
| DIV_UNGUARDED_RUNTIME_ZERO | Runtime divisor without guard | Neg 3 |
| OBS_X_TO_OBSERVABLE_SINK | x-bits reaching observable sink | Neg 5, 6 |

### 6.2 Rules Missing

| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| TERNARY_COND_NOT_1BIT | S3.2 "cond must be width-1" | May be covered by WIDTH_TERNARY_MISMATCH or needs separate rule |
| LOGICAL_OP_NOT_1BIT | S3.2 "Operands must be width-1" | Logical AND/OR/NOT operand width check |
| CONCAT_EMPTY | S3.2 | Empty concatenation should be rejected |
