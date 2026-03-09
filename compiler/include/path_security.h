/**
 * @file path_security.h
 * @brief File path security and sandboxing for JZ-HDL compiler.
 *
 * Implements Section 12 of the JZ-HDL specification: all file paths
 * referenced by @import, @file(), and chip JSON loading are validated
 * against a set of permitted sandbox roots. Absolute paths and directory
 * traversal (..) are forbidden by default but can be enabled via CLI flags.
 */

#ifndef JZ_HDL_PATH_SECURITY_H
#define JZ_HDL_PATH_SECURITY_H

#include "ast.h"
#include "diagnostic.h"

/**
 * @brief Initialize path security with default sandbox root.
 *
 * Derives the default sandbox root from the directory containing
 * the project file. Must be called before any jz_path_validate() calls.
 *
 * @param project_filename Path to the project/input file.
 */
void jz_path_security_init(const char *project_filename);

/**
 * @brief Add an additional permitted sandbox root directory.
 *
 * Multiple roots may be added via repeated --sandbox-root flags.
 *
 * @param dir Directory path to permit.
 */
void jz_path_security_add_root(const char *dir);

/**
 * @brief Allow or disallow absolute paths in @import / @file().
 * @param allow Non-zero to allow absolute paths.
 */
void jz_path_security_set_allow_absolute(int allow);

/**
 * @brief Allow or disallow '..' traversal in paths.
 * @param allow Non-zero to allow traversal.
 */
void jz_path_security_set_allow_traversal(int allow);

/**
 * @brief Validate and canonicalize a file path against security policy.
 *
 * Checks that the path does not use forbidden absolute or traversal
 * patterns, resolves it to a canonical form, and verifies it falls
 * within at least one permitted sandbox root.
 *
 * On failure, emits an appropriate PATH_* diagnostic and returns NULL.
 *
 * @param raw_path   The raw path string from source code.
 * @param base_dir   Directory to resolve relative paths against (may be NULL).
 * @param loc        Source location for diagnostic reporting.
 * @param diag       Diagnostic list to report errors to.
 * @return Heap-allocated canonical path on success, NULL on failure.
 *         Caller must free() the returned string.
 */
char *jz_path_validate(const char *raw_path,
                        const char *base_dir,
                        JZLocation loc,
                        JZDiagnosticList *diag);

/**
 * @brief Release all resources held by the path security subsystem.
 */
void jz_path_security_cleanup(void);

#endif /* JZ_HDL_PATH_SECURITY_H */
