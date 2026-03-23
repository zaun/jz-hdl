# Test Plan: 12.2 Combinational Loop Errors

**Specification Reference:** Section 12.2 of jz-hdl-specification.md

## 1. Objective

Verify combinational loop detection: unconditional cycles (e.g., `a=b; b=a;`), flow-sensitive exclusion (IF/ELSE branches do not create cycles), multi-signal cycles, and the conditional-safe warning for cycles only within mutually exclusive branches.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Flow-sensitive no loop | `IF(sel) { a=b; } ELSE { b=a; }` | Valid (mutually exclusive branches) |
| 2 | Linear chain | `a=b; c=a;` | No cycle |
| 3 | Complex valid graph | Multi-signal dependency graph with no cycles | No errors |

### 2.2 Error Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Direct two-signal cycle | `a = b; b = a;` | Error: COMB_LOOP_UNCONDITIONAL |
| 2 | Three-signal cycle | `a = b; b = c; c = a;` | Error: COMB_LOOP_UNCONDITIONAL |
| 3 | Cycle through instance port | Instance OUT drives its own IN | Error: COMB_LOOP_UNCONDITIONAL |
| 4 | Conditional cycle on same path | `a = b; IF(c) { b = a; }` | Error: COMB_LOOP_UNCONDITIONAL |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Self-assignment through expression | `a = a + 1'b0;` | May be flagged as cycle |
| 2 | Long chain (10+ signals) no cycle | Deep linear dependency | No error |
| 3 | Conditional-safe cycle | Cycles only in mutually exclusive branches | Warning: COMB_LOOP_CONDITIONAL_SAFE |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| 1 | `a=b; b=a;` | Error | COMB_LOOP_UNCONDITIONAL | error | S5.3/S8.2 |
| 2 | Three-signal cycle | Error | COMB_LOOP_UNCONDITIONAL | error | S5.3/S8.2 |
| 3 | `IF(s){a=b;}ELSE{b=a;}` | Valid | — | — | Flow-sensitive exclusion |
| 4 | Mutually exclusive branch cycles | Warning | COMB_LOOP_CONDITIONAL_SAFE | warning | S5.3/S8.2 |

## 4. Existing Validation Tests

Combinational loop tests are covered by Section 5.3 test files. See test plan `test_5_3-conditional_statements.md` for specific validation test coverage.

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| COMB_LOOP_UNCONDITIONAL | error | Combinational loop: signal feeds back through ASYNCHRONOUS assignments | Error 1, 2, 3, 4 |
| COMB_LOOP_CONDITIONAL_SAFE | warning | Cycles only within mutually exclusive branches considered safe | Edge 3 |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| — | — | All combinational loop rules are tested |
