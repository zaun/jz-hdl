#ifndef JZ_HDL_CLI_FRONTEND_H
#define JZ_HDL_CLI_FRONTEND_H

#include <stdio.h>
#include <time.h>

#include "compiler.h"

/**
 * @file cli_frontend.h
 * @brief Compiler front-end pipeline (lex, parse, semantic analysis).
 */

/** Return elapsed milliseconds since start. */
double jz_cli_elapsed_ms(clock_t start);

/**
 * Run the compiler front end: read source, expand repeats, lex, parse,
 * expand templates, and optionally run semantic analysis or print AST JSON.
 *
 * @return 0 on success, non-zero on failure.
 */
int jz_cli_run_frontend(JZCompiler *compiler,
                        const char *filename,
                        int print_ast_json,
                        FILE *ast_out,
                        int test_mode,
                        int simulate_mode,
                        int verbose);

#endif /* JZ_HDL_CLI_FRONTEND_H */
