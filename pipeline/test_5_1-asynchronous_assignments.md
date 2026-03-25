# Test Plan: 5.1 Asynchronous Assignments

**Specification Reference:** Section 5.1 of jz-hdl-specification.md

## 1. Objective

Verify ASYNC assignment forms (alias `=`, drive `=>`, receive `<=`), literal RHS restriction on aliases, register write prohibition in ASYNC blocks, invalid statement targets, net validation (exactly one active driver per path or valid tri-state), floating-z read detection, and transitive aliasing semantics.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Alias same width | `a = b;` in ASYNC | Valid, nets merged |
| 2 | Drive assignment | `a => b;` in ASYNC | Valid, directional drive |
| 3 | Receive assignment | `a <= b;` in ASYNC | Valid, directional receive |
| 4 | Sliced assignment | `bus[15:8] = word[7:0];` | Valid, partial alias |
| 5 | Concat decomposition | `{carry, sum} = wide_result;` | Valid, decomposition |
| 6 | Register read in ASYNC | `wire <= register_val;` | Valid, registers are readable |
| 7 | Transitive alias | `a = b; c = a;` | Valid, all merged into same net |
| 8 | Sign-extend alias | `extended =s compact;` | Valid, sign-extend |
| 9 | Ternary in receive | `out <= cond ? a : b;` | Valid, conditional expression |
| 10 | Constant drive | `port <= 8'hFF;` | Valid, literal via directional op |

### 2.2 Error Cases

| # | Test Case | Input | Expected | Rule ID |
|---|-----------|-------|----------|---------|
| 1 | Alias literal RHS | `data = 1'b1;` in ASYNC | Error | ASYNC_ALIAS_LITERAL_RHS |
| 2 | Register write in ASYNC | `reg <= value;` in ASYNC | Error | ASYNC_ASSIGN_REGISTER |
| 3 | Invalid LHS target | Memory sync or const on LHS in ASYNC | Error | ASYNC_INVALID_STATEMENT_TARGET |
| 4 | Floating Z read | All drivers assign `z` while net is read | Error | ASYNC_FLOATING_Z_READ |
| 5 | Undriven net in path | Wire without driver in some ASYNC path | Error | ASYNC_UNDEFINED_PATH_NO_DRIVER |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Cyclic alias | `a = b; b = c; c = a;` | Valid, all collapse to same net |
| 2 | Empty ASYNC block | `ASYNCHRONOUS { }` | Valid, no assignments |
| 3 | Multi-path conditional driving | `IF (sel) { a <= x; } ELSE { a <= y; }` | Valid, exactly one driver per path |

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1 | Alias with bare literal RHS in ASYNC | `data = 1'b1;` in ASYNCHRONOUS | ASYNC_ALIAS_LITERAL_RHS | error |
| 2 | Write to register in ASYNC block | `reg <= value;` in ASYNCHRONOUS | ASYNC_ASSIGN_REGISTER | error |
| 3 | Assign to non-assignable target in ASYNC | Memory sync field or const on LHS | ASYNC_INVALID_STATEMENT_TARGET | error |
| 4 | All drivers assign z, net has sinks | Tri-state bus fully released but read | ASYNC_FLOATING_Z_READ | error |
| 5 | Signal undriven on some ASYNC path | Wire with no driver in an IF branch | ASYNC_UNDEFINED_PATH_NO_DRIVER | error |
| 6 | Valid alias same-width | `a = b;` (both 8-bit) in ASYNC | -- | pass |
| 7 | Valid constant drive via receive | `port <= 8'hFF;` in ASYNC | -- | pass |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 5_1_HAPPY_PATH-async_assignments_ok.jz | -- | Valid ASYNC assignment forms accepted |
| 5_1_ASYNC_ALIAS_LITERAL_RHS-alias_literal_rhs.jz | ASYNC_ALIAS_LITERAL_RHS | Alias operator with bare literal RHS in ASYNC |
| 5_1_ASYNC_ASSIGN_REGISTER-register_in_async.jz | ASYNC_ASSIGN_REGISTER | Register write in ASYNC block |
| 5_1_ASYNC_FLOATING_Z_READ-floating_z_read.jz | ASYNC_FLOATING_Z_READ | Net has sinks but all drivers assign z |
| 5_1_ASYNC_INVALID_STATEMENT_TARGET-mem_sync_in_async.jz | ASYNC_INVALID_STATEMENT_TARGET | LHS not assignable in ASYNC block |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| ASYNC_ALIAS_LITERAL_RHS | error | S4.10/S5.1 Alias may not use bare literal RHS in ASYNC | 5_1_ASYNC_ALIAS_LITERAL_RHS-alias_literal_rhs.jz |
| ASYNC_ASSIGN_REGISTER | error | S4.7/S5.1 Cannot write register in ASYNC block | 5_1_ASYNC_ASSIGN_REGISTER-register_in_async.jz |
| ASYNC_FLOATING_Z_READ | error | S4.10/S1.5/S8.1 Net has sinks but all drivers assign z | 5_1_ASYNC_FLOATING_Z_READ-floating_z_read.jz |
| ASYNC_INVALID_STATEMENT_TARGET | error | S4.10/S5.1/S8.1 LHS not assignable in ASYNC | 5_1_ASYNC_INVALID_STATEMENT_TARGET-mem_sync_in_async.jz |
| ASYNC_UNDEFINED_PATH_NO_DRIVER | error | S1.5/S4.10/S5.1 Signal undriven on some ASYNCHRONOUS paths; add an ELSE branch or DEFAULT case | 1_5_ASYNC_UNDEFINED_PATH_NO_DRIVER-partial_coverage.jz, 4_10_ASYNC_UNDEFINED_PATH_NO_DRIVER-partial_coverage.jz |

### 5.2 Rules Not Tested (in this section)

All rules for this section are tested.
