# Test Plan: 8.1 Global Purpose

**Specification Reference:** Section 8.1 of jz-hdl-specification.md

## 1. Objective
Verify @global blocks define named collections of sized literal constants visible to all modules, enforce namespace uniqueness, and enforce read-only semantics.

## 2. Test Scenarios

### 2.1 Happy Path
1. Define @global with sized literals and reference `GLOBAL.CONST` in a module expression
2. Multiple @global blocks with unique names coexist without conflict
3. Single entry in @global block resolves correctly

### 2.2 Error Cases
1. Duplicate @global name produces GLOBAL_NAMESPACE_DUPLICATE error
2. Assign to global constant produces GLOBAL_ASSIGN_FORBIDDEN error (read-only)

### 2.3 Edge Cases
1. Large number of entries in a single @global block
2. Global constant referenced in multiple modules simultaneously

## 3. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Duplicate @global name | Error | GLOBAL_NAMESPACE_DUPLICATE | S8.1 uniqueness |
| 2 | Assign to global constant | Error | GLOBAL_ASSIGN_FORBIDDEN | S8.1 read-only |

## 4. Existing Validation Tests
| Test File | Rule Tested |
|-----------|-------------|
| `8_1_GLOBAL_ASSIGN_FORBIDDEN-assign_to_global.jz` | GLOBAL_ASSIGN_FORBIDDEN |
| `8_1_GLOBAL_NAMESPACE_DUPLICATE-duplicate_global_name.jz` | GLOBAL_NAMESPACE_DUPLICATE |

## 5. Rules Matrix

### 5.1 Rules Tested
| Rule ID | Severity | Test Case(s) |
|---------|----------|-------------|
| GLOBAL_NAMESPACE_DUPLICATE | error | `8_1_GLOBAL_NAMESPACE_DUPLICATE-duplicate_global_name.jz` |
| GLOBAL_ASSIGN_FORBIDDEN | error | `8_1_GLOBAL_ASSIGN_FORBIDDEN-assign_to_global.jz` |

### 5.2 Rules Not Tested
| Rule ID | Severity | Gap Description |
|---------|----------|-----------------|
| — | — | All section 8.1 rules covered |
