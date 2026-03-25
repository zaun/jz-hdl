# Test Plan: 4.11 Synchronous Block

**Specification Reference:** Section 4.11 of jz-hdl-specification.md

## 1. Objective

Verify SYNCHRONOUS block header properties (CLK, EDGE, RESET, RESET_ACTIVE, RESET_TYPE), structural constraints (clock uniqueness, register locality/home domain, read/write visibility), reset priority semantics, and error conditions (domain conflict, duplicate block, multi-clock assignment, wire write in sync, non-register assignment in sync).

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Default Rising edge | `SYNCHRONOUS(CLK=clk) { ... }` |
| 2 | Falling edge | `SYNCHRONOUS(CLK=clk EDGE=Falling) { ... }` |
| 3 | Immediate reset | `RESET_TYPE=Immediate` |
| 4 | Clocked reset | `RESET_TYPE=Clocked` |
| 5 | Reset active high | `RESET_ACTIVE=High` |
| 6 | Reset active low | `RESET_ACTIVE=Low` (default) |
| 7 | Multiple SYNC blocks | `CLK=clk_a` and `CLK=clk_b` -- different clocks |
| 8 | Empty SYNC body | Registers hold -- valid |
| 9 | Register read/write | `counter <= counter + 8'd1;` -- read current, write next |

### 2.2 Error Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | CLK not width-1 | `CLK=clk` where clk is 8-bit -- Error |
| 2 | RESET not width-1 | `RESET=rst` where rst is 8-bit -- Error |
| 3 | Invalid edge value | `EDGE=Invalid` -- Error |
| 4 | Invalid reset active | `RESET_ACTIVE=Invalid` -- Error |
| 5 | Invalid reset type | `RESET_TYPE=Invalid` -- Error |
| 6 | Unknown parameter | `SYNCHRONOUS(FOO=bar)` -- Error |
| 7 | Missing CLK | `SYNCHRONOUS() { ... }` -- Error |
| 8 | Domain conflict | Register from clk_a written in `SYNCHRONOUS(CLK=clk_b)` -- Error |
| 9 | Duplicate SYNC block | Two `SYNCHRONOUS(CLK=clk)` in same module -- Error |
| 10 | Multi-clock assign | Same register in two different SYNC blocks -- Error |
| 11 | Wire write in SYNC | `wire <= data;` in SYNCHRONOUS -- Error |
| 12 | Assign to non-register | Non-register signal assigned in SYNC -- Error |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Both edge | `EDGE=Both` -- valid but warns |
| 2 | No reset | SYNC without RESET property -- valid |
| 3 | Register read in ASYNC | Current value visible combinationally |

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1 | CLK wider than 1-bit | `CLK=clk` (clk is [8]) | SYNC_CLK_WIDTH_NOT_1 | error |
| 2 | RESET wider than 1-bit | `RESET=rst` (rst is [8]) | SYNC_RESET_WIDTH_NOT_1 | error |
| 3 | Invalid edge keyword | `EDGE=Invalid` | SYNC_EDGE_INVALID | error |
| 4 | Invalid reset active | `RESET_ACTIVE=Invalid` | SYNC_RESET_ACTIVE_INVALID | error |
| 5 | Invalid reset type | `RESET_TYPE=Invalid` | SYNC_RESET_TYPE_INVALID | error |
| 6 | Unknown parameter | `SYNCHRONOUS(FOO=bar)` | SYNC_UNKNOWN_PARAM | error |
| 7 | Missing CLK parameter | `SYNCHRONOUS() { ... }` | SYNC_MISSING_CLK | error |
| 8 | Register in wrong domain | Register from clk_a used in CLK=clk_b block | DOMAIN_CONFLICT | error |
| 9 | Duplicate CLK=clk blocks | Two SYNCHRONOUS(CLK=clk) in one module | DUPLICATE_BLOCK | error |
| 10 | Register in two SYNC blocks | Same register assigned in CLK=clk_a and CLK=clk_b | MULTI_CLK_ASSIGN | error |
| 11 | EDGE=Both used | `SYNCHRONOUS(CLK=clk EDGE=Both)` | SYNC_EDGE_BOTH_WARNING | warning |
| 12 | Wire assigned in SYNC | `wire <= data;` in SYNCHRONOUS | WRITE_WIRE_IN_SYNC | error |
| 13 | Non-register assigned in SYNC | Port or other non-register on LHS | ASSIGN_TO_NON_REGISTER_IN_SYNC | error |
| 14 | Valid SYNC block | All parameters valid, register assignments | -- | -- (pass) |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 4_11_SYNC_HAPPY_PATH-valid_sync_block_ok.jz | -- | Happy path: valid SYNCHRONOUS block with all parameters |
| 4_11_SYNC_CLK_WIDTH_NOT_1-wide_clk_signal.jz | SYNC_CLK_WIDTH_NOT_1 | CLK signal wider than 1-bit |
| 4_11_SYNC_EDGE_BOTH_WARNING-dual_edge_warning.jz | SYNC_EDGE_BOTH_WARNING | EDGE=Both triggers a warning |
| 4_11_SYNC_EDGE_INVALID-bad_edge_value.jz | SYNC_EDGE_INVALID | Invalid EDGE parameter value |
| 4_11_SYNC_MISSING_CLK-no_clk_parameter.jz | SYNC_MISSING_CLK | SYNCHRONOUS block missing CLK |
| 4_11_SYNC_RESET_ACTIVE_INVALID-bad_reset_active.jz | SYNC_RESET_ACTIVE_INVALID | Invalid RESET_ACTIVE value |
| 4_11_SYNC_RESET_TYPE_INVALID-bad_reset_type.jz | SYNC_RESET_TYPE_INVALID | Invalid RESET_TYPE value |
| 4_11_SYNC_RESET_WIDTH_NOT_1-wide_reset_signal.jz | SYNC_RESET_WIDTH_NOT_1 | RESET signal wider than 1-bit |
| 4_11_SYNC_UNKNOWN_PARAM-unknown_parameter.jz | SYNC_UNKNOWN_PARAM | Unknown parameter in SYNCHRONOUS header |
| 4_11_DOMAIN_CONFLICT-register_wrong_domain.jz | DOMAIN_CONFLICT | Register assigned in wrong clock domain |
| 4_11_DUPLICATE_BLOCK-duplicate_sync_block.jz | DUPLICATE_BLOCK | Two SYNCHRONOUS blocks for same clock |
| 4_11_MULTI_CLK_ASSIGN-register_multi_clock.jz | MULTI_CLK_ASSIGN | Same register assigned in multiple SYNC blocks |
| 4_11_WRITE_WIRE_IN_SYNC-wire_in_sync.jz | WRITE_WIRE_IN_SYNC | Wire assigned in SYNCHRONOUS block |
| 4_11_ASSIGN_TO_NON_REGISTER_IN_SYNC-non_register_in_sync.jz | ASSIGN_TO_NON_REGISTER_IN_SYNC | Non-register signal assigned in SYNCHRONOUS block |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| ASSIGN_TO_NON_REGISTER_IN_SYNC | error | S5.2 Only REGISTERs may be assigned in SYNCHRONOUS blocks | 4_11_ASSIGN_TO_NON_REGISTER_IN_SYNC-non_register_in_sync.jz |
| DOMAIN_CONFLICT | error | S4.11/S4.12 Register assigned in wrong clock domain | 4_11_DOMAIN_CONFLICT-register_wrong_domain.jz |
| DUPLICATE_BLOCK | error | S4.11/S4.12 Two SYNCHRONOUS blocks with same clock in one module | 4_11_DUPLICATE_BLOCK-duplicate_sync_block.jz |
| MULTI_CLK_ASSIGN | error | S4.11/S4.12 Same register assigned in multiple SYNCHRONOUS blocks | 4_11_MULTI_CLK_ASSIGN-register_multi_clock.jz |
| SYNC_CLK_WIDTH_NOT_1 | error | S4.11 CLK signal must be width 1 | 4_11_SYNC_CLK_WIDTH_NOT_1-wide_clk_signal.jz |
| SYNC_EDGE_BOTH_WARNING | warning | S4.11 Both-edge triggering is unusual; warns on EDGE=Both | 4_11_SYNC_EDGE_BOTH_WARNING-dual_edge_warning.jz |
| SYNC_EDGE_INVALID | error | S4.11 Invalid EDGE parameter value | 4_11_SYNC_EDGE_INVALID-bad_edge_value.jz |
| SYNC_MISSING_CLK | error | S4.11 SYNCHRONOUS block missing required CLK parameter | 4_11_SYNC_MISSING_CLK-no_clk_parameter.jz |
| SYNC_RESET_ACTIVE_INVALID | error | S4.11 Invalid RESET_ACTIVE parameter value | 4_11_SYNC_RESET_ACTIVE_INVALID-bad_reset_active.jz |
| SYNC_RESET_TYPE_INVALID | error | S4.11 Invalid RESET_TYPE parameter value | 4_11_SYNC_RESET_TYPE_INVALID-bad_reset_type.jz |
| SYNC_RESET_WIDTH_NOT_1 | error | S4.11 RESET signal must be width 1 | 4_11_SYNC_RESET_WIDTH_NOT_1-wide_reset_signal.jz |
| SYNC_UNKNOWN_PARAM | error | S4.11 Unknown parameter in SYNCHRONOUS block header | 4_11_SYNC_UNKNOWN_PARAM-unknown_parameter.jz |
| WRITE_WIRE_IN_SYNC | error | S4.5/S5.2 Cannot assign to WIRE in SYNCHRONOUS block | 4_11_WRITE_WIRE_IN_SYNC-wire_in_sync.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| -- | -- | All assigned rules for this section are covered by existing tests |
