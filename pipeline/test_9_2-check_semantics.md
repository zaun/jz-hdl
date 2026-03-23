# Test Plan: 9.2 @check Semantics

**Specification Reference:** Section 9.2 of jz-hdl-specification.md

## 1. Objective
Verify @check evaluation: true (nonzero) passes silently, false (zero) fires CHECK_FAILED error with message.

## 2. Test Scenarios

### 2.1 Happy Path
1. True condition passes silently (no diagnostic)
2. CONST-based expression evaluates correctly

### 2.2 Error Cases
1. False condition fires CHECK_FAILED error with user-provided message

### 2.3 Edge Cases
1. Expression evaluating to nonzero value other than 1 still passes
2. Expression evaluating to exactly zero fires error

## 3. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | `@check (0, "fail")` | Error: CHECK FAILED | CHECK_FAILED | S9.2 false fires |

## 4. Existing Validation Tests
| Test File | Rule Tested |
|-----------|-------------|
| `9_2_CHECK_FAILED-false_assertion.jz` | CHECK_FAILED |

## 5. Rules Matrix

### 5.1 Rules Tested
| Rule ID | Severity | Test Case(s) |
|---------|----------|-------------|
| CHECK_FAILED | error | `9_2_CHECK_FAILED-false_assertion.jz` |

### 5.2 Rules Not Tested
| Rule ID | Severity | Gap Description |
|---------|----------|-----------------|
| — | — | All section 9.2 rules covered |
