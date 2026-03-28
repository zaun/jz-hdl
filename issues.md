# Compiler Test Issues

## test_1_1-identifiers.md

### KEYWORD_AS_IDENTIFIER not enforced in WIRE block declarations
- **Severity:** Bug
- **Description:** All keywords (REGISTER, WIRE, PORT, SELECT, CONFIG, CONST, MUX, etc.) are silently accepted as wire names in `WIRE { ... }` blocks without triggering KEYWORD_AS_IDENTIFIER. The check fires correctly in CONST block, module name, port name, and instance name contexts.
- **Reproduction:** `WIRE { REGISTER [1]; }` — accepted with no error.

### KEYWORD_AS_IDENTIFIER inconsistent in REGISTER block declarations
- **Severity:** Bug
- **Description:** In REGISTER blocks, some keywords trigger KEYWORD_AS_IDENTIFIER (e.g., CONFIG) while others trigger PARSE000 (e.g., SELECT, WIRE, PORT). The behavior should be consistent — all keywords should produce KEYWORD_AS_IDENTIFIER.
- **Reproduction:** `REGISTER { SELECT [1] = 1'b0; }` → PARSE000 instead of KEYWORD_AS_IDENTIFIER.

### ID_SINGLE_UNDERSCORE not enforced in WIRE block declarations
- **Severity:** Bug
- **Description:** Single underscore `_` is silently accepted as a wire name in `WIRE { _ [1]; }` without triggering ID_SINGLE_UNDERSCORE. The check fires correctly in REGISTER, CONST, PORT, module name, and instance name contexts.
- **Reproduction:** `WIRE { _ [1]; }` — accepted with only NET_DANGLING_UNUSED warning.

### ID_SYNTAX_INVALID not enforced in WIRE block declarations
- **Severity:** Bug
- **Description:** A 256-character identifier is silently accepted as a wire name in `WIRE { ... }` blocks without triggering ID_SYNTAX_INVALID. The check fires correctly in REGISTER, CONST, PORT, and module name contexts.
- **Reproduction:** `WIRE { <256-char-name> [1]; }` — accepted with only NET_DANGLING_UNUSED warning.

### Reserved identifiers (VCC, GND) produce PARSE000 instead of KEYWORD_AS_IDENTIFIER
- **Severity:** Bug
- **Description:** VCC and GND are listed as reserved identifiers in the spec (Section 1.1) but produce `PARSE000` ("expected wire name" / "expected const name") instead of KEYWORD_AS_IDENTIFIER when used as user-declared names in CONST/WIRE blocks.
- **Reproduction:** `CONST { VCC = 1; }` → PARSE000 instead of KEYWORD_AS_IDENTIFIER.

### Most reserved identifiers not enforced as keywords
- **Severity:** Gap
- **Description:** The spec lists PLL, DLL, CLKDIV, BIT, BUS, FIFO, HANDSHAKE, PULSE, MCP, RAW, BLOCK, DISTRIBUTED, ASYNC, SYNC, WRITE_FIRST, READ_FIRST, NO_CHANGE as reserved identifiers, but only IDX is enforced as a keyword. All others are accepted as regular identifiers in all contexts.
- **Reproduction:** `CONST { PLL = 1; }` — accepted with no error.

### PORT name with keyword triggers TOP_PORT_NOT_LISTED in top module
- **Severity:** Limitation
- **Description:** Using a keyword as a port name in the `@top` module triggers both KEYWORD_AS_IDENTIFIER and TOP_PORT_NOT_LISTED (since the @top block can't list the port by its keyword name). Testing this context for the @top module is not possible without unrelated diagnostics. Testing in non-top modules works cleanly.

## test_1_2-fundamental_terms.md

### NET_MULTIPLE_ACTIVE_DRIVERS does not fire for multiple unconditional instance drivers
- **Severity:** Bug
- **Description:** When two module instances bind their output ports to the same wire (both unconditionally driving), NET_MULTIPLE_ACTIVE_DRIVERS does not fire. The net driver analysis code in `driver_net.c` checks for `instance_driver_count > 1` and calls `sem_tristate_check_net()`, but the rule never fires for any tested scenario. Tested with: (1) two instances of different modules unconditionally driving the same wire, (2) two instances of the same module with tri-state pattern using identical guard constants, (3) with and without CHIP configuration.
- **Reproduction:** Two instances both with `OUT [1] out_val = shared;` where `shared` is a WIRE — no diagnostic produced.

### Test plan rule IDs do not match rules.c
- **Severity:** Documentation
- **Description:** The test plan (Section 6.1) uses rule IDs that differ from rules.c:
  - `ASSIGN_MULTI_DRIVER` → actual: `NET_MULTIPLE_ACTIVE_DRIVERS`
  - `WIRE_UNDRIVEN` → actual: `NET_FLOATING_WITH_SINK`
  - `TRISTATE_FLOATING_NET_READ` → actual: `NET_TRI_STATE_ALL_Z_READ` (but fires as `ASYNC_FLOATING_Z_READ` in practice)
  - `LIT_RESET_HAS_X` → actual: `REG_INIT_CONTAINS_X`
  - `X_OBSERVABLE_SINK` is listed as missing but exists as `OBS_X_TO_OBSERVABLE_SINK`

### DOMAIN_CONFLICT and MULTI_CLK_ASSIGN always co-fire
- **Severity:** Limitation
- **Description:** When a register is assigned in two SYNCHRONOUS blocks with different clocks, both DOMAIN_CONFLICT and MULTI_CLK_ASSIGN fire. It is not possible to trigger one without the other because the conditions are inherently coupled: assigning in the wrong domain (DOMAIN_CONFLICT) requires assigning in multiple domains (MULTI_CLK_ASSIGN). Tests for either rule necessarily include diagnostics from both.

### ASYNC_FLOATING_Z_READ fires instead of NET_TRI_STATE_ALL_Z_READ
- **Severity:** Observation
- **Description:** When all drivers of a wire assign `z` and the wire is read, ASYNC_FLOATING_Z_READ (from ASYNC_BLOCK_RULES) fires rather than NET_TRI_STATE_ALL_Z_READ (from NET_DRIVERS_AND_TRI_STATE). The async block check occurs earlier in the analysis pipeline and catches the issue first. NET_TRI_STATE_ALL_Z_READ may be dead code or only fire in scenarios not reproducible through the current test framework.

## test_1_3-bit_slicing_and_indexing.md

### Test plan rule IDs do not match rules.c
- **Severity:** Documentation
- **Description:** The test plan uses rule IDs that differ from rules.c:
  - `SLICE_OUT_OF_RANGE` → actual: `SLICE_INDEX_OUT_OF_RANGE`
  - `SLICE_MSB_LT_LSB` → actual: `SLICE_MSB_LESS_THAN_LSB`
  - `SEMANTIC_DRIVER_SLICED` → actual: `SPECIAL_DRIVER_SLICED`
  - `CONST_UNDEFINED` → actual: `CONST_UNDEFINED_IN_WIDTH_OR_SLICE` (for declared non-CONST names) or `UNDECLARED_IDENTIFIER` (for completely undefined names)

### SLICE_INDEX_INVALID cannot be triggered
- **Severity:** Observation
- **Description:** The rule SLICE_INDEX_INVALID ("S1.3/S8.1 Slice index is not an integer/CONST or CONST is undefined/negative") exists in rules.c but could not be triggered in any tested scenario. When an undefined name is used in a slice position, UNDECLARED_IDENTIFIER fires. When a declared non-CONST name (wire, register, port) is used, CONST_UNDEFINED_IN_WIDTH_OR_SLICE fires. Negative indices via literal `-1` are a parse error. Negative CONST values are caught by CONST_NEGATIVE_OR_NONINT. The specific condition that triggers SLICE_INDEX_INVALID is unknown — it may be dead code or reachable only through internal compiler paths not expressible in user code.

### Undefined names in slices fire UNDECLARED_IDENTIFIER not slice-specific rule
- **Severity:** Observation
- **Description:** When a completely undefined name (e.g., `UNDEF_MSB`) is used in a slice index like `bus[UNDEF_MSB:0]`, the compiler fires UNDECLARED_IDENTIFIER rather than SLICE_INDEX_INVALID or CONST_UNDEFINED_IN_WIDTH_OR_SLICE. The general identifier resolution runs before slice-specific validation. This means there is no slice-specific diagnostic for undefined names — the generic UNDECLARED_IDENTIFIER covers it.

## test_1_4-comments.md

No issues found. Both testable rules (COMMENT_IN_TOKEN, COMMENT_NESTED_BLOCK) fire correctly in all tested contexts.

## test_1_5-exclusive_assignment_rule.md

### Test plan rule IDs do not match rules.c
- **Severity:** Documentation
- **Description:** The test plan (Section 6.1) uses rule IDs that differ from rules.c:
  - `ASSIGN_MULTI_DRIVER` → actual: `ASSIGN_MULTIPLE_SAME_BITS`
  - `ASSIGN_SHADOW_OUTER` → actual: `ASSIGN_SHADOWING`
  - `ASSIGN_INDEPENDENT_CHAIN_CONFLICT` → actual: `ASSIGN_INDEPENDENT_IF_SELECT`
  - `ASSIGN_PARTIAL_COVERAGE` → actual: `ASYNC_UNDEFINED_PATH_NO_DRIVER`
  - `ASSIGN_UNREACHABLE_DEAD_WRITE` → no matching rule ID (closest is `WARN_DEAD_CODE_UNREACHABLE` but it covers different scenarios)

### Wire double-assignment cascades into NET_FLOATING_WITH_SINK
- **Severity:** Observation
- **Description:** When a wire is double-assigned at root of ASYNC (`w = VCC; w = GND;`), ASSIGN_MULTIPLE_SAME_BITS fires correctly on the second assignment. However, if the wire is subsequently read (`out_b = w;`), a cascading NET_FLOATING_WITH_SINK error fires on the reading port, suggesting the compiler invalidates the wire's driver status after the exclusive assignment violation. This prevents clean testing of wire double-assignment without unrelated diagnostics.
- **Reproduction:** `WIRE { w [1]; } ASYNCHRONOUS { w = VCC; w = GND; out_b = w; }` → ASSIGN_MULTIPLE_SAME_BITS on `w = GND` plus NET_FLOATING_WITH_SINK on `out_b`.

## test_1_6-high_impedance_and_tristate.md

### Test plan rule IDs do not match rules.c
- **Severity:** Documentation
- **Description:** The test plan (Section 6.1) uses rule IDs that differ from rules.c:
  - `LIT_HEX_HAS_XZ` → actual: `LIT_INVALID_DIGIT_FOR_BASE` (generic rule for invalid digits in any base)
  - `LIT_RESET_HAS_Z` → actual: `REG_INIT_CONTAINS_Z`
  - `TRISTATE_MULTIPLE_ACTIVE_DRIVERS` → actual: `NET_MULTIPLE_ACTIVE_DRIVERS` (but this rule does not fire; see test_1_2 issues)
  - `TRISTATE_FLOATING_NET_READ` → actual: `ASYNC_FLOATING_Z_READ` (already tested under test_1_2)

### No compiler error for z assigned to register next-state (REG_Z_ASSIGN missing)
- **Severity:** Gap
- **Description:** The spec (S1.6.6) states "Registers cannot store z" but the compiler does not produce an error when z is assigned as the next-state value to a register in a SYNCHRONOUS block (`r <= 8'bzzzz_zzzz;`). Only `WARN_INTERNAL_TRISTATE` (a warning about FPGA compatibility) is produced. There is no `REG_Z_ASSIGN` or equivalent rule ID in rules.c.
- **Reproduction:** `SYNCHRONOUS(CLK=clk) { r <= 8'bzzzz_zzzz; }` — only `WARN_INTERNAL_TRISTATE` warning, no error.

### PORT_TRISTATE_MISMATCH co-fires with WARN_INTERNAL_TRISTATE
- **Severity:** Limitation
- **Description:** When z is assigned to a non-INOUT port (triggering PORT_TRISTATE_MISMATCH), WARN_INTERNAL_TRISTATE always co-fires on the port declaration. This is inherent because any z usage on an internal signal triggers the FPGA-compatibility warning. Tests for PORT_TRISTATE_MISMATCH necessarily include WARN_INTERNAL_TRISTATE diagnostics in the .out file.

## test_2_3-bit_width_constraints.md

### Test plan rule IDs do not match rules.c
- **Severity:** Documentation
- **Description:** The test plan (Section 6.1) uses rule IDs that differ from rules.c:
  - `WIDTH_BINOP_MISMATCH` → actual: `TYPE_BINOP_WIDTH_MISMATCH`
  - `WIDTH_TERNARY_MISMATCH` → actual: `TERNARY_BRANCH_WIDTH_MISMATCH`
  - `WIDTH_ASSIGN_MISMATCH_NO_EXT` → exists but is suppressed by `ASSIGN_WIDTH_NO_MODIFIER` (see below)

### WIDTH_ASSIGN_MISMATCH_NO_EXT suppressed by ASSIGN_WIDTH_NO_MODIFIER priority
- **Severity:** Observation
- **Description:** The rule `WIDTH_ASSIGN_MISMATCH_NO_EXT` (priority 0) fires on every assignment width mismatch, but `ASSIGN_WIDTH_NO_MODIFIER` (priority 1) always co-fires on the same location. The diagnostic priority system (in `diagnostic.c`) only prints the highest-priority diagnostic per line, so `WIDTH_ASSIGN_MISMATCH_NO_EXT` never appears in compiler output. Both rules are emitted by `driver_assign.c:1371-1381` on the same `stmt->loc`. The test plan's intent to test `WIDTH_ASSIGN_MISMATCH_NO_EXT` is addressed by testing `ASSIGN_WIDTH_NO_MODIFIER` instead.

### Multiply/divide/modulus NOT exempt from strict width matching
- **Severity:** Documentation
- **Description:** The test plan (Section 6.2 "Rules Missing") suggests `WIDTH_MULTIPLY_RULES` as a missing rule for specialized width behavior of multiply/divide/modulus per S3.2. In practice, the compiler enforces equal operand widths for these operators via `TYPE_BINOP_WIDTH_MISMATCH`, the same as all other binary operators. There is no exemption for arithmetic operators.

## test_2_4-special_semantic_drivers.md

### Test plan rule IDs do not match rules.c
- **Severity:** Documentation
- **Description:** The test plan (Section 6.1) uses rule IDs that differ from rules.c:
  - `SEMANTIC_DRIVER_IN_EXPR` → actual: `SPECIAL_DRIVER_IN_EXPRESSION`
  - `SEMANTIC_DRIVER_IN_CONCAT` → actual: `SPECIAL_DRIVER_IN_CONCAT`
  - `SEMANTIC_DRIVER_SLICED` → actual: `SPECIAL_DRIVER_SLICED`
  - `SEMANTIC_DRIVER_IN_SLICE_INDEX` → actual: `SPECIAL_DRIVER_IN_INDEX`
  - `ASSIGN_MULTI_DRIVER` → actual: `NET_MULTIPLE_ACTIVE_DRIVERS` (doesn't fire; see test_1_2 issues)

### SPECIAL_DRIVER_IN_INDEX unreachable via user syntax
- **Severity:** Dead code
- **Description:** The rule SPECIAL_DRIVER_IN_INDEX exists in rules.c and has a semantic check in `driver_assign.c`, but the parser (`parser_expressions.c:1064-1074`) rejects GND/VCC tokens in slice/index positions with PARSE000 before the AST is built. The semantic check can never fire because the parser prevents the required AST structure from being created. This rule is dead code.
- **Reproduction:** `data[GND]` or `data[0:GND]` → PARSE000 instead of SPECIAL_DRIVER_IN_INDEX.

## test_3_1-operator_categories.md

### Test plan rule IDs do not match rules.c
- **Severity:** Documentation
- **Description:** The test plan (Section 6.1) uses `WIDTH_BINOP_MISMATCH` but the actual rule ID is `TYPE_BINOP_WIDTH_MISMATCH`. The "Rules Missing" section lists `LOGICAL_OP_MULTI_BIT` but the rule already exists as `LOGICAL_WIDTH_NOT_1`.

## test_2_2-signedness_model.md

No issues found. This section defines the unsigned-by-default signedness model. There are no rule IDs in rules.c specific to signedness enforcement. The two rules listed as missing in the test plan (SIGNED_KEYWORD_USED, SIGNED_OP_WITHOUT_INTRINSIC) do not exist and all test scenarios are behavioral/simulation-level, not lint-level.

## test_2_1-literals.md

### Test plan rule IDs do not match rules.c
- **Severity:** Documentation
- **Description:** The test plan (Section 6.1) uses rule IDs that differ from rules.c:
  - `LIT_WIDTH_ZERO` → actual: `LIT_WIDTH_NOT_POSITIVE`
  - `LIT_HEX_HAS_XZ` → actual: `LIT_INVALID_DIGIT_FOR_BASE` (generic rule for invalid digits in any base, including x/z in hex)
  - `LIT_RESET_HAS_X` → actual: `REG_INIT_CONTAINS_X` (in PORT_WIRE_REGISTER_DECLS category, not literals)
  - `LIT_RESET_HAS_Z` → actual: `REG_INIT_CONTAINS_Z` (in PORT_WIRE_REGISTER_DECLS category, not literals)

### LIT_UNDERSCORE_POSITION listed as missing but exists
- **Severity:** Documentation
- **Description:** The test plan (Section 6.2 "Rules Missing") lists `LIT_UNDERSCORE_POSITION` as a missing rule, but it exists in rules.c as `LIT_UNDERSCORE_AT_EDGES` with the message "S2.1 Literal has underscore as first or last character of value".

### LIT_OVERFLOW does not fire in register init context
- **Severity:** Bug
- **Description:** The LIT_OVERFLOW rule fires correctly in SYNCHRONOUS block expressions (e.g., `r <= 4'b10101;`) but does NOT fire when an overflowing literal is used as a register initialization value (e.g., `r [4] = 4'd16;`). The literal `4'd16` has intrinsic width 5 > declared width 4, which should trigger LIT_OVERFLOW, but the compiler silently accepts it.
- **Reproduction:** `REGISTER { r [4] = 4'd16; }` — no diagnostic produced. Compare with `r <= 4'd16;` in SYNCHRONOUS which correctly fires LIT_OVERFLOW.

### LIT_BARE_INTEGER suppressed by ASSIGN_WIDTH_NO_MODIFIER when widths mismatch
- **Severity:** Observation
- **Description:** When a bare integer's intrinsic width differs from the assignment target width, ASSIGN_WIDTH_NO_MODIFIER fires instead of LIT_BARE_INTEGER. LIT_BARE_INTEGER only fires when the bare integer's intrinsic width happens to match the target. For example, `data = 42;` (6-bit intrinsic, 8-bit target) fires ASSIGN_WIDTH_NO_MODIFIER, but `r <= 255;` (8-bit intrinsic, 8-bit register) fires LIT_BARE_INTEGER. Tests use matching-width bare integers to trigger LIT_BARE_INTEGER cleanly.

### ASYNC_ALIAS_LITERAL_RHS prevents testing literals with `=` in ASYNC blocks
- **Severity:** Limitation
- **Description:** Using a sized literal as the RHS of `=` in an ASYNCHRONOUS block triggers ASYNC_ALIAS_LITERAL_RHS ("Literal on RHS of `=` in ASYNCHRONOUS block"), preventing clean testing of other literal rules (LIT_OVERFLOW, LIT_UNDEFINED_CONST_WIDTH) in ASYNC `=` contexts. Tests use SYNCHRONOUS `<=` contexts instead. Note: bare integers do NOT trigger ASYNC_ALIAS_LITERAL_RHS.

## test_3_2-operator_definitions.md

### Test plan rule IDs do not match rules.c
- **Severity:** Documentation
- **Description:** The test plan (Section 6.1) uses rule IDs that differ from rules.c:
  - `UNARY_NOT_PARENTHESIZED` → actual: `UNARY_ARITH_MISSING_PARENS`
  - `EXPR_DIVISION_BY_ZERO` → actual: `DIV_CONST_ZERO`
  - `WIDTH_TERNARY_MISMATCH` → does not exist; closest is `TERNARY_BRANCH_WIDTH_MISMATCH` and `TERNARY_COND_WIDTH_NOT_1`

### Rules listed as missing in test plan but actually exist in rules.c
- **Severity:** Documentation
- **Description:** The test plan (Section 6.2 "Rules Missing") lists three rules as missing, but all exist:
  - `TERNARY_COND_NOT_1BIT` → exists as `TERNARY_COND_WIDTH_NOT_1` (tested)
  - `LOGICAL_OP_NOT_1BIT` → exists as `LOGICAL_WIDTH_NOT_1` (tested)
  - `CONCAT_EMPTY` → exists in rules.c (tested)

### DIV_CONST_ZERO and DIV_UNGUARDED_RUNTIME_ZERO cannot be tested in ASYNC blocks
- **Severity:** Limitation
- **Description:** Division by constant zero in ASYNC blocks triggers ASYNC_ALIAS_LITERAL_RHS (because the expression contains a literal `8'd0`). Division guarded by `IF` in ASYNC blocks triggers ASYNC_ALIAS_IN_CONDITIONAL. The DIV_UNGUARDED_RUNTIME_ZERO warning only fires from the IR pass when no errors exist, so it requires a completely error-free file. All division tests use SYNCHRONOUS blocks exclusively. This is not a coverage gap since the operator validation is context-independent — the same code path fires in both block types.

## test_3_3-operator_precedence.md

No issues found. This section defines the 15-level operator precedence hierarchy. There are no rule IDs in rules.c specific to operator precedence enforcement. The one rule listed as missing in the test plan (PRECEDENCE_AMBIGUOUS — warning for potentially confusing expressions without parentheses) does not exist in the compiler. All test scenarios are AST structure verification or parse-level errors, neither of which produce rule-ID diagnostics testable in the lint validation framework.

## test_3_4-operator_examples.md

No issues found. Section 3.4 provides canonical usage examples of operators defined in S3.1-S3.2. No new rules are introduced. The two rules referenced in the test plan use non-matching names:
- `UNARY_NOT_PARENTHESIZED` → actual: `UNARY_ARITH_MISSING_PARENS` (already tested)
- `WIDTH_TERNARY_MISMATCH` → actual: `TERNARY_BRANCH_WIDTH_MISMATCH` (already tested)

All other scenarios (unary negation, arithmetic vs logical shift, carry capture via concatenation, ternary+concat, tri-state driver patterns) are behavioral/simulation-level checks, not lint diagnostics.

## test_4_1-module_canonical_form.md

### Test plan rule IDs do not match rules.c
- **Severity:** Documentation
- **Description:** The test plan (Section 6.2 "Rules Missing") lists two rules as missing, but both exist:
  - `MODULE_NO_PORT` → exists as `MODULE_MISSING_PORT` (tested)
  - `MODULE_ONLY_IN_PORTS` → exists as `MODULE_PORT_IN_ONLY` (tested)

### MODULE_MISSING_PORT inherently co-fires with WARN_UNUSED_MODULE
- **Severity:** Limitation
- **Description:** A module with no PORT block (or empty PORT block) cannot be instantiated by any other module, so WARN_UNUSED_MODULE always co-fires alongside MODULE_MISSING_PORT. The test .out file includes both diagnostics.

### MODULE_PORT_IN_ONLY inherently co-fires with NET_DANGLING_UNUSED
- **Severity:** Limitation
- **Description:** A module with only IN ports has no output to sink input values through. The IN port declared in the module is flagged as NET_DANGLING_UNUSED because nothing reads it (no OUT port to drive). The test .out file includes both diagnostics.

### No rule for duplicate CONST/PORT/WIRE/REGISTER blocks
- **Severity:** Observation
- **Description:** The test plan (Neg 3) mentions "Duplicate CONST block" as a negative test case, but the parser (`parser_module.c`) accepts multiple CONST blocks without error — each is simply added as a child of the module AST node. There is no rule ID for detecting duplicate declaration blocks (CONST, PORT, WIRE, REGISTER). Only duplicate SYNCHRONOUS blocks for the same clock have a dedicated rule (DUPLICATE_BLOCK). The compiler merges multiple declaration blocks silently.

### Missing @endmod is a parse error without rule ID
- **Severity:** Observation
- **Description:** The test plan (Neg 1) lists "Missing @endmod" as a negative test, but the parser produces a generic parse error ("missing @endmod for module") without a rule ID. This cannot be tested in the validation framework which requires rule-ID diagnostics.

## test_4_2-scope_and_uniqueness.md

### Test plan rule IDs do not match rules.c
- **Severity:** Documentation
- **Description:** The test plan (Section 6.1) uses rule IDs that differ from rules.c:
  - `MODULE_DUPLICATE_NAME` → actual: `MODULE_NAME_DUP_IN_PROJECT`
  - `SCOPE_DUPLICATE_SIGNAL` → actual: `ID_DUP_IN_MODULE` (for declaration duplicates), `INSTANCE_NAME_DUP_IN_MODULE` (for instance name duplicates), `INSTANCE_NAME_CONFLICT` (for instance-signal name collision)
  - `MODULE_SELF_INSTANTIATION` → no rule ID exists in rules.c

### MODULE_SELF_INSTANTIATION rule does not exist
- **Severity:** Gap
- **Description:** The test plan (Neg 7) specifies that a module instantiating itself (`@module foo` containing `@new x foo {}`) should produce an error with rule ID MODULE_SELF_INSTANTIATION. No such rule exists in rules.c. Self-instantiation may be caught by other mechanisms (e.g., circular dependency or undefined module during forward reference resolution), but there is no dedicated rule for it.

### BLACKBOX_NAME_DUP_IN_PROJECT co-fires with WARN_UNUSED_MODULE
- **Severity:** Limitation
- **Description:** When a module and a blackbox share the same name, BLACKBOX_NAME_DUP_IN_PROJECT fires on the module definition, and WARN_UNUSED_MODULE also fires because the conflicting module cannot be instantiated. The test .out file includes both diagnostics.

### AMBIGUOUS_REFERENCE rule not implemented
- **Severity:** Gap
- **Description:** The rule `AMBIGUOUS_REFERENCE` is defined in rules.c but is not emitted by any semantic pass in `src/sem/`. No code path calls `sem_report_rule` with this rule ID. The rule cannot be tested until the compiler implements the check.

## test_4_3-const.md

### Test plan rule IDs do not match rules.c
- **Severity:** Documentation
- **Description:** The test plan (Section 6.1) uses rule IDs that differ from rules.c:
  - `CONST_DUPLICATE` → actual: `ID_DUP_IN_MODULE` (generic duplicate identifier rule, already tested in test_4_2)
  - `CONST_CIRCULAR_DEP` → exists in rules.c but never fires for module CONSTs (see below)
  - `CONST_FORWARD_REF` → no rule ID exists for module CONST forward references
  - `CONST_NEGATIVE_WIDTH` → actual: `CONST_NEGATIVE_OR_NONINT`

### CONST_CIRCULAR_DEP is dead code for module-level CONSTs
- **Severity:** Bug
- **Description:** The rule `CONST_CIRCULAR_DEP` exists in rules.c and has detection code in `const_eval.c:824` (fired when eval_one encounters an already-visiting CONST) and `const_eval.c:624` (fired during expression parsing via env_parser_diag with rule "CONST002"). However, for module-level CONST evaluation, `driver.c:1367-1371` intentionally suppresses all const_eval internal diagnostics by passing `opts.diagnostics = NULL`. Any evaluation failure (including circular dependencies) is then mapped to the generic `CONST_NEGATIVE_OR_NONINT` rule on each CONST declaration that failed to evaluate. This means circular CONST dependencies produce multiple `CONST_NEGATIVE_OR_NONINT` errors instead of the more specific `CONST_CIRCULAR_DEP`.
- **Reproduction:** `CONST { CA = CB; CB = CA; }` → two CONST_NEGATIVE_OR_NONINT errors, not CONST_CIRCULAR_DEP.

### CONST_FORWARD_REF does not exist for module CONSTs
- **Severity:** Gap
- **Description:** The test plan (Neg 3) specifies that `A = B; B = 5;` should produce a forward reference error. No `CONST_FORWARD_REF` rule exists in rules.c — only `CONFIG_FORWARD_REF` (for project CONFIG) and `GLOBAL_FORWARD_REF` (for @global blocks). Module-level CONST forward references (`A = B; B = 5;`) produce `CONST_NEGATIVE_OR_NONINT` on `A` because the const_eval engine evaluates in declared order and `B` is not yet available when `A` is evaluated.

### CONST_STRING_IN_NUMERIC_CONTEXT on wire width co-fires with NET_DANGLING_UNUSED
- **Severity:** Limitation
- **Description:** When a string CONST is used as a wire width (`WIRE { w [MY_STR]; }`), the width evaluates to 0 or invalid, causing the wire to be flagged as NET_DANGLING_UNUSED in addition to CONST_STRING_IN_NUMERIC_CONTEXT. This is inherent — a wire with invalid width is always unused.

### CONST_UNDEFINED_IN_WIDTH_OR_SLICE does not fire for wire widths or instance port widths
- **Severity:** Observation
- **Description:** When an undefined CONST is used as a wire width (`WIRE { w [UNDEF]; }`), the compiler does not fire CONST_UNDEFINED_IN_WIDTH_OR_SLICE. Instead it fires NET_DANGLING_UNUSED. When an undefined CONST is used in an instance port width (`IN [BAD_W] din = r;`), the compiler fires INSTANCE_PORT_WIDTH_EXPR_INVALID instead of CONST_UNDEFINED_IN_WIDTH_OR_SLICE. The rule fires correctly for port widths and register widths.

## test_4_4-port.md

### BUS_SIGNAL_UNDEFINED fires ASYNC_INVALID_STATEMENT_TARGET for scalar BUS write
- **Severity:** Bug
- **Description:** Writing to a nonexistent signal on a scalar BUS port (e.g., `sbus.nonexistent <= r`) fires ASYNC_INVALID_STATEMENT_TARGET ("LHS in ASYNCHRONOUS assignment is not assignable") instead of BUS_SIGNAL_UNDEFINED. The same operation on an arrayed BUS port (e.g., `abus[0].missing <= r`) correctly fires BUS_SIGNAL_UNDEFINED. Read-context accesses (RHS) correctly fire BUS_SIGNAL_UNDEFINED for both scalar and arrayed ports.
- **Reproduction:** `BUS MY_BUS SOURCE sbus;` then `sbus.nonexistent <= r;` → ASYNC_INVALID_STATEMENT_TARGET instead of BUS_SIGNAL_UNDEFINED.

### COMB_LOOP_UNCONDITIONAL co-fires with ASYNC_ALIAS_IN_CONDITIONAL in SELECT and nested IF
- **Severity:** Limitation
- **Description:** When an alias (`=`) appears inside a SELECT case or nested IF, ASYNC_ALIAS_IN_CONDITIONAL fires correctly. However, COMB_LOOP_UNCONDITIONAL also co-fires on the ASYNCHRONOUS block when the same target has `<=` (receive-assign) on other branches. This occurs because the alias is still processed (creating a net merge) despite the error, and the combination of alias on one path and receive-assign on another creates what the compiler interprets as a combinational loop. Single-level IF (non-nested) with alias on one branch and receive on the other does NOT produce COMB_LOOP.
- **Reproduction:** `SELECT (ctrl) { CASE (1'b0) { data = din; } CASE (1'b1) { data <= din; } DEFAULT { data <= din; } }` → ASYNC_ALIAS_IN_CONDITIONAL on CASE(0) plus COMB_LOOP_UNCONDITIONAL on the ASYNCHRONOUS block.

### Test plan rule IDs do not match rules.c
- **Severity:** Documentation
- **Description:** The test plan (Section 6.1) uses rule IDs that differ from rules.c:
  - `PORT_INPUT_DRIVEN_INSIDE` → actual: `PORT_DIRECTION_MISMATCH_IN`
  - `PORT_DIRECTION_VIOLATION` → actual: `PORT_DIRECTION_MISMATCH_OUT`
  - `BUS_PORT_DIRECTION_VIOLATION` → actual: `BUS_SIGNAL_READ_FROM_WRITABLE` and `BUS_SIGNAL_WRITE_TO_READABLE`
  - `BUS_INDEX_OUT_OF_RANGE` → actual: `BUS_PORT_INDEX_OUT_OF_RANGE`
  - `ALIAS_IN_CONDITIONAL` → actual: `ASYNC_ALIAS_IN_CONDITIONAL`
  - `BUS_SIGNAL_NOT_FOUND` is listed as missing but exists as `BUS_SIGNAL_UNDEFINED`

### BUS_PORT_UNKNOWN_BUS never fires
- **Severity:** Bug
- **Description:** The rule `BUS_PORT_UNKNOWN_BUS` exists in rules.c and has check code in `driver_width.c:768-773` that looks up the BUS name in project symbols. However, the check never fires in practice. Declaring a BUS port referencing a non-existent BUS (e.g., `BUS FAKE_BUS SOURCE fbus;` where `FAKE_BUS` is not in the project) produces only `NET_DANGLING_UNUSED` on the port. The code path at driver_width.c:742 (`decl->block_kind == "BUS"`) may not be reached, possibly because the parser or earlier pass does not set `block_kind` to "BUS" for unknown bus names.
- **Reproduction:** `BUS FAKE_BUS SOURCE fbus;` in PORT block where FAKE_BUS is not defined → NET_DANGLING_UNUSED only.

### BUS_PORT_ARRAY_COUNT_INVALID never fires
- **Severity:** Bug
- **Description:** The rule `BUS_PORT_ARRAY_COUNT_INVALID` exists in rules.c and has check code in `driver_width.c:786-797` that evaluates array count expressions. However, the check never fires. Using `[0]` as array count or referencing an undefined constant (`[UNDEF]`) both produce only `NET_DANGLING_UNUSED`. The code path at driver_width.c:742 may not be reached for these ports (same root cause as BUS_PORT_UNKNOWN_BUS).
- **Reproduction:** `BUS MY_BUS SOURCE [UNDEF] abus;` or `BUS MY_BUS SOURCE [0] abus;` → NET_DANGLING_UNUSED only.

### BUS_PORT_NOT_BUS fires as UNDECLARED_IDENTIFIER
- **Severity:** Bug
- **Description:** The rule `BUS_PORT_NOT_BUS` exists in rules.c and has check code in `driver.c:527-537`. However, dot-access on non-BUS ports (e.g., `din.tx` where `din` is a regular IN port) produces `UNDECLARED_IDENTIFIER` instead of `BUS_PORT_NOT_BUS`. The parser creates `QualifiedIdentifier` AST nodes for `name.member` syntax. When `sem_resolve_bus_access` processes these via the QualifiedIdentifier path (driver.c:488-506), the `module_scope_lookup_kind(mod_scope, "din", JZ_SYM_PORT)` call at line 512 returns NULL even though the port is in scope. This causes UNDECLARED_IDENTIFIER (line 514-523) to fire before reaching the BUS_PORT_NOT_BUS check (line 527-537).
- **Reproduction:** `IN [8] din;` then `dout <= din.tx;` → UNDECLARED_IDENTIFIER instead of BUS_PORT_NOT_BUS.

## test_4_5-wire.md

### WARN_UNUSED_WIRE is dead code
- **Severity:** Bug
- **Description:** The rule `WARN_UNUSED_WIRE` ("S12.3 WIRE declared but never driven or read; remove it if unused") exists in rules.c but is never emitted by any semantic analysis code. No `sem_report_rule` call references this rule ID. Completely unused wires (neither driven nor read) are caught by `NET_DANGLING_UNUSED` ("S5.1/S8.3 Signal is neither driven nor read; remove it or connect it") from the net driver analysis pass instead. WARN_UNUSED_WIRE appears to be dead code that was superseded by the more general NET_DANGLING_UNUSED check.
- **Reproduction:** `WIRE { w_unused [8]; }` with no references → fires NET_DANGLING_UNUSED, not WARN_UNUSED_WIRE.

### Test plan rule IDs do not match rules.c
- **Severity:** Documentation
- **Description:** The test plan (Section 6.1) uses rule IDs that differ from rules.c:
  - `ASSIGN_OP_WRONG_BLOCK` → actual: `WRITE_WIRE_IN_SYNC`
  - `ASSIGN_MULTI_DRIVER` → actual: `NET_MULTIPLE_ACTIVE_DRIVERS` (but doesn't fire; see test_1_2 issues)

## test_4_6-mux.md

### MUX_SELECTOR_OUT_OF_RANGE_CONST does not handle sized literals
- **Severity:** Bug
- **Description:** The `MUX_SELECTOR_OUT_OF_RANGE_CONST` check in `driver.c:2290-2295` uses `parse_simple_nonnegative_int()` to extract the selector value. This function only handles plain decimal digit strings, not sized literals like `2'd3` or `8'd10`. When a sized literal is used as a MUX selector (e.g., `mux[2'd3]`), the function fails to parse the text and the out-of-range check is silently skipped. The rule fires correctly with bare integer selectors (e.g., `mux[3]`).
- **Reproduction:** `MUX { m = a, b; } ASYNCHRONOUS { data = m[2'd2]; }` — no diagnostic produced. Compare with `data = m[2];` which correctly fires MUX_SELECTOR_OUT_OF_RANGE_CONST.

### Test plan rule IDs do not match rules.c
- **Severity:** Documentation
- **Description:** The test plan (Section 6.1) uses rule IDs that differ from rules.c:
  - `MUX_ASSIGN_READONLY` → actual: `MUX_ASSIGN_LHS`
  - `MUX_WIDTH_MISMATCH` → actual: `MUX_AGG_SOURCE_WIDTH_MISMATCH`
  - `MUX_SLICE_NOT_DIVISIBLE` → actual: `MUX_SLICE_WIDTH_NOT_DIVISOR`
  - `MUX_INDEX_OUT_OF_RANGE` → actual: `MUX_SELECTOR_OUT_OF_RANGE_CONST`
  - `SCOPE_DUPLICATE_SIGNAL` → actual: `MUX_NAME_DUPLICATE`

## test_4_7-register.md

### REG_MULTI_DIMENSIONAL is dead code (parser catches first)
- **Severity:** Dead code
- **Description:** The rule `REG_MULTI_DIMENSIONAL` ("S4.7 REGISTER declared with multi-dimensional syntax") exists in rules.c but the parser rejects multi-dimensional register syntax with PARSE000 ("expected '=' and initialization literal in REGISTER block") before semantic analysis runs. The second `[` token after the width is not expected by the parser. The semantic rule can never fire.
- **Reproduction:** `REGISTER { r [8] [4] = 8'h00; }` → PARSE000 instead of REG_MULTI_DIMENSIONAL.

### REG_MISSING_INIT_LITERAL is dead code (parser catches first)
- **Severity:** Dead code
- **Description:** The rule `REG_MISSING_INIT_LITERAL` ("S4.7 Register declared without mandatory reset/power-on literal") exists in rules.c but the parser rejects register declarations without init values with PARSE000 ("expected '=' and initialization literal in REGISTER block") before semantic analysis runs. The `;` token after the width is not expected by the parser.
- **Reproduction:** `REGISTER { r [8]; }` → PARSE000 instead of REG_MISSING_INIT_LITERAL.

### Test plan rule IDs do not match rules.c
- **Severity:** Documentation
- **Description:** The test plan (Section 6.1) uses rule IDs that differ from rules.c:
  - `ASSIGN_OP_WRONG_BLOCK` → actual: `ASYNC_ASSIGN_REGISTER`
  - `LIT_RESET_HAS_X` → actual: `REG_INIT_CONTAINS_X`
  - `LIT_RESET_HAS_Z` → actual: `REG_INIT_CONTAINS_Z`
  - `REG_MISSING_RESET` → actual: `REG_MISSING_INIT_LITERAL` (exists but dead code)
  - `REG_MULTI_DIMENSIONAL` → exists but dead code (parser catches first)

## test_4_8-latches.md

### LATCH_SR_WIDTH_MISMATCH not implemented
- **Severity:** Gap
- **Description:** The rule `LATCH_SR_WIDTH_MISMATCH` ("S4.8 SR latch set/reset expression width does not match latch width") exists in rules.c but has no corresponding `sem_report_rule` call in any semantic analysis file. The `driver_assign.c:1174-1180` code has a placeholder comment "SR-specific validation can be added here if needed" with `(void)is_sr_latch;`. SR latches with mismatched set/reset widths are not caught.

### LATCH_IN_CONST_CONTEXT not implemented
- **Severity:** Gap
- **Description:** The rule `LATCH_IN_CONST_CONTEXT` ("S4.8 LATCH identifier may not be used in compile-time constant contexts (@check/@feature conditions)") exists in rules.c but has no corresponding `sem_report_rule` call in any semantic analysis file. Using a latch identifier where a compile-time constant is required is not enforced.

### LATCH_WIDTH_INVALID suppressed by WIDTH_NONPOSITIVE_OR_NONINT priority
- **Severity:** Observation
- **Description:** The rule `LATCH_WIDTH_INVALID` (priority 0) fires in `driver_width.c:887-898` but `WIDTH_NONPOSITIVE_OR_NONINT` (priority 1) fires on the same declaration at `driver_width.c:910-916`. The priority system suppresses the lower-priority rule. `LATCH_WIDTH_INVALID` never appears in compiler output, similar to `WIDTH_ASSIGN_MISMATCH_NO_EXT`.

### LATCH_INVALID_TYPE parser aborts module on first error
- **Severity:** Limitation
- **Description:** The parser (`parser_register.c:235-248`) checks latch type validity and calls `return -1` on the first invalid type, aborting the entire LATCH block and module. This means only one LATCH_INVALID_TYPE error can be produced per file, and no subsequent declarations in the same module are parsed. Multi-trigger testing across modules is not possible because the module containing the first invalid type is incomplete.

### LATCH_ALIAS_FORBIDDEN with latch on LHS co-fires LATCH_ASSIGN_NON_GUARDED
- **Severity:** Limitation
- **Description:** When a latch is on the LHS of an alias (`=`) in ASYNCHRONOUS block, both LATCH_ALIAS_FORBIDDEN and LATCH_ASSIGN_NON_GUARDED fire. This is inherent — the alias is both a forbidden alias relationship AND a non-guarded latch write. Tests for LATCH_ALIAS_FORBIDDEN use latch on the RHS of alias only to avoid this co-fire.

### Test plan rule IDs do not match rules.c
- **Severity:** Documentation
- **Description:** The test plan (Section 6.1) uses rule IDs that differ from rules.c:
  - `LATCH_IN_SYNC` → actual: `LATCH_ASSIGN_IN_SYNC`
  - `LATCH_ENABLE_WIDTH` → actual: `LATCH_ENABLE_WIDTH_NOT_1`
  - `LATCH_IN_CDC` → actual: `LATCH_AS_CLOCK_OR_CDC`
  - `LATCH_AS_CLOCK` → listed as missing but exists as `LATCH_AS_CLOCK_OR_CDC`
  - `LATCH_TYPE_INVALID` → listed as missing but exists as `LATCH_INVALID_TYPE`
  - `LATCH_SR_WIDTH_MISMATCH` → listed as missing, and indeed not implemented in semantic analysis

## test_4_9-mem_block.md

### Test plan rule IDs do not match rules.c
- **Severity:** Documentation
- **Description:** The test plan (Section 6.1) uses `MEM_DECL_TYPE_INVALID` but the actual rule ID in rules.c is `MEM_TYPE_INVALID`.

## test_4_10-asynchronous_block.md

### Test plan rule IDs do not match rules.c
- **Severity:** Documentation
- **Description:** The test plan (Section 6.1) uses rule IDs that differ from rules.c:
  - `ALIAS_IN_CONDITIONAL` → actual: `ASYNC_ALIAS_IN_CONDITIONAL` (already tested in test_4_4)
  - `ALIAS_LITERAL_BAN` → actual: `ASYNC_ALIAS_LITERAL_RHS` (tested)
  - `ASSIGN_OP_WRONG_BLOCK` → actual: `ASYNC_ASSIGN_REGISTER` (already tested in test_4_7)
  - `PORT_INPUT_DRIVEN_INSIDE` → actual: `PORT_DIRECTION_MISMATCH_IN` (already tested in test_4_4)
  - `PORT_DIRECTION_VIOLATION` → actual: `PORT_DIRECTION_MISMATCH_OUT` (already tested in test_4_4)

### WIDTH_ASSIGN_MISMATCH_NO_EXT suppressed by priority (already documented)
- **Severity:** Observation
- **Description:** WIDTH_ASSIGN_MISMATCH_NO_EXT (priority 0) is always suppressed by ASSIGN_WIDTH_NO_MODIFIER (priority 1) on the same line. This is already documented in test_2_3 issues. The test plan's "width mismatch without modifier" scenario is covered by ASSIGN_WIDTH_NO_MODIFIER tests.

### ALIAS_TRUNCATION rule does not exist
- **Severity:** Gap
- **Description:** The test plan (Section 6.2 "Rules Missing") lists `ALIAS_TRUNCATION` as a potentially missing rule for "truncation is never implicit" (S4.10). No such rule exists in rules.c. Width mismatch in the truncation direction is covered by `ASSIGN_WIDTH_NO_MODIFIER` / `ASSIGN_TRUNCATES`, not a separate alias-specific truncation rule.

### CONST assignment in ASYNC block not caught
- **Severity:** Bug
- **Description:** Assigning to a CONST identifier in ASYNCHRONOUS blocks produces no error. For example, `K <= din;` or `K = din;` where `K` is declared in CONST {} compiles silently. The `sem_check_lvalue_targets_recursive` function in `driver_assign.c` handles `JZ_AST_EXPR_IDENTIFIER` by checking for MUX, IN port, and REGISTER, but does not check for `JZ_SYM_CONST`. The ASYNC_INVALID_STATEMENT_TARGET rule should fire for CONST identifiers on the LHS but the code path breaks without emitting a diagnostic.
- **Location:** `compiler/src/sem/driver_assign.c`, `JZ_AST_EXPR_IDENTIFIER` case (around line 542-621)
- **Reproduction:** `CONST { K = 5; } ... ASYNCHRONOUS { K <= din; }` — no diagnostic produced.

## test_4_11-synchronous_block.md

### Test plan rule IDs do not match rules.c
- **Severity:** Documentation
- **Description:** The test plan (Section 6.1) uses rule IDs that differ from rules.c:
  - `SYNC_DUPLICATE_BLOCK` → actual: `DUPLICATE_BLOCK`
  - `SYNC_DOMAIN_CONFLICT` → actual: `DOMAIN_CONFLICT`
  - `SYNC_MULTI_CLK_ASSIGN` → actual: `MULTI_CLK_ASSIGN`
  - `SYNC_BOTH_EDGE_WARNING` → actual: `SYNC_EDGE_BOTH_WARNING`
  - `ASSIGN_OP_WRONG_BLOCK` → actual: `WRITE_WIRE_IN_SYNC`

### Rules listed as missing in test plan but actually exist in rules.c
- **Severity:** Documentation
- **Description:** The test plan (Section 6.2 "Rules Missing") lists three rules as missing, but all exist:
  - `SYNC_CLK_NOT_1BIT` → exists as `SYNC_CLK_WIDTH_NOT_1` (tested)
  - `SYNC_RESET_NOT_1BIT` → exists as `SYNC_RESET_WIDTH_NOT_1` (tested)
  - `SYNC_MISSING_CLK` → exists in rules.c (tested)

## test_4_12-cdc_block.md

### CDC_DEST_ALIAS_ASSIGNED not implemented (dead code)
- **Severity:** Gap
- **Description:** The rule `CDC_DEST_ALIAS_ASSIGNED` ("S4.12 CDC destination alias may not be assigned directly in block statements") exists in rules.c but `sem_report_rule` is never called with this rule ID. The code in `driver_clocks.c:491-511` has a comment and loop structure for scanning blocks for assignments to CDC alias names, but the actual diagnostic emission is missing — the inner block only has a comment "The check is conservative... emit the diagnostic as a best-effort alert" with no `sem_report_rule` call. Assigning to a CDC dest alias is not caught.
- **Reproduction:** CDC entry `BIT src (clk_a) => dest (clk_b);` then `SYNCHRONOUS(CLK=clk_b) { dest <= 1'b0; }` — no diagnostic produced.

### CDC_SOURCE_NOT_PLAIN_REG unreachable via user syntax (dead code)
- **Severity:** Dead code
- **Description:** The rule `CDC_SOURCE_NOT_PLAIN_REG` ("S4.12 CDC source must be a plain register identifier, not a slice or expression") exists in rules.c and has semantic checks in `driver_clocks.c:440-462` for `JZ_AST_EXPR_BINARY` and `JZ_AST_EXPR_CONCAT` source nodes. However, the parser (`parser_cdc.c:96-106`) only accepts a single identifier token (optionally followed by a bit-select `[index]`) in the CDC source position. There is no expression parsing path that can produce binary or concat AST nodes. The semantic check is dead code.
- **Reproduction:** No user syntax can produce the required AST structure. `BIT (a + b) (clk_a) => dest (clk_b);` is a parse error, not a CDC_SOURCE_NOT_PLAIN_REG diagnostic.

### Test plan rule IDs do not match rules.c
- **Severity:** Documentation
- **Description:** The test plan (Section 6.1) uses rule IDs that differ from rules.c:
  - `CDC_BIT_WIDTH` → actual: `CDC_BIT_WIDTH_NOT_1`
  - `CDC_RAW_NO_STAGES` → actual: `CDC_RAW_STAGES_FORBIDDEN`
  - `CDC_ALIAS_READONLY` → actual: `CDC_DEST_ALIAS_ASSIGNED` (dead code)
  - `CDC_DOMAIN_CONFLICT` → actual: `DOMAIN_CONFLICT` (already tested in test_1_2)
  - `CDC_SOURCE_NOT_PLAIN` → actual: `CDC_SOURCE_NOT_PLAIN_REG` (dead code)

### CDC_DEST_ALIAS_DUP inherently co-fires with ID_DUP_IN_MODULE
- **Severity:** Limitation
- **Description:** When a CDC destination alias name conflicts with an existing identifier in the module (register, port, const), both `CDC_DEST_ALIAS_DUP` and `ID_DUP_IN_MODULE` fire at the same location. This is inherent because CDC alias names are registered in the module scope and checked by both the CDC-specific check and the general scope uniqueness check.

## test_4_13-module_instantiation.md

### INSTANCE_PORT_WIDTH_MISMATCH does not fire when child port uses CONST width
- **Severity:** Bug
- **Description:** The `INSTANCE_PORT_WIDTH_MISMATCH` rule fires correctly when the child module declares port widths with literal integers (e.g., `IN [8] din`), but does NOT fire when the child module uses CONST expressions for port widths (e.g., `CONST { WIDTH = 8; } PORT { IN [WIDTH] din; }`). The `eval_simple_positive_decl_int()` function in `driver_instance.c:435` cannot resolve the child's CONST-based width expression, causing the comparison to be skipped entirely.
- **Reproduction:** Child module `CONST { WIDTH = 8; } PORT { IN [WIDTH] din; }`, parent `@new inst ChildMod { IN [4] din = _; }` — no diagnostic produced. Compare with child `PORT { IN [8] din; }` which correctly fires the rule.

### INSTANCE_ARRAY_COUNT_INVALID rejects valid CONST expressions in array count
- **Severity:** Bug
- **Description:** The spec (S4.13.1) says array count must be "a positive integer literal or a CONST expression", but the compiler rejects all CONST expressions in the array count `[N]` position. `@new inst[VALID_COUNT] mod { ... }` where `VALID_COUNT = 4` fires INSTANCE_ARRAY_COUNT_INVALID. The parser/semantic check only accepts literal integers, not CONST identifiers.
- **Reproduction:** `CONST { N = 4; } @new inst[N] ChildMod { ... }` → INSTANCE_ARRAY_COUNT_INVALID instead of valid compilation.

### INSTANCE_ARRAY_MULTI_DIMENSIONAL is dead code (parser catches first)
- **Severity:** Dead code
- **Description:** The rule `INSTANCE_ARRAY_MULTI_DIMENSIONAL` ("S4.13.1 Multi-dimensional instance arrays are not supported") exists in rules.c but the parser rejects `@new name[2][3] mod { ... }` with PARSE000 ("expected module or blackbox name after instance name") before semantic analysis runs. The second `[` after the first array count is interpreted as the start of the module name position. The semantic rule can never fire.
- **Reproduction:** `@new inst[2][3] ChildMod { ... }` → PARSE000 instead of INSTANCE_ARRAY_MULTI_DIMENSIONAL.

### Test plan rule IDs do not match rules.c
- **Severity:** Documentation
- **Description:** The test plan (Section 6.1) uses rule IDs that differ from rules.c:
  - `INSTANCE_IDX_IN_OVERRIDE` → actual: `INSTANCE_ARRAY_IDX_INVALID_CONTEXT`
  - `INSTANCE_OVERLAP` / `INSTANCE_ARRAY_OVERLAP` → actual: `INSTANCE_ARRAY_PARENT_BIT_OVERLAP`
  - `MODULE_SELF_INSTANTIATION` → no rule ID exists in rules.c (already documented in test_4_2 issues)
  - `SCOPE_DUPLICATE_SIGNAL` → actual: `INSTANCE_NAME_DUP_IN_MODULE` or `INSTANCE_NAME_CONFLICT`

## test_4_14-feature_guards.md

### FEATURE_VALIDATION_BOTH_PATHS is dead code
- **Severity:** Gap
- **Description:** The rule `FEATURE_VALIDATION_BOTH_PATHS` ("S4.14 Both branches of @feature guard must pass full semantic validation") exists in rules.c but `sem_report_rule` is never called with this rule ID anywhere in the codebase. The spec requires that both enabled and disabled configurations pass semantic validation, but the compiler does not enforce this. A @feature guard that declares a register in the THEN branch without an @else will not produce an error even though the disabled configuration has an undriven register.

### Feature guard condition validation only runs in ASYNC/SYNC blocks
- **Severity:** Bug
- **Description:** The semantic checks for @feature conditions (FEATURE_COND_WIDTH_NOT_1 and FEATURE_EXPR_INVALID_CONTEXT) are only performed in ASYNCHRONOUS and SYNCHRONOUS blocks. The check function `sem_check_feature_guard_cond` is called from `sem_check_block_expressions_inner` (driver_expr.c:810), which is only invoked for blocks where `is_async || is_sync` (driver_expr.c:1279-1283). @feature guards in CONST, PORT, WIRE, REGISTER, LATCH, MEM, MUX, and module-scope blocks do not have their conditions validated for width or reference restrictions.
- **Reproduction:** `CONST { @feature some_wire == 1'b1 / VALUE = 42; / @endfeat }` where `some_wire` is a WIRE — no FEATURE_EXPR_INVALID_CONTEXT diagnostic produced.

### FEATURE_NESTED not enforced in declaration blocks
- **Severity:** Bug
- **Description:** The parser only checks for nested @feature in ASYNC/SYNC statement blocks (`parse_feature_body_stmts` in parser_statements.c:683-691). In declaration blocks (CONST, PORT, WIRE, REGISTER, LATCH, MEM, MUX) and module-scope, the parser uses `parse_feature_guard_in_block` which recursively calls the block body parser for the feature body. When the body parser encounters another `@feature`, it calls `parse_feature_guard_in_block` again — this succeeds, creating nested FEATURE_GUARD AST nodes. The semantic analysis does not check for nesting in these block types (the FEATURE_NESTED check in `sem_check_block_expressions_inner` only runs for ASYNC/SYNC blocks).
- **Reproduction:** `REGISTER { @feature A == 1 / @feature B == 1 / r [1] = 1'b0; / @endfeat / @endfeat }` — no FEATURE_NESTED diagnostic produced.

### Test plan rule IDs do not match rules.c
- **Severity:** Documentation
- **Description:** The test plan (Section 6.1) uses rule IDs that differ from rules.c:
  - `FEATURE_RUNTIME_REF` → actual: `FEATURE_EXPR_INVALID_CONTEXT`
  - `PORT_OUTPUT_NOT_DRIVEN` → no matching rule in rules.c for this context
  - `FEATURE_EXPR_NOT_BOOLEAN` → actual: `FEATURE_COND_WIDTH_NOT_1`
  - `FEATURE_BOTH_CONFIG_INVALID` → actual: `FEATURE_VALIDATION_BOTH_PATHS` (dead code)

## test_5_0-assignment_operators_summary.md

### Test plan rule IDs do not match rules.c
- **Severity:** Documentation
- **Description:** The test plan (Section 6.2 "Rules Missing") lists `ASSIGN_TRUNCATION` as a missing rule, but the actual rule `ASSIGN_TRUNCATES` exists in rules.c and fires correctly for truncation attempts with extension modifiers.

### Test plan references suppressed rule
- **Severity:** Documentation
- **Description:** The test plan (Section 4, Input/Output Matrix row 1) expects `WIDTH_ASSIGN_MISMATCH_NO_EXT` for `wide = narrow;` (no modifier). This rule exists but is always suppressed by `ASSIGN_WIDTH_NO_MODIFIER` (priority 1 vs priority 0). The same scenario produces `ASSIGN_WIDTH_NO_MODIFIER` instead. Already documented in test_2_3 issues.

## test_5_1-asynchronous_assignments.md

### Test plan rule IDs do not match rules.c
- **Severity:** Documentation
- **Description:** The test plan (Section 6.1) uses rule IDs that differ from rules.c:
  - `ALIAS_LITERAL_BAN` → actual: `ASYNC_ALIAS_LITERAL_RHS`
  - `ASSIGN_OP_WRONG_BLOCK` → actual: `ASYNC_ASSIGN_REGISTER`
  - `ASSIGN_PARTIAL_COVERAGE` → actual: `ASYNC_UNDEFINED_PATH_NO_DRIVER`
  - `ASSIGN_MULTI_DRIVER` → actual: `ASSIGN_MULTIPLE_SAME_BITS` or `NET_MULTIPLE_ACTIVE_DRIVERS`

### All rules already tested in earlier sections
- **Severity:** Observation
- **Description:** All four rules in this test plan were already tested in earlier test plan sections: ASYNC_ALIAS_LITERAL_RHS (test_4_10), ASYNC_ASSIGN_REGISTER (test_4_7), ASYNC_UNDEFINED_PATH_NO_DRIVER (test_1_5), ASSIGN_MULTIPLE_SAME_BITS (test_1_5). No new test files were needed.

## test_5_2-synchronous_assignments.md

### SYNC_SLICE_WIDTH_MISMATCH suppressed by ASSIGN_SLICE_WIDTH_MISMATCH priority
- **Severity:** Observation
- **Description:** The rule `SYNC_SLICE_WIDTH_MISMATCH` (priority 0) fires in `driver_assign.c:1458` but `ASSIGN_SLICE_WIDTH_MISMATCH` (priority 2) always co-fires on the same location. The diagnostic priority system only prints the highest-priority diagnostic per line, so `SYNC_SLICE_WIDTH_MISMATCH` never appears in compiler output. This is the same pattern as `WIDTH_ASSIGN_MISMATCH_NO_EXT` (documented in test_2_3).

### Test plan rule IDs do not match rules.c
- **Severity:** Documentation
- **Description:** The test plan (Section 6.1) uses rule IDs that differ from rules.c:
  - `ASSIGN_OP_WRONG_BLOCK` → actual: `SYNC_NO_ALIAS` (alias `=` in SYNC), `WRITE_WIRE_IN_SYNC` (wire in SYNC), or `ASSIGN_TO_NON_REGISTER_IN_SYNC` (non-register in SYNC)
  - `ASSIGN_SHADOW_OUTER` → actual: `SYNC_ROOT_AND_CONDITIONAL_ASSIGN`
  - `ASSIGN_MULTI_DRIVER` → actual: `SYNC_MULTI_ASSIGN_SAME_REG_BITS`
  - `WIDTH_ASSIGN_MISMATCH_NO_EXT` → suppressed by `ASSIGN_WIDTH_NO_MODIFIER` (see test_2_3 issues)
  - `SYNC_WIRE_ASSIGN` (Section 6.2 "Rules Missing") → actual: `WRITE_WIRE_IN_SYNC` (already exists and tested)

## test_5_3-conditional_statements.md

### IF_COND_MISSING_PARENS parser abort prevents multi-context testing
- **Severity:** Limitation
- **Description:** The parser aborts the entire file after the first IF_COND_MISSING_PARENS error. This prevents testing the rule in SYNC block context or with ELIF in the same file. Separate test files are needed for IF vs ELIF contexts, and SYNC block context remains untested.
- **Reproduction:** Any `.jz` file with `IF sel { ... }` — only one error reported, parser does not continue to subsequent modules or blocks.

### Test plan lists IF_COND_NOT_1BIT as missing but rule exists
- **Severity:** Test plan inaccuracy
- **Description:** The test plan (Section 6.2 "Rules Missing") lists `IF_COND_NOT_1BIT` as a gap, but the rule exists in rules.c as `IF_COND_WIDTH_NOT_1` in the `CONTROL_FLOW_IF_SELECT` category. Tested in 5_3_IF_COND_WIDTH_NOT_1-condition_not_1bit.

### Test plan rule IDs do not match rules.c
- `ASSIGN_INDEPENDENT_CHAIN_CONFLICT` → actual: `ASSIGN_INDEPENDENT_IF_SELECT` (already tested in test_1_5)
- `COMB_LOOP_DETECTED` → actual: `COMB_LOOP_UNCONDITIONAL`

## test_5_4-select_case_statements.md

### CONST identifiers rejected in CASE values (resolved — not a bug)
- **Severity:** Closed
- **Description:** The spec previously said "CASE values are integer constants or CONST names" but CONST values are unsized compile-time integers with no width, making them incompatible with CASE pattern matching which requires width-matched values. The spec was corrected: CASE labels are sized integer literals or `@global` constants. The compiler correctly rejects CONST in CASE via `CONST_USED_WHERE_FORBIDDEN`.

### WARN_INCOMPLETE_SELECT_ASYNC is dead code
- **Severity:** Minor (dead code)
- **Description:** The rule `WARN_INCOMPLETE_SELECT_ASYNC` exists in rules.c (GENERAL_WARNINGS category, line 467) but `sem_report_rule` is never called with this rule ID anywhere in the compiler source. The functionally equivalent `SELECT_DEFAULT_RECOMMENDED_ASYNC` (CONTROL_FLOW_IF_SELECT category) is what actually fires for ASYNC SELECT without DEFAULT. The two rules have nearly identical messages but exist in different categories.

### SELECT_CASE_WIDTH_MISMATCH listed as missing in test plan but exists
- **Severity:** Documentation error in test plan
- **Description:** The test plan (Section 6.2 "Rules Missing") lists `SELECT_CASE_WIDTH_MISMATCH` as a gap, but the rule exists in rules.c and is fully implemented in `driver_control.c`. It correctly detects width mismatches between sized CASE literals and the selector expression. Bare integers are correctly exempted (implicitly cast). Tested in 5_4_SELECT_CASE_WIDTH_MISMATCH-case_width_mismatch.

### Test plan rule IDs do not match rules.c
- `SELECT_DUPLICATE_CASE` → actual: `SELECT_DUP_CASE_VALUE`
- `WARN_INCOMPLETE_SELECT_ASYNC` → actual: `SELECT_DEFAULT_RECOMMENDED_ASYNC` (WARN_INCOMPLETE_SELECT_ASYNC is dead code)

## test_5_5-intrinsic_operators.md

### strtoul parsing bug for sized literals in intrinsic index arguments
- **Severity:** Bug
- **Description:** The INDEX_OUT_OF_RANGE checks for gbit(), sbit(), gslice(), and sslice() use `strtoul(expr->children[1]->text, NULL, 0)` to parse the index argument. When a sized literal like `4'd8` is used, `strtoul` parses "4" (stopping at the `'d'` character) instead of "8". This means `gbit(data, 4'd8)` is treated as index 4 instead of index 8, allowing out-of-range indices to pass validation when expressed as sized literals.
- **Location:** `compiler/src/sem/driver_operators.c` — all INDEX_OUT_OF_RANGE checks.
- **Reproduction:** `gbit(data, 4'd8)` where `data` is 8 bits wide — should report index 8 out of range [0..7] but silently accepts it (reads index as 4).

### ASYNC_ALIAS_LITERAL_RHS false positive on intrinsic function calls
- **Severity:** Bug
- **Description:** Intrinsic function calls containing literal arguments (e.g., `gbit(data, 8)`, `b2oh(data, 4)`) trigger `ASYNC_ALIAS_LITERAL_RHS` ("S4.10.3 Alias (=) RHS is a literal; use receive-assign (<=)") when used on the RHS of `=` (alias) assignments in ASYNC blocks. The rule is intended to prevent aliasing to bare literals, not to reject function call expressions that happen to contain literal arguments.
- **Location:** `compiler/src/sem/driver_assign.c` — ASYNC_ALIAS_LITERAL_RHS check.
- **Reproduction:** `out_a = gbit(data, 8);` in ASYNC block — fires ASYNC_ALIAS_LITERAL_RHS.
- **Workaround:** Use `<=` (receive-assign) instead of `=` (alias) for intrinsic expressions in ASYNC blocks.

### FUNC_RESULT_TRUNCATED_SILENTLY always suppressed by priority system
- **Severity:** Design issue
- **Description:** The rule `FUNC_RESULT_TRUNCATED_SILENTLY` (priority 0) fires in `driver_assign.c:1478-1496` for uadd/sadd/umul/smul result truncation. However, `ASSIGN_WIDTH_NO_MODIFIER` (priority 1) always co-fires on the same line/column for the same width mismatch. The diagnostic priority system suppresses the lower-priority rule, making FUNC_RESULT_TRUNCATED_SILENTLY unreachable in practice.
- **Location:** `compiler/src/sem/driver_assign.c:1478-1496`

### Dead code rules: WIDTHOF_INVALID_TARGET, WIDTHOF_INVALID_SYNTAX, WIDTHOF_WIDTH_NOT_RESOLVABLE
- **Severity:** Dead code
- **Description:** Three WIDTHOF-related rules exist in rules.c (FUNCTIONS_AND_CLOG2 category) but `sem_report_rule` is never called with any of these rule IDs anywhere in the semantic analysis code. They appear to be remnants of planned but unimplemented validation for widthof() edge cases.
- **Location:** `compiler/src/rules.c`

### Test plan rule IDs do not match rules.c
- `INTRINSIC_ARG_COUNT` → no matching rule ID; parser handles argument count
- `INTRINSIC_COMPILE_TIME_ONLY` → actual: `CLOG2_INVALID_CONTEXT` / `WIDTHOF_INVALID_CONTEXT`
- `INTRINSIC_OPERAND_TYPE` → no matching rule ID; function-specific rules
- `INTRINSIC_BSWAP_NOT_BYTE` → actual: `BSWAP_WIDTH_NOT_BYTE_ALIGNED`
- `INTRINSIC_UNDEFINED` → no matching rule ID; parse error
- `INTRINSIC_LIT_OVERFLOW` → actual: `LIT_VALUE_OVERFLOW`
- `INTRINSIC_CLOG2_ZERO` → actual: `CLOG2_NONPOSITIVE_ARG`
- `INTRINSIC_WIDTH_MISMATCH` → no matching rule ID; function-specific rules

## test_6_1-project_purpose.md

### Test plan rule IDs do not match rules.c
- `PROJECT_CHIP_NOT_FOUND` → actual: `PROJECT_CHIP_DATA_NOT_FOUND`
- `PROJECT_CHIP_JSON_MALFORMED` → listed as missing in test plan (Section 6.2) but exists as `PROJECT_CHIP_DATA_INVALID`

### No compiler bugs found
- Both `PROJECT_CHIP_DATA_NOT_FOUND` and `PROJECT_CHIP_DATA_INVALID` fire correctly with expected diagnostics.

## test_6_2-project_canonical_form.md

### Test plan rule IDs do not match rules.c
- `IMPORT_HAS_PROJECT` → actual: `IMPORT_FILE_HAS_PROJECT`
- `IMPORT_WRONG_POSITION` → listed as missing in test plan but exists as `IMPORT_NOT_AT_PROJECT_TOP`
- `IMPORT_FILE_NOT_FOUND` → listed in I/O matrix but no corresponding rule ID in rules.c; handled as generic stderr error

### Test plan missing rules that exist in rules.c
- `IMPORT_OUTSIDE_PROJECT` — Exists in rules.c but test plan lists `DIRECTIVE_INVALID_CONTEXT` instead. The compiler uses the more specific `IMPORT_OUTSIDE_PROJECT` for `@import` outside `@project`.
- `IMPORT_DUP_MODULE_OR_BLACKBOX` — Exists in rules.c but not listed in test plan's rules matrix.

### No compiler bugs found
- All five import-related rules fire correctly with expected diagnostics.

## test_6_3-config_block.md

### CONFIG.X syntax inside CONFIG block fires CONFIG_INVALID_EXPR_TYPE for backward references
- **Severity:** Bug
- **Description:** The spec says `CONFIG.<name>` can reference earlier CONFIG entries (backward references). However, using `CONFIG.X` syntax inside the CONFIG block always fires CONFIG_INVALID_EXPR_TYPE, even for valid backward references. The compiler instead supports bare name references (e.g., `B = A;` instead of `B = CONFIG.A;`). The CONFIG_FORWARD_REF check runs first for forward references and masks this bug.
- **Reproduction:** `CONFIG { A = 8; B = CONFIG.A; }` → fires CONFIG_INVALID_EXPR_TYPE on B. But `CONFIG { A = 8; B = A; }` → no error.

### CONFIG_MULTIPLE_BLOCKS rule not used — parser catches first
- **Severity:** Discrepancy
- **Description:** The rule CONFIG_MULTIPLE_BLOCKS exists in rules.c ("S6.3 More than one CONFIG block defined in project") but the parser detects multiple CONFIG blocks first and emits PARSE000 ("multiple CONFIG blocks in a single project are not allowed"). The semantic rule never fires.
- **Reproduction:** Two CONFIG blocks in @project → PARSE000 instead of CONFIG_MULTIPLE_BLOCKS.

### @file() in MEM does not accept CONFIG references
- **Severity:** Bug
- **Description:** The spec says `@file(CONFIG.FIRMWARE)` is valid in MEM declarations (Section 6.3 example). However, the parser rejects CONFIG references in @file() with PARSE000 ("expected string path or CONST/CONFIG reference in @file(...) MEM initializer"), even though the error message mentions CONST/CONFIG references should be valid.
- **Reproduction:** `MEM { rom [8] [4] = @file(CONFIG.FIRMWARE) { OUT rd ASYNC; }; }` → PARSE000.

### CONST_NUMERIC_IN_STRING_CONTEXT untestable due to @file parser bug
- **Severity:** Test gap
- **Description:** The only string context where CONFIG/CONST can be used is @file() in MEM declarations. Since the parser rejects CONFIG references in @file() (see above), CONST_NUMERIC_IN_STRING_CONTEXT cannot be triggered via validation tests.

### CONFIG_CIRCULAR_DEP test includes CONFIG_FORWARD_REF diagnostics
- **Severity:** Expected behavior (not a bug)
- **Description:** Circular CONFIG dependencies inherently involve forward references. The compiler correctly fires both CONFIG_FORWARD_REF (for the forward reference aspect) and CONFIG_CIRCULAR_DEP (for the circularity). The test .out file includes both rule IDs.

## test_6_4-clocks_block.md

### CLOCK_SOURCE_AMBIGUOUS cascades with CLOCK_NAME_NOT_IN_PINS and CLOCK_GEN_OUTPUT_HAS_PERIOD
- **Severity:** Expected behavior (not a bug)
- **Description:** When a CLOCK_GEN output clock has a period declared in the CLOCKS block, three diagnostics fire: (1) CLOCK_NAME_NOT_IN_PINS because the period implies an external clock but it's not in IN_PINS, (2) CLOCK_SOURCE_AMBIGUOUS because the period implies external source while CLOCK_GEN implies generated source, (3) CLOCK_GEN_OUTPUT_HAS_PERIOD because CLOCK_GEN outputs must not have periods. These are three views of the same root cause and cannot be separated.

### CLOCK_GEN_INPUT_IS_SELF_OUTPUT cascades with CLOCK_GEN_INPUT_NO_PERIOD
- **Severity:** Expected behavior (not a bug)
- **Description:** When a CLOCK_GEN input references its own output, CLOCK_GEN_INPUT_NO_PERIOD also fires because the self-referencing clock (as a CLOCK_GEN output) cannot have a period in the CLOCKS block, so the input clock lacks a required period. This is an inherent cascade from the same root cause.

### CONFIG block uses bare names, not CONFIG.X for internal references
- **Severity:** Spec/implementation divergence
- **Description:** The spec says CONFIG entries reference earlier entries using `CONFIG.<name>`. The compiler implementation uses bare names instead (e.g., `B = A;` not `B = CONFIG.A;`). Expressions with bare names work (e.g., `C = A + B;`), but `CONFIG.X` syntax inside CONFIG blocks doesn't. Outside CONFIG blocks, `CONFIG.X` works correctly for port widths, CONST initializers, etc.

## test_6_5-pin_blocks.md

### PIN_DIFF_OUT_MISSING_FCLK/PCLK/RESET not enforced on INOUT_PINS
- **Severity:** Potential gap
- **Description:** The rules PIN_DIFF_OUT_MISSING_FCLK, PIN_DIFF_OUT_MISSING_PCLK, and PIN_DIFF_OUT_MISSING_RESET only fire for differential pins in OUT_PINS blocks. Differential pins in INOUT_PINS with missing fclk/pclk/reset attributes are not flagged. The spec says these attributes are "required when mode=DIFFERENTIAL on output pins" — INOUT_PINS can also drive outputs, so arguably should require these attributes too.

### PIN_TERM_ON_OUTPUT does not fire for term=OFF on OUT_PINS
- **Severity:** Expected behavior (not a bug)
- **Description:** The rule PIN_TERM_ON_OUTPUT only fires when term=ON is explicitly specified on OUT_PINS. Setting term=OFF on an output pin is not flagged, as OFF is the default/no-op value. This is reasonable behavior — the rule catches cases where active termination is mistakenly enabled on outputs.

## test_6_6-map_block.md

### Test plan rule IDs do not match rules.c
- **Severity:** Documentation
- **Description:** The test plan (Section 4, Input/Output Matrix) uses rule IDs that differ from rules.c:
  - `MAP_PIN_INVALID` → actual: `MAP_INVALID_BOARD_PIN_ID`
  - `MAP_DUPLICATE` → actual: `MAP_DUP_PHYSICAL_LOCATION`
  - `MAP_BOARD_PIN_CONFLICT` → not a separate rule; covered by `MAP_DUP_PHYSICAL_LOCATION`
  - `MAP_REQUIRED_UNMAPPED` → listed as missing (Section 6.2), but exists as `MAP_PIN_DECLARED_NOT_MAPPED`
  - `MAP_DIFF_NO_PAIR` → listed as missing (Section 6.2), but covered by `MAP_DIFF_EXPECTED_PAIR` and `MAP_DIFF_MISSING_PN`

### MAP_INVALID_BOARD_PIN_ID trigger conditions unclear
- **Severity:** Test gap
- **Description:** The rule `MAP_INVALID_BOARD_PIN_ID` ("S6.6/S6.9 Board pin ID format invalid for target device") exists in rules.c but could not be triggered in testing. Without a CHIP= project setting, the compiler does not validate board pin ID format. Integer board pin IDs are always accepted. The trigger conditions likely require chip-specific data that defines valid pin ID ranges or formats, but this could not be confirmed without a complete chip project setup.

### MAP_DIFF_SAME_PIN inherently co-fires MAP_DUP_PHYSICAL_LOCATION
- **Severity:** Expected behavior (not a bug)
- **Description:** When a differential MAP entry has the same physical pin for P and N (MAP_DIFF_SAME_PIN), the compiler also reports MAP_DUP_PHYSICAL_LOCATION because the same physical pin is indeed used twice. This is logically correct — the same-pin error is the root cause, and the duplicate physical location is a consequence. Both diagnostics fire on the same line.

### No compiler bugs found
- All tested MAP rules (MAP_PIN_DECLARED_NOT_MAPPED, MAP_PIN_MAPPED_NOT_DECLARED, MAP_DUP_PHYSICAL_LOCATION, MAP_DIFF_EXPECTED_PAIR, MAP_SINGLE_UNEXPECTED_PAIR, MAP_DIFF_MISSING_PN, MAP_DIFF_SAME_PIN) fire correctly with expected diagnostics and line/column positions.

## test_6_7-blackbox_modules.md

### BLACKBOX_BODY_DISALLOWED incorrectly rejects CONST blocks in blackbox
- **Severity:** Bug
- **Description:** The semantic check `sem_check_project_blackboxes` (driver_project.c:1605) only whitelists `JZ_AST_PORT_BLOCK` children. CONST blocks (`JZ_AST_CONST_BLOCK`) are treated as forbidden, triggering `BLACKBOX_BODY_DISALLOWED`. However:
  1. The parser (`parse_blackbox_body` in parser_module.c:296) explicitly allows CONST blocks in blackbox bodies
  2. The specification (S6.7, example at line ~4467) shows `CONST { FREQ_MHZ = 100; }` inside a blackbox
  3. The rule's own description lists only "ASYNCHRONOUS/SYNCHRONOUS/WIRE/REGISTER/MEM" as forbidden — CONST is not mentioned
- **Fix:** Add `if (child->type == JZ_AST_CONST_BLOCK) continue;` alongside the PORT_BLOCK check in `sem_check_project_blackboxes`.
- **Reproduction:** `@blackbox bb { CONST { VAL = 42; } PORT { IN [1] din; OUT [1] dout; } }` → error BLACKBOX_BODY_DISALLOWED (should be clean)

### BLACKBOX_UNDEFINED_IN_NEW rule defined but never used
- **Severity:** Dead code / incomplete implementation
- **Description:** The rule `BLACKBOX_UNDEFINED_IN_NEW` ("S6.7/S6.9 @new references undefined blackbox name") is defined in rules.c (line 338) but is never referenced in any semantic analysis code. When a @new references a non-existent module or blackbox, the compiler reports `INSTANCE_UNDEFINED_MODULE` instead (from both driver_project.c and driver_instance.c). Either the rule should be used in place of INSTANCE_UNDEFINED_MODULE for blackbox-specific cases, or it should be removed from rules.c.
- **Reproduction:** `@new inst_bad nonexistent_bb { ... }` → error INSTANCE_UNDEFINED_MODULE (never BLACKBOX_UNDEFINED_IN_NEW)

## test_6_8-bus_aggregation.md

No compiler issues discovered during test generation. All BUS definition rules (BUS_DEF_DUP_NAME, BUS_DEF_SIGNAL_DUP_NAME) fire correctly with accurate line/column numbers.

### Test plan lists BUS_SIGNAL_DUPLICATE as missing rule
- **Severity:** Test plan inaccuracy (not a compiler bug)
- **Description:** The test plan's "Rules Missing" section lists `BUS_SIGNAL_DUPLICATE` as a gap, but this rule already exists in rules.c as `BUS_DEF_SIGNAL_DUP_NAME` and functions correctly. The test plan should be updated to reflect this.

## test_6_9-top_level_module.md

### TOP_MODULE_NOT_FOUND rule ID not registered in rules.c
- **Severity:** Bug (minor)
- **Description:** When @top references a module that does not exist, the compiler emits `TOP_MODULE_NOT_FOUND` as the rule ID. However, this rule ID is not registered in `rules.c`. The test plan expected `INSTANCE_UNDEFINED_MODULE` to fire in this context. The diagnostic message includes an unusual `(orig: TOP_MODULE_NOT_FOUND)` suffix, suggesting the rule lookup falls back to a raw format when the ID is not found in the rules table.
- **Reproduction:** `@top NonExistentModule { IN [1] clk = _; OUT [1] data = _; }` where `NonExistentModule` is not defined → emits `TOP_MODULE_NOT_FOUND` instead of `INSTANCE_UNDEFINED_MODULE`.
- **Expected:** Either register `TOP_MODULE_NOT_FOUND` in rules.c with proper category/message, or use `INSTANCE_UNDEFINED_MODULE` for consistency with @new behavior.

### WARN_UNUSED_MODULE fires for modules in file with @top referencing undefined module
- **Severity:** Minor
- **Description:** When @top references a non-existent module, valid modules defined in the same file trigger `WARN_UNUSED_MODULE` because no module is actually instantiated as the top. This is technically correct behavior but adds noise to the diagnostic output.
- **Reproduction:** Define a valid `@module ActualModule` in the same file as `@top NonExistentModule` → `WARN_UNUSED_MODULE` fires for `ActualModule`.

## test_6_10-project_scope_and_uniqueness.md

### Test plan lists PROJECT_MULTIPLE as "Rules Missing" but rule exists
- **Severity:** Test plan gap
- **Description:** The test plan section 6.2 "Rules Missing" lists `PROJECT_MULTIPLE` as having no rule ID in rules.c, but the rule `PROJECT_MULTIPLE_PER_FILE` exists in rules.c and works correctly. Test created as `6_10_PROJECT_MULTIPLE_PER_FILE-two_projects_in_file`.

## test_7_0-memory_port_modes.md

### Test plan references non-existent rule MEM_PORT_MODE_INVALID
- **Severity:** Test plan gap
- **Description:** The test plan section 4 "Input/Output Matrix" and section 6.1 "Rules Tested" reference rule ID `MEM_PORT_MODE_INVALID`, which does not exist in `rules.c`. The actual rules covering this functionality are `MEM_INVALID_PORT_TYPE` (for ASYNC/SYNC on write ports) and `MEM_INVALID_WRITE_MODE` (for invalid WRITE_MODE values). Tests created for both actual rules.

### Dead code: MEM_INVALID_PORT_TYPE semantic check for OUT port with invalid qualifier
- **Severity:** Minor (dead code)
- **Description:** In `driver_mem.c` lines 920-931, there is a semantic check that fires `MEM_INVALID_PORT_TYPE` when an OUT port has a qualifier other than ASYNC or SYNC. However, the parser in `parser_mem.c` only consumes ASYNC/SYNC identifiers after OUT port names — any other identifier is not consumed and causes a parse error at the expected semicolon. This makes the semantic check unreachable through normal parsing paths.

## test_7_1-mem_declaration.md

### Test plan rule IDs don't match rules.c
- **Severity:** Test plan gap
- **Description:** The test plan references rule IDs that don't exist in `rules.c`: `MEM_DECL_TYPE_INVALID` (actual: `MEM_TYPE_INVALID`), `MEM_DECL_DEPTH_ZERO` (actual: `MEM_INVALID_DEPTH`), `MEM_DECL_WIDTH_ZERO` (actual: `MEM_INVALID_WORD_WIDTH`), `MEM_DECL_NO_PORTS` (actual: `MEM_EMPTY_PORT_LIST`), `SCOPE_DUPLICATE_SIGNAL` for MEM names (actual: `MEM_DUP_NAME`). Tests created using the actual rule IDs from `rules.c`.

### Invalid MEM declarations still emit port-never-accessed warnings
- **Severity:** Minor (noise)
- **Description:** When a MEM has an invalid depth, width, or type, the compiler still parses its port declarations and later emits `MEM_WARN_PORT_NEVER_ACCESSED` warnings for those ports. This is because the invalid MEM cannot be used, so its ports are never accessed. While technically correct, these cascading warnings add noise to error output. Similarly, `NET_DANGLING_UNUSED` fires on helper module ports that exist only to match the top module interface but aren't used because the invalid MEM cannot be accessed.

## test_7_2-port_types_and_semantics.md

### MEM_MULTIPLE_WDATA_ASSIGNS rule does not fire
- **Severity:** Bug
- **Description:** The rule `MEM_MULTIPLE_WDATA_ASSIGNS` exists in `rules.c` but does not fire when two `.wdata <=` assignments to the same INOUT port occur in the same SYNCHRONOUS block. The compiler produces no diagnostic at all for this case.
- **Reproduction:** Two `mem.rw.wdata <= value;` statements in the same SYNCHRONOUS block — no error emitted.

### MEM_MULTIPLE_ADDR_ASSIGNS rule does not fire
- **Severity:** Bug
- **Description:** The rule `MEM_MULTIPLE_ADDR_ASSIGNS` exists in `rules.c` but does not fire when two `.addr <=` assignments to the same INOUT port occur in the same SYNCHRONOUS block. The compiler produces no diagnostic at all for this case.
- **Reproduction:** Two `mem.rw.addr <= value;` statements in the same SYNCHRONOUS block — no error emitted.

### MEM_INOUT_WDATA_WRONG_OP shadowed by SYNC_NO_ALIAS
- **Severity:** Bug (dead rule)
- **Description:** The rule `MEM_INOUT_WDATA_WRONG_OP` ("INOUT port .wdata must be assigned with '<=' operator in SYNCHRONOUS blocks") exists in `rules.c` but cannot be triggered. When `.wdata` is assigned using `=` in a SYNCHRONOUS block, the generic `SYNC_NO_ALIAS` check fires first ("Aliasing `=` is forbidden in SYNCHRONOUS blocks"), preventing the MEM-specific rule from ever being reached.
- **Reproduction:** `mem.rw.wdata = value;` inside SYNCHRONOUS block — fires `SYNC_NO_ALIAS` instead of `MEM_INOUT_WDATA_WRONG_OP`.

## test_7_3-memory_access_syntax.md

### MEM_READ_SYNC_WITH_EQUALS shadowed by SYNC_NO_ALIAS
- **Severity:** Bug (dead rule)
- **Description:** The rule `MEM_READ_SYNC_WITH_EQUALS` ("Synchronous MEM read used `=` in SYNCHRONOUS block; did you mean `<=`?") exists in `rules.c` but cannot be triggered. When `=` is used in a SYNCHRONOUS block, the generic `SYNC_NO_ALIAS` check fires first, preventing the MEM-specific rule from ever being reached.
- **Reproduction:** `r = mem.rd.data;` inside SYNCHRONOUS block — fires `SYNC_NO_ALIAS` instead of `MEM_READ_SYNC_WITH_EQUALS`.

### MEM_SYNC_ADDR_WITHOUT_RECEIVE shadowed by SYNC_NO_ALIAS
- **Severity:** Bug (dead rule)
- **Description:** The rule `MEM_SYNC_ADDR_WITHOUT_RECEIVE` ("MEM read address must use `<=` in SYNCHRONOUS block; did you mean `<=` instead of `=`?") exists in `rules.c` but cannot be triggered. When `=` is used in a SYNCHRONOUS block, the generic `SYNC_NO_ALIAS` check fires first.
- **Reproduction:** `mem.rd.addr = addr;` inside SYNCHRONOUS block — fires `SYNC_NO_ALIAS` instead of `MEM_SYNC_ADDR_WITHOUT_RECEIVE`.

### MEM_SYNC_ADDR_IN_ASYNC_BLOCK shadowed by ASYNC_INVALID_STATEMENT_TARGET
- **Severity:** Bug (dead rule)
- **Description:** The rule `MEM_SYNC_ADDR_IN_ASYNC_BLOCK` ("SYNC read addresses must be assigned in SYNCHRONOUS blocks") exists in `rules.c` but cannot be triggered. When `.addr <=` is used in an ASYNCHRONOUS block, the generic `ASYNC_INVALID_STATEMENT_TARGET` check fires first since `<=` is not valid in async blocks.
- **Reproduction:** `mem.rd.addr <= addr;` inside ASYNCHRONOUS block — fires `ASYNC_INVALID_STATEMENT_TARGET` instead of `MEM_SYNC_ADDR_IN_ASYNC_BLOCK`.

## test_7_4-write_modes.md

### No new issues
- The only testable rule (`MEM_INVALID_WRITE_MODE`) is already covered by tests under `7_0_MEM_INVALID_WRITE_MODE-bad_write_mode_value`.
- The shorthand form for invalid write modes (`IN wr INVALID;`) cannot trigger `MEM_INVALID_WRITE_MODE` because the parser only recognizes the three valid keywords (WRITE_FIRST, READ_FIRST, NO_CHANGE) as shorthand qualifiers. An unrecognized identifier after the port name would result in a parse error (PARSE000), not MEM_INVALID_WRITE_MODE. This is arguably correct behavior — the attribute block form `{ WRITE_MODE = INVALID; }` is the proper way to specify write modes when not using shorthand.

## test_7_5-initialization.md

### MEM_INIT_CONTAINS_Z rule missing from rules.c
- **Severity:** Gap
- **Description:** The test plan expects a `MEM_INIT_CONTAINS_Z` rule for z bits in memory initialization literals, but no such rule exists in `rules.c`. The spec (S7.5.1) says "Memory initialization literals must not contain x" — z is not explicitly mentioned for literal init. For file-based init, `MEM_INIT_FILE_CONTAINS_X` covers both x and z. If z should also be rejected in literal init, a new rule is needed.
- **Reproduction:** `MEM { m [8] [4] = 8'bz000_0000 { ... }; }` — no `MEM_INIT_CONTAINS_Z` diagnostic fires (z in binary literal may be treated differently than x).

### PATH_OUTSIDE_SANDBOX masks MEM_INIT_FILE_NOT_FOUND when run from wrong CWD
- **Severity:** Minor / Environment-dependent
- **Description:** When the compiler is invoked from a CWD different from the input file's directory, @file() paths for nonexistent files trigger `PATH_OUTSIDE_SANDBOX` instead of `MEM_INIT_FILE_NOT_FOUND`. This is because the sandbox resolver fails before the file existence check. When invoked via `run_validation.sh` (which passes absolute paths), the correct `MEM_INIT_FILE_NOT_FOUND` fires.
- **Reproduction:** `cd /tmp && jz-hdl --info --lint /path/to/validation/7_5_MEM_INIT_FILE_NOT_FOUND-nonexistent_file.jz` → PATH_OUTSIDE_SANDBOX instead of MEM_INIT_FILE_NOT_FOUND.

## test_7_6-complete_examples.md

No issues. All 9 scenarios are happy-path examples that produce no diagnostics. No validation tests were created since the lint validation framework only tests for expected diagnostic output.

## test_7_7-error_checking_and_validation.md

### WARN_DEAD_CODE_UNREACHABLE fires alongside MEM_WARN_DEAD_CODE_ACCESS
- **Severity:** Expected behavior (not a bug)
- **Description:** Any `IF (1'b0)` dead code path triggers both `WARN_DEAD_CODE_UNREACHABLE` (general dead code) and `MEM_WARN_DEAD_CODE_ACCESS` (MEM-specific dead code). The .out file includes both diagnostics. This is correct behavior — the general warning fires on any dead code, while the MEM-specific warning provides additional context.

### MEM_WARN_DEAD_CODE_ACCESS cannot be tested in ASYNCHRONOUS blocks
- **Severity:** Language constraint (not a bug)
- **Description:** Memory reads in ASYNCHRONOUS blocks use `=` alias syntax. JZ-HDL forbids alias `=` inside IF/SELECT (ASYNC_ALIAS_IN_CONDITIONAL), which means dead-code MEM reads in ASYNC IF blocks always trigger a parse/semantic error before MEM_WARN_DEAD_CODE_ACCESS can fire. Only SYNCHRONOUS block dead paths are testable.

### MEM_MULTIPLE_WDATA_ASSIGNS .out file is empty
- **Severity:** Test gap
- **Description:** The existing test `7_3_MEM_MULTIPLE_WDATA_ASSIGNS-inout_multi_wdata.jz` has an empty `.out` file, meaning the compiler may not be emitting MEM_MULTIPLE_WDATA_ASSIGNS diagnostics for the test case. This needs investigation.

## test_7_8-mem_vs_register_vs_wire.md

### No dedicated rule for REGISTER-with-address-syntax confusion
- **Severity:** Missing rule / test gap
- **Description:** Test plan scenario 3.2.2 specifies "REGISTER with address syntax — Error", but no rule ID exists in `rules.c` for this confusion. Using bracket notation on a register (`reg[addr]`) is valid bit-slicing, not an error. Using MEM-style dot-field notation (`reg.port.addr`) would produce a generic parse or undeclared-identifier error, not a specific storage-type-confusion diagnostic. A dedicated rule (e.g., `REGISTER_NOT_ADDRESSABLE`) could improve error messages when users confuse registers with memories.

## test_7_9-mem_in_module_instantiation.md

### No MEM-specific rule for hierarchical access rejection
- **Severity:** Missing rule / design consideration
- **Description:** When a parent module attempts to access a child instance's MEM directly (e.g., `inst.child_mem.wr[addr]`), the compiler produces a generic `UNDECLARED_IDENTIFIER` error. A more specific rule (e.g., `MEM_HIERARCHICAL_ACCESS_FORBIDDEN`) could provide a clearer error message explaining that memories are module-internal and must be accessed through ports. The current behavior is correct but the error message could be more helpful.

## test_7_10-const_evaluation_in_mem.md

### Negative CONST corrupts resolution of other CONSTs in same block
- **Severity:** Bug
- **Description:** When a CONST block contains a negative value (e.g., `NEG_WIDTH = -2`), the compiler correctly fires `CONST_NEGATIVE_OR_NONINT` for that declaration. However, other valid CONSTs in the same block (e.g., `VALID_WIDTH = 8`) also fail to resolve when used in MEM dimensions, producing spurious `MEM_UNDEFINED_CONST_IN_WIDTH` errors. Valid CONSTs in the same block should still resolve correctly even when a sibling CONST has a negative value.
- **Reproduction:** Declare `NEG = -2; VALID = 8;` in same CONST block, then `MEM { m [VALID] [16] ... }` → spurious `MEM_UNDEFINED_CONST_IN_WIDTH` on the valid MEM.

## test_7_11-synthesis_implications.md

No compiler issues found. All three testable rules (`MEM_BLOCK_RESOURCE_EXCEEDED`, `MEM_DISTRIBUTED_RESOURCE_EXCEEDED`, `MEM_BLOCK_MULTI`) work as expected.

## test_8_1-global_purpose.md

No compiler issues found. Both testable rules (`GLOBAL_NAMESPACE_DUPLICATE`, `GLOBAL_ASSIGN_FORBIDDEN`) work as expected. The test plan references rule names that don't match rules.c (`GLOBAL_DUPLICATE_NAME` → `GLOBAL_NAMESPACE_DUPLICATE`, `GLOBAL_READONLY` → `GLOBAL_ASSIGN_FORBIDDEN`), but both rules exist and fire correctly.

## test_8_2-global_syntax.md

No compiler issues found. The testable rule (`GLOBAL_INVALID_EXPR_TYPE`) works as expected. The test plan references the rule as `GLOBAL_NOT_SIZED_LITERAL`, but the actual rule ID in rules.c is `GLOBAL_INVALID_EXPR_TYPE`.

## test_8_3-global_semantics.md

No compiler issues found. The testable rule (`GLOBAL_CONST_NAME_DUPLICATE`) works as expected. The test plan references the rule as `GLOBAL_DUPLICATE_CONST`, but the actual rule ID in rules.c is `GLOBAL_CONST_NAME_DUPLICATE`.

## test_8_4-global_value_semantics.md

### Test plan references wrong rule ID
- **Severity:** Documentation
- **Description:** The test plan references `WIDTH_ASSIGN_MISMATCH_NO_EXT` but the actual rule ID in rules.c is `ASSIGN_WIDTH_NO_MODIFIER`. The comment in rules.c indicates `ASSIGN_WIDTH_NO_MODIFIER` supersedes `WIDTH_ASSIGN_MISMATCH_NO_EXT`.

## test_8_5-global_errors.md

### LIT_OVERFLOW not fired for overflowing literals in @global blocks
- **Severity:** Possible bug
- **Description:** When a sized literal overflows its declared width inside a `@global` block (e.g., `4'hFF`), the compiler reports `GLOBAL_INVALID_EXPR_TYPE` ("Global value must be a sized literal") instead of `LIT_OVERFLOW` ("Literal numeric value exceeds declared width"). The test plan expects `LIT_OVERFLOW` but the compiler subsumes it under the global expression type check. The literal IS properly sized syntax-wise; only the value overflows.
- **Reproduction:** `@global G  OVER = 4'hFF; @endglob` → `GLOBAL_INVALID_EXPR_TYPE` instead of `LIT_OVERFLOW`.

### Test plan uses rule IDs not in rules.c
- **Severity:** Documentation
- **Description:** The test plan references `GLOBAL_DUPLICATE_NAME`, `GLOBAL_DUPLICATE_CONST`, `GLOBAL_NOT_SIZED_LITERAL`, and `GLOBAL_READONLY` — none of which exist in rules.c. The actual rule IDs are `GLOBAL_NAMESPACE_DUPLICATE`, `GLOBAL_CONST_NAME_DUPLICATE`, `GLOBAL_INVALID_EXPR_TYPE`, and `GLOBAL_ASSIGN_FORBIDDEN`.

### Backward references in @global blocks trigger GLOBAL_INVALID_EXPR_TYPE
- **Severity:** Possible bug / design question
- **Description:** Referencing an earlier constant within the same `@global` block (e.g., `Y = X;` where X is defined above) triggers `GLOBAL_INVALID_EXPR_TYPE`. The spec says global values must be sized literals, so this may be by design — constants cannot reference other constants, only sized literals. However, if backward references are intentionally forbidden, a more specific diagnostic would be helpful.
- **Reproduction:** `@global G  X = 8'h01; Y = X; @endglob` → `GLOBAL_INVALID_EXPR_TYPE` on `Y = X`.

## test_9_1-check_syntax.md

### No specific rule IDs for @check parse-level syntax errors
- **Severity:** Documentation / Feature request
- **Description:** The test plan lists negative tests for missing parentheses, missing message, missing semicolon, and missing comma in `@check` syntax. All of these produce generic `PARSE000` errors rather than specific rule IDs. The parser emits descriptive messages (e.g., "expected '(' after @check") but these are not defined in `rules.c`. If specific rule IDs are desired for these parse errors, they would need to be added to `rules.c`.
- **Reproduction:** `@check WIDTH == 8, "msg";` → `PARSE000 parse error near token 'WIDTH': expected '(' after @check`

## test_9_2-check_semantics.md

### No issues found
- CHECK_FAILED fires correctly in both project scope and module scope contexts.
- The test plan references CHECK_NON_CONSTANT which does not exist in `rules.c`; the equivalent rule is CHECK_INVALID_EXPR_TYPE, already tested under test_9_1.

## test_9_3-check_placement_rules.md

### Test plan references non-existent rule CHECK_WRONG_CONTEXT
- **Severity:** Test plan discrepancy
- **Description:** The test plan references `CHECK_WRONG_CONTEXT` but this rule does not exist in `rules.c`. The actual rule that fires is `DIRECTIVE_INVALID_CONTEXT`. Additionally, `CHECK_INVALID_PLACEMENT` is defined in `rules.c` but is never used anywhere in the compiler source code — it appears to be dead code.

### @check in blocks causes cascading PARSE000 errors
- **Severity:** Bug (parser recovery)
- **Description:** When `@check` appears inside an ASYNCHRONOUS or SYNCHRONOUS block, the parser reports `DIRECTIVE_INVALID_CONTEXT` but only advances past the `@check` keyword token, leaving the parenthesized arguments `(expr, "msg");` to be parsed as a statement. This causes a cascading `PARSE000` error ("expected identifier in assignment left-hand side") and aborts parsing of the rest of the file.
- **Impact:** Only one @check placement violation can be detected per compilation — the parser does not recover to find subsequent violations. Tests must include the cascading PARSE000 in their expected output.
- **Fix suggestion:** The parser should skip the entire `@check(...)` directive (including parenthesized arguments and semicolon) after reporting DIRECTIVE_INVALID_CONTEXT, similar to how other recovery paths work.

### DIRECTIVE_INVALID_CONTEXT message does not mention @check
- **Severity:** Minor (UX)
- **Description:** When `@check` appears in a block, the diagnostic uses the generic `DIRECTIVE_INVALID_CONTEXT` message: "Structural directives (@project/@module/@endproj/@endmod/@blackbox/@new/@import) used in invalid location". This message lists structural directives but does not mention `@check`, which is confusing since `@check` is the actual offending token. The unused `CHECK_INVALID_PLACEMENT` rule has a more appropriate message.

## test_9_4-check_expression_rules.md

### Test plan references non-existent rule CHECK_RUNTIME_OPERAND
- **Severity:** Test plan discrepancy
- **Description:** The test plan references `CHECK_RUNTIME_OPERAND` but this rule does not exist in `rules.c`. The actual rule that fires for runtime signals in @check is `CHECK_INVALID_EXPR_TYPE`, already tested under test_9_1 (ports, registers, wires, undefined identifiers) and now test_9_4 (signal slices).

### Memory port fields in @check not testable without incidental diagnostics
- **Severity:** Test limitation
- **Description:** S9.4 lists "any memory port" as a forbidden operand in @check. While memory port data field references (e.g., `mem.rw.data`) in @check correctly trigger CHECK_INVALID_EXPR_TYPE, structuring a test with memory that avoids all incidental warnings (WARN_UNSINKED_REGISTER, NET_FLOATING_WITH_SINK) proved difficult. The memory port context is not covered by the current test. Signal slices (port, register, wire) are covered instead.

## test_9_5-check_evaluation_order.md

### Undefined CONST in module-scope @check produces UNDECLARED_IDENTIFIER instead of CHECK_INVALID_EXPR_TYPE
- **Severity:** Possible compiler inconsistency
- **Description:** At project scope, an undefined identifier in @check produces `CHECK_INVALID_EXPR_TYPE`. At module scope, the same scenario produces `UNDECLARED_IDENTIFIER` instead. This suggests the compiler resolves identifiers before evaluating @check at module scope, but at project scope the @check evaluator handles the undefined reference directly. The behavior is consistent and correct (the error is caught either way), but the rule ID differs depending on scope. The 9_5 test only covers project-scope triggers to avoid mixing rule IDs in the .out file.

## test_9_7-check_error_conditions.md

### Test plan references non-existent rule IDs CHECK_NON_CONSTANT and CHECK_UNDEFINED
- **Severity:** Test plan discrepancy
- **Description:** The test plan's Input/Output Matrix references `CHECK_NON_CONSTANT` (for runtime signals) and `CHECK_UNDEFINED` (for undefined identifiers), but neither rule exists in `rules.c`. Both cases are handled by the existing `CHECK_INVALID_EXPR_TYPE` rule. All three test scenarios in the plan are already fully covered by existing validation tests (9_1, 9_2, 9_5), so no new test files were created.

### S9.7 error conditions without rule IDs
- **Severity:** Spec gap / untestable
- **Description:** Spec section 9.7 lists five error conditions, but only two have corresponding rule IDs in `rules.c` (CHECK_FAILED for zero expression, CHECK_INVALID_EXPR_TYPE for runtime signals/undefined identifiers). The remaining conditions — "non-integer result" and "disallowed operators" — have no distinct rule IDs and cannot be tested as separate scenarios in the validation framework.

## test_10_2-template_definition.md

No issues found. All rules from the test plan (TEMPLATE_DUP_NAME, TEMPLATE_DUP_PARAM) exist in rules.c and are fully testable.

## test_10_3-template_allowed_content.md

### TEMPLATE_SCRATCH_WIDTH_INVALID not enforced
- **Severity:** Bug (unimplemented rule)
- **Description:** Rule `TEMPLATE_SCRATCH_WIDTH_INVALID` exists in `rules.c` with message "S10.3 @scratch width must be a positive integer constant expression", but the compiler never fires it. `@scratch` declarations with zero width (`@scratch bad [0];`) and parameter-based widths (`@scratch bad [w];` where `w` is a template parameter) produce no diagnostic.
- **Reproduction:** Any `@scratch` with zero or non-constant width inside a `@template` body compiles cleanly.

### TEMPLATE_EXTERNAL_REF not enforced
- **Severity:** Bug (unimplemented rule)
- **Description:** Rule `TEMPLATE_EXTERNAL_REF` exists in `rules.c` with message "S10.3 Identifier in template body must be a parameter, @scratch wire, or compile-time constant; pass external signals as arguments", but the compiler never fires it. Template bodies can freely reference external wires, registers, and other module-scoped signals without passing them as parameters, violating S10.3 of the specification.
- **Reproduction:** Define a `@template` that references a module WIRE or REGISTER without passing it as a parameter — no diagnostic is produced.

## test_10_4-template_forbidden_content.md

### Parser does not recover after TEMPLATE_FORBIDDEN_BLOCK_HEADER
- **Severity:** Bug (parser recovery)
- **Description:** When the compiler detects a forbidden SYNCHRONOUS or ASYNCHRONOUS block header inside a template body, it correctly emits `TEMPLATE_FORBIDDEN_BLOCK_HEADER` but then fails to skip/recover past the block body `{ ... }`. The parser treats the `{` as a concatenation brace, producing cascading `PARSE000` errors for the block's content. Tests had to use the bare keyword without `{ ... }` body to avoid these cascading errors.
- **Reproduction:** `@template t(a, b) ASYNCHRONOUS { a = b; } @endtemplate` — produces TEMPLATE_FORBIDDEN_BLOCK_HEADER on the ASYNCHRONOUS keyword, then PARSE000 on `a = b` inside the block.

### Parser does not recover after TEMPLATE_FORBIDDEN_DIRECTIVE with body
- **Severity:** Bug (parser recovery)
- **Description:** When the compiler detects a forbidden structural directive (`@new`, `@module`, etc.) inside a template body, it correctly emits `TEMPLATE_FORBIDDEN_DIRECTIVE` but then fails to skip the directive's arguments/body. This produces cascading `PARSE000` errors. Tests had to use the bare directive keyword without arguments to avoid cascading errors.
- **Reproduction:** `@template t(a, b) @new inst SomeMod { IN [1] din = a; OUT [1] data = b; }; @endtemplate` — produces TEMPLATE_FORBIDDEN_DIRECTIVE then PARSE000.

### CDC directive not tested as TEMPLATE_FORBIDDEN_DIRECTIVE
- **Severity:** Test gap (parser recovery prevents testing)
- **Description:** The `CDC` keyword inside a template body should trigger `TEMPLATE_FORBIDDEN_DIRECTIVE` per S10.4, but could not be tested due to parser recovery issues. The bare `CDC` keyword without arguments may not be parsed correctly to trigger the check. This should be verified once the parser recovery issues above are fixed.

## test_10_5-template_application.md

### TEMPLATE_APPLY_OUTSIDE_BLOCK never fires
- **Severity:** Bug
- **Description:** The rule `TEMPLATE_APPLY_OUTSIDE_BLOCK` exists in `rules.c` but the compiler never produces this diagnostic. When `@apply` is used at module scope (outside any ASYNCHRONOUS or SYNCHRONOUS block), the compiler silently ignores it instead of reporting an error. The @apply statement has no effect and produces no output at all — no diagnostic, no expansion. Per S10.5, `@apply` may only appear inside ASYNCHRONOUS or SYNCHRONOUS blocks.
- **Reproduction:** Place `@apply simple(w1, din);` at module scope (between PORT and ASYNCHRONOUS blocks). Compiler produces zero output (exit code 0).

## test_10_6-template_exclusive_assignment.md

### Template expansion errors reported at template body, not callsite
- **Severity:** Observation (not necessarily a bug)
- **Description:** When two `@apply` calls to the same template target the same wire/register on the same execution path, the exclusive assignment error (`ASSIGN_MULTIPLE_SAME_BITS` or `SYNC_MULTI_ASSIGN_SAME_REG_BITS`) is reported at the template body location (the line inside the `@template` definition) rather than at the `@apply` callsite. This means if the same template is used in multiple modules with the same conflict, the errors are deduplicated to a single diagnostic at the template body location, since they share the same file:line:col.
- **Impact:** Users see the error pointing at the template definition rather than at the `@apply` call that caused the conflict, which may be confusing. Also, multiple independent violations are collapsed into a single diagnostic.

### Test plan rule ID does not match rules.c
- **Severity:** Documentation
- **Description:** The test plan (Section 10.6) uses `ASSIGN_MULTI_DRIVER` which does not exist in rules.c. The actual rules are `ASSIGN_MULTIPLE_SAME_BITS` (ASYNC) and `SYNC_MULTI_ASSIGN_SAME_REG_BITS` (SYNC).

## test_10_8-template_error_cases.md

Section 10.8 is a cross-reference summary of all template error cases from sections 10.2–10.6. All 15 rules in the test plan already have corresponding validation tests created under their respective sections, with one exception:

### TEMPLATE_APPLY_OUTSIDE_BLOCK — compiler bug (cross-ref from test_10_5)
- **Severity:** Bug (documented under test_10_5-template_application.md)
- **Description:** Rule exists in `rules.c` but is never emitted. `@apply` at module scope is silently ignored. No test can be created until the compiler implements this check.

## test_11_1-tristate_default_purpose.md

No issues. `WARN_INTERNAL_TRISTATE` fires correctly for internal tri-state nets (WIRE with z driver) and correctly suppresses for INOUT ports. `INFO_TRISTATE_TRANSFORM` cannot be tested via the validation framework as it requires `--tristate-default` CLI flag.

## test_11_2-tristate_default_applicability.md

No validation tests created. All scenarios in this test plan require the `--tristate-default` CLI flag, which the validation framework (`--info --lint`) does not support. The relevant rules (`INFO_TRISTATE_TRANSFORM`, `TRISTATE_TRANSFORM_UNUSED_DEFAULT`, `TRISTATE_TRANSFORM_SINGLE_DRIVER`) all require the transformation pass to be active. These should be tested via golden tests or a custom test script.

## test_11_3-tristate_net_identification.md

### NET_TRI_STATE_ALL_Z_READ never fires — ASYNC_FLOATING_Z_READ fires instead
- **Severity:** Possible dead rule
- **Description:** `NET_TRI_STATE_ALL_Z_READ` exists in rules.c (category `NET_DRIVERS_AND_TRI_STATE`) but never fires in any tested scenario. When all drivers assign `z` and the signal is read, `ASYNC_FLOATING_Z_READ` (category `ASYNC_BLOCK_RULES`) fires first. The test file `11_3_NET_TRI_STATE_ALL_Z_READ-all_drivers_z_but_read.jz` documents this behavior — it expects `ASYNC_FLOATING_Z_READ` in the `.out` file.
- **Impact:** The `NET_TRI_STATE_ALL_Z_READ` rule may be unreachable dead code. If it's intended to fire in a different context than ASYNC_FLOATING_Z_READ, that context is unknown.

### NET_MULTIPLE_ACTIVE_DRIVERS now fires (corrects previous assessment)
- **Severity:** Note (positive finding)
- **Description:** Previous test plans (test_1_2) documented that `NET_MULTIPLE_ACTIVE_DRIVERS` did not fire for two unconditional instance outputs bound to the same wire. The 11_3 test `11_3_NET_MULTIPLE_ACTIVE_DRIVERS-multi_driver_non_tristate.jz` demonstrates that this rule DOES fire correctly when two module instances have output ports bound to the same wire. The previous assessment may have been testing a different scenario (same-block multiple assignments, which triggers `ASSIGN_MULTIPLE_SAME_BITS` instead).

## test_11_4-tristate_transformation_algorithm.md

### TRISTATE_TRANSFORM_PER_BIT_FAIL not implemented
- **Severity:** Missing implementation
- **Description:** `TRISTATE_TRANSFORM_PER_BIT_FAIL` is defined in `rules.c` but is never emitted anywhere in the compiler source code. The rule is intended to fire when different bits of the same signal have different sets of tri-state drivers, but no code in `ir_tristate_transform.c` (or anywhere else) calls `jz_diagnostic_emit` with this rule ID. This means per-bit tri-state patterns are silently accepted or handled differently than the specification requires.
- **Impact:** Per-bit tri-state patterns (Section 11.4.2) are not validated. The compiler may produce incorrect transformations or silently ignore the pattern.

### All TRISTATE_TRANSFORM rules untestable via --lint validation
- **Severity:** Test framework limitation
- **Description:** All `TRISTATE_TRANSFORM_*` rules fire from the IR transformation pass (`ir_tristate_transform.c`), which only runs during backend code generation with `--tristate-default` flag. The validation test runner uses `--info --lint`, which does not invoke IR passes. None of the TRISTATE_TRANSFORM rules can be tested via the current validation test framework.
- **Impact:** These rules require a different testing approach (e.g., golden tests with `--tristate-default=GND` flag).

## From test_11_5-tristate_validation_rules.md

### TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL untestable via --lint
- **Severity:** Test framework limitation
- **Description:** `TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL` fires from `ir_tristate_transform.c` during the IR tristate transformation pass. This pass only runs when `--tristate-default` is specified with `--ir`, `--verilog`, or `--rtlil` modes. The `--info --lint` mode used by the validation test runner does NOT invoke the IR tristate transform pass (see `main.c` lines 655-671 — lint mode only runs `ir_build` and `div_guard_check`).
- **Impact:** Cannot be tested via the validation framework. Would need golden tests with `--tristate-default=GND` or similar backend flags.

### TRISTATE_TRANSFORM_PER_BIT_FAIL not implemented and untestable
- **Severity:** Missing implementation + test framework limitation
- **Description:** `TRISTATE_TRANSFORM_PER_BIT_FAIL` is defined in `rules.c` but no code in the compiler emits this diagnostic (already documented in test_11_4 section). Additionally, even if implemented, it would require `--tristate-default` with backend modes, making it incompatible with the `--info --lint` validation framework.
- **Impact:** Both rules in the test_11_5 plan are untestable. Neither new validation test files nor happy-path tests can be created for this section.

## From test_11_6-tristate_inout_handling.md

### TRISTATE_TRANSFORM_OE_EXTRACT_FAIL untestable via --lint
- **Severity:** Test framework limitation
- **Description:** `TRISTATE_TRANSFORM_OE_EXTRACT_FAIL` fires from `ir_tristate_transform.c` during the IR tristate transformation pass. This pass only runs when `--tristate-default` is specified with `--ir`, `--verilog`, or `--rtlil` modes. The `--info --lint` mode used by the validation test runner does NOT invoke the IR tristate transform pass.
- **Impact:** The only rule in the test_11_6 plan (`TRISTATE_TRANSFORM_OE_EXTRACT_FAIL`) cannot be tested via the validation framework. It would need golden tests that use `--tristate-default=GND` or similar backend flags.

## From test_11_7-tristate_error_conditions.md

### All 6 TRISTATE_TRANSFORM rules untestable via --lint
- **Severity:** Test framework limitation
- **Description:** All 6 rules in the test_11_7 plan fire from `ir_tristate_transform.c` during the IR tristate transformation pass: `TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL`, `TRISTATE_TRANSFORM_PER_BIT_FAIL`, `TRISTATE_TRANSFORM_BLACKBOX_PORT`, `TRISTATE_TRANSFORM_OE_EXTRACT_FAIL`, `TRISTATE_TRANSFORM_SINGLE_DRIVER`, `TRISTATE_TRANSFORM_UNUSED_DEFAULT`. This pass only runs when `--tristate-default` is specified with `--ir`, `--verilog`, or `--rtlil` modes. The `--info --lint` mode used by the validation test runner does NOT invoke the IR tristate transform pass.
- **Impact:** None of the 6 rules can be tested via the validation framework. They would need golden tests that use `--tristate-default=GND` or `--tristate-default=VCC` backend flags.

## From test_11_8-tristate_portability.md

### No testable rules — entire section is portability guidelines
- **Severity:** Test framework limitation
- **Description:** Section 11.8 describes portability best practices (explicit guards, limiting tri-state to I/O boundaries, testing both modes). All scenarios in the test plan are either happy-path (no diagnostics) or require the `--tristate-default` flag which is not supported by the `--info --lint` validation framework. The only relevant diagnostic rule (`WARN_INTERNAL_TRISTATE`) is already tested under section 11.1. No new validation tests can be created for this section.
- **Impact:** Zero tests generated. Coverage for this section's concepts is provided by existing tests in sections 11.1 (WARN_INTERNAL_TRISTATE) and 11.7 (TRISTATE_TRANSFORM rules, also untestable via validation).

## test_12_2-combinational_loop_errors.md

### No issues
- All rules from the test plan (`COMB_LOOP_UNCONDITIONAL`, `COMB_LOOP_CONDITIONAL_SAFE`) exist in `rules.c` and are tested. Tests cover two-signal loops (section 5_3), three/four-signal loops, conditional-on-same-path loops, and mutually-exclusive-branch warnings.

## test_12_3-recommended_warnings.md

### WARN_INCOMPLETE_SELECT_ASYNC not implemented
- **Severity:** Missing implementation
- **Description:** Rule `WARN_INCOMPLETE_SELECT_ASYNC` is defined in `rules.c` (line 467) with message "S5.4/S8.3 Incomplete SELECT coverage without DEFAULT in ASYNCHRONOUS block" but `sem_report_rule` is never called with this rule ID anywhere in the compiler source code. The rule cannot fire.

### WARN_UNUSED_WIRE not implemented
- **Severity:** Missing implementation
- **Description:** Rule `WARN_UNUSED_WIRE` is defined in `rules.c` (line 470) with message "S12.3 WIRE declared but never driven or read; remove it if unused" but `sem_report_rule` is never called with this rule ID anywhere in the compiler source code. Unused wires are currently caught by `NET_DANGLING_UNUSED` instead.

### WARN_UNUSED_PORT not implemented
- **Severity:** Missing implementation
- **Description:** Rule `WARN_UNUSED_PORT` is defined in `rules.c` (line 471) with message "S12.3 PORT declared but never used; remove it if unused" but `sem_report_rule` is never called with this rule ID anywhere in the compiler source code. The rule cannot fire.

## test_12_4-path_security.md

### Import failure aborts compilation before reaching MEM @file() validation
- **Severity:** Limitation (not a bug)
- **Description:** When an `@import` directive fails path validation (e.g., PATH_ABSOLUTE_FORBIDDEN or PATH_TRAVERSAL_FORBIDDEN), the compiler aborts compilation immediately. This prevents testing both `@import` and `@file()` path violations in a single test file — each context must use a separate test file. This is expected behavior (fail-fast on import errors) but limits multi-trigger test design.

### PATH_OUTSIDE_SANDBOX and PATH_SYMLINK_ESCAPE cannot be tested in validation framework
- **Severity:** Test gap
- **Description:** PATH_OUTSIDE_SANDBOX requires a real file outside the sandbox root, and PATH_SYMLINK_ESCAPE requires an actual symlink escaping the sandbox. The validation test framework only supports `.jz`/`.out` file pairs and cannot set up filesystem state. These rules would need shell-level integration tests with temporary directories and symlinks.

## test_10_1-template_purpose.md

No issues. Section 10.1 is a conceptual overview with no rule IDs. Happy-path test created successfully.

## test_11_2-tristate_default_applicability.md

No issues. Section 11.2 defines the scope of applicability for `--tristate-default` (internal WIREs vs INOUT ports/top-level pins). No diagnostic rule IDs are defined — no validation tests to create.

## test_11_4-tristate_transformation_algorithm.md

No issues. All four rules (TRISTATE_TRANSFORM_PER_BIT_FAIL, TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL, TRISTATE_TRANSFORM_SINGLE_DRIVER, INFO_TRISTATE_TRANSFORM) require the `--tristate-default` flag and are not testable via `--info --lint`. No validation tests to create.

## test_11_6-tristate_inout_handling.md

No issues. Both rules (TRISTATE_TRANSFORM_OE_EXTRACT_FAIL, TRISTATE_TRANSFORM_BLACKBOX_PORT) require the `--tristate-default` flag and are not testable via `--info --lint`. No validation tests to create.

## test_11_7-tristate_error_conditions.md

No issues. All six rules (TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL, TRISTATE_TRANSFORM_PER_BIT_FAIL, TRISTATE_TRANSFORM_BLACKBOX_PORT, TRISTATE_TRANSFORM_OE_EXTRACT_FAIL, TRISTATE_TRANSFORM_SINGLE_DRIVER, TRISTATE_TRANSFORM_UNUSED_DEFAULT) require the `--tristate-default` flag and are not testable via `--info --lint`. No validation tests to create.

## test_11_2-tristate_default_applicability.md

No issues. Section 11.2 defines the scope of applicability for `--tristate-default` (applies only to internal WIREs, not INOUT ports or top-level pins). No diagnostic rules are defined for this subsection — no validation tests to create.

## test_11_8-tristate_portability.md

No issues. Section 11.8 is a best practices section for portable tri-state designs. It cross-references WARN_INTERNAL_TRISTATE (from 11.1) and TRISTATE_TRANSFORM_UNUSED_DEFAULT (from 11.7) but defines no new diagnostic rules. No validation tests to create.

## test_12_1-compile_errors.md

No issues. Section 12.1 is an aggregation/summary section — individual error rules are tested in their respective section test plans (1.x through 11.x). Two aggregation tests were created: a happy-path test verifying a valid program produces no diagnostics, and a multiple-errors test verifying that errors from different categories (KEYWORD_AS_IDENTIFIER, REG_INIT_WIDTH_MISMATCH, SLICE_MSB_LESS_THAN_LSB, LIT_OVERFLOW) are all reported in a single file. Edge case 2 ("error in imported file") is not applicable — JZ-HDL uses `@new` instantiation, not file imports, and errors in instantiated modules are already tested across section test plans.

## test_12_3-recommended_warnings.md

### WARN_UNDRIVEN_REGISTER does not detect registers read only in ASYNCHRONOUS blocks
- **Severity:** Limitation
- **Description:** WARN_UNDRIVEN_REGISTER ("register is read but never written") only fires when the register is read in a SYNCHRONOUS block. If a register is read only in an ASYNCHRONOUS block (e.g., `data = frozen;`), the flow analysis does not record it as a sink, so the rule does not fire. The register is silently accepted with no warning.
- **Reproduction:** `REGISTER { frozen [1] = 1'b1; }` with `ASYNCHRONOUS { data = frozen; }` and no SYNCHRONOUS write — no diagnostic produced.

### Three rule IDs defined but never emitted
- **Severity:** Gap
- **Description:** Three rule IDs from the test plan are defined in rules.c but never referenced in any semantic analysis pass:
  - `WARN_INCOMPLETE_SELECT_ASYNC` — Equivalent behavior covered by `SELECT_DEFAULT_RECOMMENDED_ASYNC`
  - `WARN_UNUSED_WIRE` — Unused wires caught by `NET_DANGLING_UNUSED` instead
  - `WARN_UNUSED_PORT` — No equivalent diagnostic fires for unused ports

## test_3_3-operator_precedence.md

### No diagnostic rules to test
- **Severity:** N/A (by design)
- **Description:** Section 3.3 defines the 15-level operator precedence hierarchy. This is purely parser behavior verified through AST structure, not through diagnostic rules. No rule IDs exist in rules.c for operator precedence. The validation test framework (`--info --lint`) cannot verify precedence correctness. Precedence testing would require AST-level comparison or simulation-based tests, which are outside the scope of the validation suite.
