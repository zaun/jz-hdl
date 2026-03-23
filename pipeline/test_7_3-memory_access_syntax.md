# Test Plan: 7.3 Memory Access Syntax (7.3.1–7.3.4)

**Specification Reference:** Section 7.3 of jz-hdl-specification.md

## 1. Objective

Verify memory access syntax: async read (`mem.port[addr]`), sync read, sync write (`mem.port[addr] <= wdata;`), INOUT access, address width validation, and block context requirements.

## 2. Instrumentation Strategy

- **Span: `sem.mem_access`** — attributes: `mem_id`, `port_name`, `access_type` (read/write), `addr_width`.

## 3. Test Scenarios

### 3.1 Happy Path
1. Async read: `data <= rom.rd[addr];` in ASYNC
2. Sync read: `data <= cache.rd[addr];` in SYNC
3. Sync write: `ram.wr[addr] <= data;` in SYNC
4. INOUT access: `ram.rw[addr] <= wdata;` / `rdata <= ram.rw[addr];`

### 3.2 Boundary/Edge Cases
1. Address = 0 (first entry)
2. Address = depth-1 (last entry)
3. Wide address (16-bit)

### 3.3 Negative Testing
1. Async read on SYNC port — Error
2. Sync write in ASYNC block — Error
3. Address width mismatch — Error
4. Access to undeclared port — Error
5. Write to read-only port — Error

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Async read on SYNC port | Error | MEM_ACCESS_WRONG_MODE | S7.3 |
| 2 | Write in ASYNC | Error | MEM_ACCESS_WRONG_BLOCK | S7.3 |
| 3 | Address width wrong | Error | MEM_ACCESS_ADDR_WIDTH | S7.3 |
| 4 | Undeclared port | Error | MEM_ACCESS_PORT_UNDEFINED | S7.3 |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `driver_mem.c` | Access validation | Integration test |
| `parser_statements.c` | Access syntax parsing | Token stream |

## 6. Rules Matrix

### 6.1 Rules Tested
| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| MEM_ACCESS_WRONG_MODE | Wrong read mode | Neg 1 |
| MEM_ACCESS_WRONG_BLOCK | Access in wrong block type | Neg 2 |

### 6.2 Rules Missing
| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| MEM_ACCESS_ADDR_WIDTH | S7.3 | Address width validation |
| MEM_ACCESS_PORT_UNDEFINED | S7.3 | Nonexistent port |
