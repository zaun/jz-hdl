# Test Plan: 8.5 Global Errors

**Specification Reference:** Section 8.5 of jz-hdl-specification.md

## 1. Objective
Verify global error conditions including forward references, circular dependencies, and literal overflow.

## 2. Test Scenarios

### 2.1 Happy Path
1. Well-formed @global block with no forward references or overflows compiles cleanly

### 2.2 Error Cases
1. Forward reference in @global produces GLOBAL_FORWARD_REF error
2. Circular dependency in @global produces GLOBAL_CIRCULAR_DEP error
3. Literal overflow in @global produces LIT_OVERFLOW error

### 2.3 Edge Cases
1. Forward reference that would resolve if order were reversed
2. Literal at exact maximum width boundary (no overflow)

## 3. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Forward reference | Error | GLOBAL_FORWARD_REF | S8.5 |
| 2 | Circular dependency | Error | GLOBAL_CIRCULAR_DEP | S8.5 |
| 3 | Literal overflow | Error | LIT_OVERFLOW | S2.1 |

## 4. Existing Validation Tests
| Test File | Rule Tested |
|-----------|-------------|
| `8_5_GLOBAL_FORWARD_REF-forward_ref_in_global.jz` | GLOBAL_FORWARD_REF |
| `8_5_LIT_OVERFLOW-global_literal_overflow.jz` | LIT_OVERFLOW |

## 5. Rules Matrix

### 5.1 Rules Tested
| Rule ID | Severity | Test Case(s) |
|---------|----------|-------------|
| GLOBAL_FORWARD_REF | error | `8_5_GLOBAL_FORWARD_REF-forward_ref_in_global.jz` |
| LIT_OVERFLOW | error | `8_5_LIT_OVERFLOW-global_literal_overflow.jz` |

### 5.2 Rules Not Tested
| Rule ID | Severity | Gap Description |
|---------|----------|-----------------|
| GLOBAL_CIRCULAR_DEP | error | No validation test for circular dependency in @global |
