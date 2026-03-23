# Test Plan: 11.1 --tristate-default Purpose and Overview

**Specification Reference:** Section 11.1 of jz-hdl-specification.md

## 1. Objective

Verify the `--tristate-default` flag converts internal tri-state nets to priority-chained conditional logic with GND or VCC default. When the flag is absent, internal tri-state nets are preserved (ASIC/simulation mode) and a warning is emitted.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | GND default transform | `--tristate-default=GND` with internal tri-state net | z replaced with 0, INFO_TRISTATE_TRANSFORM emitted |
| 2 | VCC default transform | `--tristate-default=VCC` with internal tri-state net | z replaced with 1, INFO_TRISTATE_TRANSFORM emitted |
| 3 | No flag, ASIC mode | Internal tri-state net without flag | z preserved, WARN_INTERNAL_TRISTATE warning |

### 2.2 Error Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Invalid flag value | `--tristate-default=INVALID` | Compiler error (invalid argument) |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Flag with no tri-state nets | `--tristate-default=GND` on design with no tri-state | TRISTATE_TRANSFORM_UNUSED_DEFAULT warning |
| 2 | Multiple tri-state nets | `--tristate-default=GND` with several nets | Each net transformed, INFO per net |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| 1 | `--tristate-default=GND` | z replaced with 0 | INFO_TRISTATE_TRANSFORM | info | S11.1 |
| 2 | `--tristate-default=VCC` | z replaced with 1 | INFO_TRISTATE_TRANSFORM | info | S11.1 |
| 3 | No flag, internal tristate | Warning emitted | WARN_INTERNAL_TRISTATE | warning | S11 |
| 4 | Flag, no tri-state nets | Warning emitted | TRISTATE_TRANSFORM_UNUSED_DEFAULT | warning | S11.7 |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| `11_1_WARN_INTERNAL_TRISTATE-internal_tristate_warning.jz` | WARN_INTERNAL_TRISTATE | Internal tri-state without flag emits warning |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| WARN_INTERNAL_TRISTATE | warning | Internal tri-state logic without `--tristate-default` flag | Happy 3 |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| INFO_TRISTATE_TRANSFORM | info | Requires `--tristate-default` flag (backend-oriented); not testable via `--info --lint` |
| TRISTATE_TRANSFORM_UNUSED_DEFAULT | warning | Requires `--tristate-default` flag; not testable via `--info --lint` |
