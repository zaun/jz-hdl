---
title: "SOC"
lang: en-US

layout: doc
outline: deep
pageClass: soc-simple-scroll
---

# SOC: RISC-V System-on-Chip

A complete system-on-chip built around a 32-bit RISC-V CPU (RV32IM) running on a Tang Nano 20K FPGA. The CPU connects to eight peripherals through a shared 32-bit bus, with HDMI video output, 8-channel audio synthesis, SD card storage, 64 Mbit SDRAM, and a UART serial console.

```text
                  ┌────────────┐
                  │  RISC-V    │
                  │   CPU      │
                  │ (SOURCE)   │
                  └──────┬─────┘
                         │  SIMPLE_BUS
    ┌────────────────────┴─────────────────────────────────────┐
    │                        arbiter                           │
    └─┬───────┬──────┬──────┬────────┬────────┬──────┬───────┬─┘
      │       │      │      │        │        │      │       │
   ┌─────┐ ┌─────┐ ┌────┐ ┌────┐ ┌──────┐ ┌──────┐ ┌────┐ ┌─────┐
   │ ROM │ │ RAM │ │LED │ │UART│ │SDRAM │ │ Term │ │ SD │ │Audio│
   │0x0_ │ │0x1_ │ │0x2_│ │0x3_│ │ 0x4_ │ │ 0x5_ │ │0x6_│ │0x7_ │
   └─────┘ └─────┘ └────┘ └────┘ └──┬───┘ └──────┘ └──┬─┘ └─────┘
                                    │                 │
                                 SDRAM             SD Card
                                64 Mbit             (SPI)
```

::: info
Audio support is a work in progress and not currently functional.
:::

## Bus Architecture

The `SIMPLE_BUS` definition in the global file describes the shared bus:

```jz
BUS SIMPLE_BUS {
    OUT   [32]    ADDR;
    OUT   [1]     CMD;
    OUT   [1]     VALID;
    INOUT [32]    DATA;
    IN    [1]     DONE;
}
```

The CPU declares a `BUS SIMPLE_BUS SOURCE` port. Each peripheral declares a `BUS SIMPLE_BUS TARGET` port. Directions resolve automatically: `OUT` from the CPU becomes `IN` at each peripheral. The `INOUT` DATA signal participates in tristate resolution — the compiler proves at compile time that exactly one peripheral drives DATA at any moment.

## Modules

### `global`

Shared constants and enumerations used across all modules via `@global`:
- **OP**: 8-bit opcodes for the accumulator CPU variant (NOP through ST_X, 25 instructions).
- **STATE**: CPU state machine states (FETCH through HALT, 12 states).
- **WAVE**: Audio waveform types (SQUARE, TRIANGLE, SAWTOOTH, NOISE).
- **ENV**: ADSR envelope states (IDLE, ATTACK, DECAY, SUSTAIN, RELEASE).
- **CMD**: Bus commands (READ=0, WRITE=1).

### `rv_cpu` — RISC-V RV32IM CPU

A multi-cycle 32-bit RISC-V implementation supporting the base integer (RV32I) and multiply/divide (M) extensions.

**State machine** (4-bit): FETCH → WAIT_FETCH → DECODE → EXECUTE → MEM_WAIT/RMW_WAIT/MULDIV_WAIT → WRITEBACK.

**Instruction support**:
- LUI, AUIPC, JAL, JALR
- Branches: BEQ, BNE, BLT, BGE, BLTU, BGEU
- Loads: LB, LH, LW, LBU, LHU (with sign/zero extension)
- Stores: SW, SH, SB (sub-word stores use read-modify-write via RMW_WAIT)
- ALU: ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND (immediate and register variants)
- Multiply/divide: MUL, MULH, MULHSU, MULHU, DIV, DIVU, REM, REMU
- CSR: CSRRW, CSRRS, CSRRC and immediate variants
- Trap: ECALL, EBREAK, MRET

**Interrupt handling**: IRQ lines are checked at FETCH. Trap entry saves PC to mepc, sets mcause, copies MIE→MPIE, clears MIE, and jumps to mtvec. MRET restores MIE from MPIE.

**Shadow register file**: The register file (`rv_regfile`) maintains a shadow bank of 31 registers for zero-overhead trap context switching. When `shadow_mode` is active, reads and writes target the shadow bank, preserving the interrupted program's registers without software save/restore.

### `rv_alu` — Combinational ALU

Implements all RV32I ALU operations via `funct3` decoding. ADD/SUB selected by `alt` bit. Signed comparison uses `ssub` sign bit. Shifts use `b[4:0]` as shift amount. Entirely combinational — no registers.

### `rv_muldiv` — Multiply/Divide Unit

Single-cycle multiply (64-bit product via hardware multiplier, selecting upper or lower 32 bits). 32-cycle restoring division with sign correction. Handles edge cases: division by zero returns -1/dividend, overflow (-2^31 / -1) returns -2^31.

### `rv_csr` — CSR Register File

M-mode CSR registers: mstatus (MIE/MPIE/MPP), mie, mtvec, mepc, mcause, mtval, mcycle (free-running counter). Custom CSRs at 0xBC0-0xBC5: clock frequency, video mode, baud divider, SDRAM size, IRQ/SD card vectors. IRQ line status readable at 0xFC0.

### `arbiter`

Template-based address decoder routing the CPU bus to 8 targets. Each target has a config entry matched against `ADDR[31:28]` using `((addr ^ value) & care) == 0`. Three `@template` blocks handle matching, DONE collection, and VALID/ADDR/CMD routing with DATA aliasing for tristate pass-through.

Address map: ROM=0x0_, RAM=0x1_, LED=0x2_, UART=0x3_, SDRAM=0x4_, Terminal=0x5_, SD=0x6_, Audio=0x7_.

### `block_ram` — 20 KB RAM

5120 × 32-bit BLOCK memory (10 BSRAM banks). Two-stage read pipeline: assert address → data ready next cycle. Writes complete in one cycle. Maps bus address via `ADDR[14:2]`.

### `rom` — 16 KB Boot ROM

4096 × 32-bit BLOCK memory initialized from `bios.hex` via `@file`. Read-only with the same two-stage pipeline as RAM. Address mapped via `ADDR[13:2]`.

### `sdram` — SDRAM Controller

Low-level command sequencer for the GW2AR-18's embedded 64 Mbit SDRAM (2M × 32, 11-bit row, 8-bit column, 2-bit bank).

Initialization: 200µs power-up wait → PRECHARGE ALL → two AUTO REFRESH cycles → MODE REGISTER SET (CL=2, burst=1).

11-state machine: INIT → IPRE → IREF → IMODE → IDLE → ACT_W → ACT → RD → RD_CL → WR → REF. Auto-precharge (A10=1) is used for both reads and writes. Refresh fires every ~7.8µs.

Tristate control: `sdram_dq` is driven during writes (`r_dq_oe == 1'b1`) and released to high-Z during reads.

### `sdram_bus` — SDRAM Bus Wrapper

Adapts the raw SDRAM controller to the SIMPLE_BUS protocol. A 2-state machine (IDLE/WAIT) latches the bus address and data, asserts rd/wr, and waits for the controller's `done` signal before signaling bus DONE. Address mapping: `pbus.ADDR[22:2]` → 21-bit controller address.

### `led_out`

Single 32-bit write-only register mapped to 6 LEDs via `data[5:0]`. Reads return the current register value.

### `uart` — UART Controller

Wraps `uart_tx` and `uart_rx` sub-modules. Register map at two offsets:
- Offset 0x0: read returns `{30'b0, rx_has_data, tx_ready}`; write sends `DATA[7:0]`.
- Offset 0x4: read returns the received byte and clears `rx_has_data`.

Both TX and RX are 8N1 with configurable baud via a `baud_div` input from the CPU's CSR. IRQ outputs signal TX ready (rising edge) and RX data available.

### `sdcard` — SD Card SPI Controller

Full SPI-mode SD card interface with 512-byte sector buffer, CMD0/8/55/58/ACMD41 initialization sequence, block read/write with CRC, and DMA handshake for direct-to-RAM writes. Register map includes command, status, sector address, data buffer, and IRQ control. CS gap enforcement between commands per SD specification.

### `video` — DVI/HDMI Video Output

Mode-switchable video pipeline supporting 720p@60Hz (80×22 text, 1280×720) and 1080p@30Hz (120×33 text, 1920×1080), both at 74.25 MHz pixel clock. A 5-stage pipeline reads character and attribute data from the terminal framebuffer, fetches font bitmaps from ROM, and produces TMDS-encoded output. Each cell is 16×32 pixels with RGB565 foreground/background colors. Cursor rendering supports 4 styles (underline, block, blinking variants).

### `video_timing`

Dual-mode CEA-861 timing generator. Mode 0: 1280×720@60Hz (1650×750 total). Mode 1: 1920×1080@30Hz (2200×1125 total). Positive sync polarity for both modes.

### `terminal` — Terminal Framebuffer

Dual-BSRAM character/attribute storage with separate sys_clk (CPU) and pixel_clk (video) ports. Register-mapped interface for cell read/write, cursor position/style, and hardware-accelerated CLEAR and SCROLL_UP commands via a 6-state FSM. Supports up to 120×33 cells.

### `audio` — 8-Channel Audio Synthesizer

Eight independent audio channels, each with selectable waveform (square, triangle, sawtooth, noise), 24-bit frequency, 8-bit volume, 8-bit pan, 8-bit duty cycle, and full ADSR envelope. A 128-sample stereo ring buffer (DISTRIBUTED RAM) feeds the output. Register-mapped per-channel configuration at bus offsets grouped by channel index.

### `aud_gen` — Audio Channel Generator

Single voice with waveform synthesis: square wave (phase vs. duty comparison), triangle (phase fold), sawtooth (direct phase), or noise (16-bit Galois LFSR, taps 16/14/13/11). ADSR envelope with 16-bit accumulator and configurable attack/decay/sustain/release rates. Output: `wave × envelope × volume >> 24`.

### `aud_mixer` — 8-Channel Stereo Mixer

Sums 8 channels with per-channel pan (0=left, 128=center, 255=right). Each channel is scaled by `(255-pan)` for left and `pan` for right. Master volume applied via `smul`. Output clamped to ±0x7FFF on overflow.

### `cpu_accumulator` — Alternative Simple CPU

A 32-bit accumulator-based CPU with A/X registers, 16-bit stack pointer, and flags (Z/C/N). Included as a simpler alternative to the RISC-V for testing. Same bus interface.

### `por` — Power-On Reset

16-cycle delay after DONE assertion before releasing reset.

::: code-group
<<< @/../examples/soc/src/tang_nano_20k.jz[project.jz]
<<< @/../examples/soc/src/global.jz
<<< @/../examples/soc/src/soc.jz
<<< @/../examples/soc/src/rv_cpu.jz
<<< @/../examples/soc/src/rv_alu.jz
<<< @/../examples/soc/src/rv_csr.jz
<<< @/../examples/soc/src/rv_muldiv.jz
<<< @/../examples/soc/src/rv_regfile.jz
<<< @/../examples/soc/src/arbiter.jz
<<< @/../examples/soc/src/block_ram.jz
<<< @/../examples/soc/src/rom.jz
<<< @/../examples/soc/src/sdram.jz
<<< @/../examples/soc/src/sdram_bus.jz
<<< @/../examples/soc/src/led_out.jz
<<< @/../examples/soc/src/uart.jz
<<< @/../examples/soc/src/uart_tx.jz
<<< @/../examples/soc/src/uart_rx.jz
<<< @/../examples/soc/src/sdcard.jz
<<< @/../examples/soc/src/video.jz
<<< @/../examples/soc/src/video_timing.jz
<<< @/../examples/soc/src/tmds_encoder.jz
<<< @/../examples/soc/src/audio.jz
<<< @/../examples/soc/src/aud_gen.jz
<<< @/../examples/soc/src/aud_mixer.jz
<<< @/../examples/soc/src/terminal.jz
<<< @/../examples/soc/src/cpu_accumulator.jz
<<< @/../examples/soc/src/por.jz
:::

## Clock Architecture

```text
27 MHz crystal (SCLK)
  └─ PLL (IDIV=3, FBDIV=54, ODIV=2)
       └─ 371.25 MHz serial_clk
            ├─ CLKDIV (DIV_MODE=5)
            │    └─ 74.25 MHz sys_clk / pixel_clk
            └─ (sdram_clk = inverted sys_clk for DDR timing)
```

The sys_clk and pixel_clk are the same 74.25 MHz signal. The SDRAM clock is phase-inverted for proper setup/hold timing at the SDRAM chip.

## JZ-HDL Language Features

**BUS abstraction.** Adding or removing a signal from `SIMPLE_BUS` requires changing only the bus definition — all modules using `BUS SIMPLE_BUS SOURCE` or `TARGET` automatically get the updated port list. In Verilog, this change ripples through every module's port list, every instantiation, and every wire declaration.

**Tristate ownership proof.** The compiler verifies at compile time that exactly one driver is active on the shared DATA bus at any moment. Each peripheral drives DATA only when selected by the arbiter. The arbiter's template-based address decoding provides the proof structure. In Verilog, tristate conflicts are only found during simulation — or on hardware.

**Global constants.** `@global` shares opcodes, state encodings, and bus commands across all modules without parameter threading. Every module that imports the global file sees the same `CMD.READ`, `CMD.WRITE`, and `STATE.FETCH` constants.

**Mandatory reset values.** Every register in the design has a declared initial value. The SDRAM control signals (`r_cs_n`, `r_ras_n`, etc.) reset to inactive (high for active-low signals). Forgetting a register in the reset block — a common Verilog bug that sends garbage commands to SDRAM during power-on — is impossible.

**Template-based code generation.** The arbiter uses three `@template` blocks to generate address matching, DONE collection, and signal routing for all 8 targets. Adding a ninth peripheral means adding one more entry to the config constant and one more `@new` instance — no template changes needed.
