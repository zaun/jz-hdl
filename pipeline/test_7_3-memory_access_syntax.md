# Test Plan: 7.3 Memory Access Syntax

**Specification Reference:** Section 7.3 of jz-hdl-specification.md

## 1. Objective

Verify MEM access syntax for async reads (S7.3.1), sync reads (S7.3.2), sync writes (S7.3.3), and INOUT field access (S7.3.4), including port/field validation, block context requirements, operator requirements (`<=` vs `=`), address width checks, and multiple-assignment detection.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Async read in ASYNC block | `data = rom.rd[addr];` in ASYNC block | Valid combinational read |
| 2 | Sync read addr/data in SYNC block | `cache.rd.addr <= addr; out <= cache.rd.data;` in SYNC block | Valid registered read |
| 3 | Sync write in SYNC block | `ram.wr[addr] <= data;` in SYNC block | Valid write |
| 4 | INOUT field access in SYNC block | `ram.rw.addr <= a; ram.rw.wdata <= d;` in SYNC block | Valid INOUT usage |
| 5 | INOUT .data read in ASYNC block | `out = ram.rw.data;` in ASYNC block | Valid (previous cycle's data) |
| 6 | INOUT .data read in SYNC block | `out <= ram.rw.data;` in SYNC block | Valid |
| 7 | Address exactly at depth boundary | `ram.rd[8'd255]` on 256-deep MEM | Valid (last entry) |
| 8 | Address narrower than required | 4-bit addr on 256-deep MEM (zero-extended) | Valid |
| 9 | Conditional sync write via IF | `IF (en) { ram.wr[addr] <= data; }` | Valid, single write per path |
| 10 | Conditional INOUT wdata via IF | `IF (en) { ram.rw.wdata <= data; }` | Valid, read-only cycle when not taken |

### 2.2 Error Cases

| # | Test Case | Input | Expected | Rule ID |
|---|-----------|-------|----------|---------|
| 1 | Undeclared port access | `ram.nonexistent[addr]` | Error | MEM_PORT_UNDEFINED |
| 2 | Invalid field name | `ram.rd.bogus` | Error | MEM_PORT_FIELD_UNDEFINED |
| 3 | Bracket indexing on SYNC port | `ram.sync_rd[addr]` | Error | MEM_SYNC_PORT_INDEXED |
| 4 | Bare port reference as signal | `out <= ram.rd;` | Error | MEM_PORT_USED_AS_SIGNAL |
| 5 | Read addr input field | `out <= ram.rd.addr;` | Error | MEM_PORT_ADDR_READ |
| 6 | .data field on ASYNC port | `ram.async_rd.data` | Error | MEM_ASYNC_PORT_FIELD_DATA |
| 7 | .addr on ASYNC port | `ram.async_rd.addr <= val;` | Error | MEM_SYNC_ADDR_INVALID_PORT |
| 8 | SYNC addr in ASYNC block | `ram.rd.addr <= val;` in ASYNC | Error | MEM_SYNC_ADDR_IN_ASYNC_BLOCK |
| 9 | SYNC data in ASYNC block | `out <= ram.rd.data;` in ASYNC | Error | MEM_SYNC_DATA_IN_ASYNC_BLOCK |
| 10 | SYNC addr with `=` instead of `<=` | `ram.rd.addr = val;` | Error | MEM_SYNC_ADDR_WITHOUT_RECEIVE |
| 11 | SYNC read with `=` instead of `<=` | `out = ram.rd.data;` in SYNC | Error | MEM_READ_SYNC_WITH_EQUALS |
| 12 | Field access on IN port | `ram.wr.addr` | Error | MEM_IN_PORT_FIELD_ACCESS |
| 13 | Write in ASYNC block | `ram.wr[addr] <= data;` in ASYNC | Error | MEM_WRITE_IN_ASYNC_BLOCK |
| 14 | Write to read-only port | `ram.rd[addr] <= data;` | Error | MEM_WRITE_TO_READ_PORT |
| 15 | Read from write-only port | `data <= ram.wr[addr];` | Error | MEM_READ_FROM_WRITE_PORT |
| 16 | Address width exceeds log2(depth) | 16-bit addr for 256-deep MEM | Error | MEM_ADDR_WIDTH_TOO_WIDE |
| 17 | Multiple writes to same IN port | Two writes to same port in one block | Error | MEM_MULTIPLE_WRITES_SAME_IN |
| 18 | Multiple addr assigns to SYNC read OUT | Two addr assignments for same read port | Error | MEM_MULTIPLE_ASSIGN_SYNC_READ_OUT |
| 19 | Const address out of range | `ram.rd[16'd9999]` on 256-deep MEM | Error | MEM_CONST_ADDR_OUT_OF_RANGE |
| 20 | Bracket indexing on INOUT | `ram.rw[addr]` | Error | MEM_INOUT_INDEXED |
| 21 | INOUT wdata in ASYNC block | `ram.rw.wdata <= val;` in ASYNC | Error | MEM_INOUT_WDATA_IN_ASYNC |
| 22 | INOUT addr in ASYNC block | `ram.rw.addr <= val;` in ASYNC | Error | MEM_INOUT_ADDR_IN_ASYNC |
| 23 | INOUT wdata with `=` operator | `ram.rw.wdata = val;` | Error | MEM_INOUT_WDATA_WRONG_OP |
| 24 | Multiple addr assigns on INOUT | Two `ram.rw.addr <= ...` | Error | MEM_MULTIPLE_ADDR_ASSIGNS |
| 25 | Multiple wdata assigns on INOUT | Two `ram.rw.wdata <= ...` | Error | MEM_MULTIPLE_WDATA_ASSIGNS |
| 26 | Invalid write mode on port | `IN wr { WRITE_MODE = INVALID; };` | Error | MEM_INVALID_WRITE_MODE |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Address = 0 (first entry) | `ram.rd[8'd0]` | Valid |
| 2 | Address = depth-1 (last entry) | `ram.rd[8'd255]` on 256-deep MEM | Valid |
| 3 | Wide address (16-bit) matching depth | 16-bit addr on 65536-deep MEM | Valid |
| 4 | Single-word MEM access | `ram.rd[1'b0]` on depth=1 MEM | Valid |

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1 | Access to undeclared port | `ram.nonexistent[addr]` | MEM_PORT_UNDEFINED | error |
| 2 | Invalid field name on port | `ram.rd.bogus` | MEM_PORT_FIELD_UNDEFINED | error |
| 3 | Bracket indexing on SYNC port | `ram.sync_rd[addr]` | MEM_SYNC_PORT_INDEXED | error |
| 4 | Port used as bare signal | `out <= ram.rd;` | MEM_PORT_USED_AS_SIGNAL | error |
| 5 | Reading addr input field | `out <= ram.rd.addr;` | MEM_PORT_ADDR_READ | error |
| 6 | .data field on ASYNC port | `ram.async_rd.data` | MEM_ASYNC_PORT_FIELD_DATA | error |
| 7 | .addr on ASYNC port | `ram.async_rd.addr <= val;` | MEM_SYNC_ADDR_INVALID_PORT | error |
| 8 | SYNC addr in ASYNC block | `ram.rd.addr <= val;` in ASYNC | MEM_SYNC_ADDR_IN_ASYNC_BLOCK | error |
| 9 | SYNC data in ASYNC block | `out <= ram.rd.data;` in ASYNC | MEM_SYNC_DATA_IN_ASYNC_BLOCK | error |
| 10 | SYNC addr without receive operator | `ram.rd.addr = val;` | MEM_SYNC_ADDR_WITHOUT_RECEIVE | error |
| 11 | SYNC read with `=` | `out = ram.rd.data;` in SYNC | MEM_READ_SYNC_WITH_EQUALS | error |
| 12 | Field access on IN port | `ram.wr.addr` | MEM_IN_PORT_FIELD_ACCESS | error |
| 13 | Write in ASYNC block | `ram.wr[addr] <= data;` in ASYNC | MEM_WRITE_IN_ASYNC_BLOCK | error |
| 14 | Write to read-only OUT port | `ram.rd[addr] <= data;` | MEM_WRITE_TO_READ_PORT | error |
| 15 | Read from write-only IN port | `data <= ram.wr[addr];` | MEM_READ_FROM_WRITE_PORT | error |
| 16 | Address wider than log2(depth) | 16-bit addr for 256-deep MEM | MEM_ADDR_WIDTH_TOO_WIDE | error |
| 17 | Multiple writes to same IN port | `ram.wr[a] <= x; ram.wr[b] <= y;` | MEM_MULTIPLE_WRITES_SAME_IN | error |
| 18 | Multiple addr assigns on SYNC read OUT | `ram.rd.addr <= a; ram.rd.addr <= b;` | MEM_MULTIPLE_ASSIGN_SYNC_READ_OUT | error |
| 19 | Const address exceeds depth | `ram.rd[16'd9999]` on 256-deep MEM | MEM_CONST_ADDR_OUT_OF_RANGE | error |
| 20 | Bracket indexing on INOUT | `ram.rw[addr]` | MEM_INOUT_INDEXED | error |
| 21 | INOUT wdata in ASYNC block | `ram.rw.wdata <= val;` in ASYNC | MEM_INOUT_WDATA_IN_ASYNC | error |
| 22 | INOUT addr in ASYNC block | `ram.rw.addr <= val;` in ASYNC | MEM_INOUT_ADDR_IN_ASYNC | error |
| 23 | INOUT wdata with wrong operator | `ram.rw.wdata = val;` | MEM_INOUT_WDATA_WRONG_OP | error |
| 24 | Multiple addr assigns on INOUT | `ram.rw.addr <= a; ram.rw.addr <= b;` | MEM_MULTIPLE_ADDR_ASSIGNS | error |
| 25 | Multiple wdata assigns on INOUT | `ram.rw.wdata <= x; ram.rw.wdata <= y;` | MEM_MULTIPLE_WDATA_ASSIGNS | error |
| 26 | Invalid write mode value | `IN wr { WRITE_MODE = INVALID; };` | MEM_INVALID_WRITE_MODE | error |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 7_3_HAPPY_PATH-memory_access_ok.jz | — | Happy path: valid memory access patterns |
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
| MEM_PORT_UNDEFINED | error | S7.2/S7.3/S7.7.2 Access to undeclared MEM port | 7_3_MEM_PORT_UNDEFINED-undeclared_port_access.jz |
| MEM_PORT_FIELD_UNDEFINED | error | S7.2/S7.7.2 Invalid field name on MEM port | 7_3_MEM_PORT_FIELD_UNDEFINED-invalid_field_name.jz |
| MEM_SYNC_PORT_INDEXED | error | S7.2.1/S7.3.2/S7.7.2 Bracket indexing on SYNC port | 7_3_MEM_SYNC_PORT_INDEXED-sync_port_bracket.jz |
| MEM_PORT_USED_AS_SIGNAL | error | S7.2.1/S7.3.2/S7.7.2 MEM port used as bare signal | 7_3_MEM_PORT_USED_AS_SIGNAL-bare_port_ref.jz |
| MEM_PORT_ADDR_READ | error | S7.2.1/S7.3.2/S7.7.2 Reading the addr input field | 7_3_MEM_PORT_ADDR_READ-read_addr_input.jz |
| MEM_ASYNC_PORT_FIELD_DATA | error | S7.2.1/S7.3.1/S7.7.2 .data field on ASYNC port | 7_3_MEM_ASYNC_PORT_FIELD_DATA-async_port_dot_data.jz |
| MEM_SYNC_ADDR_INVALID_PORT | error | S7.2.1/S7.3.2/S7.7.2 .addr field on ASYNC port | 7_3_MEM_SYNC_ADDR_INVALID_PORT-addr_on_async_port.jz |
| MEM_SYNC_DATA_IN_ASYNC_BLOCK | error | S7.2.1/S7.3.2/S7.7.2 SYNC data in ASYNC block | 7_3_MEM_SYNC_DATA_IN_ASYNC_BLOCK-sync_data_in_async.jz |
| MEM_IN_PORT_FIELD_ACCESS | error | S7.2.2/S7.3.3 Field access on IN (write) port | 7_3_MEM_IN_PORT_FIELD_ACCESS-write_port_dot_field.jz |
| MEM_WRITE_IN_ASYNC_BLOCK | error | S7.2.2/S7.3.3 MEM write in ASYNC block | 7_3_MEM_WRITE_IN_ASYNC_BLOCK-mem_write_in_async.jz |
| MEM_WRITE_TO_READ_PORT | error | S7.2.1/S7.3.2 Write to read-only OUT port | 7_2_MEM_WRITE_TO_READ_PORT-write_to_out_port.jz |
| MEM_READ_FROM_WRITE_PORT | error | S7.2.2/S7.3.3 Read from write-only IN port | 7_2_MEM_READ_FROM_WRITE_PORT-read_from_in_port.jz |
| MEM_ADDR_WIDTH_TOO_WIDE | error | S7.2/S7.3/S7.7.2 Address wider than log2(depth) | 7_3_MEM_ADDR_WIDTH_TOO_WIDE-wide_address.jz |
| MEM_MULTIPLE_WRITES_SAME_IN | error | S7.2.2/S7.3.3/S7.7.2 Multiple writes to same IN port | 7_2_MEM_MULTIPLE_WRITES_SAME_IN-double_write_in_port.jz |
| MEM_MULTIPLE_ASSIGN_SYNC_READ_OUT | error | S7.2.1/S7.3.2/S7.7.2 Multiple addr assigns on SYNC read OUT | 7_3_MEM_MULTIPLE_ASSIGN_SYNC_READ_OUT-multi_addr_sync_read.jz |
| MEM_CONST_ADDR_OUT_OF_RANGE | error | S7.2/S7.3/S7.7.2 Constant address exceeds depth | 7_3_MEM_CONST_ADDR_OUT_OF_RANGE-const_addr_overflow.jz |
| MEM_INVALID_WRITE_MODE | error | S7.4/S7.7.2 Invalid write mode value | 7_0_MEM_INVALID_WRITE_MODE-bad_write_mode_value.jz |
| MEM_INOUT_INDEXED | error | S7.2.3 Bracket indexing on INOUT port | 7_2_MEM_INOUT_INDEXED-inout_bracket_access.jz |
| MEM_INOUT_WDATA_IN_ASYNC | error | S7.2.3 INOUT wdata in ASYNC block | 7_2_MEM_INOUT_WDATA_IN_ASYNC-wdata_in_async.jz |
| MEM_INOUT_ADDR_IN_ASYNC | error | S7.2.3 INOUT addr in ASYNC block | 7_2_MEM_INOUT_ADDR_IN_ASYNC-addr_in_async.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| MEM_SYNC_ADDR_IN_ASYNC_BLOCK | error | Suppressed: test exists (`7_3_MEM_SYNC_ADDR_IN_ASYNC_BLOCK-sync_addr_in_async.jz`) but rule is suppressed |
| MEM_SYNC_ADDR_WITHOUT_RECEIVE | error | Suppressed: test exists (`7_3_MEM_SYNC_ADDR_WITHOUT_RECEIVE-sync_addr_equals.jz`) but rule is suppressed |
| MEM_READ_SYNC_WITH_EQUALS | error | Suppressed: test exists (`7_3_MEM_READ_SYNC_WITH_EQUALS-sync_read_equals.jz`) but rule is suppressed |
| MEM_INOUT_WDATA_WRONG_OP | error | Suppressed: test exists (`7_3_MEM_INOUT_WDATA_WRONG_OP-inout_wdata_equals.jz`) but rule is suppressed |
| MEM_MULTIPLE_ADDR_ASSIGNS | error | Unimplemented: test exists (`7_3_MEM_MULTIPLE_ADDR_ASSIGNS-inout_multi_addr.jz`) but rule is not yet implemented |
| MEM_MULTIPLE_WDATA_ASSIGNS | error | Unimplemented: test exists (`7_3_MEM_MULTIPLE_WDATA_ASSIGNS-inout_multi_wdata.jz`) but rule is not yet implemented |
