# Test Plan: 6.5 PIN Blocks

**Specification Reference:** Section 6.5 of jz-hdl-specification.md

## 1. Objective

Verify PIN blocks (IN_PINS, OUT_PINS, INOUT_PINS): electrical standards, drive strength, bus syntax, pin name uniqueness across all categories, differential signaling modes, pull resistors, termination, and chip-specific standard validation.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | IN_PIN with LVCMOS33 | `clk = { standard=LVCMOS33 };` -- standard input pin |
| 2 | OUT_PIN with drive | `led = { standard=LVCMOS18, drive=8 };` -- output with drive strength |
| 3 | INOUT_PIN | `sda = { standard=LVCMOS33, drive=4 };` -- bidirectional pin |
| 4 | Bus pins | `button[4] = { standard=LVCMOS33 };` -- multi-bit bus |
| 5 | Differential input | `lvds_in = { standard=LVDS25, mode=DIFFERENTIAL };` |
| 6 | Pin with pull resistor | `btn = { standard=LVCMOS33, pull=UP };` -- input with pull-up |
| 7 | Pin with termination | `data = { standard=SSTL18, term=ON };` -- input with termination |

### 2.2 Error Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Pin in multiple blocks | Same pin name in IN_PINS and OUT_PINS |
| 2 | Invalid standard | Unrecognized electrical standard |
| 3 | Drive missing or invalid | OUT_PIN without drive or non-positive drive |
| 4 | Invalid bus width | Bus width <= 0 or non-integer |
| 5 | Duplicate name within block | Same pin name declared twice in one block |
| 6 | Invalid mode | Mode value not SINGLE or DIFFERENTIAL |
| 7 | Mode/standard mismatch | DIFFERENTIAL mode with single-ended standard |
| 8 | Invalid pull | Pull value not UP, DOWN, or NONE |
| 9 | Pull on output | Pull resistor on OUT_PINS |
| 10 | Invalid termination | Termination value not ON or OFF |
| 11 | Termination on output | Termination on OUT_PINS |
| 12 | Termination invalid for standard | Termination not supported for this I/O standard |
| 13 | Diff output missing fclk | Differential output pin without fclk attribute |
| 14 | Diff output missing pclk | Differential output pin without pclk attribute |
| 15 | Diff output missing reset | Differential output pin without reset attribute |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Width = 1 scalar pin | Explicit [1] width same as scalar |
| 2 | Large bus | `data[32] = { ... }` -- wide bus pin |
| 3 | All supported I/O standards | Each standard validated against chip |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|-----------------|---------|----------|-------|
| 1 | Pin in IN and OUT blocks | Error | PIN_DECLARED_MULTIPLE_BLOCKS | error | S6.5 |
| 2 | Invalid standard | Error | PIN_INVALID_STANDARD | error | S6.5 |
| 3 | OUT without drive or bad drive | Error | PIN_DRIVE_MISSING_OR_INVALID | error | S6.5 |
| 4 | Bus width <= 0 | Error | PIN_BUS_WIDTH_INVALID | error | S6.5 |
| 5 | Duplicate pin in same block | Error | PIN_DUP_NAME_WITHIN_BLOCK | error | S6.5 |
| 6 | Invalid mode value | Error | PIN_MODE_INVALID | error | S6.5 |
| 7 | Mode conflicts with standard | Error | PIN_MODE_STANDARD_MISMATCH | error | S6.5 |
| 8 | Invalid pull value | Error | PIN_PULL_INVALID | error | S6.5 |
| 9 | Pull on output pin | Error | PIN_PULL_ON_OUTPUT | error | S6.5 |
| 10 | Invalid termination value | Error | PIN_TERM_INVALID | error | S6.5 |
| 11 | Termination on output pin | Error | PIN_TERM_ON_OUTPUT | error | S6.5 |
| 12 | Termination unsupported for standard | Error | PIN_TERM_INVALID_FOR_STANDARD | error | S6.5 |
| 13 | Diff output missing fclk | Error | PIN_DIFF_OUT_MISSING_FCLK | error | S6.5 |
| 14 | Diff output missing pclk | Error | PIN_DIFF_OUT_MISSING_PCLK | error | S6.5 |
| 15 | Diff output missing reset | Error | PIN_DIFF_OUT_MISSING_RESET | error | S6.5 |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 6_5_PIN_BUS_WIDTH_INVALID-bad_bus_width.jz | PIN_BUS_WIDTH_INVALID | Bus pin width non-integer or <= 0 |
| 6_5_PIN_DECLARED_MULTIPLE_BLOCKS-cross_block_duplicate.jz | PIN_DECLARED_MULTIPLE_BLOCKS | Same pin name in more than one PIN block |
| 6_5_PIN_DIFF_OUT_MISSING_FCLK-no_fclk.jz | PIN_DIFF_OUT_MISSING_FCLK | Differential output pin missing required fclk attribute |
| 6_5_PIN_DIFF_OUT_MISSING_PCLK-no_pclk.jz | PIN_DIFF_OUT_MISSING_PCLK | Differential output pin missing required pclk attribute |
| 6_5_PIN_DIFF_OUT_MISSING_RESET-no_reset.jz | PIN_DIFF_OUT_MISSING_RESET | Differential output pin missing required reset attribute |
| 6_5_PIN_DRIVE_MISSING_OR_INVALID-drive_problems.jz | PIN_DRIVE_MISSING_OR_INVALID | Missing or nonpositive drive value for OUT_PINS/INOUT_PINS |
| 6_5_PIN_DUP_NAME_WITHIN_BLOCK-duplicate_within_block.jz | PIN_DUP_NAME_WITHIN_BLOCK | Duplicate pin names within same PIN block |
| 6_5_PIN_INVALID_STANDARD-bad_standard.jz | PIN_INVALID_STANDARD | Invalid electrical standard in PIN declaration |
| 6_5_PIN_MODE_INVALID-bad_mode.jz | PIN_MODE_INVALID | Invalid pin mode value (must be SINGLE or DIFFERENTIAL) |
| 6_5_PIN_MODE_STANDARD_MISMATCH-mode_standard_conflict.jz | PIN_MODE_STANDARD_MISMATCH | Pin mode conflicts with I/O standard |
| 6_5_PIN_PULL_INVALID-bad_pull.jz | PIN_PULL_INVALID | Invalid pull value (must be UP, DOWN, or NONE) |
| 6_5_PIN_PULL_ON_OUTPUT-pull_on_out.jz | PIN_PULL_ON_OUTPUT | Pull resistor specified on output-only pin |
| 6_5_PIN_TERM_INVALID-bad_termination.jz | PIN_TERM_INVALID | Invalid termination value (must be ON or OFF) |
| 6_5_PIN_TERM_INVALID_FOR_STANDARD-term_unsupported.jz | PIN_TERM_INVALID_FOR_STANDARD | Termination not supported for this I/O standard |
| 6_5_PIN_TERM_ON_OUTPUT-term_on_out.jz | PIN_TERM_ON_OUTPUT | Termination specified on output-only pin |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| PIN_DECLARED_MULTIPLE_BLOCKS | error | S6.5/S6.9 Same pin name appears in more than one of IN_PINS/OUT_PINS/INOUT_PINS | 6_5_PIN_DECLARED_MULTIPLE_BLOCKS-cross_block_duplicate.jz |
| PIN_INVALID_STANDARD | error | S6.5/S6.9 Invalid electrical standard in PIN declaration | 6_5_PIN_INVALID_STANDARD-bad_standard.jz |
| PIN_DRIVE_MISSING_OR_INVALID | error | S6.5/S6.9 Missing or nonpositive drive value for OUT_PINS/INOUT_PINS | 6_5_PIN_DRIVE_MISSING_OR_INVALID-drive_problems.jz |
| PIN_BUS_WIDTH_INVALID | error | S6.5/S6.9 Bus pin width non-integer or <= 0 | 6_5_PIN_BUS_WIDTH_INVALID-bad_bus_width.jz |
| PIN_DUP_NAME_WITHIN_BLOCK | error | S6.5/S6.9 Duplicate pin names within same PIN block | 6_5_PIN_DUP_NAME_WITHIN_BLOCK-duplicate_within_block.jz |
| PIN_MODE_INVALID | error | S6.5/S6.9 Invalid pin mode value (must be SINGLE or DIFFERENTIAL) | 6_5_PIN_MODE_INVALID-bad_mode.jz |
| PIN_MODE_STANDARD_MISMATCH | error | S6.5/S6.9 Pin mode conflicts with I/O standard | 6_5_PIN_MODE_STANDARD_MISMATCH-mode_standard_conflict.jz |
| PIN_PULL_INVALID | error | S6.5/S6.9 Invalid pull value (must be UP, DOWN, or NONE) | 6_5_PIN_PULL_INVALID-bad_pull.jz |
| PIN_PULL_ON_OUTPUT | error | S6.5/S6.9 Pull resistor specified on output-only pin (OUT_PINS) | 6_5_PIN_PULL_ON_OUTPUT-pull_on_out.jz |
| PIN_TERM_INVALID | error | S6.5/S6.9 Invalid termination value (must be ON or OFF) | 6_5_PIN_TERM_INVALID-bad_termination.jz |
| PIN_TERM_ON_OUTPUT | error | S6.5/S6.9 Termination specified on output-only pin (OUT_PINS) | 6_5_PIN_TERM_ON_OUTPUT-term_on_out.jz |
| PIN_TERM_INVALID_FOR_STANDARD | error | S6.5/S6.9 Termination not supported for this I/O standard | 6_5_PIN_TERM_INVALID_FOR_STANDARD-term_unsupported.jz |
| PIN_DIFF_OUT_MISSING_FCLK | error | S6.5/S6.9 Differential output pin missing required fclk attribute | 6_5_PIN_DIFF_OUT_MISSING_FCLK-no_fclk.jz |
| PIN_DIFF_OUT_MISSING_PCLK | error | S6.5/S6.9 Differential output pin missing required pclk attribute | 6_5_PIN_DIFF_OUT_MISSING_PCLK-no_pclk.jz |
| PIN_DIFF_OUT_MISSING_RESET | error | S6.5/S6.9 Differential output pin missing required reset attribute | 6_5_PIN_DIFF_OUT_MISSING_RESET-no_reset.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| -- | -- | All rules for this section are covered by existing tests |
