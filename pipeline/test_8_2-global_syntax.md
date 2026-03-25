# Test Plan: 8.2 Global Syntax

**Specification Reference:** Section 8.2 of jz-hdl-specification.md

## 1. Objective
Verify that `@global`/`@endglob` syntax is parsed correctly, that constants must be sized literals (explicit `<width>'<base><value>`), and that non-sized values (bare integers, CONFIG references, cross-global references, expressions) are rejected with GLOBAL_INVALID_EXPR_TYPE.

## 2. Test Scenarios

### 2.1 Happy Path
1. `@global ISA` with multiple 17-bit binary literals parses and resolves correctly
2. `@global STATUS` with hex-format sized literals parses correctly
3. Empty `@global EMPTY @endglob` block is accepted without errors
4. Constants from multiple blocks are usable in module SYNC/ASYNC contexts

### 2.2 Error Cases
1. Bare decimal integer (`X = 42;`) in `@global` produces GLOBAL_INVALID_EXPR_TYPE error
2. Bare zero (`Y = 0;`) in `@global` produces GLOBAL_INVALID_EXPR_TYPE error
3. CONFIG reference as value (`W = CONFIG.WIDTH;`) produces GLOBAL_INVALID_EXPR_TYPE error
4. Cross-global reference (`R = VALID.A;`) produces GLOBAL_INVALID_EXPR_TYPE error
5. Expression as value (`E = 8'hFF + 1;`) produces GLOBAL_INVALID_EXPR_TYPE error

### 2.3 Edge Cases
1. Empty `@global` block (no constants between `@global` and `@endglob`)
2. Single constant in a `@global` block

## 3. Input/Output Matrix
| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1 | Bare decimal integer | `X = 42;` | GLOBAL_INVALID_EXPR_TYPE | error |
| 2 | Bare zero | `Y = 0;` | GLOBAL_INVALID_EXPR_TYPE | error |
| 3 | CONFIG reference as global value | `W = CONFIG.WIDTH;` | GLOBAL_INVALID_EXPR_TYPE | error |
| 4 | Cross-global reference | `R = VALID.A;` | GLOBAL_INVALID_EXPR_TYPE | error |
| 5 | Expression (addition) as value | `E = 8'hFF + 1;` | GLOBAL_INVALID_EXPR_TYPE | error |

## 4. Existing Validation Tests
| Test File | Rule Tested | Triggers |
|-----------|-------------|----------|
| `8_2_HAPPY_PATH-global_syntax_ok.jz` | (none -- clean) | 0 diagnostics |
| `8_2_GLOBAL_INVALID_EXPR_TYPE-unsized_value.jz` | GLOBAL_INVALID_EXPR_TYPE | 5 triggers |

## 5. Rules Matrix

### 5.1 Rules Tested
| Rule ID | Severity | Test Case(s) |
|---------|----------|-------------|
| GLOBAL_INVALID_EXPR_TYPE | error | `8_2_GLOBAL_INVALID_EXPR_TYPE-unsized_value.jz` |

### 5.2 Rules Not Tested
| Rule ID | Severity | Gap Description |
|---------|----------|-----------------|
| -- | -- | All section 8.2 rules covered |
