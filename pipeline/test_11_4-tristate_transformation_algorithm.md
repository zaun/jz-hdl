# Test Plan: 11.4 Transformation Algorithm

**Specification Reference:** Section 11.4 of jz-hdl-specification.md

## 1. Objective

Verify the priority-chain conversion algorithm: z assignments are replaced with the default value (GND/VCC), drivers are chained via enable conditions, and whole-signal transformation policy is enforced (per-bit tri-state patterns are rejected).

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Two-driver priority chain | Two drivers with conditional z, flag set | Priority chain with one enable condition |
| 2 | Three-driver priority chain | Three drivers with conditional z, flag set | Two-level priority chain |
| 3 | Single driver transform | Single driver with conditional z, flag set | z replaced with default, TRISTATE_TRANSFORM_SINGLE_DRIVER warning |

### 2.2 Error Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Per-bit tri-state pattern | Individual bits assigned z independently | Error: TRISTATE_TRANSFORM_PER_BIT_FAIL |
| 2 | Non-mutually-exclusive enables | Overlapping enable conditions on drivers | Error: TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Maximum driver count | Many drivers chained | Deep priority chain generated |
| 2 | Single-bit full-width z | 1-bit signal with full-width z | Valid transform (not per-bit) |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| 1 | Per-bit z pattern | Error | TRISTATE_TRANSFORM_PER_BIT_FAIL | error | S11.7 |
| 2 | Non-exclusive enables | Error | TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL | error | S11.7 |
| 3 | Single driver tri-state | Warning | TRISTATE_TRANSFORM_SINGLE_DRIVER | warning | S11.7 |
| 4 | Successful transform | Info | INFO_TRISTATE_TRANSFORM | info | S11 |

## 4. Existing Validation Tests

No validation tests directly mapped to this subsection. Transformation is backend-oriented and requires the `--tristate-default` flag, which is outside the `--info --lint` framework.

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| TRISTATE_TRANSFORM_PER_BIT_FAIL | error | Per-bit tri-state pattern detected; only full-width z can be transformed | Error 1 |
| TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL | error | Non-mutually-exclusive enable conditions | Error 2 |
| TRISTATE_TRANSFORM_SINGLE_DRIVER | warning | Single-driver tri-state net transformed | Happy 3 |
| INFO_TRISTATE_TRANSFORM | info | Net transformed by --tristate-default | Happy 1, 2 |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| TRISTATE_TRANSFORM_PER_BIT_FAIL | error | Requires `--tristate-default` flag; not testable via `--info --lint` |
| TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL | error | Requires `--tristate-default` flag; not testable via `--info --lint` |
| TRISTATE_TRANSFORM_SINGLE_DRIVER | warning | Requires `--tristate-default` flag; not testable via `--info --lint` |
| INFO_TRISTATE_TRANSFORM | info | Requires `--tristate-default` flag; not testable via `--info --lint` |
