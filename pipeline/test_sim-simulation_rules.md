# Test Plan: Simulation Rules (SIMULATION group)
**Specification Reference:** Simulation validation rules from `compiler/src/rules.c`

## 1. Objective

Verify all SIMULATION diagnostic rules are correctly defined and documented. SIM_WRONG_TOOL and SIM_PROJECT_MIXED may be detectable via `--lint` (when a file contains @simulation blocks). SIM_RUN_COND_TIMEOUT is a runtime condition only reachable via `--simulate`.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Valid simulation file | Well-formed @simulation block run with --simulate | Clean run |

### 2.2 Error Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Wrong tool | File with @simulation run via --lint | Error: SIM_WRONG_TOOL |
| 2 | Project mixed | File with both @project and @simulation | Error: SIM_PROJECT_MIXED |
| 3 | Run condition timeout | @run_until/@run_while condition not met within timeout | Error: SIM_RUN_COND_TIMEOUT |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | @simulation with no run directives | @simulation block with no @run_until or @run_while | Depends on compiler requirements |

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1 | @simulation file run with --lint | @simulation block | SIM_WRONG_TOOL | error |
| 2 | @project + @simulation in same file | Both directives | SIM_PROJECT_MIXED | error |
| 3 | Run condition times out | @run_until with unmet condition | SIM_RUN_COND_TIMEOUT | error |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| sim_SIM_WRONG_TOOL-simulation_with_lint.jz | SIM_WRONG_TOOL | File with @simulation block run via --lint triggers wrong-tool error |
| sim_SIM_PROJECT_MIXED-project_and_simulation.jz | SIM_PROJECT_MIXED | File containing both @project and @simulation triggers mixed-file error |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| SIM_WRONG_TOOL | error | File contains @simulation blocks; use --simulate to run simulations | sim_SIM_WRONG_TOOL-simulation_with_lint.jz |
| SIM_PROJECT_MIXED | error | SIM-020 A file may not contain both @project and @simulation | sim_SIM_PROJECT_MIXED-project_and_simulation.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| SIM_RUN_COND_TIMEOUT | error | Fires at simulation runtime, not reachable via --lint or --test |
