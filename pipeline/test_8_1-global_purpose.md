# Test Plan: 8.1 Global Purpose

**Specification Reference:** Section 8.1 of jz-hdl-specification.md

## 1. Objective
Verify that `@global` blocks define named collections of sized literal constants visible to all modules and projects within the compilation unit, enforce namespace uniqueness across the compilation root, and enforce read-only semantics (assignment to a global constant is forbidden).

## 2. Test Scenarios

### 2.1 Happy Path
1. Define two `@global` blocks (`CMD`, `STATUS`) with sized literals; reference `CMD.READ` and `STATUS.OK` in module ASYNC and SYNC expressions across multiple modules -- no diagnostics
2. Multiple `@global` blocks with unique names coexist without conflict
3. Single-entry `@global` block resolves correctly
4. Global constant referenced in both a helper module and the top module simultaneously

### 2.2 Error Cases
1. Duplicate `@global` name (`COLORS` declared three times) produces GLOBAL_NAMESPACE_DUPLICATE error on second and third declarations
2. Assign to global constant in ASYNCHRONOUS block produces GLOBAL_ASSIGN_FORBIDDEN error
3. Assign to global constant in SYNCHRONOUS block produces GLOBAL_ASSIGN_FORBIDDEN error
4. Assign to global constant in top-module ASYNCHRONOUS block produces GLOBAL_ASSIGN_FORBIDDEN error
5. Assign to global constant in top-module SYNCHRONOUS block produces GLOBAL_ASSIGN_FORBIDDEN error

### 2.3 Edge Cases
1. Large number of entries in a single `@global` block
2. Global constant referenced in multiple modules simultaneously (covered by happy path)
3. Valid reads of globals on RHS coexist with invalid LHS assignments in the same module -- only assignments trigger errors

## 3. Input/Output Matrix
| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1 | Second `@global COLORS` block duplicates first | `@global COLORS` (line 20) | GLOBAL_NAMESPACE_DUPLICATE | error |
| 2 | Third `@global COLORS` block duplicates first | `@global COLORS` (line 32) | GLOBAL_NAMESPACE_DUPLICATE | error |
| 3 | Assign to global in helper ASYNC block | `CMD.WRITE = r;` | GLOBAL_ASSIGN_FORBIDDEN | error |
| 4 | Assign to global in helper SYNC block | `CMD.RESET <= r;` | GLOBAL_ASSIGN_FORBIDDEN | error |
| 5 | Assign to global in top-module ASYNC block | `CMD.READ = r;` | GLOBAL_ASSIGN_FORBIDDEN | error |
| 6 | Assign to global in top-module SYNC block | `CMD.WRITE <= r;` | GLOBAL_ASSIGN_FORBIDDEN | error |

## 4. Existing Validation Tests
| Test File | Rule Tested | Triggers |
|-----------|-------------|----------|
| `8_1_HAPPY_PATH-global_purpose_ok.jz` | (none -- clean) | 0 diagnostics |
| `8_1_GLOBAL_NAMESPACE_DUPLICATE-duplicate_global_name.jz` | GLOBAL_NAMESPACE_DUPLICATE | 2 triggers |
| `8_1_GLOBAL_ASSIGN_FORBIDDEN-assign_to_global.jz` | GLOBAL_ASSIGN_FORBIDDEN | 4 triggers |

## 5. Rules Matrix

### 5.1 Rules Tested
| Rule ID | Severity | Test Case(s) |
|---------|----------|-------------|
| GLOBAL_NAMESPACE_DUPLICATE | error | `8_1_GLOBAL_NAMESPACE_DUPLICATE-duplicate_global_name.jz` |
| GLOBAL_ASSIGN_FORBIDDEN | error | `8_1_GLOBAL_ASSIGN_FORBIDDEN-assign_to_global.jz` |

### 5.2 Rules Not Tested
| Rule ID | Severity | Gap Description |
|---------|----------|-----------------|
| -- | -- | All section 8.1 rules covered |
