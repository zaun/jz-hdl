# Test Plan: 4.13 Module Instantiation

**Specification Reference:** Section 4.13 (including 4.13.1 Instance Arrays) of jz-hdl-specification.md

## 1. Objective

Verify @new syntax, port binding (IN/OUT/INOUT/BUS), OVERRIDE block, width rules with modifiers (=, =z, =s), no-connect (`_`), port-width validation workflow, instance arrays with IDX keyword, broadcast vs indexed mapping, non-overlap rule for OUT ports, and exclusive assignment compliance.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Simple instantiation | `@new inst mod { IN [8] a = sig; OUT [8] b = res; }` |
| 2 | OVERRIDE constants | `OVERRIDE { WIDTH = 16; }` -- child CONST override |
| 3 | No-connect output | `OUT [8] debug = _;` |
| 4 | Width modifier =z | `IN [16] wide_port =z narrow_sig;` |
| 5 | Width modifier =s | `IN [16] signed_port =s narrow_sig;` |
| 6 | Instance array | `@new inst[4] mod { ... }` -- 4 instances |
| 7 | IDX in mapping | `OUT [8] data = result[(IDX+1)*8-1:IDX*8];` |
| 8 | Broadcast mapping | `IN [1] clk = clk;` (no IDX -- broadcast to all) |
| 9 | Port referencing | `inst.port_name` in ASYNC -- valid read |
| 10 | BUS port binding | `BUS SPI TARGET spi = parent_spi;` |
| 11 | Literal tie-off | `IN [2] sel = 2'b11;` |

### 2.2 Error Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Undefined module | `@new inst unknown_mod { ... }` -- Error |
| 2 | Missing port | Not all ports listed in @new -- Error |
| 3 | Port width mismatch | Port width does not match binding -- Error |
| 4 | Port direction mismatch | Direction incompatible with binding -- Error |
| 5 | BUS mismatch | BUS type or role mismatch -- Error |
| 6 | Undefined OVERRIDE const | OVERRIDE references non-existent child CONST -- Error |
| 7 | Port width expr invalid | Bad expression in port width -- Error |
| 8 | Parent signal width mismatch | Parent signal width does not match child port -- Error |
| 9 | Array count invalid | Non-positive or non-integer array count -- Error |
| 10 | IDX in invalid context | IDX used outside instance array mapping -- Error |
| 11 | Array parent bit overlap | Two array instances drive same parent bits -- Error |
| 12 | IDX slice out of range | IDX expression produces out-of-range slice -- Error |
| 13 | Literal on output port | Output port bound to literal -- Error |
| 14 | Multi-dimensional array | `@new inst[4][2] mod` -- Error |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Array count = 1 | Single instance in array notation |
| 2 | Large array | `@new inst[256] mod { ... }` |
| 3 | CONST in array count | `@new inst[COUNT] mod { ... }` |
| 4 | IDX in complex expression | Bit-slicing with IDX arithmetic |

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1 | Undefined module | `@new inst unknown_mod { ... }` | INSTANCE_UNDEFINED_MODULE | error |
| 2 | Missing port in @new | Not all ports listed | INSTANCE_MISSING_PORT | error |
| 3 | Port width mismatch | `IN [16] a = sig;` but child declares `IN [8] a` | INSTANCE_PORT_WIDTH_MISMATCH | error |
| 4 | Port direction mismatch | Direction incompatible | INSTANCE_PORT_DIRECTION_MISMATCH | error |
| 5 | BUS mismatch | BUS type/role/count mismatch | INSTANCE_BUS_MISMATCH | error |
| 6 | Undefined OVERRIDE const | `OVERRIDE { NONEXIST = 1; }` | INSTANCE_OVERRIDE_CONST_UNDEFINED | error |
| 7 | Bad port width expr | Invalid expression in port width | INSTANCE_PORT_WIDTH_EXPR_INVALID | error |
| 8 | Parent signal width mismatch | Parent signal width differs from port | INSTANCE_PARENT_SIGNAL_WIDTH_MISMATCH | error |
| 9 | Invalid array count | `@new inst[0] mod` | INSTANCE_ARRAY_COUNT_INVALID | error |
| 10 | IDX in wrong context | IDX outside instance array | INSTANCE_ARRAY_IDX_INVALID_CONTEXT | error |
| 11 | Overlapping OUT bits | Two instances drive same parent bits | INSTANCE_ARRAY_PARENT_BIT_OVERLAP | error |
| 12 | IDX slice out of range | IDX produces out-of-range slice | INSTANCE_ARRAY_IDX_SLICE_OUT_OF_RANGE | error |
| 13 | Literal on output | `OUT [8] data = 8'h00;` | INSTANCE_OUT_PORT_LITERAL | error |
| 14 | Multi-dimensional array | `@new inst[4][2] mod` | INSTANCE_ARRAY_MULTI_DIMENSIONAL | error |
| 15 | Valid instantiation | All ports bound, widths match | -- | -- (pass) |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 4_13_INSTANCE_HAPPY_PATH-valid_instantiation_ok.jz | -- | Happy path: valid module instantiation |
| 4_13_INSTANCE_ARRAY_COUNT_INVALID-bad_array_count.jz | INSTANCE_ARRAY_COUNT_INVALID | Instance array count not positive integer |
| 4_13_INSTANCE_ARRAY_IDX_INVALID_CONTEXT-idx_misuse.jz | INSTANCE_ARRAY_IDX_INVALID_CONTEXT | IDX used outside instance array port mapping |
| 4_13_INSTANCE_ARRAY_IDX_SLICE_OUT_OF_RANGE-idx_slice_oob.jz | INSTANCE_ARRAY_IDX_SLICE_OUT_OF_RANGE | IDX expression produces out-of-range slice |
| 4_13_INSTANCE_ARRAY_MULTI_DIMENSIONAL-multi_dim_array.jz | INSTANCE_ARRAY_MULTI_DIMENSIONAL | Multi-dimensional instance array |
| 4_13_INSTANCE_ARRAY_PARENT_BIT_OVERLAP-overlapping_out_bits.jz | INSTANCE_ARRAY_PARENT_BIT_OVERLAP | Two array instances drive overlapping parent bits |
| 4_13_INSTANCE_BUS_MISMATCH-bus_mismatch.jz | INSTANCE_BUS_MISMATCH | BUS type/role mismatch in instance binding |
| 4_13_INSTANCE_MISSING_PORT-incomplete_port_list.jz | INSTANCE_MISSING_PORT | Not all module ports listed in @new |
| 4_13_INSTANCE_OUT_PORT_LITERAL-literal_on_output.jz | INSTANCE_OUT_PORT_LITERAL | Output port bound to literal value |
| 4_13_INSTANCE_OVERRIDE_CONST_UNDEFINED-bad_override.jz | INSTANCE_OVERRIDE_CONST_UNDEFINED | OVERRIDE references non-existent child CONST |
| 4_13_INSTANCE_PARENT_SIGNAL_WIDTH_MISMATCH-parent_width_mismatch.jz | INSTANCE_PARENT_SIGNAL_WIDTH_MISMATCH | Parent signal width mismatch |
| 4_13_INSTANCE_PORT_DIRECTION_MISMATCH-direction_incompatible.jz | INSTANCE_PORT_DIRECTION_MISMATCH | Port direction incompatible |
| 4_13_INSTANCE_PORT_WIDTH_EXPR_INVALID-bad_width_expr.jz | INSTANCE_PORT_WIDTH_EXPR_INVALID | Invalid port width expression |
| 4_13_INSTANCE_PORT_WIDTH_MISMATCH-port_width_mismatch.jz | INSTANCE_PORT_WIDTH_MISMATCH | Port width mismatch between @new and module declaration |
| 4_13_INSTANCE_UNDEFINED_MODULE-nonexistent_module.jz | INSTANCE_UNDEFINED_MODULE | @new references non-existent module |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| INSTANCE_ARRAY_COUNT_INVALID | error | S4.13.1 Instance array count must be a positive integer | 4_13_INSTANCE_ARRAY_COUNT_INVALID-bad_array_count.jz |
| INSTANCE_ARRAY_IDX_INVALID_CONTEXT | error | S4.13.1 IDX keyword used outside instance array port mapping | 4_13_INSTANCE_ARRAY_IDX_INVALID_CONTEXT-idx_misuse.jz |
| INSTANCE_ARRAY_IDX_SLICE_OUT_OF_RANGE | error | S4.13.1 IDX expression produces an out-of-range slice in parent signal | 4_13_INSTANCE_ARRAY_IDX_SLICE_OUT_OF_RANGE-idx_slice_oob.jz |
| INSTANCE_ARRAY_PARENT_BIT_OVERLAP | error | S4.13.1 Two array instances drive overlapping bits in the same parent signal | 4_13_INSTANCE_ARRAY_PARENT_BIT_OVERLAP-overlapping_out_bits.jz |
| INSTANCE_BUS_MISMATCH | error | S4.13/S6.9 BUS type or role mismatch in instance port binding | 4_13_INSTANCE_BUS_MISMATCH-bus_mismatch.jz |
| INSTANCE_MISSING_PORT | error | S4.13/S6.9 Not all module ports listed in @new binding | 4_13_INSTANCE_MISSING_PORT-incomplete_port_list.jz |
| INSTANCE_OUT_PORT_LITERAL | error | S4.13 Output port bound to a literal value | 4_13_INSTANCE_OUT_PORT_LITERAL-literal_on_output.jz |
| INSTANCE_OVERRIDE_CONST_UNDEFINED | error | S4.13 OVERRIDE block references a CONST that does not exist in the child module | 4_13_INSTANCE_OVERRIDE_CONST_UNDEFINED-bad_override.jz |
| INSTANCE_PARENT_SIGNAL_WIDTH_MISMATCH | error | S4.13/S8.1 Parent signal width does not match child port width | 4_13_INSTANCE_PARENT_SIGNAL_WIDTH_MISMATCH-parent_width_mismatch.jz |
| INSTANCE_PORT_DIRECTION_MISMATCH | error | S4.13/S6.9 Port binding direction is incompatible with the port declaration | 4_13_INSTANCE_PORT_DIRECTION_MISMATCH-direction_incompatible.jz |
| INSTANCE_PORT_WIDTH_EXPR_INVALID | error | S4.13 Invalid expression used for port width in @new binding | 4_13_INSTANCE_PORT_WIDTH_EXPR_INVALID-bad_width_expr.jz |
| INSTANCE_PORT_WIDTH_MISMATCH | error | S4.13/S6.9/S8.1 Port width in @new does not match module declaration | 4_13_INSTANCE_PORT_WIDTH_MISMATCH-port_width_mismatch.jz |
| INSTANCE_UNDEFINED_MODULE | error | S4.13/S6.9 @new references a module that does not exist in the project | 4_13_INSTANCE_UNDEFINED_MODULE-nonexistent_module.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| INSTANCE_ARRAY_MULTI_DIMENSIONAL | error | Dead code: test exists (`4_13_INSTANCE_ARRAY_MULTI_DIMENSIONAL-multi_dim_array.jz`) but rule is dead code |
