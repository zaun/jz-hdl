# Test Plan: 11.7 Tri-State Error Conditions and Warnings

**Specification Reference:** Section 11.7 of jz-hdl-specification.md

## 1. Objective

Verify all tristate transform errors and warnings are correctly detected and reported. This is the comprehensive error matrix for the `--tristate-default` transformation pipeline.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Clean transform | Valid tri-state design with flag | Transform succeeds, INFO emitted per net |

### 2.2 Error Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Mutual exclusion fail | Non-mutually-exclusive enable conditions | Error: TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL |
| 2 | Per-bit fail | Individual bits assigned z independently | Error: TRISTATE_TRANSFORM_PER_BIT_FAIL |
| 3 | Blackbox port | Tri-state driven by blackbox port | Error: TRISTATE_TRANSFORM_BLACKBOX_PORT |
| 4 | OE extraction fail | Ambiguous output-enable condition | Error: TRISTATE_TRANSFORM_OE_EXTRACT_FAIL |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Single driver | Single-driver tri-state net with flag | Warning: TRISTATE_TRANSFORM_SINGLE_DRIVER |
| 2 | No tri-state nets | Flag specified but no internal tri-state | Warning: TRISTATE_TRANSFORM_UNUSED_DEFAULT |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| 1 | Non-exclusive enables | Error | TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL | error | S11.7 |
| 2 | Per-bit z pattern | Error | TRISTATE_TRANSFORM_PER_BIT_FAIL | error | S11.7 |
| 3 | Blackbox port connection | Error | TRISTATE_TRANSFORM_BLACKBOX_PORT | error | S11.7 |
| 4 | OE extraction failure | Error | TRISTATE_TRANSFORM_OE_EXTRACT_FAIL | error | S11.7 |
| 5 | Single driver tri-state | Warning | TRISTATE_TRANSFORM_SINGLE_DRIVER | warning | S11.7 |
| 6 | No tri-state nets found | Warning | TRISTATE_TRANSFORM_UNUSED_DEFAULT | warning | S11.7 |

## 4. Existing Validation Tests

No validation tests directly mapped to this subsection. All TRISTATE_TRANSFORM_* rules require the `--tristate-default` flag, which is outside the `--info --lint` framework.

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL | error | Non-mutually-exclusive enable conditions; cannot build safe priority chain | Error 1 |
| TRISTATE_TRANSFORM_PER_BIT_FAIL | error | Per-bit tri-state pattern; only full-width z can be transformed | Error 2 |
| TRISTATE_TRANSFORM_BLACKBOX_PORT | error | Tri-state driven by blackbox port; use external pull resistor | Error 3 |
| TRISTATE_TRANSFORM_OE_EXTRACT_FAIL | error | Could not extract output-enable condition; _oe driven high as fallback | Error 4 |
| TRISTATE_TRANSFORM_SINGLE_DRIVER | warning | Single-driver tri-state net; z replaced with GND/VCC | Edge 1 |
| TRISTATE_TRANSFORM_UNUSED_DEFAULT | warning | --tristate-default specified but no internal tri-state nets found | Edge 2 |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL | error | Requires `--tristate-default` flag; not testable via `--info --lint` |
| TRISTATE_TRANSFORM_PER_BIT_FAIL | error | Requires `--tristate-default` flag; not testable via `--info --lint` |
| TRISTATE_TRANSFORM_BLACKBOX_PORT | error | Requires `--tristate-default` flag; not testable via `--info --lint` |
| TRISTATE_TRANSFORM_OE_EXTRACT_FAIL | error | Requires `--tristate-default` flag; not testable via `--info --lint` |
| TRISTATE_TRANSFORM_SINGLE_DRIVER | warning | Requires `--tristate-default` flag; not testable via `--info --lint` |
| TRISTATE_TRANSFORM_UNUSED_DEFAULT | warning | Requires `--tristate-default` flag; not testable via `--info --lint` |
