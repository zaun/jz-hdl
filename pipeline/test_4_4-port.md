# Test Plan: 4.4 PORT (Module Interface)

**Specification Reference:** Section 4.4 (including 4.4.1 BUS Ports) of jz-hdl-specification.md

## 1. Objective

Verify PORT declarations, direction enforcement, BUS ports. Confirm that port width is mandatory, IN ports are read-only, OUT ports are write-only, INOUT tristate rules are enforced, BUS port declarations validate bus existence/role/array bounds/index requirements, BUS signal access respects directionality, wildcard width rules are enforced, and alias-in-conditional is rejected.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | IN port read | Read IN port in ASYNCHRONOUS -- valid |
| 2 | OUT port write | Write OUT port in ASYNCHRONOUS -- valid |
| 3 | INOUT conditional | Read/write INOUT based on control signal |
| 4 | BUS SOURCE port | BUS with SOURCE role, write OUT signals |
| 5 | BUS TARGET port | BUS with TARGET role, read OUT signals (resolved as IN) |
| 6 | Arrayed BUS | `BUS SPI TARGET [4] spi_bus;` -- 4 instances |
| 7 | Wildcard broadcast | `bus[*].signal <= 1'b1;` -- broadcast to all |
| 8 | BUS dot notation | `source.tx`, `target.rx` -- valid access |
| 9 | Extension modifiers | `OUT [8] p <=z narrow_sig;` -- zero-extend |

### 2.2 Error Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Write to IN port | `in_port <= data;` -- error: IN ports are read-only |
| 2 | Read from OUT port | `reg <= out_port;` -- error: OUT ports are write-only |
| 3 | Missing width | `PORT { IN data; }` -- error: width mandatory |
| 4 | Tristate on non-INOUT | IN/OUT port assigned z -- error |
| 5 | BUS references unknown bus | BUS port names a bus not declared in project -- error |
| 6 | BUS invalid role | BUS port role is neither SOURCE nor TARGET -- error |
| 7 | BUS array count invalid | Non-positive BUS array count -- error |
| 8 | BUS index required | Arrayed BUS access without index -- error |
| 9 | BUS index on non-array | Indexed access on scalar BUS port -- error |
| 10 | BUS index out of range | `bus[5].sig` where count=4 -- error |
| 11 | BUS dot on non-BUS port | Member access on regular port -- error |
| 12 | BUS signal undefined | `bus.nonexistent` -- error |
| 13 | Read from writable BUS signal | Read access to writable-only signal -- error |
| 14 | Write to readable BUS signal | Write access to readable-only signal -- error |
| 15 | Wildcard width mismatch | `bus[*].sig <= 3'b101;` where count=4 -- error |
| 16 | BUS tristate mismatch | z assigned to non-writable BUS signal -- error |
| 17 | Alias in conditional | `IF (c) { a = b; }` -- error: alias in control flow |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | 1-bit port | `IN [1] clk;` -- minimum width |
| 2 | Wide port | `OUT [256] data;` -- large width |
| 3 | Module with only OUT ports | Valid (oscillator pattern) |
| 4 | BUS array count = 1 | Single-element array |
| 5 | INOUT with z release | `inout_port <= en ? data : 8'bz;` -- valid tri-state |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|-----------------|---------|----------|-------|
| 1 | Write to IN port | Error: cannot assign to IN port | PORT_DIRECTION_MISMATCH_IN | error | S4.4 |
| 2 | Read from OUT port | Error: reading OUT port inside module | PORT_DIRECTION_MISMATCH_OUT | error | S4.4 |
| 3 | Missing port width | Error: port without [N] width | PORT_MISSING_WIDTH | error | S4.4 |
| 4 | Tristate on IN/OUT port | Error: only INOUT may use z | PORT_TRISTATE_MISMATCH | error | S4.4 |
| 5 | BUS references unknown bus | Error: BUS not declared in project | BUS_PORT_UNKNOWN_BUS | error | S4.4.1 |
| 6 | BUS invalid role | Error: role must be SOURCE or TARGET | BUS_PORT_INVALID_ROLE | error | S4.4.1 |
| 7 | BUS array count invalid | Error: non-positive array count | BUS_PORT_ARRAY_COUNT_INVALID | error | S4.4.1 |
| 8 | BUS index required on array | Error: arrayed BUS needs index | BUS_PORT_INDEX_REQUIRED | error | S4.4.1 |
| 9 | BUS index on non-array | Error: scalar BUS cannot be indexed | BUS_PORT_INDEX_NOT_ARRAY | error | S4.4.1 |
| 10 | BUS index out of range | Error: index outside declared range | BUS_PORT_INDEX_OUT_OF_RANGE | error | S4.4.1 |
| 11 | BUS dot on non-BUS port | Error: member access on non-BUS | BUS_PORT_NOT_BUS | error | S4.4.1 |
| 12 | BUS signal undefined | Error: signal not in BUS definition | BUS_SIGNAL_UNDEFINED | error | S4.4.1 |
| 13 | Read from writable BUS signal | Error: read access to writable signal | BUS_SIGNAL_READ_FROM_WRITABLE | error | S4.4.1 |
| 14 | Write to readable BUS signal | Error: write access to readable signal | BUS_SIGNAL_WRITE_TO_READABLE | error | S4.4.1 |
| 15 | Wildcard width mismatch | Error: RHS width must be 1 or array count | BUS_WILDCARD_WIDTH_MISMATCH | error | S4.4.1 |
| 16 | BUS tristate mismatch | Error: z on non-writable BUS signal | BUS_TRISTATE_MISMATCH | error | S4.4.1 |
| 17 | Alias `=` inside IF/SELECT | Error: alias in conditional | ASYNC_ALIAS_IN_CONDITIONAL | error | S4.10/S5.3 |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 4_4_ASYNC_ALIAS_IN_CONDITIONAL-alias_in_if_select.jz | ASYNC_ALIAS_IN_CONDITIONAL | Alias `=` inside IF/SELECT is forbidden |
| 4_4_BUS_PORT_INDEX_OUT_OF_RANGE-bus_index_bounds.jz | BUS_PORT_INDEX_OUT_OF_RANGE | BUS port index outside declared range |
| 4_4_BUS_SIGNAL_READ_FROM_WRITABLE-read_writable_signal.jz | BUS_SIGNAL_READ_FROM_WRITABLE | Read access to writable BUS signal |
| 4_4_BUS_SIGNAL_UNDEFINED-nonexistent_bus_signal.jz | BUS_SIGNAL_UNDEFINED | BUS signal does not exist in BUS definition |
| 4_4_BUS_SIGNAL_WRITE_TO_READABLE-write_readable_signal.jz | BUS_SIGNAL_WRITE_TO_READABLE | Write access to readable BUS signal |
| 4_4_BUS_WILDCARD_WIDTH_MISMATCH-wildcard_width_invalid.jz | BUS_WILDCARD_WIDTH_MISMATCH | BUS wildcard assignment RHS width invalid |
| 4_4_PORT_DIRECTION_MISMATCH_IN-write_to_input.jz | PORT_DIRECTION_MISMATCH_IN | Cannot assign to IN port |
| 4_4_PORT_DIRECTION_MISMATCH_OUT-read_from_output.jz | PORT_DIRECTION_MISMATCH_OUT | Reading from OUT port inside module |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| PORT_DIRECTION_MISMATCH_IN | error | S4.4/S5.1 Cannot assign to IN port; IN ports are read-only inside the module | 4_4_PORT_DIRECTION_MISMATCH_IN-write_to_input.jz |
| PORT_DIRECTION_MISMATCH_OUT | error | S4.4/S5.1/S5.2/S8.1 Reading from OUT port inside module (outputs are write-only) | 4_4_PORT_DIRECTION_MISMATCH_OUT-read_from_output.jz |
| BUS_PORT_INDEX_OUT_OF_RANGE | error | S4.4.1 BUS port index is outside the declared range | 4_4_BUS_PORT_INDEX_OUT_OF_RANGE-bus_index_bounds.jz |
| BUS_SIGNAL_UNDEFINED | error | S4.4.1 BUS signal does not exist in BUS definition | 4_4_BUS_SIGNAL_UNDEFINED-nonexistent_bus_signal.jz |
| BUS_SIGNAL_READ_FROM_WRITABLE | error | S4.4.1 Read access to writable BUS signal is not allowed | 4_4_BUS_SIGNAL_READ_FROM_WRITABLE-read_writable_signal.jz |
| BUS_SIGNAL_WRITE_TO_READABLE | error | S4.4.1 Write access to readable BUS signal is not allowed | 4_4_BUS_SIGNAL_WRITE_TO_READABLE-write_readable_signal.jz |
| BUS_WILDCARD_WIDTH_MISMATCH | error | S4.4.1 BUS wildcard assignment requires RHS width of 1 or array count | 4_4_BUS_WILDCARD_WIDTH_MISMATCH-wildcard_width_invalid.jz |
| ASYNC_ALIAS_IN_CONDITIONAL | error | S4.10/S5.3 Alias `=` inside IF/SELECT is forbidden | 4_4_ASYNC_ALIAS_IN_CONDITIONAL-alias_in_if_select.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| PORT_MISSING_WIDTH | error | No dedicated 4_4 test file; may be caught as parse error before semantic analysis |
| PORT_TRISTATE_MISMATCH | error | No dedicated 4_4 test file; tristate on non-INOUT port not yet covered |
| BUS_PORT_UNKNOWN_BUS | error | No dedicated 4_4 test file; requires project-level BUS definition context |
| BUS_PORT_INVALID_ROLE | error | No dedicated 4_4 test file; invalid BUS role not yet covered |
| BUS_PORT_ARRAY_COUNT_INVALID | error | No dedicated 4_4 test file; non-positive array count not yet covered |
| BUS_PORT_INDEX_REQUIRED | error | No dedicated 4_4 test file; arrayed BUS without index not yet covered |
| BUS_PORT_INDEX_NOT_ARRAY | error | No dedicated 4_4 test file; indexed access on scalar BUS not yet covered |
| BUS_PORT_NOT_BUS | error | No dedicated 4_4 test file; dot notation on non-BUS port not yet covered |
| BUS_TRISTATE_MISMATCH | error | No dedicated 4_4 test file; z on non-writable BUS signal not yet covered |
