# Test Plan: 11.5 Tri-State Validation Rules

**Specification Reference:** Section 11.5 of jz-hdl-specification.md

## 1. Objective

Verify the validation rules applied before and after transformation: mutual exclusion proof for enable conditions and the full-width z requirement. These cross-reference the TRISTATE_TRANSFORM_* rules defined in Section 11.7.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Mutually exclusive drivers | Drivers with provably exclusive enables | Valid transformation proceeds |
| 2 | Full-width z assignments | All z assignments cover full signal width | Valid transformation proceeds |

### 2.2 Error Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Non-mutually-exclusive enables | Overlapping enable conditions | Error: TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL |
| 2 | Partial-width z | Per-bit z assignment pattern | Error: TRISTATE_TRANSFORM_PER_BIT_FAIL |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Statically provable exclusion | IF/ELSE structure guarantees exclusion | Valid |
| 2 | Complex enable expressions | Enables derived from multi-signal logic | May fail exclusion proof |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| 1 | Non-exclusive enables | Error | TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL | error | S11.7 |
| 2 | Per-bit z | Error | TRISTATE_TRANSFORM_PER_BIT_FAIL | error | S11.7 |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| `11_GND_5_HAPPY_PATH-tristate_validation_ok.jz` | — | Happy-path: validation passes after transformation |

The TRISTATE_TRANSFORM_* rules cross-referenced here are tested in test_11_7-tristate_error_conditions.md.

## 5. Rules Matrix

### 5.1 Rules Tested

This subsection introduces no new rules. It cross-references TRISTATE_TRANSFORM_* rules defined in Section 11.7. This is a happy-path-only plan; error cases are covered in test_11_7-tristate_error_conditions.md.
### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| TRISTATE_TRANSFORM_PER_BIT_FAIL | error | Bug: test exists (`11_GND_7_TRISTATE_TRANSFORM_PER_BIT_FAIL-per_bit_tristate.jz`) but rule has a known compiler bug |
| TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL | error | Dead code: test exists (`11_GND_7_TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL-non_exclusive.jz`) but rule is dead code |
