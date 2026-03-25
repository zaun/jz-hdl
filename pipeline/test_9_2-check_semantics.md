# Test Plan: 9.2 @check Semantics

**Specification Reference:** Section 9.2 of jz-hdl-specification.md

## 1. Objective
Verify `@check` evaluation semantics: true (nonzero integer) conditions pass silently with no diagnostics; false (zero) conditions fire CHECK_FAILED error with the user-provided message string. Non-constant expressions produce CHECK_INVALID_EXPR_TYPE (covered in test_9_1).

## 2. Test Scenarios

### 2.1 Happy Path
1. True CONFIG equality (`CONFIG.WIDTH == 8`) passes silently at project scope
2. True CONFIG comparison (`CONFIG.DEPTH > 0`) passes silently
3. Nonzero value other than 1 (`CONFIG.WIDTH * 5 + 2` = 42) passes silently -- truthy
4. clog2 of CONFIG evaluates correctly and passes
5. CONST-based @check in module scope passes (`SIZE == 16`)
6. Literal `1` passes silently
7. Large nonzero literal (`999`) passes silently
8. CONST arithmetic producing nonzero (`LOCAL_W * 2` = 8) passes

### 2.2 Error Cases
1. False CONFIG equality at project scope (`CONFIG.WIDTH == 16`) produces CHECK_FAILED
2. Arithmetic evaluating to zero at project scope (`CONFIG.WIDTH - 8`) produces CHECK_FAILED
3. False CONST equality in helper module (`SIZE == 8` when SIZE=4) produces CHECK_FAILED
4. clog2 mismatch in top module (`ADDR_W == clog2(DEPTH)` when 3 != 4) produces CHECK_FAILED
5. Literal zero (`@check (0, "literal zero fails")`) produces CHECK_FAILED

### 2.3 Edge Cases
1. Expression evaluating to nonzero value other than 1 still passes (covered in happy path #3)
2. Expression evaluating to exactly zero fires error (covered in error case #2)
3. Multiple true and false @check in the same file -- only false ones trigger

## 3. Input/Output Matrix
| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1 | False CONFIG equality (project scope) | `CONFIG.WIDTH == 16` | CHECK_FAILED | error |
| 2 | Arithmetic yields zero (project scope) | `CONFIG.WIDTH - 8` | CHECK_FAILED | error |
| 3 | False CONST equality (helper module) | `SIZE == 8` (SIZE=4) | CHECK_FAILED | error |
| 4 | clog2 mismatch (top module) | `ADDR_W == clog2(DEPTH)` | CHECK_FAILED | error |
| 5 | Literal zero | `@check (0, ...)` | CHECK_FAILED | error |

## 4. Existing Validation Tests
| Test File | Rule Tested | Triggers |
|-----------|-------------|----------|
| `9_2_HAPPY_PATH-check_semantics_ok.jz` | (none -- clean) | 0 diagnostics |
| `9_2_CHECK_FAILED-false_assertion.jz` | CHECK_FAILED | 5 triggers |

## 5. Rules Matrix

### 5.1 Rules Tested
| Rule ID | Severity | Test Case(s) |
|---------|----------|-------------|
| CHECK_FAILED | error | `9_2_CHECK_FAILED-false_assertion.jz` |

### 5.2 Rules Not Tested
| Rule ID | Severity | Gap Description |
|---------|----------|-----------------|
| -- | -- | All section 9.2 rules covered |
