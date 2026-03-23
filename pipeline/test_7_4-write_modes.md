# Test Plan: 7.4 Write Modes

**Specification Reference:** Section 7.4 of jz-hdl-specification.md

## 1. Objective

Verify WRITE_FIRST (write-then-read), READ_FIRST (read-then-write), and NO_CHANGE (hold output during write) modes for INOUT ports, and their behavioral differences during simultaneous read/write.

## 2. Instrumentation Strategy

- **Span: `sem.mem_write_mode`** — attributes: `port_name`, `mode`, `collision_behavior`.

## 3. Test Scenarios

### 3.1 Happy Path
1. WRITE_FIRST: simultaneous read returns new data
2. READ_FIRST: simultaneous read returns old data
3. NO_CHANGE: output holds previous value during write

### 3.2 Boundary/Edge Cases
1. Write without simultaneous read (all modes identical)

### 3.3 Negative Testing
1. Invalid write mode keyword — Error
2. Write mode on OUT port — Error

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Invalid mode | Error | MEM_WRITE_MODE_INVALID | S7.4 |
| 2 | Write mode on OUT | Error | MEM_WRITE_MODE_ON_READ_PORT | S7.4 |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `driver_mem.c` | Write mode validation | Unit test |
| `sim_exec.c` | Simulation of write modes | Simulation comparison |
| `ir_build_memory.c` | IR generation per mode | IR verification |

## 6. Rules Matrix

### 6.1 Rules Tested
| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| MEM_WRITE_MODE_INVALID | Unknown write mode | Neg 1 |

### 6.2 Rules Missing
| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| MEM_WRITE_MODE_ON_READ_PORT | S7.4 | Write mode on read-only port |
