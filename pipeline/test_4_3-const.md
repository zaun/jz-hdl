# Test Plan: 4.3 CONST (Compile-Time Constants)

**Specification Reference:** Section 4.3 of jz-hdl-specification.md

## 1. Objective

Verify CONST declarations, compile-time evaluation, string/numeric types. Confirm that negative/non-integer CONST values are rejected, undefined CONSTs in width/slice contexts are detected, circular dependencies are caught, CONSTs in forbidden runtime contexts are flagged, and type mismatches (string in numeric context, numeric in string context) are reported.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Numeric CONST | `CONST { WIDTH = 32; }` -- used in port width |
| 2 | String CONST | `CONST { FILE = "data.hex"; }` -- used in @file |
| 3 | Multiple CONSTs | `WIDTH = 32; DEPTH = 256;` -- multiple declarations |
| 4 | CONST in width | `PORT { IN [WIDTH] data; }` -- valid |
| 5 | CONST arithmetic | `DEPTH = 2; ADDR = clog2(DEPTH);` -- expressions |
| 6 | CONST in MEM depth | `MEM { m [8] [DEPTH] = ... }` |
| 7 | CONST referencing CONST | `A = 8; B = A * 2;` -- valid chain |

### 2.2 Error Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Negative CONST value | `WIDTH = -1;` used as width -- error |
| 2 | String CONST as width | `PORT { IN [FILE] data; }` where FILE is a string -- error |
| 3 | Numeric CONST as file path | `@file(WIDTH)` where WIDTH is numeric -- error |
| 4 | CONST in runtime expression | CONST identifier used in forbidden runtime context -- error |
| 5 | Circular dependency | `A = B; B = A;` -- error |
| 6 | Undefined CONST in width | `PORT { IN [UNDEF] data; }` -- error |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | CONST = 0 | `ZERO = 0;` -- valid value but cannot be used as width |
| 2 | Large CONST | `BIG = 65536;` -- large constant |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|-----------------|---------|----------|-------|
| 1 | `WIDTH = -1` used as port width | Error: negative or non-integer value | CONST_NEGATIVE_OR_NONINT | error | S4.3/S7.10 |
| 2 | `PORT { IN [FILE] data; }` (FILE is string) | Error: string in numeric context | CONST_STRING_IN_NUMERIC_CONTEXT | error | S4.3/S6.3 |
| 3 | `@file(WIDTH)` (WIDTH is numeric) | Error: numeric in string context | CONST_NUMERIC_IN_STRING_CONTEXT | error | S4.3/S6.3 |
| 4 | CONST used in runtime expression | Error: const in forbidden context | CONST_USED_WHERE_FORBIDDEN | error | S4.3/S6.3 |
| 5 | `A = B; B = A;` circular | Error: circular dependency | CONST_CIRCULAR_DEP | error | S4.3/S7.10 |
| 6 | `PORT { IN [UNDEF] data; }` | Error: undefined CONST in width | CONST_UNDEFINED_IN_WIDTH_OR_SLICE | error | S1.3/S2.1/S7.10 |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 4_3_CONST_NEGATIVE_OR_NONINT-negative_const_value.jz | CONST_NEGATIVE_OR_NONINT | CONST initialized with negative or non-integer value |
| 4_3_CONST_NUMERIC_IN_STRING_CONTEXT-numeric_as_file_path.jz | CONST_NUMERIC_IN_STRING_CONTEXT | Numeric CONST used where a string is expected |
| 4_3_CONST_STRING_IN_NUMERIC_CONTEXT-string_as_width.jz | CONST_STRING_IN_NUMERIC_CONTEXT | String CONST used where a numeric expression is expected |
| 4_3_CONST_USED_WHERE_FORBIDDEN-const_in_runtime_expr.jz | CONST_USED_WHERE_FORBIDDEN | CONST identifier used outside compile-time constant expression contexts |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| CONST_NEGATIVE_OR_NONINT | error | S4.3/S7.10 CONST initialized with negative or non-integer value where nonnegative integer required | 4_3_CONST_NEGATIVE_OR_NONINT-negative_const_value.jz |
| CONST_STRING_IN_NUMERIC_CONTEXT | error | S4.3/S6.3 String CONST/CONFIG value used where a numeric expression is expected | 4_3_CONST_STRING_IN_NUMERIC_CONTEXT-string_as_width.jz |
| CONST_NUMERIC_IN_STRING_CONTEXT | error | S4.3/S6.3 Numeric CONST/CONFIG value used where a string is expected (e.g. @file path) | 4_3_CONST_NUMERIC_IN_STRING_CONTEXT-numeric_as_file_path.jz |
| CONST_USED_WHERE_FORBIDDEN | error | S4.3/S6.3 CONST identifier used outside compile-time constant expression contexts (runtime expression) | 4_3_CONST_USED_WHERE_FORBIDDEN-const_in_runtime_expr.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| CONST_CIRCULAR_DEP | error | No dedicated 4_3 test file; circular dependency detection not yet covered by validation tests |
| CONST_UNDEFINED_IN_WIDTH_OR_SLICE | error | No dedicated 4_3 test file; undefined CONST in width/slice not yet covered by validation tests |
