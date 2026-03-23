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
| 1 | Invalid type keyword | `MEM(type=INVALID) { ... }` — Error |
| 2 | MEM outside module | MEM block at top level — Error |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Module without MEM | Valid — MEM is optional |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | `MEM(type=BLOCK)` | Valid | — | Happy path |
| 2 | `MEM(type=INVALID)` | Error | MEM_TYPE_INVALID | S4.9/S7.1 |

## 4. Existing Validation Tests

| Test File | Rule Tested |
|-----------|-------------|
| 4_9_MEM_TYPE_INVALID-invalid_storage_type.jz | MEM_TYPE_INVALID |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Test File(s) |
|---------|----------|--------------|
| MEM_TYPE_INVALID | error | 4_9_MEM_TYPE_INVALID-invalid_storage_type.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Gap Description |
|---------|----------|-----------------|
| — | — | Full MEM coverage in Section 7 test plans |
