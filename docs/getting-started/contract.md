---
title: Contract
lang: en-US

layout: doc
outline: deep
---

# Contract

This document defines the *non-negotiable semantic contract* of JZ-HDL.
If a design obeys these rules, it represents deterministic, synthesizable
hardware. All other documents elaborate or provide syntax, but do not weaken
or override this contract.

---

## 1. Execution Model

JZ-HDL describes **cycle-accurate hardware**, not software execution.

- **ASYNCHRONOUS logic** represents purely combinational hardware.
- **SYNCHRONOUS logic** represents edge-triggered storage updated by a clock.
- There is no implicit ordering of statements; hardware exists concurrently.
- All behavior must be explainable as wires, registers, memories, and gates.

---

## 2. Nets, Drivers, and Determinism

### 2.1 Drivers and Sinks

- A **driver** actively assigns a value to a net.
- A **sink** only reads a net.
- The high-impedance state (`z`) is *not* an active driver.

### 2.2 Exclusive Assignment Rule

For every assignable identifier (wire, port, or register bit-range):

- In any single execution path, **zero or one active driver** may exist.
- Multiple drivers are permitted only if:
  - All but one drive `z` for every bit.

Violations are compile-time errors.

---

## 3. Combinational Correctness (ASYNCHRONOUS)

- ASYNCHRONOUS logic must be **fully deterministic**.
- Every net that is read must have a valid driver on every execution path.
- Reading a net driven only by `z` is illegal.
- Combinational loops are forbidden.
  - Cycles that exist only in mutually exclusive paths are permitted.

There is no latch inference.
Undriven paths are errors, not implied storage.

---

## 4. Sequential Correctness (SYNCHRONOUS)

### 4.1 Registers

- Registers are explicit storage elements.
- Every register has:
  - A current-value output
  - A next-state input
  - A mandatory reset value

### 4.2 Clock Domains

- A module may contain **at most one SYNCHRONOUS block per clock**.
- A register belongs to exactly one clock domain (its *Home Domain*).
- A register may only be written in its Home Domain.
- Registers may not be read or written from other clock domains directly.

---

## 5. Reset Semantics

- Reset has absolute priority over all logic.
- Reset values are defined at register declaration.
- Reset behavior is explicit:
  - Polarity and timing (Immediate or Clocked) are defined by the block.

No implicit reset behavior exists.

---

## 6. Clock Domain Crossing (CDC)

- All CDC behavior must be explicit.
- Cross-domain access to registers is illegal without a CDC declaration.
- CDC types define hardware structure:
  - BIT: multi-flop synchronizer
  - BUS: encoded multi-bit crossing
  - FIFO: asynchronous queue

CDC is structural, not stylistic.

---

## 7. Assignment Semantics

### 7.1 No Implicit Width Conversion

- There is **no implicit truncation**.
- There is **no implicit extension**.
- All width changes must be explicit and intentional.

### 7.2 Path Exclusivity

- Assignments in independent conditional chains are not exclusive.
- Assignments in mutually exclusive branches are exclusive.
- Assignments that shadow or overlap in the same path are illegal.

---

## 8. Arithmetic and Data Integrity

- Arithmetic is width-defined and deterministic.
- Overflow is never implicit.
- Full-precision results must be explicitly captured.

Data loss must be intentional and visible.

---

## 9. Memories

- Memories represent true hardware storage arrays.
- Read and write ports are explicit and typed.
- Asynchronous and synchronous reads are distinct and enforced.
- Writes are synchronous and deterministic.

Memory behavior must match realizable silicon structures.

---

## 10. What JZ-HDL Forbids by Construction

JZ-HDL makes the following impossible:

- Accidental multiple drivers
- Latch inference
- Implicit CDC
- Silent width truncation
- Unclocked storage
- Combinational oscillation
- Ambiguous reset behavior

If it compiles, the hardware is structurally sound.

---

## 11. Design Intent

JZ-HDL prioritizes:

- Electrical correctness
- Deterministic behavior
- Auditability
- Long-term maintainability

Convenience is always secondary to correctness.

This contract is stable. All future extensions must preserve these rules.
