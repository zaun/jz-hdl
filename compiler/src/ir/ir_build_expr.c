/**
 * @file ir_build_expr.c
 * @brief Expression lowering and literal decoding for IR construction.
 *
 * This file contains helpers for lowering AST expressions into IR_Expr
 * trees, resolving global constants, serializing feature-guard expressions,
 * and decoding sized literals.
 */

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>

#include "ir_internal.h"
#include "../sem/driver_internal.h"

static int ir_eval_lit_call_in_project(const char *expr,
                                       const JZBuffer *project_symbols,
                                       int *out_width,
                                       uint64_t *out_value)
{
    if (!expr || !out_width || !out_value) return -1;

    const char *p = expr;
    while (*p && isspace((unsigned char)*p)) {
        ++p;
    }
    if (strncmp(p, "lit", 3) != 0) {
        return -1;
    }
    p += 3;
    while (*p && isspace((unsigned char)*p)) {
        ++p;
    }
    if (*p != '(') {
        return -1;
    }
    ++p;

    int depth = 0;
    const char *arg_start = p;
    const char *comma = NULL;
    const char *end = NULL;
    for (; *p; ++p) {
        if (*p == '(') {
            depth++;
        } else if (*p == ')') {
            if (depth == 0) {
                end = p;
                break;
            }
            depth--;
        } else if (*p == ',' && depth == 0 && !comma) {
            comma = p;
        }
    }
    if (!end || !comma) {
        return -1;
    }

    const char *tail = end + 1;
    while (*tail && isspace((unsigned char)*tail)) {
        ++tail;
    }
    if (*tail != '\0') {
        return -1;
    }

    const char *w_start = arg_start;
    const char *w_end = comma;
    const char *v_start = comma + 1;
    const char *v_end = end;

    while (w_start < w_end && isspace((unsigned char)*w_start)) w_start++;
    while (w_end > w_start && isspace((unsigned char)w_end[-1])) w_end--;
    while (v_start < v_end && isspace((unsigned char)*v_start)) v_start++;
    while (v_end > v_start && isspace((unsigned char)v_end[-1])) v_end--;

    if (w_start >= w_end || v_start >= v_end) {
        return -1;
    }

    size_t w_len = (size_t)(w_end - w_start);
    size_t v_len = (size_t)(v_end - v_start);
    char *w_expr = (char *)malloc(w_len + 1);
    char *v_expr = (char *)malloc(v_len + 1);
    if (!w_expr || !v_expr) {
        free(w_expr);
        free(v_expr);
        return -1;
    }
    memcpy(w_expr, w_start, w_len);
    w_expr[w_len] = '\0';
    memcpy(v_expr, v_start, v_len);
    v_expr[v_len] = '\0';

    if (strstr(w_expr, "widthof") || strstr(v_expr, "widthof")) {
        free(w_expr);
        free(v_expr);
        return -1;
    }

    long long w_val = 0;
    long long v_val = 0;
    if (sem_eval_const_expr_in_project(w_expr, project_symbols, &w_val) != 0 || w_val <= 0) {
        free(w_expr);
        free(v_expr);
        return -1;
    }
    if (sem_eval_const_expr_in_project(v_expr, project_symbols, &v_val) != 0 || v_val < 0) {
        free(w_expr);
        free(v_expr);
        return -1;
    }
    if (w_val < 64) {
        unsigned long long limit = 1ULL << (unsigned)w_val;
        if ((unsigned long long)v_val >= limit) {
            free(w_expr);
            free(v_expr);
            return -1;
        }
    }

    *out_width = (w_val > INT_MAX) ? -1 : (int)w_val;
    *out_value = (uint64_t)v_val;
    free(w_expr);
    free(v_expr);
    return (*out_width > 0) ? 0 : -1;
}

/**
 * @brief Resolve a qualified global constant reference.
 *
 * Resolves identifiers of the form "GLOBAL.CONST" against @global namespaces
 * in the project symbol table and decodes the CONST value into an IR_Literal.
 *
 * @param qname            Qualified identifier string.
 * @param mod_scope        Module scope (for width evaluation).
 * @param project_symbols Project-level symbol table.
 * @param out              Output literal.
 * @return 0 on success, non-zero on failure.
 */
int ir_eval_global_const_qualified(const char *qname,
                                   const JZModuleScope *mod_scope,
                                   const JZBuffer *project_symbols,
                                   IR_Literal *out)
{
    if (!qname || !mod_scope || !project_symbols || !project_symbols->data || !out) {
        return -1;
    }

    const char *dot = strchr(qname, '.');
    if (!dot || dot == qname || dot[1] == '\0') {
        return -1;
    }

    char ns[128];
    char cname[128];
    size_t ns_len = (size_t)(dot - qname);
    if (ns_len == 0 || ns_len >= sizeof(ns)) {
        return -1;
    }
    memcpy(ns, qname, ns_len);
    ns[ns_len] = '\0';

    const char *cstart = dot + 1;
    size_t c_len = strlen(cstart);
    if (c_len == 0 || c_len >= sizeof(cname)) {
        return -1;
    }
    memcpy(cname, cstart, c_len + 1);

    const JZSymbol *syms = (const JZSymbol *)project_symbols->data;
    size_t count = project_symbols->len / sizeof(JZSymbol);
    const JZASTNode *glob = NULL;
    for (size_t i = 0; i < count; ++i) {
        const JZSymbol *s = &syms[i];
        if (!s->name || s->kind != JZ_SYM_GLOBAL) {
            continue;
        }
        if (strcmp(s->name, ns) == 0 && s->node && s->node->type == JZ_AST_GLOBAL_BLOCK) {
            glob = s->node;
            break;
        }
    }
    if (!glob) {
        return -1;
    }

    for (size_t j = 0; j < glob->child_count; ++j) {
        JZASTNode *decl = glob->children[j];
        if (!decl || decl->type != JZ_AST_CONST_DECL || !decl->name || !decl->text) {
            continue;
        }
        if (strcmp(decl->name, cname) != 0) {
            continue;
        }

        IR_Literal lit;
        if (ir_decode_sized_literal(decl->text, mod_scope, project_symbols, &lit) != 0) {
            int w = 0;
            uint64_t v = 0;
            if (ir_eval_lit_call_in_project(decl->text, project_symbols, &w, &v) != 0) {
                return -1;
            }
            lit.width = w;
            memset(lit.words, 0, sizeof(lit.words));
            lit.words[0] = v;
        }
        *out = lit;
        return 0;
    }

    return -1;
}

/**
 * @brief Map an AST binary operator kind to an IR expression kind.
 *
 * @param kind AST block_kind string.
 * @param out  Output IR expression kind.
 * @return 0 on success, non-zero on failure.
 */
static int ir_map_binary_kind(const char *kind, IR_ExprKind *out)
{
    if (!kind || !out) return -1;

    if (strcmp(kind, "ADD") == 0)       { *out = EXPR_BINARY_ADD; return 0; }
    if (strcmp(kind, "SUB") == 0)       { *out = EXPR_BINARY_SUB; return 0; }
    if (strcmp(kind, "MUL") == 0)       { *out = EXPR_BINARY_MUL; return 0; }
    if (strcmp(kind, "DIV") == 0)       { *out = EXPR_BINARY_DIV; return 0; }
    if (strcmp(kind, "MOD") == 0)       { *out = EXPR_BINARY_MOD; return 0; }

    if (strcmp(kind, "BIT_AND") == 0)   { *out = EXPR_BINARY_AND; return 0; }
    if (strcmp(kind, "BIT_OR") == 0)    { *out = EXPR_BINARY_OR; return 0; }
    if (strcmp(kind, "BIT_XOR") == 0)   { *out = EXPR_BINARY_XOR; return 0; }

    if (strcmp(kind, "SHL") == 0)       { *out = EXPR_BINARY_SHL; return 0; }
    if (strcmp(kind, "SHR") == 0)       { *out = EXPR_BINARY_SHR; return 0; }
    if (strcmp(kind, "ASHR") == 0)      { *out = EXPR_BINARY_ASHR; return 0; }

    if (strcmp(kind, "EQ") == 0)        { *out = EXPR_BINARY_EQ; return 0; }
    if (strcmp(kind, "NEQ") == 0)       { *out = EXPR_BINARY_NEQ; return 0; }
    if (strcmp(kind, "LT") == 0)        { *out = EXPR_BINARY_LT; return 0; }
    if (strcmp(kind, "LE") == 0)        { *out = EXPR_BINARY_LTE; return 0; }
    if (strcmp(kind, "GT") == 0)        { *out = EXPR_BINARY_GT; return 0; }
    if (strcmp(kind, "GE") == 0)        { *out = EXPR_BINARY_GTE; return 0; }

    if (strcmp(kind, "LOG_AND") == 0)   { *out = EXPR_LOGICAL_AND; return 0; }
    if (strcmp(kind, "LOG_OR") == 0)    { *out = EXPR_LOGICAL_OR; return 0; }

    return -1;
}

/**
 * @brief Map an AST unary operator kind to an IR expression kind.
 *
 * @param kind AST block_kind string.
 * @param out  Output IR expression kind.
 * @return 0 on success, non-zero on failure.
 */
static int ir_map_unary_kind(const char *kind, IR_ExprKind *out)
{
    if (!kind || !out) return -1;
    if (strcmp(kind, "BIT_NOT") == 0) {
        *out = EXPR_UNARY_NOT;
        return 0;
    }
    if (strcmp(kind, "LOG_NOT") == 0) {
        *out = EXPR_LOGICAL_NOT;
        return 0;
    }
    if (strcmp(kind, "NEG") == 0) {
        *out = EXPR_UNARY_NEG;
        return 0;
    }
    /* POS is treated as a no-op; caller can just return operand directly. */
    return -1;
}

/**
 * @brief Recursively serialize a restricted AST expression into a const-expr string.
 *
 * This is used exclusively for @feature guard evaluation and supports only
 * a limited subset of expression forms.
 *
 * @param expr AST expression node.
 * @param buf  Output buffer.
 * @return 0 on success, non-zero on failure.
 */
static int ir_expr_to_const_expr_string_rec(const JZASTNode *expr, JZBuffer *buf)
{
    if (!expr || !buf) return -1;

    switch (expr->type) {
    case JZ_AST_EXPR_LITERAL:
        if (!expr->text) return -1;
        return jz_buf_append(buf, expr->text, strlen(expr->text)) == 0 ? 0 : -1;

    case JZ_AST_EXPR_IDENTIFIER:
    case JZ_AST_EXPR_QUALIFIED_IDENTIFIER:
        if (!expr->name) return -1;
        return jz_buf_append(buf, expr->name, strlen(expr->name)) == 0 ? 0 : -1;

    case JZ_AST_EXPR_UNARY: {
        const char *op = NULL;
        if (!expr->block_kind) return -1;
        if (strcmp(expr->block_kind, "BIT_NOT") == 0) op = "~";
        else if (strcmp(expr->block_kind, "LOG_NOT") == 0) op = "!";
        else if (strcmp(expr->block_kind, "POS") == 0) op = "+";
        else if (strcmp(expr->block_kind, "NEG") == 0) op = "-";
        if (!op || expr->child_count < 1 || !expr->children[0]) return -1;
        if (jz_buf_append(buf, "(", 1) != 0) return -1;
        if (jz_buf_append(buf, op, strlen(op)) != 0) return -1;
        if (ir_expr_to_const_expr_string_rec(expr->children[0], buf) != 0) return -1;
        return jz_buf_append(buf, ")", 1) == 0 ? 0 : -1;
    }

    case JZ_AST_EXPR_BINARY: {
        if (!expr->block_kind || expr->child_count < 2 ||
            !expr->children[0] || !expr->children[1]) {
            return -1;
        }
        const char *op = NULL;
        if      (strcmp(expr->block_kind, "MUL") == 0)      op = "*";
        else if (strcmp(expr->block_kind, "DIV") == 0)      op = "/";
        else if (strcmp(expr->block_kind, "MOD") == 0)      op = "%";
        else if (strcmp(expr->block_kind, "ADD") == 0)      op = "+";
        else if (strcmp(expr->block_kind, "SUB") == 0)      op = "-";
        else if (strcmp(expr->block_kind, "SHL") == 0)      op = "<<";
        else if (strcmp(expr->block_kind, "SHR") == 0)      op = ">>";
        else if (strcmp(expr->block_kind, "ASHR") == 0)     op = ">>>";
        else if (strcmp(expr->block_kind, "LT") == 0)       op = "<";
        else if (strcmp(expr->block_kind, "LE") == 0)       op = "<=";
        else if (strcmp(expr->block_kind, "GT") == 0)       op = ">";
        else if (strcmp(expr->block_kind, "GE") == 0)       op = ">=";
        else if (strcmp(expr->block_kind, "EQ") == 0)       op = "==";
        else if (strcmp(expr->block_kind, "NEQ") == 0)      op = "!=";
        else if (strcmp(expr->block_kind, "BIT_AND") == 0)  op = "&";
        else if (strcmp(expr->block_kind, "BIT_OR") == 0)   op = "|";
        else if (strcmp(expr->block_kind, "BIT_XOR") == 0)  op = "^";
        else if (strcmp(expr->block_kind, "LOG_AND") == 0)  op = "&&";
        else if (strcmp(expr->block_kind, "LOG_OR") == 0)   op = "||";
        if (!op) return -1;

        if (jz_buf_append(buf, "(", 1) != 0) return -1;
        if (ir_expr_to_const_expr_string_rec(expr->children[0], buf) != 0) return -1;
        if (jz_buf_append(buf, op, strlen(op)) != 0) return -1;
        if (ir_expr_to_const_expr_string_rec(expr->children[1], buf) != 0) return -1;
        return jz_buf_append(buf, ")", 1) == 0 ? 0 : -1;
    }

    default:
        /* Concatenation, slices, ternary, builtin calls, etc. are not supported
         * in feature-guard conditions for IR evaluation.
         */
        return -1;
    }
}

static int ir_lit_expr_to_const_string_rec(const JZASTNode *expr,
                                           const JZModuleScope *mod_scope,
                                           const JZBuffer *project_symbols,
                                           JZBuffer *buf)
{
    if (!expr || !buf) return -1;

    switch (expr->type) {
    case JZ_AST_EXPR_LITERAL: {
        if (!expr->text) return -1;
        unsigned tmp = 0;
        if (!parse_simple_nonnegative_int(expr->text, &tmp)) {
            return -1;
        }
        return jz_buf_append(buf, expr->text, strlen(expr->text)) == 0 ? 0 : -1;
    }

    case JZ_AST_EXPR_IDENTIFIER: {
        if (!expr->name || !mod_scope) return -1;
        const JZSymbol *sym = module_scope_lookup(mod_scope, expr->name);
        if (!sym || sym->kind != JZ_SYM_CONST) {
            return -1;
        }
        return jz_buf_append(buf, expr->name, strlen(expr->name)) == 0 ? 0 : -1;
    }

    case JZ_AST_EXPR_QUALIFIED_IDENTIFIER: {
        if (!expr->name || !project_symbols || !project_symbols->data) return -1;
        const char *full = expr->name;
        const char *dot = strchr(full, '.');
        if (!dot || dot == full || !*(dot + 1)) return -1;
        size_t head_len = (size_t)(dot - full);
        char head[32];
        if (head_len >= sizeof(head)) head_len = sizeof(head) - 1u;
        memcpy(head, full, head_len);
        head[head_len] = '\0';
        const char *tail = dot + 1;
        if (strcmp(head, "CONFIG") != 0) {
            return -1;
        }

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
        if (!found) return -1;
        return jz_buf_append(buf, tail, strlen(tail)) == 0 ? 0 : -1;
    }

    case JZ_AST_EXPR_UNARY: {
        const char *op = NULL;
        if (!expr->block_kind || expr->child_count < 1 || !expr->children[0]) return -1;
        if (strcmp(expr->block_kind, "BIT_NOT") == 0) op = "~";
        else if (strcmp(expr->block_kind, "LOG_NOT") == 0) op = "!";
        else if (strcmp(expr->block_kind, "POS") == 0) op = "+";
        else if (strcmp(expr->block_kind, "NEG") == 0) op = "-";
        if (!op) return -1;
        if (jz_buf_append(buf, "(", 1) != 0) return -1;
        if (jz_buf_append(buf, op, strlen(op)) != 0) return -1;
        if (ir_lit_expr_to_const_string_rec(expr->children[0],
                                            mod_scope,
                                            project_symbols,
                                            buf) != 0) {
            return -1;
        }
        return jz_buf_append(buf, ")", 1) == 0 ? 0 : -1;
    }

    case JZ_AST_EXPR_BINARY: {
        if (!expr->block_kind || expr->child_count < 2 ||
            !expr->children[0] || !expr->children[1]) {
            return -1;
        }
        const char *op = NULL;
        if      (strcmp(expr->block_kind, "MUL") == 0)      op = "*";
        else if (strcmp(expr->block_kind, "DIV") == 0)      op = "/";
        else if (strcmp(expr->block_kind, "MOD") == 0)      op = "%";
        else if (strcmp(expr->block_kind, "ADD") == 0)      op = "+";
        else if (strcmp(expr->block_kind, "SUB") == 0)      op = "-";
        else if (strcmp(expr->block_kind, "SHL") == 0)      op = "<<";
        else if (strcmp(expr->block_kind, "SHR") == 0)      op = ">>";
        else if (strcmp(expr->block_kind, "ASHR") == 0)     op = ">>>";
        else if (strcmp(expr->block_kind, "LT") == 0)       op = "<";
        else if (strcmp(expr->block_kind, "LE") == 0)       op = "<=";
        else if (strcmp(expr->block_kind, "GT") == 0)       op = ">";
        else if (strcmp(expr->block_kind, "GE") == 0)       op = ">=";
        else if (strcmp(expr->block_kind, "EQ") == 0)       op = "==";
        else if (strcmp(expr->block_kind, "NEQ") == 0)      op = "!=";
        else if (strcmp(expr->block_kind, "BIT_AND") == 0)  op = "&";
        else if (strcmp(expr->block_kind, "BIT_OR") == 0)   op = "|";
        else if (strcmp(expr->block_kind, "BIT_XOR") == 0)  op = "^";
        else if (strcmp(expr->block_kind, "LOG_AND") == 0)  op = "&&";
        else if (strcmp(expr->block_kind, "LOG_OR") == 0)   op = "||";
        if (!op) return -1;

        if (jz_buf_append(buf, "(", 1) != 0) return -1;
        if (ir_lit_expr_to_const_string_rec(expr->children[0],
                                            mod_scope,
                                            project_symbols,
                                            buf) != 0) {
            return -1;
        }
        if (jz_buf_append(buf, op, strlen(op)) != 0) return -1;
        if (ir_lit_expr_to_const_string_rec(expr->children[1],
                                            mod_scope,
                                            project_symbols,
                                            buf) != 0) {
            return -1;
        }
        return jz_buf_append(buf, ")", 1) == 0 ? 0 : -1;
    }

    case JZ_AST_EXPR_BUILTIN_CALL: {
        if (!expr->name) return -1;
        const char *fname = expr->name;
        if (strcmp(fname, "clog2") != 0 && strcmp(fname, "widthof") != 0) {
            return -1;
        }
        if (expr->child_count == 0) return -1;
        if (jz_buf_append(buf, fname, strlen(fname)) != 0) return -1;
        if (jz_buf_append(buf, "(", 1) != 0) return -1;
        for (size_t i = 0; i < expr->child_count; ++i) {
            if (!expr->children[i]) return -1;
            if (ir_lit_expr_to_const_string_rec(expr->children[i],
                                                mod_scope,
                                                project_symbols,
                                                buf) != 0) {
                return -1;
            }
            if (i + 1 < expr->child_count) {
                if (jz_buf_append(buf, ",", 1) != 0) return -1;
            }
        }
        return jz_buf_append(buf, ")", 1) == 0 ? 0 : -1;
    }

    case JZ_AST_EXPR_CONCAT:
    case JZ_AST_EXPR_SLICE:
    case JZ_AST_EXPR_TERNARY:
    default:
        return -1;
    }
}

static int ir_lit_expr_to_const_string(const JZASTNode *expr,
                                       const JZModuleScope *mod_scope,
                                       const JZBuffer *project_symbols,
                                       char **out_str)
{
    if (!expr || !out_str) return -1;
    *out_str = NULL;

    JZBuffer buf = {0};
    if (ir_lit_expr_to_const_string_rec(expr, mod_scope, project_symbols, &buf) != 0) {
        jz_buf_free(&buf);
        return -1;
    }
    const char nul = '\0';
    if (jz_buf_append(&buf, &nul, 1) != 0) {
        jz_buf_free(&buf);
        return -1;
    }
    *out_str = (char *)buf.data;
    buf.data = NULL;
    buf.len = buf.cap = 0;
    return 0;
}

static int ir_eval_lit_const_expr(const JZASTNode *expr,
                                  const JZModuleScope *mod_scope,
                                  const JZBuffer *project_symbols,
                                  long long *out_value)
{
    if (!expr || !out_value) return -1;

    char *expr_text = NULL;
    if (ir_lit_expr_to_const_string(expr, mod_scope, project_symbols, &expr_text) != 0 || !expr_text) {
        if (expr_text) free(expr_text);
        return -1;
    }

    char *expanded = NULL;
    if (sem_expand_widthof_in_width_expr(expr_text,
                                         mod_scope,
                                         project_symbols,
                                         &expanded,
                                         0) != 0) {
        free(expr_text);
        if (expanded) free(expanded);
        return -1;
    }

    const char *to_eval = expanded ? expanded : expr_text;
    int rc = sem_eval_const_expr_in_module(to_eval,
                                           mod_scope,
                                           project_symbols,
                                           out_value);
    free(expr_text);
    if (expanded) free(expanded);
    return rc;
}


/**
 * Fast recursive evaluator for pure-literal expression trees.
 * Handles the common template-expansion case where IDX is substituted into
 * arithmetic (e.g., 0*11+10) producing EXPR_BINARY trees of EXPR_LITERAL
 * leaves.  Avoids the expensive sem_eval_const_expr_in_module path.
 * Returns 1 on success with *out set, 0 if any node is not a literal/binary.
 */
static int try_fast_const_eval(const JZASTNode *node, long long *out)
{
    if (!node || !out) return 0;

    if (node->type == JZ_AST_EXPR_LITERAL && node->text) {
        char *end = NULL;
        long long v = strtoll(node->text, &end, 0);
        if (end && end != node->text && *end == '\0') {
            *out = v;
            return 1;
        }
        return 0;
    }

    if (node->type == JZ_AST_EXPR_BINARY && node->block_kind &&
        node->child_count >= 2) {
        long long lv, rv;
        if (!try_fast_const_eval(node->children[0], &lv)) return 0;
        if (!try_fast_const_eval(node->children[1], &rv)) return 0;

        if (strcmp(node->block_kind, "ADD") == 0) { *out = lv + rv; return 1; }
        if (strcmp(node->block_kind, "SUB") == 0) { *out = lv - rv; return 1; }
        if (strcmp(node->block_kind, "MUL") == 0) { *out = lv * rv; return 1; }
        if (strcmp(node->block_kind, "DIV") == 0) {
            if (rv == 0) return 0;
            *out = lv / rv; return 1;
        }
        if (strcmp(node->block_kind, "MOD") == 0) {
            if (rv == 0) return 0;
            *out = lv % rv; return 1;
        }
    }

    return 0;
}

/**
 * Try to evaluate a slice bound (MSB or LSB) from an AST node.
 * Fast path 1: simple literal.
 * Fast path 2: pure-literal expression tree (template-expanded IDX arithmetic).
 * Slow path: full const expression evaluation via sem_eval_const_expr_in_module.
 * Returns 1 on success with *out set, 0 on failure.
 */
int ir_eval_slice_bound(const JZASTNode *node,
                        const JZModuleScope *mod_scope,
                        const JZBuffer *project_symbols,
                        unsigned *out)
{
    if (!node || !out) return 0;
    /* Fast path 1: simple literal */
    if (node->type == JZ_AST_EXPR_LITERAL && node->text) {
        return parse_simple_nonnegative_int(node->text, out) ? 1 : 0;
    }
    /* Fast path 2: pure-literal binary expression tree */
    long long fast_val = 0;
    if (try_fast_const_eval(node, &fast_val) &&
        fast_val >= 0 && (unsigned long long)fast_val <= UINT_MAX) {
        *out = (unsigned)fast_val;
        return 1;
    }
    /* Slow path: evaluate as const expression */
    long long val = 0;
    if (ir_eval_lit_const_expr(node, mod_scope, project_symbols, &val) == 0 &&
        val >= 0 && (unsigned long long)val <= UINT_MAX) {
        *out = (unsigned)val;
        return 1;
    }
    return 0;
}

/**
 * @brief Build an IR expression tree from an AST expression node.
 *
 * Supports literals, identifiers, unary/binary operators, ternary
 * expressions, concatenation, slices, memory reads, MUX selectors,
 * intrinsic calls, and BUS member access.
 *
 * @param arena           Arena for IR allocation.
 * @param expr            AST expression node.
 * @param mod_scope       Module scope.
 * @param project_symbols Project-level symbols.
 * @param bus_map         BUS signal mapping array (may be NULL).
 * @param bus_map_count   Number of bus mapping entries.
 * @param diagnostics     Diagnostic sink.
 * @return Pointer to IR_Expr, or NULL if lowering fails.
 */
IR_Expr *ir_build_expr(JZArena *arena,
                              JZASTNode *expr,
                              const JZModuleScope *mod_scope,
                              const JZBuffer *project_symbols,
                              const IR_BusSignalMapping *bus_map,
                              int bus_map_count,
                              JZDiagnosticList *diagnostics)
{
    if (!arena || !expr || !mod_scope) return NULL;

    /* Infer result width using the existing semantic helper. */
    JZBitvecType t;
    t.width = 0;
    t.is_signed = 0;
    infer_expr_type(expr, mod_scope, project_symbols, diagnostics, &t);

    switch (expr->type) {
    case JZ_AST_EXPR_LITERAL: {
        IR_Expr *ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
        if (!ir) return NULL;
        memset(ir, 0, sizeof(*ir));
        ir->kind = EXPR_LITERAL;
        ir->width = (int)t.width;
        ir->source_line = expr->loc.line;
        memset(ir->u.literal.literal.words, 0, sizeof(ir->u.literal.literal.words));
        ir->u.literal.literal.width = ir->width;
        if (expr->text) {
            /* Prefer sized literal decoding when possible; fall back to
             * width-only if the lexeme is not sized or cannot be parsed.
             */
            IR_Literal lit;
            if (ir_decode_sized_literal(expr->text,
                                        mod_scope,
                                        project_symbols,
                                        &lit) == 0) {
                /* Use the decoded width when available; semantic width
                 * inference has already validated consistency.
                 */
                ir->u.literal.literal = lit;
                if (t.width == 0) {
                    ir->width = lit.width;
                }
            }
        }
        return ir;
    }

    case JZ_AST_EXPR_SPECIAL_DRIVER: {
        /* GND/VCC are polymorphic constants. Create a literal with const_name
         * set to "GND" or "VCC" so the backend can emit width-replicated output.
         * Width is 0 (polymorphic) and will be resolved from assignment context.
         */
        IR_Expr *ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
        if (!ir) return NULL;
        memset(ir, 0, sizeof(*ir));
        ir->kind = EXPR_LITERAL;
        ir->width = 0; /* Polymorphic - resolved from assignment context */
        ir->source_line = expr->loc.line;

        const char *driver = (expr->block_kind && strcmp(expr->block_kind, "VCC") == 0)
                           ? "VCC" : "GND";
        ir->const_name = ir_strdup_arena(arena, driver);

        /* Set value: 0 for GND, all-1s for VCC (placeholder until width known) */
        memset(ir->u.literal.literal.words, 0, sizeof(ir->u.literal.literal.words));
        ir->u.literal.literal.words[0] = (strcmp(driver, "VCC") == 0) ? ~0ULL : 0ULL;
        ir->u.literal.literal.width = 0;
        return ir;
    }

    case JZ_AST_EXPR_IDENTIFIER:
    case JZ_AST_EXPR_QUALIFIED_IDENTIFIER: {
        if (!expr->name) return NULL;

        const JZSymbol *const_sym = NULL;
        if (expr->type == JZ_AST_EXPR_IDENTIFIER) {
            const_sym = module_scope_lookup_kind(mod_scope, expr->name, JZ_SYM_CONST);
        }

        /* MEM port data: treat mem.port.data as a memory read expression and
         * defer address resolution to the backend via port bindings.
         */
        if (expr->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER) {
            JZMemPortRef mem_ref;
            memset(&mem_ref, 0, sizeof(mem_ref));
            if (sem_match_mem_port_qualified_ident(expr, mod_scope, NULL, &mem_ref) &&
                mem_ref.field == MEM_PORT_FIELD_DATA &&
                mem_ref.mem_decl && mem_ref.mem_decl->name &&
                mem_ref.port && mem_ref.port->name) {
                int mem_w = (int)t.width;
                if (mem_w <= 0 && mem_ref.mem_decl->width) {
                    unsigned eval_w = 0;
                    if (sem_eval_width_expr(mem_ref.mem_decl->width,
                                            mod_scope, project_symbols, &eval_w) &&
                        eval_w > 0) {
                        mem_w = (int)eval_w;
                    }
                }
                IR_Expr *ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
                if (!ir) return NULL;
                memset(ir, 0, sizeof(*ir));
                ir->kind = EXPR_MEM_READ;
                ir->width = mem_w;
                ir->source_line = expr->loc.line;
                ir->u.mem_read.memory_name = ir_strdup_arena(arena, mem_ref.mem_decl->name);
                ir->u.mem_read.port_name = ir_strdup_arena(arena, mem_ref.port->name);
                ir->u.mem_read.address = NULL;
                return ir;
            }
        }

        /* BUS member access: for qualified identifiers of the form "port.signal"
         * where port is a BUS port, resolve to the expanded IR signal.
         */
        if (expr->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER && bus_map && bus_map_count > 0) {
            const char *full = expr->name;
            const char *dot = strchr(full, '.');
            if (dot && dot != full && dot[1] != '\0') {
                size_t port_len = (size_t)(dot - full);
                char port_name[256];
                if (port_len < sizeof(port_name)) {
                    memcpy(port_name, full, port_len);
                    port_name[port_len] = '\0';
                    const char *signal_name = dot + 1;

                    /* Check if this port is a BUS port by looking for it in bus_map. */
                    int signal_id = ir_lookup_bus_signal_id(bus_map, bus_map_count,
                                                            port_name, signal_name, -1);
                    if (signal_id >= 0) {
                        IR_Expr *ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
                        if (!ir) return NULL;
                        memset(ir, 0, sizeof(*ir));
                        ir->kind = EXPR_SIGNAL_REF;
                        ir->width = (int)t.width;
                        ir->source_line = expr->loc.line;
                        ir->u.signal_ref.signal_id = signal_id;
                        return ir;
                    }
                }
            }
        }

        /* First, try to resolve as a signal (PORT/WIRE/REGISTER/LATCH) using the
         * local module scope. Qualified identifiers that name signals are not
         * expected in the current language surface, but treating both IDENTIFIER
         * and QUALIFIED_IDENTIFIER uniformly here is harmless and future-proof.
         */
        const JZSymbol *sym = module_scope_lookup(mod_scope, expr->name);
        if (sym && (sym->kind == JZ_SYM_PORT ||
                    sym->kind == JZ_SYM_WIRE ||
                    sym->kind == JZ_SYM_REGISTER ||
                    sym->kind == JZ_SYM_LATCH)) {
            if (sym->id < 0) return NULL;
            IR_Expr *ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
            if (!ir) return NULL;
            memset(ir, 0, sizeof(*ir));
            ir->kind = EXPR_SIGNAL_REF;
            ir->width = (int)t.width;
            ir->source_line = expr->loc.line;
            ir->u.signal_ref.signal_id = sym->id;
            return ir;
        }

        /* Next, for qualified identifiers of the form GLOBAL.CONST, try to
         * resolve them against @global namespaces in the project and decode
         * their sized-literal values directly.
         */
        if (expr->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER && project_symbols) {
            IR_Literal glit;
            if (ir_eval_global_const_qualified(expr->name,
                                               mod_scope,
                                               project_symbols,
                                               &glit) == 0) {
                IR_Expr *ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
                if (!ir) return NULL;
                memset(ir, 0, sizeof(*ir));
                ir->kind = EXPR_LITERAL;
                ir->source_line = expr->loc.line;
                ir->width = glit.width;
                ir->u.literal.literal = glit;
                ir->const_name = ir_strdup_arena(arena, expr->name);
                return ir;
            }
        }

        /* Otherwise, treat IDENTIFIER/QUALIFIED_IDENTIFIER as a reference to a
         * module CONST or project CONFIG entry and lower it via the generic
         * constant-eval helper.
         */
        long long cval = 0;
        if (sem_eval_const_expr_in_module(expr->name,
                                          mod_scope,
                                          project_symbols,
                                          &cval) == 0) {
            IR_Expr *ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
            if (!ir) return NULL;
            memset(ir, 0, sizeof(*ir));
            ir->kind = EXPR_LITERAL;
            ir->source_line = expr->loc.line;
            /* Use inferred width when available; otherwise choose minimal bits
             * to represent the nonnegative value (0 → width-1).
             */
            unsigned w = (unsigned)t.width;
            if (w == 0) {
                unsigned bits = 0;
                unsigned long long v = (cval < 0) ? (unsigned long long)(-cval) : (unsigned long long)cval;
                if (v == 0) {
                    bits = 1;
                } else {
                    while (v > 0) {
                        v >>= 1;
                        bits++;
                    }
                }
                w = bits ? bits : 1u;
            }
            ir->width = (int)w;
            uint64_t masked = (w >= 64u) ? (uint64_t)cval : ((uint64_t)cval & ((1ULL << w) - 1ULL));
            ir->u.literal.literal.words[0] = masked;
            ir->u.literal.literal.width = (int)w;
            if (const_sym && const_sym->name) {
                ir->const_name = ir_strdup_arena(arena, const_sym->name);
            }
            return ir;
        }

        /* If neither a signal nor a constant, leave unsupported for now. This
         * preserves existing behavior for names like rom.read used as the base
         * of memory-slice expressions; those rely on null-RHS lowering to
         * MEM_PORT_READ_* in the backend rather than explicit EXPR trees.
         */
        return NULL;
    }

    case JZ_AST_EXPR_BUS_ACCESS: {
        /* BUS member access: pbus.DATA or pbus[i].DATA
         * The parser stores:
         *   expr->name = bus port name (e.g., "pbus")
         *   expr->text = signal name within BUS (e.g., "DATA")
         *   expr->children[0] = optional array index expression
         *
         * We need to resolve this to the expanded IR signal ID using the
         * bus_map built during signal expansion.
         */
        if (!bus_map || bus_map_count <= 0) {
            /* No bus mapping available - cannot resolve BUS access. */
            return NULL;
        }

        /* Use semantic resolver to get BUS access info. */
        JZBusAccessInfo info;
        memset(&info, 0, sizeof(info));
        if (sem_resolve_bus_access(expr, mod_scope, project_symbols, &info, NULL) != 1) {
            return NULL;
        }

        /* Look up the expanded signal ID in the bus_map.
         * When the BUS array has only 1 element, the bus_map stores
         * array_index = -1 (non-array convention), so use -1 for lookup. */
        int array_index = info.index_known
            ? (info.count > 1 ? (int)info.index_value : -1)
            : -1;

        /* Dynamic bus array index: build concat+gslice mux. */
        if (!info.index_known && info.is_array && info.count > 0) {
            /* Build index expression from the array index AST child. */
            IR_Expr *idx_expr = NULL;
            if (expr->child_count > 0 && expr->children[0]) {
                idx_expr = ir_build_expr(arena, expr->children[0],
                                         mod_scope, project_symbols,
                                         bus_map, bus_map_count, diagnostics);
            }
            if (!idx_expr) return NULL;

            /* Collect all array element signal IDs and build concat operands.
             * Element 0 at LSB (last operand in concat), element N-1 at MSB
             * (first operand in concat) — same convention as MUX AGGREGATE.
             */
            int count = (int)info.count;
            IR_Expr **operands = (IR_Expr **)jz_arena_alloc(arena,
                                    sizeof(IR_Expr *) * (size_t)count);
            if (!operands) return NULL;
            memset(operands, 0, sizeof(IR_Expr *) * (size_t)count);

            int element_width = 0;
            int total_width = 0;
            for (int i = 0; i < count; ++i) {
                int lookup_idx = (count > 1) ? i : -1;
                int elem_sig_id = ir_lookup_bus_signal_id(bus_map, bus_map_count,
                                                           expr->name, info.signal_name, lookup_idx);
                if (elem_sig_id < 0) return NULL;

                /* Find element width from bus_map. */
                int ew = 0;
                for (int m = 0; m < bus_map_count; ++m) {
                    if (bus_map[m].ir_signal_id == elem_sig_id) {
                        ew = bus_map[m].width;
                        break;
                    }
                }
                if (ew <= 0 && (int)t.width > 0) ew = (int)t.width;
                if (element_width == 0 && ew > 0) element_width = ew;

                IR_Expr *ref = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
                if (!ref) return NULL;
                memset(ref, 0, sizeof(*ref));
                ref->kind = EXPR_SIGNAL_REF;
                ref->width = ew;
                ref->source_line = expr->loc.line;
                ref->u.signal_ref.signal_id = elem_sig_id;

                /* Reverse order: element[count-1] at operands[0] (MSB). */
                operands[count - 1 - i] = ref;
                total_width += ew;
            }

            /* Build concat expression. */
            IR_Expr *concat = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
            if (!concat) return NULL;
            memset(concat, 0, sizeof(*concat));
            concat->kind = EXPR_CONCAT;
            concat->width = total_width;
            concat->source_line = expr->loc.line;
            concat->u.concat.operands = operands;
            concat->u.concat.num_operands = count;

            /* Build gslice expression. */
            IR_Expr *ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
            if (!ir) return NULL;
            memset(ir, 0, sizeof(*ir));
            ir->kind = EXPR_INTRINSIC_GSLICE;
            ir->width = (element_width > 0) ? element_width : (int)t.width;
            ir->source_line = expr->loc.line;
            ir->u.intrinsic.source = concat;
            ir->u.intrinsic.index = idx_expr;
            ir->u.intrinsic.value = NULL;
            ir->u.intrinsic.element_width = (element_width > 0) ? element_width : ir->width;
            return ir;
        }

        int signal_id = ir_lookup_bus_signal_id(bus_map, bus_map_count,
                                                 expr->name, info.signal_name,
                                                 array_index);
        if (signal_id < 0) {
            return NULL;
        }

        /* Create signal reference expression. */
        IR_Expr *ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
        if (!ir) return NULL;
        memset(ir, 0, sizeof(*ir));
        ir->kind = EXPR_SIGNAL_REF;
        ir->width = (int)t.width;
        ir->source_line = expr->loc.line;
        ir->u.signal_ref.signal_id = signal_id;
        return ir;
    }

    case JZ_AST_EXPR_UNARY: {
        if (expr->child_count < 1) return NULL;
        IR_Expr *opnd = ir_build_expr(arena,
                                      expr->children[0],
                                      mod_scope,
                                      project_symbols,
                                      bus_map,
                                      bus_map_count,
                                      diagnostics);
        if (!opnd) return NULL;
        if (!expr->block_kind) {
            return opnd;
        }
        IR_ExprKind k;
        if (ir_map_unary_kind(expr->block_kind, &k) != 0) {
            /* POS or unsupported: just return operand. */
            return opnd;
        }
        IR_Expr *ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
        if (!ir) return NULL;
        memset(ir, 0, sizeof(*ir));
        ir->kind = k;
        ir->width = (int)t.width;
        ir->source_line = expr->loc.line;
        ir->u.unary.operand = opnd;
        return ir;
    }

    case JZ_AST_EXPR_BINARY: {
        if (expr->child_count < 2) return NULL;
        IR_Expr *lhs = ir_build_expr(arena,
                                     expr->children[0],
                                     mod_scope,
                                     project_symbols,
                                     bus_map,
                                     bus_map_count,
                                     diagnostics);
        IR_Expr *rhs = ir_build_expr(arena,
                                     expr->children[1],
                                     mod_scope,
                                     project_symbols,
                                     bus_map,
                                     bus_map_count,
                                     diagnostics);
        if (!lhs || !rhs) return NULL;
        IR_ExprKind k;
        if (!expr->block_kind || ir_map_binary_kind(expr->block_kind, &k) != 0) {
            return NULL;
        }
        IR_Expr *ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
        if (!ir) return NULL;
        memset(ir, 0, sizeof(*ir));
        ir->kind = k;
        ir->width = (int)t.width;
        ir->source_line = expr->loc.line;
        ir->u.binary.left  = lhs;
        ir->u.binary.right = rhs;
        return ir;
    }

    case JZ_AST_EXPR_TERNARY: {
        if (expr->child_count < 3) return NULL;
        IR_Expr *c  = ir_build_expr(arena, expr->children[0], mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
        IR_Expr *t1 = ir_build_expr(arena, expr->children[1], mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
        IR_Expr *t2 = ir_build_expr(arena, expr->children[2], mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
        if (!c || !t1 || !t2) return NULL;
        IR_Expr *ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
        if (!ir) return NULL;
        memset(ir, 0, sizeof(*ir));
        ir->kind = EXPR_TERNARY;
        ir->width = (int)t.width;
        ir->source_line = expr->loc.line;
        ir->u.ternary.condition = c;
        ir->u.ternary.true_val  = t1;
        ir->u.ternary.false_val = t2;
        /* Propagate ternary width to GND/VCC polymorphic branches */
        if (t.width > 0) {
            if (t1->width == 0 && t1->const_name &&
                (strcmp(t1->const_name, "GND") == 0 || strcmp(t1->const_name, "VCC") == 0)) {
                t1->width = (int)t.width;
                t1->u.literal.literal.width = (int)t.width;
            }
            if (t2->width == 0 && t2->const_name &&
                (strcmp(t2->const_name, "GND") == 0 || strcmp(t2->const_name, "VCC") == 0)) {
                t2->width = (int)t.width;
                t2->u.literal.literal.width = (int)t.width;
            }
        }
        return ir;
    }

    case JZ_AST_EXPR_CONCAT: {
        if (expr->child_count == 0) return NULL;
        IR_Expr *ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
        if (!ir) return NULL;
        memset(ir, 0, sizeof(*ir));
        ir->kind = EXPR_CONCAT;
        ir->width = (int)t.width;
        ir->source_line = expr->loc.line;
        ir->u.concat.num_operands = (int)expr->child_count;
        ir->u.concat.operands = (IR_Expr **)jz_arena_alloc(arena, sizeof(IR_Expr *) * expr->child_count);
        if (!ir->u.concat.operands) return NULL;
        for (size_t i = 0; i < expr->child_count; ++i) {
            IR_Expr *elem = ir_build_expr(arena,
                                          expr->children[i],
                                          mod_scope,
                                          project_symbols,
                                          bus_map,
                                          bus_map_count,
                                          diagnostics);
            if (!elem) return NULL;
            ir->u.concat.operands[i] = elem;
        }
        return ir;
    }

    case JZ_AST_EXPR_SLICE: {
        if (expr->child_count < 2) return NULL;
        JZASTNode *base = expr->children[0];
        if (!base) return NULL;

        /* Qualified identifier with slice: could be mem.port[addr] or
         * bus.SIGNAL[msb:lsb]. Check bus_map first to distinguish.
         */
        if (base->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER && base->name) {
            const char *full = base->name;
            const char *dot = strchr(full, '.');
            if (!dot || dot == full || dot[1] == '\0') {
                return NULL;
            }
            size_t prefix_len = (size_t)(dot - full);
            const char *suffix = dot + 1;

            /* Try BUS signal resolution first. */
            if (bus_map && bus_map_count > 0 && prefix_len < 256) {
                char port_name[256];
                memcpy(port_name, full, prefix_len);
                port_name[prefix_len] = '\0';
                int signal_id = ir_lookup_bus_signal_id(bus_map, bus_map_count,
                                                        port_name, suffix, -1);
                if (signal_id >= 0 && expr->child_count >= 3) {
                    /* BUS field bit-slice: bus.SIGNAL[msb:lsb] */
                    JZASTNode *msb = expr->children[1];
                    JZASTNode *lsb = expr->children[2];
                    unsigned msb_val = 0, lsb_val = 0;
                    if (msb && lsb &&
                        ir_eval_slice_bound(msb, mod_scope, project_symbols, &msb_val) &&
                        ir_eval_slice_bound(lsb, mod_scope, project_symbols, &lsb_val)) {
                        IR_Expr *ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
                        if (!ir) return NULL;
                        memset(ir, 0, sizeof(*ir));
                        ir->kind = EXPR_SLICE;
                        ir->width = (int)t.width;
                        ir->source_line = expr->loc.line;
                        ir->u.slice.signal_id = signal_id;
                        ir->u.slice.msb = (int)msb_val;
                        ir->u.slice.lsb = (int)lsb_val;
                        return ir;
                    }
                }
            }

            /* Check if this is mem.port.data[msb:lsb] — a bit-slice on the
             * memory read data output. If so, build as EXPR_SLICE wrapping
             * EXPR_MEM_READ (no address), not as mem.port[addr].
             */
            if (expr->child_count >= 3) {
                JZMemPortRef mem_ref;
                memset(&mem_ref, 0, sizeof(mem_ref));
                if (sem_match_mem_port_qualified_ident(base, mod_scope, NULL, &mem_ref) &&
                    mem_ref.field == MEM_PORT_FIELD_DATA &&
                    mem_ref.mem_decl && mem_ref.mem_decl->name &&
                    mem_ref.port && mem_ref.port->name) {
                    JZASTNode *msb_node = expr->children[1];
                    JZASTNode *lsb_node = expr->children[2];
                    unsigned msb_val = 0, lsb_val = 0;
                    if (msb_node && lsb_node &&
                        ir_eval_slice_bound(msb_node, mod_scope, project_symbols, &msb_val) &&
                        ir_eval_slice_bound(lsb_node, mod_scope, project_symbols, &lsb_val)) {
                        /* Build the EXPR_MEM_READ base (no address). */
                        JZBitvecType base_t;
                        base_t.width = 0;
                        base_t.is_signed = 0;
                        infer_expr_type(base, mod_scope, project_symbols, diagnostics, &base_t);
                        int base_w = (int)base_t.width;
                        if (base_w <= 0 && mem_ref.mem_decl->width) {
                            unsigned eval_w = 0;
                            if (sem_eval_width_expr(mem_ref.mem_decl->width,
                                                    mod_scope, project_symbols, &eval_w) &&
                                eval_w > 0) {
                                base_w = (int)eval_w;
                            }
                        }
                        IR_Expr *mem_ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
                        if (!mem_ir) return NULL;
                        memset(mem_ir, 0, sizeof(*mem_ir));
                        mem_ir->kind = EXPR_MEM_READ;
                        mem_ir->width = base_w;
                        mem_ir->source_line = base->loc.line;
                        mem_ir->u.mem_read.memory_name = ir_strdup_arena(arena, mem_ref.mem_decl->name);
                        mem_ir->u.mem_read.port_name = ir_strdup_arena(arena, mem_ref.port->name);
                        mem_ir->u.mem_read.address = NULL;

                        /* Wrap in EXPR_SLICE. */
                        IR_Expr *ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
                        if (!ir) return NULL;
                        memset(ir, 0, sizeof(*ir));
                        ir->kind = EXPR_SLICE;
                        ir->width = (int)(msb_val - lsb_val + 1);
                        ir->source_line = expr->loc.line;
                        ir->u.slice.signal_id = -1;
                        ir->u.slice.base_expr = mem_ir;
                        ir->u.slice.msb = (int)msb_val;
                        ir->u.slice.lsb = (int)lsb_val;
                        return ir;
                    }
                }
            }

            /* Memory port slice form: mem.port[addr]. The parser represents the
             * qualified identifier "mem.port" as JZ_AST_EXPR_QUALIFIED_IDENTIFIER
             * with name "mem.port". We treat this as a memory read expression and
             * leave resolution of the memory/port names to later passes/backends.
             */
            JZASTNode *addr_node = expr->children[1];
            if (!addr_node) return NULL;
            IR_Expr *addr_ir = ir_build_expr(arena,
                                             addr_node,
                                             mod_scope,
                                             project_symbols,
                                             bus_map,
                                             bus_map_count,
                                             diagnostics);
            if (!addr_ir) return NULL;

            char *mem_name = (char *)jz_arena_alloc(arena, prefix_len + 1);
            if (!mem_name) return NULL;
            memcpy(mem_name, full, prefix_len);
            mem_name[prefix_len] = '\0';

            char *port_nm = ir_strdup_arena(arena, suffix);
            if (!port_nm) return NULL;

            /* If infer_expr_type returned width 0, try to resolve the
             * memory word width from the declaration via const-eval. */
            int mem_width = (int)t.width;
            if (mem_width <= 0) {
                JZMemPortRef mem_ref;
                memset(&mem_ref, 0, sizeof(mem_ref));
                if (sem_match_mem_port_qualified_ident(base, mod_scope, NULL, &mem_ref) &&
                    mem_ref.mem_decl && mem_ref.mem_decl->width) {
                    unsigned eval_w = 0;
                    if (sem_eval_width_expr(mem_ref.mem_decl->width,
                                            mod_scope, project_symbols, &eval_w) &&
                        eval_w > 0) {
                        mem_width = (int)eval_w;
                    }
                }
            }

            IR_Expr *ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
            if (!ir) return NULL;
            memset(ir, 0, sizeof(*ir));
            ir->kind = EXPR_MEM_READ;
            ir->width = mem_width;
            ir->source_line = expr->loc.line;
            ir->u.mem_read.memory_name = mem_name;
            ir->u.mem_read.port_name = port_nm;
            ir->u.mem_read.address = addr_ir;
            return ir;
        }

        if (base->type != JZ_AST_EXPR_IDENTIFIER || !base->name) {
            /* Expression slice: (expr)[msb:lsb]. Build the base expression
             * and create an EXPR_SLICE with base_expr set.
             */
            if (expr->child_count < 3) return NULL;
            JZASTNode *msb_node = expr->children[1];
            JZASTNode *lsb_node = expr->children[2];
            unsigned msb_val = 0, lsb_val = 0;
            if (!msb_node || !lsb_node ||
                !ir_eval_slice_bound(msb_node, mod_scope, project_symbols, &msb_val) ||
                !ir_eval_slice_bound(lsb_node, mod_scope, project_symbols, &lsb_val)) {
                return NULL;
            }

            /* Build the base expression recursively. */
            IR_Expr *base_ir = ir_build_expr(arena, base, mod_scope,
                                             project_symbols, bus_map,
                                             bus_map_count, diagnostics);
            if (!base_ir) return NULL;

            IR_Expr *ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
            if (!ir) return NULL;
            memset(ir, 0, sizeof(*ir));
            ir->kind = EXPR_SLICE;
            ir->width = (int)(msb_val - lsb_val + 1);
            ir->source_line = expr->loc.line;
            ir->u.slice.signal_id = -1;  /* Not a signal, using base_expr */
            ir->u.slice.base_expr = base_ir;
            ir->u.slice.msb = (int)msb_val;
            ir->u.slice.lsb = (int)lsb_val;
            return ir;
        }
        const JZSymbol *sym = module_scope_lookup(mod_scope, base->name);
        if (!sym || !sym->node) return NULL;

        /* MUX selector form: mux_name[index_expr]. Semantic analysis has
         * already validated the MUX declaration and selector range. In IR we
         * represent this as an EXPR_INTRINSIC_GSLICE whose source expression
         * is either a concatenation of sources (AGGREGATE form) or a wide
         * signal (SLICE form), and whose index is the selector expression.
         */
        if (sym->kind == JZ_SYM_MUX) {
            JZASTNode *idx_node = expr->children[1];
            if (!idx_node) return NULL;

            IR_Expr *idx_expr = ir_build_expr(arena,
                                              idx_node,
                                              mod_scope,
                                              project_symbols,
                                              bus_map,
                                              bus_map_count,
                                              diagnostics);
            if (!idx_expr) return NULL;

            JZASTNode *mux_decl = sym->node;
            IR_Expr *source_expr = NULL;
            int element_width = 0;

            if (mux_decl->block_kind && strcmp(mux_decl->block_kind, "AGGREGATE") == 0) {
                /* AGGREGATE form: mux_name = a, b, c, ...; Build a concat
                 * {a, b, c, ...} as the gslice source, using module-local
                 * signals only.
                 */
                if (mux_decl->child_count == 0) return NULL;
                JZASTNode *rhs = mux_decl->children[0];
                if (!rhs || rhs->type != JZ_AST_RAW_TEXT || !rhs->text) return NULL;

                const char *text = rhs->text;
                const char *p = text;
                /* First pass: count non-empty segments. */
                int seg_count = 0;
                while (*p) {
                    const char *seg_start = p;
                    const char *comma = strchr(p, ',');
                    size_t seg_len = 0;
                    if (comma) {
                        seg_len = (size_t)(comma - seg_start);
                        p = comma + 1;
                    } else {
                        seg_len = strlen(seg_start);
                        p = seg_start + seg_len;
                    }
                    /* Skip segments that are all whitespace. */
                    size_t k = 0;
                    while (k < seg_len && isspace((unsigned char)seg_start[k])) {
                        k++;
                    }
                    if (k < seg_len) {
                        seg_count++;
                    }
                }
                if (seg_count == 0) return NULL;

                IR_Expr **operands = (IR_Expr **)jz_arena_alloc(arena, sizeof(IR_Expr *) * (size_t)seg_count);
                if (!operands) return NULL;
                memset(operands, 0, sizeof(IR_Expr *) * (size_t)seg_count);

                /* Second pass: build signal-ref operands and compute widths. */
                p = text;
                int out_idx = 0;
                int total_width = 0;
                while (*p && out_idx < seg_count) {
                    const char *seg_start = p;
                    const char *comma = strchr(p, ',');
                    size_t seg_len = 0;
                    if (comma) {
                        seg_len = (size_t)(comma - seg_start);
                        p = comma + 1;
                    } else {
                        seg_len = strlen(seg_start);
                        p = seg_start + seg_len;
                    }

                    /* Trim whitespace around the segment. */
                    while (seg_len > 0 && isspace((unsigned char)seg_start[0])) {
                        seg_start++;
                        seg_len--;
                    }
                    while (seg_len > 0 && isspace((unsigned char)seg_start[seg_len - 1])) {
                        seg_len--;
                    }
                    if (seg_len == 0) {
                        continue;
                    }

                    char name_buf[128];
                    if (seg_len >= sizeof(name_buf)) {
                        continue; /* skip overly long/ill-formed segments */
                    }
                    memcpy(name_buf, seg_start, seg_len);
                    name_buf[seg_len] = '\0';

                    const JZSymbol *src_sym = module_scope_lookup(mod_scope, name_buf);
                    if (!src_sym || !src_sym->node) {
                        continue;
                    }
                    if (src_sym->kind != JZ_SYM_PORT &&
                        src_sym->kind != JZ_SYM_WIRE &&
                        src_sym->kind != JZ_SYM_REGISTER &&
                        src_sym->kind != JZ_SYM_LATCH) {
                        continue;
                    }

                    unsigned w = 0;
                    if (src_sym->node->width &&
                        sem_eval_width_expr(src_sym->node->width,
                                            mod_scope,
                                            project_symbols,
                                            &w) != 0) {
                        /* On failure, treat width as unknown (0); semantic
                         * analysis should already have rejected invalid
                         * widths before IR construction.
                         */
                        w = 0;
                    }

                    IR_Expr *src_expr = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
                    if (!src_expr) return NULL;
                    memset(src_expr, 0, sizeof(*src_expr));
                    src_expr->kind = EXPR_SIGNAL_REF;
                    src_expr->width = (int)w;
                    src_expr->source_line = src_sym->node->loc.line;
                    src_expr->u.signal_ref.signal_id = src_sym->id;

                    operands[out_idx++] = src_expr;
                    total_width += (int)w;
                    if (element_width == 0 && (int)w > 0) {
                        element_width = (int)w;
                    }
                }

                if (out_idx == 0) return NULL;

                /* Reverse the operands so that element 0 is at LSB.
                 * Verilog concat {a, b, c} puts 'a' at MSB and 'c' at LSB.
                 * The GSLICE intrinsic uses (source >> (idx * elem_w)) to extract,
                 * which gets the LSB portion at idx=0. So we need element 0 at LSB.
                 */
                for (int i = 0; i < out_idx / 2; ++i) {
                    int j = out_idx - 1 - i;
                    IR_Expr *tmp = operands[i];
                    operands[i] = operands[j];
                    operands[j] = tmp;
                }

                IR_Expr *concat = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
                if (!concat) return NULL;
                memset(concat, 0, sizeof(*concat));
                concat->kind = EXPR_CONCAT;
                concat->width = total_width;
                concat->source_line = mux_decl->loc.line;
                concat->u.concat.operands = operands;
                concat->u.concat.num_operands = out_idx;
                source_expr = concat;
            } else if (mux_decl->block_kind && strcmp(mux_decl->block_kind, "SLICE") == 0) {
                /* SLICE form: mux_name[elem_w] = wide_bus; selector chooses
                 * an element from the wide bus. We lower this to intrinsic
                 * gslice(wide_bus, index, elem_w).
                 */
                if (mux_decl->child_count == 0) return NULL;
                JZASTNode *rhs = mux_decl->children[0];
                if (!rhs || rhs->type != JZ_AST_RAW_TEXT || !rhs->text) return NULL;

                /* Extract wide bus name (single identifier). */
                const char *pname = rhs->text;
                while (*pname && isspace((unsigned char)*pname)) pname++;
                const char *end = pname;
                while (*end && !isspace((unsigned char)*end)) end++;
                size_t len = (size_t)(end - pname);
                if (len == 0 || len >= 128) return NULL;
                char wide_name[128];
                memcpy(wide_name, pname, len);
                wide_name[len] = '\0';

                const JZSymbol *wide_sym = module_scope_lookup(mod_scope, wide_name);
                if (!wide_sym || !wide_sym->node) return NULL;
                unsigned elem_w = 0;
                if (mux_decl->width &&
                    sem_eval_width_expr(mux_decl->width,
                                        mod_scope,
                                        project_symbols,
                                        &elem_w) == 0 &&
                    elem_w > 0u) {
                    element_width = (int)elem_w;
                }

                unsigned wide_w = 0;
                if (wide_sym->node->width &&
                    sem_eval_width_expr(wide_sym->node->width,
                                        mod_scope,
                                        project_symbols,
                                        &wide_w) == 0 &&
                    wide_w > 0u) {
                    (void)wide_w; /* width may be used by future passes */
                }

                IR_Expr *src_expr = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
                if (!src_expr) return NULL;
                memset(src_expr, 0, sizeof(*src_expr));
                src_expr->kind = EXPR_SIGNAL_REF;
                src_expr->width = (int)wide_w;
                src_expr->source_line = wide_sym->node->loc.line;
                src_expr->u.signal_ref.signal_id = wide_sym->id;

                source_expr = src_expr;
            }

            if (!source_expr) {
                return NULL;
            }

            IR_Expr *ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
            if (!ir) return NULL;
            memset(ir, 0, sizeof(*ir));
            ir->kind = EXPR_INTRINSIC_GSLICE;
            /* Prefer the per-element width when inferred; fall back to the
             * semantic type width otherwise.
             */
            int result_width = (element_width > 0) ? element_width : (int)t.width;
            ir->width = result_width;
            ir->source_line = expr->loc.line;
            ir->u.intrinsic.source = source_expr;
            ir->u.intrinsic.index = idx_expr;
            ir->u.intrinsic.value = NULL;
            ir->u.intrinsic.element_width = (element_width > 0) ? element_width : result_width;
            return ir;
        }

        /* Slice bounds lowered to EXPR_SLICE for non-MUX signals. */
        if (expr->child_count < 3) return NULL;
        JZASTNode *msb = expr->children[1];
        JZASTNode *lsb = expr->children[2];
        unsigned msb_val = 0, lsb_val = 0;
        if (!msb || !lsb ||
            !ir_eval_slice_bound(msb, mod_scope, project_symbols, &msb_val) ||
            !ir_eval_slice_bound(lsb, mod_scope, project_symbols, &lsb_val)) {
            return NULL;
        }
        IR_Expr *ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
        if (!ir) return NULL;
        memset(ir, 0, sizeof(*ir));
        ir->kind = EXPR_SLICE;
        ir->width = (int)t.width;
        ir->source_line = expr->loc.line;
        ir->u.slice.signal_id = sym->id;
        ir->u.slice.msb = (int)msb_val;
        ir->u.slice.lsb = (int)lsb_val;
        return ir;
    }

    case JZ_AST_EXPR_BUILTIN_CALL: {
        if (!expr->name || expr->child_count == 0) return NULL;
        const char *fname = expr->name;

        if (!strcmp(fname, "lit")) {
            if (expr->child_count < 2) return NULL;
            long long width_val = 0;
            long long value_val = 0;
            if (ir_eval_lit_const_expr(expr->children[0],
                                       mod_scope,
                                       project_symbols,
                                       &width_val) != 0 ||
                width_val <= 0 || width_val > INT_MAX) {
                return NULL;
            }
            if (ir_eval_lit_const_expr(expr->children[1],
                                       mod_scope,
                                       project_symbols,
                                       &value_val) != 0 ||
                value_val < 0) {
                return NULL;
            }

            IR_Expr *ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
            if (!ir) return NULL;
            memset(ir, 0, sizeof(*ir));
            ir->kind = EXPR_LITERAL;
            ir->width = (int)width_val;
            ir->source_line = expr->loc.line;
            ir->u.literal.literal.width = ir->width;
            ir->u.literal.literal.words[0] = (uint64_t)value_val;
            if (width_val < 64) {
                uint64_t mask = (width_val == 64) ? ~0ULL : ((1ULL << (unsigned)width_val) - 1ULL);
                ir->u.literal.literal.words[0] &= mask;
            }
            return ir;
        }

        /* Arithmetic intrinsics: uadd/sadd/umul/smul - lowered as binary
         * expressions with distinct kinds.
         */
        if (!strcmp(fname, "uadd") || !strcmp(fname, "sadd") ||
            !strcmp(fname, "umul") || !strcmp(fname, "smul")) {
            if (expr->child_count < 2) return NULL;
            IR_Expr *a = ir_build_expr(arena, expr->children[0], mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
            IR_Expr *b = ir_build_expr(arena, expr->children[1], mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
            if (!a || !b) return NULL;
            IR_Expr *ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
            if (!ir) return NULL;
            memset(ir, 0, sizeof(*ir));
            if (!strcmp(fname, "uadd")) ir->kind = EXPR_INTRINSIC_UADD;
            else if (!strcmp(fname, "sadd")) ir->kind = EXPR_INTRINSIC_SADD;
            else if (!strcmp(fname, "umul")) ir->kind = EXPR_INTRINSIC_UMUL;
            else ir->kind = EXPR_INTRINSIC_SMUL;
            ir->width = (int)t.width;
            ir->source_line = expr->loc.line;
            ir->u.binary.left = a;
            ir->u.binary.right = b;
            return ir;
        }

        /* Bit-level intrinsics: gbit(source, index), sbit(source, index, set). */
        if (!strcmp(fname, "gbit")) {
            if (expr->child_count < 2) return NULL;
            IR_Expr *src = ir_build_expr(arena, expr->children[0], mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
            IR_Expr *idx = ir_build_expr(arena, expr->children[1], mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
            if (!src || !idx) return NULL;
            IR_Expr *ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
            if (!ir) return NULL;
            memset(ir, 0, sizeof(*ir));
            ir->kind = EXPR_INTRINSIC_GBIT;
            ir->width = (int)t.width;
            ir->source_line = expr->loc.line;
            ir->u.intrinsic.source = src;
            ir->u.intrinsic.index = idx;
            ir->u.intrinsic.value = NULL;
            ir->u.intrinsic.element_width = 1;
            return ir;
        }

        if (!strcmp(fname, "sbit")) {
            if (expr->child_count < 3) return NULL;
            IR_Expr *src = ir_build_expr(arena, expr->children[0], mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
            IR_Expr *idx = ir_build_expr(arena, expr->children[1], mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
            IR_Expr *val = ir_build_expr(arena, expr->children[2], mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
            if (!src || !idx || !val) return NULL;
            IR_Expr *ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
            if (!ir) return NULL;
            memset(ir, 0, sizeof(*ir));
            ir->kind = EXPR_INTRINSIC_SBIT;
            ir->width = (int)t.width;
            ir->source_line = expr->loc.line;
            ir->u.intrinsic.source = src;
            ir->u.intrinsic.index = idx;
            ir->u.intrinsic.value = val;
            ir->u.intrinsic.element_width = 1;
            return ir;
        }

        /* Slice intrinsics: gslice(source, index, width), sslice(source, index,
         * width, value). The width argument remains in expression form; IR
         * captures selector and source/value structure, while precise element
         * widths are refined by constant-eval aware passes.
         */
        if (!strcmp(fname, "gslice")) {
            if (expr->child_count < 2) return NULL;
            IR_Expr *src = ir_build_expr(arena, expr->children[0], mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
            IR_Expr *idx = ir_build_expr(arena, expr->children[1], mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
            if (!src || !idx) return NULL;
            /* Extract element width from 3rd argument if available. */
            int ew = (int)t.width;
            if (expr->child_count >= 3 && expr->children[2]) {
                unsigned ew_val = 0;
                if (ir_eval_slice_bound(expr->children[2], mod_scope,
                                         project_symbols, &ew_val) && ew_val > 0) {
                    ew = (int)ew_val;
                }
            }
            if (ew <= 0 && src->width > 0) ew = src->width;
            IR_Expr *ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
            if (!ir) return NULL;
            memset(ir, 0, sizeof(*ir));
            ir->kind = EXPR_INTRINSIC_GSLICE;
            ir->width = ew > 0 ? ew : (int)t.width;
            ir->source_line = expr->loc.line;
            ir->u.intrinsic.source = src;
            ir->u.intrinsic.index = idx;
            ir->u.intrinsic.value = NULL;
            ir->u.intrinsic.element_width = ew;
            return ir;
        }

        if (!strcmp(fname, "sslice")) {
            if (expr->child_count < 4) return NULL;
            IR_Expr *src = ir_build_expr(arena, expr->children[0], mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
            IR_Expr *idx = ir_build_expr(arena, expr->children[1], mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
            IR_Expr *val = ir_build_expr(arena, expr->children[3], mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
            if (!src || !idx || !val) return NULL;
            IR_Expr *ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
            if (!ir) return NULL;
            memset(ir, 0, sizeof(*ir));
            ir->kind = EXPR_INTRINSIC_SSLICE;
            ir->width = (int)t.width;
            ir->source_line = expr->loc.line;
            ir->u.intrinsic.source = src;
            ir->u.intrinsic.index = idx;
            ir->u.intrinsic.value = val;
            ir->u.intrinsic.element_width = (int)t.width; /* best-effort */
            return ir;
        }

        /* oh2b(source) — one-hot to binary encoder. */
        if (!strcmp(fname, "oh2b")) {
            if (expr->child_count < 1) return NULL;
            IR_Expr *src = ir_build_expr(arena, expr->children[0], mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
            if (!src) return NULL;
            IR_Expr *ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
            if (!ir) return NULL;
            memset(ir, 0, sizeof(*ir));
            ir->kind = EXPR_INTRINSIC_OH2B;
            ir->width = (int)t.width;
            ir->source_line = expr->loc.line;
            ir->u.intrinsic.source = src;
            ir->u.intrinsic.index = NULL;
            ir->u.intrinsic.value = NULL;
            ir->u.intrinsic.element_width = 0;
            return ir;
        }

        /* b2oh(index, width) — binary to one-hot decoder. */
        if (!strcmp(fname, "b2oh")) {
            if (expr->child_count < 2) return NULL;
            IR_Expr *idx = ir_build_expr(arena, expr->children[0], mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
            if (!idx) return NULL;
            IR_Expr *ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
            if (!ir) return NULL;
            memset(ir, 0, sizeof(*ir));
            ir->kind = EXPR_INTRINSIC_B2OH;
            ir->width = (int)t.width;
            ir->source_line = expr->loc.line;
            ir->u.intrinsic.source = idx;
            ir->u.intrinsic.index = NULL;
            ir->u.intrinsic.value = NULL;
            ir->u.intrinsic.element_width = (int)t.width; /* output width */
            return ir;
        }

        /* prienc(source) — priority encoder (MSB-first). */
        if (!strcmp(fname, "prienc")) {
            if (expr->child_count < 1) return NULL;
            IR_Expr *src = ir_build_expr(arena, expr->children[0], mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
            if (!src) return NULL;
            IR_Expr *ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
            if (!ir) return NULL;
            memset(ir, 0, sizeof(*ir));
            ir->kind = EXPR_INTRINSIC_PRIENC;
            ir->width = (int)t.width;
            ir->source_line = expr->loc.line;
            ir->u.intrinsic.source = src;
            return ir;
        }

        /* lzc(source) — leading zero count. */
        if (!strcmp(fname, "lzc")) {
            if (expr->child_count < 1) return NULL;
            IR_Expr *src = ir_build_expr(arena, expr->children[0], mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
            if (!src) return NULL;
            IR_Expr *ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
            if (!ir) return NULL;
            memset(ir, 0, sizeof(*ir));
            ir->kind = EXPR_INTRINSIC_LZC;
            ir->width = (int)t.width;
            ir->source_line = expr->loc.line;
            ir->u.intrinsic.source = src;
            return ir;
        }

        /* usub/ssub — unsigned/signed widening subtract. */
        if (!strcmp(fname, "usub") || !strcmp(fname, "ssub")) {
            if (expr->child_count < 2) return NULL;
            IR_Expr *a = ir_build_expr(arena, expr->children[0], mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
            IR_Expr *b = ir_build_expr(arena, expr->children[1], mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
            if (!a || !b) return NULL;
            IR_Expr *ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
            if (!ir) return NULL;
            memset(ir, 0, sizeof(*ir));
            ir->kind = !strcmp(fname, "usub") ? EXPR_INTRINSIC_USUB : EXPR_INTRINSIC_SSUB;
            ir->width = (int)t.width;
            ir->source_line = expr->loc.line;
            ir->u.binary.left = a;
            ir->u.binary.right = b;
            return ir;
        }

        /* abs(a) — signed absolute value. */
        if (!strcmp(fname, "abs")) {
            if (expr->child_count < 1) return NULL;
            IR_Expr *src = ir_build_expr(arena, expr->children[0], mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
            if (!src) return NULL;
            IR_Expr *ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
            if (!ir) return NULL;
            memset(ir, 0, sizeof(*ir));
            ir->kind = EXPR_INTRINSIC_ABS;
            ir->width = (int)t.width;
            ir->source_line = expr->loc.line;
            ir->u.intrinsic.source = src;
            return ir;
        }

        /* umin/umax/smin/smax — min/max intrinsics. */
        if (!strcmp(fname, "umin") || !strcmp(fname, "umax") ||
            !strcmp(fname, "smin") || !strcmp(fname, "smax")) {
            if (expr->child_count < 2) return NULL;
            IR_Expr *a = ir_build_expr(arena, expr->children[0], mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
            IR_Expr *b = ir_build_expr(arena, expr->children[1], mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
            if (!a || !b) return NULL;
            IR_Expr *ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
            if (!ir) return NULL;
            memset(ir, 0, sizeof(*ir));
            if (!strcmp(fname, "umin")) ir->kind = EXPR_INTRINSIC_UMIN;
            else if (!strcmp(fname, "umax")) ir->kind = EXPR_INTRINSIC_UMAX;
            else if (!strcmp(fname, "smin")) ir->kind = EXPR_INTRINSIC_SMIN;
            else ir->kind = EXPR_INTRINSIC_SMAX;
            ir->width = (int)t.width;
            ir->source_line = expr->loc.line;
            ir->u.binary.left = a;
            ir->u.binary.right = b;
            return ir;
        }

        /* popcount(source) — population count. */
        if (!strcmp(fname, "popcount")) {
            if (expr->child_count < 1) return NULL;
            IR_Expr *src = ir_build_expr(arena, expr->children[0], mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
            if (!src) return NULL;
            IR_Expr *ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
            if (!ir) return NULL;
            memset(ir, 0, sizeof(*ir));
            ir->kind = EXPR_INTRINSIC_POPCOUNT;
            ir->width = (int)t.width;
            ir->source_line = expr->loc.line;
            ir->u.intrinsic.source = src;
            return ir;
        }

        /* reverse(source) — bit reversal. */
        if (!strcmp(fname, "reverse")) {
            if (expr->child_count < 1) return NULL;
            IR_Expr *src = ir_build_expr(arena, expr->children[0], mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
            if (!src) return NULL;
            IR_Expr *ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
            if (!ir) return NULL;
            memset(ir, 0, sizeof(*ir));
            ir->kind = EXPR_INTRINSIC_REVERSE;
            ir->width = (int)t.width;
            ir->source_line = expr->loc.line;
            ir->u.intrinsic.source = src;
            return ir;
        }

        /* bswap(source) — byte swap. */
        if (!strcmp(fname, "bswap")) {
            if (expr->child_count < 1) return NULL;
            IR_Expr *src = ir_build_expr(arena, expr->children[0], mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
            if (!src) return NULL;
            IR_Expr *ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
            if (!ir) return NULL;
            memset(ir, 0, sizeof(*ir));
            ir->kind = EXPR_INTRINSIC_BSWAP;
            ir->width = (int)t.width;
            ir->source_line = expr->loc.line;
            ir->u.intrinsic.source = src;
            return ir;
        }

        /* reduce_and/reduce_or/reduce_xor — reduction operators. */
        if (!strcmp(fname, "reduce_and") || !strcmp(fname, "reduce_or") ||
            !strcmp(fname, "reduce_xor")) {
            if (expr->child_count < 1) return NULL;
            IR_Expr *src = ir_build_expr(arena, expr->children[0], mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
            if (!src) return NULL;
            IR_Expr *ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
            if (!ir) return NULL;
            memset(ir, 0, sizeof(*ir));
            if (!strcmp(fname, "reduce_and")) ir->kind = EXPR_INTRINSIC_REDUCE_AND;
            else if (!strcmp(fname, "reduce_or")) ir->kind = EXPR_INTRINSIC_REDUCE_OR;
            else ir->kind = EXPR_INTRINSIC_REDUCE_XOR;
            ir->width = (int)t.width;
            ir->source_line = expr->loc.line;
            ir->u.intrinsic.source = src;
            return ir;
        }

        /* Other builtins (clog2/widthof) are compile-time only and should not
         * appear here; if they do, leave them unsupported for now.
         */
        return NULL;
    }

    default:
        /* Other expression forms will be lowered in later IR construction
         * stages (e.g., MEM/MUX-specific expressions).
         */
        return NULL;
    }
}

/**
 * @brief Decode a sized literal into an IR_Literal.
 *
 * Accepts literals of the form "<width>'<base><value>" and evaluates
 * width expressions when required.
 *
 * @param lex             Literal lexeme.
 * @param mod_scope       Module scope.
 * @param project_symbols Project-level symbols.
 * @param out             Output literal.
 * @return 0 on success, non-zero on failure.
 */
int ir_decode_sized_literal(const char *lex,
                            const JZModuleScope *mod_scope,
                            const JZBuffer *project_symbols,
                            IR_Literal *out)
{
    if (!lex || !out) return -1;

    const char *tick = strchr(lex, '\'');
    if (!tick) {
        return -1; /* not a sized literal */
    }

    /* Parse declared width from the prefix before the '\'' character. */
    char width_buf[32];
    size_t width_len = (size_t)(tick - lex);
    if (width_len == 0 || width_len >= sizeof(width_buf)) {
        return -1;
    }
    memcpy(width_buf, lex, width_len);
    width_buf[width_len] = '\0';

    unsigned declared_width = 0;
    int have_width = 0;
    if (parse_simple_positive_int(width_buf, &declared_width) && declared_width > 0) {
        have_width = 1;
    } else {
        /* Width may be a CONST/CONFIG expression (e.g., ADDR_W'b1). Try to
         * evaluate it as a constant expression in the module scope. If this
         * also fails, we still attempt to decode the literal's value and leave
         * width as 0 so downstream code can treat it as unsized.
         */
        if (mod_scope) {
            long long w_val = 0;
            if (sem_eval_const_expr_in_module(width_buf,
                                              mod_scope,
                                              project_symbols,
                                              &w_val) == 0 &&
                w_val > 0) {
                declared_width = (unsigned)w_val;
                have_width = 1;
            }
        }
    }

    char base_ch = tick[1];
    JZNumericBase base = JZ_NUM_BASE_NONE;
    if (base_ch == 'b' || base_ch == 'B') base = JZ_NUM_BASE_BIN;
    else if (base_ch == 'd' || base_ch == 'D') base = JZ_NUM_BASE_DEC;
    else if (base_ch == 'h' || base_ch == 'H') base = JZ_NUM_BASE_HEX;
    else return -1;

    const char *value_lexeme = tick + 2;
    if (!value_lexeme || !*value_lexeme) {
        return -1;
    }

    /* Trim trailing whitespace from the value portion so that both the
     * literal analyzer and our manual decoder see a clean token.
     */
    char value_buf[256];
    size_t vlen = strlen(value_lexeme);
    while (vlen > 0 && isspace((unsigned char)value_lexeme[vlen - 1])) {
        --vlen;
    }
    if (vlen >= sizeof(value_buf)) {
        vlen = sizeof(value_buf) - 1;
    }
    memcpy(value_buf, value_lexeme, vlen);
    value_buf[vlen] = '\0';
    const char *clean_value = value_buf;

    /* Ask the literal analyzer to validate width vs. value and compute the
     * intrinsic width and extension behavior when we know the declared width.
     * If width could not be resolved (have_width==0), skip this check and
     * treat the literal as an unsized value.
     */
    unsigned intrinsic_width = 0;
    JZLiteralExtKind ext = JZ_LITERAL_EXT_NONE;
    if (have_width) {
        if (jz_literal_analyze(base,
                               clean_value,
                               declared_width,
                               &intrinsic_width,
                               &ext) != 0) {
            return -1;
        }
    }

    /* Decode the numeric value into a multi-word payload. X/Z digits are treated
     * as zeros for the purposes of the concrete value; their extension kind is
     * already validated above via jz_literal_analyze.
     *
     * Also detect if the literal is entirely composed of 'z' digits (high-Z).
     */
    uint64_t words[IR_LIT_WORDS];
    memset(words, 0, sizeof(words));
    int all_z = 1;  /* assume all-z until we see a non-z digit */
    int has_digits = 0;
    int total_bits = 0;

    if (base == JZ_NUM_BASE_BIN) {
        /* First pass: count digits to know total bits */
        int num_bits = 0;
        for (const char *p = clean_value; *p; ++p) {
            char c = *p;
            if (c == '_' || c == ' ' || c == '\t' || c == '\r' || c == '\n')
                continue;
            num_bits++;
        }
        /* Second pass: place bits from MSB to LSB */
        int bit_idx = num_bits - 1;
        for (const char *p = clean_value; *p; ++p) {
            char c = *p;
            if (c == '_' || c == ' ' || c == '\t' || c == '\r' || c == '\n')
                continue;
            has_digits = 1;
            int bit = 0;
            if (c == '0') {
                bit = 0;
                all_z = 0;
            } else if (c == '1') {
                bit = 1;
                all_z = 0;
            } else if (c == 'z' || c == 'Z') {
                bit = 0;
            } else if (c == 'x' || c == 'X') {
                bit = 0;
                all_z = 0;
            } else {
                bit_idx--;
                continue;
            }
            if (bit_idx >= 0 && bit_idx < IR_LIT_WORDS * 64) {
                int wi = bit_idx / 64;
                int bi = bit_idx % 64;
                words[wi] |= (uint64_t)bit << bi;
            }
            bit_idx--;
        }
        total_bits = num_bits;
    } else if (base == JZ_NUM_BASE_HEX) {
        /* First pass: count hex digits */
        int num_nibs = 0;
        for (const char *p = value_lexeme; *p; ++p) {
            char c = *p;
            if (c == '_' || c == ' ' || c == '\t' || c == '\r' || c == '\n')
                continue;
            num_nibs++;
        }
        /* Second pass: place nibbles from MSB to LSB */
        int nib_idx = num_nibs - 1;
        for (const char *p = value_lexeme; *p; ++p) {
            char c = *p;
            if (c == '_' || c == ' ' || c == '\t' || c == '\r' || c == '\n')
                continue;
            has_digits = 1;
            unsigned nibble = 0;
            if (c >= '0' && c <= '9') {
                nibble = (unsigned)(c - '0');
                all_z = 0;
            } else if (c >= 'a' && c <= 'f') {
                nibble = 10u + (unsigned)(c - 'a');
                all_z = 0;
            } else if (c >= 'A' && c <= 'F') {
                nibble = 10u + (unsigned)(c - 'A');
                all_z = 0;
            } else if (c == 'z' || c == 'Z') {
                nibble = 0;
            } else if (c == 'x' || c == 'X') {
                nibble = 0;
                all_z = 0;
            } else {
                nib_idx--;
                continue;
            }
            if (nib_idx >= 0) {
                int bit_base = nib_idx * 4;
                int wi = bit_base / 64;
                int bi = bit_base % 64;
                if (wi < IR_LIT_WORDS) {
                    words[wi] |= (uint64_t)nibble << bi;
                    /* Handle cross-word nibble */
                    if (bi > 60 && wi + 1 < IR_LIT_WORDS) {
                        words[wi + 1] |= (uint64_t)nibble >> (64 - bi);
                    }
                }
            }
            nib_idx--;
        }
        total_bits = num_nibs * 4;
    } else if (base == JZ_NUM_BASE_DEC) {
        /* Decimal magnitude; ignore underscores and clamp into 64 bits. */
        all_z = 0;  /* decimal literals cannot be 'z' */
        uint64_t value = 0;
        for (const char *p = value_lexeme; *p; ++p) {
            char c = *p;
            if (c == '_' || c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                continue;
            }
            has_digits = 1;
            if (c < '0' || c > '9') {
                return -1;
            }
            unsigned d = (unsigned)(c - '0');
            value = value * 10u + (uint64_t)d;
        }
        /* Mask down to declared_width bits if smaller than 64 and known. */
        if (have_width && declared_width < 64u) {
            uint64_t mask = (declared_width == 64u) ? ~0ULL : ((1ULL << declared_width) - 1ULL);
            value &= mask;
        }
        words[0] = value;
        total_bits = 64;
    }

    (void)total_bits;
    memcpy(out->words, words, sizeof(words));
    out->width = have_width ? (int)declared_width : 0;
    out->is_z = (all_z && has_digits) ? 1 : 0;
    return 0;
}


/**
 * @brief Serialize an AST expression into a flat constant-expression string.
 *
 * This is a wrapper around the recursive serializer that produces a
 * NUL-terminated string suitable for semantic constant evaluation.
 *
 * @param expr     AST expression node.
 * @param out_str  Output string (heap-allocated).
 * @return 0 on success, non-zero on failure.
 */
int ir_expr_to_const_expr_string(const JZASTNode *expr, char **out_str)
{
    if (!expr || !out_str) return -1;
    *out_str = NULL;

    JZBuffer buf = {0};
    if (ir_expr_to_const_expr_string_rec(expr, &buf) != 0) {
        jz_buf_free(&buf);
        return -1;
    }

    const char nul = '\0';
    if (jz_buf_append(&buf, &nul, 1) != 0) {
        jz_buf_free(&buf);
        return -1;
    }

    *out_str = (char *)buf.data;
    buf.data = NULL;
    buf.len = buf.cap = 0;
    return 0;
}
