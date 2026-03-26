# Test Plan: 4.5 WIRE (Intermediate Nets)

**Specification Reference:** Section 4.5 of jz-hdl-specification.md

## 1. Objective

Verify WIRE declarations, single-dimensional constraint, ASYNC-only assignment. Confirm that multi-dimensional wires are rejected, writing to a wire in a SYNCHRONOUS block is an error, and unused wires produce a warning.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Standard wire | `WIRE { result [8]; }` -- used in ASYNCHRONOUS |
| 2 | Wire as intermediate | Computed in ASYNC, read in SYNC and ASYNC |
| 3 | Multiple wires | Several wires declared and used |
| 4 | Wire driving OUT port | `result = expr; port <= result;` |
| 5 | 1-bit wire | `WIRE { flag [1]; }` -- minimum width |

### 2.2 Error Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Wire in SYNCHRONOUS | `wire_name <= value;` in SYNC block -- error |
| 2 | Multi-dimensional wire | `WIRE { arr [8] [4]; }` -- error |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Wide wire | `WIRE { bus [256]; }` -- large width |
| 2 | Wire declared but never used | Declared, never driven or read -- warning |

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1 | Wire assigned in SYNC block | `wire <= data;` in SYNCHRONOUS | WRITE_WIRE_IN_SYNC | error |
| 2 | Multi-dimensional wire declaration | `WIRE { arr [8] [4]; }` | WIRE_MULTI_DIMENSIONAL | error |
| 3 | Wire declared but never driven or read | Unused wire declaration | WARN_UNUSED_WIRE | warning |
| 4 | Valid wire usage | Wire driven in ASYNC, read elsewhere | -- | -- (pass) |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 4_5_WIRE_HAPPY_PATH-valid_wire_ok.jz | -- | Happy path: valid wire declarations and usage |
| 4_5_WIRE_MULTI_DIMENSIONAL-multi_dim_helper.jz | WIRE_MULTI_DIMENSIONAL | WIRE declared with multi-dimensional syntax (helper module) |
| 4_5_WIRE_MULTI_DIMENSIONAL-multi_dim_top.jz | WIRE_MULTI_DIMENSIONAL | WIRE declared with multi-dimensional syntax (top module) |
| 4_5_WRITE_WIRE_IN_SYNC-wire_in_sync_block.jz | WRITE_WIRE_IN_SYNC | Cannot assign to WIRE in SYNCHRONOUS block |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| WRITE_WIRE_IN_SYNC | error | S4.5/S5.2 Cannot assign to WIRE in SYNCHRONOUS block; use a REGISTER, or move to ASYNCHRONOUS | 4_5_WRITE_WIRE_IN_SYNC-wire_in_sync_block.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| WIRE_MULTI_DIMENSIONAL | error | Dead code: test exists (`4_5_WIRE_MULTI_DIMENSIONAL-multi_dim_helper.jz`) but rule is dead code |
