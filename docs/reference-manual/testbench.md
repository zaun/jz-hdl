---
title: Testbenches
lang: en-US

layout: doc
outline: deep
---

# Testbenches

## Overview

Testbenches are verification constructs that drive and observe synthesizable modules in a cycle-accurate simulation environment. They are strictly non-synthesizable and exist in a separate namespace from RTL modules.

JZ-HDL testbench simulation operates at the **cycle level**. There are no absolute time units, no propagation delays, and no inertial/transport delay models. All timing is expressed in terms of clock cycles. If a design compiles, its structural correctness is guaranteed by the compiler; simulation verifies **functional behavior** within that structure.

**Design Principles:**
- **Cycle-relative timing** — all time advancement is expressed in clock cycles, never in absolute time units.
- **Explicit synchronization** — stimulus changes occur only at designated `@update` points between clock phases.
- **Deterministic ordering** — the sequence of operations within a test is strictly linear. No concurrent stimulus, no fork/join, no event-driven scheduling.
- **RTL isolation** — testbench constructs cannot appear inside `@module` or `@project`. RTL constructs cannot appear inside `@testbench`. The compiler enforces this boundary at parse time.
- **No delays** — there are no `#5`, `after`, `wait`, or inertial/transport delay semantics.

::: tip When to use @testbench vs @simulation
Use `@testbench` for **functional verification** — asserting specific values at specific cycle counts. Use [`@simulation`](/reference-manual/simulation) for **waveform-based analysis** — observing multi-clock behavior over time with automatic clock toggling.
:::

## Testbench Structure

```text
@testbench <module_name>
    @import "<path>";      // optional
    BUS <name> { ... }     // optional

    CLOCK { ... }
    WIRE { ... }

    TEST "<description>" { ... }
    ...
@endtb
```

- `<module_name>` must refer to a `@module` that is in scope (via `@import` or within the same compilation unit).
- A testbench may contain zero or more `@import` directives, `BUS` definitions, `CLOCK` declarations, `WIRE` declarations, and one or more `TEST` blocks.
- `@import` and `BUS` definitions must appear before `CLOCK`, `WIRE`, and `TEST` blocks.
- `CLOCK` and `WIRE` blocks are testbench-level declarations shared across all `TEST` blocks.
- Multiple `@testbench` blocks may target the same module. Each is independent.

### File Organization

Testbench files use the same `.jz` extension as RTL files. A single file may contain:
- `@import` directives
- `@global` blocks
- `BUS` definitions
- One or more `@testbench` blocks

A file may **not** contain both `@module`/`@project` definitions and `@testbench` blocks. The compiler rejects such files with a parse error.

## CLOCK Block

```text
CLOCK {
    <clock_id>;
    ...
}
```

Each clock declaration creates a named clock signal that the testbench controls.

- Clocks are width-1 signals initialized to `1'b0`.
- Clocks are **not** assignable in `@setup` or `@update` — they are driven exclusively by `@clock` directives.
- Between `@clock` directives, the clock holds its last value.
- If `@clock` is never called for a declared clock, that clock remains at `1'b0` for the entire test.

## WIRE Block

```text
WIRE {
    <wire_id> [<width>];
    BUS <bus_id> [<count>] <group_name>;
    ...
}
```

Testbench wires are the signals used to drive module inputs and observe module outputs. They follow the same width declaration syntax as module-level `WIRE` blocks.

- Wires are **procedural drivers**, not combinational nets. They hold their assigned value until explicitly changed by an `@update` block.
- A wire connected to a module `IN` port acts as a stimulus driver.
- A wire connected to a module `OUT` port acts as an observation point.
- A wire connected to a module `INOUT` port acts as both driver and observer, participating in tri-state resolution.
- Wires initialize to `0` (all bits zero) at the start of each test.

### BUS Wire Declarations

A `BUS` wire declaration expands a named BUS definition into individual testbench wires:

```text
BUS <bus_id> [<count>] <group_name>;
```

- `<bus_id>` must refer to a `BUS` definition in scope.
- `[<count>]` is optional. When present, wires are named `<group_name><N>_<signal>` for each element `N` from `0` to `count-1`. When omitted, wires are named `<group_name>_<signal>`.

**Example:** Given a BUS definition:

```text
BUS PARALLEL_BUS {
    OUT   [16] ADDR;
    OUT   [1]  CMD;
    OUT   [1]  VALID;
    INOUT [16] DATA;
    IN    [1]  DONE;
}
```

The declaration `BUS PARALLEL_BUS [2] src;` expands to:

```text
src0_ADDR [16];  src0_CMD [1];  src0_VALID [1];  src0_DATA [16];  src0_DONE [1];
src1_ADDR [16];  src1_CMD [1];  src1_VALID [1];  src1_DATA [16];  src1_DONE [1];
```

**Complete BUS array example:**

```jz
BUS PARALLEL_BUS {
    OUT   [16] ADDR;
    OUT   [1]  CMD;
    OUT   [1]  VALID;
    INOUT [16] DATA;
    IN    [1]  DONE;
}

@testbench memory_map
    CLOCK {
        clk;
    }

    WIRE {
        rst_n [1];
        BUS PARALLEL_BUS src;        // single bus port (SOURCE side)
        BUS PARALLEL_BUS [2] tgt;    // array of 2 bus ports (TARGET side)
    }

    TEST "Bus routing to target 0" {
        @new dut memory_map {
            clk [1] = clk;
            rst_n [1] = rst_n;
            BUS PARALLEL_BUS src = src;
            BUS PARALLEL_BUS tgt = tgt;
        }

        @setup {
            rst_n <= 1'b0;
            src_VALID <= 1'b0;
        }

        @clock(clk, cycle=2)
        @update { rst_n <= 1'b1; }

        // Drive a read to address in target 0's range
        @update {
            src_ADDR  <= 16'h0010;
            src_CMD   <= 1'b0;
            src_VALID <= 1'b1;
        }
        @clock(clk, cycle=1)

        // Target 0 should see the request
        @expect_equal(tgt0_VALID, 1'b1)
        @expect_equal(tgt0_ADDR, 16'h0010)

        // Respond with data on target 0
        @update {
            tgt0_DATA <= 16'hBEEF;
            tgt0_DONE <= 1'b1;
        }
        @clock(clk, cycle=1)

        // Source sees the response
        @expect_equal(src_DATA, 16'hBEEF)
        @expect_equal(src_DONE, 1'b1)
    }
@endtb
```

In this example, expanded wire names like `src_ADDR`, `tgt0_VALID`, and `tgt1_DATA` are used directly in `@update` and `@expect_equal` directives.

## TEST Block

```text
TEST "<description>" {
    @new <instance_name> <module_name> {
        <port_id> [<width>] = <wire_id | clock_id>;
        BUS <bus_id> <port_prefix> = <wire_prefix>;
        ...
    }

    @setup { ... }

    <sequence of @clock, @update, @expect directives>
}
```

Each `TEST` block is an independent test case. Test cases do not share state — each test begins with a fresh module instance and fresh wire values.

- `<description>` is a string literal used for identification in test output and failure reports.
- A test block must contain exactly one `@new` and exactly one `@setup`.
- `@setup` must appear after `@new` and before any `@clock`, `@update`, or `@expect` directives.

### Module Instantiation (@new)

```text
@new <instance_name> <module_name> {
    <port_id> [<width>] = <wire_id | clock_id>;
    BUS <bus_id> <port_prefix> = <wire_prefix>;
    ...
}
```

Creates an instance of the module under test and connects its ports to testbench clocks and wires. All module ports must be connected — unconnected ports are a compile error.

BUS ports may be connected using shorthand that binds all expanded signals at once:

```text
BUS PARALLEL_BUS src = src;   // binds src0_ADDR, src0_CMD, etc.
```

### @setup

```text
@setup {
    <wire_id> <= <literal>;
    ...
}
```

Establishes the initial electrical state of all testbench wires before simulation begins. Executes once at the start of the test, at simulation time zero, before any clock edges.

- Uses the `<=` operator for all assignments.
- Assignments take effect simultaneously.
- Any wire not assigned in `@setup` is implicitly initialized to `0`.
- Clock signals may not be assigned in `@setup`.

### @clock

```text
@clock(<clock_id>, cycle=<count>)
```

Advances simulation time by toggling the named clock for `<count>` complete cycles.

- Each cycle consists of one full period (rising edge → high → falling edge → low).
- After the directive completes, all synchronous and combinational logic has been evaluated and settled.
- Multiple `@clock` directives may appear in sequence, potentially for different clocks.

### @update

```text
@update {
    <wire_id> <= <literal_or_expression>;
    ...
}
```

Changes the values of testbench wires at a defined synchronization point between clock phases.

- Only testbench `WIRE` identifiers may be assigned. Clock signals may not be reassigned.
- All assignments within a single `@update` block take effect **simultaneously** — all RHS expressions are evaluated using the pre-update wire values before any LHS targets are written.
- Combinational logic settles after all assignments are applied.

This means swaps are well-defined:

```text
@update {
    a <= b;
    b <= a;
}
// Result: a and b exchange values
```

**All RHS expressions read the pre-update values**, even when the same wire appears on both sides:

```text
@update {
    a <= a + 8'h01;
    b <= a;
}
// b receives the OLD value of a (before the increment).
// a receives old_a + 1.
// This is NOT sequential: b does not see the incremented value.
```

Expressions in `@update` support the same syntax as ASYNCHRONOUS blocks (arithmetic, bitwise, concatenation, ternary):

```text
@update {
    addr <= addr + 8'h01;
    data <= {upper_nibble, lower_nibble};
}
```

### @expect_equal

```text
@expect_equal(<signal>, <expected_value>)
```

Asserts that a signal's current value matches the expected value.

- `<signal>` may be any testbench wire or a hierarchical reference to an internal signal (e.g., `dut.internal_reg`).
- `<expected_value>` must be a sized literal or `CONST` expression — fully determined, no `x` or `z` bits.
- Width of `<expected_value>` must exactly match the width of `<signal>`. A mismatch is a **compile error** (`TB-011 @expect value width must match signal width`).
- If the observed signal contains `z`, it is a **runtime error** — the test aborts (see [X and Z Semantics](#x-and-z-semantics)).

### @expect_not_equal

```text
@expect_not_equal(<signal>, <value>)
```

Asserts that the signal's current value does **not** match the given value. Same rules as `@expect_equal` regarding signal references, width matching, and evaluation timing.

### @expect_tristate

```text
@expect_tristate(<signal>)
```

Asserts that all bits of the signal are currently in the high-impedance (`z`) state.

- Takes only one argument — the expected state is always all-z.
- This is the only assertion that accepts `z` values; `@expect_equal` and `@expect_not_equal` treat `z` as a runtime error.

## Simulation Phases

Each simulation step proceeds through these phases in strict order:

```
Simulation Step:
  1. STIMULUS PHASE      - Apply @setup or @update values
  2. COMBINATIONAL PHASE - Evaluate all ASYNCHRONOUS logic to a fixed point
  3. SAMPLE PHASE        - Sample clock edges and reset signals
  4. SEQUENTIAL PHASE    - Apply all SYNCHRONOUS updates (NBA semantics)
  5. SETTLE PHASE        - Re-evaluate ASYNCHRONOUS logic to a fixed point
  6. OBSERVE PHASE       - All signals are stable; assertions execute
```

Phases 2–5 repeat for each clock edge within a `@clock` directive. For `@clock(clk, cycle=N)`, the simulator executes `2N` edge transitions (N rising + N falling), running phases 2–5 at each edge where synchronous logic is sensitive.

### Combinational Settling

During the COMBINATIONAL and SETTLE phases, the simulator evaluates all ASYNCHRONOUS blocks across the elaborated hierarchy to a **fixed point**:

1. Evaluate all ASYNCHRONOUS assignments, latch transparent-state updates, and asynchronous memory reads.
2. If any net value changed, re-evaluate all ASYNCHRONOUS logic.
3. Repeat until no values change (quiescence).
4. If quiescence is not reached within **100 delta cycles**, the simulator reports a combinational loop runtime error (SE-001) and aborts.

### Sequential Update Semantics

SYNCHRONOUS updates use **non-blocking assignment** (NBA) semantics:

1. All RHS expressions are evaluated using current (pre-update) register values.
2. All register updates are applied simultaneously.
3. ASYNCHRONOUS logic re-evaluates to a fixed point (SETTLE PHASE).

### Reset Semantics

- **Immediate Reset** (`RESET_TYPE=Immediate`): When the reset condition is met, registers are forced to reset values immediately, bypassing the clock. Overrides pending synchronous updates.
- **Clocked Reset** (`RESET_TYPE=Clocked`): When the reset condition is met at a clock edge, registers load reset values at that edge. Reset takes priority over all synchronous assignments.

## X and Z Semantics

### X Is Not a Runtime Value

`x` exists only as a compile-time pattern-matching wildcard in CASE/SELECT expressions. It **never appears during simulation**. All storage (registers, latches, memory) initializes with random `0`/`1` bits, not `x`. All wires and ports carry only `0`, `1`, or `z`.

If the simulator's internal computations ever produce an `x` state, this indicates a `z` value reached a non-tristate expression — which is a runtime error.

### Z Is a Real Electrical State

`z` (high-impedance) is a real runtime value for tri-state nets. Tri-state resolution follows the per-bit resolution table:

| Driver A | Driver B | Result |
| --- | --- | --- |
| `0` | `z` | `0` |
| `1` | `z` | `1` |
| `z` | `z` | `z` |
| `0` | `1` | Prohibited (compile-time structural proof) |

Multi-driver validity is enforced at compile time: the compiler proves that at most one non-z driver is active at any time across the entire elaborated hierarchy.

### Z in Non-Tristate Expressions

If `z` reaches a non-tristate expression at runtime (despite compile-time structural checks), the simulator reports a runtime error (SE-008) and aborts the current test. This includes:

- `z` in an IF or ELIF condition
- `z` in a SELECT/CASE selector
- `z` in a ternary condition
- `z` observed in an `@expect_equal` or `@expect_not_equal` assertion

::: warning
Registers and latches cannot store `z`. Only nets and ports participating in tri-state logic may carry `z`.
:::

## Storage Initialization

All REGISTER, LATCH, and MEM storage initializes with **random but determinate** bits (each bit is `0` or `1`, chosen randomly) at the start of each test case.

Reset values are applied **only** when the RESET signal meets the RESET_ACTIVE condition. Before reset is asserted, reading uninitialized storage yields random bits — exactly as in real hardware.

::: danger Important
A register's declared reset value is **never** applied automatically at instantiation or power-on. Reset values take effect only when the module's reset signal is explicitly asserted during simulation. If a module has no reset port, or if the testbench never asserts reset, all registers retain their random power-on values for the entire simulation. The test must explicitly assert and release reset to bring registers to known values.
:::

The simulator uses a different random seed per test run by default. Use `--seed=0xHEX` for reproducibility.

## Execution Model

### Testbench Execution Flow

A test case executes as a strictly ordered sequence:

1. **Storage randomizes.** All REGISTER, LATCH, and MEM storage is filled with pseudo-random values derived from the simulation seed.
2. **Clocks hold at `1'b0`.** No clock has toggled yet.
3. **`@setup` executes.** Explicit initial wire values are applied. Any wire not assigned in `@setup` is implicitly initialized to `0`.
4. **Combinational logic settles.** Inputs propagate through the DUT, all ASYNCHRONOUS paths evaluate to a fixed point.
5. **Test sequence begins.** Each directive executes in order: `@clock`, `@update`, `@expect_equal`, `@expect_not_equal`, `@expect_tristate`.
6. **Completion.** After the last directive, the test result is reported.

### Timing Guarantees

- **Setup-hold compliance:** `@update` applies stimulus between clock edges, never coincident with an active edge.
- **No race conditions:** Stimulus changes (`@update`) and clock advancement (`@clock`) are never simultaneous.
- **Combinational settling:** After every `@clock` and `@update`, all combinational paths are fully evaluated before the next directive executes.

### Determinism Guarantee

**Given identical input source files and seed, all simulation output — assertion results, pass/fail verdicts, and diagnostic messages — must be bit-identical across runs, across platforms, and across conforming implementations.**

There is no implementation-defined ordering, no thread-dependent scheduling, and no platform-dependent evaluation. Every aspect of simulation is fully determined by the source text and the seed value:

- Storage randomization is derived solely from the seed via a specified PRNG algorithm.
- Statement evaluation order is fixed by the source-level ordering of directives, blocks, and assignments.
- Combinational settling follows a deterministic iteration order.
- Assertion comparison uses exact bit-vector equality with no floating-point arithmetic.

If two conforming simulators produce different results for the same source and seed, at least one has a bug.

### Multiple Clocks

When a testbench declares multiple clocks, `@clock` advances only the named clock. Other clocks remain held at their current level. To advance multiple clocks, issue separate `@clock` directives.

::: warning Limitation
Because clocks advance sequentially rather than concurrently, the testbench model does not reproduce real-world phase relationships between clock domains. Two `@clock` directives issued back-to-back do not overlap in time — the second clock is frozen while the first advances. This model verifies **functional correctness** (data integrity, handshake compliance) but not cycle-accurate multi-clock timing. For timing-accurate multi-clock verification with realistic phase alignment, use [`@simulation`](/reference-manual/simulation) with defined clock periods.
:::

```text
CLOCK {
    fast_clk;
    slow_clk;
}

TEST "Multi-clock domain" {
    // ...
    @clock(fast_clk, cycle=4)   // 4 fast cycles (slow_clk frozen)
    @clock(slow_clk, cycle=1)   // 1 slow cycle (fast_clk frozen)
    @clock(fast_clk, cycle=4)   // 4 more fast cycles
}
```

## Failure Reporting

### Assertion Failure

```text
FAIL: "Increment and Wrap"
  @expect_equal(result, 8'h00) failed at testbench.jz:25
  Cycle: 256
  Expected: 8'h00
  Actual:   8'hFF

  Relevant State:
    counter.count = 8'hFF
    counter.carry = 1'b0
```

### Runtime Error

```text
RUNTIME ERROR: "Counter increments after reset release"
  z observed at testbench.jz:30
  Cycle: 12
  Signal: result
  Value:  8'hzz
  Bits [7:0] are z
```

### Test Summary

```text
Testbench: counter
  PASS: "Reset clears counter"
  PASS: "Single increment"
  FAIL: "Increment and Wrap"

Results: 2 passed, 1 failed, 3 total
Seed: 0xDEADBEEF
```

## Rules Summary

| Rule | Description |
| --- | --- |
| TB-001 | `@testbench` must name a module that is in scope |
| TB-002 | All module ports must be connected in the `@new` directive |
| TB-003 | Port width in `@new` must match the module's declared port width |
| TB-004 | `@new` right-hand side must be a testbench `CLOCK` or `WIRE` identifier |
| TB-005 | `@setup` must appear exactly once per `TEST` block, after `@new` and before any other directives |
| TB-006 | Any wire not assigned in `@setup` is implicitly initialized to `0` |
| TB-007 | `@clock` clock identifier must refer to a declared `CLOCK` |
| TB-008 | `@clock` cycle count must be a positive integer |
| TB-009 | `@update` may only assign testbench `WIRE` identifiers |
| TB-010 | `@update` may not assign clock signals |
| TB-011 | `@expect_equal` / `@expect_not_equal` value width must match signal width |
| TB-012 | Testbench must contain at least one `TEST` block |
| TB-013 | Each `TEST` must contain exactly one `@new` instantiation |
| TB-014 | `@expect` directives may not appear inside `@setup` or `@update` blocks |
| TB-015 | If a CLOCK is not assigned in `@setup`, it implicitly initializes to `1'b0` |
| TB-016 | All assignments within a single `@update` block are evaluated simultaneously |
| TB-017 | All storage (REGISTER, LATCH, MEM) initializes with random bits |
| TB-018 | `@expect` values must be fully determined (no `x` or `z` bits) |
| TB-019 | Observing a signal containing `z` in an assertion is a runtime error |
| TB-020 | A file may not contain both RTL definitions and verification constructs |
| TB-021 | `@expect_tristate` asserts all bits of the signal are `z` |
| SE-001 | Combinational logic must converge within 100 delta cycles |
| SE-008 | `z` reaching a non-tristate expression (IF condition, SELECT selector, ternary condition) is a runtime error |
| SE-009 | Given identical source and seed, all output must be bit-identical across runs |

## CLI Usage

```bash
jz-hdl test_file.jz --test                       # Run all testbenches
jz-hdl test_file.jz --test --seed=0xCAFE         # Fixed random seed
jz-hdl test_file.jz --test --verbose              # Show all results (pass and fail)
```

Files containing `@testbench` blocks must be run with `--test`. Using `--lint` or `--simulate` on a testbench file will produce an error.

### Exit Codes

| Code | Meaning |
| --- | --- |
| 0 | All tests passed |
| 1 | One or more test failures |
| 2 | Runtime error (z observation, combinational loop, etc.) |
| 3 | Compile error in testbench file |

## Complete Example

### Module Under Test

```jz
@module counter
    PORT {
        IN  [1] clk;
        IN  [1] rst_n;
        OUT [8] count;
    }

    REGISTER {
        cnt [8] = 8'h00;
    }

    ASYNCHRONOUS {
        count <= cnt;
    }

    SYNCHRONOUS(CLK=clk RESET=rst_n RESET_ACTIVE=Low) {
        cnt <= cnt + 8'h01;
    }
@endmod
```

### Testbench

```jz
@testbench counter
    CLOCK {
        clk;
    }

    WIRE {
        rst_n [1];
        count [8];
    }

    TEST "Reset holds counter at zero" {
        @new dut counter {
            clk [1] = clk;
            rst_n [1] = rst_n;
            count [8] = count;
        }

        @setup {
            rst_n <= 1'b0;
        }

        // Hold reset for 5 cycles
        @clock(clk, cycle=5)

        // Counter should remain at reset value
        @expect_equal(count, 8'h00)
    }

    TEST "Counter increments after reset release" {
        @new dut counter {
            clk [1] = clk;
            rst_n [1] = rst_n;
            count [8] = count;
        }

        @setup {
            rst_n <= 1'b0;
        }

        // Hold reset for 3 cycles
        @clock(clk, cycle=3)
        @expect_equal(count, 8'h00)

        // Release reset
        @update {
            rst_n <= 1'b1;
        }

        // Advance 1 cycle - first increment
        @clock(clk, cycle=1)
        @expect_equal(count, 8'h01)

        // Advance 4 more cycles
        @clock(clk, cycle=4)
        @expect_equal(count, 8'h05)
    }

    TEST "Counter wraps from FF to 00" {
        @new dut counter {
            clk [1] = clk;
            rst_n [1] = rst_n;
            count [8] = count;
        }

        @setup {
            rst_n <= 1'b0;
        }

        // Reset then release
        @clock(clk, cycle=1)
        @update {
            rst_n <= 1'b1;
        }

        // Advance 255 cycles to reach 0xFF
        @clock(clk, cycle=255)
        @expect_equal(count, 8'hFF)

        // One more cycle - should wrap
        @clock(clk, cycle=1)
        @expect_equal(count, 8'h00)
    }
@endtb
```

### Memory Testbench

```jz
@module ram
    PORT {
        IN  [1] clk;
        IN  [1] rst_n;
        IN  [8] addr;
        IN  [8] wdata;
        IN  [1] wen;
        OUT [8] rdata;
    }

    MEM {
        mem [8] [256] = 8'h00 {
            OUT rd SYNC;
            IN  wr;
        };
    }

    SYNCHRONOUS(CLK=clk RESET=rst_n RESET_ACTIVE=Low) {
        mem.rd.addr <= addr;
        IF (wen) {
            mem.wr[addr] <= wdata;
        }
    }

    ASYNCHRONOUS {
        rdata <= mem.rd.data;
    }
@endmod

@testbench ram
    CLOCK {
        clk;
    }

    WIRE {
        rst_n [1];
        addr [8];
        wdata [8];
        wen [1];
        rdata [8];
    }

    TEST "Write then read" {
        @new dut ram {
            clk [1] = clk;
            rst_n [1] = rst_n;
            addr [8] = addr;
            wdata [8] = wdata;
            wen [1] = wen;
            rdata [8] = rdata;
        }

        @setup {
            rst_n <= 1'b0;
            wen <= 1'b0;
            addr <= 8'h00;
            wdata <= 8'h00;
        }

        // Reset
        @clock(clk, cycle=2)
        @update { rst_n <= 1'b1; }

        // Write 0xAB to address 0x10
        @update {
            addr <= 8'h10;
            wdata <= 8'hAB;
            wen <= 1'b1;
        }
        @clock(clk, cycle=1)

        // Disable write, set read address
        @update {
            wen <= 1'b0;
            addr <= 8'h10;
        }
        @clock(clk, cycle=1)

        // SYNC read has 1-cycle latency; data available now
        @expect_equal(rdata, 8'hAB)
    }
@endtb
```
