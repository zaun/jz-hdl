# Test Plan: 7.1 MEM Declaration

**Specification Reference:** Section 7.1 of jz-hdl-specification.md

## 1. Objective

Verify MEM declarations including word width, depth, port lists, storage types (BLOCK/DISTRIBUTED), CONST usage in width/depth, initialization requirements, scope and uniqueness rules, automatic type inference, and all declaration-level validation errors.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | BLOCK type MEM | `MEM(TYPE=BLOCK) { m [8] [256] = 8'h0 { OUT rd SYNC; IN wr; }; }` | Valid declaration |
| 2 | DISTRIBUTED type MEM | `MEM(TYPE=DISTRIBUTED) { m [4] [16] = 4'h0 { OUT rd ASYNC; }; }` | Valid declaration |
| 3 | CONST width and depth | `MEM { m [WIDTH] [DEPTH] = ... { ... }; }` with CONST definitions | Valid declaration |
| 4 | Multiple ports | `OUT rd1 SYNC; OUT rd2 ASYNC; IN wr;` | Valid multi-port MEM |
| 5 | Multiple MEM blocks in one module | Two separate MEM blocks with distinct names | Valid |
| 6 | Auto type inference (depth <= 16) | `MEM { m [8] [16] = 8'h0 { OUT rd ASYNC; }; }` | Inferred as DISTRIBUTED |
| 7 | Auto type inference (depth > 16, all SYNC OUT) | `MEM { m [8] [256] = 8'h0 { OUT rd SYNC; IN wr; }; }` | Inferred as BLOCK |
| 8 | Address width auto-calculated | `MEM { m [8] [256] ... }` | Address width = ceil(log2(256)) = 8 bits |
| 9 | Minimum depth (1 word) | `MEM { m [8] [1] = 8'h0 { OUT rd ASYNC; }; }` | Valid, address width = 1 bit |

### 2.2 Error Cases

| # | Test Case | Input | Expected | Rule ID |
|---|-----------|-------|----------|---------|
| 1 | Duplicate MEM name | Two MEM entries with same name | Error | MEM_DUP_NAME |
| 2 | Zero word width | `m [0] [256]` | Error | MEM_INVALID_WORD_WIDTH |
| 3 | Zero depth | `m [8] [0]` | Error | MEM_INVALID_DEPTH |
| 4 | Undefined CONST in width | `m [UNDEF] [256]` | Error | MEM_UNDEFINED_CONST_IN_WIDTH |
| 5 | Duplicate port name | `OUT rd SYNC; OUT rd ASYNC;` | Error | MEM_DUP_PORT_NAME |
| 6 | Port name conflicts with module identifier | Port name matches a module-level signal | Error | MEM_PORT_NAME_CONFLICT_MODULE_ID |
| 7 | No ports declared | `m [8] [256] = 8'h0 { };` | Error | MEM_EMPTY_PORT_LIST |
| 8 | Invalid port type keyword | Unknown direction keyword | Error | MEM_INVALID_PORT_TYPE |
| 9 | Invalid type keyword | `MEM(TYPE=INVALID)` | Error | MEM_TYPE_INVALID |
| 10 | BLOCK type with ASYNC OUT port | `MEM(TYPE=BLOCK) { m ... { OUT rd ASYNC; }; }` | Error | MEM_TYPE_BLOCK_WITH_ASYNC_OUT |
| 11 | Unsupported chip config for MEM | MEM configuration not supported by target chip | Error | MEM_CHIP_CONFIG_UNSUPPORTED |
| 12 | INOUT mixed with separate IN/OUT | `OUT rd SYNC; INOUT rw;` in same MEM | Error | MEM_INOUT_MIXED_WITH_IN_OUT |
| 13 | ASYNC keyword on INOUT port | `INOUT rw ASYNC;` | Error | MEM_INOUT_ASYNC |
| 14 | Missing initialization | `m [8] [256] { OUT rd SYNC; };` | Error | MEM_MISSING_INIT |
| 15 | Undefined MEM name in access | `undef.port[addr]` | Error | MEM_UNDEFINED_NAME |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Minimum depth (1 entry) | `m [8] [1] = 8'h0 { ... };` | Valid, 1-bit address |
| 2 | Large depth (65536 entries) | `m [8] [65536] = 8'h0 { ... };` | Valid |
| 3 | 1-bit word width | `m [1] [256] = 1'b0 { ... };` | Valid |
| 4 | Non-power-of-2 depth | `m [8] [7] = 8'h0 { ... };` | Valid, address width = 3 bits |
| 5 | CONST expression for depth | `m [8] [4 * MULT] = 8'h0 { ... };` | Valid if MULT is defined |

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1 | Duplicate MEM name in same module | Two MEM entries named `m` | MEM_DUP_NAME | error |
| 2 | Word width of 0 | `m [0] [256]` | MEM_INVALID_WORD_WIDTH | error |
| 3 | Depth of 0 | `m [8] [0]` | MEM_INVALID_DEPTH | error |
| 4 | Undefined CONST used for width/depth | `m [UNDEF] [256]` | MEM_UNDEFINED_CONST_IN_WIDTH | error |
| 5 | Two ports with same name | `OUT rd SYNC; OUT rd ASYNC;` | MEM_DUP_PORT_NAME | error |
| 6 | Port name clashes with module identifier | MEM port named same as module port | MEM_PORT_NAME_CONFLICT_MODULE_ID | error |
| 7 | Empty port list | `m [8] [256] = 8'h0 { };` | MEM_EMPTY_PORT_LIST | error |
| 8 | Invalid port type keyword | Unknown port direction | MEM_INVALID_PORT_TYPE | error |
| 9 | Invalid storage type keyword | `MEM(TYPE=INVALID)` | MEM_TYPE_INVALID | error |
| 10 | BLOCK with ASYNC OUT | `MEM(TYPE=BLOCK) { ... OUT rd ASYNC; }` | MEM_TYPE_BLOCK_WITH_ASYNC_OUT | error |
| 11 | Chip does not support MEM config | MEM config vs chip data mismatch | MEM_CHIP_CONFIG_UNSUPPORTED | error |
| 12 | INOUT port mixed with IN/OUT ports | `OUT rd SYNC; INOUT rw;` | MEM_INOUT_MIXED_WITH_IN_OUT | error |
| 13 | ASYNC on INOUT port | `INOUT rw ASYNC;` | MEM_INOUT_ASYNC | error |
| 14 | No initialization expression | `m [8] [256] { ... };` | MEM_MISSING_INIT | error |
| 15 | Access to undeclared MEM name | `undef.port[addr]` | MEM_UNDEFINED_NAME | error |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 7_1_HAPPY_PATH-mem_declaration_ok.jz | — | Happy path: valid MEM declarations with various configurations |
| 7_1_MEM_CHIP_CONFIG_UNSUPPORTED-unsupported_chip.jz | MEM_CHIP_CONFIG_UNSUPPORTED | MEM configuration not supported by target chip |
| 7_1_MEM_DUP_NAME-duplicate_mem_name.jz | MEM_DUP_NAME | Duplicate MEM name within the same module |
| 7_1_MEM_DUP_PORT_NAME-duplicate_port_name.jz | MEM_DUP_PORT_NAME | Duplicate port name in MEM block |
| 7_1_MEM_EMPTY_PORT_LIST-no_ports.jz | MEM_EMPTY_PORT_LIST | MEM declared with no ports |
| 7_1_MEM_INOUT_ASYNC-inout_async.jz | MEM_INOUT_ASYNC | ASYNC keyword on INOUT port declaration |
| 7_1_MEM_INOUT_MIXED_WITH_IN_OUT-mixed_inout.jz | MEM_INOUT_MIXED_WITH_IN_OUT | INOUT port mixed with separate IN/OUT ports |
| 7_1_MEM_INVALID_DEPTH-zero_depth.jz | MEM_INVALID_DEPTH | MEM depth of zero |
| 7_1_MEM_INVALID_PORT_TYPE-invalid_port_type.jz | MEM_INVALID_PORT_TYPE | Invalid port type keyword |
| 7_1_MEM_INVALID_WORD_WIDTH-zero_width.jz | MEM_INVALID_WORD_WIDTH | MEM word width of zero |
| 7_1_MEM_MISSING_INIT-missing_init.jz | MEM_MISSING_INIT | No initialization expression on MEM |
| 7_1_MEM_PORT_NAME_CONFLICT_MODULE_ID-port_name_conflict.jz | MEM_PORT_NAME_CONFLICT_MODULE_ID | Port name conflicts with module identifier |
| 7_1_MEM_TYPE_BLOCK_WITH_ASYNC_OUT-block_async_out.jz | MEM_TYPE_BLOCK_WITH_ASYNC_OUT | BLOCK type MEM declared with ASYNC OUT port |
| 7_1_MEM_TYPE_INVALID-bad_type_keyword.jz | MEM_TYPE_INVALID | Invalid storage type keyword |
| 7_1_MEM_UNDEFINED_CONST_IN_WIDTH-undefined_const.jz | MEM_UNDEFINED_CONST_IN_WIDTH | Undefined CONST used in MEM width/depth |
| 7_1_MEM_UNDEFINED_NAME-undefined_mem_name.jz | MEM_UNDEFINED_NAME | Access to undeclared MEM name |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| MEM_CHIP_CONFIG_UNSUPPORTED | error | S7.1/S6.1 MEM configuration not supported by target chip | 7_1_MEM_CHIP_CONFIG_UNSUPPORTED-unsupported_chip.jz |
| MEM_DUP_NAME | error | S7.1/S7.7.1 Duplicate MEM name in same module | 7_1_MEM_DUP_NAME-duplicate_mem_name.jz |
| MEM_DUP_PORT_NAME | error | S7.1/S7.7.1 Duplicate port name in MEM block | 7_1_MEM_DUP_PORT_NAME-duplicate_port_name.jz |
| MEM_EMPTY_PORT_LIST | error | S7.1/S7.7.1 No ports declared in MEM block | 7_1_MEM_EMPTY_PORT_LIST-no_ports.jz |
| MEM_INOUT_ASYNC | error | S7.1 ASYNC keyword on INOUT port declaration | 7_1_MEM_INOUT_ASYNC-inout_async.jz |
| MEM_INOUT_MIXED_WITH_IN_OUT | error | S7.1 INOUT port mixed with separate IN/OUT ports | 7_1_MEM_INOUT_MIXED_WITH_IN_OUT-mixed_inout.jz |
| MEM_INVALID_DEPTH | error | S7.1/S7.7.1 Depth is zero or invalid | 7_1_MEM_INVALID_DEPTH-zero_depth.jz |
| MEM_INVALID_PORT_TYPE | error | S7.1/S7.7.1 Invalid port type keyword | 7_1_MEM_INVALID_PORT_TYPE-invalid_port_type.jz |
| MEM_INVALID_WORD_WIDTH | error | S7.1/S7.7.1 Word width is zero or invalid | 7_1_MEM_INVALID_WORD_WIDTH-zero_width.jz |
| MEM_MISSING_INIT | error | S7.5.1/S7.7.1/S7.7.3 No initialization expression on MEM | 7_1_MEM_MISSING_INIT-missing_init.jz |
| MEM_PORT_NAME_CONFLICT_MODULE_ID | error | S7.1/S7.7.1 Port name conflicts with module identifier | 7_1_MEM_PORT_NAME_CONFLICT_MODULE_ID-port_name_conflict.jz |
| MEM_TYPE_BLOCK_WITH_ASYNC_OUT | error | S7.1 BLOCK type MEM declared with ASYNC OUT port | 7_1_MEM_TYPE_BLOCK_WITH_ASYNC_OUT-block_async_out.jz |
| MEM_TYPE_INVALID | error | S7.1 Invalid storage type keyword | 7_1_MEM_TYPE_INVALID-bad_type_keyword.jz |
| MEM_UNDEFINED_CONST_IN_WIDTH | error | S7.1/S7.7.1 Undefined CONST used in MEM width/depth | 7_1_MEM_UNDEFINED_CONST_IN_WIDTH-undefined_const.jz |
| MEM_UNDEFINED_NAME | error | S7.7.1 Access to undeclared MEM name | 7_1_MEM_UNDEFINED_NAME-undefined_mem_name.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| (none) | -- | All section 7.1 declaration rules have validation tests |
