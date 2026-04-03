#ifndef JZ_HDL_CLI_MODES_H
#define JZ_HDL_CLI_MODES_H

#include "compiler.h"
#include "cli_options.h"

/**
 * @file cli_modes.h
 * @brief Backend mode dispatch (lint-IR, IR emit, Verilog, RTLIL, test, simulate).
 */

/**
 * Run the lint-mode IR build and division guard check.
 * Only called when in lint mode and frontend succeeded with no errors.
 */
int jz_cli_run_lint_ir(JZCompiler *compiler, const JZCLIOptions *opts);

/**
 * Run IR serialization to JSON (--ir mode).
 */
int jz_cli_run_ir_emit(JZCompiler *compiler, const JZCLIOptions *opts);

/**
 * Run Verilog backend (--verilog mode).
 */
int jz_cli_run_verilog(JZCompiler *compiler, const JZCLIOptions *opts);

/**
 * Run RTLIL backend (--rtlil mode).
 */
int jz_cli_run_rtlil(JZCompiler *compiler, const JZCLIOptions *opts);

/**
 * Run testbench execution (--test mode).
 */
int jz_cli_run_test(JZCompiler *compiler, const JZCLIOptions *opts);

/**
 * Run simulation (--simulate mode).
 */
int jz_cli_run_simulate(JZCompiler *compiler, const JZCLIOptions *opts);

#endif /* JZ_HDL_CLI_MODES_H */
