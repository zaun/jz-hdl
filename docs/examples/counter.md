---
title: Counter
lang: en-US

layout: doc
outline: deep
---

# Counter

A free-running counter drives 6 LEDs. The counter width is computed at compile time from the clock frequency using `clog2(CLK_MHZ) + 22`, producing a visible binary counting pattern on the LEDs that changes approximately every 4 seconds per bit position.

## Modules

### `counter`

The module takes a clock, two reset inputs (power-on reset and button reset), and outputs 6 LED signals. Four compile-time constants (`CLK_MHZ`, `COUNTER_WIDTH`, `LED_HIGH`, `LED_LOW`) parameterize the design — the project file overrides `CLK_MHZ` via `CONFIG`, and the remaining constants derive automatically.

A 1-bit wire `reset` combines the two reset sources: the FPGA's `DONE` signal (active-high power-on reset) ANDed with the button input `rst_n` (active-low). This produces an active-low reset that holds the design in reset during power-up and when the button is pressed.

The `ASYNCHRONOUS` block drives `reset` and the LED output. The LEDs show the upper 6 bits of the counter (`counter[LED_HIGH:LED_LOW]`). Active-low inversion for the Tang Nano LEDs is handled at the project level (`~LED` in the `@top` binding), keeping the module board-agnostic.

The `SYNCHRONOUS` block clocks the counter register on `clk` with active-low reset. The counter increments by 1 each cycle.

### Project Files

Project files target the Tang Nano 20K (`GW2AR-18`), Tang Nano 9K (`GW1NR-9`), and PA35T-EDU. Each declares a `CONFIG` block with `CLK_MHZ`, a 27 MHz clock (`SCLK`), two buttons (`KEY[2]`), and six LEDs (`LED[6]`). The `@top` instance uses `OVERRIDE` to pass `CONFIG.CLK_MHZ` into the module, wires `SCLK` to `clk`, `DONE` to `por`, the first button to `rst_n`, and `~LED` to `leds` (inverting for active-low LEDs). Pin assignments differ between boards.

::: code-group

<<< @/../examples/counter/src/tang_nano_20k.jz[project.jz]
<<< @/../examples/counter/src/counter.jz

:::

## JZ-HDL Language Features

**Mandatory reset values.** The `REGISTER` declaration requires an initial value (`counter [COUNTER_WIDTH] = COUNTER_WIDTH'b1`). The compiler guarantees every register has a defined reset state. In Verilog, omitting a register from the reset branch produces undefined power-on behavior that synthesizes silently.

**Compile-time constants and OVERRIDE.** The module declares `CONST { CLK_MHZ = 27; COUNTER_WIDTH = clog2(CLK_MHZ) + 22; }` — the counter width is computed at compile time from the clock frequency. The project file overrides `CLK_MHZ` via `OVERRIDE { CLK_MHZ = CONFIG.CLK_MHZ; }`, and all dependent constants recompute automatically. Verilog's `parameter` requires manual propagation through every module in the hierarchy.

**Block separation.** Combinational logic lives in `ASYNCHRONOUS`, clocked logic lives in `SYNCHRONOUS`. The compiler enforces this boundary. Verilog relies on coding conventions and lint tools to distinguish `always @(*)` from `always @(posedge clk)`.

**Integrated pin mapping.** The project file declares clock periods, I/O standards, drive strengths, and physical pin numbers alongside the module instantiation. Traditional HDL workflows split this across `.sdc`, `.cst`, and `.xdc` constraint files that the HDL compiler never sees.
