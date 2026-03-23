# Test Plan: 4.8 LATCHES

**Specification Reference:** Section 4.8 of jz-hdl-specification.md

## 1. Objective

Verify LATCH declaration (D-type and SR-type), guarded assignment syntax (enable:data for D, set:reset for SR), ASYNCHRONOUS-only placement, read semantics (passive, unconditional), power-up indeterminate state, restrictions (no alias, no CDC, no compile-time constant context), and exclusive assignment rule compliance.

## 2. Instrumentation Strategy

- **Span: `sem.latch_check`** — Trace latch validation; attributes: `latch_name`, `type` (D/SR), `width`.
- **Span: `sem.latch_guard`** — Guarded assignment analysis; attributes: `enable_width`, `data_width`.
- **Event: `latch.in_sync`** — Latch written in SYNCHRONOUS block.
- **Event: `latch.alias_attempted`** — Alias assignment on latch.

## 3. Test Scenarios

### 3.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | D-latch declaration | `LATCH { hold [8] D; }` |
| 2 | SR-latch declaration | `LATCH { state [1] SR; }` |
| 3 | D-latch guarded assign | `hold <= enable : data;` in ASYNC |
| 4 | SR-latch assign | `state <= set_sig : reset_sig;` in ASYNC |
| 5 | Latch read in ASYNC | `wire <= latch_val;` |
| 6 | Latch read in SYNC | `reg <= latch_val;` in SYNCHRONOUS |
| 7 | Latch in conditional | `IF (latch_valid) { ... }` — valid read |
| 8 | No assignment = hold | Execution path with no latch assignment → holds |

### 3.2 Boundary/Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | 1-bit D-latch | Minimum width |
| 2 | Wide SR-latch | `LATCH { bus [32] SR; }` |
| 3 | D-latch enable always 1 | Transparent continuously (valid but unusual) |
| 4 | SR both 0 | Hold state (S=0, R=0) |

### 3.3 Negative Testing

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

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Latch write in SYNC | Error | LATCH_IN_SYNC | S4.8 |
| 2 | Latch aliased | Error | LATCH_ALIAS_FORBIDDEN | S4.8 |
| 3 | Latch as CDC source | Error | LATCH_IN_CDC | S4.8 |
| 4 | Enable not 1-bit | Error | LATCH_ENABLE_WIDTH | S4.8 |
| 5 | SR width mismatch | Error | LATCH_SR_WIDTH_MISMATCH | S4.8 |
| 6 | Valid D-latch | Valid | — | Happy path |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `parser_core.c` | Parses LATCH declarations and guarded assignments | Token stream |
| `driver_assign.c` | Validates latch assignment context | Integration test |
| `driver_flow.c` | Execution path analysis for exclusive assignment | Integration test |
| `driver_clocks.c` | Clock domain validation (reject latch in CDC) | Integration test |

## 6. Rules Matrix

### 6.1 Rules Tested

| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| LATCH_IN_SYNC | Latch written in SYNCHRONOUS block | Neg 1 |
| LATCH_ALIAS_FORBIDDEN | Alias assignment on latch | Neg 2 |
| LATCH_IN_CDC | Latch used in CDC source | Neg 4 |
| LATCH_ENABLE_WIDTH | D-latch enable not width-1 | Neg 5 |
| ASSIGN_MULTI_DRIVER | Multiple assignments to same latch bits | Neg 8 |

### 6.2 Rules Missing

| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| LATCH_AS_CLOCK | S4.8 "may not be used as a clock" | Latch output driving CLK input |
| LATCH_SR_WIDTH_MISMATCH | S4.8 "set/reset must be same width" | Set and reset signal width validation |
| LATCH_TYPE_INVALID | S4.8 "type must be D or SR" | Invalid type keyword |
