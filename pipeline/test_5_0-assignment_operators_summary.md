# Test Plan: 5.0 Assignment Operators Summary

**Specification Reference:** Section 5.0 of jz-hdl-specification.md

## 1. Objective

Verify all 9 assignment operator variants: base operators (`=`, `=>`, `<=`), zero-extend variants (`=z`, `=>z`, `<=z`), and sign-extend variants (`=s`, `=>s`, `<=s`). Confirm width rules (same-width bare, modifier required for mismatch, truncation always illegal), and redundant modifier behavior.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Same-width alias | `a = b;` (both 8-bit) | Valid assignment |
| 2 | Same-width drive | `a => b;` (both 8-bit) | Valid assignment |
| 3 | Same-width receive | `a <= b;` (both 8-bit) | Valid assignment |
| 4 | Zero-extend alias | `wide =z narrow;` (16 =z 8) | Valid, narrow zero-extended to 16 |
| 5 | Sign-extend alias | `wide =s narrow;` (16 =s 8) | Valid, narrow sign-extended to 16 |
| 6 | Zero-extend receive | `wide <=z narrow;` | Valid, narrow zero-extended |
| 7 | Sign-extend drive | `wide =>s narrow;` | Valid, narrow sign-extended |
| 8 | Redundant modifier | `a =z b;` (same width) | Valid, harmless redundancy |
| 9 | All 9 operators exercised | One test per operator variant | All valid when widths match or extend |

### 2.2 Error Cases

| # | Test Case | Input | Expected | Rule ID |
|---|-----------|-------|----------|---------|
| 1 | Width mismatch no modifier | `wide = narrow;` (16 = 8) | Error | WIDTH_ASSIGN_MISMATCH_NO_EXT |
| 2 | Truncation without modifier | `narrow <= wide;` (8 <= 16) | Error | ASSIGN_TRUNCATES |
| 3 | Truncation with modifier | `narrow <=z wide;` (8 <=z 16) | Error | ASSIGN_TRUNCATES |
| 4 | Slice width mismatch | `bus[7:4] <= expr[2:0];` (4 vs 3) | Error | ASSIGN_SLICE_WIDTH_MISMATCH |
| 5 | Concat width mismatch | `{a, b} <= expr;` (sum != expr width) | Error | ASSIGN_CONCAT_WIDTH_MISMATCH |
| 6 | Drive width mismatch no modifier | `wide => narrow;` (16 => 8) | Error | ASSIGN_WIDTH_NO_MODIFIER |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | 1-bit to 256-bit extend | `wide <=z narrow;` (256 <=z 1) | Valid, maximum extension |
| 2 | Same width with all modifiers | `a =z b; a =s b;` (same width) | Valid, redundant but allowed |
| 3 | 1-bit signal all operators | Each of 9 operators on 1-bit signals | Valid when same width |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| 1 | `wide = narrow;` (no modifier) | Compile error | WIDTH_ASSIGN_MISMATCH_NO_EXT | error | Requires z/s modifier |
| 2 | `narrow <= wide;` (truncation) | Compile error | ASSIGN_TRUNCATES | error | Truncation always illegal |
| 3 | `narrow <=z wide;` (truncation with mod) | Compile error | ASSIGN_TRUNCATES | error | Modifier cannot make truncation valid |
| 4 | `wide <=z narrow;` | Accepted | -- | -- | Zero-extend narrow into wider LHS |
| 5 | `bus[7:4] <= 3'b101;` | Compile error | ASSIGN_SLICE_WIDTH_MISMATCH | error | Slice widths must match |
| 6 | `{a, b} <= expr;` (width sum mismatch) | Compile error | ASSIGN_CONCAT_WIDTH_MISMATCH | error | Concat total must equal RHS width |
| 7 | `wide => narrow;` (no modifier) | Compile error | ASSIGN_WIDTH_NO_MODIFIER | error | Drive requires modifier for mismatch |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 5_0_ASSIGN_TRUNCATES-truncation_with_modifier.jz | ASSIGN_TRUNCATES | Assignment truncates RHS into smaller LHS even with modifier |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| ASSIGN_TRUNCATES | error | S4.10/S5.0 Assignment truncates RHS into smaller LHS; use a slice or explicit truncation modifier | 5_0_ASSIGN_TRUNCATES-truncation_with_modifier.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| ASSIGN_WIDTH_NO_MODIFIER | error | No 5_0-prefixed test; covered implicitly by other sections (S4.10) |
| ASSIGN_SLICE_WIDTH_MISMATCH | error | No 5_0-prefixed test; covered by S4.10 and S5.1/S5.2 tests |
| ASSIGN_CONCAT_WIDTH_MISMATCH | error | No 5_0-prefixed test; covered by S4.10 and S5.1/S5.2 tests |
| WIDTH_ASSIGN_MISMATCH_NO_EXT | error | No 5_0-prefixed test; covered by broader width-checking tests in other sections |
