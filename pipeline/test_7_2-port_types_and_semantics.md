# Test Plan: 7.2 Port Types and Semantics

**Specification Reference:** Section 7.2 of jz-hdl-specification.md

## 1. Objective

Verify MEM port types (OUT/IN/INOUT) and their read/write semantics, including direction enforcement, INOUT restrictions, ASYNC block restrictions for INOUT fields, and multiple-write detection.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | OUT ASYNC read port -- combinational read | `data <= rom.rd[addr];` in ASYNC block | Valid read |
| 2 | OUT SYNC read port -- registered output | `data <= cache.rd[addr];` in SYNC block | Valid read |
| 3 | IN write port -- synchronous write | `ram.wr[addr] <= data;` in SYNC block | Valid write |
| 4 | INOUT SYNC WRITE_FIRST | Write-then-read on same port | Valid |
| 5 | INOUT SYNC READ_FIRST | Read-then-write on same port | Valid |
| 6 | INOUT SYNC NO_CHANGE | Hold output during write | Valid |

### 2.2 Error Cases

| # | Test Case | Input | Expected | Rule ID |
|---|-----------|-------|----------|---------|
| 1 | INOUT addr assigned in ASYNC block | `ram.rw.addr <= val;` in ASYNC | Error | MEM_INOUT_ADDR_IN_ASYNC |
| 2 | ASYNC keyword on INOUT port | `INOUT rw ASYNC;` | Error | MEM_INOUT_ASYNC |
| 3 | INOUT with bracket access | `ram.rw[addr]` | Error | MEM_INOUT_INDEXED |
| 4 | INOUT mixed with separate IN/OUT | `OUT rd; INOUT rw;` | Error | MEM_INOUT_MIXED_WITH_IN_OUT |
| 5 | INOUT wdata assigned in ASYNC block | `ram.rw.wdata <= val;` in ASYNC | Error | MEM_INOUT_WDATA_IN_ASYNC |
| 6 | Multiple writes to same IN port | Two `ram.wr[addr] <= ...` in same block | Error | MEM_MULTIPLE_WRITES_SAME_IN |
| 7 | Read from IN (write) port | `data <= ram.wr[addr];` | Error | MEM_READ_FROM_WRITE_PORT |
| 8 | Write to OUT (read) port | `ram.rd[addr] <= data;` | Error | MEM_WRITE_TO_READ_PORT |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Single IN port only | MEM with only one write port | Valid |
| 2 | Single OUT port only | MEM with only one read port | Valid |
| 3 | INOUT as sole port | MEM with only one INOUT port | Valid |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| 1 | INOUT addr assigned in ASYNC block | Error | MEM_INOUT_ADDR_IN_ASYNC | error | S7.2: INOUT fields must be in SYNC |
| 2 | ASYNC keyword on INOUT port declaration | Error | MEM_INOUT_ASYNC | error | S7.2: INOUT ports cannot be ASYNC |
| 3 | Bracket indexing on INOUT port | Error | MEM_INOUT_INDEXED | error | S7.2: INOUT uses .addr/.wdata/.rdata fields |
| 4 | INOUT port coexists with IN or OUT ports | Error | MEM_INOUT_MIXED_WITH_IN_OUT | error | S7.2: INOUT cannot be mixed with IN/OUT |
| 5 | INOUT wdata assigned in ASYNC block | Error | MEM_INOUT_WDATA_IN_ASYNC | error | S7.2: INOUT fields must be in SYNC |
| 6 | Two writes to the same IN port in one block | Error | MEM_MULTIPLE_WRITES_SAME_IN | error | S7.2: Single write per IN port per cycle |
| 7 | Read access on IN (write-only) port | Error | MEM_READ_FROM_WRITE_PORT | error | S7.2: IN ports are write-only |
| 8 | Write access on OUT (read-only) port | Error | MEM_WRITE_TO_READ_PORT | error | S7.2: OUT ports are read-only |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 7_2_MEM_INOUT_ADDR_IN_ASYNC-addr_in_async.jz | MEM_INOUT_ADDR_IN_ASYNC | INOUT addr field assigned in ASYNC block |
| 7_2_MEM_INOUT_ASYNC-async_keyword_on_inout.jz | MEM_INOUT_ASYNC | ASYNC keyword on INOUT port |
| 7_2_MEM_INOUT_INDEXED-inout_bracket_access.jz | MEM_INOUT_INDEXED | Bracket indexing used on INOUT port |
| 7_2_MEM_INOUT_MIXED_WITH_IN_OUT-mixed_port_types.jz | MEM_INOUT_MIXED_WITH_IN_OUT | INOUT mixed with separate IN/OUT ports |
| 7_2_MEM_INOUT_WDATA_IN_ASYNC-wdata_in_async.jz | MEM_INOUT_WDATA_IN_ASYNC | INOUT wdata field assigned in ASYNC block |
| 7_2_MEM_MULTIPLE_WRITES_SAME_IN-double_write_in_port.jz | MEM_MULTIPLE_WRITES_SAME_IN | Multiple writes to same IN port |
| 7_2_MEM_READ_FROM_WRITE_PORT-read_from_in_port.jz | MEM_READ_FROM_WRITE_PORT | Read from write-only IN port |
| 7_2_MEM_WRITE_TO_READ_PORT-write_to_out_port.jz | MEM_WRITE_TO_READ_PORT | Write to read-only OUT port |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| MEM_INOUT_ADDR_IN_ASYNC | error | S7.2 INOUT addr field assigned in ASYNC block | 7_2_MEM_INOUT_ADDR_IN_ASYNC-addr_in_async.jz |
| MEM_INOUT_ASYNC | error | S7.2 ASYNC keyword on INOUT port declaration | 7_2_MEM_INOUT_ASYNC-async_keyword_on_inout.jz |
| MEM_INOUT_INDEXED | error | S7.2 Bracket indexing on INOUT port | 7_2_MEM_INOUT_INDEXED-inout_bracket_access.jz |
| MEM_INOUT_MIXED_WITH_IN_OUT | error | S7.2 INOUT mixed with separate IN/OUT ports | 7_2_MEM_INOUT_MIXED_WITH_IN_OUT-mixed_port_types.jz |
| MEM_INOUT_WDATA_IN_ASYNC | error | S7.2 INOUT wdata field assigned in ASYNC block | 7_2_MEM_INOUT_WDATA_IN_ASYNC-wdata_in_async.jz |
| MEM_MULTIPLE_WRITES_SAME_IN | error | S7.2 Multiple writes to same IN port in one block | 7_2_MEM_MULTIPLE_WRITES_SAME_IN-double_write_in_port.jz |
| MEM_READ_FROM_WRITE_PORT | error | S7.2 Read from write-only IN port | 7_2_MEM_READ_FROM_WRITE_PORT-read_from_in_port.jz |
| MEM_WRITE_TO_READ_PORT | error | S7.2 Write to read-only OUT port | 7_2_MEM_WRITE_TO_READ_PORT-write_to_out_port.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| (none) | -- | All port type and semantics rules have validation tests |
