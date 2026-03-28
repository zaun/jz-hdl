# Test Coverage Summary

### Section 1: Rules Not Tested

| Rule                                          | Reason         |
|-----------------------------------------------|----------------|
| BUS_DEF_INVALID_DIR                           | Dead           |
| BUS_PORT_NOT_BUS                              | Unimplemented  |
| BUS_PORT_UNKNOWN_BUS                          | Unimplemented  |
| INFO_SERIALIZER_CASCADE                       | Infrastructure |
| IO_BACKEND                                    | Infrastructure |
| IO_IR                                         | Infrastructure |
| LATCH_WIDTH_INVALID                           | Suppressed     |
| PATH_OUTSIDE_SANDBOX                          | Infrastructure |
| PATH_SYMLINK_ESCAPE                           | Infrastructure |
| SERIALIZER_WIDTH_EXCEEDS_RATIO                | Infrastructure |
| SIM_RUN_COND_TIMEOUT                          | Behavioral     |
| WIDTH_ASSIGN_MISMATCH_NO_EXT                  | Suppressed     |

### Section 2: Bugs Found

| Rule                                          | Severity       | Description                                                                             |
|-----------------------------------------------|----------------|-----------------------------------------------------------------------------------------|
| CDC_DEST_ALIAS_DUP                            | Bug            | Does not fire when dest alias conflicts with a WIRE name                                |
| COMB_LOOP_UNCONDITIONAL                       | Bug            | Combinational loops through instance ports not detected                                 |
| FEATURE_NESTED                                | Bug            | Not detected at module level (between block declarations)                               |
| @new port bindings                            | Bug            | Parser rejects =z and =s width modifiers in @new port bindings                         |
| ASSIGN_CONCAT_WIDTH_MISMATCH                  | Bug            | Fires for concat with <=z/<=s; extension modifiers not supported in concat              |
| BLACKBOX_BODY_DISALLOWED                      | Bug            | Parser rejects forbidden blocks with PARSE000 instead of semantic rule                  |
| DIRECTIVE_INVALID_CONTEXT                     | Bug            | Parser fails to skip @check expression; cascading PARSE000                              |
| FEATURE_NESTED                                | Limitation     | Compiler stops processing after first occurrence per file                               |
| LIT_BARE_INTEGER                              | Limitation     | Semantic literal rules untestable in async context due to co-firing                     |
| @import                                       | Limitation     | Path error stops compilation; prevents combining @import and @file() tests              |
| CONST_USED_WHERE_FORBIDDEN                    | Limitation     | CONST has no width; correctly rejected in CASE. Spec corrected: use @global             |
| @apply deduplication                          | Observation    | Errors deduplicated by template source location across modules                          |
| @repeat                                       | Observation    | Errors are fatal; only one trigger per file is possible                                 |
| CDC in template                               | Observation    | Triggers TEMPLATE_FORBIDDEN_DECL instead of TEMPLATE_FORBIDDEN_BLOCK_HEADER             |
| CONFIG intra-references                       | Observation    | Requires bare names inside CONFIG block; CONFIG.name syntax rejected                    |
| KEYWORD_AS_IDENTIFIER                         | Observation    | Most spec-listed reserved identifiers (PLL, BIT, BUS, etc.) not reserved                |
| Multi-driver priority chain                   | Observation    | S11.4.1 priority chain not expressible; semantic rules block the pattern                |
| OE extraction                                 | Observation    | Only fails on concatenated z literals; IR check narrower than AST check                 |
| PROJECT_MISSING_ENDPROJ                       | Observation    | Diagnostic location points to @endmod, not @project                                     |
| Register names (block keywords)               | Observation    | Block-initiating keywords as register names cause cascading parse failures              |
| TRISTATE_TRANSFORM_OE_EXTRACT_FAIL            | Observation    | Reports File: <input> instead of actual filename                                        |
| TRISTATE_TRANSFORM_UNUSED_DEFAULT             | Observation    | Reports File: <input> instead of actual filename                                        |
| UNDECLARED_IDENTIFIER                         | Observation    | Spurious co-fire with CONST_NUMERIC_IN_STRING_CONTEXT for CONFIG in @file()             |
