# Test Plan: 10.5 Template Application
**Specification Reference:** Section 10.5 of jz-hdl-specification.md
## 1. Objective
Verify @apply syntax, argument count matching, count parameter (0/1/N), IDX substitution, placement (inside ASYNC/SYNC only), and expansion semantics.
## 2. Instrumentation Strategy
- **Span: `template.apply`** — attributes: `template_id`, `count`, `arg_count`, `idx_range`.
## 3. Test Scenarios
### 3.1 Happy Path
1. Simple @apply with matching args
2. @apply with count > 1 (IDX substitution)
3. @apply count=0 (no-op)
4. IDX in slice bounds
### 3.3 Negative Testing
1. Undefined template — Error (TEMPLATE_UNDEFINED)
2. Arg count mismatch — Error (TEMPLATE_ARG_COUNT_MISMATCH)
3. Non-nonneg count — Error (TEMPLATE_COUNT_NOT_NONNEG_INT)
4. @apply outside block — Error (TEMPLATE_APPLY_OUTSIDE_BLOCK)
5. IDX in runtime expression — Error
## 4. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Undefined template | Error | TEMPLATE_UNDEFINED | S10.5 |
| 2 | Arg count mismatch | Error | TEMPLATE_ARG_COUNT_MISMATCH | S10.5 |
| 3 | Bad count | Error | TEMPLATE_COUNT_NOT_NONNEG_INT | S10.5 |
| 4 | @apply outside block | Error | TEMPLATE_APPLY_OUTSIDE_BLOCK | S10.5 |
## 5-6. See 10.2 for integration.
