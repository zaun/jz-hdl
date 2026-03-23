# Test Plan: 7.1 MEM Declaration

**Specification Reference:** Section 7.1 of jz-hdl-specification.md

## 1. Objective

Verify MEM declarations including word width, depth, port lists, storage types (BLOCK/DISTRIBUTED), CONST usage in width/depth, initialization, and all declaration-level validation errors.

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | BLOCK type MEM | `MEM(type=BLOCK) { m [8] [256] = 32'h0 { OUT rd SYNC; IN wr; }; }` | Valid declaration |
| 2 | DISTRIBUTED type MEM | `MEM(type=DISTRIBUTED) { m [4] [16] = 8'h0 { OUT rd ASYNC; }; }` | Valid declaration |
| 3 | CONST width and depth | `MEM { m [WIDTH] [DEPTH] = ... { ... }; }` with CONST definitions | Valid declaration |
| 4 | Multiple ports | `OUT rd1 SYNC; OUT rd2 ASYNC; IN wr;` | Valid multi-port MEM |
| 5 | Multiple MEM blocks in one module | Two separate MEM blocks with distinct names | Valid |

### 2.2 Error Cases

| # | Test Case | Input | Expected | Rule ID |
|---|-----------|-------|----------|---------|
| 1 | Duplicate MEM name | Two MEM entries with same name | Error | MEM_DUP_NAME |
| 2 | Zero word width | `m [0] [256]` | Error | MEM_INVALID_WORD_WIDTH |
| 3 | Zero depth | `m [8] [0]` | Error | MEM_INVALID_DEPTH |
| 4 | Undefined CONST in width | `m [UNDEF] [256]` | Error | MEM_UNDEFINED_CONST_IN_WIDTH |
| 5 | Duplicate port name | `OUT rd SYNC; OUT rd ASYNC;` | Error | MEM_DUP_PORT_NAME |
| 6 | Port name conflicts with module identifier | Port name matches a module-level signal | Error | MEM_PORT_NAME_CONFLICT_MODULE_ID |
| 7 | No ports declared | `m [8] [256] = 32'h0 { };` | Error | MEM_EMPTY_PORT_LIST |
| 8 | Invalid port type keyword | `BOGUS rd SYNC;` | Error | MEM_INVALID_PORT_TYPE |
| 9 | Invalid type keyword | `MEM(type=INVALID)` | Error | MEM_TYPE_INVALID |
| 10 | BLOCK type with ASYNC OUT port | `MEM(type=BLOCK) { m ... { OUT rd ASYNC; }; }` | Error | MEM_TYPE_BLOCK_WITH_ASYNC_OUT |
| 11 | Unsupported chip config for MEM | MEM configuration not supported by target chip | Error | MEM_CHIP_CONFIG_UNSUPPORTED |
| 12 | INOUT mixed with separate IN/OUT | `OUT rd SYNC; INOUT rw SYNC WRITE_FIRST;` | Error | MEM_INOUT_MIXED_WITH_IN_OUT |
| 13 | ASYNC keyword on INOUT port | `INOUT rw ASYNC;` | Error | MEM_INOUT_ASYNC |
| 14 | Missing initialization | `m [8] [256] { OUT rd SYNC; };` | Error | MEM_MISSING_INIT |
| 15 | Undefined MEM name in access | `undef.port[addr]` | Error | MEM_UNDEFINED_NAME |

### 2.3 Edge Cases

| # | Test Case | Input | Expected |
|---|-----------|-------|----------|
| 1 | Minimum depth (1 entry) | `m [8] [1] = 8'h0 { ... };` | Valid |
| 2 | Large depth (65536 entries) | `m [8] [65536] = 8'h0 { ... };` | Valid |
| 3 | 1-bit word width | `m [1] [256] = 1'b0 { ... };` | Valid |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Severity | Notes |
|---|-------|----------------|---------|----------|-------|
| 1 | Duplicate MEM name in same module | Error | MEM_DUP_NAME | error | S7.1 |
| 2 | Word width of 0 | Error | MEM_INVALID_WORD_WIDTH | error | S7.1 |
| 3 | Depth of 0 | Error | MEM_INVALID_DEPTH | error | S7.1 |
| 4 | Undefined CONST used for width/depth | Error | MEM_UNDEFINED_CONST_IN_WIDTH | error | S7.1 |
| 5 | Two ports with same name | Error | MEM_DUP_PORT_NAME | error | S7.1 |
| 6 | Port name clashes with module identifier | Error | MEM_PORT_NAME_CONFLICT_MODULE_ID | error | S7.1 |
| 7 | Empty port list | Error | MEM_EMPTY_PORT_LIST | error | S7.1 |
| 8 | Invalid port type keyword | Error | MEM_INVALID_PORT_TYPE | error | S7.1 |
| 9 | Invalid storage type keyword | Error | MEM_TYPE_INVALID | error | S7.1 |
| 10 | BLOCK with ASYNC OUT | Error | MEM_TYPE_BLOCK_WITH_ASYNC_OUT | error | S7.1 |
| 11 | Chip does not support MEM config | Error | MEM_CHIP_CONFIG_UNSUPPORTED | error | S7.1 |
| 12 | INOUT port mixed with IN/OUT ports | Error | MEM_INOUT_MIXED_WITH_IN_OUT | error | S7.1 |
| 13 | ASYNC on INOUT port | Error | MEM_INOUT_ASYNC | error | S7.1 |
| 14 | No initialization expression | Error | MEM_MISSING_INIT | error | S7.1 |
| 15 | Access to undeclared MEM name | Error | MEM_UNDEFINED_NAME | error | S7.1 |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 7_1_MEM_DUP_NAME-duplicate_mem_name.jz | MEM_DUP_NAME | Duplicate MEM name within the same module |
| 7_1_MEM_EMPTY_PORT_LIST-no_ports.jz | MEM_EMPTY_PORT_LIST | MEM declared with no ports |
| 7_1_MEM_INVALID_DEPTH-zero_depth.jz | MEM_INVALID_DEPTH | MEM depth of zero |
| 7_1_MEM_INVALID_WORD_WIDTH-zero_width.jz | MEM_INVALID_WORD_WIDTH | MEM word width of zero |
| 7_1_MEM_TYPE_INVALID-bad_type_keyword.jz | MEM_TYPE_INVALID | Invalid storage type keyword |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| MEM_DUP_NAME | error | S7.1 Duplicate MEM name in same module | 7_1_MEM_DUP_NAME-duplicate_mem_name.jz |
| MEM_INVALID_WORD_WIDTH | error | S7.1 Word width is zero or invalid | 7_1_MEM_INVALID_WORD_WIDTH-zero_width.jz |
| MEM_INVALID_DEPTH | error | S7.1 Depth is zero or invalid | 7_1_MEM_INVALID_DEPTH-zero_depth.jz |
| MEM_EMPTY_PORT_LIST | error | S7.1 No ports declared in MEM block | 7_1_MEM_EMPTY_PORT_LIST-no_ports.jz |
| MEM_TYPE_INVALID | error | S7.1 Invalid storage type keyword | 7_1_MEM_TYPE_INVALID-bad_type_keyword.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| MEM_UNDEFINED_CONST_IN_WIDTH | error | No validation test yet for undefined CONST in MEM width/depth |
| MEM_DUP_PORT_NAME | error | No validation test yet for duplicate port names |
| MEM_PORT_NAME_CONFLICT_MODULE_ID | error | No validation test yet for port name conflicting with module identifier |
| MEM_INVALID_PORT_TYPE | error | Covered by 7_0 tests but no 7_1-prefixed test |
| MEM_TYPE_BLOCK_WITH_ASYNC_OUT | error | No validation test yet for BLOCK type with ASYNC OUT port |
| MEM_CHIP_CONFIG_UNSUPPORTED | error | No validation test yet for unsupported chip config |
| MEM_INOUT_MIXED_WITH_IN_OUT | error | Covered by 7_2 tests but no 7_1-prefixed test |
| MEM_INOUT_ASYNC | error | Covered by 7_2 tests but no 7_1-prefixed test |
| MEM_MISSING_INIT | error | No validation test yet for missing initialization |
| MEM_UNDEFINED_NAME | error | No validation test yet for undefined MEM name access |
