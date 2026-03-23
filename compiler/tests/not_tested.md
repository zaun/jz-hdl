# Untested Rules

## Section 4.1 — Module Canonical Form

No untested rules for section 4.1. All 4 rules from the test plan have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| DIRECTIVE_INVALID_CONTEXT | error | Tested — 6 triggers across @module, @project, @blackbox, @import, @global, @endproj inside module scope across 2 modules |
| DUPLICATE_BLOCK | error | Tested — 2 triggers for duplicate SYNCHRONOUS blocks with same clock across 2 modules; different clocks as negative test |
| MODULE_MISSING_PORT | error | Tested — 2 triggers for missing PORT block and empty PORT block |
| MODULE_PORT_IN_ONLY | warning | Tested — 2 triggers for single-IN and multi-IN modules with no OUT/INOUT |

Happy-path coverage of full canonical form (all sections), minimal module, modules without CONST/REGISTER/WIRE, multiple SYNCHRONOUS blocks with different clocks, and SYNCHRONOUS header variants is provided by `4_1_MODULE_CANONICAL_FORM-full_canonical_ok.jz`.

## Section 4.2 — Scope and Uniqueness

All 5 implemented rules from the test plan have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| ID_DUP_IN_MODULE | error | Tested — 6 triggers across port-port, reg-const, const-const, wire-port, wire-wire, reg-reg duplicates across 3 modules |
| MODULE_NAME_DUP_IN_PROJECT | error | Tested — 2 triggers for second and third definition of same module name; unique module as negative test |
| BLACKBOX_NAME_DUP_IN_PROJECT | error | Tested — 2 triggers for bb-bb duplicate and bb-module name conflict; WARN_UNUSED_MODULE co-fires |
| INSTANCE_NAME_DUP_IN_MODULE | error | Tested — 2 triggers for duplicate instance names across child and top modules |
| INSTANCE_NAME_CONFLICT | error | Tested — 5 triggers across port, wire, register, CONST, and top-wire collisions across 2 modules |
| UNDECLARED_IDENTIFIER | error | Tested — 5 triggers across async RHS, sync RHS, CLK parameter, @new binding, instance port reference across 4 modules |

Happy-path coverage of same signal names in different modules, module-local CONST isolation, unique identifiers across all declaration types (PORT, WIRE, REGISTER, CONST), and unique instance names is provided by `4_2_SCOPE_HAPPY_PATH-unique_names_ok.jz`.

| Rule ID | Severity | Reason |
|---------|----------|--------|
| AMBIGUOUS_REFERENCE | error | Not implemented — rule defined in `rules.c` but no semantic pass emits it; cannot be tested (see `issues.md`) |

No untested rules for section 10.1. This section defines no rule IDs; it is a conceptual overview. Happy-path coverage is provided by the validation test.

No untested rules for section 10.3. All rules have corresponding validation test files. However, `TEMPLATE_EXTERNAL_REF` and `TEMPLATE_SCRATCH_WIDTH_INVALID` are not yet enforced by the compiler — their test `.out` files are empty (see `issues.md` for details).

No untested rules for section 10.4. All rules from the test plan have corresponding validation tests.

No untested rules for section 10.2. All rules from the test plan (TEMPLATE_DUP_NAME, TEMPLATE_DUP_PARAM) have corresponding validation tests.

No untested rules for section 10.5. All rules have corresponding validation test files. However, `TEMPLATE_APPLY_OUTSIDE_BLOCK` is not yet enforced by the compiler — its test `.out` files are empty (see `issues.md` for details).

No untested rules for section 10.6. All rules from the test plan (`ASSIGN_MULTIPLE_SAME_BITS`, `SYNC_MULTI_ASSIGN_SAME_REG_BITS`) have corresponding validation tests.

No untested rules for section 10.8. This section is a cross-reference summary of all TEMPLATE_* error cases from S10.2-S10.6. All 15 rule IDs are covered by existing tests in their respective subsections.

No untested rules for section 11.1. All three rules from the test plan (`WARN_INTERNAL_TRISTATE`, `INFO_TRISTATE_TRANSFORM`, `TRISTATE_TRANSFORM_UNUSED_DEFAULT`) have corresponding validation tests. The test plan originally listed `INFO_TRISTATE_TRANSFORM` and `TRISTATE_TRANSFORM_UNUSED_DEFAULT` as untestable via `--info --lint`, but `--tristate-default` now works with `--lint` mode, and the validation runner supports `_GND_`/`_VCC_` filename patterns to pass the flag automatically.

No untested rules for section 11.2. This section defines the scope of applicability for `--tristate-default` (internal WIREs only, not INOUT ports or top-level pins). It introduces no diagnostic rule IDs — applicability is a behavioral specification, not a diagnosable condition.

No untested rules for section 11.3. Both rules from the test plan (`NET_MULTIPLE_ACTIVE_DRIVERS`, `NET_TRI_STATE_ALL_Z_READ`) have corresponding validation tests. Note: `NET_TRI_STATE_ALL_Z_READ` is never emitted by the compiler — `ASYNC_FLOATING_Z_READ` fires first for the same scenario (see `issues.md` for details). The test validates the behavior under the `NET_TRI_STATE_ALL_Z_READ` filename but expects `ASYNC_FLOATING_Z_READ` in its `.out` file.

## Section 11.4 — Transformation Algorithm

All four rules from the test plan have corresponding validation test files, but three are not effectively tested because the compiler does not emit the expected diagnostics:

| Rule ID | Severity | Status |
|---------|----------|--------|
| INFO_TRISTATE_TRANSFORM | info | Tested — works correctly with `--tristate-default=GND` and `--tristate-default=VCC` |
| TRISTATE_TRANSFORM_SINGLE_DRIVER | warning | Not emitted — rule defined in `rules.c` but compiler never emits it; test `.out` captures `INFO_TRISTATE_TRANSFORM` instead (see `issues.md`) |
| TRISTATE_TRANSFORM_PER_BIT_FAIL | error | Not emitted — rule defined in `rules.c` but compiler never emits it; per-bit patterns are transformed without error (see `issues.md`) |
| TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL | error | Unreachable — `NET_MULTIPLE_ACTIVE_DRIVERS` fires first at semantic analysis, preventing multi-driver patterns from reaching the tri-state transform (see `issues.md`) |

Additionally, the multi-driver priority chain scenarios (two-driver and three-driver from the test plan Happy Path 1 and 2) cannot be expressed in JZ-HDL because both `ASSIGN_INDEPENDENT_IF_SELECT` and `NET_MULTIPLE_ACTIVE_DRIVERS` block the patterns before the tri-state transform phase.

## Section 11.5 — Tri-State Validation Rules

No new tests needed. Section 11.5 is a cross-reference to the same two rules already tested under section 11.4:

| Rule ID | Severity | Status |
|---------|----------|--------|
| TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL | error | Tested under 11.4 (`11_GND_7_TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL-non_exclusive.jz`); unreachable — `NET_MULTIPLE_ACTIVE_DRIVERS` fires first (see `issues.md`) |
| TRISTATE_TRANSFORM_PER_BIT_FAIL | error | Tested under 11.4 (`11_GND_7_TRISTATE_TRANSFORM_PER_BIT_FAIL-per_bit_tristate.jz`); not emitted — compiler transforms per-bit patterns without error (see `issues.md`) |

## Section 11.6 — Handling of INOUT Ports and External Pins

| Rule ID | Severity | Status |
|---------|----------|--------|
| TRISTATE_TRANSFORM_OE_EXTRACT_FAIL | error | Tested — fires when OE extractor cannot parse z pattern (e.g., concatenated z literals); test `11_GND_7_TRISTATE_TRANSFORM_OE_EXTRACT_FAIL-ambiguous_oe.jz` |
| TRISTATE_TRANSFORM_BLACKBOX_PORT | error | Not implemented — rule defined in `rules.c` but no code in `ir_tristate_transform.c` emits it; blackbox modules are not yet handled by the tri-state transform (see `issues.md`) |

## Section 11.7 — Error Conditions and Warnings

All 6 rules from this section have corresponding validation test files under `11_GND_7_*`. This section is the comprehensive error matrix — all rules are cross-references to rules already covered in earlier sections.

| Rule ID | Severity | Status |
|---------|----------|--------|
| TRISTATE_TRANSFORM_UNUSED_DEFAULT | warning | Tested — works correctly; `11_GND_7_TRISTATE_TRANSFORM_UNUSED_DEFAULT-no_tristate_nets.jz` |
| TRISTATE_TRANSFORM_OE_EXTRACT_FAIL | error | Tested — works correctly; `11_GND_7_TRISTATE_TRANSFORM_OE_EXTRACT_FAIL-ambiguous_oe.jz` |
| TRISTATE_TRANSFORM_SINGLE_DRIVER | warning | Not emitted — test captures `INFO_TRISTATE_TRANSFORM` only; `11_GND_7_TRISTATE_TRANSFORM_SINGLE_DRIVER-single_driver.jz` (see `issues.md`) |
| TRISTATE_TRANSFORM_PER_BIT_FAIL | error | Not emitted — test captures `INFO_TRISTATE_TRANSFORM` only; `11_GND_7_TRISTATE_TRANSFORM_PER_BIT_FAIL-per_bit_tristate.jz` (see `issues.md`) |
| TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL | error | Unreachable — test captures `NET_MULTIPLE_ACTIVE_DRIVERS`; `11_GND_7_TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL-non_exclusive.jz` (see `issues.md`) |
| TRISTATE_TRANSFORM_BLACKBOX_PORT | error | Not implemented — test captures `TRISTATE_TRANSFORM_OE_EXTRACT_FAIL`; `11_GND_7_TRISTATE_TRANSFORM_BLACKBOX_PORT-blackbox_tristate.jz` (see `issues.md`) |

## Section 11.8 — Tristate Portability Guidelines

No tests generated for section 11.8. This section provides portability best practices and introduces no new diagnostic rules. All referenced rules (`WARN_INTERNAL_TRISTATE`, `INFO_TRISTATE_TRANSFORM`, `TRISTATE_TRANSFORM_UNUSED_DEFAULT`) are already tested under their defining sections (11.1, 11.4, 11.7).

## Section 12.1 — Compile Errors

No untested rules for section 12.1. This section is a cross-reference aggregation of error-severity rules from all other sections. Happy-path and multiple-error regression tests validate that the compiler reports errors from different categories simultaneously. No section-specific rule IDs exist — all rules are defined in their respective sections (1.x through 11.x).

## Section 12.2 — Combinational Loop Errors

No untested rules for section 12.2. Both rules from the test plan (`COMB_LOOP_UNCONDITIONAL`, `COMB_LOOP_CONDITIONAL_SAFE`) have corresponding validation tests that pass.

## Section 12.3 — Recommended Warnings

No untested rules for section 12.3. All 10 warning rules from the test plan have corresponding validation test files. However, three rules are not emitted by the compiler under their planned rule IDs:

| Rule ID | Severity | Status |
|---------|----------|--------|
| WARN_UNUSED_REGISTER | warning | Tested — works correctly |
| WARN_UNSINKED_REGISTER | warning | Tested — works correctly |
| WARN_UNDRIVEN_REGISTER | warning | Tested — works correctly |
| WARN_UNCONNECTED_OUTPUT | warning | Tested — works correctly |
| WARN_DEAD_CODE_UNREACHABLE | warning | Tested — works correctly |
| WARN_UNUSED_MODULE | warning | Tested — works correctly |
| WARN_INTERNAL_TRISTATE | warning | Tested — works correctly |
| WARN_INCOMPLETE_SELECT_ASYNC | warning | Not emitted — `SELECT_DEFAULT_RECOMMENDED_ASYNC` fires instead (see `issues.md`) |
| WARN_UNUSED_WIRE | warning | Not emitted — `NET_DANGLING_UNUSED` fires instead (see `issues.md`) |
| WARN_UNUSED_PORT | warning | Not emitted — `NET_DANGLING_UNUSED` fires instead (see `issues.md`) |

## Section 12.4 — Path Security

| Rule ID | Severity | Reason |
|---------|----------|--------|
| PATH_OUTSIDE_SANDBOX | error | Requires filesystem sandbox configuration at runtime; cannot be tested with `--info --lint` against a static `.jz` file |
| PATH_SYMLINK_ESCAPE | error | Requires actual symlinks on the filesystem; cannot be tested with `--info --lint` against a static `.jz` file |

## Section 2.2 — Signedness Model

No untested rules for section 2.2. The only rule from the test plan (`TYPE_BINOP_WIDTH_MISMATCH`) has a corresponding validation test (`2_2_TYPE_BINOP_WIDTH_MISMATCH-width_mismatch.jz`) with 4 triggers across comparison and arithmetic operators in ASYNC and SYNC blocks across 2 modules. The `signed` keyword does not exist in JZ-HDL (by design) — attempting to use it would be a parse error, but no dedicated rule ID exists for it. Happy-path coverage of all unsigned operators and sadd/smul intrinsics is provided by `2_2_SIGNEDNESS_HAPPY_PATH-unsigned_arithmetic_ok.jz`.

## Section 2.1 — Literals

No untested rules for section 2.1. All 8 literal rules from the test plan have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| LIT_UNSIZED | error | Tested — 4 triggers across async RHS, sync RHS, register init, sync expression |
| LIT_BARE_INTEGER | error | Tested — 2 triggers in sync RHS (async blocked by ASYNC_ALIAS_LITERAL_RHS, see `issues.md`) |
| LIT_UNDERSCORE_AT_EDGES | error | Tested — 4 triggers across async RHS, sync RHS, register init (leading + trailing) |
| LIT_UNDEFINED_CONST_WIDTH | error | Tested — 3 triggers across sync RHS, register init |
| LIT_WIDTH_NOT_POSITIVE | error | Tested — 3 triggers across sync RHS, register init |
| LIT_OVERFLOW | error | Tested — 3 triggers across sync RHS (binary, decimal, hex) |
| LIT_DECIMAL_HAS_XZ | error | Tested — 4 triggers across async RHS, sync RHS, register init (x + z) |
| LIT_INVALID_DIGIT_FOR_BASE | error | Tested — 3 triggers across async RHS, sync RHS, register init |

## Section 1.1 — Identifiers

No untested rules for section 1.1. All three identifier-related rules (`ID_SYNTAX_INVALID`, `ID_SINGLE_UNDERSCORE`, `KEYWORD_AS_IDENTIFIER`) have corresponding validation tests. However:

- WIRE block name contexts are not validated by the compiler for any of these rules (see `issues.md`)
- VCC and GND produce `PARSE000` instead of `KEYWORD_AS_IDENTIFIER` (see `issues.md`)
- Most spec-listed reserved identifiers (PLL, DLL, CLKDIV, BIT, BUS, FIFO, etc.) are not reserved in the compiler — only IDX is (see `issues.md`)
- Lexer-level errors (identifier starting with digit, special characters like hyphens) produce no rule ID from `rules.c` and cannot be tested via the validation framework
- Non-ASCII character handling is not tested (ambiguous in test plan; lexer behavior with non-ASCII bytes is undefined)

## Section 1.4 — Comments

No untested rules for section 1.4. Both comment-related rules have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| COMMENT_IN_TOKEN | error | Tested — works correctly (6 triggers across 5 contexts) |
| COMMENT_NESTED_BLOCK | error | Tested — works correctly (3 triggers across 3 contexts) |

Unterminated block comments (`/* no end`) are handled by the lexer as a generic error with no rule ID in `rules.c` — cannot be tested via the validation framework.

## Section 1.2 — Fundamental Terms

No untested rules for section 1.2. All 7 error/warning rules from the test plan have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| NET_FLOATING_WITH_SINK | error | Tested — works correctly |
| ASYNC_FLOATING_Z_READ | error | Tested — works correctly |
| OBS_X_TO_OBSERVABLE_SINK | error | Tested — works correctly |
| REG_INIT_CONTAINS_X | error | Tested — works correctly |
| DOMAIN_CONFLICT | error | Tested — works correctly |
| MULTI_CLK_ASSIGN | error | Tested — works correctly |
| LATCH_ASSIGN_IN_SYNC | error | Tested — works correctly |
| NET_DANGLING_UNUSED | warning | Tested — works correctly |

Rules from the "Not Tested" section of the test plan:
- `NET_TRI_STATE_ALL_Z_READ` — overlaps with `ASYNC_FLOATING_Z_READ` (tested under section 11.3)
- `NET_MULTIPLE_ACTIVE_DRIVERS` — tested under section 1.5 exclusive assignment tests
- `NET_DANGLING_UNUSED` — now has a dedicated test (`1_2_NET_DANGLING_UNUSED-unused_signal.jz`)

## Section 1.5 — Exclusive Assignment Rule

No untested rules for section 1.5. All 5 exclusive assignment rules from the test plan have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| ASSIGN_MULTIPLE_SAME_BITS | error | Tested — works correctly (port and wire in ASYNC, multi-module) |
| ASSIGN_INDEPENDENT_IF_SELECT | error | Tested — works correctly (independent IFs on port/wire, independent SELECTs, multi-module) |
| ASSIGN_SHADOWING | error | Tested — works correctly (root-then-IF on port/wire, root-then-SELECT, multi-module) |
| ASSIGN_SLICE_OVERLAP | error | Tested — works correctly (port/wire slices in ASYNC, register slices in SYNC, multi-module) |
| ASYNC_UNDEFINED_PATH_NO_DRIVER | error | Tested — works correctly (IF-no-ELSE on port/wire, SELECT-no-DEFAULT, multi-module) |

## Section 1.3 — Bit Slicing and Indexing

No untested rules for section 1.3. All four testable rules from the test plan have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| SLICE_MSB_LESS_THAN_LSB | error | Tested — works correctly |
| SLICE_INDEX_OUT_OF_RANGE | error | Tested — works correctly |
| CONST_UNDEFINED_IN_WIDTH_OR_SLICE | error | Tested — works correctly |
| SPECIAL_DRIVER_SLICED | error | Tested — works correctly |

| Rule ID | Severity | Reason |
|---------|----------|--------|
| SLICE_INDEX_INVALID | error | No dedicated test; fires when a literal in a slice position fails to parse as a non-negative integer. Normal JZ-HDL syntax restricts slice indices to bare decimal integers or identifiers, making this rule difficult to trigger through valid parser input. The parser rejects most non-integer slice constructs before semantic analysis. |

## Section 1.6 — High-Impedance and Tri-State Logic

No untested rules for section 1.6. All four directly testable rules have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| LIT_DECIMAL_HAS_XZ | error | Tested — 5 triggers across async, const, reg init, instance port, sync contexts |
| LIT_INVALID_DIGIT_FOR_BASE | error | Tested — 5 triggers across async, const, reg init, instance port, sync contexts |
| PORT_TRISTATE_MISMATCH | error | Tested — 2 triggers (helper OUT, top OUT) + WARN_INTERNAL_TRISTATE co-fires + negative wire test |
| REG_INIT_CONTAINS_Z | error | Tested — 5 triggers across 2 modules (1-bit, full-width, partial, mixed, extension) |

Rules from the "Not Tested" section of the test plan:
- `NET_MULTIPLE_ACTIVE_DRIVERS` — tested under section 11.3 (`11_3_NET_MULTIPLE_ACTIVE_DRIVERS-multi_driver_non_tristate.jz`)
- `NET_TRI_STATE_ALL_Z_READ` — tested under section 11.3 (`11_3_NET_TRI_STATE_ALL_Z_READ-all_drivers_z_but_read.jz`); compiler emits `ASYNC_FLOATING_Z_READ` instead (see `issues.md` section 11.3)

## Section 2.4 — Special Semantic Drivers

No untested rules for section 2.4. All four rules from the test plan have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| SPECIAL_DRIVER_IN_EXPRESSION | error | Tested — 6 triggers across binary add, binary or, unary NOT, ternary condition in ASYNC and SYNC blocks across 2 modules |
| SPECIAL_DRIVER_IN_CONCAT | error | Tested — 4 triggers across GND/VCC in concatenation in ASYNC and SYNC blocks across 2 modules |
| SPECIAL_DRIVER_SLICED | error | Tested — 5 triggers across range slice and single-bit index of GND/VCC in ASYNC and SYNC blocks across 2 modules |
| SPECIAL_DRIVER_IN_INDEX | error | Parser emits PARSE000 instead — test captures parser behavior; rule is unreachable (see `issues.md`) |

## Section 2.3 — Bit-Width Constraints

No untested rules for section 2.3. All 6 rules from the test plan have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| TYPE_BINOP_WIDTH_MISMATCH | error | Tested — 6 triggers across ADD, EQ, SUB, OR, XOR, MUL operators in ASYNC and SYNC blocks across 2 modules |
| ASSIGN_WIDTH_NO_MODIFIER | error | Tested — 6 triggers across alias (=) and receive (<=) assignments with narrower-to-wider and wider-to-narrower mismatches in ASYNC and SYNC blocks across 2 modules |
| ASSIGN_CONCAT_WIDTH_MISMATCH | error | Tested — 5 triggers across RHS concat too narrow, RHS concat too wide, LHS concat mismatch, in ASYNC and SYNC blocks across 2 modules |
| TERNARY_BRANCH_WIDTH_MISMATCH | error | Tested — 5 triggers across direct assignment, literal arm, SYNC block contexts in ASYNC and SYNC blocks across 2 modules |
| WIDTH_NONPOSITIVE_OR_NONINT | error | Tested — 4 triggers across zero-width WIRE and REGISTER declarations in 2 modules |
| WIDTH_ASSIGN_MISMATCH_NO_EXT | error | Covered by ASSIGN_WIDTH_NO_MODIFIER (both fire together in `driver_assign.c` but ASSIGN_WIDTH_NO_MODIFIER takes priority in output) |

## Section 3.4 — Operator Examples

No untested rules for section 3.4. This section provides illustrative operator examples and introduces no new rules beyond S3.1-S3.2. Both rules referenced in the test plan have corresponding validation tests under the `3_4_` prefix:

| Rule ID | Severity | Status |
|---------|----------|--------|
| UNARY_ARITH_MISSING_PARENS | error | Tested — 5 triggers across unary minus/plus in ASYNC and SYNC blocks across 2 modules |
| TERNARY_BRANCH_WIDTH_MISMATCH | error | Tested — 5 triggers across concat-vs-scalar mismatches in ASYNC and SYNC blocks across 2 modules |

Happy-path coverage of all S3.4 canonical examples (unary negation, subtraction negation, logical/arithmetic shift, carry capture, ternary+concat, tri-state driver) is provided by `3_4_OPERATOR_EXAMPLES-spec_examples_ok.jz`.

## Section 3.3 — Operator Precedence

No tests generated for section 3.3. This section defines the 15-level operator precedence hierarchy as parser behavior, not diagnostic rules. Correctness is verified by AST structure comparison, not by compiler diagnostics. The `--info --lint` validation framework cannot test precedence because it only captures diagnostic output. No rule IDs exist in `rules.c` for operator precedence.

No untested rules for section 3.1. Both rules from the test plan (`LOGICAL_WIDTH_NOT_1`, `TYPE_BINOP_WIDTH_MISMATCH`) have corresponding validation tests. The happy-path test covers all operator categories (unary arithmetic, binary arithmetic, multiply, divide, modulus, bitwise, logical, comparison, shift, ternary, concatenation).

## Section 3.2 — Operator Definitions

No untested rules for section 3.2. All 8 rules from the test plan have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| UNARY_ARITH_MISSING_PARENS | error | Tested — 5 triggers across unary minus/plus in ASYNC and SYNC blocks across 2 modules |
| LOGICAL_WIDTH_NOT_1 | error | Tested — 4 triggers across &&, \|\|, ! with multi-bit operands in ASYNC and SYNC blocks across 2 modules |
| TERNARY_COND_WIDTH_NOT_1 | error | Tested — 4 triggers with 8-bit condition in ASYNC and SYNC blocks across 2 modules |
| TERNARY_BRANCH_WIDTH_MISMATCH | error | Tested — 4 triggers with mismatched 8-bit/4-bit branches in ASYNC and SYNC blocks across 2 modules |
| CONCAT_EMPTY | error | Tested — 4 triggers with empty `{}` in ASYNC and SYNC blocks across 2 modules |
| DIV_CONST_ZERO | error | Tested — 4 triggers with `/` and `%` by literal zero in SYNC blocks across 2 modules |
| DIV_UNGUARDED_RUNTIME_ZERO | warning | Tested — 4 triggers with unguarded `/` and `%` in SYNC blocks across 2 modules; guarded patterns (!=, >, ==nonzero, nested) verify no false positives |
| OBS_X_TO_OBSERVABLE_SINK | error | Tested — 5 triggers with x-bit literals in addition, OR, ternary, concatenation, subtraction driving registers across 2 modules |

## Section 4.10 — ASYNCHRONOUS Block

No untested rules for section 4.10. All 6 testable rules from the test plan have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| ASYNC_ALIAS_LITERAL_RHS | error | Tested — 5 triggers across =, =z, =s with literal RHS in ASYNC blocks across 3 modules |
| ASYNC_INVALID_STATEMENT_TARGET | error | Tested — 3 triggers via MEM SYNC OUT .addr assigned in ASYNC blocks across 3 modules |
| ASYNC_ASSIGN_REGISTER | error | Tested — 3 triggers across receive (<=), alias (=), and drive (=>) to registers in ASYNC blocks across 3 modules |
| ASYNC_ALIAS_IN_CONDITIONAL | error | Tested — 3 triggers across alias = in IF body, ELSE body, and SELECT/CASE body in ASYNC blocks across 3 modules |
| ASYNC_FLOATING_Z_READ | error | Tested — 2 triggers for wire driven only by z but read, across 2 modules; WARN_INTERNAL_TRISTATE co-fires as expected |
| ASYNC_UNDEFINED_PATH_NO_DRIVER | error | Tested — 3 triggers across IF-without-ELSE and SELECT-without-DEFAULT in ASYNC blocks across 3 modules; SELECT_DEFAULT_RECOMMENDED_ASYNC co-fires as expected |

Happy-path coverage of all valid ASYNCHRONOUS constructs (net alias, receive, drive, sliced, extension modifiers =z/=s/<=z/<=s/=>z/=>s, transitive alias, ternary, register read, empty ASYNC block) is provided by `4_10_ASYNC_HAPPY_PATH-async_block_ok.jz`.

| Rule ID | Severity | Reason |
|---------|----------|--------|
| WIDTH_ASSIGN_MISMATCH_NO_EXT | error | Suppressed by higher-priority `ASSIGN_WIDTH_NO_MODIFIER` in `--info --lint` output (see `issues.md`). Covered by section 2.3 tests (`2_3_ASSIGN_WIDTH_NO_MODIFIER-assign_width_mismatch.jz`) |

## Section 4.11 — SYNCHRONOUS Block

No untested rules for section 4.11. All 13 rules from the test plan have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| SYNC_CLK_WIDTH_NOT_1 | error | Tested — 2 triggers across helper and top modules with 8-bit CLK port |
| SYNC_RESET_WIDTH_NOT_1 | error | Tested — 2 triggers across helper and top modules with 8-bit RESET port |
| SYNC_EDGE_INVALID | error | Tested — 2 triggers with invalid EDGE values ("Positive", "Negative") across 2 modules; valid Rising/Falling as negative tests |
| SYNC_RESET_ACTIVE_INVALID | error | Tested — 2 triggers with invalid RESET_ACTIVE values ("Rising", "Positive") across 2 modules; valid High/Low as negative tests |
| SYNC_RESET_TYPE_INVALID | error | Tested — 2 triggers with invalid RESET_TYPE values ("Async", "Synchronous") across 2 modules; valid Clocked/Immediate as negative tests |
| SYNC_UNKNOWN_PARAM | error | Tested — 2 triggers with unknown params ("MODE", "PRIORITY") across 2 modules; valid CLK+EDGE as negative test |
| SYNC_MISSING_CLK | error | Tested — 2 triggers (empty parens, RESET-only) across 2 modules; valid CLK as negative test |
| SYNC_EDGE_BOTH_WARNING | warning | Tested — 2 triggers with EDGE=Both across 2 modules; valid Rising/Falling as negative tests |
| DOMAIN_CONFLICT | error | Tested — 2 triggers for register read in wrong clock domain across 2 modules; valid same-domain use as negative test |
| DUPLICATE_BLOCK | error | Tested — 2 triggers for duplicate SYNCHRONOUS(CLK=clk) blocks across 2 modules; different clocks as negative test |
| MULTI_CLK_ASSIGN | error | Tested — 2 triggers for same register assigned in two clock domains across 2 modules; DOMAIN_CONFLICT co-fires as expected |
| WRITE_WIRE_IN_SYNC | error | Tested — 5 triggers across SYNC root (2 modules), IF body, ELSE body, SELECT/CASE body; WIRE in ASYNC as negative test; SELECT_NO_MATCH_SYNC_OK co-fires |
| ASSIGN_TO_NON_REGISTER_IN_SYNC | error | Tested — 2 triggers for OUT port assigned in SYNCHRONOUS block across 2 modules; REGISTER in SYNC as negative test |

Happy-path coverage of all valid SYNCHRONOUS constructs (Rising/Falling edge, Immediate/Clocked reset, RESET_ACTIVE High/Low, multiple SYNC blocks with different clocks, register read/write counter pattern, register read in ASYNC) is provided by `4_11_SYNC_HAPPY_PATH-valid_sync_block_ok.jz`.

## Section 4.12 — CDC Block

No untested rules for section 4.12. All 9 CDC rules from the test plan have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| CDC_BIT_WIDTH_NOT_1 | error | Tested — 2 triggers across helper and top modules with multi-bit BIT CDC source |
| CDC_PULSE_WIDTH_NOT_1 | error | Tested — 2 triggers across helper and top modules with multi-bit PULSE CDC source |
| CDC_RAW_STAGES_FORBIDDEN | error | Tested — 2 triggers across helper and top modules with RAW type using stages |
| CDC_TYPE_INVALID | error | Tested — 2 triggers across helper and top modules with invalid CDC type keywords |
| CDC_STAGES_INVALID | error | Tested — 2 triggers across helper and top modules with zero and non-integer stages |
| CDC_SOURCE_NOT_REGISTER | error | Tested — 2 triggers with wire and port as CDC source across 2 modules |
| CDC_DEST_ALIAS_DUP | error | Tested — 2 triggers with alias conflicting with register and port names; ID_DUP_IN_MODULE co-fires |
| CDC_SOURCE_NOT_PLAIN_REG | error | Not emitted — rule defined in `rules.c` but compiler silently drops CDC entries with sliced source; test captures `WARN_UNSINKED_REGISTER` instead (see `issues.md`) |
| CDC_DEST_ALIAS_ASSIGNED | error | Not emitted — rule defined in `rules.c` but compiler does not detect writes to CDC dest alias; ASYNC writes produce no error, SYNC writes fire `WRITE_WIRE_IN_SYNC` instead (see `issues.md`) |

Happy-path coverage of all 7 CDC types (BIT, BUS, FIFO, HANDSHAKE, PULSE, MCP, RAW), default and explicit n_stages, bidirectional crossing, source register written in home domain, and dest alias read in destination domain is provided by `4_12_CDC_HAPPY_PATH-valid_cdc_block_ok.jz`.

## Section 4.13 — Module Instantiation

No untested rules for section 4.13. All 14 rule IDs from the test plan have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| INSTANCE_UNDEFINED_MODULE | error | Tested — 3 triggers across scalar, array, and top module contexts |
| INSTANCE_MISSING_PORT | error | Tested — 3 triggers across scalar, top, and array contexts |
| INSTANCE_PORT_WIDTH_MISMATCH | error | Tested — 3 triggers across IN too narrow, OUT too narrow, IN too wide across 2 modules |
| INSTANCE_PORT_DIRECTION_MISMATCH | error | Tested — 3 triggers across IN-as-OUT, OUT-as-IN, and top module contexts |
| INSTANCE_BUS_MISMATCH | error | Tested — 5 triggers across BUS id mismatch, role mismatch, and combined mismatch across 2 modules |
| INSTANCE_OVERRIDE_CONST_UNDEFINED | error | Tested — 3 triggers across scalar, top, and array contexts |
| INSTANCE_PORT_WIDTH_EXPR_INVALID | error | Tested — 3 triggers with undefined CONST in IN, OUT, and top module width expressions |
| INSTANCE_PARENT_SIGNAL_WIDTH_MISMATCH | error | Tested — 4 triggers across PORT too narrow, WIRE too narrow, WIRE too wide, and top module across 2 modules |
| INSTANCE_ARRAY_COUNT_INVALID | error | Tested — 3 triggers: zero literal, CONST=0, and CONST-based count (compiler false positive, see `issues.md`) |
| INSTANCE_ARRAY_IDX_INVALID_CONTEXT | error | Tested — 2 triggers: IDX in scalar @new binding, IDX in OVERRIDE block |
| INSTANCE_ARRAY_PARENT_BIT_OVERLAP | error | Tested — 1 trigger with overlapping OUT bit ranges; non-overlapping IDX mapping as negative test |
| INSTANCE_ARRAY_IDX_SLICE_OUT_OF_RANGE | error | Tested — 1 trigger with IDX slice exceeding parent width; within-range as negative test |
| INSTANCE_OUT_PORT_LITERAL | error | Tested — 3 triggers across sized literal on OUT, literal on second OUT, and top module across 2 modules |
| INSTANCE_ARRAY_MULTI_DIMENSIONAL | error | Not emitted — parser produces PARSE000 instead; test captures actual parser behavior (see `issues.md`) |

Happy-path coverage of simple instantiation, OVERRIDE constants, no-connect output, BUS port binding, literal IN tie-off, instance arrays with IDX and broadcast mapping, and port referencing via inst.port syntax is provided by `4_13_INSTANCE_HAPPY_PATH-valid_instantiation_ok.jz`.

## Section 4.14 — Feature Guards

| Rule ID | Severity | Status |
|---------|----------|--------|
| FEATURE_COND_WIDTH_NOT_1 | error | Tested — 2 triggers across ASYNCHRONOUS and SYNCHRONOUS blocks across 2 modules |
| FEATURE_EXPR_INVALID_CONTEXT | error | Tested — 3 triggers: wire in ASYNC, register in SYNC, port in SYNC across 2 modules |
| FEATURE_NESTED | error | Tested — 3 triggers split across 3 files: ASYNC body, SYNC body, @else body (compiler stops at first per file) |
| FEATURE_VALIDATION_BOTH_PATHS | error | Not implemented — rule defined in `rules.c` but no semantic pass emits it; both-path validation is not enforced (see `issues.md`) |

Happy-path coverage of @feature with @else, feature in REGISTER/ASYNCHRONOUS/SYNCHRONOUS, CONST and CONFIG expressions, logical operators (&&, ||), comparison operators (==, !=, >), empty feature body, and multi-module instantiation is provided by `4_14_FEATURE_HAPPY_PATH-valid_feature_guards_ok.jz`.

Note: FEATURE_COND_WIDTH_NOT_1, FEATURE_EXPR_INVALID_CONTEXT, and FEATURE_NESTED are only checked in ASYNCHRONOUS and SYNCHRONOUS blocks. Declaration blocks (REGISTER, WIRE, CONST) are not validated — see `issues.md` for details.

## Section 4.3 — CONST (Compile-Time Constants)

All 6 rule IDs from the test plan have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| CONST_NEGATIVE_OR_NONINT | error | Tested — 2 triggers across helper and top module CONST blocks |
| CONST_UNDEFINED_IN_WIDTH_OR_SLICE | error | Tested — 3 triggers across port width (helper), wire width (top), register width (top) |
| CONST_STRING_IN_NUMERIC_CONTEXT | error | Tested — 3 triggers across port width (helper), wire width (top), register width (top) |
| CONST_NUMERIC_IN_STRING_CONTEXT | error | Tested — 1 trigger for numeric CONST in @file() path |
| CONST_USED_WHERE_FORBIDDEN | error | Tested — 3 triggers across SYNC RHS (helper), ASYNC RHS (top), SYNC RHS (top) |
| CONST_CIRCULAR_DEP | error | Not effectively tested — rule unreachable for module CONSTs; compiler emits CONST_NEGATIVE_OR_NONINT instead (see `issues.md`). Test captures actual behavior. |

Happy-path coverage of numeric CONST in port/wire/register widths, CONST-referencing-CONST, clog2 arithmetic, string CONST declaration, CONST=0, large CONST, and multi-module instantiation is provided by `4_3_CONST_HAPPY_PATH-valid_const_ok.jz`.

## Section 4.5 — WIRE (Intermediate Nets)

All 3 rule IDs from the test plan have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| WRITE_WIRE_IN_SYNC | error | Tested — 5 triggers across SYNC root (helper), SYNC root (top), IF body, ELSE body, SELECT/CASE body across 2 modules; WIRE in ASYNC as negative test; SELECT_NO_MATCH_SYNC_OK co-fires |
| WIRE_MULTI_DIMENSIONAL | error | Not emitted — parser rejects multi-dimensional syntax with PARSE000 before semantic check runs; tests split into 2 files capturing actual PARSE000 behavior (see `issues.md`) |
| WARN_UNUSED_WIRE | warning | Not emitted — `NET_DANGLING_UNUSED` fires instead (see `issues.md`); tested under section 12.3 (`12_3_WARN_UNUSED_WIRE-unused_wire.jz`) |

Happy-path coverage of standard wire, wire as intermediate, multiple wires, wire driving OUT port, 1-bit wire, wide wire, wire read in SYNC, and cross-module wire usage is provided by `4_5_WIRE_HAPPY_PATH-valid_wire_ok.jz`.
