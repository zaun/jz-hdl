# JZ-HDL Feature Coverage Matrix

Feature coverage across current examples. `X` = feature is exercised by that example.

Examples: **as**=ascon, **ct**=counter, **cp**=cpu, **dm**=domains, **dv**=dvi, **da**=dvi_audio, **lt**=latch, **lc**=lcd, **pl**=pll, **sc**=soc, **tm**=terminal, **ua**=uart_audio, **ue**=uart_echo

## Storage Primitives

| Feature              | as | ct | cp | dm | dv | da | lt | lc | pl | sc | tm | ua | ue |
|----------------------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| WIRE                 | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  |
| REGISTER             | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  |
| D-Latch              |    |    |    |    |    |    | X  |    |    |    |    |    |    |
| SR-Latch             |    |    |    |    |    |    |    |    |    |    |    |    |    |

## Memory

| Feature              | as | ct | cp | dm | dv | da | lt | lc | pl | sc | tm | ua | ue |
|----------------------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| MEM BLOCK            |    |    | X  | X  |    | X  |    |    |    | X  | X  | X  |    |
| MEM DISTRIBUTED      |    |    |    |    |    | X  |    |    |    | X  | X  | X  |    |
| OUT ASYNC read       |    |    |    |    |    | X  |    |    |    | X  | X  | X  |    |
| OUT SYNC read        |    |    | X  | X  |    | X  |    |    |    | X  | X  | X  |    |
| IN (write port)      |    |    | X  | X  |    | X  |    |    |    | X  | X  | X  |    |
| INOUT port           |    |    |    |    |    |    |    |    |    |    |    |    |    |
| WRITE_FIRST          |    |    |    |    |    |    |    |    |    |    |    |    |    |
| READ_FIRST           |    |    |    |    |    |    |    |    |    |    |    |    |    |
| NO_CHANGE            |    |    |    |    |    |    |    |    |    |    |    |    |    |
| Literal init         |    |    | X  | X  |    | X  |    |    |    | X  | X  | X  |    |
| @file() init         |    |    | X  |    |    | X  |    |    |    | X  | X  |    |    |

## Clock Domain Crossing (CDC)

| Feature              | as | ct | cp | dm | dv | da | lt | lc | pl | sc | tm | ua | ue |
|----------------------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| CDC BIT              |    |    |    | X  |    |    |    |    |    |    |    |    |    |
| CDC BUS              |    |    |    | X  |    |    |    |    |    |    |    |    |    |
| CDC FIFO             |    |    |    | X  |    |    |    |    |    |    |    |    |    |
| CDC HANDSHAKE        |    |    |    | X  |    |    |    |    |    |    |    |    |    |
| CDC PULSE            |    |    |    | X  |    |    |    |    |    |    |    |    |    |
| CDC MCP              |    |    |    | X  |    |    |    |    |    |    |    |    |    |
| CDC RAW              |    |    |    | X  |    |    |    |    |    |    |    |    |    |

## Clocks & Reset

| Feature              | as | ct | cp | dm | dv | da | lt | lc | pl | sc | tm | ua | ue |
|----------------------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| Single clock domain  | X  | X  | X  |    |    |    | X  | X  |    |    |    |    | X  |
| Multi-clock domain   |    |    |    | X  | X  | X  |    |    | X  | X  | X  | X  |    |
| PLL CLOCK_GEN        | X  |    |    | X  | X  | X  |    | X  | X  | X  | X  | X  |    |
| DLL CLOCK_GEN        |    |    |    |    |    |    |    |    |    |    |    |    |    |
| CLKDIV CLOCK_GEN     |    |    |    |    | X  | X  |    |    |    | X  | X  | X  |    |
| OSC CLOCK_GEN        |    |    |    |    |    |    |    |    |    |    |    |    |    |
| BUF CLOCK_GEN        |    |    |    |    |    |    |    |    |    |    |    |    | X  |
| Synchronous reset    | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  |
| RESET_ACTIVE=Low     | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  |
| RESET_ACTIVE=High    |    |    |    |    |    |    |    |    |    |    |    |    |    |
| RESET_TYPE=Immediate |    |    |    |    |    |    |    |    |    |    |    |    |    |
| RESET_TYPE=Clocked   |    |    |    |    |    |    |    |    | X  |    |    |    |    |
| EDGE=Rising          | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  |
| EDGE=Falling         |    |    |    |    |    |    |    |    |    |    |    |    |    |
| EDGE=Both            |    |    |    |    |    |    |    |    |    |    |    |    |    |

## Operators

| Feature              | as | ct | cp | dm | dv | da | lt | lc | pl | sc | tm | ua | ue |
|----------------------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| Add (+)              | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  |
| Subtract (-)         | X  |    | X  | X  | X  | X  |    | X  |    | X  | X  | X  | X  |
| Multiply (*)         |    |    |    |    |    |    |    |    |    |    |    |    |    |
| Divide (/)           |    |    |    |    |    |    |    |    |    |    |    |    |    |
| Modulus (%)          |    |    |    |    | X  |    |    |    |    |    |    |    |    |
| Bitwise AND (&)      | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  |
| Bitwise OR (\|)      | X  |    |    |    | X  |    |    | X  |    | X  | X  | X  |    |
| Bitwise XOR (^)      | X  |    |    |    | X  | X  |    |    |    | X  | X  | X  |    |
| Bitwise NOT (~)      | X  |    | X  | X  | X  | X  | X  |    | X  | X  | X  | X  |    |
| Logical AND (&&)     | X  |    |    |    | X  | X  |    | X  |    | X  | X  | X  | X  |
| Logical OR (\|\|)    |    |    |    |    | X  | X  |    | X  |    | X  | X  | X  |    |
| Logical NOT (!)      |    |    |    |    | X  | X  |    | X  |    | X  | X  | X  | X  |
| Equality (==, !=)    | X  |    | X  | X  | X  | X  |    | X  |    | X  | X  | X  | X  |
| Relational (<, >)    | X  |    |    |    | X  | X  |    | X  |    | X  | X  | X  |    |
| Relational (<=, >=)  |    |    | X  | X  | X  | X  |    | X  |    | X  | X  | X  |    |
| Shift left (<<)      | X  |    |    | X  | X  | X  |    | X  |    | X  | X  | X  | X  |
| Shift right (>>)     | X  |    |    |    | X  | X  |    | X  |    | X  | X  | X  | X  |
| Arith shift (>>>)    |    |    |    |    |    |    |    |    |    | X  |    |    |    |
| Ternary (?:)         | X  |    |    |    | X  | X  | X  |    |    | X  | X  | X  |    |
| Concatenation ({})   | X  | X  | X  | X  | X  | X  | X  | X  |    | X  | X  | X  | X  |
| Bit slice [H:L]      | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  |

## Intrinsic Functions

| Feature              | as | ct | cp | dm | dv | da | lt | lc | pl | sc | tm | ua | ue |
|----------------------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| uadd/sadd            |    |    |    |    |    | X  |    |    |    |    |    | X  |    |
| usub/ssub            |    |    |    |    |    |    |    |    |    | X  |    |    |    |
| umul/smul            |    |    |    |    |    | X  |    |    |    | X  |    | X  |    |
| umin/umax            |    |    |    |    |    |    |    |    |    |    |    |    |    |
| smin/smax            |    |    |    |    |    |    |    |    |    |    |    |    |    |
| abs()                |    |    |    |    |    |    |    |    |    | X  |    |    |    |
| gbit / sbit          |    |    |    |    |    |    |    |    |    |    |    |    |    |
| gslice / sslice      |    |    |    |    |    |    |    |    |    | X  |    |    |    |
| oh2b                 |    |    |    |    |    |    |    |    |    | X  |    |    |    |
| b2oh                 |    |    |    |    |    |    |    |    |    |    |    |    |    |
| prienc               |    |    |    |    |    |    |    |    |    |    |    |    |    |
| popcount             |    |    |    |    |    |    |    |    |    | X  |    |    |    |
| lzc                  |    |    |    |    |    |    |    |    |    |    |    |    |    |
| reverse              |    |    |    |    |    |    |    |    |    |    |    |    |    |
| bswap                |    |    |    |    |    |    |    |    |    |    |    |    |    |
| reduce_and           |    |    |    |    |    |    |    |    |    |    |    |    |    |
| reduce_or            |    |    |    |    |    |    |    |    |    |    |    |    |    |
| reduce_xor           |    |    |    |    |    |    |    |    |    |    |    |    |    |
| clog2                | X  | X  | X  | X  | X  | X  |    | X  |    | X  | X  | X  | X  |
| widthof              |    |    |    |    |    |    |    |    |    | X  |    |    |    |
| lit()                | X  |    | X  | X  | X  | X  |    | X  |    | X  | X  | X  | X  |

## Assignment Operators

| Feature              | as | ct | cp | dm | dv | da | lt | lc | pl | sc | tm | ua | ue |
|----------------------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| Alias (=)            | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  |
| Drive (=>)           |    |    |    | X  |    |    |    |    |    |    |    |    |    |
| Receive (<=)         | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  |
| Zero-ext (=z/<=z)    |    |    |    |    |    |    |    |    |    |    |    |    |    |
| Sign-ext (=s/<=s)    |    |    |    |    |    |    |    |    |    |    |    |    |    |
| Sliced LHS           | X  |    | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  |    |
| Concat LHS           |    |    |    |    |    |    |    |    |    | X  |    |    |    |

## Control Flow

| Feature              | as | ct | cp | dm | dv | da | lt | lc | pl | sc | tm | ua | ue |
|----------------------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| IF                   | X  |    | X  | X  | X  | X  |    | X  |    | X  | X  | X  | X  |
| ELIF                 | X  |    | X  |    | X  | X  |    | X  |    | X  | X  | X  | X  |
| ELSE                 | X  |    | X  | X  | X  | X  |    | X  |    | X  | X  | X  | X  |
| SELECT/CASE          | X  |    | X  | X  |    | X  |    | X  |    | X  | X  | X  | X  |
| DEFAULT in SELECT    | X  |    | X  | X  |    | X  |    | X  |    | X  | X  | X  | X  |
| x wildcard in CASE   |    |    |    |    |    |    |    |    |    |    |    |    |    |

## Module System

| Feature              | as | ct | cp | dm | dv | da | lt | lc | pl | sc | tm | ua | ue |
|----------------------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| @module              | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  |
| @new instantiation   | X  | X  | X  | X  | X  | X  | X  |    | X  | X  | X  | X  | X  |
| OVERRIDE block       | X  | X  |    | X  | X  | X  |    |    |    | X  | X  | X  | X  |
| @new[count] array    |    |    |    |    |    |    |    |    |    |    |    |    |    |
| IDX in array inst    |    |    |    |    |    |    |    |    |    |    |    |    |    |
| No-connect (_)       |    | X  |    | X  | X  |    |    | X  |    |    |    |    |    |
| @blackbox            |    |    |    |    |    |    |    |    |    |    |    |    |    |
| @import              | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  |

## Project & Configuration

| Feature              | as | ct | cp | dm | dv | da | lt | lc | pl | sc | tm | ua | ue |
|----------------------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| @project             | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  |
| CONFIG (numeric)     | X  | X  | X  | X  | X  | X  |    | X  |    | X  | X  | X  | X  |
| CONFIG (string)      |    |    |    |    |    | X  |    |    |    |    |    |    |    |
| CLOCKS block         | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  |
| IN_PINS              | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  |
| OUT_PINS             | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  |
| INOUT_PINS           |    |    |    |    |    |    |    |    |    | X  | X  |    |    |
| MAP block            | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  |
| Differential pins    | X  | X  |    | X  | X  | X  | X  |    | X  | X  | X  | X  | X  |
| Serialized pins      |    |    |    |    | X  | X  |    |    |    | X  | X  | X  |    |
| Pull-up/down         |    |    |    |    |    |    |    |    |    |    |    |    |    |
| @top                 | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  | X  |

## Bus & Mux

| Feature              | as | ct | cp | dm | dv | da | lt | lc | pl | sc | tm | ua | ue |
|----------------------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| BUS (project-level)  |    |    |    |    |    |    |    |    |    | X  |    |    |    |
| BUS in PORT          |    |    |    |    |    |    |    |    |    | X  |    |    |    |
| BUS SOURCE role      |    |    |    |    |    |    |    |    |    | X  |    |    |    |
| BUS TARGET role      |    |    |    |    |    |    |    |    |    | X  |    |    |    |
| BUS wildcard fanout  |    |    |    |    |    |    |    |    |    | X  |    |    |    |
| MUX aggregation      |    |    |    |    |    |    |    |    |    | X  |    |    |    |
| MUX auto-slicing     |    |    |    |    |    |    |    |    |    |    |    |    |    |

## Tri-state / INOUT

| Feature              | as | ct | cp | dm | dv | da | lt | lc | pl | sc | tm | ua | ue |
|----------------------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| z literal in signal  |    |    |    |    |    |    |    |    |    | X  | X  |    |    |
| INOUT module port    |    |    |    |    |    |    |    |    |    | X  | X  |    |    |
| INOUT BUS signal     |    |    |    |    |    |    |    |    |    | X  |    |    |    |
| INOUT_PINS           |    |    |    |    |    |    |    |    |    | X  | X  |    |    |
| Tri-state resolution |    |    |    |    |    |    |    |    |    | X  | X  |    |    |

## Global & Compile-time

| Feature              | as | ct | cp | dm | dv | da | lt | lc | pl | sc | tm | ua | ue |
|----------------------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| @global              |    |    | X  |    |    |    |    |    |    | X  |    |    |    |
| @check               |    |    |    |    | X  | X  |    |    |    |    |    |    |    |
| @feature             |    |    |    |    | X  | X  |    |    |    |    |    |    |    |

## Templates

| Feature              | as | ct | cp | dm | dv | da | lt | lc | pl | sc | tm | ua | ue |
|----------------------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| @template            |    |    |    |    | X  |    |    |    |    | X  |    |    |    |
| @apply               |    |    |    |    | X  |    |    |    |    | X  |    |    |    |
| @apply[count]        |    |    |    |    | X  |    |    |    |    | X  |    |    |    |
| @scratch wire        |    |    |    |    |    |    |    |    |    | X  |    |    |    |

## Simulation

| Feature              | as | ct | cp | dm | dv | da | lt | lc | pl | sc | tm | ua | ue |
|----------------------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| @simulation          |    | X  |    | X  | X  |    | X  |    | X  |    |    |    |    |
| TAP                  |    | X  |    | X  | X  |    | X  |    | X  |    |    |    |    |
| MONITOR              |    |    |    |    | X  |    |    |    |    |    |    |    |    |
| @alert_if            |    |    |    |    | X  |    |    |    |    |    |    |    |    |
| @mark_if             |    |    |    |    | X  |    |    |    |    |    |    |    |    |
| @setup / @update     |    | X  |    | X  | X  |    | X  |    | X  |    |    |    |    |
| @trace               |    |    |    |    | X  |    |    |    |    |    |    |    |    |
| @run                 |    | X  |    | X  | X  |    | X  |    | X  |    |    |    |    |

---

## Coverage Gaps — Features With No Test Coverage

### Critical gaps (core differentiating features)

**All 7 CDC types are now covered** by the `domains` example (BIT, BUS, FIFO, HANDSHAKE, PULSE, MCP, RAW). No CDC coverage gap remains.

**SR latches** — D-latch is tested via the `latch` example, but SR latch has no coverage.

**Tri-state / INOUT:**
- Only `soc` and `terminal` exercise tri-state (SDRAM buses, EDID I²C)
- Both are large designs — no small focused tri-state example exists
- `soc` is not in the synthesis test matrix, so tri-state isn't validated per-board

### Significant gaps (commonly needed features)

**Assignment operators:**
- Zero-extend (`=z`/`<=z`) — never used in any example
- Sign-extend (`=s`/`<=s`) — never used in any example
- Concatenation as LHS (`{a, b} <= expr`) — only in `soc` (rv_muldiv)

**Arithmetic:**
- Multiplication (`*`) — never used as hardware operator (only compile-time CONSTs)
- Division (`/`) — never used as hardware operator
- Division guard rules are untested

**Memory:**
- `WRITE_FIRST`, `READ_FIRST`, and `NO_CHANGE` write modes are all untested
- `INOUT` memory ports untested

**Clock/Reset:**
- `EDGE=Falling` and `EDGE=Both` untested
- `RESET_ACTIVE=High` untested (all examples use Low)
- `RESET_TYPE=Immediate` untested
- `DLL` and `OSC` clock generators untested (PLL, CLKDIV, and BUF are covered)
- Pull-up/pull-down pin modes untested

**Control flow:**
- `x` wildcards in CASE patterns never used

### Intrinsic gaps

**Never used:**
- `smin`, `smax`, `umin`, `umax`
- `gbit`, `sbit`
- `lzc`, `bswap`
- `b2oh`, `prienc`, `reverse`
- `reduce_and`, `reduce_or`, `reduce_xor`

**Only in soc (single advanced example):**
- `gslice`, `sslice`
- `oh2b`
- `popcount`, `abs`
- `widthof`
- `usub`, `ssub`
- Arith shift (`>>>`)

### Module system gaps

- Array instantiation `@new[count]` — never used
- `IDX` in array instantiation — never used (IDX is used in `@template`/`@apply[count]` in dvi and soc)
- `@blackbox` declarations — never used

### Project/compile-time gaps

- MUX auto-slicing form — never used
- Pull-up/pull-down/termination variations — no coverage

---

## Suggested New Examples

Prioritized by impact on core JZ-HDL differentiators:

1. **SPI controller** (`spi_master`) — standalone test of `INOUT` port and tri-state in a small design. Hardware testable (SPI flash, ADC).

2. **I2C controller** (`i2c_master`) — open-drain INOUT, state machine, clock stretching. Compact tri-state example.

3. **Width-mismatch demo** (`width_demo`) — exercises `=z`, `<=z`, `=s`, `<=s` assignment operators in a focused example. These are completely untested.

4. **Fixed-point FIR filter** (`fir_filter`) — exercises hardware `*`, `smul`, `sadd`, `smin`/`smax`, sign-extension operators, and width-safety rules.

5. **Bit-twiddling / CRC** (`crc32`) — exercises `reduce_xor`, `reverse`, `bswap`, `popcount` intrinsics cleanly.

6. **Encoder/decoder** (`encoder_demo`) — exercises `oh2b`, `b2oh`, `prienc`, `lzc`, and MUX auto-slicing in a compact example.

7. **LED PWM array** (`pwm_demo`) — exercises `@new[count]` array instantiation, `IDX`, and template `@apply[count]`.

_Generated on 2026-04-11_
