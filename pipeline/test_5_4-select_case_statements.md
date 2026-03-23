# Test Plan: 5.4 SELECT/CASE Statements

**Specification Reference:** Section 5.4 of jz-hdl-specification.md

## 1. Objective

Verify SELECT/CASE syntax, duplicate CASE detection, DEFAULT behavior (optional in SYNC where registers hold, recommended in ASYNC), CASE value width matching against selector, and incomplete coverage warnings.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Simple SELECT | `SELECT (state) { CASE 0 { ... } DEFAULT { ... } }` | Valid |
| 2 | Multiple CASEs | Four CASE arms with DEFAULT | Valid |
| 3 | x-wildcard CASE | `CASE 8'b1010_xxxx { ... }` | Valid, don't-care bits |
| 4 | Fall-through | `CASE 0 CASE 1 { ... }` | Valid, shared handler |
| 5 | SYNC without DEFAULT | SELECT in SYNC, no DEFAULT | Valid, registers hold |
| 6 | CONST in CASE | `CASE MY_CONST { ... }` | Valid, compile-time constant |
| 7 | Nested SELECT | SELECT inside CASE body | Valid |

### 2.2 Error Cases

| # | Test Case | Input | Expected | Rule ID |
|---|-----------|-------|----------|---------|
| 1 | Duplicate CASE values | `CASE 0 { ... } CASE 0 { ... }` | Error | SELECT_DUP_CASE_VALUE |
| 2 | CASE width mismatch | CASE value width differs from selector width | Error | SELECT_CASE_WIDTH_MISMATCH |
| 3 | ASYNC SELECT no DEFAULT | SELECT in ASYNC without DEFAULT | Warning | SELECT_DEFAULT_RECOMMENDED_ASYNC |
| 4 | SYNC SELECT no DEFAULT | SELECT in SYNC without DEFAULT | Warning | SELECT_NO_MATCH_SYNC_OK |
| 5 | Incomplete async coverage | SELECT in ASYNC with missing cases, no DEFAULT | Warning | WARN_INCOMPLETE_SELECT_ASYNC |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Single CASE + DEFAULT | Minimum useful SELECT | Valid |
| 2 | Many CASEs (256) | One per possible value of 8-bit selector | Valid |
| 3 | All-x CASE | `CASE 8'bxxxx_xxxx` | Valid, matches everything like DEFAULT |
| 4 | x-wildcard with non-x mix | `CASE 8'b10xx_0011` | Valid, partial don't-care |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| 1 | Duplicate CASE 0 | Compile error | SELECT_DUP_CASE_VALUE | error | Same value in two CASE labels |
| 2 | CASE value width != selector width | Compile error | SELECT_CASE_WIDTH_MISMATCH | error | Widths must match |
| 3 | ASYNC SELECT without DEFAULT | Warning | SELECT_DEFAULT_RECOMMENDED_ASYNC | warning | May cause floating nets |
| 4 | SYNC SELECT without DEFAULT | Warning | SELECT_NO_MATCH_SYNC_OK | warning | Registers hold, not an error |
| 5 | Incomplete ASYNC coverage | Warning | WARN_INCOMPLETE_SELECT_ASYNC | warning | Missing cases without DEFAULT |
| 6 | x-wildcard in CASE | Accepted | -- | -- | Valid pattern matching |
| 7 | Fall-through CASE 0 CASE 1 | Accepted | -- | -- | Shared handler |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 5_4_SELECT_DUP_CASE_VALUE-duplicate_case_values.jz | SELECT_DUP_CASE_VALUE | Multiple CASE labels with same value in SELECT |
| 5_4_SELECT_CASE_WIDTH_MISMATCH-case_width_mismatch.jz | SELECT_CASE_WIDTH_MISMATCH | CASE value width does not match selector expression width |
| 5_4_SELECT_DEFAULT_RECOMMENDED_ASYNC-async_select_no_default.jz | SELECT_DEFAULT_RECOMMENDED_ASYNC | ASYNC SELECT without DEFAULT (warning) |
| 5_4_SELECT_NO_MATCH_SYNC_OK-sync_select_no_default.jz | SELECT_NO_MATCH_SYNC_OK | SYNC SELECT without DEFAULT, registers hold (warning) |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| SELECT_DUP_CASE_VALUE | error | S5.4/S8.1 Multiple CASE labels with same value | 5_4_SELECT_DUP_CASE_VALUE-duplicate_case_values.jz |
| SELECT_CASE_WIDTH_MISMATCH | error | S5.4 CASE value width does not match selector width | 5_4_SELECT_CASE_WIDTH_MISMATCH-case_width_mismatch.jz |
| SELECT_DEFAULT_RECOMMENDED_ASYNC | warning | S5.4/S8.3 ASYNC SELECT without DEFAULT | 5_4_SELECT_DEFAULT_RECOMMENDED_ASYNC-async_select_no_default.jz |
| SELECT_NO_MATCH_SYNC_OK | warning | S5.4 SYNC SELECT missing DEFAULT, registers hold | 5_4_SELECT_NO_MATCH_SYNC_OK-sync_select_no_default.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| WARN_INCOMPLETE_SELECT_ASYNC | warning | No dedicated 5_4-prefixed test; overlaps with SELECT_DEFAULT_RECOMMENDED_ASYNC |
