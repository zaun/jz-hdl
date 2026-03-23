# Test Plan: 7.0 Memory Port Modes

**Specification Reference:** Section 7.0 of jz-hdl-specification.md

## 1. Objective

Verify MEM port mode declarations: OUT (read) ports support ASYNC/SYNC modes, IN (write) ports are always SYNC, INOUT ports support write modes (WRITE_FIRST, READ_FIRST, NO_CHANGE), and invalid combinations are rejected.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | OUT ASYNC read port | `OUT rd ASYNC;` | Valid port declaration |
| 2 | OUT SYNC read port | `OUT rd SYNC;` | Valid port declaration |
| 3 | IN write port (implicit SYNC) | `IN wr;` | Valid port declaration |
| 4 | INOUT with WRITE_FIRST | `INOUT rw SYNC WRITE_FIRST;` | Valid port declaration |
| 5 | INOUT with READ_FIRST | `INOUT rw SYNC READ_FIRST;` | Valid port declaration |
| 6 | INOUT with NO_CHANGE | `INOUT rw SYNC NO_CHANGE;` | Valid port declaration |

### 2.2 Error Cases

| # | Test Case | Input | Expected | Rule ID |
|---|-----------|-------|----------|---------|
| 1 | ASYNC/SYNC keyword on IN port | `IN wr ASYNC;` | Error | MEM_INVALID_PORT_TYPE |
| 2 | Invalid write mode value | `INOUT rw SYNC BOGUS;` | Error | MEM_INVALID_WRITE_MODE |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | OUT port without explicit mode | `OUT rd;` | Default mode applied |
| 2 | Multiple ports with mixed modes | `OUT rd1 ASYNC; OUT rd2 SYNC; IN wr;` | All valid |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| 1 | ASYNC or SYNC keyword on IN (write) port | Error | MEM_INVALID_PORT_TYPE | error | S7.0: IN ports are always synchronous |
| 2 | Invalid write mode keyword on INOUT port | Error | MEM_INVALID_WRITE_MODE | error | S7.0: Only WRITE_FIRST, READ_FIRST, NO_CHANGE allowed |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 7_0_MEM_INVALID_PORT_TYPE-async_sync_on_write_port.jz | MEM_INVALID_PORT_TYPE | ASYNC or SYNC keyword applied to an IN (write) port |
| 7_0_MEM_INVALID_WRITE_MODE-bad_write_mode_value.jz | MEM_INVALID_WRITE_MODE | Unrecognized write mode value on INOUT port |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| MEM_INVALID_PORT_TYPE | error | S7.0 ASYNC or SYNC keyword on IN (write) port | 7_0_MEM_INVALID_PORT_TYPE-async_sync_on_write_port.jz |
| MEM_INVALID_WRITE_MODE | error | S7.0 Invalid write mode value on port declaration | 7_0_MEM_INVALID_WRITE_MODE-bad_write_mode_value.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| (none) | -- | All port mode rules have validation tests |
