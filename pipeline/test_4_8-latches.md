# Test Plan: 4.8 Latches

**Specification Reference:** Section 4.8 of jz-hdl-specification.md

## 1. Objective

Verify LATCH declaration (D-type and SR-type), guarded assignment syntax (enable:data for D, set:reset for SR), ASYNCHRONOUS-only placement, read semantics (passive, unconditional), power-up indeterminate state, restrictions (no alias, no CDC, no compile-time constant context), and exclusive assignment rule compliance.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | D-latch declaration | `LATCH { hold [8] D; }` |
| 2 | SR-latch declaration | `LATCH { state [1] SR; }` |
| 3 | D-latch guarded assign | `hold <= enable : data;` in ASYNC |
| 4 | SR-latch assign | `state <= set_sig : reset_sig;` in ASYNC |
| 5 | Latch read in ASYNC | `wire <= latch_val;` |
| 6 | Latch read in SYNC | `reg <= latch_val;` in SYNCHRONOUS |
| 7 | Latch in conditional | `IF (latch_valid) { ... }` -- valid read |
| 8 | No assignment = hold | Execution path with no latch assignment holds value |

### 2.2 Error Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Latch in SYNCHRONOUS | `latch <= en : data;` in SYNC -- Error |
| 2 | Latch aliased | `latch = wire;` -- Error |
| 3 | Latch as clock source | Using latch output as CLK -- Error |
| 4 | Latch as CDC source | Latch in CDC block -- Error |
| 5 | D-latch enable not 1-bit | `hold <= 8'd1 : data;` -- Error: enable must be width-1 |
| 6 | SR widths mismatch | Set and reset different widths from latch -- Error |
| 7 | Latch in compile-time context | Using latch value as CONST -- Error |
| 8 | Multiple assignments same path | Two guarded assigns to same latch bits -- Error |
| 9 | Invalid latch type | Type keyword other than D or SR -- Error |
| 10 | Invalid latch width | Width value that is not valid -- Error |
| 11 | Unguarded latch write | Assignment without enable:data guard -- Error |
| 12 | Latch chip unsupported | Target chip does not support latches -- Error |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | 1-bit D-latch | Minimum width |
| 2 | Wide SR-latch | `LATCH { bus [32] SR; }` |
| 3 | D-latch enable always 1 | Transparent continuously (valid but unusual) |
| 4 | SR both 0 | Hold state (S=0, R=0) |

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1 | Latch write in SYNC block | `latch <= en : data;` in SYNCHRONOUS | LATCH_ASSIGN_IN_SYNC | error |
| 2 | Latch aliased | `latch = wire;` | LATCH_ALIAS_FORBIDDEN | error |
| 3 | Latch as clock or CDC source | Latch output used as CLK or in CDC | LATCH_AS_CLOCK_OR_CDC | error |
| 4 | D-latch enable not 1-bit | `hold <= 8'd1 : data;` | LATCH_ENABLE_WIDTH_NOT_1 | error |
| 5 | SR width mismatch | Set/reset width differs from latch | LATCH_SR_WIDTH_MISMATCH | error |
| 6 | Invalid latch type | `LATCH { x [8] INVALID; }` | LATCH_INVALID_TYPE | error |
| 7 | Invalid latch width | Non-positive or non-constant width | LATCH_WIDTH_INVALID | error |
| 8 | Unguarded latch write | `latch <= data;` without guard | LATCH_ASSIGN_NON_GUARDED | error |
| 9 | Latch in const context | Latch value in compile-time expression | LATCH_IN_CONST_CONTEXT | error |
| 10 | Target chip unsupported | Chip has no latch primitives | LATCH_CHIP_UNSUPPORTED | error |
| 11 | Valid D-latch | Properly guarded D-latch in ASYNC | -- | -- (pass) |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 4_8_LATCH_HAPPY_PATH-latch_ok.jz | -- | Happy path: valid D-latch and SR-latch usage |
| 4_8_LATCH_ALIAS_FORBIDDEN-latch_aliased.jz | LATCH_ALIAS_FORBIDDEN | Latch may not be aliased |
| 4_8_LATCH_AS_CLOCK_OR_CDC-latch_as_clock.jz | LATCH_AS_CLOCK_OR_CDC | Latch output may not be used as clock or CDC source |
| 4_8_LATCH_ASSIGN_IN_SYNC-latch_write_in_sync.jz | LATCH_ASSIGN_IN_SYNC | Cannot write latch in SYNCHRONOUS block |
| 4_8_LATCH_ASSIGN_NON_GUARDED-unguarded_latch_write.jz | LATCH_ASSIGN_NON_GUARDED | Latch assignment must use guarded enable:data syntax |
| 4_8_LATCH_CHIP_UNSUPPORTED-chip_no_latch.jz | LATCH_CHIP_UNSUPPORTED | Target chip does not support latches |
| 4_8_LATCH_ENABLE_WIDTH_NOT_1-enable_not_1bit.jz | LATCH_ENABLE_WIDTH_NOT_1 | Latch enable signal must be width 1 |
| 4_8_LATCH_IN_CONST_CONTEXT-latch_in_const.jz | LATCH_IN_CONST_CONTEXT | Latch value used in compile-time constant context |
| 4_8_LATCH_INVALID_TYPE-invalid_latch_type.jz | LATCH_INVALID_TYPE | Invalid latch type keyword (must be D or SR) |
| 4_8_LATCH_SR_WIDTH_MISMATCH-sr_width_mismatch.jz | LATCH_SR_WIDTH_MISMATCH | SR latch set/reset signal width does not match latch width |
| 4_8_LATCH_WIDTH_INVALID-invalid_latch_width.jz | LATCH_WIDTH_INVALID | Latch width value is not valid |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| LATCH_ALIAS_FORBIDDEN | error | S4.8 Latch may not be aliased | 4_8_LATCH_ALIAS_FORBIDDEN-latch_aliased.jz |
| LATCH_AS_CLOCK_OR_CDC | error | S4.8/S4.12 Latch output may not be used as clock or CDC source | 4_8_LATCH_AS_CLOCK_OR_CDC-latch_as_clock.jz |
| LATCH_ASSIGN_IN_SYNC | error | S4.8/S4.11 Cannot write latch in SYNCHRONOUS block | 4_8_LATCH_ASSIGN_IN_SYNC-latch_write_in_sync.jz |
| LATCH_ASSIGN_NON_GUARDED | error | S4.8/S4.10 Latch assignment must use guarded enable:data syntax | 4_8_LATCH_ASSIGN_NON_GUARDED-unguarded_latch_write.jz |
| LATCH_CHIP_UNSUPPORTED | error | S4.8 Target chip does not support latches | 4_8_LATCH_CHIP_UNSUPPORTED-chip_no_latch.jz |
| LATCH_ENABLE_WIDTH_NOT_1 | error | S4.8 Latch enable signal must be width 1 | 4_8_LATCH_ENABLE_WIDTH_NOT_1-enable_not_1bit.jz |
| LATCH_IN_CONST_CONTEXT | error | S4.8 Latch value used in compile-time constant context | 4_8_LATCH_IN_CONST_CONTEXT-latch_in_const.jz |
| LATCH_INVALID_TYPE | error | S4.8 Invalid latch type keyword (must be D or SR) | 4_8_LATCH_INVALID_TYPE-invalid_latch_type.jz |
| LATCH_SR_WIDTH_MISMATCH | error | S4.8 SR latch set/reset signal width does not match latch width | 4_8_LATCH_SR_WIDTH_MISMATCH-sr_width_mismatch.jz |
| LATCH_WIDTH_INVALID | error | S4.8 Latch width value is not valid | 4_8_LATCH_WIDTH_INVALID-invalid_latch_width.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| -- | -- | All assigned rules for this section are covered by existing tests |
