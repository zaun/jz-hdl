# Test Plan: 7.10 CONST Evaluation in MEM

**Specification Reference:** Section 7.10 of jz-hdl-specification.md

## 1. Objective

Verify CONST and CONFIG usage in MEM dimensions (width, depth), including address width derivation from depth via clog2, string CONST usage in @file paths, and error detection for undefined, negative, circular, or otherwise invalid constant values.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | CONST width and depth | `MEM { m [WIDTH] [DEPTH] ... }` with CONST WIDTH, DEPTH | Valid, dimensions resolved at compile time |
| 2 | CONST expression in dimension | `CONST { SIZE = 4 * 8; }` used as width | Valid, evaluated to 32 |
| 3 | CONST depth with address width auto-derived | CONST DEPTH = 256, address width = 8 | Valid |
| 4 | String CONST in @file path | `CONST { INIT_FILE = "firmware.bin"; }` + `@file(INIT_FILE)` | Valid |
| 5 | Power-of-2 CONST depth | CONST DEPTH = 1024 | Valid, address width = 10 |
| 6 | CONST multiplication in depth | `MEM { m [WIDTH] [256 * MULTIPLIER] ... }` | Valid |

### 2.2 Error Cases

| # | Test Case | Input | Expected | Rule ID |
|---|-----------|-------|----------|---------|
| 1 | Negative CONST for depth | `CONST { DEPTH = -1; }` used in MEM | Error | CONST_NEGATIVE_OR_NONINT |
| 2 | Undefined CONST in MEM width | `MEM { m [UNDEF] [16] ... }` | Error | MEM_UNDEFINED_CONST_IN_WIDTH |
| 3 | Circular CONST dependency | `CONST { A = B; B = A; }` used in MEM | Error | CONST_CIRCULAR_DEP |
| 4 | CONST used in width not declared | `MEM { m [UNDEF_CONST] [256] ... }` | Error | CONST_UNDEFINED_IN_WIDTH_OR_SLICE |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | CONST depth = 1 | Single-element MEM, address width = 1 bit | Valid, minimal memory |
| 2 | Non-power-of-2 CONST depth | CONST DEPTH = 7, address width = clog2(7) = 3 | Valid, address width rounds up |
| 3 | CONST zero value for depth | CONST DEPTH = 0 used in MEM depth | Error: MEM_INVALID_DEPTH |
| 4 | Large CONST expression | CONST DEPTH = 256 * 256 = 65536 | Valid |

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1 | Negative CONST value for depth | `CONST { DEPTH = -1; }` in MEM depth | CONST_NEGATIVE_OR_NONINT | error |
| 2 | Undefined CONST in MEM width | `MEM { m [UNDEF] [16] ... }` | MEM_UNDEFINED_CONST_IN_WIDTH | error |
| 3 | Circular CONST dependency | `CONST { A = B; B = A; }` used in MEM | CONST_CIRCULAR_DEP | error |
| 4 | CONST in width/slice not declared | `MEM { m [UNDEF_CONST] [256] ... }` | CONST_UNDEFINED_IN_WIDTH_OR_SLICE | error |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 7_10_HAPPY_PATH-const_evaluation_in_mem_ok.jz | — | Happy path: CONST-parameterized MEM dimensions |
| 7_10_CONST_NEGATIVE_OR_NONINT-negative_const_mem_depth.jz | CONST_NEGATIVE_OR_NONINT | Negative CONST used as MEM depth |
| 7_10_MEM_UNDEFINED_CONST_IN_WIDTH-undefined_const_in_mem.jz | MEM_UNDEFINED_CONST_IN_WIDTH | Undefined CONST reference in MEM width |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| CONST_NEGATIVE_OR_NONINT | error | S4.3/S7.10 Negative or non-integer CONST used in MEM dimension | 7_10_CONST_NEGATIVE_OR_NONINT-negative_const_mem_depth.jz |
| MEM_UNDEFINED_CONST_IN_WIDTH | error | S7.1/S7.7.1 Undefined CONST referenced in MEM width/depth | 7_10_MEM_UNDEFINED_CONST_IN_WIDTH-undefined_const_in_mem.jz |
| CONST_UNDEFINED_IN_WIDTH_OR_SLICE | error | S1.3/S2.1/S7.10 CONST used in width/slice not declared or evaluates invalidly | 1_3_CONST_UNDEFINED_IN_WIDTH_OR_SLICE-non_const_in_slice.jz, 4_3_CONST_UNDEFINED_IN_WIDTH_OR_SLICE-undefined_const.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| CONST_CIRCULAR_DEP | error | Bug: test exists (`4_3_CONST_CIRCULAR_DEP-circular_dependency.jz`) but rule has a known compiler bug |
