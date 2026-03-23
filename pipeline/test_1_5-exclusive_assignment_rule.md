# Test Plan: 1.5 The Exclusive Assignment Rule

**Specification Reference:** Section 1.5 (1.5.1–1.5.4) of jz-hdl-specification.md

## 1. Objective

Verify that the compiler enforces the Exclusive Assignment Rule: for any assignable identifier, every execution path through a block contains zero or one assignment to every bit. This covers the formal definition (1.5.1), Path-Exclusive Determinism analysis (1.5.2), register vs. combinational zero-assignment semantics (1.5.3), and the scope boundary with tri-state nets (1.5.4).

## 2. Instrumentation Strategy

- **Span: `sem.exclusive_assignment`** — Trace the exclusive assignment check per block; attributes: `block.type` (SYNC/ASYNC), `identifier`, `path_count`.
- **Span: `sem.ped_analysis`** — Path-Exclusive Determinism flow analysis; attributes: `nesting_depth`, `branch_count`, `conflict_detected`.
- **Event: `assign.multi_driver`** — Fires when two assignments target same bits in same path.
- **Event: `assign.shadow_outer`** — Fires when nested assignment shadows outer assignment.
- **Event: `assign.independent_chain_conflict`** — Fires when independent IF/SELECT blocks both assign same identifier.
- **Coverage Hook:** Ensure all three PED violation types (independent chain, sequential shadow, multi-driver) are exercised.

## 3. Test Scenarios

### 3.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Single assignment per path | `ASYNCHRONOUS { w <= a; }` — one assignment to `w` |
| 2 | Branch-exclusive IF/ELSE | `IF (c) { w <= a; } ELSE { w <= b; }` — mutually exclusive paths |
| 3 | Branch-exclusive SELECT | `SELECT (sel) { CASE 2'b00: w <= a; CASE 2'b01: w <= b; DEFAULT: w <= c; }` |
| 4 | Register zero-path (clock gating) | `SYNCHRONOUS { IF (en) { reg <= val; } }` — zero-assignment path holds value |
| 5 | Different identifiers same path | `w1 <= a; w2 <= b;` — different targets, no conflict |
| 6 | Bit-disjoint sliced assignments | `w[7:4] <= a; w[3:0] <= b;` — non-overlapping bits |
| 7 | Nested IF with exclusive branches | `IF (a) { IF (b) { w <= x; } ELSE { w <= y; } } ELSE { w <= z; }` |

### 3.2 Boundary/Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Deeply nested branches (10 levels) | Verify PED analysis handles deep nesting |
| 2 | Many branches in SELECT | SELECT with 256 CASE arms, all assigning same wire |
| 3 | Single-bit wire | One bit assigned in IF/ELSE |
| 4 | Wide wire (256-bit) | Full-width assignment in exclusive branches |
| 5 | Combinational with z in some paths | `IF (en) { w <= data; } ELSE { w <= 8'bzzzz_zzzz; }` — valid tri-state |
| 6 | All paths assign same value | `IF (c) { w <= GND; } ELSE { w <= GND; }` — valid, not a conflict |

### 3.3 Negative Testing

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Double assignment same path | `w <= a; w <= b;` — two assignments to same wire |
| 2 | Independent chain conflict | `IF (a) { w <= x; } IF (b) { w <= y; }` — two independent IFs |
| 3 | Sequential shadow | `w <= a; IF (c) { w <= b; }` — outer assignment shadowed by inner |
| 4 | Overlapping bit slices | `w[7:4] <= a; w[5:2] <= b;` — bits 4-5 overlap |
| 5 | ASYNC partial coverage | `IF (c) { w <= a; }` — wire undriven when `c` is false |
| 6 | Register written in wrong block | Register assigned in ASYNCHRONOUS block |
| 7 | Wire assigned in SYNCHRONOUS | Wire assigned in sequential block |
| 8 | Dead write followed by live write | `w <= a; w <= b;` — first assignment unreachable |

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | `w <= a; w <= b;` in ASYNC | Error: multi-driver | ASSIGN_MULTI_DRIVER | Same path, same identifier |
| 2 | Two independent IFs assigning `w` | Error: independent chain | ASSIGN_INDEPENDENT_CHAIN_CONFLICT | PED violation type 1 |
| 3 | `w <= a; IF(c) { w <= b; }` | Error: shadow | ASSIGN_SHADOW_OUTER | PED violation type 2 |
| 4 | `IF(c) { w <= a; } ELSE { w <= b; }` | Valid | — | Branch-exclusive |
| 5 | `IF(c) { w <= a; }` in ASYNC | Error: partial coverage | ASSIGN_PARTIAL_COVERAGE | Undriven in else path |
| 6 | `IF(en) { reg <= val; }` in SYNC | Valid | — | Clock gating (register holds) |
| 7 | `w[7:4] <= a; w[5:2] <= b;` | Error: overlapping bits | ASSIGN_MULTI_DRIVER | Overlapping slices |
| 8 | `w <= a; w <= b;` (dead write) | Error/warning | ASSIGN_UNREACHABLE_DEAD_WRITE | First write is dead |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `driver_assign.c` | Exclusive assignment analysis | Feed crafted AST blocks; verify diagnostics |
| `driver_flow.c` | Execution path enumeration | Unit test with IF/ELSE/SELECT structures |
| `driver_control.c` | Control flow analysis | Verify path merging and branching |
| `driver.c` | Orchestrates all semantic checks | Integration test with full parse → analyze pipeline |
| `parser_statements.c` | Produces assignment AST nodes | Input for semantic analysis |
| `diagnostic.c` | Error collection | Verify correct rule IDs emitted |

## 6. Rules Matrix

### 6.1 Rules Tested

| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| ASSIGN_MULTI_DRIVER | Multiple assignments to same bits in same path | Neg 1, 4 |
| ASSIGN_SHADOW_OUTER | Nested assignment shadows outer assignment | Neg 3 |
| ASSIGN_INDEPENDENT_CHAIN_CONFLICT | Independent chains assign same identifier | Neg 2 |
| ASSIGN_PARTIAL_COVERAGE | Net driven in some paths but not others (ASYNC) | Neg 5 |
| ASSIGN_UNREACHABLE_DEAD_WRITE | Dead write detected | Neg 8 |

### 6.2 Rules Missing

| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| ASSIGN_OVERLAPPING_SLICES | S1.5.1 "every bit" | No distinct rule for overlapping slice assignments; may be covered by ASSIGN_MULTI_DRIVER |
| ASSIGN_REG_IN_ASYNC | S1.5.3 | Register written in ASYNCHRONOUS block; may be covered by ASSIGN_OP_WRONG_BLOCK |
| ASSIGN_WIRE_IN_SYNC | S1.5.3 | Wire written in SYNCHRONOUS block; may be covered by ASSIGN_OP_WRONG_BLOCK |
