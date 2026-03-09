---
title: Overview
lang: en-US

layout: doc
outline: deep
---

# Overview

A concise introduction to JZ-HDL: what it is, who it’s for, high‑level design goals, essential rules to know immediately, and pointers into the rest of the documentation.

---

## What is JZ-HDL?

JZ-HDL is a hardware description language designed for clear, analyzable, and synthesis‑friendly RTL. It emphasizes:
- Deterministic net semantics (single active driver per net/path)
- Strong, flow‑sensitive checks that prevent ambiguous hardware (floating nets, combinational loops, multi‑driver races)
- Explicit, compile‑time width & bit‑state semantics (x/z handling, literal intrinsic widths)
- Practical on‑chip constructs: registers, parameterized memories (MEM), tri‑state INOUTs, and explicit CDC primitives

Think of JZ-HDL as a small, strict RTL language that trades permissiveness for safer, more predictable synthesis and verification.

---

## Target audience

- RTL designers who want stricter, safer language semantics than raw Verilog/SystemVerilog
- Tool authors and synthesizers needing an unambiguous net model
- FPGA/ASIC engineers who want explicit memory, clock‑domain, and pin mapping constructs
- Educators teaching deterministic digital design patterns

---

## Design goals

- Make illegal hardware obvious at compile time (floating nets, driver conflicts, combinational loops).
- Keep syntax and semantics small and explicit — fewer surprises for synthesis.
- Provide high‑level, synthesis‑aware helpers (intrinsics like uadd/umul, MEM modes, CDC primitives).
- Ensure compile‑time determinism for widths and indices (clog2, widthof, CONST/CONFIG separation).

---

## Must‑know rules (top‑level)

These are the things every author should absorb first.

- Observability Rule
  - Any expression containing `x` bits must be provably masked before reaching an observable sink (register reset/init, MEM init, module OUT/INOUT, top pins). If not provably masked, compilation fails.

- Exclusive Assignment Rule
  - For every assignable signal (REGISTER, WIRE, OUT/INOUT port), every execution path through a block must assign each bit at most once. The compiler enforces this with flow‑sensitive analysis.

- Driver model
  - A net must have exactly zero or one active driver per execution path, unless tri‑stated where all but one driver produce `z` for each bit.
  - Multiple potential drivers are allowed only if they are mutually tri‑stated except one.

- Strict width handling
  - Most binary operators require identical operand widths.
  - Use explicit modifiers (`=z`, `=s`, `<=z`, `<=s`, `=>z`, `=>s`) to extend narrower operands; truncation is never implicit.
  - Unsized literals are forbidden.

- Register locality and clocks
  - All registers driven by a particular clock belong to a single SYNCHRONOUS block per clock (no multiple blocks per same clock).
  - Clock‑domain crossings require explicit CDC entries; registers are bound to a "home domain".

- Memory semantics
  - MEM supports ASYNC and SYNC read ports and synchronous write ports.
  - Write mode (WRITE_FIRST / READ_FIRST / NO_CHANGE) determines read behavior when a read and write target the same address in one cycle.
  - Mem inits must be deterministic (no `x`).

---

## What's new / version 1.0 highlights

- Strict Observability Rule for `x` propagation
- Flow‑sensitive Exclusive Assignment enforcement (PED)
- Explicit CDC primitives (BIT, BUS, FIFO) with home‑domain binding
- A small but expressive set of intrinsics (uadd/sadd/umul/smul/gbit/sbit/gslice/sslice)
- Simplified, deterministic MEM model with ASYNC/SYNC read ports and explicit write modes
- Compile-time templates (`@template`/`@apply`) for reusable logic patterns with scratch wires and unrolling

---

## How to read the documentation

- **Newcomers:** Start with the [Examples](/examples/counter) to see working JZ-HDL code, then read the guide pages under Reference Manual.
- **Verilog/VHDL users:** Read the [Migration Guide](/reference-manual/migration) for a side-by-side comparison of concepts and idioms.
- **Reference lookup:** The [Formal Reference](/reference-manual/formal-reference/core-concepts) section provides concise, normative rules for every language construct.
