# Test Plan: 7.4 Write Modes

**Specification Reference:** Section 7.4 of jz-hdl-specification.md

## 1. Objective

Verify WRITE_FIRST (write-then-read), READ_FIRST (read-then-write), and NO_CHANGE (hold output during write) write modes for INOUT ports, including behavioral differences during simultaneous read/write and rejection of invalid mode values.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | WRITE_FIRST mode | `INOUT rw SYNC WRITE_FIRST;` -- simultaneous read returns new data | Valid, read sees written value |
| 2 | READ_FIRST mode | `INOUT rw SYNC READ_FIRST;` -- simultaneous read returns old data | Valid, read sees previous value |
| 3 | NO_CHANGE mode | `INOUT rw SYNC NO_CHANGE;` -- output holds during write | Valid, output unchanged during write |

### 2.2 Error Cases

| # | Test Case | Input | Expected | Rule ID |
|---|-----------|-------|----------|---------|
| 1 | Invalid write mode keyword | `INOUT rw SYNC BOGUS;` | Error | MEM_INVALID_WRITE_MODE |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Write without simultaneous read | All three modes behave identically | Valid, no behavioral difference |
| 2 | Default write mode when unspecified | INOUT port without explicit mode | Compiler default applied |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| 1 | Unrecognized write mode keyword | Error | MEM_INVALID_WRITE_MODE | error | S7.4: Only WRITE_FIRST, READ_FIRST, NO_CHANGE |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 7_0_MEM_INVALID_WRITE_MODE-bad_write_mode_value.jz | MEM_INVALID_WRITE_MODE | Invalid write mode value on INOUT port (covered by 7.0 test) |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| MEM_INVALID_WRITE_MODE | error | S7.4 Invalid write mode value | 7_0_MEM_INVALID_WRITE_MODE-bad_write_mode_value.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| (none) | -- | Write mode validation covered by 7_0 test |
