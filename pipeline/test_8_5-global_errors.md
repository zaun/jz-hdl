# Test Plan: 8.5 Global Errors

**Specification Reference:** Section 8.5 of jz-hdl-specification.md

## 1. Objective
Verify all error conditions enumerated in S8.5: forward references within a `@global` block, circular dependencies between constants, literal overflow (intrinsic width exceeds declared width), and that valid blocks with no such issues compile cleanly.

## 2. Test Scenarios

### 2.1 Happy Path
1. Well-formed `@global` blocks (`OPCODES`, `FLAGS`) with no forward references, circular dependencies, or overflows -- no diagnostics
2. Global constants referenced in sync blocks across modules

### 2.2 Error Cases
1. Forward reference (`B = C;` before `C` is defined) produces GLOBAL_FORWARD_REF error
2. Second forward reference (`D = E;` before `E` is defined) produces GLOBAL_FORWARD_REF error
3. Direct circular dependency (`A = B; B = A;`) produces GLOBAL_CIRCULAR_DEP (and GLOBAL_FORWARD_REF on the first)
4. Self-reference (`X = X;`) produces GLOBAL_CIRCULAR_DEP
5. Literal overflow (`4'hFF` requires 8 bits but only 4 declared) produces GLOBAL_INVALID_EXPR_TYPE
6. Literal overflow (`2'b111` requires 3 bits but only 2 declared) produces GLOBAL_INVALID_EXPR_TYPE
7. Literal overflow in second block (`1'b11`) produces GLOBAL_INVALID_EXPR_TYPE

### 2.3 Edge Cases
1. Forward reference that would resolve if order were reversed -- still an error
2. Literal at exact maximum width boundary (e.g., `8'hFF`) -- no overflow
3. Circular dependency detected alongside forward reference in the same block

## 3. Input/Output Matrix
| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1 | Forward ref: B references later C | `B = C;` | GLOBAL_FORWARD_REF | error |
| 2 | Forward ref: D references later E | `D = E;` | GLOBAL_FORWARD_REF | error |
| 3 | Forward ref in circular: A references B | `A = B;` | GLOBAL_FORWARD_REF | error |
| 4 | Circular dep: B references A | `B = A;` | GLOBAL_CIRCULAR_DEP | error |
| 5 | Self-reference | `X = X;` | GLOBAL_CIRCULAR_DEP | error |
| 6 | Literal overflow: 4'hFF | `OVER_A = 4'hFF;` | GLOBAL_INVALID_EXPR_TYPE | error |
| 7 | Literal overflow: 2'b111 | `OVER_B = 2'b111;` | GLOBAL_INVALID_EXPR_TYPE | error |
| 8 | Literal overflow: 1'b11 | `OVER_X = 1'b11;` | GLOBAL_INVALID_EXPR_TYPE | error |

## 4. Existing Validation Tests
| Test File | Rule Tested | Triggers |
|-----------|-------------|----------|
| `8_5_HAPPY_PATH-global_errors_ok.jz` | (none -- clean) | 0 diagnostics |
| `8_5_GLOBAL_FORWARD_REF-forward_ref_in_global.jz` | GLOBAL_FORWARD_REF | 2 triggers |
| `8_5_GLOBAL_CIRCULAR_DEP-circular_dependency.jz` | GLOBAL_CIRCULAR_DEP, GLOBAL_FORWARD_REF | 3 triggers (1 FORWARD_REF + 2 CIRCULAR_DEP) |
| `8_5_LIT_OVERFLOW-global_literal_overflow.jz` | GLOBAL_INVALID_EXPR_TYPE | 3 triggers |

## 5. Rules Matrix

### 5.1 Rules Tested
| Rule ID | Severity | Test Case(s) |
|---------|----------|-------------|
| GLOBAL_FORWARD_REF | error | `8_5_GLOBAL_FORWARD_REF-forward_ref_in_global.jz`, `8_5_GLOBAL_CIRCULAR_DEP-circular_dependency.jz` |
| GLOBAL_CIRCULAR_DEP | error | `8_5_GLOBAL_CIRCULAR_DEP-circular_dependency.jz` |
| GLOBAL_INVALID_EXPR_TYPE | error | `8_5_LIT_OVERFLOW-global_literal_overflow.jz` |
| GLOBAL_ASSIGN_FORBIDDEN | error | (covered by `8_1_GLOBAL_ASSIGN_FORBIDDEN-assign_to_global.jz` in test_8_1) |

### 5.2 Rules Not Tested
| Rule ID | Severity | Gap Description |
|---------|----------|-----------------|
| -- | -- | All section 8.5 error conditions have validation tests |
