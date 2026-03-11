---
mainfont: "Helvetica Neue"
monofont: "Menlo"
title: "JZ-HDL SIMULATION SPECIFICATION"
subtitle: "State: Beta — Version: 0.1.0"
toc: true
header-includes:
  - \usepackage{titling}
  - \pretitle{\begin{center}\vspace*{\fill}\Huge\bfseries}
  - \posttitle{\end{center}}
  - \preauthor{\begin{center}\Large}
  - \postauthor{\end{center}\vspace*{\fill}\newpage}
  - \AtBeginDocument{\let\oldtableofcontents\tableofcontents \renewcommand{\tableofcontents}{\oldtableofcontents\newpage}}
  - \let\oldrule\rule \renewcommand{\rule}[2]{\newpage}
---

## 1. OVERVIEW

This specification defines the `@simulation` construct for JZ-HDL. While `@testbench` provides strictly manual, cycle-stepped control for functional verification, `@simulation` provides a **time-based, multi-clock continuous simulation environment**.

In a `@simulation`, clocks run automatically in the background based on their defined periods. The simulator calculates a common "tick" resolution, and the test sequence advances via absolute time (`@run`) directives rather than manual edge toggles. All declared `WIRE`s, `CLOCK`s, and `TAP`ped internal signals are automatically monitored and dumped to an output waveform file (e.g., VCD/FST) per tick.

**Relationship to `@testbench`:**

`@simulation` and `@testbench` are two **independent execution models**. They share the same RTL evaluation semantics (combinational settling, NBA for synchronous updates, reset behavior, x/z rules) but differ fundamentally in how time advances:

- `@simulation` is **time-based**: clocks have defined periods and toggle independently via GCD-based tick scheduling. Time advances via `@run` directives in absolute units (nanoseconds, milliseconds). Multiple clocks run concurrently.
- `@testbench` is **cycle-stepped**: clocks advance only when explicitly commanded by `@clock` directives, one clock at a time. There is no notion of absolute time or clock periods.

Neither model is a superset of the other. A simulation implementation is not required to use the testbench engine internally, and vice versa. The simulation model is designed for multi-clock timing verification and waveform capture; the testbench model is designed for deterministic, assertion-driven functional tests.

---

## 2. TIMING AND EXECUTION MODEL

### 2.1 Internal Time Resolution

All time values are represented internally as **64-bit unsigned integers in picoseconds**. When a clock period or `@run` duration is specified in nanoseconds or milliseconds, the compiler converts it to an integer picosecond count immediately at parse time. All subsequent arithmetic — GCD computation, tick advancement, clock scheduling — uses pure integer math. This eliminates floating-point drift.

**Exact conversion required.** The specified value must convert to an **exact integer number of picoseconds**. If the conversion would produce a fractional picosecond, the compiler rejects the value with a diagnostic error. There is no silent rounding or truncation. Examples:
- `period=10.0` → 10000 ps (valid)
- `period=3.333` → 3333 ps (valid: 3.333 × 1000 = 3333.0 exactly)
- `ns=0.1` → 100 ps (valid: 0.1 × 1000 = 100.0 exactly)
- `period=3.3335` → 3333.5 ps (rejected: not an integer picosecond count)

### 2.2 The "Tick" Resolution

Unlike `@testbench`, which is strictly cycle-relative, `@simulation` operates on an absolute timeline discretized into **ticks**.
- The simulator computes the toggle interval for each clock (half its period, in picoseconds) and takes the **Greatest Common Divisor (GCD)** of all toggle intervals. This GCD becomes the fundamental tick length.
- Example: If `clk_a` has a period of 10ns (toggles every 5000ps) and `clk_b` has a period of 14ns (toggles every 7000ps), the GCD is 1000ps = 1ns.

### 2.3 Time 0 Initialization

In hardware, clocks do not instantly start toggling the picosecond power is applied. The simulator models this with a deterministic initialization sequence at Time 0, before any clock edge occurs:

1. **Storage randomizes.** All REGISTER, LATCH, and MEM storage is filled with pseudo-random values derived from the simulation seed (`--seed`). This models the indeterminate power-on state of real flip-flops and memory cells.
2. **Clocks hold at `1'b0`.** No clock has toggled yet. Clocks with a phase offset (future feature) would use their specified initial value.
3. **`@setup` executes.** Explicit initial wire values (e.g., `rst_n <= 1'b0`) are applied to the testbench wires.
4. **Combinational logic settles.** Inputs propagate through the DUT, all combinational paths evaluate to a fixed point, and outputs propagate back to testbench wires.
5. **Time 0 is written to the waveform.** The VCD/FST file records the complete initial state — randomized registers, clock-low, setup values, and settled combinational outputs — before any clock edge fires.
6. **The first `@run` begins advancing ticks.**

**Important: A register's declared reset value is never applied automatically at instantiation or power-on.** Reset values take effect only when the module's reset signal is explicitly asserted during simulation. If a module has no reset port, or if the simulation never asserts reset, all registers retain their random power-on values for the entire simulation — exactly as in real hardware.

This ensures the waveform viewer shows the full initial setup state before the first clock edge.

### 2.4 Execution Flow

1. **Time Advancement (`@run`)**: The simulator advances time tick-by-tick up to the requested duration.
2. **Tick Evaluation**: At each tick:
   - If a clock is scheduled to toggle, its value updates.
   - Combinational logic evaluates to a fixed point (see Section 2.6).
   - If an active clock edge occurs, synchronous assignments are sampled and applied (NBA semantics).
   - Combinational logic settles again.
   - Monitored signals are sampled and written to the output file.
3. **Procedural Updates (`@update`)**: Executed instantaneously between `@run` commands. They apply wire changes, propagate inputs, settle combinational logic, propagate outputs, and record the changes to the waveform before the next `@run` begins.

### 2.4.1 Determinism Guarantee

**Given identical input source files and seed, the simulation must produce bit-identical output — waveform files and all diagnostic messages — across runs, across platforms, and across conforming implementations.**

This is a normative requirement. There is no implementation-defined ordering, no thread-dependent scheduling, and no platform-dependent evaluation. Every aspect of simulation is fully determined by the source text and the seed value:

- Storage randomization is derived solely from the seed via a specified PRNG algorithm.
- Tick scheduling is computed from integer picosecond arithmetic and GCD (Section 2.1, 2.2).
- Clock toggle order within a tick is determined by declaration order.
- Combinational settling follows a deterministic iteration order (Section 2.6).
- Waveform output records signal values at every tick using exact bit-vector state with no floating-point arithmetic.

If two conforming simulators produce different waveform files for the same source and seed, at least one has a bug.

### 2.6 Combinational Settling

Combinational settling in `@simulation` follows the same algorithm and rules as `@testbench` (Testbench Specification Section 2.2). Each settling pass evaluates all ASYNCHRONOUS blocks across the elaborated hierarchy — including latch transparent-state updates and asynchronous memory reads — and repeats until quiescence or the **100 delta cycle** limit is reached. If the limit is exceeded, the simulator reports a combinational loop runtime error and aborts.

### 2.5 X and Z Semantics

Simulation follows the same x and z semantics as the JZ-HDL specification (Sections 1.6 and 2.1) and the Testbench Specification (Section 2.5):

- **`x` is not a runtime value.** It exists only as a compile-time pattern-matching wildcard in CASE/SELECT. It never appears during simulation. All storage initializes with random `0`/`1` bits, not `x`.
- **`z` is a real electrical state** for tri-state nets. Tri-state resolution follows the resolution table in JZ-HDL specification Section 1.6.3, and multi-driver validity is enforced at compile time per Section 1.6.4. If `z` reaches a non-tristate expression at runtime (despite compile-time structural checks), the simulator reports a runtime error and aborts the current simulation.
- **Registers and latches cannot store `z`** (JZ-HDL specification Section 1.6.6). Only nets and ports participating in tri-state logic may carry `z`.

---

## 3. SIMULATION STRUCTURE

### 3.1 Declaration

**Syntax:**
```text
@simulation <module_name>
    @import "<path>";      // optional
    BUS <name> { ... }     // optional

    CLOCK { ... }
    WIRE { ... }
    TAP { ... }

    @new <instance_name> <module_name> { ... }

    @setup { ... }

    // Sequence of @run, @update, and @trace directives
@endsim
```

- `<module_name>` must refer to a `@module` in scope.
- `CLOCK`, `WIRE`, and `TAP` blocks define the testbed and monitoring scope.
- `@new` instantiates the module (identical to `@testbench` connection rules).

### 3.2 CLOCK Block (Auto-toggling)

Unlike testbenches, simulation clocks must have defined periods so they can run automatically.

**Syntax:**
```text
CLOCK {
    <clock_id> = { period=<ns> };
    ...
}
```
- `<clock_id>`: Initializes to `0` and toggles automatically every `period / 2` nanoseconds during `@run` directives.

### 3.3 WIRE Block

**Syntax:** Same as module and testbench `WIRE` blocks.
- Wires act as procedural drivers for module `IN` ports and observers for `OUT` ports.
- All declared `WIRE`s are automatically included in the waveform output.

### 3.4 TAP Block (Internal Monitoring)

**Syntax:**
```text
TAP {
    <instance_name>.<signal_name>;
    <instance_name>.<sub_instance>.<signal_name>;
    ...
}
```
- **Purpose**: Defines internal RTL signals (registers, wires, memories) to be exposed in the waveform dump.
- **Rules**: Signals must exist in the elaborated hierarchy. 
- All `TAP` signals, along with all `CLOCK`s and `WIRE`s, are recorded per tick.

---

## 4. STIMULUS AND CONTROL DIRECTIVES

### 4.1 @setup

**Syntax:**
```text
@setup {
    <wire_id> <= <literal>;
    ...
}
```
- Establishes time-zero values for testbench wires. 
- Must appear exactly once, immediately after `@new`.

### 4.2 @run (Time Advancement)

**Syntax:**
```text
@run(ticks=<integer>)
@run(ns=<number>)
@run(ms=<number>)
```
- Advances the global simulation time by the specified amount.
- During this time, all declared `CLOCK`s toggle automatically based on their periods.
- Synchronous and asynchronous RTL logic evaluates identically to physical hardware as the clocks toggle.
- You may use only one time unit parameter per `@run` directive.

### 4.3 @update

**Syntax:**
```text
@update {
    <wire_id> <= <literal_or_expression>;
    ...
}
```
- Applies new stimulus to testbench wires at the exact simulation time it is called (i.e., immediately after the preceding `@run` finishes).
- All assignments within a single `@update` block use **simultaneous assignment semantics**: all RHS expressions are evaluated using the pre-update wire values before any LHS targets are written. This means `b <= a; a <= a + 1;` assigns the old value of `a` to `b`, not the incremented value (see Testbench Specification Section 6.5 for details).
- After all assignments complete, combinational logic settles, and the result is logged to the waveform before the next `@run` advances time.

### 4.4 @run_until (Conditional Time Advancement)

**Syntax:**
```text
@run_until(<signal> == <value>, timeout=<unit>=<amount>)
@run_until(<signal> != <value>, timeout=<unit>=<amount>)
```

The `@run_until` directive advances simulation time tick-by-tick until the specified condition becomes true, or until the timeout is reached. Clocks toggle automatically during advancement, exactly as with `@run`.

- `<signal>` must be a testbench `WIRE` identifier (typically connected to a module output).
- `<value>` must be a sized literal.
- The condition is evaluated after each tick (after combinational settling and synchronous domain firing).
- The `timeout` parameter specifies the maximum simulation time to advance. If the condition is not met within the timeout, the simulator reports a **TIMEOUT** runtime error and aborts the simulation.
- The timeout uses the same time units as `@run`: `ns`, `ms`, or `ticks`.

**Example:**
```text
@run_until(count == 8'h05, timeout=ns=1000)
// Simulation advances until count reaches 5, or 1000ns elapses (error)
```

### 4.5 @run_while (Conditional Time Advancement)

**Syntax:**
```text
@run_while(<signal> == <value>, timeout=<unit>=<amount>)
@run_while(<signal> != <value>, timeout=<unit>=<amount>)
```

The `@run_while` directive advances simulation time tick-by-tick while the specified condition remains true, or until the timeout is reached. It is the logical complement of `@run_until`.

- Same rules as `@run_until` for signal references, value types, and timeout behavior.
- The condition is evaluated after each tick. Simulation stops when the condition becomes false.
- If the timeout is reached while the condition is still true, the simulator reports a **TIMEOUT** runtime error.

**Example:**
```text
@run_while(busy == 1'b1, timeout=ns=5000)
// Simulation advances while busy is high, or 5000ns elapses (error)
```

**Rules:**

| Rule | Description |
| :--- | :--- |
| SIM-030 | `@run_until`/`@run_while` condition not met within timeout (TIMEOUT runtime error) |

### 4.6 @repeat

**Syntax:**
```text
@repeat <count>
<body>
@end
```

The `@repeat` directive is a **pre-parser text expansion** shared with `@testbench`. Before lexing or parsing, the compiler scans the source text for `@repeat N ... @end` blocks, duplicates the body `N` times, and replaces each standalone occurrence of the identifier `IDX` with the iteration index (0 through N-1).

- `<count>` must be a positive integer literal.
- `<body>` may contain any valid simulation content: `@run`, `@update`, comments, or any other text.
- `IDX` is replaced on word boundaries only.
- Nesting is supported.
- `@end` closes only `@repeat` blocks — it does not conflict with `@endsim`.
- `@repeat` inside comments or string literals is ignored.

**Example — Burst stimulus with IDX:**
```text
@repeat 4
@update {
    data_in <= 8'hIDX;
}
@run(ns=10)
@end
// Expands to 4 sequential update/run pairs with data_in = 0, 1, 2, 3
```

**Rules:**

| Rule | Description |
| :--- | :--- |
| RPT-001 | `@repeat` requires a positive integer count |
| RPT-002 | `@repeat` without matching `@end` |

### 4.7 @print

**Syntax:**
```text
@print("<format_string>", <arg1>, <arg2>, ...)
```

The `@print` directive outputs a formatted message to the simulator's standard output at the current simulation time.

- `<format_string>` is a string literal containing text and optional format specifiers.
- Arguments following the format string are testbench wire identifiers or hierarchical signal references, matched positionally to format specifiers.
- The message is printed after all combinational logic has settled following the most recent `@run`, `@run_until`, `@run_while`, or `@update` directive.
- `@print` may appear anywhere in the simulation body where `@update` is valid.

**Format specifiers:**

| Specifier | Description |
| :--- | :--- |
| `%h` | Display the argument value in hexadecimal |
| `%d` | Display the argument value in decimal |
| `%b` | Display the argument value in binary |
| `%tick` | Display the current tick count (no argument consumed) |
| `%ms` | Display the current simulation time in milliseconds (no argument consumed) |

- `%tick` and `%ms` are **autonomous specifiers** — they do not consume an argument from the argument list.
- The number of non-autonomous format specifiers must match the number of arguments. A mismatch is a compile error.

**Example:**
```text
@print("wr_ptr = %h at tick %tick", dut.wr_ptr)
@print("time %ms: data_out = %d", data_out)
@print("tick %tick: reset released")
```

### 4.8 @print_if

**Syntax:**
```text
@print_if(<condition>, "<format_string>", <arg1>, <arg2>, ...)
```

The `@print_if` directive conditionally outputs a formatted message. The message is printed only if `<condition>` evaluates to a non-zero (truthy) value.

- `<condition>` is a testbench wire identifier or hierarchical signal reference. The condition is truthy if any bit is `1`.
- The format string and arguments follow the same rules as `@print`.
- `@print_if` may appear anywhere `@print` is valid.

**Example:**
```text
@print_if(full, "FIFO full at time %ms, wr_ptr=%h", dut.wr_ptr)
@print_if(empty, "FIFO empty at tick %tick")
```

**Rules:**

| Rule | Description |
| :--- | :--- |
| PRT-001 | Number of non-autonomous format specifiers must match the number of arguments |
| PRT-002 | `@print` / `@print_if` may not appear inside `@setup` or `@update` blocks |

### 4.9 @trace (Waveform Recording Control)

**Syntax:**
```text
@trace(state=on)
@trace(state=off)
```

The `@trace` directive controls whether signal value changes are recorded to the output waveform file during subsequent `@run`, `@run_until`, and `@run_while` directives.

- **Default state is `on`.** If no `@trace` directive appears, all signal changes are recorded (backward compatible).
- When `state=off`, the simulation continues to execute — clocks toggle, logic evaluates, time advances — but no signal values are written to the waveform output. This dramatically reduces output file size and simulation time for uninteresting periods (e.g., waiting for a power-on reset counter).
- When `state=on` after a `state=off` period, the simulator writes a **full state snapshot** at the current simulation time before resuming per-tick recording. This ensures the waveform viewer sees correct signal values at the start of the recorded window, not stale time-0 values.
- `@trace` may be toggled any number of times to capture specific windows of interest.
- `@trace` directives apply to all waveform output formats (VCD, FST, JZW).
- For JZW output, each `@trace` toggle writes an annotation of type `trace` to the annotations table, with `message` set to `"on"` or `"off"`. Viewers can use these annotations to indicate trace-off periods visually.

**Example:**
```text
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

**Rules:**

| Rule | Description |
| :--- | :--- |
| TRC-001 | `@trace` state must be `on` or `off` |
| TRC-002 | `@trace` may not appear inside `@setup` or `@update` blocks |

### 4.10 @mark (Waveform Marker)

**Syntax:**
```text
@mark(<color>)
@mark(<color>, "<message>")
```

The `@mark` directive creates a global marker annotation at the current simulation time. Marks are unconditional — they fire exactly once at the point where they appear in the simulation sequence.

- `<color>` is a color name (one of: `RED`, `ORANGE`, `YELLOW`, `GREEN`, `BLUE`, `PURPLE`, `CYAN`, `WHITE`). Case-insensitive in source, stored as uppercase.
- `<message>` is an optional string literal describing the marker.
- Marks are written to the JZW annotations table with `type="mark"`, `signal_id=NULL` (global), and the specified color and message.
- Marks have no effect on VCD or FST output.

**Example:**
```text
@update {
    rst_n <= 1'b1;
}
@mark(GREEN, "Reset released")

@run(ns=100)

@update {
    wr_en <= 1'b1;
}
@mark(BLUE, "Write burst start")

@run(ns=50)
```

**Viewer behavior:** Marks are displayed as vertical lines at the marked time in the specified color, with an icon at the bottom. Hovering the icon shows the message.

**Rules:**

| Rule | Description |
| :--- | :--- |
| MRK-001 | `@mark` may not appear inside `@setup`, `@update`, or `@new` blocks |
| MRK-002 | Color name must be one of the predefined names |

### 4.11 @alert (Unconditional Alert Annotation)

**Syntax:**
```text
@alert(<color>)
@alert(<color>, "<message>")
```

The `@alert` directive creates a one-shot, unconditional alert annotation at the current simulation time. It is the alert equivalent of `@mark` — it fires exactly once, right where it appears in the simulation sequence.

- `<color>` is a color name (one of: `RED`, `ORANGE`, `YELLOW`, `GREEN`, `BLUE`, `PURPLE`, `CYAN`, `WHITE`).
- `<message>` is an optional string literal describing the alert.
- The annotation is written to the JZW annotations table with `type="alert"`, `signal_id=NULL`, and the specified color and message.
- For conditional per-tick alert evaluation, use `@alert_if` inside a `MONITOR` block (§4.14).

**Example:**
```text
@run(ns=1000)
@alert(RED, "Checkpoint reached at 1µs")
@run(ns=1000)
```

**Viewer behavior:** Alert annotations are displayed as small chevron markers (^) at the bottom of the waveform area at the alert time. Hovering shows the message.

**Rules:**

| Rule | Description |
| :--- | :--- |
| ALT-001 | `@alert` may not appear inside `@setup`, `@update`, or `@new` blocks |
| ALT-002 | Color name must be one of the predefined names |

### 4.12 @mark_if (Conditional One-Shot Marker)

**Syntax:**
```text
@mark_if(<condition>, <color>)
@mark_if(<condition>, <color>, "<message>")
```

The `@mark_if` directive evaluates a condition once at the current simulation time. If the condition is true (non-zero), a mark annotation is created — identical to what `@mark` would produce. If the condition is false, nothing happens.

- `<condition>` is a testbench `WIRE` identifier or hierarchical signal reference (`TAP`). If the signal is multi-bit, it is truthy if any bit is `1`.
- `<color>` is a color name (one of: `RED`, `ORANGE`, `YELLOW`, `GREEN`, `BLUE`, `PURPLE`, `CYAN`, `WHITE`).
- `<message>` is an optional string literal describing the marker.
- Unlike `@alert`, this is **not** registered for per-tick evaluation. It fires at most once, at the point it appears in the simulation sequence.

**Example:**
```text
@run(ns=1000)
@mark_if(dut.error_flag, RED, "Error detected at checkpoint")
@run(ns=1000)
```

**Rules:**

| Rule | Description |
| :--- | :--- |
| MKI-001 | `@mark_if` may not appear inside `@setup`, `@update`, or `@new` blocks |
| MKI-002 | `@mark_if` condition must reference a signal in scope (declared in `WIRE`, `CLOCK`, or `TAP`) |
| MKI-003 | Color name must be one of the predefined names |

### 4.13 @alert_if (Conditional One-Shot Alert)

**Syntax:**
```text
@alert_if(<condition>, <color>)
@alert_if(<condition>, <color>, "<message>")
```

The `@alert_if` directive evaluates a condition once at the current simulation time. If the condition is true (non-zero), an alert annotation is created — identical to what a per-tick `@alert` would produce for a single tick. If the condition is false, nothing happens.

- `<condition>` is a testbench `WIRE` identifier or hierarchical signal reference (`TAP`). If the signal is multi-bit, it is truthy if any bit is `1`.
- `<color>` is a color name (one of: `RED`, `ORANGE`, `YELLOW`, `GREEN`, `BLUE`, `PURPLE`, `CYAN`, `WHITE`).
- `<message>` is an optional string literal describing the alert.
- Unlike `@alert`, this is **not** registered for per-tick evaluation. It fires at most once, at the point it appears in the simulation sequence.

**Example:**
```text
@run(ns=5000)
@alert_if(dut.fifo_full, RED, "FIFO full at 5µs checkpoint")
@run(ns=5000)
@alert_if(dut.fifo_full, RED, "FIFO full at 10µs checkpoint")
```

**Rules:**

| Rule | Description |
| :--- | :--- |
| ALI-001 | `@alert_if` may not appear inside `@setup`, `@update`, or `@new` blocks |
| ALI-002 | `@alert_if` condition must reference a signal in scope (declared in `WIRE`, `CLOCK`, or `TAP`) |
| ALI-003 | Color name must be one of the predefined names |

### 4.14 MONITOR Block (Per-Tick Conditional Evaluation)

**Syntax:**
```text
MONITOR {
    @print_if(<condition>, "<format>", ...)
    @mark_if(<condition>, <color>)
    @mark_if(<condition>, <color>, "<message>")
    @alert_if(<condition>, <color>)
    @alert_if(<condition>, <color>, "<message>")
}
```

The `MONITOR` block defines a set of conditional directives that are evaluated **every tick** during all `@run`, `@run_until`, and `@run_while` directives. Unlike the one-shot forms of `@print_if`, `@mark_if`, and `@alert_if` that appear in the simulation body, directives inside a `MONITOR` block fire repeatedly whenever their conditions are met.

- At most **one** `MONITOR` block is permitted per `@simulation`.
- Only `@print_if`, `@mark_if`, and `@alert_if` are permitted inside a `MONITOR` block. Other directives (e.g., `@run`, `@update`, `@print`, `@mark`, `@alert`) are not allowed.
- The `MONITOR` block may appear anywhere in the simulation body (before or after `@setup`, `@run`, etc.). Its position does not affect when evaluation begins — monitor directives are always evaluated on every tick from the first `@run` onward.
- Conditions reference `WIRE`, `CLOCK`, or `TAP` signals, following the same resolution rules as one-shot directives.

**Example:**
```text
@simulation fifo_test
    @import "fifo.jz";

    CLOCK { clk = { period=10.0 }; }
    WIRE { rst_n [1]; full [1]; empty [1]; }
    TAP { dut.overflow; }

    @new dut fifo { ... }

    MONITOR {
        @alert_if(dut.overflow, RED, "FIFO overflow detected!")
        @mark_if(full, ORANGE, "FIFO full")
        @print_if(empty, "FIFO empty at %t", $time)
    }

    @setup { rst_n <= 1'b0; }
    @run(ns=50)
    @update { rst_n <= 1'b1; }
    @run(ns=1000)
@endsim
```

**Rules:**

| Rule | Description |
| :--- | :--- |
| MON-001 | Only one `MONITOR` block is permitted per `@simulation` |
| MON-002 | Only `@print_if`, `@mark_if`, and `@alert_if` are permitted inside `MONITOR` |
| MON-003 | `MONITOR` conditions must reference signals in scope (`WIRE`, `CLOCK`, or `TAP`) |

---

## 5. CLI USAGE

### 5.1 `--simulate`

Files containing `@simulation` blocks must be run with the `--simulate` flag. Using `--lint` or `--test` on a file that contains `@simulation` will produce a `SIM_WRONG_TOOL` error.

```bash
jz-hdl --simulate sim_file.jz                # produces sim_file.vcd
jz-hdl --simulate sim_file.jz -o output.vcd  # explicit output path
jz-hdl --simulate sim_file.jz --seed=0xCAFE  # reproducible register init
jz-hdl --simulate sim_file.jz --verbose       # print tick resolution, events
```

### 5.2 Flags

| Flag | Description |
|---|---|
| `--simulate` | Run all `@simulation` blocks in the file. |
| `-o <path>` | Output waveform file path. Default: `<input_basename>.vcd`. |
| `--seed=0x<hex>` | 32-bit seed for register randomization. Default: `0xDEADBEEF`. |
| `--vcd` | Force VCD output format (default). |
| `--fst` | Force FST output format (not yet supported). |
| `--verbose` | Print tick resolution, clock periods, and `@run`/`@update` events. |

---

## 6. WAVEFORM OUTPUT

### 6.1 VCD Format

The simulator produces IEEE 1364 VCD (Value Change Dump) files. The VCD timescale is fixed at `1ns`, and all timestamps are written in nanosecond units regardless of the internal tick resolution. This ensures waveform viewers display readable, properly spaced timestamps.

### 6.2 Signal Grouping

Signals are organized into VCD scopes:

| Scope | Contents |
|---|---|
| `clocks` | All `CLOCK`-declared signals. |
| `wires` | All `WIRE`-declared signals (inputs and observed outputs). |
| `<instance>` | `TAP` signals, scoped by the hierarchy prefix (e.g., `TAP { dut.wr_ptr; }` appears under scope `dut`). |

### 6.3 What Gets Recorded

Every signal in `CLOCK`, `WIRE`, and `TAP` blocks is sampled and written to the waveform file at every tick during `@run`, and at each `@setup`/`@update` event. The Time 0 dump captures the full initial state before any clock edge.

---

## 7. EXAMPLE

```text
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

    MONITOR {
        @alert_if(full, RED, "FIFO full")
        @mark_if(empty, CYAN, "FIFO empty")
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

    // Run long enough to see read clocks process the data
    @run(ns=100)
@endsim
```