# Test Plan: 5.1 ASYNCHRONOUS Assignments (Combinational)

**Specification Reference:** Section 5.1 of jz-hdl-specification.md

## 1. Objective

Verify all ASYNCHRONOUS assignment forms (alias, drive, receive, sliced, concatenation LHS), literal RHS restriction on aliases, register read-only constraint, net validation (flow-sensitive: exactly one active driver per path, or valid tri-state), and transitive aliasing semantics.

## 2. Instrumentation Strategy

- **Span: `sem.async_assign`** — Per-assignment validation; attributes: `form`, `operator`, `lhs_type`, `rhs_type`.
- **Span: `sem.net_validate`** — Net driver validation; attributes: `net_name`, `driver_count`, `path_count`.
- **Event: `async.alias_literal`** — Bare literal on alias RHS.
- **Event: `async.reg_write`** — Register assignment in ASYNC.

## 3. Test Scenarios

### 3.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Alias same width | `a = b;` |
| 2 | Drive assignment | `a => b;` |
| 3 | Receive assignment | `a <= b;` |
| 4 | Sliced assignment | `bus[15:8] = word[7:0];` |
| 5 | Concat decomposition | `{carry, sum} = wide_result;` |
| 6 | Register read | `wire <= register_val;` |
| 7 | Transitive alias | `a = b; c = a;` — all merged |
| 8 | Sign-extend alias | `extended =s compact;` |
| 9 | Ternary in receive | `out <= cond ? a : b;` |
| 10 | Constant drive | `port <= 8'hFF;` — literal via directional op |

### 3.2 Boundary/Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Cyclic alias | `a = b; b = c; c = a;` — collapses |
| 2 | Empty ASYNC | Valid |

### 3.3 Negative Testing

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Alias literal RHS | `data = 1'b1;` — Error |
| 2 | Register write | `reg <= value;` in ASYNC — Error |
| 3 | Undriven net | Wire without driver in some path — Error |
| 4 | Multi-driver net | Two assignments same wire same path — Error |

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | `data = 1'b1;` in ASYNC | Error | ALIAS_LITERAL_BAN | S5.1 |
| 2 | Reg write in ASYNC | Error | ASSIGN_OP_WRONG_BLOCK | S5.1 |
| 3 | Undriven wire in path | Error | ASSIGN_PARTIAL_COVERAGE | S1.5.3 |
| 4 | Multi-driver | Error | ASSIGN_MULTI_DRIVER | S1.5 |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `driver_assign.c` | Assignment validation | Integration test |
| `driver_net.c` | Net resolution and driver analysis | Integration test |
| `driver_flow.c` | Flow-sensitive path analysis | Integration test |
| `parser_statements.c` | Statement parsing | Token stream |

## 6. Rules Matrix

### 6.1 Rules Tested

| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| ALIAS_LITERAL_BAN | Bare literal RHS on alias in ASYNC | Neg 1 |
| ASSIGN_OP_WRONG_BLOCK | Register write in ASYNC | Neg 2 |
| ASSIGN_PARTIAL_COVERAGE | Undriven net in some path | Neg 3 |
| ASSIGN_MULTI_DRIVER | Multiple drivers same path | Neg 4 |

### 6.2 Rules Missing

| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| — | All expected rules covered | — |
