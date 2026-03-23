# Test Plan: 3.4 Operator Examples

**Specification Reference:** Section 3.4 of jz-hdl-specification.md

## 1. Objective

Verify that the canonical operator examples from the specification compile correctly and produce expected results: unary negation with parentheses, multi-bit negation via subtraction, arithmetic vs logical right shift, overflow prevention via concatenation, ternary with concatenation, and tri-state driver patterns.

## 2. Instrumentation Strategy

- **Span: `compile.example`** — Trace compilation of each example; attributes: `example_name`, `success`.
- **Span: `sim.eval_expr`** — Simulate expression evaluation; attributes: `input_values`, `result_value`.
- **Coverage Hook:** Each spec example is a separate test case with known inputs and expected outputs.

## 3. Test Scenarios

### 3.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Unary negation | `flag = (-flag);` | Two's complement of flag |
| 2 | Multi-bit negate | `negated = 8'h00 - input_val;` | Two's complement via subtraction |
| 3 | Logical right shift | `8'b1000_0001 >> 1'b1` | `8'b0100_0000` (zero-fill) |
| 4 | Arithmetic right shift | `8'b1000_0001 >>> 1'b1` | `8'b1100_0000` (MSB replicated) |
| 5 | Carry capture | `{carry, sum} = {1'b0, a} + {1'b0, b};` | 9-bit result with carry |
| 6 | Ternary + concat | `result = sel ? {a, b} : {c, d};` | Selected concatenation |
| 7 | Tri-state driver | `inout_port = enable ? data_out : 8'bzzzz_zzzz;` | Conditional drive/release |

### 3.2 Boundary/Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Negate zero | `(-8'd0)` = `8'd0` |
| 2 | Negate max | `(-8'd255)` = `8'd1` (two's complement) |
| 3 | Shift by zero | `a >> 3'd0` = `a` |
| 4 | Carry = 0 case | `{carry, sum} = {1'b0, 8'd100} + {1'b0, 8'd50}` → carry=0 |
| 5 | Carry = 1 case | `{carry, sum} = {1'b0, 8'd200} + {1'b0, 8'd200}` → carry=1 |

### 3.3 Negative Testing

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Missing parens on negate | `-flag` (no parens) → Error |
| 2 | Ternary width mismatch | `sel ? {a, b} : c` where concat width ≠ c width → Error |

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | `(-8'd5)` | `8'd251` (two's complement) | — | Parenthesized, valid |
| 2 | `8'b1000_0001 >> 1'b1` | `8'b0100_0000` | — | Logical shift: zero-fill |
| 3 | `8'b1000_0001 >>> 1'b1` | `8'b1100_0000` | — | Arith shift: MSB replicate |
| 4 | `{1'b0, 8'd200} + {1'b0, 8'd200}` | `9'b1_1001_0000` | — | Carry captured |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `parser_expressions.c` | Parse example expressions | Token stream input |
| `driver_operators.c` | Semantic validation | Expression AST |
| `sim_eval.c` | Runtime evaluation | Simulate with known values |
| `ir_build_expr.c` | IR generation | Verify IR matches expected operations |

## 6. Rules Matrix

### 6.1 Rules Tested

| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| UNARY_NOT_PARENTHESIZED | Unary without parens | Neg 1 |
| WIDTH_TERNARY_MISMATCH | Ternary arm width mismatch | Neg 2 |

### 6.2 Rules Missing

| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| — | S3.4 is examples only | No new rules introduced; all rules tested here are from S3.1-3.2 |
