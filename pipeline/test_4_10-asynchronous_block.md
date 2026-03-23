# Test Plan: 4.10 ASYNCHRONOUS Block (Combinational Logic)

**Specification Reference:** Section 4.10 of jz-hdl-specification.md

## 1. Objective

Verify ASYNCHRONOUS block semantics: assignment forms (alias `=`, drive `=>`, receive `<=`, sliced, concatenation LHS), extension modifiers (`z`/`s` suffix variants), width rules for each operator, net aliasing rules (symmetric, transitive, no conditional alias, no bare literal RHS), directional assignment rules, and interaction with all signal types (WIRE, PORT, REGISTER read-only, LATCH guarded).

## 2. Instrumentation Strategy

- **Span: `sem.async_block`** — Trace ASYNCHRONOUS block analysis; attributes: `assignment_count`, `alias_count`, `directional_count`.
- **Span: `sem.alias_resolve`** — Alias resolution; attributes: `net_groups`, `transitive_chains`.
- **Event: `alias.in_conditional`** — Alias inside IF/ELSE or SELECT.
- **Event: `alias.bare_literal`** — Bare literal on RHS of alias.

## 3. Test Scenarios

### 3.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Net alias | `a = b;` — symmetric merge |
| 2 | Receive assignment | `a <= b;` — b drives a |
| 3 | Drive assignment | `a => b;` — a drives b |
| 4 | Sliced assignment | `a[7:0] = b[7:0];` |
| 5 | Concatenation LHS | `{carry, sum} = expr;` |
| 6 | Zero-extend alias | `wide =z narrow;` |
| 7 | Sign-extend receive | `wide <=s narrow;` |
| 8 | Transitive alias | `a = b; b = c;` → all same net |
| 9 | Ternary in receive | `a <= cond ? b : c;` |
| 10 | Register read in ASYNC | `wire <= register_value;` — read current value |

### 3.2 Boundary/Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Cyclic alias | `a = b; b = c; c = a;` — collapses to one net |
| 2 | Empty ASYNC block | `ASYNCHRONOUS { }` — valid |
| 3 | Extension modifier varieties | All 6: `=z`, `=s`, `<=z`, `<=s`, `=>z`, `=>s` |

### 3.3 Negative Testing

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Alias in conditional | `IF (c) { a = b; }` — Error |
| 2 | Alias bare literal RHS | `data = 1'b1;` — Error |
| 3 | Width mismatch no modifier | `wide <= narrow;` without `z`/`s` — Error |
| 4 | Truncation attempt | `narrow <= wide;` without extension — Error |
| 5 | Register write in ASYNC | `reg <= value;` (next-state) — Error |
| 6 | Assign to IN port | `in_port <= data;` — Error |
| 7 | Read from OUT port | `wire <= out_port;` — Error |

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | `IF(c) { a = b; }` | Error | ALIAS_IN_CONDITIONAL | S4.10 |
| 2 | `data = 1'b1;` | Error | ALIAS_LITERAL_BAN | S4.10 |
| 3 | `wide <= narrow;` (no mod) | Error | WIDTH_ASSIGN_MISMATCH_NO_EXT | S4.10 |
| 4 | `a = b;` | Valid alias | — | Symmetric merge |
| 5 | `a <=z b;` | Valid: zero-extend | — | Extension modifier |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `parser_statements.c` | Parses ASYNCHRONOUS statements | Token stream |
| `driver_assign.c` | Assignment type and context validation | Integration test |
| `driver_net.c` | Net aliasing and resolution | Integration test |
| `driver_width.c` | Width matching and extension | Unit test |
| `diagnostic.c` | Error reporting | Capture diagnostics |

## 6. Rules Matrix

### 6.1 Rules Tested

| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| ALIAS_IN_CONDITIONAL | Alias inside control flow | Neg 1 |
| ALIAS_LITERAL_BAN | Bare literal RHS on alias | Neg 2 |
| WIDTH_ASSIGN_MISMATCH_NO_EXT | Width mismatch without modifier | Neg 3 |
| ASSIGN_OP_WRONG_BLOCK | Register write in ASYNC | Neg 5 |
| PORT_INPUT_DRIVEN_INSIDE | Write to IN port | Neg 6 |
| PORT_DIRECTION_VIOLATION | Read from OUT port | Neg 7 |

### 6.2 Rules Missing

| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| ALIAS_TRUNCATION | S4.10 "truncation is never implicit" | May be covered by width mismatch rule |
