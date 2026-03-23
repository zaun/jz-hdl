# Test Plan: 6.4 CLOCKS Block

**Specification Reference:** Section 6.4 of jz-hdl-specification.md

## 1. Objective

Verify CLOCKS block (period, edge properties), clock source validation (must be IN_PIN or CLOCK_GEN output, not both), CLOCK_GEN types (PLL, DLL, CLKDIV, OSC, BUF), CONFIG parameter validation against chip data, and clock uniqueness.

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
| 9 | CLOCK_GEN missing input | No IN clock declaration in CLOCK_GEN |
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
| 26 | Clock name not in pins | Clock name in CLOCKS has no matching IN_PINS declaration |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Very fast clock | period=0.5 (2 GHz) |
| 2 | Very slow clock | period=1000000 |
| 3 | CLOCK_GEN with optional CONFIG params omitted | Defaults used for unspecified params |
| 4 | No chip data available | CLOCK_GEN parameters cannot be validated (info) |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|-----------------|---------|----------|-------|
| 1 | Duplicate clock name | Error | CLOCK_DUPLICATE_NAME | error | S6.4/S6.10 |
| 2 | CLOCK_GEN input is own output | Error | CLOCK_GEN_INPUT_IS_SELF_OUTPUT | error | S6.4.1 |
| 3 | CLOCK_GEN output with period | Error | CLOCK_GEN_OUTPUT_HAS_PERIOD | error | S6.4.1 |
| 4 | Period <= 0 | Error | CLOCK_PERIOD_NONPOSITIVE | error | S6.4 |
| 5 | Clock in IN_PINS and CLOCK_GEN | Error | CLOCK_SOURCE_AMBIGUOUS | error | S6.4 |
| 6 | Invalid edge value | Error | CLOCK_EDGE_INVALID | error | S6.4 |
| 7 | External clock no period | Error | CLOCK_EXTERNAL_NO_PERIOD | error | S6.4 |
| 8 | Invalid generator type | Error | CLOCK_GEN_INVALID_TYPE | error | S6.4.1 |
| 9 | Missing IN declaration | Error | CLOCK_GEN_MISSING_INPUT | error | S6.4.1 |
| 10 | Missing OUT declaration | Error | CLOCK_GEN_MISSING_OUTPUT | error | S6.4.1 |
| 11 | Input clock not declared | Error | CLOCK_GEN_INPUT_NOT_DECLARED | error | S6.4.1 |
| 12 | Input clock no period | Error | CLOCK_GEN_INPUT_NO_PERIOD | error | S6.4.1 |
| 13 | Output clock not declared | Error | CLOCK_GEN_OUTPUT_NOT_DECLARED | error | S6.4.1 |
| 14 | Output is input pin | Error | CLOCK_GEN_OUTPUT_IS_INPUT_PIN | error | S6.4.1 |
| 15 | Multiple drivers | Error | CLOCK_GEN_MULTIPLE_DRIVERS | error | S6.4.1 |
| 16 | Param out of range | Error | CLOCK_GEN_PARAM_OUT_OF_RANGE | error | S6.4.1 |
| 17 | Param type mismatch | Error | CLOCK_GEN_PARAM_TYPE_MISMATCH | error | S6.4.1 |
| 18 | Derived out of range | Error | CLOCK_GEN_DERIVED_OUT_OF_RANGE | error | S6.4.1 |
| 19 | Input freq out of range | Error | CLOCK_GEN_INPUT_FREQ_OUT_OF_RANGE | error | S6.4.1 |
| 20 | Invalid output selector | Error | CLOCK_GEN_OUTPUT_INVALID_SELECTOR | error | S6.4.1 |
| 21 | OUT for non-clock output | Error | CLOCK_GEN_OUT_NOT_CLOCK | error | S6.4.1 |
| 22 | WIRE for clock output | Error | CLOCK_GEN_WIRE_IS_CLOCK | error | S6.4.1 |
| 23 | WIRE output in CLOCKS | Error | CLOCK_GEN_WIRE_IN_CLOCKS | error | S6.4.1 |
| 24 | Required input missing | Error | CLOCK_GEN_REQUIRED_INPUT_MISSING | error | S6.4.1 |
| 25 | Clock pin width not 1 | Error | CLOCK_PORT_WIDTH_NOT_1 | error | S6.4/S6.5 |
| 26 | Clock name not in pins | Error | CLOCK_NAME_NOT_IN_PINS | error | S6.4/S6.5 |
| 27 | No chip data | Info | CLOCK_GEN_NO_CHIP_DATA | info | S6.4.1 |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 6_4_CLOCK_DUPLICATE_NAME-duplicate_clocks.jz | CLOCK_DUPLICATE_NAME | Duplicate clock names in CLOCKS block |
| 6_4_CLOCK_GEN_INPUT_IS_SELF_OUTPUT-self_reference.jz | CLOCK_GEN_INPUT_IS_SELF_OUTPUT | CLOCK_GEN input clock is an output of the same block |
| 6_4_CLOCK_GEN_OUTPUT_HAS_PERIOD-gen_out_with_period.jz | CLOCK_GEN_OUTPUT_HAS_PERIOD | CLOCK_GEN output clock must not have a period in CLOCKS block |
| 6_4_CLOCK_PERIOD_NONPOSITIVE-invalid_period.jz | CLOCK_PERIOD_NONPOSITIVE | Clock period <= 0 |
| 6_4_CLOCK_SOURCE_AMBIGUOUS-dual_source.jz | CLOCK_SOURCE_AMBIGUOUS | Clock declared as both IN_PINS and CLOCK_GEN output |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| CLOCK_DUPLICATE_NAME | error | S6.4/S6.10/S6.9 Duplicate clock names in CLOCKS block | 6_4_CLOCK_DUPLICATE_NAME-duplicate_clocks.jz |
| CLOCK_PERIOD_NONPOSITIVE | error | S6.4/S6.9 Clock period <= 0 | 6_4_CLOCK_PERIOD_NONPOSITIVE-invalid_period.jz |
| CLOCK_SOURCE_AMBIGUOUS | error | S6.4 Clock declared as both IN_PINS and CLOCK_GEN output | 6_4_CLOCK_SOURCE_AMBIGUOUS-dual_source.jz |
| CLOCK_GEN_OUTPUT_HAS_PERIOD | error | S6.4.1 CLOCK_GEN output clock must not have a period in CLOCKS block | 6_4_CLOCK_GEN_OUTPUT_HAS_PERIOD-gen_out_with_period.jz |
| CLOCK_GEN_INPUT_IS_SELF_OUTPUT | error | S6.4.1 CLOCK_GEN input clock is an output of the same CLOCK_GEN block | 6_4_CLOCK_GEN_INPUT_IS_SELF_OUTPUT-self_reference.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| CLOCK_PORT_WIDTH_NOT_1 | error | No dedicated validation test file exists |
| CLOCK_NAME_NOT_IN_PINS | error | No dedicated validation test file exists |
| CLOCK_EDGE_INVALID | error | No dedicated validation test file exists |
| CLOCK_EXTERNAL_NO_PERIOD | error | No dedicated validation test file exists |
| CLOCK_GEN_INPUT_NOT_DECLARED | error | No dedicated validation test file exists |
| CLOCK_GEN_INPUT_NO_PERIOD | error | No dedicated validation test file exists |
| CLOCK_GEN_INPUT_FREQ_OUT_OF_RANGE | error | No dedicated validation test file exists |
| CLOCK_GEN_OUTPUT_INVALID_SELECTOR | error | No dedicated validation test file exists |
| CLOCK_GEN_OUT_NOT_CLOCK | error | No dedicated validation test file exists |
| CLOCK_GEN_WIRE_IS_CLOCK | error | No dedicated validation test file exists |
| CLOCK_GEN_WIRE_IN_CLOCKS | error | No dedicated validation test file exists |
| CLOCK_GEN_OUTPUT_NOT_DECLARED | error | No dedicated validation test file exists |
| CLOCK_GEN_OUTPUT_IS_INPUT_PIN | error | No dedicated validation test file exists |
| CLOCK_GEN_MULTIPLE_DRIVERS | error | No dedicated validation test file exists |
| CLOCK_GEN_INVALID_TYPE | error | No dedicated validation test file exists |
| CLOCK_GEN_MISSING_INPUT | error | No dedicated validation test file exists |
| CLOCK_GEN_REQUIRED_INPUT_MISSING | error | No dedicated validation test file exists |
| CLOCK_GEN_MISSING_OUTPUT | error | No dedicated validation test file exists |
| CLOCK_GEN_PARAM_OUT_OF_RANGE | error | No dedicated validation test file exists |
| CLOCK_GEN_PARAM_TYPE_MISMATCH | error | No dedicated validation test file exists |
| CLOCK_GEN_DERIVED_OUT_OF_RANGE | error | No dedicated validation test file exists |
| CLOCK_GEN_NO_CHIP_DATA | info | No dedicated validation test file exists |
