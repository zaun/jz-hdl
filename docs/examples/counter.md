---
title: Counter
lang: en-US

layout: doc
outline: deep
---

# Counter

A 27-bit free-running counter drives 6 LEDs. The counter increments every clock cycle at 27 MHz, producing a visible binary counting pattern on the LEDs that changes approximately every 4 seconds per bit position.

## Modules

### `counter`

The module takes a clock, two reset inputs (power-on reset and button reset), and outputs 6 LED signals.

A 1-bit wire `reset` combines the two reset sources: the FPGA's `DONE` signal (active-high power-on reset) ANDed with the button input `rst_n` (active-low). This produces an active-low reset that holds the design in reset during power-up and when the button is pressed.

The `ASYNCHRONOUS` block drives `reset` and the LED output. The LEDs show the inverted upper 6 bits of the counter (`~counter[26:21]`) because the Tang Nano LEDs are active-low.

The `SYNCHRONOUS` block clocks the 27-bit `counter` register on `clk` with active-low reset. The counter simply increments by 1 each cycle.

### Project Files

Two project files target the Tang Nano 20K (`GW2AR-18`) and Tang Nano 9K (`GW1NR-9`). Both declare a 27 MHz clock (`SCLK`), two buttons (`KEY[2]`), and six LEDs (`LED[6]`). The `@top` instance wires `SCLK` to `clk`, `DONE` to `por`, the first button to `rst_n`, and `LED` to `leds`. Pin assignments and button polarity differ between boards.

::: code-group

<<< @/../examples/counter/src/tang_nano_20k.jz[project.jz]
<<< @/../examples/counter/src/counter.jz

:::

## JZ-HDL Language Features

**Mandatory reset values.** The `REGISTER` declaration requires an initial value (`counter [27] = 27'b1`). The compiler guarantees every register has a defined reset state. In Verilog, omitting a register from the reset branch produces undefined power-on behavior that synthesizes silently.

**Block separation.** Combinational logic lives in `ASYNCHRONOUS`, clocked logic lives in `SYNCHRONOUS`. The compiler enforces this boundary. Verilog relies on coding conventions and lint tools to distinguish `always @(*)` from `always @(posedge clk)`.

**Integrated pin mapping.** The project file declares clock periods, I/O standards, drive strengths, and physical pin numbers alongside the module instantiation. Traditional HDL workflows split this across `.sdc`, `.cst`, and `.xdc` constraint files that the HDL compiler never sees.
