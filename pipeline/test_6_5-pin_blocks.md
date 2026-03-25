# Test Plan: 6.5 PIN Blocks

**Specification Reference:** Section 6.5 of jz-hdl-specification.md

## 1. Objective

Verify PIN blocks (IN_PINS, OUT_PINS, INOUT_PINS): electrical standards, drive strength, bus syntax, pin name uniqueness across all categories, differential signaling modes, pull resistors, termination, serialization attributes (fclk, pclk, reset), and chip-specific standard validation.

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
| 7 | Pin with termination | `data = { standard=SSTL18_I, term=ON };` -- input with termination |
| 8 | Differential output with serializer | Output pin with fclk, pclk, reset for LVDS serialization |

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
| 4 | INOUT pin with pull | Bidirectional pin with pull-up or pull-down |

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|------------------|----------|
| 1 | Pin in IN and OUT blocks | Same name in IN_PINS and OUT_PINS | PIN_DECLARED_MULTIPLE_BLOCKS | error |
| 2 | Invalid standard | `standard=INVALID` | PIN_INVALID_STANDARD | error |
| 3 | OUT without drive or bad drive | `OUT_PINS { led = { standard=LVCMOS33 }; }` | PIN_DRIVE_MISSING_OR_INVALID | error |
| 4 | Bus width <= 0 | `pin[0] = { standard=LVCMOS33 };` | PIN_BUS_WIDTH_INVALID | error |
| 5 | Duplicate pin in same block | Two `led` entries in OUT_PINS | PIN_DUP_NAME_WITHIN_BLOCK | error |
| 6 | Invalid mode value | `mode=TRISTATE` | PIN_MODE_INVALID | error |
| 7 | Mode conflicts with standard | `mode=DIFFERENTIAL, standard=LVCMOS33` | PIN_MODE_STANDARD_MISMATCH | error |
| 8 | Invalid pull value | `pull=KEEPER` | PIN_PULL_INVALID | error |
| 9 | Pull on output pin | `OUT_PINS { led = { ..., pull=UP }; }` | PIN_PULL_ON_OUTPUT | error |
| 10 | Invalid termination value | `term=MAYBE` | PIN_TERM_INVALID | error |
| 11 | Termination on output pin | `OUT_PINS { led = { ..., term=ON }; }` | PIN_TERM_ON_OUTPUT | error |
| 12 | Termination unsupported for standard | `term=ON` with LVCMOS33 | PIN_TERM_INVALID_FOR_STANDARD | error |
| 13 | Diff output missing fclk | Differential OUT missing fclk attribute | PIN_DIFF_OUT_MISSING_FCLK | error |
| 14 | Diff output missing pclk | Differential OUT missing pclk attribute | PIN_DIFF_OUT_MISSING_PCLK | error |
| 15 | Diff output missing reset | Differential OUT missing reset attribute | PIN_DIFF_OUT_MISSING_RESET | error |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 6_5_HAPPY_PATH-pin_blocks_ok.jz | -- | Valid IN_PINS/OUT_PINS/INOUT_PINS declarations (clean compile) |
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
| INFO_SERIALIZER_CASCADE | info | Serializer cascade is a chip-specific optimization note; no dedicated validation test file exists |
| SERIALIZER_WIDTH_EXCEEDS_RATIO | error | Serializer width exceeds ratio is chip-specific; no dedicated validation test file exists |
