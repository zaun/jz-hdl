---
title: UART Audio
lang: en-US

layout: doc
outline: deep
---

# UART Audio

A 1280x720 @ 30Hz DVI display with UART-streamed audio playback. A host sends 16-bit mono PCM audio over UART; the FPGA buffers it in an 8192-sample BRAM ring buffer, plays back at 48 kHz via DVI/HDMI audio data islands, and renders an 80-bar Goertzel DFT spectrum analyzer with peak hold and VU meter on screen.

The design uses the same clock architecture as the [DVI Color Bars](./dvi) example: 27 MHz crystal → 185.625 MHz serial clock (PLL) → 37.125 MHz pixel clock (CLKDIV).

## Audio Pipeline

```text
Host (PCM over UART @ 115200 baud)
  └─ uart_rx (8N1 deserializer)
       └─ uart_audio_rx (packet protocol: [LEN][DATA×LEN][PARITY])
            └─ audio_buffer (8192 × 16-bit BRAM ring buffer)
                 ├─ mixer (signed sum + DVI subpacket + BCH ECC)
                 │    └─ dvi_data_island (packet injection during hblank)
                 └─ spectrum_analyzer (Goertzel DFT, 80 bins)
                      └─ spectrum_display + level_display (visualization)
```

### `uart_audio_rx`

UART audio packet receiver. Protocol: `[LEN][DATA×LEN][PARITY]`. Accepts packets of up to 64 samples, verifies XOR parity, and sends an ACK byte with the current buffer fill level (0–127) so the host can throttle transmission. A watchdog timer resets the receiver if a packet stalls mid-transfer.

### `audio_buffer`

8192 × 16-bit BRAM ring buffer with Bresenham rate conversion for 48 kHz playback from the 37.125 MHz system clock (step=16, threshold=12375). Reports fill level for flow control back to the host via `uart_audio_rx`. When the buffer underruns, the output holds at zero.

### `mixer`

Purely combinational. Sums signed 16-bit PCM channels using `sadd` for correct sign-extended widening, then formats the result as a DVI audio subpacket:
- Left-justifies the 16-bit sample to 24 bits.
- Computes even parity via XOR folding.
- Builds status byte 6 with parity for both L and R channels (mono duplication).
- Generates BCH(64,56) ECC using polynomial 0x83 in a combinational XOR network.

### `dvi_data_island`

Injects audio sample packets, Audio InfoFrame (AIF), Audio Clock Regeneration (ACR), and AVI InfoFrame packets into the horizontal blanking interval. Shadow registers pre-load each packet's header and subpackets one cycle before transmission for clean timing.

The ACR packet carries N=6144 and CTS=37125, satisfying `48000 = 6144/37125 × 37,125,000`.

### `spectrum_analyzer`

Goertzel DFT spectrum analyzer. Computes 80 frequency bins (k=3 through k=102) from 2048-sample windows at 23.4 Hz resolution, covering approximately 55 Hz to 2400 Hz. Uses Q1.14 fixed-point coefficients. Includes auto-gain normalization and peak-hold display smoothing with exponential decay.

### `spectrum_display`

Renders 80 color-gradient bars with reflections. Each bar occupies a 16-pixel pitch (10px body + 6px gap). Bars grow upward in segments of 16 pixels (12px body + 4px gap), up to 25 segments (400px maximum height). A reflection at 1/4 brightness extends below the baseline.

The color gradient runs from red (bar 0) through yellow (bar 39) to blue (bar 79).

### `level_display`

Audio VU meter and buffer fill indicator. Two horizontal bars: the VU meter (y=300–339) shows current audio amplitude, and the buffer bar (y=380–409) shows ring buffer fill level. Both use a green-to-yellow-to-red color gradient with peak-hold and decay.

### `terc4_encoder`

Purely combinational 4-to-10-bit lookup table implementing the 16 TERC4 codewords from the DVI specification. Three instances encode the three data island channels.

### `tmds_encoder`

DVI TMDS 8b/10b encoder with running disparity tracking.

### `uart_rx` / `uart_tx`

8N1 UART at 115200 baud. The receiver includes a 2-stage metastability synchronizer. The transmitter is used for sending ACK/status bytes back to the host.

### `video_timing`

CEA-861 compliant 1280×720@30Hz timing generator. 37.125 MHz pixel clock.

### `por`

Power-on reset timer. Counts clock cycles after DONE before releasing active-low `por_n`.

::: code-group

<<< @/../examples/uart_audio/src/tang_nano_20k.jz[project.jz]
<<< @/../examples/uart_audio/src/dvi.jz
<<< @/../examples/uart_audio/src/video_timing.jz
<<< @/../examples/uart_audio/src/tmds_encoder.jz
<<< @/../examples/uart_audio/src/dvi_data_island.jz
<<< @/../examples/uart_audio/src/terc4_encoder.jz
<<< @/../examples/uart_audio/src/uart_audio_rx.jz
<<< @/../examples/uart_audio/src/audio_buffer.jz
<<< @/../examples/uart_audio/src/mixer.jz
<<< @/../examples/uart_audio/src/spectrum_analyzer.jz
<<< @/../examples/uart_audio/src/spectrum_display.jz
<<< @/../examples/uart_audio/src/level_display.jz
<<< @/../examples/uart_audio/src/uart_rx.jz
<<< @/../examples/uart_audio/src/uart_tx.jz
<<< @/../examples/uart_audio/src/por.jz

:::

## JZ-HDL Language Features

**BRAM ring buffer with rate conversion.** `MEM(TYPE=BLOCK)` explicitly places the 8192-sample audio buffer in block RAM. The Bresenham accumulator for 48 kHz sample rate derivation from 37.125 MHz uses purely integer arithmetic with compile-time constants — no floating-point IP or vendor clock-enable primitives needed.

**Fixed-point DSP in combinational logic.** The Goertzel DFT coefficients and accumulator use Q1.14 fixed-point arithmetic expressed as standard integer operations with explicit bit widths. The compiler verifies every concatenation and slice produces exactly the declared width, preventing the silent truncation bugs that plague hand-written fixed-point Verilog.

**Flow-control protocol.** The `uart_audio_rx` module implements a packet protocol with parity verification and fill-level ACK, all within a single `SYNCHRONOUS` block's state machine. The compiler's single-driver rule guarantees the UART TX is driven from exactly one place — either the ACK response or the idle state.

**Data island integration.** Audio sample injection into DVI blanking intervals requires precise cycle-by-cycle control of TMDS vs. TERC4 encoding modes. The output mux in the top-level `SYNCHRONOUS` block selects between four encoding modes per cycle, and the compiler verifies that every output path is fully covered with no undriven cycles.
