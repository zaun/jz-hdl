# Test Plan: 6.11 Error Summary

**Specification Reference:** Section 6.11 of jz-hdl-specification.md

## 1. Objective

Cross-reference of all project-level errors from sections 6.1--6.10. This section introduces no new rules; it aggregates the error conditions defined in prior subsections for completeness verification.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Valid complete project | Project with all blocks correct, no diagnostics emitted |

### 2.2 Error Cases

Error conditions are defined and tested in their respective section test plans:

| Category | Errors | Covered By |
|----------|--------|------------|
| Clock Errors | Name mismatch, width != 1, duplicates, period <= 0, invalid edge | test_6_4-clocks_block.md |
| PIN Declaration Errors | Multi-block pin, invalid standard, missing drive, width <= 0, duplicates | test_6_5-pin_blocks.md |
| MAP Errors | Unmapped pin, undeclared map entry, duplicate physical location, invalid board pin | test_6_6-map_block.md |
| Instantiation Errors | Missing top module, port not listed, width mismatch, direction mismatch, missing pin decl, undefined blackbox | test_6_9-top_level_module.md |
| Import Errors | Duplicate file import | test_6_1-project_purpose.md |
| Global Errors | Project/module name conflict, multiple @project, missing @endproj | test_6_10-project_scope_and_uniqueness.md |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Multiple errors in one project | Project with errors across several categories; verify all are reported |

## 3. Input/Output Matrix

No new input/output pairs. All error conditions are covered by their respective section test plans (6.1--6.10). See those plans for specific rule IDs, severities, and test files.

## 4. Existing Validation Tests

No tests specific to section 6.11. All project-level error validation is covered by tests in sections 6.1--6.10.

## 5. Rules Matrix

### 5.1 Rules Tested

No new rules introduced in this section. All project-level rules are defined and tested in sections 6.1--6.10.

### 5.2 Rules Not Tested

No rules to test -- this section is a cross-reference summary only.
