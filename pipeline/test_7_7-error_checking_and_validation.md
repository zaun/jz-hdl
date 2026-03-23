# Test Plan: 7.7 Error Checking and Validation (7.7.1–7.7.3)

**Specification Reference:** Section 7.7 of jz-hdl-specification.md

## 1. Objective

Verify all MEM declaration errors (7.7.1), access errors (7.7.2), and warnings (7.7.3).

## 2. Instrumentation Strategy

- **Span: `sem.mem_errors`** — Trace all MEM-related diagnostics.

## 3. Test Scenarios

### 3.1 Declaration Errors (7.7.1)
1. Zero depth / zero width
2. Invalid type
3. No ports
4. Duplicate port name
5. ASYNC on write port
6. Multiple write modes on same port

### 3.2 Access Errors (7.7.2)
1. Wrong block type for access
2. Address width mismatch
3. Undeclared port access
4. Write to read-only port
5. Multiple WDATA assigns same path

### 3.3 Warnings (7.7.3)
1. Port declared never accessed — Warning
2. Partial init file — Warning
3. Dead code access — Warning

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Port never used | Warning | MEM_WARN_PORT_NEVER_ACCESSED | S7.7.3 |
| 2 | Partial init | Warning | MEM_WARN_PARTIAL_INIT | S7.7.3 |
| 3 | Dead code access | Warning | MEM_WARN_DEAD_CODE_ACCESS | S7.7.3 |
| 4 | Multiple WDATA | Error | MEM_MULTIPLE_WDATA_ASSIGNS | S7.2.3 |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `driver_mem.c` | All MEM validation | Integration test |

## 6. Rules Matrix

### 6.1 Rules Tested
| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| MEM_WARN_PORT_NEVER_ACCESSED | Unused port | 3.3.1 |
| MEM_WARN_PARTIAL_INIT | Partial initialization | 3.3.2 |
| MEM_WARN_DEAD_CODE_ACCESS | Dead code | 3.3.3 |
| MEM_MULTIPLE_WDATA_ASSIGNS | Multiple writes | 3.2.5 |

### 6.2 Rules Missing
| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| — | All expected MEM rules appear covered | — |
