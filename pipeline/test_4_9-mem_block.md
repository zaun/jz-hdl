# Test Plan: 4.9 MEM Block (Block RAM)

**Specification Reference:** Section 4.9 of jz-hdl-specification.md

## 1. Objective

Verify that MEM block declarations are correctly parsed and validated. This section is a brief reference; full MEM coverage is in Section 7 test plans. This plan covers the declaration placement within module canonical form and cross-references to Section 7.

## 2. Instrumentation Strategy

- **Span: `parser.mem`** — Trace MEM block parsing within module; attributes: `mem_count`, `storage_type` (BLOCK/DISTRIBUTED).
- **Coverage Hook:** Ensure MEM blocks appear in correct module position.

## 3. Test Scenarios

### 3.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | BLOCK type MEM | `MEM(type=BLOCK) { ... }` in module |
| 2 | DISTRIBUTED type MEM | `MEM(type=DISTRIBUTED) { ... }` in module |
| 3 | Multiple MEMs | Several MEM declarations in one module |

### 3.2 Boundary/Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Module without MEM | Valid — MEM is optional |

### 3.3 Negative Testing

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Invalid type keyword | `MEM(type=INVALID) { ... }` — Error |
| 2 | MEM outside module | MEM block at top level — Error |

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | `MEM(type=BLOCK)` | Valid | — | Happy path |
| 2 | `MEM(type=INVALID)` | Error | MEM_DECL_TYPE_INVALID | S4.9/S7.1 |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `parser_mem.c` | Parses MEM declarations | Token stream |
| `driver_mem.c` | MEM semantic validation | See Section 7 plans |

## 6. Rules Matrix

### 6.1 Rules Tested

| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| MEM_DECL_TYPE_INVALID | Invalid storage type | Neg 1 |

### 6.2 Rules Missing

| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| — | Full coverage in Section 7 test plans | — |
