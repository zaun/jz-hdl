# Test Plan: 2.2 Signedness Model

**Specification Reference:** Section 2.2 of jz-hdl-specification.md

## 1. Objective

Verify that all signals, literals, and operators in JZ-HDL are treated as unsigned by default, that there is no implicit signed type, and that signed arithmetic requires explicit use of `sadd` and `smul` intrinsic operators (Section 5.5).

## 2. Instrumentation Strategy

- **Span: `sem.type_check`** — Trace type checking of expressions; attributes: `signedness` (always unsigned), `operand_widths`.
- **Span: `sem.intrinsic_validate`** — Validate intrinsic operator usage; attributes: `intrinsic_name`, `operand_types`.
- **Coverage Hook:** Verify that all arithmetic and comparison operators produce unsigned results; ensure `sadd`/`smul` code paths are distinct.

## 3. Test Scenarios

### 3.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Unsigned addition | `8'd200 + 8'd55` = `8'd255` (unsigned) |
| 2 | Unsigned subtraction | `8'd10 - 8'd20` wraps unsigned |
| 3 | Unsigned comparison | `8'd200 > 8'd100` = true (unsigned, no sign issue) |
| 4 | sadd intrinsic | `sadd(a, b)` produces signed result with carry |
| 5 | smul intrinsic | `smul(a, b)` produces signed product |
| 6 | All operators unsigned | `+`, `-`, `*`, `/`, `%`, `<`, `>`, `<=`, `>=` all unsigned |

### 3.2 Boundary/Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Max unsigned value | `8'd255 + 8'd1` wraps to 0 (unsigned overflow) |
| 2 | MSB = 1 in comparison | `8'd128 > 8'd127` = true (unsigned, not negative) |
| 3 | Two's complement interpretation | Value `8'b1111_1111` is 255 unsigned, not -1 |

### 3.3 Negative Testing

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | No signed keyword | No `signed` keyword exists in the language |
| 2 | Signed comparison via regular ops | `8'd255 > 8'd0` = true (unsigned always, even if user expects signed) |

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | `8'd200 + 8'd55` | `8'd255` (unsigned) | — | Normal unsigned arithmetic |
| 2 | `8'd200 + 8'd56` | `8'd0` (unsigned wrap) | — | Unsigned overflow wraps |
| 3 | `sadd(8'd200, 8'd56)` | 9-bit signed result | — | Intrinsic signed add |
| 4 | `8'd128 > 8'd127` | True | — | Unsigned comparison |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `sem_type.c` | Type system — all types are unsigned | Verify type annotations on expressions |
| `driver_operators.c` | Operator semantic checking | Verify unsigned semantics for all operators |
| `driver_expr.c` | Expression evaluation | Verify no implicit signed promotion |
| `sim_eval.c` | Simulation of unsigned operations | Compare sim output with expected unsigned results |

## 6. Rules Matrix

### 6.1 Rules Tested

| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| — | No specific rule IDs for signedness | Verified by absence of signed type errors |

### 6.2 Rules Missing

| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| SIGNED_KEYWORD_USED | S2.2 "no implicit signed type" | No rule to reject a hypothetical `signed` keyword (handled by KEYWORD_AS_IDENTIFIER or lexer) |
| SIGNED_OP_WITHOUT_INTRINSIC | S2.2 | No warning when user might intend signed comparison but uses unsigned operator |
