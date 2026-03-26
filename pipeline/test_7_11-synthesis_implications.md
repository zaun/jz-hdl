# Test Plan: 7.11 Synthesis Implications

**Specification Reference:** Section 7.11 of jz-hdl-specification.md

## 1. Objective

Verify FPGA resource mapping rules: BLOCK MEM maps to BSRAM primitives, DISTRIBUTED MEM maps to LUT-based registers, multi-BSRAM tiling is reported via INFO diagnostic, and chip resource capacity limits are enforced for both BLOCK and DISTRIBUTED memory types.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | BLOCK maps to BSRAM | Small BLOCK MEM within chip capacity | Valid, maps to BSRAM primitives |
| 2 | DISTRIBUTED maps to LUTs | Small DISTRIBUTED MEM within LUT budget | Valid, maps to LUT-based registers |
| 3 | Memory fits within chip resources | MEM sizes within chip BSRAM/LUT limits | Valid, no resource warnings |
| 4 | BSRAM mode mapping: OUT-only (ROM) | MEM with only OUT ports | Valid, maps to Read Only Memory mode |
| 5 | BSRAM mode mapping: INOUT x1 (Single Port) | MEM with single INOUT port | Valid, maps to Single Port mode |
| 6 | BSRAM mode mapping: IN+OUT (Semi-Dual Port) | MEM with IN and OUT ports | Valid, maps to Semi-Dual Port mode |
| 7 | BSRAM mode mapping: INOUT x2 (Dual Port) | MEM with two INOUT ports | Valid, maps to Dual Port mode |

### 2.2 Error Cases

| # | Test Case | Input | Expected | Rule ID |
|---|-----------|-------|----------|---------|
| 1 | BLOCK MEM exceeds BSRAM capacity | MEM too large for chip BSRAM count | Error | MEM_BLOCK_RESOURCE_EXCEEDED |
| 2 | DISTRIBUTED MEM exceeds LUT capacity | MEM too large for available LUTs | Error | MEM_DISTRIBUTED_RESOURCE_EXCEEDED |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Multi-BSRAM block tiling | BLOCK MEM requires multiple BSRAM primitives | Info: MEM_BLOCK_MULTI |
| 2 | MEM exactly at BSRAM limit | MEM uses 100% of available BSRAM | Valid, no error (at limit, not over) |
| 3 | Multiple MEMs sharing BSRAM budget | Two BLOCK MEMs that together exceed capacity | Error: MEM_BLOCK_RESOURCE_EXCEEDED |
| 4 | Chip with no BSRAM | BLOCK MEM on chip without BSRAM primitives | Error: MEM_BLOCK_RESOURCE_EXCEEDED |
| 5 | GENERIC chip (no chip-specific validation) | MEM on GENERIC chip | Valid, no resource checks |

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1 | BLOCK MEM requires multiple BSRAM primitives | Large BLOCK MEM | MEM_BLOCK_MULTI | info |
| 2 | BLOCK MEM exceeds chip BSRAM capacity | BLOCK MEM larger than all available BSRAM | MEM_BLOCK_RESOURCE_EXCEEDED | error |
| 3 | DISTRIBUTED MEM exceeds LUT capacity | DISTRIBUTED MEM larger than available LUTs | MEM_DISTRIBUTED_RESOURCE_EXCEEDED | error |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 7_11_HAPPY_PATH-synthesis_ok.jz | — | Happy path: MEM within chip resource limits |
| 7_11_MEM_BLOCK_MULTI-multi_bsram_blocks.jz | MEM_BLOCK_MULTI | BLOCK MEM tiled across multiple BSRAM primitives |
| 7_11_MEM_BLOCK_RESOURCE_EXCEEDED-exceeds_bsram_capacity.jz | MEM_BLOCK_RESOURCE_EXCEEDED | BLOCK MEM exceeds chip BSRAM capacity |
| 7_11_MEM_DISTRIBUTED_RESOURCE_EXCEEDED-exceeds_distributed_capacity.jz | MEM_DISTRIBUTED_RESOURCE_EXCEEDED | DISTRIBUTED MEM exceeds chip LUT capacity |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| MEM_BLOCK_MULTI | info | S7.1 BLOCK MEM requires multiple BSRAM primitives | 7_11_MEM_BLOCK_MULTI-multi_bsram_blocks.jz |
| MEM_BLOCK_RESOURCE_EXCEEDED | error | S7.1/S6.1 BLOCK MEM exceeds chip BSRAM capacity | 7_11_MEM_BLOCK_RESOURCE_EXCEEDED-exceeds_bsram_capacity.jz |
| MEM_DISTRIBUTED_RESOURCE_EXCEEDED | error | S7.1/S6.1 DISTRIBUTED MEM exceeds chip LUT capacity | 7_11_MEM_DISTRIBUTED_RESOURCE_EXCEEDED-exceeds_distributed_capacity.jz |

### 5.2 Rules Not Tested


All rules for this section are tested.
