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
| 1 | Condition not width-1 | `@feature 8'd255` -- must be width-1 boolean -- Error |
| 2 | Runtime signal in expr | `@feature wire_val == 1` -- Error: must be CONFIG/CONST |
| 3 | Nested @feature | `@feature X @feature Y ... @endfeat @endfeat` -- Error |
| 4 | Both-path validation fail | Feature guards driver of OUT port, no @else -- Error in disabled config |
| 5 | Missing @endfeat | `@feature X` without closing -- Error |
| 6 | @else without @feature | Standalone `@else` -- Error |
| 7 | Reference outside guard | Feature declares wire, used outside guard -- Error in disabled config |
| 8 | Semantic error in branch | `@feature` where one branch has width mismatch -- both must validate -- Error |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Feature always true | CONST makes condition always true |
| 2 | Feature always false | CONST makes condition always false |
| 3 | Empty feature body | `@feature X == 1 @endfeat` -- valid |
| 4 | Feature around entire module body | Guards declarations + logic |

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1 | Condition not width-1 | `@feature 8'd255` | FEATURE_COND_WIDTH_NOT_1 | error |
| 2 | Runtime signal in expr | `@feature wire_val == 1` | FEATURE_EXPR_INVALID_CONTEXT | error |
| 3 | Nested @feature | `@feature X @feature Y @endfeat @endfeat` | FEATURE_NESTED | error |
| 4 | Both-path validation fail | Missing @else, disabled path has undeclared identifier | FEATURE_VALIDATION_BOTH_PATHS | error |
| 5 | Valid @feature with @else | Both configurations pass semantic checks | -- | -- (pass) |
| 6 | Semantic error in branch | `@feature` branch contains width mismatch, both paths must validate | FEATURE_VALIDATION_BOTH_PATHS | error |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 4_14_HAPPY_PATH-feature_guards_ok.jz | -- | Happy path: valid @feature with @else, both paths valid |
| 4_14_FEATURE_COND_WIDTH_NOT_1-wide_condition.jz | FEATURE_COND_WIDTH_NOT_1 | Feature guard condition not width-1 boolean |
| 4_14_FEATURE_EXPR_INVALID_CONTEXT-runtime_signal_in_condition.jz | FEATURE_EXPR_INVALID_CONTEXT | Runtime signal used in feature guard expression |
| 4_14_FEATURE_NESTED-nested_feature_in_async.jz | FEATURE_NESTED | Nested @feature in ASYNCHRONOUS block |
| 4_14_FEATURE_NESTED-nested_feature_in_else.jz | FEATURE_NESTED | Nested @feature in @else branch |
| 4_14_FEATURE_NESTED-nested_feature_in_sync.jz | FEATURE_NESTED | Nested @feature in SYNCHRONOUS block |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| FEATURE_COND_WIDTH_NOT_1 | error | S4.14 Feature guard condition must evaluate to a width-1 boolean | 4_14_FEATURE_COND_WIDTH_NOT_1-wide_condition.jz |
| FEATURE_EXPR_INVALID_CONTEXT | error | S4.14 Feature guard expression must use only CONFIG or CONST values, not runtime signals | 4_14_FEATURE_EXPR_INVALID_CONTEXT-runtime_signal_in_condition.jz |
| FEATURE_NESTED | error | S4.14 @feature blocks may not be nested | 4_14_FEATURE_NESTED-nested_feature_in_async.jz, 4_14_FEATURE_NESTED-nested_feature_in_else.jz, 4_14_FEATURE_NESTED-nested_feature_in_sync.jz |
| FEATURE_VALIDATION_BOTH_PATHS | error | S4.14 Both branches of @feature guard must pass full semantic validation | 4_14_FEATURE_VALIDATION_BOTH_PATHS-semantic_error_in_branch.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| -- | -- | All rules tested |
