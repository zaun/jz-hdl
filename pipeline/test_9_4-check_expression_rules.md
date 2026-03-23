# Test Plan: 9.4 @check Expression Rules

**Specification Reference:** Section 9.4 of jz-hdl-specification.md

## 1. Objective
Verify allowed operands (literals, CONST, CONFIG, clog2, comparisons, logical operators) and forbidden operands (ports, wires, registers, memory, slices, runtime signals) in @check expressions.

## 2. Test Scenarios

### 2.1 Happy Path
1. Integer literal in @check expression
2. CONST reference in @check expression
3. CONFIG reference in @check expression
4. clog2 intrinsic in @check expression
5. Logical operators (&&, ||) in @check expression

### 2.2 Error Cases
1. Memory reference in @check produces CHECK_INVALID_EXPR_TYPE error
2. Bit slice in @check produces CHECK_INVALID_EXPR_TYPE error
3. Port reference in @check produces CHECK_INVALID_EXPR_TYPE error
4. Wire reference in @check produces CHECK_INVALID_EXPR_TYPE error
5. Register reference in @check produces CHECK_INVALID_EXPR_TYPE error

### 2.3 Edge Cases
1. Nested expressions mixing allowed operands (CONST + literal)

## 3. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Memory/slice in @check | Error | CHECK_INVALID_EXPR_TYPE | S9.4 forbidden operand |

## 4. Existing Validation Tests
| Test File | Rule Tested |
|-----------|-------------|
| `9_4_CHECK_INVALID_EXPR_TYPE-memory_and_slice_in_check.jz` | CHECK_INVALID_EXPR_TYPE |

## 5. Rules Matrix

### 5.1 Rules Tested
| Rule ID | Severity | Test Case(s) |
|---------|----------|-------------|
| CHECK_INVALID_EXPR_TYPE | error | `9_4_CHECK_INVALID_EXPR_TYPE-memory_and_slice_in_check.jz` |

### 5.2 Rules Not Tested
| Rule ID | Severity | Gap Description |
|---------|----------|-----------------|
| — | — | All section 9.4 rules covered |
