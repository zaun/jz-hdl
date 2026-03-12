/**
 * @file sim_engine.h
 * @brief Public API for testbench simulation.
 */

#ifndef JZ_SIM_ENGINE_H
#define JZ_SIM_ENGINE_H

#include "../../include/ast.h"
#include "../../include/ir.h"
#include "../../include/diagnostic.h"
#include "sim_waveform.h"

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
 * @brief Per-clock jitter configuration (from --jitter CLI flag).
 */
typedef struct SimJitterConfig {
    const char *clock_name;   /* clock identifier from CLOCK block */
    uint64_t    pp_ps;        /* peak-to-peak jitter in picoseconds */
} SimJitterConfig;

/**
 * @brief Per-clock drift configuration (from --drift CLI flag).
 */
typedef struct SimDriftConfig {
    const char *clock_name;   /* clock identifier from CLOCK block */
    double      max_ppm;      /* maximum drift in parts per million */
} SimDriftConfig;

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
 * @param output_path Output waveform file path.
 * @param format      Waveform format (SIM_WAVE_VCD or SIM_WAVE_FST).
 * @param jitter_configs Array of per-clock jitter configs (may be NULL).
 * @param num_jitter   Number of entries in jitter_configs.
 * @param drift_configs Array of per-clock drift configs (may be NULL).
 * @param num_drift    Number of entries in drift_configs.
 * @return 0 on success, non-zero otherwise.
 */
int jz_sim_run_simulations(const JZASTNode *root,
                            const IR_Design *design,
                            uint32_t seed,
                            int verbose,
                            JZDiagnosticList *diagnostics,
                            const char *filename,
                            const char *output_path,
                            SimWaveFormat format,
                            const SimJitterConfig *jitter_configs,
                            int num_jitter,
                            const SimDriftConfig *drift_configs,
                            int num_drift);

#endif /* JZ_SIM_ENGINE_H */
