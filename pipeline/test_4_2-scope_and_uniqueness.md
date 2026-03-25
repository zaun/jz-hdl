# Test Plan: 4.2 Scope and Uniqueness

**Specification Reference:** Section 4.2 of jz-hdl-specification.md

## 1. Objective

Verify identifier/module/blackbox/instance uniqueness. Confirm that duplicate identifiers within a module, duplicate module names across a project, duplicate blackbox names, duplicate instance names, instance-signal name collisions, undeclared identifiers, and ambiguous references are all detected and reported.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Unique names across modules | Two modules with different names |
| 2 | Same signal name in different modules | `data` in module A and module B -- valid (module-local) |
| 3 | Unique identifiers within module | Port, wire, register, const all different names |
| 4 | CONST module-local only | CONST `WIDTH` in module A, not visible in module B |
| 5 | Instance names unique | Multiple @new with different instance names |

### 2.2 Error Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Duplicate module names | Two `@module foo` in same project -- error |
| 2 | Module and blackbox same name | `@module foo` and `@blackbox foo` -- error |
| 3 | Duplicate signal in module | `WIRE { a [8]; a [4]; }` -- error |
| 4 | Port and wire same name | `PORT { IN [8] x; } WIRE { x [8]; }` -- error |
| 5 | Instance name matches signal | Instance `data` when `WIRE { data [8]; }` exists -- error |
| 6 | Duplicate instance names | Two `@new inst_a` in same module -- error |
| 7 | Undeclared identifier | Use of `undefined_name` in expression -- error |
| 8 | Ambiguous reference | Two instances with ports of the same name, accessed without qualifying instance prefix -- error |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Module and blackbox different names | Valid: `@module foo` and `@blackbox bar` |
| 2 | CONST in parent scope for @new widths | Parent CONST used in child port width evaluation |

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1 | Two modules named `foo` | `@module foo` twice in project | MODULE_NAME_DUP_IN_PROJECT | error |
| 2 | `@module foo` and `@blackbox foo` | Module and blackbox same name | BLACKBOX_NAME_DUP_IN_PROJECT | error |
| 3 | Wire and port named `x` in same module | `PORT { IN [8] x; } WIRE { x [8]; }` | ID_DUP_IN_MODULE | error |
| 4 | Two `@new inst_a` in same module | Duplicate instance names | INSTANCE_NAME_DUP_IN_MODULE | error |
| 5 | Instance name matches wire name | `@new data mod` when `WIRE { data [8]; }` exists | INSTANCE_NAME_CONFLICT | error |
| 6 | Use of undeclared identifier | `wire <= undefined_name;` | UNDECLARED_IDENTIFIER | error |
| 7 | Ambiguous identifier reference | Identifier matches multiple scopes without qualifier | AMBIGUOUS_REFERENCE | error |
| 8 | Same names in different modules | `data` in module A and module B | -- | -- (pass) |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 4_2_HAPPY_PATH-scope_uniqueness_ok.jz | -- | Happy path: unique names within module and across project |
| 4_2_BLACKBOX_NAME_DUP_IN_PROJECT-blackbox_name_conflicts.jz | BLACKBOX_NAME_DUP_IN_PROJECT | Blackbox name conflicts with existing module or blackbox name |
| 4_2_ID_DUP_IN_MODULE-duplicate_identifiers.jz | ID_DUP_IN_MODULE | Duplicate identifier within module scope (ports, wires, registers, consts) |
| 4_2_INSTANCE_NAME_CONFLICT-instance_signal_collision.jz | INSTANCE_NAME_CONFLICT | Instance name matches another identifier in parent module |
| 4_2_INSTANCE_NAME_DUP_IN_MODULE-duplicate_instance_names.jz | INSTANCE_NAME_DUP_IN_MODULE | Multiple instances with same name in parent module |
| 4_2_MODULE_NAME_DUP_IN_PROJECT-duplicate_module_names.jz | MODULE_NAME_DUP_IN_PROJECT | Module name not unique across project |
| 4_2_UNDECLARED_IDENTIFIER-undeclared_use.jz | UNDECLARED_IDENTIFIER | Use of undeclared identifier in expression |
| 4_2_AMBIGUOUS_REFERENCE-ambiguous_without_prefix.jz | AMBIGUOUS_REFERENCE | Two instances with same-named ports, accessed without qualifying instance prefix |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| BLACKBOX_NAME_DUP_IN_PROJECT | error | S6.7/S6.10 Blackbox name conflicts with existing module/blackbox name | 4_2_BLACKBOX_NAME_DUP_IN_PROJECT-blackbox_name_conflicts.jz |
| ID_DUP_IN_MODULE | error | S4.2/S8.1 Duplicate identifier within module (ports, wires, registers, consts, instances) | 4_2_ID_DUP_IN_MODULE-duplicate_identifiers.jz |
| INSTANCE_NAME_CONFLICT | error | S4.2/S8.1 Instance name matches another identifier (port/wire/register/CONST) in parent module | 4_2_INSTANCE_NAME_CONFLICT-instance_signal_collision.jz |
| INSTANCE_NAME_DUP_IN_MODULE | error | S4.2/S8.1 Multiple instances with same name in parent module | 4_2_INSTANCE_NAME_DUP_IN_MODULE-duplicate_instance_names.jz |
| MODULE_NAME_DUP_IN_PROJECT | error | S4.2/S6.10/S8.1 Module name not unique across project | 4_2_MODULE_NAME_DUP_IN_PROJECT-duplicate_module_names.jz |
| UNDECLARED_IDENTIFIER | error | S4.2/S8.1 Use of undeclared identifier | 4_2_UNDECLARED_IDENTIFIER-undeclared_use.jz |
| AMBIGUOUS_REFERENCE | error | S4.2/S8.1 Identifier reference ambiguous without instance prefix | 4_2_AMBIGUOUS_REFERENCE-ambiguous_without_prefix.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| -- | -- | All rules covered |
