# Test Plan: 9.7 @check Error Conditions

**Specification Reference:** Section 9.7 of jz-hdl-specification.md

## 1. Objective
Cross-reference of all @check error conditions: false expression, invalid expression type, and invalid placement.

## 2. Test Scenarios

### 2.1 Happy Path
1. Valid @check with true expression passes silently (covered in 9.2)

### 2.2 Error Cases
1. Expression evaluates to zero -- CHECK_FAILED error with message (see 9.2)
2. Runtime signal in expression -- CHECK_INVALID_EXPR_TYPE error (see 9.1)
3. Undefined identifier in expression -- CHECK_INVALID_EXPR_TYPE error (see 9.5)
4. Memory/slice in expression -- CHECK_INVALID_EXPR_TYPE error (see 9.4)
5. @check in ASYNCHRONOUS block -- DIRECTIVE_INVALID_CONTEXT error (see 9.3)
6. @check in SYNCHRONOUS block -- DIRECTIVE_INVALID_CONTEXT error (see 9.3)

### 2.3 Edge Cases
1. Non-integer result type in @check expression

## 3. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Cross-Ref |
|---|-------|----------------|---------|-----------|
| 1 | False @check | Error: CHECK FAILED | CHECK_FAILED | test_9_2 |
| 2 | Runtime signal | Error | CHECK_INVALID_EXPR_TYPE | test_9_1 |
| 3 | Undefined ID | Error | CHECK_INVALID_EXPR_TYPE | test_9_5 |
| 4 | Memory/slice | Error | CHECK_INVALID_EXPR_TYPE | test_9_4 |
| 5 | @check in ASYNC | Error | DIRECTIVE_INVALID_CONTEXT | test_9_3 |
| 6 | @check in SYNC | Error | DIRECTIVE_INVALID_CONTEXT | test_9_3 |

## 4. Existing Validation Tests
| Test File | Rule Tested | Defined In |
|-----------|-------------|------------|
| `9_1_CHECK_INVALID_EXPR_TYPE-runtime_signal_in_check.jz` | CHECK_INVALID_EXPR_TYPE | test_9_1 |
| `9_2_CHECK_FAILED-false_assertion.jz` | CHECK_FAILED | test_9_2 |
| `9_3_DIRECTIVE_INVALID_CONTEXT-check_in_async.jz` | DIRECTIVE_INVALID_CONTEXT | test_9_3 |
| `9_3_DIRECTIVE_INVALID_CONTEXT-check_in_sync.jz` | DIRECTIVE_INVALID_CONTEXT | test_9_3 |
| `9_4_CHECK_INVALID_EXPR_TYPE-memory_and_slice_in_check.jz` | CHECK_INVALID_EXPR_TYPE | test_9_4 |
| `9_5_CHECK_INVALID_EXPR_TYPE-undefined_const_in_check.jz` | CHECK_INVALID_EXPR_TYPE | test_9_5 |

## 5. Rules Matrix

### 5.1 Rules Tested
| Rule ID | Severity | Test Case(s) |
|---------|----------|-------------|
| CHECK_FAILED | error | `9_2_CHECK_FAILED-false_assertion.jz` |
| CHECK_INVALID_EXPR_TYPE | error | `9_1_...runtime_signal_in_check.jz`, `9_4_...memory_and_slice_in_check.jz`, `9_5_...undefined_const_in_check.jz` |
| CHECK_INVALID_PLACEMENT | error | Covered via DIRECTIVE_INVALID_CONTEXT tests in test_9_3 |

### 5.2 Rules Not Tested
| Rule ID | Severity | Gap Description |
|---------|----------|-----------------|
| — | — | All @check error conditions covered across sections 9.1-9.5 |
