# Rules Not Tested

## Section 1.2 — Fundamental Terms

All rules from this section are tested:
- `NET_FLOATING_WITH_SINK` — tested in `1_2_NET_FLOATING_WITH_SINK-floating_net_read.jz`
- `NET_TRI_STATE_ALL_Z_READ` — tested in `1_2_NET_TRI_STATE_ALL_Z_READ-all_z_drivers_read.jz`
- `OBS_X_TO_OBSERVABLE_SINK` — tested in `1_2_OBS_X_TO_OBSERVABLE_SINK-x_bits_to_observable.jz`
- `REG_INIT_CONTAINS_X` — tested in `1_2_REG_INIT_CONTAINS_X-x_in_register_init.jz`
- `DOMAIN_CONFLICT` — tested in `1_2_DOMAIN_CONFLICT-register_wrong_domain.jz`
- `MULTI_CLK_ASSIGN` — tested in `1_2_MULTI_CLK_ASSIGN-register_multi_clock.jz`
- `LATCH_ASSIGN_IN_SYNC` — tested in `1_2_LATCH_ASSIGN_IN_SYNC-latch_written_in_sync.jz`
- `NET_MULTIPLE_ACTIVE_DRIVERS` — tested in `1_2_NET_MULTIPLE_ACTIVE_DRIVERS-multi_driver.jz`
- `COMB_LOOP_UNCONDITIONAL` — tested in `1_2_COMB_LOOP_UNCONDITIONAL-unconditional_loop.jz`
- `COMB_LOOP_CONDITIONAL_SAFE` — tested in `1_2_COMB_LOOP_CONDITIONAL_SAFE-conditional_safe.jz`

Rules not directly testable:
- `NET_DANGLING_UNUSED` — WIRE declarations trigger `WARN_UNUSED_WIRE` instead. See `issues.md` for details. The test `1_2_NET_DANGLING_UNUSED-unused_signal.jz` captures the actual compiler behavior (`WARN_UNUSED_WIRE`).

## Section 1.6 — High-Impedance and Tri-State Logic

All rules from this section are tested:
- `LIT_DECIMAL_HAS_XZ` — tested in `1_6_LIT_DECIMAL_HAS_XZ-z_in_decimal_literal.jz`
- `LIT_INVALID_DIGIT_FOR_BASE` — tested in `1_6_LIT_INVALID_DIGIT_FOR_BASE-z_in_hex_literal.jz`
- `REG_INIT_CONTAINS_Z` — tested in `1_6_REG_INIT_CONTAINS_Z-z_in_register_init.jz`
- `PORT_TRISTATE_MISMATCH` — tested in `1_6_PORT_TRISTATE_MISMATCH-z_on_non_inout_port.jz`
- `NET_TRI_STATE_ALL_Z_READ` — tested in `1_6_NET_TRI_STATE_ALL_Z_READ-all_z_read.jz`

Cross-referenced rules tested in other sections:
- `NET_FLOATING_WITH_SINK` — tested in `1_2_NET_FLOATING_WITH_SINK-floating_net_read.jz` (section 1.2)
- `NET_MULTIPLE_ACTIVE_DRIVERS` — tested in `1_2_NET_MULTIPLE_ACTIVE_DRIVERS-multi_driver.jz` (section 1.2) and `11_3_NET_MULTIPLE_ACTIVE_DRIVERS-multi_driver_non_tristate.jz` (section 11.3)
- `NET_DANGLING_UNUSED` — tested in `1_2_NET_DANGLING_UNUSED-unused_signal.jz` (section 1.2)
- `OBS_X_TO_OBSERVABLE_SINK` — tested in `1_2_OBS_X_TO_OBSERVABLE_SINK-x_bits_to_observable.jz` (section 1.2)
- `COMB_LOOP_UNCONDITIONAL` — tested in `1_2_COMB_LOOP_UNCONDITIONAL-unconditional_loop.jz` (section 1.2) and `12_2_COMB_LOOP_UNCONDITIONAL-*.jz` (section 12.2)
- `COMB_LOOP_CONDITIONAL_SAFE` — tested in `1_2_COMB_LOOP_CONDITIONAL_SAFE-conditional_safe.jz` (section 1.2) and `12_2_COMB_LOOP_CONDITIONAL_SAFE-mutually_exclusive_cycle.jz` (section 12.2)

## Section 2.3 — Bit-Width Constraints

All rules from this section are tested:
- `TYPE_BINOP_WIDTH_MISMATCH` — tested in `2_3_TYPE_BINOP_WIDTH_MISMATCH-mismatched_operand_widths.jz`
- `ASSIGN_WIDTH_NO_MODIFIER` — tested in `2_3_ASSIGN_WIDTH_NO_MODIFIER-assign_width_mismatch.jz`
- `ASSIGN_CONCAT_WIDTH_MISMATCH` — tested in `2_3_ASSIGN_CONCAT_WIDTH_MISMATCH-concat_width_mismatch.jz`
- `TERNARY_BRANCH_WIDTH_MISMATCH` — tested in `2_3_TERNARY_BRANCH_WIDTH_MISMATCH-ternary_arm_widths.jz`
- `WIDTH_NONPOSITIVE_OR_NONINT` — tested in `2_3_WIDTH_NONPOSITIVE_OR_NONINT-zero_width.jz`

Rules not directly testable:
- `WIDTH_ASSIGN_MISMATCH_NO_EXT` — always suppressed by higher-priority `ASSIGN_WIDTH_NO_MODIFIER` (priority 1 vs 0). Every width-mismatch assignment triggers `ASSIGN_WIDTH_NO_MODIFIER` first, preventing `WIDTH_ASSIGN_MISMATCH_NO_EXT` from firing. Tested separately in `5_0_WIDTH_ASSIGN_MISMATCH_NO_EXT-alias_width_mismatch.jz`.

## Section 3.1 — Operator Categories

All rules from this section are tested:
- `LOGICAL_WIDTH_NOT_1` — tested in `3_1_LOGICAL_WIDTH_NOT_1-logical_ops_multibit.jz`
- `TYPE_BINOP_WIDTH_MISMATCH` — tested in `3_1_TYPE_BINOP_WIDTH_MISMATCH-width_mismatch.jz`
- `UNARY_ARITH_MISSING_PARENS` — tested in `3_1_UNARY_ARITH_MISSING_PARENS-unparenthesized_unary.jz`
- `TERNARY_COND_WIDTH_NOT_1` — tested in `3_1_TERNARY_COND_WIDTH_NOT_1-multibit_condition.jz`
- `TERNARY_BRANCH_WIDTH_MISMATCH` — tested in `3_1_TERNARY_BRANCH_WIDTH_MISMATCH-branch_mismatch.jz`
- `CONCAT_EMPTY` — tested in `3_1_CONCAT_EMPTY-empty_concatenation.jz`
- `DIV_CONST_ZERO` — tested in `3_1_DIV_CONST_ZERO-constant_zero_divisor.jz`
- `DIV_UNGUARDED_RUNTIME_ZERO` — tested in `3_1_DIV_UNGUARDED_RUNTIME_ZERO-unguarded_division.jz`
- `SPECIAL_DRIVER_IN_EXPRESSION` — tested in `3_1_SPECIAL_DRIVER_IN_EXPRESSION-gnd_vcc_in_expr.jz`
- `SPECIAL_DRIVER_IN_CONCAT` — tested in `3_1_SPECIAL_DRIVER_IN_CONCAT-gnd_vcc_in_concat.jz`
- `SPECIAL_DRIVER_SLICED` — tested in `3_1_SPECIAL_DRIVER_SLICED-vcc_gnd_sliced.jz`
- `SPECIAL_DRIVER_IN_INDEX` — tested in `3_1_SPECIAL_DRIVER_IN_INDEX-gnd_vcc_in_index.jz` (single trigger due to compiler halting after first diagnostic — see `issues.md` section 2.4)

## Section 3.2 — Operator Definitions

All rules from this section are tested:
- `UNARY_ARITH_MISSING_PARENS` — tested in `3_2_UNARY_ARITH_MISSING_PARENS-unparenthesized_unary.jz`
- `LOGICAL_WIDTH_NOT_1` — tested in `3_2_LOGICAL_WIDTH_NOT_1-multibit_logical_operands.jz`
- `TERNARY_COND_WIDTH_NOT_1` — tested in `3_2_TERNARY_COND_WIDTH_NOT_1-multibit_condition.jz`
- `TERNARY_BRANCH_WIDTH_MISMATCH` — tested in `3_2_TERNARY_BRANCH_WIDTH_MISMATCH-branch_width_mismatch.jz`
- `CONCAT_EMPTY` — tested in `3_2_CONCAT_EMPTY-empty_concatenation.jz`
- `DIV_CONST_ZERO` — tested in `3_2_DIV_CONST_ZERO-constant_zero_divisor.jz`
- `DIV_UNGUARDED_RUNTIME_ZERO` — tested in `3_2_DIV_UNGUARDED_RUNTIME_ZERO-unguarded_division.jz`
- `OBS_X_TO_OBSERVABLE_SINK` — tested in `3_2_OBS_X_TO_OBSERVABLE_SINK-x_bits_in_expressions.jz`
- `SPECIAL_DRIVER_IN_INDEX` — tested in `3_2_SPECIAL_DRIVER_IN_INDEX-gnd_as_index.jz` and `3_2_SPECIAL_DRIVER_IN_INDEX-vcc_as_index.jz` (split due to compiler halting after first diagnostic — see `issues.md`)

Cross-referenced rules tested in other sections:
- `SPECIAL_DRIVER_IN_EXPRESSION` — tested in `2_4_SPECIAL_DRIVER_IN_EXPRESSION-gnd_vcc_in_expr.jz` (section 2.4) and `3_1_SPECIAL_DRIVER_IN_EXPRESSION-gnd_vcc_in_expr.jz` (section 3.1)
- `SPECIAL_DRIVER_IN_CONCAT` — tested in `2_4_SPECIAL_DRIVER_IN_CONCAT-gnd_vcc_in_concat.jz` (section 2.4) and `3_1_SPECIAL_DRIVER_IN_CONCAT-gnd_vcc_in_concat.jz` (section 3.1)
- `SPECIAL_DRIVER_SLICED` — tested in `2_4_SPECIAL_DRIVER_SLICED-gnd_vcc_sliced.jz` (section 2.4) and `3_1_SPECIAL_DRIVER_SLICED-vcc_gnd_sliced.jz` (section 3.1)
- `TYPE_BINOP_WIDTH_MISMATCH` — tested in `2_2_TYPE_BINOP_WIDTH_MISMATCH-width_mismatch.jz` (section 2.2), `2_3_TYPE_BINOP_WIDTH_MISMATCH-mismatched_operand_widths.jz` (section 2.3), and `3_1_TYPE_BINOP_WIDTH_MISMATCH-width_mismatch.jz` (section 3.1)

## Section 3.4 — Operator Examples

All rules from this section are tested:
- `UNARY_ARITH_MISSING_PARENS` — tested in `3_4_UNARY_ARITH_MISSING_PARENS-negate_without_parens.jz`
- `TERNARY_BRANCH_WIDTH_MISMATCH` — tested in `3_4_TERNARY_BRANCH_WIDTH_MISMATCH-concat_width_mismatch.jz`
- `SPECIAL_DRIVER_IN_INDEX` — tested in `3_4_SPECIAL_DRIVER_IN_INDEX-gnd_in_index.jz` and `3_4_SPECIAL_DRIVER_IN_INDEX-vcc_in_index.jz` (split due to compiler halting after first diagnostic — see `issues.md`)

Cross-referenced rules tested in other sections:
- `LOGICAL_WIDTH_NOT_1` — tested in `3_1_LOGICAL_WIDTH_NOT_1-logical_ops_multibit.jz` (section 3.1) and `3_2_LOGICAL_WIDTH_NOT_1-multibit_logical_operands.jz` (section 3.2)
- `CONCAT_EMPTY` — tested in `3_1_CONCAT_EMPTY-empty_concatenation.jz` (section 3.1) and `3_2_CONCAT_EMPTY-empty_concatenation.jz` (section 3.2)
- `DIV_CONST_ZERO` — tested in `3_1_DIV_CONST_ZERO-constant_zero_divisor.jz` (section 3.1) and `3_2_DIV_CONST_ZERO-constant_zero_divisor.jz` (section 3.2)
- `DIV_UNGUARDED_RUNTIME_ZERO` — tested in `3_1_DIV_UNGUARDED_RUNTIME_ZERO-unguarded_division.jz` (section 3.1) and `3_2_DIV_UNGUARDED_RUNTIME_ZERO-unguarded_division.jz` (section 3.2)
- `TERNARY_COND_WIDTH_NOT_1` — tested in `3_1_TERNARY_COND_WIDTH_NOT_1-multibit_condition.jz` (section 3.1) and `3_2_TERNARY_COND_WIDTH_NOT_1-multibit_condition.jz` (section 3.2)
- `TYPE_BINOP_WIDTH_MISMATCH` — tested in `2_2_TYPE_BINOP_WIDTH_MISMATCH-width_mismatch.jz` (section 2.2), `2_3_TYPE_BINOP_WIDTH_MISMATCH-mismatched_operand_widths.jz` (section 2.3), and `3_1_TYPE_BINOP_WIDTH_MISMATCH-width_mismatch.jz` (section 3.1)
- `SPECIAL_DRIVER_IN_EXPRESSION` — tested in `2_4_SPECIAL_DRIVER_IN_EXPRESSION-gnd_vcc_in_expr.jz` (section 2.4) and `3_1_SPECIAL_DRIVER_IN_EXPRESSION-gnd_vcc_in_expr.jz` (section 3.1)
- `SPECIAL_DRIVER_IN_CONCAT` — tested in `2_4_SPECIAL_DRIVER_IN_CONCAT-gnd_vcc_in_concat.jz` (section 2.4) and `3_1_SPECIAL_DRIVER_IN_CONCAT-gnd_vcc_in_concat.jz` (section 3.1)
- `SPECIAL_DRIVER_SLICED` — tested in `2_4_SPECIAL_DRIVER_SLICED-gnd_vcc_sliced.jz` (section 2.4) and `3_1_SPECIAL_DRIVER_SLICED-vcc_gnd_sliced.jz` (section 3.1)

## Section 2.4 — Special Semantic Drivers

All rules from this section are tested:
- `SPECIAL_DRIVER_IN_EXPRESSION` — tested in `2_4_SPECIAL_DRIVER_IN_EXPRESSION-gnd_vcc_in_expr.jz`
- `SPECIAL_DRIVER_IN_CONCAT` — tested in `2_4_SPECIAL_DRIVER_IN_CONCAT-gnd_vcc_in_concat.jz`
- `SPECIAL_DRIVER_SLICED` — tested in `2_4_SPECIAL_DRIVER_SLICED-gnd_vcc_sliced.jz`
- `SPECIAL_DRIVER_IN_INDEX` — tested in `2_4_SPECIAL_DRIVER_IN_INDEX-gnd_in_index.jz` and `2_4_SPECIAL_DRIVER_IN_INDEX-vcc_in_index.jz` (split due to compiler halting after first diagnostic — see `issues.md`)

## Section 10.3 — Template Allowed Content

All rules from this section are tested:
- `TEMPLATE_EXTERNAL_REF` — tested in `10_3_TEMPLATE_EXTERNAL_REF-external_signal_reference.jz`
- `TEMPLATE_SCRATCH_OUTSIDE` — tested in `10_3_TEMPLATE_SCRATCH_OUTSIDE-scratch_outside_template.jz`
- `TEMPLATE_SCRATCH_WIDTH_INVALID` — tested in `10_3_TEMPLATE_SCRATCH_WIDTH_INVALID-scratch_width_not_constant.jz`

Note: `TEMPLATE_EXTERNAL_REF` LHS-of-`<=` and RHS-of-`=>` contexts are not tested due to cascading errors (see `issues.md`).

## Section 10.5 — Template Application

All rules from this section are tested:
- `TEMPLATE_UNDEFINED` — tested in `10_5_TEMPLATE_UNDEFINED-undefined_template_ref.jz`
- `TEMPLATE_ARG_COUNT_MISMATCH` — tested in `10_5_TEMPLATE_ARG_COUNT_MISMATCH-wrong_arg_count.jz`
- `TEMPLATE_COUNT_NOT_NONNEG_INT` — tested in `10_5_TEMPLATE_COUNT_NOT_NONNEG_INT-bad_count_expr.jz`
- `TEMPLATE_APPLY_OUTSIDE_BLOCK` — tested in `10_5_TEMPLATE_APPLY_OUTSIDE_BLOCK-apply_at_file_scope.jz` and `10_5_TEMPLATE_APPLY_OUTSIDE_BLOCK-apply_at_module_scope.jz`

## Section 10.8 — Template Error Cases (Cross-Reference)

All rules from this section are tested. This section cross-references rules from S10.2–S10.6:
- `TEMPLATE_DUP_NAME` — tested in `10_8_TEMPLATE_DUP_NAME-duplicate_name_error.jz`
- `TEMPLATE_DUP_PARAM` — tested in `10_8_TEMPLATE_DUP_PARAM-duplicate_param_error.jz`
- `TEMPLATE_EXTERNAL_REF` — tested in `10_8_TEMPLATE_EXTERNAL_REF-external_ref_error.jz`
- `TEMPLATE_SCRATCH_OUTSIDE` — tested in `10_8_TEMPLATE_SCRATCH_OUTSIDE-scratch_outside_error.jz`
- `TEMPLATE_SCRATCH_WIDTH_INVALID` — tested in `10_8_TEMPLATE_SCRATCH_WIDTH_INVALID-invalid_width_error.jz`
- `TEMPLATE_FORBIDDEN_DECL` — tested in `10_8_TEMPLATE_FORBIDDEN_DECL-forbidden_decl_error.jz`
- `TEMPLATE_FORBIDDEN_BLOCK_HEADER` — tested in `10_8_TEMPLATE_FORBIDDEN_BLOCK_HEADER-sync_in_template.jz` and `10_8_TEMPLATE_FORBIDDEN_BLOCK_HEADER-async_in_template.jz`
- `TEMPLATE_FORBIDDEN_DIRECTIVE` — tested in `10_8_TEMPLATE_FORBIDDEN_DIRECTIVE-new_in_template.jz`, `10_8_TEMPLATE_FORBIDDEN_DIRECTIVE-module_in_template.jz`, `10_8_TEMPLATE_FORBIDDEN_DIRECTIVE-project_in_template.jz`, and `10_8_TEMPLATE_FORBIDDEN_DIRECTIVE-feature_in_template.jz`
- `TEMPLATE_NESTED_DEF` — tested in `10_8_TEMPLATE_NESTED_DEF-nested_def_error.jz`
- `TEMPLATE_UNDEFINED` — tested in `10_8_TEMPLATE_UNDEFINED-undefined_ref_error.jz`
- `TEMPLATE_ARG_COUNT_MISMATCH` — tested in `10_8_TEMPLATE_ARG_COUNT_MISMATCH-arg_count_error.jz`
- `TEMPLATE_COUNT_NOT_NONNEG_INT` — tested in `10_8_TEMPLATE_COUNT_NOT_NONNEG_INT-bad_count_error.jz`
- `TEMPLATE_APPLY_OUTSIDE_BLOCK` — tested in `10_8_TEMPLATE_APPLY_OUTSIDE_BLOCK-apply_outside_error.jz`
- `ASSIGN_MULTIPLE_SAME_BITS` — tested in `10_8_ASSIGN_MULTIPLE_SAME_BITS-template_async_error.jz`
- `SYNC_MULTI_ASSIGN_SAME_REG_BITS` — tested in `10_8_SYNC_MULTI_ASSIGN_SAME_REG_BITS-template_sync_error.jz`

## Section 11.3 — Tri-State Net Identification

All rules from this section are tested:
- `NET_MULTIPLE_ACTIVE_DRIVERS` — tested in `11_3_NET_MULTIPLE_ACTIVE_DRIVERS-multi_driver_non_tristate.jz`
- `NET_TRI_STATE_ALL_Z_READ` — tested in `11_3_NET_TRI_STATE_ALL_Z_READ-all_drivers_z_but_read.jz`

Note: The test plan listed `NET_TRI_STATE_ALL_Z_READ` as suppressed by `ASYNC_FLOATING_Z_READ`, but the compiler correctly fires this rule. The rule is fully tested.

## Section 11.4 — Tristate Transformation Algorithm

All rules from this section are tested:
- `INFO_TRISTATE_TRANSFORM` — tested in `11_GND_4_INFO_TRISTATE_TRANSFORM-gnd_transform.jz`, `11_GND_4_INFO_TRISTATE_TRANSFORM-single_bit_fullwidth_z.jz`, `11_VCC_4_INFO_TRISTATE_TRANSFORM-vcc_transform.jz`
- `TRISTATE_TRANSFORM_SINGLE_DRIVER` — tested in `11_GND_4_TRISTATE_TRANSFORM_SINGLE_DRIVER-single_driver.jz`
- `TRISTATE_TRANSFORM_PER_BIT_FAIL` — tested in `11_GND_4_TRISTATE_TRANSFORM_PER_BIT_FAIL-per_bit_tristate.jz`
- `TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL` — tested in `11_GND_4_TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL-non_exclusive.jz`

Note: Multi-driver priority chain happy paths (two-driver, three-driver) cannot be tested — see `issues.md` for details.

## Section 11.5 — Tri-State Validation Rules

All rules from this section are tested (cross-reference of TRISTATE_TRANSFORM_* rules from S11.7):
- `INFO_TRISTATE_TRANSFORM` — tested in `11_GND_5_HAPPY_PATH-tristate_validation_ok.jz`
- `TRISTATE_TRANSFORM_SINGLE_DRIVER` — tested in `11_GND_5_HAPPY_PATH-tristate_validation_ok.jz`
- `TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL` — tested in `11_GND_5_TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL-non_exclusive.jz`
- `TRISTATE_TRANSFORM_PER_BIT_FAIL` — tested in `11_GND_5_TRISTATE_TRANSFORM_PER_BIT_FAIL-per_bit.jz`

## Section 11.7 — Tri-State Error Conditions and Warnings

All rules from this section are tested:
- `TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL` — tested in `11_GND_7_TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL-non_exclusive.jz`
- `TRISTATE_TRANSFORM_PER_BIT_FAIL` — tested in `11_GND_7_TRISTATE_TRANSFORM_PER_BIT_FAIL-per_bit_tristate.jz`
- `TRISTATE_TRANSFORM_BLACKBOX_PORT` — tested in `11_GND_7_TRISTATE_TRANSFORM_BLACKBOX_PORT-blackbox_tristate.jz`
- `TRISTATE_TRANSFORM_OE_EXTRACT_FAIL` — tested in `11_GND_7_TRISTATE_TRANSFORM_OE_EXTRACT_FAIL-ambiguous_oe.jz`
- `TRISTATE_TRANSFORM_SINGLE_DRIVER` — tested in `11_GND_7_TRISTATE_TRANSFORM_SINGLE_DRIVER-single_driver.jz`
- `TRISTATE_TRANSFORM_UNUSED_DEFAULT` — tested in `11_GND_7_TRISTATE_TRANSFORM_UNUSED_DEFAULT-no_tristate_nets.jz`
- `INFO_TRISTATE_TRANSFORM` — tested in `11_GND_7_HAPPY_PATH-clean_transform_ok.jz` (also tested in sections 11.4, 11.5, 11.6)
- `WARN_INTERNAL_TRISTATE` — tested in `11_1_WARN_INTERNAL_TRISTATE-internal_tristate_warning.jz` (section 11.1)

## Section 11.6 — Handling of INOUT Ports and External Pins

All rules from this section are tested (cross-reference of TRISTATE_TRANSFORM_* rules from S11.7):
- `INFO_TRISTATE_TRANSFORM` — tested in `11_GND_6_HAPPY_PATH-tristate_inout_ok.jz`
- `TRISTATE_TRANSFORM_OE_EXTRACT_FAIL` — tested in `11_GND_6_TRISTATE_TRANSFORM_OE_EXTRACT_FAIL-inout_oe_fail.jz`
- `TRISTATE_TRANSFORM_BLACKBOX_PORT` — tested in `11_GND_6_TRISTATE_TRANSFORM_BLACKBOX_PORT-blackbox_inout.jz`

## Section 4.2 — Scope and Uniqueness

All rules from this section are tested:
- `ID_DUP_IN_MODULE` — tested in `4_2_ID_DUP_IN_MODULE-duplicate_identifiers.jz`
- `MODULE_NAME_DUP_IN_PROJECT` — tested in `4_2_MODULE_NAME_DUP_IN_PROJECT-duplicate_module_names.jz`
- `BLACKBOX_NAME_DUP_IN_PROJECT` — tested in `4_2_BLACKBOX_NAME_DUP_IN_PROJECT-blackbox_name_conflicts.jz`
- `INSTANCE_NAME_DUP_IN_MODULE` — tested in `4_2_INSTANCE_NAME_DUP_IN_MODULE-duplicate_instance_names.jz`
- `INSTANCE_NAME_CONFLICT` — tested in `4_2_INSTANCE_NAME_CONFLICT-instance_signal_collision.jz`
- `UNDECLARED_IDENTIFIER` — tested in `4_2_UNDECLARED_IDENTIFIER-undeclared_use.jz`
- `AMBIGUOUS_REFERENCE` — tested in `4_2_AMBIGUOUS_REFERENCE-ambiguous_without_prefix.jz`

## Section 4.10 — Asynchronous Block

All rules from this section are tested:
- `ASYNC_ALIAS_IN_CONDITIONAL` — tested in `4_10_ASYNC_ALIAS_IN_CONDITIONAL-alias_in_conditional.jz`
- `ASYNC_ALIAS_LITERAL_RHS` — tested in `4_10_ASYNC_ALIAS_LITERAL_RHS-literal_rhs_in_alias.jz`
- `ASYNC_ASSIGN_REGISTER` — tested in `4_10_ASYNC_ASSIGN_REGISTER-register_write_in_async.jz`
- `ASYNC_INVALID_STATEMENT_TARGET` — tested in `4_10_ASYNC_INVALID_STATEMENT_TARGET-invalid_lhs_in_async.jz`
- `ASYNC_UNDEFINED_PATH_NO_DRIVER` — tested in `4_10_ASYNC_UNDEFINED_PATH_NO_DRIVER-partial_coverage.jz`

Rules not directly testable:
- `ASYNC_FLOATING_Z_READ` — no rule ID exists in `rules.c`. The test plan references this rule but it does not exist in the compiler. The closest rule is `NET_FLOATING_WITH_SINK` (tested in section 1.2).
- `WIDTH_ASSIGN_MISMATCH_NO_EXT` — always suppressed by higher-priority `ASSIGN_WIDTH_NO_MODIFIER` (priority 1 vs 0). Tested separately in `5_0_WIDTH_ASSIGN_MISMATCH_NO_EXT-alias_width_mismatch.jz`.

## Section 12.2 — Combinational Loop Errors

All rules from this section are tested:
- `COMB_LOOP_UNCONDITIONAL` — tested in `12_2_COMB_LOOP_UNCONDITIONAL-two_signal_cycle.jz`, `12_2_COMB_LOOP_UNCONDITIONAL-three_signal_cycle.jz`, `12_2_COMB_LOOP_UNCONDITIONAL-self_assignment.jz`, `12_2_COMB_LOOP_UNCONDITIONAL-conditional_same_path.jz`, `12_2_COMB_LOOP_UNCONDITIONAL-instance_port_cycle.jz`
- `COMB_LOOP_CONDITIONAL_SAFE` — tested in `12_2_COMB_LOOP_CONDITIONAL_SAFE-mutually_exclusive_cycle.jz`

Rules not tested:
- `PATH_OUTSIDE_SANDBOX` — not testable via `--lint`. This rule fires at the file-system access layer during compilation and requires sandbox environment configuration that cannot be exercised through standard validation test infrastructure.

Note: `12_2_COMB_LOOP_UNCONDITIONAL-instance_port_cycle.jz` currently produces no diagnostics because the compiler does not detect loops through instance ports — see `issues.md` for details.

## Section 4.12 — CDC Block

All rules from this section are tested:
- `CDC_BIT_WIDTH_NOT_1` — tested in `4_12_CDC_BIT_WIDTH_NOT_1-bit_multibit_source.jz`
- `CDC_PULSE_WIDTH_NOT_1` — tested in `4_12_CDC_PULSE_WIDTH_NOT_1-pulse_multibit_source.jz`
- `CDC_RAW_STAGES_FORBIDDEN` — tested in `4_12_CDC_RAW_STAGES_FORBIDDEN-raw_with_stages.jz`
- `CDC_SOURCE_NOT_REGISTER` — tested in `4_12_CDC_SOURCE_NOT_REGISTER-wire_as_source.jz`
- `CDC_SOURCE_NOT_PLAIN_REG` — tested in `4_12_CDC_SOURCE_NOT_PLAIN_REG-sliced_source.jz`
- `CDC_DEST_ALIAS_ASSIGNED` — tested in `4_12_CDC_DEST_ALIAS_ASSIGNED-dest_alias_written.jz`
- `CDC_DEST_ALIAS_DUP` — tested in `4_12_CDC_DEST_ALIAS_DUP-alias_name_conflict.jz`
- `CDC_STAGES_INVALID` — tested in `4_12_CDC_STAGES_INVALID-non_positive_stages.jz`
- `CDC_TYPE_INVALID` — tested in `4_12_CDC_TYPE_INVALID-unknown_cdc_type.jz`

Note: `CDC_DEST_ALIAS_DUP` does not fire when the alias conflicts with a WIRE name (only `ID_DUP_IN_MODULE` fires). See `issues.md` for details.

## Section 12.4 — Path Security

Rules tested:
- `PATH_ABSOLUTE_FORBIDDEN` — tested in `12_4_PATH_ABSOLUTE_FORBIDDEN-absolute_import.jz`, `12_4_PATH_ABSOLUTE_FORBIDDEN-absolute_file_init.jz`
- `PATH_TRAVERSAL_FORBIDDEN` — tested in `12_4_PATH_TRAVERSAL_FORBIDDEN-traversal_import.jz`, `12_4_PATH_TRAVERSAL_FORBIDDEN-traversal_file_init.jz`

Rules not tested:
- `PATH_OUTSIDE_SANDBOX` — not testable via `--lint`. Requires runtime sandbox configuration (permitted roots) that cannot be exercised through standard validation test infrastructure.
- `PATH_SYMLINK_ESCAPE` — not testable via `--lint`. Requires symlink filesystem setup and sandbox configuration that cannot be exercised through standard validation test infrastructure.

## Section 4.13 — Module Instantiation

All rules from this section are tested:
- `INSTANCE_UNDEFINED_MODULE` — tested in `4_13_INSTANCE_UNDEFINED_MODULE-nonexistent_module.jz`
- `INSTANCE_MISSING_PORT` — tested in `4_13_INSTANCE_MISSING_PORT-incomplete_port_list.jz`
- `INSTANCE_PORT_WIDTH_MISMATCH` — tested in `4_13_INSTANCE_PORT_WIDTH_MISMATCH-port_width_mismatch.jz`
- `INSTANCE_PORT_DIRECTION_MISMATCH` — tested in `4_13_INSTANCE_PORT_DIRECTION_MISMATCH-direction_incompatible.jz`
- `INSTANCE_BUS_MISMATCH` — tested in `4_13_INSTANCE_BUS_MISMATCH-bus_mismatch.jz`
- `INSTANCE_OVERRIDE_CONST_UNDEFINED` — tested in `4_13_INSTANCE_OVERRIDE_CONST_UNDEFINED-bad_override.jz`
- `INSTANCE_PORT_WIDTH_EXPR_INVALID` — tested in `4_13_INSTANCE_PORT_WIDTH_EXPR_INVALID-bad_width_expr.jz`
- `INSTANCE_PARENT_SIGNAL_WIDTH_MISMATCH` — tested in `4_13_INSTANCE_PARENT_SIGNAL_WIDTH_MISMATCH-parent_width_mismatch.jz`
- `INSTANCE_ARRAY_COUNT_INVALID` — tested in `4_13_INSTANCE_ARRAY_COUNT_INVALID-bad_array_count.jz`
- `INSTANCE_ARRAY_IDX_INVALID_CONTEXT` — tested in `4_13_INSTANCE_ARRAY_IDX_INVALID_CONTEXT-idx_misuse.jz`
- `INSTANCE_ARRAY_IDX_SLICE_OUT_OF_RANGE` — tested in `4_13_INSTANCE_ARRAY_IDX_SLICE_OUT_OF_RANGE-idx_slice_oob.jz`
- `INSTANCE_ARRAY_PARENT_BIT_OVERLAP` — tested in `4_13_INSTANCE_ARRAY_PARENT_BIT_OVERLAP-overlapping_out_bits.jz`
- `INSTANCE_OUT_PORT_LITERAL` — tested in `4_13_INSTANCE_OUT_PORT_LITERAL-literal_on_output.jz`
- `INSTANCE_ARRAY_MULTI_DIMENSIONAL` — tested in `4_13_INSTANCE_ARRAY_MULTI_DIMENSIONAL-multi_dim_array.jz`

Cross-referenced rules (tested in other sections):
- `INSTANCE_NAME_DUP_IN_MODULE` (S4.2/S8.1) — identifier scope rule, tested in section 4.2 if applicable
- `INSTANCE_NAME_CONFLICT` (S4.2/S8.1) — identifier scope rule, tested in section 4.2 if applicable

Note: `=z` and `=s` width modifiers in @new port bindings are not tested — parser does not support them. See `issues.md` for details.

## Section 4.14 — Feature Guards

All rules from this section are tested:
- `FEATURE_COND_WIDTH_NOT_1` — tested in `4_14_FEATURE_COND_WIDTH_NOT_1-wide_condition.jz` (ASYNC/SYNC blocks) and `4_14_FEATURE_COND_WIDTH_NOT_1-wide_cond_in_decl.jz` (REGISTER/WIRE/module-level)
- `FEATURE_EXPR_INVALID_CONTEXT` — tested in `4_14_FEATURE_EXPR_INVALID_CONTEXT-runtime_signal_in_condition.jz` (wire/register/port in ASYNC/SYNC) and `4_14_FEATURE_EXPR_INVALID_CONTEXT-signal_in_decl_block.jz` (port in REGISTER/WIRE/module-level)
- `FEATURE_NESTED` — tested in `4_14_FEATURE_NESTED-nested_feature_in_async.jz`, `4_14_FEATURE_NESTED-nested_feature_in_sync.jz`, `4_14_FEATURE_NESTED-nested_feature_in_else.jz`, `4_14_FEATURE_NESTED-nested_feature_in_register.jz`, `4_14_FEATURE_NESTED-nested_feature_in_wire.jz`, and `4_14_FEATURE_NESTED-nested_feature_at_module_level.jz` (split into separate files because parser cannot recover after nested @feature)

Rules not tested:
- `FEATURE_VALIDATION_BOTH_PATHS` — no rule ID exists in `rules.c`. The test plan references this rule but it has not been implemented in the compiler.

Note: `FEATURE_NESTED` is not detected at module level — see `issues.md` for details. The test `4_14_FEATURE_NESTED-nested_feature_at_module_level.jz` captures the actual compiler behavior (no diagnostics).

## Section 4.3 — CONST (Compile-Time Constants)

All rules from this section are tested:
- `CONST_NEGATIVE_OR_NONINT` — tested in `4_3_CONST_NEGATIVE_OR_NONINT-negative_const_value.jz`
- `CONST_UNDEFINED_IN_WIDTH_OR_SLICE` — tested in `4_3_CONST_UNDEFINED_IN_WIDTH_OR_SLICE-undefined_const.jz`
- `CONST_CIRCULAR_DEP` — tested in `4_3_CONST_CIRCULAR_DEP-circular_dependency.jz`
- `CONST_USED_WHERE_FORBIDDEN` — tested in `4_3_CONST_USED_WHERE_FORBIDDEN-const_in_runtime_expr.jz`
- `CONST_STRING_IN_NUMERIC_CONTEXT` — tested in `4_3_CONST_STRING_IN_NUMERIC_CONTEXT-string_as_width.jz`
- `CONST_NUMERIC_IN_STRING_CONTEXT` — tested in `4_3_CONST_NUMERIC_IN_STRING_CONTEXT-numeric_as_file_path.jz`

## Section 4.5 — WIRE (Intermediate Nets)

All rules from this section are tested:
- `WRITE_WIRE_IN_SYNC` — tested in `4_5_WRITE_WIRE_IN_SYNC-wire_in_sync_block.jz`
- `WIRE_MULTI_DIMENSIONAL` — tested in `4_5_WIRE_MULTI_DIMENSIONAL-multi_dim_helper.jz` and `4_5_WIRE_MULTI_DIMENSIONAL-multi_dim_top.jz` (split into separate files because compiler halts after first `WIRE_MULTI_DIMENSIONAL` diagnostic)
- `WARN_UNUSED_WIRE` — tested in `4_5_WARN_UNUSED_WIRE-unused_wire.jz`

## Section 4.7 — REGISTER (Storage Elements)

All rules from this section are tested:
- `ASYNC_ASSIGN_REGISTER` — tested in `4_7_ASYNC_ASSIGN_REGISTER-register_in_async.jz`
- `REG_INIT_CONTAINS_X` — tested in `4_7_REG_INIT_CONTAINS_X-x_in_init.jz`
- `REG_INIT_CONTAINS_Z` — tested in `4_7_REG_INIT_CONTAINS_Z-z_in_init.jz`
- `REG_INIT_WIDTH_MISMATCH` — tested in `4_7_REG_INIT_WIDTH_MISMATCH-init_width_mismatch.jz`
- `REG_MISSING_INIT_LITERAL` — tested in `4_7_REG_MISSING_INIT_LITERAL-missing_init_helper.jz` and `4_7_REG_MISSING_INIT_LITERAL-missing_init_top.jz` (split because compiler halts after first diagnostic)
- `REG_MULTI_DIMENSIONAL` — tested in `4_7_REG_MULTI_DIMENSIONAL-multi_dim_helper.jz` and `4_7_REG_MULTI_DIMENSIONAL-multi_dim_top.jz` (split because compiler halts after first diagnostic)
- `WARN_UNUSED_REGISTER` — tested in `4_7_WARN_UNUSED_REGISTER-unused_register.jz`
- `WARN_UNSINKED_REGISTER` — tested in `4_7_WARN_UNSINKED_REGISTER-written_never_read.jz`
- `WARN_UNDRIVEN_REGISTER` — tested in `4_7_WARN_UNDRIVEN_REGISTER-read_never_written.jz`

## Section 4.8 — LATCH (Level-Sensitive Storage)

All rules from this section are tested:
- `LATCH_ASSIGN_NON_GUARDED` — tested in `4_8_LATCH_ASSIGN_NON_GUARDED-unguarded_latch_write.jz`
- `LATCH_ASSIGN_IN_SYNC` — tested in `4_8_LATCH_ASSIGN_IN_SYNC-latch_write_in_sync.jz`
- `LATCH_ENABLE_WIDTH_NOT_1` — tested in `4_8_LATCH_ENABLE_WIDTH_NOT_1-enable_not_1bit.jz`
- `LATCH_ALIAS_FORBIDDEN` — tested in `4_8_LATCH_ALIAS_FORBIDDEN-latch_aliased.jz`
- `LATCH_INVALID_TYPE` — tested in `4_8_LATCH_INVALID_TYPE-invalid_latch_type.jz`
- `LATCH_WIDTH_INVALID` — tested indirectly in `4_8_LATCH_WIDTH_INVALID-invalid_latch_width.jz` (fires as `WIDTH_NONPOSITIVE_OR_NONINT`; see `issues.md` section 4.8 Issue 1)
- `LATCH_SR_WIDTH_MISMATCH` — tested in `4_8_LATCH_SR_WIDTH_MISMATCH-sr_width_mismatch.jz`
- `LATCH_AS_CLOCK_OR_CDC` — tested in `4_8_LATCH_AS_CLOCK_OR_CDC-latch_as_clock.jz` (CLK context) and `4_8_LATCH_AS_CLOCK_OR_CDC-latch_as_cdc.jz` (CDC context)
- `LATCH_IN_CONST_CONTEXT` — tested in `4_8_LATCH_IN_CONST_CONTEXT-latch_in_const.jz`
- `LATCH_CHIP_UNSUPPORTED` — tested in `4_8_LATCH_CHIP_UNSUPPORTED-chip_no_latch.jz`

Test gaps:
- `LATCH_WIDTH_INVALID` is unreachable — always suppressed by `WIDTH_NONPOSITIVE_OR_NONINT` (see `issues.md` section 4.8 Issue 1).

## Section 5.2 — Synchronous Assignments

All rules from this section are tested:
- `SYNC_NO_ALIAS` — tested in `5_2_SYNC_NO_ALIAS-alias_in_sync_block.jz`
- `SYNC_ROOT_AND_CONDITIONAL_ASSIGN` — tested in `5_2_SYNC_ROOT_AND_CONDITIONAL_ASSIGN-root_plus_conditional.jz`
- `SYNC_MULTI_ASSIGN_SAME_REG_BITS` — tested in `5_2_SYNC_MULTI_ASSIGN_SAME_REG_BITS-double_assign_sync.jz`
- `SYNC_CONCAT_DUP_REG` — tested in `5_2_SYNC_CONCAT_DUP_REG-duplicate_reg_in_concat.jz`
- `ASSIGN_TO_NON_REGISTER_IN_SYNC` — tested in `5_2_ASSIGN_TO_NON_REGISTER_IN_SYNC-non_register_in_sync.jz`
- `WRITE_WIRE_IN_SYNC` — tested in `5_2_WRITE_WIRE_IN_SYNC-wire_in_sync.jz`
- `SYNC_SLICE_WIDTH_MISMATCH` — tested in `5_2_SYNC_SLICE_WIDTH_MISMATCH-slice_width_mismatch.jz`

Test gaps:
- Concat with `<=z`/`<=s` cannot be tested as happy path — compiler fires `ASSIGN_CONCAT_WIDTH_MISMATCH` instead of allowing the extension (see `issues.md` section 5.2 Issue 1).

## Section 5.5 — Intrinsic Operators

All rules from this section are tested:
- `CLOG2_NONPOSITIVE_ARG` — tested in `5_5_CLOG2_NONPOSITIVE_ARG-zero_argument.jz`
- `CLOG2_INVALID_CONTEXT` — tested in `5_5_CLOG2_INVALID_CONTEXT-runtime_expression.jz`
- `WIDTHOF_INVALID_CONTEXT` — tested in `5_5_WIDTHOF_INVALID_CONTEXT-runtime_expression.jz`
- `WIDTHOF_INVALID_TARGET` — tested in `5_5_WIDTHOF_INVALID_TARGET-non_signal_target.jz`
- `WIDTHOF_INVALID_SYNTAX` — tested in `5_5_WIDTHOF_INVALID_SYNTAX-slice_argument.jz`
- `WIDTHOF_WIDTH_NOT_RESOLVABLE` — tested in `5_5_WIDTHOF_WIDTH_NOT_RESOLVABLE-unresolvable_width.jz`
- `FUNC_RESULT_TRUNCATED_SILENTLY` — tested in `5_5_FUNC_RESULT_TRUNCATED_SILENTLY-intrinsic_truncation.jz`
- `LIT_WIDTH_INVALID` — tested in `5_5_LIT_WIDTH_INVALID-non_positive_width.jz`
- `LIT_VALUE_INVALID` — tested in `5_5_LIT_VALUE_INVALID-negative_value.jz`
- `LIT_VALUE_OVERFLOW` — tested in `5_5_LIT_VALUE_OVERFLOW-value_exceeds_width.jz`
- `LIT_INVALID_CONTEXT` — tested in `5_5_LIT_INVALID_CONTEXT-non_constant_context.jz`
- `SBIT_SET_WIDTH_NOT_1` — tested in `5_5_SBIT_SET_WIDTH_NOT_1-non_unit_set_value.jz`
- `GBIT_INDEX_OUT_OF_RANGE` — tested in `5_5_GBIT_INDEX_OUT_OF_RANGE-const_index_exceeds_width.jz`
- `SBIT_INDEX_OUT_OF_RANGE` — tested in `5_5_SBIT_INDEX_OUT_OF_RANGE-const_index_exceeds_width.jz`
- `GSLICE_INDEX_OUT_OF_RANGE` — tested in `5_5_GSLICE_INDEX_OUT_OF_RANGE-const_index_exceeds_width.jz`
- `GSLICE_WIDTH_INVALID` — tested in `5_5_GSLICE_WIDTH_INVALID-zero_width_param.jz`
- `SSLICE_INDEX_OUT_OF_RANGE` — tested in `5_5_SSLICE_INDEX_OUT_OF_RANGE-const_index_exceeds_width.jz`
- `SSLICE_WIDTH_INVALID` — tested in `5_5_SSLICE_WIDTH_INVALID-zero_width_param.jz`
- `SSLICE_VALUE_WIDTH_MISMATCH` — tested in `5_5_SSLICE_VALUE_WIDTH_MISMATCH-value_width_mismatch.jz`
- `OH2B_INPUT_TOO_NARROW` — tested in `5_5_OH2B_INPUT_TOO_NARROW-single_bit_source.jz`
- `B2OH_WIDTH_INVALID` — tested in `5_5_B2OH_WIDTH_INVALID-width_below_two.jz`
- `PRIENC_INPUT_TOO_NARROW` — tested in `5_5_PRIENC_INPUT_TOO_NARROW-single_bit_source.jz`
- `BSWAP_WIDTH_NOT_BYTE_ALIGNED` — tested in `5_5_BSWAP_WIDTH_NOT_BYTE_ALIGNED-non_byte_width.jz`

No test gaps.

## Section 6.3 — CONFIG Block

All rules from this section are tested:

- `CONFIG_MULTIPLE_BLOCKS` — tested in `6_3_CONFIG_MULTIPLE_BLOCKS-multiple_config.jz`
- `CONFIG_NAME_DUPLICATE` — tested in `6_3_CONFIG_NAME_DUPLICATE-duplicate_name.jz`
- `CONFIG_INVALID_EXPR_TYPE` — tested in `6_3_CONFIG_INVALID_EXPR_TYPE-invalid_value.jz`
- `CONFIG_FORWARD_REF` — tested in `6_3_CONFIG_FORWARD_REF-forward_reference.jz`
- `CONFIG_USE_UNDECLARED` — tested in `6_3_CONFIG_USE_UNDECLARED-undeclared_reference.jz`
- `CONFIG_CIRCULAR_DEP` — tested in `6_3_CONFIG_CIRCULAR_DEP-circular_dependency.jz`
- `CONFIG_USED_WHERE_FORBIDDEN` — tested in `6_3_CONFIG_USED_WHERE_FORBIDDEN-runtime_use.jz`
- `CONST_USED_WHERE_FORBIDDEN` — tested in `6_3_CONST_USED_WHERE_FORBIDDEN-const_runtime_use.jz`
- `CONST_STRING_IN_NUMERIC_CONTEXT` — tested in `6_3_CONST_STRING_IN_NUMERIC_CONTEXT-string_as_number.jz`
- `CONST_NUMERIC_IN_STRING_CONTEXT` — tested in `6_3_CONST_NUMERIC_IN_STRING_CONTEXT-numeric_as_string.jz`

No test gaps.

## Section 6.4 — CLOCKS Block

All rules from this section are tested:
- `CLOCK_DUPLICATE_NAME` — tested in `6_4_CLOCK_DUPLICATE_NAME-duplicate_clocks.jz`
- `CLOCK_PERIOD_NONPOSITIVE` — tested in `6_4_CLOCK_PERIOD_NONPOSITIVE-invalid_period.jz`
- `CLOCK_EDGE_INVALID` — tested in `6_4_CLOCK_EDGE_INVALID-invalid_edge.jz`
- `CLOCK_PORT_WIDTH_NOT_1` — tested in `6_4_CLOCK_PORT_WIDTH_NOT_1-wide_clock.jz`
- `CLOCK_NAME_NOT_IN_PINS` — tested in `6_4_CLOCK_NAME_NOT_IN_PINS-missing_pin.jz`
- `CLOCK_EXTERNAL_NO_PERIOD` — tested in `6_4_CLOCK_EXTERNAL_NO_PERIOD-no_period.jz`
- `CLOCK_SOURCE_AMBIGUOUS` — tested in `6_4_CLOCK_SOURCE_AMBIGUOUS-dual_source.jz`
- `CLOCK_GEN_INVALID_TYPE` — tested in `6_4_CLOCK_GEN_INVALID_TYPE-bad_type.jz`
- `CLOCK_GEN_MISSING_INPUT` — tested in `6_4_CLOCK_GEN_MISSING_INPUT-no_input.jz`
- `CLOCK_GEN_MISSING_OUTPUT` — tested in `6_4_CLOCK_GEN_MISSING_OUTPUT-no_output.jz`
- `CLOCK_GEN_REQUIRED_INPUT_MISSING` — tested in `6_4_CLOCK_GEN_REQUIRED_INPUT_MISSING-missing_required.jz`
- `CLOCK_GEN_NO_CHIP_DATA` — tested in `6_4_CLOCK_GEN_NO_CHIP_DATA-no_chip.jz`
- `CLOCK_GEN_INPUT_NOT_DECLARED` — tested in `6_4_CLOCK_GEN_INPUT_NOT_DECLARED-undeclared_input.jz`
- `CLOCK_GEN_INPUT_NO_PERIOD` — tested in `6_4_CLOCK_GEN_INPUT_NO_PERIOD-no_input_period.jz`
- `CLOCK_GEN_INPUT_IS_SELF_OUTPUT` — tested in `6_4_CLOCK_GEN_INPUT_IS_SELF_OUTPUT-self_reference.jz`
- `CLOCK_GEN_INPUT_FREQ_OUT_OF_RANGE` — tested in `6_4_CLOCK_GEN_INPUT_FREQ_OUT_OF_RANGE-freq_range.jz`
- `CLOCK_GEN_OUTPUT_NOT_DECLARED` — tested in `6_4_CLOCK_GEN_OUTPUT_NOT_DECLARED-undeclared_output.jz`
- `CLOCK_GEN_OUTPUT_HAS_PERIOD` — tested in `6_4_CLOCK_GEN_OUTPUT_HAS_PERIOD-gen_out_with_period.jz`
- `CLOCK_GEN_OUTPUT_IS_INPUT_PIN` — tested in `6_4_CLOCK_GEN_OUTPUT_IS_INPUT_PIN-out_is_inpin.jz`
- `CLOCK_GEN_OUTPUT_INVALID_SELECTOR` — tested in `6_4_CLOCK_GEN_OUTPUT_INVALID_SELECTOR-bad_selector.jz`
- `CLOCK_GEN_MULTIPLE_DRIVERS` — tested in `6_4_CLOCK_GEN_MULTIPLE_DRIVERS-multi_driver.jz`
- `CLOCK_GEN_OUT_NOT_CLOCK` — tested in `6_4_CLOCK_GEN_OUT_NOT_CLOCK-out_for_nonclock.jz`
- `CLOCK_GEN_WIRE_IS_CLOCK` — tested in `6_4_CLOCK_GEN_WIRE_IS_CLOCK-wire_for_clock.jz`
- `CLOCK_GEN_WIRE_IN_CLOCKS` — tested in `6_4_CLOCK_GEN_WIRE_IN_CLOCKS-wire_in_clocks.jz`
- `CLOCK_GEN_PARAM_OUT_OF_RANGE` — tested in `6_4_CLOCK_GEN_PARAM_OUT_OF_RANGE-param_range.jz`
- `CLOCK_GEN_PARAM_TYPE_MISMATCH` — tested in `6_4_CLOCK_GEN_PARAM_TYPE_MISMATCH-type_mismatch.jz`
- `CLOCK_GEN_DERIVED_OUT_OF_RANGE` — tested in `6_4_CLOCK_GEN_DERIVED_OUT_OF_RANGE-derived_range.jz`

No test gaps.

## Section 6.5 — PIN Blocks

All rules from this section are tested:
- `PIN_DECLARED_MULTIPLE_BLOCKS` — tested in `6_5_PIN_DECLARED_MULTIPLE_BLOCKS-cross_block_duplicate.jz`
- `PIN_INVALID_STANDARD` — tested in `6_5_PIN_INVALID_STANDARD-bad_standard.jz`
- `PIN_DRIVE_MISSING_OR_INVALID` — tested in `6_5_PIN_DRIVE_MISSING_OR_INVALID-drive_problems.jz`
- `PIN_BUS_WIDTH_INVALID` — tested in `6_5_PIN_BUS_WIDTH_INVALID-bad_bus_width.jz`
- `PIN_DUP_NAME_WITHIN_BLOCK` — tested in `6_5_PIN_DUP_NAME_WITHIN_BLOCK-duplicate_within_block.jz`
- `PIN_MODE_INVALID` — tested in `6_5_PIN_MODE_INVALID-bad_mode.jz`
- `PIN_MODE_STANDARD_MISMATCH` — tested in `6_5_PIN_MODE_STANDARD_MISMATCH-mode_standard_conflict.jz`
- `PIN_PULL_INVALID` — tested in `6_5_PIN_PULL_INVALID-bad_pull.jz`
- `PIN_PULL_ON_OUTPUT` — tested in `6_5_PIN_PULL_ON_OUTPUT-pull_on_out.jz`
- `PIN_TERM_INVALID` — tested in `6_5_PIN_TERM_INVALID-bad_termination.jz`
- `PIN_TERM_ON_OUTPUT` — tested in `6_5_PIN_TERM_ON_OUTPUT-term_on_out.jz`
- `PIN_TERM_INVALID_FOR_STANDARD` — tested in `6_5_PIN_TERM_INVALID_FOR_STANDARD-term_unsupported.jz`
- `PIN_DIFF_OUT_MISSING_FCLK` — tested in `6_5_PIN_DIFF_OUT_MISSING_FCLK-no_fclk.jz`
- `PIN_DIFF_OUT_MISSING_PCLK` — tested in `6_5_PIN_DIFF_OUT_MISSING_PCLK-no_pclk.jz`
- `PIN_DIFF_OUT_MISSING_RESET` — tested in `6_5_PIN_DIFF_OUT_MISSING_RESET-no_reset.jz`

Rules not testable via `--info --lint`:
- `INFO_SERIALIZER_CASCADE` — backend-only diagnostic, not reachable via lint
- `SERIALIZER_WIDTH_EXCEEDS_RATIO` — backend-only diagnostic, not reachable via lint

## Section 6.7 — Blackbox (Opaque) Modules

All rules from this section are tested:
- `BLACKBOX_BODY_DISALLOWED` — tested in `6_7_BLACKBOX_BODY_DISALLOWED-const_in_blackbox.jz` (CONST block, fires semantic rule). ASYNCHRONOUS/SYNCHRONOUS/WIRE/REGISTER/MEM are caught by the parser (PARSE000) and tested in separate files: `6_7_BLACKBOX_BODY_DISALLOWED-async_in_blackbox.jz`, `6_7_BLACKBOX_BODY_DISALLOWED-sync_in_blackbox.jz`, `6_7_BLACKBOX_BODY_DISALLOWED-wire_in_blackbox.jz`, `6_7_BLACKBOX_BODY_DISALLOWED-register_in_blackbox.jz`, `6_7_BLACKBOX_BODY_DISALLOWED-mem_in_blackbox.jz`
- `BLACKBOX_NAME_DUP_IN_PROJECT` — tested in `6_7_BLACKBOX_NAME_DUP_IN_PROJECT-blackbox_name_conflicts.jz` (bb-bb dup, bb-module conflict)
- `BLACKBOX_OVERRIDE_UNCHECKED` — tested in `6_7_BLACKBOX_OVERRIDE_UNCHECKED-override_passthrough.jz` (sub-module, top module single param, top module multiple params)
- `BLACKBOX_UNDEFINED_IN_NEW` — tested in `6_7_BLACKBOX_UNDEFINED_IN_NEW-undefined_blackbox.jz` (sub-module, top module)

## Section 6.8 — BUS Aggregation

All rules from this section are tested except `BUS_DEF_INVALID_DIR`:
- `BUS_DEF_DUP_NAME` — tested in `6_8_BUS_DEF_DUP_NAME-duplicate_bus_name.jz` (3 triggers: two duplicates of ALPHA_BUS, one duplicate of BETA_BUS)
- `BUS_DEF_SIGNAL_DUP_NAME` — tested in `6_8_BUS_DEF_SIGNAL_DUP_NAME-duplicate_signal_in_bus.jz` (4 triggers: dup OUT, dup IN, dup INOUT, cross-direction dup)
- `BUS_BULK_BUS_MISMATCH` — tested in `6_8_BUS_BULK_BUS_MISMATCH-bus_mismatch.jz` (2 triggers: ALPHA-to-BETA and BETA-to-ALPHA mismatches)
- `BUS_BULK_ROLE_CONFLICT` — tested in `6_8_BUS_BULK_ROLE_CONFLICT-role_conflict.jz` (2 triggers: SOURCE-SOURCE, TARGET-TARGET)

Rules not directly testable:
- `BUS_DEF_INVALID_DIR` — dead code. The parser only accepts IN/OUT/INOUT inside BUS blocks; any other keyword produces `PARSE000` before the semantic rule can fire. See `issues.md` for details.

## Section 7.1 — MEM Declaration

All rules from this section are tested:
- `MEM_DUP_NAME` — tested in `7_1_MEM_DUP_NAME-duplicate_mem_name.jz` (2 triggers: helper module, top module)
- `MEM_INVALID_WORD_WIDTH` — tested in `7_1_MEM_INVALID_WORD_WIDTH-zero_width.jz` (2 triggers: helper module, top module)
- `MEM_INVALID_DEPTH` — tested in `7_1_MEM_INVALID_DEPTH-zero_depth.jz` (2 triggers: helper module, top module)
- `MEM_UNDEFINED_CONST_IN_WIDTH` — tested in `7_1_MEM_UNDEFINED_CONST_IN_WIDTH-undefined_const.jz` (2 triggers: undefined width in helper, undefined depth in top)
- `MEM_DUP_PORT_NAME` — tested in `7_1_MEM_DUP_PORT_NAME-duplicate_port_name.jz` (3 triggers: OUT-OUT dup, IN-IN dup, OUT-IN same name)
- `MEM_PORT_NAME_CONFLICT_MODULE_ID` — tested in `7_1_MEM_PORT_NAME_CONFLICT_MODULE_ID-port_name_conflict.jz` (3 triggers: conflict with PORT, REGISTER, WIRE)
- `MEM_EMPTY_PORT_LIST` — tested in `7_1_MEM_EMPTY_PORT_LIST-no_ports.jz` (2 triggers: helper module, top module)
- `MEM_INVALID_PORT_TYPE` — tested in `7_1_MEM_INVALID_PORT_TYPE-invalid_port_type.jz` (2 triggers: IN ASYNC, IN SYNC)
- `MEM_MISSING_INIT` — tested in `7_1_MEM_MISSING_INIT-missing_init.jz` (2 triggers: helper module, top module)
- `MEM_TYPE_INVALID` — tested in `7_1_MEM_TYPE_INVALID-bad_type_keyword.jz` (2 triggers: type=INVALID, type=REGISTER)
- `MEM_TYPE_BLOCK_WITH_ASYNC_OUT` — tested in `7_1_MEM_TYPE_BLOCK_WITH_ASYNC_OUT-block_async_out.jz` (2 triggers: helper module, top module)
- `MEM_CHIP_CONFIG_UNSUPPORTED` — tested in `7_1_MEM_CHIP_CONFIG_UNSUPPORTED-unsupported_chip.jz` (2 triggers: helper module, top module)
- `MEM_INOUT_MIXED_WITH_IN_OUT` — tested in `7_1_MEM_INOUT_MIXED_WITH_IN_OUT-mixed_inout.jz` (3 triggers: INOUT+OUT, INOUT+IN, INOUT+OUT+IN)
- `MEM_INOUT_ASYNC` — tested in `7_1_MEM_INOUT_ASYNC-inout_async.jz` (2 triggers: INOUT ASYNC, INOUT SYNC)
- `MEM_UNDEFINED_NAME` — tested in `7_1_MEM_UNDEFINED_NAME-undefined_mem_name.jz` (2 triggers: read in ASYNC, write in SYNC)

## Section 7.10 — CONST Evaluation in MEM

All rules from this section are tested:
- `CONST_NEGATIVE_OR_NONINT` — tested in `7_10_CONST_NEGATIVE_OR_NONINT-negative_const_mem_depth.jz` (2 triggers: negative depth in helper, negative width in helper2)
- `MEM_UNDEFINED_CONST_IN_WIDTH` — tested in `7_10_MEM_UNDEFINED_CONST_IN_WIDTH-undefined_const_in_mem.jz` (3 triggers: undefined width in helper, undefined depth in top, both undefined in top)
- `CONST_CIRCULAR_DEP` — tested in `7_10_CONST_CIRCULAR_DEP-circular_const_in_mem.jz` (2 trigger pairs: circular A/B in helper, circular X/Y in top)
- `CONST_UNDEFINED_IN_WIDTH_OR_SLICE` — tested in `7_10_CONST_UNDEFINED_IN_WIDTH_OR_SLICE-undefined_const_in_mem_width.jz` (3 triggers: port width in helper, wire width in top, register width in top)

## Section 7.3 — Memory Access Syntax

All rules from this section are tested:
- `MEM_PORT_UNDEFINED` — tested in `7_3_MEM_PORT_UNDEFINED-undeclared_port_access.jz` (3 triggers: async read, sync write, sync field access)
- `MEM_PORT_FIELD_UNDEFINED` — tested in `7_3_MEM_PORT_FIELD_UNDEFINED-invalid_field_name.jz` (3 triggers: SYNC OUT .foo, SYNC OUT .bar, INOUT .xyz)
- `MEM_SYNC_PORT_INDEXED` — tested in `7_3_MEM_SYNC_PORT_INDEXED-sync_port_bracket.jz` (2 triggers: helper, top)
- `MEM_PORT_USED_AS_SIGNAL` — tested in `7_3_MEM_PORT_USED_AS_SIGNAL-bare_port_ref.jz` (3 triggers: ASYNC bare, SYNC bare, INOUT bare)
- `MEM_PORT_ADDR_READ` — tested in `7_3_MEM_PORT_ADDR_READ-read_addr_input.jz` (3 triggers: SYNC OUT .addr read in helper, SYNC OUT .addr read in top, INOUT .addr read in top)
- `MEM_ASYNC_PORT_FIELD_DATA` — tested in `7_3_MEM_ASYNC_PORT_FIELD_DATA-async_port_dot_data.jz` (2 triggers: helper, top)
- `MEM_SYNC_ADDR_INVALID_PORT` — tested in `7_3_MEM_SYNC_ADDR_INVALID_PORT-addr_on_async_port.jz` (2 triggers: helper, top)
- `MEM_SYNC_ADDR_IN_ASYNC_BLOCK` — tested in `7_3_MEM_SYNC_ADDR_IN_ASYNC_BLOCK-sync_addr_in_async.jz` (2 triggers: helper, top)
- `MEM_SYNC_DATA_IN_ASYNC_BLOCK` — tested in `7_3_MEM_SYNC_DATA_IN_ASYNC_BLOCK-sync_data_in_async.jz` (2 triggers: helper, top)
- `MEM_SYNC_ADDR_WITHOUT_RECEIVE` — tested in `7_3_MEM_SYNC_ADDR_WITHOUT_RECEIVE-sync_addr_equals.jz` (2 triggers: helper, top)
- `MEM_READ_SYNC_WITH_EQUALS` — tested in `7_3_MEM_READ_SYNC_WITH_EQUALS-sync_read_equals.jz` (2 triggers: helper, top)
- `MEM_IN_PORT_FIELD_ACCESS` — tested in `7_3_MEM_IN_PORT_FIELD_ACCESS-write_port_dot_field.jz` (4 triggers: .addr and .data on IN port in helper, .addr and .data on IN port in top)
- `MEM_WRITE_IN_ASYNC_BLOCK` — tested in `7_3_MEM_WRITE_IN_ASYNC_BLOCK-mem_write_in_async.jz` (2 triggers: helper, top)
- `MEM_WRITE_TO_READ_PORT` — tested in `7_3_MEM_WRITE_TO_READ_PORT-write_to_read_port.jz` (2 triggers: ASYNC OUT in helper, SYNC OUT in top)
- `MEM_READ_FROM_WRITE_PORT` — tested in `7_3_MEM_READ_FROM_WRITE_PORT-read_from_write_port.jz` (2 triggers: helper, top)
- `MEM_ADDR_WIDTH_TOO_WIDE` — tested in `7_3_MEM_ADDR_WIDTH_TOO_WIDE-wide_address.jz` (4 triggers: async read, sync write in helper, async read, sync .addr in top)
- `MEM_MULTIPLE_WRITES_SAME_IN` — tested in `7_3_MEM_MULTIPLE_WRITES_SAME_IN-double_write.jz` (2 triggers: helper, top)
- `MEM_MULTIPLE_ASSIGN_SYNC_READ_OUT` — tested in `7_3_MEM_MULTIPLE_ASSIGN_SYNC_READ_OUT-multi_addr_sync_read.jz` (2 triggers: helper, top)
- `MEM_CONST_ADDR_OUT_OF_RANGE` — tested in `7_3_MEM_CONST_ADDR_OUT_OF_RANGE-const_addr_overflow.jz` (4 triggers: async read, sync write in helper, async read, sync write in top)
- `MEM_INOUT_INDEXED` — tested in `7_3_MEM_INOUT_INDEXED-inout_bracket.jz` (2 triggers: helper, top)
- `MEM_INOUT_WDATA_IN_ASYNC` — tested in `7_3_MEM_INOUT_WDATA_IN_ASYNC-wdata_in_async.jz` (2 triggers: helper, top)
- `MEM_INOUT_ADDR_IN_ASYNC` — tested in `7_3_MEM_INOUT_ADDR_IN_ASYNC-addr_in_async.jz` (2 triggers: helper, top)
- `MEM_INOUT_WDATA_WRONG_OP` — tested in `7_3_MEM_INOUT_WDATA_WRONG_OP-inout_wdata_equals.jz` (2 triggers: helper, top)
- `MEM_MULTIPLE_ADDR_ASSIGNS` — tested in `7_3_MEM_MULTIPLE_ADDR_ASSIGNS-inout_multi_addr.jz` (3 triggers: helper, 2× top)
- `MEM_MULTIPLE_WDATA_ASSIGNS` — tested in `7_3_MEM_MULTIPLE_WDATA_ASSIGNS-inout_multi_wdata.jz` (2 triggers: helper, top)
- `MEM_INVALID_WRITE_MODE` — not tested here; tested in section 7.0 (`7_0_MEM_INVALID_WRITE_MODE-bad_write_mode_value.jz`)

## Section 7.5 — Initialization

All rules from this section are tested:
- `MEM_INIT_LITERAL_OVERFLOW` — tested in `7_5_MEM_INIT_LITERAL_OVERFLOW-value_exceeds_width.jz` (2 triggers: helper, top)
- `MEM_INIT_CONTAINS_X` — tested in `7_5_MEM_INIT_CONTAINS_X-x_in_literal_init.jz` (2 triggers: partial-x helper, all-x top)
- `MEM_INIT_FILE_NOT_FOUND` — tested in `7_5_MEM_INIT_FILE_NOT_FOUND-nonexistent_file.jz` (2 triggers: helper, top)
- `MEM_INIT_FILE_TOO_LARGE` — tested in `7_5_MEM_INIT_FILE_TOO_LARGE-file_exceeds_depth.jz` (2 triggers: helper, top)
- `MEM_INIT_FILE_CONTAINS_X` — tested in `7_5_MEM_INIT_FILE_CONTAINS_X-xz_in_file.jz` (2 triggers: .hex file, .mem file)
- `MEM_WARN_PARTIAL_INIT` — tested in `7_5_MEM_WARN_PARTIAL_INIT-file_smaller_than_depth.jz` (2 triggers: helper, top)
- `CONST_NUMERIC_IN_STRING_CONTEXT` — tested in `7_5_CONST_NUMERIC_IN_STRING_CONTEXT-numeric_const_in_file.jz` (2 triggers: module CONST, CONFIG value)

Cross-referenced rules tested in other sections:
- `MEM_MISSING_INIT` — tested in `7_1_MEM_MISSING_INIT-missing_init.jz` (section 7.1)

## Section 9.7 — @check Error Conditions

All rules from this section are tested:
- `CHECK_FAILED` — tested in `9_7_CHECK_FAILED-false_assertion.jz` (6 triggers: project scope CONFIG comparison, project scope expression-to-zero, helper CONST comparison, helper literal zero, top clog2 mismatch, top logical AND)
- `CHECK_INVALID_EXPR_TYPE` — tested in `9_7_CHECK_INVALID_EXPR_TYPE-runtime_signals.jz` (10 triggers: bare port/register/slice in helper, bare port/register/wire/slices in top) and `9_7_CHECK_INVALID_EXPR_TYPE-undefined_identifier.jz` (4 triggers: bare undefined, undefined in arithmetic, in comparison, inside clog2)
- `DIRECTIVE_INVALID_CONTEXT` — tested in `9_7_DIRECTIVE_INVALID_CONTEXT-check_in_async.jz` (1 trigger + cascading PARSE000) and `9_7_DIRECTIVE_INVALID_CONTEXT-check_in_sync.jz` (1 trigger + cascading PARSE000)
- `CHECK_INVALID_PLACEMENT` — tested in `9_7_CHECK_INVALID_PLACEMENT-check_in_feature.jz` (3 triggers: @feature THEN in helper, @feature THEN in top, @feature ELSE in top)

No untested rules.

## Misc — Repeat, Serializer, and IO Rules

Tested rules:
- `RPT_COUNT_INVALID` — tested in `misc_RPT_COUNT_INVALID-zero_count.jz` (zero count), `misc_RPT_COUNT_INVALID-negative_count.jz` (negative count), `misc_RPT_COUNT_INVALID-non_numeric_count.jz` (non-numeric count)
- `RPT_NO_MATCHING_END` — tested in `misc_RPT_NO_MATCHING_END-missing_end.jz` (missing @end)
- Happy path: `misc_HAPPY_PATH-repeat_ok.jz` (valid @repeat 1 expansion)

Rules not testable via `--info --lint`:
- `INFO_SERIALIZER_CASCADE` — backend-only diagnostic, not reachable via lint
- `SERIALIZER_WIDTH_EXCEEDS_RATIO` — backend-only diagnostic, not reachable via lint
- `IO_BACKEND` — runtime I/O error, not reachable via lint
- `IO_IR` — runtime I/O error, not reachable via lint

## Simulation Rules

Tested rules:
- `SIM_WRONG_TOOL` — tested in `sim_SIM_WRONG_TOOL-simulation_with_lint.jz` (@simulation block detected by --lint)
- `SIM_PROJECT_MIXED` — tested in `sim_SIM_PROJECT_MIXED-project_and_simulation.jz` (@project and @simulation in same file)

Rules not testable via `--info --lint`:
- `SIM_RUN_COND_TIMEOUT` — runtime simulation error, fires only during `--simulate` when @run_until/@run_while condition is not met within timeout; not reachable via lint
