# Test Plan: 6.10 Project Scope and Uniqueness

**Specification Reference:** Section 6.10 of jz-hdl-specification.md

## 1. Objective

Verify project-level scope rules: one project per file, module/blackbox name uniqueness across project and imports, pin name uniqueness across categories, clock name uniqueness.

## 2. Instrumentation Strategy

- **Span: `sem.project_scope`** — attributes: `module_count`, `blackbox_count`, `pin_count`, `clock_count`.

## 3. Test Scenarios

### 3.1 Happy Path
1. All names unique across project

### 3.2 Negative Testing
1. Two @project in same file — Error
2. Module name conflicts with imported module — Error
3. Pin name duplicated across IN/OUT blocks — Error

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Two @project blocks | Error | PROJECT_MULTIPLE | S6.10 |
| 2 | Imported module name conflict | Error | MODULE_DUPLICATE_NAME | S4.2/S6.10 |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `driver_project.c` | Project scope validation | Integration test |

## 6. Rules Matrix

### 6.1 Rules Tested
| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| MODULE_DUPLICATE_NAME | Cross-import name conflict | Neg 2 |

### 6.2 Rules Missing
| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| PROJECT_MULTIPLE | S6.10 | Two @project blocks in one file |
