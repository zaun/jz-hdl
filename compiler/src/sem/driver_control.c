#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "sem_driver.h"
#include "sem.h"
#include "util.h"
#include "rules.h"
#include "driver_internal.h"

/* -------------------------------------------------------------------------
 *  CONTROL_FLOW_IF_SELECT: IF/ELIF conditions and SELECT/CASE structure
 * -------------------------------------------------------------------------
 */

typedef struct JZSelectCaseKey {
    const char *repr;   /* textual representation: literal text or identifier */
    JZLocation loc;     /* location of the CASE value expression */
} JZSelectCaseKey;

static void sem_check_if_cond_width(JZASTNode *stmt,
                                    const JZModuleScope *mod_scope,
                                    const JZBuffer *project_symbols,
                                    JZDiagnosticList *diagnostics)
{
    if (!stmt || !mod_scope || !diagnostics) return;
    if (stmt->child_count == 0) return;
    JZASTNode *cond = stmt->children[0];
    if (!cond) return;

    JZBitvecType cond_t;
    infer_expr_type(cond, mod_scope, project_symbols, diagnostics, &cond_t);
    if (cond_t.width > 0 && cond_t.width != 1u) {
        char explain[256];
        snprintf(explain, sizeof(explain),
                 "IF/ELIF condition has width [%u] but must be width [1].\n"
                 "Use a comparison operator (==, !=) or reduction to produce\n"
                 "a 1-bit result.",
                 cond_t.width);
        sem_report_rule(diagnostics,
                        cond->loc,
                        "IF_COND_WIDTH_NOT_1",
                        explain);
    }
}

static void sem_check_select_stmt_control_flow(JZASTNode *select_stmt,
                                               const JZModuleScope *mod_scope,
                                               const JZBuffer *project_symbols,
                                               int is_async,
                                               int is_sync,
                                               JZDiagnosticList *diagnostics)
{
    if (!select_stmt || select_stmt->type != JZ_AST_STMT_SELECT || !diagnostics) {
        return;
    }

    /* child[0] is the selector expression; remaining children are CASE/DEFAULT. */
    if (select_stmt->child_count < 2) {
        return;
    }

    /* SELECT_CASE_WIDTH_MISMATCH: infer selector width and compare against CASE values.
     * Only flag when CASE value has an explicitly sized literal (contains tick mark),
     * since unsized literals (bare integers) are implicitly cast.
     */
    if (mod_scope) {
        JZASTNode *selector = select_stmt->children[0];
        JZBitvecType sel_t;
        sel_t.width = 0;
        sel_t.is_signed = 0;
        if (selector) {
            infer_expr_type(selector, mod_scope, project_symbols, diagnostics, &sel_t);
        }
        if (sel_t.width > 0) {
            for (size_t i = 1; i < select_stmt->child_count; ++i) {
                JZASTNode *child = select_stmt->children[i];
                if (!child || child->type != JZ_AST_STMT_CASE) continue;
                if (child->child_count == 0) continue;
                JZASTNode *val = child->children[0];
                if (!val) continue;
                /* Skip unsized (bare integer) literals: they're implicitly cast. */
                if (val->type == JZ_AST_EXPR_LITERAL && val->text &&
                    !strchr(val->text, '\'')) {
                    continue;
                }
                JZBitvecType case_t;
                case_t.width = 0;
                case_t.is_signed = 0;
                infer_expr_type(val, mod_scope, project_symbols, diagnostics, &case_t);
                if (case_t.width > 0 && case_t.width != sel_t.width) {
                    char explain[256];
                    const char *case_repr = (val->text) ? val->text :
                                            (val->name) ? val->name : "?";
                    snprintf(explain, sizeof(explain),
                             "CASE value '%s' has width [%u] but the selector has width [%u].\n"
                             "CASE value width must match the SELECT expression width.",
                             case_repr, case_t.width, sel_t.width);
                    sem_report_rule(diagnostics,
                                    val->loc,
                                    "SELECT_CASE_WIDTH_MISMATCH",
                                    explain);
                }
            }
        }
    }

    int has_default = 0;
    JZBuffer keys = {0}; /* array of JZSelectCaseKey */

    for (size_t i = 1; i < select_stmt->child_count; ++i) {
        JZASTNode *child = select_stmt->children[i];
        if (!child) continue;

        if (child->type == JZ_AST_STMT_DEFAULT) {
            has_default = 1;
            continue;
        }

        if (child->type != JZ_AST_STMT_CASE) {
            continue;
        }
        if (child->child_count == 0) {
            continue; /* malformed or empty CASE; parser should have rejected */
        }

        JZASTNode *val = child->children[0];
        if (!val) continue;

        const char *repr = NULL;
        if (val->type == JZ_AST_EXPR_LITERAL && val->text) {
            repr = val->text;
        } else if ((val->type == JZ_AST_EXPR_IDENTIFIER ||
                    val->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER) &&
                   val->name) {
            repr = val->name;
        }

        if (!repr) {
            continue; /* non-constant expressions are ignored for duplicate detection */
        }

        /* Check against previously seen CASE values in this SELECT. */
        JZSelectCaseKey *arr = (JZSelectCaseKey *)keys.data;
        size_t key_count = keys.len / sizeof(JZSelectCaseKey);
        for (size_t k = 0; k < key_count; ++k) {
            if (arr[k].repr && strcmp(arr[k].repr, repr) == 0) {
                char explain[256];
                snprintf(explain, sizeof(explain),
                         "CASE value '%s' appears more than once in this SELECT.\n"
                         "Only the first matching CASE executes; duplicates are dead code.",
                         repr);
                sem_report_rule(diagnostics,
                                val->loc,
                                "SELECT_DUP_CASE_VALUE",
                                explain);
                break;
            }
        }

        JZSelectCaseKey key;
        key.repr = repr;
        key.loc = val->loc;
        (void)jz_buf_append(&keys, &key, sizeof(key));
    }

    jz_buf_free(&keys);

    /* DEFAULT coverage diagnostics differ between ASYNCHRONOUS and SYNCHRONOUS. */
    if (!has_default) {
        if (is_async) {
            sem_report_rule(diagnostics,
                            select_stmt->loc,
                            "SELECT_DEFAULT_RECOMMENDED_ASYNC",
                            "ASYNCHRONOUS SELECT has no DEFAULT branch. When no CASE\n"
                            "matches, driven nets receive no assignment, which can create\n"
                            "unintended latches. Add a DEFAULT to cover all values.");
        } else if (is_sync) {
            sem_report_rule(diagnostics,
                            select_stmt->loc,
                            "SELECT_NO_MATCH_SYNC_OK",
                            "SYNCHRONOUS SELECT has no DEFAULT branch. When no CASE\n"
                            "matches, registers retain their current value (implicit hold).\n"
                            "This is legal but a DEFAULT may improve readability.");
        }
    }
}

static void sem_check_control_flow_stmt(JZASTNode *stmt,
                                        const JZModuleScope *mod_scope,
                                        const JZBuffer *project_symbols,
                                        int is_async,
                                        int is_sync,
                                        int in_conditional,
                                        JZDiagnosticList *diagnostics)
{
    if (!stmt || !mod_scope) return;

    /* Enforce that alias operators are only used for unconditional net aliasing.
     * Any alias assignment that appears lexically inside IF/ELIF/ELSE/SELECT/CASE
     * bodies in an ASYNCHRONOUS block is rejected.
     */
    if (in_conditional && stmt->type == JZ_AST_STMT_ASSIGN && stmt->child_count >= 2) {
        const char *op = stmt->block_kind ? stmt->block_kind : "";
        int is_alias = (strcmp(op, "ALIAS") == 0 ||
                        strcmp(op, "ALIAS_Z") == 0 ||
                        strcmp(op, "ALIAS_S") == 0);
        /* In SYNCHRONOUS blocks aliasing is already banned via SYNC_NO_ALIAS.
         * To avoid redundant diagnostics, enforce ASYNC_ALIAS_IN_CONDITIONAL
         * only for ASYNCHRONOUS contexts.
         */
        if (is_alias && is_async) {
            char explain[256];
            snprintf(explain, sizeof(explain),
                     "alias operator '%s' inside a conditional branch; did you mean '<='?\n"
                     "Aliases (=, =z, =s) create permanent wire connections and cannot\n"
                     "be conditional. Use '<=' (receive) or '=>' (drive) instead.",
                     op);
            sem_report_rule(diagnostics,
                            stmt->loc,
                            "ASYNC_ALIAS_IN_CONDITIONAL",
                            explain);
        }
    }

    switch (stmt->type) {
    case JZ_AST_STMT_IF:
    case JZ_AST_STMT_ELIF:
        /* child[0] is the condition; remaining children form the body. */
        sem_check_if_cond_width(stmt, mod_scope, project_symbols, diagnostics);
        for (size_t j = 1; j < stmt->child_count; ++j) {
            JZASTNode *body = stmt->children[j];
            if (!body) continue;
            sem_check_control_flow_stmt(body, mod_scope, project_symbols, is_async, is_sync, 1, diagnostics);
        }
        break;

    case JZ_AST_STMT_ELSE:
        for (size_t j = 0; j < stmt->child_count; ++j) {
            JZASTNode *body = stmt->children[j];
            if (!body) continue;
            sem_check_control_flow_stmt(body, mod_scope, project_symbols, is_async, is_sync, 1, diagnostics);
        }
        break;

    case JZ_AST_STMT_SELECT:
        sem_check_select_stmt_control_flow(stmt, mod_scope, project_symbols, is_async, is_sync, diagnostics);
        /* Recurse into CASE/DEFAULT bodies for nested control-flow. */
        for (size_t j = 1; j < stmt->child_count; ++j) {
            JZASTNode *branch = stmt->children[j];
            if (!branch) continue;
            sem_check_control_flow_stmt(branch, mod_scope, project_symbols, is_async, is_sync, 1, diagnostics);
        }
        break;

    case JZ_AST_STMT_CASE:
        /* child[0] is label; remaining children are body statements. */
        for (size_t j = 1; j < stmt->child_count; ++j) {
            JZASTNode *body = stmt->children[j];
            if (!body) continue;
            sem_check_control_flow_stmt(body, mod_scope, project_symbols, is_async, is_sync, 1, diagnostics);
        }
        break;

    case JZ_AST_STMT_DEFAULT:
        for (size_t j = 0; j < stmt->child_count; ++j) {
            JZASTNode *body = stmt->children[j];
            if (!body) continue;
            sem_check_control_flow_stmt(body, mod_scope, project_symbols, is_async, is_sync, 1, diagnostics);
        }
        break;

    default:
        /* Generic recursion into children to catch nested IF/SELECT constructs
         * that appear inside expression trees or other statement forms.
         */
        for (size_t j = 0; j < stmt->child_count; ++j) {
            JZASTNode *child = stmt->children[j];
            if (!child) continue;
            sem_check_control_flow_stmt(child, mod_scope, project_symbols, is_async, is_sync, in_conditional, diagnostics);
        }
        break;
    }
}

void sem_check_block_control_flow(JZASTNode *block,
                                         const JZModuleScope *mod_scope,
                                         const JZBuffer *project_symbols,
                                         int is_async,
                                         int is_sync,
                                         JZDiagnosticList *diagnostics)
{
    if (!block || !mod_scope) return;

    for (size_t i = 0; i < block->child_count; ++i) {
        JZASTNode *stmt = block->children[i];
        if (!stmt) continue;
        sem_check_control_flow_stmt(stmt, mod_scope, project_symbols, is_async, is_sync, 0, diagnostics);
    }
}
