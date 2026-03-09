---
title: Module Structure
lang: en-US

layout: doc
outline: deep
---

# Module Structure

## Canonical form

```text
@module <module_name>
  CONST { ... }
  PORT { ... }
  WIRE { ... }
  REGISTER { ... }
  MEM(type=[BLOCK|DISTRIBUTED]) { ... }
  ASYNCHRONOUS { ... }
  SYNCHRONOUS(CLK=<name> ...) { ... }
@endmod
```

Other declaration and logic blocks (`LATCH`, `MUX`, `CDC`, `@template`, `@feature`) may appear where declarations or statements are valid, per their sections below.

## Scope and uniqueness

- Each module must declare at least one port.
- Module and blackbox names are unique project-wide.
- All identifiers within a module are unique (ports, wires, registers, constants, instances).
- `CONST` scope is module-local.
- Instance names are unique in their parent module and cannot collide with other identifiers.

## CONST

- Syntax: `NAME = nonneg_integer;` (numeric) or `NAME = "string";` (string)
- Numeric constants are compile-time only; used in widths and indices.
- String constants hold file paths; used in `@file()` path arguments.
- Using a string CONST in a numeric context or vice versa is a compile error.
- For runtime constants, use `@global` or `lit(width, value)`.

## Port

- `IN [N] name;`, `OUT [N] name;`, `INOUT [N] name;`
- Width is mandatory.
- `INOUT` supports tri-state by assigning `z`.
- All ports must be listed in `@new` instantiations.

### BUS Ports

```text
PORT {
  BUS <bus_id> <ROLE> [<count>] <bus_port_name>;
}
```

- ROLE is `SOURCE` or `TARGET`.
- Directions are resolved from the BUS definition relative to SOURCE.
- Access uses dot notation: `bus_port.signal`, `bus_port[i].signal`, or wildcard `bus_port[*].signal`.
- Wildcard `[*]` fanout:
  - **Broadcast**: 1-bit RHS is replicated to all elements.
  - **Element-wise pairing**: RHS width must equal element count; each bit drives one element.
  - Width mismatch (RHS width != 1 and != count) → compile error.
- All standard assignment operators apply; width rules and Exclusive Assignment Rule apply per signal.

## WIRE

- Syntax: `name [width];`
- Combinational net written in `ASYNCHRONOUS` blocks only.

## REGISTER

- Syntax: `name [width] = literal;`
- Mandatory reset value; reset literal must not contain `x`.
- Read anywhere; written only in `SYNCHRONOUS` blocks.

## LATCH

**Syntax:** `<name> [<width>] <type>;` where `<type>` is `D` or `SR`.

- Written only in `ASYNCHRONOUS` using guarded assignments:
  - D latch: `latch_name <= enable : data;` — enable must be width-1. When `1`, latch is transparent; when `0`, latch holds.
  - SR latch: `latch_name <= set : reset;` — set and reset must be same width as latch.
- Level-sensitive storage; no clock domains; Exclusive Assignment Rule applies.
- Reading a latch is passive and unconditional; readable in ASYNCHRONOUS and SYNCHRONOUS blocks.
- Latches may not be used as a clock, reset, or CDC source.
- Latches may not be aliased (`=`) to another net.
- Latches may not be written in `SYNCHRONOUS` blocks.
- SR latch truth table (per bit): set=1,reset=0 → 1; set=0,reset=1 → 0; set=0,reset=0 → hold; set=1,reset=1 → metastable.

## MUX

Two forms:

- Aggregation: `mux_name = source0, source1, ...;` (all sources same width)
- Auto-slicing: `mux_name [element_width] = wide_source;` (wide source width is exact multiple)

`mux_id[sel]` is read-only.

### Selector width and out-of-range behavior

- Selector width should be >= `clog2(N)` where N is the number of MUX elements.
- If the selector is narrower than `clog2(N)`, it is implicitly zero-extended.
- If the compiler can statically prove that the selector value is out of range (>= N), it is a **compile error**.
- If the selector may be out of range at runtime, the result is **all zeros**.

## MEM

- Declared in `MEM(type=[BLOCK|DISTRIBUTED]) { ... }`.
- Full semantics are defined in the Memory section.

## ASYNCHRONOUS block

- Combinational logic with assignments, conditionals, and SELECT/CASE.
- Alias operators (`=`/`=z`/`=s`) are unconditional only and cannot appear inside conditional control flow.
- Bare literal RHS is forbidden with alias operators.
- Tri-state allowed via `z`.

## SYNCHRONOUS block

```text
SYNCHRONOUS(
  CLK=<name>
  EDGE=[Rising|Falling|Both]
  RESET=<name>
  RESET_ACTIVE=[High|Low]
  RESET_TYPE=[Immediate|Clocked]
) { ... }
```

- At most one `SYNCHRONOUS` block per unique clock.
- `EDGE=Both` generates dual-edge-triggered logic; not a standard FPGA primitive — compiler emits a warning.
- Registers are bound to their first assignment clock domain.
- Reset has highest priority; Immediate reset is combinational (compiler generates an async-assert/sync-deassert reset synchronizer), Clocked reset is edge-triggered.
- Only directional operators (`<=`, `=>` and modifiers) are allowed. Alias (`=`) is forbidden.

## CDC block

```text
CDC {
  <type>[n_stages] <source_reg> (<src_clk>) => <dest_alias> (<dest_clk>);
}
```

- Types: `BIT`, `BUS`, `FIFO`, `HANDSHAKE`, `PULSE`, `MCP`, `RAW`.
- Creates a read-only alias in the destination domain.
- Source register must be a plain register identifier; CDC sets its home domain.

## Module instantiation (`@new`)

```text
@new <instance_name> <module_name> {
  OVERRIDE { <child_const_id> = <expr>; }
  IN/OUT/INOUT/BUS ... = <parent_signal_or_literal>;
}
```

- All child ports must be listed.
- Widths in `@new` are evaluated in the parent scope (CONST/CONFIG).
- `_` may be used as a no-connect placeholder.
- Assigning a parent signal to a child OUT counts as a driver in the parent (Exclusive Assignment Rule).
- BUS bindings must match the child BUS port's bus_id, role, and array count (if any).

### Array instantiation

```text
@new <instance_name>[<count>] <module_name> { ... }
```

- The count is a positive integer or parent `CONST`.
- `IDX` is available only in parent-side expressions in the array mapping. `IDX` is not a first-class value and cannot be assigned, compared, or stored.
- `IDX` is prohibited inside `OVERRIDE`.
- Non-overlap rule: per-instance OUT mappings must not drive overlapping parent bits.
- Only single-dimensional instance arrays are supported.
- Arrayed instance ports can be referenced as `instance_name[index].port_name` in ASYNCHRONOUS/SYNCHRONOUS blocks.

## Templates

- Defined with `@template <name> (<params>) ... @endtemplate`.
- Placement: module-scoped (inside `@module`) or file-scoped (outside `@module`/`@project`).
- Applied with `@apply <name> (<args>);` or `@apply [count] <name> (<args>);` inside `ASYNCHRONOUS` or `SYNCHRONOUS` bodies.
- Templates may contain only: assignments (`<=`, `=>`, `=` and modifiers), conditionals (`IF`/`SELECT`), expressions, and scratch wires (`@scratch`).
- Templates may **not** contain: `WIRE`, `REGISTER`, `PORT`, `CONST`, `MEM`, `MUX`, `@new`, `CDC`, `@feature`, nested `@template`, or block headers.
- All identifiers must be parameters or scratch wires; no external references allowed.
- `@apply [count]` unrolls the template `count` times with `IDX` substituted as a compile-time literal (0 to count-1).
- `IDX` is compile-time only; using it as a runtime value is a compile error.
- After expansion, all normal semantic rules apply (Exclusive Assignment, widths, uniqueness).
- Full details in the [Templates](/reference-manual/formal-reference/templates) reference.

## Feature guards

```text
@feature <config_expr>
  ...
@else
  ...
@endfeat
```

- May appear anywhere a declaration or statement is valid.
- `config_expr` is compile-time, width-1, and may only reference `CONFIG`, `CONST`, literals, and logical operators (`&&`, `||`, `!`, `==`, `!=`, `<`, `>`, `<=`, `>=`).
- `@feature` blocks may **not** be nested.
- The block does not create a new scope; identifiers declared inside `@feature` are visible to the entire module.
- Both enabled and disabled variants must be semantically valid.
