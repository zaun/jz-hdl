# Test Plan: 6.10 Project Scope and Uniqueness

**Specification Reference:** Section 6.10 of jz-hdl-specification.md

## 1. Objective

Verify project-level scope rules: one project per file, project name uniqueness, module name uniqueness across the project (including imports), and blackbox name uniqueness with modules.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | All names unique | Single project, unique module names, unique blackbox names |
| 2 | Module and blackbox distinct names | No name collisions across modules and blackboxes |

### 2.2 Error Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Two @project in same file | Multiple @project directives in one source file |
| 2 | Duplicate project name | Project name conflicts with another project or module name |
| 3 | Duplicate module name in project | Two modules with same name across project and imports |
| 4 | Blackbox name duplicates module | @blackbox name conflicts with @module or another @blackbox |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Imported module name conflict | Module name from @import collides with local module name |
| 2 | Pin name duplicated across blocks | Same pin name in IN_PINS and OUT_PINS (tested in 6.5) |

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|------------------|----------|
| 1 | Two @project blocks | `@project a ... @endproj @project b ... @endproj` | PROJECT_MULTIPLE_PER_FILE | error |
| 2 | Duplicate project name | Project name = module name in same file | PROJECT_NAME_NOT_UNIQUE | error |
| 3 | Duplicate module name | Two @module with same name in project scope | MODULE_NAME_DUP_IN_PROJECT | error |
| 4 | Blackbox name = module name | `@blackbox foo` and `@module foo` in project | BLACKBOX_NAME_DUP_IN_PROJECT | error |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 6_10_HAPPY_PATH-project_scope_ok.jz | -- | Valid project with unique module and blackbox names (clean compile) |
| 6_10_BLACKBOX_NAME_DUP_IN_PROJECT-blackbox_name_conflicts.jz | BLACKBOX_NAME_DUP_IN_PROJECT | @blackbox name conflicts with @module or another @blackbox |
| 6_10_MODULE_NAME_DUP_IN_PROJECT-duplicate_module_names.jz | MODULE_NAME_DUP_IN_PROJECT | Two modules with same name in project |
| 6_10_PROJECT_MULTIPLE_PER_FILE-two_projects_in_file.jz | PROJECT_MULTIPLE_PER_FILE | Multiple @project directives in same file |
| 6_10_PROJECT_NAME_NOT_UNIQUE-project_name_conflict.jz | PROJECT_NAME_NOT_UNIQUE | Project name conflicts with module name |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| BLACKBOX_NAME_DUP_IN_PROJECT | error | S6.7/S6.10 @blackbox name conflicts with @module or another @blackbox in project | 6_10_BLACKBOX_NAME_DUP_IN_PROJECT-blackbox_name_conflicts.jz |
| MODULE_NAME_DUP_IN_PROJECT | error | S4.2/S6.10/S8.1 Two modules with same name across project and imports | 6_10_MODULE_NAME_DUP_IN_PROJECT-duplicate_module_names.jz |
| PROJECT_MULTIPLE_PER_FILE | error | S6.2/S6.9 Multiple @project directives in same file | 6_10_PROJECT_MULTIPLE_PER_FILE-two_projects_in_file.jz |
| PROJECT_NAME_NOT_UNIQUE | error | S6.10 Project name conflicts with another project or module name | 6_10_PROJECT_NAME_NOT_UNIQUE-project_name_conflict.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| -- | -- | All rules for this section are covered by existing tests |
