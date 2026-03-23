# Test Plan: 5.4 SELECT / CASE Statements

**Specification Reference:** Section 5.4 of jz-hdl-specification.md

## 1. Objective

Verify SELECT/CASE syntax, CASE value matching (constants/CONST), x-bit wildcard semantics in CASE patterns, duplicate CASE detection, fall-through (naked CASE labels), DEFAULT behavior (optional in SYNC, recommended in ASYNC), and incomplete coverage handling.

## 2. Instrumentation Strategy

- **Span: `parser.select`** — Trace SELECT parsing; attributes: `case_count`, `has_default`, `has_fallthrough`.
- **Span: `sem.case_match`** — CASE value analysis; attributes: `value`, `has_x_wildcard`, `is_duplicate`.
- **Event: `select.duplicate_case`** — Two CASE values match same pattern.
- **Event: `select.incomplete_async`** — No DEFAULT in ASYNC.

## 3. Test Scenarios

### 3.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Simple SELECT | `SELECT (state) { CASE 0 { ... } DEFAULT { ... } }` |
| 2 | Multiple CASEs | Four CASE arms with DEFAULT |
| 3 | x-wildcard CASE | `CASE 8'b1010_xxxx { ... }` — don't-care bits |
| 4 | Fall-through | `CASE 0 CASE 1 { ... }` — shared handler |
| 5 | SYNC without DEFAULT | Valid: registers hold |
| 6 | CONST in CASE | `CASE MY_CONST { ... }` |
| 7 | Nested SELECT | SELECT inside CASE body |

### 3.2 Boundary/Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Single CASE + DEFAULT | Minimum useful SELECT |
| 2 | Many CASEs (256) | One per possible value of 8-bit selector |
| 3 | All-x CASE | `CASE 8'bxxxx_xxxx` — matches everything (like DEFAULT) |
| 4 | x-wildcard with non-x mix | `CASE 8'b10xx_0011` |

### 3.3 Negative Testing

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Duplicate CASE values | `CASE 0 { ... } CASE 0 { ... }` — Error |
| 2 | ASYNC without DEFAULT | Warning: incomplete coverage |
| 3 | CASE value width mismatch | CASE value width ≠ selector width |
| 4 | Runtime expression in CASE | Non-constant CASE value — Error |
| 5 | SELECT expr not matching | Selector and CASE widths differ |

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Duplicate CASE 0 | Error | SELECT_DUPLICATE_CASE | S5.4 |
| 2 | ASYNC no DEFAULT | Warning | WARN_INCOMPLETE_SELECT_ASYNC | S5.4 |
| 3 | CASE width ≠ selector | Error | SELECT_WIDTH_MISMATCH | S5.4 |
| 4 | x-wildcard in CASE | Valid | — | Pattern matching |
| 5 | Fall-through CASE 0 CASE 1 | Valid: shared handler | — | S5.4 |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `parser_statements.c` | Parses SELECT/CASE | Token stream |
| `driver_control.c` | CASE matching and coverage analysis | Integration test |
| `const_eval.c` | CONST value evaluation in CASE | Unit test |
| `driver_assign.c` | Exclusive assignment within SELECT | Integration test |

## 6. Rules Matrix

### 6.1 Rules Tested

| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| SELECT_DUPLICATE_CASE | Duplicate CASE values | Neg 1 |
| WARN_INCOMPLETE_SELECT_ASYNC | No DEFAULT in ASYNC | Neg 2 |

### 6.2 Rules Missing

| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| SELECT_CASE_WIDTH_MISMATCH | S5.4 | CASE value width vs selector width |
| SELECT_CASE_NOT_CONSTANT | S5.4 "integer constants or CONST" | Runtime expression in CASE value |
| SELECT_X_WILDCARD_OVERLAP | S5.4 | Overlapping x-wildcard patterns that could match same value |
