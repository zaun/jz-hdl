# Test Plan: 12.4 Path Security

**Specification Reference:** Section 12.4 of jz-hdl-specification.md

## 1. Objective

Verify path security enforcement: absolute paths are rejected without `--allow-absolute-paths`, traversal paths (`../`) are rejected without `--allow-traversal`, paths outside the sandbox are rejected, and symlink escapes are detected.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Relative path within sandbox | `@import "subdir/module.jz"` | Valid, no error |
| 2 | Absolute path with flag | `--allow-absolute-paths` with absolute path | Valid, no error |
| 3 | Traversal with flag | `--allow-traversal` with `../` path | Valid, no error |

### 2.2 Error Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Absolute path without flag | `/absolute/path/module.jz` in import | Error: PATH_ABSOLUTE_FORBIDDEN |
| 2 | Absolute path in file init | `/absolute/path/data.bin` in file init | Error: PATH_ABSOLUTE_FORBIDDEN |
| 3 | Traversal without flag | `../escape/module.jz` in import | Error: PATH_TRAVERSAL_FORBIDDEN |
| 4 | Traversal in file init | `../escape/data.bin` in file init | Error: PATH_TRAVERSAL_FORBIDDEN |
| 5 | Path outside sandbox | Resolved path falls outside permitted roots | Error: PATH_OUTSIDE_SANDBOX |
| 6 | Symlink escaping sandbox | Symlink resolves to target outside sandbox | Error: PATH_SYMLINK_ESCAPE |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Redundant separators | Path with `//` | Normalized, no error if within sandbox |
| 2 | Dot components | Path with `.` components | Normalized, no error if within sandbox |
| 3 | Symlink within sandbox | Symlink to target inside sandbox | Valid |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| 1 | `/absolute/path` | Error | PATH_ABSOLUTE_FORBIDDEN | error | S12.2 |
| 2 | `../escape` | Error | PATH_TRAVERSAL_FORBIDDEN | error | S12.2 |
| 3 | Path outside sandbox | Error | PATH_OUTSIDE_SANDBOX | error | S12.2 |
| 4 | Symlink escape | Error | PATH_SYMLINK_ESCAPE | error | S12.2 |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| `12_4_PATH_ABSOLUTE_FORBIDDEN-absolute_file_init.jz` | PATH_ABSOLUTE_FORBIDDEN | Absolute path in file init |
| `12_4_PATH_ABSOLUTE_FORBIDDEN-absolute_import.jz` | PATH_ABSOLUTE_FORBIDDEN | Absolute path in import |
| `12_4_PATH_TRAVERSAL_FORBIDDEN-traversal_file_init.jz` | PATH_TRAVERSAL_FORBIDDEN | Traversal path in file init |
| `12_4_PATH_TRAVERSAL_FORBIDDEN-traversal_import.jz` | PATH_TRAVERSAL_FORBIDDEN | Traversal path in import |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| PATH_ABSOLUTE_FORBIDDEN | error | Absolute path used without --allow-absolute-paths | `12_4_PATH_ABSOLUTE_FORBIDDEN-absolute_file_init.jz`, `12_4_PATH_ABSOLUTE_FORBIDDEN-absolute_import.jz` |
| PATH_TRAVERSAL_FORBIDDEN | error | Path contains `..` traversal without --allow-traversal | `12_4_PATH_TRAVERSAL_FORBIDDEN-traversal_file_init.jz`, `12_4_PATH_TRAVERSAL_FORBIDDEN-traversal_import.jz` |
| PATH_OUTSIDE_SANDBOX | error | S12.2 Resolved path falls outside all permitted sandbox roots | `12_4_PATH_OUTSIDE_SANDBOX-outside_sandbox.jz` |
| PATH_SYMLINK_ESCAPE | error | S12.2 Symbolic link resolves to target outside sandbox root | `12_4_PATH_SYMLINK_ESCAPE-symlink_escape.jz` |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
