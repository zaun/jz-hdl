# Test Plan: 7.2 Port Types and Semantics (7.2.1–7.2.3)

**Specification Reference:** Section 7.2 of jz-hdl-specification.md

## 1. Objective

Verify OUT (read) port, IN (write) port, and INOUT (read/write) port semantics including ASYNC/SYNC modes, write modes, address width requirements, and port interaction rules.

## 2. Instrumentation Strategy

- **Span: `sem.mem_port`** — attributes: `port_name`, `direction`, `mode`, `addr_width`.

## 3. Test Scenarios

### 3.1 Happy Path
1. OUT ASYNC read port — combinational read
2. OUT SYNC read port — registered output
3. IN write port — synchronous write
4. INOUT SYNC WRITE_FIRST — write-then-read
5. INOUT SYNC READ_FIRST — read-then-write
6. INOUT SYNC NO_CHANGE — hold output during write

### 3.2 Negative Testing
1. Write to OUT port — Error
2. Read from IN port — Error
3. ASYNC mode on INOUT — Error (if prohibited)
4. Multiple WDATA assigns — Error

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Write to read-only port | Error | MEM_PORT_DIRECTION | S7.2.1 |
| 2 | Multiple WDATA assigns | Error | MEM_MULTIPLE_WDATA_ASSIGNS | S7.2.3 |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `driver_mem.c` | Port semantics validation | Integration test |
| `ir_build_memory.c` | Memory IR generation | IR verification |

## 6. Rules Matrix

### 6.1 Rules Tested
| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| MEM_MULTIPLE_WDATA_ASSIGNS | Multiple writes to INOUT wdata | Neg 4 |

### 6.2 Rules Missing
| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| MEM_PORT_DIRECTION | S7.2 | Writing to read port or reading from write port |
