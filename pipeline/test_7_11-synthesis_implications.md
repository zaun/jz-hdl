# Test Plan: 7.11 Synthesis Implications

**Specification Reference:** Section 7.11 of jz-hdl-specification.md

## 1. Objective

Verify that MEM type selection (BLOCK vs DISTRIBUTED) maps correctly to FPGA resources, that chip-specific memory constraints are validated, and that the compiler reports resource usage.

## 2. Instrumentation Strategy

- **Span: `backend.mem_synthesis`** — attributes: `mem_id`, `type`, `bsram_count`, `lut_count`.

## 3. Test Scenarios

### 3.1 Happy Path
1. BLOCK maps to BSRAM primitives
2. DISTRIBUTED maps to LUT-based registers
3. Memory fitting within chip resources

### 3.2 Negative Testing
1. BLOCK MEM exceeds chip BSRAM capacity — Warning/Error
2. DISTRIBUTED MEM too large for LUTs — Warning

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | BLOCK exceeds capacity | Warning/Error | — | Chip-specific |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `chip_data.c` | BSRAM/LUT counts | Mock chip data |
| `memory_report.c` | Memory resource report | Verify report output |
| `ir_build_memory.c` | Memory IR | Integration test |

## 6. Rules Matrix

### 6.1 Rules Tested
| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| — | Chip-specific resource checks | Neg 1, 2 |

### 6.2 Rules Missing
| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| MEM_EXCEEDS_BSRAM | S7.11 | MEM too large for chip BSRAM |
