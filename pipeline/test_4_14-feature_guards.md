# Test Plan: 4.14 Feature Guards

**Specification Reference:** Section 4.14 of jz-hdl-specification.md

## 1. Objective

Verify @feature/@else/@endfeat conditional compilation: expression evaluation (CONFIG/CONST only), both-configuration validation (enabled AND disabled must pass semantic checks), no nesting, scope transparency (identifiers inside are module-visible), and error detection for missing @else when needed, reference to feature-guarded identifiers outside guards, and runtime signal references in feature expressions.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Feature with @else | Both branches provide valid semantics |
| 2 | Feature in REGISTER block | Conditional register declaration |
| 3 | Feature in ASYNCHRONOUS | Conditional assignments |
| 4 | Feature in SYNCHRONOUS | Conditional sequential logic |
| 5 | Feature with CONST expr | `@feature INC == 1` using module CONST |
| 6 | Feature with CONFIG expr | `@feature CONFIG.DEBUG == 1` using project CONFIG |
| 7 | Logical operators | `@feature A == 1 && B == 0` |
| 8 | Comparison operators | `@feature WIDTH > 16` |

### 2.2 Error Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Condition not width-1 | `@feature 8'd255` — must be width-1 boolean — Error |
| 2 | Runtime signal in expr | `@feature wire_val == 1` — Error: must be CONFIG/CONST |
| 3 | Nested @feature | `@feature X @feature Y ... @endfeat @endfeat` — Error |
| 4 | Both-path validation fail | Feature guards driver of OUT port, no @else — Error in disabled config |
| 5 | Missing @endfeat | `@feature X` without closing — Error |
| 6 | @else without @feature | Standalone `@else` — Error |
| 7 | Reference outside guard | Feature declares wire, used outside guard — Error in disabled config |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Feature always true | CONST makes condition always true |
| 2 | Feature always false | CONST makes condition always false |
| 3 | Empty feature body | `@feature X == 1 @endfeat` — valid |
| 4 | Feature around entire module body | Guards declarations + logic |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Condition not width-1 | Error | FEATURE_COND_WIDTH_NOT_1 | S4.14 |
| 2 | Runtime signal in expr | Error | FEATURE_EXPR_INVALID_CONTEXT | S4.14 |
| 3 | Nested @feature | Error | FEATURE_NESTED | S4.14 |
| 4 | Both-path validation fail | Error | FEATURE_VALIDATION_BOTH_PATHS | S4.14 |
| 5 | Valid @feature with @else | Valid in both configurations | — | Happy path |

## 4. Existing Validation Tests

| Test File | Rule Tested |
|-----------|-------------|
| 4_14_FEATURE_COND_WIDTH_NOT_1-wide_condition.jz | FEATURE_COND_WIDTH_NOT_1 |
| 4_14_FEATURE_EXPR_INVALID_CONTEXT-runtime_signal_in_condition.jz | FEATURE_EXPR_INVALID_CONTEXT |
| 4_14_FEATURE_NESTED-nested_feature_in_sync.jz | FEATURE_NESTED |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Test File(s) |
|---------|----------|--------------|
| FEATURE_COND_WIDTH_NOT_1 | error | 4_14_FEATURE_COND_WIDTH_NOT_1-wide_condition.jz |
| FEATURE_EXPR_INVALID_CONTEXT | error | 4_14_FEATURE_EXPR_INVALID_CONTEXT-runtime_signal_in_condition.jz |
| FEATURE_NESTED | error | 4_14_FEATURE_NESTED-nested_feature_in_sync.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Gap Description |
|---------|----------|-----------------|
| FEATURE_VALIDATION_BOTH_PATHS | error | Both enabled and disabled configurations must pass semantic checks |
