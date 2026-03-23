# Test Plan: 4.7 REGISTER (Storage Elements)

**Specification Reference:** Section 4.7 of jz-hdl-specification.md

## 1. Objective

Verify REGISTER declarations, mandatory reset, x/z prohibition, SYNC-only writes. Confirm that multi-dimensional registers are rejected, missing reset literals are detected, reset literals containing x or z are rejected, reset width mismatches are caught, registers cannot be assigned in ASYNCHRONOUS blocks, and unused/undriven/unsinked registers produce warnings.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Standard register | `counter [8] = 8'h00;` |
| 2 | Read in ASYNC | `output = counter;` in ASYNCHRONOUS |
| 3 | Write in SYNC | `counter <= counter + 8'd1;` in SYNCHRONOUS |
| 4 | GND reset | `data [8] = GND;` -- all zeros |
| 5 | VCC reset | `flags [8] = VCC;` -- all ones |
| 6 | Read current, write next | RHS reads current, LHS schedules next in same SYNC block |
| 7 | Multiple registers | Several registers in one REGISTER block |

### 2.2 Error Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Write in ASYNC | `reg <= data;` in ASYNCHRONOUS -- error |
| 2 | Reset has x | `r [8] = 8'bxxxx_0000;` -- error |
| 3 | Reset has z | `r [8] = 8'bzzzz_zzzz;` -- error |
| 4 | Multi-dimensional | `r [8] [4];` -- error |
| 5 | Missing reset value | `r [8];` -- error |
| 6 | Init width mismatch | Reset literal width does not match declared register width -- error |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | 1-bit register | `flag [1] = 1'b0;` |
| 2 | Wide register | `data [256] = 256'd0;` |
| 3 | Unused register | Declared, never read or written -- warning |
| 4 | Written never read | Register written but value never consumed -- warning |
| 5 | Read never written | Register read but never assigned in SYNC -- warning |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|-----------------|---------|----------|-------|
| 1 | Register assigned in ASYNC block | Error: cannot write REGISTER in ASYNCHRONOUS | ASYNC_ASSIGN_REGISTER | error | S4.7/S5.1 |
| 2 | Multi-dimensional register | Error: REGISTER with multi-dimensional syntax | REG_MULTI_DIMENSIONAL | error | S4.7 |
| 3 | Register without reset literal | Error: missing mandatory reset/power-on literal | REG_MISSING_INIT_LITERAL | error | S4.7 |
| 4 | Reset literal contains x | Error: register init must not contain x bits | REG_INIT_CONTAINS_X | error | S2.1/S4.7 |
| 5 | Reset literal contains z | Error: register init must not contain z bits | REG_INIT_CONTAINS_Z | error | S2.1/S4.7 |
| 6 | Reset literal width mismatch | Error: init width does not match register width | REG_INIT_WIDTH_MISMATCH | error | S4.7 |
| 7 | Register never read or written | Warning: unused register | WARN_UNUSED_REGISTER | warning | S8.3 |
| 8 | Register written but never read | Warning: unsinked register | WARN_UNSINKED_REGISTER | warning | S8.3 |
| 9 | Register read but never written | Warning: undriven register | WARN_UNDRIVEN_REGISTER | warning | S8.3 |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 4_7_ASYNC_ASSIGN_REGISTER-register_in_async.jz | ASYNC_ASSIGN_REGISTER | Cannot write REGISTER in ASYNCHRONOUS block |
| 4_7_REG_INIT_WIDTH_MISMATCH-init_width_mismatch.jz | REG_INIT_WIDTH_MISMATCH | Register initialization literal width does not match declared register width |
| 4_7_WARN_UNDRIVEN_REGISTER-read_never_written.jz | WARN_UNDRIVEN_REGISTER | Register is read but never written |
| 4_7_WARN_UNSINKED_REGISTER-written_never_read.jz | WARN_UNSINKED_REGISTER | Register is written but its value is never read |
| 4_7_WARN_UNUSED_REGISTER-unused_register.jz | WARN_UNUSED_REGISTER | Register is never read or written |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| ASYNC_ASSIGN_REGISTER | error | S4.7/S5.1 Cannot write REGISTER in ASYNCHRONOUS block; move assignment to a SYNCHRONOUS block | 4_7_ASYNC_ASSIGN_REGISTER-register_in_async.jz |
| REG_INIT_WIDTH_MISMATCH | error | S4.7 Register initialization literal width does not match declared register width | 4_7_REG_INIT_WIDTH_MISMATCH-init_width_mismatch.jz |
| WARN_UNUSED_REGISTER | warning | S8.3 Register is never read or written; remove it if unused | 4_7_WARN_UNUSED_REGISTER-unused_register.jz |
| WARN_UNSINKED_REGISTER | warning | S8.3 Register is written but its value is never read | 4_7_WARN_UNSINKED_REGISTER-written_never_read.jz |
| WARN_UNDRIVEN_REGISTER | warning | S8.3 Register is read but never written | 4_7_WARN_UNDRIVEN_REGISTER-read_never_written.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| REG_MULTI_DIMENSIONAL | error | No dedicated 4_7 test file; multi-dimensional register declaration not yet covered |
| REG_MISSING_INIT_LITERAL | error | No dedicated 4_7 test file; missing reset literal not yet covered |
| REG_INIT_CONTAINS_X | error | No dedicated 4_7 test file; x in reset literal not yet covered |
| REG_INIT_CONTAINS_Z | error | No dedicated 4_7 test file; z in reset literal not yet covered |
