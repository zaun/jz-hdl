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

Happy-path coverage of same signal names in different modules, module-local CONST isolation, unique identifiers across all declaration types (PORT, WIRE, REGISTER, CONST), and unique instance names is provided by `4_2_HAPPY_PATH-scope_uniqueness_ok.jz`.

| Rule ID | Severity | Reason |
|---------|----------|--------|
| AMBIGUOUS_REFERENCE | error | Not implemented — rule defined in `rules.c` but no semantic pass emits it; cannot be tested (see `issues.md`) |

No untested rules for section 10.1. This section defines no rule IDs; it is a conceptual overview. Happy-path coverage is provided by the validation test.

No untested rules for section 10.3. All rules have corresponding validation test files. `TEMPLATE_EXTERNAL_REF` is now correctly tested (fires during `@apply` expansion — 4 triggers detected). `TEMPLATE_SCRATCH_WIDTH_INVALID` is not yet enforced by the compiler — its test `.out` file is empty (see `issues.md` for details).

No untested rules for section 10.4. All rules from the test plan have corresponding validation tests.

No untested rules for section 10.2. All rules from the test plan (TEMPLATE_DUP_NAME, TEMPLATE_DUP_PARAM) have corresponding validation tests.

No untested rules for section 10.5. All rules have corresponding validation test files. However, `TEMPLATE_APPLY_OUTSIDE_BLOCK` is not yet enforced by the compiler — its test `.out` files are empty (see `issues.md` for details).

No untested rules for section 10.6. All rules from the test plan (`ASSIGN_MULTIPLE_SAME_BITS`, `SYNC_MULTI_ASSIGN_SAME_REG_BITS`) have corresponding validation tests.

No untested rules for section 10.8. This section is a cross-reference summary of all TEMPLATE_* error cases from S10.2-S10.6. All 15 rule IDs are covered by existing tests in their respective subsections.

No untested rules for section 11.1.

| Rule ID | Severity | Status |
|---------|----------|--------|
| WARN_INTERNAL_TRISTATE | warning | Tested — 3 triggers across wire-in-helper (IF/ELSE), wire-in-top (IF/ELSE), wire-in-top (SELECT/CASE) across 2 modules; INOUT port with z as negative test |
| INFO_TRISTATE_TRANSFORM | info | Not testable via `--info --lint` without `--tristate-default` flag; tested in section 11.4/11.7 via `_GND_`/`_VCC_` filename patterns |
| TRISTATE_TRANSFORM_UNUSED_DEFAULT | warning | Not testable via `--info --lint` without `--tristate-default` flag; tested in section 11.7 via `_GND_` filename pattern |

Happy-path coverage (no internal tri-state, only INOUT port with z) is provided by `11_1_HAPPY_PATH-tristate_default_ok.jz`.

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

No new error rules. Section 11.5 cross-references the same two rules already tested under section 11.7:

| Rule ID | Severity | Status |
|---------|----------|--------|
| TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL | error | Tested under 11.7 (`11_GND_7_TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL-non_exclusive.jz`); unreachable — `NET_MULTIPLE_ACTIVE_DRIVERS` fires first (see `issues.md`) |
| TRISTATE_TRANSFORM_PER_BIT_FAIL | error | Tested under 11.7 (`11_GND_7_TRISTATE_TRANSFORM_PER_BIT_FAIL-per_bit_tristate.jz`); not emitted — compiler transforms per-bit patterns without error (see `issues.md`) |

Happy-path coverage of mutually exclusive enables (IF/ELSE structure) and full-width z assignments across multiple modules is provided by `11_GND_5_HAPPY_PATH-tristate_validation_ok.jz`.

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

## Section 7.1 — MEM Declaration

All 15 rule IDs from the test plan have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| MEM_DUP_NAME | error | Tested — 2 triggers across helper + top module |
| MEM_INVALID_WORD_WIDTH | error | Tested — 2 triggers across helper + top module |
| MEM_INVALID_DEPTH | error | Tested — 2 triggers across helper + top module |
| MEM_UNDEFINED_CONST_IN_WIDTH | error | Tested — 2 triggers (undefined width in helper, undefined depth in top) |
| MEM_DUP_PORT_NAME | error | Tested — 3 triggers (OUT-OUT, IN-IN, OUT-IN duplicates) |
| MEM_PORT_NAME_CONFLICT_MODULE_ID | error | Tested — 3 triggers (conflict with PORT, REGISTER, WIRE names) |
| MEM_EMPTY_PORT_LIST | error | Tested — 2 triggers across helper + top module |
| MEM_INVALID_PORT_TYPE | error | Tested — 2 triggers (ASYNC on IN, SYNC on IN ports) |
| MEM_TYPE_INVALID | error | Tested — 2 triggers across helper + top module |
| MEM_TYPE_BLOCK_WITH_ASYNC_OUT | error | Tested — 2 triggers across helper + top module |
| MEM_CHIP_CONFIG_UNSUPPORTED | error | Tested — 2 triggers (DISTRIBUTED width=32 exceeds gw1nr-9 max) |
| MEM_INOUT_MIXED_WITH_IN_OUT | error | Tested — 3 triggers (INOUT+OUT, INOUT+IN, INOUT+OUT+IN) |
| MEM_INOUT_ASYNC | error | Tested — 2 triggers (INOUT ASYNC, INOUT SYNC) |
| MEM_MISSING_INIT | error | Tested — 2 triggers across helper + top module |
| MEM_UNDEFINED_NAME | error | Tested — 2 triggers; NOTE: compiler emits UNDECLARED_IDENTIFIER instead (see `issues.md`) |

Happy-path coverage includes BLOCK type, DISTRIBUTED type, CONST width/depth, multiple ports, multiple MEM blocks, INOUT ports, minimum depth (1), and 1-bit word width across 4 modules: `7_1_HAPPY_PATH-mem_declaration_ok.jz`.

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

No untested rules for section 3.1. Both rules from the test plan (`LOGICAL_WIDTH_NOT_1`, `TYPE_BINOP_WIDTH_MISMATCH`) have corresponding validation tests. Six rules are deferred to section 3.2 tests (`UNARY_ARITH_MISSING_PARENS`, `TERNARY_COND_WIDTH_NOT_1`, `TERNARY_BRANCH_WIDTH_MISMATCH`, `CONCAT_EMPTY`, `DIV_CONST_ZERO`, `DIV_UNGUARDED_RUNTIME_ZERO`). The happy-path test (`3_1_HAPPY_PATH-operator_categories_ok.jz`) covers all operator categories (unary arithmetic, binary arithmetic, multiply, divide, modulus, bitwise, logical, comparison, shift, ternary, concatenation).

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

Happy-path coverage of all valid ASYNCHRONOUS constructs (net alias, receive, drive, sliced, extension modifiers =z/=s/<=z/<=s/=>z/=>s, transitive alias, ternary, register read, empty ASYNC block) is provided by `4_10_HAPPY_PATH-async_block_ok.jz`.

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

Happy-path coverage of @feature with @else, feature in REGISTER/ASYNCHRONOUS/SYNCHRONOUS, CONST and CONFIG expressions, logical operators (&&, ||, !), comparison operators (==, !=, >, <, >=, <=), empty feature body, always-true/always-false conditions, and multi-module instantiation is provided by `4_14_HAPPY_PATH-feature_guards_ok.jz`.

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

## Section 4.6 — MUX (Signal Aggregation and Slicing)

All 6 rule IDs from the test plan have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| MUX_ASSIGN_LHS | error | Tested — 8 triggers: bare and indexed MUX assignment in ASYNCHRONOUS and SYNCHRONOUS blocks across 2 modules |
| MUX_AGG_SOURCE_WIDTH_MISMATCH | error | Tested — 2 triggers: port width mismatch (8-bit vs 4-bit) and register vs wire width mismatch across 2 modules |
| MUX_AGG_SOURCE_INVALID | error | Tested — 2 triggers: undefined identifier as aggregation source across 2 modules |
| MUX_SLICE_WIDTH_NOT_DIVISOR | error | Tested — 2 triggers: non-divisible auto-slice on port (8%3) and wire (16%5) across 2 modules |
| MUX_SELECTOR_OUT_OF_RANGE_CONST | error | Tested — 4 triggers: OOB constant index on aggregation and auto-slice MUXes in ASYNCHRONOUS and SYNCHRONOUS blocks across 2 modules (uses unsized literals only; sized literals not detected — see `issues.md`) |
| MUX_NAME_DUPLICATE | error | Tested — 4 triggers: MUX name conflicts with port, wire, register, and wire in top module across 4 modules |

Happy-path coverage of aggregation form, auto-slicing form, dynamic selection, constant selection, MUX in SYNCHRONOUS, and many-source aggregation is provided by `4_6_MUX_HAPPY_PATH-mux_ok.jz`.

## Section 4.7 — REGISTER (Storage Elements)

All 9 rule IDs from the test plan have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| ASYNC_ASSIGN_REGISTER | error | Tested — 6 triggers: ASYNC root, IF branch, SELECT cases across 2 modules |
| REG_INIT_WIDTH_MISMATCH | error | Tested — 4 triggers: init too narrow and too wide across 2 modules |
| REG_INIT_CONTAINS_X | error | Tested — 4 triggers: binary x bits across 2 modules |
| REG_INIT_CONTAINS_Z | error | Tested — 4 triggers: binary z bits across 2 modules |
| REG_MULTI_DIMENSIONAL | error | Tested — parser emits PARSE000 instead of REG_MULTI_DIMENSIONAL (see `issues.md`); 1 trigger captured |
| REG_MISSING_INIT_LITERAL | error | Tested — parser emits PARSE000 instead of REG_MISSING_INIT_LITERAL (see `issues.md`); 1 trigger captured |
| WARN_UNUSED_REGISTER | warning | Tested — 3 triggers: unused registers across 2 modules |
| WARN_UNSINKED_REGISTER | warning | Tested — 2 triggers: written-but-never-read registers across 2 modules |
| WARN_UNDRIVEN_REGISTER | warning | Tested — 2 triggers: read-but-never-written registers across 2 modules |

Happy-path coverage of standard register declaration, GND reset, 1-bit register, read-in-ASYNC, write-in-SYNC, and multiple registers in one block is provided by `4_7_REG_HAPPY_PATH-register_ok.jz`.

## Section 4.8 — Latches

All 10 rules from the test plan have corresponding validation test files. However, three latch-specific rules are never emitted by the compiler — generic rules fire first for the same conditions:

| Rule ID | Severity | Status |
|---------|----------|--------|
| LATCH_ALIAS_FORBIDDEN | error | Tested — 2 triggers across helper and top modules |
| LATCH_ASSIGN_NON_GUARDED | error | Tested — 2 triggers across helper and top modules |
| LATCH_AS_CLOCK_OR_CDC | error | Tested — 2 triggers for latch as CLK; CDC source context not checked by compiler (see `issues.md`) |
| LATCH_ENABLE_WIDTH_NOT_1 | error | Tested — 2 triggers across helper and top modules |
| LATCH_INVALID_TYPE | error | Tested — 1 trigger (parser aborts LATCH block on invalid type) |
| LATCH_ASSIGN_IN_SYNC | error | Tested — 2 triggers across helper and top modules |
| LATCH_CHIP_UNSUPPORTED | error | Tested — 2 triggers (D + SR) on ICE40UP-5K-SG48 chip |
| LATCH_WIDTH_INVALID | error | Never emitted — `WIDTH_NONPOSITIVE_OR_NONINT` fires first (see `issues.md`) |
| LATCH_SR_WIDTH_MISMATCH | error | Never emitted — `ASSIGN_WIDTH_NO_MODIFIER` fires first (see `issues.md`) |
| LATCH_IN_CONST_CONTEXT | error | Never emitted — `CHECK_INVALID_EXPR_TYPE` fires first (see `issues.md`) |

Happy-path coverage of D-latch and SR-latch declaration, guarded assignment in ASYNC, latch reads in ASYNC and SYNC blocks, conditional usage, and multi-module instantiation is provided by `4_8_LATCH_HAPPY_PATH-latch_ok.jz`.

## Section 4.9 — MEM Block

All rules from the test plan have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| MEM_TYPE_INVALID | error | Tested — 3 triggers (SRAM in helper, RAM and REGISTER in top module) with BLOCK and DISTRIBUTED as negative tests |

Happy-path coverage of BLOCK type, DISTRIBUTED type, and multiple MEMs in one module is provided by `4_9_MEM_HAPPY_PATH-mem_ok.jz`.

Full MEM coverage (declaration, access, warnings) is deferred to Section 7 test plans per the test plan document.

## Section 5.0 — Assignment Operators Summary

All 4 implemented rules from the test plan have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| ASSIGN_WIDTH_NO_MODIFIER | error | Tested — 5 triggers across <= and => in ASYNC (helper + top) and SYNC (helper + top) |
| ASSIGN_TRUNCATES | error | Tested — 7 triggers across <=z, <=s, =>z, =>s in ASYNC (helper + top) and SYNC (helper + top) |
| ASSIGN_SLICE_WIDTH_MISMATCH | error | Tested — 5 triggers across ASYNC (helper + top) and SYNC (helper + top) with both LHS-wider and RHS-wider mismatches |
| ASSIGN_CONCAT_WIDTH_MISMATCH | error | Tested — 4 triggers across ASYNC (helper + top) and SYNC (helper + top) with various width sum mismatches |

Happy-path coverage of all 9 assignment operator variants (=, =>, <=, =z, =s, =>z, =>s, <=z, <=s), redundant modifiers, 1-bit to 256-bit extension, and same-width usage is provided by `5_0_HAPPY_PATH-assignment_operators_ok.jz`.

| Rule ID | Severity | Reason |
|---------|----------|--------|
| WIDTH_ASSIGN_MISMATCH_NO_EXT | error | Always suppressed by higher-priority ASSIGN_WIDTH_NO_MODIFIER at the same location; both rules fire together but only ASSIGN_WIDTH_NO_MODIFIER appears in output (see `issues.md`) |

## Section 5.1 — Asynchronous Assignments

All testable rules from the test plan have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| ASYNC_INVALID_STATEMENT_TARGET | error | Tested — 2 triggers: MEM SYNC OUT port .addr in ASYNC across helper and top modules |
| ASYNC_FLOATING_Z_READ | error | Tested — 2 triggers: wire driven only with z (unconditional and conditional-all-z) across helper and top modules; WARN_INTERNAL_TRISTATE co-fires |

Happy-path coverage of alias (=), drive (=>), receive (<=), sliced, concat decomposition, register read, transitive alias, sign-extend (=s), zero-extend (=z), ternary receive, and constant drive (<=) is provided by `5_1_HAPPY_PATH-async_assignments_ok.jz`.

Rules already covered by other section tests (no 5_1 prefix needed):

| Rule ID | Severity | Existing Test |
|---------|----------|---------------|
| ASYNC_ALIAS_LITERAL_RHS | error | `4_10_ASYNC_ALIAS_LITERAL_RHS-literal_rhs_in_alias.jz` — 5 triggers across helper, extension, and top modules |
| ASYNC_ASSIGN_REGISTER | error | `4_7_ASYNC_ASSIGN_REGISTER-register_in_async.jz` — 6 triggers across root, IF, SELECT contexts |
| ASYNC_UNDEFINED_PATH_NO_DRIVER | error | `1_5_ASYNC_UNDEFINED_PATH_NO_DRIVER-partial_coverage.jz` — 4 triggers across IF-no-ELSE and SELECT-no-DEFAULT |
| WIDTH_ASSIGN_MISMATCH_NO_EXT | error | `5_0_WIDTH_ASSIGN_MISMATCH_NO_EXT-alias_width_mismatch.jz` — suppressed by ASSIGN_WIDTH_NO_MODIFIER (see `issues.md`) |

| Rule ID | Severity | Reason |
|---------|----------|--------|
| ASYNC_INVALID_STATEMENT_TARGET (CONST context) | error | Compiler bug: CONST on LHS in ASYNC not detected; `JZ_AST_EXPR_IDENTIFIER` case in `driver_assign.c` does not check for `JZ_SYM_CONST` (see `issues.md`) |

## Section 5.2 — Synchronous Assignments

All 7 rules from the test plan have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| SYNC_NO_ALIAS | error | Tested — 5 triggers: simple =, =z, =s in helper; inside IF and at root in top module |
| SYNC_ROOT_AND_CONDITIONAL_ASSIGN | error | Tested — 5 triggers: root+IF in helper; root+IF in top; root+SELECT (3 CASE/DEFAULT assignments) in top |
| SYNC_MULTI_ASSIGN_SAME_REG_BITS | error | Tested — 3 triggers: double at root in helper; double inside IF in top; double at root in top |
| SYNC_CONCAT_DUP_REG | error | Tested — 3 triggers: adjacent dup in helper; adjacent dup in top; non-adjacent dup in top |
| ASSIGN_TO_NON_REGISTER_IN_SYNC | error | Tested — 3 triggers: OUT port in helper; OUT port in top; IN port in top |
| WRITE_WIRE_IN_SYNC | error | Tested — 3 triggers: wire at root in helper; wire at root in top; wire inside IF in top |
| SYNC_SLICE_WIDTH_MISMATCH | error | Unreachable — suppressed by ASSIGN_SLICE_WIDTH_MISMATCH (priority 2 vs 0); test captures ASSIGN_SLICE_WIDTH_MISMATCH instead (see `issues.md`) |

Happy-path coverage of simple receive (<=), conditional load, zero-extend (<=z), sign-extend (<=s), sliced assignments, non-overlapping slices, concat decomposition, SELECT with DEFAULT, IF/ELSE mutually exclusive, and deeply nested conditionals is provided by `5_2_HAPPY_PATH-sync_assignments_ok.jz`.

## Section 5.3 — Conditional Statements (test_5_3-conditional_statements.md)

All rules from the test plan are tested. No untested rules.

| Rule ID | Severity | Status |
|---------|----------|--------|
| IF_COND_MISSING_PARENS | error | Tested — 3 files: IF in ASYNC, ELIF in ASYNC, IF in SYNC (parser aborts after first error, so one trigger per file) |
| IF_COND_WIDTH_NOT_1 | error | Tested — 5 triggers: IF in ASYNC (helper), IF in SYNC (helper), nested IF in ASYNC (top), ELIF in ASYNC (top), ELIF in SYNC (top) |
| CONTROL_FLOW_OUTSIDE_BLOCK | error | Tested — 2 triggers: IF at module scope, ELSE at module scope (both outside ASYNC/SYNC blocks) |
| COMB_LOOP_UNCONDITIONAL | error | Tested — 2 triggers: direct loop in helper ASYNC, direct loop in top ASYNC |
| COMB_LOOP_CONDITIONAL_SAFE | warning | Tested — 2 triggers: conditional cycle in helper ASYNC, conditional cycle in top ASYNC |
| ASYNC_ALIAS_IN_CONDITIONAL | error | Tested — 3 triggers: alias (=) in IF body (helper), alias (=) in ELSE body (helper), alias (=) in ELIF body (top) across 2 modules |

Happy-path coverage of simple IF/ELSE, IF/ELIF/ELSE chains, nested IFs, IF without ELSE in SYNC (register holds), and multiple ELIFs is provided by `5_3_HAPPY_PATH-conditional_statements_ok.jz`.

## Section 5.4 — SELECT/CASE Statements

| Rule ID | Severity | Status |
|---------|----------|--------|
| SELECT_DUP_CASE_VALUE | error | Tested — 5 triggers: dup bare int ASYNC (helper), dup sized hex ASYNC (top), dup sized binary ASYNC (top), dup in nested SELECT inner (top), dup bare int SYNC (top) |
| SELECT_CASE_WIDTH_MISMATCH | error | Tested — 4 triggers: wider ASYNC (helper), narrower 2-bit ASYNC (top), narrower 1-bit ASYNC (top), wider SYNC (top); bare int and matching-width as negative tests |
| SELECT_DEFAULT_RECOMMENDED_ASYNC | warning | Tested — 2 triggers: ASYNC SELECT without DEFAULT in helper, ASYNC SELECT without DEFAULT in top; ASYNC_UNDEFINED_PATH_NO_DRIVER co-fires; ASYNC SELECT with DEFAULT as negative test |
| SELECT_NO_MATCH_SYNC_OK | warning | Tested — 2 triggers: SYNC SELECT without DEFAULT in helper, SYNC SELECT without DEFAULT in top; SYNC SELECT with DEFAULT as negative test |
| WARN_INCOMPLETE_SELECT_ASYNC | warning | Not emitted — `SELECT_DEFAULT_RECOMMENDED_ASYNC` fires instead (see `issues.md`) |

Happy-path coverage of simple SELECT with DEFAULT, multiple CASEs, x-wildcard CASE, partial x-wildcard, single CASE + DEFAULT, fall-through CASE, nested SELECT, and SELECT in SYNC with DEFAULT is provided by `5_4_HAPPY_PATH-select_case_ok.jz`.

## Section 5.5 — Intrinsic Operators

All 23 rule IDs from the test plan have corresponding validation test files:

| Rule ID | Severity | Status |
|---------|----------|--------|
| CLOG2_NONPOSITIVE_ARG | error | Tested — 4 triggers: clog2(0) in helper CONST (x2), top CONST (x2) |
| CLOG2_INVALID_CONTEXT | error | Tested — 4 triggers: clog2() in helper async, helper sync, top async, top sync |
| WIDTHOF_INVALID_CONTEXT | error | Tested — 4 triggers: widthof() in helper async, helper sync, top async, top sync |
| WIDTHOF_INVALID_TARGET | error | Not emitted — rule defined in `rules.c` but compiler does not detect widthof() on CONST names (see `issues.md`) |
| WIDTHOF_INVALID_SYNTAX | error | Not emitted — rule defined in `rules.c` but compiler does not detect widthof() with slice arguments (see `issues.md`) |
| WIDTHOF_WIDTH_NOT_RESOLVABLE | error | Not emitted — rule defined in `rules.c` but compiler does not detect unresolvable widthof() targets (see `issues.md`) |
| LIT_WIDTH_INVALID | error | Tested — 4 triggers: lit(0, ...) in helper async, helper sync, top async, top sync |
| LIT_VALUE_INVALID | error | Tested — 4 triggers: lit(W, -N) in helper async, helper sync, top async, top sync |
| LIT_VALUE_OVERFLOW | error | Tested — 4 triggers: lit(4, 20) and lit(8, 256) in helper async, helper sync, top async, top sync |
| LIT_INVALID_CONTEXT | error | Tested — 2 triggers: lit() in CONST in helper and top; cascading LIT_WIDTH_INVALID on valid ASYNC/SYNC lit() (see `issues.md`) |
| FUNC_RESULT_TRUNCATED_SILENTLY | error | Not emitted — compiler fires ASSIGN_WIDTH_NO_MODIFIER instead (see `issues.md`) |
| SBIT_SET_WIDTH_NOT_1 | error | Tested — 4 triggers: sbit() with 8-bit set value in helper async, helper sync, top async, top sync |
| GBIT_INDEX_OUT_OF_RANGE | error | Not emitted — constant index evaluation broken for sized literals (see `issues.md`) |
| SBIT_INDEX_OUT_OF_RANGE | error | Not emitted — constant index evaluation broken for sized literals (see `issues.md`) |
| GSLICE_INDEX_OUT_OF_RANGE | error | Not emitted — constant index evaluation broken for sized literals (see `issues.md`) |
| GSLICE_WIDTH_INVALID | error | Tested — 4 triggers: gslice(width=0) in helper async, helper sync, top async, top sync |
| SSLICE_INDEX_OUT_OF_RANGE | error | Not emitted — constant index evaluation broken for sized literals (see `issues.md`) |
| SSLICE_WIDTH_INVALID | error | Tested — 4 triggers: sslice(width=0) in helper async, helper sync, top async, top sync |
| SSLICE_VALUE_WIDTH_MISMATCH | error | Tested — 4 triggers: sslice() value width mismatch in helper async, helper sync, top async, top sync |
| OH2B_INPUT_TOO_NARROW | error | Tested — 4 triggers: oh2b(1-bit) in helper async, helper sync, top async, top sync |
| B2OH_WIDTH_INVALID | error | Tested — 4 triggers: b2oh(width<2) in helper async, helper sync, top async, top sync |
| PRIENC_INPUT_TOO_NARROW | error | Tested — 4 triggers: prienc(1-bit) in helper async, helper sync, top async, top sync |
| BSWAP_WIDTH_NOT_BYTE_ALIGNED | error | Tested — 4 triggers: bswap(7-bit) in helper async, helper sync, top async, top sync |

Happy-path coverage of all 28 intrinsic operators (uadd, sadd, umul, smul, clog2, widthof, gbit, sbit, gslice, sslice, lit, oh2b, b2oh, prienc, lzc, usub, ssub, abs, umin, umax, smin, smax, popcount, reverse, bswap, reduce_and, reduce_or, reduce_xor) in ASYNC and SYNC blocks across 2 modules is provided by `5_5_HAPPY_PATH-intrinsic_operators_ok.jz`.

## Section 6.10 — Project Scope and Uniqueness

All 4 rule IDs from the test plan have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| PROJECT_MULTIPLE_PER_FILE | error | Tested — 1 trigger for second @project block in same file |
| PROJECT_NAME_NOT_UNIQUE | error | Tested — 1 trigger for project name matching module name |
| MODULE_NAME_DUP_IN_PROJECT | error | Tested — 2 triggers for second and third definition of same module name; unique module as negative test (also tested under section 4.2) |
| BLACKBOX_NAME_DUP_IN_PROJECT | error | Tested — 2 triggers for bb-bb duplicate and bb-module name conflict; WARN_UNUSED_MODULE co-fires (also tested under section 4.2) |

Happy-path coverage of valid project with unique module names, unique blackbox names, and no name collisions is provided by `6_10_HAPPY_PATH-project_scope_ok.jz`.

## Section 6.2 — Project Canonical Form

All 7 rule IDs from the test plan have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| IMPORT_OUTSIDE_PROJECT | error | Tested — 2 triggers: @import before @project, @import after @endproj |
| IMPORT_NOT_AT_PROJECT_TOP | error | Tested — 1 trigger: @import after CONFIG block (parser aborts after error) |
| IMPORT_FILE_HAS_PROJECT | error | Tested — 1 trigger: imported file contains @project block |
| IMPORT_DUP_MODULE_OR_BLACKBOX | error | Tested — 1 trigger: two imports with same module name (collision on second import) |
| IMPORT_FILE_MULTIPLE_TIMES | error | Tested — 1 trigger: same file imported twice |
| PROJECT_MULTIPLE_PER_FILE | error | Tested — 1 trigger: second @project block in same file |
| PROJECT_MISSING_ENDPROJ | error | Tested — 1 trigger: @project without @endproj |

Happy-path coverage of valid project with imports, CONFIG, @blackbox, multi-module hierarchy, and canonical ordering is provided by `6_2_HAPPY_PATH-canonical_form_ok.jz`.

## Section 6.3 — CONFIG Block

All 10 rule IDs from the test plan have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| CONFIG_MULTIPLE_BLOCKS | error | Tested — 1 trigger for second CONFIG block in same project. Note: compiler emits PARSE000 instead of CONFIG_MULTIPLE_BLOCKS (see `issues.md`) |
| CONFIG_NAME_DUPLICATE | error | Tested — 3 triggers for duplicate names (ALPHA, BETA, GAMMA) |
| CONFIG_INVALID_EXPR_TYPE | error | Tested — 3 triggers for undefined bare names and expression with undefined name |
| CONFIG_FORWARD_REF | error | Tested — 2 triggers for forward references to later CONFIG entries |
| CONFIG_USE_UNDECLARED | error | Tested — 4 triggers across port width (helper), CONST initializer (top), port width (top), register width (top) |
| CONFIG_CIRCULAR_DEP | error | Tested — 2 triggers for direct (A<->B) and indirect (X->Y->Z->X) circular dependencies. Note: CONFIG_FORWARD_REF co-fires for forward references within circular chains |
| CONFIG_USED_WHERE_FORBIDDEN | error | Tested — 4 triggers across ASYNC RHS (helper), SYNC RHS (helper), ASYNC RHS (top), SYNC RHS (top) |
| CONST_USED_WHERE_FORBIDDEN | error | Tested — 4 triggers across ASYNC RHS (helper), SYNC RHS (helper), ASYNC RHS (top), SYNC RHS (top) |
| CONST_STRING_IN_NUMERIC_CONTEXT | error | Tested — 3 triggers across port width (helper), CONST initializer (top), register width (top) |
| CONST_NUMERIC_IN_STRING_CONTEXT | error | Tested — test captures parser behavior: CONFIG.name in @file() produces PARSE000 instead of CONST_NUMERIC_IN_STRING_CONTEXT (see `issues.md`). Bare CONST numeric in @file() tested under section 4.3. |

Happy-path coverage of numeric CONFIG entries, string CONFIG entries, chained CONFIG references (bare name backward refs), CONFIG in port widths, register widths, CONST initializers, CONFIG value = 0, CONFIG/CONST same-name disjoint namespaces, and cross-module CONFIG usage is provided by `6_3_HAPPY_PATH-config_ok.jz`.

## Section 6.4 — CLOCKS Block

All 27 rule IDs from the test plan have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| CLOCK_DUPLICATE_NAME | error | Tested — 3 triggers for duplicate clock names in CLOCKS block |
| CLOCK_PERIOD_NONPOSITIVE | error | Tested — 3 triggers for zero, negative int, and negative float periods |
| CLOCK_EDGE_INVALID | error | Tested — 2 triggers for invalid edge specifiers ("Both", "Positive") |
| CLOCK_PORT_WIDTH_NOT_1 | error | Tested — 1 trigger for clock pin with width [2] |
| CLOCK_NAME_NOT_IN_PINS | error | Tested — 2 triggers for clocks not in IN_PINS and not CLOCK_GEN outputs |
| CLOCK_EXTERNAL_NO_PERIOD | error | Tested — 2 triggers for IN_PINS clocks without period |
| CLOCK_SOURCE_AMBIGUOUS | error | Tested — 2 triggers for clocks with period AND CLOCK_GEN OUT (co-fires CLOCK_NAME_NOT_IN_PINS, CLOCK_GEN_OUTPUT_HAS_PERIOD) |
| CLOCK_GEN_INPUT_IS_SELF_OUTPUT | error | Tested — 1 trigger for PLL input referencing own output (co-fires CLOCK_GEN_INPUT_NO_PERIOD) |
| CLOCK_GEN_OUTPUT_HAS_PERIOD | error | Tested — 2 triggers for CLOCK_GEN OUT with period in CLOCKS (co-fires CLOCK_NAME_NOT_IN_PINS, CLOCK_SOURCE_AMBIGUOUS) |
| CLOCK_GEN_INVALID_TYPE | error | Tested — 1 trigger; parser emits PARSE000 instead of CLOCK_GEN_INVALID_TYPE (see `issues.md`) |
| CLOCK_GEN_MISSING_INPUT | error | Tested — 1 trigger for PLL without IN declaration (co-fires CLOCK_GEN_REQUIRED_INPUT_MISSING) |
| CLOCK_GEN_MISSING_OUTPUT | error | Tested — 1 trigger for PLL without OUT declaration |
| CLOCK_GEN_INPUT_NOT_DECLARED | error | Tested — 1 trigger for IN clock not in CLOCKS block |
| CLOCK_GEN_INPUT_NO_PERIOD | error | Tested — 1 trigger for IN clock without period |
| CLOCK_GEN_OUTPUT_NOT_DECLARED | error | Tested — 1 trigger for OUT clock not in CLOCKS block |
| CLOCK_GEN_OUTPUT_IS_INPUT_PIN | error | Tested — 1 trigger for OUT clock also in IN_PINS |
| CLOCK_GEN_MULTIPLE_DRIVERS | error | Tested — 1 trigger for clock driven by two PLLs |
| CLOCK_GEN_OUTPUT_INVALID_SELECTOR | error | Tested — 1 trigger for invalid output selector "NONSENSE" |
| CLOCK_GEN_OUT_NOT_CLOCK | error | Tested — 1 trigger for OUT LOCK (is_clock=false) |
| CLOCK_GEN_WIRE_IS_CLOCK | error | Tested — 1 trigger for WIRE PHASE (is_clock=true) |
| CLOCK_GEN_WIRE_IN_CLOCKS | error | Tested — 1 trigger for WIRE LOCK declared in CLOCKS |
| CLOCK_GEN_PARAM_OUT_OF_RANGE | error | Tested — 1 trigger for IDIV=100 exceeding range 0-63 (co-fires CLOCK_GEN_DERIVED_OUT_OF_RANGE) |
| CLOCK_GEN_PARAM_TYPE_MISMATCH | error | Tested — 1 trigger for IDIV=1.5 (integer param given decimal) (co-fires CLOCK_GEN_DERIVED_OUT_OF_RANGE) |
| CLOCK_GEN_DERIVED_OUT_OF_RANGE | error | Tested — 1 trigger for FVCO below 500 MHz minimum |
| CLOCK_GEN_INPUT_FREQ_OUT_OF_RANGE | error | Tested — 1 trigger for 2000 MHz input exceeding 500 MHz max (co-fires CLOCK_GEN_DERIVED_OUT_OF_RANGE) |
| CLOCK_GEN_REQUIRED_INPUT_MISSING | error | Tested — 1 trigger for CLKDIV without REF_CLK (co-fires CLOCK_GEN_MISSING_INPUT) |
| CLOCK_GEN_NO_CHIP_DATA | info | Tested — 1 trigger for CLOCK_GEN without CHIP specification |

Happy-path coverage of IN_PIN clocks with period, falling edge, PLL with multiple outputs and WIRE LOCK, CLKDIV, OSC (no external input), and valid CONFIG parameters is provided by `6_4_HAPPY_PATH-clocks_ok.jz`.

## Section 6.5 — PIN Blocks

All 15 rule IDs from the test plan have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| PIN_BUS_WIDTH_INVALID | error | Tested — 4 triggers: zero/negative width in IN_PINS, zero in OUT_PINS, zero in INOUT_PINS |
| PIN_DECLARED_MULTIPLE_BLOCKS | error | Tested — 3 triggers: IN+OUT, IN+INOUT, OUT+INOUT cross-block duplicates |
| PIN_DIFF_OUT_MISSING_FCLK | error | Tested — 1 trigger: differential OUT_PIN without fclk; INOUT differential as negative test |
| PIN_DIFF_OUT_MISSING_PCLK | error | Tested — 1 trigger: differential OUT_PIN without pclk; INOUT differential as negative test |
| PIN_DIFF_OUT_MISSING_RESET | error | Tested — 1 trigger: differential OUT_PIN without reset; INOUT differential as negative test |
| PIN_DRIVE_MISSING_OR_INVALID | error | Tested — 5 triggers: missing/zero/negative drive in OUT_PINS, missing/zero drive in INOUT_PINS |
| PIN_DUP_NAME_WITHIN_BLOCK | error | Tested — 3 triggers: duplicate names within IN_PINS, OUT_PINS, INOUT_PINS |
| PIN_INVALID_STANDARD | error | Tested — 3 triggers: invalid standard in IN_PINS, OUT_PINS, INOUT_PINS |
| PIN_MODE_INVALID | error | Tested — 3 triggers: invalid mode in IN_PINS, OUT_PINS, INOUT_PINS |
| PIN_MODE_STANDARD_MISMATCH | error | Tested — 3 triggers: DIFFERENTIAL with single-ended standard in IN_PINS, OUT_PINS, INOUT_PINS |
| PIN_PULL_INVALID | error | Tested — 2 triggers: invalid pull in IN_PINS, INOUT_PINS |
| PIN_PULL_ON_OUTPUT | error | Tested — 2 triggers: pull=UP, pull=DOWN on OUT_PINS |
| PIN_TERM_INVALID | error | Tested — 3 triggers: invalid term in IN_PINS, OUT_PINS, INOUT_PINS |
| PIN_TERM_INVALID_FOR_STANDARD | error | Tested — 2 triggers: term=ON on LVCMOS33 in IN_PINS, LVCMOS18 in INOUT_PINS |
| PIN_TERM_ON_OUTPUT | error | Tested — 1 trigger: term=ON on OUT_PINS; term=OFF as negative test |

Happy-path coverage of all PIN block features (scalar/bus pins, drive strengths, differential mode with all attributes, pull resistors, termination on supported standards, mode=SINGLE/DIFFERENTIAL) is provided by `6_5_HAPPY_PATH-pin_blocks_ok.jz`.

Rules not testable via `--lint` validation framework:

| Rule ID | Severity | Reason |
|---------|----------|--------|
| INFO_SERIALIZER_CASCADE | info | Emitted from backend (`emit_wrapper.c`), not from lint pass; requires `--verilog` mode, not `--lint` |
| SERIALIZER_WIDTH_EXCEEDS_RATIO | error | Emitted from backend (`emit_wrapper.c`), not from lint pass; requires `--verilog` mode, not `--lint` |

## Section 6.6 — MAP Block

| Rule ID | Severity | Status |
|---------|----------|--------|
| MAP_PIN_DECLARED_NOT_MAPPED | error | Tested — 5 triggers: unmapped IN scalar, unmapped differential IN, unmapped OUT scalar, partially mapped bus, unmapped INOUT |
| MAP_PIN_MAPPED_NOT_DECLARED | error | Tested — 3 triggers: undeclared scalar pin, undeclared bus bit, undeclared differential pin (co-fires MAP_SINGLE_UNEXPECTED_PAIR) |
| MAP_DUP_PHYSICAL_LOCATION | warning | Tested — 3 triggers: scalar-scalar, scalar-bus, bus-bus duplicates (note: differential P/N values not checked by compiler — see issues.md) |
| MAP_INVALID_BOARD_PIN_ID | error | **Not tested** — rule defined in `rules.c` but not implemented in any semantic pass; no code references the rule ID outside of `rules.c` |
| MAP_DIFF_EXPECTED_PAIR | error | Tested — 3 triggers: differential IN, OUT, INOUT with scalar MAP |
| MAP_SINGLE_UNEXPECTED_PAIR | error | Tested — 3 triggers: single-ended IN, OUT, INOUT with {P,N} MAP |
| MAP_DIFF_MISSING_PN | error | Tested — 4 triggers: missing N on IN, missing P on IN, missing N on OUT, missing P on INOUT |
| MAP_DIFF_SAME_PIN | error | Tested — 3 triggers: P==N on IN, P==N on OUT, P==N on INOUT differential pins |

Happy-path coverage of all MAP block features (scalar mappings, bus bit mappings, differential {P,N} mappings, IN/OUT/INOUT pins) is provided by `6_6_HAPPY_PATH-map_ok.jz`.

## Section 6.7 — Blackbox (Opaque) Modules

| Rule ID | Severity | Status |
|---------|----------|--------|
| BLACKBOX_BODY_DISALLOWED | error | Tested — 3 triggers for CONST blocks in blackbox (single entry, multiple entries, separate blackbox); ASYNC/SYNC/WIRE/REG/MEM caught by parser PARSE000 before semantic analysis |
| BLACKBOX_OVERRIDE_UNCHECKED | info | Tested — 3 triggers for OVERRIDE in blackbox @new (sub module single param, top module single param, top module multiple params); no-OVERRIDE as negative test |
| BLACKBOX_NAME_DUP_IN_PROJECT | error | Tested — 2 triggers for bb-bb duplicate and bb-module name conflict; WARN_UNUSED_MODULE co-fires (also tested under section 4.2 and 6.10) |
| BLACKBOX_UNDEFINED_IN_NEW | error | **Not tested as intended** — rule defined in `rules.c` but not implemented; compiler emits `INSTANCE_UNDEFINED_MODULE` instead; test captures actual behavior (see `issues.md`) |

Happy-path coverage of valid blackbox declarations (simple, INOUT ports, multiple blackboxes) and instantiation (with and without OVERRIDE, cross-module) is provided by `6_7_HAPPY_PATH-blackbox_ok.jz`.

## Section 6.8 — BUS Aggregation

| Rule ID | Severity | Status |
|---------|----------|--------|
| BUS_DEF_DUP_NAME | error | Tested — 3 triggers for duplicate BUS definition names (triple ALPHA_BUS + duplicate BETA_BUS) |
| BUS_DEF_SIGNAL_DUP_NAME | error | Tested — 4 triggers for duplicate signal names across OUT-OUT, IN-IN, INOUT-INOUT, and cross-direction (OUT-IN) duplicates |
| BUS_DEF_INVALID_DIR | error | **Untestable** — parser rejects invalid BUS signal directions with "expected IN/OUT/INOUT in BUS block" (PARSE000) before semantic analysis; the semantic check in `driver_project.c` is dead code (see `issues.md`) |
| BUS_BULK_BUS_MISMATCH | error | Tested — 1 trigger for bulk assignment between ALPHA_BUS and BETA_BUS ports; valid matching bulk assignment as negative test |
| BUS_BULK_ROLE_CONFLICT | error | Tested — 2 triggers for SOURCE-to-SOURCE and TARGET-to-TARGET bulk assignments; valid SOURCE-to-TARGET as negative test |

Happy-path coverage of BUS definitions (mixed directions, CONFIG width references, single signal), SOURCE/TARGET ports, bulk BUS assignment, and multi-module instantiation with BUS bindings is provided by `6_8_HAPPY_PATH-bus_ok.jz`.

## Section 6.9 — Top-Level Module Instantiation (@top)

All 8 rules from the test plan have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| INSTANCE_UNDEFINED_MODULE | error | Tested — 1 trigger for @top referencing non-existent module. Note: compiler emits `TOP_MODULE_NOT_FOUND` instead (see `issues.md`); co-fires `WARN_UNUSED_MODULE` |
| PROJECT_MISSING_TOP_MODULE | error | Tested — 1 trigger for @project without @top directive |
| TOP_PORT_NOT_LISTED | error | Tested — 2 triggers for omitted IN and OUT ports from @top binding |
| TOP_PORT_WIDTH_MISMATCH | error | Tested — 2 triggers for IN and OUT port width mismatches in @top |
| TOP_PORT_PIN_DECL_MISSING | error | Tested — 2 triggers for IN and OUT ports bound to undeclared pin names |
| TOP_PORT_PIN_DIRECTION_MISMATCH | error | Tested — 2 triggers for IN→OUT_PINS and OUT→IN_PINS direction mismatches |
| TOP_OUT_LITERAL_BINDING | error | Tested — 2 triggers for OUT ports bound to binary and hex literals |
| TOP_NO_CONNECT_WITHOUT_WIDTH | error | Tested — 2 triggers for IN and OUT ports with empty width brackets on no-connect |

Happy-path coverage of valid @top with IN/OUT ports bound to pins, no-connect with explicit width, literal tie-off on IN, and multi-module structure is provided by `6_9_HAPPY_PATH-top_module_ok.jz`.

## Section 7.0 — Memory Port Modes

All 2 rules from the test plan have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| MEM_INVALID_PORT_TYPE | error | Tested — 2 triggers for IN port with ASYNC (helper module) and IN port with SYNC (top module); valid OUT ASYNC/SYNC and plain IN as negative tests |
| MEM_INVALID_WRITE_MODE | error | Tested — 2 triggers for IN port with invalid WRITE_MODE via attribute block (helper) and INOUT port with invalid WRITE_MODE via attribute block (top); valid IN WRITE_FIRST as negative test |

Happy-path coverage of OUT ASYNC, OUT SYNC, IN (plain), IN with WRITE_FIRST/READ_FIRST/NO_CHANGE shorthand, INOUT with WRITE_FIRST/READ_FIRST/NO_CHANGE shorthand, and multi-module instantiation is provided by `7_0_HAPPY_PATH-mem_port_modes_ok.jz`.

## Section 7.10 — CONST Evaluation in MEM

All 2 rules from the test plan have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| CONST_NEGATIVE_OR_NONINT | error | Tested — 2 triggers for negative CONST in helper module (depth) and second helper (width); valid positive CONSTs in MEM as negative test |
| MEM_UNDEFINED_CONST_IN_WIDTH | error | Tested — 3 triggers for undefined CONST in width (helper module), depth (top module), and both width+depth (top module); valid defined CONST in MEM as negative test |

Happy-path coverage of CONST in width, CONST in depth, CONFIG in depth, CONST expressions in dimensions, non-power-of-2 depth, and multi-module instantiation is provided by `7_10_HAPPY_PATH-const_evaluation_in_mem_ok.jz`.

## Section 7.11 — Synthesis Implications

All 3 rules from the test plan have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| MEM_BLOCK_MULTI | info | Tested — 2 triggers for multi-BSRAM MEM in helper module and top module |
| MEM_BLOCK_RESOURCE_EXCEEDED | error | Tested — 1 trigger (project-level) via 27 instanced BLOCK MEMs exceeding 26-block chip limit |
| MEM_DISTRIBUTED_RESOURCE_EXCEEDED | error | Tested — 1 trigger (project-level) via top module + 2 instances of distributed MEM child exceeding chip limit |

Happy-path coverage of BLOCK MEM within capacity, DISTRIBUTED MEM within capacity, and multi-module instantiation sharing chip resources is provided by `7_11_HAPPY_PATH-synthesis_ok.jz`.


## Section 7.2 — Port Types and Semantics

All 8 rules from the test plan have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| MEM_INOUT_ADDR_IN_ASYNC | error | Tested — 2 triggers for .addr in ASYNC block in helper and top modules |
| MEM_INOUT_ASYNC | error | Tested — 2 triggers for ASYNC keyword (helper) and SYNC keyword (top) on INOUT ports |
| MEM_INOUT_INDEXED | error | Tested — 2 triggers for bracket write in SYNC (helper) and bracket read in ASYNC (top) |
| MEM_INOUT_MIXED_WITH_IN_OUT | error | Tested — 2 triggers for INOUT+OUT (helper) and INOUT+IN (top) mixing |
| MEM_INOUT_WDATA_IN_ASYNC | error | Tested — 2 triggers for .wdata in ASYNC block in helper and top modules |
| MEM_MULTIPLE_WRITES_SAME_IN | error | Tested — 2 triggers for double write in helper and top modules; valid writes to different ports as negative test |
| MEM_READ_FROM_WRITE_PORT | error | Tested — 2 triggers for read from IN port in ASYNC block in helper and top modules |
| MEM_WRITE_TO_READ_PORT | error | Tested — 2 triggers for write to OUT port in SYNC block in helper and top modules |

Happy-path coverage of OUT ASYNC read, OUT SYNC read with .addr/.data, IN write, INOUT with WRITE_FIRST/READ_FIRST/NO_CHANGE, single-port memories (OUT-only, INOUT-only), and cross-module instantiation is provided by `7_2_HAPPY_PATH-port_types_ok.jz`.

## Section 7.3 — Memory Access Syntax

All 25 rule IDs from the test plan are covered. 18 have dedicated 7_3 test files; 7 are covered by 7_2 tests (MEM_WRITE_TO_READ_PORT, MEM_READ_FROM_WRITE_PORT, MEM_MULTIPLE_WRITES_SAME_IN, MEM_INOUT_INDEXED, MEM_INOUT_WDATA_IN_ASYNC, MEM_INOUT_ADDR_IN_ASYNC, and these have existing 7_2 test files).

| Rule ID | Severity | Status |
|---------|----------|--------|
| MEM_PORT_UNDEFINED | error | Tested — 3 triggers: async read, sync write, sync .addr on undefined ports across 2 modules |
| MEM_PORT_FIELD_UNDEFINED | error | Tested — 3 triggers: .foo on SYNC OUT, .bar on SYNC OUT, .xyz on INOUT across 2 modules |
| MEM_SYNC_PORT_INDEXED | error | Tested — 2 triggers for bracket index on SYNC read port across 2 modules |
| MEM_PORT_USED_AS_SIGNAL | error | Tested — 2 triggers for bare ASYNC and SYNC OUT port refs across 2 modules. INOUT bare ref NOT detected (compiler bug, see issues.md) |
| MEM_PORT_ADDR_READ | error | Tested — 2 triggers for reading .addr on SYNC OUT port across 2 modules. INOUT .addr read NOT detected (compiler bug, see issues.md) |
| MEM_ASYNC_PORT_FIELD_DATA | error | Tested — 2 triggers for .data on ASYNC port across 2 modules |
| MEM_SYNC_ADDR_INVALID_PORT | error | Tested — 2 triggers for .addr on ASYNC port across 2 modules |
| MEM_SYNC_ADDR_IN_ASYNC_BLOCK | error | Tested but compiler emits ASYNC_INVALID_STATEMENT_TARGET instead (see issues.md) |
| MEM_SYNC_DATA_IN_ASYNC_BLOCK | error | Tested — 2 triggers for SYNC .data read in ASYNC block across 2 modules |
| MEM_SYNC_ADDR_WITHOUT_RECEIVE | error | Tested but compiler emits SYNC_NO_ALIAS instead (see issues.md) |
| MEM_READ_SYNC_WITH_EQUALS | error | Tested but compiler emits SYNC_NO_ALIAS instead (see issues.md) |
| MEM_IN_PORT_FIELD_ACCESS | error | Tested — 4 triggers: .addr and .data on IN port across 2 modules |
| MEM_WRITE_IN_ASYNC_BLOCK | error | Tested — 2 triggers for bracket write in ASYNC block across 2 modules |
| MEM_WRITE_TO_READ_PORT | error | Covered by 7_2 tests |
| MEM_READ_FROM_WRITE_PORT | error | Covered by 7_2 tests |
| MEM_ADDR_WIDTH_TOO_WIDE | error | Tested — 4 triggers: async read, sync write, async read (top), sync .addr across 2 modules |
| MEM_MULTIPLE_WRITES_SAME_IN | error | Covered by 7_2 tests |
| MEM_MULTIPLE_ASSIGN_SYNC_READ_OUT | error | Tested — 2 triggers for double .addr assignment on SYNC read port across 2 modules |
| MEM_CONST_ADDR_OUT_OF_RANGE | error | Tested — 4 triggers: const addr in async read and sync write across 2 modules |
| MEM_INOUT_INDEXED | error | Covered by 7_2 tests |
| MEM_INOUT_WDATA_IN_ASYNC | error | Covered by 7_2 tests |
| MEM_INOUT_ADDR_IN_ASYNC | error | Covered by 7_2 tests |
| MEM_INOUT_WDATA_WRONG_OP | error | Tested but compiler emits SYNC_NO_ALIAS instead (see issues.md) |
| MEM_MULTIPLE_ADDR_ASSIGNS | error | Tested but rule NOT implemented — compiler emits no diagnostic (see issues.md) |
| MEM_MULTIPLE_WDATA_ASSIGNS | error | Tested but rule NOT implemented — compiler emits no diagnostic (see issues.md) |

Happy-path coverage of async read, sync read (.addr/.data), sync write, INOUT access (.addr/.data/.wdata), edge-case addresses (0, depth-1), and cross-module instantiation is provided by `7_3_HAPPY_PATH-memory_access_ok.jz`.

## Section 7.4 — Write Modes

The only error rule for this section (MEM_INVALID_WRITE_MODE) is already tested by `7_0_MEM_INVALID_WRITE_MODE-bad_write_mode_value.jz` (2 triggers: IN port with invalid WRITE_MODE via attribute block, INOUT port with invalid WRITE_MODE via attribute block).

| Rule ID | Severity | Status |
|---------|----------|--------|
| MEM_INVALID_WRITE_MODE | error | Tested — covered by 7_0 tests (2 triggers for IN and INOUT ports with invalid write mode values; valid IN WRITE_FIRST as negative test) |

Happy-path coverage of all three write modes (WRITE_FIRST, READ_FIRST, NO_CHANGE) on both IN and INOUT ports, using both shorthand and attribute block syntax, plus default (unspecified) write mode, is provided by `7_4_HAPPY_PATH-write_modes_ok.jz`.

## Section 7.5 — Initialization

All implemented rules from the test plan have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| MEM_INIT_LITERAL_OVERFLOW | error | Tested — 2 triggers: 16-bit literal for 8-bit word (helper), 32-bit literal for 8-bit word (top); valid 8-bit literal as negative test |
| MEM_INIT_CONTAINS_X | error | Tested — 2 triggers: x in binary literal (helper), all-x binary literal (top); valid hex literal as negative test |
| MEM_INIT_FILE_NOT_FOUND | error | Tested — 2 triggers: nonexistent file in helper module, nonexistent file in top module; valid literal init as negative test |
| MEM_INIT_FILE_TOO_LARGE | error | Tested — 2 triggers: 8-byte file for 4-deep memory in helper and top modules; valid literal init as negative test |
| MEM_WARN_PARTIAL_INIT | warning | Tested — 2 triggers: 2-byte file for 4-deep memory in helper and top modules; valid literal init as negative test |
| MEM_MISSING_INIT | error | Covered by 7_1 tests (declaration-level rule) |

| Rule ID | Severity | Reason |
|---------|----------|--------|
| MEM_INIT_FILE_CONTAINS_X | error | Not implemented — rule defined in `rules.c` but no semantic pass emits it; cannot be tested (see `issues.md`) |

Happy-path coverage of literal initialization (hex values), @file initialization with exact depth match, and multi-module structure is provided by `7_5_HAPPY_PATH-initialization_ok.jz`.

## Section 7.6 — Complete Examples

No new rules are introduced in Section 7.6. This section provides canonical MEM examples exercising rules from Sections 7.1–7.5 in combination.

Happy-path coverage of all 9 canonical examples plus edge cases:

| Test File | Coverage |
|-----------|----------|
| `7_6_HAPPY_PATH-simple_rom.jz` | S7.6.1 — Read-only ROM with ASYNC port, cross-module instantiation |
| `7_6_HAPPY_PATH-dual_port_regfile.jz` | S7.6.2 — Dual-port register file (2R1W), dual ASYNC reads, conditional SYNC write |
| `7_6_HAPPY_PATH-sync_fifo.jz` | S7.6.3 — Synchronous FIFO with register-based pointers, pointer slicing for address |
| `7_6_HAPPY_PATH-registered_read_cache.jz` | S7.6.4 — Registered read cache with SYNC read port (.addr/.data), pipeline stage |
| `7_6_HAPPY_PATH-triple_port.jz` | S7.6.5 — Triple-port memory (2 ASYNC read, 1 SYNC write) |
| `7_6_HAPPY_PATH-quad_port.jz` | S7.6.6 — Quad-port memory (2 ASYNC read, 2 SYNC write) |
| `7_6_HAPPY_PATH-configurable_mem.jz` | S7.6.7 — Configurable memory with CONST-parameterized width/depth |
| `7_6_HAPPY_PATH-single_port_inout.jz` | S7.6.8 — Single-port INOUT BLOCK memory |
| `7_6_HAPPY_PATH-true_dual_port.jz` | S7.6.9 — True dual-port BLOCK memory with 2 INOUT ports, simultaneous access |
| `7_6_HAPPY_PATH-min_dimensions.jz` | Edge case — Minimum dimensions (WIDTH=1, DEPTH=1) with both ASYNC and SYNC read |

## Section 7.7 — Error Checking and Validation

### Rules Tested

| Rule ID | Severity | Test File | Coverage |
|---------|----------|-----------|----------|
| MEM_WARN_PORT_NEVER_ACCESSED | warning | `7_7_MEM_WARN_PORT_NEVER_ACCESSED-unused_port.jz` | 4 triggers: unused OUT ASYNC (helper), unused IN, unused INOUT, unused OUT SYNC (all in top); used ports as negative tests |
| MEM_WARN_DEAD_CODE_ACCESS | warning | `7_7_MEM_WARN_DEAD_CODE_ACCESS-unreachable_access.jz` | 2 triggers: dead sync write via IF(1'b0) in helper and top modules; reachable writes as negative tests |
| MEM_WARN_PARTIAL_INIT | warning | Covered by `7_5_MEM_WARN_PARTIAL_INIT-file_smaller_than_depth.jz` (S7.5/S7.7.3 cross-referenced rule) |

### Happy Path

| Test File | Coverage |
|-----------|----------|
| `7_7_HAPPY_PATH-error_checking_ok.jz` | All MEM ports accessed, all paths reachable, full initialization — no warnings |

### Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| (none) | — | All S7.7.3 warning rules are covered |

## Section 7.8 — MEM vs REGISTER vs WIRE Comparison

No rules to test. Section 7.8 is a comparison/documentation table describing behavioral differences between MEM, REGISTER, and WIRE storage types. No new rule IDs are introduced.

### Happy Path

| Test File | Coverage |
|-----------|----------|
| `7_8_HAPPY_PATH-mem_vs_register_vs_wire.jz` | All three storage types (MEM, REGISTER, WIRE) used correctly in multi-module design with cross-module instantiation; includes DISTRIBUTED MEM as register file, single-element MEM, combinational WIRE path |

## Section 7.9 — MEM in Module Instantiation

All rules from the test plan are tested:

| Rule ID | Severity | Status |
|---------|----------|--------|
| UNDECLARED_IDENTIFIER | error | Tested — 4 triggers: intermediate module write to child MEM, top-level async read of child MEM, top-level sync write to child MEM, deep hierarchical access (two levels) |

### Happy Path

| Test File | Coverage |
|-----------|----------|
| `7_9_HAPPY_PATH-mem_in_module_ok.jz` | Normal MEM-containing module instantiation, multiple instances of same module (independent memories), same MEM name in parent and child (scoped independently), nested instantiation with MEM accessed via port chain |

## Section 8.2 — Global Syntax

All rules from the test plan have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| GLOBAL_INVALID_EXPR_TYPE | error | Tested — 5 triggers: bare integer, bare zero, CONFIG reference, cross-global reference, expression |

### Happy Path

| Test File | Coverage |
|-----------|----------|
| `8_2_HAPPY_PATH-global_syntax_ok.jz` | Valid @global blocks with sized literals (binary, hex, decimal), multiple entries per block, multiple @global blocks, empty @global block, cross-module usage of global constants |

## Section 8.3 — Global Semantics

All 3 rules from the test plan have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| GLOBAL_CONST_NAME_DUPLICATE | error | Tested — 3 triggers: duplicate in same block, two duplicate pairs in same block; cross-block same name as negative test |
| GLOBAL_CONST_USE_UNDECLARED | error | Tested — 2 triggers: undefined constant in valid namespace in sync RHS across 2 modules |
| GLOBAL_USED_WHERE_FORBIDDEN | error | Tested — 3 triggers: GLOBAL in CONFIG expression, GLOBAL in module CONST initializer, GLOBAL in OVERRIDE expression; valid async/sync usage as negative test |

### Happy Path

| Test File | Coverage |
|-----------|----------|
| `8_3_HAPPY_PATH-global_semantics_ok.jz` | Multiple @global blocks with disjoint namespaces, identically named constants across different blocks, valid references in SYNCHRONOUS RHS across 2 modules connected via @new |

## Section 9.1 — @check Syntax

All 1 rule from the test plan has a corresponding validation test:

| Rule ID | Severity | Status |
|---------|----------|--------|
| CHECK_INVALID_EXPR_TYPE | error | Tested — 5 triggers: undefined identifier at project scope, port signal in helper module, port signal in top module, register signal, wire signal; valid CONST, CONFIG, literal, and clog2 as negative tests |

### Untested (no rule ID in rules.c)

| Scenario | Reason |
|----------|--------|
| Missing parentheses in @check | Parse-level error, no dedicated rule ID |
| Missing message string in @check | Parse-level error, no dedicated rule ID |
| Missing semicolon after @check | Parse-level error, no dedicated rule ID |

### Happy Path

| Test File | Coverage |
|-----------|----------|
| `9_1_HAPPY_PATH-check_syntax_ok.jz` | CONFIG equality and comparison at project scope, clog2 of CONFIG at project scope, literal expression, CONST in helper module, CONST arithmetic, CONST/CONFIG/clog2 in top module, multi-module with @new |

## Section 9.3 — @check Placement Rules

### Rules Not Independently Testable

| Rule ID | Reason |
|---------|--------|
| `CHECK_INVALID_PLACEMENT` | This rule fires for @check inside conditional/@feature bodies (per rules.c: "S9.3 @check may not appear inside conditional or @feature bodies"). @feature is not yet fully supported, so the primary context for this rule cannot be exercised. @check inside ASYNC/SYNC blocks fires `DIRECTIVE_INVALID_CONTEXT` instead. See `issues.md` for details. |

### Happy Path

| Test File | Coverage |
|-----------|----------|
| `9_3_HAPPY_PATH-check_placement_ok.jz` | @check at project scope (single and multiple), @check at module scope after CONST, @check with clog2, @check between blocks, @check immediately before ASYNCHRONOUS block, @check in helper module, multi-module with @new |

## Section 9.4 — @check Expression Rules

All 1 rule from the test plan has a corresponding validation test:

| Rule ID | Severity | Status |
|---------|----------|--------|
| CHECK_INVALID_EXPR_TYPE | error | Tested — 10 triggers: bare port (helper + top), bare register (helper + top), bare wire (top), port slice (helper + top), register slice (helper + top), wire slice (top); valid CONST, CONFIG, clog2, and literal as negative tests |

### Untested (no rule ID in rules.c)

| Scenario | Reason |
|----------|--------|
| Memory port field in @check | Memory port field syntax (`mem.port.data`) cannot appear in @check expression context without triggering parse errors first; covered indirectly by CHECK_INVALID_EXPR_TYPE catching all non-constant expressions |

### Happy Path

| Test File | Coverage |
|-----------|----------|
| `9_4_HAPPY_PATH-check_expression_ok.jz` | CONFIG equality/comparison at project scope, clog2 of CONFIG, logical AND/OR of CONFIG comparisons, CONST in helper module, CONST arithmetic, nested CONST+literal, CONST/CONFIG/clog2/literal in top module, comparison operators (>=, !=), nested clog2+CONFIG+CONST, multi-module with @new |

## Section 9.5 — @check Evaluation Order

All section 9.5 rules covered by validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| CHECK_INVALID_EXPR_TYPE | error | Tested — 4 triggers for undefined identifiers at project scope (bare, arithmetic, comparison, clog2) |

### Untestable Scenarios

| Scenario | Reason |
|----------|--------|
| Transitive CONST resolution in @check | Compiler emits CONST_NEGATIVE_OR_NONINT when a CONST references another CONST; see issues.md |
| OVERRIDE values visible in @check | OVERRIDE values do not propagate to child module @check at lint time; see issues.md |

### Happy Path

| Test File | Coverage |
|-----------|----------|
| `9_5_HAPPY_PATH-check_evaluation_order_ok.jz` | CONFIG reference/arithmetic/clog2 at project scope, CONST in helper and top modules, CONST arithmetic, CONFIG in helper module, clog2 of CONST, literal, multi-module with @new |

## Section 10.1 — Template Purpose

No untested rules for section 10.1. This section is a conceptual overview — all template error rules are defined in sections 10.2–10.8.

### Happy Path

| Test File | Coverage |
|-----------|----------|
| `10_1_HAPPY_PATH-template_purpose_ok.jz` | Simple define+apply, file-scoped template, module-scoped template, @scratch wire, parameter substitution, zero-count apply, comments-only template, apply in ASYNCHRONOUS and SYNCHRONOUS contexts, duplication reduction across modules via @new |

## Section 4.4 — PORT (Module Interface)

All implemented rules from the test plan have corresponding validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| PORT_DIRECTION_MISMATCH_IN | error | Tested — 3 triggers across ASYNC root (helper), ASYNC root (top), ASYNC IF (top) across 2 modules |
| PORT_DIRECTION_MISMATCH_OUT | error | Tested — 2 triggers across SYNC RHS (helper), ASYNC RHS (top) across 2 modules |
| PORT_TRISTATE_MISMATCH | error | Tested — 2 triggers for z on OUT port in IF/ELSE across 2 helper modules; INOUT z as negative test |
| ASYNC_ALIAS_IN_CONDITIONAL | error | Tested — 3 triggers across IF then-branch (helper), IF else-branch (another), IF then-branch (top) across 3 modules |
| BUS_PORT_INVALID_ROLE | error | Tested — 2 triggers for invalid roles DRIVER and MASTER across 2 modules; SOURCE as negative test |
| BUS_PORT_ARRAY_COUNT_INVALID | error | Tested — 1 trigger for zero array count; positive array count as negative test |
| BUS_PORT_INDEX_REQUIRED | error | Tested — 2 triggers for write and read on arrayed BUS without index across helper module |
| BUS_PORT_INDEX_NOT_ARRAY | error | Tested — 2 triggers for write and read on scalar BUS with index across helper module |
| BUS_PORT_INDEX_OUT_OF_RANGE | error | Tested — 3 triggers for index 2 (count=2), index 4 (count=4), index 100 (count=4) across 2 modules |
| BUS_SIGNAL_UNDEFINED | error | Tested — 3 triggers for read/write to nonexistent signals on scalar and arrayed BUS across 2 modules |
| BUS_SIGNAL_READ_FROM_WRITABLE | error | Tested — 2 triggers for reading writable signals in SOURCE and TARGET roles across 2 modules |
| BUS_SIGNAL_WRITE_TO_READABLE | error | Tested — 2 triggers for writing readable signals in SOURCE and TARGET roles across 2 modules |
| BUS_WILDCARD_WIDTH_MISMATCH | error | Tested — 3 triggers for widths 3, 2, 8 on count=4 array across 3 modules |
| BUS_TRISTATE_MISMATCH | error | Tested — 1 trigger for z on readable signal in TARGET role; co-fires BUS_SIGNAL_WRITE_TO_READABLE and WARN_INTERNAL_TRISTATE |

Rules not effectively tested:

| Rule ID | Severity | Reason |
|---------|----------|--------|
| PORT_MISSING_WIDTH | error | Parser catches missing width as PARSE000 before semantic analysis; PORT_MISSING_WIDTH is never emitted (see `issues.md`) |
| BUS_PORT_UNKNOWN_BUS | error | Not implemented — compiler silently accepts BUS ports referencing undeclared bus names; fires NET_DANGLING_UNUSED instead (see `issues.md`) |
| BUS_PORT_NOT_BUS | error | Not implemented — compiler treats dot access on non-BUS port as UNDECLARED_IDENTIFIER instead (see `issues.md`) |

Happy-path coverage of IN read, OUT write, INOUT conditional, BUS SOURCE/TARGET, arrayed BUS, wildcard broadcast, dot notation access, and extension modifiers is provided by `4_4_HAPPY_PATH-valid_ports_ok.jz`.

## Section 5.1 — Asynchronous Assignments

All 4 rule IDs from the test plan have corresponding 5_1-prefixed validation tests:

| Rule ID | Severity | Status |
|---------|----------|--------|
| ASYNC_ALIAS_LITERAL_RHS | error | Tested — 4 triggers: `=` in helper module root, `=` in top module root, `=z` with literal, `=s` with literal |
| ASYNC_ASSIGN_REGISTER | error | Tested — 6 triggers: root (helper), root (top), IF branch, SELECT CASE ×3 |
| ASYNC_INVALID_STATEMENT_TARGET | error | Tested — 2 triggers: MEM SYNC OUT .addr in ASYNC (helper and top) |
| ASYNC_FLOATING_Z_READ | error | Tested — 2 triggers: unconditional z in helper, conditional all-z paths in top; WARN_INTERNAL_TRISTATE co-fires |

Rules from the Input/Output Matrix covered by other sections (no 5_1-prefixed test needed):

| Rule ID | Severity | Covered By |
|---------|----------|------------|
| ASYNC_UNDEFINED_PATH_NO_DRIVER | error | Covered by `4_10_ASYNC_UNDEFINED_PATH_NO_DRIVER-partial_coverage.jz` and `1_5_ASYNC_UNDEFINED_PATH_NO_DRIVER-partial_coverage.jz` |
| WIDTH_ASSIGN_MISMATCH_NO_EXT | error | Covered by `5_0_WIDTH_ASSIGN_MISMATCH_NO_EXT-alias_width_mismatch.jz` (note: ASSIGN_WIDTH_NO_MODIFIER fires at higher priority) |

Happy-path coverage of alias (=), drive (=>), receive (<=), sliced assignment, concat decomposition, register read, transitive alias, sign-extend (=s), zero-extend (=z), ternary receive, constant drive, and empty ASYNC block is provided by `5_1_HAPPY_PATH-async_assignments_ok.jz`.

## Section 7.5 — Initialization

| Rule ID | Severity | Status |
|---------|----------|--------|
| MEM_INIT_LITERAL_OVERFLOW | error | Tested — 2 triggers across helper and top modules |
| MEM_INIT_CONTAINS_X | error | Tested — 2 triggers: binary x in helper, all-x in top module |
| MEM_INIT_FILE_NOT_FOUND | error | Tested — 2 triggers across helper and top modules |
| MEM_INIT_FILE_TOO_LARGE | error | Tested — 2 triggers across helper and top modules |
| MEM_WARN_PARTIAL_INIT | warning | Tested — 2 triggers across helper and top modules |

| Rule ID | Severity | Reason |
|---------|----------|--------|
| MEM_INIT_FILE_CONTAINS_X | error | Not implemented — rule defined in `rules.c` but no semantic pass emits it; cannot be tested (see `issues.md`) |
| MEM_MISSING_INIT | error | Covered by `7_1_MEM_MISSING_INIT-missing_init.jz` (declaration-level check in section 7.1) |

Happy-path coverage of literal init (matching width, zero init) and @file init (exact-match file) across multi-module structure is provided by `7_5_HAPPY_PATH-initialization_ok.jz`.
