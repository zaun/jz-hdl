# Test Issues

## test_9_2-check_semantics.md

No issues found. All tests pass cleanly. CHECK_FAILED fires correctly in all contexts (project scope, module scope, helper module scope) with various expression types (CONFIG comparison, arithmetic zero, CONST comparison, clog2 mismatch, literal zero).

## test_4_1-module_canonical_form.md

No issues found. All tests pass cleanly. No parser recovery bugs, no cascading errors, no missing rule implementations. The MODULE_PORT_IN_ONLY test co-fires NET_DANGLING_UNUSED for unused input ports in input-only modules — this is expected behavior, not a bug.

## test_4_2-scope_and_uniqueness.md

### Bug: AMBIGUOUS_REFERENCE rule not implemented

**Severity:** Bug (missing rule implementation)

**Description:** The rule `AMBIGUOUS_REFERENCE` is defined in `rules.c` with message "S4.2/S8.1 Identifier reference ambiguous without instance prefix", but the compiler never emits this diagnostic. No semantic pass emits this rule — it is defined in `rules.c` but has no corresponding implementation code in any `src/sem/` file.

**Impact:** Cannot write a validation test for this rule. It is documented in `not_tested.md`.

### Note: BLACKBOX_NAME_DUP_IN_PROJECT co-fires WARN_UNUSED_MODULE

**Severity:** Expected behavior (not a bug)

**Description:** When a `@blackbox` name conflicts with a `@module` name, the module also fires `WARN_UNUSED_MODULE` because the blackbox name shadows it and the module cannot be instantiated. The test `.out` file includes both diagnostics. This is correct compiler behavior.

## test_10_1-template_purpose.md

No issues found. Happy-path test `10_1_HAPPY_PATH-template_purpose_ok.jz` passes with clean output (no diagnostics). Section 10.1 is conceptual overview only — all template error rules are in sections 10.2–10.8.

## test_10_3-template_allowed_content.md

### Resolved: TEMPLATE_EXTERNAL_REF works during template expansion

**Status:** Resolved (previous test was incorrect)

**Description:** The rule `TEMPLATE_EXTERNAL_REF` IS implemented in `template_expand.c` and fires correctly during `@apply` expansion. The previous test was incorrect — it defined templates with external references but never applied them with `@apply`, so the check never ran. The regenerated test uses `@apply` to trigger the templates and correctly produces 4 `TEMPLATE_EXTERNAL_REF` errors across file-scoped and module-scoped templates in multiple contexts (RHS of <=, expression, alias RHS, IF condition).

**Test file:** `10_3_TEMPLATE_EXTERNAL_REF-external_signal_reference.jz` — 4 triggers, all detected.

### Bug: TEMPLATE_SCRATCH_WIDTH_INVALID rule not implemented

**Severity:** Bug (missing rule implementation)

**Description:** The rule `TEMPLATE_SCRATCH_WIDTH_INVALID` is defined in `rules.c` with message "S10.3 @scratch width must be a positive integer constant expression", but the compiler does not emit this diagnostic. `@scratch` declarations with zero width or non-constant (parameter-based) width expressions compile without error. Note: when templates with zero-width scratches are applied via `@apply`, the generic `WIDTH_NONPOSITIVE_OR_NONINT` rule fires instead of `TEMPLATE_SCRATCH_WIDTH_INVALID`. Parameter-as-width is not caught at all.

**Minimal reproduction:**
```jz
@template bad(a, b)
    @scratch x [0];
    a <= b;
@endtemplate

@template bad2(a, b, n)
    @scratch x [n];
    a <= b;
@endtemplate
```
Expected: `TEMPLATE_SCRATCH_WIDTH_INVALID` error on `@scratch x [0]` and `@scratch x [n]`.
Actual: No diagnostic emitted (templates not applied). When applied, zero-width fires `WIDTH_NONPOSITIVE_OR_NONINT` instead.

**Test file:** `10_3_TEMPLATE_SCRATCH_WIDTH_INVALID-scratch_width_not_constant.jz` — test has 5 triggers (zero width and param-as-width in file-scoped and module-scoped templates) but the `.out` file is empty because the compiler does not detect any of them.

## test_10_4-template_forbidden_content.md

### Bug: Parser does not recover after TEMPLATE_FORBIDDEN_BLOCK_HEADER or TEMPLATE_FORBIDDEN_DIRECTIVE with realistic syntax

**Severity:** Bug (parser recovery)

**Description:** When a forbidden construct (SYNCHRONOUS/ASYNCHRONOUS block header or structural directive like @new/@module/@feature) appears inside a template body with realistic syntax (i.e., with block body `{ ... }` or full arguments), the parser correctly emits the `TEMPLATE_FORBIDDEN_BLOCK_HEADER` or `TEMPLATE_FORBIDDEN_DIRECTIVE` error, but then fails to skip the remaining tokens and produces a cascading `PARSE000` error. After the PARSE000, the parser cannot recover — it does not reach `@endtemplate`, subsequent templates, or module definitions in the rest of the file.

**Impact:** Each forbidden block header or directive trigger must be placed in its own test file. Multiple triggers cannot coexist in a single file because only the first one is reached.

**Note:** When the forbidden construct is a bare keyword (e.g., `ASYNCHRONOUS` alone, or `@new` alone without arguments), the parser DOES recover and can process subsequent templates. The recovery failure only occurs when realistic syntax follows the forbidden keyword.

**Minimal reproduction (ASYNCHRONOUS with block body):**
```jz
@template tmpl1(a, b)
    ASYNCHRONOUS {
        a = b;
    }
@endtemplate

@template tmpl2(a, b)
    SYNCHRONOUS(CLK=a) {
        b <= a;
    }
@endtemplate
```
Output: Only `tmpl1` triggers `TEMPLATE_FORBIDDEN_BLOCK_HEADER`; `tmpl2` is never parsed.

**Minimal reproduction (@new with full instantiation):**
```jz
@template tmpl1(a, b)
    @new inst_inner Mod {
        IN  [1] din = a;
        OUT [1] data = b;
    };
@endtemplate
```
Output: `TEMPLATE_FORBIDDEN_DIRECTIVE` followed by `PARSE000` on the module name token.

**Test files affected:** All `10_4_TEMPLATE_FORBIDDEN_BLOCK_HEADER-*.jz` and `10_4_TEMPLATE_FORBIDDEN_DIRECTIVE-*.jz` files were split into one-trigger-per-file to work around this.

### Observation: CDC block in template triggers TEMPLATE_FORBIDDEN_DECL instead of TEMPLATE_FORBIDDEN_BLOCK_HEADER

**Severity:** Observation

**Description:** The spec (S10.4) lists CDC alongside SYNCHRONOUS/ASYNCHRONOUS as a forbidden block header, but the compiler classifies CDC as a declaration block, triggering `TEMPLATE_FORBIDDEN_DECL` instead of `TEMPLATE_FORBIDDEN_BLOCK_HEADER`. The error message says "Declaration blocks (WIRE, REGISTER, etc.)" which does not clearly communicate that CDC was the offending construct. The compiler does correctly reject CDC in templates — the rule ID is just different from what the spec grouping implies.

**Minimal reproduction:**
```jz
@template cdc_tmpl(a, b)
    CDC {
        a <= b;
    }
@endtemplate
```
Output: `TEMPLATE_FORBIDDEN_DECL` (expected: `TEMPLATE_FORBIDDEN_BLOCK_HEADER` based on spec grouping).

## test_10_5-template_application.md

### Bug: TEMPLATE_APPLY_OUTSIDE_BLOCK rule not implemented

**Severity:** Bug (missing rule implementation)

**Description:** The rule `TEMPLATE_APPLY_OUTSIDE_BLOCK` is defined in `rules.c` with message "S10.5 @apply may only appear inside ASYNCHRONOUS or SYNCHRONOUS blocks", but the compiler does not emit this diagnostic. `@apply` statements placed at module scope (between declaration blocks) or at file scope (outside any module) are silently ignored — no error is produced.

**Minimal reproduction (module scope):**
```jz
@template simple(a, b)
    a <= b;
@endtemplate

@module Mod
    PORT { IN [1] din; OUT [1] data; }
    WIRE { w [1]; }
    @apply simple(w, din);
    ASYNCHRONOUS { data = din; }
@endmod
```
Expected: `TEMPLATE_APPLY_OUTSIDE_BLOCK` error on the `@apply` at module scope.
Actual: No diagnostic emitted; `@apply` is silently ignored.

**Minimal reproduction (file scope):**
```jz
@template simple(a, b)
    a <= b;
@endtemplate

@apply simple(x, y);

@module Mod
    PORT { IN [1] din; OUT [1] data; }
    ASYNCHRONOUS { data = din; }
@endmod
```
Expected: `TEMPLATE_APPLY_OUTSIDE_BLOCK` error on the `@apply` at file scope.
Actual: No diagnostic emitted; `@apply` is silently ignored.

**Test files:** `10_5_TEMPLATE_APPLY_OUTSIDE_BLOCK-apply_at_module_scope.jz` and `10_5_TEMPLATE_APPLY_OUTSIDE_BLOCK-apply_at_file_scope.jz` — tests have triggers but `.out` files are empty because the compiler does not detect them.

## test_10_2-template_definition.md

No issues found. All tests pass and match expected output.

## test_10_6-template_exclusive_assignment.md

### Observation: Duplicate @apply errors deduplicated by template source location

**Severity:** Observation

**Description:** When two separate modules each contain two `@apply` calls to the same template targeting the same wire/register, the compiler reports the error at the assignment line inside the template definition (e.g., `out <= GND;` at the template source location). Because both modules' violations map to the same template source line, the error appears only once in output rather than once per module. This means trigger coverage in multiple modules may not produce distinct error lines — the error is correctly detected but reported at a shared location.

**Impact:** Tests with identical @apply violations in multiple modules may show fewer error lines than the number of triggering contexts. This is not a bug per se — the compiler correctly rejects the code — but it means multi-module coverage for template-based violations is partially collapsed in diagnostic output.

## test_10_8-template_error_cases.md

No new issues. Section 10.8 is a cross-reference summary of all TEMPLATE_* error cases from S10.2-S10.6. All 15 rule IDs have existing tests that pass. Known compiler bugs (TEMPLATE_EXTERNAL_REF, TEMPLATE_SCRATCH_WIDTH_INVALID, and TEMPLATE_APPLY_OUTSIDE_BLOCK not enforced) are documented above in their respective subsection entries.

## test_12_4-path_security.md

### Observation: @import path error stops compilation — prevents combining @import and @file() triggers

**Severity:** Observation

**Description:** When an `@import` directive triggers a path security error (`PATH_ABSOLUTE_FORBIDDEN` or `PATH_TRAVERSAL_FORBIDDEN`), the compiler halts and does not continue parsing the rest of the file. This prevents testing `@import` and `@file()` contexts in the same file — they must be split into separate test files.

**Impact:** Each path security rule requires two test files (one for `@import` context, one for `@file()` context) rather than a single combined file.

### Not testable: PATH_OUTSIDE_SANDBOX and PATH_SYMLINK_ESCAPE

**Severity:** Test gap (by design)

**Description:** `PATH_OUTSIDE_SANDBOX` requires filesystem sandbox configuration at runtime (resolved canonical paths checked against sandbox roots). `PATH_SYMLINK_ESCAPE` requires actual symlinks on the filesystem. Neither can be tested with static `.jz`/`.out` pairs via `--info --lint`. These would need integration tests with filesystem setup.

## test_11_2-tristate_default_applicability.md

No issues found. Section 11.2 defines no diagnostic rules — it specifies the scope of applicability for `--tristate-default` (internal WIREs with tri-state drivers only; INOUT ports, external pins, and top-level signals are excluded). No tests were generated because there are no rule IDs to validate.

## test_11_1-tristate_default_purpose.md

### Observation: TRISTATE_TRANSFORM_UNUSED_DEFAULT reports `File: <input>` instead of actual filename

**Severity:** Observation (cosmetic)

**Description:** When `--tristate-default=GND` (or VCC) is specified but no internal tri-state nets exist to transform, the compiler emits `TRISTATE_TRANSFORM_UNUSED_DEFAULT` at location `0:0` with the file header `File: <input>` instead of the actual source filename. This is inconsistent with all other diagnostics which use the basename of the input file. The diagnostic itself is correct — only the filename in the header is wrong.

**Minimal reproduction:**
```bash
jz-hdl --info --lint --tristate-default=GND any_file_without_tristate.jz
```
Output:
```
File: <input>
       0:0    warn  TRISTATE_TRANSFORM_UNUSED_DEFAULT S11.7 --tristate-default specified but no internal tri-state nets found to transform
```
Expected: `File: any_file_without_tristate.jz`

**Impact:** The `.out` test file must use `File: <input>` to match the actual compiler output. This works but is inconsistent with the convention used by all other tests.

## test_11_3-tristate_net_identification.md

### Observation: NET_TRI_STATE_ALL_Z_READ rule unreachable — ASYNC_FLOATING_Z_READ fires instead

**Severity:** Observation

**Description:** The rule `NET_TRI_STATE_ALL_Z_READ` is defined in `rules.c` (category `NET_DRIVERS_AND_TRI_STATE`) with message "S4.10 All drivers assign `z` (tri-state) but signal is read; at least one driver must provide a value". However, the compiler never emits this diagnostic. Instead, the `ASYNC_FLOATING_Z_READ` rule (category `ASYNC_BLOCK_RULES`) fires first with message "S4.10/S1.5/S8.1 Net has sinks but all drivers assign `z` (tri-state bus fully released while read)". Both rules cover the same scenario, but `ASYNC_FLOATING_Z_READ` takes precedence.

**Minimal reproduction:**
```jz
@project TEST
    @top Mod { IN [1] en = _; OUT [8] data = _; }
@endproj
@module Mod
    PORT { IN [1] en; OUT [8] data; }
    WIRE { w [8]; }
    ASYNCHRONOUS {
        IF (en) { w <= 8'bzzzz_zzzz; } ELSE { w <= 8'bzzzz_zzzz; }
        data <= w;
    }
@endmod
```
Expected: `NET_TRI_STATE_ALL_Z_READ` error.
Actual: `ASYNC_FLOATING_Z_READ` error (plus `WARN_INTERNAL_TRISTATE` warning).

**Impact:** The test file `11_3_NET_TRI_STATE_ALL_Z_READ-all_drivers_z_but_read.jz` validates the all-z-read scenario but the `.out` file contains `ASYNC_FLOATING_Z_READ` diagnostics instead of `NET_TRI_STATE_ALL_Z_READ`. The behavior is correct (the error is detected), but the rule ID differs from what the test plan specifies.

## test_11_4-tristate_transformation_algorithm.md

### Bug: TRISTATE_TRANSFORM_SINGLE_DRIVER rule not implemented

**Severity:** Bug (missing rule implementation)

**Description:** The rule `TRISTATE_TRANSFORM_SINGLE_DRIVER` is defined in `rules.c` with message "S11.7 Single-driver tri-state net transformed to default value; original z replaced with GND/VCC", but the compiler never emits this diagnostic. When a single-driver tri-state wire is transformed by `--tristate-default`, only `INFO_TRISTATE_TRANSFORM` is emitted — no additional warning about the single-driver case.

**Minimal reproduction:**
```bash
jz-hdl --info --lint --tristate-default=GND single_driver.jz
```
With `single_driver.jz` containing a wire driven by one IF/ELSE with z in the else branch.
Expected: `TRISTATE_TRANSFORM_SINGLE_DRIVER` warning in addition to `INFO_TRISTATE_TRANSFORM`.
Actual: Only `INFO_TRISTATE_TRANSFORM` info is emitted.

**Test file:** `11_GND_4_TRISTATE_TRANSFORM_SINGLE_DRIVER-single_driver.jz` — test exercises the scenario but `.out` only contains `INFO_TRISTATE_TRANSFORM` because the warning is not emitted.

### Bug: TRISTATE_TRANSFORM_PER_BIT_FAIL rule not implemented

**Severity:** Bug (missing rule implementation)

**Description:** The rule `TRISTATE_TRANSFORM_PER_BIT_FAIL` is defined in `rules.c` with message "S11.7 Per-bit tri-state pattern detected; only full-width z assignments can be transformed", but the compiler never emits this diagnostic. When individual bit slices of the same signal have different enable conditions (different driver sets per bit group), the compiler transforms them without error.

**Minimal reproduction:**
```jz
@module Mod
    PORT { IN [1] en_a; IN [1] en_b; OUT [8] data; }
    WIRE { w [8]; }
    ASYNCHRONOUS {
        IF (en_a) { w[7:4] <= 4'hA; } ELSE { w[7:4] <= 4'bzzzz; }
        IF (en_b) { w[3:0] <= 4'hB; } ELSE { w[3:0] <= 4'bzzzz; }
        data <= w;
    }
@endmod
```
Expected: `TRISTATE_TRANSFORM_PER_BIT_FAIL` error.
Actual: `INFO_TRISTATE_TRANSFORM` info — transformation succeeds without error.

**Test file:** `11_GND_4_TRISTATE_TRANSFORM_PER_BIT_FAIL-per_bit_tristate.jz` — test exercises the per-bit pattern but `.out` only contains `INFO_TRISTATE_TRANSFORM` because the error is not emitted.

### Bug: TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL unreachable — NET_MULTIPLE_ACTIVE_DRIVERS fires first

**Severity:** Bug (unreachable rule)

**Description:** The rule `TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL` is implemented in `ir_tristate_transform.c` (line ~1610) and fires during post-transform verification when multiple always-active instance drivers remain. However, this situation cannot be reached because `NET_MULTIPLE_ACTIVE_DRIVERS` fires first during semantic analysis, preventing the IR tri-state transform from running.

The spec (S11.3) defines tri-state nets as having "two or more drivers in the same ASYNCHRONOUS block" with z assignments, but the compiler rejects multi-driver patterns earlier:
- Multiple IF/ELSE blocks assigning to the same wire → `ASSIGN_INDEPENDENT_IF_SELECT`
- Multiple instances driving the same wire → `NET_MULTIPLE_ACTIVE_DRIVERS`

**Minimal reproduction:**
```jz
@module DriverMod
    PORT { IN [1] en; INOUT [8] data; }
    ASYNCHRONOUS {
        IF (en) { data <= 8'hAA; } ELSE { data <= 8'bzzzz_zzzz; }
    }
@endmod

@module TopMod
    PORT { IN [1] en; OUT [8] data; }
    WIRE { w [8]; }
    @new inst_a DriverMod { IN [1] en = en; INOUT [8] data = w; };
    @new inst_b DriverMod { IN [1] en = en; INOUT [8] data = w; };
    ASYNCHRONOUS { data <= w; }
@endmod
```
Expected: `TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL` (after tri-state transform).
Actual: `NET_MULTIPLE_ACTIVE_DRIVERS` error (semantic analysis blocks transform).

**Test file:** `11_GND_4_TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL-non_exclusive.jz` — test exercises the multi-driver pattern but `.out` contains `NET_MULTIPLE_ACTIVE_DRIVERS` because the earlier semantic rule fires first.

## test_11_5-tristate_validation_rules.md

No new issues. Section 11.5 is a cross-reference to `TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL` and `TRISTATE_TRANSFORM_PER_BIT_FAIL`, both already documented under section 11.7 above. Happy-path test `11_GND_5_HAPPY_PATH-tristate_validation_ok.jz` verifies valid tristate transformation with mutually exclusive enables and full-width z assignments.

### Observation: Multi-driver priority chain (S11.4.1) not achievable in current compiler

**Severity:** Observation

**Description:** The spec (S11.4.1) describes a priority-chain conversion for multiple tri-state drivers on the same net. However, the compiler's current semantic rules prevent this pattern from being expressed:
- Same-block multi-assignment to same signal → blocked by `ASSIGN_INDEPENDENT_IF_SELECT`
- Multi-instance driving same wire → blocked by `NET_MULTIPLE_ACTIVE_DRIVERS`

The two-driver and three-driver priority chain scenarios from the test plan (Happy Path 1 and 2) cannot be tested because no valid JZ-HDL code can express the multi-driver pattern that would reach the tri-state transformation phase. Only single-driver tri-state (one IF/ELSE with z in else) successfully triggers `INFO_TRISTATE_TRANSFORM`.

## test_11_6-tristate_inout_handling.md

### Bug: TRISTATE_TRANSFORM_BLACKBOX_PORT rule not implemented

**Severity:** Bug (missing rule implementation)

**Description:** The rule `TRISTATE_TRANSFORM_BLACKBOX_PORT` is defined in `rules.c` with message "S11.7 Tri-state signal driven by blackbox port cannot be transformed; use external pull resistor", but no code in `ir_tristate_transform.c` emits this diagnostic. The blackbox port handling path does not exist in the tri-state transform — there is no `BLACKBOX` string in the entire file. Blackbox modules are not yet handled by the tri-state transform engine.

**Impact:** Cannot test this rule. No validation test is possible.

### Observation: TRISTATE_TRANSFORM_OE_EXTRACT_FAIL reports under `File: <input>` instead of actual filename

**Severity:** Observation (cosmetic)

**Description:** When OE extraction fails during the tri-state transform, the `TRISTATE_TRANSFORM_OE_EXTRACT_FAIL` diagnostic is emitted with `loc.filename = NULL`, which the diagnostic printer renders as `File: <input>`. This is the same cosmetic issue as `TRISTATE_TRANSFORM_UNUSED_DEFAULT` (documented in section 11.1). The diagnostic is correct — only the filename in the header is wrong.

**Impact:** The `.out` file must use `File: <input>` for this diagnostic and place it in a separate file section from the main diagnostics.

### Observation: OE extraction failure requires concatenated z literals

**Severity:** Observation

**Description:** Triggering `TRISTATE_TRANSFORM_OE_EXTRACT_FAIL` requires a narrow set of conditions:
1. Two or more instance INOUT ports must drive the same parent wire (shared net)
2. The tristate proof engine must verify mutual exclusion (requires explicit comparison guards like `en == 1'b1`)
3. At least one child module's INOUT port must be driven with a z pattern the OE extractor cannot parse

The OE extractor handles ternary patterns (`cond ? data : z`) and IF/ELSE patterns (`IF (cond) { port <= data } ELSE { port <= z }`). The only patterns that fail extraction while still passing the proof are those using non-literal z expressions — specifically, concatenated z literals like `{4'bzzzz, 4'bzzzz}`. The AST-level proof engine's `tristate_expr_is_all_z_literal` handles concatenation of z literals, but the IR-level `literal_is_z_or_zero` check only handles scalar `EXPR_LITERAL` nodes, not `EXPR_CONCAT`.

**Minimal reproduction:**
```jz
@module DriverB
    PORT { IN [1] en; IN [8] din; INOUT [8] data; }
    ASYNCHRONOUS {
        data <= (en == 1'b0) ? din : {4'bzzzz, 4'bzzzz};
    }
@endmod
```
The concatenated z `{4'bzzzz, 4'bzzzz}` is semantically equivalent to `8'bzzzz_zzzz` but the OE extractor at the IR level doesn't recognize it as z.

## test_11_7-tristate_error_conditions.md

Section 11.7 is the comprehensive error matrix for `--tristate-default`. All 6 rules in this section are cross-references to rules already documented in earlier sections. No new compiler bugs were discovered — all issues below are re-confirmations of previously documented behavior.

### Bug: TRISTATE_TRANSFORM_SINGLE_DRIVER not emitted (same as 11.4)

**Severity:** Bug (missing rule implementation)

**Description:** Same issue documented under section 11.4 above. Single-driver tri-state nets are transformed with only `INFO_TRISTATE_TRANSFORM` — no `TRISTATE_TRANSFORM_SINGLE_DRIVER` warning is emitted.

**Test file:** `11_GND_7_TRISTATE_TRANSFORM_SINGLE_DRIVER-single_driver.jz` — `.out` captures `INFO_TRISTATE_TRANSFORM` only.

### Bug: TRISTATE_TRANSFORM_PER_BIT_FAIL not emitted (same as 11.4)

**Severity:** Bug (missing rule implementation)

**Description:** Same issue documented under section 11.4. Per-bit tri-state patterns are transformed without error.

**Test file:** `11_GND_7_TRISTATE_TRANSFORM_PER_BIT_FAIL-per_bit_tristate.jz` — `.out` captures `INFO_TRISTATE_TRANSFORM` only.

### Bug: TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL unreachable (same as 11.4)

**Severity:** Bug (unreachable rule)

**Description:** Same issue documented under section 11.4. `NET_MULTIPLE_ACTIVE_DRIVERS` fires at semantic analysis before the tri-state transform runs.

**Test file:** `11_GND_7_TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL-non_exclusive.jz` — `.out` captures `NET_MULTIPLE_ACTIVE_DRIVERS` instead.

### Bug: TRISTATE_TRANSFORM_BLACKBOX_PORT not implemented (same as 11.6)

**Severity:** Bug (missing rule implementation)

**Description:** Same issue documented under section 11.6. Rule defined in `rules.c` but never emitted. When a blackbox INOUT port shares a wire with a normal tri-state driver, the compiler emits `TRISTATE_TRANSFORM_OE_EXTRACT_FAIL` instead.

**Test file:** `11_GND_7_TRISTATE_TRANSFORM_BLACKBOX_PORT-blackbox_tristate.jz` — `.out` captures `TRISTATE_TRANSFORM_OE_EXTRACT_FAIL`.

## test_11_8-tristate_portability.md

No issues found. Section 11.8 is a best practices section that introduces no new diagnostic rules. All referenced rules are cross-references to rules already tested and documented under sections 11.1, 11.4, and 11.7.

## test_12_1-compile_errors.md

No issues found. Section 12.1 is a cross-reference aggregation section. Both the happy-path and multiple-errors tests pass cleanly. The compiler correctly reports errors from 6 different categories (KEYWORD_AS_IDENTIFIER, REG_INIT_WIDTH_MISMATCH, SLICE_MSB_LESS_THAN_LSB, LIT_OVERFLOW, UNDECLARED_IDENTIFIER, ASSIGN_WIDTH_NO_MODIFIER) in a single file with correct line/column locations.

## test_12_2-combinational_loop_errors.md

No issues found. Both `COMB_LOOP_UNCONDITIONAL` and `COMB_LOOP_CONDITIONAL_SAFE` are correctly detected by the compiler. All tests pass with expected diagnostics. Note: wire assignments in ASYNCHRONOUS blocks must use `<=` (not `=` alias) to properly model combinational dependencies for loop detection; `=` alias inside IF/SELECT triggers `ASYNC_ALIAS_IN_CONDITIONAL`.

## test_12_3-recommended_warnings.md

### Bug: WARN_INCOMPLETE_SELECT_ASYNC rule not implemented

**Severity:** Bug (missing rule implementation)

**Description:** The rule `WARN_INCOMPLETE_SELECT_ASYNC` is defined in `rules.c` with message "S5.4/S8.3 Incomplete SELECT coverage without DEFAULT in ASYNCHRONOUS block", but the compiler never emits this diagnostic. Instead, the existing `SELECT_DEFAULT_RECOMMENDED_ASYNC` rule (category `CONTROL_FLOW_IF_SELECT`) fires with message "S5.4/S8.3 ASYNCHRONOUS SELECT without DEFAULT (may cause floating nets)". Both rules cover the same scenario — the `GENERAL_WARNINGS` variant is never used.

**Minimal reproduction:**
```jz
@module Mod
    PORT { IN [4] sel; OUT [1] data; }
    ASYNCHRONOUS {
        SELECT (sel) {
            CASE 0 { data <= VCC; }
            CASE 1 { data <= GND; }
        }
    }
@endmod
```
Expected: `WARN_INCOMPLETE_SELECT_ASYNC` warning.
Actual: `SELECT_DEFAULT_RECOMMENDED_ASYNC` warning + `ASYNC_UNDEFINED_PATH_NO_DRIVER` error.

**Test file:** `12_3_WARN_INCOMPLETE_SELECT_ASYNC-incomplete_select.jz` — test captures `SELECT_DEFAULT_RECOMMENDED_ASYNC` and `ASYNC_UNDEFINED_PATH_NO_DRIVER` in `.out` since `WARN_INCOMPLETE_SELECT_ASYNC` is never emitted.

### Bug: WARN_UNUSED_WIRE rule not implemented

**Severity:** Bug (missing rule implementation)

**Description:** The rule `WARN_UNUSED_WIRE` is defined in `rules.c` with message "S12.3 WIRE declared but never driven or read; remove it if unused", but the compiler never emits this diagnostic. Instead, the existing `NET_DANGLING_UNUSED` rule fires with message "S5.1/S8.3 Signal is neither driven nor read; remove it or connect it". Both rules cover the same scenario.

**Minimal reproduction:**
```jz
@module Mod
    PORT { IN [1] din; OUT [1] data; }
    WIRE { unused_w [8]; }
    ASYNCHRONOUS { data <= din; }
@endmod
```
Expected: `WARN_UNUSED_WIRE` warning.
Actual: `NET_DANGLING_UNUSED` warning.

**Test file:** `12_3_WARN_UNUSED_WIRE-unused_wire.jz` — test captures `NET_DANGLING_UNUSED` in `.out` since `WARN_UNUSED_WIRE` is never emitted.

### Bug: WARN_UNUSED_PORT rule not implemented

**Severity:** Bug (missing rule implementation)

**Description:** The rule `WARN_UNUSED_PORT` is defined in `rules.c` with message "S12.3 PORT declared but never used; remove it if unused", but the compiler never emits this diagnostic. Instead, the existing `NET_DANGLING_UNUSED` rule fires with message "S5.1/S8.3 Signal is neither driven nor read; remove it or connect it". Both rules cover the same scenario for ports that are neither driven nor read within the module.

**Minimal reproduction:**
```jz
@module Mod
    PORT { IN [1] clk; IN [1] unused_in; OUT [1] data; }
    REGISTER { r [1] = 1'b0; }
    ASYNCHRONOUS { data = r; }
    SYNCHRONOUS(CLK=clk) { r <= ~r; }
@endmod
```
Expected: `WARN_UNUSED_PORT` warning on `unused_in`.
Actual: `NET_DANGLING_UNUSED` warning on `unused_in`.

**Test file:** `12_3_WARN_UNUSED_PORT-unused_port.jz` — test captures `NET_DANGLING_UNUSED` in `.out` since `WARN_UNUSED_PORT` is never emitted.

## test_1_1-identifiers.md

### Bug: WIRE block names not validated for lexer-level identifier rules

**Severity:** Bug (missing validation)

**Description:** Identifier names declared inside `WIRE { ... }` blocks are not checked by the lexer-level rules `ID_SYNTAX_INVALID`, `ID_SINGLE_UNDERSCORE`, or `KEYWORD_AS_IDENTIFIER`. A 256-char wire name, `_` as a wire name, or a keyword (`MUX`, `REGISTER`, etc.) as a wire name is silently accepted. The wire gets registered in the symbol table and may later trigger `NET_DANGLING_UNUSED` if unused, but the expected identifier validation error is never emitted.

This contrasts with PORT, CONST, and REGISTER blocks, where identifier names are correctly validated.

**Minimal reproduction:**
```jz
@module Mod
    PORT { IN [1] din; OUT [1] data; }
    WIRE { _ [1]; }
    ASYNCHRONOUS { data <= din; }
@endmod
```
Expected: `ID_SINGLE_UNDERSCORE` error on `_` in WIRE block.
Actual: `NET_DANGLING_UNUSED` warning (wire registered but unused; no identifier error).

**Impact:** WIRE name contexts cannot be tested for `ID_SYNTAX_INVALID`, `ID_SINGLE_UNDERSCORE`, or `KEYWORD_AS_IDENTIFIER`. Tests omit the WIRE context for these rules.

### Bug: VCC and GND produce PARSE000 instead of KEYWORD_AS_IDENTIFIER

**Severity:** Bug (wrong diagnostic)

**Description:** The spec lists `VCC` and `GND` as reserved identifiers (semantic drivers). When used as user-declared names (e.g., port name, const name, register name), the compiler produces `PARSE000` ("expected identifier") instead of `KEYWORD_AS_IDENTIFIER`. This is because VCC and GND are tokenized as special expression tokens rather than as keywords, so the parser fails to recognize them in identifier positions.

**Minimal reproduction:**
```jz
@module Mod
    PORT { IN [1] clk; OUT [1] VCC; }
    REGISTER { r [1] = 1'b0; }
    ASYNCHRONOUS { VCC = r; }
    SYNCHRONOUS(CLK=clk) { r <= ~r; }
@endmod
```
Expected: `KEYWORD_AS_IDENTIFIER` error on `VCC`.
Actual: `PARSE000 parse error near token 'VCC': expected port name after width in PORT block`.

**Impact:** VCC and GND cannot be tested for `KEYWORD_AS_IDENTIFIER`. They are handled by a different code path that produces a generic parse error.

### Observation: Most spec-listed reserved identifiers are not actually reserved

**Severity:** Observation (spec/implementation divergence)

**Description:** The spec (S1.1) lists many reserved identifiers across categories: clock types (PLL, DLL, CLKDIV), CDC types (BIT, BUS, FIFO, HANDSHAKE, PULSE, MCP, RAW), memory types (BLOCK, DISTRIBUTED), memory ports (ASYNC, SYNC, WRITE_FIRST, READ_FIRST, NO_CHANGE), and semantic drivers (VCC, GND). However, only `IDX` (template/array) is actually treated as a reserved keyword by the compiler. All others are accepted as valid user identifiers without any diagnostic.

**Impact:** Only IDX can be tested for `KEYWORD_AS_IDENTIFIER` among the "reserved identifiers" category. Tests use IDX across all contexts.

### Observation: Block-level keywords cause cascading parse failures when used as register names

**Severity:** Observation (parser behavior)

**Description:** Keywords that introduce block-level constructs (`IF`, `LATCH`, `REGISTER`, etc.) cannot be tested for `KEYWORD_AS_IDENTIFIER` inside REGISTER blocks. The parser interprets them as the start of their respective constructs rather than as identifier names. For example, `IF [1] = 1'b0;` in a REGISTER block produces `IF_COND_MISSING_PARENS`, and `LATCH [1] = 1'b0;` produces `PARSE000` with cascading failures that suppress all prior diagnostics in the file. Keywords like `CONFIG`, `WIRE`, `MUX`, `SELECT` work correctly in register-name context because they are not block-initiating keywords.

**Impact:** Tests use `CONFIG` as the keyword-as-register-name test case, which is reliably detected.

## test_2_3-bit_width_constraints.md

No issues found. All 6 rules from the test plan are correctly detected by the compiler. `WIDTH_ASSIGN_MISMATCH_NO_EXT` co-fires with `ASSIGN_WIDTH_NO_MODIFIER` in `driver_assign.c` but only `ASSIGN_WIDTH_NO_MODIFIER` appears in `--info --lint` output (higher priority). The `WIDTH_NONPOSITIVE_OR_NONINT` test produces expected `NET_DANGLING_UNUSED` warnings for zero-width signals — these are a direct consequence of the construct under test (a zero-width signal cannot be meaningfully connected).

## test_1_4-comments.md

No issues found. Both `COMMENT_IN_TOKEN` and `COMMENT_NESTED_BLOCK` are correctly detected by the compiler in all tested contexts. Multiple triggers per file work without cascading errors. The happy-path test confirms no false positives from valid comments in all whitespace-legal positions.

Note: Unterminated block comments (`/* no end`) produce a lexer error with no rule ID in `rules.c` — this is by design and cannot be tested via the validation framework.

## test_1_2-fundamental_terms.md

No issues found. All 7 error/warning rules and the happy-path test pass with correct diagnostics. DOMAIN_CONFLICT and MULTI_CLK_ASSIGN both fire together when a register is assigned in two different clock domains (expected — the compiler reports both the multi-clock and domain-conflict aspects).

## test_1_5-exclusive_assignment_rule.md

No issues found. All 5 exclusive assignment rules are correctly detected by the compiler in all tested contexts. SYNC-context equivalents (`SYNC_MULTI_ASSIGN_SAME_REG_BITS`, `SYNC_ROOT_AND_CONDITIONAL_ASSIGN`) are separate rules tested under their own sections. `ASSIGN_SLICE_OVERLAP` fires in both ASYNC and SYNC contexts.

## test_2_2-signedness_model.md

No issues found. The happy-path test compiles cleanly with all unsigned arithmetic (+, -, *, ^, &, |, ~), comparison (>, <, ==, !=), and signed intrinsic (sadd, smul) operators. The TYPE_BINOP_WIDTH_MISMATCH error test correctly detects 4 width-mismatch triggers across comparison (>, <, !=) and arithmetic (+) operators in ASYNC and SYNC blocks across 2 modules. No cascading errors, parser recovery issues, or unexpected diagnostics encountered.

## test_2_1-literals.md

### Test gap: Semantic literal rules cannot be tested in async direct-assignment context

**Severity:** Test gap (compiler design constraint)

**Description:** When a sized literal or bare integer appears as the direct RHS of `=` in an ASYNCHRONOUS block (e.g., `data = 8'hFF;`), the compiler fires `ASYNC_ALIAS_LITERAL_RHS` ("Literal on RHS of `=` in ASYNCHRONOUS block; did you mean `<=` or `=>`?") alongside the literal rule under test. This prevents cleanly testing the following literal rules in async direct-assignment context without unrelated diagnostics:
- `LIT_BARE_INTEGER`
- `LIT_OVERFLOW`
- `LIT_WIDTH_NOT_POSITIVE`
- `LIT_UNDEFINED_CONST_WIDTH`

Parse-level literal errors (`LIT_UNSIZED`, `LIT_UNDERSCORE_AT_EDGES`, `LIT_DECIMAL_HAS_XZ`, `LIT_INVALID_DIGIT_FOR_BASE`) are NOT affected — they fire before `ASYNC_ALIAS_LITERAL_RHS` and suppress it.

**Impact:** The four semantic literal rules are tested only in SYNCHRONOUS and REGISTER init contexts. Async direct-assignment context is omitted from their tests to avoid `ASYNC_ALIAS_LITERAL_RHS` co-firing.

## test_2_4-special_semantic_drivers.md

### Bug: SPECIAL_DRIVER_IN_INDEX unreachable — parser emits PARSE000 instead

**Severity:** Bug (wrong diagnostic)

**Description:** The rule `SPECIAL_DRIVER_IN_INDEX` is defined in `rules.c` with message "S2.3 GND/VCC may not appear in slice/index expressions", but the parser rejects GND/VCC in index position with `PARSE000` before the semantic rule can fire. The parser does not recognize GND/VCC as valid expression tokens in index contexts, producing a generic parse error instead of the specific semantic diagnostic.

**Minimal reproduction:**
```jz
@module Mod
    PORT { IN [1] clk; OUT [1] data; }
    REGISTER { r [8] = 8'h00; }
    ASYNCHRONOUS { data = r[GND]; }
    SYNCHRONOUS(CLK=clk) { r <= r; }
@endmod
```
Expected: `SPECIAL_DRIVER_IN_INDEX` error on `GND`.
Actual: `PARSE000 parse error near token 'GND': expected expression in index`.

**Impact:** The test file `2_4_SPECIAL_DRIVER_IN_INDEX-gnd_vcc_in_index.jz` captures `PARSE000` in its `.out` file since `SPECIAL_DRIVER_IN_INDEX` is never emitted. VCC in index would produce the same result, so only one trigger is included.

## test_1_6-high_impedance_and_tristate.md

No new issues found. All 4 tested rules (`LIT_DECIMAL_HAS_XZ`, `LIT_INVALID_DIGIT_FOR_BASE`, `PORT_TRISTATE_MISMATCH`, `REG_INIT_CONTAINS_Z`) are correctly detected by the compiler in all tested contexts. Two rules from the test plan (`NET_MULTIPLE_ACTIVE_DRIVERS`, `NET_TRI_STATE_ALL_Z_READ`) are not tested under section 1.6 — they have dedicated tests under section 11.3. The `NET_TRI_STATE_ALL_Z_READ` observation (ASYNC_FLOATING_Z_READ fires instead) is already documented under section 11.3 above.

## test_3_1-operator_categories.md

No issues found. Both error rules (`LOGICAL_WIDTH_NOT_1`, `TYPE_BINOP_WIDTH_MISMATCH`) are correctly detected by the compiler in all tested contexts (ASYNC/SYNC blocks across helper and top modules). The happy-path test (`3_1_HAPPY_PATH-operator_categories_ok.jz`) compiles cleanly with all operator categories. No parser recovery issues or cascading errors encountered.

## test_3_2-operator_definitions.md

No issues found. All 8 rules (`UNARY_ARITH_MISSING_PARENS`, `LOGICAL_WIDTH_NOT_1`, `TERNARY_COND_WIDTH_NOT_1`, `TERNARY_BRANCH_WIDTH_MISMATCH`, `CONCAT_EMPTY`, `DIV_CONST_ZERO`, `DIV_UNGUARDED_RUNTIME_ZERO`, `OBS_X_TO_OBSERVABLE_SINK`) are correctly detected by the compiler in all tested contexts. The happy-path test compiles cleanly with parenthesized unary ops, guarded division patterns (!=, >, ==nonzero, nested), shift operations, ternary, and concatenation. No parser recovery issues or cascading errors encountered.

## test_4_10-asynchronous_block.md

### Observation: WIDTH_ASSIGN_MISMATCH_NO_EXT suppressed by ASSIGN_WIDTH_NO_MODIFIER

**Severity:** Observation (rule priority suppression)

**Description:** The rule `WIDTH_ASSIGN_MISMATCH_NO_EXT` is defined in `rules.c` (category `WIDTHS_AND_SLICING`) and is emitted alongside `ASSIGN_WIDTH_NO_MODIFIER` in `driver_assign.c` (line ~1380) for width-mismatch scenarios. However, only `ASSIGN_WIDTH_NO_MODIFIER` appears in `--info --lint` output due to the priority chain: `ASSIGN_CONCAT_WIDTH_MISMATCH > ASSIGN_WIDTH_NO_MODIFIER > WIDTH_ASSIGN_MISMATCH_NO_EXT` (documented in the `rules.c` header comment). The lower-priority `WIDTH_ASSIGN_MISMATCH_NO_EXT` is never visible in diagnostic output.

**Impact:** `WIDTH_ASSIGN_MISMATCH_NO_EXT` cannot be independently tested via `--info --lint`. The scenario is covered by `ASSIGN_WIDTH_NO_MODIFIER` tests under section 2.3 (`2_3_ASSIGN_WIDTH_NO_MODIFIER-assign_width_mismatch.jz`).

### Observation: ASYNC_FLOATING_Z_READ co-fires WARN_INTERNAL_TRISTATE

**Severity:** Observation (expected co-fire)

**Description:** When testing `ASYNC_FLOATING_Z_READ` (net driven only by z but read), the compiler also emits `WARN_INTERNAL_TRISTATE` ("Internal tri-state logic is not FPGA-compatible"). This is expected behavior — a wire driven by z is tri-state logic, and the internal tristate warning fires for any tri-state wire not on an INOUT port. The test `.out` file includes both diagnostics.

### Observation: ASYNC_UNDEFINED_PATH_NO_DRIVER co-fires SELECT_DEFAULT_RECOMMENDED_ASYNC for SELECT triggers

**Severity:** Observation (expected co-fire)

**Description:** When testing `ASYNC_UNDEFINED_PATH_NO_DRIVER` with a SELECT-without-DEFAULT trigger, the compiler also emits `SELECT_DEFAULT_RECOMMENDED_ASYNC` ("ASYNCHRONOUS SELECT without DEFAULT"). This is expected — missing DEFAULT both leaves signals undriven (error) and is a style warning. The test `.out` file includes both diagnostics.

## test_4_11-synchronous_block.md

### Observation: MULTI_CLK_ASSIGN co-fires DOMAIN_CONFLICT

**Severity:** Observation (expected co-fire)

**Description:** When testing `MULTI_CLK_ASSIGN` (same register assigned in SYNCHRONOUS blocks with different clocks), the compiler also emits `DOMAIN_CONFLICT` ("Register or CDC alias used in SYNCHRONOUS block whose CLK does not match its home-domain clock"). This is expected — a register assigned in two different clock domains violates both the multi-clock assignment rule and the domain conflict rule. The register's home domain is set by its first assignment, and the second assignment in a different domain triggers both diagnostics. The test `.out` file includes both diagnostics.

## test_4_12-cdc_block.md

### Bug: CDC_SOURCE_NOT_PLAIN_REG rule not implemented

**Severity:** Bug (missing rule implementation)

**Description:** The rule `CDC_SOURCE_NOT_PLAIN_REG` is defined in `rules.c` with message "S4.12 CDC source must be a plain register identifier, not a slice or expression", but the compiler does not emit this diagnostic. When a sliced register (e.g., `wide_reg[0:0]`) is used as a CDC source, the compiler silently drops the CDC entry instead of reporting an error. This causes the register to appear unsinked (`WARN_UNSINKED_REGISTER`) because the CDC entry was not recognized.

**Minimal reproduction:**
```jz
@module Mod
    PORT { IN [1] clk_a; IN [1] clk_b; OUT [1] data; }
    REGISTER { wide_reg [8] = 8'h00; }
    CDC {
        BIT wide_reg[0:0] (clk_a) => slice_sync (clk_b);
    }
    ASYNCHRONOUS { data = slice_sync; }
    SYNCHRONOUS(CLK=clk_a) { wide_reg <= 8'hAA; }
@endmod
```
Expected: `CDC_SOURCE_NOT_PLAIN_REG` error on `wide_reg[0:0]`.
Actual: No `CDC_SOURCE_NOT_PLAIN_REG` emitted. `WARN_UNSINKED_REGISTER` fires because the CDC entry is silently dropped.

**Test file:** `4_12_CDC_SOURCE_NOT_PLAIN_REG-sliced_source.jz` — test has 2 triggers across 2 modules but the `.out` file captures `WARN_UNSINKED_REGISTER` instead of `CDC_SOURCE_NOT_PLAIN_REG` because the rule is not implemented.

### Bug: CDC_DEST_ALIAS_ASSIGNED rule not implemented

**Severity:** Bug (missing rule implementation)

**Description:** The rule `CDC_DEST_ALIAS_ASSIGNED` is defined in `rules.c` with message "S4.12 CDC destination alias may not be assigned directly in block statements", but the compiler does not emit this diagnostic. When a CDC destination alias is assigned in an ASYNCHRONOUS block (e.g., `dest_sync = din;`), no error is produced. When assigned in a SYNCHRONOUS block (e.g., `top_dest <= din;`), the compiler emits `WRITE_WIRE_IN_SYNC` instead — it treats the alias as a wire rather than recognizing it as a CDC destination alias.

**Minimal reproduction:**
```jz
@module Mod
    PORT { IN [1] clk_a; IN [1] clk_b; IN [1] din; OUT [1] data; }
    REGISTER { src_reg [1] = 1'b0; }
    CDC {
        BIT src_reg (clk_a) => dest_sync (clk_b);
    }
    ASYNCHRONOUS {
        dest_sync = din;   // Expected: CDC_DEST_ALIAS_ASSIGNED — Actual: no error
        data = dest_sync;
    }
    SYNCHRONOUS(CLK=clk_a) { src_reg <= din; }
    SYNCHRONOUS(CLK=clk_b) {
        dest_sync <= din;  // Expected: CDC_DEST_ALIAS_ASSIGNED — Actual: WRITE_WIRE_IN_SYNC
    }
@endmod
```
Expected: `CDC_DEST_ALIAS_ASSIGNED` on both assignments to `dest_sync`.
Actual: ASYNC assignment produces no error; SYNC assignment produces `WRITE_WIRE_IN_SYNC`.

**Test file:** `4_12_CDC_DEST_ALIAS_ASSIGNED-dest_alias_written.jz` — test has 2 triggers (ASYNC alias-assign, SYNC receive-assign) across 2 modules. Only the SYNC trigger produces any diagnostic (`WRITE_WIRE_IN_SYNC` instead of `CDC_DEST_ALIAS_ASSIGNED`). The ASYNC trigger is silently accepted.

### Observation: CDC_DEST_ALIAS_DUP co-fires ID_DUP_IN_MODULE

**Severity:** Observation (expected co-fire)

**Description:** When testing `CDC_DEST_ALIAS_DUP` (CDC destination alias name conflicts with an existing identifier), the compiler also emits `ID_DUP_IN_MODULE` ("Duplicate identifier within module"). This is expected — a CDC dest alias that reuses an existing name violates both the CDC-specific duplicate check and the general module-scope uniqueness check. The test `.out` file includes both diagnostics.

## test_4_13-module_instantiation.md

### Bug: INSTANCE_ARRAY_MULTI_DIMENSIONAL rule never emitted

**Severity:** Bug (missing rule implementation)

**Description:** The rule `INSTANCE_ARRAY_MULTI_DIMENSIONAL` is defined in `rules.c` with message "S4.13.1 Multi-dimensional instance arrays are not supported", but the compiler's parser does not recognize the `@new name[N][M]` syntax. Instead, the parser emits `PARSE000` ("parse error near token '[': expected module or blackbox name after instance name") when it encounters the second `[` after the first array dimension. The semantic rule is never reached.

**Minimal reproduction:**
```jz
@project Test
    @top TopMod { IN [1] clk = _; OUT [1] data = _; }
@endproj
@module ChildMod
    PORT { IN [1] din; OUT [1] data; }
    ASYNCHRONOUS { data = din; }
@endmod
@module TopMod
    PORT { IN [1] clk; OUT [1] data; }
    @new inst_a[4][2] ChildMod {
        IN [1] din = r;
        OUT [1] data = _;
    };
    REGISTER { r [1] = 1'b0; }
    ASYNCHRONOUS { data = r; }
    SYNCHRONOUS(CLK=clk) { r <= ~r; }
@endmod
```
Expected: `INSTANCE_ARRAY_MULTI_DIMENSIONAL` error.
Actual: `PARSE000` at the second `[`.

### Bug: INSTANCE_ARRAY_COUNT_INVALID fires for CONST-based array counts

**Severity:** Bug (false positive)

**Description:** The spec (S4.13.1) states that instance array count can be "a positive integer literal or a CONST expression evaluated in the parent module's scope." However, the compiler rejects CONST-based array counts (e.g., `@new inst[MY_CONST] Mod { ... }`) with `INSTANCE_ARRAY_COUNT_INVALID`, even when the CONST evaluates to a valid positive integer.

**Minimal reproduction:**
```jz
@project Test
    @top TopMod { IN [1] clk = _; OUT [1] data = _; }
@endproj
@module ChildMod
    PORT { IN [1] din; OUT [1] data; }
    ASYNCHRONOUS { data = din; }
@endmod
@module TopMod
    CONST { COUNT = 4; }
    PORT { IN [1] clk; OUT [1] data; }
    @new inst[COUNT] ChildMod {
        IN [1] din = r;
        OUT [1] data = _;
    };
    REGISTER { r [1] = 1'b0; }
    ASYNCHRONOUS { data = r; }
    SYNCHRONOUS(CLK=clk) { r <= ~r; }
@endmod
```
Expected: No error (COUNT = 4 is a valid positive integer CONST).
Actual: `INSTANCE_ARRAY_COUNT_INVALID` error.

### Observation: INSTANCE_BUS_MISMATCH fires twice for combined id + role mismatch

**Severity:** Observation (expected behavior)

**Description:** When a BUS binding in `@new` has both wrong BUS id AND wrong role (e.g., `BUS BETA_BUS TARGET` when child expects `BUS ALPHA_BUS SOURCE`), the compiler emits `INSTANCE_BUS_MISMATCH` twice on the same line — once for the id mismatch and once for the role mismatch. This is reasonable behavior since both are independent violations.

## test_4_14-feature_guards.md

### Bug: FEATURE_VALIDATION_BOTH_PATHS is not implemented

**Severity:** Gap (unimplemented rule)

**Description:** Rule `FEATURE_VALIDATION_BOTH_PATHS` is defined in `rules.c` but no semantic pass emits it. The compiler does not validate both the enabled and disabled configurations of @feature guards. For example, an OUT port driven only inside `@feature X == 1` with no `@else` branch produces no diagnostic, even though the port is undriven when the feature is disabled. Similarly, a register declared inside `@feature` and referenced outside it produces no error.

**Reproduction:**
```
@module example
    CONST { INC = 1; }
    PORT { IN [1] clk; OUT [8] data; }
    REGISTER {
        r [8] = 8'h00;
    }
    ASYNCHRONOUS {
        @feature INC == 1
            data = r;
        @endfeat
        // data is undriven when INC != 1 — no error emitted
    }
    SYNCHRONOUS(CLK=clk) { r <= r + 8'd1; }
@endmod
```

### Bug: FEATURE_COND_WIDTH_NOT_1 not checked in declaration blocks

**Severity:** Bug

**Description:** The compiler only checks `FEATURE_COND_WIDTH_NOT_1` inside ASYNCHRONOUS and SYNCHRONOUS blocks. Wide-condition @feature guards in REGISTER, WIRE, and CONST declaration blocks are silently accepted without error.

**Reproduction:**
```
REGISTER {
    @feature 8'd255
        r [1] = 1'b0;
    @endfeat
}
```
Expected: `FEATURE_COND_WIDTH_NOT_1` error. Actual: no diagnostic.

### Bug: FEATURE_EXPR_INVALID_CONTEXT not checked in declaration blocks

**Severity:** Bug

**Description:** The compiler only checks `FEATURE_EXPR_INVALID_CONTEXT` inside ASYNCHRONOUS and SYNCHRONOUS blocks. Runtime signal references in @feature conditions within REGISTER, WIRE, and CONST declaration blocks are silently accepted.

**Reproduction:**
```
REGISTER {
    @feature din == 1'b1
        r [1] = 1'b0;
    @endfeat
}
```
Expected: `FEATURE_EXPR_INVALID_CONTEXT` error. Actual: no diagnostic.

### Bug: FEATURE_NESTED not checked in declaration blocks

**Severity:** Bug

**Description:** The compiler only checks `FEATURE_NESTED` inside ASYNCHRONOUS and SYNCHRONOUS blocks. Nested @feature guards in REGISTER, WIRE, and CONST declaration blocks are silently accepted.

**Reproduction:**
```
REGISTER {
    @feature FLAG == 1
        @feature MODE == 0
            r [1] = 1'b0;
        @endfeat
    @endfeat
}
```
Expected: `FEATURE_NESTED` error. Actual: no diagnostic.

### Limitation: FEATURE_NESTED stops processing after first occurrence

**Severity:** Limitation

**Description:** When the compiler detects a nested @feature, it stops processing subsequent blocks. Multiple FEATURE_NESTED triggers in different blocks of the same module (or across modules) cannot be tested in a single file — only the first is reported. Tests are split into separate files per context (ASYNC, SYNC, @else body).

## test_4_3-const.md

### Bug: CONST_CIRCULAR_DEP rule is unreachable for module CONSTs

**Severity:** Bug (unreachable rule)

**Description:** The rule `CONST_CIRCULAR_DEP` is defined in `rules.c` with message "S4.3/S7.10 Circular dependency in CONST/CONFIG definitions". The detection code exists in `const_eval.c` (line 624) and would emit the diagnostic when a CONST references another CONST that is currently being evaluated (EVAL_VISITING state). However, the higher-level semantic pass in `driver.c` (line 1372) intentionally suppresses all low-level `CONST00x` diagnostics by passing a zeroed `JZConstEvalOptions` struct (without `diagnostics` set) to `jz_const_eval_all()`. Any evaluation failure — including circular dependencies — is mapped to `CONST_NEGATIVE_OR_NONINT` instead.

**Minimal reproduction:**
```jz
@module Mod
    CONST { A = B; B = A; }
    PORT { IN [1] din; OUT [1] data; }
    ASYNCHRONOUS { data = din; }
@endmod
```
Expected: `CONST_CIRCULAR_DEP` error on A and B.
Actual: `CONST_NEGATIVE_OR_NONINT` error on A and B.

**Impact:** The `CONST_CIRCULAR_DEP` rule ID is never emitted for module-level CONST declarations. The circular dependency test (`4_3_CONST_CIRCULAR_DEP-circular_dependency.jz`) captures the actual behavior (`CONST_NEGATIVE_OR_NONINT`). CONFIG circular dependencies have a separate detection path in `driver_project.c` that may work correctly.

## test_4_5-wire.md

### Bug: WIRE_MULTI_DIMENSIONAL rule unreachable — parser emits PARSE000 instead

**Severity:** Bug (unreachable rule)

**Description:** The rule `WIRE_MULTI_DIMENSIONAL` is defined in `rules.c` with message "S4.5 WIRE declared with multi-dimensional syntax", but the parser cannot parse multi-dimensional wire syntax (`w [8] [4]`). The parser expects `;` after the first `[width]` and emits `PARSE000` when it encounters a second `[`. The semantic check for `WIRE_MULTI_DIMENSIONAL` is never reached.

**Minimal reproduction:**
```
@module Mod
    PORT { IN [1] din; OUT [1] data; }
    WIRE { w [8] [4]; }
    ASYNCHRONOUS { data <= din; }
@endmod
```
Expected: `WIRE_MULTI_DIMENSIONAL` error.
Actual: `PARSE000 parse error near token '[': expected ';' after WIRE declaration`.

**Impact:** The `WIRE_MULTI_DIMENSIONAL` rule ID is never emitted. Tests capture the actual `PARSE000` behavior. The parser would need to be extended to parse multi-dimensional syntax and then reject it at the semantic level to emit the intended rule.

**Test files:** `4_5_WIRE_MULTI_DIMENSIONAL-multi_dim_helper.jz` and `4_5_WIRE_MULTI_DIMENSIONAL-multi_dim_top.jz` — split into two files because the parser stops at the first error and cannot reach a second trigger.

### Observation: WRITE_WIRE_IN_SYNC co-fires SELECT_NO_MATCH_SYNC_OK for SELECT without DEFAULT

**Severity:** Observation (expected co-fire)

**Description:** When testing `WRITE_WIRE_IN_SYNC` with a wire assigned inside a SELECT/CASE in a SYNCHRONOUS block, the compiler also emits `SELECT_NO_MATCH_SYNC_OK` ("In SYNCHRONOUS, missing DEFAULT simply holds registers"). This is expected — the SELECT has no DEFAULT clause, and the compiler informs about register hold behavior. The test `.out` includes both diagnostics.

## test_4_6-mux.md

### Bug: MUX_SELECTOR_OUT_OF_RANGE_CONST does not detect sized literals

**Severity:** Bug (incomplete detection)

**Description:** The `MUX_SELECTOR_OUT_OF_RANGE_CONST` check uses `parse_simple_nonnegative_int()` to extract the constant value from the selector literal node's text. This function only handles plain decimal digits (e.g., `"2"`). Sized literals like `2'd2` store the full lexeme (e.g., `"2'd2"`) as the node's text, and `parse_simple_nonnegative_int()` fails to parse them — silently returning without emitting any diagnostic.

**Minimal reproduction:**
```
@module Mod
    PORT { IN [1] clk; IN [8] a; IN [8] b; OUT [8] data; }
    MUX { m = a, b; }
    REGISTER { r [8] = 8'h00; }
    ASYNCHRONOUS { data = m[2'd2] | r; }
    SYNCHRONOUS(CLK=clk) { r <= a; }
@endmod
```
Expected: `MUX_SELECTOR_OUT_OF_RANGE_CONST` error (index 2 out of range for 2-element MUX).
Actual: No diagnostic emitted.

**Impact:** Tests use unsized integer literals (e.g., `m[2]`) to trigger the check. Out-of-range constant indices expressed as sized literals are not caught at compile time. The fix would be to add sized literal parsing to `sem_check_mux_selector_expr()` in `driver.c`.

## test_4_7-register.md

### Bug: REG_MULTI_DIMENSIONAL rule unreachable — parser emits PARSE000 instead

**Severity:** Bug (unreachable rule)

**Description:** The rule `REG_MULTI_DIMENSIONAL` is defined in `rules.c` with message "S4.7 REGISTER declared with multi-dimensional syntax", but the parser catches multi-dimensional register syntax (e.g., `r [8] [4];`) before semantic analysis runs. The parser emits `PARSE000` with message "parse error near token '[': expected '=' and initialization literal in REGISTER block" instead.

**Minimal reproduction:**
```
REGISTER {
    r_bad [8] [4];
}
```
Expected: `REG_MULTI_DIMENSIONAL` error.
Actual: `PARSE000 parse error near token '[': expected '=' and initialization literal in REGISTER block`

**Impact:** The `REG_MULTI_DIMENSIONAL` semantic rule is never emitted. The test captures the parser's actual PARSE000 behavior. The parser stops after the first error in the REGISTER block, preventing subsequent triggers from being reached. Only one trigger per file is testable.

### Bug: REG_MISSING_INIT_LITERAL rule unreachable — parser emits PARSE000 instead

**Severity:** Bug (unreachable rule)

**Description:** The rule `REG_MISSING_INIT_LITERAL` is defined in `rules.c` with message "S4.7 Register declared without mandatory reset/power-on literal", but the parser catches missing init literals (e.g., `r [8];`) before semantic analysis runs. The parser emits `PARSE000` with message "parse error near token ';': expected '=' and initialization literal in REGISTER block" instead.

**Minimal reproduction:**
```
REGISTER {
    r_noinit [8];
}
```
Expected: `REG_MISSING_INIT_LITERAL` error.
Actual: `PARSE000 parse error near token ';': expected '=' and initialization literal in REGISTER block`

**Impact:** The `REG_MISSING_INIT_LITERAL` semantic rule is never emitted. The test captures the parser's actual PARSE000 behavior. Only one trigger per file is testable since the parser stops after the first error.

### Note: WARN_UNDRIVEN_REGISTER requires `<=` (receive) not `=` (alias) in ASYNC

**Severity:** Test gap / compiler behavior note

**Description:** When a register is read in an ASYNCHRONOUS block using `=` (alias assignment, e.g., `data_out = r_undriven;`), the compiler does not count this as a "read" for purposes of the WARN_UNDRIVEN_REGISTER check. Using `<=` (receive assignment, e.g., `data_out <= r_undriven;`) correctly triggers the warning. Tests use `<=` to match expected compiler behavior.

## test_4_8-latches.md

### Bug: LATCH_WIDTH_INVALID rule never fires — shadowed by WIDTH_NONPOSITIVE_OR_NONINT

**Severity:** Bug (dead rule)

**Description:** The rule `LATCH_WIDTH_INVALID` is defined in `rules.c` and implemented in `driver_width.c` (line 887), but it never fires independently. The general `WIDTH_NONPOSITIVE_OR_NONINT` check (line 910) fires for the same conditions (zero or non-integer width). Both checks call `eval_simple_positive_decl_int` and fire on `rc == -1`, but only `WIDTH_NONPOSITIVE_OR_NONINT` appears in the output. The latch-specific rule appears to be shadowed or deduplicated.

**Minimal reproduction:**
```jz
@project test @top Mod { IN [1] a = _; OUT [1] b = _; } @endproj
@module Mod
    PORT { IN [1] a; OUT [1] b; }
    LATCH { lat_zero [0] D; }
    ASYNCHRONOUS { b = a; }
@endmod
```
Expected: `LATCH_WIDTH_INVALID` error.
Actual: `WIDTH_NONPOSITIVE_OR_NONINT` error (no LATCH_WIDTH_INVALID).

**Test file:** `4_8_LATCH_WIDTH_INVALID-invalid_latch_width.jz` — test captures `WIDTH_NONPOSITIVE_OR_NONINT` and `NET_DANGLING_UNUSED` as actual output.

### Bug: LATCH_SR_WIDTH_MISMATCH rule not implemented

**Severity:** Bug (missing rule implementation)

**Description:** The rule `LATCH_SR_WIDTH_MISMATCH` is defined in `rules.c` with message "S4.8 SR latch set/reset expression width does not match latch width", but no semantic pass emits this diagnostic. When an SR latch with width 8 receives width-4 set/reset expressions, the compiler fires the general `ASSIGN_WIDTH_NO_MODIFIER` instead of the latch-specific rule.

**Minimal reproduction:**
```jz
@project test @top Mod { IN [4] s = _; IN [4] r = _; OUT [8] d = _; } @endproj
@module Mod
    PORT { IN [4] s; IN [4] r; OUT [8] d; }
    LATCH { lat_sr [8] SR; }
    ASYNCHRONOUS { lat_sr <= s : r; d <= lat_sr; }
@endmod
```
Expected: `LATCH_SR_WIDTH_MISMATCH` error.
Actual: `ASSIGN_WIDTH_NO_MODIFIER` error.

**Test file:** `4_8_LATCH_SR_WIDTH_MISMATCH-sr_width_mismatch.jz` — test captures `ASSIGN_WIDTH_NO_MODIFIER` as actual output.

### Bug: LATCH_IN_CONST_CONTEXT rule not implemented

**Severity:** Bug (missing rule implementation)

**Description:** The rule `LATCH_IN_CONST_CONTEXT` is defined in `rules.c` with message "S4.8 LATCH identifier may not be used in compile-time constant contexts (@check/@feature conditions)", but no semantic pass emits this diagnostic. When a latch identifier is used in a `@check` condition, the compiler fires the general `CHECK_INVALID_EXPR_TYPE` instead of the latch-specific rule.

**Minimal reproduction:**
```jz
@project test @top Mod { IN [1] e = _; IN [1] d = _; OUT [1] o = _; } @endproj
@module Mod
    PORT { IN [1] e; IN [1] d; OUT [1] o; }
    LATCH { lat [1] D; }
    @check (lat == 1'b0, "latch check");
    ASYNCHRONOUS { lat <= e : d; o <= lat; }
@endmod
```
Expected: `LATCH_IN_CONST_CONTEXT` error.
Actual: `CHECK_INVALID_EXPR_TYPE` error.

**Test file:** `4_8_LATCH_IN_CONST_CONTEXT-latch_in_const.jz` — test captures `CHECK_INVALID_EXPR_TYPE` as actual output.

### Observation: LATCH_AS_CLOCK_OR_CDC only checks CLK parameter, not CDC sources

**Severity:** Test gap

**Description:** The rule `LATCH_AS_CLOCK_OR_CDC` is described as covering both clock and CDC contexts, but the compiler only checks for latch identifiers in the `CLK=` parameter of `SYNCHRONOUS` blocks (in `driver_clocks.c`). There is no check for latch identifiers used as CDC source signals in `CDC { ... }` declarations. The CDC source context is untested because the compiler does not detect it.

**Impact:** A latch used as a CDC source (e.g., `BIT latch_name (clk_a) => sync_out (clk_b);`) will not trigger any diagnostic.

## test_4_9-mem_block.md

No issues found. All tests pass cleanly. The `MEM_TYPE_INVALID` rule fires correctly for invalid type strings (SRAM, RAM, REGISTER) and does not fire for valid types (BLOCK, DISTRIBUTED). No parser recovery bugs, no cascading errors.

## test_5_0-assignment_operators_summary.md

### Observation: WIDTH_ASSIGN_MISMATCH_NO_EXT always suppressed by ASSIGN_WIDTH_NO_MODIFIER

**Severity:** Test gap (priority suppression)

**Description:** The rule `WIDTH_ASSIGN_MISMATCH_NO_EXT` (priority 0, category WIDTHS_AND_SLICING) is emitted by `driver_assign.c` at the same location as `ASSIGN_WIDTH_NO_MODIFIER` (priority 1, category ASSIGNMENTS_AND_EXCLUSIVE) whenever a bare assignment operator is used with mismatched widths. Since `ASSIGN_WIDTH_NO_MODIFIER` has higher priority, it suppresses `WIDTH_ASSIGN_MISMATCH_NO_EXT` in the diagnostic output. There is no code path where `WIDTH_ASSIGN_MISMATCH_NO_EXT` fires without `ASSIGN_WIDTH_NO_MODIFIER` also firing.

**Impact:** `WIDTH_ASSIGN_MISMATCH_NO_EXT` cannot be observed in compiler output and therefore cannot be directly tested in a `.out` file. The test `5_0_WIDTH_ASSIGN_MISMATCH_NO_EXT-alias_width_mismatch.jz` exercises the same code path and captures `ASSIGN_WIDTH_NO_MODIFIER` output as proxy coverage.

**Minimal reproduction:**
```jz
@module Mod
    PORT { IN [8] din; OUT [16] dout; }
    ASYNCHRONOUS { dout = din; }
@endmod
```
Expected: `WIDTH_ASSIGN_MISMATCH_NO_EXT` visible in output.
Actual: Only `ASSIGN_WIDTH_NO_MODIFIER` appears (suppresses the lower-priority rule).

## test_5_1-asynchronous_assignments.md

### Bug: CONST on LHS in ASYNCHRONOUS block not detected

**Severity:** Bug (missing detection)

**Description:** The test plan specifies that `ASYNC_INVALID_STATEMENT_TARGET` should fire when a CONST identifier appears on the LHS of an ASYNCHRONOUS assignment (e.g., `MY_CONST = r;`). However, the compiler silently accepts this. In `driver_assign.c`, the `JZ_AST_EXPR_IDENTIFIER` case checks for MUX, IN port, and REGISTER on the LHS but does not check for `JZ_SYM_CONST`. A CONST on the LHS falls through the identifier case and breaks without emitting any diagnostic.

**Impact:** CONST values can be silently "assigned" in ASYNCHRONOUS blocks, which should be a compile-time error. The `ASYNC_INVALID_STATEMENT_TARGET` test covers the MEM SYNC port context but cannot cover the CONST context until this bug is fixed.

**Minimal reproduction:**
```jz
@project TEST
    @top Mod { IN [1] clk = _; OUT [8] dout = _; }
@endproj
@module Mod
    PORT { IN [1] clk; OUT [8] dout; }
    CONST { MY_CONST = 42; }
    REGISTER { r [8] = 8'h00; }
    ASYNCHRONOUS { dout = r; MY_CONST = r; }
    SYNCHRONOUS(CLK=clk) { r <= ~r; }
@endmod
```
Expected: `ASYNC_INVALID_STATEMENT_TARGET` error on `MY_CONST = r;`.
Actual: No diagnostic emitted.

### Note: WARN_INTERNAL_TRISTATE co-fires with ASYNC_FLOATING_Z_READ

**Severity:** Expected behavior (not a bug)

**Description:** When testing `ASYNC_FLOATING_Z_READ`, the `WARN_INTERNAL_TRISTATE` warning co-fires because assigning `z` to a non-INOUT wire triggers the internal tri-state warning. The test `.out` file includes both diagnostics. This is correct compiler behavior — any test involving `z` on non-INOUT nets will inherently co-fire this warning.

## test_5_2-synchronous_assignments.md

### Bug: SYNC_SLICE_WIDTH_MISMATCH rule is unreachable due to priority suppression

**Severity:** Bug (unreachable rule)

**Description:** The rule `SYNC_SLICE_WIDTH_MISMATCH` (priority 0) is defined in `rules.c` and is emitted by `driver_assign.c` line 1458 when a register slice assignment in a SYNCHRONOUS block has a width mismatch. However, `ASSIGN_SLICE_WIDTH_MISMATCH` (priority 2) fires on the same statement at the same location. The diagnostic printer groups diagnostics by line and only prints those with the maximum priority, so `SYNC_SLICE_WIDTH_MISMATCH` is always suppressed.

**Minimal reproduction:**
```jz
@project Test
    @top Mod { IN [1] clk = _; OUT [8] o = _; }
@endproj
@module Mod
    PORT { IN [1] clk; OUT [8] o; }
    REGISTER { r [8] = 8'h00; }
    ASYNCHRONOUS { o = r; }
    SYNCHRONOUS(CLK=clk) {
        r[7:4] <= 3'b101;
    }
@endmod
```
Expected: Both `ASSIGN_SLICE_WIDTH_MISMATCH` and `SYNC_SLICE_WIDTH_MISMATCH` in output.
Actual: Only `ASSIGN_SLICE_WIDTH_MISMATCH` appears; `SYNC_SLICE_WIDTH_MISMATCH` is suppressed by priority filtering.

**Impact:** The test `5_2_SYNC_SLICE_WIDTH_MISMATCH-slice_width_mismatch.jz` tests the slice width mismatch scenario but can only capture `ASSIGN_SLICE_WIDTH_MISMATCH` in its `.out` file.

### Note: WRITE_WIRE_IN_SYNC suppresses ASSIGN_TO_NON_REGISTER_IN_SYNC for wires

**Severity:** Expected behavior (not a bug)

**Description:** When a WIRE is assigned in a SYNCHRONOUS block, both `WRITE_WIRE_IN_SYNC` (priority 2) and `ASSIGN_TO_NON_REGISTER_IN_SYNC` (priority 1) are emitted. The diagnostic printer's priority filtering shows only `WRITE_WIRE_IN_SYNC`. This is correct behavior — the more specific wire diagnostic is preferred over the generic non-register diagnostic.

## test_5_3-conditional_statements.md

No issues found. All tests pass cleanly. No parser recovery bugs, no cascading errors, no missing rule implementations. The `IF_COND_MISSING_PARENS` rule requires separate files per trigger because the parser aborts after the first parse error — this is expected behavior for parse-level errors. The `CONTROL_FLOW_OUTSIDE_BLOCK` test emits errors on both the IF and ELSE keywords at module scope — this is correct compiler behavior.

## test_5_4-select_case_statements.md

### Issue 1: CONST identifiers rejected in CASE values (CONST_USED_WHERE_FORBIDDEN)

**Severity:** Compiler limitation / possible bug

**Description:** The specification (Section 5.4) states that "CASE values are integer constants or CONST names." However, using a CONST identifier as a CASE value triggers `CONST_USED_WHERE_FORBIDDEN` ("CONST identifier used outside compile-time constant expression contexts (runtime expression)"). The compiler treats CASE value positions as runtime expression contexts rather than compile-time constant contexts. This prevents testing CONST-in-CASE as a happy path scenario. The happy path test uses bare integer literals instead.

**Reproduction:**
```jz
@module Example
    CONST { MY_VAL = 1; }
    PORT { IN [4] sel; OUT [1] out; }
    ASYNCHRONOUS {
        SELECT (sel) {
            CASE MY_VAL { out <= VCC; }
            DEFAULT { out <= GND; }
        }
    }
@endmod
```
**Expected:** No error. **Actual:** `error CONST_USED_WHERE_FORBIDDEN`

### Issue 2: WARN_INCOMPLETE_SELECT_ASYNC never emitted

**Severity:** Expected behavior (not a bug)

**Description:** The rule `WARN_INCOMPLETE_SELECT_ASYNC` exists in `rules.c` but is never emitted by the compiler. When an ASYNC SELECT lacks DEFAULT, `SELECT_DEFAULT_RECOMMENDED_ASYNC` (warning) and `ASYNC_UNDEFINED_PATH_NO_DRIVER` (error) fire instead, fully covering the same scenario. The `WARN_INCOMPLETE_SELECT_ASYNC` rule appears to be redundant. This is already documented in `not_tested.md`.

## test_5_5-intrinsic_operators.md

### Bug: WIDTHOF_INVALID_TARGET rule not implemented

**Severity:** Bug (missing rule implementation)

**Description:** The rule `WIDTHOF_INVALID_TARGET` is defined in `rules.c` with message "S5.5.10 widthof() argument does not resolve to a WIRE, REGISTER, PORT, or BUS", but the compiler never emits this diagnostic. Using `widthof()` on a CONST name produces no error.

**Minimal reproduction:**
```jz
@module M
    CONST {
        MY_CONST = 42;
        BAD_W = widthof(MY_CONST);
    }
    PORT { IN [1] clk; OUT [8] data; }
    REGISTER { r [8] = 8'd0; }
    ASYNCHRONOUS { data = r; }
    SYNCHRONOUS(CLK=clk) { r <= 8'd0; }
@endmod
```
Expected: `WIDTHOF_INVALID_TARGET` error on `widthof(MY_CONST)`.
Actual: No diagnostic emitted.

**Test file:** `5_5_WIDTHOF_INVALID_TARGET-non_signal_target.jz` — test has 2 triggers but `.out` file is empty because the compiler does not detect them.

### Bug: WIDTHOF_INVALID_SYNTAX rule not implemented

**Severity:** Bug (missing rule implementation)

**Description:** The rule `WIDTHOF_INVALID_SYNTAX` is defined in `rules.c` with message "S5.5.10 widthof() argument must be a plain identifier, not a slice/concat/qualified expression", but the compiler never emits this diagnostic. Using `widthof()` with a slice argument produces no error.

**Minimal reproduction:**
```jz
@module M
    CONST {
        BAD_W = widthof(val[3:0]);
        GOOD_W = widthof(val);
    }
    PORT { IN [1] clk; IN [8] val; OUT [8] data; }
    REGISTER { r [GOOD_W] = 8'd0; }
    ASYNCHRONOUS { data = r; }
    SYNCHRONOUS(CLK=clk) { r <= val; }
@endmod
```
Expected: `WIDTHOF_INVALID_SYNTAX` error on `widthof(val[3:0])`.
Actual: No diagnostic emitted.

**Test file:** `5_5_WIDTHOF_INVALID_SYNTAX-slice_argument.jz` — test has 2 triggers but `.out` file is empty because the compiler does not detect them.

### Bug: WIDTHOF_WIDTH_NOT_RESOLVABLE rule not implemented

**Severity:** Bug (missing rule implementation)

**Description:** The rule `WIDTHOF_WIDTH_NOT_RESOLVABLE` is defined in `rules.c` with message "S5.5.10 widthof() target found but its width cannot be resolved", but the compiler never emits this diagnostic. Using `widthof()` on a wire whose width depends on an undefined CONST produces `NET_DANGLING_UNUSED` for the wire but no `WIDTHOF_WIDTH_NOT_RESOLVABLE` for the widthof() call.

**Minimal reproduction:**
```jz
@module M
    CONST {
        BAD_W = widthof(w_bad);
    }
    PORT { IN [1] clk; OUT [8] data; }
    WIRE { w_bad [UNDEFINED_CONST]; }
    REGISTER { r [8] = 8'd0; }
    ASYNCHRONOUS { data = r; }
    SYNCHRONOUS(CLK=clk) { r <= 8'd0; }
@endmod
```
Expected: `WIDTHOF_WIDTH_NOT_RESOLVABLE` error.
Actual: Only `NET_DANGLING_UNUSED` warning on the wire.

**Test file:** `5_5_WIDTHOF_WIDTH_NOT_RESOLVABLE-unresolvable_width.jz` — test has 1 trigger but compiler only emits unrelated `NET_DANGLING_UNUSED`.

### Bug: FUNC_RESULT_TRUNCATED_SILENTLY rule not implemented

**Severity:** Bug (missing rule implementation)

**Description:** The rule `FUNC_RESULT_TRUNCATED_SILENTLY` is defined in `rules.c` with message "S5.5/S5.2 Function result truncated by assignment without explicit slice or width check", but the compiler never emits this diagnostic. When an intrinsic function result (e.g., `uadd(8,8)` → 9 bits) is assigned to a narrower target (8 bits), the compiler emits `ASSIGN_WIDTH_NO_MODIFIER` instead of `FUNC_RESULT_TRUNCATED_SILENTLY`.

**Minimal reproduction:**
```jz
@module M
    PORT { IN [1] clk; IN [8] a; IN [8] b; OUT [8] out; }
    REGISTER { r [8] = 8'd0; }
    ASYNCHRONOUS { out = r; }
    SYNCHRONOUS(CLK=clk) { r <= uadd(a, b); }
@endmod
```
Expected: `FUNC_RESULT_TRUNCATED_SILENTLY` error on `r <= uadd(a, b)`.
Actual: `ASSIGN_WIDTH_NO_MODIFIER` error (correct width mismatch detection, wrong rule ID).

**Test file:** `5_5_FUNC_RESULT_TRUNCATED_SILENTLY-intrinsic_truncation.jz` — test has 4 triggers, all fire `ASSIGN_WIDTH_NO_MODIFIER` instead of `FUNC_RESULT_TRUNCATED_SILENTLY`.

### Observation: LIT_INVALID_CONTEXT cascades LIT_WIDTH_INVALID on subsequent valid lit() calls

**Severity:** Compiler bug (cascading error)

**Description:** When `lit()` is used in a CONST block (correctly triggering `LIT_INVALID_CONTEXT`), subsequent valid `lit()` calls in ASYNCHRONOUS and SYNCHRONOUS blocks within the same module incorrectly fire `LIT_WIDTH_INVALID`. Testing `lit()` in ASYNC in isolation (without the CONST block error) produces no error, confirming this is a cascading issue.

**Minimal reproduction:**
```jz
@module M
    CONST { BAD = lit(8, 5); }
    PORT { IN [1] clk; OUT [8] out; }
    REGISTER { r [8] = 8'd0; }
    ASYNCHRONOUS { out <= lit(8, 42); }
    SYNCHRONOUS(CLK=clk) { r <= 8'd0; }
@endmod
```
Expected: Only `LIT_INVALID_CONTEXT` on line 2 (CONST block).
Actual: `LIT_INVALID_CONTEXT` on line 2 AND `LIT_WIDTH_INVALID` on line 5 (ASYNC block).

**Test file:** `5_5_LIT_INVALID_CONTEXT-non_constant_context.jz` — `.out` includes the cascading `LIT_WIDTH_INVALID` errors as they are real compiler output.

### Bug: INDEX_OUT_OF_RANGE rules for gbit/sbit/gslice/sslice never fire

**Severity:** Bug (constant evaluation limitation)

**Description:** The rules `GBIT_INDEX_OUT_OF_RANGE`, `SBIT_INDEX_OUT_OF_RANGE`, `GSLICE_INDEX_OUT_OF_RANGE`, and `SSLICE_INDEX_OUT_OF_RANGE` are implemented in `src/sem/driver_operators.c` but never fire. The implementation checks for `JZ_AST_EXPR_LITERAL` nodes and parses their text with `parse_simple_nonnegative_int()`. However, sized literals like `4'd8` have text that includes the size prefix (e.g., `"4'd8"`), which `parse_simple_nonnegative_int()` cannot parse. Since bare integer literals in runtime expressions trigger `LIT_BARE_INTEGER`, there is no valid way to pass a constant index that the compiler can evaluate for range checking.

**Root cause:** `src/sem/driver_operators.c` lines 847-858 (gbit), 875-886 (sbit), 922-932 (gslice), 969-978 (sslice) — all use `parse_simple_nonnegative_int()` on sized literal text.

**Minimal reproduction:**
```jz
@module M
    PORT { IN [1] clk; IN [8] src; OUT [1] out; }
    REGISTER { r [1] = 1'b0; }
    ASYNCHRONOUS { out = r; }
    SYNCHRONOUS(CLK=clk) { r <= gbit(src, 4'd8); }
@endmod
```
Expected: `GBIT_INDEX_OUT_OF_RANGE` error (index 8 >= source width 8).
Actual: No diagnostic emitted.

**Test files:** `5_5_GBIT_INDEX_OUT_OF_RANGE-const_index_exceeds_width.jz`, `5_5_SBIT_INDEX_OUT_OF_RANGE-const_index_exceeds_width.jz`, `5_5_GSLICE_INDEX_OUT_OF_RANGE-const_index_exceeds_width.jz`, `5_5_SSLICE_INDEX_OUT_OF_RANGE-const_index_exceeds_width.jz` — all have triggers but `.out` files are empty because the compiler does not detect them.

### Observation: abs() returns width+1, not width

**Severity:** Observation (spec vs implementation difference)

**Description:** The test plan states `abs(a)` for an 8-bit signed input should return 8 bits. However, the compiler treats `abs()` as returning `width+1` bits (9 bits for an 8-bit input). Assigning `abs(8-bit)` to an 8-bit target fires `ASSIGN_WIDTH_NO_MODIFIER`. The happy-path test uses a 9-bit target to work with the compiler's actual behavior. This may be intentional (to handle the case where `abs(-128)` = 128, which doesn't fit in 8 signed bits).

## test_6_2-project_canonical_form.md

No issues found. All 7 rules from the test plan have corresponding validation tests that pass cleanly. No parser recovery bugs, no cascading errors, no missing rule implementations.

### Observation: PROJECT_MISSING_ENDPROJ diagnostic points to @endmod, not @project

**Severity:** Observation

**Description:** When `@project` is missing its `@endproj` terminator, the compiler emits `PROJECT_MISSING_ENDPROJ` with the line/column pointing to the `@endmod` of the last module in the file (line 25 in the test), rather than pointing back to the `@project` directive itself (line 4). This is because the parser discovers the missing `@endproj` only when it reaches the end of file or sees unexpected tokens. The diagnostic is still correct — the error is detected and reported — but the location may be surprising to users.

### Observation: Happy-path produces no File: header

**Severity:** Observation

**Description:** When a valid project file compiles with zero diagnostics, the compiler produces completely empty output (no `File:` header line). The `.out` file must be empty (0 bytes) rather than containing just `File: <name>.jz\n`. This is consistent with how other happy-path tests work in the codebase.

## test_6_3-config_block.md

### Issue 1: CONFIG_MULTIPLE_BLOCKS detected at parser level as PARSE000

**Severity:** Test gap

**Description:** The rule `CONFIG_MULTIPLE_BLOCKS` exists in `rules.c` with the message "S6.3 More than one CONFIG block defined in project", but the parser catches this condition before semantic analysis runs and emits `PARSE000` with the message "multiple CONFIG blocks in a single project are not allowed" instead. The semantic rule ID `CONFIG_MULTIPLE_BLOCKS` is never emitted in practice.

**Reproduction:**
```jz
@project Test
    CONFIG { WIDTH = 8; }
    CONFIG { DEPTH = 4; }
    @top TopMod { IN [1] clk = _; OUT [1] data = _; }
@endproj
```
**Expected:** `error CONFIG_MULTIPLE_BLOCKS S6.3 More than one CONFIG block defined in project`
**Actual:** `error PARSE000 parse error near token 'CONFIG': multiple CONFIG blocks in a single project are not allowed (orig: PARSE000)`

### Issue 2: CONST_NUMERIC_IN_STRING_CONTEXT not emitted for CONFIG.name in @file()

**Severity:** Test gap

**Description:** Using a numeric `CONFIG.<name>` reference as the path argument in `@file()` produces `PARSE000` ("expected string path or CONST/CONFIG reference in @file(...) MEM initializer") instead of the semantic rule `CONST_NUMERIC_IN_STRING_CONTEXT`. The parser appears to reject `CONFIG.name` syntax inside `@file()` at parse time before semantic analysis can check the type. The `CONST_NUMERIC_IN_STRING_CONTEXT` rule works correctly for bare CONST names in `@file()` (tested in `4_3_CONST_NUMERIC_IN_STRING_CONTEXT-numeric_as_file_path.jz`), but not for `CONFIG.name` references.

**Reproduction:**
```jz
@project Test
    CONFIG { WIDTH = 8; DEPTH = 4; }
    @top TopMod { IN [1] clk = _; OUT [8] data = _; }
@endproj
@module TopMod
    PORT { IN [1] clk; OUT [8] data; }
    MEM { m [8] [CONFIG.DEPTH] = @file(CONFIG.WIDTH) { OUT rd ASYNC; }; }
    ASYNCHRONOUS { data = m.rd[2'b00]; }
    SYNCHRONOUS(CLK=clk) { }
@endmod
```
**Expected:** `error CONST_NUMERIC_IN_STRING_CONTEXT S4.3/S6.3 Numeric CONST/CONFIG value used where a string is expected (e.g. @file path)`
**Actual:** `error PARSE000 parse error near token 'CONFIG': expected string path or CONST/CONFIG reference in @file(...) MEM initializer (orig: PARSE000)`

### Issue 3: Intra-CONFIG references require bare names, not CONFIG.name syntax

**Severity:** Observation

**Description:** Within the CONFIG block itself, referencing an earlier entry must use the bare name (e.g., `TOTAL = XLEN + DEPTH;`) rather than the `CONFIG.name` syntax (e.g., `TOTAL = CONFIG.XLEN + CONFIG.DEPTH;`). Using `CONFIG.name` syntax inside the CONFIG block fires `CONFIG_INVALID_EXPR_TYPE` because the parser/semantic pass treats `CONFIG.XLEN` as an undefined bare reference at that point. This is consistent behavior but may be surprising given that `CONFIG.name` is the required syntax everywhere outside the CONFIG block. The specification says "Expressions may reference earlier numeric CONFIG names using CONFIG.<name>" which suggests CONFIG.name should work inside CONFIG, but the compiler only supports bare names.

## test_6_4-clocks_block.md

### Observation: CLOCK_GEN_INVALID_TYPE emitted as PARSE000 instead of its own rule ID

**Severity:** Observation (parser-level detection)

**Description:** The rule `CLOCK_GEN_INVALID_TYPE` is defined in `rules.c` with message "S6.4.1 CLOCK_GEN generator must be PLL, DLL, CLKDIV, OSC, or BUF (with optional numeric suffix)", but the parser catches invalid generator types at parse time and emits `PARSE000` instead. The PARSE000 message includes the correct text but uses the generic parse error rule ID.

**Minimal reproduction:**
```jz
@project(CHIP="gw2ar-18-qn88-c8-i7") TEST
    CLOCKS { ref_clk = { period=10 }; bad_out; }
    IN_PINS { ref_clk = { standard=LVCMOS33 }; }
    MAP { ref_clk = 1; }
    CLOCK_GEN {
        RING {
            IN REF_CLK ref_clk;
            OUT BASE bad_out;
            CONFIG { FREQ = 100; };
        };
    }
    @top TopMod { IN [1] clk = ref_clk; OUT [1] data = _; }
@endproj
```
**Expected:** `error CLOCK_GEN_INVALID_TYPE S6.4.1 CLOCK_GEN generator must be PLL, DLL, CLKDIV, OSC, or BUF (with optional numeric suffix)`
**Actual:** `error PARSE000 parse error near token 'RING': CLOCK_GEN generator must be PLL, DLL, CLKDIV, OSC, or BUF (with optional numeric suffix) (orig: PARSE000)`

**Impact:** The test file `6_4_CLOCK_GEN_INVALID_TYPE-bad_type.jz` captures PARSE000 in its `.out` file since the semantic rule ID is never emitted. The error IS correctly detected — only the rule ID differs.

### Observation: CLOCK_GEN_OUTPUT_HAS_PERIOD co-fires CLOCK_NAME_NOT_IN_PINS and CLOCK_SOURCE_AMBIGUOUS

**Severity:** Observation (expected co-fire)

**Description:** When a CLOCK_GEN output clock has a period declared in the CLOCKS block, the compiler fires three rules: (1) `CLOCK_NAME_NOT_IN_PINS` because having a period implies an external clock, but it's not in IN_PINS; (2) `CLOCK_SOURCE_AMBIGUOUS` because it has both a period (implying external) and a CLOCK_GEN output; (3) `CLOCK_GEN_OUTPUT_HAS_PERIOD` from the CLOCK_GEN validation pass. This is correct behavior — all three conditions are true simultaneously.

### Observation: CLOCK_GEN_MISSING_INPUT and CLOCK_GEN_REQUIRED_INPUT_MISSING always co-fire

**Severity:** Observation (expected co-fire)

**Description:** For generator types that require an input clock (e.g., PLL, CLKDIV), omitting all IN declarations fires both `CLOCK_GEN_MISSING_INPUT` ("must have an IN clock declaration") and `CLOCK_GEN_REQUIRED_INPUT_MISSING` ("required input is not provided"). Both rules describe different aspects of the same problem — one is structural (no IN at all), the other is constraint-based (a required input is absent).

### Observation: CLOCK_GEN_PARAM_OUT_OF_RANGE and CLOCK_GEN_PARAM_TYPE_MISMATCH co-fire CLOCK_GEN_DERIVED_OUT_OF_RANGE

**Severity:** Observation (expected co-fire)

**Description:** When a PLL CONFIG parameter is out of range or has a type mismatch, the derived FVCO calculation may also produce an out-of-range result. The compiler fires both the parameter-level error and the derived-value error. Tests for PARAM_OUT_OF_RANGE and PARAM_TYPE_MISMATCH include the CLOCK_GEN_DERIVED_OUT_OF_RANGE co-fire in their `.out` files.

### Observation: CLOCK_GEN_INPUT_FREQ_OUT_OF_RANGE co-fires CLOCK_GEN_DERIVED_OUT_OF_RANGE

**Severity:** Observation (expected co-fire)

**Description:** When the input frequency exceeds the chip's supported range, the derived FVCO is also typically out of range. The compiler fires both `CLOCK_GEN_INPUT_FREQ_OUT_OF_RANGE` and `CLOCK_GEN_DERIVED_OUT_OF_RANGE`. The test includes both in the `.out` file.

## test_6_5-pin_blocks.md

All 16 tests (1 happy path + 15 error rules) pass cleanly. No parser recovery bugs, no cascading errors, no missing rule implementations. All 15 PIN_* rules from `rules.c` are correctly detected by the compiler in all applicable contexts (IN_PINS, OUT_PINS, INOUT_PINS).

### Note: PIN_PULL_ON_OUTPUT does not fire for pull=NONE on OUT_PINS

**Severity:** Possible test gap / design decision

**Description:** The spec states "The pull setting is not valid on OUT pins", which could imply any `pull=` attribute on OUT_PINS should be flagged. However, the compiler does not fire `PIN_PULL_ON_OUTPUT` when `pull=NONE` is specified on an OUT_PIN. This is arguably correct since `pull=NONE` is semantically equivalent to no pull attribute. The test includes `pull=NONE` on OUT_PINS as a negative test (no diagnostic expected).

### Note: SERIALIZER rules not testable via lint

**Severity:** Test gap (not a compiler bug)

**Description:** `INFO_SERIALIZER_CASCADE` and `SERIALIZER_WIDTH_EXCEEDS_RATIO` are emitted from the backend (`compiler/src/backend/verilog-2005/emit_wrapper.c`), not from the lint/semantic pass. The `--lint` validation framework cannot trigger them. They would need golden/backend tests using `--verilog` mode instead.

## test_6_6-map_block.md

### Bug: MAP_INVALID_BOARD_PIN_ID rule not implemented

**Severity:** Bug (missing implementation)

**Description:** The rule `MAP_INVALID_BOARD_PIN_ID` is defined in `rules.c` but is never referenced in any semantic pass source file. The compiler does not validate board pin ID formats against target device constraints. This means any integer board pin ID is accepted regardless of whether it's valid for the target FPGA/ASIC.

**Reproduction:** Cannot be reproduced because the check is not implemented — no input will trigger the diagnostic.

### Gap: MAP_DUP_PHYSICAL_LOCATION does not check differential P/N values

**Severity:** Test gap (missing compiler coverage)

**Description:** The `MAP_DUP_PHYSICAL_LOCATION` rule does not detect when a differential pin's P or N value duplicates another pin's physical location. For example, if `in_b = 25;` and `diff_in = { P=25, N=50 };`, the P=25 duplication is not flagged. The check only applies to scalar-scalar, scalar-bus, and bus-bus MAP entries.

**Reproduction:** Add `diff_in = { P=25, N=50 };` to a MAP block where pin 25 is already mapped to another pin. No `MAP_DUP_PHYSICAL_LOCATION` warning is emitted for the differential entry.

### Note: MAP_PIN_MAPPED_NOT_DECLARED co-fires MAP_SINGLE_UNEXPECTED_PAIR for undeclared differential entries

**Severity:** Expected behavior (not a bug)

**Description:** When a MAP entry references an undeclared pin using `{ P=<id>, N=<id> }` syntax, the compiler fires both `MAP_PIN_MAPPED_NOT_DECLARED` (pin not declared) and `MAP_SINGLE_UNEXPECTED_PAIR` (since the undeclared pin defaults to non-differential, the pair syntax is unexpected). The test `.out` file includes both diagnostics.

No other issues found. All 8 tests (1 happy path + 7 error rules) pass cleanly. No parser recovery bugs, no cascading errors. The MAP_DIFF_SAME_PIN test correctly co-fires MAP_DUP_PHYSICAL_LOCATION as expected behavior (same physical pin for P and N is also a duplicate physical location).

## test_6_7-blackbox_modules.md

### Bug: BLACKBOX_UNDEFINED_IN_NEW rule not implemented

**Severity:** Bug (missing rule implementation)

**Description:** The rule `BLACKBOX_UNDEFINED_IN_NEW` is defined in `rules.c` with message "S6.7/S6.9 @new references undefined blackbox name", but the compiler never emits this diagnostic. When `@new` references a non-existent module/blackbox name, the compiler fires `INSTANCE_UNDEFINED_MODULE` instead. No semantic pass emits `BLACKBOX_UNDEFINED_IN_NEW`.

**Reproduction:**
```
@project BB_TEST
    @top TopMod { IN [1] clk = _; OUT [1] data = _; }
@endproj
@module TopMod
    PORT { IN [1] clk; OUT [1] data; }
    @new inst bb_nonexistent { IN [1] din = clk; OUT [1] dout = data; };
    ASYNCHRONOUS { data = clk; }
@endmod
```
Expected: `BLACKBOX_UNDEFINED_IN_NEW`
Actual: `INSTANCE_UNDEFINED_MODULE`

**Impact:** The test `6_7_BLACKBOX_UNDEFINED_IN_NEW-undefined_blackbox.jz` captures actual compiler behavior (`INSTANCE_UNDEFINED_MODULE`) instead of the expected `BLACKBOX_UNDEFINED_IN_NEW`.

### Note: Parser rejects forbidden blocks before BLACKBOX_BODY_DISALLOWED semantic check

**Severity:** Design observation (not a bug)

**Description:** The parser rejects ASYNCHRONOUS, SYNCHRONOUS, WIRE, REGISTER, and MEM inside `@blackbox` with `PARSE000` ("unexpected token in @blackbox body; expected CONST or PORT") before semantic analysis runs. Only CONST passes through the parser to trigger `BLACKBOX_BODY_DISALLOWED`. The parser message "expected CONST or PORT" suggests CONST is a valid block in blackbox, but semantics then rejects it. This is slightly inconsistent but works correctly in practice.

### Note: BLACKBOX_NAME_DUP_IN_PROJECT co-fires WARN_UNUSED_MODULE

**Severity:** Expected behavior (not a bug)

**Description:** When a `@blackbox` name conflicts with a `@module` name, the module also fires `WARN_UNUSED_MODULE` because the blackbox name shadows it and the module cannot be instantiated. This is the same behavior documented under test_4_2-scope_and_uniqueness.md.

## test_6_8-bus_aggregation.md

### Bug: BUS_DEF_INVALID_DIR is unreachable (dead code)

**Severity:** Bug (dead code)

**Description:** The rule `BUS_DEF_INVALID_DIR` is defined in `rules.c` with message "S6.8 BUS signal direction must be IN, OUT, or INOUT", and the semantic check exists in `driver_project.c` (line ~1196). However, the parser (`parser_project.c:parse_bus_definition`) only accepts `JZ_TOK_KW_IN`, `JZ_TOK_KW_OUT`, or `JZ_TOK_KW_INOUT` tokens inside BUS blocks. Any other token triggers a parser error "expected IN/OUT/INOUT in BUS block" and aborts the BUS block parse. The semantic check therefore never encounters a `JZ_AST_BUS_DECL` node with an invalid `block_kind`, making this rule untestable.

**Minimal reproduction:**
```jz
@project test
    BUS BAD_BUS {
        WIRE [8] data;
    }
    @top TopMod { IN [1] clk = _; OUT [1] d = _; }
@endproj
@module TopMod
    PORT { IN [1] clk; OUT [1] d; }
    ASYNCHRONOUS { d = clk; }
@endmod
```
Expected: `BUS_DEF_INVALID_DIR` on `WIRE` keyword inside BUS.
Actual: `PARSE000` "expected IN/OUT/INOUT in BUS block" at the `WIRE` token. The semantic rule never fires.

### Note: BUS ports generate false-positive WARN_UNCONNECTED_OUTPUT warnings

**Severity:** Bug (false positive warning)

**Description:** Modules with BUS ports that use individual signal access (`sbus.tx = r; data = sbus.rx;`) still generate `WARN_UNCONNECTED_OUTPUT` warnings on the BUS port declaration itself. The compiler does not recognize that individual BUS signal assignments connect the BUS port. This affects all 6_8 validation tests — the `.out` files include these spurious warnings.

**Minimal reproduction:**
```jz
@project test
    BUS MY_BUS { OUT [8] tx; IN [8] rx; }
    @top TopMod { IN [1] clk = _; OUT [8] dout = _; }
@endproj
@module SourceMod
    PORT { IN [1] clk; BUS MY_BUS SOURCE sbus; OUT [8] data; }
    REGISTER { r [8] = 8'h00; }
    ASYNCHRONOUS { sbus.tx = r; data = sbus.rx; }
    SYNCHRONOUS(CLK=clk) { r <= sbus.rx; }
@endmod
@module TopMod
    PORT { IN [1] clk; OUT [8] dout; }
    @new inst_s SourceMod { IN [1] clk = clk; BUS MY_BUS SOURCE sbus = _; OUT [8] data = dout; };
    ASYNCHRONOUS { }
@endmod
```
Expected: No warnings (BUS signals are properly connected via individual access).
Actual: `WARN_UNCONNECTED_OUTPUT` on the `sbus` BUS port declaration in SourceMod.

## test_6_9-top_level_module.md

### Bug: INSTANCE_UNDEFINED_MODULE rule not emitted for @top — uses hardcoded TOP_MODULE_NOT_FOUND instead

**Severity:** Bug (rule ID mismatch)

**Description:** The test plan specifies that `INSTANCE_UNDEFINED_MODULE` should fire when `@top` references a non-existent module. However, the compiler emits a hardcoded `TOP_MODULE_NOT_FOUND` rule ID (not defined in `rules.c`) with the message "top-level @top module not defined in project". The code in `src/sem/driver_project_hw.c:1675` uses `sem_report_rule()` with a string literal `"TOP_MODULE_NOT_FOUND"` instead of looking up the `INSTANCE_UNDEFINED_MODULE` rule from the rules table.

**Minimal reproduction:**
```jz
@project Test
    @top NonExistent {
        IN  [1] clk = _;
        OUT [1] data = _;
    }
@endproj
```
Expected: `error INSTANCE_UNDEFINED_MODULE S4.13/S6.9 Instantiation references non-existent module`
Actual: `error TOP_MODULE_NOT_FOUND top-level @top module not defined in project (orig: TOP_MODULE_NOT_FOUND)`

**Impact:** The test captures the actual compiler behavior. The `.out` file uses `TOP_MODULE_NOT_FOUND` as emitted. Additionally, the compiler co-fires `WARN_UNUSED_MODULE` for the defined-but-unreferenced module.

### Note: WARN_UNUSED_MODULE co-fires in INSTANCE_UNDEFINED_MODULE test

**Severity:** Expected behavior (not a bug)

**Description:** When `@top` references a non-existent module, any modules defined in the file are unused (since the @top resolution fails). The compiler correctly emits `WARN_UNUSED_MODULE` for the defined `ActualModule`. The test `.out` file includes both diagnostics.

## test_7_0-memory_port_modes.md

No issues found. All tests pass cleanly. No parser recovery bugs, no cascading errors, no missing rule implementations. Both MEM_INVALID_PORT_TYPE and MEM_INVALID_WRITE_MODE are correctly detected by the compiler in all tested contexts.

## test_7_1-mem_declaration.md

### Bug: MEM_UNDEFINED_NAME never fires — UNDECLARED_IDENTIFIER catches it first

**Severity:** Bug (unreachable rule)

**Description:** The rule `MEM_UNDEFINED_NAME` is defined in `rules.c` with message "S7.7.1 Access to MEM name not declared in module" and has implementation in `driver_mem.c` (line ~415). However, the general-purpose `UNDECLARED_IDENTIFIER` check fires first during identifier resolution, before the MEM-specific semantic pass runs. As a result, `MEM_UNDEFINED_NAME` is never emitted for undeclared MEM names.

**Minimal reproduction:**
```jz
@project test @top Mod { IN [1] clk = _; OUT [8] data = _; } @endproj
@module Mod
    PORT { IN [1] clk; OUT [8] data; }
    REGISTER { r [8] = 8'h00; }
    ASYNCHRONOUS { data = undef_mem.rd[1'b0]; }
    SYNCHRONOUS(CLK=clk) { r <= r; }
@endmod
```
Expected: `MEM_UNDEFINED_NAME` error on `undef_mem.rd[1'b0]`.
Actual: `UNDECLARED_IDENTIFIER` error is emitted instead.

**Impact:** The test `7_1_MEM_UNDEFINED_NAME-undefined_mem_name.jz` captures `UNDECLARED_IDENTIFIER` as the actual compiler output. The test validates the compiler catches the error (just with a different rule ID than intended).

### Note: MEM_INVALID_PORT_TYPE only catches ASYNC/SYNC on IN ports at parser level

**Severity:** Expected behavior (not a bug)

**Description:** The parser only recovers from `IN <name> ASYNC;` and `IN <name> SYNC;` (emitting `MEM_INVALID_PORT_TYPE`). Completely unknown qualifiers like `OUT rd BOGUS;` cause a `PARSE000` error because the parser expects `;` after the port name. The semantic-level `MEM_INVALID_PORT_TYPE` check for invalid OUT qualifiers (line ~929 in `driver_mem.c`) cannot be reached because the parser doesn't allow unknown tokens in that position. The test uses `IN wr ASYNC;` and `IN wr SYNC;` which are the testable cases.

## test_7_10-const_evaluation_in_mem.md

No issues found. All tests pass cleanly. No parser recovery bugs, no cascading errors, no missing rule implementations. The `MEM_UNDEFINED_CONST_IN_WIDTH` test correctly co-fires `MEM_WARN_PORT_NEVER_ACCESSED` for MEM ports on memories with undefined CONST dimensions — this is expected behavior since the compiler cannot resolve the memory dimensions and the ports are effectively inaccessible.

## test_7_11-synthesis_implications.md

No issues found. All tests pass cleanly. No parser recovery bugs, no cascading errors, no missing rule implementations. All 3 rule IDs from the test plan are tested. Resource-exceeded rules (`MEM_BLOCK_RESOURCE_EXCEEDED`, `MEM_DISTRIBUTED_RESOURCE_EXCEEDED`) are project-level checks that fire once per project at the `@top` declaration — they have a single context (project resource totaling). `MEM_BLOCK_MULTI` fires per-MEM declaration and is tested in both helper and top modules.


## test_7_2-port_types_and_semantics.md

No issues found. All tests pass cleanly. No parser recovery bugs, no cascading errors, no missing rule implementations. Expected co-firing behavior:
- `MEM_INOUT_ASYNC` tests co-fire `MEM_WARN_PORT_NEVER_ACCESSED` because INOUT ports with invalid ASYNC/SYNC keywords cannot be used, leaving them unaccessed.
- `MEM_INOUT_MIXED_WITH_IN_OUT` tests co-fire `MEM_WARN_PORT_NEVER_ACCESSED` because ports in invalid mixed MEM declarations cannot be used.
- `MEM_READ_FROM_WRITE_PORT` tests co-fire `MEM_WARN_PORT_NEVER_ACCESSED` for the OUT (read) port because the test reads from the wrong port (IN), leaving the OUT port unaccessed.

## test_7_3-memory_access_syntax.md

### MEM_SYNC_ADDR_IN_ASYNC_BLOCK not fired by compiler

**Severity:** Bug — rule exists in rules.c but is never emitted

**Description:** When assigning a SYNC read port `.addr` field in an ASYNCHRONOUS block (e.g., `mem.rd.addr = addr;`), the compiler emits `ASYNC_INVALID_STATEMENT_TARGET` instead of the MEM-specific `MEM_SYNC_ADDR_IN_ASYNC_BLOCK`. The generic ASYNC assignment check fires before the MEM-specific check can run.

**Minimal reproduction:**
```
ASYNCHRONOUS {
    mem.rd.addr = addr;  // Expected: MEM_SYNC_ADDR_IN_ASYNC_BLOCK
                         // Actual: ASYNC_INVALID_STATEMENT_TARGET
}
```

**Impact:** The test `7_3_MEM_SYNC_ADDR_IN_ASYNC_BLOCK-sync_addr_in_async.jz` captures `ASYNC_INVALID_STATEMENT_TARGET` as the actual compiler output. The error is still caught, just with a different rule ID.

### MEM_SYNC_ADDR_WITHOUT_RECEIVE not fired by compiler

**Severity:** Bug — rule exists in rules.c but is never emitted

**Description:** When assigning a SYNC read port `.addr` with `=` instead of `<=` in a SYNCHRONOUS block, the compiler emits `SYNC_NO_ALIAS` instead of `MEM_SYNC_ADDR_WITHOUT_RECEIVE`. The generic `=` prohibition in SYNC blocks fires before the MEM-specific check.

**Minimal reproduction:**
```
SYNCHRONOUS(CLK=clk) {
    mem.rd.addr = addr;  // Expected: MEM_SYNC_ADDR_WITHOUT_RECEIVE
                         // Actual: SYNC_NO_ALIAS
}
```

### MEM_READ_SYNC_WITH_EQUALS not fired by compiler

**Severity:** Bug — rule exists in rules.c but is never emitted

**Description:** When reading SYNC `.data` with `=` instead of `<=` in a SYNCHRONOUS block, the compiler emits `SYNC_NO_ALIAS` instead of `MEM_READ_SYNC_WITH_EQUALS`. Same root cause as MEM_SYNC_ADDR_WITHOUT_RECEIVE.

**Minimal reproduction:**
```
SYNCHRONOUS(CLK=clk) {
    r = mem.rd.data;  // Expected: MEM_READ_SYNC_WITH_EQUALS
                      // Actual: SYNC_NO_ALIAS
}
```

### MEM_INOUT_WDATA_WRONG_OP not fired by compiler

**Severity:** Bug — rule exists in rules.c but is never emitted

**Description:** When assigning INOUT `.wdata` with `=` instead of `<=` in a SYNCHRONOUS block, the compiler emits `SYNC_NO_ALIAS` instead of `MEM_INOUT_WDATA_WRONG_OP`. Same root cause — generic `=` prohibition fires first.

**Minimal reproduction:**
```
SYNCHRONOUS(CLK=clk) {
    mem.rw.wdata = val;  // Expected: MEM_INOUT_WDATA_WRONG_OP
                         // Actual: SYNC_NO_ALIAS
}
```

### MEM_MULTIPLE_ADDR_ASSIGNS not implemented

**Severity:** Bug — rule exists in rules.c but is never emitted

**Description:** Multiple `.addr` assignments to the same INOUT port within a single SYNCHRONOUS block produce no diagnostic. The rule `MEM_MULTIPLE_ADDR_ASSIGNS` is defined in rules.c but the semantic check is not implemented.

**Minimal reproduction:**
```
SYNCHRONOUS(CLK=clk) {
    mem.rw.addr <= addr;   // first assignment
    mem.rw.addr <= 4'h0;   // second — expected error, none emitted
}
```

**Impact:** The test `7_3_MEM_MULTIPLE_ADDR_ASSIGNS-inout_multi_addr.jz` has an empty .out file. The test passes (compiler produces no output, matching the empty expectation) but the rule violation is not caught.

### MEM_MULTIPLE_WDATA_ASSIGNS not implemented

**Severity:** Bug — rule exists in rules.c but is never emitted

**Description:** Multiple `.wdata` assignments to the same INOUT port within a single SYNCHRONOUS block produce no diagnostic. Same root cause as MEM_MULTIPLE_ADDR_ASSIGNS.

**Minimal reproduction:**
```
SYNCHRONOUS(CLK=clk) {
    mem.rw.wdata <= data;  // first assignment
    mem.rw.wdata <= 8'hFF; // second — expected error, none emitted
}
```

### MEM_PORT_USED_AS_SIGNAL not detected for INOUT bare port references

**Severity:** Bug — incomplete rule implementation

**Description:** Using a bare INOUT port reference (e.g., `r <= mem.rw;`) without a field does not trigger `MEM_PORT_USED_AS_SIGNAL`. The rule fires correctly for ASYNC OUT and SYNC OUT bare references but not for INOUT.

### MEM_PORT_ADDR_READ not detected for INOUT .addr reads

**Severity:** Bug — incomplete rule implementation

**Description:** Reading `.addr` from an INOUT port (e.g., `r <= mem.rw.addr;`) does not trigger `MEM_PORT_ADDR_READ`. The rule fires correctly for SYNC OUT `.addr` reads but not for INOUT `.addr`.

### Note: MEM_PORT_USED_AS_SIGNAL and MEM_WARN_PORT_NEVER_ACCESSED co-firing

**Severity:** Expected behavior (not a bug)

**Description:** When testing bare port references, the ASYNC OUT port in HelperMod is declared but only accessed via the invalid bare ref `r <= mem.rd;`, which doesn't count as a valid access. This causes `MEM_WARN_PORT_NEVER_ACCESSED` to co-fire. This is a natural consequence of the construct under test.

## test_7_5-initialization.md

### Bug: MEM_INIT_FILE_CONTAINS_X rule not implemented

**Severity:** Bug (missing rule implementation)

**Description:** The rule `MEM_INIT_FILE_CONTAINS_X` is defined in `rules.c` with message "S7.5.2 Memory initialization file contains x or z values", but the compiler never emits this diagnostic. No semantic pass references this rule ID. When a `.mem` file containing `x` values is used as a MEM initialization source, the compiler either ignores the x values or treats the file as having fewer valid entries (emitting `MEM_WARN_PARTIAL_INIT` instead).

**Minimal reproduction:**
Create a file `mem_contains_x.mem` with contents:
```
00
xx
00
00
```
Then reference it:
```jz
MEM {
    m [8] [4] = @file("mem_contains_x.mem") {
        OUT rd ASYNC;
        IN  wr;
    };
}
```
Expected: `MEM_INIT_FILE_CONTAINS_X` error.
Actual: `MEM_WARN_PARTIAL_INIT` warning (compiler treats file as smaller than depth).

## test_7_6-complete_examples.md

No issues discovered. All 10 happy-path tests (9 canonical examples from S7.6.1–S7.6.9 plus 1 minimum-dimensions edge case) compile cleanly with no unexpected diagnostics.

## test_7_8-mem_vs_register_vs_wire.md

No issues discovered. Section 7.8 is a comparison/documentation table with no new rules. The single happy-path test compiles cleanly with no unexpected diagnostics.

## test_7_9-mem_in_module_instantiation.md

No issues discovered. All tests pass cleanly. The UNDECLARED_IDENTIFIER rule correctly fires in all four tested contexts: intermediate module write to child MEM, top-level async read of child MEM, top-level sync write to child MEM, and deep hierarchical access (two levels). No parser recovery bugs or cascading errors.

## test_8_2-global_syntax.md

No issues found. All tests pass cleanly. The GLOBAL_INVALID_EXPR_TYPE rule correctly fires for all five invalid value forms: bare integer, bare zero, CONFIG reference, cross-global reference, and expression. No parser recovery bugs or cascading errors.

## test_8_3-global_semantics.md

### Note: CONFIG_INVALID_EXPR_TYPE co-fires with GLOBAL_USED_WHERE_FORBIDDEN in CONFIG context

**Severity:** Expected behavior (not a bug)

**Description:** When a GLOBAL reference is used in a CONFIG expression, the compiler correctly emits `GLOBAL_USED_WHERE_FORBIDDEN` first, then also emits `CONFIG_INVALID_EXPR_TYPE` because the GLOBAL reference cannot be evaluated as a valid CONFIG value. Both diagnostics fire at the same location (line 13, column 9 in the test). This is correct cascading behavior — the forbidden reference makes the CONFIG value unresolvable, which is a separate valid error. The test `.out` file includes both diagnostics.


## test_8_5-global_errors.md

### Note: LIT_OVERFLOW fires as GLOBAL_INVALID_EXPR_TYPE in @global context

**Severity:** Test plan discrepancy (not a compiler bug)

**Description:** The test plan expects `LIT_OVERFLOW` to fire for overflowing literals in @global blocks (e.g., `4'hFF`). However, the compiler emits `GLOBAL_INVALID_EXPR_TYPE` instead. This is because the @global semantic pass checks that each value is a valid sized literal and rejects overflows as invalid expressions before the general `LIT_OVERFLOW` rule fires. This is reasonable behavior — the @global-specific rule catches the error first. The test `8_5_LIT_OVERFLOW-global_literal_overflow.jz` matches the actual compiler output (`GLOBAL_INVALID_EXPR_TYPE`).

## test_9_3-check_placement_rules.md

### Note: PARSE000 cascading error after DIRECTIVE_INVALID_CONTEXT for @check in blocks

**Severity:** Parser recovery issue (minor)

**Description:** When `@check` appears inside an ASYNCHRONOUS or SYNCHRONOUS block, the compiler correctly emits `DIRECTIVE_INVALID_CONTEXT` for the `@check` directive. However, the parser then fails to skip the `(expression, "message");` portion and emits a cascading `PARSE000` error on the `(` token. This cascading error is expected and documented in the `.out` files. Due to this cascading, @check-in-ASYNC and @check-in-SYNC tests are kept in separate files to avoid the parser failing to reach subsequent triggers.

**Reproduction:**
```
ASYNCHRONOUS {
    @check (1, "async check");  // line 57
    data = r;
}
```
Output:
```
57:9    error DIRECTIVE_INVALID_CONTEXT S1.1/S6.2 Structural directives used in invalid location
57:16   error PARSE000 parse error near token '('
```

### Note: CHECK_INVALID_PLACEMENT vs DIRECTIVE_INVALID_CONTEXT

**Severity:** Test gap / potential rule overlap

**Description:** The `CHECK_INVALID_PLACEMENT` rule in `rules.c` says "S9.3 @check may not appear inside conditional or @feature bodies", which targets @check inside IF/@feature bodies — a different context from @check inside ASYNC/SYNC blocks (which fires `DIRECTIVE_INVALID_CONTEXT`). The test plan lists `CHECK_INVALID_PLACEMENT` as "covered via DIRECTIVE_INVALID_CONTEXT tests", but these are actually different placement violations. `CHECK_INVALID_PLACEMENT` cannot be independently tested until @feature support enables writing @check inside a @feature body. Documented in `not_tested.md`.

## test_9_5-check_evaluation_order.md

### Limitation: CONST-referencing-CONST (transitive resolution) not supported

**Severity:** Test gap / compiler limitation

**Description:** The specification (S9.5) says @check evaluates after "resolution of all preceding CONST definitions", implying transitive resolution (e.g., `CONST { A = 4; } CONST { B = A * 2; }`) should work. However, the compiler emits `CONST_NEGATIVE_OR_NONINT` when a CONST expression references another CONST. This prevents testing the "transitive CONST resolution" edge case from the test plan. The happy-path test only covers direct CONST and CONFIG references.

**Reproduction:**
```jz
@module Mod
    PORT { IN [1] a; OUT [1] b; }
    CONST { A = 4; }
    CONST { B = A * 2; }  // CONST_NEGATIVE_OR_NONINT error
    @check (B == 8, "transitive ok");
    ASYNCHRONOUS { b = a; }
@endmod
```

### Limitation: OVERRIDE values not visible to @check at lint time

**Severity:** Test gap / compiler limitation

**Description:** The specification (S9.5) says @check sees "CONST values after any OVERRIDE values applied". However, at lint time (`--lint`), OVERRIDE values from `@new` instantiations do not propagate into the child module's @check evaluations. This prevents testing the "OVERRIDE applied to CONFIG" edge case. The @check in the child module evaluates with the default CONST value, not the overridden one.

**Reproduction:**
```jz
@module Child
    PORT { IN [1] a; OUT [1] b; }
    CONST { BASE = 16; }
    @check (BASE == 32, "should see override");  // CHECK_FAILED because BASE is still 16
    ASYNCHRONOUS { b = a; }
@endmod

@module Top
    PORT { IN [1] a; OUT [1] b; }
    @new inst Child { OVERRIDE { BASE = 32; } IN [1] a = a; OUT [1] b = b; };
    ASYNCHRONOUS { b = a; }
@endmod
```


## test_4_4-port.md

### Bug: PORT_MISSING_WIDTH rule not emitted (caught as PARSE000)

**Severity:** Test gap (parser preempts semantic check)

**Description:** The rule `PORT_MISSING_WIDTH` is defined in `rules.c` with message "S4.4/S8.1 Port declaration without mandatory `[N]` width", but the compiler never emits it. The parser catches the missing width bracket before semantic analysis and emits `PARSE000` ("expected '[' after port direction") instead. The test file `4_4_PORT_MISSING_WIDTH-missing_port_width.jz` captures the actual parser behavior.

**Minimal reproduction:**
```jz
@module Mod
    PORT {
        IN data;
    }
    ASYNCHRONOUS { }
@endmod
```
Expected: `PORT_MISSING_WIDTH`
Actual: `PARSE000 parse error near token 'data': expected '[' after port direction`

### Bug: BUS_PORT_UNKNOWN_BUS rule not implemented

**Severity:** Bug (missing rule implementation)

**Description:** The rule `BUS_PORT_UNKNOWN_BUS` is defined in `rules.c` with message "S4.4.1/S6.8 BUS port references BUS name not declared in project", but the compiler never emits this diagnostic. When a module declares a BUS port referencing an undeclared bus name (e.g., `BUS FAKE_BUS SOURCE fbus;`), the compiler accepts it silently and the port appears as a dangling unused signal (`NET_DANGLING_UNUSED`).

**Minimal reproduction:**
```jz
@project Test
    @top TopMod { OUT [1] x = _; }
@endproj
@module TopMod
    PORT { BUS NONEXISTENT SOURCE nb; OUT [1] x; }
    ASYNCHRONOUS { x <= 1'b0; }
@endmod
```
Expected: `BUS_PORT_UNKNOWN_BUS`
Actual: `NET_DANGLING_UNUSED` on the BUS port

### Bug: BUS_PORT_NOT_BUS rule not implemented

**Severity:** Bug (missing rule implementation)

**Description:** The rule `BUS_PORT_NOT_BUS` is defined in `rules.c` with message "S4.4.1 BUS member access used on non-BUS port", but the compiler never emits this diagnostic. When code uses dot-notation member access on a regular (non-BUS) port (e.g., `data_in.tx`), the compiler treats the entire `data_in.tx` expression as an undeclared identifier and emits `UNDECLARED_IDENTIFIER` instead of the more specific `BUS_PORT_NOT_BUS`.

**Minimal reproduction:**
```jz
@project Test
    @top TopMod { IN [8] din = _; OUT [8] dout = _; }
@endproj
@module TopMod
    PORT { IN [8] din; OUT [8] dout; }
    ASYNCHRONOUS { dout <= din.field; }
@endmod
```
Expected: `BUS_PORT_NOT_BUS`
Actual: `UNDECLARED_IDENTIFIER`

## test_5_1-asynchronous_assignments.md

No issues found. All 5 tests pass cleanly. No parser recovery bugs, no cascading errors, no missing rule implementations. The ASYNC_FLOATING_Z_READ test co-fires WARN_INTERNAL_TRISTATE for z-literal usage on internal (non-INOUT) nets — this is expected behavior, not a bug.

## test_9_7-check_error_conditions.md

No new issues. This test plan is a consolidation/cross-reference of @check error conditions from sections 9.1–9.5. All 3 testable rules (CHECK_FAILED, CHECK_INVALID_EXPR_TYPE, DIRECTIVE_INVALID_CONTEXT) are fully covered by existing tests from those sections. All 11 section 9 tests pass. The only untested rule, CHECK_INVALID_PLACEMENT, is already documented in `not_tested.md` and in the test_9_3 section of this file — the compiler never emits it (DIRECTIVE_INVALID_CONTEXT fires instead for @check inside blocks, and @check inside @feature is accepted as valid).

## test_misc-repeat_serializer_io.md

### Bug: repeat_expand.c uses raw codes instead of rules.c rule IDs

**Severity:** Minor bug (diagnostic formatting)

**Description:** `repeat_expand.c` emits diagnostics with raw codes `RPT-001` and `RPT-002` via `jz_diagnostic_report()`, rather than using the rule IDs `RPT_COUNT_INVALID` and `RPT_NO_MATCHING_END` defined in `rules.c`. Because `jz_rule_lookup()` searches by rule ID, the lookup fails and the diagnostic output includes a redundant `(orig: RPT-001)` suffix. The diagnostic messages also differ from the `rules.c` definitions:

- `rules.c` defines `RPT_COUNT_INVALID` with message: `"RPT-001 @repeat requires a positive integer count"`
- `repeat_expand.c` emits code `RPT-001` with two distinct messages:
  - `"@repeat requires a positive integer count"` (for non-digit arguments)
  - `"@repeat count must be a positive integer"` (for parsed zero count)

**Impact:** The output format includes `(orig: RPT-001)` which is cosmetically undesirable. The rule description from `rules.c` is not shown; instead the raw message is displayed. Tests capture the actual compiler behavior including the `(orig: ...)` suffix.

**Minimal reproduction:**
```
@repeat 0
@end
```
Output: `error RPT-001 @repeat count must be a positive integer (orig: RPT-001)`
Expected: `error RPT_COUNT_INVALID RPT-001 @repeat requires a positive integer count`

### Note: @repeat errors are fatal — only one trigger per file

**Severity:** Test gap (inherent to design)

**Description:** The `@repeat` expansion runs before lexing on the raw source text. When an error is encountered, `expand_region()` returns 1 immediately, causing `jz_repeat_expand()` to return NULL. This means only ONE repeat error can be detected per compilation. Multiple triggers in the same file are not possible — the first error stops all processing. Tests are split into separate files accordingly.

### Note: SERIALIZER and IO rules not testable via --lint

**Severity:** Test gap (by design)

**Description:** `INFO_SERIALIZER_CASCADE` and `SERIALIZER_WIDTH_EXCEEDS_RATIO` are emitted from `compiler/src/backend/verilog-2005/emit_wrapper.c` during Verilog backend emission, not during lint/semantic analysis. They require chip-specific differential serializer configuration and are not reachable via `--info --lint`. `IO_BACKEND` and `IO_IR` are runtime I/O errors that cannot be triggered via `--info --lint`.

## test_sim-simulation_rules.md

No issues found. Both lint-detectable simulation rules (`SIM_WRONG_TOOL`, `SIM_PROJECT_MIXED`) fire correctly with expected messages and locations. `SIM_RUN_COND_TIMEOUT` is a runtime-only rule that cannot be tested via `--lint` — documented in `not_tested.md`.
