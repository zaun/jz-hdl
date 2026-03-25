# Test Coverage Summary

## Rules Not Tested

| Rule                                          | Reason         |
|-----------------------------------------------|----------------|
| AMBIGUOUS_REFERENCE                           | Unimplemented  |
| BLACKBOX_UNDEFINED_IN_NEW                     | Suppressed     |
| BUS_DEF_INVALID_DIR                           | Dead           |
| BUS_PORT_NOT_BUS                              | Unimplemented  |
| BUS_PORT_UNKNOWN_BUS                          | Unimplemented  |
| CDC_DEST_ALIAS_ASSIGNED                       | Unimplemented  |
| CDC_SOURCE_NOT_PLAIN_REG                      | Bug            |
| CHECK_INVALID_PLACEMENT                       | Unimplemented  |
| CLOCK_GEN_INVALID_TYPE                        | Parser         |
| CONFIG_MULTIPLE_BLOCKS                        | Parser         |
| CONST_CIRCULAR_DEP                            | Suppressed     |
| FEATURE_VALIDATION_BOTH_PATHS                 | Unimplemented  |
| FUNC_RESULT_TRUNCATED_SILENTLY                | Suppressed     |
| GBIT_INDEX_OUT_OF_RANGE                       | Bug            |
| GSLICE_INDEX_OUT_OF_RANGE                     | Bug            |
| INFO_SERIALIZER_CASCADE                       | Infrastructure |
| INSTANCE_ARRAY_MULTI_DIMENSIONAL              | Parser         |
| LATCH_IN_CONST_CONTEXT                        | Suppressed     |
| LATCH_SR_WIDTH_MISMATCH                       | Suppressed     |
| LATCH_WIDTH_INVALID                           | Dead           |
| MAP_INVALID_BOARD_PIN_ID                      | Unimplemented  |
| MEM_INIT_FILE_CONTAINS_X                      | Unimplemented  |
| MEM_INOUT_WDATA_WRONG_OP                      | Suppressed     |
| MEM_MULTIPLE_ADDR_ASSIGNS                     | Unimplemented  |
| MEM_MULTIPLE_WDATA_ASSIGNS                    | Unimplemented  |
| MEM_READ_SYNC_WITH_EQUALS                     | Suppressed     |
| MEM_SYNC_ADDR_IN_ASYNC_BLOCK                  | Suppressed     |
| MEM_SYNC_ADDR_WITHOUT_RECEIVE                 | Suppressed     |
| MEM_UNDEFINED_NAME                            | Suppressed     |
| NET_TRI_STATE_ALL_Z_READ                      | Suppressed     |
| PATH_OUTSIDE_SANDBOX                          | Infrastructure |
| PATH_SYMLINK_ESCAPE                           | Infrastructure |
| PORT_MISSING_WIDTH                            | Parser         |
| REG_MISSING_INIT_LITERAL                      | Parser         |
| REG_MULTI_DIMENSIONAL                         | Parser         |
| SBIT_INDEX_OUT_OF_RANGE                       | Bug            |
| SERIALIZER_WIDTH_EXCEEDS_RATIO                | Infrastructure |
| SLICE_INDEX_INVALID                           | Parser         |
| SPECIAL_DRIVER_IN_INDEX                       | Parser         |
| SSLICE_INDEX_OUT_OF_RANGE                     | Bug            |
| SYNC_SLICE_WIDTH_MISMATCH                     | Suppressed     |
| TEMPLATE_APPLY_OUTSIDE_BLOCK                  | Unimplemented  |
| TEMPLATE_SCRATCH_WIDTH_INVALID                | Unimplemented  |
| TRISTATE_TRANSFORM_BLACKBOX_PORT              | Unimplemented  |
| TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL      | Suppressed     |
| TRISTATE_TRANSFORM_PER_BIT_FAIL               | Unimplemented  |
| TRISTATE_TRANSFORM_SINGLE_DRIVER              | Unimplemented  |
| WARN_INCOMPLETE_SELECT_ASYNC                  | Suppressed     |
| WARN_UNUSED_PORT                              | Suppressed     |
| WARN_UNUSED_WIRE                              | Suppressed     |
| WIDTH_ASSIGN_MISMATCH_NO_EXT                  | Suppressed     |
| WIDTHOF_INVALID_SYNTAX                        | Unimplemented  |
| WIDTHOF_INVALID_TARGET                        | Unimplemented  |
| WIDTHOF_WIDTH_NOT_RESOLVABLE                  | Unimplemented  |
| WIRE_MULTI_DIMENSIONAL                        | Parser         |

## Bugs Found

| Rule                                          | Severity    | Description                                                                          |
|-----------------------------------------------|-------------|--------------------------------------------------------------------------------------|
| AMBIGUOUS_REFERENCE                           | Bug         | Rule defined in rules.c but no semantic pass emits it                                |
| ASYNC_INVALID_STATEMENT_TARGET                | Bug         | CONST on LHS in ASYNC block not detected by identifier check                        |
| BLACKBOX_UNDEFINED_IN_NEW                     | Bug         | INSTANCE_UNDEFINED_MODULE fires instead of blackbox-specific rule                    |
| BUS_PORT_NOT_BUS                              | Bug         | Not implemented, UNDECLARED_IDENTIFIER fires instead of BUS-specific rule            |
| BUS_PORT_UNKNOWN_BUS                          | Bug         | Not implemented, NET_DANGLING_UNUSED fires instead of BUS-specific rule              |
| BUS ports                                     | Bug         | False-positive WARN_UNCONNECTED_OUTPUT on individually accessed BUS ports             |
| CDC_DEST_ALIAS_ASSIGNED                       | Bug         | Writes to CDC destination alias produce no diagnostic in ASYNC                       |
| CDC_SOURCE_NOT_PLAIN_REG                      | Bug         | Sliced CDC source silently dropped instead of reporting error                        |
| FEATURE_COND_WIDTH_NOT_1                      | Bug         | Not checked in REGISTER/WIRE/CONST declaration blocks                                |
| FEATURE_EXPR_INVALID_CONTEXT                  | Bug         | Not checked in REGISTER/WIRE/CONST declaration blocks                                |
| FEATURE_NESTED                                | Bug         | Not checked in REGISTER/WIRE/CONST declaration blocks                                |
| FUNC_RESULT_TRUNCATED_SILENTLY                | Bug         | ASSIGN_WIDTH_NO_MODIFIER fires instead of function-specific rule                     |
| GBIT_INDEX_OUT_OF_RANGE                       | Bug         | Sized literals not parsed by constant index checker                                  |
| GSLICE_INDEX_OUT_OF_RANGE                     | Bug         | Sized literals not parsed by constant index checker                                  |
| INSTANCE_ARRAY_COUNT_INVALID                  | Bug         | False positive on valid CONST-based array counts                                     |
| INSTANCE_UNDEFINED_MODULE                     | Bug         | @top uses hardcoded TOP_MODULE_NOT_FOUND instead of standard rule ID                 |
| LATCH_IN_CONST_CONTEXT                        | Bug         | CHECK_INVALID_EXPR_TYPE fires instead of latch-specific rule                         |
| LATCH_SR_WIDTH_MISMATCH                       | Bug         | ASSIGN_WIDTH_NO_MODIFIER fires instead of latch-specific rule                        |
| LIT_INVALID_CONTEXT                           | Bug         | Cascading LIT_WIDTH_INVALID on subsequent valid lit() calls in same module            |
| MAP_INVALID_BOARD_PIN_ID                      | Bug         | Rule defined in rules.c but not referenced in any semantic pass                      |
| MEM_INIT_FILE_CONTAINS_X                      | Bug         | Rule defined in rules.c but never emitted by any semantic pass                       |
| MEM_INOUT_WDATA_WRONG_OP                      | Bug         | SYNC_NO_ALIAS fires instead of MEM-specific rule                                    |
| MEM_MULTIPLE_ADDR_ASSIGNS                     | Bug         | Rule defined in rules.c but semantic check not implemented                           |
| MEM_MULTIPLE_WDATA_ASSIGNS                    | Bug         | Rule defined in rules.c but semantic check not implemented                           |
| MEM_PORT_ADDR_READ                            | Bug         | Not detected for INOUT .addr reads, only for SYNC OUT                                |
| MEM_PORT_USED_AS_SIGNAL                       | Bug         | Not detected for INOUT bare port references, only for OUT                            |
| MEM_READ_SYNC_WITH_EQUALS                     | Bug         | SYNC_NO_ALIAS fires instead of MEM-specific rule                                    |
| MEM_SYNC_ADDR_IN_ASYNC_BLOCK                  | Bug         | ASYNC_INVALID_STATEMENT_TARGET fires instead of MEM-specific rule                    |
| MEM_SYNC_ADDR_WITHOUT_RECEIVE                 | Bug         | SYNC_NO_ALIAS fires instead of MEM-specific rule                                    |
| MUX_SELECTOR_OUT_OF_RANGE_CONST               | Bug         | Sized literals not parsed by constant index checker                                  |
| SBIT_INDEX_OUT_OF_RANGE                       | Bug         | Sized literals not parsed by constant index checker                                  |
| SSLICE_INDEX_OUT_OF_RANGE                     | Bug         | Sized literals not parsed by constant index checker                                  |
| TEMPLATE_APPLY_OUTSIDE_BLOCK                  | Bug         | @apply at module or file scope silently ignored, no diagnostic                       |
| TEMPLATE_FORBIDDEN_BLOCK_HEADER               | Bug         | Parser fails to recover after forbidden construct in template body                   |
| TEMPLATE_SCRATCH_WIDTH_INVALID                | Bug         | Invalid @scratch width expressions (zero, parameter) not detected                    |
| TRISTATE_TRANSFORM_BLACKBOX_PORT              | Bug         | No blackbox handling in tri-state transform engine                                   |
| TRISTATE_TRANSFORM_PER_BIT_FAIL               | Bug         | Per-bit tri-state patterns transformed without error                                 |
| TRISTATE_TRANSFORM_SINGLE_DRIVER              | Bug         | Single-driver tri-state warning never emitted during transform                       |
| VCC/GND as identifiers                        | Bug         | PARSE000 fires instead of KEYWORD_AS_IDENTIFIER for VCC/GND                         |
| WARN_INCOMPLETE_SELECT_ASYNC                  | Bug         | SELECT_DEFAULT_RECOMMENDED_ASYNC fires instead of this rule                          |
| WARN_UNUSED_PORT                              | Bug         | NET_DANGLING_UNUSED fires instead of port-specific warning                           |
| WARN_UNUSED_WIRE                              | Bug         | NET_DANGLING_UNUSED fires instead of wire-specific warning                           |
| WIDTHOF_INVALID_SYNTAX                        | Bug         | widthof() with slice argument produces no diagnostic                                 |
| WIDTHOF_INVALID_TARGET                        | Bug         | widthof() on CONST name produces no diagnostic                                       |
| WIDTHOF_WIDTH_NOT_RESOLVABLE                  | Bug         | Unresolvable widthof() target produces no diagnostic                                 |
| WIRE block names                              | Bug         | WIRE names skip ID_SYNTAX_INVALID and KEYWORD_AS_IDENTIFIER validation               |
| BUS_DEF_INVALID_DIR                           | Dead code   | Parser rejects invalid BUS direction before semantic check runs                      |
| CLOCK_GEN_INVALID_TYPE                        | Dead code   | Parser catches invalid generator type before semantic check runs                     |
| CONFIG_MULTIPLE_BLOCKS                        | Dead code   | Parser catches duplicate CONFIG block before semantic check runs                     |
| CONST_CIRCULAR_DEP                            | Dead code   | Module CONSTs emit CONST_NEGATIVE_OR_NONINT instead of circular dep                 |
| INSTANCE_ARRAY_MULTI_DIMENSIONAL              | Dead code   | Parser rejects multi-dimensional syntax before semantic check runs                   |
| LATCH_WIDTH_INVALID                           | Dead code   | WIDTH_NONPOSITIVE_OR_NONINT always fires first for same condition                    |
| MEM_UNDEFINED_NAME                            | Dead code   | UNDECLARED_IDENTIFIER always catches undeclared MEM access first                     |
| NET_TRI_STATE_ALL_Z_READ                      | Dead code   | ASYNC_FLOATING_Z_READ always fires first for all-z scenario                          |
| PORT_MISSING_WIDTH                            | Dead code   | Parser catches missing width before semantic check runs                              |
| REG_MISSING_INIT_LITERAL                      | Dead code   | Parser catches missing init literal before semantic check runs                       |
| REG_MULTI_DIMENSIONAL                         | Dead code   | Parser catches multi-dimensional syntax before semantic check runs                   |
| SLICE_INDEX_INVALID                           | Dead code   | Parser rejects non-integer slice constructs before semantic analysis                 |
| SPECIAL_DRIVER_IN_INDEX                       | Dead code   | Parser rejects GND/VCC in index position with PARSE000                               |
| SYNC_SLICE_WIDTH_MISMATCH                     | Dead code   | ASSIGN_SLICE_WIDTH_MISMATCH (priority 2) always suppresses this (priority 0)         |
| TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL      | Dead code   | NET_MULTIPLE_ACTIVE_DRIVERS blocks multi-driver patterns before transform             |
| WIDTH_ASSIGN_MISMATCH_NO_EXT                  | Dead code   | ASSIGN_WIDTH_NO_MODIFIER (priority 1) always suppresses this (priority 0)            |
| WIRE_MULTI_DIMENSIONAL                        | Dead code   | Parser catches multi-dimensional syntax before semantic check runs                   |
| CHECK_INVALID_PLACEMENT                       | Gap         | Cannot trigger until @feature body support is fully implemented                      |
| CONST_NUMERIC_IN_STRING_CONTEXT               | Gap         | CONFIG.name in @file() triggers PARSE000 instead of semantic rule                    |
| FEATURE_VALIDATION_BOTH_PATHS                 | Gap         | Rule defined in rules.c but both-path validation not enforced                        |
| LATCH_AS_CLOCK_OR_CDC                         | Gap         | Only validates CLK parameter, does not check CDC source context                      |
| MAP_DUP_PHYSICAL_LOCATION differential        | Gap         | Differential P/N values not checked for duplicate physical locations                 |
| PATH_OUTSIDE_SANDBOX                          | Gap         | Requires runtime filesystem sandbox, not testable with static files                  |
| PATH_SYMLINK_ESCAPE                           | Gap         | Requires actual filesystem symlinks, not testable with static files                  |
| CONST in CASE values                          | Limitation  | CONST identifiers rejected with CONST_USED_WHERE_FORBIDDEN in SELECT CASE            |
| CONST transitive resolution                   | Limitation  | CONST referencing another CONST fails with CONST_NEGATIVE_OR_NONINT                 |
| FEATURE_NESTED processing                     | Limitation  | Compiler stops processing after first nested @feature occurrence                     |
| OVERRIDE in @check                            | Limitation  | OVERRIDE values from @new not visible to child module @check at lint time            |
| @import path errors                           | Observation | Path security error halts compilation, prevents combining test triggers               |
| CDC in template                               | Observation | CDC block triggers TEMPLATE_FORBIDDEN_DECL instead of BLOCK_HEADER                   |
| CONFIG intra-references                       | Observation | CONFIG.name syntax not supported inside CONFIG block, bare names required             |
| Duplicate @apply errors                       | Observation | Multiple @apply violations collapsed to shared template source location              |
| Multi-driver priority chain                   | Observation | Spec multi-driver tri-state pattern not expressible in current compiler               |
| PROJECT_MISSING_ENDPROJ location              | Observation | Diagnostic location points to @endmod instead of @project directive                  |
| Reserved identifiers                          | Observation | Only IDX is reserved; spec lists PLL, DLL, CLKDIV, BIT, BUS, FIFO, etc.             |
| TRISTATE_TRANSFORM_OE_EXTRACT_FAIL            | Observation | Reports "File: <input>" instead of actual source filename                            |
| TRISTATE_TRANSFORM_UNUSED_DEFAULT             | Observation | Reports "File: <input>" instead of actual source filename                            |
| abs() output width                            | Observation | abs() returns width+1 bits (not width), may be intentional for overflow              |
