# Test Plan: 4.9 MEM Block

**Specification Reference:** Section 4.9 of jz-hdl-specification.md

## 1. Objective

Verify MEM block declaration within module. Full MEM coverage is in Section 7 test plans. This plan covers declaration placement within module canonical form, storage type validation, and cross-references to Section 7.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | BLOCK type MEM | `MEM(type=BLOCK) { ... }` in module |
| 2 | DISTRIBUTED type MEM | `MEM(type=DISTRIBUTED) { ... }` in module |
| 3 | Multiple MEMs | Several MEM declarations in one module |

### 2.2 Error Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Invalid type keyword | `MEM(type=INVALID) { ... }` -- Error |
| 2 | MEM outside module | MEM block at top level -- Error |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Module without MEM | Valid -- MEM is optional |

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1 | Invalid storage type keyword | `MEM(type=INVALID)` | MEM_TYPE_INVALID | error |
| 2 | Valid BLOCK type | `MEM(type=BLOCK) { ... }` | -- | -- (pass) |
| 3 | Valid DISTRIBUTED type | `MEM(type=DISTRIBUTED) { ... }` | -- | -- (pass) |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 4_9_MEM_HAPPY_PATH-mem_ok.jz | -- | Happy path: valid MEM declarations |
| 4_9_MEM_TYPE_INVALID-invalid_storage_type.jz | MEM_TYPE_INVALID | Invalid MEM storage type keyword |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| MEM_TYPE_INVALID | error | S4.9/S7.1 Invalid MEM storage type keyword | 4_9_MEM_TYPE_INVALID-invalid_storage_type.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| -- | -- | Full MEM coverage is in Section 7 test plans (test_7_1 through test_7_8) |
