# Test Plan: 7.11 Synthesis Implications

**Specification Reference:** Section 7.11 of jz-hdl-specification.md

## 1. Objective

Verify FPGA resource mapping rules: BLOCK MEM maps to BSRAM primitives, DISTRIBUTED MEM maps to LUT-based registers, multi-BSRAM tiling is reported, and chip resource capacity limits are enforced.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | BLOCK maps to BSRAM | Small BLOCK MEM within chip capacity | Valid, maps to BSRAM primitives |
| 2 | DISTRIBUTED maps to LUTs | Small DISTRIBUTED MEM within LUT budget | Valid, maps to LUT-based registers |
| 3 | Memory fits within chip resources | MEM sizes within chip BSRAM/LUT limits | Valid, no resource warnings |

### 2.2 Error Cases

| # | Test Case | Input | Expected | Rule ID |
|---|-----------|-------|----------|---------|
| 1 | BLOCK MEM exceeds BSRAM capacity | MEM too large for chip BSRAM count | Error | MEM_BLOCK_RESOURCE_EXCEEDED |
| 2 | DISTRIBUTED MEM exceeds LUT capacity | MEM too large for available LUTs | Error | MEM_DISTRIBUTED_RESOURCE_EXCEEDED |
| 3 | Multi-BSRAM block tiling | BLOCK MEM requires multiple BSRAM primitives | Info | MEM_BLOCK_MULTI |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | MEM exactly at BSRAM limit | MEM uses 100% of available BSRAM | Valid, no error (at limit, not over) |
| 2 | Multiple MEMs sharing BSRAM budget | Two BLOCK MEMs that together exceed capacity | Error: MEM_BLOCK_RESOURCE_EXCEEDED |
| 3 | Chip with no BSRAM | BLOCK MEM on chip without BSRAM primitives | Error: MEM_BLOCK_RESOURCE_EXCEEDED |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| 1 | BLOCK MEM requires multiple BSRAM primitives | Info | MEM_BLOCK_MULTI | info | S7.11: tiling notification |
| 2 | BLOCK MEM exceeds chip BSRAM capacity | Error | MEM_BLOCK_RESOURCE_EXCEEDED | error | S7.11: chip-specific |
| 3 | DISTRIBUTED MEM exceeds LUT capacity | Error | MEM_DISTRIBUTED_RESOURCE_EXCEEDED | error | S7.11: chip-specific |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 7_11_MEM_BLOCK_MULTI-multi_bsram_blocks.jz | MEM_BLOCK_MULTI | BLOCK MEM tiled across multiple BSRAM primitives |
| 7_11_MEM_BLOCK_RESOURCE_EXCEEDED-exceeds_bsram_capacity.jz | MEM_BLOCK_RESOURCE_EXCEEDED | BLOCK MEM exceeds chip BSRAM capacity |
| 7_11_MEM_DISTRIBUTED_RESOURCE_EXCEEDED-exceeds_distributed_capacity.jz | MEM_DISTRIBUTED_RESOURCE_EXCEEDED | DISTRIBUTED MEM exceeds chip LUT capacity |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| MEM_BLOCK_MULTI | info | S7.11 BLOCK MEM requires multiple BSRAM primitives | 7_11_MEM_BLOCK_MULTI-multi_bsram_blocks.jz |
| MEM_BLOCK_RESOURCE_EXCEEDED | error | S7.11 BLOCK MEM exceeds chip BSRAM capacity | 7_11_MEM_BLOCK_RESOURCE_EXCEEDED-exceeds_bsram_capacity.jz |
| MEM_DISTRIBUTED_RESOURCE_EXCEEDED | error | S7.11 DISTRIBUTED MEM exceeds chip LUT capacity | 7_11_MEM_DISTRIBUTED_RESOURCE_EXCEEDED-exceeds_distributed_capacity.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| — | — | All expected rules covered |
