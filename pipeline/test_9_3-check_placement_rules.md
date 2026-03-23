# Test Plan: 9.3 @check Placement Rules

**Specification Reference:** Section 9.3 of jz-hdl-specification.md

## 1. Objective
Verify valid @check contexts (module scope, project scope) and reject @check inside blocks (ASYNCHRONOUS, SYNCHRONOUS).

## 2. Test Scenarios

### 2.1 Happy Path
1. @check inside @module at module scope compiles cleanly
2. @check inside @project compiles cleanly

### 2.2 Error Cases
1. @check inside ASYNCHRONOUS block produces DIRECTIVE_INVALID_CONTEXT error
2. @check inside SYNCHRONOUS block produces DIRECTIVE_INVALID_CONTEXT error

### 2.3 Edge Cases
1. @check immediately before a block definition (valid, at module scope)

## 3. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | @check in ASYNC block | Error | DIRECTIVE_INVALID_CONTEXT | S9.3 block forbidden |
| 2 | @check in SYNC block | Error | DIRECTIVE_INVALID_CONTEXT | S9.3 block forbidden |

## 4. Existing Validation Tests
| Test File | Rule Tested |
|-----------|-------------|
| `9_3_DIRECTIVE_INVALID_CONTEXT-check_in_async.jz` | DIRECTIVE_INVALID_CONTEXT |
| `9_3_DIRECTIVE_INVALID_CONTEXT-check_in_sync.jz` | DIRECTIVE_INVALID_CONTEXT |

## 5. Rules Matrix

### 5.1 Rules Tested
| Rule ID | Severity | Test Case(s) |
|---------|----------|-------------|
| CHECK_INVALID_PLACEMENT | error | Covered via DIRECTIVE_INVALID_CONTEXT tests |
| DIRECTIVE_INVALID_CONTEXT | error | `9_3_DIRECTIVE_INVALID_CONTEXT-check_in_async.jz`, `9_3_DIRECTIVE_INVALID_CONTEXT-check_in_sync.jz` |

### 5.2 Rules Not Tested
| Rule ID | Severity | Gap Description |
|---------|----------|-----------------|
| — | — | All section 9.3 rules covered |
