# Test Plan: 7.9 MEM in Module Instantiation

**Specification Reference:** Section 7.9 of jz-hdl-specification.md

## 1. Objective

Verify MEM access scoping across module hierarchy: MEM declared in a child module cannot be directly accessed from the parent and must be accessed through module ports. The canonical pattern is to wrap memory interfaces in module ports.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Module with MEM instantiated normally | Parent instantiates child containing MEM, accesses via ports | Valid, compiles without errors |
| 2 | Multiple instances of MEM-containing module | Two instances of same MEM module, independent memories | Valid, no conflicts |
| 3 | MEM controller pattern | Child module wraps MEM behind port interface, parent uses ports | Valid |

### 2.2 Error Cases

| # | Test Case | Input | Expected | Rule ID |
|---|-----------|-------|----------|---------|
| 1 | Direct access to child MEM from parent | Parent references `child.mem_name` directly | Error | UNDECLARED_IDENTIFIER |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Same MEM name in parent and child | Both modules declare MEM with same name | Valid, scoped independently |
| 2 | Child MEM exposed via tristate bus | Child uses MEM + INOUT port for external bus | Valid if module ports are correct |

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1 | Parent accesses child MEM directly | `child_inst.mem_name.port[addr]` | UNDECLARED_IDENTIFIER | error |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 7_9_HAPPY_PATH-mem_in_module_ok.jz | — | Happy path: child module with MEM accessed via ports |
| 7_9_UNDECLARED_IDENTIFIER-child_mem_access_from_parent.jz | UNDECLARED_IDENTIFIER | Parent attempts direct access to child module MEM |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| UNDECLARED_IDENTIFIER | error | S7.9 Direct access to child MEM from parent scope | 7_9_UNDECLARED_IDENTIFIER-child_mem_access_from_parent.jz |

### 5.2 Rules Not Tested


All rules for this section are tested.
