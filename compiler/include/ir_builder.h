/**
 * @file ir_builder.h
 * @brief IR construction from a verified AST.
 *
 * Provides the entry point for building an IR_Design from a semantically
 * validated AST. All IR allocations are made through the supplied arena.
 */

#ifndef JZ_HDL_IR_BUILDER_H
#define JZ_HDL_IR_BUILDER_H

#include "ast.h"
#include "arena.h"
#include "diagnostic.h"
#include "ir.h"

/**
 * @brief Build an IR_Design from a verified AST.
 *
 * @param root        Root AST node (typically JZ_AST_PROJECT). Semantic
 *                    analysis must have already run with no errors.
 * @param out_design  Receives a pointer to the constructed IR_Design on success.
 *                    Left unchanged on failure.
 * @param arena       Initialized arena that will own all IR allocations.
 * @param diagnostics Diagnostic list for internal error reporting.
 * @return 0 on success, non-zero on failure.
 */
int jz_ir_build_design(JZASTNode *root,
                       IR_Design **out_design,
                       JZArena *arena,
                       JZDiagnosticList *diagnostics);

/**
 * @brief Transform tri-state nets into priority-chained mux logic.
 *
 * Walks each module's async_block looking for z-guarded ternary
 * assignments and replaces them with GND/VCC default chains.
 * Only runs when design->tristate_default != TRISTATE_DEFAULT_NONE.
 *
 * @param design      IR design to transform in place.
 * @param arena       Arena for new IR node allocations.
 * @param diagnostics Diagnostic list for warnings/errors.
 * @return 0 on success, non-zero on failure.
 */
int jz_ir_tristate_transform(IR_Design *design,
                              JZArena *arena,
                              JZDiagnosticList *diagnostics);

/**
 * @brief Print a post-transform tri-state report from the IR.
 *
 * Shows the state of all signals after the tristate transform pass:
 * which nets were split, the mux chains built in parent modules,
 * and confirms no z values remain on internal nets.
 *
 * @param out          Output stream.
 * @param design       IR design (post-transform).
 * @param tool_version Version string for the report header.
 * @param input_file   Input filename for the report header.
 */
/**
 * @brief Mark unreachable modules as eliminated.
 *
 * Performs a BFS from the @top module through instance edges and sets
 * IR_Module.eliminated = true for any module not transitively reachable.
 * Backends skip eliminated modules during emission.
 *
 * @param design IR design to modify in place.
 */
void jz_ir_eliminate_dead_modules(IR_Design *design);

/**
 * @brief Check that all DIV/MOD expressions have guarded divisors.
 *
 * Walks statement trees in every non-eliminated module, tracking
 * enclosing IF conditions that prove a divisor is nonzero. Emits
 * DIV_UNGUARDED_RUNTIME_ZERO for any unguarded runtime division.
 *
 * @param design      IR design to check.
 * @param diagnostics Diagnostic list for warnings.
 * @return 0 (always succeeds; diagnostics are advisory).
 */
int jz_ir_div_guard_check(IR_Design *design,
                           JZDiagnosticList *diagnostics);

void jz_ir_tristate_report(FILE *out,
                            const IR_Design *design,
                            const char *tool_version,
                            const char *input_file);

#endif /* JZ_HDL_IR_BUILDER_H */
