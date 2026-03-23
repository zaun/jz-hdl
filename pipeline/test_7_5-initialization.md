# Test Plan: 7.5 Initialization (7.5.1–7.5.2)

**Specification Reference:** Section 7.5 of jz-hdl-specification.md

## 1. Objective

Verify literal initialization (inline hex values) and file-based initialization (@file directive), zero-padding for smaller files, and x/z prohibition in init values.

## 2. Instrumentation Strategy

- **Span: `sem.mem_init`** — attributes: `mem_id`, `init_type` (literal/file), `init_depth`.
- **Event: `mem_init.partial`** — File smaller than depth.

## 3. Test Scenarios

### 3.1 Happy Path
1. Literal init: `m [8] [4] = 32'h00000000 { ... };`
2. File init: `m [8] [256] = @file("data.hex") { ... };`
3. CONST string path: `@file(ROM_FILE)`

### 3.2 Boundary/Edge Cases
1. File exactly matches depth — no padding
2. File smaller than depth — zero-padded, warning
3. Single-entry memory with literal

### 3.3 Negative Testing
1. Init literal with x — Error
2. Init literal with z — Error
3. Init file not found — Error
4. File larger than depth — Error
5. Init literal overflow — Error

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Init with x | Error | MEM_INIT_CONTAINS_X | S7.5 |
| 2 | Init with z | Error | MEM_INIT_CONTAINS_Z | S7.5 |
| 3 | File not found | Error | MEM_INIT_FILE_NOT_FOUND | S7.5.2 |
| 4 | File smaller | Warning | MEM_WARN_PARTIAL_INIT | S7.7.3 |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `driver_mem.c` | Init validation | Unit test |
| `parser_mem.c` | Init parsing | Token stream |
| File system | @file data loading | Mock file contents |

## 6. Rules Matrix

### 6.1 Rules Tested
| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| MEM_INIT_CONTAINS_X | x in memory init | Neg 1 |
| MEM_INIT_CONTAINS_Z | z in memory init | Neg 2 |
| MEM_WARN_PARTIAL_INIT | File smaller than depth | Boundary 2 |

### 6.2 Rules Missing
| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| MEM_INIT_FILE_NOT_FOUND | S7.5.2 | Init file doesn't exist |
| MEM_INIT_OVERFLOW | S7.5 | Init literal too large |
