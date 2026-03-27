# Rule Coverage Map

**Total rules in `compiler/src/rules.c`: 375**

| Rule ID | Test Plan File | Status |
|---------|---------------|--------|
| COMMENT_IN_TOKEN | test_1_4-comments.md | Tested |
| COMMENT_NESTED_BLOCK | test_1_4-comments.md | Tested |
| DIRECTIVE_INVALID_CONTEXT | test_1_1-identifiers.md | Tested |
| KEYWORD_AS_IDENTIFIER | test_1_1-identifiers.md | Tested |
| IF_COND_MISSING_PARENS | test_5_3-conditional_statements.md | Tested |
| INSTANCE_UNDEFINED_MODULE | test_4_13-module_instantiation.md | Tested |
| LIT_DECIMAL_HAS_XZ | test_2_1-literals.md | Tested |
| LIT_INVALID_DIGIT_FOR_BASE | test_2_1-literals.md | Tested |
| ID_SYNTAX_INVALID | test_1_1-identifiers.md | Tested |
| ID_SINGLE_UNDERSCORE | test_1_1-identifiers.md | Tested |
| LIT_UNSIZED | test_2_1-literals.md | Tested |
| LIT_BARE_INTEGER | test_2_1-literals.md | Tested |
| LIT_UNDERSCORE_AT_EDGES | test_2_1-literals.md | Tested |
| LIT_UNDEFINED_CONST_WIDTH | test_2_1-literals.md | Tested |
| LIT_WIDTH_NOT_POSITIVE | test_2_1-literals.md | Tested |
| LIT_OVERFLOW | test_2_1-literals.md | Tested |
| TYPE_BINOP_WIDTH_MISMATCH | test_2_2-signedness_model.md | Tested |
| WIDTH_NONPOSITIVE_OR_NONINT | test_2_2-signedness_model.md | Tested |
| WIDTH_ASSIGN_MISMATCH_NO_EXT | test_2_3-bit_width_constraints.md | Not Tested: Suppressed |
| SLICE_MSB_LESS_THAN_LSB | test_1_3-bit_slicing_and_indexing.md | Tested |
| SLICE_INDEX_OUT_OF_RANGE | test_1_3-bit_slicing_and_indexing.md | Tested |
| SLICE_INDEX_INVALID | test_1_3-bit_slicing_and_indexing.md | Tested |
| UNARY_ARITH_MISSING_PARENS | test_3_1-operator_categories.md | Tested |
| LOGICAL_WIDTH_NOT_1 | test_3_1-operator_categories.md | Tested |
| TERNARY_COND_WIDTH_NOT_1 | test_3_1-operator_categories.md | Tested |
| TERNARY_BRANCH_WIDTH_MISMATCH | test_2_3-bit_width_constraints.md | Tested |
| CONCAT_EMPTY | test_3_1-operator_categories.md | Tested |
| DIV_CONST_ZERO | test_3_1-operator_categories.md | Tested |
| DIV_UNGUARDED_RUNTIME_ZERO | test_3_1-operator_categories.md | Tested |
| SPECIAL_DRIVER_IN_EXPRESSION | test_2_4-special_semantic_drivers.md | Tested |
| SPECIAL_DRIVER_IN_CONCAT | test_2_4-special_semantic_drivers.md | Tested |
| SPECIAL_DRIVER_SLICED | test_1_3-bit_slicing_and_indexing.md | Tested |
| SPECIAL_DRIVER_IN_INDEX | test_2_4-special_semantic_drivers.md | Not Tested: Dead code |
| SBIT_SET_WIDTH_NOT_1 | test_5_5-intrinsic_operators.md | Tested |
| GBIT_INDEX_OUT_OF_RANGE | test_5_5-intrinsic_operators.md | Not Tested: Bug |
| SBIT_INDEX_OUT_OF_RANGE | test_5_5-intrinsic_operators.md | Not Tested: Bug |
| GSLICE_INDEX_OUT_OF_RANGE | test_5_5-intrinsic_operators.md | Not Tested: Bug |
| GSLICE_WIDTH_INVALID | test_5_5-intrinsic_operators.md | Tested |
| SSLICE_INDEX_OUT_OF_RANGE | test_5_5-intrinsic_operators.md | Not Tested: Bug |
| SSLICE_WIDTH_INVALID | test_5_5-intrinsic_operators.md | Tested |
| SSLICE_VALUE_WIDTH_MISMATCH | test_5_5-intrinsic_operators.md | Tested |
| OH2B_INPUT_TOO_NARROW | test_5_5-intrinsic_operators.md | Tested |
| B2OH_WIDTH_INVALID | test_5_5-intrinsic_operators.md | Tested |
| PRIENC_INPUT_TOO_NARROW | test_5_5-intrinsic_operators.md | Tested |
| BSWAP_WIDTH_NOT_BYTE_ALIGNED | test_5_5-intrinsic_operators.md | Tested |
| ID_DUP_IN_MODULE | test_4_2-scope_and_uniqueness.md | Tested |
| MODULE_NAME_DUP_IN_PROJECT | test_4_2-scope_and_uniqueness.md | Tested |
| BLACKBOX_NAME_DUP_IN_PROJECT | test_4_2-scope_and_uniqueness.md | Tested |
| INSTANCE_NAME_DUP_IN_MODULE | test_4_2-scope_and_uniqueness.md | Tested |
| INSTANCE_NAME_CONFLICT | test_4_2-scope_and_uniqueness.md | Tested |
| UNDECLARED_IDENTIFIER | test_4_2-scope_and_uniqueness.md | Tested |
| AMBIGUOUS_REFERENCE | test_4_2-scope_and_uniqueness.md | Not Tested: Unimplemented |
| CONST_NEGATIVE_OR_NONINT | test_4_3-const.md | Tested |
| CONST_UNDEFINED_IN_WIDTH_OR_SLICE | test_1_3-bit_slicing_and_indexing.md | Tested |
| CONST_CIRCULAR_DEP | test_4_3-const.md | Not Tested: Bug |
| PORT_MISSING_WIDTH | test_4_4-port.md | Tested |
| PORT_DIRECTION_MISMATCH_IN | test_4_4-port.md | Tested |
| PORT_DIRECTION_MISMATCH_OUT | test_4_4-port.md | Tested |
| PORT_TRISTATE_MISMATCH | test_4_4-port.md | Tested |
| WIRE_MULTI_DIMENSIONAL | test_4_5-wire.md | Not Tested: Dead code |
| REG_MULTI_DIMENSIONAL | test_4_7-register.md | Not Tested: Dead code |
| REG_MISSING_INIT_LITERAL | test_4_7-register.md | Not Tested: Dead code |
| REG_INIT_CONTAINS_X | test_4_7-register.md | Tested |
| REG_INIT_CONTAINS_Z | test_4_7-register.md | Tested |
| REG_INIT_WIDTH_MISMATCH | test_4_7-register.md | Tested |
| WRITE_WIRE_IN_SYNC | test_4_11-synchronous_block.md | Tested |
| ASSIGN_TO_NON_REGISTER_IN_SYNC | test_4_11-synchronous_block.md | Tested |
| MODULE_MISSING_PORT | test_4_1-module_canonical_form.md | Tested |
| MODULE_PORT_IN_ONLY | test_4_1-module_canonical_form.md | Tested |
| LATCH_ASSIGN_NON_GUARDED | test_4_8-latches.md | Tested |
| LATCH_ASSIGN_IN_SYNC | test_4_8-latches.md | Tested |
| LATCH_ENABLE_WIDTH_NOT_1 | test_4_8-latches.md | Tested |
| LATCH_ALIAS_FORBIDDEN | test_4_8-latches.md | Tested |
| LATCH_INVALID_TYPE | test_4_8-latches.md | Tested |
| LATCH_WIDTH_INVALID | test_4_8-latches.md | Not Tested: Suppressed |
| LATCH_SR_WIDTH_MISMATCH | test_4_8-latches.md | Not Tested: Unimplemented |
| LATCH_AS_CLOCK_OR_CDC | test_4_8-latches.md | Tested |
| LATCH_IN_CONST_CONTEXT | test_4_8-latches.md | Not Tested: Unimplemented |
| LATCH_CHIP_UNSUPPORTED | test_4_8-latches.md | Tested |
| INSTANCE_MISSING_PORT | test_4_13-module_instantiation.md | Tested |
| INSTANCE_PORT_WIDTH_MISMATCH | test_4_13-module_instantiation.md | Tested |
| INSTANCE_PORT_DIRECTION_MISMATCH | test_4_13-module_instantiation.md | Tested |
| INSTANCE_BUS_MISMATCH | test_4_13-module_instantiation.md | Tested |
| INSTANCE_OVERRIDE_CONST_UNDEFINED | test_4_13-module_instantiation.md | Tested |
| INSTANCE_PORT_WIDTH_EXPR_INVALID | test_4_13-module_instantiation.md | Tested |
| INSTANCE_PARENT_SIGNAL_WIDTH_MISMATCH | test_4_13-module_instantiation.md | Tested |
| INSTANCE_ARRAY_COUNT_INVALID | test_4_13-module_instantiation.md | Tested |
| INSTANCE_ARRAY_IDX_INVALID_CONTEXT | test_4_13-module_instantiation.md | Tested |
| INSTANCE_ARRAY_PARENT_BIT_OVERLAP | test_4_13-module_instantiation.md | Tested |
| INSTANCE_ARRAY_IDX_SLICE_OUT_OF_RANGE | test_4_13-module_instantiation.md | Tested |
| INSTANCE_OUT_PORT_LITERAL | test_4_13-module_instantiation.md | Tested |
| INSTANCE_ARRAY_MULTI_DIMENSIONAL | test_4_13-module_instantiation.md | Not Tested: Dead code |
| MUX_ASSIGN_LHS | test_4_6-mux.md | Tested |
| MUX_AGG_SOURCE_WIDTH_MISMATCH | test_4_6-mux.md | Tested |
| MUX_AGG_SOURCE_INVALID | test_4_6-mux.md | Tested |
| MUX_SLICE_WIDTH_NOT_DIVISOR | test_4_6-mux.md | Tested |
| MUX_SELECTOR_OUT_OF_RANGE_CONST | test_4_6-mux.md | Tested |
| MUX_NAME_DUPLICATE | test_4_6-mux.md | Tested |
| ASSIGN_WIDTH_NO_MODIFIER | test_2_3-bit_width_constraints.md | Tested |
| ASSIGN_TRUNCATES | test_5_0-assignment_operators_summary.md | Tested |
| ASSIGN_SLICE_WIDTH_MISMATCH | test_5_0-assignment_operators_summary.md | Tested |
| ASSIGN_CONCAT_WIDTH_MISMATCH | test_2_3-bit_width_constraints.md | Tested |
| ASSIGN_MULTIPLE_SAME_BITS | test_10_6-template_exclusive_assignment.md | Tested |
| ASSIGN_INDEPENDENT_IF_SELECT | test_1_5-exclusive_assignment_rule.md | Tested |
| ASSIGN_SHADOWING | test_1_5-exclusive_assignment_rule.md | Tested |
| ASSIGN_SLICE_OVERLAP | test_1_5-exclusive_assignment_rule.md | Tested |
| ASYNC_UNDEFINED_PATH_NO_DRIVER | test_1_5-exclusive_assignment_rule.md | Tested |
| ASYNC_INVALID_STATEMENT_TARGET | test_4_10-asynchronous_block.md | Tested |
| ASYNC_ASSIGN_REGISTER | test_4_10-asynchronous_block.md | Tested |
| ASYNC_ALIAS_LITERAL_RHS | test_4_10-asynchronous_block.md | Tested |
| ASYNC_FLOATING_Z_READ | test_4_10-asynchronous_block.md | Tested |
| SYNC_MULTI_ASSIGN_SAME_REG_BITS | test_10_6-template_exclusive_assignment.md | Tested |
| SYNC_ROOT_AND_CONDITIONAL_ASSIGN | test_5_2-synchronous_assignments.md | Tested |
| SYNC_SLICE_WIDTH_MISMATCH | test_5_2-synchronous_assignments.md | Not Tested: Suppressed |
| SYNC_CONCAT_DUP_REG | test_5_2-synchronous_assignments.md | Tested |
| SYNC_NO_ALIAS | test_5_2-synchronous_assignments.md | Tested |
| DOMAIN_CONFLICT | test_4_11-synchronous_block.md | Tested |
| DUPLICATE_BLOCK | test_4_1-module_canonical_form.md | Tested |
| MULTI_CLK_ASSIGN | test_4_11-synchronous_block.md | Tested |
| CDC_SOURCE_NOT_REGISTER | test_4_12-cdc_block.md | Tested |
| CDC_BIT_WIDTH_NOT_1 | test_4_12-cdc_block.md | Tested |
| SYNC_CLK_WIDTH_NOT_1 | test_4_11-synchronous_block.md | Tested |
| SYNC_RESET_WIDTH_NOT_1 | test_4_11-synchronous_block.md | Tested |
| SYNC_EDGE_INVALID | test_4_11-synchronous_block.md | Tested |
| SYNC_RESET_ACTIVE_INVALID | test_4_11-synchronous_block.md | Tested |
| SYNC_RESET_TYPE_INVALID | test_4_11-synchronous_block.md | Tested |
| SYNC_UNKNOWN_PARAM | test_4_11-synchronous_block.md | Tested |
| SYNC_MISSING_CLK | test_4_11-synchronous_block.md | Tested |
| CDC_SOURCE_NOT_PLAIN_REG | test_4_12-cdc_block.md | Not Tested: Unimplemented |
| CDC_DEST_ALIAS_ASSIGNED | test_4_12-cdc_block.md | Not Tested: Unimplemented |
| CDC_STAGES_INVALID | test_4_12-cdc_block.md | Tested |
| CDC_TYPE_INVALID | test_4_12-cdc_block.md | Tested |
| CDC_RAW_STAGES_FORBIDDEN | test_4_12-cdc_block.md | Tested |
| CDC_PULSE_WIDTH_NOT_1 | test_4_12-cdc_block.md | Tested |
| CDC_DEST_ALIAS_DUP | test_4_12-cdc_block.md | Tested |
| SYNC_EDGE_BOTH_WARNING | test_4_11-synchronous_block.md | Tested |
| IF_COND_WIDTH_NOT_1 | test_5_3-conditional_statements.md | Tested |
| CONTROL_FLOW_OUTSIDE_BLOCK | test_5_3-conditional_statements.md | Tested |
| SELECT_DUP_CASE_VALUE | test_5_4-select_case_statements.md | Tested |
| ASYNC_ALIAS_IN_CONDITIONAL | test_4_10-asynchronous_block.md | Tested |
| SELECT_DEFAULT_RECOMMENDED_ASYNC | test_5_4-select_case_statements.md | Tested |
| SELECT_NO_MATCH_SYNC_OK | test_5_4-select_case_statements.md | Tested |
| SELECT_CASE_WIDTH_MISMATCH | test_5_4-select_case_statements.md | Tested |
| FEATURE_COND_WIDTH_NOT_1 | test_4_14-feature_guards.md | Tested |
| FEATURE_EXPR_INVALID_CONTEXT | test_4_14-feature_guards.md | Tested |
| FEATURE_NESTED | test_4_14-feature_guards.md | Tested |
| FEATURE_VALIDATION_BOTH_PATHS | test_4_14-feature_guards.md | Not Tested: Unimplemented |
| LIT_WIDTH_INVALID | test_5_5-intrinsic_operators.md | Tested |
| LIT_VALUE_INVALID | test_5_5-intrinsic_operators.md | Tested |
| LIT_VALUE_OVERFLOW | test_5_5-intrinsic_operators.md | Tested |
| LIT_INVALID_CONTEXT | test_5_5-intrinsic_operators.md | Tested |
| NET_FLOATING_WITH_SINK | test_1_2-fundamental_terms.md | Tested |
| NET_TRI_STATE_ALL_Z_READ | test_11_3-tristate_net_identification.md | Not Tested: Suppressed |
| NET_MULTIPLE_ACTIVE_DRIVERS | test_11_3-tristate_net_identification.md | Tested |
| NET_DANGLING_UNUSED | test_1_2-fundamental_terms.md | Tested |
| OBS_X_TO_OBSERVABLE_SINK | test_1_2-fundamental_terms.md | Tested |
| COMB_LOOP_UNCONDITIONAL | test_12_2-combinational_loop_errors.md | Tested |
| COMB_LOOP_CONDITIONAL_SAFE | test_12_2-combinational_loop_errors.md | Tested |
| BUS_DEF_DUP_NAME | test_6_8-bus_aggregation.md | Tested |
| BUS_DEF_SIGNAL_DUP_NAME | test_6_8-bus_aggregation.md | Tested |
| BUS_DEF_INVALID_DIR | test_6_8-bus_aggregation.md | Not Tested: Dead code |
| BUS_PORT_UNKNOWN_BUS | test_4_4-port.md | Tested |
| BUS_PORT_INVALID_ROLE | test_4_4-port.md | Tested |
| BUS_PORT_ARRAY_COUNT_INVALID | test_4_4-port.md | Tested |
| BUS_PORT_INDEX_REQUIRED | test_4_4-port.md | Tested |
| BUS_PORT_INDEX_NOT_ARRAY | test_4_4-port.md | Tested |
| BUS_PORT_INDEX_OUT_OF_RANGE | test_4_4-port.md | Tested |
| BUS_PORT_NOT_BUS | test_4_4-port.md | Tested |
| BUS_SIGNAL_UNDEFINED | test_4_4-port.md | Tested |
| BUS_SIGNAL_READ_FROM_WRITABLE | test_4_4-port.md | Tested |
| BUS_SIGNAL_WRITE_TO_READABLE | test_4_4-port.md | Tested |
| BUS_WILDCARD_WIDTH_MISMATCH | test_4_4-port.md | Tested |
| BUS_TRISTATE_MISMATCH | test_4_4-port.md | Tested |
| BUS_BULK_BUS_MISMATCH | test_6_8-bus_aggregation.md | Tested |
| BUS_BULK_ROLE_CONFLICT | test_6_8-bus_aggregation.md | Tested |
| PROJECT_MULTIPLE_PER_FILE | test_6_10-project_scope_and_uniqueness.md | Tested |
| PROJECT_NAME_NOT_UNIQUE | test_6_10-project_scope_and_uniqueness.md | Tested |
| PROJECT_MISSING_TOP_MODULE | test_6_9-top_level_module.md | Tested |
| PROJECT_CHIP_DATA_NOT_FOUND | test_6_1-project_purpose.md | Tested |
| PROJECT_CHIP_DATA_INVALID | test_6_1-project_purpose.md | Tested |
| IMPORT_OUTSIDE_PROJECT | test_6_2-project_canonical_form.md | Tested |
| IMPORT_NOT_AT_PROJECT_TOP | test_6_2-project_canonical_form.md | Tested |
| IMPORT_FILE_HAS_PROJECT | test_6_2-project_canonical_form.md | Tested |
| IMPORT_DUP_MODULE_OR_BLACKBOX | test_6_2-project_canonical_form.md | Tested |
| IMPORT_FILE_MULTIPLE_TIMES | test_6_2-project_canonical_form.md | Tested |
| PROJECT_MISSING_ENDPROJ | test_6_2-project_canonical_form.md | Tested |
| CONFIG_MULTIPLE_BLOCKS | test_6_3-config_block.md | Not Tested: Dead code |
| CONFIG_NAME_DUPLICATE | test_6_3-config_block.md | Tested |
| CONFIG_INVALID_EXPR_TYPE | test_6_3-config_block.md | Tested |
| CONFIG_FORWARD_REF | test_6_3-config_block.md | Tested |
| CONFIG_USE_UNDECLARED | test_6_3-config_block.md | Tested |
| CONFIG_CIRCULAR_DEP | test_6_3-config_block.md | Tested |
| CONFIG_USED_WHERE_FORBIDDEN | test_6_3-config_block.md | Tested |
| CONST_USED_WHERE_FORBIDDEN | test_6_3-config_block.md | Tested |
| CONST_STRING_IN_NUMERIC_CONTEXT | test_6_3-config_block.md | Tested |
| CONST_NUMERIC_IN_STRING_CONTEXT | test_6_3-config_block.md | Tested |
| CHECK_FAILED | test_9_2-check_semantics.md | Tested |
| CHECK_INVALID_EXPR_TYPE | test_9_1-check_syntax.md | Tested |
| CHECK_INVALID_PLACEMENT | test_9_3-check_placement_rules.md | Not Tested: Bug |
| GLOBAL_NAMESPACE_DUPLICATE | test_8_1-global_purpose.md | Tested |
| GLOBAL_CONST_NAME_DUPLICATE | test_8_3-global_semantics.md | Tested |
| GLOBAL_FORWARD_REF | test_8_5-global_errors.md | Tested |
| GLOBAL_CIRCULAR_DEP | test_8_5-global_errors.md | Tested |
| GLOBAL_INVALID_EXPR_TYPE | test_8_2-global_syntax.md | Tested |
| GLOBAL_CONST_USE_UNDECLARED | test_8_3-global_semantics.md | Tested |
| GLOBAL_USED_WHERE_FORBIDDEN | test_8_3-global_semantics.md | Tested |
| GLOBAL_ASSIGN_FORBIDDEN | test_8_1-global_purpose.md | Tested |
| CLOCK_PORT_WIDTH_NOT_1 | test_6_4-clocks_block.md | Tested |
| CLOCK_NAME_NOT_IN_PINS | test_6_4-clocks_block.md | Tested |
| CLOCK_DUPLICATE_NAME | test_6_4-clocks_block.md | Tested |
| CLOCK_PERIOD_NONPOSITIVE | test_6_4-clocks_block.md | Tested |
| CLOCK_EDGE_INVALID | test_6_4-clocks_block.md | Tested |
| PIN_DECLARED_MULTIPLE_BLOCKS | test_6_5-pin_blocks.md | Tested |
| PIN_INVALID_STANDARD | test_6_5-pin_blocks.md | Tested |
| PIN_DRIVE_MISSING_OR_INVALID | test_6_5-pin_blocks.md | Tested |
| PIN_BUS_WIDTH_INVALID | test_6_5-pin_blocks.md | Tested |
| PIN_DUP_NAME_WITHIN_BLOCK | test_6_5-pin_blocks.md | Tested |
| MAP_PIN_DECLARED_NOT_MAPPED | test_6_6-map_block.md | Tested |
| MAP_PIN_MAPPED_NOT_DECLARED | test_6_6-map_block.md | Tested |
| MAP_DUP_PHYSICAL_LOCATION | test_6_6-map_block.md | Tested |
| MAP_INVALID_BOARD_PIN_ID | test_6_6-map_block.md | Not Tested: Unimplemented |
| PIN_MODE_INVALID | test_6_5-pin_blocks.md | Tested |
| PIN_MODE_STANDARD_MISMATCH | test_6_5-pin_blocks.md | Tested |
| PIN_PULL_INVALID | test_6_5-pin_blocks.md | Tested |
| PIN_PULL_ON_OUTPUT | test_6_5-pin_blocks.md | Tested |
| PIN_TERM_INVALID | test_6_5-pin_blocks.md | Tested |
| PIN_TERM_ON_OUTPUT | test_6_5-pin_blocks.md | Tested |
| PIN_TERM_INVALID_FOR_STANDARD | test_6_5-pin_blocks.md | Tested |
| PIN_DIFF_OUT_MISSING_FCLK | test_6_5-pin_blocks.md | Tested |
| PIN_DIFF_OUT_MISSING_PCLK | test_6_5-pin_blocks.md | Tested |
| PIN_DIFF_OUT_MISSING_RESET | test_6_5-pin_blocks.md | Tested |
| MAP_DIFF_EXPECTED_PAIR | test_6_6-map_block.md | Tested |
| MAP_SINGLE_UNEXPECTED_PAIR | test_6_6-map_block.md | Tested |
| MAP_DIFF_MISSING_PN | test_6_6-map_block.md | Tested |
| MAP_DIFF_SAME_PIN | test_6_6-map_block.md | Tested |
| CLOCK_GEN_INPUT_NOT_DECLARED | test_6_4-clocks_block.md | Tested |
| CLOCK_GEN_INPUT_NO_PERIOD | test_6_4-clocks_block.md | Tested |
| CLOCK_GEN_INPUT_FREQ_OUT_OF_RANGE | test_6_4-clocks_block.md | Tested |
| CLOCK_GEN_OUTPUT_INVALID_SELECTOR | test_6_4-clocks_block.md | Tested |
| CLOCK_GEN_OUT_NOT_CLOCK | test_6_4-clocks_block.md | Tested |
| CLOCK_GEN_WIRE_IS_CLOCK | test_6_4-clocks_block.md | Tested |
| CLOCK_GEN_WIRE_IN_CLOCKS | test_6_4-clocks_block.md | Tested |
| CLOCK_GEN_OUTPUT_NOT_DECLARED | test_6_4-clocks_block.md | Tested |
| CLOCK_GEN_OUTPUT_HAS_PERIOD | test_6_4-clocks_block.md | Tested |
| CLOCK_GEN_OUTPUT_IS_INPUT_PIN | test_6_4-clocks_block.md | Tested |
| CLOCK_GEN_MULTIPLE_DRIVERS | test_6_4-clocks_block.md | Tested |
| CLOCK_GEN_INPUT_IS_SELF_OUTPUT | test_6_4-clocks_block.md | Tested |
| CLOCK_GEN_INVALID_TYPE | test_6_4-clocks_block.md | Not Tested: Dead code |
| CLOCK_GEN_MISSING_INPUT | test_6_4-clocks_block.md | Tested |
| CLOCK_GEN_REQUIRED_INPUT_MISSING | test_6_4-clocks_block.md | Tested |
| CLOCK_GEN_MISSING_OUTPUT | test_6_4-clocks_block.md | Tested |
| CLOCK_EXTERNAL_NO_PERIOD | test_6_4-clocks_block.md | Tested |
| CLOCK_SOURCE_AMBIGUOUS | test_6_4-clocks_block.md | Tested |
| CLOCK_GEN_PARAM_OUT_OF_RANGE | test_6_4-clocks_block.md | Tested |
| CLOCK_GEN_PARAM_TYPE_MISMATCH | test_6_4-clocks_block.md | Tested |
| CLOCK_GEN_DERIVED_OUT_OF_RANGE | test_6_4-clocks_block.md | Tested |
| CLOCK_GEN_NO_CHIP_DATA | test_6_4-clocks_block.md | Tested |
| BLACKBOX_BODY_DISALLOWED | test_6_7-blackbox_modules.md | Tested |
| BLACKBOX_UNDEFINED_IN_NEW | test_6_7-blackbox_modules.md | Not Tested: Dead code |
| BLACKBOX_OVERRIDE_UNCHECKED | test_6_7-blackbox_modules.md | Tested |
| TOP_PORT_NOT_LISTED | test_6_9-top_level_module.md | Tested |
| TOP_PORT_WIDTH_MISMATCH | test_6_9-top_level_module.md | Tested |
| TOP_PORT_PIN_DECL_MISSING | test_6_9-top_level_module.md | Tested |
| TOP_PORT_PIN_DIRECTION_MISMATCH | test_6_9-top_level_module.md | Tested |
| TOP_OUT_LITERAL_BINDING | test_6_9-top_level_module.md | Tested |
| TOP_NO_CONNECT_WITHOUT_WIDTH | test_6_9-top_level_module.md | Tested |
| MEM_UNDEFINED_NAME | test_7_1-mem_declaration.md | Not Tested: Dead code |
| MEM_DUP_NAME | test_7_1-mem_declaration.md | Tested |
| MEM_INVALID_WORD_WIDTH | test_7_1-mem_declaration.md | Tested |
| MEM_INVALID_DEPTH | test_7_1-mem_declaration.md | Tested |
| MEM_UNDEFINED_CONST_IN_WIDTH | test_7_1-mem_declaration.md | Tested |
| MEM_INIT_LITERAL_OVERFLOW | test_7_5-initialization.md | Tested |
| MEM_DUP_PORT_NAME | test_7_1-mem_declaration.md | Tested |
| MEM_PORT_NAME_CONFLICT_MODULE_ID | test_7_1-mem_declaration.md | Tested |
| MEM_EMPTY_PORT_LIST | test_7_1-mem_declaration.md | Tested |
| MEM_INVALID_PORT_TYPE | test_7_0-memory_port_modes.md | Tested |
| MEM_MISSING_INIT | test_7_1-mem_declaration.md | Tested |
| MEM_INIT_FILE_NOT_FOUND | test_7_5-initialization.md | Tested |
| MEM_INIT_CONTAINS_X | test_7_5-initialization.md | Tested |
| MEM_INIT_FILE_CONTAINS_X | test_7_5-initialization.md | Not Tested: Unimplemented |
| MEM_INIT_FILE_TOO_LARGE | test_7_5-initialization.md | Tested |
| MEM_TYPE_INVALID | test_4_9-mem_block.md | Tested |
| MEM_TYPE_BLOCK_WITH_ASYNC_OUT | test_7_1-mem_declaration.md | Tested |
| MEM_CHIP_CONFIG_UNSUPPORTED | test_7_1-mem_declaration.md | Tested |
| MEM_INOUT_MIXED_WITH_IN_OUT | test_7_1-mem_declaration.md | Tested |
| MEM_INOUT_ASYNC | test_7_1-mem_declaration.md | Tested |
| MEM_BLOCK_MULTI | test_7_11-synthesis_implications.md | Tested |
| MEM_BLOCK_RESOURCE_EXCEEDED | test_7_11-synthesis_implications.md | Tested |
| MEM_DISTRIBUTED_RESOURCE_EXCEEDED | test_7_11-synthesis_implications.md | Tested |
| MEM_PORT_UNDEFINED | test_7_3-memory_access_syntax.md | Tested |
| MEM_PORT_FIELD_UNDEFINED | test_7_3-memory_access_syntax.md | Tested |
| MEM_SYNC_PORT_INDEXED | test_7_3-memory_access_syntax.md | Tested |
| MEM_PORT_USED_AS_SIGNAL | test_7_3-memory_access_syntax.md | Tested |
| MEM_PORT_ADDR_READ | test_7_3-memory_access_syntax.md | Tested |
| MEM_ASYNC_PORT_FIELD_DATA | test_7_3-memory_access_syntax.md | Tested |
| MEM_SYNC_ADDR_INVALID_PORT | test_7_3-memory_access_syntax.md | Tested |
| MEM_SYNC_ADDR_IN_ASYNC_BLOCK | test_7_3-memory_access_syntax.md | Not Tested: Dead code |
| MEM_SYNC_DATA_IN_ASYNC_BLOCK | test_7_3-memory_access_syntax.md | Tested |
| MEM_SYNC_ADDR_WITHOUT_RECEIVE | test_7_3-memory_access_syntax.md | Not Tested: Dead code |
| MEM_READ_SYNC_WITH_EQUALS | test_7_3-memory_access_syntax.md | Not Tested: Dead code |
| MEM_IN_PORT_FIELD_ACCESS | test_7_3-memory_access_syntax.md | Tested |
| MEM_WRITE_IN_ASYNC_BLOCK | test_7_3-memory_access_syntax.md | Tested |
| MEM_WRITE_TO_READ_PORT | test_7_2-port_types_and_semantics.md | Tested |
| MEM_READ_FROM_WRITE_PORT | test_7_2-port_types_and_semantics.md | Tested |
| MEM_ADDR_WIDTH_TOO_WIDE | test_7_3-memory_access_syntax.md | Tested |
| MEM_MULTIPLE_WRITES_SAME_IN | test_7_2-port_types_and_semantics.md | Tested |
| MEM_MULTIPLE_ASSIGN_SYNC_READ_OUT | test_7_3-memory_access_syntax.md | Tested |
| MEM_CONST_ADDR_OUT_OF_RANGE | test_7_3-memory_access_syntax.md | Tested |
| MEM_INVALID_WRITE_MODE | test_7_0-memory_port_modes.md | Tested |
| MEM_INOUT_INDEXED | test_7_2-port_types_and_semantics.md | Tested |
| MEM_INOUT_WDATA_IN_ASYNC | test_7_2-port_types_and_semantics.md | Tested |
| MEM_INOUT_ADDR_IN_ASYNC | test_7_2-port_types_and_semantics.md | Tested |
| MEM_INOUT_WDATA_WRONG_OP | test_7_3-memory_access_syntax.md | Not Tested: Dead code |
| MEM_MULTIPLE_ADDR_ASSIGNS | test_7_3-memory_access_syntax.md | Not Tested: Unimplemented |
| MEM_MULTIPLE_WDATA_ASSIGNS | test_7_3-memory_access_syntax.md | Not Tested: Unimplemented |
| MEM_WARN_PORT_NEVER_ACCESSED | test_7_7-error_checking_and_validation.md | Tested |
| MEM_WARN_PARTIAL_INIT | test_7_5-initialization.md | Tested |
| MEM_WARN_DEAD_CODE_ACCESS | test_7_7-error_checking_and_validation.md | Tested |
| TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL | test_11_4-tristate_transformation_algorithm.md | Not Tested: Bug |
| TRISTATE_TRANSFORM_PER_BIT_FAIL | test_11_4-tristate_transformation_algorithm.md | Not Tested: Unimplemented |
| TRISTATE_TRANSFORM_BLACKBOX_PORT | test_11_6-tristate_inout_handling.md | Not Tested: Unimplemented |
| TRISTATE_TRANSFORM_SINGLE_DRIVER | test_11_4-tristate_transformation_algorithm.md | Not Tested: Unimplemented |
| TRISTATE_TRANSFORM_OE_EXTRACT_FAIL | test_11_6-tristate_inout_handling.md | Tested |
| TRISTATE_TRANSFORM_UNUSED_DEFAULT | test_11_1-tristate_default_purpose.md | Tested |
| INFO_TRISTATE_TRANSFORM | test_11_1-tristate_default_purpose.md | Tested |
| INFO_SERIALIZER_CASCADE | test_misc-repeat_serializer_io.md | Not Testable: backend-only, emitted during Verilog emission |
| SERIALIZER_WIDTH_EXCEEDS_RATIO | test_misc-repeat_serializer_io.md | Not Testable: backend-only, emitted during Verilog emission |
| TEMPLATE_UNDEFINED | test_10_5-template_application.md | Tested |
| TEMPLATE_ARG_COUNT_MISMATCH | test_10_5-template_application.md | Tested |
| TEMPLATE_COUNT_NOT_NONNEG_INT | test_10_5-template_application.md | Tested |
| TEMPLATE_NESTED_DEF | test_10_4-template_forbidden_content.md | Tested |
| TEMPLATE_FORBIDDEN_DECL | test_10_4-template_forbidden_content.md | Tested |
| TEMPLATE_FORBIDDEN_BLOCK_HEADER | test_10_4-template_forbidden_content.md | Tested |
| TEMPLATE_FORBIDDEN_DIRECTIVE | test_10_4-template_forbidden_content.md | Tested |
| TEMPLATE_SCRATCH_OUTSIDE | test_10_3-template_allowed_content.md | Tested |
| TEMPLATE_APPLY_OUTSIDE_BLOCK | test_10_5-template_application.md | Not Tested: Unimplemented |
| TEMPLATE_DUP_NAME | test_10_2-template_definition.md | Tested |
| TEMPLATE_DUP_PARAM | test_10_2-template_definition.md | Tested |
| TEMPLATE_SCRATCH_WIDTH_INVALID | test_10_3-template_allowed_content.md | Not Tested: Unimplemented |
| TEMPLATE_EXTERNAL_REF | test_10_3-template_allowed_content.md | Tested |
| TB_WRONG_TOOL | test_tb-testbench_rules.md | Tested |
| TB_PROJECT_MIXED | test_tb-testbench_rules.md | Tested |
| TB_MODULE_NOT_FOUND | test_tb-testbench_rules.md | Tested |
| TB_PORT_NOT_CONNECTED | test_tb-testbench_rules.md | Tested |
| TB_PORT_WIDTH_MISMATCH | test_tb-testbench_rules.md | Tested |
| TB_NEW_RHS_INVALID | test_tb-testbench_rules.md | Tested |
| TB_SETUP_POSITION | test_tb-testbench_rules.md | Tested |
| TB_CLOCK_NOT_DECLARED | test_tb-testbench_rules.md | Tested |
| TB_CLOCK_CYCLE_NOT_POSITIVE | test_tb-testbench_rules.md | Tested |
| TB_UPDATE_NOT_WIRE | test_tb-testbench_rules.md | Tested |
| TB_UPDATE_CLOCK_ASSIGN | test_tb-testbench_rules.md | Tested |
| TB_EXPECT_WIDTH_MISMATCH | test_tb-testbench_rules.md | Tested |
| TB_NO_TEST_BLOCKS | test_tb-testbench_rules.md | Tested |
| TB_MULTIPLE_NEW | test_tb-testbench_rules.md | Tested |
| SIM_WRONG_TOOL | test_sim-simulation_rules.md | Tested |
| SIM_PROJECT_MIXED | test_sim-simulation_rules.md | Tested |
| SIM_RUN_COND_TIMEOUT | test_sim-simulation_rules.md | Not Testable: simulation runtime-only, not reachable via --lint |
| RPT_COUNT_INVALID | test_misc-repeat_serializer_io.md | Tested |
| RPT_NO_MATCHING_END | test_misc-repeat_serializer_io.md | Tested |
| WARN_UNUSED_REGISTER | test_12_3-recommended_warnings.md | Tested |
| WARN_UNSINKED_REGISTER | test_12_3-recommended_warnings.md | Tested |
| WARN_UNDRIVEN_REGISTER | test_12_3-recommended_warnings.md | Tested |
| WARN_UNCONNECTED_OUTPUT | test_12_3-recommended_warnings.md | Tested |
| WARN_INCOMPLETE_SELECT_ASYNC | test_12_3-recommended_warnings.md | Not Tested: Dead code |
| WARN_DEAD_CODE_UNREACHABLE | test_12_3-recommended_warnings.md | Tested |
| WARN_UNUSED_MODULE | test_12_3-recommended_warnings.md | Tested |
| WARN_UNUSED_WIRE | test_12_3-recommended_warnings.md | Tested |
| WARN_UNUSED_PORT | test_12_3-recommended_warnings.md | Tested |
| WARN_INTERNAL_TRISTATE | test_11_1-tristate_default_purpose.md | Tested |
| IO_BACKEND | test_misc-repeat_serializer_io.md | Not Testable: backend-only, not reachable via --lint |
| IO_IR | test_misc-repeat_serializer_io.md | Not Testable: backend-only, not reachable via --lint |
| PATH_ABSOLUTE_FORBIDDEN | test_12_4-path_security.md | Tested |
| PATH_TRAVERSAL_FORBIDDEN | test_12_4-path_security.md | Tested |
| PATH_OUTSIDE_SANDBOX | test_12_4-path_security.md | Not Testable: requires filesystem sandbox configuration |
| PATH_SYMLINK_ESCAPE | test_12_4-path_security.md | Not Testable: requires actual symlinks on filesystem |
