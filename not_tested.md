# Rules Not Tested

## From test_1_1-identifiers.md

### Rules missing from rules.c (untestable)
- **ID_STARTS_WITH_DIGIT** — No explicit rule ID for digit-leading identifiers; handled by lexer as non-identifier token. Cannot produce a diagnostic with a rule ID.
- **ID_NON_ASCII** — No explicit rule ID for non-ASCII characters in identifiers; handled by lexer. Cannot produce a diagnostic with a rule ID.

### Contexts not covered due to compiler bugs
- **KEYWORD_AS_IDENTIFIER in WIRE block** — Keywords are silently accepted as wire names (see issues.md).
- **KEYWORD_AS_IDENTIFIER in REGISTER block (some keywords)** — SELECT, WIRE, PORT produce PARSE000 instead of KEYWORD_AS_IDENTIFIER (see issues.md).
- **ID_SINGLE_UNDERSCORE in WIRE block** — Single underscore accepted as wire name (see issues.md).
- **ID_SYNTAX_INVALID in WIRE block** — 256-char identifiers accepted as wire names (see issues.md).
- **KEYWORD_AS_IDENTIFIER for VCC/GND** — Produce PARSE000 instead of KEYWORD_AS_IDENTIFIER (see issues.md).
- **KEYWORD_AS_IDENTIFIER for most reserved identifiers** — PLL, DLL, CLKDIV, BIT, BUS, FIFO, HANDSHAKE, PULSE, MCP, RAW, BLOCK, DISTRIBUTED, ASYNC, SYNC, WRITE_FIRST, READ_FIRST, NO_CHANGE are not enforced as keywords (see issues.md).

## From test_1_2-fundamental_terms.md

### Rules missing from rules.c (untestable)
- **X_OBSERVABLE_SINK** — Test plan lists this as missing, but it exists as OBS_X_TO_OBSERVABLE_SINK. Not a gap.
- **NET_NON_DETERMINISTIC** — No explicit rule ID for non-deterministic net detection as a separate check. Covered implicitly by NET_MULTIPLE_ACTIVE_DRIVERS and NET_FLOATING_WITH_SINK.

### Rules not firing due to compiler bugs
- **NET_MULTIPLE_ACTIVE_DRIVERS** — Now tested in 11_3. Two unconditional instance outputs bound to the same wire DO trigger this rule (previous assessment corrected).
- **NET_TRI_STATE_ALL_Z_READ** — This rule exists in rules.c but never fires in practice. The same scenario is caught earlier by ASYNC_FLOATING_Z_READ (from the ASYNC_BLOCK_RULES category). Tests use ASYNC_FLOATING_Z_READ instead.

## From test_1_3-bit_slicing_and_indexing.md

### Rules missing from rules.c (untestable)
- **SLICE_NEGATIVE_INDEX** — No explicit rule ID for negative indices in slices; the parser does not accept `-` in slice index syntax. Negative values via CONST are caught by CONST_NEGATIVE_OR_NONINT before reaching slice validation.

### Rules not firing as expected
- **SLICE_INDEX_INVALID** — This rule exists in rules.c but could not be triggered in any tested scenario. Undefined names in slice positions fire UNDECLARED_IDENTIFIER instead. Declared non-CONST names (wires, registers, ports) in slice positions fire CONST_UNDEFINED_IN_WIDTH_OR_SLICE instead. The specific conditions that trigger SLICE_INDEX_INVALID remain unknown.

## From test_1_4-comments.md

### Rules missing from rules.c (untestable)
- **COMMENT_UNTERMINATED** — No explicit rule ID for unterminated block comments (`/* no end`); handled by the lexer as a generic error. Cannot produce a diagnostic with a rule ID.

## From test_1_5-exclusive_assignment_rule.md

### Rules missing from rules.c (untestable)
- **ASSIGN_UNREACHABLE_DEAD_WRITE** — No matching rule ID in rules.c. The test plan lists this for dead writes (`w <= a; w <= b;` where the first is unreachable), but in practice this scenario fires ASSIGN_MULTIPLE_SAME_BITS instead. WARN_DEAD_CODE_UNREACHABLE exists but covers unreachable statements, not dead writes specifically.
- **ASSIGN_OVERLAPPING_SLICES** — No distinct rule ID; covered by ASSIGN_SLICE_OVERLAP (tested).
- **ASSIGN_REG_IN_ASYNC** — No distinct rule ID; covered by ASYNC_ASSIGN_REGISTER (in ASYNC_BLOCK_RULES category, not section 1.5).
- **ASSIGN_WIRE_IN_SYNC** — No distinct rule ID; covered by WRITE_WIRE_IN_SYNC (in PORT_WIRE_REGISTER_DECLS category, not section 1.5).

### Contexts not covered
- **ASSIGN_MULTIPLE_SAME_BITS in wire context** — Double-assigning a wire with `=` at root of ASYNC triggers the rule, but reading the wire afterward cascades into NET_FLOATING_WITH_SINK. Test covers port double-assignment only. Wire double-assignment triggers the rule but produces unrelated cascading errors.

## From test_1_6-high_impedance_and_tristate.md

### Rules missing from rules.c (untestable)
- **LIT_HEX_HAS_XZ** — No explicit rule ID for z in hex literals. The compiler fires the generic `LIT_INVALID_DIGIT_FOR_BASE` instead. Test uses LIT_INVALID_DIGIT_FOR_BASE as the actual rule.
- **REG_Z_ASSIGN** — No explicit rule for z assigned as next-state to register in SYNCHRONOUS block (S1.6.6). The compiler only produces `WARN_INTERNAL_TRISTATE` (a warning), not an error. The spec says "Registers cannot store z" but the compiler does not enforce this for next-state assignments.
- **LATCH_Z_ASSIGN** — No explicit rule for z assigned to latch (S1.6.6). Same gap as REG_Z_ASSIGN.
- **TRISTATE_Z_OBSERVABILITY** — No explicit rule for z bits reaching observable sinks (S1.6.7). May be partially covered by general observability checks but no dedicated rule exists.
- **TRISTATE_HIERARCHY_UNPROVEN** — No explicit rule for hierarchical proof failure vs. single-module proof (S1.6.4).

### Rules not firing due to compiler bugs
- **NET_MULTIPLE_ACTIVE_DRIVERS** — Already documented in test_1_2 section. Two unconditional instance outputs bound to the same wire do not trigger this rule. This also prevents testing multi-driver tri-state scenarios (S1.6.4).
- **NET_TRI_STATE_ALL_Z_READ** — Already documented in test_1_2 section. Caught earlier by ASYNC_FLOATING_Z_READ.

## From test_2_3-bit_width_constraints.md

### Rules missing from rules.c (untestable)
- **WIDTH_SHIFT_RULES** — No rule ID in rules.c. The test plan notes shift operators have different width rules per S3.2 and should be exempt from strict matching. No dedicated rule exists to verify this exemption.
- **WIDTH_MULTIPLY_RULES** — No rule ID in rules.c. The test plan notes multiply/divide/modulus have specialized width rules per S3.2. In practice, these operators DO enforce equal widths via TYPE_BINOP_WIDTH_MISMATCH (tested), so the "exemption" described in the test plan does not exist in the compiler.

### Rules suppressed by priority system
- **WIDTH_ASSIGN_MISMATCH_NO_EXT** — This rule exists in rules.c (priority 0) and fires on every assignment width mismatch. However, ASSIGN_WIDTH_NO_MODIFIER (priority 1) always co-fires on the same line and the diagnostic priority system suppresses the lower-priority rule. WIDTH_ASSIGN_MISMATCH_NO_EXT never appears in compiler output. Tests use ASSIGN_WIDTH_NO_MODIFIER instead.

## From test_2_4-special_semantic_drivers.md

### Rules missing from rules.c (untestable)
- **SEMANTIC_DRIVER_IN_TERNARY** — Test plan lists this as missing. GND/VCC in ternary condition is caught by SPECIAL_DRIVER_IN_EXPRESSION (tested). GND/VCC in ternary branches is valid (polymorphic expansion). No separate rule needed.
- **SEMANTIC_DRIVER_AS_CONDITION** — Test plan lists this as missing. Same as above — caught by SPECIAL_DRIVER_IN_EXPRESSION when GND/VCC is used as ternary or IF condition.
- **ASSIGN_MULTI_DRIVER** — Test plan uses this name but actual rule is NET_MULTIPLE_ACTIVE_DRIVERS. Already documented as non-firing in test_1_2 section.

### Rules not testable due to parser restrictions
- **SPECIAL_DRIVER_IN_INDEX** — This rule exists in rules.c but cannot be triggered through user syntax. The parser (`parser_expressions.c:1064-1074`) only accepts identifiers, numbers, sized numbers, config, and operators in slice/index positions — GND/VCC keywords produce PARSE000 ("expected expression in index") before reaching semantic analysis. The SPECIAL_DRIVER_IN_INDEX check in `driver_assign.c` is dead code for user-facing syntax.

## From test_3_1-operator_categories.md

### Rules missing from rules.c (untestable)
- **LOGICAL_OP_MULTI_BIT** — Test plan lists this as a missing rule, but it already exists as LOGICAL_WIDTH_NOT_1 (tested).

### Happy-path and edge-case scenarios (not testable in lint framework)
- All scenarios in sections 3.1 (happy path) and 3.2 (boundary/edge cases) verify correct result widths, which are behavioral/simulation-level checks, not lint diagnostics.
- Unknown operator rejection (Neg 3) is handled by the lexer and does not produce a rule ID.

### Rules already tested in other sections
- **TYPE_BINOP_WIDTH_MISMATCH** — Already tested in test_2_3 (2_3_TYPE_BINOP_WIDTH_MISMATCH-mismatched_operand_widths).

## From test_10_2-template_definition.md

All rules from this test plan are tested. No missing rules, no untestable scenarios.

## From test_2_1-literals.md

### Rules missing from rules.c (untestable)
- **LIT_HEX_HAS_XZ** — No distinct rule ID; hex with x/z fires `LIT_INVALID_DIGIT_FOR_BASE` (tested).
- **LIT_MEM_INIT_HAS_XZ** — Test plan mentions potential need for separate rule for x/z in MEM init vs register reset. No rule ID exists in rules.c.

### Contexts not covered due to compiler bugs
- **LIT_OVERFLOW in register init** — The LIT_OVERFLOW rule does not fire for overflowing literals used as register initialization values (e.g., `r [4] = 4'd16;`). It fires correctly in SYNCHRONOUS expressions. See issues.md.

### Rules already tested in other sections
- **REG_INIT_CONTAINS_X** — Already tested in test_1_2 (1_2_REG_INIT_CONTAINS_X-x_in_register_init).
- **REG_INIT_CONTAINS_Z** — Already tested in test_1_6 (1_6_REG_INIT_CONTAINS_Z-z_in_register_init).

### Happy-path and edge-case scenarios (not testable in lint framework)
- All scenarios in sections 3.1 (happy path) and 3.2 (boundary/edge cases) verify correct literal parsing and extension, which are behavioral/simulation-level checks, not lint diagnostics.

## From test_2_2-signedness_model.md

### No testable rules — entire section is behavioral
- **SIGNED_KEYWORD_USED** — No rule ID in rules.c. The spec (S2.2) says there is no signed type, but there is no compiler rule to reject a hypothetical `signed` keyword. If used, it would be caught by KEYWORD_AS_IDENTIFIER or the lexer, not a signedness-specific rule.
- **SIGNED_OP_WITHOUT_INTRINSIC** — No rule ID in rules.c. No warning exists for when a user might intend signed comparison but uses an unsigned operator.
- All other scenarios in the test plan are happy-path (unsigned arithmetic correctness) or simulation-level tests, which are not testable in the lint validation framework.

## From test_3_2-operator_definitions.md

### Test plan rule IDs do not match rules.c
- **UNARY_NOT_PARENTHESIZED** → actual: `UNARY_ARITH_MISSING_PARENS` (tested).
- **EXPR_DIVISION_BY_ZERO** → actual: `DIV_CONST_ZERO` (tested).
- **WIDTH_TERNARY_MISMATCH** → does not exist; closest is `TERNARY_BRANCH_WIDTH_MISMATCH` (tested) and `TERNARY_COND_WIDTH_NOT_1` (tested).

### Rules listed as missing in test plan but actually exist
- **TERNARY_COND_NOT_1BIT** → exists as `TERNARY_COND_WIDTH_NOT_1` (tested).
- **LOGICAL_OP_NOT_1BIT** → exists as `LOGICAL_WIDTH_NOT_1` (tested).
- **CONCAT_EMPTY** → exists in rules.c (tested).

### Rules also tested in other sections
- **OBS_X_TO_OBSERVABLE_SINK** — Tested in test_1_2 (1_2_OBS_X_TO_OBSERVABLE_SINK-x_bits_to_observable) for direct x-literal assignments, and now also in 3_2_OBS_X_TO_OBSERVABLE_SINK-x_bits_in_expressions for x bits within operator expressions (addition, bitwise OR, ternary, concatenation, subtraction).

### Contexts limited by ASYNC block rules
- **DIV_CONST_ZERO in ASYNC blocks** — Division by constant zero uses literals which trigger ASYNC_ALIAS_LITERAL_RHS when used in ASYNC `=` assignments. Tests use SYNCHRONOUS blocks only. The rule is still fully covered since the operator check is context-independent.
- **DIV_UNGUARDED_RUNTIME_ZERO in ASYNC blocks** — ASYNC IF blocks trigger ASYNC_ALIAS_IN_CONDITIONAL, and the DIV_UNGUARDED IR pass only runs when no errors exist. Tests use SYNCHRONOUS blocks only.

### Happy-path and edge-case scenarios (not testable in lint framework)
- Multiply 2N result width, shift fill behavior, comparison result width, concatenation MSB ordering — all behavioral/simulation-level checks, not lint diagnostics.

## From test_3_3-operator_precedence.md

### No testable rules — entire section is structural/behavioral
- **PRECEDENCE_AMBIGUOUS** — Listed as missing in the test plan (Section 6.2). No rule ID exists in rules.c. No warning is emitted for expressions mixing operators from different precedence levels without parentheses.
- All happy-path scenarios (3.1, 3.2) verify AST operator binding order (e.g., `a + b * c` parses as `a + (b * c)`), which is structural parser behavior, not lint diagnostics.
- Negative scenarios (missing operand, double operator, mismatched parens) are parse-level errors handled by the parser without specific rule IDs from rules.c.

## From test_3_4-operator_examples.md

### No new testable rules — section is examples only
- Section 3.4 provides canonical operator usage examples (unary negation, shifts, carry capture, ternary+concat, tri-state). No new rules are introduced beyond those already tested from S3.1-S3.2.
- **UNARY_NOT_PARENTHESIZED** → actual `UNARY_ARITH_MISSING_PARENS`, already tested in test_3_2 (3_2_UNARY_ARITH_MISSING_PARENS-unparenthesized_unary).
- **WIDTH_TERNARY_MISMATCH** → actual `TERNARY_BRANCH_WIDTH_MISMATCH`, already tested in test_2_3 and test_3_2.

### Happy-path lint test created
- `3_4_OPERATOR_EXAMPLES-spec_examples_ok` — validates all S3.4 canonical examples (unary negation, multi-bit negation via subtraction, logical/arithmetic right shift, carry capture via concatenation, ternary+concat, tri-state driver on INOUT) compile without false-positive diagnostics.

### Edge-case scenarios (not testable in lint framework)
- Boundary/edge cases (negate zero, negate max, shift by zero, carry=0/carry=1) verify runtime expression evaluation (two's complement, shift fill, carry capture), which are behavioral/simulation-level checks, not lint diagnostics.

## From test_4_1-module_canonical_form.md

### Rules missing from rules.c (untestable)
- **MODULE_NO_PORT** — Test plan lists this as a missing rule (Section 6.2), but it already exists as `MODULE_MISSING_PORT` (tested).
- **MODULE_ONLY_IN_PORTS** — Test plan lists this as a missing rule (Section 6.2), but it already exists as `MODULE_PORT_IN_ONLY` (tested).

### Happy-path and edge-case scenarios (not testable in lint framework)
- Full canonical form (all sections present), minimal module, module without CONST/REGISTER/WIRE, multiple SYNCHRONOUS blocks with different clocks, SYNCHRONOUS header variants — these verify correct parsing and AST construction, not lint diagnostics.
- Empty ASYNC/SYNC bodies, module with only IN ports (boundary cases) — structural parsing scenarios that don't produce rule-ID diagnostics beyond MODULE_PORT_IN_ONLY (tested).

### Parse-level errors without rule IDs
- **Missing @endmod** — Produces a generic parse error ("missing @endmod for module"), not a rule-ID diagnostic. Cannot be tested in the validation framework.

### Inherent co-firing limitations
- **MODULE_MISSING_PORT** always co-fires with **WARN_UNUSED_MODULE** — A module with no ports cannot be instantiated, so it is always flagged as unused.
- **MODULE_PORT_IN_ONLY** always co-fires with **NET_DANGLING_UNUSED** — A module with only IN ports has no way to sink the input signals.

## From test_4_2-scope_and_uniqueness.md

### Rules missing from rules.c (untestable)
- **MODULE_SELF_INSTANTIATION** — No rule ID exists in rules.c for a module instantiating itself. The test plan lists this (Neg 7) but it cannot be tested.
- **CONST_CROSS_MODULE_ACCESS** — No explicit rule for accessing another module's CONST (listed as missing in test plan Section 6.2). Would be caught by UNDECLARED_IDENTIFIER.

### Test plan rule IDs do not match rules.c
- `MODULE_DUPLICATE_NAME` → actual: `MODULE_NAME_DUP_IN_PROJECT`
- `SCOPE_DUPLICATE_SIGNAL` → actual: `ID_DUP_IN_MODULE`, `INSTANCE_NAME_DUP_IN_MODULE`, or `INSTANCE_NAME_CONFLICT` depending on context

### Inherent co-firing limitations
- **BLACKBOX_NAME_DUP_IN_PROJECT** with module-blackbox name conflict inherently co-fires with **WARN_UNUSED_MODULE** — The conflicting module cannot be instantiated (name collision), so it is always flagged as unused.

### Rules not implemented in compiler
- **AMBIGUOUS_REFERENCE** — Rule ID exists in rules.c but is not implemented in any semantic pass. No code in `src/sem/` emits this diagnostic. Cannot be tested until the compiler implements it.

## From test_4_3-const.md

### Rules missing from rules.c (untestable)
- **CONST_FORWARD_REF** — No rule ID exists in rules.c for CONST forward references within a module. Only `CONFIG_FORWARD_REF` (for CONFIG) and `GLOBAL_FORWARD_REF` (for @global) exist. CONST forward references produce `CONST_NEGATIVE_OR_NONINT` instead.

### Rules not firing due to compiler behavior
- **CONST_CIRCULAR_DEP** — This rule exists in rules.c and has detection code in `const_eval.c`, but it never fires for module-level CONST blocks. The driver suppresses all internal const_eval diagnostics (`opts.diagnostics = NULL`) and maps any evaluation failure to `CONST_NEGATIVE_OR_NONINT`. Circular CONST dependencies produce `CONST_NEGATIVE_OR_NONINT` on each participating CONST declaration, not `CONST_CIRCULAR_DEP`. See issues.md.

### Test plan rule IDs do not match rules.c
- `CONST_DUPLICATE` → actual: `ID_DUP_IN_MODULE` (already tested in test_4_2)
- `CONST_CIRCULAR_DEP` → fires as `CONST_NEGATIVE_OR_NONINT` for module CONSTs
- `CONST_FORWARD_REF` → no module CONST forward ref rule; fires as `CONST_NEGATIVE_OR_NONINT`
- `CONST_NEGATIVE_WIDTH` → actual: `CONST_NEGATIVE_OR_NONINT`

### Inherent co-firing limitations
- **CONST_STRING_IN_NUMERIC_CONTEXT** on wire width inherently co-fires with **NET_DANGLING_UNUSED** — A wire with invalid width (string CONST) evaluates to width 0 and is flagged as dangling.

## From test_4_4-port.md

### Rules missing from rules.c (untestable)
- **PORT_OUTPUT_NOT_DRIVEN** — No rule ID in rules.c for OUT port never driven. WARN_UNCONNECTED_OUTPUT exists but covers unconnected output at instantiation site, not undriven output within a module.
- **PORT_INOUT_NOT_BIDIRECTIONAL** — No rule ID in rules.c for INOUT port not used bidirectionally.
- **PORT_MISSING_WIDTH** — Parse error, no semantic rule ID. The parser rejects `IN data;` before reaching semantic analysis.
- **PORT_BLOCK_EMPTY** — Covered by `MODULE_MISSING_PORT` (already tested in test_4_1). An empty PORT block fires MODULE_MISSING_PORT.
- **BUS_SIGNAL_NOT_FOUND** — Test plan lists this as missing (Section 6.2), but it exists as `BUS_SIGNAL_UNDEFINED` (tested).

### Test plan rule IDs do not match rules.c
- `PORT_INPUT_DRIVEN_INSIDE` → actual: `PORT_DIRECTION_MISMATCH_IN`
- `PORT_DIRECTION_VIOLATION` → actual: `PORT_DIRECTION_MISMATCH_OUT`
- `BUS_PORT_DIRECTION_VIOLATION` → actual: `BUS_SIGNAL_READ_FROM_WRITABLE` and `BUS_SIGNAL_WRITE_TO_READABLE` (two separate rules)
- `BUS_INDEX_OUT_OF_RANGE` → actual: `BUS_PORT_INDEX_OUT_OF_RANGE`
- `ALIAS_IN_CONDITIONAL` → actual: `ASYNC_ALIAS_IN_CONDITIONAL`

### Rules not firing due to compiler bugs
- **BUS_PORT_UNKNOWN_BUS** — Rule exists in rules.c and semantic check code exists in `driver_width.c:768-773`, but never fires. Declaring `BUS FAKE_BUS SOURCE fbus;` where `FAKE_BUS` is not defined in the project produces only `NET_DANGLING_UNUSED`, not `BUS_PORT_UNKNOWN_BUS`. See issues.md.
- **BUS_PORT_ARRAY_COUNT_INVALID** — Rule exists in rules.c and check code exists in `driver_width.c:786-797`, but never fires. Using `BUS MY_BUS SOURCE [UNDEF] abus;` or `BUS MY_BUS SOURCE [0] abus;` produces only `NET_DANGLING_UNUSED`, not `BUS_PORT_ARRAY_COUNT_INVALID`. See issues.md.
- **BUS_PORT_NOT_BUS** — Rule exists in rules.c and check code exists in `driver.c:527-537`, but never fires for user-facing syntax. Dot-access on non-BUS ports (e.g., `din.tx`) fires `UNDECLARED_IDENTIFIER` instead. The parser creates `QualifiedIdentifier` nodes for `name.member`, and the semantic lookup fails to find the port despite it being in scope. See issues.md.

### Contexts not covered due to compiler behavior
- **BUS_SIGNAL_UNDEFINED in write (LHS) context on scalar BUS** — Writing to a nonexistent signal on a scalar BUS port fires ASYNC_INVALID_STATEMENT_TARGET instead of BUS_SIGNAL_UNDEFINED. The rule fires correctly for arrayed BUS writes and for read (RHS) contexts. Tests use read contexts and arrayed BUS writes.
- **ASYNC_ALIAS_IN_CONDITIONAL in SELECT case** — Alias `=` inside a SELECT case fires ASYNC_ALIAS_IN_CONDITIONAL but also co-fires COMB_LOOP_UNCONDITIONAL when the same target has `<=` on other cases. This is inherent to mixing alias and receive-assign on different conditional paths within SELECT. Tests use IF branches only.
- **ASYNC_ALIAS_IN_CONDITIONAL in nested IF** — Similar to SELECT: alias in nested IF co-fires COMB_LOOP_UNCONDITIONAL even with different conditions on outer and inner IF. Tests use single-level IF branches only.
- **PORT_DIRECTION_MISMATCH_OUT in ASYNC with alias** — Reading OUT port via alias (`wire = out_port`) would also test this rule, but alias-based reads were not tested to avoid complex net-merge side effects.

## From test_4_5-wire.md

### Rules missing from rules.c (untestable)
- **WIRE_MULTI_DIMENSIONAL** — No explicit rule ID for multi-dimensional wire syntax; handled by the parser as a parse error. Cannot produce a diagnostic with a rule ID.

### Rules not firing due to compiler behavior
- **WARN_UNUSED_WIRE** — This rule exists in rules.c but is never emitted by any semantic analysis code. Completely unused wires (neither driven nor read) fire `NET_DANGLING_UNUSED` instead. The WARN_UNUSED_WIRE rule appears to be dead code.
- **NET_MULTIPLE_ACTIVE_DRIVERS for wires** — Already documented in test_1_2. Two unconditional drivers on the same wire do not trigger this rule.

### Test plan rule IDs do not match rules.c
- `ASSIGN_OP_WRONG_BLOCK` → actual: `WRITE_WIRE_IN_SYNC` (tested)
- `ASSIGN_MULTI_DRIVER` → actual: `NET_MULTIPLE_ACTIVE_DRIVERS` (doesn't fire; see test_1_2 issues)

## From test_4_6-mux.md

### Rules missing from rules.c (untestable)
- **MUX_ELEMENT_WIDTH_ZERO** — No rule ID exists in rules.c for element width = 0 in auto-slicing form. The test plan (Section 6.2) lists this as a missing rule.
- **MUX_SELECTOR_TOO_NARROW** — No rule ID exists in rules.c for a selector narrower than clog2(N). The spec says narrower selectors are implicitly zero-extended. The test plan (Section 6.2) lists this as a potential info/warning.

### Test plan rule IDs do not match rules.c
- `MUX_ASSIGN_READONLY` → actual: `MUX_ASSIGN_LHS` (tested)
- `MUX_WIDTH_MISMATCH` → actual: `MUX_AGG_SOURCE_WIDTH_MISMATCH` (tested)
- `MUX_SLICE_NOT_DIVISIBLE` → actual: `MUX_SLICE_WIDTH_NOT_DIVISOR` (tested)
- `MUX_INDEX_OUT_OF_RANGE` → actual: `MUX_SELECTOR_OUT_OF_RANGE_CONST` (tested)
- `SCOPE_DUPLICATE_SIGNAL` → actual: `MUX_NAME_DUPLICATE` (tested)

### Contexts not covered due to compiler limitations
- **MUX_SELECTOR_OUT_OF_RANGE_CONST with sized literals** — The check in `driver.c:2290-2295` uses `parse_simple_nonnegative_int()` which only handles bare integers, not sized literals like `2'd3`. Tests use bare integer selectors. Sized literal selectors silently pass the check (see issues.md).
- **MUX_ASSIGN_LHS in SYNCHRONOUS blocks** — Not tested because assigning to a MUX in SYNC would likely co-fire `ASSIGN_TO_NON_REGISTER_IN_SYNC`. Tests use ASYNCHRONOUS blocks only.

## From test_4_7-register.md

### Rules not firing due to parser catching first
- **REG_MULTI_DIMENSIONAL** — This rule exists in rules.c ("S4.7 REGISTER declared with multi-dimensional syntax") but the parser produces PARSE000 ("expected '=' and initialization literal in REGISTER block") before semantic analysis runs. The parser rejects `r [8] [4] = 8'h00;` at the second `[` token. Cannot be tested in the validation framework.
- **REG_MISSING_INIT_LITERAL** — This rule exists in rules.c ("S4.7 Register declared without mandatory reset/power-on literal") but the parser produces PARSE000 ("expected '=' and initialization literal in REGISTER block") before semantic analysis runs. The parser rejects `r [8];` at the `;` token. Cannot be tested in the validation framework.

### Rules already tested in other sections
- **REG_INIT_CONTAINS_X** — Already tested in test_1_2 (1_2_REG_INIT_CONTAINS_X-x_in_register_init).
- **REG_INIT_CONTAINS_Z** — Already tested in test_1_6 (1_6_REG_INIT_CONTAINS_Z-z_in_register_init).

### Test plan rule IDs do not match rules.c
- `ASSIGN_OP_WRONG_BLOCK` → actual: `ASYNC_ASSIGN_REGISTER` (tested)
- `LIT_RESET_HAS_X` → actual: `REG_INIT_CONTAINS_X` (tested in test_1_2)
- `LIT_RESET_HAS_Z` → actual: `REG_INIT_CONTAINS_Z` (tested in test_1_6)

## From test_4_8-latches.md

### Rules not implemented in semantic analysis (dead code in rules.c)
- **LATCH_SR_WIDTH_MISMATCH** — Rule exists in rules.c but no `sem_report_rule` call references it in any semantic analysis code. The comment in `driver_assign.c:1174-1180` says "SR-specific validation can be added here if needed" with a `(void)is_sr_latch;` placeholder. SR latch set/reset width validation is not implemented.
- **LATCH_IN_CONST_CONTEXT** — Rule exists in rules.c but no `sem_report_rule` call references it in any semantic analysis code. Using a latch identifier in compile-time constant contexts (@check/@feature conditions) is not enforced.

### Rules suppressed by priority system
- **LATCH_WIDTH_INVALID** — This rule (priority 0) exists in rules.c and fires in `driver_width.c:887-898`, but `WIDTH_NONPOSITIVE_OR_NONINT` (priority 1) always co-fires on the same declaration. The priority system suppresses the lower-priority rule, so `LATCH_WIDTH_INVALID` never appears in output.

### Rules requiring CHIP infrastructure (not testable in isolation)
- **LATCH_CHIP_UNSUPPORTED** — This rule requires a `CHIP=` project configuration, which in turn requires full `IN_PINS`, `OUT_PINS`, `MAP`, and `CLOCKS` blocks. The pin infrastructure produces many unrelated diagnostics (TOP_PORT_PIN_DECL_MISSING, MAP_PIN_DECLARED_NOT_MAPPED, etc.) that contaminate the test. Cannot be tested in the validation framework without a complete chip project.

### Rules already tested in other sections
- **LATCH_ASSIGN_IN_SYNC** — Already tested in test_1_2 (1_2_LATCH_ASSIGN_IN_SYNC-latch_written_in_sync).

### Test plan rule IDs do not match rules.c
- `LATCH_IN_SYNC` → actual: `LATCH_ASSIGN_IN_SYNC`
- `LATCH_ENABLE_WIDTH` → actual: `LATCH_ENABLE_WIDTH_NOT_1`
- `LATCH_IN_CDC` → actual: `LATCH_AS_CLOCK_OR_CDC` (covers both clock and CDC)
- `LATCH_AS_CLOCK` → listed as missing in test plan but exists as `LATCH_AS_CLOCK_OR_CDC`
- `LATCH_TYPE_INVALID` → listed as missing in test plan but exists as `LATCH_INVALID_TYPE`
- `ASSIGN_MULTI_DRIVER` → actual: `ASSIGN_MULTIPLE_SAME_BITS` or `NET_MULTIPLE_ACTIVE_DRIVERS` depending on context

### Parser limitations
- **LATCH_INVALID_TYPE** — Only one trigger per file possible. The parser (`parser_register.c:248`) returns -1 after reporting the first invalid latch type, aborting the LATCH block and the rest of the module. Multi-trigger testing is not possible.

## From test_4_9-mem_block.md

### No untestable rules
- All rules listed in the test plan were testable. The test plan references `MEM_DECL_TYPE_INVALID` but the actual rule ID is `MEM_TYPE_INVALID` (tested).
- Full MEM coverage deferred to Section 7 test plans per the test plan document.

## From test_4_10-asynchronous_block.md

### Rules already tested in other sections
- **ASYNC_ALIAS_IN_CONDITIONAL** — Already tested in test_4_4 (4_4_ASYNC_ALIAS_IN_CONDITIONAL-alias_in_if_select).
- **ASYNC_ASSIGN_REGISTER** — Already tested in test_4_7 (4_7_ASYNC_ASSIGN_REGISTER-register_in_async).
- **ASYNC_FLOATING_Z_READ** — Already tested in test_1_2 (1_2_ASYNC_FLOATING_Z_READ-all_z_drivers_read).
- **ASYNC_UNDEFINED_PATH_NO_DRIVER** — Already tested in test_1_5 (1_5_ASYNC_UNDEFINED_PATH_NO_DRIVER-partial_coverage).
- **PORT_DIRECTION_MISMATCH_IN** — Already tested in test_4_4 (4_4_PORT_DIRECTION_MISMATCH_IN-write_to_input).
- **PORT_DIRECTION_MISMATCH_OUT** — Already tested in test_4_4 (4_4_PORT_DIRECTION_MISMATCH_OUT-read_from_output).

### Rules suppressed by priority system
- **WIDTH_ASSIGN_MISMATCH_NO_EXT** — Already documented in test_2_3 section. Suppressed by ASSIGN_WIDTH_NO_MODIFIER (priority 1). Never appears in compiler output.

### Rules missing from rules.c (untestable)
- **ALIAS_TRUNCATION** — No rule ID in rules.c (Section 6.2 of test plan). Truncation is covered by ASSIGN_WIDTH_NO_MODIFIER / ASSIGN_TRUNCATES.

### Compiler gaps
- **CONST assignment in ASYNC block** — Assigning to a CONST identifier in ASYNCHRONOUS blocks (`K <= din;` or `K = din;`) produces no error. The ASYNC_INVALID_STATEMENT_TARGET rule should fire but the `JZ_AST_EXPR_IDENTIFIER` case in `sem_check_lvalue_targets_recursive` does not check for `JZ_SYM_CONST`. See issues.md.

## From test_4_11-synchronous_block.md

### Rules already tested in other sections
- **DUPLICATE_BLOCK** — Already tested in test_4_1 (4_1_DUPLICATE_BLOCK-duplicate_sync_block).
- **DOMAIN_CONFLICT** — Already tested in test_1_2 (1_2_DOMAIN_CONFLICT-register_wrong_domain).
- **MULTI_CLK_ASSIGN** — Already tested in test_1_2 (1_2_MULTI_CLK_ASSIGN-register_multi_clock).
- **WRITE_WIRE_IN_SYNC** — Already tested in test_4_5 (4_5_WRITE_WIRE_IN_SYNC-wire_in_sync_block).
- **ASSIGN_TO_NON_REGISTER_IN_SYNC** — Already tested in test_5_2 (5_2_ASSIGN_TO_NON_REGISTER_IN_SYNC-non_register_in_sync).

### Rules listed as missing in test plan but actually exist in rules.c
- **SYNC_CLK_NOT_1BIT** → exists as `SYNC_CLK_WIDTH_NOT_1` (tested).
- **SYNC_RESET_NOT_1BIT** → exists as `SYNC_RESET_WIDTH_NOT_1` (tested).
- **SYNC_MISSING_CLK** → exists in rules.c (tested).

### Happy-path test
- **SYNC_HAPPY_PATH** — Now tested in `4_11_SYNC_HAPPY_PATH-valid_sync_block_ok.jz`. Covers default Rising edge, Falling edge, Immediate/Clocked reset types, RESET_ACTIVE High/Low, multiple SYNC blocks with different clocks, counter pattern, and register read in ASYNC.

## From test_4_12-cdc_block.md

### Rules missing from rules.c (untestable)
- **CDC_MISSING_BRIDGE** — No rule ID exists in rules.c for cross-domain access without a CDC bridge. The test plan (Section 6.1) lists this as expected but no dedicated rule enforces it.
- **CDC_DEST_WRONG_DOMAIN** — No rule ID in rules.c (Section 6.2 of test plan). Dest alias used outside destination domain has no dedicated check.
- **CDC_BUS_GRAY_CODE** — No rule ID in rules.c (Section 6.2 of test plan). No compile-time check for Gray coding discipline on BUS CDC sources.

### Rules not implemented (dead code in rules.c)
- **CDC_DEST_ALIAS_ASSIGNED** — Rule exists in rules.c but `sem_report_rule` is never called for it. The code in `driver_clocks.c:491-511` has the comment "CDC_DEST_ALIAS_ASSIGNED: scan blocks for assignments to CDC alias names" but contains only a conservative best-effort check stub with no actual diagnostic emission.
- **CDC_SOURCE_NOT_PLAIN_REG** — Rule exists in rules.c and has semantic checks for `JZ_AST_EXPR_BINARY` and `JZ_AST_EXPR_CONCAT` source nodes. However, the parser (`parser_cdc.c`) only accepts a single identifier token (optionally followed by a bit-select) in the source position — there is no expression or concatenation parsing. The AST structures that would trigger this check cannot be constructed from user syntax. This rule is dead code.

### Test plan rule IDs do not match rules.c
- `CDC_BIT_WIDTH` → actual: `CDC_BIT_WIDTH_NOT_1`
- `CDC_RAW_NO_STAGES` → actual: `CDC_RAW_STAGES_FORBIDDEN`
- `CDC_ALIAS_READONLY` → actual: `CDC_DEST_ALIAS_ASSIGNED` (dead code — see above)
- `CDC_DOMAIN_CONFLICT` → actual: `DOMAIN_CONFLICT` (already tested in test_1_2)
- `CDC_SOURCE_NOT_PLAIN` → actual: `CDC_SOURCE_NOT_PLAIN_REG` (dead code — see above)

### Inherent co-firing limitations
- **CDC_DEST_ALIAS_DUP** always co-fires with **ID_DUP_IN_MODULE** — When a CDC destination alias name conflicts with an existing identifier, both rules fire on the same location. This is inherent because CDC alias creation goes through the same scope as other declarations.

### Happy-path test
- **CDC_HAPPY_PATH** — Now tested in `4_12_CDC_HAPPY_PATH-valid_cdc_block_ok.jz`. Covers all 7 CDC types (BIT, BUS, FIFO, HANDSHAKE, PULSE, MCP, RAW), default and explicit n_stages, bidirectional crossing, source register written in home domain, and dest alias read in destination domain.

## From test_4_13-module_instantiation.md

### Rules missing from rules.c (untestable)
- **MODULE_SELF_INSTANTIATION** — No rule ID exists in rules.c for a module instantiating itself. Already documented in test_4_2 section.
- **INSTANCE_IDX_IN_OVERRIDE** — Test plan lists this (Section 6.2) but the actual rule is `INSTANCE_ARRAY_IDX_INVALID_CONTEXT` which covers both scalar @new and OVERRIDE contexts (tested).
- **INSTANCE_ARRAY_OVERLAP** — Test plan lists this (Section 6.2) but the actual rule is `INSTANCE_ARRAY_PARENT_BIT_OVERLAP` (tested).
- **INSTANCE_TRUNCATION** — No rule ID in rules.c (Section 6.2 of test plan). Parent wider than child with no extension modifier is not checked as a dedicated truncation rule.

### Rules not firing due to parser catching first (dead code)
- **INSTANCE_ARRAY_MULTI_DIMENSIONAL** — Rule exists in rules.c but the parser produces PARSE000 ("expected module or blackbox name after instance name") when encountering `@new name[2][3]`. The second `[` is not expected by the parser after the first array count. The semantic rule can never fire.

### Rules not firing due to compiler behavior
- **INSTANCE_PORT_WIDTH_MISMATCH with CONST-based child port widths** — The rule fires correctly when the child module uses literal port widths (e.g., `IN [8] din`), but does NOT fire when the child uses CONST expressions (e.g., `CONST { WIDTH = 8; } PORT { IN [WIDTH] din; }`). The `eval_simple_positive_decl_int()` function in `driver_instance.c` cannot resolve the child's CONST-based width, so the comparison is skipped. Tests use literal port widths only.
- **INSTANCE_ARRAY_COUNT_INVALID with CONST count** — The rule fires on `@new inst[0]` (literal zero) but also fires on `@new inst[VALID_COUNT]` where VALID_COUNT is a positive CONST. The parser/semantic check does not evaluate CONST expressions in the array count position — it only accepts literal integers. This means all CONST-based array counts are rejected.

### Test plan rule IDs do not match rules.c
- `INSTANCE_IDX_IN_OVERRIDE` → actual: `INSTANCE_ARRAY_IDX_INVALID_CONTEXT`
- `INSTANCE_OVERLAP` / `INSTANCE_ARRAY_OVERLAP` → actual: `INSTANCE_ARRAY_PARENT_BIT_OVERLAP`
- `SCOPE_DUPLICATE_SIGNAL` → actual: `INSTANCE_NAME_DUP_IN_MODULE` or `INSTANCE_NAME_CONFLICT` (already tested in test_4_2)

### Happy-path and edge-case scenarios (not testable in lint framework)
- Simple instantiation, OVERRIDE constants, no-connect output, width modifiers (=z, =s), instance arrays with IDX, broadcast mapping, port referencing (`inst.port`), BUS port binding, literal tie-off — these verify correct parsing and code generation, not lint diagnostics.

## From test_4_14-feature_guards.md

### Rules not implemented in semantic analysis (dead code in rules.c)
- **FEATURE_VALIDATION_BOTH_PATHS** — Rule exists in rules.c ("S4.14 Both branches of @feature guard must pass full semantic validation") but `sem_report_rule` is never called with this rule ID anywhere in the semantic analysis code. The compiler does not enforce that both enabled and disabled configurations pass semantic validation.

### Rules missing from rules.c (untestable)
- **FEATURE_MISSING_ENDFEAT** — No rule ID in rules.c. Unclosed @feature blocks produce a generic parse error ("unterminated @feature block (missing @endfeat)") without a rule ID.
- **FEATURE_ORPHAN_ELSE** — No rule ID in rules.c. Standalone `@else` without `@feature` produces a generic parse error ("unexpected feature directive without matching @feature") without a rule ID.

### Rules listed as missing in test plan but actually exist in rules.c
- **FEATURE_EXPR_NOT_BOOLEAN** → exists as `FEATURE_COND_WIDTH_NOT_1` (tested).
- **FEATURE_BOTH_CONFIG_INVALID** → exists as `FEATURE_VALIDATION_BOTH_PATHS` (dead code — see above).

### Parser limitations
- **FEATURE_NESTED** — Only one trigger per file possible. The parser (`parser_statements.c:683-691`) detects nesting, reports FEATURE_NESTED, then returns -1 which cascades up and aborts the entire file parse. No subsequent modules are parsed.

### Contexts not covered due to compiler behavior
- **FEATURE_NESTED in declaration blocks** — The parser only checks for nested @feature in ASYNC/SYNC statement blocks (`parse_feature_body_stmts`). In declaration blocks (CONST, PORT, WIRE, REGISTER, LATCH, MEM, MUX) and module-scope, nested @feature guards are silently accepted by the parser via recursive `parse_feature_guard_in_block` calls. The semantic analysis does not check for nesting in these blocks. See issues.md.
- **FEATURE_COND_WIDTH_NOT_1 in declaration blocks** — The semantic check (`sem_check_feature_guard_cond`) is only called from `sem_check_block_expressions_inner`, which only processes ASYNCHRONOUS and SYNCHRONOUS blocks. @feature conditions in CONST, PORT, WIRE, REGISTER, and other declaration blocks are not validated for width. See issues.md.
- **FEATURE_EXPR_INVALID_CONTEXT in declaration blocks** — Same as FEATURE_COND_WIDTH_NOT_1: the reference validation only runs for ASYNC/SYNC blocks. Runtime signal references in @feature conditions within declaration blocks are not caught. See issues.md.

### Happy-path test created
- **FEATURE_HAPPY_PATH** — `4_14_FEATURE_HAPPY_PATH-valid_feature_guards_ok.jz` covers valid @feature with @else, feature in REGISTER/ASYNCHRONOUS/SYNCHRONOUS blocks, CONFIG and CONST expression evaluation, logical operators (&&, ||), comparison operators (==, !=, >), empty feature body, and multi-module interaction via @new.

## From test_5_0-assignment_operators_summary.md

### Rules suppressed by priority system
- **WIDTH_ASSIGN_MISMATCH_NO_EXT** — Already documented in test_2_3 section. Suppressed by ASSIGN_WIDTH_NO_MODIFIER (priority 1). Never appears in compiler output.

### Rules already tested in other sections
- **ASSIGN_WIDTH_NO_MODIFIER** — Already tested in test_2_3 (2_3_ASSIGN_WIDTH_NO_MODIFIER-assign_width_mismatch).

### Rules missing from rules.c (untestable)
- **ASSIGN_TRUNCATION** — Test plan lists this as missing (Section 6.2), but the actual rule `ASSIGN_TRUNCATES` exists in rules.c and is tested (5_0_ASSIGN_TRUNCATES-truncation_with_modifier).

### Happy-path and edge-case scenarios (not testable in lint framework)
- Same-width assignments with all 9 operator variants (=, =z, =s, <=, <=z, <=s, =>, =>z, =>s), redundant modifier on same-width assignment, 1-bit to 256-bit maximum extension — these verify correct parsing and code generation, not lint diagnostics.

## From test_5_1-asynchronous_assignments.md

### Rules already tested in other sections
- **ASYNC_ALIAS_LITERAL_RHS** — Already tested in test_4_10 (4_10_ASYNC_ALIAS_LITERAL_RHS-literal_rhs_in_alias).
- **ASYNC_ASSIGN_REGISTER** — Already tested in test_4_7 (4_7_ASYNC_ASSIGN_REGISTER-register_in_async).
- **ASYNC_UNDEFINED_PATH_NO_DRIVER** — Already tested in test_1_5 (1_5_ASYNC_UNDEFINED_PATH_NO_DRIVER-partial_coverage).
- **ASSIGN_MULTIPLE_SAME_BITS** — Already tested in test_1_5 (1_5_ASSIGN_MULTIPLE_SAME_BITS-double_assign_same_path).

### Test plan rule IDs do not match rules.c
- `ALIAS_LITERAL_BAN` → actual: `ASYNC_ALIAS_LITERAL_RHS`
- `ASSIGN_OP_WRONG_BLOCK` → actual: `ASYNC_ASSIGN_REGISTER`
- `ASSIGN_PARTIAL_COVERAGE` → actual: `ASYNC_UNDEFINED_PATH_NO_DRIVER`
- `ASSIGN_MULTI_DRIVER` → actual: `ASSIGN_MULTIPLE_SAME_BITS` or `NET_MULTIPLE_ACTIVE_DRIVERS` (NET_MULTIPLE_ACTIVE_DRIVERS doesn't fire; see test_1_2 issues)

### Happy-path and edge-case scenarios (not testable in lint framework)
- Alias same width, drive/receive assignments, sliced assignments, concat decomposition, register read, transitive alias, sign-extend alias, ternary in receive, constant drive, cyclic alias collapse, empty ASYNC — these verify correct parsing and code generation, not lint diagnostics.

## From test_5_2-synchronous_assignments.md

### Rules suppressed by priority system
- **SYNC_SLICE_WIDTH_MISMATCH** — This rule (priority 0) exists in rules.c and fires in `driver_assign.c:1458`, but `ASSIGN_SLICE_WIDTH_MISMATCH` (priority 2) always co-fires on the same location. The priority system suppresses the lower-priority rule, so `SYNC_SLICE_WIDTH_MISMATCH` never appears in compiler output. Tests use `ASSIGN_SLICE_WIDTH_MISMATCH` instead (already tested in earlier sections).
- **WIDTH_ASSIGN_MISMATCH_NO_EXT** — Already documented in test_2_3 section. Suppressed by ASSIGN_WIDTH_NO_MODIFIER (priority 1). Never appears in compiler output.

### Rules already tested in other sections
- **WRITE_WIRE_IN_SYNC** — Already tested in test_4_5 (4_5_WRITE_WIRE_IN_SYNC-wire_in_sync_block).
- **ASSIGN_MULTIPLE_SAME_BITS** — Already tested in test_1_5 (1_5_ASSIGN_MULTIPLE_SAME_BITS-double_assign_same_path).
- **ASSIGN_SHADOWING** — Already tested in test_1_5 (1_5_ASSIGN_SHADOWING-shadowed_assignments).
- **ASSIGN_SLICE_OVERLAP** — Already tested in test_1_5 (1_5_ASSIGN_SLICE_OVERLAP-overlapping_slices).
- **PORT_DIRECTION_MISMATCH_OUT** — Already tested in test_4_4 (4_4_PORT_DIRECTION_MISMATCH_OUT-read_from_output).
- **FUNC_RESULT_TRUNCATED_SILENTLY** — Belongs to S5.5, not S5.2; will be tested with S5.5 test plan.

### Test plan rule IDs do not match rules.c
- `ASSIGN_OP_WRONG_BLOCK` → actual: `SYNC_NO_ALIAS` (for alias `=` in SYNC) or `WRITE_WIRE_IN_SYNC` (for wires) or `ASSIGN_TO_NON_REGISTER_IN_SYNC` (for non-register targets)
- `ASSIGN_SHADOW_OUTER` → actual: `SYNC_ROOT_AND_CONDITIONAL_ASSIGN`
- `ASSIGN_MULTI_DRIVER` → actual: `SYNC_MULTI_ASSIGN_SAME_REG_BITS`
- `WIDTH_ASSIGN_MISMATCH_NO_EXT` → suppressed by `ASSIGN_WIDTH_NO_MODIFIER`

### Happy-path and edge-case scenarios (not testable in lint framework)
- Simple register load, conditional load, zero/sign-extend load, sliced assignment, non-overlapping slices, concat decomposition, register hold, SELECT-based assign, empty SYNC body, deeply nested conditionals — these verify correct parsing and code generation, not lint diagnostics.

## From test_5_3-conditional_statements.md

### Rules listed as missing in test plan but actually exist in rules.c
- **IF_COND_NOT_1BIT** → exists as `IF_COND_WIDTH_NOT_1` (tested).

### Test plan rule IDs do not match rules.c
- `ASSIGN_INDEPENDENT_CHAIN_CONFLICT` → actual: `ASSIGN_INDEPENDENT_IF_SELECT` (already tested in test_1_5)
- `COMB_LOOP_DETECTED` → actual: `COMB_LOOP_UNCONDITIONAL` (tested)

### Parser limitations
- **IF_COND_MISSING_PARENS** — Only one trigger per file possible. The parser aborts the entire file after the first missing-parens error. Separate test files needed for IF vs ELIF contexts. SYNC block context cannot be tested because the parser abort prevents reaching subsequent code.

### Happy-path and edge-case scenarios (not testable in lint framework)
- Simple IF/ELSE, IF/ELIF/ELSE, nested IF, IF without ELSE in SYNC, flow-sensitive no-loop, multiple ELIFs, deeply nested (10 levels), empty IF body — these verify correct parsing and control flow, not lint diagnostics.
- IF with only ELSE (no preceding IF) — parse-level error without a rule ID.

## From test_5_4-select_case_statements.md

### Rules missing from rules.c (untestable)
- **SELECT_CASE_NOT_CONSTANT** — No rule ID in rules.c for runtime (non-constant) expressions in CASE values. The test plan (Section 6.2) lists this as a missing rule.
- **SELECT_X_WILDCARD_OVERLAP** — No rule ID in rules.c for overlapping x-wildcard patterns that could match the same value. The test plan (Section 6.2) lists this as a missing rule.

### Dead code in rules.c
- **WARN_INCOMPLETE_SELECT_ASYNC** — This rule exists in rules.c (GENERAL_WARNINGS category) but `sem_report_rule` is never called with this rule ID anywhere in the semantic analysis code. The functionally equivalent `SELECT_DEFAULT_RECOMMENDED_ASYNC` (CONTROL_FLOW_IF_SELECT category) is what actually fires. WARN_INCOMPLETE_SELECT_ASYNC appears to be dead code.

### Rules not testable due to compiler behavior
- **CONST in CASE values** — CONST values are unsized compile-time integers with no width, making them incompatible with CASE pattern matching which requires width-matched values. The spec was corrected: CASE labels are sized integer literals or `@global` constants. The compiler correctly rejects CONST in CASE via `CONST_USED_WHERE_FORBIDDEN`.

### Inherent co-firing limitations
- **SELECT_DEFAULT_RECOMMENDED_ASYNC** always co-fires with **ASYNC_UNDEFINED_PATH_NO_DRIVER** — An ASYNC SELECT without DEFAULT leaves ports/wires undriven on some execution paths, which triggers the "signal undriven" error on the port declaration.

### Test plan rule IDs do not match rules.c
- `SELECT_DUPLICATE_CASE` → actual: `SELECT_DUP_CASE_VALUE`
- `WARN_INCOMPLETE_SELECT_ASYNC` → actual: `SELECT_DEFAULT_RECOMMENDED_ASYNC` (WARN_INCOMPLETE_SELECT_ASYNC is dead code)
- `SELECT_CASE_WIDTH_MISMATCH` → listed as missing in test plan (Section 6.2) but it EXISTS in rules.c and is fully implemented (tested)

### Happy-path and edge-case scenarios (not testable in lint framework)
- Simple SELECT, multiple CASEs, x-wildcard CASE, fall-through, SYNC without DEFAULT (valid hold behavior), nested SELECT, single CASE + DEFAULT, many CASEs (256), all-x CASE — these verify correct parsing and runtime behavior, not lint diagnostics.

## From test_5_5-intrinsic_operators.md

### Rules suppressed by priority system
- **FUNC_RESULT_TRUNCATED_SILENTLY** — This rule (priority 0) exists in rules.c and fires in `driver_assign.c:1478-1496` for uadd/sadd/umul/smul results. However, `ASSIGN_WIDTH_NO_MODIFIER` (priority 1) always co-fires on the same location when the result width doesn't match the target. The priority system suppresses the lower-priority rule, so `FUNC_RESULT_TRUNCATED_SILENTLY` never appears in compiler output.

### Dead code in rules.c
- **WIDTHOF_INVALID_TARGET** — Rule exists in rules.c but `sem_report_rule` is never called with this rule ID anywhere in the semantic analysis code.
- **WIDTHOF_INVALID_SYNTAX** — Rule exists in rules.c but `sem_report_rule` is never called with this rule ID anywhere in the semantic analysis code.
- **WIDTHOF_WIDTH_NOT_RESOLVABLE** — Rule exists in rules.c but `sem_report_rule` is never called with this rule ID anywhere in the semantic analysis code.

### Rules missing from rules.c (untestable)
- **LIT_VALUE_INVALID** — Rule exists in rules.c ("S5.5.11 lit() value must be a nonnegative integer constant expression") but could not be triggered in tested scenarios. Negative values in `lit()` would require a unary minus which the parser doesn't accept in intrinsic argument position. Zero is accepted as valid. The conditions that trigger this rule remain unknown.

### Test plan rule IDs do not match rules.c
- `INTRINSIC_ARG_COUNT` → no matching rule ID in rules.c; argument count mismatches are handled by the parser.
- `INTRINSIC_COMPILE_TIME_ONLY` → actual: `CLOG2_INVALID_CONTEXT` and `WIDTHOF_INVALID_CONTEXT` (separate rules per function).
- `INTRINSIC_OPERAND_TYPE` → no matching rule ID in rules.c; covered by function-specific rules.
- `INTRINSIC_BSWAP_NOT_BYTE` → actual: `BSWAP_WIDTH_NOT_BYTE_ALIGNED`.
- `INTRINSIC_UNDEFINED` → no matching rule ID in rules.c; unknown function names are parse errors.
- `INTRINSIC_LIT_OVERFLOW` → actual: `LIT_VALUE_OVERFLOW`.
- `INTRINSIC_CLOG2_ZERO` → actual: `CLOG2_NONPOSITIVE_ARG`.
- `INTRINSIC_WIDTH_MISMATCH` → no matching rule ID in rules.c; covered by function-specific rules.

### Limitations due to compiler behavior
- **INDEX_OUT_OF_RANGE checks (gbit/sbit/gslice/sslice) only work with bare integers** — The index parameter parsing uses `strtoul()` which stops at the `'d'` character in sized literals like `4'd8`, reading "4" instead of "8". Tests use bare integers for index arguments. See issues.md.
- **Intrinsic function calls in ASYNC alias (`=`) context** — Intrinsic calls containing literal arguments trigger ASYNC_ALIAS_LITERAL_RHS when used on the RHS of `=` in ASYNC blocks. Tests use `<=` (receive-assign) for intrinsic expressions in ASYNC blocks.

## From test_6_1-project_purpose.md

### Rules missing from rules.c (untestable)
- **PROJECT_CHIP_JSON_MALFORMED** — Test plan lists this (Section 6.2) as a missing rule, but the actual rule `PROJECT_CHIP_DATA_INVALID` exists in rules.c and is tested (6_1_PROJECT_CHIP_DATA_INVALID-malformed_json).

### Happy-path and edge-case scenarios (not testable in lint framework)
- CHIP=GENERIC (default), specific chip with string literal, chip ID as identifier, case-insensitive matching, no CHIP property (defaults to GENERIC), chip JSON in same directory — these verify correct chip data loading, not lint diagnostics.

## From test_6_2-project_canonical_form.md

### Rules missing from rules.c (untestable)
- **IMPORT_WRONG_POSITION** — Test plan lists this as missing. The equivalent rule `IMPORT_NOT_AT_PROJECT_TOP` exists and is tested.
- **IMPORT_FILE_NOT_FOUND** — No explicit rule ID; handled as a generic parse error via stderr (`"failed to read imported file"`). Cannot produce a diagnostic with a rule ID.

### Happy-path and edge-case scenarios (not testable in lint framework)
- Full project with all sections in order, importing files with modules/blackboxes, relative path imports, zero imports, deep import chains — these verify correct import resolution, not lint diagnostics.

## From test_6_3-config_block.md

### Rules missing from rules.c (untestable)
- **CONFIG_RUNTIME_USE** — Test plan lists this as missing. The equivalent rule `CONFIG_USED_WHERE_FORBIDDEN` exists in rules.c and is tested (6_3_CONFIG_USED_WHERE_FORBIDDEN-runtime_use).

### Rules not firing due to parser behavior
- **CONFIG_MULTIPLE_BLOCKS** — Rule exists in rules.c but the parser catches multiple CONFIG blocks first with PARSE000. The semantic rule never fires. See issues.md.
- **CONST_NUMERIC_IN_STRING_CONTEXT** — The only string context (@file() in MEM) doesn't accept CONFIG references due to a parser bug. Cannot be triggered. See issues.md.

### Happy-path and edge-case scenarios (not testable in lint framework)
- Numeric CONFIG (`XLEN = 32;`), string CONFIG (`FIRMWARE = "out/fw.bin";`), CONFIG in port width, CONFIG in MEM depth, CONFIG referencing earlier CONFIG, CONFIG.name in CONST initializer, CONFIG value = 0, single CONFIG entry, CONFIG and CONST same name — these verify correct CONFIG evaluation, not lint diagnostics.

## From test_6_4-clocks_block.md

### Rules missing from rules.c (untestable)
- **CLOCK_GEN_SELF_REF** — Test plan lists this as missing, but it exists as CLOCK_GEN_INPUT_IS_SELF_OUTPUT in rules.c. Tested (6_4_CLOCK_GEN_INPUT_IS_SELF_OUTPUT-self_reference).
- **CLOCK_GEN_OUT_HAS_PERIOD** — Test plan lists this as missing, but it exists as CLOCK_GEN_OUTPUT_HAS_PERIOD in rules.c. Tested (6_4_CLOCK_GEN_OUTPUT_HAS_PERIOD-gen_out_with_period).
- **CLOCK_DUAL_SOURCE** — Test plan uses this name, but it exists as CLOCK_SOURCE_AMBIGUOUS in rules.c. Tested (6_4_CLOCK_SOURCE_AMBIGUOUS-dual_source).
- **CLOCK_PERIOD_INVALID** — Test plan uses this name, but it exists as CLOCK_PERIOD_NONPOSITIVE in rules.c. Tested (6_4_CLOCK_PERIOD_NONPOSITIVE-invalid_period).

### Happy-path and edge-case scenarios (not testable in lint framework)
- IN_PIN clock with period, CLOCK_GEN PLL/CLKDIV/OSC/BUF with multiple outputs, falling edge clock, clock with no config, very fast/slow periods, optional CONFIG params omitted — these verify correct CLOCKS/CLOCK_GEN evaluation, not lint diagnostics.

## From test_6_5-pin_blocks.md

### Rules missing from rules.c (untestable)
- **PIN_STANDARD_INVALID** — Test plan lists this as missing, but it exists as PIN_INVALID_STANDARD in rules.c. Tested (6_5_PIN_INVALID_STANDARD-bad_standard).
- **PIN_IN_HAS_DRIVE** — No rule ID in rules.c. The spec says "No drive property (inputs are passive)" for IN_PINS, but there is no compiler check that rejects drive on IN_PINS. The compiler silently ignores drive on input pins.
- **PIN_DUPLICATE_ACROSS** — Test plan uses this name, but it exists as PIN_DECLARED_MULTIPLE_BLOCKS in rules.c. Tested (6_5_PIN_DECLARED_MULTIPLE_BLOCKS-cross_block_duplicate).
- **PIN_MISSING_DRIVE** — Test plan uses this name, but it exists as PIN_DRIVE_MISSING_OR_INVALID in rules.c. Tested (6_5_PIN_DRIVE_MISSING_OR_INVALID-drive_problems).

### Contexts not covered due to compiler behavior
- **PIN_DIFF_OUT_MISSING_FCLK/PCLK/RESET on INOUT_PINS** — These three rules only fire for OUT_PINS differential pins, not INOUT_PINS (see issues.md).
- **PIN_TERM_ON_OUTPUT with term=OFF** — term=OFF on OUT_PINS is not flagged (see issues.md).

### Happy-path and edge-case scenarios (not testable in lint framework)
- Valid IN_PIN/OUT_PIN/INOUT_PIN with all supported standards, bus pins with various widths, differential pins with proper attributes, all valid drive strengths — these verify correct PIN block evaluation, not lint diagnostics.

## From test_6_6-map_block.md

### Rules not tested (unclear trigger conditions)
- **MAP_INVALID_BOARD_PIN_ID** — Rule exists in rules.c ("S6.6/S6.9 Board pin ID format invalid for target device") but requires chip-specific validation. Without a target device configuration, the compiler does not validate board pin ID format. The trigger conditions for this rule are unclear — it likely requires a CHIP= project setting with chip data that defines valid pin ID formats.

### Test plan rule IDs do not match rules.c
- `MAP_PIN_INVALID` → actual: `MAP_INVALID_BOARD_PIN_ID` (not tested; see above)
- `MAP_DUPLICATE` → actual: `MAP_DUP_PHYSICAL_LOCATION` (tested)
- `MAP_BOARD_PIN_CONFLICT` → actual: `MAP_DUP_PHYSICAL_LOCATION` (tested)
- `MAP_REQUIRED_UNMAPPED` → listed as missing in test plan, but exists as `MAP_PIN_DECLARED_NOT_MAPPED` (tested)
- `MAP_DIFF_NO_PAIR` → listed as missing in test plan, but covered by `MAP_DIFF_EXPECTED_PAIR` and `MAP_DIFF_MISSING_PN` (both tested)

### Inherent co-firing limitations
- **MAP_DIFF_SAME_PIN** always co-fires with **MAP_DUP_PHYSICAL_LOCATION** — When P and N reference the same physical pin, the duplicate physical location warning is logically correct and unavoidable.

### Happy-path and edge-case scenarios (not testable in lint framework)
- Scalar pin mapping, bus bit mapping, differential pin mapping with valid {P,N}, complete design with all pins mapped, optional pin unmapped (no-connect) — these verify correct MAP block evaluation, not lint diagnostics.

## From test_6_7-blackbox_modules.md

### Rules missing from rules.c (untestable)
- **BLACKBOX_HAS_BODY** — Test plan lists this as missing, but it exists as `BLACKBOX_BODY_DISALLOWED` in rules.c. Not a gap.
- **MODULE_DUPLICATE_NAME** — Test plan references this rule ID, but it does not exist in rules.c. The actual rule is `BLACKBOX_NAME_DUP_IN_PROJECT` (already tested at `4_2_BLACKBOX_NAME_DUP_IN_PROJECT-blackbox_name_conflicts`).

### Rules defined but never used in compiler code
- **BLACKBOX_UNDEFINED_IN_NEW** — Defined in rules.c ("S6.7/S6.9 @new references undefined blackbox name") but never referenced in any semantic analysis code. The compiler uses `INSTANCE_UNDEFINED_MODULE` instead when a @new references a non-existent module or blackbox. This rule cannot be triggered.

### Contexts not testable via semantic rules
- **BLACKBOX_BODY_DISALLOWED for ASYNCHRONOUS/SYNCHRONOUS/WIRE/REGISTER/MEM blocks** — The parser rejects these blocks inside @blackbox with PARSE000 ("unexpected token in @blackbox body; expected CONST or PORT") before semantic analysis runs. The semantic rule can only fire for CONST blocks, which the parser allows through.

### Happy-path and edge-case scenarios (not testable in lint framework)
- Simple blackbox with PORT only, blackbox with INOUT ports, blackbox instantiated via @new, multiple blackboxes in project — these verify correct blackbox parsing and instantiation, not lint diagnostics.

## From test_6_8-bus_aggregation.md

### Rules missing from rules.c (untestable)
- None — the test plan listed `BUS_SIGNAL_DUPLICATE` as missing, but it exists in rules.c as `BUS_DEF_SIGNAL_DUP_NAME` and is now tested.

### Happy-path and edge-case scenarios (not testable in lint framework)
- BUS with IN/OUT/INOUT signals, BUS SOURCE/TARGET port usage, bus-to-bus aggregation, BUS with single signal, BUS with many signals — these verify correct BUS parsing and aggregation, not lint diagnostics.
- Two SOURCE ports on same BUS without tri-state — this is a multi-driver scenario covered by existing NET_MULTIPLE_ACTIVE_DRIVERS rules, not a BUS-specific rule.

## From test_6_9-top_level_module.md

### Rules missing from rules.c (untestable)
- **PROJECT_MULTIPLE_TOP** — No rule ID in rules.c for multiple @top directives in a single project. The parser emits a generic parse error ("multiple top-level @top instantiations in a single project are not allowed") without a rule ID.
- **PROJECT_NO_TOP** — Test plan lists this as missing, but it exists in rules.c as `PROJECT_MISSING_TOP_MODULE` and is now tested.
- **TOP_PORT_UNBOUND** — Test plan lists this as missing, but it exists in rules.c as `TOP_PORT_NOT_LISTED` and is now tested.

### Contexts not covered
- **INSTANCE_UNDEFINED_MODULE in @top context** — The compiler uses an ad-hoc `TOP_MODULE_NOT_FOUND` rule (not in rules.c) instead of `INSTANCE_UNDEFINED_MODULE` for undefined module references in @top. The test covers the actual compiler behavior but the rule ID mismatch is noted as an issue.
- **TOP_PORT_PIN_DIRECTION_MISMATCH with INOUT ports** — INOUT port bound to non-INOUT_PINS not tested due to complexity of INOUT port setup in test modules. Only IN→OUT_PINS and OUT→IN_PINS mismatches are tested.

### Happy-path and edge-case scenarios (not testable in lint framework)
- @top with all ports bound to pins, no-connect on debug port, imported module reference, port bound to pin with matching width — these verify correct @top resolution, not lint diagnostics.
- Top module with only IN ports, INOUT ports bound to INOUT_PINS — edge cases that produce no diagnostics.

## From test_6_10-project_scope_and_uniqueness.md

### Rules already tested under other sections
- **MODULE_NAME_DUP_IN_PROJECT** — Already tested as `4_2_MODULE_NAME_DUP_IN_PROJECT-duplicate_module_names`. No duplicate test created.

### Happy-path scenarios (not testable in lint framework)
- All names unique across project — produces no diagnostics, cannot be tested in lint validation framework.

## From test_7_0-memory_port_modes.md

### Rules missing from rules.c (untestable)
- **MEM_PORT_MODE_INVALID** — The test plan references this rule ID but it does not exist in `rules.c`. The actual rules covering memory port mode validation are `MEM_INVALID_PORT_TYPE` and `MEM_INVALID_WRITE_MODE`, both of which are tested.

### Contexts not covered
- **MEM_INVALID_PORT_TYPE for OUT port with non-ASYNC/SYNC qualifier** — The parser only accepts ASYNC/SYNC identifiers after OUT port names; any other identifier causes a parse error (PARSE000) before reaching the semantic check. The semantic check for invalid OUT qualifiers is unreachable through normal parsing.

### Happy-path scenarios (not testable in lint framework)
- Valid OUT ASYNC/SYNC read ports, valid IN write ports, valid INOUT ports with WRITE_FIRST/READ_FIRST/NO_CHANGE — produce no diagnostics, cannot be tested in lint validation framework.

## From test_7_1-mem_declaration.md

### Rules missing from rules.c (untestable)
- **MEM_DECL_TYPE_INVALID** — Test plan references this rule ID but the actual rule in `rules.c` is `MEM_TYPE_INVALID`. Tested under actual ID.
- **MEM_DECL_DEPTH_ZERO** — Test plan references this rule ID but the actual rule in `rules.c` is `MEM_INVALID_DEPTH`. Tested under actual ID.
- **MEM_DECL_WIDTH_ZERO** — Test plan references this rule ID but the actual rule in `rules.c` is `MEM_INVALID_WORD_WIDTH`. Tested under actual ID.
- **MEM_DECL_NO_PORTS** — Test plan references this rule ID but the actual rule in `rules.c` is `MEM_EMPTY_PORT_LIST`. Tested under actual ID.
- **SCOPE_DUPLICATE_SIGNAL** — Test plan suggests this for duplicate MEM names but the actual rule is `MEM_DUP_NAME`. Tested under actual ID.

### Happy-path scenarios (not testable in lint framework)
- Valid BLOCK/DISTRIBUTED type MEMs, valid depth/width, multiple ports — produce no diagnostics, cannot be tested in lint validation framework.

## From test_7_2-port_types_and_semantics.md

### Rules missing from rules.c (untestable)
- **MEM_PORT_DIRECTION** — Test plan lists this as missing. Covered by existing rules `MEM_WRITE_TO_READ_PORT` and `MEM_READ_FROM_WRITE_PORT`.

### Rules not firing due to compiler bugs
- **MEM_MULTIPLE_WDATA_ASSIGNS** — Rule exists in `rules.c` but does not fire when two `.wdata <=` assignments occur in the same SYNCHRONOUS block (see issues.md).
- **MEM_MULTIPLE_ADDR_ASSIGNS** — Rule exists in `rules.c` but does not fire when two `.addr <=` assignments occur in the same SYNCHRONOUS block (see issues.md).
- **MEM_INOUT_WDATA_WRONG_OP** — Rule exists in `rules.c` but is shadowed by `SYNC_NO_ALIAS` which fires first when `=` is used in a SYNCHRONOUS block (see issues.md).

### Happy-path scenarios (not testable in lint framework)
- Valid OUT ASYNC/SYNC reads, valid IN writes, valid INOUT .addr/.data/.wdata access — produce no diagnostics, cannot be tested in lint validation framework.

## From test_7_3-memory_access_syntax.md

### Rules missing from rules.c (untestable)
- **MEM_ACCESS_WRONG_MODE** — Test plan rule ID does not exist in `rules.c`. The semantics are covered by existing rules: `MEM_SYNC_PORT_INDEXED`, `MEM_ASYNC_PORT_FIELD_DATA`, `MEM_SYNC_ADDR_INVALID_PORT`.
- **MEM_ACCESS_WRONG_BLOCK** — Test plan rule ID does not exist in `rules.c`. The semantics are covered by: `MEM_WRITE_IN_ASYNC_BLOCK`, `MEM_SYNC_DATA_IN_ASYNC_BLOCK`, `MEM_SYNC_ADDR_IN_ASYNC_BLOCK`.
- **MEM_ACCESS_ADDR_WIDTH** — Test plan rule ID does not exist in `rules.c`. Covered by `MEM_ADDR_WIDTH_TOO_WIDE`.
- **MEM_ACCESS_PORT_UNDEFINED** — Test plan rule ID does not exist in `rules.c`. Covered by `MEM_PORT_UNDEFINED`.

### Rules not firing due to compiler bugs
- **MEM_READ_SYNC_WITH_EQUALS** — Rule exists in `rules.c` but is shadowed by `SYNC_NO_ALIAS` which fires first when `=` is used in a SYNCHRONOUS block (see issues.md).
- **MEM_SYNC_ADDR_WITHOUT_RECEIVE** — Rule exists in `rules.c` but is shadowed by `SYNC_NO_ALIAS` which fires first (see issues.md).
- **MEM_SYNC_ADDR_IN_ASYNC_BLOCK** — Rule exists in `rules.c` but is shadowed by `ASYNC_INVALID_STATEMENT_TARGET` which fires first when `.addr` receive is used in ASYNCHRONOUS block (see issues.md).

### Happy-path scenarios (not testable in lint framework)
- Valid async reads, sync reads, sync writes, INOUT access — produce no diagnostics, cannot be tested in lint validation framework.

## From test_7_4-write_modes.md

### Rules already tested under different section prefix
- **MEM_INVALID_WRITE_MODE** — Already tested at `7_0_MEM_INVALID_WRITE_MODE-bad_write_mode_value`. Covers invalid WRITE_MODE on IN port (attribute block form) and INOUT port (attribute block form). Shorthand form (`IN wr INVALID;`) cannot trigger this rule because the parser only accepts the three valid keywords as shorthand; invalid shorthand identifiers produce PARSE000 instead.

### Rules missing from rules.c (untestable)
- **MEM_WRITE_MODE_ON_READ_PORT** — No rule ID exists in `rules.c` for applying a write mode to a read-only (OUT) port. Test plan lists this as a gap.

### Happy-path scenarios (not testable in lint framework)
- WRITE_FIRST, READ_FIRST, NO_CHANGE valid usage — produce no diagnostics, cannot be tested in lint validation framework.
- Write without simultaneous read (all modes identical) — behavioral, not a lint check.

## From test_7_5-initialization.md

### Rules missing from rules.c (untestable)
- **MEM_INIT_CONTAINS_Z** — Test plan lists this rule but no `MEM_INIT_CONTAINS_Z` rule ID exists in `rules.c`. The spec says memory init literals must not contain `x` bits (S7.5.1), and z-bit prohibition for literals may be handled implicitly by the literal parser or is not separately checked. File-based z values are covered by `MEM_INIT_FILE_CONTAINS_X` ("contains x or z values").

### Rules tested but with caveats
- **MEM_INIT_FILE_NOT_FOUND** — Listed as "missing" in the test plan but the rule ID does exist in `rules.c`. When run from CWD != validation directory, the compiler fires PATH_OUTSIDE_SANDBOX instead (sandbox resolution is relative to the input file). The test passes when run via `run_validation.sh` which uses absolute paths.

### Happy-path scenarios (not testable in lint framework)
- Valid literal init, valid @file init, @file with CONST string path — produce no diagnostics, cannot be tested in lint validation framework.
- File exactly matching depth — produces no diagnostics.
- Single-entry memory with literal — produces no diagnostics.

## From test_7_6-complete_examples.md

### All scenarios are happy-path (not testable in lint framework)
- All 9 canonical MEM examples (S7.6.1–S7.6.9) are happy-path compilation tests that should produce no diagnostics. The validation framework only tests for expected diagnostic output, so these cannot be tested here.
- Scenarios: Simple ROM, Dual-Port Register File, Synchronous FIFO, Registered Read Cache, Triple-Port, Quad-Port, Configurable Memory, Single Port INOUT, True Dual Port.

## From test_7_7-error_checking_and_validation.md

### Contexts not covered due to language constraints
- **MEM_WARN_DEAD_CODE_ACCESS in ASYNC blocks** — Cannot test dead MEM reads in ASYNCHRONOUS IF blocks because `=` alias inside IF is forbidden (ASYNC_ALIAS_IN_CONDITIONAL). Only SYNCHRONOUS block dead code paths are tested.
- **MEM_WARN_DEAD_CODE_ACCESS for INOUT ports** — INOUT .addr/.wdata assignments in dead SYNC paths were not separately tested; the rule fires on any mem port slice in unreachable code, which is covered by IN port dead writes.

### Rules already tested by other test plans
- **MEM_WARN_PARTIAL_INIT** — Tested in `7_5_MEM_WARN_PARTIAL_INIT-file_smaller_than_depth.jz` (from test_7_5).
- **MEM_MULTIPLE_WDATA_ASSIGNS** — Tested in `7_3_MEM_MULTIPLE_WDATA_ASSIGNS-inout_multi_wdata.jz` (from test_7_3).

## From test_7_8-mem_vs_register_vs_wire.md

### No new rules to test
Section 7.8 is a comparison table (MEM vs REGISTER vs WIRE). It introduces no new rule IDs. All negative test scenarios map to rules already tested in other sections:
- **MEM access without address** → `MEM_PORT_USED_AS_SIGNAL` (tested in `7_3_MEM_PORT_USED_AS_SIGNAL-bare_port_ref.jz`)
- **WIRE with storage semantics** → `WRITE_WIRE_IN_SYNC` (tested in `4_5_WRITE_WIRE_IN_SYNC-wire_in_sync_block.jz`)
- **REGISTER in async (wire-like usage)** → `ASYNC_ASSIGN_REGISTER` (tested in `4_7_ASYNC_ASSIGN_REGISTER-register_in_async.jz`)
- **Non-register in sync** → `ASSIGN_TO_NON_REGISTER_IN_SYNC` (tested in `5_2_ASSIGN_TO_NON_REGISTER_IN_SYNC-non_register_in_sync.jz`)
- **MEM write in async** → `MEM_WRITE_IN_ASYNC_BLOCK` (tested in `7_3_MEM_WRITE_IN_ASYNC_BLOCK-mem_write_in_async.jz`)

### Rules missing from rules.c (untestable)
- **REGISTER with address syntax** — Test plan scenario 3.2.2 ("REGISTER with address syntax"). No specific rule exists. Bracket notation on registers is valid bit-slicing syntax, and dot-field notation (`reg.port.addr`) would produce parse/undeclared identifier errors, not a storage-type-confusion diagnostic.

## From test_7_9-mem_in_module_instantiation.md

### Happy-path scenarios (skipped)
- Module with MEM instantiated normally — happy path, no diagnostic to test.
- OVERRIDE changes MEM depth via CONST — happy path, no diagnostic to test.
- Multiple instances of MEM-containing module — happy path, no diagnostic to test.

### Notes
- "Direct access to child MEM from parent" fires `UNDECLARED_IDENTIFIER` (not a MEM-specific rule). Tested in `7_9_UNDECLARED_IDENTIFIER-child_mem_access_from_parent.jz`. No MEM-specific rule exists for hierarchical MEM access rejection.

## From test_7_10-const_evaluation_in_mem.md

### Happy-path scenarios (no diagnostic to test)
- CONST width in MEM: `MEM { m [WIDTH] [DEPTH] ... }` — valid usage, no error.
- CONFIG depth in MEM: `MEM { m [8] [CONFIG.DEPTH] ... }` — valid usage, no error.
- Address width = clog2(depth) — implicit derivation, no error.

### Notes
- All negative scenarios from the test plan are covered by existing rule IDs (`MEM_UNDEFINED_CONST_IN_WIDTH`, `CONST_NEGATIVE_OR_NONINT`).

## From test_7_11-synthesis_implications.md

### Rules missing from rules.c (untestable)
- **MEM_EXCEEDS_BSRAM** — Listed in test plan as missing. However, the existing rules `MEM_BLOCK_RESOURCE_EXCEEDED` and `MEM_DISTRIBUTED_RESOURCE_EXCEEDED` cover this functionality.

### Happy-path scenarios (skipped)
- BLOCK maps to BSRAM primitives — no diagnostic produced, no validation test needed.
- DISTRIBUTED maps to LUT-based registers — no diagnostic produced, no validation test needed.
- Memory fitting within chip resources — no diagnostic produced, no validation test needed.

### Notes
- All three testable rules (`MEM_BLOCK_RESOURCE_EXCEEDED`, `MEM_DISTRIBUTED_RESOURCE_EXCEEDED`, `MEM_BLOCK_MULTI`) are now covered by validation tests.

## From test_8_1-global_purpose.md

### Rules missing from rules.c (untestable)
- **GLOBAL_READONLY** — Test plan lists this as missing, but it exists as `GLOBAL_ASSIGN_FORBIDDEN`. Not a gap.
- **GLOBAL_DUPLICATE_NAME** — Test plan uses this name, but the actual rule ID is `GLOBAL_NAMESPACE_DUPLICATE`. Not a gap.

### Notes
- Both testable rules (`GLOBAL_NAMESPACE_DUPLICATE`, `GLOBAL_ASSIGN_FORBIDDEN`) are now covered by validation tests.

## From test_8_2-global_syntax.md

### Rules missing from rules.c (untestable)
- **GLOBAL_NOT_SIZED_LITERAL** — Test plan uses this name, but the actual rule ID is `GLOBAL_INVALID_EXPR_TYPE`. Not a gap.

### Happy-path scenarios (skipped)
- Valid @global block with sized literals — no diagnostic produced, no validation test needed.
- Multiple entries in one @global block — no diagnostic produced, no validation test needed.
- Empty @global block — no diagnostic produced, no validation test needed.

### Scenarios not testable
- Missing @endglob — produces a parser error (PARSE000), not a semantic rule. Cannot be tested with rule-based validation.
- CONFIG reference as @global value — CONFIG references are not valid inside @global blocks and produce GLOBAL_INVALID_EXPR_TYPE (same as bare integers). Covered by the bare integer test.

### Notes
- The testable rule (`GLOBAL_INVALID_EXPR_TYPE`) is now covered by a validation test.

## From test_8_3-global_semantics.md

### Rules missing from rules.c (untestable)
- **GLOBAL_DUPLICATE_CONST** — Test plan uses this name, but the actual rule ID is `GLOBAL_CONST_NAME_DUPLICATE`. Not a gap; covered by test.
- **Undefined global reference** — Test plan lists this with no rule ID ("—"). Covered by existing rule `GLOBAL_CONST_USE_UNDECLARED` (tested in section 8.1/8.4).

### Happy-path scenarios (skipped)
- Reference via `ISA.INST_ADD` resolves correctly — no diagnostic produced, no validation test needed.
- Width matches target in assignment — no diagnostic produced, no validation test needed.

### Boundary/edge cases (skipped)
- Multiple blocks with disjoint namespaces — no diagnostic produced, no validation test needed. Implicitly covered as negative testing in `GLOBAL_CONST_NAME_DUPLICATE` test (same const_id in different blocks does not trigger).

### Notes
- The testable rule (`GLOBAL_CONST_NAME_DUPLICATE`) is now covered by a validation test.

## From test_8_4-global_value_semantics.md

### Happy-path scenarios (skipped)
- Global constant as RHS of ASYNC assignment — no diagnostic, validation framework only tests for errors.
- Global constant in expression — no diagnostic.
- Global constant in concatenation — no diagnostic.
- Global constant in SYNCHRONOUS block — no diagnostic.

### Notes
- The testable rule (`ASSIGN_WIDTH_NO_MODIFIER`) is now covered by a validation test. The test plan referenced the rule as `WIDTH_ASSIGN_MISMATCH_NO_EXT` but the actual rule ID is `ASSIGN_WIDTH_NO_MODIFIER`.

## From test_8_5-global_errors.md

### Rules with mismatched IDs (test plan vs rules.c)
- **GLOBAL_DUPLICATE_NAME** → actual rule is `GLOBAL_NAMESPACE_DUPLICATE` (already tested in 8_1)
- **GLOBAL_DUPLICATE_CONST** → actual rule is `GLOBAL_CONST_NAME_DUPLICATE` (already tested in 8_3)
- **GLOBAL_NOT_SIZED_LITERAL** → actual rule is `GLOBAL_INVALID_EXPR_TYPE` (already tested in 8_2)
- **GLOBAL_READONLY** → actual rule is `GLOBAL_ASSIGN_FORBIDDEN` (already tested in 8_1)

### LIT_OVERFLOW in global context
- **LIT_OVERFLOW** does not fire inside `@global` blocks; the compiler reports `GLOBAL_INVALID_EXPR_TYPE` instead. Test `8_5_LIT_OVERFLOW-global_literal_overflow` was created but uses `GLOBAL_INVALID_EXPR_TYPE` diagnostics to match actual compiler behavior. See issues.md for details.

## From test_9_1-check_syntax.md

### Rules missing from rules.c (untestable)
- **Missing parentheses in @check** — Produces generic PARSE000 ("expected '(' after @check"), not a rule-ID diagnostic. Cannot be tested with a specific rule ID.
- **Missing message in @check** — Produces generic PARSE000 ("expected string literal message in @check"), not a rule-ID diagnostic.
- **Missing semicolon after @check** — Produces generic PARSE000 ("expected ';' after @check"), not a rule-ID diagnostic.
- **Missing comma in @check** — Produces generic PARSE000 ("expected ',' after @check condition expression"), not a rule-ID diagnostic.

### Happy-path scenarios (not testable in lint framework)
- Valid `@check` with literals, CONST, CONFIG, and clog2 — these produce no diagnostics and cannot be tested in the validation framework. They are included as negative testing within the CHECK_INVALID_EXPR_TYPE test.

## From test_9_2-check_semantics.md

### Rules missing from rules.c (untestable)
- **CHECK_NON_CONSTANT** — Test plan references this rule but it does not exist in `rules.c`. The equivalent functionality is covered by CHECK_INVALID_EXPR_TYPE (already tested in 9_1_CHECK_INVALID_EXPR_TYPE-runtime_signal_in_check).

### Happy-path scenarios (not testable in lint framework)
- True @check conditions and CONST-based expressions — produce no diagnostics. Included as negative testing within the CHECK_FAILED test.

## From test_9_3-check_placement_rules.md

### Rules missing from rules.c (untestable)
- **CHECK_WRONG_CONTEXT** — Test plan references this rule but it does not exist in `rules.c`. The compiler uses DIRECTIVE_INVALID_CONTEXT instead.
- **CHECK_INVALID_PLACEMENT** — Defined in `rules.c` but never used in compiler code. Dead rule.

### Contexts limited by compiler bugs
- **@check in ASYNC and SYNC blocks** — Tested but each test produces a cascading PARSE000 due to incomplete parser recovery (see issues.md). Only one context can be tested per file.
- **@check inside @feature body within ASYNC/SYNC blocks** — Not separately tested because the parser abort from the first @check-in-block error prevents reaching @feature contexts in the same file.

### Happy-path scenarios (not testable in lint framework)
- Valid @check at project scope and module scope — produce no diagnostics. Included as negative testing within the placement tests.

## From test_9_4-check_expression_rules.md

### Rules missing from rules.c (untestable)
- **CHECK_RUNTIME_OPERAND** — Test plan references this rule but it does not exist in `rules.c`. The actual rule is CHECK_INVALID_EXPR_TYPE.

### Contexts not covered
- **Memory port field in @check** — S9.4 forbids "any memory port" but structuring a clean test without incidental warnings proved difficult. The rule does fire correctly (confirmed manually) but no clean test exists for this context. See issues.md.

### Happy-path scenarios (not testable in lint framework)
- Valid @check with integer literals, CONST, CONFIG, clog2, comparisons, logical operators — produce no diagnostics. Included as negative testing within the expression rule tests.

## From test_9_5-check_evaluation_order.md

### Contexts not covered
- **Undefined CONST in module-scope @check** — At module scope, undefined identifiers produce UNDECLARED_IDENTIFIER instead of CHECK_INVALID_EXPR_TYPE. Only project-scope triggers are tested for CHECK_INVALID_EXPR_TYPE. See issues.md.

### Happy-path scenarios (not testable in lint framework)
- @check references CONST defined earlier — valid, no diagnostic. Included as negative testing.
- @check references CONFIG from project — valid, no diagnostic. Included as negative testing.
- @check sees OVERRIDE values — valid, no diagnostic. Not testable in lint framework (requires @new with OVERRIDE and @check in child module seeing overridden value, which is a passing scenario).

## From test_9_7-check_error_conditions.md

### Rules missing from rules.c (untestable)
- **CHECK_NON_CONSTANT** — Test plan references this rule ID for runtime signals in @check, but it does not exist in `rules.c`. The actual rule is CHECK_INVALID_EXPR_TYPE, already tested in 9_1_CHECK_INVALID_EXPR_TYPE-runtime_signal_in_check.
- **CHECK_UNDEFINED** — Test plan references this rule ID for undefined identifiers in @check, but it does not exist in `rules.c`. The actual rule is CHECK_INVALID_EXPR_TYPE, already tested in 9_5_CHECK_INVALID_EXPR_TYPE-undefined_const_in_check.

### All scenarios already covered by existing tests
- **CHECK_FAILED (expression evaluates to zero)** — Already tested in 9_2_CHECK_FAILED-false_assertion.jz with 5 triggers across project scope, module scope, literal zero, arithmetic, and CONST comparisons.
- **CHECK_INVALID_EXPR_TYPE (runtime signal)** — Already tested in 9_1_CHECK_INVALID_EXPR_TYPE-runtime_signal_in_check.jz with 5 triggers across port, register, wire, and undefined identifier contexts.
- **CHECK_INVALID_EXPR_TYPE (undefined identifier)** — Already tested in 9_5_CHECK_INVALID_EXPR_TYPE-undefined_const_in_check.jz with 3 triggers at project scope.

### Spec error conditions without rule IDs
- **Non-integer result** — S9.7 lists "Expression produces a non-integer" as an error condition, but no specific rule ID exists in `rules.c` for this case. Would likely be caught by CHECK_INVALID_EXPR_TYPE or a parse error.
- **Disallowed operators** — S9.7 lists "Expression uses operators disallowed in constant expressions" as an error condition, but no specific rule ID exists in `rules.c` for this case. Would likely be caught by CHECK_INVALID_EXPR_TYPE.

## From test_10_3-template_allowed_content.md

### Rules not firing due to compiler bugs
- **TEMPLATE_SCRATCH_WIDTH_INVALID** — Rule exists in `rules.c` but the compiler does not fire it. Zero-width `@scratch` and parameter-based widths are silently accepted. Test file exists (`10_3_TEMPLATE_SCRATCH_WIDTH_INVALID-scratch_width_not_constant.jz`) with empty `.out` matching current behavior. See issues.md.
- **TEMPLATE_EXTERNAL_REF** — Rule exists in `rules.c` but the compiler does not fire it. External signal references in template bodies (wires, registers not passed as params) are silently accepted. Test file exists (`10_3_TEMPLATE_EXTERNAL_REF-external_signal_reference.jz`) with empty `.out` matching current behavior. See issues.md.

## From test_10_4-template_forbidden_content.md

### Contexts not covered due to compiler bugs
- **TEMPLATE_FORBIDDEN_BLOCK_HEADER with block body** — SYNCHRONOUS/ASYNCHRONOUS with `{ ... }` body triggers the rule correctly but also produces cascading PARSE000 errors due to parser recovery failure. Tests use bare keywords without body to avoid cascading errors. See issues.md.
- **TEMPLATE_FORBIDDEN_DIRECTIVE with arguments** — @new, @module, etc. with arguments/body trigger the rule correctly but produce cascading PARSE000 errors. Tests use bare directive keywords. See issues.md.
- **CDC inside template** — Could not be tested as TEMPLATE_FORBIDDEN_DIRECTIVE because the bare `CDC` keyword (without arguments) may not trigger the check, and `CDC` with arguments causes parser recovery issues. See issues.md.

## From test_10_5-template_application.md

### Rules not firing due to compiler bugs
- **TEMPLATE_APPLY_OUTSIDE_BLOCK** — Rule exists in rules.c but never fires. The compiler silently ignores `@apply` at module scope instead of reporting an error. See issues.md.

### Rules not testable (test plan scenarios)
- **IDX in runtime expression** — Test plan lists this as an error case but no corresponding rule ID exists in rules.c for this check.

## From test_10_6-template_exclusive_assignment.md

### Observations
- **Error location deduplication** — When two `@apply` calls to the same template both violate exclusive assignment, the error is reported at the template body location, not the `@apply` callsite. Multiple violations using the same template are deduplicated. See issues.md.
- **ASSIGN_MULTI_DRIVER** — Test plan references this rule ID but it does not exist in rules.c. Actual rules are `ASSIGN_MULTIPLE_SAME_BITS` (ASYNC) and `SYNC_MULTI_ASSIGN_SAME_REG_BITS` (SYNC), both tested.

## From test_11_1-tristate_default_purpose.md

### Rules tested
- **WARN_INTERNAL_TRISTATE** — Tested: internal tri-state on WIRE in helper module, WIRE in top module. Valid negative: INOUT port with z does not trigger.

### Rules not testable via validation framework
- **INFO_TRISTATE_TRANSFORM** — Requires `--tristate-default=GND` or `--tristate-default=VCC` CLI flag. Validation tests only run `--info --lint`, so this info diagnostic cannot be triggered. Needs integration test instead.

### Happy-path scenarios skipped
- `--tristate-default=GND` transforms z to 0 — integration/backend test, not lint validation.
- `--tristate-default=VCC` transforms z to 1 — integration/backend test, not lint validation.
- No flag preserves z (ASIC/sim mode) — no diagnostic to validate.

### Negative tests skipped
- Invalid `--tristate-default` flag value — CLI argument validation, not a lint rule.

## From test_11_2-tristate_default_applicability.md

### Entire test plan untestable via validation framework
All scenarios in Section 11.2 require the `--tristate-default` CLI flag to be passed to the compiler. The validation framework only runs `jz-hdl --info --lint <file>.jz` and has no mechanism to pass additional flags. Scenarios that cannot be tested:
- Internal wire with tri-state → transformed (requires `--tristate-default`)
- INOUT port → NOT transformed by `--tristate-default` (requires the flag)
- Transform applied to INOUT port should be skipped (requires the flag)

These scenarios would need golden tests or a custom test script that invokes the compiler with `--tristate-default=GND` or `--tristate-default=VCC`.

## From test_11_3-tristate_net_identification.md

### Rules that never fire
- **NET_TRI_STATE_ALL_Z_READ** — This rule exists in rules.c but never fires in practice. All-z-driver scenarios are caught earlier by ASYNC_FLOATING_Z_READ. The 11_3 test file exercises this scenario and documents that ASYNC_FLOATING_Z_READ fires instead.

### Scenarios not testable via validation framework
- **Happy path: net with conditional z identified as tri-state** — Identification itself is an internal IR pass. The observable effect is `WARN_INTERNAL_TRISTATE`, which is already tested in 11_1.
- **Happy path: net with no z not identified** — Negative test (no diagnostic to verify). Already covered as negative testing in 11_1 WARN_INTERNAL_TRISTATE test.
- **Edge case: net with z in only one path** — Single conditional z path is already tested in 11_1 (WARN_INTERNAL_TRISTATE fires for any wire with z in any path).

## From test_11_4-tristate_transformation_algorithm.md

### Rules not implemented in compiler
- **TRISTATE_TRANSFORM_PER_BIT_FAIL** — Defined in rules.c but never emitted anywhere in the compiler source. No implementation exists to detect per-bit tri-state patterns. Additionally, all TRISTATE_TRANSFORM rules fire from the IR transformation pass (`ir_tristate_transform.c`), not the lint pass, so they cannot be tested via the `--info --lint` validation framework.

### Scenarios not testable via validation framework
- **Happy path: two-driver priority chain** — Priority-chain conversion is an IR transformation, not a lint diagnostic. No observable diagnostic to validate.
- **Happy path: three-driver priority chain** — Same as above; IR transformation produces no lint diagnostic.

## From test_11_5-tristate_validation_rules.md

### All rules untestable via --lint validation framework
- **TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL** — Fires from IR transformation pass (`ir_tristate_transform.c`) only during backend code generation with `--tristate-default` flag. The validation test runner uses `--info --lint`, which does not invoke IR tristate transformation. Cannot be tested via the current validation framework. Requires golden tests with `--tristate-default=GND` or `--tristate-default=VCC` flag.
- **TRISTATE_TRANSFORM_PER_BIT_FAIL** — Defined in `rules.c` but not implemented in the compiler (no code emits this rule ID). Additionally, requires `--tristate-default` flag which is incompatible with `--info --lint`. Doubly untestable: unimplemented and requires backend mode.

### Happy path scenarios (no diagnostic to test)
- **Mutually exclusive drivers → valid transformation** — IR transformation produces no lint diagnostic; only observable via backend output.
- **Full-width z assignments → valid transformation** — IR transformation produces no lint diagnostic; only observable via backend output.

## From test_11_6-tristate_inout_handling.md

### All rules untestable via --lint validation framework
- **TRISTATE_TRANSFORM_OE_EXTRACT_FAIL** — Fires from IR transformation pass (`ir_tristate_transform.c`) only during backend code generation with `--tristate-default` flag. The validation test runner uses `--info --lint`, which does not invoke IR tristate transformation. Cannot be tested via the current validation framework. Requires golden tests with `--tristate-default=GND` or `--tristate-default=VCC` flag.

### Happy path scenarios (no diagnostic to test)
- **INOUT port preserved with z capability** — IR transformation produces no lint diagnostic; only observable via backend output.
- **Internal driver to INOUT with output-enable extraction** — IR transformation produces no lint diagnostic; only observable via backend output.

## From test_11_7-tristate_error_conditions.md

### All rules untestable via --lint validation framework
- **TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL** — Fires from IR transformation pass (`ir_tristate_transform.c`) only during backend code generation with `--tristate-default` flag. Cannot be tested via the current validation framework.
- **TRISTATE_TRANSFORM_PER_BIT_FAIL** — Fires from IR transformation pass (`ir_tristate_transform.c`) only during backend code generation with `--tristate-default` flag. Cannot be tested via the current validation framework.
- **TRISTATE_TRANSFORM_BLACKBOX_PORT** — Fires from IR transformation pass (`ir_tristate_transform.c`) only during backend code generation with `--tristate-default` flag. Cannot be tested via the current validation framework.
- **TRISTATE_TRANSFORM_OE_EXTRACT_FAIL** — Fires from IR transformation pass (`ir_tristate_transform.c`) only during backend code generation with `--tristate-default` flag. Cannot be tested via the current validation framework.
- **TRISTATE_TRANSFORM_SINGLE_DRIVER** — Fires from IR transformation pass (`ir_tristate_transform.c`) only during backend code generation with `--tristate-default` flag. Cannot be tested via the current validation framework.
- **TRISTATE_TRANSFORM_UNUSED_DEFAULT** — Fires from IR transformation pass (`ir_tristate_transform.c`) only during backend code generation with `--tristate-default` flag. Cannot be tested via the current validation framework.

### Happy path scenarios (no diagnostic to test)
- **Valid tristate transform with mutually exclusive drivers** — IR transformation produces no lint diagnostic; only observable via backend output.
- **Successful OE extraction** — IR transformation produces no lint diagnostic; only observable via backend output.

## From test_11_8-tristate_portability.md

### All scenarios untestable via validation framework
- **WARN_INTERNAL_TRISTATE** — Already tested in `11_1_WARN_INTERNAL_TRISTATE-internal_tristate_warning.jz`. No additional coverage needed from section 11.8.
- **TRISTATE_TRANSFORM_UNUSED_DEFAULT** — Requires `--tristate-default` flag, which is not supported by the `--info --lint` validation framework. Already documented as untestable in test_11_7 section.

### Happy path scenarios (no diagnostic to test)
- **Design with --tristate-default compiles for FPGA** — Behavioral/backend scenario requiring `--tristate-default` flag. Not a lint diagnostic.
- **Same design without flag compiles for ASIC/sim** — Happy path with no diagnostics to verify.
- **Design with no internal tri-state (flag is no-op)** — Would fire TRISTATE_TRANSFORM_UNUSED_DEFAULT but requires `--tristate-default` flag (untestable via validation framework).

## Section 12.2 — Combinational Loop Errors

### All rules already tested under section 5.3
- `COMB_LOOP_UNCONDITIONAL` — Already tested as `5_3_COMB_LOOP_UNCONDITIONAL-unconditional_loop`
- `COMB_LOOP_CONDITIONAL_SAFE` — Already tested as `5_3_COMB_LOOP_CONDITIONAL_SAFE-conditional_safe_cycle`

### Rules referenced in test plan that don't exist in rules.c
- `COMB_LOOP_DIRECT` — Test plan name; actual rule is `COMB_LOOP_UNCONDITIONAL` (already tested)
- `COMB_LOOP_INDIRECT` — Test plan name; actual rule is `COMB_LOOP_UNCONDITIONAL` (already tested, covers multi-signal chains)
- `COMB_LOOP_THROUGH_INSTANCE` — Missing from `rules.c` entirely; cannot be tested

## From test_12_3-recommended_warnings.md

### Rules not implemented in compiler (defined in rules.c but never emitted)
- **WARN_INCOMPLETE_SELECT_ASYNC** — Rule exists in rules.c but `sem_report_rule` is never called with this rule ID anywhere in the compiler source. Cannot be tested.
- **WARN_UNUSED_WIRE** — Rule exists in rules.c but `sem_report_rule` is never called with this rule ID anywhere in the compiler source. Cannot be tested.
- **WARN_UNUSED_PORT** — Rule exists in rules.c but `sem_report_rule` is never called with this rule ID anywhere in the compiler source. Cannot be tested.

### Rules already tested under other sections
- **WARN_UNUSED_REGISTER** — Already tested as `4_7_WARN_UNUSED_REGISTER-unused_register`
- **WARN_UNSINKED_REGISTER** — Already tested as `4_7_WARN_UNSINKED_REGISTER-written_never_read`
- **WARN_UNDRIVEN_REGISTER** — Already tested as `4_7_WARN_UNDRIVEN_REGISTER-read_never_written`
- **WARN_UNCONNECTED_OUTPUT** — Already tested as `12_3_WARN_UNCONNECTED_OUTPUT-unconnected_output_port`
- **WARN_DEAD_CODE_UNREACHABLE** — Already tested as `12_3_WARN_DEAD_CODE_UNREACHABLE-unreachable_branches`
- **WARN_UNUSED_MODULE** — Already tested as `12_3_WARN_UNUSED_MODULE-unused_module`
- **WARN_INTERNAL_TRISTATE** — Already tested as `11_1_WARN_INTERNAL_TRISTATE-internal_tristate_warning`

### Happy-path test
- **12_3_WARN_RECOMMENDED_WARNINGS-clean_design_ok** — Clean design with no warning conditions; verifies no false positives

## From test_12_4-path_security.md

### Rules untestable in validation framework
- **PATH_OUTSIDE_SANDBOX** — Requires a file that exists on the filesystem but resolves outside the sandbox root. The validation test framework runs `jz-hdl --info --lint` with the sandbox root set to the test file's directory; cannot set up the required filesystem state from a `.jz` file alone.
- **PATH_SYMLINK_ESCAPE** — Requires an actual symbolic link on the filesystem that resolves to a target outside the sandbox root. Cannot be set up from a `.jz` file alone; would require a shell-level integration test.

### Contexts limited by compiler behavior
- **PATH_ABSOLUTE_FORBIDDEN and PATH_TRAVERSAL_FORBIDDEN with multiple @import triggers** — A failing `@import` aborts compilation before reaching subsequent `@import` or MEM declarations. Each context (import vs @file()) must be tested in separate files. Both contexts are tested but in separate test files rather than a single multi-trigger file.

## From test_10_1-template_purpose.md

### No rules to test
- Section 10.1 is a conceptual overview of the template system. No rule IDs are defined for this section — all template rules are in sections 10.2–10.8. A happy-path test (`10_1_TEMPLATE_PURPOSE-simple_template_ok`) verifies clean template expansion.

## From test_11_2-tristate_default_applicability.md

### No rules to test
- Section 11.2 defines the scope of applicability for the `--tristate-default` flag (internal WIREs only, not INOUT ports or top-level pins). No diagnostic rule IDs are defined for this subsection — it is a scope definition, not a diagnosable condition. No validation tests can be created.

## From test_11_4-tristate_transformation_algorithm.md

### All rules untestable in validation framework
All rules in section 11.4 require the `--tristate-default` flag, which is outside the `--info --lint` framework used by the validation test runner. No validation tests can be created.

- **TRISTATE_TRANSFORM_PER_BIT_FAIL** (error) — Requires `--tristate-default` flag to trigger per-bit tri-state pattern detection
- **TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL** (error) — Requires `--tristate-default` flag to trigger mutual exclusion checking
- **TRISTATE_TRANSFORM_SINGLE_DRIVER** (warning) — Requires `--tristate-default` flag to trigger single-driver transformation warning
- **INFO_TRISTATE_TRANSFORM** (info) — Requires `--tristate-default` flag to trigger transformation info message

## From test_11_6-tristate_inout_handling.md

### All rules untestable in validation framework
All rules in section 11.6 require the `--tristate-default` flag, which is outside the `--info --lint` framework used by the validation test runner. No validation tests can be created.

- **TRISTATE_TRANSFORM_OE_EXTRACT_FAIL** (error) — Requires `--tristate-default` flag to trigger OE extraction from tri-state INOUT ports
- **TRISTATE_TRANSFORM_BLACKBOX_PORT** (error) — Requires `--tristate-default` flag to trigger blackbox port tri-state transformation

## From test_11_7-tristate_error_conditions.md

### All rules untestable in validation framework
All rules in section 11.7 require the `--tristate-default` flag, which is outside the `--info --lint` framework used by the validation test runner. No validation tests can be created.

- **TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL** (error) — Requires `--tristate-default` flag to trigger mutual exclusion checking
- **TRISTATE_TRANSFORM_PER_BIT_FAIL** (error) — Requires `--tristate-default` flag to trigger per-bit tri-state pattern detection
- **TRISTATE_TRANSFORM_BLACKBOX_PORT** (error) — Requires `--tristate-default` flag to trigger blackbox port tri-state transformation
- **TRISTATE_TRANSFORM_OE_EXTRACT_FAIL** (error) — Requires `--tristate-default` flag to trigger OE extraction failure
- **TRISTATE_TRANSFORM_SINGLE_DRIVER** (warning) — Requires `--tristate-default` flag to trigger single-driver transformation warning
- **TRISTATE_TRANSFORM_UNUSED_DEFAULT** (warning) — Requires `--tristate-default` flag to trigger unused default warning

## From test_11_2-tristate_default_applicability.md

Section 11.2 defines the scope of applicability for the `--tristate-default` flag (internal WIREs only, not INOUT ports or top-level pins). No diagnostic rule IDs are defined for this subsection — it is a scope definition, not a diagnosable condition. No validation tests to create.

## From test_11_8-tristate_portability.md

Section 11.8 provides portability best practices for tri-state designs (using `--tristate-default` for FPGA targets, reserving INOUT for external interfaces). No new diagnostic rule IDs are defined — it cross-references WARN_INTERNAL_TRISTATE (from 11.1) and TRISTATE_TRANSFORM_UNUSED_DEFAULT (from 11.7), both of which are tested in their respective section test plans. No validation tests to create.

## From test_12_1-compile_errors.md

Section 12.1 is an aggregation/summary section cross-referencing error-severity rules from Sections 1-11. No individual rule IDs are defined specifically for this section. All error rules are tested in their respective section test plans. Two aggregation-level tests were created (happy-path and multiple-errors). Edge case "error in imported file" is not applicable — JZ-HDL uses `@new` instantiation, not file imports.

## From test_12_3-recommended_warnings.md

### Rules defined in rules.c but never emitted by the compiler
- **WARN_INCOMPLETE_SELECT_ASYNC** (warning) — Rule ID exists in rules.c ("S5.4/S8.3 Incomplete SELECT coverage without DEFAULT in ASYNCHRONOUS block") but is never referenced in any semantic pass. The related behavior is covered by SELECT_DEFAULT_RECOMMENDED_ASYNC (tested in `5_4_SELECT_DEFAULT_RECOMMENDED_ASYNC-async_select_no_default`).
- **WARN_UNUSED_WIRE** (warning) — Rule ID exists in rules.c ("S12.3 WIRE declared but never driven or read; remove it if unused") but is never referenced in any semantic pass. Unused wires are caught by NET_DANGLING_UNUSED instead.
- **WARN_UNUSED_PORT** (warning) — Rule ID exists in rules.c ("S12.3 PORT declared but never used; remove it if unused") but is never referenced in any semantic pass. No equivalent diagnostic fires for unused ports.

### Rules tested in other sections
- **WARN_INTERNAL_TRISTATE** — Tested in `11_1_WARN_INTERNAL_TRISTATE-internal_tristate_warning` (section 11.1).

## From test_3_3-operator_precedence.md

### No testable rules

Section 3.3 (Operator Precedence) defines parse behavior, not diagnostic rules. Operator precedence is verified by AST structure comparison, not by lint diagnostics. The validation test framework (`--info --lint`) only tests diagnostic rule output, so no `.jz`/`.out` pairs can be created for this section. The test plan's Rules Matrix confirms: "None specific — operator precedence is verified by AST structure comparison, not diagnostic rules."
