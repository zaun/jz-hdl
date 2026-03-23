# Test Plan: 8.3 Global Semantics

**Specification Reference:** Section 8.3 of jz-hdl-specification.md

## 1. Objective
Verify namespace roots (`<global>.<id>`), uniqueness within block, and forbidden contexts for global references.

## 2. Test Scenarios

### 2.1 Happy Path
1. Reference via `ISA.INST_ADD` resolves correctly
2. Width matches target in assignment
3. Multiple blocks with disjoint namespaces coexist

### 2.2 Error Cases
1. Duplicate const_id in same block produces GLOBAL_CONST_NAME_DUPLICATE error
2. Undefined global reference produces GLOBAL_CONST_USE_UNDECLARED error
3. Global used in forbidden context produces GLOBAL_USED_WHERE_FORBIDDEN error

### 2.3 Edge Cases
1. Multiple blocks with disjoint namespaces, each containing identically named constants

## 3. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Duplicate const in block | Error | GLOBAL_CONST_NAME_DUPLICATE | S8.3 uniqueness |
| 2 | Undefined global ref | Error | GLOBAL_CONST_USE_UNDECLARED | S8.3 |
| 3 | Global in forbidden context | Error | GLOBAL_USED_WHERE_FORBIDDEN | S8.3 |

## 4. Existing Validation Tests
| Test File | Rule Tested |
|-----------|-------------|
| `8_3_GLOBAL_CONST_NAME_DUPLICATE-dup_const_in_block.jz` | GLOBAL_CONST_NAME_DUPLICATE |

## 5. Rules Matrix

### 5.1 Rules Tested
| Rule ID | Severity | Test Case(s) |
|---------|----------|-------------|
| GLOBAL_CONST_NAME_DUPLICATE | error | `8_3_GLOBAL_CONST_NAME_DUPLICATE-dup_const_in_block.jz` |

### 5.2 Rules Not Tested
| Rule ID | Severity | Gap Description |
|---------|----------|-----------------|
| GLOBAL_CONST_USE_UNDECLARED | error | No validation test for undefined global reference |
| GLOBAL_USED_WHERE_FORBIDDEN | error | No validation test for global in forbidden context |
