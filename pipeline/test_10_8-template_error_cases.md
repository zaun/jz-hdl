# Test Plan: 10.8 Template Error Cases
**Specification Reference:** Section 10.8 of jz-hdl-specification.md

## 1. Objective
Verify the canonical error case summary for templates. This section cross-references all TEMPLATE_* error conditions defined in sections 10.2-10.6 and confirms each produces the correct diagnostic.

## 2. Test Scenarios

### 2.1 Happy Path
No happy-path scenarios — this section enumerates error cases only.

### 2.2 Error Cases
1. Duplicate template name — Error (TEMPLATE_DUP_NAME, see S10.2)
2. Duplicate parameter name — Error (TEMPLATE_DUP_PARAM, see S10.2)
3. External signal reference in template — Error (TEMPLATE_EXTERNAL_REF, see S10.3)
4. @scratch outside template — Error (TEMPLATE_SCRATCH_OUTSIDE, see S10.3)
5. @scratch with non-constant width — Error (TEMPLATE_SCRATCH_WIDTH_INVALID, see S10.3)
6. Declaration block in template — Error (TEMPLATE_FORBIDDEN_DECL, see S10.4)
7. SYNC/ASYNC header in template — Error (TEMPLATE_FORBIDDEN_BLOCK_HEADER, see S10.4)
8. Structural directive in template — Error (TEMPLATE_FORBIDDEN_DIRECTIVE, see S10.4)
9. Nested @template definition — Error (TEMPLATE_NESTED_DEF, see S10.4)
10. Undefined template reference — Error (TEMPLATE_UNDEFINED, see S10.5)
11. Argument count mismatch — Error (TEMPLATE_ARG_COUNT_MISMATCH, see S10.5)
12. Non-nonneg count expression — Error (TEMPLATE_COUNT_NOT_NONNEG_INT, see S10.5)
13. @apply outside block — Error (TEMPLATE_APPLY_OUTSIDE_BLOCK, see S10.5)
14. Exclusive assignment violation via expansion — Error (ASSIGN_MULTIPLE_SAME_BITS / SYNC_MULTI_ASSIGN_SAME_REG_BITS, see S10.6)

### 2.3 Edge Cases
No additional edge cases — all edge cases are covered in their respective subsection plans.

## 3. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Source Section |
|---|-------|----------------|---------|----------------|
| 1 | Duplicate template name | Error | TEMPLATE_DUP_NAME | S10.2 |
| 2 | Duplicate param name | Error | TEMPLATE_DUP_PARAM | S10.2 |
| 3 | External signal ref | Error | TEMPLATE_EXTERNAL_REF | S10.3 |
| 4 | @scratch outside template | Error | TEMPLATE_SCRATCH_OUTSIDE | S10.3 |
| 5 | Non-constant scratch width | Error | TEMPLATE_SCRATCH_WIDTH_INVALID | S10.3 |
| 6 | Declaration in template | Error | TEMPLATE_FORBIDDEN_DECL | S10.4 |
| 7 | SYNC/ASYNC header in template | Error | TEMPLATE_FORBIDDEN_BLOCK_HEADER | S10.4 |
| 8 | Structural directive in template | Error | TEMPLATE_FORBIDDEN_DIRECTIVE | S10.4 |
| 9 | Nested @template | Error | TEMPLATE_NESTED_DEF | S10.4 |
| 10 | Undefined template | Error | TEMPLATE_UNDEFINED | S10.5 |
| 11 | Arg count mismatch | Error | TEMPLATE_ARG_COUNT_MISMATCH | S10.5 |
| 12 | Bad count expression | Error | TEMPLATE_COUNT_NOT_NONNEG_INT | S10.5 |
| 13 | @apply outside block | Error | TEMPLATE_APPLY_OUTSIDE_BLOCK | S10.5 |
| 14 | Double @apply same bits | Error | ASSIGN_MULTIPLE_SAME_BITS | S10.6 |
| 15 | Double @apply same reg bits | Error | SYNC_MULTI_ASSIGN_SAME_REG_BITS | S10.6 |

## 4. Existing Validation Tests
Cross-references to all template validation tests:

| Test File | Rule ID | Source Section |
|-----------|---------|----------------|
| `10_2_TEMPLATE_DUP_NAME-duplicate_template_names.jz` | TEMPLATE_DUP_NAME | S10.2 |
| `10_2_TEMPLATE_DUP_PARAM-duplicate_param_names.jz` | TEMPLATE_DUP_PARAM | S10.2 |
| `10_3_TEMPLATE_EXTERNAL_REF-external_signal_reference.jz` | TEMPLATE_EXTERNAL_REF | S10.3 |
| `10_3_TEMPLATE_SCRATCH_OUTSIDE-scratch_outside_template.jz` | TEMPLATE_SCRATCH_OUTSIDE | S10.3 |
| `10_3_TEMPLATE_SCRATCH_WIDTH_INVALID-scratch_width_not_constant.jz` | TEMPLATE_SCRATCH_WIDTH_INVALID | S10.3 |
| `10_4_TEMPLATE_FORBIDDEN_DECL-declaration_blocks_in_template.jz` | TEMPLATE_FORBIDDEN_DECL | S10.4 |
| `10_4_TEMPLATE_FORBIDDEN_BLOCK_HEADER-sync_async_in_template.jz` | TEMPLATE_FORBIDDEN_BLOCK_HEADER | S10.4 |
| `10_4_TEMPLATE_FORBIDDEN_DIRECTIVE-structural_directives_in_template.jz` | TEMPLATE_FORBIDDEN_DIRECTIVE | S10.4 |
| `10_4_TEMPLATE_NESTED_DEF-nested_template_definition.jz` | TEMPLATE_NESTED_DEF | S10.4 |
| `10_5_TEMPLATE_UNDEFINED-undefined_template_ref.jz` | TEMPLATE_UNDEFINED | S10.5 |
| `10_5_TEMPLATE_ARG_COUNT_MISMATCH-wrong_arg_count.jz` | TEMPLATE_ARG_COUNT_MISMATCH | S10.5 |
| `10_5_TEMPLATE_COUNT_NOT_NONNEG_INT-bad_count_expr.jz` | TEMPLATE_COUNT_NOT_NONNEG_INT | S10.5 |
| `10_6_ASSIGN_MULTIPLE_SAME_BITS-template_double_apply_async.jz` | ASSIGN_MULTIPLE_SAME_BITS | S10.6 |
| `10_6_SYNC_MULTI_ASSIGN_SAME_REG_BITS-template_double_apply_sync.jz` | SYNC_MULTI_ASSIGN_SAME_REG_BITS | S10.6 |

## 5. Rules Matrix

### 5.1 Rules Tested
| Rule ID | Description | Tested In |
|---------|-------------|-----------|
| TEMPLATE_DUP_NAME | Duplicate template name in same scope | S10.2 |
| TEMPLATE_DUP_PARAM | Duplicate parameter name in definition | S10.2 |
| TEMPLATE_EXTERNAL_REF | External signal must be passed as argument | S10.3 |
| TEMPLATE_SCRATCH_OUTSIDE | @scratch only inside @template body | S10.3 |
| TEMPLATE_SCRATCH_WIDTH_INVALID | @scratch width must be positive constant | S10.3 |
| TEMPLATE_FORBIDDEN_DECL | No declaration blocks inside template | S10.4 |
| TEMPLATE_FORBIDDEN_BLOCK_HEADER | No SYNC/ASYNC headers inside template | S10.4 |
| TEMPLATE_FORBIDDEN_DIRECTIVE | No structural directives inside template | S10.4 |
| TEMPLATE_NESTED_DEF | No nested @template definitions | S10.4 |
| TEMPLATE_UNDEFINED | @apply references undefined template | S10.5 |
| TEMPLATE_ARG_COUNT_MISMATCH | @apply arg count mismatch | S10.5 |
| TEMPLATE_COUNT_NOT_NONNEG_INT | @apply count not non-negative integer | S10.5 |
| TEMPLATE_APPLY_OUTSIDE_BLOCK | @apply outside ASYNC/SYNC block | S10.5 |
| ASSIGN_MULTIPLE_SAME_BITS | Exclusive assignment violation via expansion | S10.6 |
| SYNC_MULTI_ASSIGN_SAME_REG_BITS | Sync exclusive assignment violation via expansion | S10.6 |

### 5.2 Rules Not Tested
| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| — | — | All template-related rules covered across S10.2-S10.6 |
