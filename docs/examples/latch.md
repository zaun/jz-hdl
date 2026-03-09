---
title: Latch
lang: en-US

layout: doc
outline: deep
---

# Latch

A D-type latch captures a snapshot of a running counter when a button is held. When the button is released, the latch holds its last captured value and the LEDs freeze.

## Modules

### `latch_test`

The module takes a clock, power-on reset, active-low reset button, a latch-enable button (`btn`), and outputs 6 LEDs.

The `ASYNCHRONOUS` block does three things:
1. Combines `por` and `rst_n` into an active-low `reset` wire.
2. Drives the D-latch: `display_buffer <= btn : ~counter[26:21]`. The `enable : data` syntax means `btn` is the gate — when high, the inverted upper counter bits flow through; when low, the latch holds.
3. Outputs `leds <= { btn, display_buffer[4:0] }` — the MSB LED shows the button state, the lower 5 LEDs show the latched value.

The `SYNCHRONOUS` block increments a 27-bit counter on every clock cycle, identical to the counter example.

The key declaration is `LATCH display_buffer [6] D` — an explicitly declared 6-bit D-type latch. The width and type are part of the declaration, not inferred from context.

### Project Files

Two project files target the Tang Nano 20K and 9K. Both map two buttons — one for reset, one for latch enable — and six LEDs. Button polarity is inverted between boards.

::: code-group

<<< @/../examples/latch/src/tang_nano_20k.jz[project.jz]
<<< @/../examples/latch/src/latch.jz

:::

## JZ-HDL Language Features

**Explicit latch declaration.** In Verilog, latches appear silently whenever an `always @(*)` block has an incomplete `if` or `case` — the designer never asked for a latch, but one appears in the netlist. These accidental latches are a top source of synthesis bugs, and tools may only flag them as warnings.

JZ-HDL requires latches to be declared in a `LATCH` block with a name, width, and type. The gated assignment syntax (`enable : data`) makes the enable condition visible at the point of use. If no latch is declared, the compiler will not infer one — incomplete assignments in `ASYNCHRONOUS` blocks are compile errors. Every latch in a JZ-HDL design exists because the designer put it there intentionally.
