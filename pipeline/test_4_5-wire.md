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

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|-----------------|---------|----------|-------|
| 1 | Wire assigned in SYNC block | Error: cannot assign WIRE in SYNCHRONOUS | WRITE_WIRE_IN_SYNC | error | S4.5/S5.2 |
| 2 | Multi-dimensional wire declaration | Error: WIRE with multi-dimensional syntax | WIRE_MULTI_DIMENSIONAL | error | S4.5 |
| 3 | Wire declared but never driven or read | Warning: unused wire | WARN_UNUSED_WIRE | warning | S12.3 |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 4_5_WRITE_WIRE_IN_SYNC-wire_in_sync_block.jz | WRITE_WIRE_IN_SYNC | Cannot assign to WIRE in SYNCHRONOUS block |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| WRITE_WIRE_IN_SYNC | error | S4.5/S5.2 Cannot assign to WIRE in SYNCHRONOUS block; use a REGISTER, or move to ASYNCHRONOUS | 4_5_WRITE_WIRE_IN_SYNC-wire_in_sync_block.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| WIRE_MULTI_DIMENSIONAL | error | No dedicated 4_5 test file; multi-dimensional wire declaration not yet covered |
| WARN_UNUSED_WIRE | warning | No dedicated 4_5 test file; unused wire detection not yet covered |
