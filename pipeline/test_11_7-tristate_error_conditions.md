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

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| `11_GND_7_TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL-non_exclusive.jz` | TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL | Non-mutually-exclusive enables (GND default) |
| `11_GND_7_TRISTATE_TRANSFORM_PER_BIT_FAIL-per_bit_tristate.jz` | TRISTATE_TRANSFORM_PER_BIT_FAIL | Per-bit tri-state pattern (GND default) |
| `11_GND_7_TRISTATE_TRANSFORM_BLACKBOX_PORT-blackbox_tristate.jz` | TRISTATE_TRANSFORM_BLACKBOX_PORT | Tri-state driven by blackbox port (GND default) |
| `11_GND_7_TRISTATE_TRANSFORM_OE_EXTRACT_FAIL-ambiguous_oe.jz` | TRISTATE_TRANSFORM_OE_EXTRACT_FAIL | Ambiguous output-enable extraction (GND default) |
| `11_GND_7_TRISTATE_TRANSFORM_SINGLE_DRIVER-single_driver.jz` | TRISTATE_TRANSFORM_SINGLE_DRIVER | Single-driver tri-state net (GND default) |
| `11_GND_7_TRISTATE_TRANSFORM_UNUSED_DEFAULT-no_tristate_nets.jz` | TRISTATE_TRANSFORM_UNUSED_DEFAULT | No internal tri-state nets found (GND default) |
| `11_GND_4_INFO_TRISTATE_TRANSFORM-gnd_transform.jz` | INFO_TRISTATE_TRANSFORM | Successful transform info (GND default) |
| `11_GND_4_INFO_TRISTATE_TRANSFORM-single_bit_fullwidth_z.jz` | INFO_TRISTATE_TRANSFORM | Single-bit full-width z transform (GND default) |
| `11_VCC_4_INFO_TRISTATE_TRANSFORM-vcc_transform.jz` | INFO_TRISTATE_TRANSFORM | Successful transform info (VCC default) |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| INFO_TRISTATE_TRANSFORM | info | Tri-state net successfully transformed | `11_GND_4_INFO_TRISTATE_TRANSFORM-gnd_transform.jz`, `11_GND_4_INFO_TRISTATE_TRANSFORM-single_bit_fullwidth_z.jz`, `11_VCC_4_INFO_TRISTATE_TRANSFORM-vcc_transform.jz` |
| TRISTATE_TRANSFORM_OE_EXTRACT_FAIL | error | Could not extract output-enable condition; _oe driven high as fallback | `11_GND_7_TRISTATE_TRANSFORM_OE_EXTRACT_FAIL-ambiguous_oe.jz` |
| TRISTATE_TRANSFORM_UNUSED_DEFAULT | warning | --tristate-default specified but no internal tri-state nets found | `11_GND_7_TRISTATE_TRANSFORM_UNUSED_DEFAULT-no_tristate_nets.jz` |
| WARN_INTERNAL_TRISTATE | warning | S11 Internal tri-state logic is not FPGA-compatible; use --tristate-default=GND or --tristate-default=VCC | 11_1_WARN_INTERNAL_TRISTATE-internal_tristate_warning.jz, 12_3_WARN_INTERNAL_TRISTATE-internal_tristate.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| TRISTATE_TRANSFORM_SINGLE_DRIVER | warning | Bug: test exists (`11_GND_7_TRISTATE_TRANSFORM_SINGLE_DRIVER-single_driver.jz`) but rule has a known compiler bug |
| TRISTATE_TRANSFORM_PER_BIT_FAIL | error | Bug: test exists (`11_GND_7_TRISTATE_TRANSFORM_PER_BIT_FAIL-per_bit_tristate.jz`) but rule has a known compiler bug |
| TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL | error | Dead code: test exists (`11_GND_7_TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL-non_exclusive.jz`) but rule is dead code |
| TRISTATE_TRANSFORM_BLACKBOX_PORT | error | Bug: test exists (`11_GND_7_TRISTATE_TRANSFORM_BLACKBOX_PORT-blackbox_tristate.jz`) but rule has a known compiler bug |
