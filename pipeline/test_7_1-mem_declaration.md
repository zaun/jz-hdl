# Test Plan: 7.1 MEM Declaration

**Specification Reference:** Section 7.1 of jz-hdl-specification.md

## 1. Objective

Verify MEM declaration: type (BLOCK/DISTRIBUTED), word width, depth, initialization, port declarations, CONST in width/depth, and validation errors.

## 2. Instrumentation Strategy

- **Span: `sem.mem_decl`** — attributes: `mem_id`, `type`, `word_width`, `depth`, `port_count`, `init_type`.

## 3. Test Scenarios

### 3.1 Happy Path
1. BLOCK type: `MEM(type=BLOCK) { m [8] [256] = 32'h0 { OUT rd SYNC; IN wr; }; }`
2. DISTRIBUTED type
3. CONST width and depth
4. Multiple ports (2 read, 1 write)
5. Multiple MEM blocks in one module

### 3.2 Boundary/Edge Cases
1. Minimum depth: 1 entry
2. Large depth: 65536 entries
3. 1-bit word width

### 3.3 Negative Testing
1. Invalid type keyword — Error
2. Depth = 0 — Error
3. Width = 0 — Error
4. No ports — Error
5. Duplicate MEM name — Error
6. MEM with multi-dimensional word — Error

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | `MEM(type=INVALID)` | Error | MEM_DECL_TYPE_INVALID | S7.1 |
| 2 | Depth = 0 | Error | MEM_DECL_DEPTH_ZERO | S7.1 |
| 3 | Width = 0 | Error | MEM_DECL_WIDTH_ZERO | S7.1 |
| 4 | No ports declared | Error | MEM_DECL_NO_PORTS | S7.1 |
| 5 | Duplicate MEM name | Error | SCOPE_DUPLICATE_SIGNAL | S4.2 |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `parser_mem.c` | MEM parsing | Token stream |
| `driver_mem.c` | Declaration validation | Unit test |
| `const_eval.c` | CONST evaluation for width/depth | Mock |

## 6. Rules Matrix

### 6.1 Rules Tested
| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| MEM_DECL_TYPE_INVALID | Invalid storage type | Neg 1 |
| MEM_DECL_DEPTH_ZERO | Zero depth | Neg 2 |
| MEM_DECL_WIDTH_ZERO | Zero width | Neg 3 |
| MEM_DECL_NO_PORTS | No port declarations | Neg 4 |

### 6.2 Rules Missing
| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| — | See S7.7 for comprehensive error list | — |
