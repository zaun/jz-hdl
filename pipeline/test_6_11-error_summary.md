# Test Plan: 6.11 Error Summary (Project Errors)

**Specification Reference:** Section 6.11 of jz-hdl-specification.md

## 1. Objective

Verify that all project-level error conditions listed in the spec's error summary are properly detected and reported.

## 2. Instrumentation Strategy

- **Span: `diagnostic.project_errors`** — Trace all project-level diagnostics.

## 3. Test Scenarios

### 3.1 Happy Path
1. Valid project with no errors

### 3.2 Negative Testing — Each Error Condition
Each error from the spec summary should have a dedicated test case. Cross-reference with individual section test plans (6.1–6.10) for detailed scenarios.

## 4. Input/Output Matrix

Refer to individual section test plans for complete error matrices.

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `driver_project.c` | Project error detection | Integration test |
| `diagnostic.c` | Error formatting | Verify message content |

## 6. Rules Matrix

### 6.1 Rules Tested
All project-level rules from sections 6.1–6.10 plans.

### 6.2 Rules Missing
See individual section plans for gap analysis.
