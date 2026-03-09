/**
 * @file diagnostic.h
 * @brief Diagnostic collection, filtering, and rendering.
 *
 * Provides a buffered diagnostic list that accumulates errors, warnings,
 * and notes throughout compilation. Diagnostics can be filtered and
 * promoted via a warning policy, then rendered to a terminal with
 * optional ANSI color output.
 */

#ifndef JZ_HDL_DIAGNOSTIC_H
#define JZ_HDL_DIAGNOSTIC_H

#include <stddef.h>
#include <stdio.h>

#include "ast.h"
#include "util.h"

/**
 * @enum JZSeverity
 * @brief Severity level for a diagnostic message.
 */
typedef enum JZSeverity {
    JZ_SEVERITY_NOTE = 0, /**< Informational note. */
    JZ_SEVERITY_WARNING,  /**< Warning (may be promoted to error by policy). */
    JZ_SEVERITY_ERROR     /**< Error (compilation fails). */
} JZSeverity;

/**
 * @struct JZDiagnostic
 * @brief A single diagnostic message with location and severity.
 */
typedef struct JZDiagnostic {
    JZLocation  loc;       /**< Source location. */
    JZSeverity  severity;  /**< Severity level. */
    const char *code;      /**< Short rule code (e.g., "LEX001"). */
    char       *message;   /**< Heap-allocated human-readable message. */
} JZDiagnostic;

/**
 * @struct JZDiagnosticList
 * @brief Growable collection of diagnostic messages.
 *
 * Internally backed by a JZBuffer storing an array of JZDiagnostic
 * elements. Initialize with jz_diagnostic_list_init() and free with
 * jz_diagnostic_list_free().
 */
typedef struct JZDiagnosticList {
    JZBuffer buffer; /**< Underlying storage for JZDiagnostic elements. */
} JZDiagnosticList;

/**
 * @struct JZWarningGroupOverride
 * @brief Per-group enable/disable override for warnings.
 */
typedef struct JZWarningGroupOverride {
    const char *group;   /**< Rule group name (e.g., "GENERAL_WARNINGS"). */
    int         enabled; /**< 0 = disable warnings in this group, 1 = enable. */
} JZWarningGroupOverride;

/**
 * @struct JZWarningPolicy
 * @brief Policy controlling warning behavior for a compilation.
 */
typedef struct JZWarningPolicy {
    int warn_as_error;                    /**< Non-zero to treat warnings as errors for exit status. */
    const JZWarningGroupOverride *groups; /**< Optional array of group overrides (may be NULL). */
    size_t group_count;                   /**< Number of entries in the groups array. */
} JZWarningPolicy;

/**
 * @brief Initialize a diagnostic list to an empty state.
 * @param list Pointer to the list to initialize. Must not be NULL.
 */
void jz_diagnostic_list_init(JZDiagnosticList *list);

/**
 * @brief Remove all diagnostics from the list, freeing their messages.
 * @param list Pointer to the list to clear. Must not be NULL.
 */
void jz_diagnostic_list_clear(JZDiagnosticList *list);

/**
 * @brief Free all resources owned by a diagnostic list.
 * @param list Pointer to the list to free. Must not be NULL.
 */
void jz_diagnostic_list_free(JZDiagnosticList *list);

/**
 * @brief Record a new diagnostic.
 * @param list     Diagnostic list to append to. Must not be NULL.
 * @param loc      Source location of the diagnostic.
 * @param severity Severity level.
 * @param code     Short rule code (e.g., "LEX001"). Not copied; must remain valid.
 * @param message  Human-readable message. Copied into the list.
 * @return 0 on success, non-zero on allocation failure.
 */
int jz_diagnostic_report(JZDiagnosticList *list,
                         JZLocation loc,
                         JZSeverity severity,
                         const char *code,
                         const char *message);

/**
 * @brief Apply a warning policy to an existing diagnostic list.
 *
 * May drop diagnostics whose group is disabled, and/or promote
 * warnings to errors when warn_as_error is set.
 *
 * @param list   Diagnostic list to modify in place. Must not be NULL.
 * @param policy Warning policy to apply. Must not be NULL.
 */
void jz_diagnostic_apply_warning_policy(JZDiagnosticList *list,
                                        const JZWarningPolicy *policy);

/**
 * @brief Check whether any diagnostic meets or exceeds a severity level.
 * @param list     Diagnostic list to scan. Must not be NULL.
 * @param severity Minimum severity to check for.
 * @return Non-zero if at least one diagnostic has severity >= the given level.
 */
int jz_diagnostic_has_severity(const JZDiagnosticList *list,
                               JZSeverity severity);

/**
 * @brief Render all diagnostics to an output stream.
 * @param list             Diagnostic list to print. Must not be NULL.
 * @param out              Output stream (e.g., stderr).
 * @param use_color        Non-zero to emit ANSI color escape codes.
 * @param primary_filename Primary source filename for context (may be NULL).
 * @param show_info        Non-zero to include JZ_SEVERITY_NOTE diagnostics.
 * @param show_explain     Non-zero to print detailed explanation under each diagnostic.
 */
void jz_diagnostic_print_all(const JZDiagnosticList *list,
                             FILE *out,
                             int use_color,
                             const char *primary_filename,
                             int show_info,
                             int show_explain);

#endif /* JZ_HDL_DIAGNOSTIC_H */
