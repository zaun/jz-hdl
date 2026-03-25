# Test Plan: 9.4 @check Expression Rules

**Specification Reference:** Section 9.4 of jz-hdl-specification.md

## 1. Objective
Verify the allowed and forbidden operands in `@check` expressions. Allowed: integer literals, CONST identifiers, `CONFIG.<name>` identifiers, compile-time integer operators, `clog2()`, parentheses, comparisons (`==`, `!=`, `<`, `<=`, `>`, `>=`), logical operators (`&&`, `||`, `!`). Forbidden: any module port, wire, register, memory port, signal slice, or runtime expression.

## 2. Test Scenarios

### 2.1 Happy Path
1. Integer literal in @check expression (`@check (1, ...)`)
2. CONST reference in @check expression (`@check (SIZE == 16, ...)`)
3. CONFIG reference in @check expression (`@check (CONFIG.WIDTH == 8, ...)`)
4. clog2 intrinsic in @check expression (`@check (clog2(CONFIG.DEPTH) == 8, ...)`)
5. Logical AND (`@check (CONFIG.WIDTH > 0 && CONFIG.DEPTH > 0, ...)`)
6. Logical OR (`@check (CONFIG.WIDTH == 8 || CONFIG.WIDTH == 16, ...)`)
7. Comparison operators: `>=`, `!=` (`@check (SIZE >= 16, ...)`, `@check (SIZE != 0, ...)`)
8. Nested expressions mixing CONST + literal (`LOCAL_W + 4 == 8`)
9. Nested expressions mixing clog2 + CONFIG + CONST (`clog2(SIZE) + CONFIG.WIDTH > 0`)
10. CONST arithmetic (`LOCAL_W * 2 == 8`)

### 2.2 Error Cases
1. Bare port reference in helper module (`din == 8'h00`) produces CHECK_INVALID_EXPR_TYPE
2. Bare register reference in helper module (`r == 8'h00`) produces CHECK_INVALID_EXPR_TYPE
3. Port bit-slice in helper module (`din[3:0] == 4'h0`) produces CHECK_INVALID_EXPR_TYPE
4. Register bit-slice in helper module (`r[7:4] == 4'h0`) produces CHECK_INVALID_EXPR_TYPE
5. Bare port reference in top module (`din == 8'hFF`) produces CHECK_INVALID_EXPR_TYPE
6. Bare register reference in top module (`r == 8'h00`) produces CHECK_INVALID_EXPR_TYPE
7. Bare wire reference in top module (`w == 8'h00`) produces CHECK_INVALID_EXPR_TYPE
8. Port bit-slice in top module (`din[7:4] == 4'hF`) produces CHECK_INVALID_EXPR_TYPE
9. Register bit-index in top module (`r[0] == 1'b0`) produces CHECK_INVALID_EXPR_TYPE
10. Wire bit-slice in top module (`w[3:0] == 4'h0`) produces CHECK_INVALID_EXPR_TYPE

### 2.3 Edge Cases
1. Valid CONST and CONFIG checks coexist with forbidden operand checks in the same file -- only forbidden ones trigger
2. Nested expressions mixing allowed operands (CONST + literal) pass cleanly
3. clog2 of CONST passes while port reference in same module fails

## 3. Input/Output Matrix
| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1 | Port in helper @check | `din == 8'h00` | CHECK_INVALID_EXPR_TYPE | error |
| 2 | Register in helper @check | `r == 8'h00` | CHECK_INVALID_EXPR_TYPE | error |
| 3 | Port slice in helper @check | `din[3:0] == 4'h0` | CHECK_INVALID_EXPR_TYPE | error |
| 4 | Register slice in helper @check | `r[7:4] == 4'h0` | CHECK_INVALID_EXPR_TYPE | error |
| 5 | Port in top @check | `din == 8'hFF` | CHECK_INVALID_EXPR_TYPE | error |
| 6 | Register in top @check | `r == 8'h00` | CHECK_INVALID_EXPR_TYPE | error |
| 7 | Wire in top @check | `w == 8'h00` | CHECK_INVALID_EXPR_TYPE | error |
| 8 | Port slice in top @check | `din[7:4] == 4'hF` | CHECK_INVALID_EXPR_TYPE | error |
| 9 | Register bit-index in top @check | `r[0] == 1'b0` | CHECK_INVALID_EXPR_TYPE | error |
| 10 | Wire slice in top @check | `w[3:0] == 4'h0` | CHECK_INVALID_EXPR_TYPE | error |

## 4. Existing Validation Tests
| Test File | Rule Tested | Triggers |
|-----------|-------------|----------|
| `9_4_HAPPY_PATH-check_expression_ok.jz` | (none -- clean) | 0 diagnostics |
| `9_4_CHECK_INVALID_EXPR_TYPE-forbidden_operands.jz` | CHECK_INVALID_EXPR_TYPE | 10 triggers |

## 5. Rules Matrix

### 5.1 Rules Tested
| Rule ID | Severity | Test Case(s) |
|---------|----------|-------------|
| CHECK_INVALID_EXPR_TYPE | error | `9_4_CHECK_INVALID_EXPR_TYPE-forbidden_operands.jz` (ports, registers, wires, slices) |

### 5.2 Rules Not Tested
| Rule ID | Severity | Gap Description |
|---------|----------|-----------------|
| -- | -- | All section 9.4 rules covered; memory port field references in @check are not explicitly tested but would require a mem declaration within module scope and @check referencing `mem.port.field`, which is covered implicitly by the general runtime-signal rejection |
