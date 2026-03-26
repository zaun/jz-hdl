# Test Plan: 7.6 Complete Examples

**Specification Reference:** Section 7.6 (7.6.1-7.6.9) of jz-hdl-specification.md

## 1. Objective

Verify that all 9 canonical MEM examples from the specification compile without errors and produce valid IR. These examples exercise rules from Sections 7.1-7.5 in combination. No new rules are introduced in this section.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Simple ROM (S7.6.1) | Read-only MEM with single ASYNC read port, @file init | Valid, compiles without errors |
| 2 | Dual-Port Register File (S7.6.2) | 2 ASYNC read ports + 1 write port, literal init | Valid, compiles without errors |
| 3 | Synchronous FIFO (S7.6.3) | 1 ASYNC read + 1 write, pointer-based addressing | Valid, compiles without errors |
| 4 | Registered Read Cache (S7.6.4) | 1 SYNC read + 1 write port | Valid, compiles without errors |
| 5 | Triple-Port (S7.6.5) | 2 ASYNC read ports + 1 write port | Valid, compiles without errors |
| 6 | Quad-Port (S7.6.6) | 2 ASYNC read ports + 2 write ports | Valid, compiles without errors |
| 7 | Configurable Memory (S7.6.7) | CONST-parameterized width/depth, SYNC read + write | Valid, compiles without errors |
| 8 | Single Port INOUT (S7.6.8) | Single INOUT port, TYPE=BLOCK | Valid, compiles without errors |
| 9 | True Dual Port (S7.6.9) | 2 INOUT ports, TYPE=BLOCK | Valid, compiles without errors |

### 2.2 Error Cases

No error cases specific to this section. Examples are canonical happy-path demonstrations. Error cases are covered by Sections 7.1-7.5 and 7.7.

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Configurable memory at minimum dimensions | WIDTH=1, DEPTH=1 | Valid, compiles without errors |
| 2 | True Dual Port simultaneous access pattern | Both INOUT ports active in same SYNC block | Valid, no conflict with distinct port names |

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1-9 | Canonical spec examples | Complete module declarations | (none) | -- |
| 10 | Minimum dimensions edge case | WIDTH=1, DEPTH=1 MEM | (none) | -- |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 7_6_HAPPY_PATH-simple_rom.jz | — | S7.6.1: Simple ROM with ASYNC read |
| 7_6_HAPPY_PATH-dual_port_regfile.jz | — | S7.6.2: Dual-port register file (2R1W) |
| 7_6_HAPPY_PATH-sync_fifo.jz | — | S7.6.3: Synchronous FIFO |
| 7_6_HAPPY_PATH-registered_read_cache.jz | — | S7.6.4: Registered read cache with SYNC read |
| 7_6_HAPPY_PATH-triple_port.jz | — | S7.6.5: Triple-port memory (2R1W) |
| 7_6_HAPPY_PATH-quad_port.jz | — | S7.6.6: Quad-port memory (2R2W) |
| 7_6_HAPPY_PATH-configurable_mem.jz | — | S7.6.7: Configurable memory with CONST parameters |
| 7_6_HAPPY_PATH-single_port_inout.jz | — | S7.6.8: Single-port INOUT |
| 7_6_HAPPY_PATH-true_dual_port.jz | — | S7.6.9: True dual-port (2x INOUT) |
| 7_6_HAPPY_PATH-min_dimensions.jz | — | Edge case: minimum dimensions (WIDTH=1, DEPTH=1) |

## 5. Rules Matrix

### 5.1 Rules Tested

No new rules introduced. This is a happy-path-only plan. The tests exercise rules from Sections 7.1-7.5 in combination.

### 5.2 Rules Not Tested


All rules for this section are tested.
