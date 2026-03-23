# Test Plan: 12.4 Path Security
**Specification Reference:** Section 12.4 of jz-hdl-specification.md
## 1. Objective
Verify path security enforcement: absolute path rejection (without flag), traversal rejection (without flag), sandbox enforcement, and symlink escape detection.
## 2. Instrumentation Strategy
- **Span: `security.path_check`** — attributes: `path`, `is_absolute`, `has_traversal`, `in_sandbox`, `is_symlink_escape`.
## 3. Test Scenarios
### 3.1 Happy Path
1. Relative path within sandbox — valid
2. Absolute path with --allow-absolute-paths flag — valid
3. Traversal with --allow-traversal flag — valid
### 3.2 Boundary/Edge Cases
1. Path with redundant separators (`//`)
2. Path with `.` components
3. Symlink within sandbox — valid
### 3.3 Negative Testing
1. Absolute path without flag — Error (PATH_ABSOLUTE_FORBIDDEN)
2. Traversal `../` without flag — Error (PATH_TRAVERSAL_FORBIDDEN)
3. Path outside sandbox — Error (PATH_OUTSIDE_SANDBOX)
4. Symlink escaping sandbox — Error (PATH_SYMLINK_ESCAPE)
## 4. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | `/absolute/path` | Error | PATH_ABSOLUTE_FORBIDDEN | S12.4 |
| 2 | `../escape` | Error | PATH_TRAVERSAL_FORBIDDEN | S12.4 |
| 3 | Path outside sandbox | Error | PATH_OUTSIDE_SANDBOX | S12.4 |
| 4 | Symlink escape | Error | PATH_SYMLINK_ESCAPE | S12.4 |
## 5. Integration Points
| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `path_security.c` | Path validation | Mock filesystem (stat, realpath) |
| `parser_import.c` | Import path resolution | Integration test |
## 6. Rules Matrix
### 6.1 Rules Tested
| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| PATH_ABSOLUTE_FORBIDDEN | Absolute path without flag | Neg 1 |
| PATH_TRAVERSAL_FORBIDDEN | Traversal without flag | Neg 2 |
| PATH_OUTSIDE_SANDBOX | Path outside allowed roots | Neg 3 |
| PATH_SYMLINK_ESCAPE | Symlink to outside sandbox | Neg 4 |
### 6.2 Rules Missing
| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| — | All path security rules appear covered | — |
