# JZ-HDL

JZ-HDL is a hardware correctness language and compiler. Unlike traditional HDLs (Verilog, VHDL), JZ-HDL is designed from physical hardware invariants upward, making entire classes of silicon bugs impossible to express.

The compiler enforces strict rules around clocks, resets, driver ownership, width safety, and clock domain crossing (CDC) at compile time — catching errors that would otherwise become silent silicon bugs.

## Quick Example

```
@module blinker
    PORT {
        IN  [1] clk;
        OUT [1] led;
    }

    REGISTER {
        counter [27] = 27'b0;
    }

    ASYNCHRONOUS {
        led <= ~counter[26];
    }

    SYNCHRONOUS (CLK=clk RESET=reset RESET_ACTIVE=Low RESET_TYPE=Clocked) {
        counter <= counter + 27'b1;
    }
@endmod
```

## Key Features

- **Single-driver enforcement** — each net has exactly one active driver at any time, eliminating contention bugs
- **Compile-time width checking** — all bit widths are explicit and verified, no implicit truncation or extension
- **Clock domain crossing analysis** — CDC violations are caught at compile time
- **Deterministic reset semantics** — reset type, polarity, and behavior are declared explicitly
- **Tristate proof engine** — verifies that tristate buses have proper non-overlapping enable conditions
- **Bus abstraction** — structured bus interfaces with source/target ports and automatic interconnect
- **Memory primitives** — first-class support for register files, block RAM, and ROM with explicit read/write port semantics
- **Cycle-accurate simulator** — built-in simulation engine with VCD waveform output
- **FPGA backend** — generates synthesizable Verilog targeting Gowin, iCE40, and ECP5 FPGAs

## Building

Requires CMake and a C99 compiler.

```bash
# Configure
cmake -S compiler -B compiler/build

# Build
cmake --build compiler/build
```

## Usage

```bash
$ ./jz-hdl
Usage: ./jz-hdl JZ_FILE --lint [--warn-as-error] [--color] [--info] [--explain] [--Wno-group=NAME] [-o OUT_FILE]
       ./jz-hdl JZ_FILE --verilog [-o OUT_FILE] [--sdc SDC_FILE] [--xdc XDC_FILE] [--pcf PCF_FILE] [--cst CST_FILE] [--tristate-default=GND|VCC]
       ./jz-hdl JZ_FILE --rtlil [-o OUT_FILE] [--sdc SDC_FILE] [--xdc XDC_FILE] [--pcf PCF_FILE] [--cst CST_FILE] [--tristate-default=GND|VCC]
       ./jz-hdl JZ_FILE --alias-report [-o OUT_FILE]
       ./jz-hdl JZ_FILE --memory-report [-o OUT_FILE]
       ./jz-hdl JZ_FILE --tristate-report [-o OUT_FILE]
       ./jz-hdl JZ_FILE --ast [-o OUT_FILE]
       ./jz-hdl JZ_FILE --ir [-o OUT_FILE] [--tristate-default=GND|VCC]
       ./jz-hdl JZ_FILE --test [--verbose] [--seed=0xHEX]
       ./jz-hdl JZ_FILE --simulate [-o WAVEFORM_FILE] [--vcd] [--fst] [--jzw] [--verbose] [--seed=0xHEX]
       ./jz-hdl --chip-info [CHIP_ID] [-o OUT_FILE]
       ./jz-hdl --lint-rules
       ./jz-hdl --lsp
       ./jz-hdl --help
       ./jz-hdl --version

Path security options:
  --sandbox-root=<dir>     Add permitted root directory for file access
  --allow-absolute-paths   Allow absolute paths in @import / @file()
  --allow-traversal        Allow '..' directory traversal in paths
```

## Testing

```bash
# Run all validation tests (316 test cases)
bash compiler/tests/run_validation.sh

# Run CTest suite
cmake -S compilerz-hdl -B compiler/build -DBUILD_TESTING=ON
ctest --test-dir compiler/build
```

## Examples

The `examples/` directory contains complete projects ranging from simple counters to a full SoC with a RISC-V CPU, BIOS, kernel, SD card controller, and DVI terminal. Each example has a Makefile and can be built and flashed to supported FPGA boards.

## Documentation

Full documentation is available at [jz-hdl.github.io/jz-hdl](https://jz-hdl.github.io/jz-hdl/) and in the `docs/` directory, covering the language specification, type system, module system, memory semantics, simulation, and testbench authoring.

### Specification

The `specification/` directory contains the authoritative language specifications. These are the source of truth for all compiler behavior:

- `jz-hdl-specification.md` — Core language semantics, syntax, and rules
- `simulation-specification.md` — Cycle-accurate simulator behavior
- `testbench-specification.md` — Testbench language and execution model

The compiler implements these specifications directly. When compiler behavior disagrees with the spec, the compiler is wrong and should be fixed — the spec is not modified to match implementation quirks.

## Editor Support

The `vscode-ext/` directory contains a VS Code extension providing:

- **Syntax highlighting** for `.jz` files
- **LSP integration** — diagnostics, hover info, keyword completion, and go-to-definition via `jz-hdl --lsp`

### Settings

| Setting | Default | Description |
| --- | --- | --- |
| `jz-hdl.binaryPath` | `""` (system PATH) | Path to the `jz-hdl` binary |
| `jz-hdl.lsp.enabled` | `true` | Enable/disable the language server |

### Installation

```bash
cd vscode-ext
npm install
npm run compile
```

Then in VS Code: **Extensions** → **...** → **Install from VSIX** or use **Developer: Install Extension from Location** and select the `vscode-ext/` directory.
