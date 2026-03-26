# Test Plan: 7.2 Port Types and Semantics

**Specification Reference:** Section 7.2 of jz-hdl-specification.md

## 1. Objective

Verify MEM port types (OUT/IN/INOUT) and their read/write semantics, including direction enforcement (write-to-read / read-from-write), INOUT restrictions (no bracket indexing, no ASYNC keyword, no field assignment in ASYNC blocks), multiple-write detection, and port count rules.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | OUT ASYNC read port -- combinational read | `data = rom.rd[addr];` in ASYNC block | Valid read |
| 2 | OUT SYNC read port -- registered output | `cache.rd.addr <= addr; data <= cache.rd.data;` in SYNC block | Valid read |
| 3 | IN write port -- synchronous write | `ram.wr[addr] <= data;` in SYNC block | Valid write |
| 4 | INOUT -- addr, data, wdata access | `ram.rw.addr <= a; out <= ram.rw.data; ram.rw.wdata <= d;` in SYNC | Valid |
| 5 | INOUT read-only cycle (no wdata) | `ram.rw.addr <= a; out <= ram.rw.data;` with no wdata assignment | Valid, no write occurs |
| 6 | Multiple OUT ports (any mix of ASYNC/SYNC) | `OUT rd_a ASYNC; OUT rd_b SYNC;` | Valid |
| 7 | Multiple IN ports (all synchronous) | `IN wr_a; IN wr_b;` | Valid |
| 8 | Multiple INOUT ports | `INOUT port_a; INOUT port_b;` | Valid (Dual Port mode) |
| 9 | INOUT .data readable in ASYNC block | `out = ram.rw.data;` in ASYNC block | Valid (previous cycle's data) |
| 10 | Conditional write via IF | `IF (en) { ram.wr[addr] <= data; }` | Valid, exactly one path |

### 2.2 Error Cases

| # | Test Case | Input | Expected | Rule ID |
|---|-----------|-------|----------|---------|
| 1 | INOUT addr assigned in ASYNC block | `ram.rw.addr <= val;` in ASYNC | Error | MEM_INOUT_ADDR_IN_ASYNC |
| 2 | ASYNC keyword on INOUT port | `INOUT rw ASYNC;` | Error | MEM_INOUT_ASYNC |
| 3 | INOUT with bracket access | `ram.rw[addr]` | Error | MEM_INOUT_INDEXED |
| 4 | INOUT mixed with separate IN/OUT | `OUT rd SYNC; INOUT rw;` | Error | MEM_INOUT_MIXED_WITH_IN_OUT |
| 5 | INOUT wdata assigned in ASYNC block | `ram.rw.wdata <= val;` in ASYNC | Error | MEM_INOUT_WDATA_IN_ASYNC |
| 6 | Multiple writes to same IN port | Two `ram.wr[addr] <= ...` in same block | Error | MEM_MULTIPLE_WRITES_SAME_IN |
| 7 | Read from IN (write) port | `data <= ram.wr[addr];` | Error | MEM_READ_FROM_WRITE_PORT |
| 8 | Write to OUT (read) port | `ram.rd[addr] <= data;` | Error | MEM_WRITE_TO_READ_PORT |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Single IN port only | MEM with only one write port | Valid (no read port) |
| 2 | Single OUT port only (ROM) | MEM with only one read port | Valid |
| 3 | INOUT as sole port | MEM with only one INOUT port | Valid (Single Port mode) |
| 4 | Write to IN port inside IF/SELECT | Conditional write counts as single write | Valid |

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1 | INOUT addr assigned in ASYNC block | `ram.rw.addr <= val;` in ASYNC | MEM_INOUT_ADDR_IN_ASYNC | error |
| 2 | ASYNC keyword on INOUT port declaration | `INOUT rw ASYNC;` | MEM_INOUT_ASYNC | error |
| 3 | Bracket indexing on INOUT port | `ram.rw[addr]` | MEM_INOUT_INDEXED | error |
| 4 | INOUT port coexists with IN or OUT ports | `OUT rd SYNC; INOUT rw;` | MEM_INOUT_MIXED_WITH_IN_OUT | error |
| 5 | INOUT wdata assigned in ASYNC block | `ram.rw.wdata <= val;` in ASYNC | MEM_INOUT_WDATA_IN_ASYNC | error |
| 6 | Two writes to the same IN port in one block | `ram.wr[a] <= x; ram.wr[b] <= y;` | MEM_MULTIPLE_WRITES_SAME_IN | error |
| 7 | Read access on IN (write-only) port | `data <= ram.wr[addr];` | MEM_READ_FROM_WRITE_PORT | error |
| 8 | Write access on OUT (read-only) port | `ram.rd[addr] <= data;` | MEM_WRITE_TO_READ_PORT | error |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 7_2_HAPPY_PATH-port_types_ok.jz | — | Happy path: valid port type usage |
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
| MEM_INOUT_ADDR_IN_ASYNC | error | S7.2.3 INOUT addr field assigned in ASYNC block | 7_2_MEM_INOUT_ADDR_IN_ASYNC-addr_in_async.jz |
| MEM_INOUT_ASYNC | error | S7.1 ASYNC keyword on INOUT port declaration | 7_2_MEM_INOUT_ASYNC-async_keyword_on_inout.jz |
| MEM_INOUT_INDEXED | error | S7.2.3 Bracket indexing on INOUT port | 7_2_MEM_INOUT_INDEXED-inout_bracket_access.jz |
| MEM_INOUT_MIXED_WITH_IN_OUT | error | S7.1 INOUT mixed with separate IN/OUT ports | 7_2_MEM_INOUT_MIXED_WITH_IN_OUT-mixed_port_types.jz |
| MEM_INOUT_WDATA_IN_ASYNC | error | S7.2.3 INOUT wdata field assigned in ASYNC block | 7_2_MEM_INOUT_WDATA_IN_ASYNC-wdata_in_async.jz |
| MEM_MULTIPLE_WRITES_SAME_IN | error | S7.2.2/S7.3.3/S7.7.2 Multiple writes to same IN port in one block | 7_2_MEM_MULTIPLE_WRITES_SAME_IN-double_write_in_port.jz |
| MEM_READ_FROM_WRITE_PORT | error | S7.2.2/S7.3.3 Read from write-only IN port | 7_2_MEM_READ_FROM_WRITE_PORT-read_from_in_port.jz |
| MEM_WRITE_TO_READ_PORT | error | S7.2.1/S7.3.2 Write to read-only OUT port | 7_2_MEM_WRITE_TO_READ_PORT-write_to_out_port.jz |

### 5.2 Rules Not Tested


All rules for this section are tested.
