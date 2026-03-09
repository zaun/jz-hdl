---
title: Core Concepts
lang: en-US

layout: doc
outline: deep
---

# Core Concepts

## Identifiers

**Syntax:** `(?!^_$)^[A-Za-z_][A-Za-z0-9_]{0,254}$`

- ASCII letters, digits, underscore
- Max length 255; case-sensitive
- Single underscore `_` is reserved for module instantiation no-connect only
- Keywords (uppercase, reserved):
  - Project: `CLOCKS`, `IN_PINS`, `OUT_PINS`, `INOUT_PINS`, `MAP`, `CLOCK_GEN`, `BUS`
  - Flow Control: `IF`, `ELIF`, `ELSE`, `SELECT`, `CASE`, `DEFAULT`, `MUX`
  - Config / Parameters: `CONFIG`, `CONST`, `OVERRIDE`
  - Connections: `PORT`, `IN`, `OUT`, `INOUT`, `WIRE`, `SOURCE`, `TARGET`
  - Storage: `REGISTER`, `LATCH`, `MEM`
  - Logic Type: `ASYNCHRONOUS`, `SYNCHRONOUS`
  - Clock Domains: `CDC`
- Identifiers (uppercase, reserved):
  - Clock Types: `PLL`, `DLL`, `CLKDIV`
  - Clock Domain Crossing Types: `BIT`, `BUS`, `FIFO`, `HANDSHAKE`, `PULSE`, `MCP`, `RAW`
  - Memory Types: `BLOCK`, `DISTRIBUTED`
  - Memory Ports: `ASYNC`, `SYNC`, `WRITE_FIRST`, `READ_FIRST`, `NO_CHANGE`
  - Template / Array: `IDX`
  - Semantic Drivers: `VCC`, `GND`
- Directives (prefixed with `@`, structural):
  - `@project` / `@endproj`: Project definition including board connections, clocks and the top module
    - `@import`: Import external modules, blackboxes, globals into a project
    - `@blackbox`: Declare an opaque (blackbox) module inside a project
    - `@top`: Set the top level module or blackbox for the project
  - `@module` / `@endmod`: A single hardware definition
    - `@new`: Instantiate module or blackbox
    - `@check`: Compile-time assertion
    - `@feature` / `@else` / `@endfeat`: Conditional feature guard block
    - `@template` / `@endtemplate`: Template definition for reusable logic
      - `@scratch`: Declare a scratch wire inside a template body
    - `@apply`: Expand a template at the callsite
    - `@file`: File-level directive for memory content initialization
  - `@global` / `@endglob`: Global constants

## Comments

- Line: `//` to end of line
- Block: `/* ... */` (no nesting)
- Comments may appear between tokens, not inside tokens

## Fundamental terms

- **Signal:** A declared identifier representing a hardware object (PORT, WIRE, REGISTER, LATCH, MEM port, or BUS signal). A signal has a declared width and static type. Signals are syntax-level objects that appear in source declarations.
- **Net:** The resolved electrical connectivity resulting from aliasing and directional connections between signals after elaboration. A net may connect multiple signals but represents a single driven value per bit. After all assignments and aliases are resolved, each net must have zero or one active driver within any single execution path, unless it is a tri-state net where all but one driver assigns `z` for each bit.
- **Deterministic net:** A net that, for every execution path, has exactly one active driver per bit (or is resolved via valid tri-state rules). Any deviation is a compile-time error.
- **Driver:** Any signal or construct that can contribute a value to a net in a given execution path. Examples: REGISTER current-value output, LATCH output, OUT/INOUT port when assigned, WIRE with a directional assignment, constant literal in a directional assignment, semantic drivers (GND, VCC).
- **Active driver:** A driver that contributes a determinate logic value (`0` or `1`) to a net in a given execution path. A driver assigning `z` is not active and does not participate in active-driver counting. A driver whose value contains `x` is active but produces an x-dependent net.
- **Sink:** Any signal position that receives a value from a driver in a given assignment context (e.g., module input, register next-state input, intermediate targets). A signal may act as a driver in one context and a sink in another.
- **Observable sink:** A signal location whose value is architecturally visible or externally observable. Includes: REGISTER next-state and reset values, MEM initialization contents, OUT/INOUT ports, and top-level project pins.
- **Execution path:** A statically distinct control-flow branch through a block, defined by mutually exclusive IF/ELIF/ELSE or SELECT/CASE structures. Execution paths are determined structurally, not by symbolic reasoning. Independent control-flow chains at the same nesting level are treated as potentially concurrent.
- **Assignable identifier:** A signal that may legally appear on the left-hand side of an assignment. Includes REGISTER (in SYNCHRONOUS only), WIRE, OUT port, INOUT port, LATCH (guarded, in ASYNCHRONOUS only), and writable BUS signals.
- **Register:** A storage signal bound to a clock domain (flip-flops) that exposes a read-only current-value output readable in all blocks (acts as a driver) and a write-only next-state input assignable only in its home SYNCHRONOUS block (acts as a sink). Has a mandatory reset value defined at declaration.
- **Latch:** A level-sensitive storage signal updated via guarded assignments in ASYNCHRONOUS blocks. A latch exposes a continuously readable stored value and does not belong to a clock domain.
- **Home domain:** The clock domain in which a REGISTER is defined and may be legally assigned. A register may only be written in its home domain and may not be observed in other domains without an explicit CDC bridge.

## Observability rule (x handling)

Values containing any `x` bits may only be used if all such bits are provably masked before reaching an observable sink. If an `x` bit can reach any observable sink, compilation fails. Masking is structural, not algebraic — the compiler does not assume logical identities (e.g., `x & 0 = 0`) as masking unless the bit is structurally removed. The unused branch of a ternary (`? :`) is considered structurally masked only if the condition is provably constant at compile time; if the condition is runtime-dependent, both branches must independently satisfy the observability rule.

## Exclusive Assignment Rule

For any assignable identifier (REGISTER, WIRE, OUT, INOUT), every execution path through a block must contain zero or one assignment to every bit of that identifier.

Path-exclusive determinism (PED) rules:

1. Independent IF/SELECT chains at the same nesting level may not assign to the same bits.
2. A root-level assignment followed by a nested assignment to the same bits is a compile error.
3. Assignments in branches of a single IF/ELIF/ELSE or SELECT/CASE tree are mutually exclusive and allowed.

Zero-assignment paths:

- Registers hold their current value (clock gating semantics).
- Wires/ports are undriven (possible floating net error if read).

**Note:** If you want a net to be driven in only some, but not all, execution paths, you must assign the net with High-Impedance (`z`) in the undriven paths.

## High-impedance and tri-state logic

High-impedance (`z`) represents the absence of a driven value on a net or port. A driver that assigns `z` is considered electrically disconnected and does not contribute an active logic value.

**Literal form:**
- `z` is permitted only in binary literals. `z` is not permitted in decimal or hexadecimal literals.
- When the MSB is `z`, extension pads with `z`.

**Active vs. high-impedance drivers:**
- An active driver supplies a determinate `0` or `1`; a driver supplying `z` is not active.
- A net may have multiple drivers only if, for every execution path, at most one driver supplies a non-`z` value.

**Tri-state net semantics:**
- Exactly one active driver must be present in each execution path; all others (if any) assign `z`.
- If all drivers assign `z` on an execution path and the net is read, this is a compile-time error (floating net).

**Tri-state ports:**
- OUT ports may assign `z` to release the port.
- INOUT ports may assign `z` to release the port and may be read when released.

**Registers and latches:**
- Registers and latches cannot store or produce `z`.
- Register reset literals must not contain `x` or `z`.
- To implement tri-state behavior, assign `z` to a port, not to a register.

**Examples:**
```text
// Valid:
out_port <= enable ? data : 8'bzzzz_zzzz;
inout_port <= wr_en ? data : 8'bzzzz_zzzz;
IF (!wr_en) { register <= inout_port; }

// Invalid (compile error — reading a floating net):
// All drivers assign z to the net:
// register <= floating_net;   // ERROR
```

## Bit slicing and indexing

**Syntax:** `signal[MSB:LSB]`

- MSB and LSB are nonnegative integers or `CONST` names
- `MSB >= LSB` is required
- Indices are inclusive; width = MSB - LSB + 1
- Indices must be within declared width

**Examples:**
```text
data[7:0]        // lower byte of 'data'
data[15:8]       // upper byte of a 16-bit 'data'
data[XLEN-1:0]   // CONST-based slice
data[0:0]        // single bit (width 1)
```

## Concatenation

**Syntax:** `{a, b, c}`

- MSB-first ordering: `a` is the most-significant portion
- Width = sum of all element widths
- Can be used on LHS for decomposition

## Design hierarchy

- **@project** is the top-level integration point (one per design unit).
- **@module** defines reusable hardware blocks with ports, logic, and state.
- **@new** instantiates child modules inside parent modules.
- **@blackbox** provides opaque interfaces for external IP.
- **@global** provides cross-module sized literal constants.
- **@template** defines compile-time reusable logic blocks that expand inline via `@apply`.
- **@feature** conditionally includes/excludes declarations and logic.
- **@check** validates compile-time invariants.
