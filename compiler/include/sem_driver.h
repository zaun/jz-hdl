/**
 * @file sem_driver.h
 * @brief Semantic analysis entry point and report configuration.
 *
 * Provides the main semantic validation pass and optional report
 * generators for alias resolution, memory analysis, and tri-state
 * driver resolution.
 */

#ifndef JZ_HDL_SEM_DRIVER_H
#define JZ_HDL_SEM_DRIVER_H

#include "ast.h"
#include "diagnostic.h"
#include <stdio.h>

/**
 * @brief Run all semantic validation passes on an AST.
 *
 * Performs constant evaluation, expression typing, literal semantics,
 * driver ownership checks, clock domain analysis, and all other
 * semantic rules.
 *
 * @param root        Root AST node (typically JZ_AST_PROJECT or JZ_AST_MODULE).
 * @param diagnostics Diagnostic list for error/warning collection. Must not be NULL.
 * @param filename    Primary source filename for diagnostics.
 * @return 0 on success (no errors), non-zero if errors were found.
 */
int jz_sem_run(JZASTNode *root,
               JZDiagnosticList *diagnostics,
               const char *filename,
               int verbose);

/**
 * @brief Enable alias-resolution reporting during semantic analysis.
 *
 * When enabled, the flow analysis pass emits a human-readable alias
 * report for each module to the given output stream.
 *
 * @param out            Output stream for the report.
 * @param tool_version   Tool version string for the report header (may be NULL).
 * @param input_filename Source filename for the report header (may be NULL).
 */
void jz_sem_enable_alias_report(FILE *out,
                                const char *tool_version,
                                const char *input_filename);

/**
 * @brief Enable memory-report generation during semantic analysis.
 *
 * When enabled, emits a human-readable memory analysis report for
 * each module to the given output stream.
 *
 * @param out            Output stream for the report.
 * @param tool_version   Tool version string for the report header (may be NULL).
 * @param input_filename Source filename for the report header (may be NULL).
 */
void jz_sem_enable_memory_report(FILE *out,
                                 const char *tool_version,
                                 const char *input_filename);

/**
 * @brief Enable tri-state resolution reporting during semantic analysis.
 *
 * When enabled, the flow analysis pass emits a human-readable tri-state
 * resolution report for each module showing driver analysis, enable
 * conditions, proof obligations, and resolution status for multi-driver
 * nets.
 *
 * @param out            Output stream for the report.
 * @param tool_version   Tool version string for the report header (may be NULL).
 * @param input_filename Source filename for the report header (may be NULL).
 */
void jz_sem_enable_tristate_report(FILE *out,
                                   const char *tool_version,
                                   const char *input_filename);

/**
 * @brief Inform semantic analysis that --tristate-default is active.
 *
 * When set, the WARN_INTERNAL_TRISTATE warning is suppressed because
 * the IR tri-state transform will handle internal tri-state nets.
 *
 * @param active Non-zero if --tristate-default is enabled.
 */
void jz_sem_set_tristate_default(int active);

#endif /* JZ_HDL_SEM_DRIVER_H */
