# Test Plan: 6.4 CLOCKS Block

**Specification Reference:** Section 6.4 of jz-hdl-specification.md

## 1. Objective

Verify CLOCKS block (period, edge properties), clock source validation (must be IN_PIN or CLOCK_GEN output, not both), CLOCK_GEN types (PLL, DLL, CLKDIV, OSC, BUF), CONFIG parameter validation against chip data, clock uniqueness, and all CLOCK_GEN input/output/wire rules.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | IN_PIN clock with period | `sys_clk = { period=10 };` -- standard external clock |
| 2 | CLOCK_GEN PLL with multiple outputs | PLL generating several derived clocks |
| 3 | CLOCK_GEN CLKDIV | Clock divider from input clock |
| 4 | CLOCK_GEN OSC | On-chip oscillator, no input required |
| 5 | CLOCK_GEN BUF | Clock buffer pass-through |
| 6 | Falling edge clock | Clock with edge=Falling |
| 7 | Minimal clock declaration | `clk;` -- clock with no optional config |
| 8 | Indexed clock pins | `clk[0] = { period=10 }; clk[1] = { period=100 };` |

### 2.2 Error Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Duplicate clock name | Two clocks with same name in CLOCKS block |
| 2 | CLOCK_GEN input is self output | CLOCK_GEN input clock is an output of the same block |
| 3 | CLOCK_GEN output has period | CLOCK_GEN output clock declares period in CLOCKS block |
| 4 | Non-positive period | Clock period <= 0 |
| 5 | Ambiguous source | Clock declared as both IN_PINS and CLOCK_GEN output |
| 6 | Invalid edge specifier | Edge not Rising or Falling |
| 7 | External clock without period | IN_PINS clock missing period in CLOCKS |
| 8 | Invalid CLOCK_GEN type | Generator type not PLL/DLL/CLKDIV/OSC/BUF |
| 9 | CLOCK_GEN missing input | No IN clock declaration in CLOCK_GEN (non-OSC) |
| 10 | CLOCK_GEN missing output | No OUT clock declaration in CLOCK_GEN |
| 11 | CLOCK_GEN input not declared | Input clock not in CLOCKS block |
| 12 | CLOCK_GEN input no period | Input clock has no period declared |
| 13 | CLOCK_GEN output not declared | Output clock not in CLOCKS block |
| 14 | CLOCK_GEN output is input pin | Output clock declared as IN_PINS |
| 15 | CLOCK_GEN multiple drivers | Clock driven by multiple CLOCK_GEN outputs |
| 16 | CLOCK_GEN param out of range | CONFIG parameter outside chip-defined range |
| 17 | CLOCK_GEN param type mismatch | Integer parameter given decimal value |
| 18 | CLOCK_GEN derived out of range | Derived frequency/VCO outside valid range |
| 19 | CLOCK_GEN input freq out of range | Input frequency outside chip's supported range |
| 20 | CLOCK_GEN output invalid selector | Output selector not valid for generator type |
| 21 | CLOCK_GEN OUT used for non-clock | OUT used for non-clock output; should use WIRE |
| 22 | CLOCK_GEN WIRE used for clock | WIRE used for clock output; should use OUT |
| 23 | CLOCK_GEN WIRE in CLOCKS | WIRE output declared in CLOCKS block |
| 24 | CLOCK_GEN required input missing | Required input parameter not provided |
| 25 | Clock port width not 1 | Clock pin width is not [1] |
| 26 | Clock name not in pins | Clock name in CLOCKS has no matching IN_PINS or CLOCK_GEN output |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Very fast clock | period=0.5 (2 GHz) |
| 2 | Very slow clock | period=1000000 |
| 3 | CLOCK_GEN with optional CONFIG params omitted | Defaults used for unspecified params |
| 4 | No chip data available | CLOCK_GEN parameters cannot be validated (info) |
| 5 | Numbered generator type | CLKDIV2 or BUF2 with chip support |

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|------------------|----------|
| 1 | Duplicate clock name | Two entries with same name in CLOCKS | CLOCK_DUPLICATE_NAME | error |
| 2 | CLOCK_GEN input is own output | IN REF_CLK fast_clk with OUT BASE fast_clk | CLOCK_GEN_INPUT_IS_SELF_OUTPUT | error |
| 3 | CLOCK_GEN output with period | `fast_clk = { period=10 };` for GEN output | CLOCK_GEN_OUTPUT_HAS_PERIOD | error |
| 4 | Period <= 0 | `clk = { period=0 };` | CLOCK_PERIOD_NONPOSITIVE | error |
| 5 | Clock in IN_PINS and CLOCK_GEN | Same name in IN_PINS and CLOCK_GEN OUT | CLOCK_SOURCE_AMBIGUOUS | error |
| 6 | Invalid edge value | `clk = { edge=Both };` | CLOCK_EDGE_INVALID | error |
| 7 | External clock no period | IN_PINS clock with no period in CLOCKS | CLOCK_EXTERNAL_NO_PERIOD | error |
| 8 | Invalid generator type | `INVALID { ... }` in CLOCK_GEN | CLOCK_GEN_INVALID_TYPE | error |
| 9 | Missing IN declaration | PLL with no IN line | CLOCK_GEN_MISSING_INPUT | error |
| 10 | Missing OUT declaration | PLL with no OUT line | CLOCK_GEN_MISSING_OUTPUT | error |
| 11 | Input clock not declared | `IN REF_CLK undeclared_clk` | CLOCK_GEN_INPUT_NOT_DECLARED | error |
| 12 | Input clock no period | Input clock declared but no period | CLOCK_GEN_INPUT_NO_PERIOD | error |
| 13 | Output clock not declared | `OUT BASE undeclared_clk` | CLOCK_GEN_OUTPUT_NOT_DECLARED | error |
| 14 | Output is input pin | OUT clock that is also in IN_PINS | CLOCK_GEN_OUTPUT_IS_INPUT_PIN | error |
| 15 | Multiple drivers | Same clock driven by two GEN outputs | CLOCK_GEN_MULTIPLE_DRIVERS | error |
| 16 | Param out of range | `IDIV = 999;` exceeds chip range | CLOCK_GEN_PARAM_OUT_OF_RANGE | error |
| 17 | Param type mismatch | Integer param given string value | CLOCK_GEN_PARAM_TYPE_MISMATCH | error |
| 18 | Derived out of range | PLL parameters produce out-of-range VCO | CLOCK_GEN_DERIVED_OUT_OF_RANGE | error |
| 19 | Input freq out of range | Input clock frequency outside chip limits | CLOCK_GEN_INPUT_FREQ_OUT_OF_RANGE | error |
| 20 | Invalid output selector | `OUT INVALID fast_clk` | CLOCK_GEN_OUTPUT_INVALID_SELECTOR | error |
| 21 | OUT for non-clock output | `OUT LOCK lock_sig` (LOCK is not a clock) | CLOCK_GEN_OUT_NOT_CLOCK | error |
| 22 | WIRE for clock output | `WIRE BASE fast_clk` (BASE is a clock) | CLOCK_GEN_WIRE_IS_CLOCK | error |
| 23 | WIRE output in CLOCKS | WIRE output name appears in CLOCKS block | CLOCK_GEN_WIRE_IN_CLOCKS | error |
| 24 | Required input missing | Omitting a required IN for PLL | CLOCK_GEN_REQUIRED_INPUT_MISSING | error |
| 25 | Clock pin width not 1 | `clk[4] = { standard=LVCMOS33 }` used as clock | CLOCK_PORT_WIDTH_NOT_1 | error |
| 26 | Clock name not in pins | CLOCKS name with no IN_PINS match and no GEN | CLOCK_NAME_NOT_IN_PINS | error |
| 27 | No chip data | CLOCK_GEN with CHIP=GENERIC (no chip data) | CLOCK_GEN_NO_CHIP_DATA | info |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 6_4_HAPPY_PATH-clocks_ok.jz | -- | Valid CLOCKS block with IN_PIN and CLOCK_GEN clocks (clean compile) |
| 6_4_CLOCK_DUPLICATE_NAME-duplicate_clocks.jz | CLOCK_DUPLICATE_NAME | Duplicate clock names in CLOCKS block |
| 6_4_CLOCK_EDGE_INVALID-invalid_edge.jz | CLOCK_EDGE_INVALID | Invalid edge specifier (not Rising or Falling) |
| 6_4_CLOCK_EXTERNAL_NO_PERIOD-no_period.jz | CLOCK_EXTERNAL_NO_PERIOD | External (IN_PINS) clock missing period |
| 6_4_CLOCK_GEN_DERIVED_OUT_OF_RANGE-derived_range.jz | CLOCK_GEN_DERIVED_OUT_OF_RANGE | Derived frequency/VCO outside valid chip range |
| 6_4_CLOCK_GEN_INPUT_FREQ_OUT_OF_RANGE-freq_range.jz | CLOCK_GEN_INPUT_FREQ_OUT_OF_RANGE | Input frequency outside chip-supported range |
| 6_4_CLOCK_GEN_INPUT_IS_SELF_OUTPUT-self_reference.jz | CLOCK_GEN_INPUT_IS_SELF_OUTPUT | CLOCK_GEN input clock is an output of the same block |
| 6_4_CLOCK_GEN_INPUT_NOT_DECLARED-undeclared_input.jz | CLOCK_GEN_INPUT_NOT_DECLARED | CLOCK_GEN input clock not declared in CLOCKS block |
| 6_4_CLOCK_GEN_INPUT_NO_PERIOD-no_input_period.jz | CLOCK_GEN_INPUT_NO_PERIOD | CLOCK_GEN input clock has no period declared |
| 6_4_CLOCK_GEN_INVALID_TYPE-bad_type.jz | CLOCK_GEN_INVALID_TYPE | Generator type not PLL/DLL/CLKDIV/OSC/BUF |
| 6_4_CLOCK_GEN_MISSING_INPUT-no_input.jz | CLOCK_GEN_MISSING_INPUT | No IN clock declaration in CLOCK_GEN block |
| 6_4_CLOCK_GEN_MISSING_OUTPUT-no_output.jz | CLOCK_GEN_MISSING_OUTPUT | No OUT clock declaration in CLOCK_GEN block |
| 6_4_CLOCK_GEN_MULTIPLE_DRIVERS-multi_driver.jz | CLOCK_GEN_MULTIPLE_DRIVERS | Clock driven by multiple CLOCK_GEN outputs |
| 6_4_CLOCK_GEN_NO_CHIP_DATA-no_chip.jz | CLOCK_GEN_NO_CHIP_DATA | CLOCK_GEN parameters cannot be validated (no chip data) |
| 6_4_CLOCK_GEN_OUTPUT_HAS_PERIOD-gen_out_with_period.jz | CLOCK_GEN_OUTPUT_HAS_PERIOD | CLOCK_GEN output clock must not have a period in CLOCKS block |
| 6_4_CLOCK_GEN_OUTPUT_INVALID_SELECTOR-bad_selector.jz | CLOCK_GEN_OUTPUT_INVALID_SELECTOR | Output selector not valid for generator type |
| 6_4_CLOCK_GEN_OUTPUT_IS_INPUT_PIN-out_is_inpin.jz | CLOCK_GEN_OUTPUT_IS_INPUT_PIN | CLOCK_GEN output clock declared as IN_PINS |
| 6_4_CLOCK_GEN_OUTPUT_NOT_DECLARED-undeclared_output.jz | CLOCK_GEN_OUTPUT_NOT_DECLARED | CLOCK_GEN output clock not declared in CLOCKS block |
| 6_4_CLOCK_GEN_OUT_NOT_CLOCK-out_for_nonclock.jz | CLOCK_GEN_OUT_NOT_CLOCK | OUT used for non-clock output; should use WIRE |
| 6_4_CLOCK_GEN_PARAM_OUT_OF_RANGE-param_range.jz | CLOCK_GEN_PARAM_OUT_OF_RANGE | CONFIG parameter outside chip-defined range |
| 6_4_CLOCK_GEN_PARAM_TYPE_MISMATCH-type_mismatch.jz | CLOCK_GEN_PARAM_TYPE_MISMATCH | Integer parameter given decimal or wrong-type value |
| 6_4_CLOCK_GEN_REQUIRED_INPUT_MISSING-missing_required.jz | CLOCK_GEN_REQUIRED_INPUT_MISSING | Required input parameter not provided |
| 6_4_CLOCK_GEN_WIRE_IN_CLOCKS-wire_in_clocks.jz | CLOCK_GEN_WIRE_IN_CLOCKS | CLOCK_GEN WIRE output declared in CLOCKS block |
| 6_4_CLOCK_GEN_WIRE_IS_CLOCK-wire_for_clock.jz | CLOCK_GEN_WIRE_IS_CLOCK | WIRE used for clock output; should use OUT |
| 6_4_CLOCK_NAME_NOT_IN_PINS-missing_pin.jz | CLOCK_NAME_NOT_IN_PINS | Clock name in CLOCKS has no matching IN_PINS declaration |
| 6_4_CLOCK_PERIOD_NONPOSITIVE-invalid_period.jz | CLOCK_PERIOD_NONPOSITIVE | Clock period <= 0 |
| 6_4_CLOCK_PORT_WIDTH_NOT_1-wide_clock.jz | CLOCK_PORT_WIDTH_NOT_1 | Clock pin width is not [1] |
| 6_4_CLOCK_SOURCE_AMBIGUOUS-dual_source.jz | CLOCK_SOURCE_AMBIGUOUS | Clock declared as both IN_PINS and CLOCK_GEN output |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| CLOCK_DUPLICATE_NAME | error | S6.4/S6.10/S6.9 Duplicate clock names in CLOCKS block | 6_4_CLOCK_DUPLICATE_NAME-duplicate_clocks.jz |
| CLOCK_EDGE_INVALID | error | S6.4/S6.9 Invalid edge specifier (not Rising or Falling) | 6_4_CLOCK_EDGE_INVALID-invalid_edge.jz |
| CLOCK_EXTERNAL_NO_PERIOD | error | S6.4 External (IN_PINS) clock missing period in CLOCKS block | 6_4_CLOCK_EXTERNAL_NO_PERIOD-no_period.jz |
| CLOCK_NAME_NOT_IN_PINS | error | S6.4/S6.5/S6.9 Clock name in CLOCKS has no matching IN_PINS declaration | 6_4_CLOCK_NAME_NOT_IN_PINS-missing_pin.jz |
| CLOCK_PERIOD_NONPOSITIVE | error | S6.4/S6.9 Clock period <= 0 | 6_4_CLOCK_PERIOD_NONPOSITIVE-invalid_period.jz |
| CLOCK_PORT_WIDTH_NOT_1 | error | S6.4/S6.5/S6.9 Clock pin width is not [1] | 6_4_CLOCK_PORT_WIDTH_NOT_1-wide_clock.jz |
| CLOCK_SOURCE_AMBIGUOUS | error | S6.4 Clock declared as both IN_PINS and CLOCK_GEN output | 6_4_CLOCK_SOURCE_AMBIGUOUS-dual_source.jz |
| CLOCK_GEN_DERIVED_OUT_OF_RANGE | error | S6.4.1 Derived frequency or VCO outside valid chip range | 6_4_CLOCK_GEN_DERIVED_OUT_OF_RANGE-derived_range.jz |
| CLOCK_GEN_INPUT_FREQ_OUT_OF_RANGE | error | S6.4.1 Input frequency outside chip-supported range | 6_4_CLOCK_GEN_INPUT_FREQ_OUT_OF_RANGE-freq_range.jz |
| CLOCK_GEN_INPUT_IS_SELF_OUTPUT | error | S6.4.1 CLOCK_GEN input clock is an output of the same CLOCK_GEN block | 6_4_CLOCK_GEN_INPUT_IS_SELF_OUTPUT-self_reference.jz |
| CLOCK_GEN_INPUT_NOT_DECLARED | error | S6.4.1 CLOCK_GEN input clock not declared in CLOCKS block | 6_4_CLOCK_GEN_INPUT_NOT_DECLARED-undeclared_input.jz |
| CLOCK_GEN_INPUT_NO_PERIOD | error | S6.4.1 CLOCK_GEN input clock has no period declared | 6_4_CLOCK_GEN_INPUT_NO_PERIOD-no_input_period.jz |
| CLOCK_GEN_INVALID_TYPE | error | S6.4.1 Generator type not PLL/DLL/CLKDIV/OSC/BUF | 6_4_CLOCK_GEN_INVALID_TYPE-bad_type.jz |
| CLOCK_GEN_MISSING_INPUT | error | S6.4.1 No IN clock declaration in CLOCK_GEN block | 6_4_CLOCK_GEN_MISSING_INPUT-no_input.jz |
| CLOCK_GEN_MISSING_OUTPUT | error | S6.4.1 No OUT clock declaration in CLOCK_GEN block | 6_4_CLOCK_GEN_MISSING_OUTPUT-no_output.jz |
| CLOCK_GEN_MULTIPLE_DRIVERS | error | S6.4.1 Clock driven by multiple CLOCK_GEN outputs | 6_4_CLOCK_GEN_MULTIPLE_DRIVERS-multi_driver.jz |
| CLOCK_GEN_NO_CHIP_DATA | info | S6.4.1 CLOCK_GEN parameters cannot be validated (no chip data available) | 6_4_CLOCK_GEN_NO_CHIP_DATA-no_chip.jz |
| CLOCK_GEN_OUTPUT_HAS_PERIOD | error | S6.4.1 CLOCK_GEN output clock must not have a period in CLOCKS block | 6_4_CLOCK_GEN_OUTPUT_HAS_PERIOD-gen_out_with_period.jz |
| CLOCK_GEN_OUTPUT_INVALID_SELECTOR | error | S6.4.1 Output selector not valid for generator type | 6_4_CLOCK_GEN_OUTPUT_INVALID_SELECTOR-bad_selector.jz |
| CLOCK_GEN_OUTPUT_IS_INPUT_PIN | error | S6.4.1 CLOCK_GEN output clock declared as IN_PINS | 6_4_CLOCK_GEN_OUTPUT_IS_INPUT_PIN-out_is_inpin.jz |
| CLOCK_GEN_OUTPUT_NOT_DECLARED | error | S6.4.1 CLOCK_GEN output clock not declared in CLOCKS block | 6_4_CLOCK_GEN_OUTPUT_NOT_DECLARED-undeclared_output.jz |
| CLOCK_GEN_OUT_NOT_CLOCK | error | S6.4.1 OUT used for non-clock output; should use WIRE | 6_4_CLOCK_GEN_OUT_NOT_CLOCK-out_for_nonclock.jz |
| CLOCK_GEN_PARAM_OUT_OF_RANGE | error | S6.4.1 CONFIG parameter outside chip-defined range | 6_4_CLOCK_GEN_PARAM_OUT_OF_RANGE-param_range.jz |
| CLOCK_GEN_PARAM_TYPE_MISMATCH | error | S6.4.1 Integer parameter given decimal or wrong-type value | 6_4_CLOCK_GEN_PARAM_TYPE_MISMATCH-type_mismatch.jz |
| CLOCK_GEN_REQUIRED_INPUT_MISSING | error | S6.4.1 Required input parameter not provided | 6_4_CLOCK_GEN_REQUIRED_INPUT_MISSING-missing_required.jz |
| CLOCK_GEN_WIRE_IN_CLOCKS | error | S6.4.1 CLOCK_GEN WIRE output declared in CLOCKS block | 6_4_CLOCK_GEN_WIRE_IN_CLOCKS-wire_in_clocks.jz |
| CLOCK_GEN_WIRE_IS_CLOCK | error | S6.4.1 WIRE used for clock output; should use OUT | 6_4_CLOCK_GEN_WIRE_IS_CLOCK-wire_for_clock.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| -- | -- | All rules for this section are covered by existing tests |
