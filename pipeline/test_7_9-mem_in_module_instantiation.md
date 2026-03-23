# Test Plan: 7.9 MEM in Module Instantiation

**Specification Reference:** Section 7.9 of jz-hdl-specification.md

## 1. Objective

Verify MEM access scoping across module hierarchy: MEM declared in a child module cannot be directly accessed from the parent, and must be accessed through ports.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Module with MEM instantiated normally | Parent instantiates child containing MEM, accesses via ports | Valid, compiles without errors |
| 2 | OVERRIDE changes MEM depth via CONST | Parent overrides CONFIG that controls child MEM depth | Valid, MEM resized correctly |
| 3 | Multiple instances of MEM-containing module | Two instances of same MEM module, independent memories | Valid, no conflicts |

### 2.2 Error Cases

| # | Test Case | Input | Expected | Rule ID |
|---|-----------|-------|----------|---------|
| 1 | Direct access to child MEM from parent | Parent references `child.mem_name` directly | Error | UNDECLARED_IDENTIFIER |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Nested instantiation with MEM | Grandchild module contains MEM, parent accesses via chain | Valid, each level uses ports |
| 2 | Same MEM name in parent and child | Both modules declare MEM with same name | Valid, scoped independently |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| 1 | Parent accesses child MEM directly | Error | UNDECLARED_IDENTIFIER | error | S7.9: MEM scoped to declaring module |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 7_9_UNDECLARED_IDENTIFIER-child_mem_access_from_parent.jz | UNDECLARED_IDENTIFIER | Parent attempts direct access to child module MEM |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| UNDECLARED_IDENTIFIER | error | S7.9 Direct access to child MEM from parent scope | 7_9_UNDECLARED_IDENTIFIER-child_mem_access_from_parent.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| — | — | All expected rules covered |
