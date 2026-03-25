# Test Plan: 9.7 @check Error Conditions

**Specification Reference:** Section 9.7 of jz-hdl-specification.md

## 1. Objective
Cross-reference and consolidate all `@check` error conditions from sections 9.1--9.5. A `@check` results in a compile error if: the expression evaluates to zero (CHECK_FAILED), the expression contains a runtime signal (CHECK_INVALID_EXPR_TYPE), the expression contains undefined identifiers (CHECK_INVALID_EXPR_TYPE), the expression uses operators disallowed in constant expressions (CHECK_INVALID_EXPR_TYPE), or the @check appears inside a block (DIRECTIVE_INVALID_CONTEXT).

## 2. Test Scenarios

### 2.1 Happy Path
1. Valid @check with true expression passes silently (covered in test_9_2)

### 2.2 Error Cases
1. Expression evaluates to zero -- CHECK_FAILED error with message (see test_9_2)
2. Runtime signal (port) in expression -- CHECK_INVALID_EXPR_TYPE error (see test_9_1)
3. Runtime signal (register) in expression -- CHECK_INVALID_EXPR_TYPE error (see test_9_1)
4. Runtime signal (wire) in expression -- CHECK_INVALID_EXPR_TYPE error (see test_9_1)
5. Undefined identifier in expression -- CHECK_INVALID_EXPR_TYPE error (see test_9_5)
6. Port bit-slice in expression -- CHECK_INVALID_EXPR_TYPE error (see test_9_4)
7. Register bit-slice in expression -- CHECK_INVALID_EXPR_TYPE error (see test_9_4)
8. Wire bit-slice in expression -- CHECK_INVALID_EXPR_TYPE error (see test_9_4)
9. @check in ASYNCHRONOUS block -- DIRECTIVE_INVALID_CONTEXT error (see test_9_3)
10. @check in SYNCHRONOUS block -- DIRECTIVE_INVALID_CONTEXT error (see test_9_3)

### 2.3 Edge Cases
1. Non-integer result type in @check expression (covered implicitly by runtime signal rejection)
2. Multiple error conditions in one file (e.g., false @check + undefined @check)

## 3. Input/Output Matrix
| # | Scenario | Expected Rule ID | Severity | Cross-Ref |
|---|----------|-----------------|----------|-----------|
| 1 | False @check (zero expression) | CHECK_FAILED | error | test_9_2 |
| 2 | Runtime signal: port | CHECK_INVALID_EXPR_TYPE | error | test_9_1 |
| 3 | Runtime signal: register | CHECK_INVALID_EXPR_TYPE | error | test_9_1 |
| 4 | Runtime signal: wire | CHECK_INVALID_EXPR_TYPE | error | test_9_1 |
| 5 | Undefined identifier | CHECK_INVALID_EXPR_TYPE | error | test_9_5 |
| 6 | Port bit-slice | CHECK_INVALID_EXPR_TYPE | error | test_9_4 |
| 7 | Register bit-slice | CHECK_INVALID_EXPR_TYPE | error | test_9_4 |
| 8 | Wire bit-slice | CHECK_INVALID_EXPR_TYPE | error | test_9_4 |
| 9 | @check in ASYNC block | DIRECTIVE_INVALID_CONTEXT | error | test_9_3 |
| 10 | @check in SYNC block | DIRECTIVE_INVALID_CONTEXT | error | test_9_3 |

## 4. Existing Validation Tests
| Test File | Rule Tested | Defined In |
|-----------|-------------|------------|
| `9_1_CHECK_INVALID_EXPR_TYPE-runtime_signal_in_check.jz` | CHECK_INVALID_EXPR_TYPE | test_9_1 |
| `9_2_CHECK_FAILED-false_assertion.jz` | CHECK_FAILED | test_9_2 |
| `9_3_DIRECTIVE_INVALID_CONTEXT-check_in_async.jz` | DIRECTIVE_INVALID_CONTEXT | test_9_3 |
| `9_3_DIRECTIVE_INVALID_CONTEXT-check_in_sync.jz` | DIRECTIVE_INVALID_CONTEXT | test_9_3 |
| `9_4_CHECK_INVALID_EXPR_TYPE-forbidden_operands.jz` | CHECK_INVALID_EXPR_TYPE | test_9_4 |
| `9_5_CHECK_INVALID_EXPR_TYPE-undefined_const_in_check.jz` | CHECK_INVALID_EXPR_TYPE | test_9_5 |

## 5. Rules Matrix

### 5.1 Rules Tested
| Rule ID | Severity | Test Case(s) |
|---------|----------|-------------|
| CHECK_FAILED | error | `9_2_CHECK_FAILED-false_assertion.jz` |
| CHECK_INVALID_EXPR_TYPE | error | `9_1_CHECK_INVALID_EXPR_TYPE-runtime_signal_in_check.jz`, `9_4_CHECK_INVALID_EXPR_TYPE-forbidden_operands.jz`, `9_5_CHECK_INVALID_EXPR_TYPE-undefined_const_in_check.jz` |
| DIRECTIVE_INVALID_CONTEXT | error | `9_3_DIRECTIVE_INVALID_CONTEXT-check_in_async.jz`, `9_3_DIRECTIVE_INVALID_CONTEXT-check_in_sync.jz` |

### 5.2 Rules Not Tested
| Rule ID | Severity | Gap Description |
|---------|----------|-----------------|
| CHECK_INVALID_PLACEMENT | error | Rule ID exists in rules.c for "@check may not appear inside conditional or @feature bodies" but is not directly emitted by current tests. The compiler emits DIRECTIVE_INVALID_CONTEXT for @check inside ASYNC/SYNC blocks. CHECK_INVALID_PLACEMENT may trigger for @check inside @feature guards or other conditional directive bodies -- no dedicated test exists for that scenario yet. |
