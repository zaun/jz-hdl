---
title: PLL Blink
lang: en-US

layout: doc
outline: deep
---

# PLL Blink

Five LEDs blink at different rates, each driven by an independent clock domain. The 27 MHz board oscillator feeds the first counter directly and drives a PLL that generates four additional clocks. The visible blink differences confirm that all PLL outputs are active and running at the expected frequencies.

## Clock Tree

```text
27 MHz crystal (SCLK)
  └─ PLL (IDIV=2, FBDIV=24, ODIV=4)
       ├─ OUT BASE   CLK_A = 225 MHz
       ├─ OUT PHASE  CLK_B = 225 MHz (phase-shifted)
       ├─ OUT DIV    CLK_C = 56.25 MHz (CLKOUTD_DIV=4)
       └─ OUT DIV3   CLK_D = 75 MHz
```

PLL math: VCO = 27 × (24+1) × 4 / (2+1) = 900 MHz. Base output = 900/4 = 225 MHz. Divided outputs: 225/4 = 56.25 MHz and 225/3 = 75 MHz.

Each LED blinks at `clock_freq / 2^27` — roughly 0.2 Hz for the 27 MHz counter, up to 1.7 Hz for the 225 MHz counters.

## Modules

### `blinkers`

Five clock inputs (`clk1` through `clk5`), a POR input, a reset button, and 5 LED outputs.

The `ASYNCHRONOUS` block combines `por & rst_n` into an active-low `reset` wire and drives each LED from the inverted MSB of its corresponding counter (`leds[N] <= ~counter_X[26]`).

Five `SYNCHRONOUS` blocks each declare their own clock (`CLK=clk1` through `CLK=clk5`) with `RESET_TYPE=Clocked` for proper synchronous reset deassertion within each domain. Each block increments its 27-bit counter by 1.

### Project Files

Both Tang Nano 20K and 9K project files use identical PLL configuration. The `CLOCK_GEN` block declares the PLL with all four outputs (BASE, PHASE, DIV, DIV3) and configuration parameters. The `@top` instance maps all five clocks and the reset.

::: code-group

<<< @/../examples/pll/src/tang_nano_20k.jz[project.jz]
<<< @/../examples/pll/src/blinkers.jz

:::

## JZ-HDL Language Features

**PLL in the source.** The `CLOCK_GEN` block declares the PLL's input, outputs, and divider parameters directly in the project file. Clock outputs use `OUT` (e.g., `OUT BASE CLK_A`), while non-clock outputs like the lock indicator use `WIRE` (e.g., `WIRE LOCK pll_lock`). The compiler validates VCO range and divider ratios against the target chip's constraints. Traditional FPGA flows configure PLLs through vendor GUI tools that generate opaque wrapper modules disconnected from the HDL.

**Multi-clock enforcement.** Each `SYNCHRONOUS` block names its clock explicitly. The compiler tracks which registers belong to which domain. Reading a register from `clk1` inside a `clk2` synchronous block would be a compile-time error, catching cross-domain violations before they become metastability bugs on hardware. Verilog has no such check — nothing prevents reading a `clk1` register inside an `always @(posedge clk2)` block.
