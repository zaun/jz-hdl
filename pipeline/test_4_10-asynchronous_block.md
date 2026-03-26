# Test Plan: 4.10 Asynchronous Block

**Specification Reference:** Section 4.10 of jz-hdl-specification.md

## 1. Objective

Verify ASYNCHRONOUS block semantics: assignment forms (alias `=`, drive `=>`, receive `<=`, sliced, concatenation LHS), extension modifiers (`z`/`s` suffix variants), width rules for each operator, net aliasing rules (symmetric, transitive, no conditional alias, no bare literal RHS), directional assignment rules, and interaction with all signal types (WIRE, PORT, REGISTER read-only, LATCH guarded).

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Net alias | `a = b;` -- symmetric merge |
| 2 | Receive assignment | `a <= b;` -- b drives a |
| 3 | Drive assignment | `a => b;` -- a drives b |
| 4 | Sliced assignment | `a[7:0] = b[7:0];` |
| 5 | Concatenation LHS | `{carry, sum} = expr;` |
| 6 | Zero-extend alias | `wide =z narrow;` |
| 7 | Sign-extend receive | `wide <=s narrow;` |
| 8 | Transitive alias | `a = b; b = c;` -- all same net |
| 9 | Ternary in receive | `a <= cond ? b : c;` |
| 10 | Register read in ASYNC | `wire <= register_value;` -- read current value |

### 2.2 Error Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Alias in conditional | `IF (c) { a = b; }` -- Error |
| 2 | Alias bare literal RHS | `data = 1'b1;` -- Error |
| 3 | Width mismatch no modifier | `wide <= narrow;` without `z`/`s` -- Error |
| 4 | Truncation attempt | `narrow <= wide;` without extension -- Error |
| 5 | Register write in ASYNC | `reg <= value;` (next-state) -- Error |
| 6 | Assign to IN port | `in_port <= data;` -- Error |
| 7 | Read floating Z net | Reading a net with high-impedance value -- Error |
| 8 | Invalid statement target | Assignment to invalid target in ASYNC -- Error |
| 9 | Undefined path no driver | Execution path leaves wire undriven -- Error |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Cyclic alias | `a = b; b = c; c = a;` -- collapses to one net |
| 2 | Empty ASYNC block | `ASYNCHRONOUS { }` -- valid |
| 3 | Extension modifier varieties | All 6: `=z`, `=s`, `<=z`, `<=s`, `=>z`, `=>s` |

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1 | Alias inside IF/SELECT | `IF(c) { a = b; }` | ASYNC_ALIAS_IN_CONDITIONAL | error |
| 2 | Alias with bare literal RHS | `data = 1'b1;` | ASYNC_ALIAS_LITERAL_RHS | error |
| 3 | Width mismatch without modifier | `wide <= narrow;` (no z/s) | WIDTH_ASSIGN_MISMATCH_NO_EXT | error |
| 4 | Register write in ASYNC | `reg <= value;` in ASYNCHRONOUS | ASYNC_ASSIGN_REGISTER | error |
| 5 | Reading floating Z net | Net driven only by z, read combinationally | ASYNC_FLOATING_Z_READ | error |
| 6 | Invalid statement target | Assignment to non-assignable LHS | ASYNC_INVALID_STATEMENT_TARGET | error |
| 7 | Undriven execution path | Conditional leaves wire undriven | ASYNC_UNDEFINED_PATH_NO_DRIVER | error |
| 8 | Valid alias | `a = b;` symmetric merge | -- | -- (pass) |
| 9 | Valid zero-extend | `a <=z b;` | -- | -- (pass) |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 4_10_HAPPY_PATH-async_block_ok.jz | -- | Happy path: valid ASYNCHRONOUS block with all assignment forms |
| 4_10_ASYNC_ALIAS_IN_CONDITIONAL-alias_in_conditional.jz | ASYNC_ALIAS_IN_CONDITIONAL | Alias `=` inside IF/SELECT is forbidden |
| 4_10_ASYNC_ALIAS_LITERAL_RHS-literal_rhs_in_alias.jz | ASYNC_ALIAS_LITERAL_RHS | Alias RHS must not be a bare literal |
| 4_10_ASYNC_ASSIGN_REGISTER-register_write_in_async.jz | ASYNC_ASSIGN_REGISTER | Cannot write REGISTER in ASYNCHRONOUS block |
| 4_10_ASYNC_FLOATING_Z_READ-floating_z_read.jz | ASYNC_FLOATING_Z_READ | Reading a net with floating z value |
| 4_10_ASYNC_INVALID_STATEMENT_TARGET-invalid_lhs_in_async.jz | ASYNC_INVALID_STATEMENT_TARGET | Invalid assignment target in ASYNCHRONOUS block |
| 4_10_ASYNC_UNDEFINED_PATH_NO_DRIVER-partial_coverage.jz | ASYNC_UNDEFINED_PATH_NO_DRIVER | Execution path leaves wire undriven |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| ASYNC_ALIAS_IN_CONDITIONAL | error | S4.10/S5.3 Alias operator inside conditional control flow is forbidden | 4_10_ASYNC_ALIAS_IN_CONDITIONAL-alias_in_conditional.jz |
| ASYNC_ALIAS_LITERAL_RHS | error | S4.10 RHS of alias operator must not be a bare literal | 4_10_ASYNC_ALIAS_LITERAL_RHS-literal_rhs_in_alias.jz |
| ASYNC_ASSIGN_REGISTER | error | S4.7/S4.10/S5.1 Cannot write REGISTER in ASYNCHRONOUS block | 4_10_ASYNC_ASSIGN_REGISTER-register_write_in_async.jz |
| ASYNC_FLOATING_Z_READ | error | S4.10 Reading a net whose only driver is z (floating) | 4_10_ASYNC_FLOATING_Z_READ-floating_z_read.jz |
| ASYNC_INVALID_STATEMENT_TARGET | error | S4.10 Invalid assignment target in ASYNCHRONOUS block | 4_10_ASYNC_INVALID_STATEMENT_TARGET-invalid_lhs_in_async.jz |
| ASYNC_UNDEFINED_PATH_NO_DRIVER | error | S4.10 Execution path leaves wire or port undriven | 4_10_ASYNC_UNDEFINED_PATH_NO_DRIVER-partial_coverage.jz |
### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| WIDTH_ASSIGN_MISMATCH_NO_EXT | error | Suppressed by ASSIGN_WIDTH_NO_MODIFIER: test exists (`5_0_WIDTH_ASSIGN_MISMATCH_NO_EXT-alias_width_mismatch.jz`) but rule is suppressed |
