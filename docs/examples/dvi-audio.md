---
title: DVI with Audio
lang: en-US

layout: doc
outline: deep
---

# DVI with Audio

A 1280x720 @ 30Hz DVI display with embedded audio. Four BRAM-backed tone generators play classical melodies as square wave PCM, mixed into a stereo stream and injected into DVI data island packets during horizontal blanking. An 80-bar spectrum analyzer with peak hold and color-gradient rendering provides real-time visualization on screen.

The design uses the same clock architecture as the [DVI Color Bars](./dvi) example: 27 MHz crystal → 185.625 MHz serial clock (PLL) → 37.125 MHz pixel clock (CLKDIV).

## Audio Pipeline

```text
Binary melody files (track0-3.bin)
  └─ melodies (BRAM, 3-cycle read latency)
       └─ tone_gen ×4 (Bresenham 48kHz, square wave PCM)
            └─ mixer (4ch signed sum + DVI subpacket + BCH ECC)
                 └─ dvi_data_island (packet injection during hblank)
```

### `melodies`

BRAM wrapper storing note data as pairs of 32-bit words (64 bits per note). Up to 1500 notes across 3 songs of 500 notes each. A phase-alternating read toggles between the two words of each note entry, latching `word_hi` and `word_lo` on alternate cycles. Fields extracted combinationally:

| Field | Bits | Description |
|-------|------|-------------|
| half_period | 16 | Square wave half-period in 48 kHz samples |
| duration | 24 | Note length in samples |
| gap | 16 | Articulation silence at end of note |
| volume | 8 | Amplitude (0-255) |

A sentinel value (`half_period = 0xFFFF`) marks the end of each song for automatic loop-back.

### `tone_gen`

Each instance reads from its own BRAM melody via `OVERRIDE` of a `DATA_FILE` constant. A Bresenham accumulator derives a 48 kHz sample clock from the 37.125 MHz pixel clock (step=16, threshold=12375). The tone generator toggles polarity at the note's half-period boundary to produce a square wave, applies volume scaling (left-shifted 3 bits for 8x gain), and runs the output through a 2-tap moving average filter for anti-aliasing. During the note's gap period, the output is zeroed for articulation.

A button press cycles between three songs by advancing `song_base` through offsets 0, 500, and 1000.

### `mixer`

Purely combinational. Sums four signed 16-bit PCM channels using `sadd` for correct sign-extended widening, then formats the result as a DVI audio subpacket:
- Left-justifies the 16-bit mix to 24 bits.
- Computes even parity via XOR folding.
- Builds status byte 6 with parity for both L and R channels (mono duplication).
- Generates BCH(64,56) ECC using polynomial 0x83 in a combinational XOR network.

### `dvi_data_island`

Injects four packet types into the 370-clock horizontal blanking interval:

```text
Blanking period:
  ├─ Preamble:         8 clocks
  ├─ Leading guard:    2 clocks
  ├─ PKT0 (AVI):     32 clocks
  ├─ PKT1 (ACR):     32 clocks
  ├─ PKT2 (Audio):   32 clocks
  ├─ PKT3 (AIF):     32 clocks
  ├─ Trailing guard:   2 clocks
  └─ Control period
```

Shadow registers pre-load each packet's header and four 64-bit subpackets one cycle before transmission starts, reducing the bit-extraction mux to a single `SELECT` table. Audio samples are buffered between lines (2-3 samples per line at 48 kHz / ~22.5 kHz line rate).

The ACR packet carries N=6144 and CTS=37125, satisfying `48000 = 6144/37125 × 37,125,000`.

### `terc4_encoder`

Purely combinational 4-to-10-bit lookup table implementing the 16 TERC4 codewords from the DVI specification. Three instances encode the three data island channels.

## Spectrum Analyzer

### `spectrum_analyzer`

Maps each tone generator's current half-period to one of 80 log-spaced frequency bins (65 Hz to 5 kHz). For visual fullness, simulated 3rd and 5th harmonics are added with spectral spread:
- **Fundamental**: full volume at center bar, half at ±1, quarter at ±2.
- **3rd harmonic**: 1/4 volume at center, 1/8 at ±1, 1/16 at ±2.
- **5th harmonic**: 1/8 volume at center, 1/16 at ±1.

Amplitudes are smoothed per frame with an exponential moving average and stored in DISTRIBUTED RAM alongside peak-hold values with a 4-bit decay timer.

### `spectrum_display`

Renders 80 color-gradient bars with reflections. Each bar occupies a 16-pixel pitch (10px body + 6px gap). Bars grow upward from y=600 in segments of 16 pixels (12px body + 4px gap), up to 25 segments (400px maximum height). A reflection at 1/4 brightness extends below the baseline for up to 6 segments.

The color gradient runs from red (bar 0) through yellow (bar 39) to blue (bar 79), computed via a linear ramp approximation (`half_pos × 13 >> 1`).

## Per-Instance Data with OVERRIDE

A single `tone_gen` module declares a `DATA_FILE` constant. Each of the four instances overrides it to point at a different binary file:

```jz
@new tg0 tone_gen {
    OVERRIDE {
        DATA_FILE = "out/track0.bin";
    }
    IN  [1]  clk       = clk;
    IN  [1]  rst_n     = reset;
    ...
}
```

The `OVERRIDE` propagates through the module hierarchy — `tone_gen` passes `DATA_FILE` down to its `melodies` instance, which uses it in `@file` for BLOCK RAM initialization. The compiler generates separate BRAM contents for each instance.

## Output Pipeline

The top-level module uses a two-stage output register pipeline for clean timing to the IO serializer. A per-cycle mux selects between four output modes:

1. **Data island + guard**: channel 0 gets TERC4, channels 1-2 get guard pattern `0100110011`.
2. **Data island active**: all three channels get TERC4-encoded packet data.
3. **Video guard band**: channels 0 and 2 get `1011001100`, channel 1 gets `0100110011`.
4. **Default**: all channels get standard TMDS-encoded video/control data.

## Build Process

```bash
cd examples/dvi_audio
python3 tools/generate_melodies.py out/
make
```

The script produces `track0.bin` through `track3.bin` from musical notation, which are embedded into BLOCK RAM via `@file` during compilation.

::: code-group

<<< @/../examples/dvi_audio/src/tang_nano_20k.jz[project.jz]
<<< @/../examples/dvi_audio/src/dvi.jz
<<< @/../examples/dvi_audio/src/video_timing.jz
<<< @/../examples/dvi_audio/src/tmds_encoder_10.jz
<<< @/../examples/dvi_audio/src/tmds_encoder_2.jz
<<< @/../examples/dvi_audio/src/dvi_data_island.jz
<<< @/../examples/dvi_audio/src/terc4_encoder.jz
<<< @/../examples/dvi_audio/src/tone_gen.jz
<<< @/../examples/dvi_audio/src/melodies.jz
<<< @/../examples/dvi_audio/src/mixer.jz
<<< @/../examples/dvi_audio/src/spectrum_analyzer.jz
<<< @/../examples/dvi_audio/src/spectrum_display.jz
<<< @/../examples/dvi_audio/src/debounce.jz
<<< @/../examples/dvi_audio/src/por.jz

:::

## JZ-HDL Language Features

**OVERRIDE for parameterized instances.** A single module definition serves all four tone generators. `OVERRIDE` replaces Verilog's `parameter` passing with a mechanism that propagates through the module hierarchy — the override on `tone_gen` automatically reaches the `melodies` instance inside it, which controls the `@file` BRAM initializer. Verilog would require explicit parameter threading through every intermediate module.

**Intrinsic signed arithmetic.** `sadd` performs sign-extended addition with automatic width promotion. In Verilog, mixing signed and unsigned arithmetic requires careful `$signed()` casts that are easy to get wrong.

**Memory type control.** `MEM(TYPE=BLOCK)` for tone data and `MEM(TYPE=DISTRIBUTED)` for the spectrum analyzer's small lookup tables give the designer explicit control over BRAM vs. LUT RAM inference. Verilog leaves this to synthesis heuristics that vary by vendor.
