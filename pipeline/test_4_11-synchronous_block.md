# Test Plan: 4.11 Synchronous Block

**Specification Reference:** Section 4.11 of jz-hdl-specification.md

## 1. Objective

Verify SYNCHRONOUS block header properties (CLK, EDGE, RESET, RESET_ACTIVE, RESET_TYPE), structural constraints (clock uniqueness, register locality/home domain, read/write visibility), reset priority semantics, and error conditions (domain conflict, duplicate block, multi-clock assignment).

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
| 7 | Multiple SYNC blocks | `CLK=clk_a` and `CLK=clk_b` — different clocks |
| 8 | Empty SYNC body | Registers hold — valid |
| 9 | Register read/write | `counter <= counter + 8'd1;` — read current, write next |

### 2.2 Error Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | CLK not width-1 | `CLK=clk` where clk is 8-bit — Error |
| 2 | RESET not width-1 | `RESET=rst` where rst is 8-bit — Error |
| 3 | Invalid edge value | `EDGE=Invalid` — Error |
| 4 | Invalid reset active | `RESET_ACTIVE=Invalid` — Error |
| 5 | Invalid reset type | `RESET_TYPE=Invalid` — Error |
| 6 | Unknown parameter | `SYNCHRONOUS(FOO=bar)` — Error |
| 7 | Missing CLK | `SYNCHRONOUS() { ... }` — Error |
| 8 | Domain conflict | Register from clk_a written in `SYNCHRONOUS(CLK=clk_b)` — Error |
| 9 | Duplicate SYNC block | Two `SYNCHRONOUS(CLK=clk)` in same module — Error |
| 10 | Multi-clock assign | Same register in two different SYNC blocks — Error |
| 11 | Wire write in SYNC | `wire <= data;` in SYNCHRONOUS — Error |
| 12 | Assign to non-register | Non-register signal assigned in SYNC — Error |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Both edge | `EDGE=Both` — valid but warns |
| 2 | No reset | SYNC without RESET property — valid |
| 3 | Register read in ASYNC | Current value visible combinationally |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | CLK wider than 1-bit | Error | SYNC_CLK_WIDTH_NOT_1 | S4.11 |
| 2 | RESET wider than 1-bit | Error | SYNC_RESET_WIDTH_NOT_1 | S4.11 |
| 3 | Invalid edge keyword | Error | SYNC_EDGE_INVALID | S4.11 |
| 4 | Invalid reset active | Error | SYNC_RESET_ACTIVE_INVALID | S4.11 |
| 5 | Invalid reset type | Error | SYNC_RESET_TYPE_INVALID | S4.11 |
| 6 | Unknown parameter | Error | SYNC_UNKNOWN_PARAM | S4.11 |
| 7 | Missing CLK parameter | Error | SYNC_MISSING_CLK | S4.11 |
| 8 | Register in wrong domain | Error | DOMAIN_CONFLICT | S4.11 |
| 9 | Duplicate CLK=clk blocks | Error | DUPLICATE_BLOCK | S4.11 |
| 10 | Register in two SYNC blocks | Error | MULTI_CLK_ASSIGN | S4.11 |
| 11 | EDGE=Both | Warning | SYNC_EDGE_BOTH_WARNING | S4.11 |
| 12 | Wire assigned in SYNC | Error | WRITE_WIRE_IN_SYNC | S4.11 |
| 13 | Non-register assigned in SYNC | Error | ASSIGN_TO_NON_REGISTER_IN_SYNC | S4.11 |
| 14 | Valid SYNC block | Valid | — | Happy path |

## 4. Existing Validation Tests

| Test File | Rule Tested |
|-----------|-------------|
| 4_11_SYNC_CLK_WIDTH_NOT_1-wide_clk_signal.jz | SYNC_CLK_WIDTH_NOT_1 |
| 4_11_SYNC_EDGE_BOTH_WARNING-dual_edge_warning.jz | SYNC_EDGE_BOTH_WARNING |
| 4_11_SYNC_EDGE_INVALID-bad_edge_value.jz | SYNC_EDGE_INVALID |
| 4_11_SYNC_MISSING_CLK-no_clk_parameter.jz | SYNC_MISSING_CLK |
| 4_11_SYNC_RESET_ACTIVE_INVALID-bad_reset_active.jz | SYNC_RESET_ACTIVE_INVALID |
| 4_11_SYNC_RESET_TYPE_INVALID-bad_reset_type.jz | SYNC_RESET_TYPE_INVALID |
| 4_11_SYNC_RESET_WIDTH_NOT_1-wide_reset_signal.jz | SYNC_RESET_WIDTH_NOT_1 |
| 4_11_SYNC_UNKNOWN_PARAM-unknown_parameter.jz | SYNC_UNKNOWN_PARAM |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Test File(s) |
|---------|----------|--------------|
| SYNC_CLK_WIDTH_NOT_1 | error | 4_11_SYNC_CLK_WIDTH_NOT_1-wide_clk_signal.jz |
| SYNC_EDGE_BOTH_WARNING | warning | 4_11_SYNC_EDGE_BOTH_WARNING-dual_edge_warning.jz |
| SYNC_EDGE_INVALID | error | 4_11_SYNC_EDGE_INVALID-bad_edge_value.jz |
| SYNC_MISSING_CLK | error | 4_11_SYNC_MISSING_CLK-no_clk_parameter.jz |
| SYNC_RESET_ACTIVE_INVALID | error | 4_11_SYNC_RESET_ACTIVE_INVALID-bad_reset_active.jz |
| SYNC_RESET_TYPE_INVALID | error | 4_11_SYNC_RESET_TYPE_INVALID-bad_reset_type.jz |
| SYNC_RESET_WIDTH_NOT_1 | error | 4_11_SYNC_RESET_WIDTH_NOT_1-wide_reset_signal.jz |
| SYNC_UNKNOWN_PARAM | error | 4_11_SYNC_UNKNOWN_PARAM-unknown_parameter.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Gap Description |
|---------|----------|-----------------|
| DOMAIN_CONFLICT | error | Register used in wrong clock domain |
| DUPLICATE_BLOCK | error | Two SYNC blocks with same clock in one module |
| MULTI_CLK_ASSIGN | error | Same register assigned in multiple SYNC blocks |
| WRITE_WIRE_IN_SYNC | error | Wire assigned in SYNCHRONOUS block |
| ASSIGN_TO_NON_REGISTER_IN_SYNC | error | Non-register signal assigned in SYNC block |
