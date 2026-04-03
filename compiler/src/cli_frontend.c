#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cli_frontend.h"
#include "ast.h"
#include "ast_json.h"
#include "lexer.h"
#include "parser.h"
#include "util.h"
#include "sem_driver.h"
#include "template_expand.h"
#include "repeat_expand.h"

double jz_cli_elapsed_ms(clock_t start) {
    return (double)(clock() - start) / CLOCKS_PER_SEC * 1000.0;
}

int jz_cli_run_frontend(JZCompiler *compiler,
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
    if (verbose) fprintf(stderr, "[verbose] lex: %.1f ms\n", jz_cli_elapsed_ms(t0));

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
    if (verbose) fprintf(stderr, "[verbose] parse: %.1f ms\n", jz_cli_elapsed_ms(t0));

    compiler->ast_root = ast;

    /* Expand templates before semantic analysis or AST output. */
    t0 = clock();
    jz_template_expand(compiler->ast_root, &compiler->diagnostics, filename);
    t1 = clock();
    if (verbose) fprintf(stderr, "[verbose] template_expand: %.1f ms\n", jz_cli_elapsed_ms(t0));

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
        if (verbose) fprintf(stderr, "[verbose] sem_run (total): %.1f ms\n", jz_cli_elapsed_ms(t0));
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
