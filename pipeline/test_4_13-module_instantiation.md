# Test Plan: 4.13 Module Instantiation

**Specification Reference:** Section 4.13 (including 4.13.1 Instance Arrays) of jz-hdl-specification.md

## 1. Objective

Verify @new syntax, port binding (IN/OUT/INOUT/BUS), OVERRIDE block, width rules with modifiers (=, =z, =s), no-connect (`_`), port-width validation workflow, instance arrays with IDX keyword, broadcast vs indexed mapping, non-overlap rule for OUT ports, and exclusive assignment compliance.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Simple instantiation | `@new inst mod { IN [8] a = sig; OUT [8] b = res; }` |
| 2 | OVERRIDE constants | `OVERRIDE { WIDTH = 16; }` — child CONST override |
| 3 | No-connect output | `OUT [8] debug = _;` |
| 4 | Width modifier =z | `IN [16] wide_port =z narrow_sig;` |
| 5 | Width modifier =s | `IN [16] signed_port =s narrow_sig;` |
| 6 | Instance array | `@new inst[4] mod { ... }` — 4 instances |
| 7 | IDX in mapping | `OUT [8] data = result[(IDX+1)*8-1:IDX*8];` |
| 8 | Broadcast mapping | `IN [1] clk = clk;` (no IDX — broadcast to all) |
| 9 | Port referencing | `inst.port_name` in ASYNC — valid read |
| 10 | BUS port binding | `BUS SPI TARGET spi = parent_spi;` |
| 11 | Literal tie-off | `IN [2] sel = 2'b11;` |

### 2.2 Error Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Undefined module | `@new inst unknown_mod { ... }` — Error |
| 2 | Missing port | Not all ports listed in @new — Error |
| 3 | Port width mismatch | Port width does not match binding — Error |
| 4 | Port direction mismatch | Direction incompatible with binding — Error |
| 5 | BUS mismatch | BUS type or role mismatch — Error |
| 6 | Undefined OVERRIDE const | OVERRIDE references non-existent child CONST — Error |
| 7 | Port width expr invalid | Bad expression in port width — Error |
| 8 | Parent signal width mismatch | Parent signal width does not match child port — Error |
| 9 | Array count invalid | Non-positive or non-integer array count — Error |
| 10 | IDX in invalid context | IDX used outside instance array mapping — Error |
| 11 | Array parent bit overlap | Two array instances drive same parent bits — Error |
| 12 | IDX slice out of range | IDX expression produces out-of-range slice — Error |
| 13 | Literal on output port | Output port bound to literal — Error |
| 14 | Multi-dimensional array | `@new inst[4][2] mod` — Error |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Array count = 1 | Single instance in array notation |
| 2 | Large array | `@new inst[256] mod { ... }` |
| 3 | CONST in array count | `@new inst[COUNT] mod { ... }` |
| 4 | IDX in complex expression | Bit-slicing with IDX arithmetic |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Undefined module | Error | INSTANCE_UNDEFINED_MODULE | S4.13 |
| 2 | Missing port in @new | Error | INSTANCE_MISSING_PORT | S4.13 |
| 3 | Port width mismatch | Error | INSTANCE_PORT_WIDTH_MISMATCH | S4.13 |
| 4 | Port direction mismatch | Error | INSTANCE_PORT_DIRECTION_MISMATCH | S4.13 |
| 5 | BUS mismatch | Error | INSTANCE_BUS_MISMATCH | S4.13 |
| 6 | Undefined OVERRIDE const | Error | INSTANCE_OVERRIDE_CONST_UNDEFINED | S4.13 |
| 7 | Bad port width expr | Error | INSTANCE_PORT_WIDTH_EXPR_INVALID | S4.13 |
| 8 | Parent signal width mismatch | Error | INSTANCE_PARENT_SIGNAL_WIDTH_MISMATCH | S4.13 |
| 9 | Invalid array count | Error | INSTANCE_ARRAY_COUNT_INVALID | S4.13.1 |
| 10 | IDX in wrong context | Error | INSTANCE_ARRAY_IDX_INVALID_CONTEXT | S4.13.1 |
| 11 | Overlapping OUT bits | Error | INSTANCE_ARRAY_PARENT_BIT_OVERLAP | S4.13.1 |
| 12 | IDX slice out of range | Error | INSTANCE_ARRAY_IDX_SLICE_OUT_OF_RANGE | S4.13.1 |
| 13 | Literal on output | Error | INSTANCE_OUT_PORT_LITERAL | S4.13 |
| 14 | Multi-dimensional array | Error | INSTANCE_ARRAY_MULTI_DIMENSIONAL | S4.13.1 |
| 15 | Valid instantiation | Valid | — | Happy path |

## 4. Existing Validation Tests

| Test File | Rule Tested |
|-----------|-------------|
| 4_13_INSTANCE_ARRAY_COUNT_INVALID-bad_array_count.jz | INSTANCE_ARRAY_COUNT_INVALID |
| 4_13_INSTANCE_ARRAY_IDX_INVALID_CONTEXT-idx_misuse.jz | INSTANCE_ARRAY_IDX_INVALID_CONTEXT |
| 4_13_INSTANCE_ARRAY_IDX_SLICE_OUT_OF_RANGE-idx_slice_oob.jz | INSTANCE_ARRAY_IDX_SLICE_OUT_OF_RANGE |
| 4_13_INSTANCE_ARRAY_PARENT_BIT_OVERLAP-overlapping_out_bits.jz | INSTANCE_ARRAY_PARENT_BIT_OVERLAP |
| 4_13_INSTANCE_MISSING_PORT-incomplete_port_list.jz | INSTANCE_MISSING_PORT |
| 4_13_INSTANCE_OUT_PORT_LITERAL-literal_on_output.jz | INSTANCE_OUT_PORT_LITERAL |
| 4_13_INSTANCE_OVERRIDE_CONST_UNDEFINED-bad_override.jz | INSTANCE_OVERRIDE_CONST_UNDEFINED |
| 4_13_INSTANCE_PORT_DIRECTION_MISMATCH-direction_incompatible.jz | INSTANCE_PORT_DIRECTION_MISMATCH |
| 4_13_INSTANCE_PORT_WIDTH_EXPR_INVALID-bad_width_expr.jz | INSTANCE_PORT_WIDTH_EXPR_INVALID |
| 4_13_INSTANCE_PORT_WIDTH_MISMATCH-port_width_mismatch.jz | INSTANCE_PORT_WIDTH_MISMATCH |
| 4_13_INSTANCE_UNDEFINED_MODULE-nonexistent_module.jz | INSTANCE_UNDEFINED_MODULE |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Test File(s) |
|---------|----------|--------------|
| INSTANCE_ARRAY_COUNT_INVALID | error | 4_13_INSTANCE_ARRAY_COUNT_INVALID-bad_array_count.jz |
| INSTANCE_ARRAY_IDX_INVALID_CONTEXT | error | 4_13_INSTANCE_ARRAY_IDX_INVALID_CONTEXT-idx_misuse.jz |
| INSTANCE_ARRAY_IDX_SLICE_OUT_OF_RANGE | error | 4_13_INSTANCE_ARRAY_IDX_SLICE_OUT_OF_RANGE-idx_slice_oob.jz |
| INSTANCE_ARRAY_PARENT_BIT_OVERLAP | error | 4_13_INSTANCE_ARRAY_PARENT_BIT_OVERLAP-overlapping_out_bits.jz |
| INSTANCE_MISSING_PORT | error | 4_13_INSTANCE_MISSING_PORT-incomplete_port_list.jz |
| INSTANCE_OUT_PORT_LITERAL | error | 4_13_INSTANCE_OUT_PORT_LITERAL-literal_on_output.jz |
| INSTANCE_OVERRIDE_CONST_UNDEFINED | error | 4_13_INSTANCE_OVERRIDE_CONST_UNDEFINED-bad_override.jz |
| INSTANCE_PORT_DIRECTION_MISMATCH | error | 4_13_INSTANCE_PORT_DIRECTION_MISMATCH-direction_incompatible.jz |
| INSTANCE_PORT_WIDTH_EXPR_INVALID | error | 4_13_INSTANCE_PORT_WIDTH_EXPR_INVALID-bad_width_expr.jz |
| INSTANCE_PORT_WIDTH_MISMATCH | error | 4_13_INSTANCE_PORT_WIDTH_MISMATCH-port_width_mismatch.jz |
| INSTANCE_UNDEFINED_MODULE | error | 4_13_INSTANCE_UNDEFINED_MODULE-nonexistent_module.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Gap Description |
|---------|----------|-----------------|
| INSTANCE_BUS_MISMATCH | error | BUS type or role mismatch in port binding |
| INSTANCE_PARENT_SIGNAL_WIDTH_MISMATCH | error | Parent signal width does not match child port width |
| INSTANCE_ARRAY_MULTI_DIMENSIONAL | error | Multi-dimensional instance arrays not supported |
