# Test Plan: 4.3 CONST (Compile-Time Constants)

**Specification Reference:** Section 4.3 of jz-hdl-specification.md

## 1. Objective

Verify CONST declaration (numeric and string), compile-time evaluation, module-local scope, usage in width expressions and array indices, string CONST restrictions (@file paths only), type restrictions (string in numeric context, numeric in string context), and forward/circular reference detection.

## 2. Instrumentation Strategy

- **Span: `sem.const_eval`** — Trace CONST evaluation; attributes: `const_name`, `value`, `type` (numeric/string).
- **Event: `const.circular_dep`** — Circular dependency detected.
- **Event: `const.forward_ref`** — Forward reference detected.
- **Event: `const.type_mismatch`** — String used in numeric context or vice versa.

## 3. Test Scenarios

### 3.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Numeric CONST | `CONST { WIDTH = 32; }` — used in port width |
| 2 | String CONST | `CONST { FILE = "data.hex"; }` — used in @file |
| 3 | Multiple CONSTs | `WIDTH = 32; DEPTH = 256;` — multiple declarations |
| 4 | CONST in width | `PORT { IN [WIDTH] data; }` — valid |
| 5 | CONST arithmetic | `DEPTH = 2; ADDR = clog2(DEPTH);` — expressions |
| 6 | CONST in MEM depth | `MEM { m [8] [DEPTH] = ... }` |

### 3.2 Boundary/Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | CONST = 0 | `ZERO = 0;` — valid value but can't use as width |
| 2 | Large CONST | `BIG = 65536;` — large constant |
| 3 | CONST referencing CONST | `A = 8; B = A * 2;` — valid chain |

### 3.3 Negative Testing

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Duplicate CONST | `CONST { A = 1; A = 2; }` — Error |
| 2 | Circular dependency | `A = B; B = A;` — Error |
| 3 | Forward reference | `A = B; B = 5;` — Error |
| 4 | Negative width | `WIDTH = -1;` — Error |
| 5 | String in numeric context | `PORT { IN [FILE] data; }` — Error |
| 6 | Numeric in string context | `@file(WIDTH)` — Error |
| 7 | Undefined CONST | `PORT { IN [UNDEF] data; }` — Error |

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | `CONST { A = 1; A = 2; }` | Error | CONST_DUPLICATE | S4.3 |
| 2 | `A = B; B = A;` | Error | CONST_CIRCULAR_DEP | S4.3 |
| 3 | `A = B; B = 5;` | Error | CONST_FORWARD_REF | S4.3 |
| 4 | `WIDTH = -1` in port width | Error | CONST_NEGATIVE_WIDTH | S4.3 |
| 5 | `PORT { IN [FILE] data; }` | Error | CONST_STRING_IN_NUMERIC_CONTEXT | S4.3 |
| 6 | `@file(WIDTH)` | Error | CONST_NUMERIC_IN_STRING_CONTEXT | S4.3 |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `const_eval.c` | CONST evaluation engine | Unit test with CONST declarations |
| `parser_core.c` | CONST block parsing | Token stream input |
| `driver.c` | Scope validation | Integration test |
| `diagnostic.c` | Error reporting | Capture diagnostics |

## 6. Rules Matrix

### 6.1 Rules Tested

| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| CONST_DUPLICATE | Duplicate CONST name | Neg 1 |
| CONST_CIRCULAR_DEP | Circular CONST dependency | Neg 2 |
| CONST_FORWARD_REF | Forward reference in CONST | Neg 3 |
| CONST_NEGATIVE_WIDTH | Negative value used as width | Neg 4 |
| CONST_STRING_IN_NUMERIC_CONTEXT | String CONST where numeric expected | Neg 5 |
| CONST_NUMERIC_IN_STRING_CONTEXT | Numeric CONST where string expected | Neg 6 |

### 6.2 Rules Missing

| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| — | All expected rules appear covered | — |
