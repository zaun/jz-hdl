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
| 1 | Two @project in same file | Multiple @project directives in one source file -- Error |
| 2 | Duplicate project name | Project name conflicts with another project or module name -- Error |
| 3 | Duplicate module name in project | Two modules with same name across project and imports -- Error |
| 4 | Blackbox name duplicates module | @blackbox name conflicts with @module or another @blackbox -- Error |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Imported module name conflict | Module name from @import collides with local module name |
| 2 | Pin name duplicated across blocks | Same pin name in IN_PINS and OUT_PINS |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|-----------------|---------|----------|-------|
| 1 | Two @project blocks | Error | PROJECT_MULTIPLE_PER_FILE | error | S6.2/S6.9 |
| 2 | Duplicate project name | Error | PROJECT_NAME_NOT_UNIQUE | error | S6.10 |
| 3 | Duplicate module name | Error | MODULE_NAME_DUP_IN_PROJECT | error | S4.2/S6.10/S8.1 |
| 4 | Blackbox name = module name | Error | BLACKBOX_NAME_DUP_IN_PROJECT | error | S6.7/S6.10 |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 6_10_PROJECT_MULTIPLE_PER_FILE-two_projects_in_file.jz | PROJECT_MULTIPLE_PER_FILE | Multiple @project directives in same file |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| PROJECT_MULTIPLE_PER_FILE | error | S6.2/S6.9 Multiple @project directives in same file | 6_10_PROJECT_MULTIPLE_PER_FILE-two_projects_in_file.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| PROJECT_NAME_NOT_UNIQUE | error | No dedicated validation test file exists |
| MODULE_NAME_DUP_IN_PROJECT | error | No dedicated validation test file exists |
| BLACKBOX_NAME_DUP_IN_PROJECT | error | No dedicated validation test file exists |
