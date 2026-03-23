# Test Plan: 4.2 Scope and Uniqueness

**Specification Reference:** Section 4.2 of jz-hdl-specification.md

## 1. Objective

Verify module/blackbox name uniqueness across a project, identifier uniqueness within a module (ports, registers, wires, constants, instance names), CONST module-local scoping, and instance name uniqueness rules.

## 2. Instrumentation Strategy

- **Span: `sem.scope_check`** — Trace scope validation; attributes: `module_name`, `identifier`, `scope_level`.
- **Event: `scope.duplicate`** — Fires when duplicate identifier detected.
- **Coverage Hook:** Test all identifier types for uniqueness conflicts.

## 3. Test Scenarios

### 3.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Unique names across modules | Two modules with different names |
| 2 | Same signal name in different modules | `data` in module A and module B — valid (module-local) |
| 3 | Unique identifiers within module | Port, wire, register, const all different names |
| 4 | CONST module-local only | CONST `WIDTH` in module A, not visible in module B |
| 5 | Instance names unique | Multiple @new with different instance names |

### 3.2 Boundary/Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Module and blackbox different names | Valid: `@module foo` and `@blackbox bar` |
| 2 | CONST in parent scope for @new widths | Parent CONST used in child port width evaluation |

### 3.3 Negative Testing

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Duplicate module names | Two `@module foo` — Error |
| 2 | Module and blackbox same name | `@module foo` and `@blackbox foo` — Error |
| 3 | Duplicate signal in module | `WIRE { a [8]; a [4]; }` — Error |
| 4 | Port and wire same name | `PORT { IN [8] x; } WIRE { x [8]; }` — Error |
| 5 | Instance name matches signal | Instance `data` when `WIRE { data [8]; }` exists — Error |
| 6 | Duplicate instance names | Two `@new inst_a` — Error |
| 7 | Self-instantiation | `@module foo` containing `@new x foo {}` — Error |

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Two modules named `foo` | Error | MODULE_DUPLICATE_NAME | S4.2 |
| 2 | Wire and port named `x` | Error | SCOPE_DUPLICATE_SIGNAL | S4.2 |
| 3 | Self-instantiation | Error | MODULE_SELF_INSTANTIATION | S4.2 |
| 4 | Duplicate instance names | Error | SCOPE_DUPLICATE_SIGNAL | S4.2 |
| 5 | Same names in different modules | Valid | — | Module-local scope |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `driver.c` | Scope and uniqueness checking | Feed multi-module AST |
| `parser_module.c` | Module name registration | Integration test |
| `parser_instance.c` | Instance name validation | Integration test |
| `diagnostic.c` | Error reporting | Capture rule IDs |

## 6. Rules Matrix

### 6.1 Rules Tested

| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| MODULE_DUPLICATE_NAME | Duplicate module/blackbox name | Neg 1, 2 |
| MODULE_SELF_INSTANTIATION | Module instantiates itself | Neg 7 |
| SCOPE_DUPLICATE_SIGNAL | Duplicate identifier in module scope | Neg 3, 4, 5, 6 |

### 6.2 Rules Missing

| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| CONST_CROSS_MODULE_ACCESS | S4.2 "module-local only" | No explicit rule for accessing another module's CONST (would be undefined identifier) |
