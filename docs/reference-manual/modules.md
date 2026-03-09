---
title: Modules
lang: en-US

layout: doc
outline: deep
---

# Modules

## Module canonical form

A module uses this canonical structure:

```text
@module <module_name>
  CONST { ... }           // optional
  PORT { ... }            // required, at least one PORT entry
  WIRE { ... }            // optional
  REGISTER { ... }        // optional
  MUX { ... }             // optional
  MEM { ... }             // optional (see MEM section)
  CDC { ... }             // optional (defines clock domain crossings)
  @template <name> (...)   // optional (compile-time reusable logic)
  ASYNCHRONOUS { ... }    // optional
  SYNCHRONOUS(CLK=...) { ... } // optional (can have multiple SYNCHRONOUS with distinct clocks)
@endmod
```

Notes
- Module name and all internal identifiers must follow identifier syntax.
- Every module must declare at least one PORT block. An empty or missing PORT block is a compile error.
- Names inside a module (ports, wires, registers, constants, instance names) must be unique.

---

## CONST (module-local compile-time constants)

Purpose
- Define compile-time constants usable inside the module.

Syntax
```text
CONST {
  NAME = <nonnegative_integer>;    // numeric constant
  NAME = "<string>";               // string constant
  ...
}
```

Rules
- CONST values are evaluated at compile time and are visible only inside the module that declares them.
- Numeric CONST names may be used in width brackets and other compile-time integer expressions (not as runtime values).
- String CONST names hold file paths or other text. They may be used in `@file()` path arguments in MEM declarations.
- Using a string CONST where a number is expected (or vice versa) is a compile error.
- Forward references or undefined CONST usage → compile error.

Example
```text
CONST {
  XLEN = 32;
  DEPTH = 256;
  ROM_FILE = "data/lookup.hex";
}
```

---

## PORT (module interface)

Purpose
- Define the external interface of the module.

Syntax
```text
PORT {
  IN    [<width>]  name;
  OUT   [<width>]  name;
  INOUT [<width>]  name;
  ...
}
```

Rules and semantics
- Width is mandatory; omission → compile error.
- Direction enforces usage: assigning to an IN port is a compile error; reading from an OUT port is a compile error.
- INOUT ports support tri-state: assign `z` to release the port. OUT ports may also assign `z` to release the port.
- All ports must be present when this module is instantiated by a parent (the parent must list every child port in its `@new`).
- Port identifiers share the module namespace.

Example
```text
PORT {
  IN    [1] clk;
  IN    [8] data_in;
  OUT   [8] data_out;
  INOUT [8] bus;
}
```

### BUS Ports

Syntax (inside a module PORT block):
```text
PORT {
  BUS <bus_id> <ROLE> \[N\] <port_name>;
}
```

- ROLE is SOURCE or TARGET and tells the compiler which side of the transaction the module represents.
  - SOURCE: the module acts as the bus initiator — the bus signals keep the directions defined in the BUS block.
  - TARGET: the module acts as the responder — each signal direction is flipped relative to the BUS block.
- The optional \[N\] produces an array of N independent bus instances. Each element `port_name[i]` is a separate BUS port with identical structure and role (0‑based indices).
- Access to individual signals uses dot notation: `port_name[i].signal` (or `port_name.signal` when not arrayed).

Direction resolution (summary)
- For SOURCE role, bus signal directions match the BUS definition.
- For TARGET role, IN ↔ OUT are swapped; INOUT remains INOUT.

Connecting buses (bulk and per-signal wiring)

Bulk assignment (concise connection of two compatible bus ports):
- Two bus ports of the same bus_id can be bulk‑connected in ASYNCHRONOUS (or other appropriate) context:
  `inst_a.bus_a = inst_b.bus_b;`
- The compiler expands the bulk connection into individual signal assignments per the BUS definition and roles:
  - For signals that are SOURCE-OUT / TARGET-IN: assignment flows from SOURCE → TARGET.
  - For SOURCE-IN / TARGET-OUT: assignment flows from TARGET → SOURCE.
  - For INOUT signals: a symmetric alias is created (tri-state/shared net).

Elaboration rules
- Bulk expansion becomes a set of directional assignments (<= or =>) or aliases, as appropriate.
- All paired signals must have identical widths; no implicit sign- or zero-extension or truncation is performed.
- Arrayed bus bulk connections expand element-wise; indexes must align or be explicitly addressed.

Validation rules and common errors
- Compatibility: bulk connection valid only when both ports reference the same bus_id.
- Role conflicts: connecting two SOURCEs or two TARGETs for the same driven signal will typically produce multiple‑driver or floating‑net errors.
- Width mismatch between corresponding signals → compile error.
- Partial connections: any unconnected signal must be explicitly tied to `_` or otherwise handled; implicit omissions are errors.
- BUS definitions referenced by ports or bulk connects must appear earlier in the project so they can be resolved.

Usage guidance and best practices
- Use BUS when several signals are always carried together (data+ctrl+ready/valid, memory interfaces, peripheral groups).
- Prefer SOURCE/TARGET roles over manual per-signal directioning to avoid mistakes when reusing a bus across initiator and responder modules.
- For INOUT signals that are intended to be shared tri-state nets, use the symmetric alias form (bulk assignment) and ensure the design-level arbitration/tri-state semantics are correct.
- When instantiating arrays of bus ports, prefer explicit index mapping in the MAP or in connections to keep intent clear.

Example (conceptual)
- Define a bus with data, valid and ready fields in the project BUS block.
- In a CPU module PORT: BUS membus SOURCE [1] cpu_mem;
- In a peripheral module PORT: BUS membus TARGET [1] periph_mem;
- In a parent module ASYNCHRONOUS block: cpu_inst.cpu_mem = periph_inst.periph_mem; (expands into per-signal assignments with directions resolved by role)

Validation checklist (before compile)
- BUS ids are unique and declared before use.
- Module ports that use BUS reference an existing bus_id.
- Roles (SOURCE/TARGET) are chosen consistently for the intended transaction direction.
- Bulk connections only link matching bus_id ports and have matching widths.
- Any partially-connected bus signals are explicitly handled (assigned to `_` or wired).

---

## WIRE (combinational nets)

Purpose
- Declare intermediate combinational signals.

Syntax
```text
WIRE {
  name [<width>];
  ...
}
```

Rules
- WIREs have no storage and must be driven in ASYNCHRONOUS blocks only.
- Each net must have exactly one active driver per path, or tri-state drivers where all but one assign `z`.
- WIREs are readable anywhere in the module but writable only in ASYNCHRONOUS.
- Multi-dimensional wire syntax is unsupported (use MEM for arrays).

Example
```text
WIRE {
  alu_out [32];
  flag    [1];
}
```

---

## REGISTER (storage elements)

Purpose
- Declare flip-flop storage; mandatory reset value required.

Syntax
```text
REGISTER {
  name [<width>] = <literal>;
  ...
}
```

Rules and semantics
- Registers expose a current-value output (readable in all blocks) and a next-state input (written only in SYNCHRONOUS blocks).
- Reset/power-on literal must be fully known (no `x` bits).
- Assigning to a register in ASYNCHRONOUS → compile error.
- Sliced register writes allowed in SYNCHRONOUS blocks (non-overlapping per execution path).

Example
```text
REGISTER {
  counter [16] = 16'h0000;
  state   [4]  = 4'h0;
}
```

::: info Why doesn't JZ-HDL support register arrays?
REGISTER is intentionally single-dimensional. Any indexed storage requires addressing, which raises questions that REGISTER's implicit interface cannot express: how many simultaneous reads? How many writes? Synchronous or asynchronous reads?

Use `MEM(type=DISTRIBUTED)` with an `OUT ASYNC` read port instead. It synthesizes to the same flip-flops and muxes as a register array would, with the same zero-cycle read latency and same synchronous writes. The only difference is that MEM requires you to declare your ports explicitly, making the hardware intent unambiguous. See the [Memory](/reference-manual/memory) page for details and examples.
:::

---

## MUX (aggregation & slicing)

Purpose
- Provide a read-only, indexable view that selects among multiple like‑width signals or slices a wide signal into fixed-size elements.

Syntax (two forms)
1) Aggregation
```text
MUX {
  name = src0, src1, src2, ...;
}
```
2) Auto-slicing
```text
MUX {
  name [element_width] = wide_source;
}
```

Key rules
- MUX names share the module namespace and are read-only.
- All aggregated sources must have identical widths; otherwise compile error.
- Auto-slice requires wide_source width to be an exact multiple of element_width.
- Index out of range:
  - If statically provable → compile error.
  - Otherwise → behavior is hardware implementation-defined at runtime. Simulation tools must abort on out-of-range access.
- Selector width must be >= clog2(N) (implicit zero-extend if narrower).

Usage
- `mux_id[sel]` can be used on RHS in ASYNCHRONOUS and SYNCHRONOUS.
- Assigning to a MUX is a compile error.

Examples
```text
MUX {
  bytes = byte0, byte1, byte2, byte3;
  slice [8] = data_bus;   // slices data_bus into 8-bit elements
}
```

---

## LATCH (level-sensitive storage)

Purpose
- Declare level-sensitive storage elements (D latches or SR latches) for designs that require transparent latching behavior.

Syntax
```text
LATCH {
  <name> [<width>] <type>;   // type is D or SR
}
```

Rules
- Latches are written only in ASYNCHRONOUS blocks using guarded assignment syntax:
  - D latch: `latch_name <= enable : data;` — when enable is high, data passes through; when low, output holds.
  - SR latch: `latch_name <= set : reset;` — set/reset signals control the stored value.
- Latches have no clock domain and are purely level-sensitive.
- The Exclusive Assignment Rule applies: each latch may be assigned at most once per execution path.
- Latches may not be used as a clock, reset, or CDC source.
- Latches may not be aliased (`=`) to another net.
- Latches may not be written in `SYNCHRONOUS` blocks.

SR latch truth table (per bit):

| set | reset | latch value |
|-----|-------|-------------|
| 1   | 0     | 1           |
| 0   | 1     | 0           |
| 0   | 0     | hold        |
| 1   | 1     | metastable  |

Example
```text
LATCH {
  transparent_val [8] D;
}

ASYNCHRONOUS {
  transparent_val <= enable : data_in;
}
```

---

## MEM (overview & pointers)

Note
- MEM is described in detail on the Memory page. This section summarizes module-level constraints.

Highlights
- MEM blocks are declared inside modules to define block/distributed RAM.
- MEM declares word width, depth, initial content (literal, `@file("path")`, or `@file(CONST/CONFIG)` reference), and ports (`OUT` ASYNC|SYNC, `IN` write ports).
- Address widths are computed as clog2(depth) (0 if depth==1).
- MEM names and port names are local to the module and must be unique.

For full details, see the Memory reference page (link from site).

---

## CDC (clock‑domain crossings inside a module)

Purpose
- Declare safe, synthesized clock-domain crossings and set the home-domain clocks of registers.

Syntax
```text
CDC {
  BIT[n]  src_reg (src_clk) => dest_alias (dest_clk);
  BUS[n]  src_reg (src_clk) => dest_alias (dest_clk);
  FIFO[n] src_reg (src_clk) => dest_alias (dest_clk);
}
```
- `[n]` optional stage count (default 2)

Semantics & rules
- The source register must be a REGISTER; the CDC entry *sets its home clock* to the source clock.
- The destination alias is a read-only synchronized view usable in the dest domain; reading it in other domains is a domain-conflict.
- `BIT` is for single-bit synchronization; `BUS` is for Gray-coded multi-bit crossings; `FIFO` is for full multi-bit FIFOs; `HANDSHAKE` is for infrequent multi-bit req/ack transfers; `PULSE` is for single-bit pulse events; `MCP` is for multi-cycle path stable data transfers.
- CDC entries define visibility and enforce the rule that a REGISTER cannot be used across domains without explicit CDC.

Example
```text
CDC {
  BIT io_flag (clk_io) => cpu_flag (clk_cpu);
  BUS cpu_buf (clk_cpu) => io_buf (clk_io);
}
```

Notes
- Use `BUS` only when the source follows Gray-code discipline or when semantics permit.
- `FIFO` implements an async FIFO pattern with handshake behavior.

---

## ASYNCHRONOUS (combinational logic)

Purpose
- Define purely combinational behavior and net wiring.

Syntax
```text
ASYNCHRONOUS {
  <statement>;
  ...
}
```

Allowed statements
- Alias: `a = b;` (merges nets; cannot alias a literal RHS)
- Directional drive: `a => b;`
- Directional receive: `a <= b;`
- Sliced: `a[H:L] = b[H2:L2];`
- Concatenation LHS: `{a, b} = expr;`
- IF / ELIF / ELSE, SELECT / CASE control flow

Important alias rules
- Alias operators (`=` family) create net merges and are forbidden inside conditional control flow. Use directional assignments or ternary expressions for conditional wiring.
- Alias RHS may not be a bare literal in ASYNCHRONOUS (use `<=` or `=>` to drive literals).

Extension modifiers
- `=z`, `=s`, `=>z`, `=>s`, `<=z`, `<=s` allow zero- or sign-extension when widths differ.
- Without modifiers, widths must match exactly.

Flow-sensitive net rules
- Exclusive Assignment Rule applies: per execution path, each assignable identifier bit must be assigned zero or one time.
- Combinational loop detection is flow-sensitive: mutually exclusive assignments do not create cycles.

Example
```text
ASYNCHRONOUS {
  result = sel ? src_a : src_b;
  bus = enable ? out_val : 8'bzzzz_zzzz;
}
```

Common pitfalls
- `a = 1'b1;` inside ASYNCHRONOUS → ERROR (alias with literal RHS disallowed)
- Two independent IFs assigning same net at same nesting level → exclusive-assignment violation

---

## SYNCHRONOUS (sequential logic & clock-domain rules)

Purpose
- Define register next-state drivers and other clocked behavior.

Syntax (header)
```text
SYNCHRONOUS(
  CLK=<name>
  EDGE=[Rising|Falling|Both]         // optional (default Rising)
  RESET=<name>                       // optional
  RESET_ACTIVE=[High|Low]            // optional (default Low)
  RESET_TYPE=[Immediate|Clocked]     // optional (default Clocked)
) {
  <stmt>;
}
```

Key rules
- `EDGE=Both` generates dual-edge-triggered logic. This is not a standard FPGA primitive on most architectures and may not synthesize correctly. The compiler emits the warning: **"S4.11 EDGE=Both (dual-edge clocking) may not be supported by all FPGA architectures"** (`SYNC_EDGE_BOTH_WARNING`).
- A module may contain at most one SYNCHRONOUS block for any given clock signal (clock uniqueness). All logic using the same clock must be in the same block.
- Registers are bound to the SYNCHRONOUS block where they are first assigned (home domain). Using a register in another clock domain without CDC is a DOMAIN_CONFLICT error.
- Reset (if present) takes priority; when reset is active, registers load their reset values according to RESET_TYPE.
- Only directional operators (`<=`, `=>` and modifiers) are allowed for register updates. Net aliasing (`=`) is forbidden.
- Each register (or bit-range) must be assigned at most once per execution path (Exclusive Assignment Rule applied to registers).
- If a register is unassigned on a path, it holds its current value (clock gating semantics).

Allowed statements
- Register assignments (sliced, concatenation decomposition)
- IF/ELIF/ELSE, SELECT/CASE
- Writes to MEM write ports (synchronous IN ports)
- Synchronous reads from MEM `OUT SYNC` ports via `<=`

Examples
```text
SYNCHRONOUS(CLK=clk) {
  IF (load) {
    reg_value <= input_val;
  } ELSE {
    reg_value <= reg_value + 1;
  }
}
```

Errors to watch
- Multiple SYNCHRONOUS blocks with same CLK → DUPLICATE_BLOCK
- Assigning a register in a SYNCHRONOUS block that is not its home domain → DOMAIN_CONFLICT
- Assigning same register twice on the same path → Exclusive Assignment Violation

---

## Module instantiation (`@new`)

Purpose
- Instantiate child modules inside a parent module, creating a hierarchical design.

Syntax
```text
@new <instance_name> <module_name> {
  OVERRIDE {
    <child_const_id> = <expr>;   // optional: override child CONST values
    ...
  }
  IN    [<width>] <port_name> = <parent_signal_or_literal>;
  OUT   [<width>] <port_name> = <parent_signal>;
  INOUT [<width>] <port_name> = <parent_signal>;
  BUS   <bus_id> <ROLE> [<count>] <port_name> = <parent_bus>;
  ...
}
```

Rules
- All child ports must be listed; omitting a port is a compile error.
- Use `_` as a no-connect placeholder for unused ports.
- Width expressions in `@new` are evaluated in the parent scope (parent CONST/CONFIG).
- Assigning a parent signal to a child OUT port counts as a driver in the parent module (the Exclusive Assignment Rule applies).
- BUS bindings must match the child BUS port's bus_id, role, and array count.
- OVERRIDE values replace child CONST values before the child module is elaborated.

### Array instantiation

```text
@new <instance_name>[<count>] <module_name> {
  IN  [<width>] <port_name> = <parent_expr_with_IDX>;
  OUT [<width>] <port_name> = <parent_expr_with_IDX>;
  ...
}
```

- The count is a positive integer or parent `CONST`.
- `IDX` is a special variable available only in parent-side expressions within the array mapping. It represents the current array index (0-based).
- `IDX` is prohibited inside `OVERRIDE` blocks — override values must be the same for every array element.
- Per-instance OUT mappings must not drive overlapping parent bits (non-overlap rule).

Example
```text
@new alu_inst alu_module {
  OVERRIDE { WIDTH = 16; }
  IN  [16] a = data_a;
  IN  [16] b = data_b;
  OUT [16] result = alu_result;
}

// Array instantiation
@new decoders[4] decoder {
  IN  [8] input = wide_data[IDX*8+7 : IDX*8];
  OUT [1] valid = valid_bits[IDX];
}
```

---

## Templates (`@template` / `@apply`)

Purpose
- Define compile-time reusable logic blocks that expand inline, reducing code duplication without creating new hardware structure.

Definition syntax
```text
@template <template_id> (<param_0>, <param_1>, ...)
  <template_body>
@endtemplate
```

Application syntax
```text
@apply <template_id> (<arg_0>, <arg_1>, ...);
@apply [count] <template_id> (<arg_0>, <arg_1>, ...);
```

Rules and semantics
- Templates may be placed at module scope (visible within the module) or file scope (visible to all modules).
- `@apply` is valid only inside `ASYNCHRONOUS` or `SYNCHRONOUS` bodies.
- Templates may contain assignments (`<=`, `=>`, `=` and modifiers), conditionals (`IF`/`ELIF`/`ELSE`, `SELECT`/`CASE`), expressions, and scratch wires.
- Templates may **not** contain declarations (`WIRE`, `REGISTER`, `PORT`, `CONST`, `MEM`, `MUX`), `@new`, `CDC`, `@feature`, nested `@template`, or block headers.
- All identifiers in a template must be parameters or scratch wires. External signals must be passed as arguments.
- Scratch wires (`@scratch name [width];`) are anonymous internal nets that exist only within the expansion site. Width must be a compile-time constant (may use `widthof()`, `clog2()`).

Unrolling with `@apply [count]`
- When count > 1, the template is expanded `count` times with `IDX` substituted as a compile-time integer literal (0 to count-1).
- `IDX` may only be used in compile-time contexts (slice bounds, indices, CONST initializers). Using it as a runtime value is a compile error.
- If expansion produces duplicate identifiers, the template author must include `IDX` in names to ensure uniqueness.

After expansion, all normal semantic rules apply — the Exclusive Assignment Rule, width matching, and driver determinism are enforced as if the code were written by hand.

Example — saturating add
```text
@template SAT_ADD(a, b, out)
  @scratch sum [widthof(a)+1];
  sum <=z uadd(a, b);
  out <= sum[widthof(a)] ? {widthof(a){1'b1}} : sum[widthof(a)-1:0];
@endtemplate

// Usage
ASYNCHRONOUS {
  @apply SAT_ADD(x, y, result);
}
```

Example — unrolling with IDX
```text
@template grant(pbus_in, grant_reg)
  IF (pbus_in[IDX].REQ == 1'b1) {
    grant_reg[IDX+1:0] <= 1'b1;
  } ELSE {
    grant_reg[IDX+1:0] <= 1'b0;
  }
@endtemplate

SYNCHRONOUS(CLK=clk, RESET=reset, RESET_ACTIVE=High) {
  @apply[CONFIG.SOURCES] grant(pbus, reg);
}
```

For full details, see the [Templates formal reference](/reference-manual/formal-reference/templates).

---

## Feature guards (`@feature`)

Purpose
- Conditionally include or exclude declarations and statements based on compile-time configuration values. Enables building parameterized designs where entire blocks of logic can be enabled or disabled.

Syntax
```text
@feature <config_expr>
  ... // declarations or statements when condition is true
@else
  ... // optional alternative
@endfeat
```

Rules
- The config expression must be a compile-time, width-1 expression.
- The expression may only reference `CONFIG`, `CONST`, literals, and logical operators (`&&`, `||`, `!`, `==`, `!=`, `<`, `>`, `<=`, `>=`).
- `@feature` may appear anywhere a declaration or statement is valid (inside modules, inside blocks).
- `@feature` blocks may **not** be nested.
- The block does not create a new scope. Identifiers declared inside `@feature` are visible to the entire module.
- Both the enabled and disabled variants must be semantically valid (the compiler checks both paths).
- `@else` is optional.

Example
```text
@feature CONFIG.HAS_UART
  @new uart_inst uart_module {
    IN  [1] clk = clk;
    IN  [8] tx_data = uart_tx_data;
    OUT [1] tx = uart_tx;
  }
@else
  ASYNCHRONOUS {
    uart_tx <= 1'b1;  // idle high when UART not present
  }
@endfeat
```

---

## Module-level examples

1) Minimal module
```text
@module simple
  PORT {
    IN  [1] clk;
    IN  [8] inb;
    OUT [8] outb;
  }

  REGISTER {
    r [8] = 8'h00;
  }

  ASYNCHRONOUS {
    outb = r;
  }

  SYNCHRONOUS(CLK=clk) {
    r <= inb;
  }
@endmod
```

2) MUX aggregation
```text
@module mux_example
  PORT {
    IN  [2] sel;
    IN  [8] a, b, c, d;
    OUT [8] out;
  }

  MUX {
    group = a, b, c, d;
  }

  ASYNCHRONOUS {
    out = group[sel];
  }
@endmod
```

3) Register locality + CDC
```text
@module cdc_demo
  PORT {
    IN [1] clk_src;
    IN [1] clk_dst;
  }

  REGISTER {
    src_reg [8] = 8'h00;
  }

  CDC {
    BUS src_reg (clk_src) => dst_view (clk_dst);
  }

  SYNCHRONOUS(CLK=clk_src) {
    src_reg <= src_reg + 1;
  }

  SYNCHRONOUS(CLK=clk_dst) {
    // dst_view is the synchronized view
    some_reg <= dst_view;
  }
@endmod
```
