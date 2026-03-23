# Test Plan: 4.11 SYNCHRONOUS Block (Sequential Logic)

**Specification Reference:** Section 4.11 of jz-hdl-specification.md

## 1. Objective

Verify SYNCHRONOUS block header properties (CLK, EDGE, RESET, RESET_ACTIVE, RESET_TYPE), structural constraints (clock uniqueness, register locality/home domain, read/write visibility), reset priority semantics, and error conditions (domain conflict, duplicate block, multi-clock assignment).

## 2. Instrumentation Strategy

- **Span: `sem.sync_block`** — Trace SYNCHRONOUS block; attributes: `clk`, `edge`, `reset`, `reset_active`, `reset_type`.
- **Span: `sem.domain_check`** — Register domain validation; attributes: `reg_name`, `home_clk`, `block_clk`.
- **Event: `sync.duplicate_block`** — Two SYNC blocks with same clock.
- **Event: `sync.domain_conflict`** — Register used in wrong domain.

## 3. Test Scenarios

### 3.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Default Rising edge | `SYNCHRONOUS(CLK=clk) { ... }` |
| 2 | Falling edge | `SYNCHRONOUS(CLK=clk EDGE=Falling) { ... }` |
| 3 | Immediate reset | `RESET_TYPE=Immediate` |
| 4 | Clocked reset | `RESET_TYPE=Clocked` |
| 5 | Reset active high | `RESET_ACTIVE=High` |
| 6 | Reset active low | `RESET_ACTIVE=Low` (default) |
| 7 | Multiple SYNC blocks | `CLK=clk_a` and `CLK=clk_b` — different clocks |
| 8 | Empty SYNC body | Registers hold — valid |
| 9 | Register read/write | `counter <= counter + 8'd1;` — read current, write next |

### 3.2 Boundary/Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Both edge | `EDGE=Both` — valid but warns |
| 2 | No reset | SYNC without RESET property — valid |
| 3 | Register read in ASYNC | Current value visible combinationally |

### 3.3 Negative Testing

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Duplicate SYNC block | Two `SYNCHRONOUS(CLK=clk)` in same module — Error |
| 2 | Domain conflict | Register from clk_a written in `SYNCHRONOUS(CLK=clk_b)` — Error |
| 3 | Multi-clock assign | Same register in two different SYNC blocks — Error |
| 4 | Wire write in SYNC | `wire <= data;` in SYNCHRONOUS — Error |
| 5 | CLK not width-1 | `CLK=clk` where clk is 8-bit — Error |
| 6 | RESET not width-1 | `RESET=rst` where rst is 8-bit — Error |
| 7 | Missing CLK | `SYNCHRONOUS() { ... }` — Error |

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Duplicate CLK=clk blocks | Error | SYNC_DUPLICATE_BLOCK | S4.11 |
| 2 | Register in wrong domain | Error | SYNC_DOMAIN_CONFLICT | S4.11 |
| 3 | Register in two SYNC blocks | Error | SYNC_MULTI_CLK_ASSIGN | S4.11 |
| 4 | EDGE=Both | Warning | SYNC_BOTH_EDGE_WARNING | S4.11 |
| 5 | Wire in SYNC | Error | ASSIGN_OP_WRONG_BLOCK | S4.5 |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `parser_blocks.c` | Parses SYNCHRONOUS headers | Token stream |
| `driver_clocks.c` | Clock domain analysis | Integration test |
| `driver_assign.c` | Assignment context validation | Integration test |
| `ir_build_clock.c` | Clock/reset IR generation | Verify reset synchronizer for Immediate |

## 6. Rules Matrix

### 6.1 Rules Tested

| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| SYNC_DUPLICATE_BLOCK | Two SYNC blocks same clock | Neg 1 |
| SYNC_DOMAIN_CONFLICT | Register in wrong clock domain | Neg 2 |
| SYNC_MULTI_CLK_ASSIGN | Register assigned in multiple SYNC blocks | Neg 3 |
| ASSIGN_OP_WRONG_BLOCK | Wire assigned in SYNC | Neg 4 |

### 6.2 Rules Missing

| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| SYNC_CLK_NOT_1BIT | S4.11 "Width-1 net" | CLK signal must be 1-bit |
| SYNC_RESET_NOT_1BIT | S4.11 "Width-1 reset signal" | Reset signal must be 1-bit |
| SYNC_MISSING_CLK | S4.11 "CLK required" | SYNCHRONOUS without CLK property |
