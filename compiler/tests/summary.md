# Test Coverage Summary

### Section 1: Rules Not Tested

| Rule                                          | Reason        |
|-----------------------------------------------|---------------|
| AMBIGUOUS_REFERENCE                           | Unimplemented |
| BLACKBOX_UNDEFINED_IN_NEW                     | Unimplemented |
| BUS_DEF_INVALID_DIR                           | Dead          |
| BUS_PORT_NOT_BUS                              | Unimplemented |
| BUS_PORT_UNKNOWN_BUS                          | Unimplemented |
| CDC_DEST_ALIAS_ASSIGNED                       | Unimplemented |
| CDC_SOURCE_NOT_PLAIN_REG                      | Unimplemented |
| CHECK_INVALID_PLACEMENT                       | Unimplemented |
| CLOCK_GEN_INVALID_TYPE                        | Parser        |
| CONFIG_MULTIPLE_BLOCKS                        | Parser        |
| CONST_CIRCULAR_DEP                            | Suppressed    |
| FEATURE_VALIDATION_BOTH_PATHS                 | Unimplemented |
| FUNC_RESULT_TRUNCATED_SILENTLY                | Unimplemented |
| GBIT_INDEX_OUT_OF_RANGE                       | Bug           |
| GSLICE_INDEX_OUT_OF_RANGE                     | Bug           |
| INFO_SERIALIZER_CASCADE                       | Infrastructure|
| INSTANCE_ARRAY_MULTI_DIMENSIONAL              | Parser        |
| IO_BACKEND                                    | Infrastructure|
| IO_IR                                         | Infrastructure|
| LATCH_IN_CONST_CONTEXT                        | Suppressed    |
| LATCH_SR_WIDTH_MISMATCH                       | Suppressed    |
| LATCH_WIDTH_INVALID                           | Suppressed    |
| MAP_INVALID_BOARD_PIN_ID                      | Unimplemented |
| MEM_INIT_FILE_CONTAINS_X                      | Unimplemented |
| MEM_INOUT_WDATA_WRONG_OP                      | Suppressed    |
| MEM_MULTIPLE_ADDR_ASSIGNS                     | Unimplemented |
| MEM_MULTIPLE_WDATA_ASSIGNS                    | Unimplemented |
| MEM_READ_SYNC_WITH_EQUALS                     | Suppressed    |
| MEM_SYNC_ADDR_IN_ASYNC_BLOCK                  | Suppressed    |
| MEM_SYNC_ADDR_WITHOUT_RECEIVE                 | Suppressed    |
| MEM_UNDEFINED_NAME                            | Suppressed    |
| NET_TRI_STATE_ALL_Z_READ                      | Dead          |
| PATH_OUTSIDE_SANDBOX                          | Infrastructure|
| PATH_SYMLINK_ESCAPE                           | Infrastructure|
| PORT_MISSING_WIDTH                            | Parser        |
| REG_MISSING_INIT_LITERAL                      | Parser        |
| REG_MULTI_DIMENSIONAL                         | Parser        |
| SBIT_INDEX_OUT_OF_RANGE                       | Bug           |
| SERIALIZER_WIDTH_EXCEEDS_RATIO                | Infrastructure|
| SIM_RUN_COND_TIMEOUT                          | Behavioral    |
| SPECIAL_DRIVER_IN_INDEX                       | Parser        |
| SSLICE_INDEX_OUT_OF_RANGE                     | Bug           |
| SYNC_SLICE_WIDTH_MISMATCH                     | Suppressed    |
| TEMPLATE_APPLY_OUTSIDE_BLOCK                  | Unimplemented |
| TEMPLATE_SCRATCH_WIDTH_INVALID                | Unimplemented |
| TRISTATE_TRANSFORM_BLACKBOX_PORT              | Unimplemented |
| TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL      | Suppressed    |
| TRISTATE_TRANSFORM_PER_BIT_FAIL               | Unimplemented |
| TRISTATE_TRANSFORM_SINGLE_DRIVER              | Unimplemented |
| WARN_INCOMPLETE_SELECT_ASYNC                  | Dead          |
| WIDTHOF_INVALID_SYNTAX                        | Unimplemented |
| WIDTHOF_INVALID_TARGET                        | Unimplemented |
| WIDTHOF_WIDTH_NOT_RESOLVABLE                  | Unimplemented |
| WIDTH_ASSIGN_MISMATCH_NO_EXT                  | Suppressed    |
| WIRE_MULTI_DIMENSIONAL                        | Parser        |

### Section 2: Bugs Found

| Rule                                          | Severity    | Description                                                                              |
|-----------------------------------------------|-------------|------------------------------------------------------------------------------------------|
| TRISTATE_TRANSFORM_SINGLE_DRIVER              | Bug         | Rule defined but never emitted; only INFO_TRISTATE_TRANSFORM fires                      |
| WIDTHOF_INVALID_SYNTAX                        | Bug         | Rule defined but never emitted for slice arguments                                       |
| WIDTHOF_INVALID_TARGET                        | Bug         | Rule defined but never emitted for CONST names                                           |
| WIDTHOF_WIDTH_NOT_RESOLVABLE                  | Bug         | Rule defined but never emitted for unresolvable targets                                  |
| WIRE_MULTI_DIMENSIONAL                        | Bug         | Parser emits PARSE000; semantic rule unreachable                                         |
| BUS_DEF_INVALID_DIR                           | Dead code   | Parser rejects invalid BUS direction; semantic check unreachable                         |
| LATCH_WIDTH_INVALID                           | Dead code   | WIDTH_NONPOSITIVE_OR_NONINT always fires first for same condition                        |
| NET_TRI_STATE_ALL_Z_READ                      | Dead code   | ASYNC_FLOATING_Z_READ covers same condition; no code emits this rule                     |
| WARN_INCOMPLETE_SELECT_ASYNC                  | Dead code   | SELECT_DEFAULT_RECOMMENDED_ASYNC covers same condition; redundant rule                   |
| CONST_NUMERIC_IN_STRING_CONTEXT               | Gap         | Not emitted for CONFIG.name in @file(); parser fires PARSE000 instead                   |
| FEATURE_VALIDATION_BOTH_PATHS                 | Gap         | Rule defined but no semantic pass validates both feature paths                           |
| LATCH_AS_CLOCK_OR_CDC                         | Gap         | Only checks CLK parameter; latch as CDC source not detected                              |
| MAP_DUP_PHYSICAL_LOCATION                     | Gap         | Does not check differential P/N values for duplicate locations                           |
| PATH_OUTSIDE_SANDBOX                          | Gap         | Requires filesystem sandbox config; not testable via --lint                              |
| PATH_SYMLINK_ESCAPE                           | Gap         | Requires actual symlinks; not testable via --lint                                        |
| CONST_USED_WHERE_FORBIDDEN                    | Limitation  | CONST identifiers rejected in CASE values; spec says they should work                   |
| FEATURE_NESTED                                | Limitation  | Compiler stops processing after first occurrence per file                                |
| LIT_BARE_INTEGER                              | Limitation  | Semantic literal rules untestable in async context due to co-firing                      |
| @import                                       | Limitation  | Path error stops compilation; prevents combining @import and @file() tests               |
| @apply deduplication                          | Observation | Errors deduplicated by template source location across modules                           |
| @repeat                                       | Observation | Errors are fatal; only one trigger per file is possible                                  |
| CDC in template                               | Observation | Triggers TEMPLATE_FORBIDDEN_DECL instead of TEMPLATE_FORBIDDEN_BLOCK_HEADER              |
| CONFIG intra-references                       | Observation | Requires bare names inside CONFIG block; CONFIG.name syntax rejected                     |
| KEYWORD_AS_IDENTIFIER                         | Observation | Most spec-listed reserved identifiers (PLL, BIT, BUS, etc.) not reserved                 |
| Multi-driver priority chain                   | Observation | S11.4.1 priority chain not expressible; semantic rules block the pattern                 |
| OE extraction                                 | Observation | Only fails on concatenated z literals; IR check narrower than AST check                  |
| PROJECT_MISSING_ENDPROJ                       | Observation | Diagnostic location points to @endmod, not @project                                      |
| Register names (block keywords)               | Observation | Block-initiating keywords as register names cause cascading parse failures               |
| TRISTATE_TRANSFORM_OE_EXTRACT_FAIL            | Observation | Reports File: <input> instead of actual filename                                         |
| TRISTATE_TRANSFORM_UNUSED_DEFAULT             | Observation | Reports File: <input> instead of actual filename                                         |
| WIDTH_ASSIGN_MISMATCH_NO_EXT                  | Observation | Always suppressed by higher-priority ASSIGN_WIDTH_NO_MODIFIER                            |
| abs()                                         | Observation | Returns width+1 bits, not width; may be intentional for -128 case                       |
