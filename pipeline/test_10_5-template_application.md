# Test Plan: 10.5 Template Application
**Specification Reference:** Section 10.5 of jz-hdl-specification.md

## 1. Objective
Verify @apply syntax, argument count matching, count parameter (0/1/N), IDX substitution, placement restrictions (inside ASYNC/SYNC only), and expansion semantics.

## 2. Test Scenarios

### 2.1 Happy Path
1. Simple @apply with matching argument count — valid expansion
2. @apply with count > 1 — IDX substitution for each iteration
3. @apply with count=0 — no-op, no expansion
4. IDX used in slice bounds — correct index substitution
5. @apply with count=1 — single expansion, IDX=0

### 2.2 Error Cases
1. Reference to undefined template — Error (TEMPLATE_UNDEFINED)
2. Argument count does not match parameter count — Error (TEMPLATE_ARG_COUNT_MISMATCH)
3. Count expression not a non-negative integer — Error (TEMPLATE_COUNT_NOT_NONNEG_INT)
4. @apply outside ASYNC/SYNC block — Error (TEMPLATE_APPLY_OUTSIDE_BLOCK)

### 2.3 Edge Cases
1. @apply with count from CONST expression — valid if resolvable
2. IDX used in concatenation or complex expression — valid
3. @apply with large count value — valid but generates many expansions
4. IDX referenced in runtime expression — Error

## 3. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Undefined template ref | Error | TEMPLATE_UNDEFINED | S10.5 |
| 2 | Wrong argument count | Error | TEMPLATE_ARG_COUNT_MISMATCH | S10.5 |
| 3 | Non-nonneg count expr | Error | TEMPLATE_COUNT_NOT_NONNEG_INT | S10.5 |
| 4 | @apply outside block | Error | TEMPLATE_APPLY_OUTSIDE_BLOCK | S10.5 |

## 4. Existing Validation Tests
| Test File | Rule ID | Description |
|-----------|---------|-------------|
| `10_5_HAPPY_PATH-template_application_ok.jz` | — | Happy-path: valid @apply with correct args, count, IDX |
| `10_5_TEMPLATE_UNDEFINED-undefined_template_ref.jz` | TEMPLATE_UNDEFINED | @apply references a template that does not exist |
| `10_5_TEMPLATE_ARG_COUNT_MISMATCH-wrong_arg_count.jz` | TEMPLATE_ARG_COUNT_MISMATCH | Argument count differs from parameter count |
| `10_5_TEMPLATE_COUNT_NOT_NONNEG_INT-bad_count_expr.jz` | TEMPLATE_COUNT_NOT_NONNEG_INT | Count expression is negative or non-integer |
| `10_5_TEMPLATE_APPLY_OUTSIDE_BLOCK-apply_at_file_scope.jz` | TEMPLATE_APPLY_OUTSIDE_BLOCK | @apply used at file scope outside any block |
| `10_5_TEMPLATE_APPLY_OUTSIDE_BLOCK-apply_at_module_scope.jz` | TEMPLATE_APPLY_OUTSIDE_BLOCK | @apply used at module scope outside any block |

## 5. Rules Matrix

### 5.1 Rules Tested
| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| TEMPLATE_UNDEFINED | @apply references undefined template | `10_5_TEMPLATE_UNDEFINED-undefined_template_ref.jz` |
| TEMPLATE_ARG_COUNT_MISMATCH | @apply argument count does not match template parameter count | `10_5_TEMPLATE_ARG_COUNT_MISMATCH-wrong_arg_count.jz` |
| TEMPLATE_COUNT_NOT_NONNEG_INT | @apply count expression does not resolve to a non-negative integer | `10_5_TEMPLATE_COUNT_NOT_NONNEG_INT-bad_count_expr.jz` |
| TEMPLATE_APPLY_OUTSIDE_BLOCK | @apply may only appear inside ASYNCHRONOUS or SYNCHRONOUS blocks | `10_5_TEMPLATE_APPLY_OUTSIDE_BLOCK-apply_at_file_scope.jz`, `10_5_TEMPLATE_APPLY_OUTSIDE_BLOCK-apply_at_module_scope.jz` |

### 5.2 Rules Not Tested
| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| — | — | All S10.5 rules covered |
