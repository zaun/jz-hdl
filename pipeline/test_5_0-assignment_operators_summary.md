# Test Plan: 5.0 Assignment Operators Summary

**Specification Reference:** Section 5.0 of jz-hdl-specification.md

## 1. Objective

Verify all 9 assignment operator variants: base operators (`=`, `=>`, `<=`), zero-extend variants (`=z`, `=>z`, `<=z`), and sign-extend variants (`=s`, `=>s`, `<=s`). Confirm width rules (same-width bare, modifier required for mismatch, truncation always illegal), sliced and concatenation assignments, and redundant modifier behavior. These rules apply across both ASYNCHRONOUS and SYNCHRONOUS contexts.

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
| 10 | Sliced assignment exact width | `bus[7:4] = expr[3:0];` (4 = 4) | Valid, exact match |
| 11 | Concatenation decomposition | `{a, b} = wide_result;` (sum == expr width) | Valid |

### 2.2 Error Cases

| # | Test Case | Input | Expected | Rule ID |
|---|-----------|-------|----------|---------|
| 1 | Directional width mismatch no modifier | `wide => narrow;` (16 => 8, no modifier) | Error | ASSIGN_WIDTH_NO_MODIFIER |
| 2 | Truncation with modifier | `narrow <=z wide;` (8 <=z 16) | Error | ASSIGN_TRUNCATES |
| 3 | Truncation without modifier | `narrow <= wide;` (8 <= 16) | Error | ASSIGN_TRUNCATES |
| 4 | Slice width mismatch | `bus[7:4] <= expr[2:0];` (4 vs 3) | Error | ASSIGN_SLICE_WIDTH_MISMATCH |
| 5 | Concat width mismatch | `{a, b} <= expr;` (sum != expr width) | Error | ASSIGN_CONCAT_WIDTH_MISMATCH |
| 6 | Alias width mismatch no modifier | `wide = narrow;` (16 = 8) | Error | WIDTH_ASSIGN_MISMATCH_NO_EXT |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | 1-bit to 256-bit extend | `wide <=z narrow;` (256 <=z 1) | Valid, maximum extension |
| 2 | Same width with all modifiers | `a =z b; a =s b;` (same width) | Valid, redundant but allowed |
| 3 | 1-bit signal all operators | Each of 9 operators on 1-bit signals | Valid when same width |

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1 | Directional width mismatch, no modifier | `wide => narrow;` (16 => 8) | ASSIGN_WIDTH_NO_MODIFIER | error |
| 2 | Truncation with zero-extend modifier | `narrow <=z wide;` (8 <=z 16) | ASSIGN_TRUNCATES | error |
| 3 | Truncation without modifier | `narrow <= wide;` (8 <= 16) | ASSIGN_TRUNCATES | error |
| 4 | Slice assignment width mismatch | `bus[7:4] <= 3'b101;` (4 vs 3) | ASSIGN_SLICE_WIDTH_MISMATCH | error |
| 5 | Concat total width != RHS width | `{a, b} <= expr;` (sum mismatch) | ASSIGN_CONCAT_WIDTH_MISMATCH | error |
| 6 | Alias width mismatch, no modifier | `wide = narrow;` (16 = 8) | WIDTH_ASSIGN_MISMATCH_NO_EXT | error |
| 7 | Valid zero-extend receive | `wide <=z narrow;` | -- | pass |
| 8 | Valid sign-extend alias | `wide =s narrow;` | -- | pass |
| 9 | Valid redundant modifier | `a =z b;` (same width) | -- | pass |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 5_0_HAPPY_PATH-assignment_operators_ok.jz | -- | All 9 assignment operator variants accepted |
| 5_0_ASSIGN_WIDTH_NO_MODIFIER-directional_width_mismatch.jz | ASSIGN_WIDTH_NO_MODIFIER | Width mismatch without modifier on directional operator |
| 5_0_ASSIGN_TRUNCATES-truncation_with_modifier.jz | ASSIGN_TRUNCATES | Assignment truncates RHS into smaller LHS even with modifier |
| 5_0_ASSIGN_SLICE_WIDTH_MISMATCH-slice_width_mismatch.jz | ASSIGN_SLICE_WIDTH_MISMATCH | Slice assignment width mismatch |
| 5_0_ASSIGN_CONCAT_WIDTH_MISMATCH-concat_width_mismatch.jz | ASSIGN_CONCAT_WIDTH_MISMATCH | Concatenation width does not match RHS width |
| 5_0_WIDTH_ASSIGN_MISMATCH_NO_EXT-alias_width_mismatch.jz | WIDTH_ASSIGN_MISMATCH_NO_EXT | Width mismatch on alias; add =z/=s or slice |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| ASSIGN_WIDTH_NO_MODIFIER | error | S4.10/S5.0 Width mismatch on directional operator without modifier | 5_0_ASSIGN_WIDTH_NO_MODIFIER-directional_width_mismatch.jz |
| ASSIGN_TRUNCATES | error | S4.10/S5.0 Assignment truncates RHS into smaller LHS | 5_0_ASSIGN_TRUNCATES-truncation_with_modifier.jz |
| ASSIGN_SLICE_WIDTH_MISMATCH | error | S4.10/S5.0/S5.1/S5.2 Slice assignment width mismatch | 5_0_ASSIGN_SLICE_WIDTH_MISMATCH-slice_width_mismatch.jz |
| ASSIGN_CONCAT_WIDTH_MISMATCH | error | S4.10/S5.0/S5.1/S5.2 Concatenation width does not match | 5_0_ASSIGN_CONCAT_WIDTH_MISMATCH-concat_width_mismatch.jz |
| ASSIGN_MULTIPLE_SAME_BITS | error | S1.5/S5.2 Same bits assigned more than once on a single execution path (exclusive assignment violation) | 10_6_ASSIGN_MULTIPLE_SAME_BITS-template_double_apply_async.jz, 1_5_ASSIGN_MULTIPLE_SAME_BITS-double_assign_same_path.jz |
| ASSIGN_INDEPENDENT_IF_SELECT | error | S1.5/S5.3/S5.4/S8.1 Same identifier assigned in multiple independent IF/SELECT chains at same nesting level | 1_5_ASSIGN_INDEPENDENT_IF_SELECT-independent_chains.jz |
| ASSIGN_SHADOWING | error | S1.5/S5.2/S8.1 Assignment at higher nesting level followed by nested assignment to same bits (sequential shadowing) | 1_5_ASSIGN_SHADOWING-sequential_shadow.jz |
| ASSIGN_SLICE_OVERLAP | error | S5.2/S8.1 Overlapping part-select assignments to same identifier bits in any single execution path | 1_5_ASSIGN_SLICE_OVERLAP-overlapping_slices.jz |

### 5.2 Rules Not Tested (in this section)

All rules for this section are tested.

### 5.3 Additional Rules Tested (not in primary assignment)

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| WIDTH_ASSIGN_MISMATCH_NO_EXT | error | S5.0/S5.1 Alias width mismatch; add =z/=s or slice | 5_0_WIDTH_ASSIGN_MISMATCH_NO_EXT-alias_width_mismatch.jz |
