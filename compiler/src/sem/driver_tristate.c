#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

#include "sem_driver.h"
#include "sem.h"
#include "driver_internal.h"
#include "util.h"
#include "rules.h"
#include "tristate_types.h"

/* -------------------------------------------------------------------------
 *  Tri-state proof engine
 *
 *  All analysis and proof logic for multi-driver net resolution.
 *  Used by both the tristate report and the linter (NET_MULTIPLE_ACTIVE_DRIVERS).
 * -------------------------------------------------------------------------
 */

/* -------------------------------------------------------------------------
 *  Global context for resolving identifiers in tristate analysis.
 *  - g_tristate_project_symbols: for resolving qualified identifiers (e.g., DEV.ROM)
 *  - g_tristate_parent_scope: for resolving module CONST values (e.g., DEV_A)
 *  These are set before tristate analysis and used by tristate_get_binding_literal.
 * -------------------------------------------------------------------------
 */
static const JZBuffer *g_tristate_project_symbols = NULL;
static const JZModuleScope *g_tristate_parent_scope = NULL;

void jz_tristate_set_project_symbols(const JZBuffer *project_symbols)
{
    g_tristate_project_symbols = project_symbols;
}

void jz_tristate_set_parent_scope(const JZModuleScope *scope)
{
    g_tristate_parent_scope = scope;
}

/* -------------------------------------------------------------------------
 *  Helper functions
 * -------------------------------------------------------------------------
 */

unsigned jz_tristate_decl_width_simple(JZASTNode *decl)
{
    if (!decl || !decl->width) {
        return 0;
    }
    unsigned w = 0;
    int rc = eval_simple_positive_decl_int(decl->width, &w);
    return (rc == 1) ? w : 0;
}

const char *jz_tristate_extract_bus_field(const JZASTNode *stmt)
{
    if (!stmt || stmt->type != JZ_AST_STMT_ASSIGN || stmt->child_count < 2) {
        return NULL;
    }
    const JZASTNode *lhs = stmt->children[0];
    /* Walk through SLICE wrappers. */
    while (lhs && lhs->type == JZ_AST_EXPR_SLICE && lhs->child_count >= 1) {
        lhs = lhs->children[0];
    }
    if (!lhs) return NULL;

    if (lhs->type == JZ_AST_EXPR_BUS_ACCESS && lhs->text) {
        return lhs->text;
    }
    /* QUALIFIED_IDENTIFIER: name is "port.FIELD" -- return the part after the dot. */
    if (lhs->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER && lhs->name) {
        const char *dot = strchr(lhs->name, '.');
        if (dot && dot[1]) {
            return dot + 1;
        }
    }
    return NULL;
}

int jz_tristate_net_is_bus_port(const JZNet *net)
{
    if (!net) return 0;
    JZASTNode **atoms = (JZASTNode **)net->atoms.data;
    size_t atom_count = net->atoms.len / sizeof(JZASTNode *);
    if (atom_count == 0 || !atoms[0]) return 0;
    return (atoms[0]->block_kind && strcmp(atoms[0]->block_kind, "BUS") == 0);
}

static int tristate_lhs_matches_port(const JZASTNode *lhs,
                                     const char *port_name,
                                     const char *bus_field)
{
    if (!lhs || !port_name) return 0;
    /* Walk through SLICE wrappers. */
    while (lhs && lhs->type == JZ_AST_EXPR_SLICE && lhs->child_count >= 1) {
        lhs = lhs->children[0];
    }
    if (!lhs) return 0;

    if (bus_field) {
        if (lhs->type == JZ_AST_EXPR_BUS_ACCESS &&
            lhs->name && strcmp(lhs->name, port_name) == 0 &&
            lhs->text && strcmp(lhs->text, bus_field) == 0) {
            return 1;
        }
        if (lhs->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER && lhs->name) {
            size_t plen = strlen(port_name);
            if (strncmp(lhs->name, port_name, plen) == 0 &&
                lhs->name[plen] == '.' &&
                strcmp(lhs->name + plen + 1, bus_field) == 0) {
                return 1;
            }
        }
        return 0;
    }

    if (lhs->type == JZ_AST_EXPR_IDENTIFIER &&
        lhs->name && strcmp(lhs->name, port_name) == 0) {
        return 1;
    }
    if (lhs->type == JZ_AST_EXPR_BUS_ACCESS &&
        lhs->name && strcmp(lhs->name, port_name) == 0) {
        return 1;
    }
    if (lhs->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER && lhs->name) {
        size_t plen = strlen(port_name);
        if (strncmp(lhs->name, port_name, plen) == 0 &&
            (lhs->name[plen] == '.' || lhs->name[plen] == '\0')) {
            return 1;
        }
    }
    return 0;
}

static int tristate_expr_is_all_z_literal(const JZASTNode *expr)
{
    if (!expr) return 0;

    if (expr->type == JZ_AST_EXPR_LITERAL && expr->text) {
        const char *lex = expr->text;
        const char *tick = strchr(lex, '\'');
        if (!tick) {
            return 0;
        }
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
            if (!tristate_expr_is_all_z_literal(expr->children[i])) {
                return 0;
            }
        }
        return 1;
    }

    return 0;
}

static int tristate_expr_contains_z_anywhere(const JZASTNode *expr)
{
    if (!expr) return 0;

    if (expr->type == JZ_AST_EXPR_LITERAL && expr->text) {
        const char *lex = expr->text;
        const char *tick = strchr(lex, '\'');
        if (tick) {
            const char *value = tick + 2;
            for (const char *p = value; *p; ++p) {
                char c = *p;
                if (c == 'z' || c == 'Z') {
                    return 1;
                }
            }
        }
        return 0;
    }

    for (size_t i = 0; i < expr->child_count; ++i) {
        if (tristate_expr_contains_z_anywhere(expr->children[i])) {
            return 1;
        }
    }
    return 0;
}

static int tristate_port_can_drive_z_in_ast(const JZASTNode *node, const char *name,
                                            const char *bus_field)
{
    if (!node || !name) return 0;

    if (node->type == JZ_AST_STMT_ASSIGN && node->child_count >= 2) {
        const char *op = node->block_kind ? node->block_kind : "";
        int is_alias = (strcmp(op, "ALIAS") == 0 ||
                        strcmp(op, "ALIAS_Z") == 0 ||
                        strcmp(op, "ALIAS_S") == 0);

        if (!is_alias) {
            const JZASTNode *lhs = node->children[0];
            const JZASTNode *rhs = node->children[1];

            int match = tristate_lhs_matches_port(lhs, name, bus_field);
            if (match) {
                if (tristate_expr_contains_z_anywhere(rhs)) {
                    return 1;
                }
            }
        }
    }

    for (size_t i = 0; i < node->child_count; ++i) {
        if (tristate_port_can_drive_z_in_ast(node->children[i], name, bus_field)) {
            return 1;
        }
    }
    return 0;
}

static int tristate_port_can_drive_non_z_in_ast(const JZASTNode *node, const char *name,
                                                const char *bus_field)
{
    if (!node || !name) return 0;

    if (node->type == JZ_AST_STMT_ASSIGN && node->child_count >= 2) {
        const char *op = node->block_kind ? node->block_kind : "";
        int is_alias = (strcmp(op, "ALIAS") == 0 ||
                        strcmp(op, "ALIAS_Z") == 0 ||
                        strcmp(op, "ALIAS_S") == 0);

        if (!is_alias) {
            const JZASTNode *lhs = node->children[0];
            const JZASTNode *rhs = node->children[1];

            if (tristate_lhs_matches_port(lhs, name, bus_field)) {
                if (!tristate_expr_is_all_z_literal(rhs)) {
                    return 1;
                }
            }
        }
    }

    for (size_t i = 0; i < node->child_count; ++i) {
        if (tristate_port_can_drive_non_z_in_ast(node->children[i], name, bus_field)) {
            return 1;
        }
    }
    return 0;
}

static const JZModuleScope *tristate_find_scope_by_name(const JZBuffer *module_scopes,
                                                         const char *name)
{
    if (!module_scopes || !name) return NULL;
    size_t count = module_scopes->len / sizeof(JZModuleScope);
    const JZModuleScope *scopes = (const JZModuleScope *)module_scopes->data;
    for (size_t i = 0; i < count; ++i) {
        if (scopes[i].name && strcmp(scopes[i].name, name) == 0) {
            return &scopes[i];
        }
    }
    return NULL;
}

static int tristate_is_identifier(const JZASTNode *node)
{
    if (!node) return 0;
    return (node->type == JZ_AST_EXPR_IDENTIFIER ||
            node->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER ||
            node->type == JZ_AST_EXPR_BUS_ACCESS);
}

/* Forward declaration for alias pool (defined later in file). */
static const char *alias_pool_store(const char *s);

/* Return the effective name for an expression node.
 * For BUS_ACCESS (e.g., pbus.SEL), returns the composite "pbus.SEL".
 * For IDENTIFIER and QUALIFIED_IDENTIFIER, returns node->name directly.
 * Uses the alias pool for persistent storage of composite names.
 */
static const char *tristate_effective_name(const JZASTNode *node)
{
    if (!node || !node->name) return NULL;
    if (node->type == JZ_AST_EXPR_BUS_ACCESS && node->text) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s.%s", node->name, node->text);
        return alias_pool_store(buf);
    }
    return node->name;
}

static void tristate_format_expr(const JZASTNode *expr, char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0) return;
    buf[0] = '\0';
    if (!expr) return;

    if (tristate_is_identifier(expr) && expr->name) {
        snprintf(buf, buf_size, "%s", tristate_effective_name(expr));
    } else if (expr->type == JZ_AST_EXPR_LITERAL && expr->text) {
        snprintf(buf, buf_size, "%s", expr->text);
    } else if (expr->type == JZ_AST_EXPR_SLICE && expr->child_count >= 3) {
        const JZASTNode *base = expr->children[0];
        const JZASTNode *idx1 = expr->children[1];
        const JZASTNode *idx2 = expr->children[2];

        char base_buf[32] = "";
        char idx1_buf[16] = "";
        char idx2_buf[16] = "";

        if (base && tristate_is_identifier(base) && base->name) {
            snprintf(base_buf, sizeof(base_buf), "%s", tristate_effective_name(base));
        }
        if (idx1 && idx1->type == JZ_AST_EXPR_LITERAL && idx1->text) {
            snprintf(idx1_buf, sizeof(idx1_buf), "%s", idx1->text);
        }
        if (idx2 && idx2->type == JZ_AST_EXPR_LITERAL && idx2->text) {
            snprintf(idx2_buf, sizeof(idx2_buf), "%s", idx2->text);
        }

        if (base_buf[0] && idx1_buf[0]) {
            if (idx2_buf[0] && strcmp(idx1_buf, idx2_buf) != 0) {
                snprintf(buf, buf_size, "%s[%s:%s]", base_buf, idx1_buf, idx2_buf);
            } else {
                snprintf(buf, buf_size, "%s[%s]", base_buf, idx1_buf);
            }
        }
    }
}

static size_t tristate_format_condition(const JZASTNode *expr, char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0) return 0;
    buf[0] = '\0';
    if (!expr || buf_size < 2) return 0;

    if (tristate_is_identifier(expr) && expr->name) {
        int n = snprintf(buf, buf_size, "%s", tristate_effective_name(expr));
        return (n > 0 && (size_t)n < buf_size) ? (size_t)n : 0;
    }

    if (expr->type == JZ_AST_EXPR_LITERAL && expr->text) {
        int n = snprintf(buf, buf_size, "%s", expr->text);
        return (n > 0 && (size_t)n < buf_size) ? (size_t)n : 0;
    }

    if (expr->type == JZ_AST_EXPR_SLICE && expr->child_count >= 3) {
        char tmp[128];
        tristate_format_expr(expr, tmp, sizeof(tmp));
        if (tmp[0]) {
            int n = snprintf(buf, buf_size, "%s", tmp);
            return (n > 0 && (size_t)n < buf_size) ? (size_t)n : 0;
        }
        return 0;
    }

    if (expr->type == JZ_AST_EXPR_UNARY && expr->block_kind && expr->child_count >= 1) {
        const char *op_str = "";
        if (strcmp(expr->block_kind, "LOG_NOT") == 0) op_str = "!";
        else if (strcmp(expr->block_kind, "BIT_NOT") == 0) op_str = "~";
        else op_str = expr->block_kind;

        char operand[192];
        size_t olen = tristate_format_condition(expr->children[0], operand, sizeof(operand));
        if (olen) {
            int n = snprintf(buf, buf_size, "%s%s", op_str, operand);
            return (n > 0 && (size_t)n < buf_size) ? (size_t)n : 0;
        }
        return 0;
    }

    if (expr->type == JZ_AST_EXPR_BINARY && expr->block_kind && expr->child_count >= 2) {
        const char *op_str = "??";
        if (strcmp(expr->block_kind, "EQ") == 0) op_str = " == ";
        else if (strcmp(expr->block_kind, "NE") == 0) op_str = " != ";
        else if (strcmp(expr->block_kind, "LT") == 0) op_str = " < ";
        else if (strcmp(expr->block_kind, "GT") == 0) op_str = " > ";
        else if (strcmp(expr->block_kind, "LE") == 0) op_str = " <= ";
        else if (strcmp(expr->block_kind, "GE") == 0) op_str = " >= ";
        else if (strcmp(expr->block_kind, "LOG_AND") == 0) op_str = " && ";
        else if (strcmp(expr->block_kind, "LOG_OR") == 0) op_str = " || ";
        else if (strcmp(expr->block_kind, "BIT_AND") == 0) op_str = " & ";
        else if (strcmp(expr->block_kind, "BIT_OR") == 0) op_str = " | ";
        else if (strcmp(expr->block_kind, "BIT_XOR") == 0) op_str = " ^ ";
        else op_str = expr->block_kind;

        char lhs_buf[128], rhs_buf[128];
        size_t llen = tristate_format_condition(expr->children[0], lhs_buf, sizeof(lhs_buf));
        size_t rlen = tristate_format_condition(expr->children[1], rhs_buf, sizeof(rhs_buf));
        if (llen && rlen) {
            int n = snprintf(buf, buf_size, "%s%s%s", lhs_buf, op_str, rhs_buf);
            return (n > 0 && (size_t)n < buf_size) ? (size_t)n : 0;
        }
        return 0;
    }

    return 0;
}

static void tristate_collect_compare_terms(const JZASTNode *cond,
                                           JZCompareTerm *terms,
                                           size_t *n_terms,
                                           size_t max_terms)
{
    if (!cond || !terms || !n_terms || *n_terms >= max_terms) return;

    if (cond->type != JZ_AST_EXPR_BINARY || !cond->block_kind) return;

    if (strcmp(cond->block_kind, "LOG_AND") == 0 && cond->child_count >= 2) {
        tristate_collect_compare_terms(cond->children[0], terms, n_terms, max_terms);
        tristate_collect_compare_terms(cond->children[1], terms, n_terms, max_terms);
        return;
    }

    int is_eq = (strcmp(cond->block_kind, "EQ") == 0);
    int is_ne = (strcmp(cond->block_kind, "NE") == 0);
    if (!is_eq && !is_ne) return;
    if (cond->child_count < 2) return;

    const JZASTNode *lhs = cond->children[0];
    const JZASTNode *rhs = cond->children[1];

    if (lhs && tristate_is_identifier(lhs) && lhs->name &&
        rhs && rhs->type == JZ_AST_EXPR_LITERAL && rhs->text) {
        terms[*n_terms].input_name = tristate_effective_name(lhs);
        terms[*n_terms].compare_value = rhs->text;
        terms[*n_terms].is_inverted = is_ne ? 1 : 0;
        (*n_terms)++;
        return;
    }
    if (rhs && tristate_is_identifier(rhs) && rhs->name &&
        lhs && lhs->type == JZ_AST_EXPR_LITERAL && lhs->text) {
        terms[*n_terms].input_name = tristate_effective_name(rhs);
        terms[*n_terms].compare_value = lhs->text;
        terms[*n_terms].is_inverted = is_ne ? 1 : 0;
        (*n_terms)++;
        return;
    }

    if (lhs && tristate_is_identifier(lhs) && lhs->name &&
        rhs && tristate_is_identifier(rhs) && rhs->name) {
        terms[*n_terms].input_name = tristate_effective_name(lhs);
        terms[*n_terms].compare_value = tristate_effective_name(rhs);
        terms[*n_terms].is_inverted = is_ne ? 1 : 0;
        (*n_terms)++;
        return;
    }
}

/* Forward declaration. */
static int tristate_extract_guard_from_cond(const JZASTNode *cond,
                                            JZTristateGuardInfo *out);

/* -------------------------------------------------------------------------
 *  IF/ELIF guard extraction
 * -------------------------------------------------------------------------
 */

typedef struct {
    const JZASTNode *expr;
    int              neg;
} JZGuardTerm;

static int tristate_find_if_guards_recurse(const JZASTNode *node,
                                            const JZASTNode *target,
                                            JZGuardTerm *terms,
                                            size_t *n_terms,
                                            size_t max_terms);

static int tristate_scan_children(JZASTNode **children,
                                   size_t count,
                                   const JZASTNode *target,
                                   JZGuardTerm *terms,
                                   size_t *n_terms,
                                   size_t max_terms)
{
    for (size_t i = 0; i < count; ++i) {
        const JZASTNode *child = children[i];
        if (!child) continue;

        if (child->type == JZ_AST_STMT_ELSE || child->type == JZ_AST_STMT_ELIF) {
            size_t saved = *n_terms;

            /* Push negation of all preceding IF/ELIF conditions in
             * the chain.  An ELIF/ELSE is only reachable when every
             * earlier branch in the same chain was false. */
            for (size_t j = i; j > 0; --j) {
                const JZASTNode *prev = children[j - 1];
                if (!prev) break;
                if ((prev->type == JZ_AST_STMT_IF || prev->type == JZ_AST_STMT_ELIF) &&
                    prev->child_count >= 1 && prev->children[0]) {
                    if (*n_terms < max_terms) {
                        terms[*n_terms].expr = prev->children[0];
                        terms[*n_terms].neg  = 1;
                        (*n_terms)++;
                    }
                    if (prev->type == JZ_AST_STMT_IF) break;
                } else {
                    break;
                }
            }

            if (child->type == JZ_AST_STMT_ELIF) {
                /* For ELIF, recurse normally — this will push the
                 * ELIF's own condition (positive) on top of the
                 * negated predecessor terms we just added. */
                if (tristate_find_if_guards_recurse(child, target,
                                                     terms, n_terms, max_terms)) {
                    return 1;
                }
            } else {
                /* ELSE: scan body directly. */
                if (tristate_scan_children(child->children, child->child_count,
                                            target, terms, n_terms, max_terms)) {
                    return 1;
                }
            }

            *n_terms = saved;
        } else {
            if (tristate_find_if_guards_recurse(child, target,
                                                 terms, n_terms, max_terms)) {
                return 1;
            }
        }
    }
    return 0;
}

static int tristate_find_if_guards_recurse(const JZASTNode *node,
                                            const JZASTNode *target,
                                            JZGuardTerm *terms,
                                            size_t *n_terms,
                                            size_t max_terms)
{
    if (!node || !target) return 0;

    if (node == target) return 1;

    if ((node->type == JZ_AST_STMT_IF || node->type == JZ_AST_STMT_ELIF) &&
        node->child_count >= 2 && node->children[0]) {

        size_t saved = *n_terms;
        if (*n_terms < max_terms) {
            terms[*n_terms].expr = node->children[0];
            terms[*n_terms].neg  = 0;
            (*n_terms)++;
        }

        if (tristate_scan_children(node->children + 1, node->child_count - 1,
                                    target, terms, n_terms, max_terms)) {
            return 1;
        }

        *n_terms = saved;
        return 0;
    }

    return tristate_scan_children(node->children, node->child_count,
                                   target, terms, n_terms, max_terms);
}

static int tristate_extract_if_guard(const JZASTNode *module_root,
                                      const JZASTNode *target_stmt,
                                      JZTristateGuardInfo *guard)
{
    if (!module_root || !target_stmt || !guard) return 0;

    JZGuardTerm terms[16];
    size_t n_terms = 0;

    if (!tristate_find_if_guards_recurse(module_root, target_stmt,
                                          terms, &n_terms, 16)) {
        return 0;
    }
    if (n_terms == 0) return 0;

    char buf[256];
    size_t pos = 0;
    buf[0] = '\0';

    for (size_t i = 0; i < n_terms; ++i) {
        if (i > 0 && pos < sizeof(buf) - 1) {
            size_t added = (size_t)snprintf(buf + pos, sizeof(buf) - pos, " && ");
            if (pos + added >= sizeof(buf)) break;
            pos += added;
        }

        char term[128];
        size_t tlen = tristate_format_condition(terms[i].expr, term, sizeof(term));
        if (tlen == 0) {
            return 0;
        }

        size_t added;
        if (terms[i].neg) {
            added = (size_t)snprintf(buf + pos, sizeof(buf) - pos, "!(%s)", term);
        } else {
            int needs_parens = (n_terms > 1 &&
                                terms[i].expr->type == JZ_AST_EXPR_BINARY &&
                                terms[i].expr->block_kind &&
                                strcmp(terms[i].expr->block_kind, "LOG_OR") == 0);
            if (needs_parens) {
                added = (size_t)snprintf(buf + pos, sizeof(buf) - pos, "(%s)", term);
            } else {
                added = (size_t)snprintf(buf + pos, sizeof(buf) - pos, "%s", term);
            }
        }
        if (pos + added >= sizeof(buf)) break;
        pos += added;
    }

    strncpy(guard->condition_text, buf, sizeof(guard->condition_text) - 1);
    guard->condition_text[sizeof(guard->condition_text) - 1] = '\0';

    guard->n_guard_terms = 0;
    for (size_t i = 0; i < n_terms && i < JZ_MAX_GUARD_TERMS; ++i) {
        guard->guard_terms[guard->n_guard_terms].expr = terms[i].expr;
        guard->guard_terms[guard->n_guard_terms].neg  = terms[i].neg;
        guard->n_guard_terms++;
    }

    for (size_t i = 0; i < n_terms; ++i) {
        JZTristateGuardInfo tmp;
        memset(&tmp, 0, sizeof(tmp));
        int rc = tristate_extract_guard_from_cond(terms[i].expr, &tmp);
        if (rc) {
            guard->input_name = tmp.input_name;
            guard->compare_lit = tmp.compare_lit;
            guard->is_inverted = terms[i].neg ? !tmp.is_inverted : tmp.is_inverted;
            strncpy(guard->normalized_lhs, tmp.normalized_lhs,
                    sizeof(guard->normalized_lhs) - 1);
            guard->normalized_lhs[sizeof(guard->normalized_lhs) - 1] = '\0';
            strncpy(guard->normalized_rhs, tmp.normalized_rhs,
                    sizeof(guard->normalized_rhs) - 1);
            guard->normalized_rhs[sizeof(guard->normalized_rhs) - 1] = '\0';
            return rc;
        }
    }

    return 3;
}

static int tristate_extract_guard_from_cond(const JZASTNode *cond,
                                            JZTristateGuardInfo *out)
{
    if (!cond || !out) return 0;
    out->input_name = NULL;
    out->compare_lit = NULL;
    out->is_inverted = 0;
    out->normalized_lhs[0] = '\0';
    out->normalized_rhs[0] = '\0';

    if (cond->type != JZ_AST_EXPR_BINARY) return 0;
    if (!cond->block_kind) return 0;

    int is_eq = (strcmp(cond->block_kind, "EQ") == 0);
    int is_ne = (strcmp(cond->block_kind, "NE") == 0);

    if (!is_eq && !is_ne) {
        if (strcmp(cond->block_kind, "LOG_AND") == 0 && cond->child_count >= 2) {
            out->n_compare_terms = 0;
            tristate_collect_compare_terms(cond, out->compare_terms,
                                           &out->n_compare_terms,
                                           JZ_MAX_COMPARE_TERMS);

            size_t saved_n = out->n_compare_terms;
            int rc = tristate_extract_guard_from_cond(cond->children[0], out);
            if (!rc) {
                rc = tristate_extract_guard_from_cond(cond->children[1], out);
            }
            out->n_compare_terms = saved_n;
            return rc;
        }
        return 0;
    }
    if (cond->child_count < 2) return 0;

    const JZASTNode *lhs = cond->children[0];
    const JZASTNode *rhs = cond->children[1];

    tristate_format_expr(lhs, out->normalized_lhs, sizeof(out->normalized_lhs));
    tristate_format_expr(rhs, out->normalized_rhs, sizeof(out->normalized_rhs));

    /* Case 1: identifier == literal */
    if (lhs && tristate_is_identifier(lhs) && lhs->name &&
        rhs && rhs->type == JZ_AST_EXPR_LITERAL && rhs->text) {
        out->input_name = tristate_effective_name(lhs);
        out->compare_lit = rhs->text;
        out->is_inverted = is_ne ? 1 : 0;
        if (out->n_compare_terms == 0) {
            out->compare_terms[0].input_name = tristate_effective_name(lhs);
            out->compare_terms[0].compare_value = rhs->text;
            out->compare_terms[0].is_inverted = is_ne ? 1 : 0;
            out->n_compare_terms = 1;
        }
        return 1;
    }
    if (rhs && tristate_is_identifier(rhs) && rhs->name &&
        lhs && lhs->type == JZ_AST_EXPR_LITERAL && lhs->text) {
        out->input_name = tristate_effective_name(rhs);
        out->compare_lit = lhs->text;
        out->is_inverted = is_ne ? 1 : 0;
        if (out->n_compare_terms == 0) {
            out->compare_terms[0].input_name = tristate_effective_name(rhs);
            out->compare_terms[0].compare_value = lhs->text;
            out->compare_terms[0].is_inverted = is_ne ? 1 : 0;
            out->n_compare_terms = 1;
        }
        return 1;
    }

    /* Case 2: identifier == identifier */
    if (lhs && tristate_is_identifier(lhs) && lhs->name &&
        rhs && tristate_is_identifier(rhs) && rhs->name) {
        out->input_name = tristate_effective_name(lhs);
        out->compare_lit = tristate_effective_name(rhs);
        out->is_inverted = is_ne ? 1 : 0;
        if (out->n_compare_terms == 0) {
            out->compare_terms[0].input_name = tristate_effective_name(lhs);
            out->compare_terms[0].compare_value = tristate_effective_name(rhs);
            out->compare_terms[0].is_inverted = is_ne ? 1 : 0;
            out->n_compare_terms = 1;
        }
        return 2;
    }

    /* Case 3: complex expressions */
    if (out->normalized_lhs[0] && out->normalized_rhs[0]) {
        out->is_inverted = is_ne ? 1 : 0;
        return 3;
    }

    return 0;
}

static int tristate_find_port_guard_in_ast(const JZASTNode *node,
                                           const char *port_name,
                                           const char *bus_field,
                                           JZTristateGuardInfo *out)
{
    if (!node || !port_name || !out) return 0;

    if (node->type == JZ_AST_STMT_ASSIGN && node->child_count >= 2) {
        const JZASTNode *lhs = node->children[0];
        const JZASTNode *rhs = node->children[1];

        if (tristate_lhs_matches_port(lhs, port_name, bus_field)) {
            if (rhs && rhs->type == JZ_AST_EXPR_TERNARY && rhs->child_count >= 3) {
                const JZASTNode *cond = rhs->children[0];
                const JZASTNode *true_branch = rhs->children[1];
                const JZASTNode *false_branch = rhs->children[2];

                int false_is_z = tristate_expr_is_all_z_literal(false_branch);
                int true_is_z = tristate_expr_is_all_z_literal(true_branch);

                if (false_is_z && !true_is_z) {
                    int rc = tristate_extract_guard_from_cond(cond, out);
                    if (rc) {
                        tristate_format_condition(cond, out->condition_text,
                                                  sizeof(out->condition_text));
                        return rc;
                    }
                } else if (true_is_z && !false_is_z) {
                    int rc = tristate_extract_guard_from_cond(cond, out);
                    if (rc) {
                        out->is_inverted = !out->is_inverted;
                        tristate_format_condition(cond, out->condition_text,
                                                  sizeof(out->condition_text));
                        return rc;
                    }
                }
            }
        }
    }

    for (size_t i = 0; i < node->child_count; ++i) {
        int rc = tristate_find_port_guard_in_ast(node->children[i], port_name, bus_field, out);
        if (rc) {
            return rc;
        }
    }
    return 0;
}

/* Resolve a qualified identifier (e.g., DEV.ROM) to its literal value
 * by looking up the global constant definition.
 */
static const char *tristate_resolve_qualified_identifier(const char *full_name)
{
    if (!full_name || !g_tristate_project_symbols || !g_tristate_project_symbols->data) {
        return NULL;
    }

    /* Parse "NAMESPACE.CONST_NAME" */
    const char *dot = strchr(full_name, '.');
    if (!dot || !*(dot + 1)) return NULL;

    char namespace_name[128];
    size_t ns_len = (size_t)(dot - full_name);
    if (ns_len >= sizeof(namespace_name)) return NULL;
    memcpy(namespace_name, full_name, ns_len);
    namespace_name[ns_len] = '\0';

    const char *const_name = dot + 1;

    /* Search project symbols for the global namespace */
    const JZSymbol *syms = (const JZSymbol *)g_tristate_project_symbols->data;
    size_t count = g_tristate_project_symbols->len / sizeof(JZSymbol);

    for (size_t i = 0; i < count; ++i) {
        if (syms[i].kind != JZ_SYM_GLOBAL || !syms[i].name || !syms[i].node) continue;
        if (strcmp(syms[i].name, namespace_name) != 0) continue;

        /* Found the global namespace, now search for the constant */
        const JZASTNode *glob_block = syms[i].node;
        for (size_t ci = 0; ci < glob_block->child_count; ++ci) {
            const JZASTNode *child = glob_block->children[ci];
            if (!child || !child->name || !child->text) continue;
            if (strcmp(child->name, const_name) == 0) {
                return child->text;  /* Return the literal value (e.g., "3'b000") */
            }
        }
        break;
    }

    return NULL;
}

/* Resolve a simple identifier (e.g., DEV_A) to its literal value
 * by looking up module CONST values in the parent module scope.
 */
static const char *tristate_resolve_module_const(const char *name)
{
    if (!name || !g_tristate_parent_scope || !g_tristate_parent_scope->node) {
        return NULL;
    }

    /* Search for CONST blocks in the parent module */
    const JZASTNode *mod = g_tristate_parent_scope->node;
    for (size_t bi = 0; bi < mod->child_count; ++bi) {
        const JZASTNode *blk = mod->children[bi];
        if (!blk || blk->type != JZ_AST_CONST_BLOCK) continue;

        /* Search for the constant in this block */
        for (size_t ci = 0; ci < blk->child_count; ++ci) {
            const JZASTNode *decl = blk->children[ci];
            if (!decl || decl->type != JZ_AST_CONST_DECL) continue;
            if (!decl->name || !decl->text) continue;
            if (strcmp(decl->name, name) == 0) {
                return decl->text;  /* Return the literal value (e.g., "3'b000") */
            }
        }
    }

    return NULL;
}

static const char *tristate_get_binding_literal(const JZASTNode *inst_node,
                                                 const char *port_name)
{
    if (!inst_node || !port_name) return NULL;
    for (size_t i = 0; i < inst_node->child_count; ++i) {
        const JZASTNode *bind = inst_node->children[i];
        if (!bind || bind->type != JZ_AST_PORT_DECL) continue;
        if (!bind->name || strcmp(bind->name, port_name) != 0) continue;
        if (!bind->block_kind || strcmp(bind->block_kind, "IN") != 0) continue;
        if (bind->child_count >= 1 && bind->children[0]) {
            const JZASTNode *expr = bind->children[0];
            if (expr->type == JZ_AST_EXPR_LITERAL && expr->text) {
                return expr->text;
            }
            /* Handle qualified identifiers (e.g., DEV.ROM for globals) */
            if (expr->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER && expr->name) {
                return tristate_resolve_qualified_identifier(expr->name);
            }
            /* Handle simple identifiers (e.g., DEV_A for module CONST) */
            if (expr->type == JZ_AST_EXPR_IDENTIFIER && expr->name) {
                return tristate_resolve_module_const(expr->name);
            }
        }
    }
    return NULL;
}

/* Static string pool for alias resolution. */
#define ALIAS_POOL_SLOTS 64
#define ALIAS_POOL_SLOT_SIZE 128
static char g_alias_pool[ALIAS_POOL_SLOTS][ALIAS_POOL_SLOT_SIZE];
static size_t g_alias_pool_idx = 0;

static const char *alias_pool_store(const char *s)
{
    if (!s) return NULL;
    size_t slot = g_alias_pool_idx % ALIAS_POOL_SLOTS;
    g_alias_pool_idx++;
    strncpy(g_alias_pool[slot], s, ALIAS_POOL_SLOT_SIZE - 1);
    g_alias_pool[slot][ALIAS_POOL_SLOT_SIZE - 1] = '\0';
    return g_alias_pool[slot];
}

typedef struct {
    const char *from;
    const char *to;
} JZAliasEntry;

#define JZ_MAX_ALIASES 32

static size_t tristate_collect_bus_aliases(const JZASTNode *node,
                                           const char *port_name,
                                           JZAliasEntry *aliases,
                                           size_t max_aliases)
{
    size_t count = 0;
    if (!node || !port_name || !aliases) return 0;

    if (node->type == JZ_AST_STMT_ASSIGN && node->child_count >= 2 &&
        node->block_kind) {
        int is_alias = (strcmp(node->block_kind, "ALIAS") == 0 ||
                        strcmp(node->block_kind, "ALIAS_Z") == 0 ||
                        strcmp(node->block_kind, "ALIAS_S") == 0);
        if (is_alias) {
            const JZASTNode *lhs = node->children[0];
            const JZASTNode *rhs = node->children[1];

            if (lhs && lhs->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER && lhs->name) {
                size_t plen = strlen(port_name);
                if (strncmp(lhs->name, port_name, plen) == 0 &&
                    lhs->name[plen] == '.') {
                    if (rhs && rhs->type == JZ_AST_EXPR_IDENTIFIER && rhs->name) {
                        if (count < max_aliases) {
                            aliases[count].from = rhs->name;
                            aliases[count].to = lhs->name;
                            count++;
                        }
                    }
                }
            }
            if (rhs && rhs->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER && rhs->name) {
                size_t plen = strlen(port_name);
                if (strncmp(rhs->name, port_name, plen) == 0 &&
                    rhs->name[plen] == '.') {
                    if (lhs && lhs->type == JZ_AST_EXPR_IDENTIFIER && lhs->name) {
                        if (count < max_aliases) {
                            aliases[count].from = lhs->name;
                            aliases[count].to = rhs->name;
                            count++;
                        }
                    }
                }
            }
        }
    }

    for (size_t i = 0; i < node->child_count; ++i) {
        count += tristate_collect_bus_aliases(node->children[i], port_name,
                                              aliases + count,
                                              max_aliases - count);
        if (count >= max_aliases) break;
    }
    return count;
}

static int tristate_get_instance_guard(const JZASTNode *inst_node,
                                        const char *net_name,
                                        const char *bus_field,
                                        const JZBuffer *module_scopes,
                                        JZTristateGuardInfo *out)
{
    if (!inst_node || inst_node->type != JZ_AST_MODULE_INSTANCE ||
        !net_name || !module_scopes || !out) {
        return 0;
    }

    const char *child_port_name = NULL;
    for (size_t bi = 0; bi < inst_node->child_count; ++bi) {
        const JZASTNode *bind = inst_node->children[bi];
        if (!bind || bind->type != JZ_AST_PORT_DECL || !bind->block_kind) continue;
        const char *dir = bind->block_kind;
        int is_out   = (strcmp(dir, "OUT") == 0);
        int is_inout = (strcmp(dir, "INOUT") == 0);
        int is_bus   = (strcmp(dir, "BUS") == 0);
        if (!is_out && !is_inout && !is_bus) continue;
        if (bind->child_count == 0) continue;

        const JZASTNode *rhs = bind->children[0];
        if (rhs && rhs->type == JZ_AST_EXPR_IDENTIFIER && rhs->name &&
            strcmp(rhs->name, net_name) == 0) {
            child_port_name = bind->name;
            break;
        }
        const JZASTNode *base = rhs;
        while (base && base->type == JZ_AST_EXPR_SLICE && base->child_count >= 1) {
            base = base->children[0];
        }
        if (base && base->type == JZ_AST_EXPR_IDENTIFIER && base->name &&
            strcmp(base->name, net_name) == 0) {
            child_port_name = bind->name;
            break;
        }
    }

    if (!child_port_name) return 0;

    const char *child_mod_name = inst_node->text;
    if (!child_mod_name) return 0;

    const JZModuleScope *child_scope = tristate_find_scope_by_name(module_scopes, child_mod_name);
    if (!child_scope || !child_scope->node) return 0;

    int rc = tristate_find_port_guard_in_ast(child_scope->node, child_port_name, bus_field, out);

    if (rc == 2 && out->compare_lit) {
        const char *bound_lit = tristate_get_binding_literal(inst_node, out->compare_lit);
        if (bound_lit) {
            out->compare_lit = bound_lit;
            rc = 1;
        } else {
            const char *input_lit = tristate_get_binding_literal(inst_node, out->input_name);
            if (input_lit) {
                const char *tmp = out->compare_lit;
                out->compare_lit = input_lit;
                out->input_name = tmp;
                rc = 1;
            }
            /* If neither side resolves to a constant, keep rc=2 and let
             * the multi-term compare proof use the collected compare_terms. */
        }
    }

    if (rc && out->n_compare_terms > 0) {
        JZAliasEntry aliases[JZ_MAX_ALIASES];
        size_t n_aliases = tristate_collect_bus_aliases(child_scope->node,
                                                        child_port_name,
                                                        aliases,
                                                        JZ_MAX_ALIASES);

        out->n_aliases = 0;
        for (size_t ai = 0; ai < n_aliases && out->n_aliases < JZ_MAX_DRIVER_ALIASES; ++ai) {
            out->aliases[out->n_aliases].from = alias_pool_store(aliases[ai].from);
            out->aliases[out->n_aliases].to = alias_pool_store(aliases[ai].to);
            out->n_aliases++;
        }

        for (size_t ti = 0; ti < out->n_compare_terms; ++ti) {
            JZCompareTerm *term = &out->compare_terms[ti];

            if (term->input_name) {
                for (size_t ai = 0; ai < n_aliases; ++ai) {
                    if (strcmp(term->input_name, aliases[ai].from) == 0) {
                        term->input_name = alias_pool_store(aliases[ai].to);
                        break;
                    }
                }
            }

            if (term->compare_value) {
                for (size_t ai = 0; ai < n_aliases; ++ai) {
                    if (strcmp(term->compare_value, aliases[ai].from) == 0) {
                        term->compare_value = alias_pool_store(aliases[ai].to);
                        break;
                    }
                }
            }

            if (term->compare_value) {
                const char *bound = tristate_get_binding_literal(inst_node, term->compare_value);
                if (bound) {
                    term->compare_value = bound;
                } else {
                    const char *input_bound = tristate_get_binding_literal(inst_node, term->input_name);
                    if (input_bound) {
                        const char *tmp_name = term->compare_value;
                        term->compare_value = input_bound;
                        term->input_name = tmp_name;
                    }
                }
            }
        }

        /* Normalize bus field names: strip the child port prefix so that
         * compare terms from different instances on the same net can be
         * compared by bus field name alone.
         * e.g. "pbus.CMD" -> "CMD", "src.VALID" -> "VALID" */
        size_t cplen = strlen(child_port_name);
        for (size_t ti = 0; ti < out->n_compare_terms; ++ti) {
            JZCompareTerm *term = &out->compare_terms[ti];
            if (term->input_name &&
                strncmp(term->input_name, child_port_name, cplen) == 0 &&
                term->input_name[cplen] == '.') {
                term->input_name = alias_pool_store(term->input_name + cplen + 1);
            }
        }
    }

    /* Also normalize the top-level input_name for simple guard proofs. */
    if (rc && out->input_name) {
        size_t cplen = strlen(child_port_name);
        if (strncmp(out->input_name, child_port_name, cplen) == 0 &&
            out->input_name[cplen] == '.') {
            out->input_name = alias_pool_store(out->input_name + cplen + 1);
        }
    }

    return rc;
}

static int tristate_instance_can_produce_z(const JZASTNode *inst_node,
                                            const char *net_name,
                                            const char *bus_field,
                                            const JZBuffer *module_scopes)
{
    if (!inst_node || inst_node->type != JZ_AST_MODULE_INSTANCE ||
        !net_name || !module_scopes) {
        return 0;
    }

    const char *child_port_name = NULL;
    for (size_t bi = 0; bi < inst_node->child_count; ++bi) {
        const JZASTNode *bind = inst_node->children[bi];
        if (!bind || bind->type != JZ_AST_PORT_DECL || !bind->block_kind) continue;
        const char *dir = bind->block_kind;
        int is_out   = (strcmp(dir, "OUT") == 0);
        int is_inout = (strcmp(dir, "INOUT") == 0);
        int is_bus   = (strcmp(dir, "BUS") == 0);
        if (!is_out && !is_inout && !is_bus) continue;
        if (bind->child_count == 0) continue;

        const JZASTNode *rhs = bind->children[0];
        if (rhs && rhs->type == JZ_AST_EXPR_IDENTIFIER && rhs->name &&
            strcmp(rhs->name, net_name) == 0) {
            child_port_name = bind->name;
            break;
        }
        const JZASTNode *base = rhs;
        while (base && base->type == JZ_AST_EXPR_SLICE && base->child_count >= 1) {
            base = base->children[0];
        }
        if (base && base->type == JZ_AST_EXPR_IDENTIFIER && base->name &&
            strcmp(base->name, net_name) == 0) {
            child_port_name = bind->name;
            break;
        }
    }

    if (!child_port_name) return 0;

    const char *child_mod_name = inst_node->text;
    if (!child_mod_name) return 0;

    const JZModuleScope *child_scope = tristate_find_scope_by_name(module_scopes, child_mod_name);
    if (!child_scope || !child_scope->node) return 0;

    return tristate_port_can_drive_z_in_ast(child_scope->node, child_port_name, bus_field);
}

/*
 * Determine whether a sized literal is nonzero (truthy).
 * Returns 1 if the value portion contains any digit other than '0',
 * 0 if all digits are '0', and -1 if it cannot be determined.
 */
static int tristate_literal_is_nonzero(const char *lit)
{
    if (!lit) return -1;
    const char *tick = strchr(lit, '\'');
    if (!tick) {
        /* Unsized decimal: check if all digits are '0'. */
        int saw_digit = 0;
        for (const char *p = lit; *p; ++p) {
            if (*p == '_') continue;
            if (*p >= '1' && *p <= '9') return 1;
            if (*p == '0') { saw_digit = 1; continue; }
            return -1; /* not a simple decimal */
        }
        return saw_digit ? 0 : -1;
    }
    const char *value = tick + 2; /* skip base char */
    if (!*value) return -1;
    for (const char *p = value; *p; ++p) {
        char c = *p;
        if (c == '_') continue;
        if (c == '0') continue;
        if ((c >= '1' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
            return 1;
        return -1; /* x, z, etc. */
    }
    return 0;
}

/*
 * Walk the child module AST looking for a ternary assignment to the given
 * port where one branch is all-z.  If found, check whether the ternary
 * condition is a bare identifier whose corresponding input port is bound
 * to a constant at the instance level.  When the constant selects the
 * all-z branch, the instance cannot produce non-z through this port.
 *
 * Returns: 1 = definitely produces only z (caller should return 0 for
 *              can_produce_non_z), 0 = cannot prove.
 */
static int tristate_instance_ternary_forces_z(const JZASTNode *node,
                                               const char *port_name,
                                               const char *bus_field,
                                               const JZASTNode *inst_node)
{
    if (!node || !port_name) return 0;

    if (node->type == JZ_AST_STMT_ASSIGN && node->child_count >= 2) {
        const char *op = node->block_kind ? node->block_kind : "";
        int is_alias = (strcmp(op, "ALIAS") == 0 ||
                        strcmp(op, "ALIAS_Z") == 0 ||
                        strcmp(op, "ALIAS_S") == 0);

        if (!is_alias) {
            const JZASTNode *lhs = node->children[0];
            const JZASTNode *rhs_node = node->children[1];

            if (tristate_lhs_matches_port(lhs, port_name, bus_field) &&
                rhs_node && rhs_node->type == JZ_AST_EXPR_TERNARY &&
                rhs_node->child_count >= 3) {
                const JZASTNode *cond = rhs_node->children[0];
                const JZASTNode *true_branch = rhs_node->children[1];
                const JZASTNode *false_branch = rhs_node->children[2];

                int true_is_z  = tristate_expr_is_all_z_literal(true_branch);
                int false_is_z = tristate_expr_is_all_z_literal(false_branch);

                if (true_is_z != false_is_z) {
                    /* One branch is z, the other is not.  Check if the
                     * condition is a bare identifier bound to a constant. */
                    const char *cond_name = NULL;
                    if (cond && tristate_is_identifier(cond) && cond->name) {
                        cond_name = tristate_effective_name(cond);
                    }
                    if (cond_name) {
                        const char *bound = tristate_get_binding_literal(inst_node, cond_name);
                        if (bound) {
                            int nz = tristate_literal_is_nonzero(bound);
                            if (nz >= 0) {
                                /* nz=1 → true branch selected; nz=0 → false branch */
                                int selected_is_z = nz ? true_is_z : false_is_z;
                                if (selected_is_z) {
                                    return 1; /* constant forces the z branch */
                                }
                            }
                        }
                    }
                    /* Also handle comparison conditions (en == 1'b1, etc.) */
                    if (cond && cond->type == JZ_AST_EXPR_BINARY && cond->block_kind &&
                        cond->child_count >= 2) {
                        int is_eq = (strcmp(cond->block_kind, "EQ") == 0);
                        int is_ne = (strcmp(cond->block_kind, "NE") == 0);
                        if (is_eq || is_ne) {
                            const JZASTNode *clhs = cond->children[0];
                            const JZASTNode *crhs = cond->children[1];
                            const char *id_name = NULL;
                            const char *cmp_lit = NULL;
                            if (tristate_is_identifier(clhs) && clhs->name &&
                                crhs && crhs->type == JZ_AST_EXPR_LITERAL && crhs->text) {
                                id_name = tristate_effective_name(clhs);
                                cmp_lit = crhs->text;
                            } else if (tristate_is_identifier(crhs) && crhs->name &&
                                       clhs && clhs->type == JZ_AST_EXPR_LITERAL && clhs->text) {
                                id_name = tristate_effective_name(crhs);
                                cmp_lit = clhs->text;
                            }
                            if (id_name && cmp_lit) {
                                const char *bound = tristate_get_binding_literal(inst_node, id_name);
                                if (bound) {
                                    /* Compare the bound value to the comparison literal. */
                                    int match = (strcmp(bound, cmp_lit) == 0);
                                    int cond_true = is_eq ? match : !match;
                                    int selected_is_z = cond_true ? true_is_z : false_is_z;
                                    if (selected_is_z) {
                                        return 1;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    for (size_t i = 0; i < node->child_count; ++i) {
        if (tristate_instance_ternary_forces_z(node->children[i], port_name, bus_field, inst_node)) {
            return 1;
        }
    }
    return 0;
}

static int tristate_instance_can_produce_non_z(const JZASTNode *inst_node,
                                                const char *net_name,
                                                const char *bus_field,
                                                const JZBuffer *module_scopes)
{
    if (!inst_node || inst_node->type != JZ_AST_MODULE_INSTANCE ||
        !net_name || !module_scopes) {
        return 0;
    }

    const char *child_port_name = NULL;
    for (size_t bi = 0; bi < inst_node->child_count; ++bi) {
        const JZASTNode *bind = inst_node->children[bi];
        if (!bind || bind->type != JZ_AST_PORT_DECL || !bind->block_kind) continue;
        const char *dir = bind->block_kind;
        int is_out   = (strcmp(dir, "OUT") == 0);
        int is_inout = (strcmp(dir, "INOUT") == 0);
        int is_bus   = (strcmp(dir, "BUS") == 0);
        if (!is_out && !is_inout && !is_bus) continue;
        if (bind->child_count == 0) continue;

        const JZASTNode *rhs = bind->children[0];
        if (rhs && rhs->type == JZ_AST_EXPR_IDENTIFIER && rhs->name &&
            strcmp(rhs->name, net_name) == 0) {
            child_port_name = bind->name;
            break;
        }
        const JZASTNode *base = rhs;
        while (base && base->type == JZ_AST_EXPR_SLICE && base->child_count >= 1) {
            base = base->children[0];
        }
        if (base && base->type == JZ_AST_EXPR_IDENTIFIER && base->name &&
            strcmp(base->name, net_name) == 0) {
            child_port_name = bind->name;
            break;
        }
    }

    if (!child_port_name) return 0;

    const char *child_mod_name = inst_node->text;
    if (!child_mod_name) return 0;

    const JZModuleScope *child_scope = tristate_find_scope_by_name(module_scopes, child_mod_name);
    if (!child_scope || !child_scope->node) return 0;

    /* If the child module has a ternary assignment to this port where the
     * condition is bound to a constant that selects the all-z branch,
     * this instance cannot produce non-z.
     */
    if (tristate_instance_ternary_forces_z(child_scope->node, child_port_name, bus_field, inst_node)) {
        return 0;
    }

    return tristate_port_can_drive_non_z_in_ast(child_scope->node, child_port_name, bus_field);
}

static const JZASTNode *tristate_find_port_driver_stmt(const JZASTNode *node,
                                                        const char *port_name,
                                                        const char *bus_field)
{
    if (!node || !port_name) return NULL;

    if (node->type == JZ_AST_STMT_ASSIGN && node->child_count >= 2) {
        const char *op = node->block_kind ? node->block_kind : "";
        int is_alias = (strcmp(op, "ALIAS") == 0 ||
                        strcmp(op, "ALIAS_Z") == 0 ||
                        strcmp(op, "ALIAS_S") == 0);

        if (!is_alias) {
            const JZASTNode *lhs = node->children[0];

            if (tristate_lhs_matches_port(lhs, port_name, bus_field)) {
                return node;
            }
        }
    }

    for (size_t i = 0; i < node->child_count; ++i) {
        const JZASTNode *found = tristate_find_port_driver_stmt(node->children[i], port_name, bus_field);
        if (found) {
            return found;
        }
    }
    return NULL;
}

static JZLocation tristate_get_instance_driver_loc(const JZASTNode *inst_node,
                                                    const char *net_name,
                                                    const char *bus_field,
                                                    const JZBuffer *module_scopes)
{
    JZLocation default_loc = {NULL, 0, 0};
    if (!inst_node || inst_node->type != JZ_AST_MODULE_INSTANCE ||
        !net_name || !module_scopes) {
        return default_loc;
    }

    const char *child_port_name = NULL;
    for (size_t bi = 0; bi < inst_node->child_count; ++bi) {
        const JZASTNode *bind = inst_node->children[bi];
        if (!bind || bind->type != JZ_AST_PORT_DECL || !bind->block_kind) continue;
        const char *dir = bind->block_kind;
        int is_out   = (strcmp(dir, "OUT") == 0);
        int is_inout = (strcmp(dir, "INOUT") == 0);
        int is_bus   = (strcmp(dir, "BUS") == 0);
        if (!is_out && !is_inout && !is_bus) continue;
        if (bind->child_count == 0) continue;

        const JZASTNode *rhs = bind->children[0];
        if (rhs && rhs->type == JZ_AST_EXPR_IDENTIFIER && rhs->name &&
            strcmp(rhs->name, net_name) == 0) {
            child_port_name = bind->name;
            break;
        }
        const JZASTNode *base = rhs;
        while (base && base->type == JZ_AST_EXPR_SLICE && base->child_count >= 1) {
            base = base->children[0];
        }
        if (base && base->type == JZ_AST_EXPR_IDENTIFIER && base->name &&
            strcmp(base->name, net_name) == 0) {
            child_port_name = bind->name;
            break;
        }
    }

    if (!child_port_name) return default_loc;

    const char *child_mod_name = inst_node->text;
    if (!child_mod_name) return default_loc;

    const JZModuleScope *child_scope = tristate_find_scope_by_name(module_scopes, child_mod_name);
    if (!child_scope || !child_scope->node) return default_loc;

    const JZASTNode *driver_stmt = tristate_find_port_driver_stmt(child_scope->node, child_port_name, bus_field);
    if (driver_stmt) {
        return driver_stmt->loc;
    }

    return default_loc;
}

/* -------------------------------------------------------------------------
 *  Pairwise mutual-exclusion proof helper
 * -------------------------------------------------------------------------
 */
static int tristate_prove_pair(const JZTristateDriver *a,
                               const JZTristateDriver *b)
{
    /* Try simple guard proofs (input_name + compare_value). */
    if (a->enable.input_name && a->enable.compare_value &&
        b->enable.input_name && b->enable.compare_value) {
        if (strcmp(a->enable.input_name, b->enable.input_name) == 0) {
            if (strcmp(a->enable.compare_value, b->enable.compare_value) != 0 &&
                a->enable.is_inverted == b->enable.is_inverted) {
                return 1;
            }
            if (strcmp(a->enable.compare_value, b->enable.compare_value) == 0 &&
                a->enable.is_inverted != b->enable.is_inverted) {
                return 1;
            }
        }
    }

    /* Try multi-term comparison proof. */
    if (a->enable.n_compare_terms > 0 && b->enable.n_compare_terms > 0) {
        for (size_t ai = 0; ai < a->enable.n_compare_terms; ++ai) {
            const JZCompareTerm *ta = &a->enable.compare_terms[ai];
            if (!ta->input_name || !ta->compare_value) continue;
            for (size_t bi = 0; bi < b->enable.n_compare_terms; ++bi) {
                const JZCompareTerm *tb = &b->enable.compare_terms[bi];
                if (!tb->input_name || !tb->compare_value) continue;
                if (strcmp(ta->input_name, tb->input_name) != 0) continue;
                if (strcmp(ta->compare_value, tb->compare_value) != 0 &&
                    ta->is_inverted == tb->is_inverted) {
                    return 1;
                }
                if (strcmp(ta->compare_value, tb->compare_value) == 0 &&
                    ta->is_inverted != tb->is_inverted) {
                    return 1;
                }
            }
        }
    }

    /* Try IF/ELSE branch proof via structured guard terms.
     * Guard terms are conjunctive (all must be true for the driver to fire).
     * If any term in A has the same expression but opposite negation as any
     * term in B, the two drivers cannot be active simultaneously. */
    if (a->enable.n_guard_terms > 0 && b->enable.n_guard_terms > 0) {
        for (size_t ai = 0; ai < a->enable.n_guard_terms; ++ai) {
            for (size_t bi = 0; bi < b->enable.n_guard_terms; ++bi) {
                if (a->enable.guard_terms[ai].expr ==
                        b->enable.guard_terms[bi].expr &&
                    a->enable.guard_terms[ai].neg !=
                        b->enable.guard_terms[bi].neg) {
                    return 1;
                }
            }
        }
    }

    return 0;
}

/* -------------------------------------------------------------------------
 *  Core analysis: jz_tristate_analyze_net
 * -------------------------------------------------------------------------
 */
void jz_tristate_analyze_net(JZTristateNetInfo *info,
                             const JZNet *net,
                             const char *net_name,
                             const char *bus_field,
                             const JZModuleScope *scope,
                             const JZBuffer *module_scopes)
{
    if (!info || !net) return;
    memset(info, 0, sizeof(*info));
    info->name = net_name;

    JZASTNode **atoms = (JZASTNode **)net->atoms.data;
    size_t atom_count = net->atoms.len / sizeof(JZASTNode *);
    if (atom_count > 0 && atoms[0]) {
        info->width = jz_tristate_decl_width_simple(atoms[0]);
        info->decl_loc = atoms[0]->loc;
    }

    /* Collect drivers. */
    JZASTNode **drv = (JZASTNode **)net->driver_stmts.data;
    size_t drv_count = net->driver_stmts.len / sizeof(JZASTNode *);

    for (size_t di = 0; di < drv_count; ++di) {
        JZASTNode *stmt = drv[di];
        if (!stmt) continue;

        int seen = 0;
        for (size_t dj = 0; dj < di; ++dj) {
            if (drv[dj] == stmt) {
                seen = 1;
                break;
            }
        }
        if (seen) continue;

        JZTristateDriver driver;
        memset(&driver, 0, sizeof(driver));
        driver.stmt = stmt;
        driver.loc = stmt->loc;

        if (stmt->type == JZ_AST_MODULE_INSTANCE) {
            driver.instance_name = stmt->name;
            driver.module_name = stmt->text;
            driver.can_produce_z = tristate_instance_can_produce_z(stmt, net_name, bus_field, module_scopes);
            driver.can_produce_non_z = tristate_instance_can_produce_non_z(stmt, net_name, bus_field, module_scopes);

            JZLocation real_loc = tristate_get_instance_driver_loc(stmt, net_name, bus_field, module_scopes);
            if (real_loc.filename) {
                driver.loc = real_loc;
            }

            JZTristateGuardInfo guard;
            memset(&guard, 0, sizeof(guard));
            int guard_rc = tristate_get_instance_guard(stmt, net_name, bus_field, module_scopes, &guard);
            if (guard_rc) {
                driver.enable.input_name = guard.input_name;
                driver.enable.compare_value = guard.compare_lit;
                driver.enable.is_inverted = guard.is_inverted;
                driver.enable.is_complex = (guard_rc == 3) ? 1 : 0;
                strncpy(driver.enable.normalized_lhs, guard.normalized_lhs,
                        sizeof(driver.enable.normalized_lhs) - 1);
                driver.enable.normalized_lhs[sizeof(driver.enable.normalized_lhs) - 1] = '\0';
                strncpy(driver.enable.normalized_rhs, guard.normalized_rhs,
                        sizeof(driver.enable.normalized_rhs) - 1);
                driver.enable.normalized_rhs[sizeof(driver.enable.normalized_rhs) - 1] = '\0';
                strncpy(driver.enable.condition_text, guard.condition_text,
                        sizeof(driver.enable.condition_text) - 1);
                driver.enable.condition_text[sizeof(driver.enable.condition_text) - 1] = '\0';
                driver.enable.n_compare_terms = guard.n_compare_terms;
                for (size_t ci = 0; ci < guard.n_compare_terms; ++ci) {
                    driver.enable.compare_terms[ci] = guard.compare_terms[ci];
                }
                driver.n_aliases = guard.n_aliases;
                for (size_t ai = 0; ai < guard.n_aliases; ++ai) {
                    driver.aliases[ai].from = guard.aliases[ai].from;
                    driver.aliases[ai].to = guard.aliases[ai].to;
                }
            }
        } else if (stmt->type == JZ_AST_STMT_ASSIGN && stmt->child_count >= 2) {
            if (bus_field) {
                const char *stmt_field = jz_tristate_extract_bus_field(stmt);
                if (!stmt_field || strcmp(stmt_field, bus_field) != 0) {
                    continue;
                }
            }

            JZASTNode *rhs = stmt->children[1];
            driver.can_produce_z = tristate_expr_contains_z_anywhere(rhs) ? 1 : 0;
            driver.can_produce_non_z = !tristate_expr_is_all_z_literal(rhs) ? 1 : 0;

            if (rhs && rhs->type == JZ_AST_EXPR_TERNARY && rhs->child_count >= 3) {
                const JZASTNode *cond = rhs->children[0];
                const JZASTNode *true_branch = rhs->children[1];
                const JZASTNode *false_branch = rhs->children[2];

                int false_is_z = tristate_expr_is_all_z_literal(false_branch);
                int true_is_z = tristate_expr_is_all_z_literal(true_branch);

                if (false_is_z || true_is_z) {
                    JZTristateGuardInfo guard;
                    memset(&guard, 0, sizeof(guard));
                    int guard_rc = tristate_extract_guard_from_cond(cond, &guard);
                    if (guard_rc && true_is_z && !false_is_z) {
                        guard.is_inverted = !guard.is_inverted;
                    }
                    if (guard_rc) {
                        tristate_format_condition(cond, guard.condition_text,
                                                  sizeof(guard.condition_text));
                        driver.enable.input_name = guard.input_name;
                        driver.enable.compare_value = guard.compare_lit;
                        driver.enable.is_inverted = guard.is_inverted;
                        driver.enable.is_complex = (guard_rc == 3) ? 1 : 0;
                        strncpy(driver.enable.normalized_lhs, guard.normalized_lhs,
                                sizeof(driver.enable.normalized_lhs) - 1);
                        driver.enable.normalized_lhs[sizeof(driver.enable.normalized_lhs) - 1] = '\0';
                        strncpy(driver.enable.normalized_rhs, guard.normalized_rhs,
                                sizeof(driver.enable.normalized_rhs) - 1);
                        driver.enable.normalized_rhs[sizeof(driver.enable.normalized_rhs) - 1] = '\0';
                        strncpy(driver.enable.condition_text, guard.condition_text,
                                sizeof(driver.enable.condition_text) - 1);
                        driver.enable.condition_text[sizeof(driver.enable.condition_text) - 1] = '\0';
                        driver.enable.n_compare_terms = guard.n_compare_terms;
                        for (size_t ci = 0; ci < guard.n_compare_terms; ++ci) {
                            driver.enable.compare_terms[ci] = guard.compare_terms[ci];
                        }
                    }
                }
            }

            if (!driver.enable.input_name && !driver.enable.condition_text[0] &&
                scope && scope->node) {
                JZTristateGuardInfo guard;
                memset(&guard, 0, sizeof(guard));
                int guard_rc = tristate_extract_if_guard(scope->node, stmt, &guard);
                if (guard_rc) {
                    driver.enable.input_name = guard.input_name;
                    driver.enable.compare_value = guard.compare_lit;
                    driver.enable.is_inverted = guard.is_inverted;
                    driver.enable.is_complex = (guard_rc == 3) ? 1 : 0;
                    strncpy(driver.enable.normalized_lhs, guard.normalized_lhs,
                            sizeof(driver.enable.normalized_lhs) - 1);
                    driver.enable.normalized_lhs[sizeof(driver.enable.normalized_lhs) - 1] = '\0';
                    strncpy(driver.enable.normalized_rhs, guard.normalized_rhs,
                            sizeof(driver.enable.normalized_rhs) - 1);
                    driver.enable.normalized_rhs[sizeof(driver.enable.normalized_rhs) - 1] = '\0';
                    strncpy(driver.enable.condition_text, guard.condition_text,
                            sizeof(driver.enable.condition_text) - 1);
                    driver.enable.condition_text[sizeof(driver.enable.condition_text) - 1] = '\0';
                    driver.enable.n_guard_terms = guard.n_guard_terms;
                    for (size_t gi = 0; gi < guard.n_guard_terms; ++gi) {
                        driver.enable.guard_terms[gi].expr = guard.guard_terms[gi].expr;
                        driver.enable.guard_terms[gi].neg  = guard.guard_terms[gi].neg;
                    }
                    driver.enable.n_compare_terms = guard.n_compare_terms;
                    for (size_t ci = 0; ci < guard.n_compare_terms; ++ci) {
                        driver.enable.compare_terms[ci] = guard.compare_terms[ci];
                    }
                }
            }
        }

        (void)jz_buf_append(&info->drivers, &driver, sizeof(driver));
    }

    /* Collect sinks. */
    JZASTNode **snk = (JZASTNode **)net->sink_stmts.data;
    size_t snk_count = net->sink_stmts.len / sizeof(JZASTNode *);

    for (size_t si = 0; si < snk_count; ++si) {
        JZASTNode *stmt = snk[si];
        if (!stmt) continue;

        int seen = 0;
        for (size_t sj = 0; sj < si; ++sj) {
            if (snk[sj] == stmt) {
                seen = 1;
                break;
            }
        }
        if (seen) continue;

        JZTristateSink sink;
        memset(&sink, 0, sizeof(sink));
        sink.stmt = stmt;
        sink.loc = stmt->loc;
        (void)jz_buf_append(&info->sinks, &sink, sizeof(sink));
    }

    /* Analyze drivers. */
    size_t driver_count = info->drivers.len / sizeof(JZTristateDriver);
    JZTristateDriver *drivers = (JZTristateDriver *)info->drivers.data;

    if (driver_count <= 1) {
        info->result = JZ_TRISTATE_PROVEN;
        info->proof_method = JZ_TRISTATE_PROOF_SINGLE_DRIVER;
        return;
    }

    size_t non_z_count = 0;
    for (size_t i = 0; i < driver_count; ++i) {
        if (drivers[i].can_produce_non_z) {
            non_z_count++;
        }
    }

    if (non_z_count <= 1) {
        info->result = JZ_TRISTATE_PROVEN;
        info->proof_method = JZ_TRISTATE_PROOF_SINGLE_NON_Z;
        return;
    }

    /* Check for distinct constant inputs. */
    int all_have_guards = 1;
    int all_have_simple_guards = 1;
    int any_has_complex_guard = 0;
    int all_same_input = 1;
    const char *common_input = NULL;

    for (size_t i = 0; i < driver_count; ++i) {
        if (!drivers[i].can_produce_non_z) continue;
        if (drivers[i].enable.input_name && drivers[i].enable.compare_value) {
            if (!common_input) {
                common_input = drivers[i].enable.input_name;
            } else if (strcmp(common_input, drivers[i].enable.input_name) != 0) {
                all_same_input = 0;
            }
        } else if (drivers[i].enable.n_compare_terms > 0) {
            all_have_simple_guards = 0;
        } else if (drivers[i].enable.normalized_lhs[0] && drivers[i].enable.normalized_rhs[0]) {
            all_have_simple_guards = 0;
            any_has_complex_guard = 1;
        } else {
            all_have_guards = 0;
            all_have_simple_guards = 0;
        }
    }

    if (all_have_simple_guards && all_same_input && common_input) {
        int all_distinct = 1;
        for (size_t i = 0; i < driver_count && all_distinct; ++i) {
            if (!drivers[i].can_produce_non_z) continue;
            for (size_t j = i + 1; j < driver_count && all_distinct; ++j) {
                if (!drivers[j].can_produce_non_z) continue;
                if (drivers[i].enable.compare_value && drivers[j].enable.compare_value &&
                    strcmp(drivers[i].enable.compare_value, drivers[j].enable.compare_value) == 0 &&
                    drivers[i].enable.is_inverted == drivers[j].enable.is_inverted) {
                    all_distinct = 0;
                    info->conflict.driver_a = i;
                    info->conflict.driver_b = j;
                    info->conflict.reason = "Same enable condition";
                }
            }
        }

        if (all_distinct) {
            info->result = JZ_TRISTATE_PROVEN;
            info->proof_method = JZ_TRISTATE_PROOF_DISTINCT_CONSTANTS;
            return;
        }
    }

    /* Check for complementary guards (exactly 2 non-z drivers). */
    if (non_z_count == 2 && all_have_simple_guards) {
        size_t idx_a = (size_t)-1, idx_b = (size_t)-1;
        for (size_t i = 0; i < driver_count; ++i) {
            if (!drivers[i].can_produce_non_z) continue;
            if (idx_a == (size_t)-1) idx_a = i;
            else idx_b = i;
        }

        if (idx_a != (size_t)-1 && idx_b != (size_t)-1) {
            JZTristateDriver *da = &drivers[idx_a];
            JZTristateDriver *db = &drivers[idx_b];

            if (da->enable.input_name && db->enable.input_name &&
                strcmp(da->enable.input_name, db->enable.input_name) == 0 &&
                da->enable.compare_value && db->enable.compare_value &&
                strcmp(da->enable.compare_value, db->enable.compare_value) == 0 &&
                da->enable.is_inverted != db->enable.is_inverted) {
                info->result = JZ_TRISTATE_PROVEN;
                info->proof_method = JZ_TRISTATE_PROOF_COMPLEMENTARY_GUARDS;
                return;
            }
        }
    }

    /* Check for complementary IF/ELSE branches.
     * Guard terms are conjunctive.  If any term in one driver has the same
     * expression but opposite negation as any term in another driver, the
     * pair cannot fire simultaneously. */
    {
        int all_pairs_proven = 1;
        int any_pair_checked = 0;

        for (size_t i = 0; i < driver_count && all_pairs_proven; ++i) {
            if (!drivers[i].can_produce_non_z) continue;
            for (size_t j = i + 1; j < driver_count && all_pairs_proven; ++j) {
                if (!drivers[j].can_produce_non_z) continue;
                any_pair_checked = 1;

                JZTristateDriver *da = &drivers[i];
                JZTristateDriver *db = &drivers[j];

                if (da->enable.n_guard_terms == 0 ||
                    db->enable.n_guard_terms == 0) {
                    all_pairs_proven = 0;
                    break;
                }

                int found_contradiction = 0;
                for (size_t ai = 0; ai < da->enable.n_guard_terms && !found_contradiction; ++ai) {
                    for (size_t bi = 0; bi < db->enable.n_guard_terms; ++bi) {
                        if (da->enable.guard_terms[ai].expr ==
                                db->enable.guard_terms[bi].expr &&
                            da->enable.guard_terms[ai].neg !=
                                db->enable.guard_terms[bi].neg) {
                            found_contradiction = 1;
                            break;
                        }
                    }
                }

                if (!found_contradiction) {
                    all_pairs_proven = 0;
                }
            }
        }

        if (any_pair_checked && all_pairs_proven) {
            info->result = JZ_TRISTATE_PROVEN;
            info->proof_method = JZ_TRISTATE_PROOF_IF_ELSE_BRANCHES;
            return;
        }
    }

    /* Pairwise mutual-exclusion proof. */
    if (all_have_guards) {
        int all_pairs_ok = 1;
        size_t fail_a = 0, fail_b = 0;

        for (size_t i = 0; i < driver_count && all_pairs_ok; ++i) {
            if (!drivers[i].can_produce_non_z) continue;
            for (size_t j = i + 1; j < driver_count && all_pairs_ok; ++j) {
                if (!drivers[j].can_produce_non_z) continue;
                if (!tristate_prove_pair(&drivers[i], &drivers[j])) {
                    all_pairs_ok = 0;
                    fail_a = i;
                    fail_b = j;
                }
            }
        }

        if (all_pairs_ok) {
            info->result = JZ_TRISTATE_PROVEN;
            info->proof_method = JZ_TRISTATE_PROOF_PAIRWISE;
            return;
        }

        info->conflict.driver_a = fail_a;
        info->conflict.driver_b = fail_b;
        if (!info->conflict.reason) {
            info->conflict.reason = "Guards are not mutually exclusive";
        }
    }

    if (!all_have_guards) {
        info->result = JZ_TRISTATE_UNKNOWN;
        info->unknown_reason = JZ_TRISTATE_UNKNOWN_NO_GUARD;
        return;
    }

    if (any_has_complex_guard) {
        info->result = JZ_TRISTATE_UNKNOWN;
        info->unknown_reason = JZ_TRISTATE_UNKNOWN_COMPLEX_EXPR;
        return;
    }

    info->result = JZ_TRISTATE_DISPROVEN;
    if (info->conflict.reason == NULL) {
        info->conflict.reason = "Guards are not mutually exclusive";
    }
}

/* -------------------------------------------------------------------------
 *  Linter check: sem_tristate_check_net
 * -------------------------------------------------------------------------
 *  Returns 1 if net is safe (no conflict), 0 if conflict detected.
 */
int sem_tristate_check_net(const JZNet *net,
                           const char *net_name,
                           const JZModuleScope *scope,
                           const JZBuffer *module_scopes)
{
    if (!net || !net_name) return 1;

    JZTristateNetInfo info;
    jz_tristate_analyze_net(&info, net, net_name, NULL, scope, module_scopes);

    /* For the linter, both DISPROVEN and UNKNOWN are treated as conflicts.
     * UNKNOWN means we couldn't prove mutual exclusion — the conservative
     * choice is to report the diagnostic so users investigate.
     * Only PROVEN is considered safe.
     */
    int safe = (info.result == JZ_TRISTATE_PROVEN);

    jz_buf_free(&info.drivers);
    jz_buf_free(&info.sinks);

    return safe;
}
