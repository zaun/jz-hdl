# Test Plan: 5.1 Asynchronous Assignments

**Specification Reference:** Section 5.1 of jz-hdl-specification.md

## 1. Objective

Verify ASYNC assignment forms (alias `=`, drive `=>`, receive `<=`), literal RHS restriction on aliases, register write prohibition in ASYNC blocks, net validation (exactly one active driver per path or valid tri-state), and transitive aliasing semantics.

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
| 3 | Invalid LHS target | `CONST = expr;` in ASYNC | Error | ASYNC_INVALID_STATEMENT_TARGET |
| 4 | Floating Z read | All drivers assign `z` while net is read | Error | ASYNC_FLOATING_Z_READ |
| 5 | Undriven net in path | Wire without driver in some ASYNC path | Error | ASYNC_UNDEFINED_PATH_NO_DRIVER |
| 6 | Width mismatch no modifier | `wide = narrow;` in ASYNC | Error | WIDTH_ASSIGN_MISMATCH_NO_EXT |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Cyclic alias | `a = b; b = c; c = a;` | Valid, all collapse to same net |
| 2 | Empty ASYNC block | `ASYNCHRONOUS { }` | Valid, no assignments |
| 3 | Multi-path conditional driving | `IF (sel) { a <= x; } ELSE { a <= y; }` | Valid, exactly one driver per path |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| 1 | `data = 1'b1;` in ASYNC | Compile error | ASYNC_ALIAS_LITERAL_RHS | error | Alias cannot have literal RHS |
| 2 | Register write in ASYNC | Compile error | ASYNC_ASSIGN_REGISTER | error | Registers are read-only in ASYNC |
| 3 | CONST on LHS in ASYNC | Compile error | ASYNC_INVALID_STATEMENT_TARGET | error | LHS must be assignable signal |
| 4 | All drivers assign `z`, net read | Compile error | ASYNC_FLOATING_Z_READ | error | Tri-state bus fully released |
| 5 | Wire undriven in some path | Compile error | ASYNC_UNDEFINED_PATH_NO_DRIVER | error | Signal must be driven on all paths |
| 6 | `wide = narrow;` (no modifier) | Compile error | WIDTH_ASSIGN_MISMATCH_NO_EXT | error | Width mismatch requires modifier |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 4_10_ASYNC_ALIAS_LITERAL_RHS-literal_rhs_in_alias.jz | ASYNC_ALIAS_LITERAL_RHS | Literal on RHS of alias `=` in ASYNC block (4_10 prefix, covers 5.1 rule) |
| 1_5_ASYNC_UNDEFINED_PATH_NO_DRIVER-partial_coverage.jz | ASYNC_UNDEFINED_PATH_NO_DRIVER | Signal undriven on some ASYNC paths (1_5 prefix, covers 5.1 rule) |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| ASYNC_ALIAS_LITERAL_RHS | error | S4.10/S5.1 Literal on RHS of `=` in ASYNCHRONOUS block | 4_10_ASYNC_ALIAS_LITERAL_RHS-literal_rhs_in_alias.jz |
| ASYNC_UNDEFINED_PATH_NO_DRIVER | error | S1.5/S4.10/S5.1 Signal undriven on some ASYNCHRONOUS paths | 1_5_ASYNC_UNDEFINED_PATH_NO_DRIVER-partial_coverage.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| ASYNC_INVALID_STATEMENT_TARGET | error | No dedicated validation test for non-assignable LHS in ASYNC |
| ASYNC_ASSIGN_REGISTER | error | No dedicated validation test for register write in ASYNC block |
| ASYNC_FLOATING_Z_READ | error | No dedicated validation test for floating tri-state read |
| WIDTH_ASSIGN_MISMATCH_NO_EXT | error | Covered by broader width tests, no 5_1-prefixed test |
