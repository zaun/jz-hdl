---
title: Projects
lang: en-US

layout: doc
outline: deep
---

# Projects

## Project overview

A JZ-HDL project (@project ... @endproj) is the chip-level integration unit. It collects:
- project-wide compile-time configuration (CONFIG),
- clock & pin declarations (CLOCKS, IN_PINS, OUT_PINS, INOUT_PINS),
- physical pin mapping (MAP),
- optional blackbox declarations usable anywhere in the project,
- a single root/top instantiation via `@top`.

A project:
- defines how modules are bound to board pins and clocks,
- imports module/blackbox definitions from source files,
- provides global configuration values (CONFIG) accessible to all module instantiations as `CONFIG.NAME`,
- may include `@global` blocks (sized literal constants) that are usable as runtime values across all modules.

Every design must contain exactly one `@project` block (per compilation unit) and exactly one `@top` declaration inside it.

---

## Project canonical form

Canonical skeleton:

```text
@project(CHIP=<chipid>) <project_name>

  @import "<path/to/library.jzhdl>";

  CONFIG {
    <config_id> = <nonnegative_integer_expression>;
    ...
  }

  CLOCKS {
    <clock_name> = { period=<ns>, edge=[Rising|Falling] };
  }

  IN_PINS { ... }
  OUT_PINS { ... }
  INOUT_PINS { ... }

  MAP { ... }

  @blackbox <name> { PORT { ... } }
  @blackbox ...

  @global <global_name>
    <const_id> = <width>'<base><value>;
    ...
  @endglob

  @top <top_module_name> {
    IN    [<width>] <port_name> = <pin_expr | _>;
    OUT   [<width>] <port_name> = <pin_expr | _>;
    INOUT [<width>] <port_name> = <pin_expr | _>;
  }

@endproj
```

- `CHIP=<chipid>` is optional; defaults to `GENERIC`. The chip ID is case-insensitive and may be an identifier or string literal.
- If `CHIP` is not `GENERIC`, the compiler loads `<chipid>.json` from the project directory (falling back to its built-in database). The JSON file describes supported memory configurations, clock generators, and constraints.

Key rules:
- `@import` directives (optional) must appear immediately after `@project` header.
- One `CONFIG` block maximum; `CONFIG` names are visible across modules via `CONFIG.NAME`.
- All pins declared in IN/OUT/INOUT blocks must be mapped in `MAP`.
- Exactly one `@top` instantiation must be present; the referenced module or blackbox must be defined in the project or imported.

---

## @import

Purpose
- Import module and blackbox definitions from other .jzhdl source files into the current project namespace.

Placement and rules
- Only valid inside `@project`, and must appear immediately after the `@project` header and before CONFIG/CLOCKS/PIN blocks.
- Imported files must not contain their own `@project`/`@endproj` pair.
- Import paths are resolved and normalized. Re-importing the same resolved file (directly or transitively) is a compile error (IMPORT_FILE_MULTIPLE_TIMES).
- After imports, all module and blackbox definitions form a single project-wide namespace and must be unique.

Semantics
- Modules and blackboxes from imports are available to `@top` and `@new` instantiations.
- Imported definitions are not automatically parameterized; overrides happen at instantiation via `OVERRIDE`.

Path normalization
- Import paths use the OS canonical path resolution (POSIX `realpath`, Windows `_fullpath`) to normalize symbolic links, `.`/`..`, and redundant separators.
- On case-insensitive filesystems, canonical resolution normalizes case for correct dedup.
- Diagnostic messages report the original user-supplied path, not the resolved canonical path.

Path security (sandbox)
- All user-specified file paths (`@import`, `@file()`) are subject to sandbox restrictions.
- By default: absolute paths and `..` traversal are forbidden. Resolved paths must be within the sandbox root (directory of the input file).
- CLI flags: `--sandbox-root=<dir>`, `--allow-absolute-paths`, `--allow-traversal`.
- See the [formal reference](/reference-manual/formal-reference/projects#path-security-sandbox) for full details.

Error conditions
- File not found (tool-specific handling)
- Imported file contains `@project` block → compile error
- Duplicate module/blackbox name across imports or local definitions → compile error
- Absolute path without `--allow-absolute-paths` → `PATH_ABSOLUTE_FORBIDDEN`
- Path traversal (`..`) without `--allow-traversal` → `PATH_TRAVERSAL_FORBIDDEN`
- Path outside sandbox → `PATH_OUTSIDE_SANDBOX`

---

## CONFIG (project-wide constants)

Purpose
- Provide compile-time constants visible in all modules via `CONFIG.NAME`.

Syntax
```text
CONFIG {
  NAME = <nonnegative_integer_expression>;   // numeric
  NAME = "<string>";                          // string
  ...
}
```

Key properties
- Evaluated at compile time
- Numeric entries: used in compile-time integer contexts (widths, MEM depths, OVERRIDE expressions)
- String entries: used in string contexts such as `@file()` path arguments (e.g., `@file(CONFIG.FIRMWARE)`)
- Not usable as runtime values in ASYNCHRONOUS or SYNCHRONOUS expressions
- No shadowing between CONFIG and module-local CONST (use `CONFIG.NAME` to refer to project-wide values)

Rules & validation
- Only one CONFIG block per project
- Numeric values must be nonnegative integers; expressions may reference earlier CONFIG names (forward references forbidden)
- String values are double-quoted string literals
- Circular dependencies among CONFIG entries → compile error
- Using a string CONFIG where a number is expected → `CONST_STRING_IN_NUMERIC_CONTEXT`
- Using a numeric CONFIG where a string is expected → `CONST_NUMERIC_IN_STRING_CONTEXT`

Common uses
- XLEN, ADDR_WIDTH, feature enables, sizes shared across modules
- File paths for MEM initialization (firmware images, lookup tables)

Error examples
- Using CONFIG in runtime context → CONST_USED_WHERE_FORBIDDEN / CONFIG_USED_WHERE_FORBIDDEN
- Missing CONFIG name referenced in a module instantiation width → CONFIG_USE_UNDECLARED
- String CONFIG used as width → CONST_STRING_IN_NUMERIC_CONTEXT

---

## CLOCKS

Purpose
- Declare named clocks (period and edge) for timing analysis and to validate top‑level clock pin bindings.

Syntax
```text
CLOCKS {
  clk_name = { period=<positive_number>, edge=[Rising|Falling] };
}
```

Rules
- `clk_name` must be declared as an input pin (IN_PINS or CLOCKS-matched)
- Period is in nanoseconds; must be > 0
- Edge defaults to Rising if omitted
- Names must be unique within CLOCKS

Validation
- Top-level pins referencing clock names must exist and be width `[1]`
- Duplicate clock name → error
- Period ≤ 0 or invalid edge specifier → error

---

## CLOCK_GEN (clock generators)

Purpose
- Define on-chip clock generation from PLLs, DLLs, or CLKDIVs, deriving new clocks from declared input clocks.

Syntax
```text
CLOCK_GEN {
  <gen_type> {
    IN   <input_name>  <signal>;
    OUT  <output_name> <clock_signal>;
    WIRE <output_name> <signal>;
    ...
    CONFIG {
      <param_name> = <param_value>;
      ...
    }
  };
  ...
}
```

`<gen_type>` is one of: `PLL`, `DLL`, `CLKDIV`, `OSC`, `BUF`, or a numbered variant (e.g., `PLL2`). Numbered variants represent alternate implementations of the same function — for example, a chip may have both a `PLL` (integer dividers) and a `PLL2` (fractional dividers).

Semantics
- `<input_name>`: The input selector name as defined by the chip data (e.g., `REF_CLK`, `CE`). The bound signal must refer to a clock declared in CLOCKS with a period (or a clock generated by another generator in the same CLOCK_GEN block for chaining).
- `<output_name>`: The output selector name as defined by the chip (e.g., `BASE`, `PHASE`, `DIV`, `DIV3`, `LOCK`). The keyword (`OUT` vs `WIRE`) must match the output's `is_clock` property in the chip data.
- **`OUT`** declares a **clock output**: must refer to a clock declared in CLOCKS **without** a period (the compiler computes it). Must not be declared as an IN_PIN and must not already be driven by another CLOCK_GEN. Only valid for outputs with `is_clock: true` in the chip data.
- **`WIRE`** declares a **non-clock output** (e.g., PLL lock indicator): must **not** be declared in the CLOCKS block. The compiler automatically declares the signal as a wire in the generated output. Only valid for outputs with `is_clock: false` in the chip data (e.g., `LOCK`).
- CONFIG parameters are chip-specific and validated against the chip's generator definition.
- Not all outputs or parameters need to be used; omitting them is acceptable.
- A single CLOCK_GEN block may contain multiple generators (e.g., a PLL followed by a CLKDIV that divides the PLL output).

### Generator types

- **PLL** (Phase-Locked Loop): Frequency synthesis with VCO, feedback divider, and multiple outputs (BASE, PHASE, DIV, DIV3). Used for generating arbitrary frequencies from a reference clock. Numbered variants (e.g., **PLL2**) represent alternate PLL implementations on the same chip — for example, a variant with fractional dividers or additional outputs. CONFIG parameters and available outputs are chip-specific.
- **DLL** (Delay-Locked Loop): Phase alignment without frequency multiplication. Used for clock de-skew.
- **CLKDIV** (Clock Divider): Simple fixed-ratio frequency division. Divides an input clock by a fixed ratio (e.g., 2, 3.5, 4, 5). No VCO or feedback — simpler and lower-power than PLL. Produces a single BASE output. The `MODE` CONFIG parameter selects the chip-specific variant (e.g., `local` for IO-logic dividers, `global` for fabric-wide dividers) when multiple variants are available.
- **OSC** (Internal Oscillator): On-chip RC oscillator that generates a clock without any external reference. Does not require an IN clock. The output frequency is determined by chip-specific CONFIG parameters (e.g., `FREQ_DIV`, `DEVICE`). Produces a single BASE output.
- **BUF** (Clock Buffer): Buffers an input clock onto a global clock network without frequency change. Used to route external clocks through dedicated clock routing resources.

Validation
- Input clock must have a declared period in CLOCKS (or be driven by a prior generator in the same block).
- Output clocks must not have a period in CLOCKS (it is derived).
- VCO frequency (for PLL; derived from reference clock and divider parameters) must be within the chip's valid range.
- CONFIG parameter values must match their declared type: integer parameters reject decimal values (e.g., `CLKFBOUT_MULT = 5.0` is an error; use `5`), while double parameters accept both (e.g., `CLKFBOUT_MULT_F = 5.125`).
- CLOCK_GEN chaining is only permitted if the chip explicitly supports it.

### PLL example
```text
CLOCKS {
  sys_clk = { period=37.04 };   // 27 MHz external clock
  fast_clk;                      // derived by PLL (no period)
}

CLOCK_GEN {
  PLL {
    IN REF_CLK sys_clk;
    OUT BASE fast_clk;
    WIRE LOCK pll_lock;          // non-clock output: PLL lock indicator

    CONFIG {
      IDIV = 3;
      FBDIV = 50;
      ODIV = 2;
    }
  };
}
```

The `WIRE LOCK pll_lock` line declares the PLL's lock indicator as a wire signal. This signal is typically referenced by differential output pins via the `reset` attribute (see [PIN blocks](#pin-blocks)).

### CLKDIV example
```text
CLOCKS {
  serial_clk = { period=4.0 };  // 250 MHz serial clock
  pixel_clk;                     // 50 MHz pixel clock (serial / 5)
}

CLOCK_GEN {
  CLKDIV {
    IN REF_CLK serial_clk;
    OUT BASE pixel_clk;

    CONFIG {
      DIV_MODE = 5;
      MODE = local;
    };
  };
}
```

### OSC example
```text
CLOCKS {
  osc_clk;  // generated by internal oscillator
}

CLOCK_GEN {
  OSC {
    OUT BASE osc_clk;

    CONFIG {
      FREQ_DIV = 10;
    };
  };
}
```

### Chaining PLL + CLKDIV
A common pattern is to use a PLL to synthesize a high-frequency clock, then a CLKDIV to derive a lower-frequency clock from it. Both generators appear in the same CLOCK_GEN block:
```text
CLOCKS {
  SCLK       = { period=37.037 };  // 27 MHz crystal
  serial_clk;                       // 126 MHz (from PLL)
  pixel_clk;                        // 25.2 MHz (serial / 5)
}

CLOCK_GEN {
  PLL {
    IN REF_CLK SCLK;
    OUT BASE serial_clk;
    WIRE LOCK pll_lock;
    CONFIG {
      IDIV = 2;
      FBDIV = 13;
      ODIV = 8;
    };
  };
  CLKDIV {
    IN REF_CLK serial_clk;
    OUT BASE pixel_clk;
    CONFIG {
      DIV_MODE = 5;
    };
  };
}
```

---

## PIN BLOCKS

Three blocks declare the chip's electrical I/O: IN_PINS, OUT_PINS and INOUT_PINS. Each entry names a logical pin and its electrical properties.

### Pin attributes

| Attribute | Required | Values | Default | Description |
|-----------|----------|--------|---------|-------------|
| `standard` | Yes (all pins) | See I/O standards below | — | Electrical I/O standard |
| `drive` | Yes (OUT, INOUT) | Integer or fractional (e.g., `8`, `3.5`) | — | Drive strength in milliamps |
| `mode` | No | `SINGLE`, `DIFFERENTIAL` | `SINGLE` | Single-ended or differential signaling |
| `term` | No | `ON`, `OFF` | `OFF` | On-die termination resistor. Always emitted explicitly in constraints (e.g., `IN_TERM NONE` in XDC when `OFF`). |
| `pull` | No | `UP`, `DOWN`, `NONE` | `NONE` | Pull-up/pull-down resistor |
| `fclk` | No | clock name | — | Serialization clock for differential output (fast clock) |
| `pclk` | No | clock name | — | Parallel clock for differential output (pixel/data clock) |
| `reset` | No | signal name | — | Serializer reset signal for differential output (typically a CLOCK_GEN `LOCK` output) |

### I/O standards

**Single-ended standards** (`mode=SINGLE` or default):
`LVTTL`, `LVCMOS33`, `LVCMOS25`, `LVCMOS18`, `LVCMOS15`, `LVCMOS12`, `PCI33`, `SSTL25_I`, `SSTL25_II`, `SSTL18_I`, `SSTL18_II`, `SSTL15`, `SSTL135`, `HSTL18_I`, `HSTL18_II`, `HSTL15_I`, `HSTL15_II`

**Differential standards** (`mode=DIFFERENTIAL`):
`LVDS25`, `LVDS33`, `BLVDS25`, `EXT_LVDS25`, `TMDS33`, `RSDS`, `MINI_LVDS`, `PPDS`, `SUB_LVDS`, `SLVS`, `LVPECL33`, `DIFF_SSTL25_I`, `DIFF_SSTL25_II`, `DIFF_SSTL18_I`, `DIFF_SSTL18_II`, `DIFF_SSTL15`, `DIFF_SSTL135`, `DIFF_HSTL18_I`, `DIFF_HSTL18_II`, `DIFF_HSTL15_I`, `DIFF_HSTL15_II`

### Syntax examples

```text
IN_PINS {
  clk = { standard=LVCMOS33 };
  button[2] = { standard=LVCMOS18 };
}

OUT_PINS {
  led[6] = { standard=LVCMOS18, drive=8 };
  TMDS_CLK = { mode=DIFFERENTIAL, standard=LVDS25, drive=3.5, fclk=serial_clk, pclk=pixel_clk, reset=pll_lock };
  TMDS_DATA[3] = { mode=DIFFERENTIAL, standard=LVDS25, drive=3.5, fclk=serial_clk, pclk=pixel_clk, reset=pll_lock };
}

INOUT_PINS {
  data_bus[8] = { standard=LVCMOS33, drive=8 };
  EDID_DAT = { standard=LVCMOS33, drive=4, pull=UP };
  EDID_CLK = { standard=LVCMOS33, drive=4, pull=UP };
}
```

### Rules

- Pin names must be unique across all PIN blocks
- Bus syntax `[N]` declares a multi-bit pin; width must be positive integer
- `standard` required for all pins; `drive` required for OUT and INOUT pins
- For buses, MAP must map each bit individually
- `mode` must be consistent with `standard`: differential standards require `mode=DIFFERENTIAL`, single-ended standards require `mode=SINGLE` (or omit `mode`)
- `term` is valid only for `mode=DIFFERENTIAL` or for single-ended SSTL/HSTL standards (which support on-die termination)
- `pull` is not valid on `OUT_PINS` (outputs drive the pin, they cannot be pulled)

### Validation errors

- Declaring a pin in multiple pin blocks
- Missing or invalid `drive` on OUT/INOUT pins
- Bus width ≤ 0 → error
- `mode=DIFFERENTIAL` with a single-ended standard (or vice versa) → `PIN_MODE_STANDARD_MISMATCH`
- `pull=UP` or `pull=DOWN` on an OUT_PINS entry → `PIN_PULL_ON_OUTPUT`
- `term=ON` on a standard that doesn't support termination → `PIN_TERM_INVALID_FOR_STANDARD`

### Differential serialization attributes

When `mode=DIFFERENTIAL` is used on an output pin, three additional attributes are required:

- **`fclk`** (fast clock): The high-speed serialization clock. Must be an integer multiple of `pclk` matching the chip's serializer ratio (e.g., 5x for a 10:1 serializer with DDR).
- **`pclk`** (parallel clock): The parallel data clock at which the module produces data.
- **`reset`** (serializer reset): The signal used to reset the serializer after power-on or clock stabilization. Typically references the PLL `LOCK` output declared via `WIRE LOCK` in the `CLOCK_GEN` block. **The compiler automatically inverts this signal**: the serializer is held in reset while the lock signal is low (PLL not locked) and released when it goes high (PLL locked).

Both `fclk` and `pclk` must reference clocks declared in `CLOCKS` or generated by `CLOCK_GEN`. `reset` must reference a valid signal name — typically a `CLOCK_GEN` `LOCK` wire output.

### Notes

- CLOCKS entries should reference an IN_PINS clock pin name
- Pin physical mapping is performed in MAP block; missing mapping is an error
- Fractional drive values (e.g., `3.5`) are supported for standards that use them

---

## MAP (physical pin mapping)

Purpose
- Bind logical top-level pin names to physical board/package pins.

### Single-ended syntax
```text
MAP {
  clk = 52;
  led[0] = 10;
  data_bus[7] = GPIO_27;
}
```

### Differential syntax

Differential pins (`mode=DIFFERENTIAL`) use the `{ P=<id>, N=<id> }` syntax to specify both the positive and negative physical pin:

```text
MAP {
  TMDS_CLK = { P=33, N=34 };
  TMDS_DATA[0] = { P=35, N=36 };
  TMDS_DATA[1] = { P=37, N=38 };
  TMDS_DATA[2] = { P=39, N=40 };
}
```

### Rules

- Every declared pin (from IN/OUT/INOUT) must have corresponding MAP entries
- Bus bits must be mapped individually
- Map entries must reference declared pins; mapping undeclared pins → error
- Duplicate assignment of the same physical pin must be validated (allowed only if tri-state by design)
- Differential pins must use `{ P=<id>, N=<id> }` syntax; single-ended pins must use scalar syntax
- Both P and N physical pins are checked for duplicate physical location conflicts
- P and N must be different physical pins

### Validation errors

- Pin declared but not mapped → compile error
- Mapped pin not declared → compile error
- Duplicate mappings → warning or error (tool-dependent)
- Differential pin with scalar MAP → `MAP_DIFF_EXPECTED_PAIR`
- Single-ended pin with `{ P, N }` MAP → `MAP_SINGLE_UNEXPECTED_PAIR`
- Differential MAP missing P or N → `MAP_DIFF_MISSING_PN`
- P and N are the same physical pin → `MAP_DIFF_SAME_PIN`

---

## BUS (physical pin mapping)

Purpose
- Provide a reusable, project‑level description of a grouped set of signals (a "bus") and a compact way for modules to instantiate and connect those buses.
- Let module authors declare a bus-shaped port and have the tool automatically resolve each member signal's direction based on the module's role (initiator vs responder).

Project-level BUS definition

Syntax (inside @project):
```text
BUS <bus_id> {
  IN    [<width>]  <signal>;
  OUT   [<width>]  <signal>;
  INOUT [<width>]  <signal>;
  ...
}
```

Notes
- The bus_id is a project-wide identifier and must be unique.
- Directions (IN/OUT/INOUT) in a BUS block are written from the SOURCE (initiator) perspective.
- Widths may be literals or CONFIG expressions (e.g., CONFIG.XLEN).

---

## Chip ID JSON

When `CHIP` is not `GENERIC`, the compiler loads a chip data JSON file that describes:
- **memory**: Array of objects describing supported memory configurations (DISTRIBUTED, BLOCK types with width/depth combinations).
- **clock_gen**: Array of objects describing clock generators (PLL, PLL2, DLL, CLKDIV, OSC, BUF) with parameters, derived expressions, outputs, and constraints.

The JSON schema includes:
- `memory[].type`: `DISTRIBUTED` or `BLOCK`
- `memory[].configurations[]`: `{ width, depth }` pairs
- `clock_gen[].type`: `pll`, `pll2`, `dll`, `clkdiv`, `osc`, `buf`, or numbered variants
- `clock_gen[].parameters`: Named parameters with type (`int` or `double`), min, max constraints
- `clock_gen[].derived`: Computed values (e.g., VCO frequency) with expressions and valid ranges
- `clock_gen[].outputs`: Named outputs (e.g., `BASE`, `PHASE`, `DIV`, `DIV3`, `LOCK`) with frequency expressions
- `clock_gen[].feedback_wire`: Optional feedback wire base name for automatic internal feedback path wiring

All periods are in nanoseconds; all derived frequency expressions output MHz. Expressions are evaluated in a restricted expression language (identifiers + math, no side effects).

The compiler validates MEM declarations and CLOCK_GEN configurations against the chip data. If validation fails, a compile error is emitted with the specific constraint that was violated.

---

## @blackbox

Purpose
- Declare opaque/externally-supplied modules (hard macros, vendor IP, encrypted cores) inside the project.

Syntax
```text
@blackbox <name> {
  PORT {
    IN    [<width>] a;
    OUT   [<width>] b;
    INOUT [<width>] bus;
  }
}
```

Rules
- Blackbox declarations must appear inside `@project` (before `@top`)
- Name shares same namespace as modules; must be unique project-wide
- No body: cannot contain WIRE, REGISTER, ASYNCHRONOUS, MEM, etc.
- Blackboxes may include CONST blocks; interpreted as local configuration for that IP

Instantiation
- Use `@new` inside modules to instantiate a blackbox, same as module instantiation. OVERRIDE is allowed and passed through.

Example with OVERRIDE:
```text
@module parent
  CONST { DATA_WIDTH = 16; }

  @new mem0 memory_wrapper {
    OVERRIDE {
      WORD_WIDTH = 32;
      DEPTH = 512;
    }
    IN  [1]  clk = clk;
    IN  [9]  addr = mem_addr;
    IN  [32] wr_data = mem_wdata;
    IN  [1]  wr_en = mem_wen;
    OUT [32] rd_data = mem_rdata;
  }
@endmod
```

The `OVERRIDE` block replaces the child module's `CONST` values before elaboration. This allows a single module definition to be instantiated with different configurations.

Validation
- Port widths and directions must match at instantiation
- Missing blackbox declaration for `@new` → error

Example use cases
- BRAM/DSP primitives, PLLs, vendor SERDES, encrypted IP

---

## @top (top-level module instantiation)

Purpose
- Define the design root and bind module ports to physical pins or `_` (no-connect).

Syntax
```text
@top <top_module_name> {
  IN  [1] clk = hw_clk;
  OUT [6] led = leds;
  INOUT [8] data = data_bus;
  OUT [1] debug = _;
}
```

Rules
- Exactly one @top block required inside a project
- The top module name must refer to a module or blackbox defined in project or imported files
- All module ports must be listed in @top (omitting a port is a compile error)
- For each port:
  - If name is `_`, the port is intentionally not connected to board pins
  - Otherwise the name must resolve to a declared pin in a compatible PIN block (IN → IN_PINS/INOUT_PINS/CLOCKS, OUT → OUT_PINS/INOUT_PINS, INOUT → INOUT_PINS)
- Port widths must exactly match module port widths

Validation errors
- Top module not found
- Port not listed
- Port width mismatch
- Port direction vs pin category mismatch
- Pin not mapped in MAP

### Logical-to-Physical Expansion

For `@top` port bindings, the compiler handles width matching between module ports and physical pins:
- `width(port_name)` must equal `width(pin_name)` regardless of pin mode (`SINGLE` or `DIFFERENTIAL`).
- For `mode=DIFFERENTIAL` pins, the compiler internally handles the 2-pin (P/N) expansion per bit. From the module's perspective, the port is a single-ended signal of the declared width.

---

## @global (project-wide sized literals)

Purpose
- Provide named, sized literal constants accessible as runtime values in any module: instruction encodings, bit-pattern constants, opcodes, etc.

Syntax
```text
@global <namespace_name>
  NAME = <width>'<base><value>;
  OTHER = 8'b1010_1100;
@endglob
```

Key properties
- Each constant is a sized literal with explicit width (follows literal rules from Section 2.1)
- Referenced in modules as `namespace_name.NAME`
- Global constants are values (unlike CONFIG which are compile-time integers)
- Usable anywhere a value may appear: RHS of assignments, conditions, intrinsic operator arguments, etc.

Rules & validation
- The namespace name must be unique across project compilation
- Each const_id must be unique inside its `@global` block
- Values must obey literal rules: size, base, intrinsic-width ≤ declared width (overflow → error)
- `x` bits are allowed in binary literals but must follow Observability Rule (may not propagate into observable sinks)

Use cases
- Instruction encodings, opcode masks, default packet fields, fixed bit patterns used at runtime

Errors
- Duplicate global namespace
- Duplicate constant identifier in the same block
- Invalid literal syntax or overflow
- Attempting to assign to a global constant (read-only)

Example
```text
@global ISA
  INST_ADD   = 17'b0110011_0000000_000;
  INST_SUB   = 17'b0110011_0100000_000;
@endglob
```
Usage inside a module:
```text
IF (opcode == ISA.INST_ADD) { ... }
```

---

## @check (compile-time assertions)

Purpose
- Validate compile-time constraints (widths, config sanity, address ranges) during elaboration. A `@check` failure aborts compilation. `@check` never generates hardware or runtime behavior.

Syntax
```text
@check (<constant_expression>, <string_message>);
```

Rules
- The expression must evaluate to a constant, nonnegative integer at compile time.
- If the expression is zero (false), compilation fails with the message.
- Allowed operands: integer literals, `CONST`, `CONFIG.NAME`, `clog2()`, comparisons, logical operators.
- Forbidden: any runtime signal (ports, wires, registers, memory ports, slices).
- `@check` may appear inside `@project` or `@module`, but not inside blocks (ASYNCHRONOUS, SYNCHRONOUS, etc.).

Examples
```text
// Valid: width constraint
CONST { WIDTH = 32; }
@check (WIDTH % 8 == 0, "Width must be a multiple of 8.");

// Valid: project config constraint
@check (CONFIG.DATA_WIDTH >= 8, "Data width must be at least 8.");

// Valid: address width sanity
CONST { DEPTH = 256; ADDR_W = 8; }
@check (ADDR_W == clog2(DEPTH), "Address width does not match depth.");

// Invalid: runtime signal
@check (select == 3, "...");  // ERROR: non-constant expression
```

---

## Validation rules and common errors (project-level)

- Only one `@project` per file; missing `@endproj` → error.
- `@import` files must be unique after path normalization; duplicate import → error.
- `CONFIG` forward reference or circular dependency → compile error.
- `CLOCKS` entries must reference declared IN pins and have width `[1]`.
- Pin conflicts: same pin declared in multiple PIN blocks → error.
- MAP must include entries for every declared pin; unmapped pins → error.
- `@top` must reference an existing module/blackbox and list all its ports.
- Port/pin direction mismatch (e.g., module IN bound to OUT_PINS, module INOUT bound to IN_PINS) → error.
- Duplicate module or blackbox names across imports and local definitions → compile error.
- Invalid `@global` literal (unsized, overflow, invalid digits) → compile error.

---

## Best practices and checklists

Project checklist before compilation
- [ ] All module/blackbox names required by `@top` and `@new` are defined or imported
- [ ] Each declared pin has a MAP entry (bus bits mapped individually)
- [ ] `CONFIG` contains shared sizes (XLEN, ADDR_WIDTH) used by multiple modules
- [ ] Clocks declared in CLOCKS match top-level clock pins (names and widths)
- [ ] `@global` values for opcode masks and encodings are defined and used consistently
- [ ] `@import` paths are normalized and not duplicated across project imports
- [ ] Blackboxes declared for vendor IP; OVERRIDE used at instantiation where needed
- [ ] No port width mismatch between top and module definitions
- [ ] Pin electrical standards and drive strengths set correctly for board hardware

Recommended style
- Use `CONFIG` for integer sizes shared across modules (compile-time); use `@global` for sized bit-patterns used at runtime.
- Keep `@import` lines centralized at top of `@project` for clarity.
- Name clocks clearly (e.g., sys_clk, ref_clk) and document periods in CLOCKS.
- Map pins with clear comments in MAP for board reference.
- Use `_` in `@top` to explicitly ignore unused top ports and avoid accidental omissions.

---

## Examples

Minimal project with @global and imports:
```text
@project example_proj

  @import "cores.jzhdl";

  CONFIG {
    XLEN = 32;
    REG_DEPTH = 32;
  }

  CLOCKS {
    sys_clk = { period=10 };  // 100 MHz
  }

  IN_PINS {
    sys_clk = { standard=LVCMOS33 };
    btn = { standard=LVCMOS18 };
  }

  OUT_PINS {
    leds[4] = { standard=LVCMOS18, drive=8 };
  }

  MAP {
    sys_clk = 1;
    btn = 2;
    leds[0] = 10;
    leds[1] = 11;
    leds[2] = 12;
    leds[3] = 13;
  }

  @blackbox pll_ip {
    PORT { IN [1] clk_in; OUT [1] clk_out; OUT [1] locked; }
  }

  @global ISA
    INST_ADD = 17'b0110011_0000000_000;
    INST_SUB = 17'b0110011_0100000_000;
  @endglob

  @top top_core {
    IN  [1] clk = sys_clk;
    IN  [1] button = btn;
    OUT [4] led = leds;
  }

@endproj
```

Example with differential signaling, CLOCK_GEN with WIRE LOCK, and pull-up resistors (DVI output):
```text
@project dvi_proj

  CLOCKS {
    SCLK       = { period=37.037 };  // 27 MHz crystal
    serial_clk;                       // 185.625 MHz (from PLL)
    pixel_clk;                        // 37.125 MHz (from CLKDIV)
  }

  IN_PINS {
    SCLK = { standard=LVCMOS33 };
  }

  OUT_PINS {
    LED[6]       = { standard=LVCMOS33, drive=8 };
    TMDS_CLK     = { mode=DIFFERENTIAL, standard=LVDS25, drive=3.5, fclk=serial_clk, pclk=pixel_clk, reset=pll_lock };
    TMDS_DATA[3] = { mode=DIFFERENTIAL, standard=LVDS25, drive=3.5, fclk=serial_clk, pclk=pixel_clk, reset=pll_lock };
  }

  INOUT_PINS {
    EDID_DAT = { standard=LVCMOS33, drive=4, pull=UP };
    EDID_CLK = { standard=LVCMOS33, drive=4, pull=UP };
  }

  MAP {
    SCLK = 4;
    LED[0] = 15;
    LED[1] = 16;
    LED[2] = 17;
    LED[3] = 18;
    LED[4] = 19;
    LED[5] = 20;
    TMDS_CLK     = { P=33, N=34 };
    TMDS_DATA[0] = { P=35, N=36 };
    TMDS_DATA[1] = { P=37, N=38 };
    TMDS_DATA[2] = { P=39, N=40 };
    EDID_DAT = 52;
    EDID_CLK = 53;
  }

  CLOCK_GEN {
    PLL {
      IN REF_CLK SCLK;
      OUT BASE serial_clk;       // 185.625 MHz (5x pixel clock)
      WIRE LOCK pll_lock;        // lock indicator → used as serializer reset
      CONFIG {
        IDIV = 7;
        FBDIV = 54;
        ODIV = 4;
      };
    };
    CLKDIV {
      IN REF_CLK serial_clk;
      OUT BASE pixel_clk;        // 185.625 / 5 = 37.125 MHz
      CONFIG {
        DIV_MODE = 5;
      };
    };
  }

  @top dvi_top {
    IN  [1]  clk       = pixel_clk;
    OUT [6]  leds      = LED;
    OUT [10] tmds_clk  = TMDS_CLK;
    OUT [10] tmds_d0   = TMDS_DATA[0];
    OUT [10] tmds_d1   = TMDS_DATA[1];
    OUT [10] tmds_d2   = TMDS_DATA[2];
    INOUT [1] edid_dat = EDID_DAT;
    INOUT [1] edid_clk = EDID_CLK;
  }

@endproj
```

In this example, `WIRE LOCK pll_lock` declares the PLL's lock indicator as a wire. The `reset=pll_lock` attribute on the TMDS pins tells the compiler to hold the serializer in reset until the PLL is locked. The compiler automatically inverts `pll_lock` for the active-low serializer reset.

Example showing CONFIG + top instantiation using CONFIG:
```text
@project param_proj
  CONFIG { XLEN = 64; }
  CLOCKS { clk = { period=5 }; }
  IN_PINS { clk = { standard=LVCMOS33 }; }
  OUT_PINS { out_bus[XLEN] = { standard=LVCMOS33, drive=8 }; }
  MAP { clk = 1; out_bus[0] = 10; /* ... map remaining bits ... */ }
  @top wide_top {
    IN [1] clk = clk;
    OUT [XLEN] out_bus = out_bus;
  }
@endproj
```
