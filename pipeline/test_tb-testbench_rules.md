# Test Plan: Testbench Rules (TESTBENCH group)
**Specification Reference:** Testbench validation rules from `compiler/src/rules.c`

## 1. Objective

Verify all TESTBENCH diagnostic rules are correctly defined and documented. These rules apply to `@testbench` blocks processed via `--test` mode and are not reachable through `--info --lint` validation.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Valid testbench | Well-formed @testbench with @new, @setup, @clock, @update, @expect | Clean run via --test |

### 2.2 Error Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Wrong tool | File with @testbench run via --lint | Error: TB_WRONG_TOOL |
| 2 | Project mixed | File with both @project and @testbench | Error: TB_PROJECT_MIXED |
| 3 | Module not found | @testbench references non-existent module | Error: TB_MODULE_NOT_FOUND |
| 4 | Port not connected | @new omits a module port | Error: TB_PORT_NOT_CONNECTED |
| 5 | Port width mismatch | @new port width differs from module declaration | Error: TB_PORT_WIDTH_MISMATCH |
| 6 | Invalid @new RHS | @new RHS is not a testbench CLOCK or WIRE | Error: TB_NEW_RHS_INVALID |
| 7 | @setup position | @setup not after @new or appears multiple times | Error: TB_SETUP_POSITION |
| 8 | Clock not declared | @clock references undeclared CLOCK | Error: TB_CLOCK_NOT_DECLARED |
| 9 | Clock cycle not positive | @clock cycle count is zero or negative | Error: TB_CLOCK_CYCLE_NOT_POSITIVE |
| 10 | Update not wire | @update assigns to non-WIRE identifier | Error: TB_UPDATE_NOT_WIRE |
| 11 | Update clock assign | @update assigns to clock signal | Error: TB_UPDATE_CLOCK_ASSIGN |
| 12 | Expect width mismatch | @expect value width differs from signal width | Error: TB_EXPECT_WIDTH_MISMATCH |
| 13 | No test blocks | @testbench contains no TEST blocks | Error: TB_NO_TEST_BLOCKS |
| 14 | Multiple @new | TEST block contains more than one @new | Error: TB_MULTIPLE_NEW |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Multiple TEST blocks | @testbench with several TEST blocks | Valid if each has exactly one @new |
| 2 | @setup with no updates | @setup present but no @update directives | Valid (minimal test) |

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1 | @testbench file run with --lint | @testbench block | TB_WRONG_TOOL | error |
| 2 | @project + @testbench in same file | Both directives | TB_PROJECT_MIXED | error |
| 3 | @testbench references missing module | @testbench module_name | TB_MODULE_NOT_FOUND | error |
| 4 | Port omitted from @new | @new without all ports | TB_PORT_NOT_CONNECTED | error |
| 5 | Port width differs | @new port width mismatch | TB_PORT_WIDTH_MISMATCH | error |
| 6 | @new RHS not CLOCK/WIRE | @new with invalid RHS | TB_NEW_RHS_INVALID | error |
| 7 | @setup misplaced | @setup before @new or duplicated | TB_SETUP_POSITION | error |
| 8 | Undeclared clock in @clock | @clock with unknown id | TB_CLOCK_NOT_DECLARED | error |
| 9 | Zero/negative cycle count | @clock 0 | TB_CLOCK_CYCLE_NOT_POSITIVE | error |
| 10 | @update targets non-wire | @update on register/port | TB_UPDATE_NOT_WIRE | error |
| 11 | @update assigns clock | @update on clock signal | TB_UPDATE_CLOCK_ASSIGN | error |
| 12 | @expect width differs | @expect with wrong width | TB_EXPECT_WIDTH_MISMATCH | error |
| 13 | No TEST in @testbench | Empty @testbench | TB_NO_TEST_BLOCKS | error |
| 14 | Two @new in one TEST | Duplicate @new | TB_MULTIPLE_NEW | error |

## 4. Existing Validation Tests

No validation tests exist for TESTBENCH rules. These rules require `--test` mode and are not reachable via `--info --lint`.

## 5. Rules Matrix

### 5.1 Rules Tested

> **Note:** All TESTBENCH rules are tested via `--test` mode, not `--info --lint`.

| Rule ID | Severity | Test Mode | Test Case |
|---------|----------|-----------|-----------|
| TB_WRONG_TOOL | error | --lint (triggers error directing to --test) | Error Cases #1 |
| TB_PROJECT_MIXED | error | --test | Error Cases #2 |
| TB_MODULE_NOT_FOUND | error | --test | Error Cases #3 |
| TB_PORT_NOT_CONNECTED | error | --test | Error Cases #4 |
| TB_PORT_WIDTH_MISMATCH | error | --test | Error Cases #5 |
| TB_NEW_RHS_INVALID | error | --test | Error Cases #6 |
| TB_SETUP_POSITION | error | --test | Error Cases #7 |
| TB_CLOCK_NOT_DECLARED | error | --test | Error Cases #8 |
| TB_CLOCK_CYCLE_NOT_POSITIVE | error | --test | Error Cases #9 |
| TB_UPDATE_NOT_WIRE | error | --test | Error Cases #10 |
| TB_UPDATE_CLOCK_ASSIGN | error | --test | Error Cases #11 |
| TB_EXPECT_WIDTH_MISMATCH | error | --test | Error Cases #12 |
| TB_NO_TEST_BLOCKS | error | --test | Error Cases #13 |
| TB_MULTIPLE_NEW | error | --test | Error Cases #14 |

### 5.2 Rules Not Tested

All TESTBENCH rules are now covered in section 5.1. No untested rules remain.
