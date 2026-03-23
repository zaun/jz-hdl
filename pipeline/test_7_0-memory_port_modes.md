# Test Plan: 7.0 Memory Port Modes

**Specification Reference:** Section 7.0 of jz-hdl-specification.md

## 1. Objective

Verify MEM port mode declarations: OUT (read), IN (write), INOUT (read/write), ASYNC/SYNC read modes, and write modes (WRITE_FIRST, READ_FIRST, NO_CHANGE).

## 2. Instrumentation Strategy

- **Span: `sem.mem_port_mode`** — attributes: `port_name`, `direction`, `read_mode`, `write_mode`.

## 3. Test Scenarios

### 3.1 Happy Path
1. OUT ASYNC read port
2. OUT SYNC read port
3. IN write port
4. INOUT read/write port with WRITE_FIRST
5. INOUT with READ_FIRST
6. INOUT with NO_CHANGE

### 3.2 Negative Testing
1. Invalid port mode combination
2. ASYNC on IN (write) port — Error
3. Write mode on OUT (read) port — Error

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | ASYNC on IN port | Error | MEM_PORT_MODE_INVALID | S7.0 |
| 2 | WRITE_FIRST on OUT port | Error | MEM_PORT_MODE_INVALID | S7.0 |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `parser_mem.c` | MEM port parsing | Token stream |
| `driver_mem.c` | Port mode validation | Unit test |

## 6. Rules Matrix

### 6.1 Rules Tested
| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| MEM_PORT_MODE_INVALID | Invalid port mode combination | Neg 1-3 |

### 6.2 Rules Missing
| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| — | Full coverage in S7.2 and S7.7 | — |
