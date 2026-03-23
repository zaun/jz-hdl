# Test Plan: 5.0 Assignment Operators Summary

**Specification Reference:** Section 5.0 of jz-hdl-specification.md

## 1. Objective

Verify all 9 assignment operator variants: base operators (`=`, `=>`, `<=`), zero-extend variants (`=z`, `=>z`, `<=z`), and sign-extend variants (`=s`, `=>s`, `<=s`). Confirm width rules (same-width bare, modifier required for mismatch, truncation always illegal), and redundant modifier behavior.

## 2. Instrumentation Strategy

- **Span: `sem.assign_op`** — Trace each assignment; attributes: `operator`, `lhs_width`, `rhs_width`, `modifier`, `block_type`.
- **Event: `assign.width_mismatch`** — Widths differ without modifier.
- **Event: `assign.truncation`** — Truncation would be needed.

## 3. Test Scenarios

### 3.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Same-width alias | `a = b;` (both 8-bit) |
| 2 | Same-width drive | `a => b;` (both 8-bit) |
| 3 | Same-width receive | `a <= b;` (both 8-bit) |
| 4 | Zero-extend alias | `wide =z narrow;` (16 =z 8) |
| 5 | Sign-extend alias | `wide =s narrow;` (16 =s 8) |
| 6 | Zero-extend receive | `wide <=z narrow;` |
| 7 | Sign-extend drive | `wide =>s narrow;` |
| 8 | Redundant modifier | `a =z b;` (same width — harmless) |
| 9 | All 9 operators exercised | One test per operator variant |

### 3.2 Boundary/Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | 1-bit to 256-bit extend | Maximum extension |
| 2 | Same width with all modifiers | Redundant but valid |

### 3.3 Negative Testing

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Width mismatch no modifier | `wide = narrow;` — Error |
| 2 | Truncation attempt | `narrow <= wide;` without modifier — Error |
| 3 | Truncation with modifier | `narrow <=z wide;` — still Error (wider RHS) |

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | `wide = narrow;` (no mod) | Error | WIDTH_ASSIGN_MISMATCH_NO_EXT | Requires modifier |
| 2 | `narrow <= wide;` | Error | ASSIGN_WIDTH_NO_MODIFIER | Truncation |
| 3 | `narrow <=z wide;` | Error | — | Can't zero-extend into narrower |
| 4 | `wide <=z narrow;` | Valid | — | Zero-extend narrow to wide |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `driver_assign.c` | Assignment operator validation | Unit test |
| `driver_width.c` | Width matching and extension | Unit test |
| `parser_statements.c` | Parses assignment operators | Token stream |

## 6. Rules Matrix

### 6.1 Rules Tested

| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| WIDTH_ASSIGN_MISMATCH_NO_EXT | Width mismatch without modifier | Neg 1 |
| ASSIGN_WIDTH_NO_MODIFIER | Truncation or mismatch | Neg 2 |

### 6.2 Rules Missing

| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| ASSIGN_TRUNCATION | S5.0 "truncation is never implicit" | Explicit rule for truncation attempt even with modifier |
