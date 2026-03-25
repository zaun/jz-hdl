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

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1 | Negative or non-integer CONST | `WIDTH = -1;` used as port width | CONST_NEGATIVE_OR_NONINT | error |
| 2 | String CONST in numeric context | `PORT { IN [FILE] data; }` (FILE is string) | CONST_STRING_IN_NUMERIC_CONTEXT | error |
| 3 | Numeric CONST in string context | `@file(WIDTH)` (WIDTH is numeric) | CONST_NUMERIC_IN_STRING_CONTEXT | error |
| 4 | CONST in runtime expression | CONST used in forbidden runtime context | CONST_USED_WHERE_FORBIDDEN | error |
| 5 | Circular CONST dependency | `A = B; B = A;` | CONST_CIRCULAR_DEP | error |
| 6 | Undefined CONST in width/slice | `PORT { IN [UNDEF] data; }` | CONST_UNDEFINED_IN_WIDTH_OR_SLICE | error |
| 7 | Valid CONST usage | `WIDTH = 32; PORT { IN [WIDTH] d; }` | -- | -- (pass) |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 4_3_CONST_HAPPY_PATH-valid_const_ok.jz | -- | Happy path: valid numeric and string CONST usage |
| 4_3_CONST_NEGATIVE_OR_NONINT-negative_const_value.jz | CONST_NEGATIVE_OR_NONINT | CONST initialized with negative or non-integer value |
| 4_3_CONST_NUMERIC_IN_STRING_CONTEXT-numeric_as_file_path.jz | CONST_NUMERIC_IN_STRING_CONTEXT | Numeric CONST used where a string is expected |
| 4_3_CONST_STRING_IN_NUMERIC_CONTEXT-string_as_width.jz | CONST_STRING_IN_NUMERIC_CONTEXT | String CONST used where a numeric expression is expected |
| 4_3_CONST_USED_WHERE_FORBIDDEN-const_in_runtime_expr.jz | CONST_USED_WHERE_FORBIDDEN | CONST identifier used outside compile-time constant expression contexts |
| 4_3_CONST_CIRCULAR_DEP-circular_dependency.jz | CONST_CIRCULAR_DEP | Circular dependency in CONST definitions |
| 4_3_CONST_UNDEFINED_IN_WIDTH_OR_SLICE-undefined_const.jz | CONST_UNDEFINED_IN_WIDTH_OR_SLICE | Undefined CONST used in width or slice expression |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| CONST_CIRCULAR_DEP | error | S4.3/S7.10 Circular dependency in CONST definitions | 4_3_CONST_CIRCULAR_DEP-circular_dependency.jz |
| CONST_NEGATIVE_OR_NONINT | error | S4.3/S7.10 CONST initialized with negative or non-integer value where nonnegative integer required | 4_3_CONST_NEGATIVE_OR_NONINT-negative_const_value.jz |
| CONST_UNDEFINED_IN_WIDTH_OR_SLICE | error | S1.3/S2.1/S7.10 CONST used in width/slice not declared or evaluates invalidly | 4_3_CONST_UNDEFINED_IN_WIDTH_OR_SLICE-undefined_const.jz |

### 5.2 Rules Not Tested

All rules for this section are tested.
