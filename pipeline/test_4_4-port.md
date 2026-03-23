# Test Plan: 4.4 PORT (Module Interface)

**Specification Reference:** Section 4.4 (including 4.4.1 BUS Ports) of jz-hdl-specification.md

## 1. Objective

Verify PORT declaration syntax (IN/OUT/INOUT with mandatory width), direction enforcement (IN read-only, OUT write-only, INOUT bidirectional), BUS port declarations (SOURCE/TARGET roles, scalar/arrayed, dot notation, wildcard fanout), assignment rules with extension modifiers, and validation rules (signal existence, directionality, index bounds, wildcard compatibility).

## 2. Instrumentation Strategy

- **Span: `parser.port`** — Trace port parsing; attributes: `direction`, `width`, `name`, `is_bus`.
- **Span: `sem.port_direction`** — Direction enforcement check; attributes: `port_name`, `direction`, `usage` (read/write).
- **Span: `sem.bus_resolve`** — BUS role resolution; attributes: `bus_id`, `role`, `resolved_directions`.
- **Event: `port.direction_violation`** — Reading OUT or writing IN.

## 3. Test Scenarios

### 3.1 Happy Path

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | IN port read | Read IN port in ASYNCHRONOUS — valid |
| 2 | OUT port write | Write OUT port in ASYNCHRONOUS — valid |
| 3 | INOUT conditional | Read/write INOUT based on control signal |
| 4 | BUS SOURCE port | BUS with SOURCE role, write OUT signals |
| 5 | BUS TARGET port | BUS with TARGET role, read OUT signals (resolved as IN) |
| 6 | Arrayed BUS | `BUS SPI TARGET [4] spi_bus;` — 4 instances |
| 7 | Wildcard broadcast | `bus[*].signal <= 1'b1;` — broadcast to all |
| 8 | Wildcard element-wise | `bus[*].signal <= 4'b1010;` where count=4 |
| 9 | BUS dot notation | `source.tx`, `target.rx` — valid access |
| 10 | Extension modifiers | `OUT [8] p <=z narrow_sig;` — zero-extend |

### 3.2 Boundary/Edge Cases

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | 1-bit port | `IN [1] clk;` — minimum width |
| 2 | Wide port | `OUT [256] data;` — large width |
| 3 | Module with only OUT ports | Valid (oscillator pattern) |
| 4 | BUS array count = 1 | Single-element array |
| 5 | INOUT with z release | `inout_port <= en ? data : 8'bz;` — valid tri-state |

### 3.3 Negative Testing

| # | Test Case | Description |
|---|-----------|-------------|
| 1 | Write to IN port | `in_port <= data;` — Error |
| 2 | Read from OUT port | `reg <= out_port;` — Error |
| 3 | Missing width | `PORT { IN data; }` — Error |
| 4 | Empty PORT block | `PORT { }` — Error |
| 5 | Module with only IN ports | Warning: dead code |
| 6 | BUS signal not found | `bus.nonexistent` — Error |
| 7 | BUS wrong direction | Read from writable-only BUS signal — Error |
| 8 | Wildcard width mismatch | `bus[*].sig <= 3'b101;` where count=4 — Error (3≠1 and 3≠4) |
| 9 | BUS array out of bounds | `bus[5].sig` where count=4 — Error |
| 10 | Alias in conditional | `IF (c) { a = b; }` — Error: alias in control flow |

## 4. Input/Output Matrix

| # | Input | Expected Output | Rule ID | Notes |
|---|-------|----------------|---------|-------|
| 1 | Write to IN port | Error | PORT_INPUT_DRIVEN_INSIDE | S4.4 |
| 2 | Read from OUT port | Error | PORT_DIRECTION_VIOLATION | S4.4 |
| 3 | OUT port not driven | Error | PORT_OUTPUT_NOT_DRIVEN | S4.4 |
| 4 | Missing port width | Error | — | Parse error |
| 5 | BUS wrong direction access | Error | BUS_PORT_DIRECTION_VIOLATION | S4.4.1 |
| 6 | Wildcard width mismatch | Error | BUS_WILDCARD_WIDTH_MISMATCH | S4.4.1 |
| 7 | BUS index out of range | Error | BUS_INDEX_OUT_OF_RANGE | S4.4.1 |
| 8 | Alias in IF block | Error | ALIAS_IN_CONDITIONAL | S4.10 |

## 5. Integration Points

| Dependency | Role | Mock/Stub Strategy |
|-----------|------|-------------------|
| `parser_port.c` | Parses PORT declarations | Token stream input |
| `driver_net.c` | Port direction enforcement | Integration test with assignments |
| `driver_assign.c` | Assignment validation | Integration test |
| `driver_width.c` | Width matching for extensions | Unit test |
| `chip_data.c` | BUS definitions (project context) | Mock BUS definitions |

## 6. Rules Matrix

### 6.1 Rules Tested

| Rule ID | Description | Test Case(s) |
|---------|-------------|-------------|
| PORT_INPUT_DRIVEN_INSIDE | Writing to IN port | Neg 1 |
| PORT_DIRECTION_VIOLATION | Reading OUT port | Neg 2 |
| PORT_OUTPUT_NOT_DRIVEN | OUT port never driven | Neg (implicit) |
| PORT_INOUT_NOT_BIDIRECTIONAL | INOUT not used bidirectionally | (Boundary 5) |
| BUS_PORT_DIRECTION_VIOLATION | Wrong direction access on BUS | Neg 7 |
| BUS_WILDCARD_WIDTH_MISMATCH | Wildcard RHS width invalid | Neg 8 |
| BUS_INDEX_OUT_OF_RANGE | BUS array index out of bounds | Neg 9 |

### 6.2 Rules Missing

| Expected Rule | Spec Reference | Gap Description |
|--------------|---------------|-----------------|
| PORT_MISSING_WIDTH | S4.4 "Width [N] is mandatory" | Likely a parse error, no semantic rule |
| PORT_BLOCK_EMPTY | S4.2 "empty PORT block -> compile error" | May need explicit rule |
| BUS_SIGNAL_NOT_FOUND | S4.4.1 "signal_id must be declared in BUS" | Need rule for accessing nonexistent BUS signal |
