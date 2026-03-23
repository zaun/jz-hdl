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
| 7 | Latch in conditional | `IF (latch_valid) { ... }` — valid read |
| 8 | No assignment = hold | Execution path with no latch assignment holds value |

### 2.2 Error Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Latch in SYNCHRONOUS | `latch <= en : data;` in SYNC — Error |
| 2 | Latch aliased | `latch = wire;` — Error |
| 3 | Latch as clock source | Using latch output as CLK — Error |
| 4 | Latch as CDC source | Latch in CDC block — Error |
| 5 | D-latch enable not 1-bit | `hold <= 8'd1 : data;` — Error: enable must be width-1 |
| 6 | SR widths mismatch | Set and reset different widths from latch — Error |
| 7 | Latch in compile-time context | Using latch value as CONST — Error |
| 8 | Multiple assignments same path | Two guarded assigns to same latch bits — Error |
| 9 | Invalid latch type | Type keyword other than D or SR — Error |
| 10 | Invalid latch width | Width value that is not valid — Error |
| 11 | Unguarded latch write | Assignment without enable:data guard — Error |
| 12 | Latch chip unsupported | Target chip does not support latches — Error |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | 1-bit D-latch | Minimum width |
| 2 | Wide SR-latch | `LATCH { bus [32] SR; }` |
| 3 | D-latch enable always 1 | Transparent continuously (valid but unusual) |
| 4 | SR both 0 | Hold state (S=0, R=0) |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Latch write in SYNC | Error | LATCH_ASSIGN_IN_SYNC | S4.8 |
| 2 | Latch aliased | Error | LATCH_ALIAS_FORBIDDEN | S4.8 |
| 3 | Latch as clock or CDC source | Error | LATCH_AS_CLOCK_OR_CDC | S4.8 |
| 4 | Enable not 1-bit | Error | LATCH_ENABLE_WIDTH_NOT_1 | S4.8 |
| 5 | SR width mismatch | Error | LATCH_SR_WIDTH_MISMATCH | S4.8 |
| 6 | Invalid latch type | Error | LATCH_INVALID_TYPE | S4.8 |
| 7 | Invalid latch width | Error | LATCH_WIDTH_INVALID | S4.8 |
| 8 | Unguarded latch write | Error | LATCH_ASSIGN_NON_GUARDED | S4.8 |
| 9 | Latch in const context | Error | LATCH_IN_CONST_CONTEXT | S4.8 |
| 10 | Target chip unsupported | Error | LATCH_CHIP_UNSUPPORTED | S4.8 |
| 11 | Valid D-latch | Valid | — | Happy path |

## 4. Existing Validation Tests

| Test File | Rule Tested |
|-----------|-------------|
| 4_8_LATCH_ALIAS_FORBIDDEN-latch_aliased.jz | LATCH_ALIAS_FORBIDDEN |
| 4_8_LATCH_AS_CLOCK_OR_CDC-latch_as_clock.jz | LATCH_AS_CLOCK_OR_CDC |
| 4_8_LATCH_ASSIGN_NON_GUARDED-unguarded_latch_write.jz | LATCH_ASSIGN_NON_GUARDED |
| 4_8_LATCH_ENABLE_WIDTH_NOT_1-enable_not_1bit.jz | LATCH_ENABLE_WIDTH_NOT_1 |
| 4_8_LATCH_INVALID_TYPE-invalid_latch_type.jz | LATCH_INVALID_TYPE |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Test File(s) |
|---------|----------|--------------|
| LATCH_ALIAS_FORBIDDEN | error | 4_8_LATCH_ALIAS_FORBIDDEN-latch_aliased.jz |
| LATCH_AS_CLOCK_OR_CDC | error | 4_8_LATCH_AS_CLOCK_OR_CDC-latch_as_clock.jz |
| LATCH_ASSIGN_NON_GUARDED | error | 4_8_LATCH_ASSIGN_NON_GUARDED-unguarded_latch_write.jz |
| LATCH_ENABLE_WIDTH_NOT_1 | error | 4_8_LATCH_ENABLE_WIDTH_NOT_1-enable_not_1bit.jz |
| LATCH_INVALID_TYPE | error | 4_8_LATCH_INVALID_TYPE-invalid_latch_type.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Gap Description |
|---------|----------|-----------------|
| LATCH_ASSIGN_IN_SYNC | error | Latch written in SYNCHRONOUS block |
| LATCH_WIDTH_INVALID | error | Invalid latch width value |
| LATCH_SR_WIDTH_MISMATCH | error | Set and reset signal width validation |
| LATCH_IN_CONST_CONTEXT | error | Latch value used in compile-time constant context |
| LATCH_CHIP_UNSUPPORTED | error | Target chip does not support latches |
