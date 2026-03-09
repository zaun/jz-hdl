---
title: Memory (MEM) Reference
lang: en-US

layout: doc
outline: deep
---

# Memory (MEM) Reference

## Memory Port Modes

JZ-HDL MEM declarations express all common BSRAM operating modes through port type combinations.

| Port Configuration | BSRAM Mode | Description |
| :--- | :--- | :--- |
| `OUT` only | Read Only Memory | Read-only, initialized at power-on |
| `INOUT` ×1 | Single Port | One shared-address port for read and write |
| `IN` + `OUT` | Semi-Dual Port | Separate write port and read port |
| `INOUT` ×2 | Dual Port | Two independent read/write ports |

**Note:** `INOUT` ports cannot be mixed with `IN` or `OUT` ports in the same MEM declaration.

## Declaration

```text
MEM(type=[BLOCK|DISTRIBUTED]) {
  <name> [<word_width>] [<depth>] = <init> {
    OUT   <port_id> [ASYNC | SYNC];
    IN    <port_id>;
    IN    <port_id> { WRITE_MODE = <mode>; };
    INOUT <port_id>;
  };
}
```

- `type` is optional. When omitted: depth <= 16 infers DISTRIBUTED; depth > 16 with all SYNC OUT ports infers BLOCK.
- Address width is `clog2(depth)`. Minimum address width is 1 bit; a single-word memory (depth=1) uses a 1-bit address (the sole word lives at address `1'b0`).
- OUT ports are read ports; IN ports are write ports; INOUT ports are combined read/write.
- At least one port (IN, OUT, or INOUT) is required.
- `INOUT` ports cannot be mixed with `IN` or `OUT` ports in the same MEM declaration.

## Port types

### OUT ASYNC (asynchronous read)

- Combinational address-to-data path.
- Used in `ASYNCHRONOUS` blocks only.
- Access: `mem.port[address]`
- Result width equals word_width.

### OUT SYNC (synchronous read)

- Exposes two pseudo-fields: `.addr` and `.data`.
- `.addr` is assigned in `SYNCHRONOUS` blocks with `<=`.
- `.data` is the read result, valid the cycle after `.addr` is sampled.
- `.data` is readable in any block (ASYNCHRONOUS or SYNCHRONOUS).
- `mem.port[addr]` indexing is illegal for SYNC ports (use `.addr`/`.data` instead).
- `.addr` may be assigned at most once per execution path.

### IN (synchronous write)

- Always synchronous; address and data sampled at clock edge.
- Used only in `SYNCHRONOUS` blocks.
- Access: `mem.port[address] <= value;`
- Each IN port may be written at most once per synchronous execution path.

### INOUT (read/write port)

- Combined read/write port with shared address.
- Always synchronous (no `ASYNC`/`SYNC` keyword; `SYNC` is implicit).
- Exposes three pseudo-fields:
  - `.addr` — address input, set via `<=` in `SYNCHRONOUS` blocks
  - `.data` — read data output (1 cycle latency), readable in any block
  - `.wdata` — write data input, assigned via `<=` in `SYNCHRONOUS` blocks
- If `.wdata` is not assigned in a given execution path, no write occurs (read-only cycle).
- `.addr` and `.wdata` may each be assigned at most once per execution path.
- `mem.port[addr]` indexing syntax is illegal on `INOUT` ports (must use `.addr`).
- Write modes (`WRITE_FIRST`, `READ_FIRST`, `NO_CHANGE`) apply to `INOUT` ports.
- Cannot be mixed with `IN` or `OUT` ports in the same MEM declaration.

## Write modes

Write mode governs read-during-write behavior when a read and write target the same address in the same cycle.

| Mode | Read returns | Default? |
| --- | --- | --- |
| `WRITE_FIRST` | Newly written data (post-write) | Yes |
| `READ_FIRST` | Old data (pre-write) | No |
| `NO_CHANGE` | Previous output value (unchanged) | No |

Syntax (shorthand or attribute form):
```text
IN wr;                          // default WRITE_FIRST
IN wr WRITE_FIRST;
IN wr READ_FIRST;
IN wr NO_CHANGE;
IN wr { WRITE_MODE = READ_FIRST; };

INOUT rw;                       // default WRITE_FIRST
INOUT rw WRITE_FIRST;
INOUT rw READ_FIRST;
INOUT rw NO_CHANGE;
INOUT rw { WRITE_MODE = READ_FIRST; };
```

When `addr_r != addr_w`, all modes behave identically (return stored word at addr_r).

For `INOUT` ports, write mode controls what `.data` shows when `.wdata` is assigned in the same cycle.

## Initialization

### Literal initialization

- All words initialized to the same literal value.
- Init literal must not contain `x`.
- Result width must be <= word_width; overflow is a compile error.

### File-based initialization

- Syntax: `= @file("path")`, `= @file(CONST_NAME)`, or `= @file(CONFIG.NAME)`
- The path argument may be a literal string, a module-local string CONST, or a project-level string CONFIG reference.
- Using a numeric CONST/CONFIG where a string path is expected is a compile error.
- File size must not exceed `depth * word_width` bits.
- Smaller files are zero-padded; larger files produce a compile error.
- Files must not contain undefined bits.
- Supported formats: `.bin`, `.hex`, `.mif`, `.coe`, `.mem` (Verilog memory format with `0`/`1` and `//` comments). Tool-specific formats allowed.

## Access rules summary

| Context | ASYNC OUT | SYNC OUT .addr | SYNC OUT .data | IN write | INOUT .addr | INOUT .data | INOUT .wdata |
| --- | --- | --- | --- | --- | --- | --- | --- |
| ASYNCHRONOUS | `mem.port[addr]` (read) | illegal | read OK | illegal | illegal | read OK | illegal |
| SYNCHRONOUS | illegal | `<= addr` | read OK | `mem.port[addr] <= val` | `<= addr` | read OK | `<= val` |

## Scope and uniqueness

- Memory names must be unique within the module.
- Port names must be unique within the MEM block and must not conflict with module-level identifiers.

## Chip-specific validation

When the project `CHIP` is not "GENERIC" and chip data is available, the compiler validates MEM width, depth, and port counts against the chip's supported memory configurations. If `CHIP` is "GENERIC", no chip-specific validation is performed.

## MEM vs REGISTER vs WIRE

| Feature | REGISTER | WIRE | MEM |
| --- | --- | --- | --- |
| Storage | Single value (N bits) | None (combinational) | Array (word_width x depth) |
| Read timing | Combinational (current value) | Combinational | Async or sync |
| Write timing | Clock-synchronized | Continuous assignment | Clock-synchronized |
| Port count | 1 (implicit) | N/A | Multiple explicit |
| Access syntax | `reg_name` | `wire_name` | ASYNC: `mem.port[addr]`, SYNC/INOUT: `.addr`/`.data`/`.wdata` |
| Initialization | Literal only | N/A | Literal or file |

## INOUT Examples

### Single Port Memory (INOUT)

```text
MEM(TYPE=BLOCK) {
  mem [16] [256] = 16'h0000 {
    INOUT rw;
  };
}

SYNCHRONOUS(CLK=clk) {
  mem.rw.addr <= addr;
  rd_data <= mem.rw.data;
  IF (wr_en) {
    mem.rw.wdata <= wr_data;
  }
}
```

- Single `INOUT` port → Single Port BSRAM mode
- Shared address for read and write
- Synchronous read (1 cycle latency)
- Write gated by condition

### True Dual Port Memory (2× INOUT)

```text
MEM(TYPE=BLOCK) {
  mem [16] [256] = 16'h0000 {
    INOUT port_a;
    INOUT port_b;
  };
}

SYNCHRONOUS(CLK=clk) {
  mem.port_a.addr <= addr_a;
  rd_data_a <= mem.port_a.data;
  IF (wr_en_a) {
    mem.port_a.wdata <= wr_data_a;
  }

  mem.port_b.addr <= addr_b;
  rd_data_b <= mem.port_b.data;
  IF (wr_en_b) {
    mem.port_b.wdata <= wr_data_b;
  }
}
```

- Two `INOUT` ports → Dual Port BSRAM mode
- Each port has independent address, read data, and write data
- Both ports can read and write independently in the same cycle
- **Note:** Write behavior when both ports write to the same address in the same cycle is undefined (hardware-dependent)
