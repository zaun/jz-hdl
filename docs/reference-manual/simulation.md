---
title: Simulation
lang: en-US

layout: doc
outline: deep
---

# Simulation

## Overview

The `@simulation` construct provides a **time-based, multi-clock continuous simulation environment**. While [`@testbench`](/reference-manual/testbench) provides strictly manual, cycle-stepped control for functional verification, `@simulation` runs clocks automatically in the background based on their defined periods.

In a simulation, the simulator uses event-driven scheduling with 1 picosecond resolution, jumping directly between clock edges. The test sequence advances via absolute time (`@run`) directives rather than manual edge toggles. All declared `WIRE`s, `CLOCK`s, and `TAP`ped internal signals are automatically monitored and dumped to an output waveform file (VCD) at each clock event.

::: tip When to use @simulation vs @testbench
Use `@simulation` for **waveform-based analysis** — observing multi-clock behavior over time with automatic clock toggling and VCD output. Use [`@testbench`](/reference-manual/testbench) for **functional verification** — asserting specific values at specific cycle counts.
:::

## Timing and Execution Model

### Internal Time Resolution

All time values are represented internally as **64-bit unsigned integers in picoseconds**. When a clock period or `@run` duration is specified in nanoseconds or milliseconds, the compiler converts it to an integer picosecond count at parse time. All subsequent arithmetic — event scheduling, tick advancement, clock toggling — uses pure integer math. This eliminates floating-point drift.

**Exact conversion required.** The specified value must convert to an **exact integer number of picoseconds**. If the conversion would produce a fractional picosecond, the compiler rejects the value with an error. There is no silent rounding or truncation.

| Value | Conversion | Result |
| --- | --- | --- |
| `period=10.0` | 10.0 × 1000 | 10000 ps (valid) |
| `period=3.333` | 3.333 × 1000 | 3333 ps (valid) |
| `ns=0.1` | 0.1 × 1000 | 100 ps (valid) |
| `period=3.3335` | 3.3335 × 1000 | 3333.5 ps (rejected) |

### Event-Driven Clock Scheduling

Unlike `@testbench`, which is strictly cycle-relative, `@simulation` operates on an absolute timeline with **1 picosecond resolution**.

The simulator uses **event-driven scheduling** for clock toggles. Each clock maintains a `next_toggle_ps` timestamp indicating when it will next change value. When advancing time, the simulator jumps directly to the earliest scheduled clock event — skipping empty time where no clock toggles. After toggling a clock, its `next_toggle_ps` advances by half the clock's period. This provides exact 1ps timing resolution while maintaining O(number of clock edges) performance.

**GCD tick for `@run(ticks=N)`:** The simulator also computes the Greatest Common Divisor (GCD) of all clock toggle intervals to define the **tick unit**. When `@run(ticks=N)` is specified, the duration advanced is `N × GCD` picoseconds. For example, if `clk_a` toggles every 5000ps and `clk_b` every 7000ps, the GCD is 1000ps, and `@run(ticks=10)` advances 10ns. The `ns` and `ms` forms are unaffected.

### Time 0 Initialization

The simulator models the deterministic power-on initialization sequence at Time 0, before any clock edge occurs:

1. **Storage randomizes.** All REGISTER, LATCH, and MEM storage is filled with pseudo-random values derived from the simulation seed (`--seed`). This models the indeterminate power-on state of real flip-flops and memory cells.
2. **Clocks hold at `1'b0`.** No clock has toggled yet.
3. **`@setup` executes.** Explicit initial wire values (e.g., `rst_n <= 1'b0`) are applied.
4. **Combinational logic settles.** Inputs propagate through the DUT, all combinational paths evaluate to a fixed point.
5. **Time 0 is written to the waveform.** The VCD file records the complete initial state — randomized registers, clock-low, setup values, and settled combinational outputs — before any clock edge fires.
6. **The first `@run` begins advancing ticks.**

::: danger Important
A register's declared reset value is **never** applied automatically at instantiation or power-on. Reset values take effect only when the module's reset signal is explicitly asserted during simulation. If a module has no reset port, or if the simulation never asserts reset, all registers retain their random power-on values for the entire simulation — exactly as in real hardware.
:::

### Execution Flow

1. **Time Advancement (`@run`, `@run_until`, `@run_while`)**: The simulator advances time by jumping to successive clock events up to the requested duration or until a condition is met.
2. **Clock Event Evaluation**: At each clock event (a time where one or more clocks toggle):
   - All clocks scheduled to toggle at this time update their values and schedule their next toggle.
   - Combinational logic evaluates to a fixed point.
   - Synchronous assignments are sampled and applied (NBA semantics).
   - Combinational logic settles again.
   - Monitored signals are sampled and written to the output file.
3. **Procedural Updates (`@update`)**: Executed instantaneously between `@run` commands. They apply wire changes, propagate inputs, settle combinational logic, propagate outputs, and record the changes before the next `@run` begins.

### Determinism Guarantee

**Given identical input source files and seed, the simulation must produce bit-identical output — waveform files and all diagnostic messages — across runs, across platforms, and across conforming implementations.**

There is no implementation-defined ordering, no thread-dependent scheduling, and no platform-dependent evaluation. Every aspect of simulation is fully determined by the source text and the seed value:

- Storage randomization is derived solely from the seed via a specified PRNG algorithm.
- Clock scheduling uses event-driven `next_toggle_ps` timestamps computed from integer picosecond arithmetic.
- When multiple clocks toggle at the same time, toggle order is determined by declaration order.
- Combinational settling follows a deterministic iteration order.
- Waveform output records signal values at every tick using exact bit-vector state with no floating-point arithmetic.

If two conforming simulators produce different waveform files for the same source and seed, at least one has a bug.

### Combinational Settling

Combinational settling follows the same algorithm as `@testbench`. Each settling pass evaluates all ASYNCHRONOUS blocks across the elaborated hierarchy — including latch transparent-state updates and asynchronous memory reads — and repeats until quiescence or the **100 delta cycle** limit is reached. If the limit is exceeded, the simulator reports a combinational loop runtime error (SE-001) and aborts.

### X and Z Semantics

Simulation follows the same x and z semantics as the main JZ-HDL specification and the Testbench specification:

- **`x` is not a runtime value.** It exists only as a compile-time pattern-matching wildcard in CASE/SELECT. It never appears during simulation. All storage initializes with random `0`/`1` bits, not `x`.
- **`z` is a real electrical state** for tri-state nets. Tri-state resolution follows the per-bit resolution table in the JZ-HDL specification, and multi-driver validity is enforced at compile time. If `z` reaches a non-tristate expression at runtime (IF condition, SELECT selector, ternary condition), the simulator reports a runtime error (SE-008) and aborts.
- **Registers and latches cannot store `z`.** Only nets and ports participating in tri-state logic may carry `z`.

## Simulation Structure

```text
@simulation <module_name>
    @import "<path>";      // optional
    BUS <name> { ... }     // optional

    CLOCK { ... }
    WIRE { ... }
    TAP { ... }

    @new <instance_name> <module_name> { ... }

    @setup { ... }

    // Sequence of @run, @run_until, @run_while, @update, and @trace directives
@endsim
```

- `<module_name>` must refer to a `@module` in scope.
- `CLOCK`, `WIRE`, and `TAP` blocks define the testbed and monitoring scope.
- `@new` instantiates the module (identical to `@testbench` connection rules).

## CLOCK Block

```text
CLOCK {
    <clock_id> = { period=<ns> };
    ...
}
```

Unlike testbenches, simulation clocks must have defined periods so they can run automatically.

- `<clock_id>`: Initializes to `1'b0` and toggles automatically every `period / 2` nanoseconds during `@run` directives.
- Period values are specified in nanoseconds. The value must convert to an exact integer number of picoseconds (see [Internal Time Resolution](#internal-time-resolution)).

## WIRE Block

```text
WIRE {
    <wire_id> [<width>];
    BUS <bus_id> [<count>] <group_name>;
    ...
}
```

Same syntax as module and testbench `WIRE` blocks.

- Wires act as procedural drivers for module `IN` ports and observers for `OUT` ports.
- All declared wires are automatically included in the waveform output.

## TAP Block

```text
TAP {
    <instance_name>.<signal_name>;
    <instance_name>.<sub_instance>.<signal_name>;
    ...
}
```

The TAP block defines internal RTL signals to be exposed in the waveform dump.

- Signals must exist in the elaborated hierarchy.
- All TAP signals, along with all CLOCKs and WIREs, are recorded per tick.
- Use hierarchical references to reach signals at any level of the design: `dut.wr_ptr`, `dut.sub_module.counter`.

## Directives

### @setup

```text
@setup {
    <wire_id> <= <literal>;
    ...
}
```

Establishes time-zero values for testbench wires. Must appear exactly once, immediately after `@new`.

### @run

```text
@run(ticks=<integer>)
@run(ns=<number>)
@run(ms=<number>)
```

Advances the global simulation time by the specified amount.

- During this time, all declared clocks toggle automatically based on their periods.
- Synchronous and asynchronous RTL logic evaluates as the clocks toggle.
- Only one time unit parameter is allowed per `@run` directive.
- Values specified in `ns` or `ms` must convert to an exact integer number of picoseconds (see [Internal Time Resolution](#internal-time-resolution)).

| Unit | Description |
| --- | --- |
| `ticks=<integer>` | Advance by the specified number of GCD ticks (see [Event-Driven Clock Scheduling](#event-driven-clock-scheduling)). |
| `ns=<number>` | Advance by nanoseconds. Converted to picoseconds internally. |
| `ms=<number>` | Advance by milliseconds. Converted to picoseconds internally. |

### @update

```text
@update {
    <wire_id> <= <literal_or_expression>;
    ...
}
```

Applies new stimulus to testbench wires at the exact simulation time it is called (immediately after the preceding `@run` finishes).

- All assignments within a single `@update` block use **simultaneous assignment semantics**: all RHS expressions are evaluated using the pre-update wire values before any LHS targets are written. This means `b <= a; a <= a + 1;` assigns the old value of `a` to `b`, not the incremented value (see [Testbench @update](/reference-manual/testbench#update) for details).
- After all assignments complete, combinational logic settles, and the result is logged to the waveform before the next `@run` advances time.

### @run_until

```text
@run_until(<signal> == <value>, timeout=<unit>=<amount>)
@run_until(<signal> != <value>, timeout=<unit>=<amount>)
```

Advances simulation time tick-by-tick until the specified condition becomes true, or until the timeout is reached. Clocks toggle automatically during advancement, exactly as with `@run`.

- `<signal>` must be a testbench `WIRE` identifier (typically connected to a module output).
- `<value>` must be a sized literal.
- The condition is evaluated after each tick (after combinational settling and synchronous domain firing).
- The `timeout` parameter specifies the maximum simulation time. If the condition is not met within the timeout, the simulator reports a **TIMEOUT** runtime error and aborts.
- Timeout uses the same time units as `@run`: `ns`, `ms`, or `ticks`.

```jz
// Run until counter reaches 5, with a 1000ns safety timeout
@run_until(count == 8'h05, timeout=ns=1000)
```

### @run_while

```text
@run_while(<signal> == <value>, timeout=<unit>=<amount>)
@run_while(<signal> != <value>, timeout=<unit>=<amount>)
```

Advances simulation time tick-by-tick while the specified condition remains true, or until the timeout is reached. This is the logical complement of `@run_until`.

- Same rules as `@run_until` for signal references, value types, and timeout behavior.
- Simulation stops when the condition becomes false.
- If the timeout is reached while the condition is still true, the simulator reports a **TIMEOUT** runtime error.

```jz
// Run while busy is high, with a 5000ns safety timeout
@run_while(busy == 1'b1, timeout=ns=5000)
```

::: tip @run_until vs @run_while
`@run_until(x == val)` and `@run_while(x != val)` are logically equivalent — both stop when `x` equals `val`. Choose whichever reads more naturally for your use case.
:::

### @repeat

```text
@repeat <count>
<body>
@end
```

The `@repeat` directive is a **pre-parser text expansion**. Before lexing or parsing, the compiler duplicates the body `<count>` times, replacing each standalone occurrence of `IDX` with the iteration index (0 through N-1).

- `<count>` must be a positive integer literal.
- `<body>` may contain any valid simulation content: `@run`, `@update`, `@run_until`, `@run_while`, comments, or any other text.
- `IDX` is replaced on word boundaries only — it will not match inside identifiers like `INDEX` or `MY_IDX_VAR`.
- Nesting is supported: an inner `@repeat` expands fully within each iteration of the outer `@repeat`.
- `@end` closes only `@repeat` blocks — it does not conflict with `@endsim` or other closing directives.
- `@repeat` inside comments or string literals is ignored.

```jz
// Issue 4 sequential update/run pairs with incrementing data
@repeat 4
@update {
    data_in <= 8'hIDX;
}
@run(ns=10)
@end
```

### @print

```text
@print("<format_string>", <arg1>, <arg2>, ...)
```

Outputs a formatted message to the simulator's standard output at the current simulation time. The message is printed after all combinational logic has settled.

**Format specifiers:**

| Specifier | Description |
| --- | --- |
| `%h` | Hexadecimal |
| `%d` | Decimal |
| `%b` | Binary |
| `%tick` | Current tick count (no argument consumed) |
| `%ms` | Current simulation time in ms (no argument consumed) |

`%tick` and `%ms` are autonomous — they do not consume an argument from the argument list.

```jz
@print("wr_ptr = %h at tick %tick", dut.wr_ptr)
@print("time %ms: data_out = %d", data_out)
@print("tick %tick: reset released")
```

### @print_if

```text
@print_if(<condition>, "<format_string>", <arg1>, <arg2>, ...)
```

Conditionally outputs a formatted message. The message is printed only if `<condition>` is non-zero (truthy — any bit is `1`).

- `<condition>` is a testbench wire or hierarchical signal reference.
- The format string and arguments follow the same rules as `@print`.

```jz
@print_if(full, "FIFO full at time %ms, wr_ptr=%h", dut.wr_ptr)
@print_if(empty, "FIFO empty at tick %tick")
```

### @trace

```text
@trace(state=on)
@trace(state=off)
```

Controls whether signal value changes are recorded to the output waveform file during subsequent `@run`, `@run_until`, and `@run_while` directives.

- **Default state is `on`.** If no `@trace` directive appears, all signal changes are recorded (backward compatible).
- When `state=off`, the simulation continues to execute — clocks toggle, logic evaluates, time advances — but no signal values are written to the waveform output. This dramatically reduces output file size and simulation time for uninteresting periods (e.g., waiting for a power-on reset counter).
- When `state=on` after a `state=off` period, the simulator writes a **full state snapshot** at the current time before resuming per-tick recording. This ensures the waveform viewer sees correct signal values at the start of the recorded window.
- `@trace` may be toggled any number of times to capture specific windows of interest.
- For JZW output, each toggle writes a `trace` annotation to the annotations table.

```jz
@setup {
    por   <= 1'b1;
    rst_n <= 1'b1;
}

// Skip recording during 28ms POR wait
@trace(state=off)
@run(ns=28200000)
@trace(state=on)

// Now record the interesting video output
@run(ns=200000)
```

::: tip When to use @trace
Use `@trace(state=off)` to skip long initialization periods (POR counters, PLL lock times, debounce waits) that produce large waveforms of mostly static signals. The simulation still runs correctly — only the recording is suppressed.
:::

## Waveform Output

### VCD Format

The simulator produces IEEE 1364 VCD (Value Change Dump) files. The VCD timescale is fixed at `1ns`, and all timestamps are written in nanosecond units regardless of the internal tick resolution.

### Signal Grouping

Signals are organized into VCD scopes:

| Scope | Contents |
| --- | --- |
| `clocks` | All CLOCK-declared signals. |
| `wires` | All WIRE-declared signals (inputs and observed outputs). |
| `<instance>` | TAP signals, scoped by the hierarchy prefix (e.g., `TAP { dut.wr_ptr; }` appears under scope `dut`). |

### What Gets Recorded

Every signal in CLOCK, WIRE, and TAP blocks is sampled and written to the waveform file at every tick during `@run`, and at each `@setup`/`@update` event. The Time 0 dump captures the full initial state before any clock edge.

## CLI Usage

```bash
jz-hdl sim_file.jz --simulate                    # produces sim_file.vcd
jz-hdl sim_file.jz --simulate -o output.vcd      # explicit output path
jz-hdl sim_file.jz --simulate --seed=0xCAFE      # reproducible register init
jz-hdl sim_file.jz --simulate --verbose           # print tick resolution, events
```

Files containing `@simulation` blocks must be run with `--simulate`. Using `--lint` or `--test` on a file that contains `@simulation` will produce a `SIM_WRONG_TOOL` error.

### Flags

| Flag | Description |
| --- | --- |
| `--simulate` | Run all `@simulation` blocks in the file. |
| `-o <path>` | Output waveform file path. Default: `<input_basename>.vcd`. |
| `--seed=0xHEX` | 32-bit seed for register randomization. Default: `0xDEADBEEF`. |
| `--vcd` | Force VCD output format (default). |
| `--fst` | Force FST output format (not yet supported). |
| `--verbose` | Print tick resolution, clock periods, and `@run`/`@update` events. |

## Complete Example

```jz
@simulation fifo_ctrl
    @import "fifo.jz";

    // Define clocks with different frequencies
    CLOCK {
        clk_wr = { period=10.0 }; // 100 MHz
        clk_rd = { period=25.0 }; // 40 MHz
    }

    WIRE {
        rst_n [1];
        wr_en [1];
        rd_en [1];
        data_in [8];
        data_out [8];
        full [1];
        empty [1];
    }

    // Monitor internal FIFO pointers
    TAP {
        dut.wr_ptr;
        dut.rd_ptr;
    }

    @new dut async_fifo {
        clk_wr [1] = clk_wr;
        clk_rd [1] = clk_rd;
        rst_n  [1] = rst_n;
        wr_en  [1] = wr_en;
        rd_en  [1] = rd_en;
        din    [8] = data_in;
        dout   [8] = data_out;
        full   [1] = full;
        empty  [1] = empty;
    }

    @setup {
        rst_n   <= 1'b0;
        wr_en   <= 1'b0;
        rd_en   <= 1'b0;
        data_in <= 8'h00;
    }

    // Hold reset for 50ns while clocks run
    @run(ns=50)

    // Release reset
    @update {
        rst_n <= 1'b1;
    }

    // Wait for internal synchronization
    @run(ns=50)

    // Burst write data
    @update {
        wr_en   <= 1'b1;
        data_in <= 8'hAA;
    }
    @run(ns=10)

    @update {
        data_in <= 8'hBB;
    }
    @run(ns=10)

    @update {
        wr_en <= 1'b0;
        rd_en <= 1'b1;
    }

    // Wait until FIFO is no longer empty, then run a bit more
    @run_until(empty == 1'b0, timeout=ns=200)
    @run(ns=100)
@endsim
```
