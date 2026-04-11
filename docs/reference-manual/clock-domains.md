---
title: Clock Domains
lang: en-US

layout: doc
outline: deep
---

# Clock Domains

## Overview

Clock domains are explicit in JZ-HDL. Registers have a single home domain (the clock they are synchronized to). Direct use of a register across different clock domains is forbidden. The `CDC` block provides explicit, designer‑declared crossings that create safe, compiler‑understood synchronized views of register values between domains.

Key goals:
- Make cross‑domain behavior explicit and auditable.
- Prevent accidental multi‑domain register usage.
- Let the compiler enforce domain locality and generate appropriate synchronizer structures.

## Syntax

CDC entries appear inside a module body in a `CDC { ... }` block.

Basic form:
```text
CDC {
  <cdc_type>[n_stages] <source_reg> (<src_clk>) => <dest_alias> (<dest_clk>);
  ...
}
```

- `cdc_type` is one of: `BIT`, `BUS`, `FIFO`, `HANDSHAKE`, `PULSE`, `MCP`, `RAW`
- `[n_stages]` optional positive integer; default = 2 (must NOT be provided for `RAW`)
- `source_reg`: a `REGISTER` identifier defined in the same module (plain name, no slices/concat)
- `(src_clk)`: clock identifier for the source/home domain (must match a `SYNCHRONOUS` block `CLK` or a top-level clock input)
- `=> dest_alias (dest_clk)`: creates a read‑only alias visible in the destination clock domain

Examples:
```text
CDC {
  BIT    status_sync (clk_io) => cpu_status (clk_cpu);
  BUS[3] flags_bus    (clk_a)  => flags_view (clk_b);
  FIFO[4] payload     (clk_prod) => consumer_view (clk_cons);
}
```

## Semantics

- The CDC entry sets the **home clock domain** of the source register to `src_clk`.
- The dest alias is a read‑only signal that represents the synchronized view of the source register after approximately `n_stages` cycles in the destination clock.
- The source register may only be read/written in `SYNCHRONOUS` blocks whose `CLK` equals `src_clk` (its home domain).
- The dest alias may only be read in `SYNCHRONOUS` blocks whose `CLK` equals `dest_clk`. It can also be read combinationally in `ASYNCHRONOUS` blocks, but that combinational use must respect domain semantics (see "Usage Notes").
- The compiler elaborates the `CDC` entry into appropriate synchronization hardware:
  - `BIT[n]`: N-stage single-bit flip-flop synchronizer. The source register **must have width [1]** — using BIT with a multi-bit register is a compile error (`CDC_BIT_WIDTH_NOT_1`).
  - `BUS[n]`: Multi‑bit synchronized path; intended only when the source follows Gray‑code or single‑bit change discipline.
  - `FIFO[n]`: Asynchronous FIFO for arbitrary multi‑bit transfers (handles wide or multi‑bit simultaneous changes safely).
  - `HANDSHAKE[n]`: Req/ack handshake protocol for infrequent multi‑bit transfers.
  - `PULSE[n]`: Toggle-based pulse synchronizer. The source register **must have width [1]** — using PULSE with a multi-bit register is a compile error (`CDC_PULSE_WIDTH_NOT_1`).
  - `MCP[n]`: Multi‑cycle path formulation for stable multi‑bit data.
  - `RAW`: Direct unsynchronized wire connection (no crossing logic).

## CDC Types and Intended Use

- BIT[n_stages]
  - Use for single‑bit control/status signals.
  - Result `dest_alias` width: 1.
  - `n_stages` typically 2 or 3 in practice.
  - Latency: exactly `n_stages` cycles of `dest_clk`.

- BUS[n_stages]
  - Use for multi‑bit values that change in a safe, bounded way (e.g., Gray‑encoded counters, handshaked state encodings where only one bit flips at a time).
  - Not safe for arbitrary parallel changes (multi‑bit updates).
  - If source can change arbitrarily, prefer `FIFO`.
  - Latency: exactly `n_stages` cycles of `dest_clk`.

- FIFO[n_stages]
  - Use for arbitrary multi‑bit transfers (data buses, registers updated with new values every cycle).
  - Produces a staged, safe transfer; semantics approximates a buffer or asynchronous FIFO depending on implementation.
  - Latency: between 1 cycle (existing data) and `n_stages + 2` cycles (fresh write).

- HANDSHAKE[n_stages]
  - Use for infrequent multi‑bit transfers where throughput is not critical.
  - Source latches data and asserts request; destination syncs request, latches data, and asserts ack; source syncs ack and deasserts request.
  - Safe for arbitrary data widths.
  - Latency: variable depending on clock ratio; transfer completes after full req/ack handshake.

- PULSE[n_stages]
  - Use for single‑bit pulse events (width == 1 only).
  - Source toggles a register on each pulse; destination syncs the toggle and XOR‑detects edges to produce output pulses.
  - Latency: `n_stages` cycles for pulse detection; one output pulse per input pulse.

- MCP[n_stages]
  - Multi‑cycle path formulation for stable multi‑bit data transfers.
  - Source holds data stable and asserts an enable; destination syncs the enable and samples data when the enable is seen.
  - Uses the same req/ack protocol as HANDSHAKE for safe handoff.
  - Latency: variable, similar to HANDSHAKE; data held stable during transfer.

- RAW
  - Direct unsynchronized view — no crossing logic is inserted.
  - The destination alias is a direct wire connection (`assign`) to the source register.
  - The `[n_stages]` parameter must NOT be provided (compile error if present).
  - Any register width is allowed.
  - Latency: 0 cycles (direct connection).
  - Use only when the designer knows the signals are safe (e.g., quasi‑static configuration registers, signals with external synchronization, or signals that are stable at the time of sampling).
  - **Warning:** RAW explicitly opts out of CDC safety guarantees. Metastability protection is the designer's responsibility.

## Validation Rules (Compiler Enforced)

- Source register must be a `REGISTER` declared in the containing module.
- Source register identifier must be a plain name (no slices, concatenations, instance-qualified names).
- The `CDC` entry establishes the home domain for the source register; that register:
  - May only be assigned (in `SYNCHRONOUS`) inside a `SYNCHRONOUS` block whose `CLK` equals `src_clk`.
  - May only be read inside `SYNCHRONOUS` blocks whose `CLK` equals `src_clk`.
  - Any attempt to use the source register in a `SYNCHRONOUS` block with a different `CLK` is a DOMAIN_CONFLICT error.
- The dest alias:
  - Is created by the compiler as a read‑only signal name (not a register the designer assigns).
  - May only be read in `SYNCHRONOUS` blocks whose `CLK` equals `dest_clk`.
  - Attempting to assign to the dest alias is a compile error.
- A module may have at most one `SYNCHRONOUS` block per unique clock signal. (See SYNCHRONOUS block rules.)
- Multiple CDC entries may declare crossings between the same pair of clocks or different clocks; each source register's home domain is set by its CORESCDC entry.
- If a register is referenced by a CDC entry, that CDC entry must appear before any `SYNCHRONOUS` block that uses the alias or the source register (tool-specific ordering rule for resolvability).
- For `BUS[n_stages]`, compiler may generate warnings if it detects potential multi‑bit simultaneous changes that violate Gray‑code assumptions (static/flow analysis best‑effort).

## Usage Notes

- The `CDC` block does not replace proper CDC design discipline; it documents intent and enables the toolchain to synthesize correct synchronizers and raise violations.
- The destination alias is an explicit name you should use in the destination domain's synchronous logic, not the source register name.
  - Correct: `IF (cpu_status) { ... }` where `cpu_status` is `dest_alias`.
  - Incorrect: reading `status_reg` inside `CLK=cpu_clk` when `status_reg` is home to `clk_io`.
- `dest_alias` is visible combinationally in `ASYNCHRONOUS` blocks, but be careful: combinational logic that mixes `dest_alias` with signals from the destination clock domain should not be used to create cross‑domain control paths (avoid asynchronous handshakes without explicit synchronizers).
- If you need to sample a multi‑bit value that changes arbitrarily, use `FIFO` to avoid metastability and data corruption.
- A single register may have multiple destination aliases to different clocks (fan‑out synchronization to multiple domains).

## Examples

### Single‑bit synchronizer (BIT, 2 stages default)

A 1‑bit `event_flag` is written in the `clk_a` domain and read via the synchronized alias `event_flag_sync` in the `clk_b` domain. The compiler inserts a 2‑stage flip‑flop chain.

::: code-group

<<< @/reference-manual/cdc-examples/bit/cdc_bit.jz
<<< @/reference-manual/cdc-examples/bit/project.jz
<<< @/reference-manual/cdc-examples/bit/project.v[Generated Verilog]
<<< @/reference-manual/cdc-examples/bit/project.il[Generated RTLIL]

:::

### Multi‑bit Gray‑code bus (BUS, 3 stages)

An 8‑bit `gray_ptr` register crosses from `clk_a` to `clk_b` using a 3‑stage BUS synchronizer. This is safe only when the source follows Gray‑code or single‑bit‑change discipline.

::: code-group

<<< @/reference-manual/cdc-examples/bus/cdc_bus.jz
<<< @/reference-manual/cdc-examples/bus/project.jz
<<< @/reference-manual/cdc-examples/bus/project.v[Generated Verilog]
<<< @/reference-manual/cdc-examples/bus/project.il[Generated RTLIL]

:::

### Wide arbitrary data via FIFO (FIFO, 4 stages)

A 64‑bit `packet_word` register crosses from `clk_a` to `clk_b` using a 4‑stage FIFO synchronizer. Unlike BUS, FIFO handles arbitrary multi‑bit changes safely.

::: code-group

<<< @/reference-manual/cdc-examples/fifo/cdc_fifo.jz
<<< @/reference-manual/cdc-examples/fifo/project.jz
<<< @/reference-manual/cdc-examples/fifo/project.v[Generated Verilog]
<<< @/reference-manual/cdc-examples/fifo/project.il[Generated RTLIL]

:::

### Raw unsynchronized view (RAW, quasi‑static config)

A 16‑bit `config_word` register crosses unsynchronized via RAW. No CDC logic is inserted — the designer guarantees the value is stable when read.

::: code-group

<<< @/reference-manual/cdc-examples/raw/cdc_raw.jz
<<< @/reference-manual/cdc-examples/raw/project.jz
<<< @/reference-manual/cdc-examples/raw/project.v[Generated Verilog]
<<< @/reference-manual/cdc-examples/raw/project.il[Generated RTLIL]

:::

## Common Errors and Diagnostics

- DOMAIN_CONFLICT
  - Cause: Using a `REGISTER` in a synchronous block whose `CLK` differs from the register's home domain (as set by CDC or by where the register is first assigned).
  - Fix: Move the register usage to the correct `SYNCHRONOUS` block or add a CDC entry that creates an alias for cross‑domain use.

- DUPLICATE_CDC_ENTRY
  - Cause: Two CDC entries attempt to set home domain for the same source register inconsistently.
  - Fix: Consolidate CDC entries; each register should have a single definitive home domain.

- INVALID_CDC_TARGET
  - Cause: The source register is not a plain register identifier (slice, concat, or undefined).
  - Fix: Use the plain register name; if you need to cross slices or fields, create separate registers or use FIFO.

- INVALID_CDC_TYPE
  - Cause: Using `BIT` for multi‑bit sources or `BUS` for sources that change arbitrarily.
  - Fix: Use the correct CDC type. Use `BIT` for width==1, `BUS` only when Gray‑code discipline is followed, otherwise `FIFO`.

- UNSAFE_BUS_WARNING (warning)
  - Cause: Static/heuristic analysis detects that a `BUS` source may change multiple bits simultaneously.
  - Fix: Use `FIFO` or redesign the producer to follow Gray‑code or single‑bit change discipline.

- MISSING_CDC_FOR_CROSS_DOMAIN_USE
  - Cause: A register is referenced in a different domain without a CDC entry.
  - Fix: Add a CDC entry or redesign to avoid cross‑domain reads.

- MULTI_CLK_ASSIGN / REGISTER_LOCALITY_VIOLATION
  - Cause: Assigning the same register in more than one `SYNCHRONOUS` block for different clocks.
  - Fix: Ensure a register is written only in its home domain; use CDC to observe it elsewhere.

## Best Practices

- Be explicit and conservative:
  - Prefer `FIFO` for multi‑bit transfers unless you can guarantee single‑bit changes (Gray code) and understand the implications.
  - Use `BIT[2]` or `BIT[3]` for status/control signals; 2 stages is common, 3 for safety in noisy environments.

- Name aliases clearly:
  - Use systematic naming like `reg_sync_destclk` or `src_to_dst_signal` so intent is obvious.

- One synchronous block per clock:
  - Place all logic for a given clock in the same `SYNCHRONOUS(CLK=...)` block to satisfy Synchronous Block Uniqueness.

- Keep CDC block near register declarations:
  - Place `CDC { ... }` entries close to the `REGISTER` declarations for readability and to help tools resolve names.

- Document constraints:
  - If a `BUS` CDC requires Gray‑coding, document the producer's requirement in comments and add static checks or assertions when possible.

- Verify with static checks and timing:
  - Run CDC-specific static checks and, where possible, formal checks to ensure no combinational paths create control hazards across domains.
  - Ensure timing constraints for synchronizers are included in downstream SDC (synthesis/timing) flows.

## Anti‑patterns (What to avoid)

- Reading a source register directly in another clock domain without CDC — causes DOMAIN_CONFLICT and metastability.
- Using `BUS` for wide registers that can change every cycle with arbitrary bit patterns — leads to data corruption.
- Mixing alias names and source register names across domains (ambiguous intent).
- Trying to synchronize slices of a multi‑bit register via separate `BIT` synchronizers without ensuring atomic update semantics on the producer side.

## Checklist for Adding a CDC Crossing

- Confirm the source is a `REGISTER` and has a single, clear producer domain.
- Decide the appropriate CDC type:
  - BIT → single‑bit flag.
  - BUS → multi‑bit Gray‑code or single‑bit change guaranteed.
  - FIFO → arbitrary multi‑bit transfers.
  - RAW → quasi‑static or externally synchronized signals (no CDC logic inserted).
- Choose `n_stages` (default 2). For safety or noisy inputs, increase to 3.
- Add the `CDC` entry near the register declaration.
- Replace any cross‑domain uses with reads of the `dest_alias`.
- Run static checks; address compiler warnings/errors.
- Add documentation/comments explaining invariants (e.g., Gray‑code property).

## Synthesis and Implementation Notes

- The compiler will lower `CDC` entries into synthesizable primitives:
  - BIT → chain of flip‑flops with optional meta‑stable handling.
  - BUS → bank of flip‑flops and optional handshaking or gating depending on implementation.
  - FIFO → dual‑clock FIFO or asynchronous FIFO implementation using pointers and synchronizers.
- Downstream tools should map these to:
  - Vendor synchronizer primitives or hand‑optimized flops.
  - FIFO IP blocks for `FIFO` CDC entries when available.
- Ensure timing constraints (SDC) include created synchronizer registers so STA correctly analyzes setup/hold for destination domain.
