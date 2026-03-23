# Test Plan: 4.10 Asynchronous Block

**Specification Reference:** Section 4.10 of jz-hdl-specification.md

## 1. Objective

Verify ASYNCHRONOUS block semantics: assignment forms (alias `=`, drive `=>`, receive `<=`, sliced, concatenation LHS), extension modifiers (`z`/`s` suffix variants), width rules for each operator, net aliasing rules (symmetric, transitive, no conditional alias, no bare literal RHS), directional assignment rules, and interaction with all signal types (WIRE, PORT, REGISTER read-only, LATCH guarded).

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Net alias | `a = b;` — symmetric merge |
| 2 | Receive assignment | `a <= b;` — b drives a |
| 3 | Drive assignment | `a => b;` — a drives b |
| 4 | Sliced assignment | `a[7:0] = b[7:0];` |
| 5 | Concatenation LHS | `{carry, sum} = expr;` |
| 6 | Zero-extend alias | `wide =z narrow;` |
| 7 | Sign-extend receive | `wide <=s narrow;` |
| 8 | Transitive alias | `a = b; b = c;` — all same net |
| 9 | Ternary in receive | `a <= cond ? b : c;` |
| 10 | Register read in ASYNC | `wire <= register_value;` — read current value |

### 2.2 Error Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Alias in conditional | `IF (c) { a = b; }` — Error |
| 2 | Alias bare literal RHS | `data = 1'b1;` — Error |
| 3 | Width mismatch no modifier | `wide <= narrow;` without `z`/`s` — Error |
| 4 | Truncation attempt | `narrow <= wide;` without extension — Error |
| 5 | Register write in ASYNC | `reg <= value;` (next-state) — Error |
| 6 | Assign to IN port | `in_port <= data;` — Error |
| 7 | Read floating Z net | Reading a net with high-impedance value — Error |
| 8 | Invalid statement target | Assignment to invalid target in ASYNC — Error |
| 9 | Undefined path no driver | Execution path leaves wire undriven — Error |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Cyclic alias | `a = b; b = c; c = a;` — collapses to one net |
| 2 | Empty ASYNC block | `ASYNCHRONOUS { }` — valid |
| 3 | Extension modifier varieties | All 6: `=z`, `=s`, `<=z`, `<=s`, `=>z`, `=>s` |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | `IF(c) { a = b; }` | Error | ASYNC_ALIAS_IN_CONDITIONAL | S4.10 |
| 2 | `data = 1'b1;` | Error | ASYNC_ALIAS_LITERAL_RHS | S4.10 |
| 3 | `wide <= narrow;` (no mod) | Error | WIDTH_ASSIGN_MISMATCH_NO_EXT | S4.10 |
| 4 | Register write in ASYNC | Error | ASYNC_ASSIGN_REGISTER | S4.10 |
| 5 | Reading floating Z net | Error | ASYNC_FLOATING_Z_READ | S4.10 |
| 6 | Invalid statement target | Error | ASYNC_INVALID_STATEMENT_TARGET | S4.10 |
| 7 | Undriven execution path | Error | ASYNC_UNDEFINED_PATH_NO_DRIVER | S4.10 |
| 8 | `a = b;` | Valid alias | — | Symmetric merge |
| 9 | `a <=z b;` | Valid: zero-extend | — | Extension modifier |

## 4. Existing Validation Tests

| Test File | Rule Tested |
|-----------|-------------|
| 4_10_ASYNC_ALIAS_LITERAL_RHS-literal_rhs_in_alias.jz | ASYNC_ALIAS_LITERAL_RHS |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Test File(s) |
|---------|----------|--------------|
| ASYNC_ALIAS_LITERAL_RHS | error | 4_10_ASYNC_ALIAS_LITERAL_RHS-literal_rhs_in_alias.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Gap Description |
|---------|----------|-----------------|
| ASYNC_INVALID_STATEMENT_TARGET | error | Assignment to invalid target in ASYNC block |
| ASYNC_ASSIGN_REGISTER | error | Register write attempted in ASYNC block |
| ASYNC_FLOATING_Z_READ | error | Reading a net with high-impedance value |
| ASYNC_ALIAS_IN_CONDITIONAL | error | Alias inside IF/ELSE or SELECT control flow |
| WIDTH_ASSIGN_MISMATCH_NO_EXT | error | Width mismatch without z/s extension modifier |
| ASYNC_UNDEFINED_PATH_NO_DRIVER | error | Execution path leaves wire without a driver |
