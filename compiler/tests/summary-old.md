# Test Summary

## Rules Not Tested

| Rule                                     | Reason         |
|------------------------------------------|----------------|
| AMBIGUOUS_REFERENCE                      | Unimplemented  |
| BLACKBOX_UNDEFINED_IN_NEW                | Unimplemented  |
| BUS_PORT_ARRAY_COUNT_INVALID             | Bug            |
| BUS_PORT_NOT_BUS                         | Bug            |
| BUS_PORT_UNKNOWN_BUS                     | Bug            |
| CDC_BUS_GRAY_CODE                        | Missing        |
| CDC_DEST_ALIAS_ASSIGNED                  | Unimplemented  |
| CDC_DEST_WRONG_DOMAIN                    | Missing        |
| CDC_MISSING_BRIDGE                       | Missing        |
| CDC_SOURCE_NOT_PLAIN_REG                 | Dead           |
| CHECK_INVALID_PLACEMENT                  | Unimplemented  |
| COMB_LOOP_THROUGH_INSTANCE               | Missing        |
| CONFIG_MULTIPLE_BLOCKS                   | Parser         |
| CONST_CIRCULAR_DEP                       | Bug            |
| CONST_FORWARD_REF                        | Missing        |
| CONST_NUMERIC_IN_STRING_CONTEXT          | Bug            |
| FEATURE_VALIDATION_BOTH_PATHS            | Unimplemented  |
| FUNC_RESULT_TRUNCATED_SILENTLY           | Suppressed     |
| INFO_TRISTATE_TRANSFORM                  | Infrastructure |
| INSTANCE_ARRAY_MULTI_DIMENSIONAL         | Dead           |
| INSTANCE_PORT_WIDTH_MISMATCH             | Bug            |
| LATCH_CHIP_UNSUPPORTED                   | Infrastructure |
| LATCH_IN_CONST_CONTEXT                   | Unimplemented  |
| LATCH_SR_WIDTH_MISMATCH                  | Unimplemented  |
| LATCH_WIDTH_INVALID                      | Suppressed     |
| LIT_VALUE_INVALID                        | Dead           |
| MAP_INVALID_BOARD_PIN_ID                 | Infrastructure |
| MEM_INIT_CONTAINS_Z                      | Missing        |
| MEM_INOUT_WDATA_WRONG_OP                 | Dead           |
| MEM_MULTIPLE_ADDR_ASSIGNS                | Bug            |
| MEM_MULTIPLE_WDATA_ASSIGNS               | Bug            |
| MEM_READ_SYNC_WITH_EQUALS                | Dead           |
| MEM_SYNC_ADDR_IN_ASYNC_BLOCK             | Dead           |
| MEM_SYNC_ADDR_WITHOUT_RECEIVE            | Dead           |
| MEM_WRITE_MODE_ON_READ_PORT              | Missing        |
| MODULE_SELF_INSTANTIATION                | Missing        |
| NET_TRI_STATE_ALL_Z_READ                 | Dead           |
| PATH_OUTSIDE_SANDBOX                     | Infrastructure |
| PATH_SYMLINK_ESCAPE                      | Infrastructure |
| PORT_INOUT_NOT_BIDIRECTIONAL             | Missing        |
| PORT_OUTPUT_NOT_DRIVEN                   | Missing        |
| PRECEDENCE_AMBIGUOUS                     | Missing        |
| REG_MISSING_INIT_LITERAL                 | Dead           |
| REG_MULTI_DIMENSIONAL                    | Dead           |
| REG_Z_ASSIGN                             | Missing        |
| SELECT_CASE_NOT_CONSTANT                 | Missing        |
| SELECT_X_WILDCARD_OVERLAP                | Missing        |
| SLICE_INDEX_INVALID                      | Dead           |
| SPECIAL_DRIVER_IN_INDEX                  | Dead           |
| SYNC_SLICE_WIDTH_MISMATCH                | Suppressed     |
| TEMPLATE_APPLY_OUTSIDE_BLOCK             | Bug            |
| TEMPLATE_EXTERNAL_REF                    | Bug            |
| TEMPLATE_SCRATCH_WIDTH_INVALID           | Bug            |
| TRISTATE_TRANSFORM_BLACKBOX_PORT         | Infrastructure |
| TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL | Infrastructure |
| TRISTATE_TRANSFORM_OE_EXTRACT_FAIL       | Infrastructure |
| TRISTATE_TRANSFORM_PER_BIT_FAIL          | Unimplemented  |
| TRISTATE_TRANSFORM_SINGLE_DRIVER         | Infrastructure |
| TRISTATE_TRANSFORM_UNUSED_DEFAULT        | Infrastructure |
| WARN_INCOMPLETE_SELECT_ASYNC             | Unimplemented  |
| WARN_UNUSED_PORT                         | Unimplemented  |
| WARN_UNUSED_WIRE                         | Unimplemented  |
| WIDTH_ASSIGN_MISMATCH_NO_EXT             | Suppressed     |
| WIDTHOF_INVALID_SYNTAX                   | Unimplemented  |
| WIDTHOF_INVALID_TARGET                   | Unimplemented  |
| WIDTHOF_WIDTH_NOT_RESOLVABLE             | Unimplemented  |

## Bugs Found

| Rule                                | Severity      | Description                                                           |
|-------------------------------------|---------------|-----------------------------------------------------------------------|
| ASYNC_ALIAS_LITERAL_RHS             | Bug           | False positive on intrinsic function calls with literal arguments      |
| BLACKBOX_BODY_DISALLOWED            | Bug           | Incorrectly rejects CONST blocks inside blackbox bodies               |
| BUS_PORT_ARRAY_COUNT_INVALID        | Bug           | Rule never fires; code path unreachable for BUS port declarations     |
| BUS_PORT_NOT_BUS                    | Bug           | Dot-access on non-BUS ports fires UNDECLARED_IDENTIFIER instead       |
| BUS_PORT_UNKNOWN_BUS                | Bug           | Rule never fires; semantic check code path is unreachable             |
| BUS_SIGNAL_UNDEFINED                | Bug           | Scalar BUS write fires ASYNC_INVALID_STATEMENT_TARGET instead         |
| CONFIG.X backward ref               | Bug           | CONFIG.X syntax inside CONFIG block fires CONFIG_INVALID_EXPR_TYPE    |
| CONST assignment in ASYNC           | Bug           | Assigning to CONST in ASYNC block produces no error at all            |
| CONST_CIRCULAR_DEP                  | Bug           | Driver suppresses diagnostics; fires CONST_NEGATIVE_OR_NONINT         |
| CONST_NUMERIC_IN_STRING_CONTEXT     | Bug           | Parser rejects CONFIG references in @file() MEM initializer           |
| CONST_USED_WHERE_FORBIDDEN          | Bug           | CONST identifiers rejected in SELECT CASE contrary to spec            |
| FEATURE_COND_WIDTH_NOT_1            | Bug           | Condition validation only runs in ASYNC/SYNC, not decl blocks         |
| FEATURE_NESTED                      | Bug           | Nesting check not enforced in declaration blocks                      |
| ID_SINGLE_UNDERSCORE                | Bug           | Not enforced in WIRE block declarations                               |
| ID_SYNTAX_INVALID                   | Bug           | Not enforced in WIRE block declarations                               |
| INSTANCE_ARRAY_COUNT_INVALID        | Bug           | Rejects valid CONST expressions in instance array count               |
| INSTANCE_PORT_WIDTH_MISMATCH        | Bug           | Does not fire when child port uses CONST-based width                  |
| KEYWORD_AS_IDENTIFIER               | Bug           | Not enforced in WIRE blocks; inconsistent in REGISTER blocks          |
| LIT_OVERFLOW                        | Bug           | Does not fire in register init or @global block contexts              |
| MEM_MULTIPLE_ADDR_ASSIGNS           | Bug           | Rule exists but does not fire for duplicate .addr assignments         |
| MEM_MULTIPLE_WDATA_ASSIGNS          | Bug           | Rule exists but does not fire for duplicate .wdata assignments        |
| MEM_UNDEFINED_CONST_IN_WIDTH        | Bug           | Negative CONST corrupts resolution of valid sibling CONSTs            |
| MUX_SELECTOR_OUT_OF_RANGE_CONST     | Bug           | Does not handle sized literals; only bare integers checked            |
| NET_MULTIPLE_ACTIVE_DRIVERS         | Bug           | Does not fire for multiple unconditional instance drivers             |
| PARSE000 (@check recovery)          | Bug           | @check in blocks causes cascading parse errors from no recovery       |
| PARSE000 (intrinsic index)          | Bug           | strtoul parses sized literal prefix instead of actual value           |
| PARSE000 (template block)           | Bug           | Parser fails to recover after TEMPLATE_FORBIDDEN_BLOCK_HEADER         |
| PARSE000 (template directive)       | Bug           | Parser fails to recover after TEMPLATE_FORBIDDEN_DIRECTIVE            |
| TEMPLATE_APPLY_OUTSIDE_BLOCK        | Bug           | Rule never fires; @apply at module scope silently ignored             |
| TEMPLATE_EXTERNAL_REF               | Bug           | Rule never fires; external signal refs in templates accepted          |
| TEMPLATE_SCRATCH_WIDTH_INVALID      | Bug           | Rule never fires; zero-width @scratch silently accepted               |
| TOP_MODULE_NOT_FOUND                | Bug           | Rule ID not registered in rules.c; emitted as ad-hoc diagnostic      |
| VCC/GND as identifier               | Bug           | Produce PARSE000 instead of KEYWORD_AS_IDENTIFIER                     |
| CDC_DEST_ALIAS_ASSIGNED             | Dead code     | Rule exists but sem_report_rule never called with this ID             |
| CDC_SOURCE_NOT_PLAIN_REG            | Dead code     | Parser prevents required AST from being constructed                   |
| INSTANCE_ARRAY_MULTI_DIMENSIONAL    | Dead code     | Parser rejects syntax before semantic rule can fire                   |
| MEM_INOUT_WDATA_WRONG_OP            | Dead code     | Shadowed by SYNC_NO_ALIAS which fires first                          |
| MEM_READ_SYNC_WITH_EQUALS           | Dead code     | Shadowed by SYNC_NO_ALIAS which fires first                          |
| MEM_SYNC_ADDR_IN_ASYNC_BLOCK        | Dead code     | Shadowed by ASYNC_INVALID_STATEMENT_TARGET which fires first          |
| MEM_SYNC_ADDR_WITHOUT_RECEIVE       | Dead code     | Shadowed by SYNC_NO_ALIAS which fires first                          |
| REG_MISSING_INIT_LITERAL            | Dead code     | Parser rejects syntax before semantic rule can fire                   |
| REG_MULTI_DIMENSIONAL               | Dead code     | Parser rejects syntax before semantic rule can fire                   |
| SPECIAL_DRIVER_IN_INDEX             | Dead code     | Parser rejects GND/VCC in index before semantic check                 |
| WARN_INCOMPLETE_SELECT_ASYNC        | Dead code     | Rule never emitted; SELECT_DEFAULT_RECOMMENDED_ASYNC used             |
| WARN_UNUSED_WIRE                    | Dead code     | Rule never emitted; NET_DANGLING_UNUSED fires instead                 |
| WIDTHOF_INVALID_SYNTAX              | Dead code     | Rule exists but sem_report_rule never called with this ID             |
| WIDTHOF_INVALID_TARGET              | Dead code     | Rule exists but sem_report_rule never called with this ID             |
| WIDTHOF_WIDTH_NOT_RESOLVABLE        | Dead code     | Rule exists but sem_report_rule never called with this ID             |
| ALIAS_TRUNCATION                    | Gap           | No rule ID in rules.c for alias-specific truncation                   |
| AMBIGUOUS_REFERENCE                 | Gap           | Rule defined in rules.c but not implemented in any semantic pass      |
| ASYNC_INVALID_STATEMENT_TARGET      | Gap           | CONST assignment in ASYNC block not caught by this rule               |
| BLACKBOX_UNDEFINED_IN_NEW           | Gap           | Rule defined but INSTANCE_UNDEFINED_MODULE used instead               |
| CDC_BUS_GRAY_CODE                   | Gap           | No rule for Gray coding discipline on BUS CDC sources                 |
| CDC_DEST_WRONG_DOMAIN               | Gap           | No rule for dest alias used outside destination domain                |
| CDC_MISSING_BRIDGE                  | Gap           | No rule for cross-domain access without CDC bridge                    |
| CHECK_INVALID_PLACEMENT             | Gap           | Rule defined in rules.c but never used in compiler code               |
| CONST_FORWARD_REF                   | Gap           | No rule ID for module CONST forward references                        |
| FEATURE_VALIDATION_BOTH_PATHS       | Gap           | Rule defined but no semantic code enforces it                         |
| LATCH_IN_CONST_CONTEXT              | Gap           | Rule defined but no semantic code enforces it                         |
| LATCH_SR_WIDTH_MISMATCH             | Gap           | Rule defined but SR latch width validation not implemented            |
| MEM_INIT_CONTAINS_Z                 | Gap           | No rule ID for z bits in memory initialization literals               |
| MEM_WRITE_MODE_ON_READ_PORT         | Gap           | No rule for applying write mode to read-only OUT port                 |
| MODULE_SELF_INSTANTIATION           | Gap           | No rule ID for a module instantiating itself                          |
| PIN_IN_HAS_DRIVE                    | Gap           | No check rejects drive property on IN_PINS                           |
| PORT_INOUT_NOT_BIDIRECTIONAL        | Gap           | No rule for INOUT port not used bidirectionally                       |
| PORT_OUTPUT_NOT_DRIVEN              | Gap           | No rule for OUT port never driven within module                       |
| REG_Z_ASSIGN                        | Gap           | No error for z assigned to register next-state in SYNC               |
| RESERVED_IDENTIFIERS                | Gap           | PLL, DLL, CLKDIV, BIT, BUS, etc. not enforced as keywords            |
| TRISTATE_TRANSFORM_PER_BIT_FAIL     | Gap           | Rule defined but no code emits it; per-bit validation missing         |
| WARN_INCOMPLETE_SELECT_ASYNC        | Gap           | Rule exists but never emitted; covered by different rule              |
| WARN_UNUSED_PORT                    | Gap           | Rule defined but no semantic code emits it                            |
| PIN_DIFF_OUT_MISSING_FCLK           | Limitation    | Only fires for OUT_PINS, not INOUT_PINS differential pins            |
| PIN_DIFF_OUT_MISSING_PCLK           | Limitation    | Only fires for OUT_PINS, not INOUT_PINS differential pins            |
| PIN_DIFF_OUT_MISSING_RESET          | Limitation    | Only fires for OUT_PINS, not INOUT_PINS differential pins            |
| WARN_UNDRIVEN_REGISTER              | Limitation    | Does not detect registers read only in ASYNC blocks                   |
| ASYNC_FLOATING_Z_READ               | Observation   | Fires instead of NET_TRI_STATE_ALL_Z_READ for all-z scenarios         |
| FUNC_RESULT_TRUNCATED_SILENTLY      | Observation   | Always suppressed by ASSIGN_WIDTH_NO_MODIFIER priority                |
| LATCH_WIDTH_INVALID                 | Observation   | Always suppressed by WIDTH_NONPOSITIVE_OR_NONINT priority             |
| LIT_BARE_INTEGER                    | Observation   | Suppressed by ASSIGN_WIDTH_NO_MODIFIER when widths mismatch          |
| NET_TRI_STATE_ALL_Z_READ            | Observation   | May be dead code; ASYNC_FLOATING_Z_READ catches scenario first        |
| SLICE_INDEX_INVALID                 | Observation   | Cannot be triggered; other rules fire first in all scenarios          |
| SYNC_SLICE_WIDTH_MISMATCH           | Observation   | Suppressed by ASSIGN_SLICE_WIDTH_MISMATCH priority                    |
| WIDTH_ASSIGN_MISMATCH_NO_EXT        | Observation   | Always suppressed by ASSIGN_WIDTH_NO_MODIFIER priority                |
| Test plan S1.2                      | Documentation | Mismatched IDs: ASSIGN_MULTI_DRIVER, WIRE_UNDRIVEN, X_OBSERVABLE_SINK |
| Test plan S1.3                      | Documentation | Mismatched IDs: SLICE_OUT_OF_RANGE, SLICE_MSB_LT_LSB, CONST_UNDEFINED |
| Test plan S1.5                      | Documentation | Mismatched IDs: ASSIGN_MULTI_DRIVER, ASSIGN_SHADOW_OUTER, etc.       |
| Test plan S1.6                      | Documentation | Mismatched IDs: LIT_HEX_HAS_XZ, LIT_RESET_HAS_Z, etc.               |
| Test plan S2.1                      | Documentation | Mismatched IDs: LIT_WIDTH_ZERO, LIT_HEX_HAS_XZ, LIT_RESET_HAS_X     |
| Test plan S2.3                      | Documentation | Mismatched IDs: WIDTH_BINOP_MISMATCH, WIDTH_TERNARY_MISMATCH         |
| Test plan S2.4                      | Documentation | Mismatched IDs: SEMANTIC_DRIVER_IN_EXPR, IN_CONCAT, SLICED, etc.     |
| Test plan S3.1                      | Documentation | Mismatched IDs: WIDTH_BINOP_MISMATCH, LOGICAL_OP_MULTI_BIT           |
| Test plan S3.2                      | Documentation | Mismatched IDs: UNARY_NOT_PARENTHESIZED, EXPR_DIVISION_BY_ZERO       |
| Test plan S4.1                      | Documentation | Mismatched IDs: MODULE_NO_PORT, MODULE_ONLY_IN_PORTS                  |
| Test plan S4.2                      | Documentation | Mismatched IDs: MODULE_DUPLICATE_NAME, SCOPE_DUPLICATE_SIGNAL         |
| Test plan S4.3                      | Documentation | Mismatched IDs: CONST_DUPLICATE, CONST_FORWARD_REF, etc.             |
| Test plan S4.4                      | Documentation | Mismatched IDs: PORT_INPUT_DRIVEN_INSIDE, BUS_INDEX_OUT_OF_RANGE      |
| Test plan S4.5                      | Documentation | Mismatched IDs: ASSIGN_OP_WRONG_BLOCK, ASSIGN_MULTI_DRIVER           |
| Test plan S4.6                      | Documentation | Mismatched IDs: MUX_ASSIGN_READONLY, MUX_WIDTH_MISMATCH, etc.        |
| Test plan S4.7                      | Documentation | Mismatched IDs: ASSIGN_OP_WRONG_BLOCK, LIT_RESET_HAS_X, etc.        |
| Test plan S4.8                      | Documentation | Mismatched IDs: LATCH_IN_SYNC, LATCH_ENABLE_WIDTH, LATCH_IN_CDC      |
| Test plan S4.9                      | Documentation | Mismatched IDs: MEM_DECL_TYPE_INVALID                                 |
| Test plan S4.10                     | Documentation | Mismatched IDs: ALIAS_IN_CONDITIONAL, ALIAS_LITERAL_BAN, etc.        |
| Test plan S4.11                     | Documentation | Mismatched IDs: SYNC_DUPLICATE_BLOCK, SYNC_DOMAIN_CONFLICT, etc.     |
| Test plan S4.12                     | Documentation | Mismatched IDs: CDC_BIT_WIDTH, CDC_RAW_NO_STAGES, CDC_ALIAS_READONLY  |
| Test plan S4.13                     | Documentation | Mismatched IDs: INSTANCE_IDX_IN_OVERRIDE, INSTANCE_OVERLAP, etc.     |
| Test plan S4.14                     | Documentation | Mismatched IDs: FEATURE_RUNTIME_REF, FEATURE_EXPR_NOT_BOOLEAN         |
| Test plan S5.0                      | Documentation | Mismatched IDs: ASSIGN_TRUNCATION, WIDTH_ASSIGN_MISMATCH_NO_EXT      |
| Test plan S5.1                      | Documentation | Mismatched IDs: ALIAS_LITERAL_BAN, ASSIGN_OP_WRONG_BLOCK, etc.       |
| Test plan S5.2                      | Documentation | Mismatched IDs: ASSIGN_OP_WRONG_BLOCK, ASSIGN_SHADOW_OUTER, etc.     |
| Test plan S5.3                      | Documentation | Mismatched IDs: ASSIGN_INDEPENDENT_CHAIN_CONFLICT, COMB_LOOP_DETECTED |
| Test plan S5.4                      | Documentation | Mismatched IDs: SELECT_DUPLICATE_CASE, WARN_INCOMPLETE_SELECT_ASYNC   |
| Test plan S5.5                      | Documentation | Mismatched IDs: INTRINSIC_ARG_COUNT, INTRINSIC_BSWAP_NOT_BYTE, etc.  |
| Test plan S6.1                      | Documentation | Mismatched IDs: PROJECT_CHIP_NOT_FOUND, PROJECT_CHIP_JSON_MALFORMED   |
| Test plan S6.2                      | Documentation | Mismatched IDs: IMPORT_HAS_PROJECT, IMPORT_WRONG_POSITION             |
| Test plan S6.6                      | Documentation | Mismatched IDs: MAP_PIN_INVALID, MAP_DUPLICATE, etc.                  |
| Test plan S6.9                      | Documentation | Mismatched IDs: TOP_MODULE_NOT_FOUND vs INSTANCE_UNDEFINED_MODULE     |
| Test plan S7.0                      | Documentation | Mismatched IDs: MEM_PORT_MODE_INVALID                                  |
| Test plan S7.1                      | Documentation | Mismatched IDs: MEM_DECL_TYPE_INVALID, MEM_DECL_DEPTH_ZERO, etc.     |
| Test plan S8.4                      | Documentation | Mismatched IDs: WIDTH_ASSIGN_MISMATCH_NO_EXT                          |
| Test plan S8.5                      | Documentation | Mismatched IDs: GLOBAL_DUPLICATE_NAME, GLOBAL_READONLY, etc.          |
| Test plan S10.6                     | Documentation | Mismatched IDs: ASSIGN_MULTI_DRIVER                                   |
