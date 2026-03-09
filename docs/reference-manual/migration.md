---
title: Migration Guide
lang: en-US

layout: doc
outline: deep
---

# Migration Guide

This guide helps engineers coming from Verilog or VHDL understand how JZ-HDL differs — not just syntactically, but in philosophy, mental model, and the kinds of mistakes each language prevents (or allows).

---

## Migrating from Verilog

### Mindset Shift

Verilog trusts the designer. It gives you maximum flexibility — multiple drivers, unsized literals, implicit truncation, arbitrary `initial` blocks — and assumes you know what you're doing. When things go wrong, you find out in simulation (or worse, in silicon).

JZ-HDL trusts the compiler. It is designed so that entire classes of hardware bugs — multi-driver conflicts, clock domain violations, uninitialized registers, latch inference — are **impossible to express**. The language is more restrictive, but every restriction maps to a real hardware failure mode that JZ-HDL prevents at compile time.

**The key shift:** Stop thinking "how do I express this behavior?" and start thinking "what is the hardware I'm building?" JZ-HDL's constructs map directly to physical hardware elements (flip-flops, combinational nets, memory arrays, tri-state buses). If your design compiles, it is structurally correct.

### Structural Differences

| Verilog | JZ-HDL | Why |
| --- | --- | --- |
| `module` / `endmodule` | `@module` / `@endmod` | Directive syntax distinguishes structural elements from logic |
| `wire`, `reg` | `WIRE { }`, `REGISTER { }` | Separate declaration blocks enforce clear intent — wires are combinational, registers are sequential |
| `assign a = b;` | `a <= b;` or `a = b;` | Alias (`=`) merges nets; receive (`<=`) and drive (`=>`) are directional. No ambiguity about driver vs sink |
| `always @(*)` | `ASYNCHRONOUS { }` | Sensitivity list is implicit and always complete — no risk of incomplete sensitivity |
| `always @(posedge clk)` | `SYNCHRONOUS(CLK=clk ...) { }` | Clock, reset, reset polarity, and reset type are declared explicitly — not inferred from code patterns |
| Multiple `always` blocks per clock | One `SYNCHRONOUS` block per clock per module | Eliminates conflicting always blocks; all logic for a domain lives in one place |
| `reg [7:0] mem [0:255];` | `MEM { mem [8] [256] = 8'h00 { ... }; }` | Memories are first-class with explicit port types, read modes, write modes, and initialization |
| No width checking | Strict width matching | `a + b` is an error if `a` and `b` differ in width. Prevents silent truncation bugs |
| `'hFF` (unsized) | `8'hFF` (always sized) | Every literal carries its width. No implicit sizing surprises |

### Quick Mapping Table

```
module              → @module ... @endmod
port declarations   → PORT { IN/OUT/INOUT [N] name; }
wire                → WIRE { name [N]; }
reg                 → REGISTER { name [N] = literal; }  // reset value mandatory
assign (continuous) → ASYNCHRONOUS { lhs <= rhs; }      // or alias: lhs = rhs;
always @(*)         → ASYNCHRONOUS { ... }
always @(posedge c) → SYNCHRONOUS(CLK=c RESET=r RESET_ACTIVE=Low) { ... }
initial mem file    → MEM { name [w] [d] = @file("...") { ... }; }
module inst         → @new inst_name mod_name { ... }
```

### Gotchas

**1. Alias (`=`) is not `assign`.**
In Verilog, `assign a = b;` creates a continuous driver. In JZ-HDL, `a = b;` in ASYNCHRONOUS creates a **bidirectional net alias** — `a` and `b` become the same net. If you want unidirectional drive, use `a <= b;` (receive) or `a => b;` (drive). Critically, `a = 1'b1;` is illegal in ASYNCHRONOUS — you cannot alias to a literal.

```jz
// WRONG: alias to literal
ASYNCHRONOUS {
    valid = 1'b1;  // ERROR: ASYNC_ALIAS_LITERAL_RHS
}

// CORRECT: directional drive
ASYNCHRONOUS {
    valid <= 1'b1;
}
```

**2. Every register needs a reset value.**
Verilog lets you declare `reg [7:0] count;` without initialization. JZ-HDL requires `REGISTER { count [8] = 8'h00; }` — the reset value is mandatory and must not contain `x` bits. This prevents uninitialized register bugs that are notoriously hard to catch in simulation.

**3. One SYNCHRONOUS block per clock.**
In Verilog, you can scatter `always @(posedge clk)` blocks throughout a module. JZ-HDL requires exactly one `SYNCHRONOUS(CLK=clk ...)` block per clock domain per module. Merge all same-clock logic into one block.

```verilog
// Verilog: two separate blocks for same clock
always @(posedge clk) a <= next_a;
always @(posedge clk) b <= next_b;
```

```jz
// JZ-HDL: merged into one block
SYNCHRONOUS(CLK=clk RESET=rst_n RESET_ACTIVE=Low) {
    a <= next_a;
    b <= next_b;
}
```

**4. Exclusive Assignment Rule.**
In Verilog, you can have two independent `if` blocks that both assign to the same signal — the last one wins. In JZ-HDL, each signal bit must be assigned **zero or one time** on every execution path. Overlapping assignments are a compile error.

```jz
// WRONG: two independent IFs assigning same target
ASYNCHRONOUS {
    IF (sel_a) { out <= a; }    // assigns out
    IF (sel_b) { out <= b; }    // ERROR: second assignment to same bits
}

// CORRECT: mutually exclusive branches
ASYNCHRONOUS {
    IF (sel_a) {
        out <= a;
    } ELIF (sel_b) {
        out <= b;
    } ELSE {
        out <= GND;
    }
}
```

**5. No implicit width extension or truncation.**
Verilog silently zero-extends narrow values and truncates wide values. JZ-HDL requires exact width matches for most operators and assignments. Use `<=z` (zero-extend), `<=s` (sign-extend), or explicit slicing/concatenation.

```jz
// WRONG: width mismatch
WIRE { narrow [4]; wide [8]; }
ASYNCHRONOUS {
    wide <= narrow;   // ERROR: 4-bit into 8-bit
}

// CORRECT: explicit zero-extension
ASYNCHRONOUS {
    wide <=z narrow;  // zero-extend narrow from 4 to 8 bits
}
```

**6. Cross-clock reads require CDC.**
In Verilog, nothing stops you from reading a register clocked by `clk_a` inside an `always @(posedge clk_b)` block. JZ-HDL makes this a compile error. You must declare a CDC entry to cross domains.

```jz
CDC {
    BIT flag (clk_a) => flag_sync (clk_b);
}

SYNCHRONOUS(CLK=clk_b RESET=rst_n RESET_ACTIVE=Low) {
    IF (flag_sync) { ... }  // safe: uses synchronized version
}
```

**7. No latch inference.**
In Verilog, an incomplete `if` in `always @(*)` silently infers a latch. In JZ-HDL, latches must be explicitly declared in a `LATCH` block. An incomplete conditional in `ASYNCHRONOUS` that leaves a wire undriven is a compile error, not a latch.

**8. MEM is not a reg array.**
Verilog `reg [7:0] mem [0:255];` is a generic array. JZ-HDL `MEM` is a first-class memory with explicit port types (`OUT ASYNC`, `OUT SYNC`, `IN`, `INOUT`), write modes (`WRITE_FIRST`, `READ_FIRST`, `NO_CHANGE`), and mandatory initialization. The compiler maps these directly to BSRAM, LUTRAM, or distributed memory based on the declaration.

### Worked Examples

#### Continuous assignment to literal

```verilog
// Verilog
assign valid = 1'b1;
```

```jz
// JZ-HDL
ASYNCHRONOUS {
    valid <= 1'b1;
}
```

#### Behavioral RAM to MEM

```verilog
// Verilog
reg [31:0] mem [0:255];

always @(posedge clk) begin
    if (we) mem[addr] <= wdata;
end

assign rdata = mem[addr2];
```

```jz
// JZ-HDL
MEM {
    mem [32] [256] = 32'h0000_0000 {
        OUT rd ASYNC;    // combinational read
        IN  wr;          // synchronous write
    };
}

ASYNCHRONOUS {
    rdata <= mem.rd[addr2];
}

SYNCHRONOUS(CLK=clk RESET=rst_n RESET_ACTIVE=Low) {
    IF (we) {
        mem.wr[addr] <= wdata;
    }
}
```

#### Cross-clock register read

```verilog
// Verilog (BUG: unsynchronized clock crossing)
always @(posedge clk_a) reg_x <= next_val;
always @(posedge clk_b) snapshot <= reg_x;  // metastability risk!
```

```jz
// JZ-HDL (compiler-enforced CDC)
REGISTER {
    reg_x [8] = 8'h00;
    snapshot [8] = 8'h00;
}

CDC {
    BIT reg_x (clk_a) => reg_x_sync (clk_b);
}

SYNCHRONOUS(CLK=clk_a RESET=rst_n RESET_ACTIVE=Low) {
    reg_x <= next_val;
}

SYNCHRONOUS(CLK=clk_b RESET=rst_n RESET_ACTIVE=Low) {
    snapshot <= reg_x_sync;
}
```

### Migration Checklist (Verilog)

- [ ] Replace all unsized literals with sized ones
- [ ] Add reset literal to every REGISTER
- [ ] Merge multiple `always @(posedge clk)` blocks into one SYNCHRONOUS block per clock
- [ ] Replace cross-clock register reads with CDC entries
- [ ] Convert behavioral memories to MEM declarations with correct port types
- [ ] Review all assignments for Exclusive Assignment Rule compliance
- [ ] Replace `assign` to literal with directional `<=` in ASYNCHRONOUS
- [ ] Add `=z`/`=s` modifiers or explicit extension wherever widths differ
- [ ] Remove any `x` values that can reach observable sinks
- [ ] Verify top-level pin mapping matches project PIN declarations
- [ ] Replace implicit latch inference with explicit LATCH declarations

---

## Migrating from VHDL

### Mindset Shift

VHDL engineers will find JZ-HDL's strictness familiar — both languages value strong typing and compile-time checking. However, the two languages have fundamentally different philosophies about *what* they check and *how* they express hardware.

VHDL is a **simulation language** that also synthesizes. Its roots in Ada mean it has a rich type system, generics, packages, configurations, and a detailed simulation kernel with delta cycles, wait statements, and signal resolution functions. It models hardware behavior through simulation semantics, and synthesis tools extract hardware from behavioral descriptions.

JZ-HDL is a **hardware description language** that also simulates. It starts from physical hardware invariants — what a flip-flop is, what a net is, what a clock domain is — and builds its type system and rules around making hardware bugs impossible. There is no simulation kernel, no delta cycle model, no process sensitivity lists. The language constructs *are* the hardware.

**The key shift:** Stop thinking in terms of processes, signals, and simulation time. Think in terms of registers, wires, clock domains, and driver ownership. JZ-HDL has no equivalent of VHDL's simulation-oriented features (wait, after, transport delay, signal attributes like `'event`) because these are not hardware — they are simulation abstractions.

### Structural Differences

| VHDL | JZ-HDL | Why |
| --- | --- | --- |
| `entity` + `architecture` | `@module` / `@endmod` | No separation between interface and implementation — the module is one unit |
| `signal`, `variable` | `WIRE { }`, `REGISTER { }` | No distinction between signal and variable. Wires are combinational nets; registers are flip-flops. Period. |
| `process(clk)` with sensitivity | `SYNCHRONOUS(CLK=clk ...)` | Clock, reset, and reset behavior are declared, not inferred from `if rising_edge(clk)` patterns |
| `process(all)` / explicit sensitivity | `ASYNCHRONOUS { }` | Always fully sensitive — no risk of incomplete sensitivity list |
| `generic` | `CONST { }` + `OVERRIDE` | Module-local constants overridden at instantiation. No packages or configurations needed |
| `package` / `use` | `@global` / `@import` | Global constants are sized literals; imports bring modules into scope |
| `std_logic_vector` | `[N]` width annotation | No type declarations needed — every signal has a bit width, that's it |
| `integer`, `natural`, `boolean` | Not applicable | JZ-HDL has one type: bit vectors. No integers, no enums, no records at the RTL level. CONST values are compile-time integers only |
| `generate` | `@template` / `@apply` + array instances | Compile-time code generation through templates and array instantiation |
| `configuration` | Not applicable | No configurations — one module, one implementation |
| `component` + `port map` | `@new inst mod { ... }` | Direct instantiation with inline port binding |
| `type state_t is (...)` | `CONST { }` + `@global` | State encodings are explicit constants, not abstract enums |
| `after 10 ns`, `wait for`, `wait until` | Not applicable | No timing constructs in RTL. Use `@testbench` or `@simulation` for verification |

### Philosophical Differences

**1. No separation of interface and implementation.**
VHDL separates `entity` (ports) from `architecture` (logic), and allows multiple architectures per entity. JZ-HDL combines both into a single `@module` — there is exactly one implementation per module. If you need variants, use `CONST` with `OVERRIDE` at instantiation.

**2. No type system beyond bit widths.**
VHDL has `std_logic`, `std_logic_vector`, `unsigned`, `signed`, `integer`, `natural`, `boolean`, custom types, records, and arrays of records. JZ-HDL has one type: a bit vector with a known width. This is deliberate — the type system is the **width system**. Every operation is width-checked at compile time, and there is no ambiguity about what `+` does (it is always unsigned, same-width addition). For signed operations, use `sadd`/`smul` intrinsics explicitly.

**3. No simulation semantics in RTL.**
VHDL's `signal` vs `variable` distinction, delta cycles, postponed processes, and `'event` / `'stable` attributes are all simulation concepts that happen to synthesize. JZ-HDL has no simulation model in its RTL — ASYNCHRONOUS blocks are combinational logic, SYNCHRONOUS blocks are clocked registers. The simulation semantics are defined separately in `@testbench` and `@simulation` constructs.

**4. No `process` — explicit block types instead.**
In VHDL, a `process` can be combinational or sequential depending on what you put in it. In JZ-HDL, you declare your intent: `ASYNCHRONOUS` for combinational logic, `SYNCHRONOUS` for sequential. The compiler enforces the rules for each (e.g., you cannot write to a register in ASYNCHRONOUS).

**5. Explicit reset, always.**
VHDL patterns like `if rising_edge(clk) then if rst = '0' then ...` are synthesizable but fragile — swap the order and you change the reset type (sync vs async). JZ-HDL declares `RESET`, `RESET_ACTIVE`, and `RESET_TYPE` explicitly in the SYNCHRONOUS block header. No ambiguity.

### Gotchas

**1. No `std_logic` resolution.**
VHDL's `std_logic` has a 9-value resolution function (`U`, `X`, `0`, `1`, `Z`, `W`, `L`, `H`, `-`). JZ-HDL has 4 values: `0`, `1`, `x`, `z`. There is no `U` (uninitialized) — registers start with random bits in simulation. There is no `W`, `L`, `H` — use `0`, `1`, and `z` directly. The resolution function for tri-state is built into the language (one active driver, others must be `z`).

**2. No generics/packages — use CONST and @global.**
VHDL packages with constants and type definitions don't exist. Instead:
- Module-local constants: `CONST { WIDTH = 8; }`
- Override at instantiation: `OVERRIDE { WIDTH = 16; }`
- Global constants shared across modules: `@global NAME ... @endglob`

```jz
@module fifo
    CONST {
        WIDTH = 8;
        DEPTH = 16;
        ADDR_W = clog2(DEPTH);
    }

    PORT {
        IN  [WIDTH] din;
        OUT [WIDTH] dout;
        // ...
    }
@endmod

// Instantiation with override
@new big_fifo fifo {
    OVERRIDE { WIDTH = 32; DEPTH = 256; }
    // ...
}
```

**3. Signal assignment (<=) means the same thing, sort of.**
In VHDL, `<=` is signal assignment (scheduled update). In JZ-HDL, `<=` is "receive" — the RHS drives the LHS. In SYNCHRONOUS blocks, `<=` behaves like VHDL's `<=` (non-blocking, register update). In ASYNCHRONOUS blocks, `<=` is a combinational drive (no scheduling — it's a continuous assignment). The mental model is similar but the semantics are subtly different.

**4. No `generate` — use templates and array instances.**
VHDL's `for ... generate` and `if ... generate` become `@template` / `@apply` (for parameterized logic blocks) and array instantiation `@new name[N] module { ... }` with the `IDX` keyword for indexed expansion.

**5. No `wait` statements or timing in RTL.**
VHDL testbenches use `wait for 10 ns;`, `wait until rising_edge(clk);`, etc. JZ-HDL separates verification from RTL entirely:
- `@testbench` for cycle-accurate assertions with `@clock(clk, cycle=N)`
- `@simulation` for time-based waveform analysis with `@run(ns=100)`

Neither construct can appear in `@module` definitions.

**6. Incomplete conditionals don't infer latches.**
In VHDL, an incomplete `if` in a combinational `process` infers a latch (which is usually a bug). In JZ-HDL, an incomplete conditional in `ASYNCHRONOUS` that leaves a wire undriven is a **compile error**. If you actually want a latch, declare it explicitly in a `LATCH` block.

**7. No records or arrays of records.**
VHDL's record types and arrays of records are common for bus structures. JZ-HDL uses `BUS` definitions for structured signal groups:

```jz
BUS PARALLEL_BUS {
    OUT   [16] ADDR;
    OUT   [1]  CMD;
    INOUT [16] DATA;
    IN    [1]  DONE;
}

// In a module:
PORT {
    BUS PARALLEL_BUS SOURCE [4] tgt;  // 4-element bus array
}
```

**8. No `integer` type for counters and indices.**
In VHDL, you might use `integer range 0 to 255` for a counter. In JZ-HDL, counters are always bit vectors: `REGISTER { count [8] = 8'h00; }`. Comparisons and arithmetic are always on fixed-width bit vectors. Use `clog2()` to compute address widths from depth constants.

### Migration Checklist (VHDL)

- [ ] Merge entity and architecture into a single `@module`
- [ ] Replace `std_logic_vector(N-1 downto 0)` with `[N]`
- [ ] Replace `signal` declarations with `WIRE` or `REGISTER` as appropriate
- [ ] Replace combinational `process(all)` with `ASYNCHRONOUS { }`
- [ ] Replace clocked `process(clk)` with `SYNCHRONOUS(CLK=clk ...)` — declare reset explicitly
- [ ] Replace generics with `CONST { }` and `OVERRIDE` at instantiation
- [ ] Replace packages with `@global` blocks and `@import`
- [ ] Replace record types with `BUS` definitions
- [ ] Replace `generate` statements with `@template`/`@apply` or array instances
- [ ] Replace `integer` and enumeration types with sized bit vectors and CONST values
- [ ] Move testbench processes to `@testbench` or `@simulation` constructs
- [ ] Replace `wait for` / `wait until` with `@clock` / `@run` directives
- [ ] Ensure all registers have reset values (no uninitialized signals)
- [ ] Check width matching on all assignments and operators
