# Test Plan: 4.12 CDC Block

**Specification Reference:** Section 4.12 of jz-hdl-specification.md

## 1. Objective

Verify CDC block syntax and semantics for all 7 types (BIT, BUS, FIFO, HANDSHAKE, PULSE, MCP, RAW), n_stages parameter handling, source register constraints (must be REGISTER, plain identifier, sets home domain), destination alias behavior (read-only, scoped to dest domain), clock-domain rules (block uniqueness, source home domain, destination domain, crossing rule), and width constraints per type (BIT/PULSE: width-1 only).

## 2. Test Scenarios

### 2.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | BIT synchronizer | `BIT[2] flag (clk_a) => flag_sync (clk_b);` |
| 2 | BUS synchronizer | `BUS[3] counter (clk_a) => counter_sync (clk_b);` |
| 3 | FIFO crossing | `FIFO data_reg (clk_a) => data_view (clk_b);` |
| 4 | HANDSHAKE | `HANDSHAKE config (clk_a) => config_sync (clk_b);` |
| 5 | PULSE | `PULSE[2] trigger (clk_a) => trigger_sync (clk_b);` |
| 6 | MCP | `MCP[3] word (clk_a) => word_sync (clk_b);` |
| 7 | RAW | `RAW status (clk_a) => status_raw (clk_b);` -- no n_stages |
| 8 | Default n_stages=2 | `BIT flag (clk_a) => ...` -- defaults to 2 |
| 9 | Bidirectional CDC | Two entries, one each direction |
| 10 | Source in SYNC(src_clk) | Source register read/written in its home domain |
| 11 | Dest alias in SYNC(dest_clk) | Read dest_alias in destination domain only |

### 2.2 Error Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | BIT with multi-bit source | `BIT[2] wide_reg (clk_a) => ...` where wide_reg is 8-bit -- Error |
| 2 | PULSE with multi-bit | `PULSE trigger (clk_a) => ...` where trigger is 4-bit -- Error |
| 3 | RAW with n_stages | `RAW[2] sig ...` -- Error: RAW must not have n_stages |
| 4 | Assign to dest alias | `dest_alias <= data;` -- Error: read-only |
| 5 | Duplicate dest alias name | Two CDC entries with same dest alias name -- Error |
| 6 | n_stages invalid | Non-positive stages value -- Error |
| 7 | Invalid CDC type | Unknown type keyword -- Error |
| 8 | Source is wire | `BIT wire_sig (clk_a) => ...` -- Error: must be REGISTER |
| 9 | Source is expression | `BIT (a + b) (clk_a) => ...` -- Error: must be plain identifier |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | n_stages=1 | Minimum stages (unusual but valid) |
| 2 | n_stages=8 | Many synchronization stages |
| 3 | Source reg read in ASYNC | `source_reg` read in ASYNCHRONOUS -- valid (home domain) |
| 4 | Multiple CDC entries | Several registers crossing between same domains |

## 3. Input/Output Matrix

| # | Scenario | Triggering Construct | Expected Rule ID | Severity |
|---|----------|---------------------|-----------------|----------|
| 1 | BIT with multi-bit source | `BIT[2] wide_reg (clk_a) => ...` (wide_reg is 8-bit) | CDC_BIT_WIDTH_NOT_1 | error |
| 2 | PULSE with multi-bit source | `PULSE trigger (clk_a) => ...` (trigger is 4-bit) | CDC_PULSE_WIDTH_NOT_1 | error |
| 3 | RAW with n_stages parameter | `RAW[2] sig (clk_a) => ...` | CDC_RAW_STAGES_FORBIDDEN | error |
| 4 | Assign to dest alias | `dest_alias <= data;` in SYNC block | CDC_DEST_ALIAS_ASSIGNED | error |
| 5 | Duplicate dest alias | Two entries produce same dest alias name | CDC_DEST_ALIAS_DUP | error |
| 6 | Non-positive stages | `BIT[0] flag ...` | CDC_STAGES_INVALID | error |
| 7 | Unknown CDC type | `UNKNOWN sig ...` | CDC_TYPE_INVALID | error |
| 8 | Source is wire | `BIT wire_sig ...` | CDC_SOURCE_NOT_REGISTER | error |
| 9 | Source is sliced/expression | `BIT reg[3:0] ...` | CDC_SOURCE_NOT_PLAIN_REG | error |
| 10 | Valid BIT[2] crossing | `BIT[2] flag (clk_a) => flag_s (clk_b);` | -- | -- (pass) |

## 4. Existing Validation Tests

| Test File | Rule ID | Description |
|-----------|---------|-------------|
| 4_12_CDC_HAPPY_PATH-valid_cdc_block_ok.jz | -- | Happy path: valid CDC block with multiple types |
| 4_12_CDC_BIT_WIDTH_NOT_1-bit_multibit_source.jz | CDC_BIT_WIDTH_NOT_1 | BIT CDC source must be width 1 |
| 4_12_CDC_DEST_ALIAS_ASSIGNED-dest_alias_written.jz | CDC_DEST_ALIAS_ASSIGNED | CDC dest alias is read-only |
| 4_12_CDC_DEST_ALIAS_DUP-alias_name_conflict.jz | CDC_DEST_ALIAS_DUP | Duplicate CDC dest alias name |
| 4_12_CDC_PULSE_WIDTH_NOT_1-pulse_multibit_source.jz | CDC_PULSE_WIDTH_NOT_1 | PULSE CDC source must be width 1 |
| 4_12_CDC_RAW_STAGES_FORBIDDEN-raw_with_stages.jz | CDC_RAW_STAGES_FORBIDDEN | RAW type must not have n_stages |
| 4_12_CDC_SOURCE_NOT_PLAIN_REG-sliced_source.jz | CDC_SOURCE_NOT_PLAIN_REG | CDC source must be plain register, not expression |
| 4_12_CDC_SOURCE_NOT_REGISTER-wire_as_source.jz | CDC_SOURCE_NOT_REGISTER | CDC source must be REGISTER |
| 4_12_CDC_STAGES_INVALID-non_positive_stages.jz | CDC_STAGES_INVALID | n_stages must be positive integer |
| 4_12_CDC_TYPE_INVALID-unknown_cdc_type.jz | CDC_TYPE_INVALID | Unknown CDC type keyword |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Description | Test Case(s) |
|---------|----------|-------------|--------------|
| CDC_BIT_WIDTH_NOT_1 | error | S4.12 BIT CDC source register must be width 1 | 4_12_CDC_BIT_WIDTH_NOT_1-bit_multibit_source.jz |
| CDC_DEST_ALIAS_ASSIGNED | error | S4.12 CDC destination alias is read-only and may not be assigned | 4_12_CDC_DEST_ALIAS_ASSIGNED-dest_alias_written.jz |
| CDC_DEST_ALIAS_DUP | error | S4.12 Duplicate CDC destination alias name | 4_12_CDC_DEST_ALIAS_DUP-alias_name_conflict.jz |
| CDC_PULSE_WIDTH_NOT_1 | error | S4.12 PULSE CDC source register must be width 1 | 4_12_CDC_PULSE_WIDTH_NOT_1-pulse_multibit_source.jz |
| CDC_RAW_STAGES_FORBIDDEN | error | S4.12 RAW CDC type may not specify n_stages | 4_12_CDC_RAW_STAGES_FORBIDDEN-raw_with_stages.jz |
| CDC_SOURCE_NOT_PLAIN_REG | error | S4.12 CDC source must be a plain register identifier, not an expression | 4_12_CDC_SOURCE_NOT_PLAIN_REG-sliced_source.jz |
| CDC_SOURCE_NOT_REGISTER | error | S4.12 CDC source must be a REGISTER, not a wire or other signal type | 4_12_CDC_SOURCE_NOT_REGISTER-wire_as_source.jz |
| CDC_STAGES_INVALID | error | S4.12 CDC n_stages value must be a positive integer | 4_12_CDC_STAGES_INVALID-non_positive_stages.jz |
| CDC_TYPE_INVALID | error | S4.12 Unknown or invalid CDC type keyword | 4_12_CDC_TYPE_INVALID-unknown_cdc_type.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Reason |
|---------|----------|--------|
| -- | -- | All assigned rules for this section are covered by existing tests |
