---
title: Ascon-128 Encryption
lang: en-US
layout: doc
outline: deep
---

# Ascon-128 Encryption

A hardware implementation of the [Ascon-128](https://ascon.iaik.tugraz.at/) authenticated encryption cipher (NIST SP 800-232) on the Tang Nano 20K. The FPGA accepts plaintext over UART, encrypts or decrypts it using a 128-bit key and nonce, and returns the ciphertext with a 128-bit authentication tag. The design processes one permutation round per clock cycle with no large data buffers — blocks stream through as they arrive.

## Architecture

```
                  ┌──────────────────────────────────┐
   UART RX ──────►│  ascon_top                       │
                  │                                  │
                  │  ┌──────────┐    ┌────────────┐  │
                  │  │ uart_rx  │───►│            │  │
                  │  └──────────┘    │  16-state  │  │
                  │                  │  protocol  │  │
                  │  ┌──────────┐    │  FSM       │  │
                  │  │ uart_tx  │◄───│            │  │
                  │  └──────────┘    │            │  │
                  │                  └─────┬──────┘  │
                  │                        │         │
                  │                  ┌─────▼──────┐  │
                  │                  │   ascon    │  │
                  │                  │  320-bit   │  │
                  │                  │  state     │  │
                  │                  │  engine    │  │
                  │                  └────────────┘  │
                  │                                  │
   UART TX ◄──────│  ┌──────────┐                    │
                  │  │   por    │  Power-on reset    │
                  │  └──────────┘                    │
                  └──────────────────────────────────┘
```

## UART Protocol

The host communicates at 115200 baud, 8N1. Data is sent in 8-byte blocks with the host reading each block's response before sending the next, since the FPGA has no RX FIFO.

**Encrypt request:** `'E'` + 16-byte key + 16-byte nonce + 1-byte length + data bytes

**Decrypt request:** `'D'` + 16-byte key + 16-byte nonce + 1-byte length + data bytes, then 16-byte tag (sent after reading plaintext)

**Response:** Streamed output data (per block) + status byte (`'K'` or `'F'`) + 16-byte tag (encrypt only)

## Modules

### ascon

The core Ascon-128 AEAD engine. Maintains a 320-bit state split across five 64-bit registers (`s0`–`s4`) plus a 128-bit key stored as `k0`/`k1`.

**Permutation.** The full Ascon permutation is computed combinationally in the `ASYNCHRONOUS` block as four chained stages: round constant addition + pre-XOR, chi-like S-box substitution, post-XOR with bit inversion, and linear diffusion via circular shifts. Each clock cycle in `ST_PERM` commits one round's result. Initialization and finalization use 12 rounds (a-rounds); intermediate blocks use 6 rounds (b-rounds, starting from `round_cnt = 6`).

**Encrypt vs. decrypt.** For intermediate full blocks, encrypt XORs the plaintext into `s0` while decrypt replaces `s0` entirely with the ciphertext. For partial last blocks, decrypt requires special handling — only the ciphertext bytes are replaced in `s0`, the padding byte is XORed at the correct position, and the remaining state bytes are preserved. This is implemented with a 7-way `SELECT` on `din_partial_len`.

**Tag computation.** After the final permutation, the tag is `{s3 ^ k0, s4 ^ k1}`. For decrypt, the engine also compares the computed tag against the expected `tag_in`.

### ascon_top

The streaming UART interface with a 16-state protocol FSM. Handles key/nonce/data reception, block accumulation into a 64-bit shift register, padding (0x80 followed by zeros for partial blocks, or a separate empty padding block for full blocks), and interleaved transmission of output bytes.

The design uses only two 64-bit shift registers for data — one for block accumulation (`blk_acc`) and one for TX output (`tx_shift`). No large buffers are needed because blocks are processed and transmitted as they stream through.

### uart_rx / uart_tx

Standard 8N1 UART modules at 115200 baud (27 MHz / 234 ≈ 115384 baud). The receiver includes a 2-stage metastability synchronizer and samples at the mid-bit point. The transmitter shifts data out LSB-first with a ready/valid handshake.

### por

Power-on reset module. Waits for the FPGA's `DONE` signal, then counts 16 clock cycles before asserting `por_n`. Uses `clog2()` for the counter width — the compiler computes the minimum bit count at compile time.

## Test Tool

A Python test script (`tools/ascon_test.py`) communicates with the FPGA over serial. It supports interactive encrypt/decrypt of strings or files (chunked into 255-byte segments with per-chunk nonce derivation), and a self-test mode that verifies encrypt-decrypt round-trips and tag rejection for five test vectors.

```bash
# Self-test (5 vectors: empty, 1-block, partial, 2-block, multi+partial)
python3 tools/ascon_test.py /dev/ttyUSB0 selftest

# Encrypt a file
python3 tools/ascon_test.py /dev/ttyUSB0 encrypt myfile.txt > myfile.enc

# Decrypt it back
python3 tools/ascon_test.py /dev/ttyUSB0 decrypt myfile.enc > recovered.txt
```

::: code-group

<<< @/../examples/ascon/src/tang_nano_20k.jz[project.jz]
<<< @/../examples/ascon/src/ascon.jz
<<< @/../examples/ascon/src/top.jz
<<< @/../examples/ascon/src/uart_rx.jz
<<< @/../examples/ascon/src/uart_tx.jz
<<< @/../examples/ascon/src/por.jz

:::

## JZ-HDL Language Features

- **Combinational permutation in `ASYNCHRONOUS`.** All four stages of the Ascon S-box and linear layer are expressed as chained wire assignments. The compiler verifies there are no combinational loops and that every wire is driven exactly once — the kind of bug that would silently produce wrong ciphertext in Verilog.

- **Width-safe concatenation and slicing.** The padding logic, key splitting, and circular shifts use explicit widths throughout (`{ din[63:24], s0[23:16] ^ 8'h80, s0[15:0] }`). The compiler checks that every concatenation and slice produces exactly the declared width, preventing the off-by-one bit errors that plague hand-written cryptographic RTL.

- **`clog2()` compile-time evaluation.** The POR module uses `clog2(POR_CYCLES)` to compute the counter width. The compiler evaluates this at compile time, ensuring the counter is exactly wide enough without manual calculation.

- **Single-driver enforcement.** Each register and wire has exactly one driver. The Ascon engine's 320-bit state is modified in multiple FSM states, but the compiler verifies through control-flow analysis that no state can be driven from two places simultaneously — a guarantee that would require manual review in Verilog.
