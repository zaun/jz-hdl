/**
 * @file template_expand.h
 * @brief Template expansion pass for JZ-HDL.
 *
 * Expands @template/@apply/@scratch AST nodes into plain statements
 * before semantic analysis. After expansion, the AST looks as if the
 * user wrote everything by hand.
 */

#ifndef JZ_HDL_TEMPLATE_EXPAND_H
#define JZ_HDL_TEMPLATE_EXPAND_H

#include "ast.h"
#include "diagnostic.h"

/**
 * @brief Expand all template applications in the AST.
 *
 * Walks the AST, collects @template definitions, expands @apply nodes
 * by cloning and substituting template bodies, materializes @scratch
 * wires as module-level WIRE declarations, and removes template
 * definition nodes.
 *
 * @param root        Root AST node (typically JZ_AST_PROJECT).
 * @param diagnostics Diagnostic list for error collection.
 * @param filename    Primary source filename for diagnostics.
 * @return 0 on success, non-zero if errors were found.
 */
int jz_template_expand(JZASTNode *root,
                        JZDiagnosticList *diagnostics,
                        const char *filename);

#endif /* JZ_HDL_TEMPLATE_EXPAND_H */
