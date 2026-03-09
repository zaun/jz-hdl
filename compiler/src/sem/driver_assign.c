#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

#include "sem_driver.h"
#include "sem.h"
#include "util.h"
#include "rules.h"
#include "driver_internal.h"

/* Forward declaration for internal use. */
/* sem_expr_contains_x_literal_anywhere: declared in driver_internal.h */

void sem_lhs_observable_classify(JZASTNode *lhs,
                                        const JZModuleScope *mod_scope,
                                        int *out_has_register,
                                        int *out_has_out_inout)
{
    if (!lhs || !mod_scope) return;

    switch (lhs->type) {
    case JZ_AST_EXPR_CONCAT:
        for (size_t i = 0; i < lhs->child_count; ++i) {
            sem_lhs_observable_classify(lhs->children[i], mod_scope,
                                        out_has_register, out_has_out_inout);
        }
        break;

    case JZ_AST_EXPR_SLICE:
        if (lhs->child_count >= 1) {
            sem_lhs_observable_classify(lhs->children[0], mod_scope,
                                        out_has_register, out_has_out_inout);
        }
        break;

    case JZ_AST_EXPR_IDENTIFIER: {
        if (!lhs->name) return;
        const JZSymbol *sym = module_scope_lookup(mod_scope, lhs->name);
        if (!sym || !sym->node) return;

        if (sym->kind == JZ_SYM_REGISTER || sym->kind == JZ_SYM_LATCH) {
            if (out_has_register) *out_has_register = 1;
        } else if (sym->kind == JZ_SYM_PORT && sym->node->block_kind) {
            if (strcmp(sym->node->block_kind, "OUT") == 0 ||
                strcmp(sym->node->block_kind, "INOUT") == 0) {
                if (out_has_out_inout) *out_has_out_inout = 1;
            }
        }
        break;
    }

    default:
        /* Recurse generically into children for any other expression forms. */
        for (size_t i = 0; i < lhs->child_count; ++i) {
            sem_lhs_observable_classify(lhs->children[i], mod_scope,
                                        out_has_register, out_has_out_inout);
        }
        break;
    }
}

/* Return non-zero if expr contains any identifier that resolves to a LATCH
 * declaration in the given module scope. Used to forbid aliasing latches via
 * '=' in ASYNCHRONOUS blocks, regardless of which side of the alias they
 * appear on.
 */
int sem_expr_has_latch_identifier(const JZASTNode *expr,
                                         const JZModuleScope *mod_scope)
{
    if (!expr || !mod_scope) return 0;

    switch (expr->type) {
    case JZ_AST_EXPR_IDENTIFIER:
        if (expr->name) {
            const JZSymbol *sym = module_scope_lookup(mod_scope, expr->name);
            if (sym && sym->kind == JZ_SYM_LATCH) {
                return 1;
            }
        }
        break;

    case JZ_AST_EXPR_SLICE:
    case JZ_AST_EXPR_CONCAT:
    case JZ_AST_EXPR_UNARY:
    case JZ_AST_EXPR_BINARY:
    case JZ_AST_EXPR_TERNARY:
    case JZ_AST_EXPR_BUILTIN_CALL:
        for (size_t i = 0; i < expr->child_count; ++i) {
            if (sem_expr_has_latch_identifier(expr->children[i], mod_scope)) {
                return 1;
            }
        }
        break;

    default:
        break;
    }

    return 0;
}

static int sem_expr_is_all_z_literal_simple(const JZASTNode *expr)
{
    if (!expr) return 0;

    if (expr->type == JZ_AST_EXPR_LITERAL && expr->text) {
        const char *lex  = expr->text;
        const char *tick = strchr(lex, '\'');
        if (!tick) {
            return 0;
        }
        /* Skip width and base: "8'bzzzz" -> point to first digit after base. */
        const char *value = tick + 2;
        if (!*value) return 0;

        int saw_bit = 0;
        for (const char *p = value; *p; ++p) {
            char c = *p;
            if (c == '_' || isspace((unsigned char)c)) continue;
            saw_bit = 1;
            if (c != 'z' && c != 'Z') {
                return 0;
            }
        }
        return saw_bit ? 1 : 0;
    }

    if (expr->type == JZ_AST_EXPR_CONCAT) {
        if (expr->child_count == 0) return 0;
        for (size_t i = 0; i < expr->child_count; ++i) {
            if (!sem_expr_is_all_z_literal_simple(expr->children[i])) {
                return 0;
            }
        }
        return 1;
    }

    return 0;
}

/* Return 1 if expr's subtree contains at least one literal node. This is used
 * for ASYNC_ALIAS_LITERAL_RHS so that even expressions such as
 *   lhs = (cond ? 1'b1 : 1'b0);
 * are treated as constant-style drivers when used with alias operators.
 */
static int sem_expr_contains_literal_anywhere(const JZASTNode *expr)
{
    if (!expr) return 0;
    if (expr->type == JZ_AST_EXPR_LITERAL) {
        return 1;
    }

    /* For slices, ignore index expressions (children[1], [2], ...). We only
     * recurse into the base expression (child[0]) so that constructs like
     *   lhs = some_bus[7:2];
     * do not trigger ASYNC_ALIAS_LITERAL_RHS solely because of literal
     * indices on the slice bounds.
     *
     * For BUS_ACCESS, all children are array index selectors (e.g. the
     * `0` in `src[0].VALID`), not values being driven.  Skip them
     * entirely — the bus access itself is an identifier, not a literal.
     */
    size_t start = 0;
    size_t end   = expr->child_count;
    if (expr->type == JZ_AST_EXPR_SLICE && expr->child_count >= 1) {
        end = 1; /* only recurse into base expression */
    }
    if (expr->type == JZ_AST_EXPR_BUS_ACCESS) {
        end = 0; /* children are array indices, not values */
    }

    for (size_t i = start; i < end; ++i) {
        if (sem_expr_contains_literal_anywhere(expr->children[i])) {
            return 1;
        }
    }
    return 0;
}

/* -------------------------------------------------------------------------
 *  X-observability helpers
 * -------------------------------------------------------------------------
 */

/* Return 1 if a literal lexeme encodes one or more 'x' bits in its value
 * portion. This is a purely lexical check; the lexer has already ensured that
 * only binary literals may contain 'x' digits, and decimal/hex literals will
 * not reach this point with 'x' in their value.
 *
 * This helper is used within this file and from driver_mem.c; keep the
 * definition in this TU and add an extern prototype in driver_mem.c.
 */
int sem_literal_has_x_bits(const char *lex);

int sem_literal_has_x_bits(const char *lex)
{
    if (!lex || !*lex) return 0;

    const char *value = lex;
    const char *tick = strchr(lex, '\'');
    if (tick) {
        /* Sized literal of the form <width>'<base><value>. Skip width and base
         * so we only scan the value portion for 'x' digits.
         */
        if (!tick[1]) return 0;
        value = tick + 2;
    }

    for (const char *p = value; *p; ++p) {
        if (*p == 'x' || *p == 'X') {
            return 1;
        }
    }
    return 0;
}

int sem_literal_has_z_bits(const char *lex)
{
    if (!lex || !*lex) return 0;

    const char *value = lex;
    const char *tick = strchr(lex, '\'');
    if (tick) {
        if (!tick[1]) return 0;
        value = tick + 2;
    }

    for (const char *p = value; *p; ++p) {
        if (*p == 'z' || *p == 'Z') {
            return 1;
        }
    }
    return 0;
}

/* Return 1 if any literal within the expression subtree contains one or more
 * 'x' bits in its value. This is a conservative approximation of an
 * "x-dependent" expression used for enforcing the Observability Rule at
 * obvious sinks (register loads and OUT/INOUT ports).
 */
int sem_expr_contains_x_literal_anywhere(const JZASTNode *expr)
{
    if (!expr) return 0;
    if (expr->type == JZ_AST_EXPR_LITERAL && expr->text) {
        return sem_literal_has_x_bits(expr->text);
    }

    /* As with sem_expr_contains_literal_anywhere, do not treat slice index
     * literals as contributing to x-dependence; only recurse into the base.
     */
    size_t end = expr->child_count;
    if (expr->type == JZ_AST_EXPR_SLICE && expr->child_count >= 1) {
        end = 1;
    }
    for (size_t i = 0; i < end; ++i) {
        if (sem_expr_contains_x_literal_anywhere(expr->children[i])) {
            return 1;
        }
    }
    return 0;
}

/* Recursively check an expression for SYNC MEM read data accesses. Reports
 * MEM_SYNC_DATA_IN_ASYNC_BLOCK for any mem.port.data reference where the port
 * is declared as OUT SYNC.
 */
static void sem_check_sync_mem_data_in_expr_recursive(JZASTNode *expr,
                                                       const JZModuleScope *mod_scope,
                                                       JZDiagnosticList *diagnostics)
{
    if (!expr || !mod_scope) return;

    /* Check if this node is a qualified identifier referencing mem.port.data */
    if (expr->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER) {
        JZMemPortRef ref;
        memset(&ref, 0, sizeof(ref));
        if (sem_match_mem_port_qualified_ident(expr, mod_scope, NULL, &ref) &&
            ref.port && ref.port->block_kind &&
            strcmp(ref.port->block_kind, "OUT") == 0 &&
            ref.field == MEM_PORT_FIELD_DATA &&
            ref.port->text && strcmp(ref.port->text, "SYNC") == 0) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "%s.%s.data is a SYNC read output and may not be read in ASYNCHRONOUS blocks\n"
                     "move the read into a SYNCHRONOUS block",
                     ref.mem_decl && ref.mem_decl->name ? ref.mem_decl->name : "mem",
                     ref.port->name ? ref.port->name : "port");
            sem_report_rule(diagnostics,
                            expr->loc,
                            "MEM_SYNC_DATA_IN_ASYNC_BLOCK",
                            msg);
        }
    }

    /* Recurse into children */
    for (size_t i = 0; i < expr->child_count; ++i) {
        sem_check_sync_mem_data_in_expr_recursive(expr->children[i], mod_scope, diagnostics);
    }
}

/* Walk an ASYNCHRONOUS block and enforce ASYNC_ALIAS_LITERAL_RHS for every
 * alias assignment, regardless of nesting (IF/ELSE/SELECT/CASE).
 */

static void sem_check_async_alias_literal_rhs_recursive(JZASTNode *node,
                                                        JZDiagnosticList *diagnostics)
{
    if (!node) return;

    if (node->type == JZ_AST_STMT_ASSIGN && node->child_count >= 2) {
        JZASTNode *rhs = node->children[1];
        const char *op = node->block_kind ? node->block_kind : "";
        int is_alias = (strcmp(op, "ALIAS") == 0 ||
                        strcmp(op, "ALIAS_Z") == 0 ||
                        strcmp(op, "ALIAS_S") == 0);
        if (is_alias && rhs && sem_expr_contains_literal_anywhere(rhs)) {
            sem_report_rule(diagnostics,
                            node->loc,
                            "ASYNC_ALIAS_LITERAL_RHS",
                            "alias '=' with literal on RHS is forbidden; did you mean '<=' or '=>'?\n"
                            "use '<=' (receive) or '=>' (drive) to assign constant values");
        }
    }

    for (size_t i = 0; i < node->child_count; ++i) {
        sem_check_async_alias_literal_rhs_recursive(node->children[i], diagnostics);
    }
}

/* -------------------------------------------------------------------------
 *  Special Semantic Driver (GND/VCC) validation helpers
 * -------------------------------------------------------------------------
 */

/* Return 1 if the node is a special driver (GND or VCC). */
static int sem_expr_is_special_driver(const JZASTNode *expr)
{
    return expr && expr->type == JZ_AST_EXPR_SPECIAL_DRIVER;
}

/* Recursively validate that special drivers (GND/VCC) are not used in
 * forbidden contexts per Section 2.3:
 *   - May not appear in arithmetic/logical expressions
 *   - May not appear in concatenations
 *   - May not be sliced or indexed
 *   - May not appear in slice/index expressions
 */
static void sem_check_special_driver_usage(const JZASTNode *expr,
                                           JZDiagnosticList *diagnostics)
{
    if (!expr) return;

    switch (expr->type) {
    case JZ_AST_EXPR_BINARY:
        /* Check if either operand is a special driver */
        if (expr->child_count >= 2) {
            if (sem_expr_is_special_driver(expr->children[0])) {
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "%s may not appear in arithmetic/logical expressions\n"
                         "special drivers can only be used as standalone RHS values",
                         expr->children[0]->name ? expr->children[0]->name : "GND/VCC");
                sem_report_rule(diagnostics,
                                expr->children[0]->loc,
                                "SPECIAL_DRIVER_IN_EXPRESSION",
                                msg);
            }
            if (sem_expr_is_special_driver(expr->children[1])) {
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "%s may not appear in arithmetic/logical expressions\n"
                         "special drivers can only be used as standalone RHS values",
                         expr->children[1]->name ? expr->children[1]->name : "GND/VCC");
                sem_report_rule(diagnostics,
                                expr->children[1]->loc,
                                "SPECIAL_DRIVER_IN_EXPRESSION",
                                msg);
            }
            /* Recurse into operands */
            sem_check_special_driver_usage(expr->children[0], diagnostics);
            sem_check_special_driver_usage(expr->children[1], diagnostics);
        }
        break;

    case JZ_AST_EXPR_UNARY:
        /* Check if operand is a special driver */
        if (expr->child_count >= 1) {
            if (sem_expr_is_special_driver(expr->children[0])) {
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "%s may not appear in unary expressions\n"
                         "special drivers can only be used as standalone RHS values",
                         expr->children[0]->name ? expr->children[0]->name : "GND/VCC");
                sem_report_rule(diagnostics,
                                expr->children[0]->loc,
                                "SPECIAL_DRIVER_IN_EXPRESSION",
                                msg);
            }
            sem_check_special_driver_usage(expr->children[0], diagnostics);
        }
        break;

    case JZ_AST_EXPR_CONCAT:
        /* Check if any element is a special driver */
        for (size_t i = 0; i < expr->child_count; ++i) {
            if (sem_expr_is_special_driver(expr->children[i])) {
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "%s may not appear inside concatenation {}\n"
                         "use a sized literal instead (e.g. 1'b0 or 1'b1)",
                         expr->children[i]->name ? expr->children[i]->name : "GND/VCC");
                sem_report_rule(diagnostics,
                                expr->children[i]->loc,
                                "SPECIAL_DRIVER_IN_CONCAT",
                                msg);
            }
            sem_check_special_driver_usage(expr->children[i], diagnostics);
        }
        break;

    case JZ_AST_EXPR_SLICE:
        /* Check if base is a special driver (slicing GND/VCC is forbidden) */
        if (expr->child_count >= 1 && sem_expr_is_special_driver(expr->children[0])) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "%s may not be sliced or indexed\n"
                     "special drivers have no bit structure",
                     expr->children[0]->name ? expr->children[0]->name : "GND/VCC");
            sem_report_rule(diagnostics,
                            expr->children[0]->loc,
                            "SPECIAL_DRIVER_SLICED",
                            msg);
        }
        /* Check if indices contain special drivers */
        for (size_t i = 1; i < expr->child_count; ++i) {
            if (expr->children[i] && sem_expr_is_special_driver(expr->children[i])) {
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "%s may not be used as a slice/index value",
                         expr->children[i]->name ? expr->children[i]->name : "GND/VCC");
                sem_report_rule(diagnostics,
                                expr->children[i]->loc,
                                "SPECIAL_DRIVER_IN_INDEX",
                                msg);
            }
            sem_check_special_driver_usage(expr->children[i], diagnostics);
        }
        /* Recurse into base */
        if (expr->child_count >= 1) {
            sem_check_special_driver_usage(expr->children[0], diagnostics);
        }
        break;

    case JZ_AST_EXPR_TERNARY:
        /* Special drivers are allowed in ternary branches, but check the
         * condition is not a special driver (width-1 requirement) */
        if (expr->child_count >= 1 && sem_expr_is_special_driver(expr->children[0])) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "%s may not be used as a ternary condition\n"
                     "condition must be a 1-bit expression",
                     expr->children[0]->name ? expr->children[0]->name : "GND/VCC");
            sem_report_rule(diagnostics,
                            expr->children[0]->loc,
                            "SPECIAL_DRIVER_IN_EXPRESSION",
                            msg);
        }
        /* Recurse into all parts including branches (to catch nested violations) */
        for (size_t i = 0; i < expr->child_count; ++i) {
            sem_check_special_driver_usage(expr->children[i], diagnostics);
        }
        break;

    case JZ_AST_EXPR_BUILTIN_CALL:
        /* Check if any argument is a special driver */
        for (size_t i = 0; i < expr->child_count; ++i) {
            if (sem_expr_is_special_driver(expr->children[i])) {
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "%s may not be used as a function argument\n"
                         "use a sized literal instead",
                         expr->children[i]->name ? expr->children[i]->name : "GND/VCC");
                sem_report_rule(diagnostics,
                                expr->children[i]->loc,
                                "SPECIAL_DRIVER_IN_EXPRESSION",
                                msg);
            }
            sem_check_special_driver_usage(expr->children[i], diagnostics);
        }
        break;

    default:
        /* For other expression types, just recurse into children */
        for (size_t i = 0; i < expr->child_count; ++i) {
            sem_check_special_driver_usage(expr->children[i], diagnostics);
        }
        break;
    }
}

/* -------------------------------------------------------------------------
 *  MEM helpers (declaration/introspection) used by MEM_* rules
 * -------------------------------------------------------------------------
 */
    /* MEM helpers implemented in driver_mem.c. */

/* Recursively walk an lvalue expression (identifier/concat/slice) and apply
 * target-specific checks based on module symbols and block kind.
 */
static void sem_check_lvalue_targets_recursive(JZASTNode *node,
                                               const JZModuleScope *mod_scope,
                                               const JZBuffer *project_symbols,
                                               JZDiagnosticList *diagnostics,
                                               int is_sync,
                                               int is_alias)
{
    if (!node || !mod_scope) return;

    /* Empty concatenation on the left-hand side is always invalid. */
    if (node->type == JZ_AST_EXPR_CONCAT && node->child_count == 0) {
        sem_report_rule(diagnostics,
                        node->loc,
                        "CONCAT_EMPTY",
                        "empty concatenation '{}' is not allowed");
        return;
    }

    switch (node->type) {
    case JZ_AST_EXPR_CONCAT:
        for (size_t i = 0; i < node->child_count; ++i) {
            sem_check_lvalue_targets_recursive(node->children[i], mod_scope, project_symbols, diagnostics, is_sync, is_alias);
        }
        break;

    case JZ_AST_EXPR_SLICE:
        if (node->child_count >= 1) {
            sem_check_lvalue_targets_recursive(node->children[0], mod_scope, project_symbols, diagnostics, is_sync, is_alias);
        }
        break;

    case JZ_AST_EXPR_IDENTIFIER: {
        if (!node->name) return;
        const JZSymbol *sym = module_scope_lookup(mod_scope, node->name);
        if (!sym) return; /* undeclared already reported by name resolution */

        if (sym->kind == JZ_SYM_MUX) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "'%s' is a MUX and cannot be assigned to\n"
                     "MUX values are read-only aggregations; assign to the underlying source signals",
                     node->name);
            sem_report_rule(diagnostics,
                            node->loc,
                            "MUX_ASSIGN_LHS",
                            msg);
        }

        /* Disallow driving IN ports (directional mismatch). Legal tri-state
         * driving is expressed via INOUT ports, not IN ports.
         */
        if (sym->kind == JZ_SYM_PORT &&
            sym->node && sym->node->block_kind &&
            strcmp(sym->node->block_kind, "IN") == 0) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "'%s' is an IN port and cannot be driven from inside the module\n"
                     "IN ports are read-only; use OUT or INOUT for outputs",
                     node->name);
            sem_report_rule(diagnostics,
                            node->loc,
                            "PORT_DIRECTION_MISMATCH_IN",
                            msg);
        }

        if (!is_sync) {
            /* ASYNCHRONOUS: registers are read-only; writes forbidden. */
            if (sym->kind == JZ_SYM_REGISTER) {
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "'%s' is a REGISTER and cannot be written in an ASYNCHRONOUS block\n"
                         "registers may only be assigned in SYNCHRONOUS blocks",
                         node->name);
                sem_report_rule(diagnostics,
                                node->loc,
                                "ASYNC_ASSIGN_REGISTER",
                                msg);
            }
        } else {
            /* SYNCHRONOUS: only registers (or their slices/concats) are valid
             * assignment targets.
             */
            if (sym->kind != JZ_SYM_REGISTER) {
                /* More specific WIRE case. */
                if (sym->kind == JZ_SYM_WIRE) {
                    char msg[512];
                    snprintf(msg, sizeof(msg),
                             "'%s' is a WIRE and cannot be written in a SYNCHRONOUS block\n"
                             "wires are combinational; assign them in ASYNCHRONOUS blocks",
                             node->name);
                    sem_report_rule(diagnostics,
                                    node->loc,
                                    "WRITE_WIRE_IN_SYNC",
                                    msg);
                }

                /* Generic non-register assignment in sync block. */
                {
                    char msg[512];
                    snprintf(msg, sizeof(msg),
                             "'%s' is not a REGISTER; only registers may be assigned in SYNCHRONOUS blocks\n"
                             "declare '%s' as REGISTER, or move this assignment to an ASYNCHRONOUS block",
                             node->name, node->name);
                    sem_report_rule(diagnostics,
                                    node->loc,
                                    "ASSIGN_TO_NON_REGISTER_IN_SYNC",
                                    msg);
                }
            }
        }
        break;
    }

    case JZ_AST_EXPR_QUALIFIED_IDENTIFIER: {
        /* GLOBAL_ASSIGN_FORBIDDEN: cannot assign to a global namespace constant. */
        if (node->name && diagnostics && project_symbols && project_symbols->data) {
            const char *dot = strchr(node->name, '.');
            if (dot && dot > node->name) {
                size_t hlen = (size_t)(dot - node->name);
                char head[64];
                if (hlen < sizeof(head)) {
                    memcpy(head, node->name, hlen);
                    head[hlen] = '\0';
                    const JZSymbol *syms = (const JZSymbol *)project_symbols->data;
                    size_t count = project_symbols->len / sizeof(JZSymbol);
                    int is_global = 0;
                    for (size_t gi = 0; gi < count; ++gi) {
                        if (syms[gi].kind == JZ_SYM_GLOBAL && syms[gi].name &&
                            strcmp(syms[gi].name, head) == 0) {
                            is_global = 1;
                            break;
                        }
                    }
                    if (is_global) {
                        char msg[512];
                        snprintf(msg, sizeof(msg),
                                 "'%s' is a GLOBAL constant and cannot be assigned\n"
                                 "GLOBAL values are read-only compile-time constants",
                                 node->name);
                        sem_report_rule(diagnostics,
                                        node->loc,
                                        "GLOBAL_ASSIGN_FORBIDDEN",
                                        msg);
                        break;
                    }
                }
            }
        }

        JZMemPortRef mem_ref;
        memset(&mem_ref, 0, sizeof(mem_ref));
        if (sem_match_mem_port_qualified_ident(node, mod_scope, NULL, &mem_ref) &&
            mem_ref.port && mem_ref.port->block_kind &&
            strcmp(mem_ref.port->block_kind, "OUT") == 0 &&
            mem_ref.port->text && strcmp(mem_ref.port->text, "SYNC") == 0 &&
            mem_ref.field == MEM_PORT_FIELD_ADDR) {
            if (!is_sync) {
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "'%s' is not a valid assignment target in ASYNCHRONOUS blocks",
                         node->name ? node->name : "(expression)");
                sem_report_rule(diagnostics,
                                node->loc,
                                "ASYNC_INVALID_STATEMENT_TARGET",
                                msg);
            }
            break;
        }
        /* INOUT port .addr and .wdata are valid LHS targets only in SYNC blocks */
        if (sem_match_mem_port_qualified_ident(node, mod_scope, NULL, &mem_ref) &&
            mem_ref.port && mem_ref.port->block_kind &&
            strcmp(mem_ref.port->block_kind, "INOUT") == 0 &&
            (mem_ref.field == MEM_PORT_FIELD_ADDR ||
             mem_ref.field == MEM_PORT_FIELD_WDATA)) {
            /* These are valid targets in SYNC blocks, so break without error.
             * The more specific error checks are done in sem_check_assignment_stmt. */
            break;
        }
        if (project_symbols) {
            JZBusAccessInfo info;
            if (sem_resolve_bus_access(node, mod_scope, project_symbols, &info, NULL) &&
                info.signal_decl) {
                if (!info.writable && !is_alias) {
                    char msg[512];
                    snprintf(msg, sizeof(msg),
                             "'%s' is read-only for this BUS role; write access is not permitted",
                             node->name ? node->name : "(bus signal)");
                    sem_report_rule(diagnostics,
                                    node->loc,
                                    "BUS_SIGNAL_WRITE_TO_READABLE",
                                    msg);
                }
                if (is_sync) {
                    char msg[512];
                    snprintf(msg, sizeof(msg),
                             "'%s' is a BUS signal (not a REGISTER); only registers may be assigned in SYNCHRONOUS blocks",
                             node->name ? node->name : "(bus signal)");
                    sem_report_rule(diagnostics,
                                    node->loc,
                                    "ASSIGN_TO_NON_REGISTER_IN_SYNC",
                                    msg);
                }
                break;
            }
        }
        if (!is_sync && diagnostics) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "'%s' is not a valid assignment target in ASYNCHRONOUS blocks",
                     node->name ? node->name : "(expression)");
            sem_report_rule(diagnostics,
                            node->loc,
                            "ASYNC_INVALID_STATEMENT_TARGET",
                            msg);
        }
        break;
    }

    case JZ_AST_EXPR_BUS_ACCESS: {
        if (project_symbols) {
            JZBusAccessInfo info;
            if (sem_resolve_bus_access(node, mod_scope, project_symbols, &info, NULL) &&
                info.signal_decl) {
                if (!info.writable && !is_alias) {
                    char msg[512];
                    snprintf(msg, sizeof(msg),
                             "%s.%s is read-only for this BUS role; write access is not permitted",
                             node->name ? node->name : "bus",
                             node->text ? node->text : "signal");
                    sem_report_rule(diagnostics,
                                    node->loc,
                                    "BUS_SIGNAL_WRITE_TO_READABLE",
                                    msg);
                }
                if (is_sync) {
                    char msg[512];
                    snprintf(msg, sizeof(msg),
                             "%s.%s is a BUS signal (not a REGISTER); only registers may be assigned in SYNCHRONOUS blocks",
                             node->name ? node->name : "bus",
                             node->text ? node->text : "signal");
                    sem_report_rule(diagnostics,
                                    node->loc,
                                    "ASSIGN_TO_NON_REGISTER_IN_SYNC",
                                    msg);
                }
            }
        }
        break;
    }

    default:
        /* Any other expression form on the LHS is not a valid assignment
         * target. In ASYNCHRONOUS blocks this is reported explicitly as an
         * invalid statement target; in SYNCHRONOUS blocks earlier checks
         * (e.g., ASSIGN_TO_NON_REGISTER_IN_SYNC) should already have
         * produced diagnostics.
         */
        if (!is_sync && diagnostics) {
            sem_report_rule(diagnostics,
                            node->loc,
                            "ASYNC_INVALID_STATEMENT_TARGET",
                            "expression is not a valid assignment target in ASYNCHRONOUS blocks\n"
                            "only WIRE, PORT OUT/INOUT, LATCH, and BUS signals may appear on the LHS");
        }
        break;
    }
}

static void sem_check_sync_concat_dup_reg(JZASTNode *lhs,
                                          const JZModuleScope *mod_scope,
                                          JZDiagnosticList *diagnostics)
{
    if (!lhs || lhs->type != JZ_AST_EXPR_CONCAT || !mod_scope || !diagnostics) return;

    const size_t MAX_REGS = 64;
    JZASTNode *regs[MAX_REGS];
    size_t reg_count = 0;

    for (size_t i = 0; i < lhs->child_count; ++i) {
        JZASTNode *elem = lhs->children[i];
        if (!elem) continue;

        /* Peel off slices, e.g. reg[H:L]. */
        while (elem->type == JZ_AST_EXPR_SLICE && elem->child_count >= 1) {
            elem = elem->children[0];
        }
        if (!elem || elem->type != JZ_AST_EXPR_IDENTIFIER || !elem->name) {
            continue;
        }

        const JZSymbol *sym = module_scope_lookup_kind(mod_scope, elem->name, JZ_SYM_REGISTER);
        if (!sym || !sym->node) continue;

        int dup = 0;
        for (size_t j = 0; j < reg_count; ++j) {
            if (regs[j] == sym->node) {
                dup = 1;
                break;
            }
        }
        if (dup) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "register '%s' appears more than once in concatenation LHS\n"
                     "each register may only be assigned once per statement",
                     elem->name ? elem->name : "(unknown)");
            sem_report_rule(diagnostics,
                            lhs->loc,
                            "SYNC_CONCAT_DUP_REG",
                            msg);
            return;
        }
        if (reg_count < MAX_REGS) {
            regs[reg_count++] = sym->node;
        }
    }
}

/* Detect and validate BUS bulk assignments of the form
 *   instance_a.bus_port = instance_b.bus_port;
 * inside ASYNCHRONOUS blocks. When both sides resolve to BUS ports on
 * child modules, this helper enforces the basic S6.8 compatibility rules:
 *   - both ports must reference the same BUS id;
 *   - roles must be complementary (SOURCE/TARGET or TARGET/SOURCE).
 *
 * On success (whether the assignment is valid or an error was reported),
 * this function returns 1 to indicate that the statement has been fully
 * handled and should be excluded from generic assignment checks.
 */
static int sem_try_handle_bus_bulk_assignment(JZASTNode *stmt,
                                              const JZModuleScope *mod_scope,
                                              const JZBuffer *project_symbols,
                                              JZDiagnosticList *diagnostics,
                                              int is_sync)
{
    if (!stmt || !mod_scope || !project_symbols || is_sync) {
        return 0;
    }
    if (stmt->child_count < 2) {
        return 0;
    }

    JZASTNode *lhs = stmt->children[0];
    JZASTNode *rhs = stmt->children[1];
    if (!lhs || !rhs) {
        return 0;
    }

    const char *op = stmt->block_kind ? stmt->block_kind : "";
    if (strcmp(op, "ALIAS") != 0) {
        /* Bulk BUS connectivity is defined for the alias operator '=' only. */
        return 0;
    }

    if (lhs->type != JZ_AST_EXPR_QUALIFIED_IDENTIFIER ||
        rhs->type != JZ_AST_EXPR_QUALIFIED_IDENTIFIER ||
        !lhs->name || !rhs->name) {
        return 0;
    }

    /* Split qualified names "inst.port" into instance and port identifiers. */
    char inst_a[128];
    char port_a[128];
    char inst_b[128];
    char port_b[128];

    const char *dot = strchr(lhs->name, '.');
    if (!dot || dot == lhs->name || !*(dot + 1)) {
        return 0;
    }
    size_t head_len = (size_t)(dot - lhs->name);
    if (head_len >= sizeof(inst_a)) head_len = sizeof(inst_a) - 1u;
    memcpy(inst_a, lhs->name, head_len);
    inst_a[head_len] = '\0';
    strncpy(port_a, dot + 1, sizeof(port_a) - 1u);
    port_a[sizeof(port_a) - 1u] = '\0';

    dot = strchr(rhs->name, '.');
    if (!dot || dot == rhs->name || !*(dot + 1)) {
        return 0;
    }
    head_len = (size_t)(dot - rhs->name);
    if (head_len >= sizeof(inst_b)) head_len = sizeof(inst_b) - 1u;
    memcpy(inst_b, rhs->name, head_len);
    inst_b[head_len] = '\0';
    strncpy(port_b, dot + 1, sizeof(port_b) - 1u);
    port_b[sizeof(port_b) - 1u] = '\0';

    /* Resolve instances in the parent module scope. */
    const JZSymbol *inst_sym_a = module_scope_lookup_kind(mod_scope, inst_a, JZ_SYM_INSTANCE);
    const JZSymbol *inst_sym_b = module_scope_lookup_kind(mod_scope, inst_b, JZ_SYM_INSTANCE);
    if (!inst_sym_a || !inst_sym_a->node || inst_sym_a->node->type != JZ_AST_MODULE_INSTANCE ||
        !inst_sym_b || !inst_sym_b->node || inst_sym_b->node->type != JZ_AST_MODULE_INSTANCE) {
        return 0;
    }

    JZASTNode *inst_node_a = inst_sym_a->node;
    JZASTNode *inst_node_b = inst_sym_b->node;

    /* Resolve child modules for each instance via the project symbol table. */
    const char *child_name_a = inst_node_a->text;
    const char *child_name_b = inst_node_b->text;
    if (!child_name_a || !child_name_b) {
        return 0;
    }

    const JZSymbol *proj_syms = (const JZSymbol *)project_symbols->data;
    size_t proj_count = project_symbols->len / sizeof(JZSymbol);

    const JZSymbol *child_sym_a = NULL;
    const JZSymbol *child_sym_b = NULL;
    for (size_t i = 0; i < proj_count; ++i) {
        const JZSymbol *s = &proj_syms[i];
        if (!s->name) continue;
        if (!child_sym_a && strcmp(s->name, child_name_a) == 0 && s->kind == JZ_SYM_MODULE) {
            child_sym_a = s;
        }
        if (!child_sym_b && strcmp(s->name, child_name_b) == 0 && s->kind == JZ_SYM_MODULE) {
            child_sym_b = s;
        }
        if (child_sym_a && child_sym_b) break;
    }
    if (!child_sym_a || !child_sym_a->node || child_sym_a->node->type != JZ_AST_MODULE ||
        !child_sym_b || !child_sym_b->node || child_sym_b->node->type != JZ_AST_MODULE) {
        return 0;
    }

    JZASTNode *child_mod_a = child_sym_a->node;
    JZASTNode *child_mod_b = child_sym_b->node;

    /* Locate BUS ports with the requested names on each child module. */
    JZASTNode *bus_port_a = NULL;
    JZASTNode *bus_port_b = NULL;

    for (size_t i = 0; i < child_mod_a->child_count && !bus_port_a; ++i) {
        JZASTNode *blk = child_mod_a->children[i];
        if (!blk || blk->type != JZ_AST_PORT_BLOCK) continue;
        for (size_t j = 0; j < blk->child_count; ++j) {
            JZASTNode *pd = blk->children[j];
            if (!pd || pd->type != JZ_AST_PORT_DECL || !pd->name) continue;
            if (!pd->block_kind || strcmp(pd->block_kind, "BUS") != 0) continue;
            if (strcmp(pd->name, port_a) == 0) {
                bus_port_a = pd;
                break;
            }
        }
    }

    for (size_t i = 0; i < child_mod_b->child_count && !bus_port_b; ++i) {
        JZASTNode *blk = child_mod_b->children[i];
        if (!blk || blk->type != JZ_AST_PORT_BLOCK) continue;
        for (size_t j = 0; j < blk->child_count; ++j) {
            JZASTNode *pd = blk->children[j];
            if (!pd || pd->type != JZ_AST_PORT_DECL || !pd->name) continue;
            if (!pd->block_kind || strcmp(pd->block_kind, "BUS") != 0) continue;
            if (strcmp(pd->name, port_b) == 0) {
                bus_port_b = pd;
                break;
            }
        }
    }

    if (!bus_port_a || !bus_port_b || !bus_port_a->text || !bus_port_b->text) {
        /* At least one side is not a BUS port; let generic rules handle it. */
        return 0;
    }

    char bus_a[128] = {0};
    char role_a[128] = {0};
    char bus_b[128] = {0};
    char role_b[128] = {0};

    sscanf(bus_port_a->text, "%127s %127s", bus_a, role_a);
    sscanf(bus_port_b->text, "%127s %127s", bus_b, role_b);

    if (bus_a[0] == '\0' || bus_b[0] == '\0') {
        /* Malformed BUS metadata; underlying BUS_PORT_UNKNOWN_BUS/BUS_* rules
         * on the module declarations should already report this.
         */
        return 1;
    }

    /* Compatibility: BUS ids must match. */
    if (strcmp(bus_a, bus_b) != 0) {
        if (diagnostics) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "%s.%s uses BUS '%s' but %s.%s uses BUS '%s'\n"
                     "bulk BUS assignment requires both ports to reference the same BUS type",
                     inst_a, port_a, bus_a, inst_b, port_b, bus_b);
            sem_report_rule(diagnostics,
                            stmt->loc,
                            "BUS_BULK_BUS_MISMATCH",
                            msg);
        }
        return 1;
    }

    /* Role conflict: require complementary SOURCE/TARGET roles. */
    int is_src_a = (strcmp(role_a, "SOURCE") == 0);
    int is_tgt_a = (strcmp(role_a, "TARGET") == 0);
    int is_src_b = (strcmp(role_b, "SOURCE") == 0);
    int is_tgt_b = (strcmp(role_b, "TARGET") == 0);

    if ((is_src_a && is_src_b) || (is_tgt_a && is_tgt_b)) {
        if (diagnostics) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "%s.%s and %s.%s are both %s\n"
                     "bulk BUS assignment requires complementary roles (SOURCE/TARGET)",
                     inst_a, port_a, inst_b, port_b,
                     is_src_a ? "SOURCE" : "TARGET");
            sem_report_rule(diagnostics,
                            stmt->loc,
                            "BUS_BULK_ROLE_CONFLICT",
                            msg);
        }
        return 1;
    }

    /* Valid BUS bulk assignment (roles complementary, BUS id matches). For now
     * this is treated as a semantic success without further width or net
     * analysis; detailed expansion into per-signal connections is handled by
     * later implementation work.
     */
    return 1;
}


static void sem_check_assignment_stmt(JZASTNode *stmt,
                                      const JZModuleScope *mod_scope,
                                      const JZBuffer *project_symbols,
                                      JZDiagnosticList *diagnostics,
                                      int is_sync,
                                      JZBuffer *mem_out_writes,
                                      JZBuffer *mem_sync_reads)
{
    if (!stmt || stmt->type != JZ_AST_STMT_ASSIGN || stmt->child_count < 2) return;
    JZASTNode *lhs = stmt->children[0];
    JZASTNode *rhs = stmt->children[1];

    /* Validate special driver (GND/VCC) usage in both LHS and RHS.
     * Special drivers are only valid as standalone RHS values, not in
     * expressions, concatenations, or as slice operands/indices.
     */
    sem_check_special_driver_usage(lhs, diagnostics);
    sem_check_special_driver_usage(rhs, diagnostics);

    JZBusAccessInfo lhs_bus;
    int lhs_is_bus = 0;
    if (project_symbols) {
        if (sem_resolve_bus_access(lhs, mod_scope, project_symbols, &lhs_bus, NULL) &&
            lhs_bus.signal_decl) {
            lhs_is_bus = 1;
        }
    }

    /* BUS bulk assignment (instA.bus = instB.bus) in ASYNCHRONOUS blocks. When
     * this returns non-zero, the statement has been fully handled.
     */
    if (project_symbols &&
        lhs && rhs &&
        sem_try_handle_bus_bulk_assignment(stmt, mod_scope, project_symbols, diagnostics, is_sync)) {
        return;
    }

    /* Determine the operator kind and classify the LHS for later checks. */
    const char *op = stmt->block_kind ? stmt->block_kind : "";
    int is_alias       = (strcmp(op, "ALIAS") == 0 ||
                          strcmp(op, "ALIAS_Z") == 0 ||
                          strcmp(op, "ALIAS_S") == 0);
    int is_drive       = (strncmp(op, "DRIVE", 5) == 0);
    int is_receive     = (strncmp(op, "RECEIVE", 7) == 0);
    int has_ext        = (strstr(op, "_Z") != NULL) || (strstr(op, "_S") != NULL);
    int is_latch_guard = (strcmp(op, "RECEIVE_LATCH") == 0);

    /* LATCH-specific aliasing rule: a latch may not participate in '=' alias
     * relationships on either side of the operator.
     */
    if (!is_sync && is_alias) {
        if (sem_expr_has_latch_identifier(lhs, mod_scope) ||
            sem_expr_has_latch_identifier(rhs, mod_scope)) {
            sem_report_rule(diagnostics,
                            stmt->loc,
                            "LATCH_ALIAS_FORBIDDEN",
                            "LATCH signals may not be aliased using '='\n"
                            "latches are level-sensitive storage and must not be merged into other nets");
        }
    }

    /* LATCH-specific placement and operator rules (Section 4.8). */
    if (lhs) {
        JZASTNode *lhs_base = lhs;
        while (lhs_base && lhs_base->type == JZ_AST_EXPR_SLICE && lhs_base->child_count >= 1) {
            lhs_base = lhs_base->children[0];
        }
        if (lhs_base && lhs_base->type == JZ_AST_EXPR_IDENTIFIER && lhs_base->name) {
            const JZSymbol *lhs_sym = module_scope_lookup(mod_scope, lhs_base->name);
            if (lhs_sym && lhs_sym->kind == JZ_SYM_LATCH) {
                const char *latch_type = (lhs_sym->node && lhs_sym->node->block_kind)
                                       ? lhs_sym->node->block_kind
                                       : NULL;
                int is_d_latch  = latch_type && strcmp(latch_type, "D")  == 0;
                int is_sr_latch = latch_type && strcmp(latch_type, "SR") == 0;

                if (is_sync) {
                    /* Latches are level-sensitive storage; they must not be
                     * written from SYNCHRONOUS blocks.
                     */
                    char msg[512];
                    snprintf(msg, sizeof(msg),
                             "LATCH '%s' may not be written in SYNCHRONOUS blocks\n"
                             "latches are level-sensitive; use REGISTER for edge-triggered storage",
                             lhs_base->name);
                    sem_report_rule(diagnostics,
                                    lhs_base->loc,
                                    "LATCH_ASSIGN_IN_SYNC",
                                    msg);
                } else {
                    /* In ASYNCHRONOUS blocks, latch writes must use guarded
                     * '<name> <= ... : ...;' form only (RECEIVE_LATCH).
                     * Any alias or unguarded/extended receive/drive to a
                     * LATCH is illegal.
                     */
                    if (!is_latch_guard) {
                        char msg[512];
                        snprintf(msg, sizeof(msg),
                                 "LATCH '%s' must use guarded assignment syntax: %s <= data : enable;\n"
                                 "unguarded assignments to latches are forbidden",
                                 lhs_base->name, lhs_base->name);
                        sem_report_rule(diagnostics,
                                        lhs_base->loc,
                                        "LATCH_ASSIGN_NON_GUARDED",
                                        msg);
                    }

                    /* For D latches, enforce enable width == 1 on the guard
                     * expression (child[2] of the assignment AST when present).
                     */
                    if (is_latch_guard && is_d_latch && stmt->child_count >= 3) {
                        JZASTNode *enable_ast = stmt->children[2];
                        if (enable_ast) {
                            JZBitvecType en_t;
                            en_t.width = 0;
                            en_t.is_signed = 0;
                            infer_expr_type(enable_ast,
                                            mod_scope,
                                            project_symbols,
                                            diagnostics,
                                            &en_t);
                            if (en_t.width != 1) {
                                char msg[512];
                                snprintf(msg, sizeof(msg),
                                         "D-latch '%s' enable expression has width %u, but must be exactly 1 bit",
                                         lhs_base->name, en_t.width);
                                sem_report_rule(diagnostics,
                                                enable_ast->loc,
                                                "LATCH_ENABLE_WIDTH_NOT_1",
                                                msg);
                            }
                        }
                    }

                    /* SR latches are allowed syntactically; semantic width
                     * rules (set/reset width == latch width) are enforced via
                     * general type/assignment checks.
                     * Additional SR-specific validation can be added here if
                     * needed.
                     */
                    (void)is_sr_latch;
                }
            }
        }
    }

    /* Width checks: enforce ASSIGN_WIDTH_NO_MODIFIER and ASSIGN_TRUNCATES,
     * plus slice/concat-specific width rules.
     */
    JZBitvecType lhs_t, rhs_t;
    lhs_t.width = rhs_t.width = 0;
    lhs_t.is_signed = rhs_t.is_signed = 0;

    infer_expr_type(lhs, mod_scope, project_symbols, diagnostics, &lhs_t);
    infer_expr_type(rhs, mod_scope, project_symbols, diagnostics, &rhs_t);

    unsigned lhs_w = lhs_t.width;
    unsigned rhs_w = rhs_t.width;

    int skip_width_checks = 0;
    if (lhs_is_bus && lhs_bus.is_wildcard) {
        if (lhs_bus.count > 0 && rhs_w > 0 &&
            rhs_w != 1 && rhs_w != lhs_bus.count) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "BUS wildcard [*] has %u elements but RHS width is %u\n"
                     "RHS must be width 1 (broadcast) or %u (one bit per element)",
                     lhs_bus.count, rhs_w, lhs_bus.count);
            sem_report_rule(diagnostics,
                            stmt->loc,
                            "BUS_WILDCARD_WIDTH_MISMATCH",
                            msg);
        }
        skip_width_checks = 1;
    }

    /* For MEM port accesses like mem.port[addr] used on the LHS, treat the
     * slice width as the MEM word width rather than the literal index
     * distance. This preserves the semantic model from Section 7.3 where
     * mem.wr[addr] denotes a full word at address `addr`, not a single bit.
     */
    int lhs_is_mem_slice = 0;
    if (lhs && lhs->type == JZ_AST_EXPR_SLICE) {
        JZMemPortRef lhs_mem_ref;
        memset(&lhs_mem_ref, 0, sizeof(lhs_mem_ref));
        if (sem_match_mem_port_slice(lhs, mod_scope, NULL, &lhs_mem_ref) &&
            lhs_mem_ref.mem_decl && lhs_mem_ref.mem_decl->width) {
            unsigned word_w = 0;
            int rc = eval_simple_positive_decl_int(lhs_mem_ref.mem_decl->width, &word_w);
            if (rc == 1 && word_w > 0) {
                lhs_w = word_w;
                lhs_is_mem_slice = 1;
            }
        }
    }
    if (!lhs_is_mem_slice && lhs && lhs->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER) {
        JZMemPortRef lhs_mem_ref;
        memset(&lhs_mem_ref, 0, sizeof(lhs_mem_ref));
        if (sem_match_mem_port_qualified_ident(lhs, mod_scope, NULL, &lhs_mem_ref) &&
            lhs_mem_ref.field == MEM_PORT_FIELD_ADDR) {
            skip_width_checks = 1;
        }
    }

    /* PORT_TRISTATE_MISMATCH: writing all-'z' literal to IN/OUT ports is
     * forbidden; only INOUT may be tri-stated.
     */
    if (sem_expr_is_all_z_literal_simple(rhs)) {
        /* Peel slices from LHS to get the base identifier. */
        JZASTNode *lhs_base = lhs;
        while (lhs_base && lhs_base->type == JZ_AST_EXPR_SLICE && lhs_base->child_count >= 1) {
            lhs_base = lhs_base->children[0];
        }
        if (lhs_base && lhs_base->type == JZ_AST_EXPR_IDENTIFIER && lhs_base->name) {
            const JZSymbol *sym = module_scope_lookup_kind(mod_scope, lhs_base->name, JZ_SYM_PORT);
            if (sym && sym->node && sym->node->block_kind) {
                const char *dir = sym->node->block_kind;
                if (strcmp(dir, "IN") == 0 || strcmp(dir, "OUT") == 0) {
                    char msg[512];
                    snprintf(msg, sizeof(msg),
                             "'%s' is a PORT %s and cannot be assigned 'z'\n"
                             "only INOUT ports may be tri-stated",
                             lhs_base->name, dir);
                    sem_report_rule(diagnostics,
                                    lhs_base->loc,
                                    "PORT_TRISTATE_MISMATCH",
                                    msg);
                }
            }
        }

        if (lhs_is_bus && lhs_bus.signal_decl && !lhs_bus.writable) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "BUS signal '%s' is read-only for this role and cannot be assigned 'z'\n"
                     "only writable BUS signals (INOUT or OUT from this role) may be tri-stated",
                     lhs_bus.signal_name);
            sem_report_rule(diagnostics,
                            lhs->loc,
                            "BUS_TRISTATE_MISMATCH",
                            msg);
        }
    }

    /* strengthen slice-width inference for simple literal-index slices so that
     * ASSIGN_SLICE_WIDTH_MISMATCH can trigger even when general expression
     * typing is conservative. Skip MEM port slices, which are treated as
     * word-wide accesses (mem.port[addr]) rather than bit/part-selects.
     */
    if (lhs->type == JZ_AST_EXPR_SLICE && !lhs_is_mem_slice) {
        unsigned slice_w = 0;
        if (sem_slice_literal_width(lhs, &slice_w)) {
            lhs_w = slice_w;
        }
    }

    /* Strengthen RHS slice-width inference symmetrically with LHS above.
     * Without this, a bare RHS slice like data[6:0] (or ~data[6:0]) would
     * keep rhs_w == 0, silently bypassing the width comparison below.
     */
    if (rhs->type == JZ_AST_EXPR_SLICE) {
        int rhs_skip = 0;
        JZMemPortRef rhs_mem_ref;
        memset(&rhs_mem_ref, 0, sizeof(rhs_mem_ref));
        if (sem_match_mem_port_slice(rhs, mod_scope, NULL, &rhs_mem_ref) &&
            rhs_mem_ref.mem_decl) {
            rhs_skip = 1;
        }
        /* Also catch qualified identifiers whose prefix is a MEM name. */
        if (!rhs_skip && rhs->child_count >= 1) {
            JZASTNode *rbase = rhs->children[0];
            if (rbase && rbase->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER && rbase->name) {
                const char *dot = strchr(rbase->name, '.');
                if (dot && dot != rbase->name) {
                    char prefix[256];
                    size_t plen = (size_t)(dot - rbase->name);
                    if (plen > 0 && plen < sizeof(prefix)) {
                        memcpy(prefix, rbase->name, plen);
                        prefix[plen] = '\0';
                        if (module_scope_lookup_kind(mod_scope, prefix, JZ_SYM_MEM)) {
                            rhs_skip = 1;
                        }
                    }
                }
            }
            /* MUX selectors use index as entry selector, not bit range. */
            if (!rhs_skip && rbase &&
                rbase->type == JZ_AST_EXPR_IDENTIFIER && rbase->name) {
                if (module_scope_lookup_kind(mod_scope, rbase->name, JZ_SYM_MUX)) {
                    rhs_skip = 1;
                }
            }
        }
        if (!rhs_skip) {
            unsigned slice_w = 0;
            if (sem_slice_literal_width(rhs, &slice_w)) {
                rhs_w = slice_w;
            }
        }
    }

    /* Note: ASYNC_ALIAS_LITERAL_RHS is now enforced via a dedicated recursive
     * walk over ASYNCHRONOUS blocks (see sem_check_block_assignments).
     * We intentionally do not enforce it here to avoid depending on the
     * control-flow walker reaching every nested assignment.
     */

    /* In SYNCHRONOUS blocks, aliasing assignments with '=', '=z', or '=s' are
     * forbidden. Registers and other nets must be driven via directional
     * operators (<=, => and their z/s variants) so that next-state semantics
     * remain well-defined.
     */
    if (is_sync && is_alias) {
        sem_report_rule(diagnostics,
                        stmt->loc,
                        "SYNC_NO_ALIAS",
                        "SYNCHRONOUS blocks do not allow '=' (alias); did you mean '<='?\n"
                        "use '<=' (receive) or '=>' (drive) for register assignments");
        return;
    }

    if (!skip_width_checks && lhs_w > 0 && rhs_w > 0) {
        /* Bare operators require equal widths. */
        if (!has_ext && lhs_w != rhs_w) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "LHS width is %u but RHS width is %u\n"
                     "bare operator requires equal widths; add 'z' or 's' suffix to extend",
                     lhs_w, rhs_w);
            sem_report_rule(diagnostics,
                            stmt->loc,
                            "ASSIGN_WIDTH_NO_MODIFIER",
                            msg);
            /* Also classify under WIDTHS_AND_SLICING for rule coverage. */
            char msg2[320];
            snprintf(msg2, sizeof(msg2),
                     "LHS width %u != RHS width %u; use <=z/<=s or =>z/=>s to zero/sign-extend",
                     lhs_w, rhs_w);
            sem_report_rule(diagnostics,
                            stmt->loc,
                            "WIDTH_ASSIGN_MISMATCH_NO_EXT",
                            msg2);
        }

        /* Truncation detection for directional assignments with explicit
         * extension modifiers. For aliases, z/s only ever widen the narrower
         * side, so no truncation occurs.
         */
        if (!is_alias && has_ext) {
            if (is_drive && lhs_w > rhs_w) {
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "drive '=>' source is %u bits but sink is %u bits\n"
                         "the wider source will be truncated; slice or widen the destination",
                         lhs_w, rhs_w);
                sem_report_rule(diagnostics,
                                stmt->loc,
                                "ASSIGN_TRUNCATES",
                                msg);
            } else if (is_receive && rhs_w > lhs_w) {
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "receive '<=' source is %u bits but destination is %u bits\n"
                         "the wider source will be truncated; slice or widen the destination",
                         rhs_w, lhs_w);
                sem_report_rule(diagnostics,
                                stmt->loc,
                                "ASSIGN_TRUNCATES",
                                msg);
            }
        }

    /* Slice/concat specific mismatches.
     *
     * For MUX lvalues, assignments (including to slices like mux[3]) are
     * classified under MUX_ASSIGN_LHS and treated as read-only violations.
     * In that case we suppress the more generic ASSIGN_SLICE_WIDTH_MISMATCH
     * so that the primary rule remains the MUX_RULES category, matching the
     * spec text for MUX_ASSIGN_LHS.
     */
    if (lhs->type == JZ_AST_EXPR_SLICE && lhs_w != rhs_w) {
        int is_mux_lhs = 0;
        if (lhs->child_count >= 1) {
            JZASTNode *base = lhs->children[0];
            if (base && base->type == JZ_AST_EXPR_IDENTIFIER && base->name) {
                const JZSymbol *base_sym = module_scope_lookup(mod_scope, base->name);
                if (base_sym && base_sym->kind == JZ_SYM_MUX) {
                    is_mux_lhs = 1;
                }
            }
        }

        /* If the slice is of a MUX, rely solely on MUX_ASSIGN_LHS. */
        if (!is_mux_lhs) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "slice width is %u bits but RHS is %u bits",
                     lhs_w, rhs_w);
            sem_report_rule(diagnostics,
                            stmt->loc,
                            "ASSIGN_SLICE_WIDTH_MISMATCH",
                            msg);
        }

        /* For SYNCHRONOUS blocks, also classify register slice width
         * mismatches under the SYNC_SLICE_WIDTH_MISMATCH rule.
         */
        if (is_sync && lhs->child_count >= 1) {
            JZASTNode *base = lhs->children[0];
            if (base && base->type == JZ_AST_EXPR_IDENTIFIER && base->name) {
                const JZSymbol *sym = module_scope_lookup_kind(mod_scope, base->name, JZ_SYM_REGISTER);
                if (sym) {
                    char msg[512];
                    snprintf(msg, sizeof(msg),
                             "register '%s' slice is %u bits but RHS expression is %u bits",
                             base->name, lhs_w, rhs_w);
                    sem_report_rule(diagnostics,
                                    stmt->loc,
                                    "SYNC_SLICE_WIDTH_MISMATCH",
                                    msg);
                }
            }
        }
    }

        if ((lhs->type == JZ_AST_EXPR_CONCAT || rhs->type == JZ_AST_EXPR_CONCAT) &&
            lhs_w != rhs_w) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "concatenation total width is %u bits but paired expression is %u bits",
                     lhs->type == JZ_AST_EXPR_CONCAT ? lhs_w : rhs_w,
                     lhs->type == JZ_AST_EXPR_CONCAT ? rhs_w : lhs_w);
            sem_report_rule(diagnostics,
                            stmt->loc,
                            "ASSIGN_CONCAT_WIDTH_MISMATCH",
                            msg);
        }

        /* FUNC_RESULT_TRUNCATED_SILENTLY: builtin arithmetic function result
         * is wider than the assignment target and would be truncated without
         * an explicit slice.
         */
        if (rhs->type == JZ_AST_EXPR_BUILTIN_CALL &&
            rhs_t.width > 0 && lhs_w > 0 && lhs_w < rhs_t.width &&
            rhs->name &&
            (!strcmp(rhs->name, "uadd") || !strcmp(rhs->name, "sadd") ||
             !strcmp(rhs->name, "umul") || !strcmp(rhs->name, "smul"))) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "%s() result is %u bits but destination is only %u bits\n"
                     "use an explicit slice or widen the destination to avoid silent truncation",
                     rhs->name, rhs_t.width, lhs_w);
            sem_report_rule(diagnostics,
                            stmt->loc,
                            "FUNC_RESULT_TRUNCATED_SILENTLY",
                            msg);
        }
    }

    /* Additional SYNCHRONOUS-only checks on register concatenation LHS. */
    if (is_sync && lhs->type == JZ_AST_EXPR_CONCAT) {
        sem_check_sync_concat_dup_reg(lhs, mod_scope, diagnostics);
    }

    /* MEM-specific access rules (address width & constant-range) on both
     * sides of the assignment.
     */
    sem_check_mem_access_expr(lhs, mod_scope, project_symbols, diagnostics);
    sem_check_mem_access_expr(rhs, mod_scope, project_symbols, diagnostics);

    /* Check for SYNC MEM data reads in ASYNCHRONOUS blocks. This must be done
     * recursively since mem.port.data may appear anywhere in the RHS expression
     * (e.g., inside a ternary condition or concatenation).
     */
    if (!is_sync) {
        sem_check_sync_mem_data_in_expr_recursive(rhs, mod_scope, diagnostics);
    }

    /* MEM read/write context rules.
     * - MEM_WRITE_IN_ASYNC_BLOCK: IN write used in ASYNCHRONOUS block.
     * - MEM_MULTIPLE_WRITES_SAME_IN: track writes per SYNCHRONOUS block.
     * - MEM_READ_SYNC_WITH_EQUALS: synchronous read used with '=' instead of
     *   '<=' in a SYNCHRONOUS block.
     * - MEM_MULTIPLE_ASSIGN_SYNC_READ_OUT: multiple assignments to the same
     *   synchronous read output in one SYNCHRONOUS block.
     */
    JZMemPortRef mem_ref;
    memset(&mem_ref, 0, sizeof(mem_ref));

    /* Writes to MEM ports come from the LHS. Disallow writes to read (OUT)
     * ports entirely, and then apply IN-specific rules.
     */
    JZMemPortRef lhs_qref;
    memset(&lhs_qref, 0, sizeof(lhs_qref));
    int lhs_is_mem_q = sem_match_mem_port_qualified_ident(lhs, mod_scope, diagnostics, &lhs_qref);

    JZMemPortRef rhs_qref;
    memset(&rhs_qref, 0, sizeof(rhs_qref));
    int rhs_is_mem_q = sem_match_mem_port_qualified_ident(rhs, mod_scope, diagnostics, &rhs_qref);

    if (lhs_is_mem_q && lhs_qref.port && lhs_qref.port->block_kind &&
        strcmp(lhs_qref.port->block_kind, "OUT") == 0) {
        if (lhs_qref.field == MEM_PORT_FIELD_DATA) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "%s.%s is an OUT (read) port and cannot be written\n"
                     "use an IN port for write access",
                     lhs_qref.mem_decl && lhs_qref.mem_decl->name ? lhs_qref.mem_decl->name : "mem",
                     lhs_qref.port->name ? lhs_qref.port->name : "port");
            sem_report_rule(diagnostics,
                            lhs->loc,
                            "MEM_WRITE_TO_READ_PORT",
                            msg);
        } else if (lhs_qref.field == MEM_PORT_FIELD_ADDR) {
            if (!lhs_qref.port->text || strcmp(lhs_qref.port->text, "SYNC") != 0) {
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "%s.%s.addr is only valid for SYNC OUT ports\n"
                         "this port is %s, not SYNC",
                         lhs_qref.mem_decl && lhs_qref.mem_decl->name ? lhs_qref.mem_decl->name : "mem",
                         lhs_qref.port->name ? lhs_qref.port->name : "port",
                         lhs_qref.port->text ? lhs_qref.port->text : "ASYNC");
                sem_report_rule(diagnostics,
                                lhs->loc,
                                "MEM_SYNC_ADDR_INVALID_PORT",
                                msg);
            } else if (!is_sync) {
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "%s.%s.addr may only be assigned in SYNCHRONOUS blocks",
                         lhs_qref.mem_decl && lhs_qref.mem_decl->name ? lhs_qref.mem_decl->name : "mem",
                         lhs_qref.port->name ? lhs_qref.port->name : "port");
                sem_report_rule(diagnostics,
                                lhs->loc,
                                "MEM_SYNC_ADDR_IN_ASYNC_BLOCK",
                                msg);
            } else {
                if (!is_receive) {
                    char msg[512];
                    snprintf(msg, sizeof(msg),
                             "%s.%s.addr must use '<=' (receive) operator in SYNCHRONOUS blocks",
                             lhs_qref.mem_decl && lhs_qref.mem_decl->name ? lhs_qref.mem_decl->name : "mem",
                             lhs_qref.port->name ? lhs_qref.port->name : "port");
                    sem_report_rule(diagnostics,
                                    stmt->loc,
                                    "MEM_SYNC_ADDR_WITHOUT_RECEIVE",
                                    msg);
                }
                sem_check_mem_addr_assign(&lhs_qref, rhs, mod_scope, project_symbols, diagnostics);
                if (mem_sync_reads) {
                    size_t count = mem_sync_reads->len / sizeof(JZMemSyncReadAssignKey);
                    JZMemSyncReadAssignKey *arr = (JZMemSyncReadAssignKey *)mem_sync_reads->data;
                    for (size_t i = 0; i < count; ++i) {
                        if (arr[i].mem_decl == lhs_qref.mem_decl &&
                            arr[i].port == lhs_qref.port) {
                            JZASTNode *prev_addr = arr[i].addr_expr;
                            JZASTNode *addr_expr = rhs;
                            int same_addr = 0;
                            if (prev_addr && addr_expr &&
                                prev_addr->type == addr_expr->type) {
                                if ((addr_expr->type == JZ_AST_EXPR_IDENTIFIER ||
                                     addr_expr->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER) &&
                                    prev_addr->name && addr_expr->name &&
                                    strcmp(prev_addr->name, addr_expr->name) == 0) {
                                    same_addr = 1;
                                } else if (addr_expr->type == JZ_AST_EXPR_LITERAL &&
                                           prev_addr->text && addr_expr->text &&
                                           strcmp(prev_addr->text, addr_expr->text) == 0) {
                                    same_addr = 1;
                                }
                            }
                            if (!same_addr) {
                                char msg[512];
                                snprintf(msg, sizeof(msg),
                                         "%s.%s.addr is assigned different addresses in the same SYNCHRONOUS block\n"
                                         "a SYNC read port may only sample one address per clock cycle",
                                         lhs_qref.mem_decl && lhs_qref.mem_decl->name ? lhs_qref.mem_decl->name : "mem",
                                         lhs_qref.port->name ? lhs_qref.port->name : "port");
                                sem_report_rule(diagnostics,
                                                stmt->loc,
                                                "MEM_MULTIPLE_ASSIGN_SYNC_READ_OUT",
                                                msg);
                            }
                            goto done_mem_sync_read_track;
                        }
                    }

                    JZMemSyncReadAssignKey key;
                    key.mem_decl = lhs_qref.mem_decl;
                    key.port = lhs_qref.port;
                    key.dest_decl = NULL;
                    key.addr_expr = rhs;
                    (void)jz_buf_append(mem_sync_reads, &key, sizeof(key));
                }
            }
        } else {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "%s.%s cannot be used directly as a signal\n"
                     "use %s.%s.addr to set the address or %s.%s.data to read data",
                     lhs_qref.mem_decl && lhs_qref.mem_decl->name ? lhs_qref.mem_decl->name : "mem",
                     lhs_qref.port->name ? lhs_qref.port->name : "port",
                     lhs_qref.mem_decl && lhs_qref.mem_decl->name ? lhs_qref.mem_decl->name : "mem",
                     lhs_qref.port->name ? lhs_qref.port->name : "port",
                     lhs_qref.mem_decl && lhs_qref.mem_decl->name ? lhs_qref.mem_decl->name : "mem",
                     lhs_qref.port->name ? lhs_qref.port->name : "port");
            sem_report_rule(diagnostics,
                            lhs->loc,
                            "MEM_PORT_USED_AS_SIGNAL",
                            msg);
        }
    }

    /* INOUT port access rules:
     * - .addr and .wdata may only be assigned in SYNCHRONOUS blocks with '<='
     * - .data is read-only (handled implicitly; assigning to it is an error)
     */
    if (lhs_is_mem_q && lhs_qref.port && lhs_qref.port->block_kind &&
        strcmp(lhs_qref.port->block_kind, "INOUT") == 0) {
        if (lhs_qref.field == MEM_PORT_FIELD_DATA) {
            /* .data is read-only on INOUT ports */
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "%s.%s.data is read-only on INOUT ports; use .wdata to write",
                     lhs_qref.mem_decl && lhs_qref.mem_decl->name ? lhs_qref.mem_decl->name : "mem",
                     lhs_qref.port->name ? lhs_qref.port->name : "port");
            sem_report_rule(diagnostics,
                            lhs->loc,
                            "MEM_WRITE_TO_READ_PORT",
                            msg);
        } else if (lhs_qref.field == MEM_PORT_FIELD_ADDR) {
            if (!is_sync) {
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "%s.%s.addr may only be assigned in SYNCHRONOUS blocks",
                         lhs_qref.mem_decl && lhs_qref.mem_decl->name ? lhs_qref.mem_decl->name : "mem",
                         lhs_qref.port->name ? lhs_qref.port->name : "port");
                sem_report_rule(diagnostics,
                                lhs->loc,
                                "MEM_INOUT_ADDR_IN_ASYNC",
                                msg);
            } else if (!is_receive) {
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "%s.%s.addr must use '<=' (receive) operator",
                         lhs_qref.mem_decl && lhs_qref.mem_decl->name ? lhs_qref.mem_decl->name : "mem",
                         lhs_qref.port->name ? lhs_qref.port->name : "port");
                sem_report_rule(diagnostics,
                                stmt->loc,
                                "MEM_INOUT_WDATA_WRONG_OP",
                                msg);
            } else {
                sem_check_mem_addr_assign(&lhs_qref, rhs, mod_scope, project_symbols, diagnostics);
            }
        } else if (lhs_qref.field == MEM_PORT_FIELD_WDATA) {
            if (!is_sync) {
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "%s.%s.wdata may only be assigned in SYNCHRONOUS blocks",
                         lhs_qref.mem_decl && lhs_qref.mem_decl->name ? lhs_qref.mem_decl->name : "mem",
                         lhs_qref.port->name ? lhs_qref.port->name : "port");
                sem_report_rule(diagnostics,
                                lhs->loc,
                                "MEM_INOUT_WDATA_IN_ASYNC",
                                msg);
            } else if (!is_receive) {
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "%s.%s.wdata must use '<=' (receive) operator",
                         lhs_qref.mem_decl && lhs_qref.mem_decl->name ? lhs_qref.mem_decl->name : "mem",
                         lhs_qref.port->name ? lhs_qref.port->name : "port");
                sem_report_rule(diagnostics,
                                stmt->loc,
                                "MEM_INOUT_WDATA_WRONG_OP",
                                msg);
            }
        } else if (lhs_qref.field == MEM_PORT_FIELD_NONE) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "%s.%s cannot be used directly as a signal\n"
                     "use .addr, .data, or .wdata pseudo-fields",
                     lhs_qref.mem_decl && lhs_qref.mem_decl->name ? lhs_qref.mem_decl->name : "mem",
                     lhs_qref.port->name ? lhs_qref.port->name : "port");
            sem_report_rule(diagnostics,
                            lhs->loc,
                            "MEM_PORT_USED_AS_SIGNAL",
                            msg);
        }
    }

    /* IN (write) port field access: .addr/.data are invalid on IN ports.
     * IN ports require bracket syntax: mem.port[addr] <= data;
     */
    if (lhs_is_mem_q && lhs_qref.port && lhs_qref.port->block_kind &&
        strcmp(lhs_qref.port->block_kind, "IN") == 0) {
        const char *mem_name = lhs_qref.mem_decl && lhs_qref.mem_decl->name
                               ? lhs_qref.mem_decl->name : "mem";
        const char *port_name = lhs_qref.port->name ? lhs_qref.port->name : "port";
        char msg[512];
        snprintf(msg, sizeof(msg),
                 "%s.%s is an IN (write) port; use bracket syntax: %s.%s[addr] <= data",
                 mem_name, port_name, mem_name, port_name);
        sem_report_rule(diagnostics,
                        lhs->loc,
                        "MEM_IN_PORT_FIELD_ACCESS",
                        msg);
    }

    if (sem_match_mem_port_slice(lhs, mod_scope, diagnostics, &mem_ref) &&
        mem_ref.port && mem_ref.port->block_kind) {
        if (strcmp(mem_ref.port->block_kind, "OUT") == 0) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "%s.%s is an OUT (read) port and cannot be written\n"
                     "use an IN port for write access",
                     mem_ref.mem_decl && mem_ref.mem_decl->name ? mem_ref.mem_decl->name : "mem",
                     mem_ref.port->name ? mem_ref.port->name : "port");
            sem_report_rule(diagnostics,
                            lhs->loc,
                            "MEM_WRITE_TO_READ_PORT",
                            msg);
        } else if (strcmp(mem_ref.port->block_kind, "IN") == 0) {
            if (!is_sync) {
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "%s.%s is an IN (write) port and may only be written in SYNCHRONOUS blocks",
                         mem_ref.mem_decl && mem_ref.mem_decl->name ? mem_ref.mem_decl->name : "mem",
                         mem_ref.port->name ? mem_ref.port->name : "port");
                sem_report_rule(diagnostics,
                                lhs->loc,
                                "MEM_WRITE_IN_ASYNC_BLOCK",
                                msg);
            } else if (mem_out_writes) {
                sem_track_mem_out_write(mem_out_writes, &mem_ref, diagnostics, lhs->loc);
            }
        } else if (strcmp(mem_ref.port->block_kind, "INOUT") == 0) {
            /* INOUT ports may not be indexed; must use .addr/.data/.wdata */
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "%s.%s is an INOUT port and may not be indexed directly\n"
                     "use .addr, .data, or .wdata pseudo-fields instead",
                     mem_ref.mem_decl && mem_ref.mem_decl->name ? mem_ref.mem_decl->name : "mem",
                     mem_ref.port->name ? mem_ref.port->name : "port");
            sem_report_rule(diagnostics,
                            lhs->loc,
                            "MEM_INOUT_INDEXED",
                            msg);
        }
    }

    /* Synchronous read ports must be used with RECEIVE (`<=`) in SYNCHRONOUS
     * blocks; using '=' is a MEM_READ_SYNC_WITH_EQUALS violation.
     */
    memset(&mem_ref, 0, sizeof(mem_ref));
    if (sem_match_mem_port_slice(rhs, mod_scope, diagnostics, &mem_ref) &&
        mem_ref.port && mem_ref.port->block_kind) {
        if (strcmp(mem_ref.port->block_kind, "IN") == 0) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "%s.%s is an IN (write) port and cannot be read\n"
                     "use an OUT port for read access",
                     mem_ref.mem_decl && mem_ref.mem_decl->name ? mem_ref.mem_decl->name : "mem",
                     mem_ref.port->name ? mem_ref.port->name : "port");
            sem_report_rule(diagnostics,
                            rhs->loc,
                            "MEM_READ_FROM_WRITE_PORT",
                            msg);
        } else if (strcmp(mem_ref.port->block_kind, "INOUT") == 0) {
            /* INOUT ports may not be indexed; must use .addr/.data/.wdata */
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "%s.%s is an INOUT port and may not be indexed directly\n"
                     "use .addr, .data, or .wdata pseudo-fields instead",
                     mem_ref.mem_decl && mem_ref.mem_decl->name ? mem_ref.mem_decl->name : "mem",
                     mem_ref.port->name ? mem_ref.port->name : "port");
            sem_report_rule(diagnostics,
                            rhs->loc,
                            "MEM_INOUT_INDEXED",
                            msg);
        }
    }

    if (rhs_is_mem_q && rhs_qref.port && rhs_qref.port->block_kind &&
        strcmp(rhs_qref.port->block_kind, "IN") == 0) {
        char msg[512];
        snprintf(msg, sizeof(msg),
                 "%s.%s is an IN (write) port and cannot be read\n"
                 "use an OUT port for read access",
                 rhs_qref.mem_decl && rhs_qref.mem_decl->name ? rhs_qref.mem_decl->name : "mem",
                 rhs_qref.port->name ? rhs_qref.port->name : "port");
        sem_report_rule(diagnostics,
                        rhs->loc,
                        "MEM_READ_FROM_WRITE_PORT",
                        msg);
    }

    if (rhs_is_mem_q && rhs_qref.port && rhs_qref.port->block_kind &&
        strcmp(rhs_qref.port->block_kind, "OUT") == 0) {
        if (rhs_qref.field == MEM_PORT_FIELD_NONE) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "%s.%s cannot be used directly as a signal\n"
                     "use %s.%s.data to read or %s.%s.addr to set address",
                     rhs_qref.mem_decl && rhs_qref.mem_decl->name ? rhs_qref.mem_decl->name : "mem",
                     rhs_qref.port->name ? rhs_qref.port->name : "port",
                     rhs_qref.mem_decl && rhs_qref.mem_decl->name ? rhs_qref.mem_decl->name : "mem",
                     rhs_qref.port->name ? rhs_qref.port->name : "port",
                     rhs_qref.mem_decl && rhs_qref.mem_decl->name ? rhs_qref.mem_decl->name : "mem",
                     rhs_qref.port->name ? rhs_qref.port->name : "port");
            sem_report_rule(diagnostics,
                            rhs->loc,
                            "MEM_PORT_USED_AS_SIGNAL",
                            msg);
        } else if (rhs_qref.field == MEM_PORT_FIELD_ADDR) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "%s.%s.addr is a write-only input and cannot be read\n"
                     "use %s.%s.data to read memory output",
                     rhs_qref.mem_decl && rhs_qref.mem_decl->name ? rhs_qref.mem_decl->name : "mem",
                     rhs_qref.port->name ? rhs_qref.port->name : "port",
                     rhs_qref.mem_decl && rhs_qref.mem_decl->name ? rhs_qref.mem_decl->name : "mem",
                     rhs_qref.port->name ? rhs_qref.port->name : "port");
            sem_report_rule(diagnostics,
                            rhs->loc,
                            "MEM_PORT_ADDR_READ",
                            msg);
        } else if (rhs_qref.field == MEM_PORT_FIELD_DATA &&
                   rhs_qref.port->text && strcmp(rhs_qref.port->text, "ASYNC") == 0) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "%s.%s is an ASYNC read port; use %s.%s[addr] indexed syntax instead of .data",
                     rhs_qref.mem_decl && rhs_qref.mem_decl->name ? rhs_qref.mem_decl->name : "mem",
                     rhs_qref.port->name ? rhs_qref.port->name : "port",
                     rhs_qref.mem_decl && rhs_qref.mem_decl->name ? rhs_qref.mem_decl->name : "mem",
                     rhs_qref.port->name ? rhs_qref.port->name : "port");
            sem_report_rule(diagnostics,
                            rhs->loc,
                            "MEM_ASYNC_PORT_FIELD_DATA",
                            msg);
        } else if (rhs_qref.field == MEM_PORT_FIELD_DATA &&
                   rhs_qref.port->text && strcmp(rhs_qref.port->text, "SYNC") == 0 &&
                   !is_sync) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "%s.%s.data is a SYNC read output and may not be read in ASYNCHRONOUS blocks\n"
                     "move the read into a SYNCHRONOUS block",
                     rhs_qref.mem_decl && rhs_qref.mem_decl->name ? rhs_qref.mem_decl->name : "mem",
                     rhs_qref.port->name ? rhs_qref.port->name : "port");
            sem_report_rule(diagnostics,
                            rhs->loc,
                            "MEM_SYNC_DATA_IN_ASYNC_BLOCK",
                            msg);
        }
    }

    memset(&mem_ref, 0, sizeof(mem_ref));
    if (is_sync && rhs_is_mem_q && rhs_qref.port && rhs_qref.port->block_kind &&
        strcmp(rhs_qref.port->block_kind, "OUT") == 0 &&
        rhs_qref.port->text && strcmp(rhs_qref.port->text, "SYNC") == 0 &&
        rhs_qref.field == MEM_PORT_FIELD_DATA &&
        !is_receive) {
        char msg[512];
        snprintf(msg, sizeof(msg),
                 "%s.%s.data must use '<=' (receive) operator in SYNCHRONOUS blocks\n"
                 "'=' aliasing is not allowed for synchronous MEM reads",
                 rhs_qref.mem_decl && rhs_qref.mem_decl->name ? rhs_qref.mem_decl->name : "mem",
                 rhs_qref.port->name ? rhs_qref.port->name : "port");
        sem_report_rule(diagnostics,
                        stmt->loc,
                        "MEM_READ_SYNC_WITH_EQUALS",
                        msg);
    }

done_mem_sync_read_track: ;

    /* Target-kind checks (register vs wire/port/MUX) and basic block rules.
     *
     * The driven signal depends on the directional operator semantics:
     *   - DRIVE*:  source => sink   (RHS is driven)
     *   - RECEIVE*: sink <= source  (LHS is driven)
     *   - plain '=' or anything else: LHS is driven.
     */
    {
        JZASTNode *target = lhs;
        if (is_drive) {
            target = rhs;
        }
        /* For bus-to-bus alias ('='), skip the individual writable check on
         * the LHS because the compatibility is validated in the direction
         * expression checker (both-readable / both-writable checks). */
        int bus_alias = 0;
        if (is_alias && lhs && rhs && project_symbols) {
            JZBusAccessInfo li, ri;
            int lbus = sem_resolve_bus_access(lhs, mod_scope, project_symbols, &li, NULL) && li.signal_decl;
            int rbus = sem_resolve_bus_access(rhs, mod_scope, project_symbols, &ri, NULL) && ri.signal_decl;
            bus_alias = lbus && rbus;
        }
        sem_check_lvalue_targets_recursive(target, mod_scope, project_symbols, diagnostics, is_sync, bus_alias);
    }
}

void sem_check_block_assignments(JZASTNode *block,
                                        const JZModuleScope *mod_scope,
                                        const JZBuffer *project_symbols,
                                        JZDiagnosticList *diagnostics,
                                        int is_sync,
                                        JZBuffer *mem_out_writes,
                                        JZBuffer *mem_sync_reads)
{
    if (!block || !mod_scope) return;

    /* When called on a bare statement node (e.g. from IF/ELSE body children
     * which are individual statements rather than block containers), handle
     * the statement directly rather than iterating its expression children.
     */
    if (block->type == JZ_AST_STMT_ASSIGN) {
        sem_check_assignment_stmt(block, mod_scope, project_symbols, diagnostics, is_sync, mem_out_writes, mem_sync_reads);
        return;
    }

    /* For ASYNCHRONOUS blocks, run a dedicated walk that enforces
     * ASYNC_ALIAS_LITERAL_RHS on every alias assignment, regardless of how
     * deeply it is nested under IF/ELSE/SELECT/CASE.
     */
    if (!is_sync && block->type == JZ_AST_BLOCK &&
        block->block_kind && strcmp(block->block_kind, "ASYNCHRONOUS") == 0) {
        sem_check_async_alias_literal_rhs_recursive(block, diagnostics);
    }

    for (size_t i = 0; i < block->child_count; ++i) {
        JZASTNode *stmt = block->children[i];
        if (!stmt) continue;

        switch (stmt->type) {
        case JZ_AST_STMT_ASSIGN:
            sem_check_assignment_stmt(stmt, mod_scope, project_symbols, diagnostics, is_sync, mem_out_writes, mem_sync_reads);
            break;

        case JZ_AST_STMT_IF:
        case JZ_AST_STMT_ELIF:
            /* child[0] is the condition; remaining children form the body. */
            for (size_t j = 1; j < stmt->child_count; ++j) {
                sem_check_block_assignments(stmt->children[j], mod_scope, project_symbols, diagnostics, is_sync, mem_out_writes, mem_sync_reads);
            }
            break;

        case JZ_AST_STMT_ELSE:
            for (size_t j = 0; j < stmt->child_count; ++j) {
                sem_check_block_assignments(stmt->children[j], mod_scope, project_symbols, diagnostics, is_sync, mem_out_writes, mem_sync_reads);
            }
            break;

        case JZ_AST_STMT_SELECT:
            /* child[0] is selector expression; remaining are CASE/DEFAULT.
             * Each CASE/DEFAULT body executes exclusively, so we track
             * MEM sync-read usage per branch. Unconditional reads that
             * occur before the SELECT (and are present in mem_sync_reads)
             * are still visible inside each branch via an initial copy.
             */
            for (size_t j = 1; j < stmt->child_count; ++j) {
                JZBuffer branch_reads = {0};
                if (mem_sync_reads && mem_sync_reads->data && mem_sync_reads->len > 0) {
                    (void)jz_buf_append(&branch_reads,
                                        mem_sync_reads->data,
                                        mem_sync_reads->len);
                }
                sem_check_block_assignments(stmt->children[j],
                                            mod_scope,
                                            project_symbols,
                                            diagnostics,
                                            is_sync,
                                            mem_out_writes,
                                            &branch_reads);
                jz_buf_free(&branch_reads);
            }
            break;

        case JZ_AST_STMT_CASE:
            /* child[0] is case value; remaining children are body statements. */
            for (size_t j = 1; j < stmt->child_count; ++j) {
                sem_check_block_assignments(stmt->children[j], mod_scope, project_symbols, diagnostics, is_sync, mem_out_writes, mem_sync_reads);
            }
            break;

        case JZ_AST_STMT_DEFAULT:
            for (size_t j = 0; j < stmt->child_count; ++j) {
                sem_check_block_assignments(stmt->children[j], mod_scope, project_symbols, diagnostics, is_sync, mem_out_writes, mem_sync_reads);
            }
            break;

        default:
            break;
        }
    }
}
