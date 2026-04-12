# JZ-HDL Feature Coverage Matrix

Feature coverage across current examples. `X` = feature is exercised by that example.

Examples: **as**=ascon, **ct**=counter, **cp**=cpu, **dv**=dvi, **da**=dvi_audio, **lt**=latch, **lc**=lcd, **pl**=pll, **sc**=soc, **tm**=terminal, **ua**=uart_audio, **ue**=uart_echo

## Storage Primitives

| Feature              | as | ct | cp | dv | da | lt | lc | pl | sc | tm | ua | ue |
|----------------------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| WIRE                 | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  |
| REGISTER             | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  |
| D-Latch              |    |    |    |    |    | X  |    |    |    |    |    |    |
| SR-Latch             |    |    |    |    |    |    |    |    |    |    |    |    |

## Memory

| Feature              | as | ct | cp | dv | da | lt | lc | pl | sc | tm | ua | ue |
|----------------------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| MEM BLOCK            |    |    | X  |    | X  |    |    |    | X  | X  | X  |    |
| MEM DISTRIBUTED      |    |    |    |    |    |    |    |    |    |    | X  |    |
| OUT ASYNC read       |    |    |    |    |    |    |    |    |    |    | X  |    |
| OUT SYNC read        |    |    | X  |    | X  |    |    |    | X  | X  |    |    |
| IN (write port)      |    |    | X  |    | X  |    |    |    | X  | X  | X  |    |
| INOUT port           |    |    |    |    |    |    |    |    |    |    |    |    |
| WRITE_FIRST          |    |    |    | X  | X  |    |    |    |    |    |    |    |
| READ_FIRST           |    |    |    |    |    |    |    |    |    |    |    |    |
| NO_CHANGE            |    |    |    |    |    |    |    |    |    |    |    |    |
| Literal init         | X  |    |    |    |    |    |    |    |    |    |    |    |
| @file() init         |    |    | X  |    | X  |    |    |    | X  | X  |    |    |

## Clock Domain Crossing (CDC)

| Feature              | as | ct | cp | dv | da | lt | lc | pl | sc | tm | ua | ue |
|----------------------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| CDC BIT              |    |    |    |    |    |    |    |    |    |    |    |    |
| CDC BUS              |    |    |    |    |    |    |    |    |    |    |    |    |
| CDC FIFO             |    |    |    |    |    |    |    |    |    |    |    |    |
| CDC HANDSHAKE        |    |    |    |    |    |    |    |    |    |    |    |    |
| CDC PULSE            |    |    |    |    |    |    |    |    |    |    |    |    |
| CDC MCP              |    |    |    |    |    |    |    |    |    |    |    |    |
| CDC RAW              |    |    |    |    |    |    |    |    |    |    |    |    |

## Clocks & Reset

| Feature              | as | ct | cp | dv | da | lt | lc | pl | sc | tm | ua | ue |
|----------------------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| Single clock domain  | X  | X  | X  |    |    | X  | X  |    |    |    | X  | X  |
| Multi-clock domain   |    |    |    | X  | X  |    |    | X  | X  | X  |    |    |
| PLL CLOCK_GEN        |    |    |    | X  | X  |    |    | X  | X  | X  |    |    |
| DLL CLOCK_GEN        |    |    |    |    |    |    |    |    |    |    |    |    |
| CLKDIV CLOCK_GEN     |    |    |    |    |    |    |    |    |    |    |    |    |
| OSC CLOCK_GEN        |    |    |    |    |    |    |    |    |    |    |    |    |
| BUF CLOCK_GEN        |    |    |    |    |    |    |    |    |    |    |    |    |
| Synchronous reset    | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  |
| RESET_ACTIVE=Low     | X  | X  | X  | X  | X  | X  | X  |    | X  | X  | X  | X  |
| RESET_ACTIVE=High    |    |    |    |    |    |    |    | X  |    |    |    |    |
| RESET_TYPE=Immediate |    |    |    |    |    |    |    | X  |    |    |    |    |
| RESET_TYPE=Clocked   |    |    |    |    |    |    |    | X  |    |    |    |    |
| EDGE=Rising          | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  |
| EDGE=Falling         |    |    |    |    |    |    |    |    |    |    |    |    |
| EDGE=Both            |    |    |    |    |    |    |    |    |    |    |    |    |

## Operators

| Feature              | as | ct | cp | dv | da | lt | lc | pl | sc | tm | ua | ue |
|----------------------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| Add (+)              | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  |
| Subtract (-)         | X  |    | X  | X  | X  |    | X  |    | X  | X  | X  |    |
| Multiply (*)         |    |    |    |    |    |    |    |    |    |    |    |    |
| Divide (/)           |    |    |    |    |    |    |    |    |    |    |    |    |
| Modulus (%)          |    |    |    | X  | X  |    |    |    | X  |    |    |    |
| Bitwise AND (&)      | X  | X  | X  | X  | X  | X  | X  |    | X  | X  | X  | X  |
| Bitwise OR (\|)      | X  |    | X  | X  | X  |    | X  |    | X  | X  | X  |    |
| Bitwise XOR (^)      | X  |    | X  | X  | X  |    |    |    | X  |    | X  |    |
| Bitwise NOT (~)      | X  |    | X  | X  | X  | X  | X  |    | X  | X  | X  |    |
| Logical AND (&&)     | X  |    | X  | X  | X  |    | X  |    | X  | X  | X  | X  |
| Logical OR (\|\|)    | X  |    | X  | X  | X  |    | X  |    | X  | X  | X  |    |
| Logical NOT (!)      |    |    | X  | X  | X  |    | X  |    | X  | X  |    |    |
| Equality (==, !=)    | X  |    | X  | X  | X  |    | X  |    | X  | X  | X  | X  |
| Relational (<, >)    | X  |    | X  | X  | X  |    | X  |    | X  | X  | X  |    |
| Relational (<=, >=)  |    |    | X  | X  | X  |    | X  |    | X  | X  | X  |    |
| Shift left (<<)      |    |    | X  |    |    |    |    |    | X  |    |    |    |
| Shift right (>>)     |    |    | X  | X  | X  |    |    |    | X  | X  |    |    |
| Arith shift (>>>)    |    |    |    |    |    |    |    |    | X  |    |    |    |
| Ternary (?:)         | X  |    | X  | X  | X  |    | X  |    | X  | X  | X  |    |
| Concatenation ({})   | X  |    | X  | X  | X  | X  | X  |    | X  | X  | X  | X  |
| Bit slice [H:L]      | X  | X  | X  | X  | X  | X  | X  |    | X  | X  | X  | X  |

## Intrinsic Functions

| Feature              | as | ct | cp | dv | da | lt | lc | pl | sc | tm | ua | ue |
|----------------------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| uadd/sadd            | X  |    |    |    | X  |    |    |    | X  | X  | X  |    |
| usub/ssub            |    |    |    |    | X  |    |    |    | X  |    | X  |    |
| umul/smul            |    |    |    |    | X  |    |    |    | X  |    |    |    |
| umin/umax            |    |    |    |    |    |    |    |    | X  | X  |    |    |
| smin/smax            |    |    |    |    |    |    |    |    |    |    |    |    |
| abs()                |    |    |    |    |    |    |    |    | X  | X  |    |    |
| gbit / sbit          |    |    |    |    |    |    |    |    | X  |    |    |    |
| gslice / sslice      |    |    |    |    |    |    |    |    | X  |    |    |    |
| oh2b                 |    |    |    |    |    |    |    |    | X  |    |    |    |
| b2oh                 |    |    |    |    |    |    |    |    | X  |    |    |    |
| prienc               |    |    |    |    |    |    |    |    | X  |    |    |    |
| popcount             |    |    |    |    |    |    |    |    | X  |    |    |    |
| lzc                  |    |    |    |    |    |    |    |    |    |    |    |    |
| reverse              |    |    |    |    |    |    |    |    | X  |    |    |    |
| bswap                |    |    |    |    |    |    |    |    |    |    |    |    |
| reduce_and           |    |    |    |    |    |    |    |    |    |    |    |    |
| reduce_or            |    |    |    |    |    |    |    |    |    |    |    |    |
| reduce_xor           |    |    |    |    |    |    |    |    |    |    |    |    |
| clog2                |    | X  | X  | X  | X  |    | X  |    | X  | X  | X  |    |
| widthof              |    |    |    |    |    |    |    |    | X  |    |    |    |
| lit()                | X  |    |    |    |    |    |    |    |    |    |    |    |

## Assignment Operators

| Feature              | as | ct | cp | dv | da | lt | lc | pl | sc | tm | ua | ue |
|----------------------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| Alias (=)            | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  |
| Drive (=>)           |    |    |    |    |    |    |    |    |    |    |    |    |
| Receive (<=)         | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  |
| Zero-ext (=z/<=z)    |    |    |    | X  | X  |    |    |    | X  | X  | X  |    |
| Sign-ext (=s/<=s)    |    |    |    | X  | X  |    |    |    | X  | X  | X  |    |
| Sliced LHS           | X  |    | X  | X  | X  |    | X  |    | X  | X  | X  |    |
| Concat LHS           |    |    |    |    |    |    |    |    |    |    |    |    |

## Control Flow

| Feature              | as | ct | cp | dv | da | lt | lc | pl | sc | tm | ua | ue |
|----------------------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| IF                   | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  |
| ELIF                 | X  |    | X  | X  | X  |    | X  |    | X  | X  | X  |    |
| ELSE                 | X  |    | X  | X  | X  | X  | X  |    | X  | X  | X  | X  |
| SELECT/CASE          | X  |    | X  | X  | X  |    | X  |    | X  | X  | X  |    |
| DEFAULT in SELECT    |    |    |    |    |    |    |    |    |    |    |    |    |
| x wildcard in CASE   |    |    |    |    |    |    |    |    |    |    |    |    |

## Module System

| Feature              | as | ct | cp | dv | da | lt | lc | pl | sc | tm | ua | ue |
|----------------------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| @module              | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  |
| @new instantiation   | X  |    | X  | X  | X  |    |    |    | X  | X  | X  | X  |
| OVERRIDE block       | X  |    | X  | X  | X  |    |    | X  | X  | X  | X  | X  |
| @new[count] array    |    |    |    |    |    |    |    |    |    |    |    |    |
| IDX in array inst    |    |    |    |    |    |    |    |    |    |    |    |    |
| No-connect (_)       |    |    |    |    |    |    |    |    |    |    |    |    |
| @blackbox            |    |    |    |    |    |    |    |    |    |    |    |    |
| @import              |    |    | X  | X  | X  |    |    | X  | X  | X  | X  | X  |

## Project & Configuration

| Feature              | as | ct | cp | dv | da | lt | lc | pl | sc | tm | ua | ue |
|----------------------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| @project             | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  |
| CONFIG (numeric)     | X  | X  | X  | X  | X  | X  | X  |    | X  | X  | X  | X  |
| CONFIG (string)      |    |    | X  |    | X  |    |    |    | X  | X  |    |    |
| CLOCKS block         | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  |
| IN_PINS              | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  |
| OUT_PINS             | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  |
| INOUT_PINS           |    |    |    |    |    |    |    |    |    |    |    |    |
| MAP block            | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  |
| Differential pins    |    |    |    | X  | X  |    |    |    | X  |    |    |    |
| Serialized pins      |    |    |    | X  | X  |    |    |    | X  |    |    |    |
| Pull-up/down         |    |    |    |    |    |    |    |    |    |    |    |    |
| @top                 | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  |

## Bus & Mux

| Feature              | as | ct | cp | dv | da | lt | lc | pl | sc | tm | ua | ue |
|----------------------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| BUS (project-level)  |    |    |    |    |    |    |    |    | X  |    |    |    |
| BUS in PORT          |    |    |    |    |    |    |    |    | X  |    |    |    |
| BUS SOURCE role      |    |    |    |    |    |    |    |    | X  |    |    |    |
| BUS TARGET role      |    |    |    |    |    |    |    |    | X  |    |    |    |
| BUS wildcard fanout  |    |    |    |    |    |    |    |    |    |    |    |    |
| MUX aggregation      |    |    |    |    |    |    |    |    | X  |    |    |    |
| MUX auto-slicing     |    |    |    |    |    |    |    |    |    |    |    |    |

## Tri-state / INOUT

| Feature              | as | ct | cp | dv | da | lt | lc | pl | sc | tm | ua | ue |
|----------------------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| z literal in signal  |    |    |    |    |    |    |    |    | X  |    |    |    |
| INOUT module port    |    |    |    |    |    |    |    |    | X  |    |    |    |
| INOUT BUS signal     |    |    |    |    |    |    |    |    | X  |    |    |    |
| INOUT_PINS           |    |    |    |    |    |    |    |    | X  |    |    |    |
| Tri-state resolution |    |    |    |    |    |    |    |    | X  |    |    |    |

## Global & Compile-time

| Feature              | as | ct | cp | dv | da | lt | lc | pl | sc | tm | ua | ue |
|----------------------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| @global              |    |    | X  |    |    |    |    |    | X  |    |    |    |
| @check               |    |    |    | X  | X  |    |    |    |    |    |    |    |
| @feature             |    |    |    |    |    |    |    |    |    |    |    |    |

## Templates

| Feature              | as | ct | cp | dv | da | lt | lc | pl | sc | tm | ua | ue |
|----------------------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| @template            |    |    |    | X  |    |    |    |    | X  |    |    |    |
| @apply               |    |    |    | X  |    |    |    |    | X  |    |    |    |
| @apply[count]        |    |    |    |    |    |    |    |    |    |    |    |    |
| @scratch wire        |    |    |    |    |    |    |    |    |    |    |    |    |

## Simulation

| Feature              | as | ct | cp | dv | da | lt | lc | pl | sc | tm | ua | ue |
|----------------------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| @simulation          |    |    |    | X  |    |    |    | X  |    |    |    |    |
| TAP                  |    |    |    | X  |    |    |    |    |    |    |    |    |
| MONITOR              |    |    |    | X  |    |    |    |    |    |    |    |    |
| @alert_if            |    |    |    | X  |    |    |    |    |    |    |    |    |
| @mark_if             |    |    |    | X  |    |    |    |    |    |    |    |    |
| @setup / @update     |    |    |    | X  |    |    |    |    |    |    |    |    |
| @trace               |    |    |    | X  |    |    |    |    |    |    |    |    |
| @run                 |    |    |    | X  |    |    |    |    |    |    |    |    |

---

## Coverage Gaps — Features With No Test Coverage

### Critical gaps (core differentiating features)

**All CDC constructs are untested:**
- CDC BIT, CDC BUS, CDC FIFO, CDC HANDSHAKE, CDC PULSE, CDC MCP, CDC RAW
- CDC is one of JZ-HDL's core correctness features — this is the biggest gap.

**SR latches** — D-latch is tested via the `latch` example, but SR latch has no coverage.

**Tri-state / INOUT:**
- Only `soc` exercises tri-state (SDRAM `sdram_dq` bus and peripheral bus disconnection via `32'bz`/`1'bz`)
- No smaller/simpler example demonstrates these features — they're buried in a complex design
- `soc` is not in the synthesis test matrix, so tri-state isn't validated on real hardware per-board

### Significant gaps (commonly needed features)

**Assignment operators:**
- Drive operator (`=>`) — never used
- Concatenation as LHS (`{a, b} = expr`) — never used

**Arithmetic:**
- Multiplication (`*`) — never used as operator
- Division (`/`) — never used (and no division guard proofs tested)
- Division guard rules are untested

**Memory:**
- `READ_FIRST` and `NO_CHANGE` write modes untested
- `INOUT` memory ports untested

**Clock/Reset:**
- `EDGE=Falling` and `EDGE=Both` untested
- `DLL`, `CLKDIV`, `OSC`, `BUF` clock generators untested (only PLL)
- Pull-up/pull-down pin modes untested

**Control flow:**
- `DEFAULT` in SELECT blocks never used
- `x` wildcards in CASE patterns never used

### Intrinsic gaps

**Never used:**
- `smin`, `smax`
- `lzc`, `bswap`
- `reduce_and`, `reduce_or`, `reduce_xor`

**Only in soc (advanced examples):**
- `gbit`, `sbit`, `gslice`, `sslice`
- `oh2b`, `b2oh`, `prienc`
- `popcount`, `reverse`
- `widthof`
- `umin`, `umax`, `abs`

### Module system gaps

- Array instantiation `@new[count]` — never used
- `IDX` in array instantiation — never used
- No-connect `_` — never used
- `@blackbox` declarations — never used

### Project/compile-time gaps

- `@feature` conditional compilation — never used
- BUS wildcard fanout (`bus[*].sig`) — never used
- MUX auto-slicing form — never used
- Differential/pull/termination variations — limited coverage

---

## Suggested New Examples

Prioritized by impact on core JZ-HDL differentiators:

1. **Async FIFO** (`async_fifo`) — exercises `CDC FIFO`, multi-clock, BLOCK RAM, and is board-agnostic. **Single biggest gap filler.**

2. **Pulse/handshake synchronizer** (`cdc_demo`) — small example that exercises `CDC BIT`, `CDC PULSE`, `CDC HANDSHAKE`, `CDC MCP` on a multi-clock design.

3. **SPI controller** (`spi_master`) — standalone test of `INOUT` port and tri-state. Tri-state is exercised in `soc` but only in a large design; a small focused example would validate it per-board. Hardware testable (SPI flash, ADC).

4. **I2C controller** (`i2c_master`) — open-drain INOUT, state machine, clock stretching. Compact tri-state example.

5. **Register file / CPU** — already covered by `soc`/`cpu` but those aren't in the synthesis test matrix. Consider promoting a simplified variant.

6. **Fixed-point FIR filter** (`fir_filter`) — exercises `smul`, `sadd`, `smin`/`smax`, sign-extension operators, and width-safety rules.

7. **Bit-twiddling / CRC** (`crc32`) — exercises `reduce_xor`, `reverse`, `bswap`, `popcount` intrinsics cleanly.

8. **Encoder/decoder** (`encoder_demo`) — exercises `oh2b`, `b2oh`, `prienc`, `lzc`, and MUX auto-slicing in a compact example.

9. **LED PWM array** (`pwm_demo`) — exercises `@new[count]` array instantiation, `IDX`, and template `@apply[count]`.

_Generated on 2026-04-08_
