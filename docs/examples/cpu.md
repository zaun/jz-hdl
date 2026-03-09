---
title: "CPU Examples"
lang: en-US

layout: doc
outline: deep
---

# CPUs

Three implementations of the same 8-bit processor, each with a different microarchitecture. All three execute the same 12-instruction ISA, share the same 256-byte BLOCK ROM (program) and 256-byte BLOCK RAM (data), and produce identical results — they differ only in how many clock cycles each instruction takes.

## Instruction Set

| Opcode | Mnemonic | Operand | Description |
|--------|----------|---------|-------------|
| 0x00 | NOP | — | No operation |
| 0x01 | LDI | imm8 | Load immediate into accumulator |
| 0x02 | LDA | addr8 | Load accumulator from RAM |
| 0x03 | STA | addr8 | Store accumulator to RAM |
| 0x04 | ADD | addr8 | Add RAM value to accumulator |
| 0x05 | SUB | addr8 | Subtract RAM value from accumulator |
| 0x06 | JMP | addr8 | Unconditional jump |
| 0x07 | JZ | addr8 | Jump if zero flag set |
| 0x08 | LDIN | — | Load button state into accumulator |
| 0x09 | STOU | — | Store accumulator to output register |
| 0x0A | WAI | count8 | Wait N milliseconds |
| 0xFF | HLT | — | Halt execution |

The zero flag is updated by LDI, LDIN, LDA, ADD, and SUB.

## Modules

### `cpu` — Multi-Cycle Baseline

A 4-bit state machine walks through up to 7 states per instruction:

1. **FETCH_INS_ADDR** — Drive ROM address with PC.
2. **FETCH_INS_DATA** — Latch instruction from ROM, increment PC, drive ROM address for operand.
3. **FETCH_OP_ADDR** — Wait cycle for ROM latency.
4. **FETCH_OP_DATA** — Latch operand from ROM (if instruction needs one), increment PC.
5. **EXEC** — Execute: ALU operations, jumps, I/O, or issue RAM read.
6. **MEM_READ** — Wait for RAM latency, then complete LDA/ADD/SUB.
7. **WAIT** / **HALT** — Millisecond timer loop or infinite halt.

The WAI instruction uses a Bresenham-style millisecond counter: a 15-bit `ms_counter` counts up to 27,000 (one millisecond at 27 MHz), then decrements `wait_counter`. When `wait_counter` reaches zero, execution resumes.

LED output multiplexes between the output register and the PC based on the button: `IF (btn==0) leds <= ~out_data[5:0] ELSE leds <= ~PC[5:0]`.

### `cpu_efficient` — Overlapped Fetch

Eliminates the `FETCH_OP_ADDR` state by issuing the operand ROM read during `FETCH_INS_DATA` using `PC + 1`. The operand is consumed directly from `rom.read.data` in `EXEC` rather than from a register. This removes the `operand` register, shrinks the state encoding from 4 bits to 3, and saves one or two cycles per instruction.

### `cpu_pipeline` — 3-Stage Pipeline

Uses dual-port ROM (`rom.fetch` and `rom.oper`) to read the instruction and operand simultaneously. Two pipeline registers (`id_ex_instr`, `id_ex_operand`) hold the decoded instruction while the next one is being fetched.

Pipeline states:
- **STARTUP** — Initialize ROM addresses after reset or branch. One-cycle penalty.
- **BUBBLE** — Fill the pipeline: latch first instruction into ID registers.
- **RUNNING** — Full pipeline: EX executes the current instruction while ID latches the next one and IF fetches the one after that. Steady-state throughput is one instruction per cycle.
- **WAIT** / **HALT** — Same as baseline.

Branches (JMP, JZ) flush the pipeline by returning to STARTUP. For LDA/ADD/SUB, the BUBBLE state prefetches the RAM address from the operand so the data is ready when EX runs.

### `por` — Power-On Reset

Counts 16 clock cycles after the FPGA's `DONE` signal goes high before releasing the active-low `por_n` output. Ensures stable operation after configuration.

::: code-group

<<< @/../examples/cpu/src/tang_nano_20k.jz[project.jz]
<<< @/../examples/cpu/src/cpu.jz
<<< @/../examples/cpu/src/cpu_efficient.jz[cpu_efficient.jz]
<<< @/../examples/cpu/src/cpu_pipeline.jz[cpu_pipeline.jz]
<<< @/../examples/cpu/src/por.jz

:::

## JZ-HDL Language Features

**State machine coverage.** JZ-HDL's `SELECT`/`CASE` with `DEFAULT` combined with mandatory register reset values prevents the class of bugs where a missing state in Verilog silently infers a latch or holds stale values.

**BLOCK memory declaration.** `MEM(TYPE=BLOCK)` with explicit read/write ports replaces vendor-specific BRAM inference pragmas. The compiler generates the correct structure; in Verilog, getting BRAM inference right requires following coding patterns that vary by vendor, and mistakes silently fall back to distributed logic.

**Port-checked instantiation.** `@new por0 por { ... }` declares connections with widths and directions. The compiler catches width mismatches and missing ports. Verilog port-connection mismatches may only produce warnings.

**Compile-time constants.** `CONST MILISEC_COUNT = 27000` with `clog2()` and `lit()` replaces Verilog's `parameter`/`localparam` with guaranteed compile-time evaluation and explicit bit sizing at every use site.
