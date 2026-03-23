# Test Plan: 7.10 CONST Evaluation in MEM

**Specification Reference:** Section 7.10 of jz-hdl-specification.md

## 1. Objective

Verify CONST and CONFIG usage in MEM dimensions (width, depth), including address width derivation from depth via clog2, and error detection for undefined or invalid constant values.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | CONST width | `MEM { m [WIDTH] [DEPTH] ... }` with CONST WIDTH, DEPTH | Valid, dimensions resolved at compile time |
| 2 | CONFIG depth | `MEM { m [8] [CONFIG.DEPTH] ... }` | Valid, depth from CONFIG block |
| 3 | Address width = clog2(depth) | Depth=256, address width auto-derived as 8 | Valid, port address widths correct |
| 4 | Power-of-2 depth | CONST DEPTH = 1024 | Valid, address width = 10 |

### 2.2 Error Cases

| # | Test Case | Input | Expected | Rule ID |
|---|-----------|-------|----------|---------|
| 1 | Negative CONST for depth | `CONST DEPTH = -1;` used in MEM | Error | CONST_NEGATIVE_OR_NONINT |
| 2 | Undefined CONST in MEM width | `MEM { m [UNDEF] [16] ... }` | Error | MEM_UNDEFINED_CONST_IN_WIDTH |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | CONST depth = 1 | Single-element MEM, address width = 0 or 1 | Valid, minimal memory |
| 2 | Non-power-of-2 depth | CONST DEPTH = 7, address width = clog2(7) = 3 | Valid, address width rounds up |
| 3 | CONST expression in dimension | `CONST SIZE = 4 * 8;` used as width | Valid, evaluated to 32 |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| 1 | Negative CONST value for depth | Error | CONST_NEGATIVE_OR_NONINT | error | S7.10: dimensions must be positive |
| 2 | Undefined CONST in MEM width | Error | MEM_UNDEFINED_CONST_IN_WIDTH | error | S7.10: CONST must resolve |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 7_10_CONST_NEGATIVE_OR_NONINT-negative_const_mem_depth.jz | CONST_NEGATIVE_OR_NONINT | Negative CONST used as MEM depth |
| 7_10_MEM_UNDEFINED_CONST_IN_WIDTH-undefined_const_in_mem.jz | MEM_UNDEFINED_CONST_IN_WIDTH | Undefined CONST reference in MEM width |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| CONST_NEGATIVE_OR_NONINT | error | S7.10 Negative or non-integer CONST used in MEM dimension | 7_10_CONST_NEGATIVE_OR_NONINT-negative_const_mem_depth.jz |
| MEM_UNDEFINED_CONST_IN_WIDTH | error | S7.10 Undefined CONST referenced in MEM width/depth | 7_10_MEM_UNDEFINED_CONST_IN_WIDTH-undefined_const_in_mem.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| — | — | All expected rules covered |
