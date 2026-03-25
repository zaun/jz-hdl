# Test Plan: 6.1 Project Purpose

**Specification Reference:** Section 6.1 of jz-hdl-specification.md

## 1. Objective

Verify @project with CHIP property, chip ID resolution (case-insensitive, JSON file lookup, built-in database fallback), and GENERIC default behavior.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Default chip | `@project(CHIP=GENERIC) test` -- uses generic chip data |
| 2 | Specific chip string | `@project(CHIP="GW2AR-18") test` -- chip ID as string literal |
| 3 | Specific chip identifier | `@project(CHIP=GW2AR18) test` -- chip ID as identifier |
| 4 | Case-insensitive match | `CHIP=gw2ar18` matches `GW2AR18` |

### 2.2 Error Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Unknown chip ID | CHIP=INVALID -- no matching chip data in local dir or built-in database |
| 2 | Malformed chip JSON | Chip JSON file exists but is not valid JSON |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | No CHIP property | Omitting CHIP defaults to GENERIC |
| 2 | Chip JSON in source directory | Chip JSON co-located with source file |

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|------------------|----------|
| 1 | Unknown CHIP value | `@project(CHIP=INVALID) test` | PROJECT_CHIP_DATA_NOT_FOUND | error |
| 2 | Malformed chip JSON file | `@project(CHIP=broken) test` with broken JSON | PROJECT_CHIP_DATA_INVALID | error |
| 3 | Valid CHIP=GENERIC | `@project(CHIP=GENERIC) test` | -- (clean compile) | -- |
| 4 | Valid specific chip | `@project(CHIP=GW2AR18) test` | -- (clean compile) | -- |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 6_1_HAPPY_PATH-project_chip_ok.jz | -- | Valid project with chip ID (clean compile) |
| 6_1_PROJECT_CHIP_DATA_INVALID-malformed_json.jz | PROJECT_CHIP_DATA_INVALID | Chip JSON data could not be parsed |
| 6_1_PROJECT_CHIP_DATA_NOT_FOUND-unknown_chip_id.jz | PROJECT_CHIP_DATA_NOT_FOUND | Chip data not found for CHIP id |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| PROJECT_CHIP_DATA_NOT_FOUND | error | S6.1 Chip data not found for CHIP id (no local JSON and no built-in data) | 6_1_PROJECT_CHIP_DATA_NOT_FOUND-unknown_chip_id.jz |
| PROJECT_CHIP_DATA_INVALID | error | S6.1 Chip JSON data could not be parsed | 6_1_PROJECT_CHIP_DATA_INVALID-malformed_json.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| -- | -- | All rules for this section are covered by existing tests |
