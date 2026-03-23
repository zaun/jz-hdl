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

No validation tests specific to this subsection. These rules are cross-references to TRISTATE_TRANSFORM_* rules that require the `--tristate-default` flag.

## 5. Rules Matrix

### 5.1 Rules Tested

Cross-references to rules tested in Sections 11.4 and 11.7:

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL | error | Non-mutually-exclusive enable conditions | Error 1 |
| TRISTATE_TRANSFORM_PER_BIT_FAIL | error | Per-bit tri-state pattern | Error 2 |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL | error | Requires `--tristate-default` flag; not testable via `--info --lint` |
| TRISTATE_TRANSFORM_PER_BIT_FAIL | error | Requires `--tristate-default` flag; not testable via `--info --lint` |
