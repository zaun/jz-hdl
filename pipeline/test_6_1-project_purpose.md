# Test Plan: 6.1 Project Purpose and Role

**Specification Reference:** Section 6.1 of jz-hdl-specification.md

## 1. Objective

Verify @project declaration with CHIP property, chip ID resolution (case-insensitive, JSON file lookup, built-in database fallback), and GENERIC default behavior.

## 2. Instrumentation Strategy

- **Span: `compiler.project_init`** — attributes: `chip_id`, `chip_source` (file/builtin/generic).
- **Event: `chip.not_found`** — chip ID not found in file or builtin database.

## 3. Test Scenarios

### 3.1 Happy Path
1. `@project(CHIP=GENERIC) test` — default chip
2. `@project(CHIP="GW2AR-18") test` — specific chip with string literal
3. `@project(CHIP=GW2AR18) test` — chip ID as identifier
4. Case-insensitive: `CHIP=gw2ar18` matches `GW2AR18`

### 3.2 Boundary/Edge Cases
1. No CHIP property → defaults to GENERIC
2. Chip JSON in same directory as source file

### 3.3 Negative Testing
1. Unknown chip ID → Error
2. Malformed chip JSON → Error

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Unknown CHIP=INVALID | Error | PROJECT_CHIP_NOT_FOUND | S6.1 |
| 2 | CHIP=GENERIC | Valid | — | Default |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `chip_data.c` | Chip JSON parsing | Mock chip database |
| `parser_project.c` | Project header parsing | Token stream |

## 6. Rules Matrix

### 6.1 Rules Tested
| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| PROJECT_CHIP_NOT_FOUND | Unknown chip ID | Neg 1 |

### 6.2 Rules Missing
| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| PROJECT_CHIP_JSON_MALFORMED | S6.1.1 | Corrupted chip JSON file |
