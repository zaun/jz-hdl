# Test Issues

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

No issues found. All happy-path tests pass with clean output.

## test_10_3-template_allowed_content.md

### Bug: TEMPLATE_EXTERNAL_REF rule not implemented

**Severity:** Bug (missing rule implementation)

**Description:** The rule `TEMPLATE_EXTERNAL_REF` is defined in `rules.c` with message "S10.3 Identifier in template body must be a parameter, @scratch wire, or compile-time constant; pass external signals as arguments", but the compiler does not emit this diagnostic. Templates that reference external signals (wires, registers) not passed as parameters compile without error.

**Minimal reproduction:**
```jz
@module Mod
    PORT { IN [1] din; OUT [1] data; }
    WIRE { w [1]; }
    @template ext_ref(a)
        a <= w;
    @endtemplate
    ASYNCHRONOUS { w = din; data = w; }
@endmod
```
Expected: `TEMPLATE_EXTERNAL_REF` error on `w` inside the template.
Actual: No diagnostic emitted.

**Test file:** `10_3_TEMPLATE_EXTERNAL_REF-external_signal_reference.jz` — test has 6 triggers across multiple contexts but the `.out` file is empty because the compiler does not detect any of them.

### Bug: TEMPLATE_SCRATCH_WIDTH_INVALID rule not implemented

**Severity:** Bug (missing rule implementation)

**Description:** The rule `TEMPLATE_SCRATCH_WIDTH_INVALID` is defined in `rules.c` with message "S10.3 @scratch width must be a positive integer constant expression", but the compiler does not emit this diagnostic. `@scratch` declarations with zero width or non-constant (parameter-based) width expressions compile without error.

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
Actual: No diagnostic emitted.

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

No new issues. Section 11.5 is a cross-reference to `TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL` and `TRISTATE_TRANSFORM_PER_BIT_FAIL`, both already documented under section 11.4 above. No new test files were generated — existing tests under section 11.4 naming provide full coverage.

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

No issues found. Both error rules (`LOGICAL_WIDTH_NOT_1`, `TYPE_BINOP_WIDTH_MISMATCH`) are correctly detected by the compiler in all tested contexts. The happy-path test compiles cleanly with all operator categories. No parser recovery issues or cascading errors encountered.

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
