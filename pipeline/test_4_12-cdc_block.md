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
| 7 | RAW | `RAW status (clk_a) => status_raw (clk_b);` — no n_stages |
| 8 | Default n_stages=2 | `BIT flag (clk_a) => ...` — defaults to 2 |
| 9 | Bidirectional CDC | Two entries, one each direction |
| 10 | Source in SYNC(src_clk) | Source register read/written in its home domain |
| 11 | Dest alias in SYNC(dest_clk) | Read dest_alias in destination domain only |

### 2.2 Error Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | BIT with multi-bit source | `BIT[2] wide_reg (clk_a) => ...` where wide_reg is 8-bit — Error |
| 2 | PULSE with multi-bit | `PULSE trigger (clk_a) => ...` where trigger is 4-bit — Error |
| 3 | RAW with n_stages | `RAW[2] sig ...` — Error: RAW must not have n_stages |
| 4 | Assign to dest alias | `dest_alias <= data;` — Error: read-only |
| 5 | Duplicate dest alias name | Two CDC entries with same dest alias name — Error |
| 6 | n_stages invalid | Non-positive stages value — Error |
| 7 | Invalid CDC type | Unknown type keyword — Error |
| 8 | Source is wire | `BIT wire_sig (clk_a) => ...` — Error: must be REGISTER |
| 9 | Source is expression | `BIT (a + b) (clk_a) => ...` — Error: must be plain identifier |

### 2.3 Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | n_stages=1 | Minimum stages (unusual but valid) |
| 2 | n_stages=8 | Many synchronization stages |
| 3 | Source reg read in ASYNC | `source_reg` read in ASYNCHRONOUS — valid (home domain) |
| 4 | Multiple CDC entries | Several registers crossing between same domains |

## 3. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | BIT with 8-bit reg | Error | CDC_BIT_WIDTH_NOT_1 | Must be width-1 |
| 2 | PULSE with multi-bit | Error | CDC_PULSE_WIDTH_NOT_1 | Must be width-1 |
| 3 | RAW with n_stages | Error | CDC_RAW_STAGES_FORBIDDEN | RAW cannot have stages |
| 4 | Assign to dest alias | Error | CDC_DEST_ALIAS_ASSIGNED | Read-only alias |
| 5 | Duplicate dest alias | Error | CDC_DEST_ALIAS_DUP | Alias name conflict |
| 6 | Non-positive stages | Error | CDC_STAGES_INVALID | Stages must be positive |
| 7 | Unknown CDC type | Error | CDC_TYPE_INVALID | Type keyword not recognized |
| 8 | Source is wire | Error | CDC_SOURCE_NOT_REGISTER | Source must be REGISTER |
| 9 | Source is expression | Error | CDC_SOURCE_NOT_PLAIN_REG | Must be plain identifier |
| 10 | Valid BIT[2] crossing | Valid | — | Happy path |

## 4. Existing Validation Tests

| Test File | Rule Tested |
|-----------|-------------|
| 4_12_CDC_BIT_WIDTH_NOT_1-bit_multibit_source.jz | CDC_BIT_WIDTH_NOT_1 |
| 4_12_CDC_DEST_ALIAS_DUP-alias_name_conflict.jz | CDC_DEST_ALIAS_DUP |
| 4_12_CDC_PULSE_WIDTH_NOT_1-pulse_multibit_source.jz | CDC_PULSE_WIDTH_NOT_1 |
| 4_12_CDC_RAW_STAGES_FORBIDDEN-raw_with_stages.jz | CDC_RAW_STAGES_FORBIDDEN |
| 4_12_CDC_SOURCE_NOT_REGISTER-wire_as_source.jz | CDC_SOURCE_NOT_REGISTER |
| 4_12_CDC_STAGES_INVALID-non_positive_stages.jz | CDC_STAGES_INVALID |
| 4_12_CDC_TYPE_INVALID-unknown_cdc_type.jz | CDC_TYPE_INVALID |

## 5. Rules Matrix

### 5.1 Rules Tested

| Rule ID | Severity | Test File(s) |
|---------|----------|--------------|
| CDC_BIT_WIDTH_NOT_1 | error | 4_12_CDC_BIT_WIDTH_NOT_1-bit_multibit_source.jz |
| CDC_DEST_ALIAS_DUP | error | 4_12_CDC_DEST_ALIAS_DUP-alias_name_conflict.jz |
| CDC_PULSE_WIDTH_NOT_1 | error | 4_12_CDC_PULSE_WIDTH_NOT_1-pulse_multibit_source.jz |
| CDC_RAW_STAGES_FORBIDDEN | error | 4_12_CDC_RAW_STAGES_FORBIDDEN-raw_with_stages.jz |
| CDC_SOURCE_NOT_REGISTER | error | 4_12_CDC_SOURCE_NOT_REGISTER-wire_as_source.jz |
| CDC_STAGES_INVALID | error | 4_12_CDC_STAGES_INVALID-non_positive_stages.jz |
| CDC_TYPE_INVALID | error | 4_12_CDC_TYPE_INVALID-unknown_cdc_type.jz |

### 5.2 Rules Not Tested

| Rule ID | Severity | Gap Description |
|---------|----------|-----------------|
| CDC_SOURCE_NOT_PLAIN_REG | error | Source must be plain register identifier, not expression |
| CDC_DEST_ALIAS_ASSIGNED | error | Destination alias written to (should be read-only) |
