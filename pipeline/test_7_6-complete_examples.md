# Test Plan: 7.6 Complete Examples

**Specification Reference:** Section 7.6 (7.6.1–7.6.9) of jz-hdl-specification.md

## 1. Objective

Verify that all 9 canonical MEM examples from the specification compile correctly: Simple ROM, Dual-Port Register File, Synchronous FIFO, Registered Read Cache, Triple-Port, Quad-Port, Configurable Memory, Single Port INOUT, True Dual Port.

## 2. Instrumentation Strategy

- **Span: `compile.example`** — Per-example compilation trace.

## 3. Test Scenarios

### 3.1 Happy Path
1. Simple ROM (read-only, ASYNC read) — S7.6.1
2. Dual-Port Register File (1 read, 1 write) — S7.6.2
3. Synchronous FIFO — S7.6.3
4. Registered Read Cache (SYNC read) — S7.6.4
5. Triple-Port (2 read, 1 write) — S7.6.5
6. Quad-Port (2 read, 2 write) — S7.6.6
7. Configurable Memory with parameters — S7.6.7
8. Single Port INOUT — S7.6.8
9. True Dual Port (2× INOUT) — S7.6.9

Each example should compile without errors and produce valid IR.

## 4-6. See individual section plans for detailed matrices.
