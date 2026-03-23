# Test Plan: 9.5 @check Evaluation Order

**Specification Reference:** Section 9.5 of jz-hdl-specification.md

## 1. Objective
Verify @check evaluates after CONST, CONFIG, and OVERRIDE resolution, and that undefined references are caught.

## 2. Test Scenarios

### 2.1 Happy Path
1. @check references CONST defined earlier in module -- resolves correctly
2. @check references CONFIG from project -- resolves correctly
3. @check sees OVERRIDE values applied to CONFIG

### 2.2 Error Cases
1. @check references undefined CONST produces CHECK_INVALID_EXPR_TYPE error

### 2.3 Edge Cases
1. @check depends on CONST that itself depends on another CONST (transitive resolution)
2. @check with CONFIG that has been overridden at instantiation

## 3. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Undefined CONST in @check | Error | CHECK_INVALID_EXPR_TYPE | S9.5 resolution order |

## 4. Existing Validation Tests
| Test File | Rule Tested |
|-----------|-------------|
| `9_5_CHECK_INVALID_EXPR_TYPE-undefined_const_in_check.jz` | CHECK_INVALID_EXPR_TYPE |

## 5. Rules Matrix

### 5.1 Rules Tested
| Rule ID | Severity | Test Case(s) |
|---------|----------|-------------|
| CHECK_INVALID_EXPR_TYPE | error | `9_5_CHECK_INVALID_EXPR_TYPE-undefined_const_in_check.jz` |

### 5.2 Rules Not Tested
| Rule ID | Severity | Gap Description |
|---------|----------|-----------------|
| — | — | All section 9.5 rules covered |
