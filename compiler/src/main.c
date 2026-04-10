#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "compiler.h"
#include "rules.h"
#include "chip_data.h"
#include "path_security.h"
#include "parser.h"
#include "sem_driver.h"
#include "lsp.h"
#include "ir.h"

#include "cli_options.h"
#include "cli_frontend.h"
#include "cli_modes.h"

/* Global verbose flag for timing diagnostics. */
int jz_verbose = 0;

int main(int argc, char **argv) {
    if (argc < 2) {
        jz_cli_print_usage(argv[0]);
        return 1;
    }

    /* Check for --lsp mode early — it takes over the process. */
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--lsp") == 0) {
            return jz_lsp_run();
        }
    }

    JZCLIOptions opts;
    int parse_rc = jz_cli_parse_options(&opts, argc, argv);
    if (parse_rc < 0) return 0;  /* --help or --version */
    if (parse_rc > 0) return 1;  /* parse error */

    /* Handle --chip-info (standalone, no input file needed). */
    if (opts.chip_info) {
        FILE *chip_out = stdout;
        if (opts.output_filename) {
            chip_out = fopen(opts.output_filename, "w");
            if (!chip_out) {
                fprintf(stderr, "Failed to open output file '%s'\n", opts.output_filename);
                return 1;
            }
        }
        int chip_rc = 0;
        if (!opts.chip_info_id) {
            size_t count = jz_chip_builtin_count();
            if (count == 0) {
                fprintf(chip_out, "No built-in chips available.\n");
            } else {
                fprintf(chip_out, "Built-in CHIP IDs:\n");
                for (size_t i = 0; i < count; ++i) {
                    const char *id = jz_chip_builtin_id(i);
                    if (id) {
                        fprintf(chip_out, "  %s\n", id);
                    }
                }
            }
        } else if (jz_chip_print_info(opts.chip_info_id, chip_out) != 0) {
            fprintf(stderr, "CHIP \"%s\" not found in built-in database.\n", opts.chip_info_id);
            chip_rc = 1;
        }
        if (chip_out != stdout) {
            fclose(chip_out);
        }
        return chip_rc;
    }

    /* Handle --lint-rules (standalone). */
    if (opts.lint_rules) {
        jz_rules_print_all(stdout);
        return 0;
    }

    /* Default mode when a file is provided without an explicit mode flag. */
    if (!opts.mode && opts.input_filename) {
        opts.mode = "--lint";
    }

    if (!opts.input_filename && opts.mode && strcmp(opts.mode, "--lint-rules") != 0) {
        jz_cli_print_usage(argv[0]);
        return 1;
    }

    /* Classify the primary mode. */
    int is_ast_mode = (opts.mode && strcmp(opts.mode, "--ast") == 0);
    int is_ir_mode = (opts.mode && strcmp(opts.mode, "--ir") == 0);
    int is_verilog_mode = (opts.mode && (strcmp(opts.mode, "--verilog") == 0 || strcmp(opts.mode, "--emit-verilog") == 0));
    int is_rtlil_mode = (opts.mode && (strcmp(opts.mode, "--rtlil") == 0 || strcmp(opts.mode, "--emit-rtlil") == 0));
    int is_lint_mode = !is_ast_mode && !is_ir_mode && !is_verilog_mode && !is_rtlil_mode;

    if (!is_verilog_mode && !is_rtlil_mode && (opts.sdc_filename || opts.xdc_filename || opts.pcf_filename || opts.cst_filename)) {
        fprintf(stderr, "--sdc/--xdc/--pcf/--cst may only be used with --verilog or --rtlil\n");
        return 1;
    }
    if (opts.tristate_default != 0 && is_ast_mode) {
        fprintf(stderr, "--tristate-default may not be used with --ast\n");
        return 1;
    }
    int alias_report_active = opts.alias_report && is_lint_mode;
    int memory_report_active = opts.memory_report && is_lint_mode;
    int tristate_report_active = opts.tristate_report && is_lint_mode;

    /* Only one consumer of -o is supported at a time. */
    int use_output_for_ast = is_ast_mode && (opts.output_filename != NULL);
    int use_output_for_ir = is_ir_mode && (opts.output_filename != NULL);
    int use_output_for_alias = alias_report_active && (opts.output_filename != NULL);
    int use_output_for_memory = memory_report_active && (opts.output_filename != NULL);
    int use_output_for_tristate = tristate_report_active && (opts.output_filename != NULL);
    int use_output_for_verilog = is_verilog_mode && (opts.output_filename != NULL);
    int output_consumers = (use_output_for_ast ? 1 : 0) +
                           (use_output_for_ir ? 1 : 0) +
                           (use_output_for_alias ? 1 : 0) +
                           (use_output_for_memory ? 1 : 0) +
                           (use_output_for_tristate ? 1 : 0) +
                           (use_output_for_verilog ? 1 : 0);
    if (output_consumers > 1) {
        fprintf(stderr, "-o is ambiguous with the given mode flags; use it with only one of --ast, --ir, --verilog, --alias-report, --memory-report, or --tristate-report.\n");
        return 1;
    }

    JZCompilerMode cmode = JZ_COMPILER_MODE_LINT;
    int print_ast_json = 0;
    int emit_ir = 0;

    if (is_ast_mode) {
        cmode = JZ_COMPILER_MODE_AST;
        print_ast_json = 1;
    } else if (is_verilog_mode || is_rtlil_mode) {
        cmode = JZ_COMPILER_MODE_VERILOG;
    } else {
        cmode = JZ_COMPILER_MODE_LINT;
        if (is_ir_mode) {
            emit_ir = 1;
        }
    }

    /* Prepare optional output streams. */
    FILE *ast_out = NULL;
    FILE *alias_out = NULL;
    FILE *memory_out = NULL;
    FILE *tristate_out = NULL;

    if (is_ast_mode) {
        if (opts.output_filename) {
            ast_out = fopen(opts.output_filename, "w");
            if (!ast_out) {
                fprintf(stderr, "Failed to open AST output file '%s'\n", opts.output_filename);
                return 1;
            }
        } else {
            ast_out = stdout;
        }
    }

    if (alias_report_active) {
        if (opts.output_filename) {
            alias_out = fopen(opts.output_filename, "w");
            if (!alias_out) {
                fprintf(stderr, "Failed to open alias-report output file '%s'\n", opts.output_filename);
                if (ast_out && ast_out != stdout) {
                    fclose(ast_out);
                }
                return 1;
            }
        } else {
            alias_out = stdout;
        }
    }

    if (memory_report_active) {
        if (opts.output_filename) {
            memory_out = fopen(opts.output_filename, "w");
            if (!memory_out) {
                fprintf(stderr, "Failed to open memory-report output file '%s'\n", opts.output_filename);
                if (ast_out && ast_out != stdout) {
                    fclose(ast_out);
                }
                if (alias_out && alias_out != stdout) {
                    fclose(alias_out);
                }
                return 1;
            }
        } else {
            memory_out = stdout;
        }
    }

    if (tristate_report_active) {
        if (opts.output_filename) {
            tristate_out = fopen(opts.output_filename, "w");
            if (!tristate_out) {
                fprintf(stderr, "Failed to open tristate-report output file '%s'\n", opts.output_filename);
                if (ast_out && ast_out != stdout) {
                    fclose(ast_out);
                }
                if (alias_out && alias_out != stdout) {
                    fclose(alias_out);
                }
                if (memory_out && memory_out != stdout) {
                    fclose(memory_out);
                }
                return 1;
            }
        } else {
            tristate_out = stdout;
        }
    }

    JZCompiler compiler;
    jz_compiler_init(&compiler, cmode);

    /* Initialize path security sandbox. */
    if (opts.input_filename) {
        jz_path_security_init(opts.input_filename);
        if (opts.allow_absolute_paths) {
            jz_path_security_set_allow_absolute(1);
        }
        if (opts.allow_traversal) {
            jz_path_security_set_allow_traversal(1);
        }
        for (size_t i = 0; i < opts.sandbox_root_count; i++) {
            jz_path_security_add_root(opts.sandbox_roots[i]);
        }
    }

    if (alias_report_active && opts.input_filename) {
        FILE *out = alias_out ? alias_out : stdout;
        jz_sem_enable_alias_report(out, "JZ-HDL 1.0", opts.input_filename);
    }
    if (memory_report_active && opts.input_filename) {
        FILE *out = memory_out ? memory_out : stdout;
        jz_sem_enable_memory_report(out, "JZ-HDL 1.0", opts.input_filename);
    }
    if (tristate_report_active && opts.input_filename) {
        FILE *out = tristate_out ? tristate_out : stdout;
        jz_sem_enable_tristate_report(out, "JZ-HDL 1.0", opts.input_filename);
    }

    if (opts.tristate_default != 0) {
        jz_sem_set_tristate_default(1);
        opts.show_info = 1;
    }

    /* Set global verbose flag for use by IR builder and other subsystems. */
    jz_verbose = opts.verbose;

    int rc = 0;
    clock_t phase_t0;

    /* Always run the front end for AST, lint, IR, Verilog, and test modes. */
    phase_t0 = clock();
    rc = jz_cli_run_frontend(&compiler, opts.input_filename, print_ast_json, ast_out, opts.test_mode, opts.simulate_mode, opts.verbose);
    if (opts.verbose) fprintf(stderr, "[verbose] frontend (total): %.1f ms\n", jz_cli_elapsed_ms(phase_t0));

    /* In lint mode, build IR (if no errors) to run the division guard check. */
    if (is_lint_mode && rc == 0 &&
        !jz_diagnostic_has_severity(&compiler.diagnostics, JZ_SEVERITY_ERROR) &&
        compiler.ast_root != NULL && !compiler.ir_root) {
        rc = jz_cli_run_lint_ir(&compiler, &opts);
    }

    /* IR output mode. */
    if (emit_ir && rc == 0 &&
        !jz_diagnostic_has_severity(&compiler.diagnostics, JZ_SEVERITY_ERROR) &&
        compiler.ast_root != NULL) {
        rc = jz_cli_run_ir_emit(&compiler, &opts);
    }

    /* Verilog output mode. */
    if (cmode == JZ_COMPILER_MODE_VERILOG && !is_rtlil_mode &&
        rc == 0 &&
        !jz_diagnostic_has_severity(&compiler.diagnostics, JZ_SEVERITY_ERROR) &&
        compiler.ast_root != NULL) {
        rc = jz_cli_run_verilog(&compiler, &opts);
    }

    /* RTLIL output mode. */
    if (is_rtlil_mode &&
        rc == 0 &&
        !jz_diagnostic_has_severity(&compiler.diagnostics, JZ_SEVERITY_ERROR) &&
        compiler.ast_root != NULL) {
        rc = jz_cli_run_rtlil(&compiler, &opts);
    }

    /* Testbench mode. */
    if (opts.test_mode &&
        rc == 0 &&
        !jz_diagnostic_has_severity(&compiler.diagnostics, JZ_SEVERITY_ERROR) &&
        compiler.ast_root != NULL) {
        rc = jz_cli_run_test(&compiler, &opts);
    }

    /* Simulation mode. */
    if (opts.simulate_mode &&
        rc == 0 &&
        !jz_diagnostic_has_severity(&compiler.diagnostics, JZ_SEVERITY_ERROR) &&
        compiler.ast_root != NULL) {
        rc = jz_cli_run_simulate(&compiler, &opts);
    }

    /* Apply warning policy before printing or computing final exit status. */
    JZWarningPolicy policy;
    policy.warn_as_error = opts.warn_as_error;
    policy.groups = (opts.group_override_count > 0) ? opts.group_overrides : NULL;
    policy.group_count = opts.group_override_count;
    jz_diagnostic_apply_warning_policy(&compiler.diagnostics, &policy);

    /* Print buffered diagnostics for non-AST modes, or on failure. */
    if (cmode != JZ_COMPILER_MODE_AST || rc != 0) {
        jz_diagnostic_print_all(&compiler.diagnostics, stderr, opts.use_color, opts.input_filename, opts.show_info, opts.show_explain);
    }

    /* Any error-severity diagnostic forces a non-zero exit code. */
    if (rc == 0 &&
        jz_diagnostic_has_severity(&compiler.diagnostics, JZ_SEVERITY_ERROR)) {
        rc = 1;
    }

    jz_compiler_dispose(&compiler);

    if (ast_out && ast_out != stdout) {
        fclose(ast_out);
    }
    if (alias_out && alias_out != stdout) {
        fclose(alias_out);
    }
    if (memory_out && memory_out != stdout) {
        fclose(memory_out);
    }
    if (tristate_out && tristate_out != stdout) {
        fclose(tristate_out);
    }

    /* Release path security state. */
    jz_path_security_cleanup();

    /* Release any filename strings retained for imported modules. */
    jz_parser_free_imported_filenames();

    return rc;
}
