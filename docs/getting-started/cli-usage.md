---
title: CLI Usage
lang: en-US

layout: doc
outline: deep
---

# CLI Usage

## Synopsis

```bash
jz-hdl JZ_FILE --lint [-o OUT_FILE] [--warn-as-error] [--color] [--info] [--Wno-group=NAME]
jz-hdl JZ_FILE --verilog [-o OUT_FILE] [--sdc SDC_FILE] [--xdc XDC_FILE] [--pcf PCF_FILE] [--cst CST_FILE] [--tristate-default=GND|VCC]
jz-hdl JZ_FILE --rtlil [-o OUT_FILE] [--tristate-default=GND|VCC]
jz-hdl JZ_FILE --alias-report [-o OUT_FILE]
jz-hdl JZ_FILE --memory-report [-o OUT_FILE]
jz-hdl JZ_FILE --tristate-report [-o OUT_FILE]
jz-hdl JZ_FILE --ast [-o OUT_FILE]
jz-hdl JZ_FILE --ir [-o OUT_FILE] [--tristate-default=GND|VCC]
jz-hdl JZ_FILE --test [--verbose] [--seed=0xHEX]
jz-hdl JZ_FILE --simulate [-o WAVEFORM_FILE] [--vcd] [--fst] [--verbose] [--seed=0xHEX]
jz-hdl --chip-info [CHIP_ID] [-o OUT_FILE]
jz-hdl JZ_FILE --lint-rules
jz-hdl --help
jz-hdl --version
```

## Modes

| Flag | Description |
| --- | --- |
| `--lint` | Run all semantic checks and report diagnostics. No output files are generated unless `-o` is given. |
| `--verilog` | Compile to Verilog. Optionally emit constraint files (`--sdc`, `--xdc`, `--pcf`, `--cst`). |
| `--rtlil` | Compile to RTLIL (yosys intermediate representation). |
| `--ast` | Emit the parsed AST as JSON. |
| `--ir` | Emit the intermediate representation as JSON. |
| `--test` | Run all `@testbench` blocks in the file. See [Testbench](../reference-manual/testbench.md). |
| `--simulate` | Run all `@simulation` blocks in the file. See [Simulation](../reference-manual/simulation.md). |
| `--alias-report` | Print the net alias resolution report. |
| `--memory-report` | Print the memory inference and mapping report. |
| `--tristate-report` | Print the tri-state driver analysis report. |
| `--chip-info` | Print chip data for the given CHIP_ID (or list all supported chips if no ID given). |
| `--lint-rules` | List all diagnostic rule IDs and their descriptions. |
| `--help` | Print usage information. |
| `--version` | Print the compiler version. |

## Common options

| Option | Description |
| --- | --- |
| `-o OUT_FILE` | Write primary output to `OUT_FILE` instead of stdout. |
| `--color` | Force colored diagnostic output. |
| `--info` | Include informational diagnostics (not just warnings and errors). |
| `--warn-as-error` | Treat all warnings as errors. |
| `--Wno-group=NAME` | Suppress diagnostics in the named group. |

## Constraint file options (with `--verilog`)

| Option | Description |
| --- | --- |
| `--sdc SDC_FILE` | Emit Synopsys Design Constraints. |
| `--xdc XDC_FILE` | Emit Xilinx Design Constraints. |
| `--pcf PCF_FILE` | Emit Physical Constraints File (open-source toolchains). |
| `--cst CST_FILE` | Emit Gowin Constraints File. |

## Path security options

| Option | Description |
| --- | --- |
| `--sandbox-root=<dir>` | Add an additional permitted root directory for `@import` and `@file()` paths. May be specified multiple times. |
| `--allow-absolute-paths` | Allow absolute paths in `@import` and `@file()` directives (disabled by default). |
| `--allow-traversal` | Allow `..` directory traversal in paths (disabled by default). |

By default, the compiler restricts all file paths to be within the directory containing the input file. See [Path Security](/reference-manual/formal-reference/projects#path-security-sandbox) for details.

## Tristate default (with `--verilog`, `--rtlil`, or `--ir`)

| Option | Description |
| --- | --- |
| `--tristate-default=GND` | Convert internal tri-state nets to priority muxes with GND default. |
| `--tristate-default=VCC` | Convert internal tri-state nets to priority muxes with VCC default. |

See [Tristate Default](../reference-manual/tristate-default.md) for details.

## Testbench options (with `--test`)

| Option | Description |
| --- | --- |
| `--verbose` | Print all assertion results (pass and fail), not just failures. |
| `--seed=0xHEX` | 32-bit hex seed for register/memory randomization. Default: `0xDEADBEEF`. |

### Exit codes

| Code | Meaning |
| --- | --- |
| 0 | All tests passed |
| 1 | One or more test failures |
| 2 | Runtime error (x/z observation, combinational loop, etc.) |
| 3 | Compile error in testbench file |

## Simulation options (with `--simulate`)

| Option | Description |
| --- | --- |
| `-o WAVEFORM_FILE` | Output waveform file path. Default: `<input_basename>.vcd`. |
| `--vcd` | Force VCD output format (default). |
| `--fst` | Force FST output format (not yet supported). |
| `--verbose` | Print tick resolution, clock periods, and `@run`/`@update` events. |
| `--seed=0xHEX` | 32-bit hex seed for register/memory randomization. Default: `0xDEADBEEF`. |

## Examples

```bash
# Check a design for errors
jz-hdl design.jz --lint --color

# Check with full info-level output
jz-hdl design.jz --lint --info --color

# Compile to Verilog
jz-hdl design.jz --verilog -o design.v

# Compile to Verilog with tristate elimination
jz-hdl design.jz --verilog -o design.v --tristate-default=GND

# Compile to RTLIL for yosys
jz-hdl design.jz --rtlil -o design.il

# Compile with Gowin constraint output
jz-hdl design.jz --verilog -o design.v --cst design.cst

# Dump the AST for debugging
jz-hdl design.jz --ast -o design_ast.json

# View alias resolution
jz-hdl design.jz --alias-report

# View memory inference report
jz-hdl design.jz --memory-report

# View tristate analysis
jz-hdl design.jz --tristate-report

# List supported chips
jz-hdl --chip-info

# View chip details
jz-hdl --chip-info GW2AR-18

# Run testbenches
jz-hdl test_counter.jz --test

# Run testbenches with verbose output and fixed seed
jz-hdl test_counter.jz --test --verbose --seed=0xCAFE

# Run a simulation
jz-hdl sim_fifo.jz --simulate

# Run simulation with explicit output path
jz-hdl sim_fifo.jz --simulate -o fifo_waves.vcd

# Run simulation with verbose output and fixed seed
jz-hdl sim_fifo.jz --simulate --verbose --seed=0x1234
```
