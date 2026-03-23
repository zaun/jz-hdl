# Test Plan: 8.2 Global Syntax

**Specification Reference:** Section 8.2 of jz-hdl-specification.md

## 1. Objective
Verify @global/@endglob syntax, sized literal requirement for values, and namespace root creation.

## 2. Test Scenarios

### 2.1 Happy Path
1. `@global ISA INST_ADD = 17'b0110011_0000000_000; @endglob` parses correctly
2. Multiple entries in one @global block

### 2.2 Error Cases
1. Bare integer as value (`CONST_A = 42;`) produces GLOBAL_INVALID_EXPR_TYPE error (not sized)
2. CONFIG reference as value produces error
3. Missing @endglob produces parse error

### 2.3 Edge Cases
1. Empty @global block

## 3. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Bare integer (unsized) in @global | Error | GLOBAL_INVALID_EXPR_TYPE | S8.2 sized literal required |

## 4. Existing Validation Tests
| Test File | Rule Tested |
|-----------|-------------|
| `8_2_GLOBAL_INVALID_EXPR_TYPE-unsized_value.jz` | GLOBAL_INVALID_EXPR_TYPE |

## 5. Rules Matrix

### 5.1 Rules Tested
| Rule ID | Severity | Test Case(s) |
|---------|----------|-------------|
| GLOBAL_INVALID_EXPR_TYPE | error | `8_2_GLOBAL_INVALID_EXPR_TYPE-unsized_value.jz` |

### 5.2 Rules Not Tested
| Rule ID | Severity | Gap Description |
|---------|----------|-----------------|
| — | — | All section 8.2 rules covered |
