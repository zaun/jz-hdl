# Test Plan: 7.7 Error Checking and Validation

**Specification Reference:** Section 7.7 (7.7.1-7.7.3) of jz-hdl-specification.md

## 1. Objective

Verify MEM warning conditions: unused port detection, dead code access detection, and partial initialization warnings.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | All ports accessed | MEM with every declared port used | No warnings |
| 2 | Fully initialized MEM | Init file matches depth exactly | No partial init warning |
| 3 | All access paths reachable | No dead code around MEM accesses | No dead code warning |

### 2.2 Error Cases

| # | Test Case | Input | Expected | Rule ID |
|---|-----------|-------|----------|---------|
| 1 | Port declared never accessed | MEM port declared but never read/written | Warning | MEM_WARN_PORT_NEVER_ACCESSED |
| 2 | Dead code access | MEM access inside unreachable code path | Warning | MEM_WARN_DEAD_CODE_ACCESS |
| 3 | Partial initialization | Init file smaller than MEM depth | Warning | MEM_WARN_PARTIAL_INIT |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Port used in only one branch | MEM port accessed inside conditional | No warning (port is reachable) |
| 2 | Multiple unused ports | MEM with 3 ports, only 1 used | Multiple MEM_WARN_PORT_NEVER_ACCESSED warnings |
| 3 | Dead code after unconditional return | MEM access after always-true guard | Warning: MEM_WARN_DEAD_CODE_ACCESS |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| 1 | Port declared never accessed | Warning | MEM_WARN_PORT_NEVER_ACCESSED | warning | S7.7.3 |
| 2 | MEM access in unreachable path | Warning | MEM_WARN_DEAD_CODE_ACCESS | warning | S7.7.3 |
| 3 | Init file smaller than depth | Warning | MEM_WARN_PARTIAL_INIT | warning | S7.7.3 |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 7_7_MEM_WARN_DEAD_CODE_ACCESS-unreachable_access.jz | MEM_WARN_DEAD_CODE_ACCESS | MEM access inside unreachable code path |
| 7_7_MEM_WARN_PORT_NEVER_ACCESSED-unused_port.jz | MEM_WARN_PORT_NEVER_ACCESSED | MEM port declared but never accessed |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| MEM_WARN_PORT_NEVER_ACCESSED | warning | S7.7.3 Port declared but never accessed | 7_7_MEM_WARN_PORT_NEVER_ACCESSED-unused_port.jz |
| MEM_WARN_DEAD_CODE_ACCESS | warning | S7.7.3 MEM access in unreachable code path | 7_7_MEM_WARN_DEAD_CODE_ACCESS-unreachable_access.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| MEM_WARN_PARTIAL_INIT | warning | No validation test yet for partial initialization warning (also referenced in S7.5) |
