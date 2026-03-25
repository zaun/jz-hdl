# Test Plan: 12.1 Compile Errors

**Specification Reference:** Section 12.1 of jz-hdl-specification.md

## 1. Objective

Verify all compile errors listed in the specification error summary are properly detected and reported with correct rule IDs, messages, and source locations. This is an aggregation section that cross-references error-severity rules from all other sections.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Valid program | Syntactically and semantically correct program | No errors emitted |

### 2.2 Error Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Cross-reference | Each error condition from Sections 1-11 test plans | Respective error rule emitted |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Multiple errors in single file | File with several violations | All errors reported |
| 2 | Error in imported file | Violation in an imported module | Correct source location in imported file |

## 3. Input/Output Matrix

See individual section test plans (1.x through 11.x) for per-rule error matrices. This plan serves as the regression aggregation point.

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| `12_1_COMPILE_ERRORS-valid_program_ok.jz` | — | Happy-path: valid program with no errors |
| `12_1_COMPILE_ERRORS-multiple_errors.jz` | — | Multiple compile errors reported in a single program |

## 5. Rules Matrix

### 5.1 Rules Tested

All rules with `JZ_RULE_MODE_ERR` from `rules.c` are cross-referenced across all section test plans (Sections 1-11). See each section's Rules Matrix for specific coverage.

### 5.2 Rules Not Tested

Aggregate gap analysis from all section plans. Any error-severity rule not covered in Sections 1-11 test plans represents a gap that should be addressed in the respective section.
