# Test Plan: 6.2 Project Canonical Form (including 6.2.1 @import)

**Specification Reference:** Section 6.2 and 6.2.1 of jz-hdl-specification.md

## 1. Objective

Verify project structure (canonical ordering), @import directive (placement, semantics, de-duplication, path normalization, nested imports), and imported file constraints (no @project in imported files).

## 2. Instrumentation Strategy

- **Span: `parser.project`** — attributes: `import_count`, `has_config`, `has_clocks`, `has_pins`, `has_top`.
- **Span: `import.resolve`** — attributes: `path`, `canonical_path`, `is_duplicate`.
- **Event: `import.duplicate`** — Same file imported twice.

## 3. Test Scenarios

### 3.1 Happy Path
1. Full project with all sections in order
2. @import two files with modules
3. @import file with @blackbox definitions
4. Relative path import
5. Multiple @imports before CONFIG

### 3.2 Boundary/Edge Cases
1. Zero @imports — valid
2. Import file with zero modules — valid
3. Deep import chain (A imports B imports C)

### 3.3 Negative Testing
1. @import after CONFIG — Error (placement)
2. Imported file contains @project — Error
3. Duplicate import same path — Error (IMPORT_FILE_MULTIPLE_TIMES)
4. Duplicate import via symlink (same canonical path) — Error
5. Duplicate module name across imports — Error
6. @import outside @project — Error
7. Import file not found — Error

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | @import after CONFIG | Error | IMPORT_WRONG_POSITION | S6.2.1 |
| 2 | Imported file has @project | Error | IMPORT_HAS_PROJECT | S6.2.1 |
| 3 | Same file imported twice | Error | IMPORT_FILE_MULTIPLE_TIMES | S6.2.1 |
| 4 | Import not found | Error | IMPORT_FILE_NOT_FOUND | S6.2.1 |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `parser_import.c` | Import resolution and parsing | Mock filesystem |
| `path_security.c` | Path validation | Integration test |
| `parser_project.c` | Project structure validation | Token stream |

## 6. Rules Matrix

### 6.1 Rules Tested
| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| IMPORT_FILE_MULTIPLE_TIMES | Duplicate import | Neg 3, 4 |
| IMPORT_HAS_PROJECT | Imported file contains @project | Neg 2 |
| DIRECTIVE_INVALID_CONTEXT | @import outside @project | Neg 6 |

### 6.2 Rules Missing
| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| IMPORT_WRONG_POSITION | S6.2.1 "immediately after header" | @import after CONFIG/CLOCKS |
