# Test Plan: 6.2 Project Canonical Form

**Specification Reference:** Section 6.2 of jz-hdl-specification.md

## 1. Objective

Verify project structure (canonical ordering), @import directive rules (placement, semantics, de-duplication, path normalization, nested imports), imported file constraints (no @project in imported files), @endproj requirement, and single-project-per-file constraint.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Full project | Project with all sections in canonical order |
| 2 | Two module imports | @import two files each containing modules |
| 3 | Blackbox import | @import file with @blackbox definitions |
| 4 | Relative path import | @import using relative path |
| 5 | Multiple imports before CONFIG | Several @imports before CONFIG block |
| 6 | Zero imports | Project with no @import directives -- valid |

### 2.2 Error Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Import outside project | @import at file scope, not inside @project |
| 2 | Import after CONFIG | @import appears after CONFIG block |
| 3 | Imported file has @project | Imported file contains its own @project/@endproj |
| 4 | Duplicate module name across imports | Two imported files define same module/blackbox name |
| 5 | Same file imported twice | Duplicate @import of identical path |
| 6 | Multiple @project per file | More than one @project directive in same file |
| 7 | Missing @endproj | @project block without closing @endproj |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Import file with zero modules | Valid but no definitions contributed |
| 2 | Duplicate import via symlink | Same canonical path imported twice |

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|------------------|----------|
| 1 | @import at file scope | `@import "lib.jz";` outside @project | IMPORT_OUTSIDE_PROJECT | error |
| 2 | @import after CONFIG | `CONFIG { ... } @import "lib.jz";` | IMPORT_NOT_AT_PROJECT_TOP | error |
| 3 | Imported file has @project | Imported file contains `@project ... @endproj` | IMPORT_FILE_HAS_PROJECT | error |
| 4 | Duplicate module/blackbox name | Two imports with same module name | IMPORT_DUP_MODULE_OR_BLACKBOX | error |
| 5 | Same file imported twice | `@import "lib.jz"; @import "lib.jz";` | IMPORT_FILE_MULTIPLE_TIMES | error |
| 6 | Two @project in one file | `@project a ... @endproj @project b ...` | PROJECT_MULTIPLE_PER_FILE | error |
| 7 | Missing @endproj | `@project test` with no `@endproj` | PROJECT_MISSING_ENDPROJ | error |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 6_2_HAPPY_PATH-canonical_form_ok.jz | -- | Valid project in canonical form (clean compile) |
| 6_2_HAPPY_PATH-canonical_form_ok_lib.jz | -- | Support library file for canonical form happy path |
| 6_2_IMPORT_DUP_MODULE_OR_BLACKBOX-name_collision.jz | IMPORT_DUP_MODULE_OR_BLACKBOX | Imported module/blackbox name duplicates existing project name |
| 6_2_IMPORT_DUP_MODULE_OR_BLACKBOX-name_collision_libA.jz | -- | Support file for name collision test (library A) |
| 6_2_IMPORT_DUP_MODULE_OR_BLACKBOX-name_collision_libB.jz | -- | Support file for name collision test (library B) |
| 6_2_IMPORT_FILE_HAS_PROJECT-imported_has_project.jz | IMPORT_FILE_HAS_PROJECT | Imported file contains its own @project/@endproj |
| 6_2_IMPORT_FILE_HAS_PROJECT-imported_has_project_lib.jz | -- | Support file for imported-has-project test |
| 6_2_IMPORT_FILE_MULTIPLE_TIMES-duplicate_import.jz | IMPORT_FILE_MULTIPLE_TIMES | Same source file imported more than once |
| 6_2_IMPORT_FILE_MULTIPLE_TIMES-duplicate_import_lib.jz | -- | Support file for duplicate import test |
| 6_2_IMPORT_NOT_AT_PROJECT_TOP-import_after_config.jz | IMPORT_NOT_AT_PROJECT_TOP | @import appears after CONFIG/CLOCKS/PIN blocks |
| 6_2_IMPORT_NOT_AT_PROJECT_TOP-import_after_config_lib.jz | -- | Support file for import-after-config test |
| 6_2_IMPORT_OUTSIDE_PROJECT-import_at_file_scope.jz | IMPORT_OUTSIDE_PROJECT | @import used outside @project block |
| 6_2_PROJECT_MISSING_ENDPROJ-missing_endproj.jz | PROJECT_MISSING_ENDPROJ | @project block missing @endproj terminator |
| 6_2_PROJECT_MULTIPLE_PER_FILE-two_projects.jz | PROJECT_MULTIPLE_PER_FILE | Multiple @project directives in same file |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| IMPORT_OUTSIDE_PROJECT | error | S6.2.1 @import used outside @project block | 6_2_IMPORT_OUTSIDE_PROJECT-import_at_file_scope.jz |
| IMPORT_NOT_AT_PROJECT_TOP | error | S6.2.1 @import appears after CONFIG/CLOCKS/PIN/blackbox/top-level new blocks | 6_2_IMPORT_NOT_AT_PROJECT_TOP-import_after_config.jz |
| IMPORT_FILE_HAS_PROJECT | error | S6.2.1 Imported file contains its own @project/@endproj (forbidden) | 6_2_IMPORT_FILE_HAS_PROJECT-imported_has_project.jz |
| IMPORT_DUP_MODULE_OR_BLACKBOX | error | S6.2.1/S6.10 Imported module/blackbox name duplicates existing project name | 6_2_IMPORT_DUP_MODULE_OR_BLACKBOX-name_collision.jz |
| IMPORT_FILE_MULTIPLE_TIMES | error | S6.2.1 Same source file imported more than once into a single project | 6_2_IMPORT_FILE_MULTIPLE_TIMES-duplicate_import.jz |
| PROJECT_MULTIPLE_PER_FILE | error | S6.2/S6.9 Multiple @project directives in same file | 6_2_PROJECT_MULTIPLE_PER_FILE-two_projects.jz |
| PROJECT_MISSING_ENDPROJ | error | S6.2 @project block missing @endproj terminator | 6_2_PROJECT_MISSING_ENDPROJ-missing_endproj.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| -- | -- | All rules for this section are covered by existing tests |
