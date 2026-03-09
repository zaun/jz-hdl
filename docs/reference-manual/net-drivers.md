---
title: Net Drivers
lang: en-US

layout: doc
outline: deep
---

# Net Drivers

## Overview

Every FPGA net in JZ-HDL is the culmination of declared signals, aliases, and directional connects. This page translates the Reference Manual’s driver model into practical rules, diagnostics, and patterns so you can safely describe buses, tri-state I/O, and combinational logic without tripping the compiler’s safety net.

## Key Concepts

### Signals, Nets, Drivers

| Term | What it represents |
| --- | --- |
| **Signal** | A declared entity (`WIRE`, `REGISTER`, `PORT`) that names a storage or connectivity point. |
| **Net** | The combinational interconnect created after alias resolution. Multiple signals can collapse into the same net. |
| **Driver** | Any construct that actively assigns `0` or `1` to a net (register outputs, instance `OUT` ports, directional assignments, literals when allowed). |

### Active Drivers and Tri-State Behavior

Only determinate `0/1` values count toward the active driver budget; `z` is treated as “released” and ignored so tri-state buses can exist if exactly one driver is active when the net is read. The compiler tracks drivers flow-sensitively, so mutual exclusion and explicit `z` releases are required wherever more than one source may assert the same net.

### Observability and `x`

The compiler forbids `x` bits from propagating into observable sinks (register resets, MEM inits, `OUT`/`INOUT` ports, top-level pins). Use structural masking (e.g., bitwise AND) to guarantee that any `x` is removed before a sink sees it; logical identities or “maybe x” reasoning is not tolerated.

### Flow-Sensitive Execution Paths

Every net and register assignment is checked per execution path. A driver that is inactive on one path but active on another still counts toward the per-path limit, so your control structures must explicitly describe exclusion (e.g., `IF/ELSE`, `SELECT/CASE`, ternaries).

## Building Nets from Connections

### Aliasing nets with `=`, `=z`, `=s`

Alias operators merge signals into a single net. Aliasing is transitive—if `a = b` and `b = c`, then `a`, `b`, and `c` share one net. Width changes require `=z` or `=s` to show intent, and aliases inside `ASYNCHRONOUS` logic may not drive bare literals (use directional assignment for constants). After aliasing finishes, the compiler evaluates driver counts on each net bit.

### Directional connectors (`=>`, `<=`)

Use `driver => sink;` (`=>`) or `driver <= sink;` (`<=`) when you want to create a directed driver→sink relationship without merging nets. Directional connects keep the driver/source net separate while ensuring the sink sees the driver’s value. They are ideal when you need explicit arbitration (e.g., `a => b` for a registered output driving a bus) or when connecting combinational logic outputs without aliasing.

### Constants and default drivers

If a net is read anywhere, at least one driver must provide a known `0` or `1` value on every path. Nets with no active drivers but also no sinks are legal (e.g., dangling constants, debug hooks), but the compiler will flag a floating net if the net is observed. Provide defaults (a `DEFAULT` branch, explicit ternary, or a constant driver) to satisfy every path.

```text
// Alias example: `alias_net` merges these signals
a = b;
c = a;

// Directional driver relationship: no net merge
adder_out => result;
```

## Driver Exclusivity and Flow Rules

### Exclusive Assignment Rule (Flow-sensitive)

Every assignable identifier (`REGISTER`, `WIRE`, `PORT`) must receive zero or one assignment per bit on every execution path through a block. Independent `IF` statements at the same nesting level both assigning the same signal violate this rule—even if they are mutually exclusive at runtime—because the compiler cannot assume holistically exclusive behavior unless it appears in the same control structure (`IF/ELSE/ELSEIF`, `SELECT/CASE`, ternary, etc.). Always restructure logic into exclusive branches or express the choice combinationally with a mux.

### Registers and synchronous updates

Registers expose two semantic roles:
- **Current-value output**: readable in any block (acts as a driver).
- **Next-state input**: written only inside `SYNCHRONOUS` blocks.

The Exclusive Assignment Rule applies to these next-state assignments, and register updates must remain within synchronous blocks (never inside `ASYNCHRONOUS`). Omitting an assignment on some paths simply keeps the previous value, supporting intended latch-like behavior without violating the rule.

### Flow-sensitive combinational loop detection

The compiler builds a dependency graph of `ASYNCHRONOUS` assignments and flags cycles that can execute on the same path. True combinational loops (e.g., `a = b; b = a;`) are errors, but mutually exclusive cycles—where the loop can never be fully active on one run—are permitted (e.g., `IF (sel) { a = b; } ELSE { b = a; }`). Break problematic cycles by registering feedback or restructuring logic to eliminate simultaneous dependencies.

## Tri-State Nets and INOUT Ports

`INOUT` ports and shared buses rely on tri-state arbitration. Each driver must release the net (`z`) whenever another driver could be active, and at least one driver has to drive a valid `0`/`1` when the net is actually sampled. The compiler enforces that no net is read while every driver is `z`, otherwise it reports a floating net.

```text
ASYNCHRONOUS {
  data_bus = master_en ? master_data : 8'bzzzz_zzzz;
  data_bus = slave_en  ? slave_data  : 8'bzzzz_zzzz;
}
// Compiler ensures master and slave cannot both drive at once.
```

Use tri-state only when board-level buses require it; otherwise prefer muxed drivers so the Exclusive Assignment Rule is satisfied trivially.

## Compiler Diagnostics at a Glance

The diagnostics below come from the net/tri-state and assignment rule sections of `jz-hdl/src/rules.c`. Their short names are the compiler’s rule IDs; the emitted message provides the exact reason.

| Diagnostic | What happened | How to fix it |
| --- | --- | --- |
| `NET_FLOATING_WITH_SINK` | A net has sinks (reads) but no active driver (or only `z`) on a given execution path. | Ensure every path drives the net with a determinate `0/1` value before it is read (default branch, constant driver, `DEFAULT`, etc.). |
| `NET_TRI_STATE_ALL_Z_READ` | Every driver puts the net into `z` by the time a read occurs, so the value is undefined. | Guarantee at least one driver remains active when the net is sampled, or enforce mutual exclusion around reads. |
| `ASSIGN_MULTIPLE_SAME_BITS` | The same identifier bits are assigned more than once along a single execution path. | Collapse the assignments into a single control structure (IF/ELSE/ELSEIF, SELECT/CASE, ternary) so each path writes once. |
| `ASSIGN_INDEPENDENT_IF_SELECT` | Independent IF/SELECT chains at the same level assign the same net, which the compiler treats as concurrent drivers. | Merge them into one control tree so exclusivity is explicit or move the conflicting assignment into a different net. |
| `ASSIGN_SHADOWING` | A parent-level assignment is followed by a nested assignment targeting overlapping bits (shadowing). | Restructure control flow to avoid overlapping assignments or refactor the nested logic. |
| `ASYNC_ALIAS_LITERAL_RHS` | An alias (`=`, `=z`, `=s`) inside `ASYNCHRONOUS` uses a bare literal RHS. | Drive the literal with `<=/=>` or introduce a dedicated source net instead. |
| `OBS_X_TO_OBSERVABLE_SINK` | `x` bits can flow into observable sinks (register resets, MEM init, `OUT`/`INOUT`, top pins). | Mask or eliminate the `x` bits structurally; the compiler does not assume algebraic simplifications. |
| `COMB_LOOP_UNCONDITIONAL` | Flow-sensitive analysis found a combinational cycle that is active on the same path. | Break feedback using registers or restructure so the loop cannot be active on any path (mutually exclusive loops report `COMB_LOOP_CONDITIONAL_SAFE`). |

Additional flow-sensitive checks enforce one active driver per net bit (tri-state excepted), no floating nets, and alias-rules such as forbidding literals in `ASYNCHRONOUS` before aliasing.

## Best Practices

- Prefer directional connects (`=>`, `<=`) when you want controlled driver→sink relationships without aliasing.
- Limit `=` to intentional net aliasing; double-check width changes and avoid aliasing literals inside `ASYNCHRONOUS`.
- Collapse multiple assignments to one structure (`IF/ELSE`, `SELECT/CASE`, ternary) to satisfy the Exclusive Assignment Rule.
- Keep combinational paths acyclic by passing feedback through registers.
- Use tri-state drivers only for real shared buses; otherwise implement arbitration with muxes or explicit enable signals.
- Mask any literal `x` bits before they reach observable outputs.
- Document arbitration assumptions so the compiler can prove mutual exclusion.

## Author Checklist

- [ ] Every read net has at least one driver supplying `0`/`1` on all paths.
- [ ] No two drivers can be active on the same path for the same bit (unless tri-state `z` releases prove safety).
- [ ] `=` aliases are intentional; replace with directional connectors unless a merged net is required.
- [ ] ASYNCHRONOUS aliases never target bare literals; use directional constants instead.
- [ ] Combinational dependencies form an acyclic graph on each execution path.
- [ ] No `x` bits reach registers, MEM init, ports, or pins without masking.
- [ ] Tri-state drivers are mutually exclusive or ensure a single active driver during reads.

## Pattern Examples

### Safe muxed assignment

```text
ASYNCHRONOUS {
  out = sel ? data1 : data0;
}
```

Single driver, deterministic on every path.

### Independent IFs (invalid)

```text
ASYNCHRONOUS {
  IF (a) { sig = x; }
  IF (b) { sig = y; }  // ERROR: two paths assign `sig` separately
}
```

### Registered feedback to break loops

```text
REGISTER { state [8] = 8'h00; }
SYNCHRONOUS(CLK=clk) {
  state = next_state;
}
ASYNCHRONOUS {
  next_state = f(input, state);  // `state` is registered—no combinational loop
}
```

### Tri-state bus with explicit releases

```text
ASYNCHRONOUS {
  data_bus = master_en ? master_data : 8'bzzzz_zzzz;
  data_bus = slave_en  ? slave_data  : 8'bzzzz_zzzz;
}
```

Guarantee `master_en` and `slave_en` can never be high simultaneously to avoid `MULTIPLE_ACTIVE_DRIVERS`.
