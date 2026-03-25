# Test Plan: 6.3 CONFIG Block

**Specification Reference:** Section 6.3 of jz-hdl-specification.md

## 1. Objective

Verify CONFIG block: numeric and string entries, project-wide visibility via `CONFIG.<name>`, compile-time-only usage restriction, type constraints (string in numeric context, numeric in string context), forward reference and circular dependency detection, runtime usage prohibition, and at-most-one CONFIG block per project.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Numeric CONFIG | `XLEN = 32;` -- simple integer value |
| 2 | String CONFIG | `FIRMWARE = "out/fw.bin";` -- string value |
| 3 | CONFIG in port width | `IN [CONFIG.XLEN] data;` -- compile-time width |
| 4 | CONFIG in MEM depth | `MEM { m [8] [CONFIG.DEPTH] ... }` -- compile-time depth |
| 5 | CONFIG referencing earlier CONFIG | `TOTAL = CONFIG.A + CONFIG.B;` -- chained references |
| 6 | CONFIG in CONST initializer | CONST using CONFIG.name value |
| 7 | String CONFIG in @file path | `@file(CONFIG.FIRMWARE)` -- string used in file context |

### 2.2 Error Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Forward reference | `A = CONFIG.B; B = 5;` -- B not yet declared |
| 2 | Circular dependency | `A = CONFIG.B; B = CONFIG.A;` -- mutual reference |
| 3 | Runtime use of CONFIG | `data <= CONFIG.XLEN;` -- forbidden in runtime expression |
| 4 | String in numeric context | `[CONFIG.FIRMWARE]` -- string used as width |
| 5 | Duplicate CONFIG name | Two entries with same name in CONFIG block |
| 6 | Undeclared CONFIG reference | `CONFIG.NONEXISTENT` -- name not in CONFIG block |
| 7 | Invalid expression type | CONFIG value that is not a valid expression |
| 8 | Multiple CONFIG blocks | More than one CONFIG block in project |
| 9 | CONST runtime use | CONST identifier used in runtime expression |
| 10 | Numeric in string context | Numeric CONFIG used where string expected (e.g. @file path) |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | CONFIG value = 0 | Valid but may not work as width |
| 2 | Single CONFIG entry | Minimal CONFIG block |
| 3 | CONFIG and CONST same name | Disjoint namespaces, no shadowing |

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|------------------|----------|
| 1 | Multiple CONFIG blocks | Two `CONFIG { }` blocks in project | CONFIG_MULTIPLE_BLOCKS | error |
| 2 | Duplicate config name | `CONFIG { A = 1; A = 2; }` | CONFIG_NAME_DUPLICATE | error |
| 3 | Invalid expression type | `CONFIG { A = -5; }` or non-integer expression | CONFIG_INVALID_EXPR_TYPE | error |
| 4 | Forward reference | `A = CONFIG.B; B = 5;` in CONFIG | CONFIG_FORWARD_REF | error |
| 5 | Undeclared CONFIG reference | `CONFIG.NONEXISTENT` in width bracket | CONFIG_USE_UNDECLARED | error |
| 6 | Circular dependency | `A = CONFIG.B; B = CONFIG.A;` | CONFIG_CIRCULAR_DEP | error |
| 7 | CONFIG in runtime expression | `data <= CONFIG.XLEN;` in ASYNC/SYNC | CONFIG_USED_WHERE_FORBIDDEN | error |
| 8 | CONST in runtime expression | `data <= MY_CONST;` in ASYNC/SYNC | CONST_USED_WHERE_FORBIDDEN | error |
| 9 | String in numeric context | `[CONFIG.FIRMWARE]` as port width | CONST_STRING_IN_NUMERIC_CONTEXT | error |
| 10 | Numeric in string context | `@file(CONFIG.DEPTH)` with numeric value | CONST_NUMERIC_IN_STRING_CONTEXT | error |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 6_3_HAPPY_PATH-config_ok.jz | -- | Valid CONFIG block with numeric and string entries (clean compile) |
| 6_3_CONFIG_CIRCULAR_DEP-circular_dependency.jz | CONFIG_CIRCULAR_DEP | Circular dependency between CONFIG entries |
| 6_3_CONFIG_FORWARD_REF-forward_reference.jz | CONFIG_FORWARD_REF | CONFIG entry references later CONFIG.<name> (forward reference) |
| 6_3_CONFIG_INVALID_EXPR_TYPE-invalid_value.jz | CONFIG_INVALID_EXPR_TYPE | CONFIG value not a nonnegative integer expression |
| 6_3_CONFIG_MULTIPLE_BLOCKS-multiple_config.jz | CONFIG_MULTIPLE_BLOCKS | More than one CONFIG block in project |
| 6_3_CONFIG_NAME_DUPLICATE-duplicate_name.jz | CONFIG_NAME_DUPLICATE | Duplicate config_id within CONFIG block |
| 6_3_CONFIG_USE_UNDECLARED-undeclared_reference.jz | CONFIG_USE_UNDECLARED | Use of CONFIG.<name> not declared in project CONFIG |
| 6_3_CONFIG_USED_WHERE_FORBIDDEN-runtime_use.jz | CONFIG_USED_WHERE_FORBIDDEN | CONFIG.<name> used outside compile-time constant expression contexts |
| 6_3_CONST_NUMERIC_IN_STRING_CONTEXT-numeric_as_string.jz | CONST_NUMERIC_IN_STRING_CONTEXT | Numeric CONST/CONFIG used in string context |
| 6_3_CONST_STRING_IN_NUMERIC_CONTEXT-string_as_number.jz | CONST_STRING_IN_NUMERIC_CONTEXT | String CONST/CONFIG value used where numeric expected |
| 6_3_CONST_USED_WHERE_FORBIDDEN-const_runtime_use.jz | CONST_USED_WHERE_FORBIDDEN | CONST identifier used outside compile-time constant expression contexts |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| CONFIG_MULTIPLE_BLOCKS | error | S6.3 More than one CONFIG block defined in project | 6_3_CONFIG_MULTIPLE_BLOCKS-multiple_config.jz |
| CONFIG_NAME_DUPLICATE | error | S6.3 Duplicate config_id within CONFIG block | 6_3_CONFIG_NAME_DUPLICATE-duplicate_name.jz |
| CONFIG_INVALID_EXPR_TYPE | error | S6.3 CONFIG value not a nonnegative integer expression | 6_3_CONFIG_INVALID_EXPR_TYPE-invalid_value.jz |
| CONFIG_FORWARD_REF | error | S6.3 CONFIG entry references later CONFIG.<name> (forward reference) | 6_3_CONFIG_FORWARD_REF-forward_reference.jz |
| CONFIG_USE_UNDECLARED | error | S6.3 Use of CONFIG.<name> not declared in project CONFIG | 6_3_CONFIG_USE_UNDECLARED-undeclared_reference.jz |
| CONFIG_CIRCULAR_DEP | error | S6.3 Circular dependency between CONFIG entries | 6_3_CONFIG_CIRCULAR_DEP-circular_dependency.jz |
| CONFIG_USED_WHERE_FORBIDDEN | error | S6.3 CONFIG.<name> used outside compile-time constant expression contexts (runtime expression) | 6_3_CONFIG_USED_WHERE_FORBIDDEN-runtime_use.jz |
| CONST_USED_WHERE_FORBIDDEN | error | S4.3/S6.3 CONST identifier used outside compile-time constant expression contexts (runtime expression) | 6_3_CONST_USED_WHERE_FORBIDDEN-const_runtime_use.jz |
| CONST_STRING_IN_NUMERIC_CONTEXT | error | S4.3/S6.3 String CONST/CONFIG value used where a numeric expression is expected | 6_3_CONST_STRING_IN_NUMERIC_CONTEXT-string_as_number.jz |
| CONST_NUMERIC_IN_STRING_CONTEXT | error | S4.3/S6.3 Numeric CONST/CONFIG value used where a string is expected (e.g. @file path) | 6_3_CONST_NUMERIC_IN_STRING_CONTEXT-numeric_as_string.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| -- | -- | All rules for this section are covered by existing tests |
