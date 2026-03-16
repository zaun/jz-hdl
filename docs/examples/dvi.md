---
title: DVI Color Bars
lang: en-US

layout: doc
outline: deep
---

# DVI Color Bars

A 1280x720 @ 30Hz DVI display with selectable test patterns: horizontal color bars, vertical color bars, and an optional animated starfield. A debounced button cycles through the patterns. The design runs on both Tang Nano 20K and 9K boards.

## Clock Tree

```text
27 MHz crystal (SCLK)
  └─ PLL (IDIV=7, FBDIV=54, ODIV=4)
       └─ 185.625 MHz serial_clk
            └─ CLKDIV (DIV_MODE=5)
                 └─ 37.125 MHz pixel_clk
```

Both generators appear in a single `CLOCK_GEN` block. The CLKDIV's input references the PLL's output — the compiler resolves the chain and validates that the VCO (742.5 MHz) is within range and the serial-to-pixel ratio is 5:1 for 10-bit DDR serialization.

## Modules

### `video_timing`

CEA-861 compliant 1280x720 timing generator. Constants define the full timing: H_ACTIVE=1280, H_FRONT=110, H_SYNC=40, H_BACK=220 (H_TOTAL=1650); V_ACTIVE=720, V_FRONT=5, V_SYNC=5, V_BACK=20 (V_TOTAL=750). Two counters (`h_cnt`, `v_cnt`) free-run and wrap. Hsync and vsync are positive polarity. Outputs include pixel coordinates (`x_pos`, `y_pos`) and `display_enable`.

### `tmds_encoder_10` / `tmds_encoder_2`

Two TMDS 8b/10b encoder variants, selected by `@feature CONFIG.serializer`. Both implement the same DVI-specification encoding pipeline:
1. Popcount the 8-bit input via an adder tree.
2. Select XOR or XNOR mode based on whether the popcount exceeds 4.
3. Build a 9-bit transition-minimized word by chaining XOR/XNOR of adjacent bits.
4. Popcount the result, track running disparity with a 5-bit signed counter, and conditionally invert the output for DC balance.

During blanking (`display_enable == 0`), the encoder outputs fixed control tokens based on `c0`/`c1` and resets disparity to zero.

**`tmds_encoder_10`** outputs the full 10-bit encoded word each pixel clock, intended for connection to a 10:1 serializer (e.g., Gowin OSER10).

**`tmds_encoder_2`** adds an internal 2-bit shift register that serializes the 10-bit word at 5× pixel clock, intended for 2:1 DDR primitives (e.g., Lattice ODDRX1F). The TMDS encoding and disparity update are gated to run once every 5 serial clocks (`shift_cnt == 0`).

The project file sets `CONFIG.serializer = 10` or `CONFIG.serializer = 2` to select the appropriate variant via `@feature`/`@else` conditional compilation.

### `hbars` / `vbars`

Purely combinational pattern generators. `hbars` maps `x_pos` to 5 horizontal bars of 256 pixels each (Red, Green, Blue, White, Black). `vbars` maps `y_pos` to 5 vertical bars of 144 pixels each in the same color order.

### `warp`

Animated starfield with 30 stars. Each star is a 2x2 white pixel that accelerates radially outward from screen center (640, 360). Stars that leave the screen respawn near center with positions randomized by a 32-bit Galois LFSR (taps at bits 31, 21, 1, 0). Updates occur every 4th vsync via a frame counter.

Three `@template` blocks handle per-star computation:
- **STAR_DIST** — Absolute distance from center: `dx = |sx - 640|`, `dy = |sy - 360|`.
- **STAR_HIT** — Pixel hit-test against the star's 2x2 bounding box.
- **STAR_NEXT** — Radial movement with velocity = `distance/32 + 1`, or respawn if offscreen.

Each template is expanded 30 times by `@apply [NUM_STARS]` with `IDX` substitution.

The warp module is conditionally compiled via `@feature CONFIG.warp == 1`. The 9K project sets `warp = 0` to fit the smaller device; the 20K project sets `warp = 1`.

### `debounce`

2-stage metastability synchronizer followed by a counter-based debouncer. When the synchronized input disagrees with the stable state, a 20-bit counter increments until it reaches 742,499 (~20ms at 37.125 MHz), then latches the new state. A falling-edge detector produces a single-cycle `btn_press` pulse.

### `por`

Power-on reset timer. After the FPGA's `DONE` signal goes high, a 20-bit counter counts to 1,048,575 (~28ms) before releasing the active-low `por_n` output. This delay ensures the PLL has locked before the design starts running.

### `dvi_top`

Top-level integration. Instantiates all submodules and handles:
- **Pattern selection**: A 2-bit `pattern_sel` register increments on vsync rising edge when `pattern_pending` is set by the debounced button. Color outputs are muxed from hbars, vbars, or warp based on this register.
- **TMDS output pipeline**: A 2-stage register pipeline (pre-mux → output register) ensures a clean FF-to-OSER10 timing path. The TMDS clock channel drives a fixed `10'b1111100000` pattern.
- **Heartbeat**: A 25-bit counter toggles an LED.

## Differential Output

TMDS pins are declared with `mode=DIFFERENTIAL`, `standard=LVDS25`, serialization clock bindings (`fclk=serial_clk`, `pclk=pixel_clk`), and `reset=pll_lock`. The PLL's lock indicator is declared as a `WIRE LOCK pll_lock` output in the `CLOCK_GEN` block — this is a non-clock output that the compiler automatically declares as a wire in the generated Verilog. The `reset` attribute references this lock signal to hold the serializer in reset until the PLL is stable. The compiler automatically inverts the lock signal for the active-low serializer reset. The compiler generates the OSER10 serializer and TLVDS_OBUF differential buffer from these attributes. Each channel shifts out a 10-bit word per pixel clock using DDR at 185.625 MHz.

::: code-group

<<< @/../examples/dvi/src/tang_nano_20k.jz[project.jz]
<<< @/../examples/dvi/src/dvi.jz
<<< @/../examples/dvi/src/video_timing.jz
<<< @/../examples/dvi/src/tmds_encoder_10.jz
<<< @/../examples/dvi/src/tmds_encoder_2.jz
<<< @/../examples/dvi/src/hbars.jz
<<< @/../examples/dvi/src/vbars.jz
<<< @/../examples/dvi/src/warp.jz
<<< @/../examples/dvi/src/debounce.jz
<<< @/../examples/dvi/src/por.jz

:::

## JZ-HDL Language Features

**Clock chain validation.** The `CLOCK_GEN` block declares PLL and CLKDIV together with their input/output relationships. The compiler verifies VCO range, divider ratios, and serializer clock compatibility end-to-end. Traditional flows configure these separately in vendor GUI tools with no cross-validation.

**Templates.** `@template` and `@apply` eliminate repetitive per-instance logic. The 30-star warp module would require 30 copies of identical code in Verilog — or a `generate` block with synthesis-tool-dependent behavior.

**Conditional compilation.** `@feature` / `@endfeat` directives gate entire module instantiations and their associated wiring on project-level `CONFIG` values, enabling single-source multi-target builds.

**Integrated differential I/O.** Pin declarations with `mode=DIFFERENTIAL`, clock bindings (`fclk`, `pclk`), and `reset` replace manual instantiation of vendor serializer primitives (OSER10, TLVDS_OBUF) and their associated constraint files. The `WIRE LOCK` output from `CLOCK_GEN` feeds the serializer reset automatically — the compiler inverts the lock signal so the serializer is held in reset until the PLL stabilizes.
