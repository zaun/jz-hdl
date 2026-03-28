# Compiler Test Issues

## test_sim-simulation_rules.md

No issues discovered. Both testable rules fire correctly:
- `SIM_WRONG_TOOL` fires at the `@simulation` keyword when the file is run with `--lint`.
- `SIM_PROJECT_MIXED` fires at the `@simulation` keyword when `@project` was already parsed in the same file.
- `SIM_RUN_COND_TIMEOUT` is runtime-only (fires during `--simulate`) and cannot be tested via `--info --lint`.

## test_2_3-bit_width_constraints.md

No issues discovered. All rules fire correctly:
- `TYPE_BINOP_WIDTH_MISMATCH` fires for all 14 binary operators with mismatched widths in ASYNC and SYNC blocks.
- `ASSIGN_WIDTH_NO_MODIFIER` fires for alias (=), receive (<=), and drive (=>) width mismatches in both directions.
- `ASSIGN_CONCAT_WIDTH_MISMATCH` fires for RHS-too-narrow, RHS-too-wide, and LHS concat mismatches.
- `TERNARY_BRANCH_WIDTH_MISMATCH` fires for ternary branch width mismatches with signals and literals.
- `WIDTH_NONPOSITIVE_OR_NONINT` fires for zero-width WIRE and REGISTER declarations (co-fires `WARN_UNUSED_WIRE` for unused zero-width wires — expected behavior).
- `WIDTH_ASSIGN_MISMATCH_NO_EXT` is always suppressed by higher-priority `ASSIGN_WIDTH_NO_MODIFIER` and cannot be independently triggered.

## test_10_3-template_allowed_content.md

### Issue 1: TEMPLATE_EXTERNAL_REF — LHS/reverse-directional contexts produce cascading errors

- **Severity:** test gap
- **Description:** Testing `TEMPLATE_EXTERNAL_REF` with external signals on the LHS of `<=` or the RHS of `=>` correctly triggers the rule, but the compiler also expands the template and produces unrelated cascading errors (`ASSIGN_MULTIPLE_SAME_BITS`, `PORT_DIRECTION_MISMATCH_OUT`, `COMB_LOOP_UNCONDITIONAL`). This makes it impossible to test these contexts without introducing unrelated diagnostics.
- **Reproduction:**
  ```jz
  @template ext_lhs(a)
      ext_w <= a;
  @endtemplate

  // When applied in a module where ext_w exists:
  // TEMPLATE_EXTERNAL_REF fires correctly, but template expansion
  // also generates ASSIGN_MULTIPLE_SAME_BITS if ext_w is already driven.
  ```
- **Impact:** LHS-of-`<=` and RHS-of-`=>` contexts for `TEMPLATE_EXTERNAL_REF` are not tested in the validation suite. The rule DOES fire in these contexts, but clean testing requires the compiler to skip template expansion when `TEMPLATE_EXTERNAL_REF` errors are present.

## test_10_8-template_error_cases.md

### Issue 1: ASSIGN_MULTIPLE_SAME_BITS / SYNC_MULTI_ASSIGN_SAME_REG_BITS — error reported at template definition, not callsite

- **Severity:** test observation (not a bug)
- **Description:** When multiple `@apply` callsites in different modules trigger `ASSIGN_MULTIPLE_SAME_BITS` or `SYNC_MULTI_ASSIGN_SAME_REG_BITS` via the same template, the compiler reports the error at the template body line (where the conflicting assignment is written), not at the `@apply` callsite. Since all callsites reference the same template body, the error location is identical and the compiler deduplicates to a single diagnostic.
- **Impact:** Tests have 2 trigger locations (HelperMod + TopMod) but only 1 diagnostic appears in the `.out` file. This is expected compiler behavior — the template body is the true source of the conflict pattern. Coverage is still achieved because the single diagnostic confirms the rule fires for template-expanded assignments.

### Issue 2: Test plan incorrectly lists TEMPLATE_SCRATCH_WIDTH_INVALID and TEMPLATE_APPLY_OUTSIDE_BLOCK as having compiler bugs

- **Severity:** test plan inaccuracy
- **Description:** The test plan's section 5.2 lists `TEMPLATE_SCRATCH_WIDTH_INVALID` and `TEMPLATE_APPLY_OUTSIDE_BLOCK` under "Rules Not Tested" with reason "Bug: test exists but rule has a known compiler bug." Both rules work correctly — `TEMPLATE_SCRATCH_WIDTH_INVALID` fires for zero-width and parameter-width @scratch declarations, and `TEMPLATE_APPLY_OUTSIDE_BLOCK` fires for @apply at file scope and module scope. Tests for both rules pass cleanly.
- **Impact:** None — both rules are fully tested in the 10.8 cross-reference suite.

## test_11_4-tristate_transformation_algorithm.md

### Issue 1: Multi-driver priority chain happy path cannot be tested

- **Severity:** test gap (compiler limitation)
- **Description:** The test plan specifies happy-path scenarios for two-driver and three-driver priority chains. However, the compiler rejects all multi-driver tri-state patterns with `TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL` even when enables are provably mutually exclusive (e.g., `sel` and `~sel` via a wire). The compiler cannot statically prove mutual exclusion for any instance-based multi-driver pattern. Within a single ASYNCHRONOUS block, multiple drivers of the same wire are blocked by `ASSIGN_INDEPENDENT_IF_SELECT` before the tristate transform runs.
- **Reproduction:**
  ```jz
  @module DriverMod
      PORT { IN [1] en; INOUT [8] bus; }
      ASYNCHRONOUS {
          IF (en) { bus <= 8'hAA; } ELSE { bus <= 8'bzzzz_zzzz; }
      }
  @endmod
  @module TopMod
      PORT { IN [1] sel; OUT [8] data; }
      WIRE { shared [8]; not_sel [1]; }
      @new inst_a DriverMod { IN [1] en = sel; INOUT [8] bus = shared; };
      @new inst_b DriverMod { IN [1] en = not_sel; INOUT [8] bus = shared; };
      ASYNCHRONOUS { not_sel <= ~sel; data <= shared; }
  @endmod
  ```
  Result: `TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL` even though `sel` and `not_sel` are mutually exclusive.
- **Impact:** Two-driver and three-driver priority chain happy paths (test plan scenarios 2.1.1, 2.1.2) and the max driver count edge case (2.3.1) cannot be tested. Only single-driver transformation (with `TRISTATE_TRANSFORM_SINGLE_DRIVER` warning) is testable as a happy path.

### Issue 2: Test plan incorrectly lists TRISTATE_TRANSFORM rules as having compiler bugs

- **Severity:** test plan inaccuracy
- **Description:** The test plan's section 5.2 lists `TRISTATE_TRANSFORM_SINGLE_DRIVER`, `TRISTATE_TRANSFORM_PER_BIT_FAIL`, and `TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL` under "Rules Not Tested" citing compiler bugs or dead code. All three rules work correctly and fire as expected. Tests for all three pass cleanly.
- **Impact:** None — all rules are fully tested in the 11.4 suite.

## test_11_6-tristate_inout_handling.md

### Issue 1: Test plan incorrectly lists TRISTATE_TRANSFORM_BLACKBOX_PORT as having a compiler bug

- **Severity:** test plan inaccuracy
- **Description:** The test plan's section 5.2 lists `TRISTATE_TRANSFORM_BLACKBOX_PORT` under "Rules Not Tested" with reason "Bug: test exists but rule has a known compiler bug." The rule fires correctly — when a blackbox module's INOUT port is connected to a shared tri-state wire, the compiler emits `TRISTATE_TRANSFORM_BLACKBOX_PORT` as expected. The test passes cleanly.
- **Impact:** None — the rule is fully tested in `11_GND_6_TRISTATE_TRANSFORM_BLACKBOX_PORT-blackbox_inout.jz`.

## test_11_7-tristate_error_conditions.md

No new issues discovered. All TRISTATE_TRANSFORM_* rules fire correctly. The test plan's section 5.2 lists several rules as having compiler bugs or being dead code, but all rules work correctly (consistent with findings from test_11_4 and test_11_6).

### Issue 2: Compiler diagnostic output order depends on absolute vs relative file path

- **Severity:** test observation (not a bug)
- **Description:** When the compiler is invoked with a relative file path, `File: <input>` diagnostics (from IR-level errors like `TRISTATE_TRANSFORM_OE_EXTRACT_FAIL`) appear before `File: <filename>.jz` diagnostics. When invoked with an absolute file path (as `run_validation.sh` does), the order is reversed — `File: <filename>.jz` appears first. The `.out` file must match the absolute-path order used by the validation script.
- **Impact:** Test authors must capture expected output using the same invocation pattern as `run_validation.sh` (absolute path, `>"$tmp" 2>&1`). No functional impact on compiler correctness.

## test_3_1-operator_categories.md

No new issues discovered. All rules fire correctly:
- `LOGICAL_WIDTH_NOT_1` fires for all three logical operators (`&&`, `||`, `!`) with multi-bit operands in ASYNC and SYNC blocks across multiple modules (8 triggers).
- `TYPE_BINOP_WIDTH_MISMATCH` fires for add, subtract, bitwise AND/XOR, comparisons (==, !=, <), and multiply with mismatched widths (8 triggers).
- `UNARY_ARITH_MISSING_PARENS` fires for unparenthesized unary minus and plus in ASYNC and SYNC blocks (5 triggers).
- `TERNARY_COND_WIDTH_NOT_1` fires for multi-bit ternary conditions in ASYNC and SYNC (4 triggers).
- `TERNARY_BRANCH_WIDTH_MISMATCH` fires for mismatched ternary branch widths in ASYNC and SYNC (4 triggers).
- `CONCAT_EMPTY` fires for empty `{}` in ASYNC and SYNC (4 triggers).
- `DIV_CONST_ZERO` fires for division and modulus by literal zero (4 triggers).
- `DIV_UNGUARDED_RUNTIME_ZERO` fires for unguarded runtime division/modulus (4 triggers). Guarded forms (`!= 0`, `> 0`, `== nonzero`) produce no false positives.
- `SPECIAL_DRIVER_IN_EXPRESSION` fires for GND/VCC in binary ops, unary ops, and ternary conditions (6 triggers).
- `SPECIAL_DRIVER_IN_CONCAT` fires for GND/VCC inside concatenation (4 triggers).
- `SPECIAL_DRIVER_SLICED` fires for VCC/GND with slice operators (4 triggers).
- `SPECIAL_DRIVER_IN_INDEX` fires for GND/VCC as index expressions (1 trigger — limited by compiler halting after first diagnostic, see section 2.4 issues).

## test_1_2-fundamental_terms.md

### Issue 1: NET_DANGLING_UNUSED rule never fires — WARN_UNUSED_WIRE fires instead

- **Severity:** test observation (rule overlap)
- **Description:** The test plan lists `NET_DANGLING_UNUSED` (S5.1/S8.3, "Signal is neither driven nor read; remove it or connect it") as the expected rule for unused signals. However, the compiler fires `WARN_UNUSED_WIRE` (S12.3, "WIRE declared but never driven or read; remove it if unused") instead. Both rules exist in `rules.c`, but `WARN_UNUSED_WIRE` appears to take precedence for WIRE declarations. `NET_DANGLING_UNUSED` may be intended for other signal types (ports, registers) or may be dead code.
- **Reproduction:**
  ```jz
  WIRE {
      dangling [8];  // never driven or read
  }
  ```
  Result: `WARN_UNUSED_WIRE`, not `NET_DANGLING_UNUSED`.
- **Impact:** The test `1_2_NET_DANGLING_UNUSED-unused_signal.jz` captures `WARN_UNUSED_WIRE` diagnostics, which is the compiler's actual behavior. `NET_DANGLING_UNUSED` cannot be triggered via `--info --lint` for WIRE declarations.

### Issue 2: NET_TRI_STATE_ALL_Z_READ test inherently triggers WARN_INTERNAL_TRISTATE

- **Severity:** test observation (not a bug)
- **Description:** Testing `NET_TRI_STATE_ALL_Z_READ` requires assigning `z` values to wires, which inherently triggers `WARN_INTERNAL_TRISTATE` ("Internal tri-state logic is not FPGA-compatible"). These warnings are included in the `.out` file as they are a direct consequence of the z-assignment construct being tested.
- **Impact:** The `1_2_NET_TRI_STATE_ALL_Z_READ-all_z_drivers_read.jz` test includes `WARN_INTERNAL_TRISTATE` warnings in its expected output. These are not scaffolding errors but inherent to testing z-assignment behavior.

## test_1_6-high_impedance_and_tristate.md

No new issues discovered. All rules fire correctly:
- `LIT_DECIMAL_HAS_XZ` fires for z digits in decimal literals across all expression contexts (async, CONST, register init, instance binding, sync).
- `LIT_INVALID_DIGIT_FOR_BASE` fires for z digits in hex literals across the same contexts.
- `REG_INIT_CONTAINS_Z` fires for z bits in register init across multiple modules and width patterns.
- `PORT_TRISTATE_MISMATCH` fires for z assigned to OUT ports. `WARN_INTERNAL_TRISTATE` co-fires as expected (same observation as test_1_2 Issue 2).
- `NET_TRI_STATE_ALL_Z_READ` fires when all drivers assign z and the wire is read. `WARN_INTERNAL_TRISTATE` co-fires as expected.
- Happy-path test with valid z literals, conditional tri-state on INOUT ports, and INOUT read-when-released produces no diagnostics.

## test_12_2-combinational_loop_errors.md

### Issue 1: Compiler does not detect combinational loops through instance ports

- **Severity:** bug (missing detection)
- **Description:** When a wire drives an instance's input port and the same wire is driven by the instance's output port (which depends combinationally on the input), the compiler does not detect the resulting combinational loop. The loop detection appears to only analyze dependencies within a single module's ASYNCHRONOUS block, not across instance port boundaries.
- **Reproduction:**
  ```jz
  @module Passthrough
      PORT {
          IN  [1] din;
          OUT [1] dout;
      }
      ASYNCHRONOUS {
          dout = din;
      }
  @endmod

  @module TopMod
      PORT {
          IN  [1] in_a;
          OUT [1] out_a;
      }
      WIRE {
          feedback [1];
      }
      // feedback -> inst.din -> inst.dout -> feedback (cycle!)
      @new inst_loop Passthrough {
          IN  [1] din = feedback;
          OUT [1] dout = feedback;
      };
      ASYNCHRONOUS {
          out_a = feedback;
      }
  @endmod
  ```
  Result: No diagnostic. Expected: `COMB_LOOP_UNCONDITIONAL` error.
- **Impact:** Cross-instance combinational loops are not detected. The test `12_2_COMB_LOOP_UNCONDITIONAL-instance_port_cycle.jz` currently has an empty `.out` file (matching actual compiler behavior). When this bug is fixed, the `.out` should be updated to include the expected `COMB_LOOP_UNCONDITIONAL` error.

## test_12_4-path_security.md

### Issue 1: @import path error prevents subsequent triggers from firing

- **Severity:** test observation (not a bug)
- **Description:** When `@import` triggers a `PATH_ABSOLUTE_FORBIDDEN` or `PATH_TRAVERSAL_FORBIDDEN` error, the compiler halts further processing. This means `@file()` triggers in the same file's MEM blocks are never reached. Tests for @import and @file() contexts must therefore be in separate files.
- **Impact:** PATH_ABSOLUTE_FORBIDDEN and PATH_TRAVERSAL_FORBIDDEN each require 2 test files (one per context) rather than a combined file. This is expected behavior — @import errors are fatal to project loading.

## test_2_4-special_semantic_drivers.md

### Issue 1: SPECIAL_DRIVER_IN_INDEX — compiler halts after first diagnostic

- **Severity:** test observation (compiler limitation)
- **Description:** The compiler only emits one `SPECIAL_DRIVER_IN_INDEX` diagnostic per compilation. After the first `r[GND]` or `r[VCC]` index expression triggers the rule, subsequent instances in the same file (even in different modules or blocks) are not reported. This prevents testing multiple index contexts in a single file.
- **Reproduction:**
  ```jz
  ASYNCHRONOUS {
      out_a = data[GND];   // fires SPECIAL_DRIVER_IN_INDEX
      out_b = data[VCC];   // NOT reported
  }
  ```
  Result: Only the first `SPECIAL_DRIVER_IN_INDEX` is emitted.
- **Impact:** `SPECIAL_DRIVER_IN_INDEX` tests are split into two files (`2_4_SPECIAL_DRIVER_IN_INDEX-gnd_in_index.jz` and `2_4_SPECIAL_DRIVER_IN_INDEX-vcc_in_index.jz`) — one for GND-as-index in async, one for VCC-as-index in sync. Each file contains one trigger with realistic multi-module structure.

## test_3_2-operator_definitions.md

### Issue 1: Test plan incorrectly lists SPECIAL_DRIVER_IN_INDEX as dead code

- **Severity:** test plan inaccuracy
- **Description:** The test plan's section 5.2 lists `SPECIAL_DRIVER_IN_INDEX` under "Rules Not Tested" with reason "Dead code: test exists but rule is dead code." The rule fires correctly — `data[GND]` and `data[VCC]` both trigger `SPECIAL_DRIVER_IN_INDEX` as expected. Tests for both GND and VCC variants pass cleanly.
- **Impact:** None — the rule is fully tested in `3_2_SPECIAL_DRIVER_IN_INDEX-gnd_as_index.jz` and `3_2_SPECIAL_DRIVER_IN_INDEX-vcc_as_index.jz` (also tested in sections 2.4 and 3.1).

### Issue 2: SPECIAL_DRIVER_IN_INDEX — compiler halts after first diagnostic (same as section 2.4)

- **Severity:** test observation (compiler limitation)
- **Description:** Same as section 2.4 Issue 1. The compiler only emits one `SPECIAL_DRIVER_IN_INDEX` diagnostic per compilation. Tests are split into separate GND and VCC files.
- **Impact:** `3_2_SPECIAL_DRIVER_IN_INDEX` tests are split into two files — one for GND-as-index in async, one for VCC-as-index in sync.

## test_3_4-operator_examples.md

### Issue 1: Test plan incorrectly lists SPECIAL_DRIVER_IN_INDEX as dead code (same as section 3.2)

- **Severity:** test plan inaccuracy
- **Description:** The test plan's section 5.2 lists `SPECIAL_DRIVER_IN_INDEX` under "Rules Not Tested" with reason "Dead code: test exists but rule is dead code." The rule fires correctly — `data[GND]` and `data[VCC]` both trigger `SPECIAL_DRIVER_IN_INDEX` as expected. Tests for both GND and VCC variants pass cleanly.
- **Impact:** None — the rule is fully tested in `3_4_SPECIAL_DRIVER_IN_INDEX-gnd_in_index.jz` and `3_4_SPECIAL_DRIVER_IN_INDEX-vcc_in_index.jz` (also tested in sections 2.4, 3.1, and 3.2).

### Issue 2: SPECIAL_DRIVER_IN_INDEX — compiler halts after first diagnostic (same as section 2.4)

- **Severity:** test observation (compiler limitation)
- **Description:** Same as section 2.4 Issue 1. The compiler only emits one `SPECIAL_DRIVER_IN_INDEX` diagnostic per compilation. Tests are split into separate GND and VCC files.
- **Impact:** `3_4_SPECIAL_DRIVER_IN_INDEX` tests are split into two files — one for GND-as-index in async, one for VCC-as-index in sync.

## test_4_2-scope_and_uniqueness.md

No issues discovered. All rules fire correctly:
- `ID_DUP_IN_MODULE` fires for port-port, port-wire, const-const, wire-port, wire-wire, and register-register duplicates across multiple modules.
- `MODULE_NAME_DUP_IN_PROJECT` fires for duplicate module definitions (second and third copies).
- `BLACKBOX_NAME_DUP_IN_PROJECT` fires for blackbox-blackbox and blackbox-module name collisions. Note: `WARN_UNUSED_MODULE` co-fires on the shadowed module — expected behavior since the blackbox name takes precedence.
- `INSTANCE_NAME_DUP_IN_MODULE` fires for duplicate instance names in both child and top modules.
- `INSTANCE_NAME_CONFLICT` fires for instance names matching ports, wires, registers, and CONSTs.
- `UNDECLARED_IDENTIFIER` fires in ASYNC RHS, SYNC RHS, CLK parameter, @new port binding, and instance port reference contexts.
- `AMBIGUOUS_REFERENCE` fires for bare identifier that matches only an instance port name.

## test_4_10-asynchronous_block.md

No new issues discovered. All rules fire correctly:
- `ASYNC_ALIAS_IN_CONDITIONAL` fires for alias `=`, `=z`, and `=s` inside IF, ELSE, ELIF, SELECT/CASE, and DEFAULT blocks across multiple modules (5 triggers).
- `ASYNC_ALIAS_LITERAL_RHS` fires for alias `=`, `=z`, and `=s` with bare literal RHS across multiple modules (5 triggers).
- `ASYNC_ASSIGN_REGISTER` fires for receive (`<=`), alias (`=`), and drive (`=>`) assignments to registers in ASYNC blocks across multiple modules (3 triggers).
- `ASYNC_INVALID_STATEMENT_TARGET` fires for receive (`<=`), alias (`=`), and drive (`=>`) assignments to CONST in ASYNC blocks across multiple modules (3 triggers).
- `ASYNC_UNDEFINED_PATH_NO_DRIVER` fires for IF-without-ELSE and SELECT-without-DEFAULT patterns across multiple modules (3 triggers). `WARN_INCOMPLETE_SELECT_ASYNC` co-fires for SELECT-without-DEFAULT as expected.
- `ASYNC_FLOATING_Z_READ` does not exist in `rules.c` — the test plan references a non-existent rule ID. The closest rule is `NET_FLOATING_WITH_SINK` (tested in section 1.2).
- `WIDTH_ASSIGN_MISMATCH_NO_EXT` is always suppressed by higher-priority `ASSIGN_WIDTH_NO_MODIFIER` (priority 1 vs 0) and cannot be independently triggered (same as section 2.3).

## test_4_12-cdc_block.md

### Issue 1: CDC_DEST_ALIAS_DUP does not fire when dest alias conflicts with a WIRE name

- **Severity:** bug (missing detection)
- **Description:** When a CDC destination alias name conflicts with an existing WIRE declaration, the compiler emits `ID_DUP_IN_MODULE` (the generic duplicate identifier error) but does NOT emit `CDC_DEST_ALIAS_DUP`. In contrast, when the alias conflicts with a REGISTER or PORT name, both `ID_DUP_IN_MODULE` and `CDC_DEST_ALIAS_DUP` fire correctly. This suggests the CDC-specific duplicate check does not consider WIRE declarations.
- **Reproduction:**
  ```jz
  WIRE {
      my_wire [1];
  }
  REGISTER {
      src [1] = 1'b0;
  }
  CDC {
      BIT src (clk_a) => my_wire (clk_b);  // ID_DUP_IN_MODULE fires, CDC_DEST_ALIAS_DUP does NOT
  }
  ```
  Result: Only `ID_DUP_IN_MODULE`. Expected: Both `ID_DUP_IN_MODULE` and `CDC_DEST_ALIAS_DUP`.
- **Impact:** Test `4_12_CDC_DEST_ALIAS_DUP-alias_name_conflict.jz` includes the wire conflict trigger and captures `ID_DUP_IN_MODULE` only. When this bug is fixed, the `.out` should be updated to include `CDC_DEST_ALIAS_DUP` for the wire conflict line.

### Issue 2: Test plan incorrectly lists CDC_SOURCE_NOT_PLAIN_REG and CDC_DEST_ALIAS_ASSIGNED as having compiler bugs

- **Severity:** test plan inaccuracy
- **Description:** The test plan's section 5.2 lists `CDC_SOURCE_NOT_PLAIN_REG` and `CDC_DEST_ALIAS_ASSIGNED` under "Rules Not Tested" with reason "Bug: test exists but rule has a known compiler bug." Both rules work correctly — `CDC_SOURCE_NOT_PLAIN_REG` fires for sliced register sources (also co-fires `WARN_UNSINKED_REGISTER` since the parent register is not directly read), and `CDC_DEST_ALIAS_ASSIGNED` fires for alias assignments in ASYNC and receive assignments in SYNC blocks. Tests for both rules pass cleanly.
- **Impact:** None — both rules are fully tested in the 4.12 suite.

### Issue 3: CDC_SOURCE_NOT_PLAIN_REG co-fires WARN_UNSINKED_REGISTER

- **Severity:** test observation (not a bug)
- **Description:** When testing `CDC_SOURCE_NOT_PLAIN_REG` with a sliced register like `wide_reg[0:0]` as the CDC source, the compiler also emits `WARN_UNSINKED_REGISTER` for `wide_reg`. This occurs because the CDC source syntax uses a slice (which is the violation being tested), and the compiler treats the base register as unread when only a slice is used in the CDC context. The `WARN_UNSINKED_REGISTER` warnings are included in the `.out` file.
- **Impact:** The test `4_12_CDC_SOURCE_NOT_PLAIN_REG-sliced_source.jz` includes `WARN_UNSINKED_REGISTER` warnings in its expected output. These are not scaffolding errors but a direct consequence of the slice construct being tested.

## test_4_13-module_instantiation.md

### Issue 1: Parser does not support =z and =s width modifiers in @new port bindings

- **Severity:** bug (missing feature)
- **Description:** The specification (S4.13) defines `=z` (zero-extend) and `=s` (sign-extend) width modifiers for port bindings in `@new` blocks. However, the parser rejects these with `PARSE000: expected '=' after port name in @new instance body`. The `=z` and `=s` modifiers work correctly in ASYNCHRONOUS assignment contexts but are not recognized in `@new` port binding syntax.
- **Reproduction:**
  ```jz
  @new inst_zext NarrowMod {
      IN  [8] wide_in =z din[3:0];
      OUT [8] wide_out = result;
  };
  ```
  Result: `PARSE000 parse error near token '=z': expected '=' after port name in @new instance body`
- **Impact:** Width modifiers `=z` and `=s` cannot be tested in `@new` port bindings. The happy path test omits these features. When the parser is fixed, the happy path should be updated to include `=z` and `=s` test cases.

### Issue 2: INSTANCE_ARRAY_MULTI_DIMENSIONAL may be dead code

- **Severity:** test observation (not a bug)
- **Description:** The test plan lists `INSTANCE_ARRAY_MULTI_DIMENSIONAL` as potentially dead code. However, the rule fires correctly when `@new inst[4][2] Mod { ... }` is used — the compiler emits the expected diagnostic. The test passes cleanly.
- **Impact:** None — the rule is fully tested in `4_13_INSTANCE_ARRAY_MULTI_DIMENSIONAL-multi_dim_array.jz`.

## test_4_14-feature_guards.md

### Issue 1: FEATURE_NESTED not detected at module level

- **Severity:** bug (missing detection)
- **Description:** When `@feature` guards are nested at module level (between block declarations, not inside ASYNCHRONOUS/SYNCHRONOUS/REGISTER/WIRE blocks), the compiler does not emit `FEATURE_NESTED`. The rule fires correctly inside all block types (ASYNCHRONOUS, SYNCHRONOUS, REGISTER, WIRE) and in @else bodies, but not at module scope.
- **Reproduction:**
  ```jz
  @module TopMod
      PORT {
          IN  [1] clk;
          OUT [1] data;
      }
      CONST {
          FLAG = 1;
          LEVEL = 0;
      }
      @feature FLAG == 1
          @feature LEVEL == 0
          @endfeat
      @endfeat
      REGISTER { r [1] = 1'b0; }
      ASYNCHRONOUS { data = r; }
      SYNCHRONOUS(CLK=clk) { r <= ~r; }
  @endmod
  ```
  Result: No diagnostic. Expected: `FEATURE_NESTED` error at the inner `@feature`.
- **Impact:** The test `4_14_FEATURE_NESTED-nested_feature_at_module_level.jz` currently has an empty `.out` file (matching actual compiler behavior). When this bug is fixed, the `.out` should be updated to include the expected `FEATURE_NESTED` error.

### Issue 2: Test plan incorrectly lists FEATURE_VALIDATION_BOTH_PATHS as unimplemented

- **Severity:** test plan observation
- **Description:** The test plan's section 5.2 lists `FEATURE_VALIDATION_BOTH_PATHS` under "Rules Not Tested" with reason "Unimplemented: no validation test exists; rule is not yet implemented in the compiler." This rule ID does not exist in `rules.c` at all, so it cannot be tested.
- **Impact:** None — the rule does not exist in the compiler and cannot be tested until it is implemented and added to `rules.c`.

## test_4_3-const.md

No issues discovered. All rules fire correctly:
- `CONST_NEGATIVE_OR_NONINT` fires for negative CONST values in helper and top modules (2 triggers).
- `CONST_UNDEFINED_IN_WIDTH_OR_SLICE` fires for undefined CONST names in IN port width, OUT port width, wire width, and register width across multiple modules (4 triggers).
- `CONST_CIRCULAR_DEP` fires for direct circular dependencies (A=B; B=A) in both helper and top modules (4 triggers, 2 per module).
- `CONST_USED_WHERE_FORBIDDEN` fires for CONST identifiers in SYNCHRONOUS receive expressions and ASYNCHRONOUS alias expressions across multiple modules (3 triggers).
- `CONST_STRING_IN_NUMERIC_CONTEXT` fires for string CONST in port width, wire width, and register width contexts (3 triggers).
- `CONST_NUMERIC_IN_STRING_CONTEXT` fires for numeric CONST in @file() path argument (1 trigger — only string context in the language).

Note: The test plan's section 5.2 listed `CONST_CIRCULAR_DEP` under "Rules Not Tested" with reason "Bug: test exists but rule has a known compiler bug." The rule works correctly — circular dependencies are detected and reported as expected. Tests pass cleanly.

## test_4_5-wire.md

### Issue 1: WIRE_MULTI_DIMENSIONAL — compiler halts after first diagnostic

- **Severity:** test observation (compiler limitation)
- **Description:** The compiler only emits one `WIRE_MULTI_DIMENSIONAL` diagnostic per compilation. When both a helper module and the top module contain multi-dimensional wire declarations, only the first module's error is reported. The second module's `WIRE_MULTI_DIMENSIONAL` is never reached.
- **Reproduction:**
  ```jz
  @module HelperMod
      WIRE {
          w_bad [8] [4];   // fires WIRE_MULTI_DIMENSIONAL
      }
  @endmod

  @module TopMod
      WIRE {
          w_multi [16] [2]; // NOT reported
      }
  @endmod
  ```
  Result: Only the first `WIRE_MULTI_DIMENSIONAL` is emitted.
- **Impact:** `WIRE_MULTI_DIMENSIONAL` tests are split into two files (`4_5_WIRE_MULTI_DIMENSIONAL-multi_dim_helper.jz` and `4_5_WIRE_MULTI_DIMENSIONAL-multi_dim_top.jz`) — one for the helper module context, one for the top module context. Each file contains one trigger with realistic syntax.

### Issue 2: Test plan incorrectly lists WIRE_MULTI_DIMENSIONAL as dead code

- **Severity:** test plan inaccuracy
- **Description:** The test plan's section 5.2 lists `WIRE_MULTI_DIMENSIONAL` under "Rules Not Tested" with reason "Dead code: test exists but rule is dead code." The rule fires correctly — `w_bad [8] [4]` triggers `WIRE_MULTI_DIMENSIONAL` as expected. Tests for both helper and top module variants pass cleanly.
- **Impact:** None — the rule is fully tested in `4_5_WIRE_MULTI_DIMENSIONAL-multi_dim_helper.jz` and `4_5_WIRE_MULTI_DIMENSIONAL-multi_dim_top.jz`.

## test_4_7-register.md

### Issue 1: REG_MULTI_DIMENSIONAL — compiler halts after first diagnostic

- **Severity:** test observation (compiler limitation)
- **Description:** The compiler only emits one `REG_MULTI_DIMENSIONAL` diagnostic per compilation. When both a helper module and the top module contain multi-dimensional register declarations, only the first module's error is reported. This is consistent with the `WIRE_MULTI_DIMENSIONAL` behavior (see test_4_5 Issue 1).
- **Reproduction:**
  ```jz
  @module HelperMod
      REGISTER {
          r_bad [8] [4];   // fires REG_MULTI_DIMENSIONAL
      }
  @endmod

  @module TopMod
      REGISTER {
          r_bad_top [16] [2]; // NOT reported
      }
  @endmod
  ```
  Result: Only the first `REG_MULTI_DIMENSIONAL` is emitted.
- **Impact:** `REG_MULTI_DIMENSIONAL` tests are split into two files (`4_7_REG_MULTI_DIMENSIONAL-multi_dim_helper.jz` and `4_7_REG_MULTI_DIMENSIONAL-multi_dim_top.jz`) — one per module context.

### Issue 2: REG_MISSING_INIT_LITERAL — compiler halts after first diagnostic

- **Severity:** test observation (compiler limitation)
- **Description:** The compiler only emits one `REG_MISSING_INIT_LITERAL` diagnostic per compilation. When both a helper module and the top module contain registers without init literals, only the first module's error is reported.
- **Reproduction:**
  ```jz
  @module HelperMod
      REGISTER {
          r_noinit [8];   // fires REG_MISSING_INIT_LITERAL
      }
  @endmod

  @module TopMod
      REGISTER {
          r_noinit_top [8]; // NOT reported
      }
  @endmod
  ```
  Result: Only the first `REG_MISSING_INIT_LITERAL` is emitted.
- **Impact:** `REG_MISSING_INIT_LITERAL` tests are split into two files (`4_7_REG_MISSING_INIT_LITERAL-missing_init_helper.jz` and `4_7_REG_MISSING_INIT_LITERAL-missing_init_top.jz`) — one per module context.

### Issue 3: Test plan incorrectly lists REG_MULTI_DIMENSIONAL and REG_MISSING_INIT_LITERAL as dead code

- **Severity:** test plan inaccuracy
- **Description:** The test plan's section 5.2 lists `REG_MULTI_DIMENSIONAL` and `REG_MISSING_INIT_LITERAL` under "Rules Not Tested" with reason "Dead code." Both rules fire correctly — `r [8] [4]` triggers `REG_MULTI_DIMENSIONAL` and `r [8];` triggers `REG_MISSING_INIT_LITERAL` as expected. Tests pass cleanly.
- **Impact:** None — both rules are fully tested in the 4.7 suite.

## test_4_8-latches.md

### Issue 1: LATCH_WIDTH_INVALID is unreachable — suppressed by WIDTH_NONPOSITIVE_OR_NONINT

- **Severity:** test gap (rule suppression)
- **Description:** `LATCH_WIDTH_INVALID` ("S4.8 LATCH width must be a positive integer") is never emitted by the compiler. When a latch is declared with width 0 (e.g., `lat [0] D;`), the generic `WIDTH_NONPOSITIVE_OR_NONINT` fires first, preventing the latch-specific rule from triggering. The test `4_8_LATCH_WIDTH_INVALID-invalid_latch_width.jz` documents this by expecting `WIDTH_NONPOSITIVE_OR_NONINT` instead.
- **Reproduction:**
  ```jz
  LATCH {
      lat_zero [0] D;
  }
  ```
  Result: `WIDTH_NONPOSITIVE_OR_NONINT` fires at the declaration, not `LATCH_WIDTH_INVALID`.
- **Impact:** `LATCH_WIDTH_INVALID` cannot be independently triggered. The zero-width case is still caught, just by a different rule.

### Issue 2: LATCH_AS_CLOCK_OR_CDC CDC test produces cascading MULTI_CLK_ASSIGN and DOMAIN_CONFLICT

- **Severity:** test observation (not a bug)
- **Description:** When testing latch as CDC source/destination clock in `4_8_LATCH_AS_CLOCK_OR_CDC-latch_as_cdc.jz`, the compiler correctly fires `LATCH_AS_CLOCK_OR_CDC` for both the source clock and destination clock positions. However, using a latch as a CDC clock also causes `MULTI_CLK_ASSIGN` (because the register is assigned in a SYNCHRONOUS block with a different clock than its CDC home domain) and `DOMAIN_CONFLICT` as cascading consequences. These are included in the `.out` file as they are real compiler behavior triggered by the construct under test.
- **Impact:** The test captures all 4 diagnostics (2 target + 2 cascading). Coverage is complete.

### Issue 3: Test plan incorrectly lists LATCH_SR_WIDTH_MISMATCH, LATCH_IN_CONST_CONTEXT, and LATCH_WIDTH_INVALID as having compiler bugs

- **Severity:** test plan inaccuracy
- **Description:** The test plan's section 5.2 lists these three rules under "Rules Not Tested" with reasons citing compiler bugs or suppression. In reality:
  - `LATCH_SR_WIDTH_MISMATCH` fires correctly when set/reset expression widths don't match the latch width.
  - `LATCH_IN_CONST_CONTEXT` fires correctly when a latch identifier is used in `@check` conditions.
  - `LATCH_WIDTH_INVALID` is genuinely suppressed by `WIDTH_NONPOSITIVE_OR_NONINT` (see Issue 1), but the zero-width case is still caught.
- **Impact:** All three rules have tests. `LATCH_SR_WIDTH_MISMATCH` and `LATCH_IN_CONST_CONTEXT` work correctly. `LATCH_WIDTH_INVALID` is tested indirectly via `WIDTH_NONPOSITIVE_OR_NONINT`.

## test_5_2-synchronous_assignments.md

### Issue 1: ASSIGN_CONCAT_WIDTH_MISMATCH fires for concat with <=z/<=s

- **Severity:** compiler bug (or unimplemented feature)
- **Description:** The spec states that concatenation decomposition supports `<=z` and `<=s` operators: `{reg_a, reg_b} <=z narrow_expr;` should zero-extend the narrow expression to match the total concat width. However, the compiler fires `ASSIGN_CONCAT_WIDTH_MISMATCH` when the RHS is narrower than the concat total, even with `<=z` or `<=s`. This prevents testing concat with extension modifiers as a happy-path scenario.
- **Reproduction:**
  ```jz
  REGISTER {
      r_a [4] = 4'h0;
      r_b [4] = 4'h0;
  }
  SYNCHRONOUS(CLK=clk) {
      {r_a, r_b} <=z narrow;  // narrow is [4], concat total is [8]
      // Expected: valid (zero-extend 4 to 8)
      // Actual: ASSIGN_CONCAT_WIDTH_MISMATCH error
  }
  ```
- **Impact:** Concat with `<=z`/`<=s` cannot be tested in the happy path. These forms are omitted from `5_2_HAPPY_PATH-sync_assignments_ok.jz`. Non-concat `<=z`/`<=s` works correctly.

## test_5_5-intrinsic_operators.md

No issues discovered. All 23 rules fire correctly:
- `CLOG2_NONPOSITIVE_ARG` fires for `clog2(0)` in CONST blocks.
- `CLOG2_INVALID_CONTEXT` fires for `clog2()` in ASYNC and SYNC blocks.
- `WIDTHOF_INVALID_CONTEXT` fires for `widthof()` in ASYNC and SYNC blocks.
- `WIDTHOF_INVALID_TARGET` fires for `widthof()` on CONST names.
- `WIDTHOF_INVALID_SYNTAX` fires for `widthof()` with slice arguments.
- `WIDTHOF_WIDTH_NOT_RESOLVABLE` fires for `widthof()` on signals with undefined width (co-fires `WARN_UNUSED_WIRE` — expected behavior since the unresolvable wire is unused).
- `FUNC_RESULT_TRUNCATED_SILENTLY` fires for intrinsic results assigned to narrower targets.
- `LIT_WIDTH_INVALID`, `LIT_VALUE_INVALID`, `LIT_VALUE_OVERFLOW`, `LIT_INVALID_CONTEXT` all fire correctly.
- `SBIT_SET_WIDTH_NOT_1` fires for non-1-bit third argument.
- `GBIT_INDEX_OUT_OF_RANGE`, `SBIT_INDEX_OUT_OF_RANGE`, `GSLICE_INDEX_OUT_OF_RANGE`, `SSLICE_INDEX_OUT_OF_RANGE` all fire for constant indices >= source width.
- `GSLICE_WIDTH_INVALID`, `SSLICE_WIDTH_INVALID` fire for zero width parameter.
- `SSLICE_VALUE_WIDTH_MISMATCH` fires for value width != width parameter.
- `OH2B_INPUT_TOO_NARROW`, `PRIENC_INPUT_TOO_NARROW` fire for 1-bit sources.
- `B2OH_WIDTH_INVALID` fires for width < 2.
- `BSWAP_WIDTH_NOT_BYTE_ALIGNED` fires for non-multiple-of-8 width.

## test_6_3-config_block.md

### Issue 1: WARN_UNUSED_WIRE fires for wire with undeclared/string CONFIG width

- **Severity:** Test gap
- **Description:** When a WIRE declaration uses an undeclared CONFIG reference (e.g., `w [CONFIG.MISSING];`) or a string CONFIG reference (e.g., `w [CONFIG.FIRMWARE];`) as its width, the compiler fires `WARN_UNUSED_WIRE` in addition to the expected rule (`CONFIG_USE_UNDECLARED` or `CONST_STRING_IN_NUMERIC_CONTEXT`). This occurs because the wire's width cannot be resolved, making it effectively unusable. The `WARN_UNUSED_WIRE` diagnostic is a cascading consequence, not an independent issue.
- **Impact:** Wire width contexts cannot be tested in the same file as other contexts without including the unrelated `WARN_UNUSED_WIRE` diagnostic. Wire width triggers were removed from `CONFIG_USE_UNDECLARED` and `CONST_STRING_IN_NUMERIC_CONTEXT` tests to keep the tests focused on the rule under test.
- **Reproduction:** Add `WIRE { w [CONFIG.NONEXISTENT]; }` to a module — compiler emits both `CONFIG_USE_UNDECLARED` and `WARN_UNUSED_WIRE`.

### Issue 2: UNDECLARED_IDENTIFIER fires alongside CONST_NUMERIC_IN_STRING_CONTEXT

- **Severity:** Compiler behavior (not a bug)
- **Description:** When a numeric CONFIG value is used in a `@file()` path argument (e.g., `@file(CONFIG.WIDTH)` where WIDTH is numeric), the compiler emits both `UNDECLARED_IDENTIFIER` and `CONST_NUMERIC_IN_STRING_CONTEXT` at the same location. The `UNDECLARED_IDENTIFIER` appears because the numeric CONFIG value cannot resolve as a file path, making the MEM effectively reference an undeclared symbol.
- **Impact:** The `.out` file for `CONST_NUMERIC_IN_STRING_CONTEXT` must include both diagnostics per trigger location.

### Issue 3: CONFIG_MULTIPLE_BLOCKS may be dead code

- **Severity:** Test gap (potential)
- **Description:** The test plan notes `CONFIG_MULTIPLE_BLOCKS` as "dead code." However, the test (`6_3_CONFIG_MULTIPLE_BLOCKS-multiple_config.jz`) passes — the compiler does emit the diagnostic when a second `CONFIG {}` block is present in `@project`. The rule appears to be functional, not dead.

## test_6_4-clocks_block.md

No issues discovered. All rules fire correctly:
- All 7 CLOCKS-block rules (`CLOCK_DUPLICATE_NAME`, `CLOCK_PERIOD_NONPOSITIVE`, `CLOCK_EDGE_INVALID`, `CLOCK_PORT_WIDTH_NOT_1`, `CLOCK_NAME_NOT_IN_PINS`, `CLOCK_EXTERNAL_NO_PERIOD`, `CLOCK_SOURCE_AMBIGUOUS`) fire as expected with correct line/column references.
- All 20 CLOCK_GEN rules fire correctly, including cascading diagnostics (e.g., `CLOCK_GEN_INPUT_NO_PERIOD` co-fires with `CLOCK_GEN_INPUT_IS_SELF_OUTPUT`; `CLOCK_GEN_DERIVED_OUT_OF_RANGE` co-fires with `CLOCK_GEN_PARAM_OUT_OF_RANGE` and `CLOCK_GEN_PARAM_TYPE_MISMATCH`).
- `CLOCK_GEN_INVALID_TYPE` fires correctly despite the test plan labeling it "dead code."
- `CLOCK_SOURCE_AMBIGUOUS` correctly cascades `CLOCK_NAME_NOT_IN_PINS` and `CLOCK_GEN_OUTPUT_HAS_PERIOD` for clocks with period that are also CLOCK_GEN outputs.
- `CLOCK_GEN_NO_CHIP_DATA` fires at info level when no CHIP is specified.

## test_6_5-pin_blocks.md

No issues discovered. All 15 PIN block rules fire correctly:
- All rules fire in every applicable PIN block context (IN_PINS, OUT_PINS, INOUT_PINS).
- `PIN_DECLARED_MULTIPLE_BLOCKS` correctly detects all three cross-block combinations (IN/OUT, IN/INOUT, OUT/INOUT).
- `PIN_DRIVE_MISSING_OR_INVALID` fires for missing, zero, and negative drive values in both OUT_PINS and INOUT_PINS.
- `PIN_BUS_WIDTH_INVALID` fires for zero and negative widths in all three PIN block types.
- `PIN_DIFF_OUT_MISSING_FCLK`, `PIN_DIFF_OUT_MISSING_PCLK`, and `PIN_DIFF_OUT_MISSING_RESET` correctly fire only for OUT_PINS differential pins (not for IN_PINS or INOUT_PINS differential pins).
- `PIN_PULL_ON_OUTPUT` correctly allows `pull=NONE` on OUT_PINS without triggering.
- `PIN_TERM_ON_OUTPUT` correctly allows `term=OFF` on OUT_PINS without triggering.
- No cascading errors or parser recovery issues encountered.

## test_6_7-blackbox_modules.md

### Issue 1: Parser rejects forbidden blocks in @blackbox with PARSE000 instead of BLACKBOX_BODY_DISALLOWED

- **Severity:** compiler inconsistency
- **Description:** The `BLACKBOX_BODY_DISALLOWED` rule message lists "ASYNCHRONOUS/SYNCHRONOUS/WIRE/REGISTER/MEM" as forbidden blocks, but the parser rejects these with `PARSE000` ("unexpected token in @blackbox body; expected CONST or PORT") before semantic analysis runs. Only `CONST` reaches the semantic pass and fires `BLACKBOX_BODY_DISALLOWED`. The parser's error message ("expected CONST or PORT") implies CONST is valid inside @blackbox, but semantics then rejects it — an inconsistency between parser acceptance and semantic rejection.
- **Impact:** The forbidden blocks are still caught, but by the parser (PARSE000) rather than by the intended semantic rule (BLACKBOX_BODY_DISALLOWED). Tests capture the actual PARSE000 output for ASYNCHRONOUS, SYNCHRONOUS, WIRE, REGISTER, and MEM.
- **Reproduction:** Place `ASYNCHRONOUS { dout = din; }` inside a `@blackbox` body after `PORT`.

### Issue 2: Parser does not recover after PARSE000 in @blackbox body

- **Severity:** parser recovery bug
- **Description:** When the parser encounters a forbidden block keyword (ASYNCHRONOUS, SYNCHRONOUS, WIRE, REGISTER, MEM) inside a `@blackbox` body, it emits PARSE000 and aborts parsing entirely. Subsequent blackbox declarations, module definitions, and semantic checks in the same file are not processed. This prevents combining multiple forbidden-block triggers in a single test file.
- **Impact:** Each forbidden block type requires its own separate test file. A CONST trigger (which fires BLACKBOX_BODY_DISALLOWED via semantics) in a different blackbox in the same file is also suppressed if any blackbox has a PARSE000-producing block.
- **Reproduction:** Define two @blackbox declarations in the same file — the first with `ASYNCHRONOUS {}` and the second with `CONST {}`. Only the first PARSE000 fires; the second blackbox's BLACKBOX_BODY_DISALLOWED is never reached.

## test_6_8-bus_aggregation.md

### Issue 1: BUS_DEF_INVALID_DIR is dead code — parser catches invalid directions before semantic rule fires

- **Severity:** dead code (unreachable rule)
- **Description:** The semantic rule `BUS_DEF_INVALID_DIR` ("S6.8 BUS signal direction must be IN, OUT, or INOUT") is defined in `rules.c` but can never be triggered via `--info --lint`. The parser only accepts `IN`, `OUT`, and `INOUT` keywords inside a BUS definition block. Any other keyword (e.g., `REG`, `WIRE`) produces a `PARSE000` error ("expected IN/OUT/INOUT in BUS block") before semantic analysis runs.
- **Reproduction:**
  ```jz
  BUS BAD_BUS {
      REG [8] data;
  }
  ```
  Result: `PARSE000 parse error near token 'REG': expected IN/OUT/INOUT in BUS block`
  Expected: `BUS_DEF_INVALID_DIR S6.8 BUS signal direction must be IN, OUT, or INOUT`
- **Impact:** `BUS_DEF_INVALID_DIR` cannot be tested. The parser's early rejection is correct behavior — the semantic rule is simply unreachable. Documented in `not_tested.md`.

### Issue 2: WARN_UNUSED_PORT fires for BUS port in happy path

- **Severity:** test observation (minor compiler quirk)
- **Description:** In the happy path test, `EnableMod` declares a `BUS ENABLE_BUS SOURCE ebus` port and uses it via `ebus.en = r`. Despite the BUS signal being written, the compiler emits `WARN_UNUSED_PORT` for the `ebus` port. This suggests the compiler does not consider BUS member signal access (e.g., `ebus.en`) as "using" the BUS port for the purposes of unused-port detection.
- **Impact:** The happy path `.out` file includes this warning. It does not affect correctness — the test still validates that all BUS constructs compile without errors.

## test_7_1-mem_declaration.md

No issues discovered. All 15 MEM declaration rules fire correctly:
- `MEM_DUP_NAME` fires for duplicate MEM names in both helper and top modules.
- `MEM_INVALID_WORD_WIDTH` fires for zero word width.
- `MEM_INVALID_DEPTH` fires for zero depth.
- `MEM_UNDEFINED_CONST_IN_WIDTH` fires for undefined CONST in both width and depth positions.
- `MEM_DUP_PORT_NAME` fires for OUT-OUT, IN-IN, and OUT-IN duplicate port names.
- `MEM_PORT_NAME_CONFLICT_MODULE_ID` fires for conflicts with PORT, REGISTER, and WIRE names.
- `MEM_EMPTY_PORT_LIST` fires for MEM with empty port list.
- `MEM_INVALID_PORT_TYPE` fires for IN ports with ASYNC and SYNC qualifiers.
- `MEM_MISSING_INIT` fires for MEM without initialization.
- `MEM_TYPE_INVALID` fires for invalid type keywords (INVALID, REGISTER).
- `MEM_TYPE_BLOCK_WITH_ASYNC_OUT` fires for BLOCK type with ASYNC OUT port.
- `MEM_CHIP_CONFIG_UNSUPPORTED` fires for MEM config exceeding chip capabilities.
- `MEM_INOUT_MIXED_WITH_IN_OUT` fires for INOUT+OUT, INOUT+IN, and INOUT+OUT+IN combinations.
- `MEM_INOUT_ASYNC` fires for INOUT ports with ASYNC and SYNC qualifiers.
- `MEM_UNDEFINED_NAME` fires for access to undeclared MEM names in both ASYNC and SYNC blocks.

Note: Several error tests produce cascading `MEM_WARN_PORT_NEVER_ACCESSED` and `WARN_UNUSED_PORT` warnings as direct consequences of the MEM declaration error under test. These are expected — when a MEM has an invalid configuration, its ports cannot be meaningfully accessed, triggering the port-unused warnings.

## test_7_10-const_evaluation_in_mem.md

No issues discovered. All rules fire correctly:
- `CONST_NEGATIVE_OR_NONINT` fires for negative CONST values in MEM dimension context across modules.
- `MEM_UNDEFINED_CONST_IN_WIDTH` fires for undefined CONST in width position, depth position, and both positions. Cascading `MEM_WARN_PORT_NEVER_ACCESSED` warnings are expected consequences.
- `CONST_CIRCULAR_DEP` fires for circular A->B->A dependencies in both helper and top module CONST blocks.
- `CONST_UNDEFINED_IN_WIDTH_OR_SLICE` fires for undefined CONST in port width, wire width, and register width contexts.

## test_7_3-memory_access_syntax.md

### Cascading diagnostics from scaffolding interactions (minor, not bugs)

Several tests produce additional diagnostics beyond the primary rule under test. These are expected consequences of the test structure, not compiler bugs:

1. **`MEM_PORT_UNDEFINED` test**: When an undefined port is accessed with bracket syntax (`mem.fake[addr]`), the compiler also emits `CONST_UNDEFINED_IN_WIDTH_OR_SLICE` because it cannot resolve the address width of a nonexistent port. The `MEM_WARN_PORT_NEVER_ACCESSED` warning fires because the valid `rd` port (ASYNC OUT) in HelperMod is not accessed in any read context (the only read accesses use the fake port name). Both are expected cascading behavior.

2. **`MEM_SYNC_PORT_INDEXED`, `MEM_INOUT_INDEXED`, `MEM_PORT_USED_AS_SIGNAL`, `MEM_PORT_ADDR_READ` tests**: These produce `SYNC_MULTI_ASSIGN_SAME_REG_BITS` because the erroneous memory access is an additional assignment to the same register `r` that is already assigned elsewhere in the same SYNCHRONOUS block. This is expected — the compiler correctly detects both the memory access error and the duplicate register assignment.

3. **`MEM_READ_FROM_WRITE_PORT` test**: Produces `WARN_UNSINKED_REGISTER` because register `r` is written (from the erroneous read) but never read as an output. This is expected scaffolding noise.

4. **`MEM_PORT_USED_AS_SIGNAL` test**: Produces `MEM_WARN_PORT_NEVER_ACCESSED` because in HelperMod, the ASYNC OUT `rd` port is never read via bracket indexing (only referenced as a bare port, which is the error). This is expected.

### No compiler bugs discovered

All 26 rules in the test plan fire correctly in all tested contexts. No rules were found to be suppressed or unimplemented that should be working.

## test_7_5-initialization.md

### Issue 1: UNDECLARED_IDENTIFIER co-fires with CONST_NUMERIC_IN_STRING_CONTEXT for CONFIG values

- **Severity:** minor diagnostic noise
- **Description:** When a numeric `CONFIG` value is used in `@file(CONFIG.NUM_CFG)`, the compiler emits both `UNDECLARED_IDENTIFIER` and `CONST_NUMERIC_IN_STRING_CONTEXT` at the same location. The `UNDECLARED_IDENTIFIER` is spurious — the CONFIG entry does exist but is numeric, not string. Module-scope numeric `CONST` values used in `@file(DEPTH)` only emit `CONST_NUMERIC_IN_STRING_CONTEXT` (correct behavior).
- **Reproduction:**
  ```jz
  @project test
      CONFIG { NUM_CFG = 42; }
      @top Mod { IN [1] clk = _; OUT [1] d = _; }
  @endproj
  @module Mod
      PORT { IN [1] clk; OUT [1] d; }
      MEM {
          m [8] [4] = @file(CONFIG.NUM_CFG) {  // fires UNDECLARED_IDENTIFIER + CONST_NUMERIC_IN_STRING_CONTEXT
              OUT rd ASYNC;
          };
      }
      ASYNCHRONOUS { d = m.rd[1'b0]; }
      SYNCHRONOUS(CLK=clk) { }
  @endmod
  ```
- **Impact:** Test `7_5_CONST_NUMERIC_IN_STRING_CONTEXT-numeric_const_in_file.jz` captures the actual behavior including the spurious `UNDECLARED_IDENTIFIER`.

### Issue 2: PATH_OUTSIDE_SANDBOX vs MEM_INIT_FILE_NOT_FOUND when running from project root

- **Severity:** environment-dependent behavior (not a bug)
- **Description:** When running `jz-hdl --info --lint` from the project root (outside the validation directory), nonexistent `@file` paths produce `PATH_OUTSIDE_SANDBOX` instead of `MEM_INIT_FILE_NOT_FOUND`, because the sandbox path resolver cannot canonicalize nonexistent paths. When run from the validation directory (as the test script does), the sandbox correctly resolves the base directory and `MEM_INIT_FILE_NOT_FOUND` fires as expected.
- **Impact:** None for automated testing — the validation script runs with correct sandbox context. Developers running individual tests from the project root may see different diagnostics.

## test_9_3-check_placement_rules.md

### Issue 1: DIRECTIVE_INVALID_CONTEXT produces cascading PARSE000 for @check inside blocks

- **Severity:** parser recovery bug (minor)
- **Description:** When `@check` appears inside an ASYNCHRONOUS or SYNCHRONOUS block, the compiler correctly emits `DIRECTIVE_INVALID_CONTEXT` but then fails to skip the `@check (...)` expression, producing a follow-on `PARSE000` error on the `(` token. This prevents combining both ASYNC and SYNC triggers in a single test file, as cascading errors from the first trigger would interfere with the second.
- **Reproduction:**
  ```jz
  ASYNCHRONOUS {
      @check (1, "test");  // DIRECTIVE_INVALID_CONTEXT at @check
                           // PARSE000 at '(' — cascading
      data = r;
  }
  ```
- **Workaround:** Split ASYNC and SYNC @check triggers into separate test files. Each file uses realistic syntax and its `.out` captures both the correct diagnostic and the cascading PARSE000.
- **Impact:** Tests work correctly with split files. No syntax simplification needed.

## test_9_7-check_error_conditions.md

### Issue 1: DIRECTIVE_INVALID_CONTEXT — Parser recovery bug with @check inside blocks

- **Severity:** bug (parser recovery)
- **Description:** When `@check` appears inside an ASYNCHRONOUS or SYNCHRONOUS block, the compiler correctly emits `DIRECTIVE_INVALID_CONTEXT` but then fails to recover, producing a cascading `PARSE000` error (`parse error near token '(': expected identifier in assignment left-hand side`). This prevents multiple @check-in-block triggers from being tested in the same file, since the parser cannot continue past the first violation.
- **Reproduction:**
  ```jz
  ASYNCHRONOUS {
      @check (1, "test");  // DIRECTIVE_INVALID_CONTEXT at @check
                           // PARSE000 at '(' — cascading
      data = r;
  }
  ```
- **Workaround:** Split ASYNC and SYNC @check-in-block triggers into separate test files (`9_7_DIRECTIVE_INVALID_CONTEXT-check_in_async.jz` and `9_7_DIRECTIVE_INVALID_CONTEXT-check_in_sync.jz`). Each file captures both the correct diagnostic and the cascading PARSE000.
- **Impact:** Tests work correctly with split files. No syntax simplification needed. This is the same parser recovery issue documented in test_9_3.

## test_misc-repeat_serializer_io.md

No issues discovered. All REPEAT rules fire correctly:
- `RPT_COUNT_INVALID` fires for zero, negative, and non-numeric @repeat counts.
- `RPT_NO_MATCHING_END` fires for @repeat without matching @end.
- @repeat errors are fatal preprocessor errors — the repeat expander stops processing the file after the first error, so each trigger requires a separate test file.

Rules not testable via `--info --lint` (no issues, by design):
- `INFO_SERIALIZER_CASCADE` — backend-only diagnostic
- `SERIALIZER_WIDTH_EXCEEDS_RATIO` — backend-only diagnostic
- `IO_BACKEND` — runtime I/O error
- `IO_IR` — runtime I/O error
