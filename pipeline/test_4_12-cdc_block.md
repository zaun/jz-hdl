# Test Plan: 4.12 CDC Block

**Specification Reference:** Section 4.12 of jz-hdl-specification.md

## 1. Objective

Verify CDC block syntax and semantics for all 7 types (BIT, BUS, FIFO, HANDSHAKE, PULSE, MCP, RAW), n_stages parameter handling, source register constraints (must be REGISTER, plain identifier, sets home domain), destination alias behavior (read-only, scoped to dest domain), clock-domain rules (block uniqueness, source home domain, destination domain, crossing rule), and width constraints per type (BIT/PULSE: width-1 only).

## 2. Instrumentation Strategy

- **Span: `sem.cdc_check`** — Trace CDC entry; attributes: `type`, `n_stages`, `source_reg`, `src_clk`, `dest_alias`, `dest_clk`.
- **Event: `cdc.domain_conflict`** — Source register used in wrong domain.
- **Event: `cdc.alias_write`** — Attempt to assign destination alias.
- **Event: `cdc.width_violation`** — BIT/PULSE with width > 1.

## 3. Test Scenarios

### 3.1 Happy Path

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

### 3.2 Boundary/Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | n_stages=1 | Minimum stages (unusual but valid) |
| 2 | n_stages=8 | Many synchronization stages |
| 3 | Source reg read in ASYNC | `source_reg` read in ASYNCHRONOUS — valid (home domain) |
| 4 | Multiple CDC entries | Several registers crossing between same domains |

### 3.3 Negative Testing

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | BIT with multi-bit source | `BIT[2] wide_reg (clk_a) => ...` where wide_reg is 8-bit — Error |
| 2 | PULSE with multi-bit | `PULSE trigger (clk_a) => ...` where trigger is 4-bit — Error |
| 3 | RAW with n_stages | `RAW[2] sig ...` — Error: RAW must not have n_stages |
| 4 | Assign to dest alias | `dest_alias <= data;` — Error: read-only |
| 5 | Source in wrong SYNC | Source register used in SYNC(clk_b) when home is clk_a — Error |
| 6 | Dest alias in wrong SYNC | Dest alias used in SYNC(clk_a) when dest is clk_b — Error |
| 7 | Direct cross-domain | Register read in another domain without CDC — Error |
| 8 | Source is expression | `BIT (a + b) (clk_a) => ...` — Error: must be plain identifier |
| 9 | Source is wire | `BIT wire_sig (clk_a) => ...` — Error: must be REGISTER |

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | BIT with 8-bit reg | Error | CDC_BIT_WIDTH | Must be width-1 |
| 2 | RAW with n_stages | Error | CDC_RAW_NO_STAGES | RAW cannot have stages |
| 3 | Assign to dest alias | Error | CDC_ALIAS_READONLY | Read-only alias |
| 4 | Source in wrong domain | Error | CDC_DOMAIN_CONFLICT | Home domain violation |
| 5 | Cross-domain without CDC | Error | CDC_MISSING_BRIDGE | Must use CDC |
| 6 | Valid BIT[2] crossing | Valid | — | Happy path |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `parser_cdc.c` | Parses CDC block | Token stream |
| `driver_clocks.c` | Clock domain analysis | Integration test |
| `driver_instance.c` | CDC bridge instantiation in IR | IR verification |
| `ir_build_instance.c` | Generates synchronizer instances | Integration test |

## 6. Rules Matrix

### 6.1 Rules Tested

| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| CDC_BIT_WIDTH | BIT/PULSE must be width-1 | Neg 1, 2 |
| CDC_RAW_NO_STAGES | RAW must not have n_stages | Neg 3 |
| CDC_ALIAS_READONLY | Dest alias cannot be written | Neg 4 |
| CDC_DOMAIN_CONFLICT | Source used in wrong domain | Neg 5 |
| CDC_MISSING_BRIDGE | Cross-domain access without CDC | Neg 7 |
| CDC_SOURCE_NOT_REGISTER | Source must be REGISTER | Neg 9 |
| CDC_SOURCE_NOT_PLAIN | Source must be plain identifier | Neg 8 |

### 6.2 Rules Missing

| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| CDC_DEST_WRONG_DOMAIN | S4.12 "readable only in destination domain" | Dest alias used outside dest domain |
| CDC_BUS_GRAY_CODE | S4.12 "Gray-code discipline" | No compile-time check for Gray coding (may be warning) |
