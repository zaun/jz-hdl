/**
 * @file repeat_expand.h
 * @brief Pre-parser @repeat expansion for JZ-HDL.
 *
 * Expands @repeat N ... @end blocks in raw source text before lexing.
 * The body is duplicated N times, with each occurrence of the identifier
 * IDX replaced by the iteration index (0..N-1). Supports nesting.
 */

#ifndef JZ_HDL_REPEAT_EXPAND_H
#define JZ_HDL_REPEAT_EXPAND_H

#include "diagnostic.h"

/**
 * @brief Expand all @repeat blocks in source text.
 *
 * Scans the source string for @repeat N ... @end patterns and
 * performs text expansion. IDX is substituted as a decimal integer
 * on word boundaries.
 *
 * @param source   The raw source text (null-terminated).
 * @param filename Source filename for diagnostics.
 * @param diagnostics Diagnostic list for error reporting.
 * @return Newly allocated expanded source (caller must free),
 *         or NULL on error.
 */
char *jz_repeat_expand(const char *source,
                       const char *filename,
                       JZDiagnosticList *diagnostics);

#endif /* JZ_HDL_REPEAT_EXPAND_H */
