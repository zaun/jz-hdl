# Test Plan: 9.1 @check Syntax

**Specification Reference:** Section 9.1 of jz-hdl-specification.md

## 1. Objective
Verify `@check (<expr>, <message>);` syntax with parenthesized expressions, detecting invalid expression types.

## 2. Test Scenarios

### 2.1 Happy Path
1. `@check (WIDTH == 32, "Width must be 32");` parses and evaluates correctly
2. `@check (DEPTH > 0, "Depth must be positive");` parses and evaluates correctly
3. Complex expression with clog2: `@check (clog2(DEPTH) <= 16, "Too deep");`

### 2.2 Error Cases
1. Runtime signal in @check expression produces CHECK_INVALID_EXPR_TYPE error
2. Missing parentheses produces parse error
3. Missing message string produces parse error
4. Missing semicolon produces parse error

### 2.3 Edge Cases
1. Deeply nested sub-expressions in @check

## 3. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Runtime signal in @check | Error | CHECK_INVALID_EXPR_TYPE | S9.1 compile-time only |

## 4. Existing Validation Tests
| Test File | Rule Tested |
|-----------|-------------|
| `9_1_CHECK_INVALID_EXPR_TYPE-runtime_signal_in_check.jz` | CHECK_INVALID_EXPR_TYPE |

## 5. Rules Matrix

### 5.1 Rules Tested
| Rule ID | Severity | Test Case(s) |
|---------|----------|-------------|
| CHECK_INVALID_EXPR_TYPE | error | `9_1_CHECK_INVALID_EXPR_TYPE-runtime_signal_in_check.jz` |

### 5.2 Rules Not Tested
| Rule ID | Severity | Gap Description |
|---------|----------|-----------------|
| — | — | All section 9.1 rules covered |
