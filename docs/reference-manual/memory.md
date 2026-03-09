---
title: Memory
lang: en-US

layout: doc
outline: deep
---

# Memory

::: tip Formal Reference
For a concise normative summary of all MEM rules, port types, and access syntax, see the [Memory Formal Reference](/reference-manual/formal-reference/memory). This page provides detailed explanations, examples, and practical guidance.
:::

## Overview

- MEM declares internal arrays (word_width × depth) synthesized as Block RAM, Distributed RAM, or vendor-specific memories.
- MEM supports multiple named read (`OUT`) ports (ASYNC or SYNC), synchronous write (`IN`) ports, and combined read/write (`INOUT`) ports.
- Address widths are derived as `ceil(log2(depth))`. Minimum address width is 1 bit; a single-word memory (depth=1) uses a 1-bit address.
- Initialization can be a sized literal (same value for every word) or a file (`@file(...)`).
- MEM access obeys synchronous vs. asynchronous rules depending on port kind and block context.

---

## Memory Port Modes

JZ-HDL MEM declarations express all common BSRAM operating modes through port type combinations. The compiler analyzes port declarations to determine the required BSRAM mode.

| Port Configuration | BSRAM Mode | Description |
| :--- | :--- | :--- |
| `OUT` only | Read Only Memory | Read-only, initialized at power-on |
| `INOUT` ×1 | Single Port | One shared-address port for read and write |
| `IN` + `OUT` | Semi-Dual Port | Separate write port (Port A) and read port (Port B) |
| `INOUT` ×2 | Dual Port | Two independent read/write ports |

**Notes:**
- `INOUT` ports cannot be mixed with `IN` or `OUT` ports in the same MEM declaration.
- A MEM uses either `IN`/`OUT` ports (Semi-Dual Port or Read Only) or `INOUT` ports (Single Port or Dual Port), never both.

---

## Canonical Syntax

Use inside a `@module` body:

```text
MEM(type=[BLOCK|DISTRIBUTED]) {
  <name> [<word_width>] [<depth>] = <init> {
    OUT   <port_id> [ASYNC | SYNC];
    IN    <port_id> { WRITE_MODE = <mode>; };   // or: IN <port_id>;
    INOUT <port_id>;                            // combined read/write port
  };
  ...
}
```

- `type` optional; compiler may infer when omitted.
- Word width and depth are positive integers or module-local `CONST` names (compile-time).
- The init value is a sized literal, `@file("path")`, `@file(CONST_NAME)`, or `@file(CONFIG.NAME)`.
- Each MEM must include at least one port.
- `INOUT` ports cannot be mixed with `IN` or `OUT` ports in the same MEM declaration.

---

## Declaration Rules

- Memory name must be unique in the module.
- Port names must be unique within the MEM and must not clash with module-level identifiers.
- If `type=BLOCK` is specified, the compiler verifies constraints (e.g., all OUT ports must be `SYNC` if required by target).
- Address width = `clog2(depth)` (compile-time). Minimum address width is 1 bit; a single-word memory (depth=1) uses a 1-bit address (the sole word lives at address `1'b0`).
- Word init literals must not contain `x`.
- File-based init files must not contain undefined bits.
- `INOUT` ports cannot be mixed with `IN` or `OUT` ports in the same MEM declaration.
- All port names (`IN`, `OUT`, `INOUT`) must be distinct within the MEM block.

---

## Port Types

- `OUT port_id ASYNC`
  - Asynchronous read: combinational address → data path.
  - Used in `ASYNCHRONOUS` logic (`=` / `=>` / `<=` as allowed).
  - Access syntax: `mem.port[address_expr]` (RHS).

- `OUT port_id SYNC`
  - Synchronous read: address sampled on clock; data available on a latched read output next cycle.
  - Exposes two pseudo-fields: `.addr` and `.data`.
  - `.addr` is assigned in `SYNCHRONOUS` blocks with `<=`.
  - `.data` is the read result, valid the cycle after `.addr` is sampled.
  - `.data` is readable in any block (ASYNCHRONOUS or SYNCHRONOUS).
  - `mem.port[addr]` indexing is illegal for SYNC ports (use `.addr`/`.data` instead).
  - `.addr` may be assigned at most once per execution path.

- `IN port_id`
  - Write port(s) are always synchronous: address and data sampled at clock.
  - Access in `SYNCHRONOUS` blocks: `mem.port[address_expr] <= data_expr;`
  - Each `IN` port may be written at most once per `SYNCHRONOUS` block (per Exclusive Assignment Rule).

- `INOUT port_id`
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

---

## Access Syntax and Context

- Asynchronous read (ASYNCHRONOUS blocks):
  - `dst = mem.port[index];`
  - `index` width must be ≤ address width; narrower indices are zero‑extended.
  - `dst` width must equal `word_width`.

- Synchronous read via SYNC OUT (SYNCHRONOUS blocks):
  - `mem.port.addr <= index;` schedules the address to be sampled.
  - `reg_or_net <= mem.port.data;` reads the registered output.
  - The `.data` output is valid one cycle after `.addr` is sampled.

- Synchronous write via IN (SYNCHRONOUS blocks):
  - `mem.port[index] <= data;`
  - `index` and `data` may be narrower; zero‑extend to address/word width as required.
  - Conditional writes via `IF`/`SELECT` are allowed (must obey Exclusive Assignment Rule).

- INOUT access (SYNCHRONOUS blocks):
  - `mem.port.addr <= index;` sets the shared address.
  - `reg_or_net <= mem.port.data;` reads the data (1 cycle latency).
  - `mem.port.wdata <= data;` writes data at the current address.
  - If `.wdata` is not assigned, no write occurs (read-only cycle).

- General width behavior:
  - Address index widths must be ≤ derived address width. If narrower, zero-extend. If statically ≥ depth → compile error.
  - Data width must be ≤ `word_width`. If narrower, zero-extend or use modifiers (where supported) per assignment rules.

---

## Read / Write Semantics

- Reads and writes are independent ports; a memory may have multiple OUT and IN ports.
- When a read and a write target the same address in the same cycle, the observed read value depends on the corresponding write port’s `WRITE_MODE`:
  - `WRITE_FIRST` (default): read returns the newly written data.
  - `READ_FIRST`: read returns the old data (pre-write).
  - `NO_CHANGE`: read retains its previous output value for that cycle.
- On subsequent cycles the stored word equals the write data regardless of mode.

---

## Write Modes

You may declare write mode per `IN` or `INOUT` port:

Shorthand:
```text
IN wr;                // default WRITE_FIRST
IN wr WRITE_FIRST;
IN wr READ_FIRST;
IN wr NO_CHANGE;

INOUT rw;             // default WRITE_FIRST
INOUT rw WRITE_FIRST;
INOUT rw READ_FIRST;
INOUT rw NO_CHANGE;
```

Attribute form:
```text
IN wr {
  WRITE_MODE = READ_FIRST;
};

INOUT rw {
  WRITE_MODE = READ_FIRST;
};
```

Meaning:
- WRITE_FIRST — newly written data visible on same-cycle reads.
- READ_FIRST — old data visible on same-cycle reads.
- NO_CHANGE — read output unchanged for that cycle.

For `INOUT` ports, write mode controls what `.data` shows when `.wdata` is assigned in the same cycle.

---

## Initialization

- Literal initialization: the init value is a sized literal (must not contain `x`) and applies to all words.
  - Example: `mem [8] [256] = 8'h00 { ... }`
- File-based initialization: `= @file("path")`, `= @file(CONST_NAME)`, or `= @file(CONFIG.NAME)`
  - The path argument may be a literal string, a module-local string CONST, or a project-level string CONFIG reference.
  - Using a numeric CONST/CONFIG where a string path is expected is a compile error (`CONST_NUMERIC_IN_STRING_CONTEXT`).
  - Supported formats: `.bin`, `.hex` (Intel HEX), `.mif`, `.coe`, `.mem` (Verilog memory format with `0`/`1` and `//` comments), and tool-specific formats.
  - File size must be ≤ `depth × word_width` bits. Smaller files are zero-padded. Larger files → compile error.
  - Files must not encode unknown bits; any undefined bits cause a compile error.
- Initialization evaluated at compile time.

---

## Derived Address Width and Bounds

- Address width W = `clog2(depth)` (compile-time).
- Minimum address width is 1 bit; a single-word memory (depth=1) uses a 1-bit address (the sole word lives at address `1'b0`).
- If an address expression is statically provable ≥ `depth` → compile-time error.
- If an address may be out of range at runtime, the behavior:
  - For asynchronous reads: implementation-defined / simulation must abort (per MUX/INDEX rules).
  - For gslice/gsbit-like constructs, out-of-range read bits return 0; but for plain mem access, tools must follow the spec (static OOB is error; dynamic OOB is tool-defined — prefer guarding or ensuring index width correctness).

(Compiler implementations should statically check constants; for dynamic indices, zero-extend narrower indices and do bounds handling per the design policy.)

---

## Constraints & Rules Summary

- MEM must have at least one declared port.
- OUT ASYNC reads may not be used in SYNCHRONOUS blocks as RHS without appropriate `<=` semantics.
- IN ports may only be written in SYNCHRONOUS blocks. Writing in ASYNCHRONOUS → compile error.
- Each `IN` port: at most one write per `SYNCHRONOUS` block (per-path exclusivity applies).
- Each MEM port name unique per MEM and distinct from module identifiers.
- Literal init must not contain `x`.
- CONST names in word_width/depth resolved in module `CONST` scope; must be resolvable at compile time.

---

## Examples

Simple ROM (async read)
```text
MEM {
  sine_lut [8] [256] = @file("sine_table.hex") {
    OUT addr ASYNC;
  };
}

ASYNCHRONOUS {
  data = sine_lut.addr[index];
}
```

Synchronous register-file style
```text
MEM {
  regfile [32] [32] = 32'h0000_0000 {
    OUT rd_a ASYNC;
    OUT rd_b ASYNC;
    IN  wr;
  };
}

ASYNCHRONOUS {
  rd_a_out = regfile.rd_a[rd_addr_a];
  rd_b_out = regfile.rd_b[rd_addr_b];
}

SYNCHRONOUS(CLK=clk) {
  IF (wr_en) {
    regfile.wr[wr_addr] <= wr_data;
  }
}
```

Synchronous read (registered output)
```text
MEM {
  cache [32] [1024] = 32'h0000_0000 {
    OUT rd SYNC;
  };
}

SYNCHRONOUS(CLK=clk) {
  cache.rd.addr <= addr;
  read_data <= cache.rd.data;
}
```

Dual-write prohibition (illegal)
```text
SYNCHRONOUS(CLK=clk) {
  mem.wr[a] <= x;
  mem.wr[b] <= y;  // ERROR: same IN port written twice in the same block
}
```

Single Port Memory (INOUT)
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

True Dual Port Memory (2× INOUT)
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

**Note:** For True Dual Port memories, write behavior when both ports write to the same address in the same cycle is undefined (hardware-dependent).

Synchronous FIFO
```text
@module sync_fifo_8x32
  CONST {
    WIDTH = 8;
    DEPTH = 32;
    ADDR_WIDTH = 5;
  }

  PORT {
    IN  [8] din;
    OUT [8] dout;
    IN  [1] wr_en;
    IN  [1] rd_en;
    IN  [1] clk;
    OUT [1] full;
    OUT [1] empty;
  }

  REGISTER {
    wr_ptr [ADDR_WIDTH + 1] = {(ADDR_WIDTH + 1){1'b0}};
    rd_ptr [ADDR_WIDTH + 1] = {(ADDR_WIDTH + 1){1'b0}};
  }

  MEM {
    fifo_mem [8] [32] = 8'h00 {
      OUT rd ASYNC;
      IN  wr;
    };
  }

  ASYNCHRONOUS {
    full = (wr_ptr[ADDR_WIDTH] != rd_ptr[ADDR_WIDTH]) &
           (wr_ptr[ADDR_WIDTH - 1 : 0] == rd_ptr[ADDR_WIDTH - 1 : 0]);
    empty = (wr_ptr == rd_ptr) ? 1'b1 : 1'b0;
    dout = fifo_mem.rd[rd_ptr[ADDR_WIDTH - 1 : 0]];
  }

  SYNCHRONOUS(CLK=clk) {
    IF (wr_en & ~full) {
      fifo_mem.wr[wr_ptr[ADDR_WIDTH - 1 : 0]] <= din;
      wr_ptr <= wr_ptr + 1;
    }

    IF (rd_en & ~empty) {
      rd_ptr <= rd_ptr + 1;
    }
  }
@endmod
```

Registered Read Cache (Semi-Dual Port with SYNC read)
```text
@module l1_cache_64x256
  CONST {
    LINE_WIDTH = 64;
    NUM_LINES = 256;
  }

  PORT {
    IN  [8] read_addr;
    OUT [64] read_data;
    IN  [8] write_addr;
    IN  [64] write_data;
    IN  [1] write_en;
    IN  [1] clk;
  }

  MEM {
    cache_mem [64] [256] = 64'h0000_0000_0000_0000 {
      OUT rd SYNC;
      IN  wr;
    };
  }

  SYNCHRONOUS(CLK=clk) {
    cache_mem.rd.addr <= read_addr;
    read_data <= cache_mem.rd.data;

    IF (write_en) {
      cache_mem.wr[write_addr] <= write_data;
    }
  }
@endmod
```

Triple-Port Memory (2 Read, 1 Write)
```text
@module triple_port_mem_32x256
  PORT {
    IN  [8] rd_addr_0;
    IN  [8] rd_addr_1;
    OUT [32] rd_data_0;
    OUT [32] rd_data_1;
    IN  [8] wr_addr;
    IN  [32] wr_data;
    IN  [1] wr_en;
    IN  [1] clk;
  }

  MEM {
    mem [32] [256] = 32'h0000_0000 {
      OUT rd_0 ASYNC;
      OUT rd_1 ASYNC;
      IN  wr;
    };
  }

  ASYNCHRONOUS {
    rd_data_0 = mem.rd_0[rd_addr_0];
    rd_data_1 = mem.rd_1[rd_addr_1];
  }

  SYNCHRONOUS(CLK=clk) {
    IF (wr_en) {
      mem.wr[wr_addr] <= wr_data;
    }
  }
@endmod
```

Quad-Port Memory (2 Read, 2 Write)
```text
@module quad_port_mem_16x128
  PORT {
    IN  [7] rd_addr_0;
    IN  [7] rd_addr_1;
    OUT [16] rd_data_0;
    OUT [16] rd_data_1;
    IN  [7] wr_addr_0;
    IN  [7] wr_addr_1;
    IN  [16] wr_data_0;
    IN  [16] wr_data_1;
    IN  [1] wr_en_0;
    IN  [1] wr_en_1;
    IN  [1] clk;
  }

  MEM {
    mem [16] [128] = 16'h0000 {
      OUT rd_0 ASYNC;
      OUT rd_1 ASYNC;
      IN  wr_0;
      IN  wr_1;
    };
  }

  ASYNCHRONOUS {
    rd_data_0 = mem.rd_0[rd_addr_0];
    rd_data_1 = mem.rd_1[rd_addr_1];
  }

  SYNCHRONOUS(CLK=clk) {
    IF (wr_en_0) {
      mem.wr_0[wr_addr_0] <= wr_data_0;
    }

    IF (wr_en_1) {
      mem.wr_1[wr_addr_1] <= wr_data_1;
    }
  }
@endmod
```

Configurable Memory with Parameters
```text
@module param_mem
  CONST {
    WORD_WIDTH = 32;
    DEPTH = 256;
    ADDR_WIDTH = 8;
  }

  PORT {
    IN  [ADDR_WIDTH] rd_addr;
    IN  [ADDR_WIDTH] wr_addr;
    IN  [WORD_WIDTH] wr_data;
    IN  [1] wr_en;
    OUT [WORD_WIDTH] rd_data;
    IN  [1] clk;
  }

  MEM {
    storage [WORD_WIDTH] [DEPTH] = {WORD_WIDTH{1'b0}} {
      OUT rd SYNC;
      IN  wr;
    };
  }

  SYNCHRONOUS(CLK=clk) {
    storage.rd.addr <= rd_addr;
    rd_data <= storage.rd.data;

    IF (wr_en) {
      storage.wr[wr_addr] <= wr_data;
    }
  }
@endmod
```

## MEM in Module Instantiation

Memories are internal to modules and cannot be directly accessed from parent modules. To expose memory operations, wrap them in a module interface:

```text
@module memory_wrapper
  PORT {
    IN  [1]  clk;
    IN  [8]  addr;
    IN  [16] wr_data;
    IN  [1]  wr_en;
    OUT [16] rd_data;
  }

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
@endmod
```

The parent module instantiates the wrapper and accesses memory through its ports.

## CONST Evaluation in MEM

Numeric CONST names may be used in word_width and depth expressions. These are resolved in the module's CONST scope at compile time:

```text
CONST {
  WIDTH = 32;
  DEPTH = 1024;
}

MEM {
  mem [WIDTH] [DEPTH] = {WIDTH{1'b0}} {
    OUT rd SYNC;
    IN  wr;
  };
}
```

String CONST names may be used in `@file()` path arguments:

```text
CONST {
  WIDTH = 32;
  DEPTH = 1024;
  INIT_FILE = "firmware.bin";
}

MEM {
  rom [WIDTH] [DEPTH] = @file(INIT_FILE) {
    OUT rd ASYNC;
  };
}
```

Project-level string CONFIG references may also be used: `@file(CONFIG.FIRMWARE)`.

When used with OVERRIDE, the overriding module's CONST values apply, allowing the same module to be instantiated with different memory sizes.

---

## Common Errors and Diagnostics

- Declaration Errors
  - Invalid or duplicate MEM name.
  - Invalid `word_width` or `depth` (≤ 0 or unresolved CONST).
  - Missing initialization clause.
  - Port name duplicates or conflicts with module identifiers.
  - `INOUT` ports mixed with `IN` or `OUT` ports in the same MEM declaration.
  - `INOUT` port declared with `ASYNC` keyword (not supported).

- Access Errors
  - Asynchronous read used in SYNCHRONOUS incorrectly (forgetting `<=`).
  - Write in ASYNCHRONOUS block.
  - Multiple writes to the same `IN` port in one `SYNCHRONOUS` block (Exclusive Assignment violation).
  - Address or data width mismatch (too wide) — truncation is not implicit.
  - Constant out-of-range address (address literal ≥ depth) — compile error.
  - Using `mem.port[addr]` indexing syntax on `INOUT` ports (must use `.addr`).
  - Assigning `.wdata` in ASYNCHRONOUS block.
  - Assigning `.addr` in ASYNCHRONOUS block.
  - Multiple `.addr` assignments to the same `INOUT` port per execution path.
  - Multiple `.wdata` assignments to the same `INOUT` port per execution path.

- Initialization Errors
  - Init literal overflow (literal intrinsic width > declared word width).
  - Init file not found or too large for memory depth.
  - Init literal or file contains `x`/undefined bits.
  - Numeric CONST/CONFIG used in `@file()` path (`CONST_NUMERIC_IN_STRING_CONTEXT`).
  - String CONST/CONFIG used in width/depth expression (`CONST_STRING_IN_NUMERIC_CONTEXT`).

- Behavioral Warnings
  - Port declared but never accessed.
  - Partial initialization (file smaller than memory) — zero-padding warning.
  - Using ASYNC reads in combinational paths that create loops — flow-sensitive loop detection may flag cycles.

---

## Synthesis and Implementation Notes

- Compiler infers FPGA/ASIC memory primitives from MEM declarations.
  - ASYNC vs SYNC read ports influence whether the tool implements combinational read paths or registered outputs.
  - `type=BLOCK` guides inference toward block RAMs; compiler may validate constraints (e.g., read port timing).
- Write modes map to vendor BRAM settings:
  - `WRITE_FIRST`, `READ_FIRST`, `NO_CHANGE` → vendor BRAM write-mode attributes.
- File-based initialization may be passed to the backend (vendor tools) as memory init files.
- For small depths use DISTRIBUTED RAM; for larger depths prefer BLOCK BRAM. If omitted, compiler makes a choice based on `depth` and port timing.

**Port Type Inference:**
- `ASYNC` `OUT` ports → combinational read (data available same cycle)
- `SYNC` `OUT` ports → registered read (data available next cycle)
- `IN` ports → synchronous write (captured at clock edge)
- `INOUT` ports → synchronous read/write with shared address (read data available next cycle)

---

## MEM as a Register Array Replacement

::: info Looking for register arrays?
JZ-HDL does not support multi-dimensional REGISTER syntax (e.g., `name [depth] [width]`). Use MEM instead — it provides the same functionality with explicit port semantics.
:::

A `MEM(type=DISTRIBUTED)` with an `OUT ASYNC` read port is the direct equivalent of a register array in other HDLs. Both synthesize to flip-flops plus a read mux, with identical timing:

- **Read latency**: zero additional cycles (combinational, same as a register read)
- **Write timing**: synchronous, captured at the clock edge (same as a register write)
- **Synthesis result**: LUT-based storage with mux/decoder (same fabric resources)

The only difference is that MEM requires explicit port declarations, which makes the number of read ports, write ports, and their timing (ASYNC vs SYNC) unambiguous in the source code.

**Example: 8-entry, 32-bit register file with 2 read ports and 1 write port**
```text
MEM(type=DISTRIBUTED) {
  regfile [32] [8] = 32'h0000_0000 {
    OUT rd_a ASYNC;
    OUT rd_b ASYNC;
    IN  wr;
  };
}

ASYNCHRONOUS {
  read_data_a = regfile.rd_a[rd_addr_a];
  read_data_b = regfile.rd_b[rd_addr_b];
}

SYNCHRONOUS(CLK=clk) {
  IF (wr_en) {
    regfile.wr[wr_addr] <= wr_data;
  }
}
```

This is equivalent to what `reg [31:0] regfile [0:7]` would provide in Verilog, but with the read/write port structure made explicit.

---

## Best Practices

- Always declare `WORD_WIDTH` and `DEPTH` using numeric `CONST` when parameterizing modules.
- Use string `CONST` or `CONFIG` for `@file()` paths to make initialization files configurable across builds.
- Prefer explicit `clog2(DEPTH)` or `ADDR_WIDTH` constants for address signals so widths are consistent and self-documenting.
- Use synchronous reads (`SYNC`) when you need registered, timing-stable outputs or to pipeline memory reads.
- Guard dynamic indices when they might be out-of-range or ensure index width covers `clog2(depth)`.
- For register files, prefer ASYNC read ports for zero-latency reads and a single synchronous write port; ensure write-first/read-first behavior matches desired architectural semantics.
- Avoid reading and writing the same address from different ports in the same cycle unless the write mode semantics are explicitly what you require.

---

## Troubleshooting Checklist

- If you see a floating/undefined read value:
  - Verify read port and write ports address widths and that at least one driver provides known data.
  - Check for mistaken ASYNCHRONOUS write attempts (illegal).
- If synthesis maps memory into many small LUTs:
  - Consider changing `type` or increasing depth to encourage BRAM inference or supply vendor-specific attributes.
- If reads return unexpected values on same-cycle read/write:
  - Confirm the `IN` port’s `WRITE_MODE` and whether you intended WRITE_FIRST (default) or READ_FIRST.
