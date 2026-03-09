/**
 * @file sim_engine.h
 * @brief Public API for testbench simulation.
 */

#ifndef JZ_SIM_ENGINE_H
#define JZ_SIM_ENGINE_H

#include "../../include/ast.h"
#include "../../include/ir.h"
#include "../../include/diagnostic.h"

/**
 * @brief Run all testbenches found in the AST against the IR design.
 *
 * @param root        Root AST node containing @testbench nodes.
 * @param design      IR design with compiled modules.
 * @param seed        Random seed for register initialization.
 * @param verbose     Non-zero for verbose output.
 * @param diagnostics Diagnostic list for errors.
 * @param filename    Source filename for error messages.
 * @return 0 if all tests pass, non-zero otherwise.
 */
int jz_sim_run_testbenches(const JZASTNode *root,
                           const IR_Design *design,
                           uint32_t seed,
                           int verbose,
                           JZDiagnosticList *diagnostics,
                           const char *filename);

/**
 * @brief Run all simulations found in the AST against the IR design.
 *
 * Time-based simulation with auto-toggling clocks and VCD output.
 *
 * @param root        Root AST node containing @simulation nodes.
 * @param design      IR design with compiled modules.
 * @param seed        Random seed for register initialization.
 * @param verbose     Non-zero for verbose output.
 * @param diagnostics Diagnostic list for errors.
 * @param filename    Source filename for error messages.
 * @param vcd_path    Output VCD file path.
 * @return 0 on success, non-zero otherwise.
 */
int jz_sim_run_simulations(const JZASTNode *root,
                            const IR_Design *design,
                            uint32_t seed,
                            int verbose,
                            JZDiagnosticList *diagnostics,
                            const char *filename,
                            const char *vcd_path);

#endif /* JZ_SIM_ENGINE_H */
