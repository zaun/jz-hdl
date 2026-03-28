# Test Plan: 9.5 @check Evaluation Order

**Specification Reference:** Section 9.5 of jz-hdl-specification.md

## 1. Objective
Verify that `@check` evaluates after resolution of all preceding CONST definitions, CONFIG entries, and OVERRIDE evaluation. Specifically: @check can reference CONFIG declared in the same project, @check can reference CONST declared in the same module, and undefined references at project scope produce CHECK_INVALID_EXPR_TYPE.

## 2. Test Scenarios

### 2.1 Happy Path
1. @check references CONFIG from the same project (`CONFIG.WIDTH >= 8`) -- resolves correctly
2. @check references CONFIG arithmetic at project scope (`CONFIG.DEPTH > CONFIG.WIDTH`) -- resolves
3. @check references clog2 of CONFIG at project scope -- resolves
4. @check references CONST defined earlier in helper module (`LOCAL_W > 0`) -- resolves
5. @check references CONFIG from project inside helper module (`CONFIG.WIDTH > 0`) -- resolves
6. @check references CONST in top module (`SIZE > 0`) -- resolves
7. @check uses clog2 of CONST in top module (`clog2(SIZE) == 4`) -- resolves
8. @check references CONFIG in top module (`CONFIG.WIDTH == 8`) -- resolves
9. @check with CONFIG arithmetic in top module (`CONFIG.WIDTH + CONFIG.DEPTH > 0`) -- resolves

### 2.2 Error Cases
1. Bare undefined identifier at project scope (`UNDEF_A > 0`) produces CHECK_INVALID_EXPR_TYPE
2. Undefined identifier in arithmetic at project scope (`UNDEF_B + CONFIG.WIDTH > 0`) produces CHECK_INVALID_EXPR_TYPE
3. Undefined identifier in comparison at project scope (`UNDEF_C == CONFIG.DEPTH`) produces CHECK_INVALID_EXPR_TYPE
4. Undefined identifier inside clog2 at project scope (`clog2(UNDEF_D) > 0`) produces CHECK_INVALID_EXPR_TYPE

### 2.3 Edge Cases
1. @check depends on CONST that itself depends on another CONST (transitive resolution) -- valid if all are defined
2. @check with CONFIG that has been overridden at instantiation -- sees post-override value
3. Defined CONST @check in module scope coexists with undefined @check at project scope -- only project-scope ones trigger

## 3. Input/Output Matrix
| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1 | Bare undefined ID at project scope | `UNDEF_A > 0` | CHECK_INVALID_EXPR_TYPE | error |
| 2 | Undefined ID in arithmetic | `UNDEF_B + CONFIG.WIDTH > 0` | CHECK_INVALID_EXPR_TYPE | error |
| 3 | Undefined ID in comparison | `UNDEF_C == CONFIG.DEPTH` | CHECK_INVALID_EXPR_TYPE | error |
| 4 | Undefined ID inside clog2 | `clog2(UNDEF_D) > 0` | CHECK_INVALID_EXPR_TYPE | error |

## 4. Existing Validation Tests
| Test File | Rule Tested | Triggers |
|-----------|-------------|----------|
| `9_5_HAPPY_PATH-check_evaluation_order_ok.jz` | (none -- clean) | 0 diagnostics |
| `9_5_CHECK_INVALID_EXPR_TYPE-undefined_const_in_check.jz` | CHECK_INVALID_EXPR_TYPE | 4 triggers |

## 5. Rules Matrix

### 5.1 Rules Tested
| Rule ID | Severity | Test Case(s) |
|---------|----------|-------------|
| CHECK_INVALID_EXPR_TYPE | error | `9_5_CHECK_INVALID_EXPR_TYPE-undefined_const_in_check.jz` |

### 5.2 Rules Not Tested
| Rule ID | Severity | Gap Description |
|---------|----------|-----------------|
| -- | -- | All section 9.5 rules covered. Note: OVERRIDE-affected @check evaluation is implicitly covered by the resolution order but no dedicated test exercises @check seeing a post-OVERRIDE CONST value. This could be added as an enhancement. |
