#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

#include "sem_driver.h"
#include "sem.h"
#include "util.h"
#include "rules.h"
#include "driver_internal.h"

static void sem_check_runtime_const_config_expr(const JZASTNode *node,
                                                const JZModuleScope *mod_scope,
                                                JZDiagnosticList *diagnostics)
{
    if (!node || !mod_scope) return;

    if (node->type == JZ_AST_EXPR_BUILTIN_CALL && node->name &&
        strcmp(node->name, "lit") == 0) {
        /* lit() arguments are compile-time expressions; CONST/CONFIG usage is
         * validated elsewhere and is permitted inside lit(). */
        return;
    }

    if (node->type == JZ_AST_EXPR_IDENTIFIER && node->name) {
        const JZSymbol *sym = module_scope_lookup(mod_scope, node->name);
        if (sym && sym->kind == JZ_SYM_CONST) {
            char explain[256];
            snprintf(explain, sizeof(explain),
                     "'%s' is a CONST and cannot appear in runtime expressions.\n"
                     "CONST values are compile-time only — valid in widths, depths,\n"
                     "slice indices, and OVERRIDE blocks.",
                     node->name);
            sem_report_rule(diagnostics,
                            node->loc,
                            "CONST_USED_WHERE_FORBIDDEN",
                            explain);
        }
    } else if (node->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER && node->name) {
        const char *full = node->name;
        const char *dot  = strchr(full, '.');
        if (dot && *(dot + 1)) {
            char head[256];
            size_t head_len = (size_t)(dot - full);
            if (head_len >= sizeof(head)) head_len = sizeof(head) - 1;
            memcpy(head, full, head_len);
            head[head_len] = '\0';

            if (strcmp(head, "CONFIG") == 0) {
                /* Treat any CONFIG.<name> appearing in an executable block as
                 * a misuse, regardless of whether <name> itself is declared;
                 * CONFIG_USE_UNDECLARED handles the declaration aspect.
                 */
                char explain[256];
                snprintf(explain, sizeof(explain),
                         "'%s' is a CONFIG reference and cannot appear in runtime\n"
                         "expressions. CONFIG values are compile-time only — valid\n"
                         "in widths, depths, MEM attributes, and OVERRIDE blocks.",
                         full);
                sem_report_rule(diagnostics,
                                node->loc,
                                "CONFIG_USED_WHERE_FORBIDDEN",
                                explain);
            }
        }
    }

    /* Skip MEM address expressions: CONST/CONFIG are allowed in MEM indices,
     * and dedicated MEM_* rules (MEM_CONST_ADDR_OUT_OF_RANGE,
     * MEM_ADDR_WIDTH_TOO_WIDE, etc.) handle those contexts.
     */
    if (node->type == JZ_AST_EXPR_SLICE) {
        JZMemPortRef mem_ref;
        memset(&mem_ref, 0, sizeof(mem_ref));
        if (sem_match_mem_port_slice((JZASTNode *)node, mod_scope, NULL, &mem_ref)) {
            return;
        }
    }
    if (node->type == JZ_AST_STMT_ASSIGN && node->child_count > 1) {
        JZASTNode *lhs = node->children[0];
        JZMemPortRef mem_ref;
        memset(&mem_ref, 0, sizeof(mem_ref));
        if (sem_match_mem_port_qualified_ident(lhs, mod_scope, NULL, &mem_ref) &&
            mem_ref.port && mem_ref.port->block_kind &&
            strcmp(mem_ref.port->block_kind, "OUT") == 0 &&
            mem_ref.port->text && strcmp(mem_ref.port->text, "SYNC") == 0 &&
            mem_ref.field == MEM_PORT_FIELD_ADDR) {
            return;
        }
    }

    /* @feature conditions are compile-time constant expressions; CONFIG/CONST
     * references are valid there. Skip the entire FEATURE_GUARD node to avoid
     * false CONFIG_USED_WHERE_FORBIDDEN errors on the condition. The THEN/ELSE
     * bodies are validated separately by sem_check_block_expressions_inner.
     */
    if (node->type == JZ_AST_FEATURE_GUARD) {
        return;
    }

    /* Recurse into children, but avoid treating assignment LHS as a runtime
     * expression. CONST_USED_WHERE_FORBIDDEN is intended for value contexts
     * (RHS, conditions, operands), not for illegal assignment targets, which
     * are handled by other rules.
     */
    if (node->type == JZ_AST_STMT_ASSIGN && node->child_count > 0) {
        for (size_t i = 1; i < node->child_count; ++i) {
            if (node->children[i]) {
                sem_check_runtime_const_config_expr(node->children[i], mod_scope, diagnostics);
            }
        }
        return;
    }

    /* Slice indices (children[1] and children[2]) are compile-time constant
     * expression contexts where CONST/CONFIG names are allowed per S3.2.
     * Only recurse into child[0] (the base signal expression).
     */
    if (node->type == JZ_AST_EXPR_SLICE) {
        if (node->child_count > 0 && node->children[0]) {
            sem_check_runtime_const_config_expr(node->children[0], mod_scope, diagnostics);
        }
        return;
    }

    for (size_t i = 0; i < node->child_count; ++i) {
        if (node->children[i]) {
            sem_check_runtime_const_config_expr(node->children[i], mod_scope, diagnostics);
        }
    }
}

/* ---------------------------------------------------------------------------
 * Bare integer literal check: reject unsized decimal integers (e.g. 1, 42)
 * in runtime expression contexts.  Bare integers are valid in compile-time
 * contexts (CONST, CONFIG, widths, slice indices, replication counts, lit()
 * arguments) but not in ASYNCHRONOUS/SYNCHRONOUS block expressions.
 * ---------------------------------------------------------------------------
 */
static void sem_check_bare_integer_in_expr(const JZASTNode *node,
                                           JZDiagnosticList *diagnostics)
{
    if (!node) return;

    /* Builtin call arguments may include compile-time integer parameters
     * (e.g. lit(width, value), b2oh(idx, width), gbit, sbit, gslice, etc.).
     * Each builtin validates its own arguments via dedicated rules; skip all
     * builtin children to avoid false LIT_BARE_INTEGER on compile-time args.
     */
    if (node->type == JZ_AST_EXPR_BUILTIN_CALL) {
        return;
    }

    /* Bus array indices (link[2].tx) are compile-time; skip children. */
    if (node->type == JZ_AST_EXPR_BUS_ACCESS) {
        return;
    }

    /* CASE label values (CASE 0 { ... }) are compile-time pattern constants;
     * skip children[0] (the value), recurse into children[1+] (the body).
     */
    if (node->type == JZ_AST_STMT_CASE) {
        for (size_t i = 1; i < node->child_count; ++i) {
            if (node->children[i]) {
                sem_check_bare_integer_in_expr(node->children[i], diagnostics);
            }
        }
        return;
    }

    /* @feature conditions are compile-time; skip entirely. */
    if (node->type == JZ_AST_FEATURE_GUARD) {
        return;
    }

    /* Check: if this is a literal node whose text has no tick, it's bare. */
    if (node->type == JZ_AST_EXPR_LITERAL && node->text) {
        const char *tick = strchr(node->text, '\'');
        if (!tick) {
            char explain[256];
            snprintf(explain, sizeof(explain),
                     "bare integer '%s' is not permitted in runtime expressions; "
                     "use a sized literal (e.g. %s'd%s) or lit(width, %s)",
                     node->text, node->text, node->text, node->text);
            sem_report_rule(diagnostics,
                            node->loc,
                            "LIT_BARE_INTEGER",
                            explain);
        }
    }

    /* Slice indices (children[1], children[2]) are compile-time; skip them. */
    if (node->type == JZ_AST_EXPR_SLICE) {
        if (node->child_count > 0 && node->children[0]) {
            sem_check_bare_integer_in_expr(node->children[0], diagnostics);
        }
        return;
    }

    /* For assignments, only check RHS (children[1+]). */
    if (node->type == JZ_AST_STMT_ASSIGN && node->child_count > 0) {
        for (size_t i = 1; i < node->child_count; ++i) {
            if (node->children[i]) {
                sem_check_bare_integer_in_expr(node->children[i], diagnostics);
            }
        }
        return;
    }

    for (size_t i = 0; i < node->child_count; ++i) {
        if (node->children[i]) {
            sem_check_bare_integer_in_expr(node->children[i], diagnostics);
        }
    }
}

/*
 * Feature Guard helpers: validate @feature condition expressions and recursively
 * analyze their guarded bodies.
 */

static void sem_check_feature_guard_cond(JZASTNode *feature,
                                         const JZModuleScope *mod_scope,
                                         const JZBuffer *project_symbols,
                                         JZDiagnosticList *diagnostics)
{
    if (!feature || feature->type != JZ_AST_FEATURE_GUARD || !mod_scope) return;
    if (feature->child_count == 0) return;

    JZASTNode *cond = feature->children[0];
    if (!cond) return;

    /* Width-1 boolean requirement. */
    JZBitvecType cond_t;
    infer_expr_type(cond, mod_scope, project_symbols, diagnostics, &cond_t);
    if (cond_t.width > 0 && cond_t.width != 1u) {
        char explain[256];
        snprintf(explain, sizeof(explain),
                 "@feature condition has width [%u] but must be width [1].\n"
                 "Use a comparison operator to produce a 1-bit boolean result.",
                 cond_t.width);
        sem_report_rule(diagnostics,
                        cond->loc,
                        "FEATURE_COND_WIDTH_NOT_1",
                        explain);
    }

    /* Enforce that the condition references only CONFIG.<name>, module CONST,
     * literals, and logical/arithmetic operators. Reuse symbol tables.
     */
    JZBuffer stack = {0}; /* simple DFS stack of nodes to visit */
    (void)jz_buf_append(&stack, &cond, sizeof(JZASTNode *));

    while (stack.len >= sizeof(JZASTNode *)) {
        JZASTNode *cur = NULL;
        memcpy(&cur, (char *)stack.data + stack.len - sizeof(JZASTNode *), sizeof(JZASTNode *));
        stack.len -= sizeof(JZASTNode *);
        if (!cur) continue;

        switch (cur->type) {
        case JZ_AST_EXPR_LITERAL:
            /* always allowed */
            break;

        case JZ_AST_EXPR_IDENTIFIER: {
            if (!cur->name) break;
            const JZSymbol *sym = module_scope_lookup(mod_scope, cur->name);
            if (!sym) {
                /* Unresolved here; resolve_names_recursive will report separately. */
                break;
            }
            if (sym->kind != JZ_SYM_CONST) {
                const char *kind_str = "identifier";
                if (sym->kind == JZ_SYM_REGISTER) kind_str = "REGISTER";
                else if (sym->kind == JZ_SYM_PORT) kind_str = "PORT";
                else if (sym->kind == JZ_SYM_WIRE) kind_str = "WIRE";
                else if (sym->kind == JZ_SYM_LATCH) kind_str = "LATCH";
                char explain[256];
                snprintf(explain, sizeof(explain),
                         "'%s' is a %s, not a CONST. @feature conditions may only\n"
                         "reference CONFIG.<name>, module CONST, and literals.",
                         cur->name, kind_str);
                sem_report_rule(diagnostics,
                                cur->loc,
                                "FEATURE_EXPR_INVALID_CONTEXT",
                                explain);
            }
            break;
        }

        case JZ_AST_EXPR_QUALIFIED_IDENTIFIER: {
            if (!cur->name || !project_symbols || !project_symbols->data) {
                char explain[256];
                snprintf(explain, sizeof(explain),
                         "'%s' cannot be resolved in @feature condition.\n"
                         "Only CONFIG.<name>, module CONST, and literals are allowed.",
                         cur->name ? cur->name : "?");
                sem_report_rule(diagnostics,
                                cur->loc,
                                "FEATURE_EXPR_INVALID_CONTEXT",
                                explain);
                break;
            }
            const char *full = cur->name;
            const char *dot = strchr(full, '.');
            if (!dot || dot == full || !*(dot + 1)) {
                char explain[256];
                snprintf(explain, sizeof(explain),
                         "'%s' is not a valid qualified name in @feature condition.\n"
                         "Only CONFIG.<name>, module CONST, and literals are allowed.",
                         full);
                sem_report_rule(diagnostics,
                                cur->loc,
                                "FEATURE_EXPR_INVALID_CONTEXT",
                                explain);
                break;
            }
            size_t head_len = (size_t)(dot - full);
            char head[32];
            if (head_len >= sizeof(head)) head_len = sizeof(head) - 1u;
            memcpy(head, full, head_len);
            head[head_len] = '\0';
            const char *tail = dot + 1;
            if (strcmp(head, "CONFIG") != 0) {
                char explain[256];
                snprintf(explain, sizeof(explain),
                         "'%s' uses namespace '%s' but only CONFIG.<name> is allowed\n"
                         "in @feature conditions.",
                         full, head);
                sem_report_rule(diagnostics,
                                cur->loc,
                                "FEATURE_EXPR_INVALID_CONTEXT",
                                explain);
                break;
            }
            /* Ensure CONFIG.<tail> exists. */
            const JZSymbol *syms = (const JZSymbol *)project_symbols->data;
            size_t count = project_symbols->len / sizeof(JZSymbol);
            int found = 0;
            for (size_t i = 0; i < count; ++i) {
                if (syms[i].kind == JZ_SYM_CONFIG && syms[i].name &&
                    strcmp(syms[i].name, tail) == 0) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                char explain[256];
                snprintf(explain, sizeof(explain),
                         "CONFIG.%s is not declared in the project CONFIG block.",
                         tail);
                sem_report_rule(diagnostics,
                                cur->loc,
                                "CONFIG_USE_UNDECLARED",
                                explain);
            }
            break;
        }

        default:
            /* Recurse into children for composite expressions. */
            for (size_t ci = 0; ci < cur->child_count; ++ci) {
                if (!cur->children[ci]) continue;
                (void)jz_buf_append(&stack, &cur->children[ci], sizeof(JZASTNode *));
            }
            break;
        }
    }

    jz_buf_free(&stack);
}

/* Shared helper for @check compile-time assertions: ensure that an expression
 * is restricted to literals, module CONST, CONFIG.<name>, and allowed builtin
 * functions (clog2/widthof) combined with arithmetic/relational/logical
 * operators.
 *
 * Returns 0 when the subtree is allowed, non-zero after reporting at least
 * one diagnostic.
 */
static int sem_check_check_expr_allowed(JZASTNode *expr,
                                        const JZModuleScope *mod_scope,
                                        const JZBuffer *project_symbols,
                                        int in_project_scope,
                                        JZDiagnosticList *diagnostics)
{
    if (!expr) return 0;

    switch (expr->type) {
    case JZ_AST_EXPR_LITERAL:
        /* Always allowed. */
        return 0;

    case JZ_AST_EXPR_IDENTIFIER: {
        if (!expr->name) return 0;
        if (in_project_scope) {
            /* Bare identifiers are not allowed in project-scope @check. */
            char explain[256];
            snprintf(explain, sizeof(explain),
                     "'%s' is a bare identifier but project-scope @check may only\n"
                     "reference CONFIG.<name> and literals.",
                     expr->name);
            sem_report_rule(diagnostics,
                            expr->loc,
                            "CHECK_INVALID_EXPR_TYPE",
                            explain);
            return -1;
        }
        if (!mod_scope) return 0;
        const JZSymbol *sym = module_scope_lookup(mod_scope, expr->name);
        if (!sym) {
            /* Unresolved here; resolve_names_recursive will report separately. */
            return 0;
        }
        if (sym->kind != JZ_SYM_CONST) {
            const char *kind_str = "identifier";
            if (sym->kind == JZ_SYM_REGISTER) kind_str = "REGISTER";
            else if (sym->kind == JZ_SYM_PORT) kind_str = "PORT";
            else if (sym->kind == JZ_SYM_WIRE) kind_str = "WIRE";
            else if (sym->kind == JZ_SYM_LATCH) kind_str = "LATCH";
            char explain[256];
            snprintf(explain, sizeof(explain),
                     "'%s' is a %s, not a CONST. @check conditions may only\n"
                     "reference module CONST, CONFIG.<name>, and literals.",
                     expr->name, kind_str);
            sem_report_rule(diagnostics,
                            expr->loc,
                            "CHECK_INVALID_EXPR_TYPE",
                            explain);
            return -1;
        }
        return 0;
    }

    case JZ_AST_EXPR_QUALIFIED_IDENTIFIER: {
        if (!expr->name || !project_symbols || !project_symbols->data) {
            char explain[256];
            snprintf(explain, sizeof(explain),
                     "'%s' cannot be resolved in @check condition.\n"
                     "Only module CONST, CONFIG.<name>, and literals are allowed.",
                     expr->name ? expr->name : "?");
            sem_report_rule(diagnostics,
                            expr->loc,
                            "CHECK_INVALID_EXPR_TYPE",
                            explain);
            return -1;
        }
        const char *full = expr->name;
        const char *dot = strchr(full, '.');
        if (!dot || dot == full || !*(dot + 1)) {
            char explain[256];
            snprintf(explain, sizeof(explain),
                     "'%s' is not a valid qualified name in @check condition.\n"
                     "Only module CONST, CONFIG.<name>, and literals are allowed.",
                     full);
            sem_report_rule(diagnostics,
                            expr->loc,
                            "CHECK_INVALID_EXPR_TYPE",
                            explain);
            return -1;
        }
        size_t head_len = (size_t)(dot - full);
        char head[32];
        if (head_len >= sizeof(head)) head_len = sizeof(head) - 1u;
        memcpy(head, full, head_len);
        head[head_len] = '\0';
        const char *tail = dot + 1;
        if (strcmp(head, "CONFIG") != 0) {
            char explain[256];
            snprintf(explain, sizeof(explain),
                     "'%s' uses namespace '%s' but only CONFIG.<name> is allowed\n"
                     "in @check conditions.",
                     full, head);
            sem_report_rule(diagnostics,
                            expr->loc,
                            "CHECK_INVALID_EXPR_TYPE",
                            explain);
            return -1;
        }

        /* Ensure CONFIG.<tail> exists; reuse CONFIG_USE_UNDECLARED. */
        const JZSymbol *syms = (const JZSymbol *)project_symbols->data;
        size_t count = project_symbols->len / sizeof(JZSymbol);
        int found = 0;
        for (size_t i = 0; i < count; ++i) {
            if (syms[i].kind == JZ_SYM_CONFIG && syms[i].name &&
                strcmp(syms[i].name, tail) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            char explain[256];
            snprintf(explain, sizeof(explain),
                     "CONFIG.%s is not declared in the project CONFIG block.",
                     tail);
            sem_report_rule(diagnostics,
                            expr->loc,
                            "CONFIG_USE_UNDECLARED",
                            explain);
        }
        return 0;
    }

    case JZ_AST_EXPR_UNARY:
        /* Only simple arithmetic/logical unaries are meaningful here. */
        if (!expr->block_kind) {
            sem_report_rule(diagnostics,
                            expr->loc,
                            "CHECK_INVALID_EXPR_TYPE",
                            "@check condition uses an unrecognized unary operator.\n"
                            "Only ~, !, +, and - are allowed in compile-time checks.");
            return -1;
        }
        if (strcmp(expr->block_kind, "BIT_NOT") != 0 &&
            strcmp(expr->block_kind, "LOG_NOT") != 0 &&
            strcmp(expr->block_kind, "POS")     != 0 &&
            strcmp(expr->block_kind, "NEG")     != 0) {
            char explain[256];
            snprintf(explain, sizeof(explain),
                     "@check condition uses unsupported unary operator '%s'.\n"
                     "Only ~, !, +, and - are allowed in compile-time checks.",
                     expr->block_kind);
            sem_report_rule(diagnostics,
                            expr->loc,
                            "CHECK_INVALID_EXPR_TYPE",
                            explain);
            return -1;
        }
        if (expr->child_count > 0 && expr->children[0]) {
            return sem_check_check_expr_allowed(expr->children[0],
                                                mod_scope,
                                                project_symbols,
                                                in_project_scope,
                                                diagnostics);
        }
        return 0;

    case JZ_AST_EXPR_BINARY:
        /* Allow all binary arithmetic/relational/logical operators that the
         * constant-eval engine understands; rely on it for detailed errors.
         */
        if (expr->child_count >= 1 && expr->children[0]) {
            if (sem_check_check_expr_allowed(expr->children[0],
                                             mod_scope,
                                             project_symbols,
                                             in_project_scope,
                                             diagnostics) != 0) {
                return -1;
            }
        }
        if (expr->child_count >= 2 && expr->children[1]) {
            if (sem_check_check_expr_allowed(expr->children[1],
                                             mod_scope,
                                             project_symbols,
                                             in_project_scope,
                                             diagnostics) != 0) {
                return -1;
            }
        }
        return 0;

    case JZ_AST_EXPR_BUILTIN_CALL: {
        if (!expr->name) {
            sem_report_rule(diagnostics,
                            expr->loc,
                            "CHECK_INVALID_EXPR_TYPE",
                            "@check condition calls an unrecognized builtin function.\n"
                            "Only clog2() and widthof() are allowed in compile-time checks.");
            return -1;
        }
        const char *fname = expr->name;
        if (strcmp(fname, "clog2") != 0 && strcmp(fname, "widthof") != 0) {
            char explain[256];
            snprintf(explain, sizeof(explain),
                     "@check condition calls '%s()' which is not a compile-time\n"
                     "function. Only clog2() and widthof() are allowed.",
                     fname);
            sem_report_rule(diagnostics,
                            expr->loc,
                            "CHECK_INVALID_EXPR_TYPE",
                            explain);
            return -1;
        }
        for (size_t i = 0; i < expr->child_count; ++i) {
            if (!expr->children[i]) continue;
            if (sem_check_check_expr_allowed(expr->children[i],
                                             mod_scope,
                                             project_symbols,
                                             in_project_scope,
                                             diagnostics) != 0) {
                return -1;
            }
        }
        return 0;
    }

    case JZ_AST_EXPR_CONCAT:
    case JZ_AST_EXPR_SLICE:
    case JZ_AST_EXPR_TERNARY: {
        const char *op_name = "expression";
        if (expr->type == JZ_AST_EXPR_CONCAT) op_name = "concatenation ({...})";
        else if (expr->type == JZ_AST_EXPR_SLICE) op_name = "slice ([m:l])";
        else if (expr->type == JZ_AST_EXPR_TERNARY) op_name = "ternary (? :)";
        char explain[256];
        snprintf(explain, sizeof(explain),
                 "@check condition contains a %s which is not allowed.\n"
                 "Only arithmetic, relational, and logical operators are\n"
                 "valid in compile-time assertions.",
                 op_name);
        sem_report_rule(diagnostics,
                        expr->loc,
                        "CHECK_INVALID_EXPR_TYPE",
                        explain);
        return -1;
    }

    default:
        /* Recursively inspect any children to be defensive. */
        for (size_t i = 0; i < expr->child_count; ++i) {
            if (!expr->children[i]) continue;
            if (sem_check_check_expr_allowed(expr->children[i],
                                             mod_scope,
                                             project_symbols,
                                             in_project_scope,
                                             diagnostics) != 0) {
                return -1;
            }
        }
        return 0;
    }
}


static void sem_check_single_project_check(JZASTNode *check,
                                           const JZBuffer *project_symbols,
                                           JZDiagnosticList *diagnostics)
{
    if (!check) return;

    JZASTNode *expr = (check->child_count > 0) ? check->children[0] : NULL;
    const char *expr_text = check->width;
    const char *msg = check->text;

    if (!expr || !expr_text) {
        return;
    }

    if (sem_check_check_expr_allowed(expr,
                                     NULL,
                                     project_symbols,
                                     /*in_project_scope=*/1,
                                     diagnostics) != 0) {
        return;
    }

    long long value = 0;
    if (sem_eval_const_expr_in_project(expr_text,
                                       project_symbols,
                                       &value) != 0 ||
        value < 0) {
        char explain[256];
        snprintf(explain, sizeof(explain),
                 "@check expression '%s' could not be evaluated to a\n"
                 "nonnegative integer. It must be a constant expression\n"
                 "over literals and project CONFIG.<name> values.",
                 expr_text);
        sem_report_rule(diagnostics,
                        check->loc,
                        "CHECK_INVALID_EXPR_TYPE",
                        explain);
        return;
    }

    if (value == 0) {
        const char *detail = (msg && *msg) ? msg : "CHECK FAILED";
        sem_report_rule(diagnostics,
                        check->loc,
                        "CHECK_FAILED",
                        detail);
    }
}

void sem_check_project_checks(JZASTNode *root,
                                     const JZBuffer *project_symbols,
                                     JZDiagnosticList *diagnostics)
{
    if (!root || root->type != JZ_AST_PROJECT) return;

    for (size_t i = 0; i < root->child_count; ++i) {
        JZASTNode *child = root->children[i];
        if (!child) continue;

        if (child->type == JZ_AST_CHECK) {
            sem_check_single_project_check(child, project_symbols, diagnostics);
        } else if (child->type == JZ_AST_PROJECT) {
            /* Nested @project body inside compilation root. */
            for (size_t j = 0; j < child->child_count; ++j) {
                JZASTNode *cc = child->children[j];
                if (cc && cc->type == JZ_AST_CHECK) {
                    sem_check_single_project_check(cc, project_symbols, diagnostics);
                }
            }
        }
    }
}

void sem_check_module_checks(const JZModuleScope *scope,
                                    const JZBuffer *project_symbols,
                                    JZDiagnosticList *diagnostics)
{
    if (!scope || !scope->node) return;

    JZASTNode *mod = scope->node;
    for (size_t i = 0; i < mod->child_count; ++i) {
        JZASTNode *child = mod->children[i];
        if (!child || child->type != JZ_AST_CHECK) continue;

        JZASTNode *expr = (child->child_count > 0) ? child->children[0] : NULL;
        const char *expr_text = child->width;
        const char *msg = child->text;

        if (!expr || !expr_text) {
            continue;
        }

        if (sem_check_check_expr_allowed(expr,
                                         scope,
                                         project_symbols,
                                         /*in_project_scope=*/0,
                                         diagnostics) != 0) {
            continue;
        }

        long long value = 0;
        if (sem_eval_const_expr_in_module(expr_text,
                                          scope,
                                          project_symbols,
                                          &value) != 0 ||
            value < 0) {
            char explain[256];
            snprintf(explain, sizeof(explain),
                     "@check expression '%s' could not be evaluated to a\n"
                     "nonnegative integer. It must be a constant expression\n"
                     "over literals, module CONST, and CONFIG.<name> values.",
                     expr_text);
            sem_report_rule(diagnostics,
                            child->loc,
                            "CHECK_INVALID_EXPR_TYPE",
                            explain);
            continue;
        }

        if (value == 0) {
            const char *detail = (msg && *msg) ? msg : "CHECK FAILED";
            sem_report_rule(diagnostics,
                            child->loc,
                            "CHECK_FAILED",
                            detail);
        }
    }
}

static void sem_check_block_expressions_inner(JZASTNode *block,
                                              const JZModuleScope *mod_scope,
                                              const JZBuffer *project_symbols,
                                              int is_async,
                                              int is_sync,
                                              int in_feature,
                                              JZDiagnosticList *diagnostics);

static void sem_check_block_expressions(JZASTNode *block,
                                        const JZModuleScope *mod_scope,
                                        const JZBuffer *project_symbols,
                                        int is_async,
                                        int is_sync,
                                        JZDiagnosticList *diagnostics)
{
    sem_check_block_expressions_inner(block, mod_scope, project_symbols,
                                      is_async, is_sync, 0, diagnostics);
}

static void sem_check_block_expressions_inner(JZASTNode *block,
                                              const JZModuleScope *mod_scope,
                                              const JZBuffer *project_symbols,
                                              int is_async,
                                              int is_sync,
                                              int in_feature,
                                              JZDiagnosticList *diagnostics)
{
    if (!block || !mod_scope) return;
    (void)is_async; /* currently unused but kept for symmetry/future use */

    for (size_t i = 0; i < block->child_count; ++i) {
        JZASTNode *stmt = block->children[i];
        if (!stmt) continue;

        if (stmt->type == JZ_AST_FEATURE_GUARD) {
            /* FEATURE_NESTED: detect nested @feature guards. */
            if (in_feature) {
                sem_report_rule(diagnostics,
                                stmt->loc,
                                "FEATURE_NESTED",
                                "@feature guard is nested inside another @feature guard.\n"
                                "Nesting is not allowed — combine conditions with logical\n"
                                "operators in a single @feature instead.");
            }

            /* Validate condition and then/else bodies explicitly. */
            sem_check_feature_guard_cond(stmt, mod_scope, project_symbols, diagnostics);

            /* THEN body: child[1], optional ELSE: child[2]. */
            if (stmt->child_count > 1 && stmt->children[1]) {
                sem_check_block_expressions_inner(stmt->children[1],
                                                  mod_scope,
                                                  project_symbols,
                                                  is_async,
                                                  is_sync,
                                                  1,
                                                  diagnostics);
            }
            if (stmt->child_count > 2 && stmt->children[2]) {
                sem_check_block_expressions_inner(stmt->children[2],
                                                  mod_scope,
                                                  project_symbols,
                                                  is_async,
                                                  is_sync,
                                                  1,
                                                  diagnostics);
            }
            continue;
        }

        /* Run MUX selector range checks on the full statement subtree. */
        sem_check_mux_selectors_recursive(stmt, mod_scope, diagnostics);

        /* Enforce CONFIG/CONST runtime scope restrictions on all expressions
         * in this executable statement subtree.
         */
        sem_check_runtime_const_config_expr(stmt, mod_scope, diagnostics);

        /* Reject bare integer literals (e.g. 1, 42) in runtime expressions.
         * S2.1: unsized literals are not permitted.
         */
        sem_check_bare_integer_in_expr(stmt, diagnostics);

        if (stmt->type == JZ_AST_STMT_ASSIGN && stmt->child_count >= 2) {
            JZASTNode *lhs = stmt->children[0];
            JZASTNode *rhs = stmt->children[1];

            /* Force type inference on the RHS as before (width checks, etc.). */
            JZBitvecType tmp;
            infer_expr_type(rhs, mod_scope, project_symbols, diagnostics, &tmp);

            /* Enforce Observability Rule at obvious sinks: registers in
             * SYNCHRONOUS blocks and OUT/INOUT ports in either block kind.
             * If the RHS expression tree contains any binary literal with 'x'
             * bits, it may not directly feed these sinks.
             */
            if (sem_expr_contains_x_literal_anywhere(rhs)) {
                int has_reg = 0;
                int has_out_inout = 0;
                sem_lhs_observable_classify(lhs, mod_scope,
                                            &has_reg, &has_out_inout);

                if ((has_reg && is_sync) || has_out_inout) {
                    /* Point the diagnostic at the RHS expression for
                     * better usability when complex expressions are involved.
                     */
                    JZLocation loc = rhs->loc.line ? rhs->loc : stmt->loc;
                    char explain[256];
                    const char *sink_type = has_out_inout ? "OUT/INOUT port" : "register";
                    snprintf(explain, sizeof(explain),
                             "RHS expression contains x-valued literal bits that would\n"
                             "propagate to a %s. Mask or select away x bits\n"
                             "before they reach observable sinks.",
                             sink_type);
                    sem_report_rule(diagnostics,
                                    loc,
                                    "OBS_X_TO_OBSERVABLE_SINK",
                                    explain);
                }
            }
        }
    }
}

static void sem_check_slice_expr(JZASTNode *slice,
                                 const JZModuleScope *scope,
                                 const JZBuffer *project_symbols,
                                 JZDiagnosticList *diagnostics)
{
    if (!slice || !scope || !diagnostics) return;
    if (slice->type != JZ_AST_EXPR_SLICE || slice->child_count < 3) return;

    /* Skip MEM port accesses like mem.port[addr]; these are handled by
     * MEM_ACCESS rules (MEM_CONST_ADDR_OUT_OF_RANGE, MEM_ADDR_WIDTH_TOO_WIDE).
     */
    JZMemPortRef mem_ref;
    memset(&mem_ref, 0, sizeof(mem_ref));
    if (sem_match_mem_port_slice(slice, scope, NULL, &mem_ref)) {
        return;
    }

    JZASTNode *base = slice->children[0];
    if (!base) return;

    /* Skip MUX selectors mux_id[idx]; handled by MUX_* rules. */
    if (base->type == JZ_AST_EXPR_IDENTIFIER && base->name) {
        const JZSymbol *mux_sym = module_scope_lookup_kind(scope, base->name, JZ_SYM_MUX);
        if (mux_sym) {
            return;
        }
    }

    JZASTNode *msb_node = slice->children[1];
    JZASTNode *lsb_node = slice->children[2];
    if (!msb_node || !lsb_node) return;

    int msb_valid = 0, lsb_valid = 0;
    unsigned msb_val = 0, lsb_val = 0;

    /* Literal indices: must be non-negative decimal integers. */
    if (msb_node->type == JZ_AST_EXPR_LITERAL && msb_node->text) {
        if (parse_simple_nonnegative_int(msb_node->text, &msb_val)) {
            msb_valid = 1;
        } else {
            char explain[256];
            snprintf(explain, sizeof(explain),
                     "MSB slice index '%s' is not a valid non-negative integer.\n"
                     "Slice indices must be non-negative integer literals or\n"
                     "CONST/CONFIG names.",
                     msb_node->text);
            sem_report_rule(diagnostics,
                            msb_node->loc,
                            "SLICE_INDEX_INVALID",
                            explain);
        }
    }
    int same_index = (lsb_node == msb_node) ||
                     (lsb_node->loc.line == msb_node->loc.line &&
                      lsb_node->loc.column == msb_node->loc.column);
    if (!same_index &&
        lsb_node->type == JZ_AST_EXPR_LITERAL && lsb_node->text) {
        if (parse_simple_nonnegative_int(lsb_node->text, &lsb_val)) {
            lsb_valid = 1;
        } else {
            char explain[256];
            snprintf(explain, sizeof(explain),
                     "LSB slice index '%s' is not a valid non-negative integer.\n"
                     "Slice indices must be non-negative integer literals or\n"
                     "CONST/CONFIG names.",
                     lsb_node->text);
            sem_report_rule(diagnostics,
                            lsb_node->loc,
                            "SLICE_INDEX_INVALID",
                            explain);
        }
    }

    /* Fallback for template-expanded expression trees (e.g., IDX*11+10
     * becomes EXPR_BINARY after @apply substitution). */
    if (!msb_valid) {
        long v = 0;
        if (sem_try_const_eval_ast_expr(msb_node, &v) && v >= 0) {
            msb_val = (unsigned)v;
            msb_valid = 1;
        }
    }
    if (!lsb_valid) {
        long v = 0;
        if (sem_try_const_eval_ast_expr(lsb_node, &v) && v >= 0) {
            lsb_val = (unsigned)v;
            lsb_valid = 1;
        }
    }

    /* Identifier indices: must refer to defined CONST, CONFIG, or IDX names. */
    if (msb_node->type == JZ_AST_EXPR_IDENTIFIER && msb_node->name &&
        strcmp(msb_node->name, "IDX") != 0) {
        const JZSymbol *c_sym = module_scope_lookup_kind(scope, msb_node->name, JZ_SYM_CONST);
        const JZSymbol *cfg_sym = NULL;
        if (project_symbols && project_symbols->data) {
            const JZSymbol *syms = (const JZSymbol *)project_symbols->data;
            size_t count = project_symbols->len / sizeof(JZSymbol);
            for (size_t i = 0; i < count; ++i) {
                if (syms[i].kind == JZ_SYM_CONFIG && syms[i].name &&
                    strcmp(syms[i].name, msb_node->name) == 0) {
                    cfg_sym = &syms[i];
                    break;
                }
            }
        }
        if (!c_sym && !cfg_sym) {
            char explain[256];
            snprintf(explain, sizeof(explain),
                     "MSB slice index '%s' is not a declared CONST or CONFIG name.",
                     msb_node->name);
            sem_report_rule(diagnostics,
                            msb_node->loc,
                            "CONST_UNDEFINED_IN_WIDTH_OR_SLICE",
                            explain);
        }
    }
    if (!same_index &&
        lsb_node->type == JZ_AST_EXPR_IDENTIFIER && lsb_node->name &&
        strcmp(lsb_node->name, "IDX") != 0) {
        const JZSymbol *c_sym = module_scope_lookup_kind(scope, lsb_node->name, JZ_SYM_CONST);
        const JZSymbol *cfg_sym = NULL;
        if (project_symbols && project_symbols->data) {
            const JZSymbol *syms = (const JZSymbol *)project_symbols->data;
            size_t count = project_symbols->len / sizeof(JZSymbol);
            for (size_t i = 0; i < count; ++i) {
                if (syms[i].kind == JZ_SYM_CONFIG && syms[i].name &&
                    strcmp(syms[i].name, lsb_node->name) == 0) {
                    cfg_sym = &syms[i];
                    break;
                }
            }
        }
        if (!c_sym && !cfg_sym) {
            char explain[256];
            snprintf(explain, sizeof(explain),
                     "LSB slice index '%s' is not a declared CONST or CONFIG name.",
                     lsb_node->name);
            sem_report_rule(diagnostics,
                            lsb_node->loc,
                            "CONST_UNDEFINED_IN_WIDTH_OR_SLICE",
                            explain);
        }
    }

    if (msb_valid && lsb_valid) {
        if (msb_val < lsb_val) {
            char explain[256];
            const char *base_name = (base->name) ? base->name : "signal";
            snprintf(explain, sizeof(explain),
                     "slice [%u:%u] on '%s' has MSB < LSB. JZ-HDL slices use\n"
                     "big-endian bit ordering: MSB must be >= LSB.",
                     msb_val, lsb_val, base_name);
            sem_report_rule(diagnostics,
                            slice->loc,
                            "SLICE_MSB_LESS_THAN_LSB",
                            explain);
            return;
        }

        /* When base width is known, enforce index range constraints. */
        JZBitvecType base_t;
        base_t.width = 0;
        base_t.is_signed = 0;
        infer_expr_type(base, scope, project_symbols, diagnostics, &base_t);
        if (base_t.width > 0u) {
            if (msb_val >= base_t.width || lsb_val >= base_t.width) {
                char explain[256];
                const char *base_name = (base->name) ? base->name : "signal";
                snprintf(explain, sizeof(explain),
                         "slice [%u:%u] on '%s' exceeds its width [%u].\n"
                         "Valid bit indices are 0 to %u.",
                         msb_val, lsb_val, base_name, base_t.width,
                         base_t.width - 1);
                sem_report_rule(diagnostics,
                                slice->loc,
                                "SLICE_INDEX_OUT_OF_RANGE",
                                explain);
            }
        }
    }
}

static void sem_check_slices_recursive(JZASTNode *node,
                                       const JZModuleScope *scope,
                                       const JZBuffer *project_symbols,
                                       JZDiagnosticList *diagnostics)
{
    if (!node) return;

    if (node->type == JZ_AST_EXPR_SLICE) {
        sem_check_slice_expr(node, scope, project_symbols, diagnostics);
    }
    for (size_t i = 0; i < node->child_count; ++i) {
        sem_check_slices_recursive(node->children[i], scope, project_symbols, diagnostics);
    }
}

void sem_check_module_slices(const JZModuleScope *scope,
                                    const JZBuffer *project_symbols,
                                    JZDiagnosticList *diagnostics)
{
    if (!scope || !scope->node) return;
    sem_check_slices_recursive(scope->node, scope, project_symbols, diagnostics);
}

/* Recursively walk AST and flag reads of OUT ports in expression contexts.
 * LHS of assignments are explicitly skipped so that driving an OUT is legal.
 */
static void sem_check_port_direction_expr_recursive(JZASTNode *node,
                                                    const JZModuleScope *mod_scope,
                                                    const JZBuffer *project_symbols,
                                                    JZDiagnosticList *diagnostics,
                                                    int in_alias)
{
    if (!node || !mod_scope || !diagnostics) return;

    switch (node->type) {
    case JZ_AST_STMT_ASSIGN: {
        /* Determine which side(s) are read contexts based on assignment kind. */
        const char *op = node->block_kind ? node->block_kind : "";
        int is_alias   = (strcmp(op, "ALIAS") == 0 || strcmp(op, "ALIAS_Z") == 0 || strcmp(op, "ALIAS_S") == 0);
        int is_drive   = (strncmp(op, "DRIVE", 5) == 0);
        int is_receive = (strncmp(op, "RECEIVE", 7) == 0);

        if (node->child_count >= 2) {
            JZASTNode *lhs = node->children[0];
            JZASTNode *rhs = node->children[1];

            if (is_alias) {
                /* For alias assignments, treat RHS as the value being read and
                 * LHS as primarily a write target for direction-mismatch
                 * purposes. This avoids flagging legal writes to OUT ports on
                 * the LHS while still catching "wire = out_port;".
                 *
                 * When both sides are BUS signals (bus-to-bus pass-through),
                 * validate direction compatibility instead of individual
                 * read/write checks: the connected signals must have
                 * complementary directions (one readable, one writable) or
                 * at least one must be INOUT.
                 */
                int bus_alias = 0;
                if (lhs && rhs && project_symbols) {
                    JZBusAccessInfo li, ri;
                    int lbus = sem_resolve_bus_access(lhs, mod_scope, project_symbols, &li, NULL) && li.signal_decl;
                    int rbus = sem_resolve_bus_access(rhs, mod_scope, project_symbols, &ri, NULL) && ri.signal_decl;
                    if (lbus && rbus) {
                        bus_alias = 1;
                        /* Both readable-only (IN=IN): no driver on the net. */
                        if (!li.writable && !ri.writable) {
                            sem_report_rule(diagnostics,
                                            node->loc,
                                            "BUS_SIGNAL_WRITE_TO_READABLE",
                                            "BUS alias connects two read-only signals — neither\n"
                                            "side can drive the net. At least one side must be a\n"
                                            "writable (SOURCE/INOUT) signal.");
                        }
                        /* Both writable-only (OUT=OUT): driver conflict. */
                        if (!li.readable && !ri.readable) {
                            sem_report_rule(diagnostics,
                                            node->loc,
                                            "BUS_SIGNAL_READ_FROM_WRITABLE",
                                            "BUS alias connects two write-only signals — neither\n"
                                            "side can be read. At least one side must be a\n"
                                            "readable (TARGET/INOUT) signal.");
                        }
                    }
                }
                if (rhs) sem_check_port_direction_expr_recursive(rhs, mod_scope, project_symbols, diagnostics, bus_alias);
            } else if (is_drive) {
                /* DRIVE: LHS is the driver (read context), RHS is sink (write). */
                if (lhs) sem_check_port_direction_expr_recursive(lhs, mod_scope, project_symbols, diagnostics, 0);
            } else if (is_receive) {
                /* RECEIVE: RHS is the driver (read context), LHS is sink (write). */
                if (rhs) sem_check_port_direction_expr_recursive(rhs, mod_scope, project_symbols, diagnostics, 0);
            } else {
                /* Fallback: treat both sides as potential read contexts. */
                if (lhs) sem_check_port_direction_expr_recursive(lhs, mod_scope, project_symbols, diagnostics, 0);
                if (rhs) sem_check_port_direction_expr_recursive(rhs, mod_scope, project_symbols, diagnostics, 0);
            }
        }
        return;
    }

    case JZ_AST_STMT_IF:
    case JZ_AST_STMT_ELIF:
        /* child[0] is condition; remaining children are bodies. */
        if (node->child_count >= 1 && node->children[0]) {
            sem_check_port_direction_expr_recursive(node->children[0], mod_scope, project_symbols, diagnostics, 0);
        }
        for (size_t i = 1; i < node->child_count; ++i) {
            if (node->children[i]) {
                sem_check_port_direction_expr_recursive(node->children[i], mod_scope, project_symbols, diagnostics, 0);
            }
        }
        return;

    case JZ_AST_STMT_SELECT:
        /* child[0] is selector; remaining are CASE/DEFAULT. */
        if (node->child_count >= 1 && node->children[0]) {
            sem_check_port_direction_expr_recursive(node->children[0], mod_scope, project_symbols, diagnostics, 0);
        }
        for (size_t i = 1; i < node->child_count; ++i) {
            if (node->children[i]) {
                sem_check_port_direction_expr_recursive(node->children[i], mod_scope, project_symbols, diagnostics, 0);
            }
        }
        return;

    case JZ_AST_STMT_CASE:
        /* child[0] is label; remaining children are body statements. */
        if (node->child_count >= 1 && node->children[0]) {
            sem_check_port_direction_expr_recursive(node->children[0], mod_scope, project_symbols, diagnostics, 0);
        }
        for (size_t i = 1; i < node->child_count; ++i) {
            if (node->children[i]) {
                sem_check_port_direction_expr_recursive(node->children[i], mod_scope, project_symbols, diagnostics, 0);
            }
        }
        return;

    case JZ_AST_STMT_DEFAULT:
    case JZ_AST_BLOCK:
        for (size_t i = 0; i < node->child_count; ++i) {
            if (node->children[i]) {
                sem_check_port_direction_expr_recursive(node->children[i], mod_scope, project_symbols, diagnostics, 0);
            }
        }
        return;

    default:
        break;
    }

    /* Expression node (or other) – check identifiers, then recurse. */
    if (node->type == JZ_AST_EXPR_IDENTIFIER && node->name) {
        const JZSymbol *sym = module_scope_lookup(mod_scope, node->name);
        if (sym && sym->kind == JZ_SYM_PORT && sym->node && sym->node->block_kind) {
            if (strcmp(sym->node->block_kind, "OUT") == 0) {
                char explain[256];
                snprintf(explain, sizeof(explain),
                         "port '%s' is declared OUT and cannot be read inside\n"
                         "the module. Use an internal WIRE to hold the value,\n"
                         "or change the port to INOUT.",
                         node->name);
                sem_report_rule(diagnostics,
                                node->loc,
                                "PORT_DIRECTION_MISMATCH_OUT",
                                explain);
            }
        }
    } else if ((node->type == JZ_AST_EXPR_BUS_ACCESS ||
                node->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER) &&
               project_symbols) {
        JZBusAccessInfo info;
        if (sem_resolve_bus_access(node, mod_scope, project_symbols, &info, NULL) &&
            info.signal_decl) {
            if (!info.readable && !in_alias) {
                const char *sig_name = node->name ? node->name : "?";
                char explain[256];
                snprintf(explain, sizeof(explain),
                         "BUS signal '%s' is write-only (SOURCE direction) and\n"
                         "cannot be read. Only TARGET or INOUT signals are readable.",
                         sig_name);
                sem_report_rule(diagnostics,
                                node->loc,
                                "BUS_SIGNAL_READ_FROM_WRITABLE",
                                explain);
            }
        }
    }

    for (size_t i = 0; i < node->child_count; ++i) {
        if (node->children[i]) {
            sem_check_port_direction_expr_recursive(node->children[i], mod_scope, project_symbols, diagnostics, in_alias);
        }
    }
}

void sem_check_module_expressions(const JZModuleScope *scope,
                                         const JZBuffer *project_symbols,
                                         JZDiagnosticList *diagnostics)
{
    if (!scope || !scope->node) return;
    JZASTNode *mod = scope->node;

    for (size_t i = 0; i < mod->child_count; ++i) {
        JZASTNode *child = mod->children[i];
        if (!child) continue;
        if (child->type != JZ_AST_BLOCK || !child->block_kind) continue;

        int is_async = (strcmp(child->block_kind, "ASYNCHRONOUS") == 0);
        int is_sync  = (strcmp(child->block_kind, "SYNCHRONOUS") == 0);
        if (!is_async && !is_sync) {
            continue;
        }

        /* Enforce PORT_DIRECTION_MISMATCH_OUT only within executable blocks
         * (ASYNCHRONOUS/SYNCHRONOUS), not in port lists or @new bindings.
         */
        sem_check_port_direction_expr_recursive(child, scope, project_symbols, diagnostics, 0);

        sem_check_block_expressions(child, scope, project_symbols,
                                    is_async, is_sync, diagnostics);

        /* For each executable block, track MEM OUT writes so that
         * MEM_MULTIPLE_WRITES_SAME_IN can be enforced per SYNCHRONOUS block,
         * and MEM_MULTIPLE_ASSIGN_SYNC_READ_OUT for synchronous read outputs.
         */
        JZBuffer mem_out_writes = (JZBuffer){0};
        JZBuffer mem_sync_reads = (JZBuffer){0};
        sem_check_block_assignments(child, scope, project_symbols, diagnostics, is_sync, &mem_out_writes, &mem_sync_reads);
        jz_buf_free(&mem_out_writes);
        jz_buf_free(&mem_sync_reads);

        /* CONTROL_FLOW_IF_SELECT: IF/ELIF conditions and SELECT/CASE structure. */
        sem_check_block_control_flow(child, scope, project_symbols, is_async, is_sync, diagnostics);
    }
}

void sem_check_expressions(JZASTNode *root,
                                  JZBuffer *module_scopes,
                                  const JZBuffer *project_symbols,
                                  const JZChipData *chip,
                                  JZDiagnosticList *diagnostics)
{
    if (!root || root->type != JZ_AST_PROJECT) return;
    (void)root; /* root shape already validated by parser. */

    size_t scope_count = module_scopes->len / sizeof(JZModuleScope);
    JZModuleScope *scopes = (JZModuleScope *)module_scopes->data;
    for (size_t i = 0; i < scope_count; ++i) {
        /* First, validate module-level CONST initializers. */
        sem_check_module_const_blocks(&scopes[i], project_symbols, diagnostics);

        sem_check_module_mem_and_mux_decls(&scopes[i], project_symbols, diagnostics);
        sem_check_module_mem_chip_configs(&scopes[i], project_symbols, chip, diagnostics);
        sem_check_module_latch_chip_support(&scopes[i], chip, root, project_symbols, diagnostics);
        sem_check_module_instantiations(&scopes[i], module_scopes, project_symbols, diagnostics);
        sem_check_module_decl_widths(&scopes[i], project_symbols, diagnostics);
        sem_check_module_expressions(&scopes[i], project_symbols, diagnostics);
        sem_check_module_mem_port_usage(&scopes[i], diagnostics);
        sem_check_module_slices(&scopes[i], project_symbols, diagnostics);
        sem_check_module_literal_widths(&scopes[i], project_symbols, diagnostics);
        sem_check_module_checks(&scopes[i], project_symbols, diagnostics);
    }
}
