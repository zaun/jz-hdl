#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cli_modes.h"
#include "cli_frontend.h"
#include "ir_builder.h"
#include "ir_serialize.h"
#include "verilog_backend.h"
#include "rtlil_backend.h"
#include "sim/sim_engine.h"
#include "sim/sim_waveform.h"

/**
 * Build IR if not already built, apply tristate transform and division guard.
 * Common preamble for most backend modes.
 * Returns 0 on success, non-zero on failure.
 */
static int ensure_ir(JZCompiler *compiler, const JZCLIOptions *opts) {
    clock_t phase_t0;
    int rc = 0;

    if (!compiler->ir_root) {
        phase_t0 = clock();
        if (jz_ir_build_design(compiler->ast_root,
                               &compiler->ir_root,
                               &compiler->ir_arena,
                               &compiler->diagnostics) != 0) {
            if (opts->verbose) fprintf(stderr, "[verbose] ir_build: %.1f ms (failed)\n", jz_cli_elapsed_ms(phase_t0));
            return 1;
        }
        if (opts->verbose) fprintf(stderr, "[verbose] ir_build: %.1f ms\n", jz_cli_elapsed_ms(phase_t0));
    }

    if (compiler->ir_root && opts->tristate_default != 0) {
        phase_t0 = clock();
        compiler->ir_root->tristate_default = (IR_TristateDefault)opts->tristate_default;
        if (jz_ir_tristate_transform(compiler->ir_root,
                                      &compiler->ir_arena,
                                      &compiler->diagnostics) != 0) {
            rc = 1;
        }
        if (opts->verbose) fprintf(stderr, "[verbose] tristate_transform: %.1f ms\n", jz_cli_elapsed_ms(phase_t0));
    }

    if (rc == 0 && compiler->ir_root) {
        phase_t0 = clock();
        jz_ir_div_guard_check(compiler->ir_root, &compiler->diagnostics);
        if (opts->verbose) fprintf(stderr, "[verbose] div_guard_check: %.1f ms\n", jz_cli_elapsed_ms(phase_t0));
    }

    return rc;
}

/** Emit constraint files (SDC, XDC, PCF, CST) if requested. */
static int emit_constraints(JZCompiler *compiler, const JZCLIOptions *opts) {
    int rc = 0;
    if (opts->sdc_filename) {
        if (jz_emit_sdc_constraints(compiler->ir_root,
                                    opts->sdc_filename,
                                    &compiler->diagnostics,
                                    opts->input_filename) != 0) {
            rc = 1;
        }
    }
    if (rc == 0 && opts->xdc_filename) {
        if (jz_emit_xdc_constraints(compiler->ir_root,
                                    opts->xdc_filename,
                                    &compiler->diagnostics,
                                    opts->input_filename) != 0) {
            rc = 1;
        }
    }
    if (rc == 0 && opts->pcf_filename) {
        if (jz_emit_pcf_constraints(compiler->ir_root,
                                    opts->pcf_filename,
                                    &compiler->diagnostics,
                                    opts->input_filename) != 0) {
            rc = 1;
        }
    }
    if (rc == 0 && opts->cst_filename) {
        if (jz_emit_cst_constraints(compiler->ir_root,
                                    opts->cst_filename,
                                    &compiler->diagnostics,
                                    opts->input_filename) != 0) {
            rc = 1;
        }
    }
    return rc;
}

int jz_cli_run_lint_ir(JZCompiler *compiler, const JZCLIOptions *opts) {
    clock_t phase_t0 = clock();
    if (jz_ir_build_design(compiler->ast_root,
                           &compiler->ir_root,
                           &compiler->ir_arena,
                           &compiler->diagnostics) == 0 &&
        compiler->ir_root) {
        if (opts->verbose) fprintf(stderr, "[verbose] ir_build (lint): %.1f ms\n", jz_cli_elapsed_ms(phase_t0));
        int rc = 0;
        if (opts->tristate_default != 0) {
            phase_t0 = clock();
            compiler->ir_root->tristate_default = (IR_TristateDefault)opts->tristate_default;
            if (jz_ir_tristate_transform(compiler->ir_root,
                                          &compiler->ir_arena,
                                          &compiler->diagnostics) != 0) {
                rc = 1;
            }
            if (opts->verbose) fprintf(stderr, "[verbose] tristate_transform (lint): %.1f ms\n", jz_cli_elapsed_ms(phase_t0));
        }
        if (rc == 0) {
            phase_t0 = clock();
            jz_ir_div_guard_check(compiler->ir_root, &compiler->diagnostics);
            if (opts->verbose) fprintf(stderr, "[verbose] div_guard_check: %.1f ms\n", jz_cli_elapsed_ms(phase_t0));
        }
        return rc;
    } else {
        if (opts->verbose) fprintf(stderr, "[verbose] ir_build (lint): %.1f ms (failed)\n", jz_cli_elapsed_ms(phase_t0));
        return 0; /* Lint-mode IR build failure is not fatal. */
    }
}

int jz_cli_run_ir_emit(JZCompiler *compiler, const JZCLIOptions *opts) {
    int rc = ensure_ir(compiler, opts);
    if (rc != 0) return rc;

    const char *ir_target = opts->output_filename ? opts->output_filename : "-";
    if (jz_ir_write_json(compiler->ir_root,
                          ir_target,
                          &compiler->diagnostics,
                          opts->input_filename) != 0) {
        return 1;
    }
    return 0;
}

int jz_cli_run_verilog(JZCompiler *compiler, const JZCLIOptions *opts) {
    int rc = ensure_ir(compiler, opts);
    if (rc != 0) return rc;
    if (!compiler->ir_root) return 1;

    clock_t phase_t0 = clock();
    const char *verilog_target = opts->output_filename ? opts->output_filename : "-";
    if (jz_emit_verilog(compiler->ir_root,
                        verilog_target,
                        &compiler->diagnostics,
                        opts->input_filename) != 0) {
        return 1;
    }
    if (opts->verbose) fprintf(stderr, "[verbose] emit_verilog: %.1f ms\n", jz_cli_elapsed_ms(phase_t0));

    return emit_constraints(compiler, opts);
}

int jz_cli_run_rtlil(JZCompiler *compiler, const JZCLIOptions *opts) {
    int rc = ensure_ir(compiler, opts);
    if (rc != 0) return rc;
    if (!compiler->ir_root) return 1;

    const char *rtlil_target = opts->output_filename ? opts->output_filename : "-";
    if (jz_emit_rtlil(compiler->ir_root,
                       rtlil_target,
                       &compiler->diagnostics,
                       opts->input_filename) != 0) {
        return 1;
    }

    return emit_constraints(compiler, opts);
}

int jz_cli_run_test(JZCompiler *compiler, const JZCLIOptions *opts) {
    int rc = ensure_ir(compiler, opts);
    if (rc != 0) return rc;
    if (!compiler->ir_root) return 1;

    uint32_t seed = opts->test_seed_set ? opts->test_seed : 0;
    int sim_rc = jz_sim_run_testbenches(compiler->ast_root,
                                         compiler->ir_root,
                                         seed,
                                         opts->verbose,
                                         &compiler->diagnostics,
                                         opts->input_filename);
    return sim_rc != 0 ? 1 : 0;
}

int jz_cli_run_simulate(JZCompiler *compiler, const JZCLIOptions *opts) {
    int rc = ensure_ir(compiler, opts);
    if (rc != 0) return rc;
    if (!compiler->ir_root) return 1;

    SimWaveFormat wave_format = opts->sim_format_jzw ? SIM_WAVE_JZW
                              : opts->sim_format_fst ? SIM_WAVE_FST
                              : SIM_WAVE_VCD;
    const char *ext = opts->sim_format_jzw ? ".jzw"
                    : opts->sim_format_fst ? ".fst" : ".vcd";

    uint32_t seed = opts->test_seed_set ? opts->test_seed : 0;

    /* Determine output path: -o flag or default to <input>.<ext> */
    char default_wave[1024];
    const char *sim_output = opts->output_filename;
    if (!sim_output) {
        const char *dot = strrchr(opts->input_filename, '.');
        size_t base_len = dot ? (size_t)(dot - opts->input_filename) : strlen(opts->input_filename);
        if (base_len > sizeof(default_wave) - 5) base_len = sizeof(default_wave) - 5;
        memcpy(default_wave, opts->input_filename, base_len);
        memcpy(default_wave + base_len, ext, strlen(ext) + 1);
        sim_output = default_wave;
    }

    int sim_rc = jz_sim_run_simulations(compiler->ast_root,
                                         compiler->ir_root,
                                         seed,
                                         opts->verbose,
                                         &compiler->diagnostics,
                                         opts->input_filename,
                                         sim_output,
                                         wave_format,
                                         opts->num_jitter > 0 ? opts->jitter_configs : NULL,
                                         opts->num_jitter,
                                         opts->num_drift > 0 ? opts->drift_configs : NULL,
                                         opts->num_drift);
    return sim_rc != 0 ? 1 : 0;
}
