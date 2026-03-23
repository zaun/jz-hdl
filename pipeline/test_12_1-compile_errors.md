# Test Plan: 12.1 Compile Errors
**Specification Reference:** Section 12.1 of jz-hdl-specification.md
## 1. Objective
Verify all compile errors listed in the specification error summary are properly detected and reported with correct rule IDs, messages, and source locations.
## 2. Instrumentation Strategy
- **Span: `diagnostic.compile_errors`** — Trace all error-level diagnostics.
- **Coverage Hook:** Map each rule ID in rules.c with JZ_RULE_MODE_ERR to at least one test case.
## 3. Test Scenarios
### 3.1 Happy Path
1. Valid program with no errors
### 3.2 Boundary/Edge Cases
1. Multiple errors in single file — all reported
2. Error in imported file — correct source location
### 3.3 Negative Testing
Cross-reference: Each error condition from Sections 1-11 test plans should be exercised. This is the comprehensive error matrix.
## 4. Input/Output Matrix
See individual section test plans for per-rule error matrices. This plan aggregates them for regression.
## 5. Integration Points
| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `diagnostic.c` | Error formatting and output | Capture FILE* output |
| `rules.c` | Rule ID registry | Verify all ERR rules tested |
| All `driver_*.c` | Semantic error detection | Full pipeline integration |
## 6. Rules Matrix
### 6.1 Rules Tested
All rules with `JZ_RULE_MODE_ERR` from `rules.c` — cross-referenced across all section test plans.
### 6.2 Rules Missing
Aggregate gap analysis from all section plans.
