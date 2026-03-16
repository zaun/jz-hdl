---
title: Terminal Emulator
lang: en-US

layout: doc
outline: deep
---

# Terminal Emulator

A full ANSI terminal emulator with 1280x720 @ 60Hz DVI output, 142×45 character grid with 9×16 pixel cells, SDRAM-backed framebuffer, and compressed font decompression. The host connects via 115200 baud UART; the FPGA renders text, cursor, and ANSI attributes to an HDMI monitor in real time. An EDID reader queries the connected monitor's identity over DDC/I2C.

## Clock Tree

```text
27 MHz crystal (SCLK)
  └─ PLL (IDIV=3, FBDIV=54, ODIV=2)
       └─ 371.25 MHz serial_clk
            └─ CLKDIV (DIV_MODE=5)
                 └─ 74.25 MHz pixel_clk
```

PLL math: VCO = 27 × (54+1) × 2 / (3+1) = 742.5 MHz. Base output = 742.5/2 = 371.25 MHz. Pixel clock = 371.25/5 = 74.25 MHz, matching the CEA-861 1280×720@60Hz standard.

## Architecture

```text
Startup:
  BSRAM (compressed font) → LZSS decompressor → SDRAM

Runtime:
  UART RX → terminal (ANSI parser) → terminal_buffer → SDRAM
  SDRAM → font_cache (scanline buffer) → pixel_generator → TMDS → DVI

  sdram_arb multiplexes 3 clients:
    1. decompressor (highest priority)
    2. terminal_buffer
    3. font_cache
```

## Modules

### `terminal`

Full ANSI terminal emulator. Processes escape sequences (CSI, OSC), manages cursor position, scroll region, character attributes (bold, underline, blink, inverse, strikethrough), 16-color ANSI palette with foreground/background, insert/delete line/character, and alternate screen buffer. Receives bytes from UART RX and drives the terminal buffer with character and attribute writes.

### `terminal_buffer`

142×45 SDRAM-backed double-buffered line cache. Characters and attributes are written by the terminal module and read by the pixel generator, with SDRAM as the backing store. Supports scroll, alternate screen, write-through caching, and read-back for insert mode. Two independent clock-domain ports: `sys_clk` for CPU writes and `pixel_clk` for video reads.

### `pixel_generator`

4-cycle pipeline pixel generator. Reads character and attribute data from the terminal buffer, fetches font bitmaps from the font cache, and produces RGB pixel output. Renders a 142×45 grid of 9×16 pixel cells. Supports 16-color ANSI palette with blink, underline, strikethrough, and inverse attributes. Cursor rendering supports 4 styles (underline, block, blinking variants).

### `font_cache`

SDRAM-backed double-buffered font cache. 256×8 BSRAM scanline buffers with character capture and prefetch coordination. Fetches glyph bitmaps from SDRAM (where the decompressor placed them at startup) and serves them to the pixel generator with minimal latency.

### `lzss_decomp`

LZSS + ZeroGlyph decompressor. Decompresses font data from 8 BSRAM banks into SDRAM at startup. Uses a state machine with parity checking and supports a double-layer compression scheme: ZeroGlyph outer layer with LZSS inner layer.

### `sdram_arb`

3-port SDRAM arbiter. Multiplexes the decompressor, terminal buffer, and font cache onto a single SDRAM controller with fixed priority: decompressor (highest, runs only at startup) > terminal buffer > font cache.

### `sdram`

Low-level SDRAM controller for the GW2AR-18's embedded 64 Mbit SDRAM (2M × 32). Initialization: 200µs power-up wait → PRECHARGE ALL → two AUTO REFRESH cycles → MODE REGISTER SET (CL=2, burst=1). Auto-refresh fires every ~7.8µs.

### `edid_reader`

EDID DDC/I2C reader. Reads 128-byte EDID blocks from the connected monitor, parses descriptors for monitor name, serial, and text fields, and extracts native resolution. Implements a full I2C state machine with SDA/SCL open-drain control.

### `video_timing`

CEA-861 compliant 1280×720@60Hz timing generator. 74.25 MHz pixel clock, positive sync polarity. H_ACTIVE=1280, H_TOTAL=1650; V_ACTIVE=720, V_TOTAL=750.

### `tmds_encoder`

DVI TMDS 8b/10b encoder with running disparity tracking for DC balance.

### `uart_rx` / `uart_tx`

8N1 UART at 115200 baud (74.25 MHz / 644 ≈ 115264 baud). The receiver includes a 2-stage metastability synchronizer and samples at mid-bit.

### `por`

Power-on reset. Counts clock cycles after DONE goes high before releasing active-low `por_n`.

### `dvi_top`

Top-level integration. Instantiates all submodules and orchestrates the startup sequence (font decompression to SDRAM) followed by runtime operation (UART → terminal → SDRAM → video pipeline → TMDS output).

::: code-group

<<< @/../examples/terminal/src/tang_nano_20k.jz[project.jz]
<<< @/../examples/terminal/src/dvi.jz
<<< @/../examples/terminal/src/terminal.jz
<<< @/../examples/terminal/src/terminal_buffer.jz
<<< @/../examples/terminal/src/pixel_generator.jz
<<< @/../examples/terminal/src/font_cache.jz
<<< @/../examples/terminal/src/lzss_decomp.jz
<<< @/../examples/terminal/src/sdram_arb.jz
<<< @/../examples/terminal/src/sdram.jz
<<< @/../examples/terminal/src/edid_reader.jz
<<< @/../examples/terminal/src/video_timing.jz
<<< @/../examples/terminal/src/tmds_encoder.jz
<<< @/../examples/terminal/src/uart_rx.jz
<<< @/../examples/terminal/src/uart_tx.jz
<<< @/../examples/terminal/src/por.jz

:::

## JZ-HDL Language Features

**SDRAM tristate control.** The 32-bit `sdram_dq` bus uses `INOUT` ports — the SDRAM controller drives during writes and releases to high-Z during reads. The compiler proves at compile time that no two modules drive the bus simultaneously.

**Multi-client bus arbitration.** The SDRAM arbiter multiplexes three independent clients using priority-based selection. Each client communicates through explicit request/acknowledge signals — no implicit bus sharing or synthesis-dependent arbitration.

**Compressed data in BSRAM.** Font data is stored compressed in BSRAM (initialized via `@file`) and decompressed to SDRAM at startup. The compiler embeds binary data directly into block RAM initialization, replacing vendor-specific memory initialization files.

**I2C open-drain modeling.** The EDID reader uses `INOUT` for SDA — driving low for logic 0 and releasing to high-Z for logic 1 (open-drain). The compiler tracks tristate ownership to prevent bus contention between the reader and the monitor's pull-up.
