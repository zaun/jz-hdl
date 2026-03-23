# Rules Not Testable via `--info --lint`

This document lists rule IDs from `compiler/src/rules.c` that cannot be validated using the standard `--info --lint` test framework. Each entry includes the reason the rule is excluded from lint-based validation testing.

## Testbench Rules (requires `--test`)

These rules fire only when the compiler is invoked with `--test` to process `@testbench` blocks.

| Rule ID | Severity | Description | Reason Not Tested |
|---------|----------|-------------|-------------------|
| TB_WRONG_TOOL | error | File contains @testbench blocks; use --test | Fires during --lint on testbench files (meta-diagnostic) |
| TB_PROJECT_MIXED | error | File may not contain both @project and @testbench | Requires --test context |
| TB_MODULE_NOT_FOUND | error | @testbench module name must refer to a module in scope | Requires --test parsing |
| TB_PORT_NOT_CONNECTED | error | All module ports must be connected in @new | Requires --test parsing |
| TB_PORT_WIDTH_MISMATCH | error | Port width must match module declared width | Requires --test parsing |
| TB_NEW_RHS_INVALID | error | @new RHS must be a testbench CLOCK or WIRE | Requires --test parsing |
| TB_SETUP_POSITION | error | @setup must appear exactly once per TEST | Requires --test parsing |
| TB_CLOCK_NOT_DECLARED | error | @clock identifier must refer to a declared CLOCK | Requires --test parsing |
| TB_CLOCK_CYCLE_NOT_POSITIVE | error | @clock cycle count must be a positive integer | Requires --test parsing |
| TB_UPDATE_NOT_WIRE | error | @update may only assign testbench WIRE identifiers | Requires --test parsing |
| TB_UPDATE_CLOCK_ASSIGN | error | @update may not assign clock signals | Requires --test parsing |
| TB_EXPECT_WIDTH_MISMATCH | error | @expect value width must match signal width | Requires --test parsing |
| TB_NO_TEST_BLOCKS | error | @testbench must contain at least one TEST block | Requires --test parsing |
| TB_MULTIPLE_NEW | error | Each TEST must contain exactly one @new | Requires --test parsing |

## Simulation Rules (requires `--simulate`)

These rules fire only during simulation execution.

| Rule ID | Severity | Description | Reason Not Tested |
|---------|----------|-------------|-------------------|
| SIM_WRONG_TOOL | error | File contains @simulation blocks; use --simulate | Fires during --lint on simulation files (meta-diagnostic) |
| SIM_PROJECT_MIXED | error | File may not contain both @project and @simulation | Requires --simulate context |
| SIM_RUN_COND_TIMEOUT | error | @run_until/@run_while condition not met within timeout | Runtime-only; requires simulation execution |

## Repeat Rules (requires `--test` or `--simulate`)

| Rule ID | Severity | Description | Reason Not Tested |
|---------|----------|-------------|-------------------|
| RPT_COUNT_INVALID | error | @repeat requires a positive integer count | Requires testbench/simulation context |
| RPT_NO_MATCHING_END | error | @repeat without matching @end | Requires testbench/simulation context |

## Backend/IO Rules (fires during code generation)

These rules fire only during backend code generation or file output, not during lint analysis.

| Rule ID | Severity | Description | Reason Not Tested |
|---------|----------|-------------|-------------------|
| IO_BACKEND | error | Failed to open or write backend output file | Runtime I/O error during code generation |
| IO_IR | error | Failed to write or finalize IR output file | Runtime I/O error during IR generation |

## Serializer Rules (backend-only, chip-specific)

These rules fire during backend processing of differential output pins with serializers.

| Rule ID | Severity | Description | Reason Not Tested |
|---------|----------|-------------|-------------------|
| INFO_SERIALIZER_CASCADE | info | Differential output uses cascaded serializers | Backend-only; fires during code generation |
| SERIALIZER_WIDTH_EXCEEDS_RATIO | error | Port width exceeds chip serializer ratio | Backend-only; requires specific chip data |

## Rules Testable but Requiring Special Conditions

These rules CAN be tested via `--info --lint` but require specific conditions that may make testing difficult or platform-dependent.

| Rule ID | Severity | Description | Testing Difficulty |
|---------|----------|-------------|-------------------|
| PATH_OUTSIDE_SANDBOX | error | Resolved path falls outside sandbox roots | Requires sandbox configuration via CLI flags |
| PATH_SYMLINK_ESCAPE | error | Symbolic link resolves outside sandbox root | Requires creating symlinks; platform-dependent |
| LATCH_CHIP_UNSUPPORTED | error | LATCH type not supported by selected CHIP | Requires chip JSON without latch support |
| MEM_CHIP_CONFIG_UNSUPPORTED | error | MEM configuration not supported by chip | Requires chip JSON without memory support |

## Summary

| Category | Count |
|----------|-------|
| Testbench rules (--test only) | 14 |
| Simulation rules (--simulate only) | 3 |
| Repeat rules (--test/--simulate only) | 2 |
| Backend/IO rules | 2 |
| Serializer rules (backend only) | 2 |
| Special conditions | 4 |
| **Total not testable via --info --lint** | **27** |
