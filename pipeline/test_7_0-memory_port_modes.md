# Test Plan: 7.0 Memory Port Modes

**Specification Reference:** Section 7.0 of jz-hdl-specification.md

## 1. Objective

Verify MEM port mode declarations: OUT (read) ports support ASYNC/SYNC modes, IN (write) ports are always SYNC and support write modes (WRITE_FIRST, READ_FIRST, NO_CHANGE), INOUT ports support write modes, and invalid combinations are rejected. Validate that the compiler correctly maps port configurations to BSRAM modes (Read Only, Single Port, Semi-Dual Port, Dual Port).

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | OUT ASYNC read port | `OUT rd ASYNC;` | Valid port declaration |
| 2 | OUT SYNC read port | `OUT rd SYNC;` | Valid port declaration |
| 3 | IN write port (implicit SYNC) | `IN wr;` | Valid port declaration |
| 4 | IN write port with WRITE_FIRST | `IN wr WRITE_FIRST;` | Valid port declaration |
| 5 | IN write port with READ_FIRST | `IN wr READ_FIRST;` | Valid port declaration |
| 6 | IN write port with NO_CHANGE | `IN wr NO_CHANGE;` | Valid port declaration |
| 7 | INOUT with default (WRITE_FIRST) | `INOUT rw;` | Valid port declaration |
| 8 | INOUT with WRITE_FIRST | `INOUT rw WRITE_FIRST;` | Valid port declaration |
| 9 | INOUT with READ_FIRST | `INOUT rw READ_FIRST;` | Valid port declaration |
| 10 | INOUT with NO_CHANGE | `INOUT rw NO_CHANGE;` | Valid port declaration |
| 11 | OUT-only configuration (ROM) | MEM with only OUT ports | Valid, maps to Read Only Memory mode |
| 12 | INOUT x1 configuration | MEM with single INOUT port | Valid, maps to Single Port mode |
| 13 | IN + OUT configuration | MEM with IN and OUT ports | Valid, maps to Semi-Dual Port mode |
| 14 | INOUT x2 configuration | MEM with two INOUT ports | Valid, maps to Dual Port mode |
| 15 | IN write mode via attribute form | `IN wr { WRITE_MODE = READ_FIRST; };` | Valid port declaration |
| 16 | INOUT write mode via attribute form | `INOUT rw { WRITE_MODE = NO_CHANGE; };` | Valid port declaration |

### 2.2 Error Cases

| # | Test Case | Input | Expected | Rule ID |
|---|-----------|-------|----------|---------|
| 1 | ASYNC/SYNC keyword on IN port | `IN wr ASYNC;` | Error | MEM_INVALID_PORT_TYPE |
| 2 | Invalid write mode value on INOUT | `INOUT rw { WRITE_MODE = BOGUS; };` | Error | MEM_INVALID_WRITE_MODE |
| 3 | Invalid write mode value on IN | `IN wr { WRITE_MODE = INVALID; };` | Error | MEM_INVALID_WRITE_MODE |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | OUT port without explicit mode | `OUT rd;` | Default mode applied (compiler-dependent) |
| 2 | Multiple ports with mixed modes | `OUT rd1 ASYNC; OUT rd2 SYNC; IN wr;` | All valid |
| 3 | Multiple IN ports with different write modes | `IN wr_a WRITE_FIRST; IN wr_b READ_FIRST;` | All valid |

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1 | ASYNC or SYNC keyword on IN (write) port | `IN wr ASYNC;` | MEM_INVALID_PORT_TYPE | error |
| 2 | Invalid write mode keyword on INOUT port | `INOUT rw { WRITE_MODE = BOGUS; };` | MEM_INVALID_WRITE_MODE | error |
| 3 | Invalid write mode keyword on IN port | `IN wr { WRITE_MODE = INVALID; };` | MEM_INVALID_WRITE_MODE | error |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 7_0_HAPPY_PATH-mem_port_modes_ok.jz | — | Happy path: all valid port mode declarations |
| 7_0_MEM_INVALID_PORT_TYPE-async_sync_on_write_port.jz | MEM_INVALID_PORT_TYPE | ASYNC or SYNC keyword applied to an IN (write) port |
| 7_0_MEM_INVALID_WRITE_MODE-bad_write_mode_value.jz | MEM_INVALID_WRITE_MODE | Unrecognized write mode value on port |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| MEM_INVALID_PORT_TYPE | error | S7.0 ASYNC or SYNC keyword on IN (write) port | 7_0_MEM_INVALID_PORT_TYPE-async_sync_on_write_port.jz |
| MEM_INVALID_WRITE_MODE | error | S7.4/S7.7.2 Invalid write mode value on port declaration | 7_0_MEM_INVALID_WRITE_MODE-bad_write_mode_value.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| (none) | -- | All port mode rules have validation tests |
