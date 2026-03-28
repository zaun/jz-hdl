# Test Plan: 9.1 @check Syntax

**Specification Reference:** Section 9.1 of jz-hdl-specification.md

## 1. Objective
Verify that `@check (<constant_expression>, <string_message>);` syntax is parsed correctly with parenthesized expressions and string messages, that valid constant expressions (literals, CONST, CONFIG, clog2, arithmetic) are accepted, and that non-constant expressions (runtime signals, undefined identifiers) produce CHECK_INVALID_EXPR_TYPE errors.

## 2. Test Scenarios

### 2.1 Happy Path
1. `@check (CONFIG.WIDTH == 8, "Width must be 8");` at project scope -- no diagnostics
2. `@check (CONFIG.DEPTH > 0, "Depth must be positive");` at project scope -- no diagnostics
3. `@check (clog2(CONFIG.DEPTH) <= 16, "Too deep");` with clog2 intrinsic -- no diagnostics
4. `@check (1 == 1, "literal truth");` literal-only expression -- no diagnostics
5. `@check (LOCAL_W > 0, "local width ok");` CONST in helper module -- no diagnostics
6. `@check (LOCAL_W * 2 == 8, "double width ok");` CONST arithmetic -- no diagnostics
7. `@check (CONFIG.WIDTH == 8, "config ok");` CONFIG reference at module scope -- no diagnostics
8. `@check (clog2(WIDTH) == ADDR_W, "address width ok");` clog2 of CONST at module scope -- no diagnostics

### 2.2 Error Cases
1. Undefined identifier at project scope (`UNKNOWN_VAR > 0`) produces CHECK_INVALID_EXPR_TYPE
2. Port signal in @check in helper module (`din == 1'b1`) produces CHECK_INVALID_EXPR_TYPE
3. Port signal in @check in top module (`clk == 1'b1`) produces CHECK_INVALID_EXPR_TYPE
4. Register signal in @check (`r == 1'b0`) produces CHECK_INVALID_EXPR_TYPE
5. Wire signal in @check (`w == 1'b0`) produces CHECK_INVALID_EXPR_TYPE

### 2.3 Edge Cases
1. Deeply nested sub-expressions in @check (covered by complex expression in happy path)
2. Multiple valid @check and invalid @check in the same file -- only invalid ones trigger

## 3. Input/Output Matrix
| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1 | Undefined identifier at project scope | `UNKNOWN_VAR > 0` | CHECK_INVALID_EXPR_TYPE | error |
| 2 | Port signal in helper @check | `din == 1'b1` | CHECK_INVALID_EXPR_TYPE | error |
| 3 | Port signal in top @check | `clk == 1'b1` | CHECK_INVALID_EXPR_TYPE | error |
| 4 | Register signal in @check | `r == 1'b0` | CHECK_INVALID_EXPR_TYPE | error |
| 5 | Wire signal in @check | `w == 1'b0` | CHECK_INVALID_EXPR_TYPE | error |

## 4. Existing Validation Tests
| Test File | Rule Tested | Triggers |
|-----------|-------------|----------|
| `9_1_HAPPY_PATH-check_syntax_ok.jz` | (none -- clean) | 0 diagnostics |
| `9_1_CHECK_INVALID_EXPR_TYPE-runtime_signal_in_check.jz` | CHECK_INVALID_EXPR_TYPE | 5 triggers |

## 5. Rules Matrix

### 5.1 Rules Tested
| Rule ID | Severity | Test Case(s) |
|---------|----------|-------------|
| CHECK_INVALID_EXPR_TYPE | error | `9_1_CHECK_INVALID_EXPR_TYPE-runtime_signal_in_check.jz` |

### 5.2 Rules Not Tested
| Rule ID | Severity | Gap Description |
|---------|----------|-----------------|
| -- | -- | All section 9.1 rules covered |
