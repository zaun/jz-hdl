# Test Plan: 7.4 Write Modes

**Specification Reference:** Section 7.4 of jz-hdl-specification.md

## 1. Objective

Verify WRITE_FIRST, READ_FIRST, and NO_CHANGE write modes for both IN and INOUT ports, including shorthand syntax, attribute form syntax, default behavior when unspecified, behavioral differences during simultaneous read/write, and rejection of invalid mode values.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | IN port default (WRITE_FIRST) | `IN wr;` | Valid, default WRITE_FIRST applied |
| 2 | IN port shorthand WRITE_FIRST | `IN wr WRITE_FIRST;` | Valid |
| 3 | IN port shorthand READ_FIRST | `IN wr READ_FIRST;` | Valid |
| 4 | IN port shorthand NO_CHANGE | `IN wr NO_CHANGE;` | Valid |
| 5 | IN port attribute form | `IN wr { WRITE_MODE = READ_FIRST; };` | Valid |
| 6 | INOUT port default (WRITE_FIRST) | `INOUT rw;` | Valid, default WRITE_FIRST applied |
| 7 | INOUT port shorthand WRITE_FIRST | `INOUT rw WRITE_FIRST;` | Valid |
| 8 | INOUT port shorthand READ_FIRST | `INOUT rw READ_FIRST;` | Valid |
| 9 | INOUT port shorthand NO_CHANGE | `INOUT rw NO_CHANGE;` | Valid |
| 10 | INOUT port attribute form | `INOUT rw { WRITE_MODE = NO_CHANGE; };` | Valid |
| 11 | Semi-Dual Port WRITE_FIRST behavior | IN wr WRITE_FIRST + OUT rd SYNC, same addr read/write | Read sees new data |
| 12 | Semi-Dual Port READ_FIRST behavior | IN wr READ_FIRST + OUT rd SYNC, same addr read/write | Read sees old data |
| 13 | Semi-Dual Port NO_CHANGE behavior | IN wr NO_CHANGE + OUT rd SYNC, same addr read/write | Read holds previous output |
| 14 | INOUT WRITE_FIRST behavior | INOUT rw WRITE_FIRST, .wdata assigned | .data sees new data |
| 15 | INOUT READ_FIRST behavior | INOUT rw READ_FIRST, .wdata assigned | .data sees old data |
| 16 | INOUT NO_CHANGE behavior | INOUT rw NO_CHANGE, .wdata assigned | .data holds previous value |
| 17 | INOUT read-only cycle (no wdata) | INOUT rw, .wdata not assigned | .data reflects stored value regardless of mode |

### 2.2 Error Cases

| # | Test Case | Input | Expected | Rule ID |
|---|-----------|-------|----------|---------|
| 1 | Invalid write mode keyword on IN | `IN wr { WRITE_MODE = BOGUS; };` | Error | MEM_INVALID_WRITE_MODE |
| 2 | Invalid write mode keyword on INOUT | `INOUT rw { WRITE_MODE = INVALID; };` | Error | MEM_INVALID_WRITE_MODE |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Write without simultaneous read | All three modes behave identically when addr_r != addr_w | Valid, no behavioral difference |
| 2 | Default write mode when unspecified | IN/INOUT port without explicit mode | WRITE_FIRST applied |
| 3 | Different write modes on different IN ports | `IN wr_a WRITE_FIRST; IN wr_b READ_FIRST;` | Valid, each port has own mode |

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1 | Unrecognized write mode keyword on IN | `IN wr { WRITE_MODE = BOGUS; };` | MEM_INVALID_WRITE_MODE | error |
| 2 | Unrecognized write mode keyword on INOUT | `INOUT rw { WRITE_MODE = INVALID; };` | MEM_INVALID_WRITE_MODE | error |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 7_4_HAPPY_PATH-write_modes_ok.jz | — | Happy path: all valid write mode declarations (shorthand and attribute forms) |
| 7_0_MEM_INVALID_WRITE_MODE-bad_write_mode_value.jz | MEM_INVALID_WRITE_MODE | Invalid write mode value on port (covered by 7.0 test) |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| MEM_INVALID_WRITE_MODE | error | S7.4/S7.7.2 Invalid write mode value | 7_0_MEM_INVALID_WRITE_MODE-bad_write_mode_value.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| (none) | -- | Write mode validation covered by 7_0 test |
