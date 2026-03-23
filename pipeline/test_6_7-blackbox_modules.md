# Test Plan: 6.7 Blackbox (Opaque) Modules

**Specification Reference:** Section 6.7 of jz-hdl-specification.md

## 1. Objective

Verify @blackbox declaration within @project, PORT-only interface (no internal logic visible), instantiation via @new, OVERRIDE passthrough semantics, and name uniqueness with regular modules.

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
| 1 | Blackbox with logic body | ASYNCHRONOUS/SYNCHRONOUS/WIRE/REGISTER/MEM inside @blackbox -- Error |
| 2 | Blackbox name conflicts with module | @blackbox and @module share same name -- Error |
| 3 | @new references undefined blackbox | Instantiation targets a blackbox name that does not exist -- Error |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Blackbox with only IN ports | No OUT or INOUT ports declared |
| 2 | Blackbox with only OUT ports | No IN ports declared |
| 3 | OVERRIDE with many parameters | Large OVERRIDE block passed through without validation |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|-----------------|---------|----------|-------|
| 1 | Blackbox with ASYNC block | Error | BLACKBOX_BODY_DISALLOWED | error | S6.7 |
| 2 | Blackbox name = module name | Error | BLACKBOX_NAME_DUP_IN_PROJECT | error | S6.7/S6.10 |
| 3 | @new references undefined blackbox | Error | BLACKBOX_UNDEFINED_IN_NEW | error | S6.7/S6.9 |
| 4 | OVERRIDE in blackbox @new | Info | BLACKBOX_OVERRIDE_UNCHECKED | info | S6.7/S4.13 |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 6_7_BLACKBOX_BODY_DISALLOWED-const_in_blackbox.jz | BLACKBOX_BODY_DISALLOWED | Blackbox contains forbidden blocks |
| 6_7_BLACKBOX_OVERRIDE_UNCHECKED-override_passthrough.jz | BLACKBOX_OVERRIDE_UNCHECKED | OVERRIDE in blackbox instantiation is passed through |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| BLACKBOX_BODY_DISALLOWED | error | S6.7 Blackbox contains forbidden blocks (ASYNCHRONOUS/SYNCHRONOUS/WIRE/REGISTER/MEM) | 6_7_BLACKBOX_BODY_DISALLOWED-const_in_blackbox.jz |
| BLACKBOX_OVERRIDE_UNCHECKED | info | S6.7/S4.13 OVERRIDE in blackbox instantiation is not validated and is passed through to vendor IP | 6_7_BLACKBOX_OVERRIDE_UNCHECKED-override_passthrough.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| BLACKBOX_UNDEFINED_IN_NEW | error | No dedicated validation test file exists |
| BLACKBOX_NAME_DUP_IN_PROJECT | error | No dedicated validation test file exists |
