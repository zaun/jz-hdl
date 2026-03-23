# Test Plan: 4.5 WIRE (Intermediate Nets)

**Specification Reference:** Section 4.5 of jz-hdl-specification.md

## 1. Objective

Verify WIRE declaration syntax, single-dimensional constraint, requirement for exactly one active driver, restriction to ASYNCHRONOUS-only assignment, and detection of undriven/unused wires.

## 2. Instrumentation Strategy

- **Span: `sem.wire_check`** — Trace wire validation; attributes: `wire_name`, `width`, `driver_count`, `read_count`.
- **Event: `wire.undriven`** — Wire declared but never assigned.
- **Event: `wire.unused`** — Wire assigned but never read.

## 3. Test Scenarios

### 3.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Standard wire | `WIRE { result [8]; }` — used in ASYNCHRONOUS |
| 2 | Wire as intermediate | Computed in ASYNC, read in SYNC and ASYNC |
| 3 | Multiple wires | Several wires declared and used |
| 4 | Wire driving OUT port | `result = expr; port <= result;` |

### 3.2 Boundary/Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | 1-bit wire | `WIRE { flag [1]; }` |
| 2 | Wide wire | `WIRE { bus [256]; }` |

### 3.3 Negative Testing

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Wire in SYNCHRONOUS | `wire_name <= value;` in SYNC block — Error |
| 2 | Multi-dimensional wire | `WIRE { arr [8] [4]; }` — Error |
| 3 | Undriven wire | Wire declared but never assigned — Warning |
| 4 | Unused wire | Wire assigned but never read — Warning |
| 5 | Multiple drivers | Wire driven by two assignments in same path — Error |

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Wire assigned in SYNC | Error | ASSIGN_OP_WRONG_BLOCK | Wires: ASYNC only |
| 2 | Multi-dim wire | Error | — | Parse error |
| 3 | Declared never driven | Warning | WARN_UNUSED_WIRE | S12.3 |
| 4 | Driven never read | Warning | WARN_UNUSED_WIRE | S12.3 |
| 5 | Two drivers same path | Error | ASSIGN_MULTI_DRIVER | S1.5 |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `parser_wire.c` | Parses WIRE declarations | Token stream |
| `driver_net.c` | Net/driver analysis | Integration test |
| `driver_assign.c` | Assignment context check | Verify ASYNC-only |
| `diagnostic.c` | Warning/error collection | Capture diagnostics |

## 6. Rules Matrix

### 6.1 Rules Tested

| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| ASSIGN_OP_WRONG_BLOCK | Wire assigned in wrong block type | Neg 1 |
| WARN_UNUSED_WIRE | Wire declared but unused | Neg 3, 4 |
| ASSIGN_MULTI_DRIVER | Multiple drivers on wire | Neg 5 |

### 6.2 Rules Missing

| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| WIRE_MULTI_DIMENSIONAL | S4.5 "multi-dimensional syntax is compile error" | May be parse error rather than semantic rule |
