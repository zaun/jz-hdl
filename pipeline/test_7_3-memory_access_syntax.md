# Test Plan: 7.3 Memory Access Syntax

**Specification Reference:** Section 7.3 of jz-hdl-specification.md

## 1. Objective

Verify MEM access syntax for async reads, sync reads, sync writes, and INOUT field access, including port/field validation, block context requirements, address width checks, and multiple-assignment detection.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Async read | `data <= rom.rd[addr];` in ASYNC block | Valid combinational read |
| 2 | Sync read | `data <= cache.rd[addr];` in SYNC block | Valid registered read |
| 3 | Sync write | `ram.wr[addr] <= data;` in SYNC block | Valid write |
| 4 | INOUT field access | `ram.rw.addr <= a; ram.rw.wdata <= d; out <= ram.rw.rdata;` | Valid INOUT usage |

### 2.2 Error Cases

| # | Test Case | Input | Expected | Rule ID |
|---|-----------|-------|----------|---------|
| 1 | Undeclared port access | `ram.nonexistent[addr]` | Error | MEM_PORT_UNDEFINED |
| 2 | Invalid field name | `ram.rd.bogus` | Error | MEM_PORT_FIELD_UNDEFINED |
| 3 | Bracket indexing on SYNC port | `ram.sync_rd[addr]` with wrong syntax | Error | MEM_SYNC_PORT_INDEXED |
| 4 | Bare port reference as signal | `out <= ram.rd;` | Error | MEM_PORT_USED_AS_SIGNAL |
| 5 | Read addr input field | `out <= ram.rd.addr;` | Error | MEM_PORT_ADDR_READ |
| 6 | .data field on ASYNC port | `ram.async_rd.data` | Error | MEM_ASYNC_PORT_FIELD_DATA |
| 7 | .addr on ASYNC port | `ram.async_rd.addr <= val;` | Error | MEM_SYNC_ADDR_INVALID_PORT |
| 8 | SYNC addr in ASYNC block | `ram.rd.addr <= val;` in ASYNC | Error | MEM_SYNC_ADDR_IN_ASYNC_BLOCK |
| 9 | SYNC data in ASYNC block | `out <= ram.rd.data;` in ASYNC | Error | MEM_SYNC_DATA_IN_ASYNC_BLOCK |
| 10 | SYNC addr with `=` instead of `<=` | `ram.rd.addr = val;` | Error | MEM_SYNC_ADDR_WITHOUT_RECEIVE |
| 11 | SYNC read with `=` instead of `<=` | `out = ram.rd.data;` | Error | MEM_READ_SYNC_WITH_EQUALS |
| 12 | Field access on IN port | `ram.wr.field` | Error | MEM_IN_PORT_FIELD_ACCESS |
| 13 | Write in ASYNC block | `ram.wr[addr] <= data;` in ASYNC | Error | MEM_WRITE_IN_ASYNC_BLOCK |
| 14 | Write to read-only port | `ram.rd[addr] <= data;` | Error | MEM_WRITE_TO_READ_PORT |
| 15 | Read from write-only port | `data <= ram.wr[addr];` | Error | MEM_READ_FROM_WRITE_PORT |
| 16 | Address width exceeds log2(depth) | 16-bit addr for 256-deep MEM | Error | MEM_ADDR_WIDTH_TOO_WIDE |
| 17 | Multiple writes to same IN port | Two writes to same port | Error | MEM_MULTIPLE_WRITES_SAME_IN |
| 18 | Multiple addr assigns to SYNC read OUT | Two addr assignments for same read port | Error | MEM_MULTIPLE_ASSIGN_SYNC_READ_OUT |
| 19 | Const address out of range | `ram.rd[16'd9999]` on small MEM | Error | MEM_CONST_ADDR_OUT_OF_RANGE |
| 20 | Bracket indexing on INOUT | `ram.rw[addr]` | Error | MEM_INOUT_INDEXED |
| 21 | INOUT wdata in ASYNC block | `ram.rw.wdata <= val;` in ASYNC | Error | MEM_INOUT_WDATA_IN_ASYNC |
| 22 | INOUT addr in ASYNC block | `ram.rw.addr <= val;` in ASYNC | Error | MEM_INOUT_ADDR_IN_ASYNC |
| 23 | INOUT wdata with `=` operator | `ram.rw.wdata = val;` | Error | MEM_INOUT_WDATA_WRONG_OP |
| 24 | Multiple addr assigns on INOUT | Two `ram.rw.addr <= ...` | Error | MEM_MULTIPLE_ADDR_ASSIGNS |
| 25 | Multiple wdata assigns on INOUT | Two `ram.rw.wdata <= ...` | Error | MEM_MULTIPLE_WDATA_ASSIGNS |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Address = 0 (first entry) | `ram.rd[8'd0]` | Valid |
| 2 | Address = depth-1 (last entry) | `ram.rd[8'd255]` on 256-deep MEM | Valid |
| 3 | Wide address (16-bit) matching depth | 16-bit addr on 65536-deep MEM | Valid |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| 1 | Access to undeclared port | Error | MEM_PORT_UNDEFINED | error | S7.3 |
| 2 | Invalid field name on port | Error | MEM_PORT_FIELD_UNDEFINED | error | S7.3 |
| 3 | Bracket indexing on SYNC port | Error | MEM_SYNC_PORT_INDEXED | error | S7.3 |
| 4 | Port used as bare signal | Error | MEM_PORT_USED_AS_SIGNAL | error | S7.3 |
| 5 | Reading addr input field | Error | MEM_PORT_ADDR_READ | error | S7.3 |
| 6 | .data field on ASYNC port | Error | MEM_ASYNC_PORT_FIELD_DATA | error | S7.3 |
| 7 | .addr on ASYNC port | Error | MEM_SYNC_ADDR_INVALID_PORT | error | S7.3 |
| 8 | SYNC addr in ASYNC block | Error | MEM_SYNC_ADDR_IN_ASYNC_BLOCK | error | S7.3 |
| 9 | SYNC data in ASYNC block | Error | MEM_SYNC_DATA_IN_ASYNC_BLOCK | error | S7.3 |
| 10 | SYNC addr without receive operator | Error | MEM_SYNC_ADDR_WITHOUT_RECEIVE | error | S7.3 |
| 11 | SYNC read with `=` | Error | MEM_READ_SYNC_WITH_EQUALS | error | S7.3 |
| 12 | Field access on IN port | Error | MEM_IN_PORT_FIELD_ACCESS | error | S7.3 |
| 13 | Write in ASYNC block | Error | MEM_WRITE_IN_ASYNC_BLOCK | error | S7.3 |
| 14 | Write to read-only OUT port | Error | MEM_WRITE_TO_READ_PORT | error | S7.3 |
| 15 | Read from write-only IN port | Error | MEM_READ_FROM_WRITE_PORT | error | S7.3 |
| 16 | Address wider than log2(depth) | Error | MEM_ADDR_WIDTH_TOO_WIDE | error | S7.3 |
| 17 | Multiple writes to same IN port | Error | MEM_MULTIPLE_WRITES_SAME_IN | error | S7.3 |
| 18 | Multiple addr assigns on SYNC read OUT | Error | MEM_MULTIPLE_ASSIGN_SYNC_READ_OUT | error | S7.3 |
| 19 | Const address exceeds depth | Error | MEM_CONST_ADDR_OUT_OF_RANGE | error | S7.3 |
| 20 | Bracket indexing on INOUT | Error | MEM_INOUT_INDEXED | error | S7.3 |
| 21 | INOUT wdata in ASYNC block | Error | MEM_INOUT_WDATA_IN_ASYNC | error | S7.3 |
| 22 | INOUT addr in ASYNC block | Error | MEM_INOUT_ADDR_IN_ASYNC | error | S7.3 |
| 23 | INOUT wdata with wrong operator | Error | MEM_INOUT_WDATA_WRONG_OP | error | S7.3 |
| 24 | Multiple addr assigns on INOUT | Error | MEM_MULTIPLE_ADDR_ASSIGNS | error | S7.3 |
| 25 | Multiple wdata assigns on INOUT | Error | MEM_MULTIPLE_WDATA_ASSIGNS | error | S7.3 |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 7_3_MEM_ADDR_WIDTH_TOO_WIDE-wide_address.jz | MEM_ADDR_WIDTH_TOO_WIDE | Address width exceeds log2(depth) |
| 7_3_MEM_ASYNC_PORT_FIELD_DATA-async_port_dot_data.jz | MEM_ASYNC_PORT_FIELD_DATA | .data field access on ASYNC port |
| 7_3_MEM_CONST_ADDR_OUT_OF_RANGE-const_addr_overflow.jz | MEM_CONST_ADDR_OUT_OF_RANGE | Constant address exceeds MEM depth |
| 7_3_MEM_IN_PORT_FIELD_ACCESS-write_port_dot_field.jz | MEM_IN_PORT_FIELD_ACCESS | Field access on IN (write) port |
| 7_3_MEM_INOUT_WDATA_WRONG_OP-inout_wdata_equals.jz | MEM_INOUT_WDATA_WRONG_OP | INOUT wdata assigned with `=` instead of `<=` |
| 7_3_MEM_MULTIPLE_ADDR_ASSIGNS-inout_multi_addr.jz | MEM_MULTIPLE_ADDR_ASSIGNS | Multiple addr assignments on INOUT port |
| 7_3_MEM_MULTIPLE_ASSIGN_SYNC_READ_OUT-multi_addr_sync_read.jz | MEM_MULTIPLE_ASSIGN_SYNC_READ_OUT | Multiple addr assignments on SYNC read OUT port |
| 7_3_MEM_MULTIPLE_WDATA_ASSIGNS-inout_multi_wdata.jz | MEM_MULTIPLE_WDATA_ASSIGNS | Multiple wdata assignments on INOUT port |
| 7_3_MEM_PORT_ADDR_READ-read_addr_input.jz | MEM_PORT_ADDR_READ | Reading the addr input field |
| 7_3_MEM_PORT_FIELD_UNDEFINED-invalid_field_name.jz | MEM_PORT_FIELD_UNDEFINED | Invalid field name on MEM port |
| 7_3_MEM_PORT_UNDEFINED-undeclared_port_access.jz | MEM_PORT_UNDEFINED | Access to undeclared MEM port |
| 7_3_MEM_PORT_USED_AS_SIGNAL-bare_port_ref.jz | MEM_PORT_USED_AS_SIGNAL | MEM port used as bare signal reference |
| 7_3_MEM_READ_SYNC_WITH_EQUALS-sync_read_equals.jz | MEM_READ_SYNC_WITH_EQUALS | SYNC read using `=` instead of `<=` |
| 7_3_MEM_SYNC_ADDR_IN_ASYNC_BLOCK-sync_addr_in_async.jz | MEM_SYNC_ADDR_IN_ASYNC_BLOCK | SYNC addr assignment in ASYNC block |
| 7_3_MEM_SYNC_ADDR_INVALID_PORT-addr_on_async_port.jz | MEM_SYNC_ADDR_INVALID_PORT | .addr field on ASYNC port |
| 7_3_MEM_SYNC_ADDR_WITHOUT_RECEIVE-sync_addr_equals.jz | MEM_SYNC_ADDR_WITHOUT_RECEIVE | SYNC addr with `=` instead of `<=` |
| 7_3_MEM_SYNC_DATA_IN_ASYNC_BLOCK-sync_data_in_async.jz | MEM_SYNC_DATA_IN_ASYNC_BLOCK | SYNC data access in ASYNC block |
| 7_3_MEM_SYNC_PORT_INDEXED-sync_port_bracket.jz | MEM_SYNC_PORT_INDEXED | Bracket indexing on SYNC port |
| 7_3_MEM_WRITE_IN_ASYNC_BLOCK-mem_write_in_async.jz | MEM_WRITE_IN_ASYNC_BLOCK | MEM write in ASYNC block |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| MEM_PORT_UNDEFINED | error | S7.3 Access to undeclared MEM port | 7_3_MEM_PORT_UNDEFINED-undeclared_port_access.jz |
| MEM_PORT_FIELD_UNDEFINED | error | S7.3 Invalid field name on MEM port | 7_3_MEM_PORT_FIELD_UNDEFINED-invalid_field_name.jz |
| MEM_SYNC_PORT_INDEXED | error | S7.3 Bracket indexing on SYNC port | 7_3_MEM_SYNC_PORT_INDEXED-sync_port_bracket.jz |
| MEM_PORT_USED_AS_SIGNAL | error | S7.3 MEM port used as bare signal | 7_3_MEM_PORT_USED_AS_SIGNAL-bare_port_ref.jz |
| MEM_PORT_ADDR_READ | error | S7.3 Reading the addr input field | 7_3_MEM_PORT_ADDR_READ-read_addr_input.jz |
| MEM_ASYNC_PORT_FIELD_DATA | error | S7.3 .data field on ASYNC port | 7_3_MEM_ASYNC_PORT_FIELD_DATA-async_port_dot_data.jz |
| MEM_SYNC_ADDR_INVALID_PORT | error | S7.3 .addr field on ASYNC port | 7_3_MEM_SYNC_ADDR_INVALID_PORT-addr_on_async_port.jz |
| MEM_SYNC_ADDR_IN_ASYNC_BLOCK | error | S7.3 SYNC addr in ASYNC block | 7_3_MEM_SYNC_ADDR_IN_ASYNC_BLOCK-sync_addr_in_async.jz |
| MEM_SYNC_DATA_IN_ASYNC_BLOCK | error | S7.3 SYNC data in ASYNC block | 7_3_MEM_SYNC_DATA_IN_ASYNC_BLOCK-sync_data_in_async.jz |
| MEM_SYNC_ADDR_WITHOUT_RECEIVE | error | S7.3 SYNC addr with `=` instead of `<=` | 7_3_MEM_SYNC_ADDR_WITHOUT_RECEIVE-sync_addr_equals.jz |
| MEM_READ_SYNC_WITH_EQUALS | error | S7.3 SYNC read with `=` instead of `<=` | 7_3_MEM_READ_SYNC_WITH_EQUALS-sync_read_equals.jz |
| MEM_IN_PORT_FIELD_ACCESS | error | S7.3 Field access on IN (write) port | 7_3_MEM_IN_PORT_FIELD_ACCESS-write_port_dot_field.jz |
| MEM_WRITE_IN_ASYNC_BLOCK | error | S7.3 MEM write in ASYNC block | 7_3_MEM_WRITE_IN_ASYNC_BLOCK-mem_write_in_async.jz |
| MEM_WRITE_TO_READ_PORT | error | S7.3 Write to read-only OUT port | (covered by 7_2 tests) |
| MEM_READ_FROM_WRITE_PORT | error | S7.3 Read from write-only IN port | (covered by 7_2 tests) |
| MEM_ADDR_WIDTH_TOO_WIDE | error | S7.3 Address wider than log2(depth) | 7_3_MEM_ADDR_WIDTH_TOO_WIDE-wide_address.jz |
| MEM_MULTIPLE_WRITES_SAME_IN | error | S7.3 Multiple writes to same IN port | (covered by 7_2 tests) |
| MEM_MULTIPLE_ASSIGN_SYNC_READ_OUT | error | S7.3 Multiple addr assigns on SYNC read OUT | 7_3_MEM_MULTIPLE_ASSIGN_SYNC_READ_OUT-multi_addr_sync_read.jz |
| MEM_CONST_ADDR_OUT_OF_RANGE | error | S7.3 Constant address exceeds depth | 7_3_MEM_CONST_ADDR_OUT_OF_RANGE-const_addr_overflow.jz |
| MEM_INOUT_INDEXED | error | S7.3 Bracket indexing on INOUT port | (covered by 7_2 tests) |
| MEM_INOUT_WDATA_IN_ASYNC | error | S7.3 INOUT wdata in ASYNC block | (covered by 7_2 tests) |
| MEM_INOUT_ADDR_IN_ASYNC | error | S7.3 INOUT addr in ASYNC block | (covered by 7_2 tests) |
| MEM_INOUT_WDATA_WRONG_OP | error | S7.3 INOUT wdata with `=` operator | 7_3_MEM_INOUT_WDATA_WRONG_OP-inout_wdata_equals.jz |
| MEM_MULTIPLE_ADDR_ASSIGNS | error | S7.3 Multiple addr assigns on INOUT | 7_3_MEM_MULTIPLE_ADDR_ASSIGNS-inout_multi_addr.jz |
| MEM_MULTIPLE_WDATA_ASSIGNS | error | S7.3 Multiple wdata assigns on INOUT | 7_3_MEM_MULTIPLE_WDATA_ASSIGNS-inout_multi_wdata.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| (none) | -- | All memory access syntax rules have validation tests |
