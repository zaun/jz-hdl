# Test Plan: 6.7 Blackbox (Opaque) Modules

**Specification Reference:** Section 6.7 of jz-hdl-specification.md

## 1. Objective

Verify @blackbox declaration within @project, PORT-only interface (no internal logic visible), instantiation via @new, OVERRIDE passthrough semantics, name uniqueness with regular modules, and undefined blackbox reference detection.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Simple blackbox | `@blackbox uart { PORT { IN [1] clk; OUT [8] data; } }` |
| 2 | Blackbox instantiated via @new | Module uses `@new inst bb_name { ... }` to instantiate a blackbox |
| 3 | Multiple blackboxes in project | Two or more @blackbox declarations coexisting |
| 4 | Blackbox with OVERRIDE | `@new inst bb { OVERRIDE { FREQ = 125; } ... }` passes through unchecked |
| 5 | Blackbox with INOUT ports | Bidirectional interface on blackbox |

### 2.2 Error Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Blackbox with logic body | ASYNCHRONOUS/SYNCHRONOUS/WIRE/REGISTER/MEM/CONST inside @blackbox |
| 2 | Blackbox name conflicts with module | @blackbox and @module share same name |
| 3 | @new references undefined blackbox | Instantiation targets a blackbox name that does not exist |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Blackbox with only IN ports | No OUT or INOUT ports declared |
| 2 | Blackbox with only OUT ports | No IN ports declared |
| 3 | OVERRIDE with many parameters | Large OVERRIDE block passed through without validation |

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|------------------|----------|
| 1 | Blackbox with forbidden block | CONST/ASYNC/SYNC/WIRE/REGISTER/MEM inside @blackbox | BLACKBOX_BODY_DISALLOWED | error |
| 2 | Blackbox name = module name | `@blackbox foo` and `@module foo` in same project | BLACKBOX_NAME_DUP_IN_PROJECT | error |
| 3 | @new references undefined blackbox | `@new inst nonexistent { ... }` | BLACKBOX_UNDEFINED_IN_NEW | error |
| 4 | OVERRIDE in blackbox @new | `@new inst bb { OVERRIDE { ... } ... }` | BLACKBOX_OVERRIDE_UNCHECKED | info |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 6_7_HAPPY_PATH-blackbox_ok.jz | -- | Valid blackbox declaration and instantiation (clean compile) |
| 6_7_BLACKBOX_BODY_DISALLOWED-const_in_blackbox.jz | BLACKBOX_BODY_DISALLOWED | Blackbox contains forbidden blocks |
| 6_7_BLACKBOX_NAME_DUP_IN_PROJECT-blackbox_name_conflicts.jz | BLACKBOX_NAME_DUP_IN_PROJECT | @blackbox name conflicts with @module or another @blackbox |
| 6_7_BLACKBOX_OVERRIDE_UNCHECKED-override_passthrough.jz | BLACKBOX_OVERRIDE_UNCHECKED | OVERRIDE in blackbox instantiation is passed through |
| 6_7_BLACKBOX_UNDEFINED_IN_NEW-undefined_blackbox.jz | BLACKBOX_UNDEFINED_IN_NEW | @new instantiation targets a blackbox name that does not exist |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| BLACKBOX_BODY_DISALLOWED | error | S6.7 Blackbox contains forbidden blocks (ASYNCHRONOUS/SYNCHRONOUS/WIRE/REGISTER/MEM/CONST) | 6_7_BLACKBOX_BODY_DISALLOWED-const_in_blackbox.jz |
| BLACKBOX_NAME_DUP_IN_PROJECT | error | S6.7/S6.10 @blackbox name conflicts with @module or another @blackbox | 6_7_BLACKBOX_NAME_DUP_IN_PROJECT-blackbox_name_conflicts.jz |
| BLACKBOX_OVERRIDE_UNCHECKED | info | S6.7/S4.13 OVERRIDE in blackbox instantiation is not validated and is passed through to vendor IP | 6_7_BLACKBOX_OVERRIDE_UNCHECKED-override_passthrough.jz |
| BLACKBOX_UNDEFINED_IN_NEW | error | S6.7/S6.9 @new instantiation targets a blackbox name that does not exist | 6_7_BLACKBOX_UNDEFINED_IN_NEW-undefined_blackbox.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| -- | -- | All rules for this section are covered by existing tests |
