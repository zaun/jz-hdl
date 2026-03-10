#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ast.h"
#include "ast_json.h"
#include "lexer.h"
#include "parser.h"
#include "util.h"
#include "compiler.h"
#include "sem_driver.h"
#include "ir_builder.h"
#include "ir_serialize.h"
#include "verilog_backend.h"
#include "rtlil_backend.h"
#include "rules.h"
#include "version.h"
#include "chip_data.h"
#include "template_expand.h"
#include "repeat_expand.h"
#include "sim/sim_engine.h"
#include "path_security.h"

/* Global verbose flag for timing diagnostics. */
int jz_verbose = 0;

static void print_usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s JZ_FILE --lint [--warn-as-error] [--color] [--info] [--explain] [--Wno-group=NAME] [-o OUT_FILE]\n"
            "       %s JZ_FILE --verilog [-o OUT_FILE] [--sdc SDC_FILE] [--xdc XDC_FILE] [--pcf PCF_FILE] [--cst CST_FILE] [--tristate-default=GND|VCC]\n"
            "       %s JZ_FILE --rtlil [-o OUT_FILE] [--tristate-default=GND|VCC]\n"
            "       %s JZ_FILE --alias-report [-o OUT_FILE]\n"
            "       %s JZ_FILE --memory-report [-o OUT_FILE]\n"
            "       %s JZ_FILE --tristate-report [-o OUT_FILE]\n"
            "       %s JZ_FILE --ast [-o OUT_FILE]\n"
            "       %s JZ_FILE --ir [-o OUT_FILE] [--tristate-default=GND|VCC]\n"
            "       %s JZ_FILE --test [--verbose] [--seed=0xHEX]\n"
            "       %s JZ_FILE --simulate [-o WAVEFORM_FILE] [--vcd] [--fst] [--verbose] [--seed=0xHEX]\n"
            "       %s --chip-info [CHIP_ID] [-o OUT_FILE]\n"
            "       %s --lint-rules\n"
            "       %s --help\n"
            "       %s --version\n"
            "\n"
            "Path security options:\n"
            "  --sandbox-root=<dir>     Add permitted root directory for file access\n"
            "  --allow-absolute-paths   Allow absolute paths in @import / @file()\n"
            "  --allow-traversal        Allow '..' directory traversal in paths\n",
            prog, prog, prog, prog, prog, prog, prog, prog, prog, prog, prog, prog, prog, prog);
}

static void print_version(void) {
    fprintf(stdout, "%s\n", JZ_HDL_VERSION_STRING);
}

static double elapsed_ms(clock_t start) {
    return (double)(clock() - start) / CLOCKS_PER_SEC * 1000.0;
}

static int run_frontend(JZCompiler *compiler,
                         const char *filename,
                         int print_ast_json,
                         FILE *ast_out,
                         int test_mode,
                         int simulate_mode,
                         int verbose)
{
    clock_t t0, t1;

    t0 = clock();
    size_t size = 0;
    char *source = jz_read_entire_file(filename, &size);
    if (!source) {
        JZLocation loc = { filename, 1, 1 };
        jz_diagnostic_report(&compiler->diagnostics, loc, JZ_SEVERITY_ERROR,
                             "IO001", "failed to read source file");
        return 1;
    }

    /* Expand @repeat N ... @end blocks before lexing */
    char *expanded = jz_repeat_expand(source, filename, &compiler->diagnostics);
    if (!expanded) {
        free(source);
        return 1;
    }
    free(source);
    source = expanded;

    JZTokenStream tokens;
    size_t diag_before = compiler->diagnostics.buffer.len;
    if (jz_lex_source(filename, source, &tokens, &compiler->diagnostics) != 0) {
        /* If the lexer did not report a specific rule-based diagnostic, emit a
         * generic fallback.
         */
        if (compiler->diagnostics.buffer.len == diag_before) {
            JZLocation loc = { filename, 1, 1 };
            jz_diagnostic_report(&compiler->diagnostics, loc, JZ_SEVERITY_ERROR,
                                 "LEX000", "lexing failed");
        }
        free(source);
        return 1;
    }
    t1 = clock();
    if (verbose) fprintf(stderr, "[verbose] lex: %.1f ms\n", elapsed_ms(t0));

    t0 = clock();
    size_t diag_before_parse = compiler->diagnostics.buffer.len;
    JZASTNode *ast = jz_parse_file(filename, &tokens, &compiler->diagnostics);
    if (!ast) {
        /* If the parser did not emit any rule-based diagnostics, provide a
         * generic fallback code so validation still has an anchor.
         */
        if (compiler->diagnostics.buffer.len == diag_before_parse) {
            JZLocation loc = { filename, 1, 1 };
            jz_diagnostic_report(&compiler->diagnostics, loc, JZ_SEVERITY_ERROR,
                                 "PARSE000", "parsing failed");
        }
        jz_token_stream_free(&tokens);
        free(source);
        return 1;
    }
    t1 = clock();
    if (verbose) fprintf(stderr, "[verbose] parse: %.1f ms\n", elapsed_ms(t0));

    compiler->ast_root = ast;

    /* Expand templates before semantic analysis or AST output. */
    t0 = clock();
    jz_template_expand(compiler->ast_root, &compiler->diagnostics, filename);
    t1 = clock();
    if (verbose) fprintf(stderr, "[verbose] template_expand: %.1f ms\n", elapsed_ms(t0));

    /* Reject testbench files unless --test mode is active. */
    if (!test_mode) {
        for (size_t i = 0; i < compiler->ast_root->child_count; ++i) {
            JZASTNode *child = compiler->ast_root->children[i];
            if (child && child->type == JZ_AST_TESTBENCH) {
                JZLocation loc = child->loc;
                jz_diagnostic_report(&compiler->diagnostics, loc, JZ_SEVERITY_ERROR,
                                     "TB_WRONG_TOOL",
                                     "this file contains @testbench blocks; "
                                     "use --test to run testbenches");
                return 1;
            }
        }
    }

    /* Reject simulation files unless --simulate mode is active. */
    if (!simulate_mode) {
        for (size_t i = 0; i < compiler->ast_root->child_count; ++i) {
            JZASTNode *child = compiler->ast_root->children[i];
            if (child && child->type == JZ_AST_SIMULATION) {
                JZLocation loc = child->loc;
                jz_diagnostic_report(&compiler->diagnostics, loc, JZ_SEVERITY_ERROR,
                                     "SIM_WRONG_TOOL",
                                     "this file contains @simulation blocks; "
                                     "use --simulate to run simulations");
                return 1;
            }
        }
    }

    if (print_ast_json) {
        /* --ast mode: print JSON AST only; no validation yet. */
        FILE *out = ast_out ? ast_out : stdout;
        jz_ast_print_json(out, compiler->ast_root);
    } else {
        /* Default / --lint mode: run semantic validation (section 5 rules). */
        t0 = clock();
        if (jz_sem_run(compiler->ast_root,
                       &compiler->diagnostics,
                       filename,
                       verbose) != 0) {
            /* Non-zero return indicates semantic failure; diagnostics are
             * expected to have been recorded already.
             */
        }
        t1 = clock();
        if (verbose) fprintf(stderr, "[verbose] sem_run (total): %.1f ms\n", elapsed_ms(t0));
    }

    /* Leave compiler->ast_root populated so that later stages (IR
     * construction, backends) can consume the verified AST. The token stream
     * and raw source buffer can be freed immediately.
     */
    jz_token_stream_free(&tokens);
    free(source);
    (void)t1;
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *mode = NULL;
    const char *input_filename = NULL;
    const char *output_filename = NULL;
    const char *sdc_filename = NULL;
    const char *xdc_filename = NULL;
    const char *pcf_filename = NULL;
    const char *cst_filename = NULL;

    int lint_rules = 0;
    int warn_as_error = 0;
    int show_info = 0;
    int use_color = 0;
    int alias_report = 0;
    int memory_report = 0;
    int tristate_report = 0;
    int chip_info = 0;
    const char *chip_info_id = NULL;
    int tristate_default = 0; /* 0=none, 1=GND, 2=VCC */
    int test_mode = 0;
    int simulate_mode = 0;
    int sim_format_vcd = 0;
    int sim_format_fst = 0;
    int verbose = 0;
    uint32_t test_seed = 0;
    int test_seed_set = 0;
    int show_explain = 0;
    int allow_absolute_paths = 0;
    int allow_traversal = 0;
    const char *sandbox_roots[16];
    size_t sandbox_root_count = 0;
    JZWarningGroupOverride group_overrides[16];
    size_t group_override_count = 0;

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (strcmp(arg, "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(arg, "--version") == 0) {
            print_version();
            return 0;
        } else if (strcmp(arg, "--warn-as-error") == 0) {
            warn_as_error = 1;
        } else if (strcmp(arg, "--info") == 0) {
            show_info = 1;
        } else if (strcmp(arg, "--color") == 0) {
            use_color = 1;
        } else if (strcmp(arg, "--explain") == 0) {
            show_explain = 1;
        } else if (strcmp(arg, "--alias-report") == 0) {
            alias_report = 1;
        } else if (strcmp(arg, "--memory-report") == 0) {
            memory_report = 1;
        } else if (strcmp(arg, "--tristate-report") == 0) {
            tristate_report = 1;
        } else if (strcmp(arg, "--chip-info") == 0) {
            chip_info = 1;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                chip_info_id = argv[++i];
            }
        } else if (strcmp(arg, "--test") == 0) {
            test_mode = 1;
        } else if (strcmp(arg, "--simulate") == 0) {
            simulate_mode = 1;
        } else if (strcmp(arg, "--vcd") == 0) {
            sim_format_vcd = 1;
        } else if (strcmp(arg, "--fst") == 0) {
            sim_format_fst = 1;
        } else if (strcmp(arg, "--verbose") == 0) {
            verbose = 1;
        } else if (strncmp(arg, "--seed=", 7) == 0) {
            test_seed = (uint32_t)strtoul(arg + 7, NULL, 16);
            test_seed_set = 1;
        } else if (strcmp(arg, "--lint-rules") == 0) {
            lint_rules = 1;
        } else if (strcmp(arg, "--sdc") == 0 ||
                   strcmp(arg, "--xdc") == 0 ||
                   strcmp(arg, "--pcf") == 0 ||
                   strcmp(arg, "--cst") == 0) {
            const char **target = NULL;
            if (strcmp(arg, "--sdc") == 0) {
                target = &sdc_filename;
            } else if (strcmp(arg, "--xdc") == 0) {
                target = &xdc_filename;
            } else if (strcmp(arg, "--pcf") == 0) {
                target = &pcf_filename;
            } else {
                target = &cst_filename;
            }
            if (i + 1 >= argc) {
                fprintf(stderr, "Missing filename after %s\n", arg);
                return 1;
            }
            if (*target) {
                fprintf(stderr, "%s specified more than once\n", arg);
                return 1;
            }
            *target = argv[++i];
        } else if (strcmp(arg, "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Missing output filename after -o\n");
                return 1;
            }
            if (output_filename) {
                fprintf(stderr, "-o specified more than once\n");
                return 1;
            }
            output_filename = argv[++i];
        } else if (strncmp(arg, "--tristate-default=", 19) == 0) {
            const char *val = arg + 19;
            if (strcmp(val, "GND") == 0) {
                tristate_default = 1;
            } else if (strcmp(val, "VCC") == 0) {
                tristate_default = 2;
            } else {
                fprintf(stderr, "Invalid value for --tristate-default: '%s' (expected GND or VCC)\n", val);
                return 1;
            }
        } else if (strncmp(arg, "--Wno-group=", 12) == 0) {
            const char *group = arg + 12;
            if (*group && group_override_count < sizeof(group_overrides) / sizeof(group_overrides[0])) {
                group_overrides[group_override_count].group = group;
                group_overrides[group_override_count].enabled = 0;
                group_override_count++;
            }
        } else if (strncmp(arg, "--Wgroup=", 9) == 0) {
            const char *group = arg + 9;
            if (*group && group_override_count < sizeof(group_overrides) / sizeof(group_overrides[0])) {
                group_overrides[group_override_count].group = group;
                group_overrides[group_override_count].enabled = 1;
                group_override_count++;
            }
        } else if (strcmp(arg, "--allow-absolute-paths") == 0) {
            allow_absolute_paths = 1;
        } else if (strcmp(arg, "--allow-traversal") == 0) {
            allow_traversal = 1;
        } else if (strncmp(arg, "--sandbox-root=", 15) == 0) {
            const char *val = arg + 15;
            if (*val && sandbox_root_count < sizeof(sandbox_roots) / sizeof(sandbox_roots[0])) {
                sandbox_roots[sandbox_root_count++] = val;
            }
        } else if (strcmp(arg, "--ast") == 0 || strcmp(arg, "--lint") == 0 ||
                   strcmp(arg, "--verilog") == 0 || strcmp(arg, "--emit-verilog") == 0 ||
                   strcmp(arg, "--rtlil") == 0 || strcmp(arg, "--emit-rtlil") == 0 ||
                   strcmp(arg, "--ir") == 0) {
            if (mode && strcmp(mode, arg) != 0) {
                fprintf(stderr, "Multiple modes specified (%s and %s)\n", mode, arg);
                return 1;
            }
            mode = arg;
        } else if (arg[0] == '-' && arg[1] != '\0') {
            fprintf(stderr, "Unknown option: %s\n", arg);
            return 1;
        } else {
            /* Bare filename: treated as JZ_FILE input. */
            if (input_filename) {
                fprintf(stderr, "Multiple input files specified (%s and %s)\n", input_filename, arg);
                return 1;
            }
            input_filename = arg;
        }
    }

    if (chip_info) {
        FILE *chip_out = stdout;
        if (output_filename) {
            chip_out = fopen(output_filename, "w");
            if (!chip_out) {
                fprintf(stderr, "Failed to open output file '%s'\n", output_filename);
                return 1;
            }
        }
        int chip_rc = 0;
        if (!chip_info_id) {
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
        } else if (jz_chip_print_info(chip_info_id, chip_out) != 0) {
            fprintf(stderr, "CHIP \"%s\" not found in built-in database.\n", chip_info_id);
            chip_rc = 1;
        }
        if (chip_out != stdout) {
            fclose(chip_out);
        }
        return chip_rc;
    }

    if (lint_rules) {
        jz_rules_print_all(stdout);
        return 0;
    }

    /* Default mode when a file is provided without an explicit mode flag. */
    if (!mode && input_filename) {
        mode = "--lint";
    }

    if (!input_filename && mode && strcmp(mode, "--lint-rules") != 0) {
        print_usage(argv[0]);
        return 1;
    }

    /* Classify the primary mode. */
    int is_ast_mode = (mode && strcmp(mode, "--ast") == 0);
    int is_ir_mode = (mode && strcmp(mode, "--ir") == 0);
    int is_verilog_mode = (mode && (strcmp(mode, "--verilog") == 0 || strcmp(mode, "--emit-verilog") == 0));
    int is_rtlil_mode = (mode && (strcmp(mode, "--rtlil") == 0 || strcmp(mode, "--emit-rtlil") == 0));
    int is_lint_mode = !is_ast_mode && !is_ir_mode && !is_verilog_mode && !is_rtlil_mode; /* includes default lint */

    if (!is_verilog_mode && !is_rtlil_mode && (sdc_filename || xdc_filename || pcf_filename || cst_filename)) {
        fprintf(stderr, "--sdc/--xdc/--pcf/--cst may only be used with --verilog or --rtlil\n");
        return 1;
    }
    if (tristate_default != 0 && !is_verilog_mode && !is_rtlil_mode && !is_ir_mode) {
        fprintf(stderr, "--tristate-default may only be used with --verilog, --rtlil, or --ir\n");
        return 1;
    }
    int alias_report_active = alias_report && is_lint_mode;
    int memory_report_active = memory_report && is_lint_mode;
    int tristate_report_active = tristate_report && is_lint_mode;

    /* Only one consumer of -o is supported at a time (AST, IR, alias-report, etc.). */
    int use_output_for_ast = is_ast_mode && (output_filename != NULL);
    int use_output_for_ir = is_ir_mode && (output_filename != NULL);
    int use_output_for_alias = alias_report_active && (output_filename != NULL);
    int use_output_for_memory = memory_report_active && (output_filename != NULL);
    int use_output_for_tristate = tristate_report_active && (output_filename != NULL);
    int use_output_for_verilog = is_verilog_mode && (output_filename != NULL);
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
        /* Lint and IR both use the lint front-end mode. */
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
        if (output_filename) {
            ast_out = fopen(output_filename, "w");
            if (!ast_out) {
                fprintf(stderr, "Failed to open AST output file '%s'\n", output_filename);
                return 1;
            }
        } else {
            ast_out = stdout;
        }
    }

    if (alias_report_active) {
        if (output_filename) {
            alias_out = fopen(output_filename, "w");
            if (!alias_out) {
                fprintf(stderr, "Failed to open alias-report output file '%s'\n", output_filename);
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
        if (output_filename) {
            memory_out = fopen(output_filename, "w");
            if (!memory_out) {
                fprintf(stderr, "Failed to open memory-report output file '%s'\n", output_filename);
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
        if (output_filename) {
            tristate_out = fopen(output_filename, "w");
            if (!tristate_out) {
                fprintf(stderr, "Failed to open tristate-report output file '%s'\n", output_filename);
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
    if (input_filename) {
        jz_path_security_init(input_filename);
        if (allow_absolute_paths) {
            jz_path_security_set_allow_absolute(1);
        }
        if (allow_traversal) {
            jz_path_security_set_allow_traversal(1);
        }
        for (size_t i = 0; i < sandbox_root_count; i++) {
            jz_path_security_add_root(sandbox_roots[i]);
        }
    }

    if (alias_report_active && input_filename) {
        /* Enable alias-resolution reporting during semantic analysis. */
        FILE *out = alias_out ? alias_out : stdout;
        jz_sem_enable_alias_report(out, "JZ-HDL 1.0", input_filename);
    }
    if (memory_report_active && input_filename) {
        FILE *out = memory_out ? memory_out : stdout;
        jz_sem_enable_memory_report(out, "JZ-HDL 1.0", input_filename);
    }
    if (tristate_report_active && input_filename) {
        FILE *out = tristate_out ? tristate_out : stdout;
        jz_sem_enable_tristate_report(out, "JZ-HDL 1.0", input_filename);
    }

    if (tristate_default != 0) {
        jz_sem_set_tristate_default(1);
        show_info = 1;
    }

    /* Set global verbose flag for use by IR builder and other subsystems. */
    jz_verbose = verbose;

    int rc = 0;
    clock_t phase_t0;
    /* Always run the front end for AST, lint, IR, Verilog, and test modes. */
    phase_t0 = clock();
    rc = run_frontend(&compiler, input_filename, print_ast_json, ast_out, test_mode, simulate_mode, verbose);
    if (verbose) fprintf(stderr, "[verbose] frontend (total): %.1f ms\n", elapsed_ms(phase_t0));

    /* In lint mode, build IR (if no errors) to run the division guard check.
     * This surfaces DIV_UNGUARDED_RUNTIME_ZERO warnings during --lint.
     */
    if (is_lint_mode && rc == 0 &&
        !jz_diagnostic_has_severity(&compiler.diagnostics, JZ_SEVERITY_ERROR) &&
        compiler.ast_root != NULL && !compiler.ir_root) {
        phase_t0 = clock();
        if (jz_ir_build_design(compiler.ast_root,
                               &compiler.ir_root,
                               &compiler.ir_arena,
                               &compiler.diagnostics) == 0 &&
            compiler.ir_root) {
            if (verbose) fprintf(stderr, "[verbose] ir_build (lint): %.1f ms\n", elapsed_ms(phase_t0));
            phase_t0 = clock();
            jz_ir_div_guard_check(compiler.ir_root, &compiler.diagnostics);
            if (verbose) fprintf(stderr, "[verbose] div_guard_check: %.1f ms\n", elapsed_ms(phase_t0));
        } else {
            if (verbose) fprintf(stderr, "[verbose] ir_build (lint): %.1f ms (failed)\n", elapsed_ms(phase_t0));
        }
    }

    /* If IR output was requested and the front end completed without
     * diagnostics at ERROR severity, construct IR and serialize to JSON.
     */
    if (emit_ir && rc == 0 &&
        !jz_diagnostic_has_severity(&compiler.diagnostics, JZ_SEVERITY_ERROR) &&
        compiler.ast_root != NULL) {
        if (jz_ir_build_design(compiler.ast_root,
                               &compiler.ir_root,
                               &compiler.ir_arena,
                               &compiler.diagnostics) != 0) {
            rc = 1;
        } else {
            if (tristate_default != 0 && compiler.ir_root) {
                compiler.ir_root->tristate_default = (IR_TristateDefault)tristate_default;
                if (jz_ir_tristate_transform(compiler.ir_root,
                                              &compiler.ir_arena,
                                              &compiler.diagnostics) != 0) {
                    rc = 1;
                }
            }
            if (rc == 0 && compiler.ir_root) {
                jz_ir_div_guard_check(compiler.ir_root, &compiler.diagnostics);
            }
            if (rc == 0) {
                const char *ir_target = output_filename ? output_filename : "-";
                if (jz_ir_write_json(compiler.ir_root,
                                      ir_target,
                                      &compiler.diagnostics,
                                      input_filename) != 0) {
                    rc = 1;
                }
            }
        }
    }

    /* If Verilog output was requested and the front end completed without
     * diagnostics at ERROR severity, construct IR (if needed) and invoke the
     * Verilog backend.
     */
    if (cmode == JZ_COMPILER_MODE_VERILOG && !is_rtlil_mode &&
        rc == 0 &&
        !jz_diagnostic_has_severity(&compiler.diagnostics, JZ_SEVERITY_ERROR) &&
        compiler.ast_root != NULL) {
        if (!compiler.ir_root) {
            phase_t0 = clock();
            if (jz_ir_build_design(compiler.ast_root,
                                   &compiler.ir_root,
                                   &compiler.ir_arena,
                                   &compiler.diagnostics) != 0) {
                rc = 1;
            }
            if (verbose) fprintf(stderr, "[verbose] ir_build (verilog): %.1f ms\n", elapsed_ms(phase_t0));
        }
        if (rc == 0 && compiler.ir_root) {
            if (tristate_default != 0) {
                phase_t0 = clock();
                compiler.ir_root->tristate_default = (IR_TristateDefault)tristate_default;
                if (jz_ir_tristate_transform(compiler.ir_root,
                                              &compiler.ir_arena,
                                              &compiler.diagnostics) != 0) {
                    rc = 1;
                }
                if (verbose) fprintf(stderr, "[verbose] tristate_transform: %.1f ms\n", elapsed_ms(phase_t0));
            }
        }
        if (rc == 0 && compiler.ir_root) {
            phase_t0 = clock();
            jz_ir_div_guard_check(compiler.ir_root, &compiler.diagnostics);
            if (verbose) fprintf(stderr, "[verbose] div_guard_check: %.1f ms\n", elapsed_ms(phase_t0));
        }
        if (rc == 0 && compiler.ir_root) {
            phase_t0 = clock();
            const char *verilog_target = output_filename ? output_filename : "-";
            if (jz_emit_verilog(compiler.ir_root,
                                verilog_target,
                                &compiler.diagnostics,
                                input_filename) != 0) {
                rc = 1;
            }
            if (verbose) fprintf(stderr, "[verbose] emit_verilog: %.1f ms\n", elapsed_ms(phase_t0));
            if (rc == 0 && sdc_filename) {
                if (jz_emit_sdc_constraints(compiler.ir_root,
                                            sdc_filename,
                                            &compiler.diagnostics,
                                            input_filename) != 0) {
                    rc = 1;
                }
            }
            if (rc == 0 && xdc_filename) {
                if (jz_emit_xdc_constraints(compiler.ir_root,
                                            xdc_filename,
                                            &compiler.diagnostics,
                                            input_filename) != 0) {
                    rc = 1;
                }
            }
            if (rc == 0 && pcf_filename) {
                if (jz_emit_pcf_constraints(compiler.ir_root,
                                            pcf_filename,
                                            &compiler.diagnostics,
                                            input_filename) != 0) {
                    rc = 1;
                }
            }
            if (rc == 0 && cst_filename) {
                if (jz_emit_cst_constraints(compiler.ir_root,
                                            cst_filename,
                                            &compiler.diagnostics,
                                            input_filename) != 0) {
                    rc = 1;
                }
            }
        }
    }

    /* If RTLIL output was requested and the front end completed without
     * diagnostics at ERROR severity, construct IR and invoke the RTLIL backend.
     */
    if (is_rtlil_mode &&
        rc == 0 &&
        !jz_diagnostic_has_severity(&compiler.diagnostics, JZ_SEVERITY_ERROR) &&
        compiler.ast_root != NULL) {
        if (!compiler.ir_root) {
            if (jz_ir_build_design(compiler.ast_root,
                                   &compiler.ir_root,
                                   &compiler.ir_arena,
                                   &compiler.diagnostics) != 0) {
                rc = 1;
            }
        }
        if (rc == 0 && compiler.ir_root) {
            if (tristate_default != 0) {
                compiler.ir_root->tristate_default = (IR_TristateDefault)tristate_default;
                if (jz_ir_tristate_transform(compiler.ir_root,
                                              &compiler.ir_arena,
                                              &compiler.diagnostics) != 0) {
                    rc = 1;
                }
            }
        }
        if (rc == 0 && compiler.ir_root) {
            jz_ir_div_guard_check(compiler.ir_root, &compiler.diagnostics);
        }
        if (rc == 0 && compiler.ir_root) {
            const char *rtlil_target = output_filename ? output_filename : "-";
            if (jz_emit_rtlil(compiler.ir_root,
                               rtlil_target,
                               &compiler.diagnostics,
                               input_filename) != 0) {
                rc = 1;
            }
            /* Constraint files work with RTLIL mode too. */
            if (rc == 0 && sdc_filename) {
                if (jz_emit_sdc_constraints(compiler.ir_root,
                                            sdc_filename,
                                            &compiler.diagnostics,
                                            input_filename) != 0) {
                    rc = 1;
                }
            }
            if (rc == 0 && xdc_filename) {
                if (jz_emit_xdc_constraints(compiler.ir_root,
                                            xdc_filename,
                                            &compiler.diagnostics,
                                            input_filename) != 0) {
                    rc = 1;
                }
            }
            if (rc == 0 && pcf_filename) {
                if (jz_emit_pcf_constraints(compiler.ir_root,
                                            pcf_filename,
                                            &compiler.diagnostics,
                                            input_filename) != 0) {
                    rc = 1;
                }
            }
            if (rc == 0 && cst_filename) {
                if (jz_emit_cst_constraints(compiler.ir_root,
                                            cst_filename,
                                            &compiler.diagnostics,
                                            input_filename) != 0) {
                    rc = 1;
                }
            }
        }
    }

    /* If --test mode was requested and the front end completed without
     * diagnostics at ERROR severity, build IR and run testbenches.
     */
    if (test_mode &&
        rc == 0 &&
        !jz_diagnostic_has_severity(&compiler.diagnostics, JZ_SEVERITY_ERROR) &&
        compiler.ast_root != NULL) {
        if (!compiler.ir_root) {
            if (jz_ir_build_design(compiler.ast_root,
                                   &compiler.ir_root,
                                   &compiler.ir_arena,
                                   &compiler.diagnostics) != 0) {
                rc = 1;
            }
        }
        if (rc == 0 && compiler.ir_root) {
            jz_ir_div_guard_check(compiler.ir_root, &compiler.diagnostics);
        }
        if (rc == 0 && compiler.ir_root) {
            if (!test_seed_set) {
                test_seed = 0;
            }
            int sim_rc = jz_sim_run_testbenches(compiler.ast_root,
                                                 compiler.ir_root,
                                                 test_seed,
                                                 verbose,
                                                 &compiler.diagnostics,
                                                 input_filename);
            if (sim_rc != 0) {
                rc = 1;
            }
        }
    }

    /* If --simulate mode was requested and the front end completed without
     * diagnostics at ERROR severity, build IR and run simulations.
     */
    if (simulate_mode &&
        rc == 0 &&
        !jz_diagnostic_has_severity(&compiler.diagnostics, JZ_SEVERITY_ERROR) &&
        compiler.ast_root != NULL) {
        if (sim_format_fst) {
            fprintf(stderr, "FST output format is not yet supported; use --vcd instead.\n");
            rc = 1;
        } else {
            (void)sim_format_vcd; /* VCD is the default */
            if (!compiler.ir_root) {
                if (jz_ir_build_design(compiler.ast_root,
                                       &compiler.ir_root,
                                       &compiler.ir_arena,
                                       &compiler.diagnostics) != 0) {
                    rc = 1;
                }
            }
            if (rc == 0 && compiler.ir_root) {
                jz_ir_div_guard_check(compiler.ir_root, &compiler.diagnostics);
            }
            if (rc == 0 && compiler.ir_root) {
                if (!test_seed_set) {
                    test_seed = 0;
                }
                /* Determine output path: -o flag or default to <input>.vcd */
                char default_vcd[1024];
                const char *sim_output = output_filename;
                if (!sim_output) {
                    /* Strip extension and append .vcd */
                    const char *dot = strrchr(input_filename, '.');
                    size_t base_len = dot ? (size_t)(dot - input_filename) : strlen(input_filename);
                    if (base_len > sizeof(default_vcd) - 5) base_len = sizeof(default_vcd) - 5;
                    memcpy(default_vcd, input_filename, base_len);
                    memcpy(default_vcd + base_len, ".vcd", 5);
                    sim_output = default_vcd;
                }
                int sim_rc = jz_sim_run_simulations(compiler.ast_root,
                                                     compiler.ir_root,
                                                     test_seed,
                                                     verbose,
                                                     &compiler.diagnostics,
                                                     input_filename,
                                                     sim_output);
                if (sim_rc != 0) {
                    rc = 1;
                }
            }
        }
    }

    /* Apply warning policy (group enables/disables and optional warn-as-error)
     * before printing or computing final exit status.
     */
    JZWarningPolicy policy;
    policy.warn_as_error = warn_as_error;
    policy.groups = (group_override_count > 0) ? group_overrides : NULL;
    policy.group_count = group_override_count;
    jz_diagnostic_apply_warning_policy(&compiler.diagnostics, &policy);

    /* Print buffered diagnostics for non-AST modes, or on failure. */
    if (cmode != JZ_COMPILER_MODE_AST || rc != 0) {
        jz_diagnostic_print_all(&compiler.diagnostics, stderr, use_color, input_filename, show_info, show_explain);
    }

    /* Any error-severity diagnostic forces a non-zero exit code, regardless
     * of mode.  This includes diagnostics emitted by semantic analysis
     * (e.g. MEM_INIT_FILE_NOT_FOUND) that don't cause the pipeline to abort
     * internally but still represent a build failure.
     */
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
