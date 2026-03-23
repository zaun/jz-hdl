# Test Plan: 8.1 Global Purpose

**Specification Reference:** Section 8.1 of jz-hdl-specification.md

## 1. Objective
Verify @global blocks define named collections of sized literal constants visible to all modules.

## 2. Instrumentation Strategy
- **Span: `sem.global_block`** — attributes: `global_name`, `const_count`.

## 3. Test Scenarios
### 3.1 Happy Path
1. Define @global with sized literals
2. Reference `GLOBAL.CONST` in module expressions
3. Multiple @global blocks with unique names

### 3.2 Boundary/Edge Cases
1. Single entry in @global block
2. Large number of entries

### 3.3 Negative Testing
1. Duplicate @global name — Error
2. Assign to global constant — Error (read-only)

## 4. Input/Output Matrix
| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Duplicate @global name | Error | GLOBAL_DUPLICATE_NAME | S8.3 |
| 2 | Assign to global | Error | GLOBAL_READONLY | S8.5 |

## 5. Integration Points
| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `parser_core.c` | @global parsing | Token stream |
| `driver.c` | Global visibility | Integration test |

## 6. Rules Matrix
### 6.1 Rules Tested
| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| GLOBAL_DUPLICATE_NAME | Duplicate global block name | Neg 1 |
### 6.2 Rules Missing
| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| GLOBAL_READONLY | S8.5 "read-only" | Assigning to global constant |
