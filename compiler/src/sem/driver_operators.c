#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>

#include "sem.h"
#include "driver_internal.h"

/* -------------------------------------------------------------------------
 *  Expression typing and operator rules
 * -------------------------------------------------------------------------
 */

/* Helper for UNARY_ARITH_MISSING_PARENS: best-effort check that a unary
 * arithmetic operator appears immediately inside parentheses, e.g. "(-flag)".
 */
static int sem_unary_has_required_parens(const JZASTNode *expr)
{
    if (!expr) return 1;
    const char *filename = expr->loc.filename;
    if (!filename || expr->loc.line <= 0 || expr->loc.column <= 0) {
        return 1; /* cannot reliably check */
    }

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        return 1; /* I/O failure: do not emit spurious diagnostics */
    }

    char line_buf[4096];
    int current_line = 1;
    int found = 0;
    while (fgets(line_buf, sizeof(line_buf), fp)) {
        if (current_line == expr->loc.line) {
            found = 1;
            break;
        }
        current_line++;
    }
    fclose(fp);
    if (!found) {
        return 1;
    }

    size_t len = strlen(line_buf);
    int col_index = expr->loc.column - 1; /* 0-based */
    if (col_index <= 0 || (size_t)col_index >= len) {
        return 1;
    }

    /* Sanity check: the operator token should be '+' or '-'. */
    char op_ch = line_buf[col_index];
    if (op_ch != '+' && op_ch != '-') {
        return 1;
    }

    /* Walk left to find the previous non-whitespace character. */
    for (int i = col_index - 1; i >= 0; --i) {
        unsigned char c = (unsigned char)line_buf[i];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            continue;
        }
        return (c == '(') ? 1 : 0;
    }

    return 0;
}

static int sem_lit_expr_to_const_string_rec(const JZASTNode *expr,
                                            const JZModuleScope *mod_scope,
                                            const JZBuffer *project_symbols,
                                            JZDiagnosticList *diagnostics,
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
        if (!found) {
            sem_report_rule(diagnostics,
                            expr->loc,
                            "CONFIG_USE_UNDECLARED",
                            "Use of CONFIG.<name> not declared in project CONFIG");
            return -1;
        }
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
        if (sem_lit_expr_to_const_string_rec(expr->children[0],
                                             mod_scope,
                                             project_symbols,
                                             diagnostics,
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
        if (sem_lit_expr_to_const_string_rec(expr->children[0],
                                             mod_scope,
                                             project_symbols,
                                             diagnostics,
                                             buf) != 0) {
            return -1;
        }
        if (jz_buf_append(buf, op, strlen(op)) != 0) return -1;
        if (sem_lit_expr_to_const_string_rec(expr->children[1],
                                             mod_scope,
                                             project_symbols,
                                             diagnostics,
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
            if (sem_lit_expr_to_const_string_rec(expr->children[i],
                                                 mod_scope,
                                                 project_symbols,
                                                 diagnostics,
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

static int sem_lit_expr_to_const_string(const JZASTNode *expr,
                                        const JZModuleScope *mod_scope,
                                        const JZBuffer *project_symbols,
                                        JZDiagnosticList *diagnostics,
                                        char **out_str)
{
    if (!expr || !out_str) return -1;
    *out_str = NULL;

    JZBuffer buf = {0};
    if (sem_lit_expr_to_const_string_rec(expr, mod_scope, project_symbols, diagnostics, &buf) != 0) {
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

static int sem_lit_eval_const_expr(const JZASTNode *expr,
                                   const JZModuleScope *mod_scope,
                                   const JZBuffer *project_symbols,
                                   JZDiagnosticList *diagnostics,
                                   long long *out_value)
{
    if (!expr || !out_value) return -1;

    char *expr_text = NULL;
    if (sem_lit_expr_to_const_string(expr, mod_scope, project_symbols, diagnostics, &expr_text) != 0 || !expr_text) {
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

static void infer_identifier_type(JZASTNode *node,
                                  const JZModuleScope *mod_scope,
                                  const JZBuffer *project_symbols,
                                  JZBitvecType *out)
{
    if (!out) return;
    out->width = 0;
    out->is_signed = 0;
    if (!node || !node->name || !mod_scope) return;

    const JZSymbol *sym = module_scope_lookup(mod_scope, node->name);
    if (!sym || !sym->node) return;

    const char *width_text = sym->node->width;
    unsigned w = 0;
    if (!width_text) {
        return; /* unknown width */
    }

    if (sem_eval_width_expr(width_text, mod_scope, project_symbols, &w) != 0) {
        return; /* expression not yet resolvable; leave width unknown */
    }

    jz_type_scalar(w, 0, out);
}

static void infer_qualified_identifier_type(JZASTNode *node,
                                            const JZModuleScope *mod_scope,
                                            const JZBuffer *project_symbols,
                                            JZBitvecType *out)
{
    if (!out) return;
    out->width = 0;
    out->is_signed = 0;
    if (!node || !node->name || !mod_scope) return;

    if (project_symbols) {
        JZBusAccessInfo info;
        if (sem_resolve_bus_access(node, mod_scope, project_symbols, &info, NULL) &&
            info.signal_decl && info.signal_decl->width) {
            unsigned w = 0;
            if (info.is_wildcard) {
                if (info.count > 0) {
                    jz_type_scalar(info.count, 0, out);
                }
                return;
            }
            if (sem_eval_width_expr(info.signal_decl->width,
                                    mod_scope,
                                    project_symbols,
                                    &w) == 0) {
                jz_type_scalar(w, 0, out);
                return;
            }
        }
    }

    const char *full = node->name;
    const char *dot = strchr(full, '.');
    if (!dot || !*(dot + 1)) return;

    char head[256];
    size_t head_len = (size_t)(dot - full);
    if (head_len >= sizeof(head)) head_len = sizeof(head) - 1;
    memcpy(head, full, head_len);
    head[head_len] = '\0';

    /* Check for @global constant reference: GLOBAL.CONST_NAME */
    if (project_symbols) {
        const JZSymbol *glob_sym = project_lookup(project_symbols, head, JZ_SYM_GLOBAL);
        if (glob_sym && glob_sym->node) {
            const char *cname = dot + 1;
            /* Search the global block's children for the constant. */
            for (size_t ci = 0; ci < glob_sym->node->child_count; ++ci) {
                JZASTNode *child = glob_sym->node->children[ci];
                if (!child || !child->name || !child->text) continue;
                if (strcmp(child->name, cname) != 0) continue;
                /* child->text is a sized literal like "16'hA5A5".
                 * Parse the width prefix before the tick mark.
                 */
                const char *tick = strchr(child->text, '\'');
                if (tick && tick > child->text) {
                    char wbuf[32];
                    size_t wlen = (size_t)(tick - child->text);
                    if (wlen < sizeof(wbuf)) {
                        memcpy(wbuf, child->text, wlen);
                        wbuf[wlen] = '\0';
                        unsigned w = 0;
                        if (parse_simple_positive_int(wbuf, &w)) {
                            jz_type_scalar(w, 0, out);
                        }
                    }
                }
                return;
            }
            return; /* Global namespace found but constant name not found */
        }
    }

    const JZSymbol *head_sym = module_scope_lookup(mod_scope, head);
    if (!head_sym || !head_sym->node) return;

    if (head_sym->kind == JZ_SYM_MEM) {
        const char *port_str = dot + 1;
        const char *second_dot = strchr(port_str, '.');
        const char *field_str = second_dot ? second_dot + 1 : NULL;
        int field = MEM_PORT_FIELD_NONE;

        if (second_dot) {
            if (!field_str || !*field_str) {
                return;
            }
            if (strcmp(field_str, "addr") == 0) {
                field = MEM_PORT_FIELD_ADDR;
            } else if (strcmp(field_str, "data") == 0) {
                field = MEM_PORT_FIELD_DATA;
            } else {
                return;
            }
        }

        if (field == MEM_PORT_FIELD_ADDR) {
            unsigned depth = 0;
            unsigned addr_width = 0;
            if (head_sym->node->text &&
                eval_simple_positive_decl_int(head_sym->node->text, &depth) == 1) {
                if (depth > 1) {
                    unsigned v = depth - 1u;
                    while (v) {
                        addr_width++;
                        v >>= 1;
                    }
                }
                /* Minimum address width is 1 bit. */
                if (addr_width == 0) addr_width = 1;
                jz_type_scalar(addr_width, 0, out);
            }
            return;
        }

        /* mem.port.data or mem.port (legacy): treat result width as word width. */
        const char *width_text = head_sym->node->width;
        unsigned w = 0;
        if (width_text && parse_simple_positive_int(width_text, &w)) {
            jz_type_scalar(w, 0, out);
        }
    }
}

static void infer_bus_access_type(JZASTNode *node,
                                  const JZModuleScope *mod_scope,
                                  const JZBuffer *project_symbols,
                                  JZBitvecType *out)
{
    if (!out) return;
    out->width = 0;
    out->is_signed = 0;
    if (!node || !mod_scope || !project_symbols) return;

    JZBusAccessInfo info;
    if (!sem_resolve_bus_access(node, mod_scope, project_symbols, &info, NULL)) {
        return;
    }
    if (!info.signal_decl || !info.signal_decl->width) return;

    if (info.is_wildcard) {
        if (info.count > 0) {
            jz_type_scalar(info.count, 0, out);
        }
        return;
    }

    unsigned w = 0;
    if (sem_eval_width_expr(info.signal_decl->width,
                            mod_scope,
                            project_symbols,
                            &w) == 0) {
        jz_type_scalar(w, 0, out);
    }
}

void infer_expr_type(JZASTNode *expr,
                     const JZModuleScope *mod_scope,
                     const JZBuffer *project_symbols,
                     JZDiagnosticList *diagnostics,
                     JZBitvecType *out)
{
    (void)project_symbols; /* reserved for future CONST/CONFIG-based typing */

    if (!out) return;
    out->width = 0;
    out->is_signed = 0;
    if (!expr) return;

    switch (expr->type) {
    case JZ_AST_EXPR_LITERAL:
        infer_literal_type(expr, diagnostics, out);
        return;

    case JZ_AST_EXPR_SPECIAL_DRIVER:
        /* GND/VCC are polymorphic; width is determined by assignment context.
         * Return width=0 to indicate "unknown/context-dependent". */
        out->width = 0;
        out->is_signed = 0;
        return;

    case JZ_AST_EXPR_IDENTIFIER:
        infer_identifier_type(expr, mod_scope, project_symbols, out);
        return;

    case JZ_AST_EXPR_QUALIFIED_IDENTIFIER:
        infer_qualified_identifier_type(expr, mod_scope, project_symbols, out);
        return;

    case JZ_AST_EXPR_BUS_ACCESS:
        infer_bus_access_type(expr, mod_scope, project_symbols, out);
        return;

    case JZ_AST_EXPR_UNARY: {
        if (expr->child_count < 1) return;
        JZBitvecType operand;
        infer_expr_type(expr->children[0], mod_scope, project_symbols, diagnostics, &operand);
        if (operand.width == 0) return;

        const char *kind = expr->block_kind ? expr->block_kind : "";
        JZUnaryOp op;
        const char *rule = NULL;

        int is_unary_arith = 0;
        if (strcmp(kind, "POS") == 0) {
            op = JZ_UNARY_PLUS;
            is_unary_arith = 1;
        } else if (strcmp(kind, "NEG") == 0) {
            op = JZ_UNARY_MINUS;
            is_unary_arith = 1;
        } else if (strcmp(kind, "BIT_NOT") == 0) {
            op = JZ_UNARY_BIT_NOT;
        } else if (strcmp(kind, "LOG_NOT") == 0) {
            op = JZ_UNARY_LOGICAL_NOT;
            rule = "LOGICAL_WIDTH_NOT_1";
        } else {
            return; /* unknown unary kind */
        }

        /* Enforce required parentheses for unary arithmetic operators. */
        if (is_unary_arith && diagnostics) {
            if (!sem_unary_has_required_parens(expr)) {
                sem_report_rule(diagnostics,
                                expr->loc,
                                "UNARY_ARITH_MISSING_PARENS",
                                "unary '+'/'-' must be written with parentheses, e.g. '(-flag)'");
            }
        }

        if (jz_type_unary(op, &operand, out) != 0) {
            if (diagnostics && rule) {
                const char *msg = (strcmp(rule, "LOGICAL_WIDTH_NOT_1") == 0)
                    ? "logical operator operand width must be 1"
                    : "unary arithmetic operator requires width-1 operand";
                sem_report_rule(diagnostics,
                                expr->loc,
                                rule,
                                msg);
            }
            out->width = 0;
            out->is_signed = 0;
        }
        return;
    }

    case JZ_AST_EXPR_BINARY: {
        if (expr->child_count < 2) return;
        JZBitvecType lhs, rhs;
        infer_expr_type(expr->children[0], mod_scope, project_symbols, diagnostics, &lhs);
        infer_expr_type(expr->children[1], mod_scope, project_symbols, diagnostics, &rhs);
        if (lhs.width == 0 || rhs.width == 0) return;

        const char *kind = expr->block_kind ? expr->block_kind : "";
        JZBinaryOp op;
        const char *rule = NULL;

        if (strcmp(kind, "ADD") == 0) op = JZ_BIN_ADD, rule = "TYPE_BINOP_WIDTH_MISMATCH";
        else if (strcmp(kind, "SUB") == 0) op = JZ_BIN_SUB, rule = "TYPE_BINOP_WIDTH_MISMATCH";
        else if (strcmp(kind, "MUL") == 0) op = JZ_BIN_MUL, rule = "TYPE_BINOP_WIDTH_MISMATCH";
        else if (strcmp(kind, "DIV") == 0) op = JZ_BIN_DIV, rule = "TYPE_BINOP_WIDTH_MISMATCH";
        else if (strcmp(kind, "MOD") == 0) op = JZ_BIN_MOD, rule = "TYPE_BINOP_WIDTH_MISMATCH";
        else if (strcmp(kind, "BIT_AND") == 0) op = JZ_BIN_BIT_AND, rule = "TYPE_BINOP_WIDTH_MISMATCH";
        else if (strcmp(kind, "BIT_OR") == 0) op = JZ_BIN_BIT_OR, rule = "TYPE_BINOP_WIDTH_MISMATCH";
        else if (strcmp(kind, "BIT_XOR") == 0) op = JZ_BIN_BIT_XOR, rule = "TYPE_BINOP_WIDTH_MISMATCH";
        else if (strcmp(kind, "LOG_AND") == 0) op = JZ_BIN_LOG_AND, rule = "LOGICAL_WIDTH_NOT_1";
        else if (strcmp(kind, "LOG_OR") == 0) op = JZ_BIN_LOG_OR, rule = "LOGICAL_WIDTH_NOT_1";
        else if (strcmp(kind, "EQ") == 0) op = JZ_BIN_EQ, rule = "TYPE_BINOP_WIDTH_MISMATCH";
        else if (strcmp(kind, "NEQ") == 0) op = JZ_BIN_NE, rule = "TYPE_BINOP_WIDTH_MISMATCH";
        else if (strcmp(kind, "LT") == 0) op = JZ_BIN_LT, rule = "TYPE_BINOP_WIDTH_MISMATCH";
        else if (strcmp(kind, "LE") == 0) op = JZ_BIN_LE, rule = "TYPE_BINOP_WIDTH_MISMATCH";
        else if (strcmp(kind, "GT") == 0) op = JZ_BIN_GT, rule = "TYPE_BINOP_WIDTH_MISMATCH";
        else if (strcmp(kind, "GE") == 0) op = JZ_BIN_GE, rule = "TYPE_BINOP_WIDTH_MISMATCH";
        else if (strcmp(kind, "SHL") == 0) op = JZ_BIN_SHL, rule = NULL;
        else if (strcmp(kind, "SHR") == 0) op = JZ_BIN_SHR, rule = NULL;
        else if (strcmp(kind, "ASHR") == 0) op = JZ_BIN_ASHR, rule = NULL;
        else return; /* unknown binary kind */

        /* Division/modulus by constant zero (compile-time check only).
         * Runtime zero detection is handled by the IR-level div guard pass
         * (jz_ir_div_guard_check) which can prove divisor-nonzero through
         * enclosing IF guards.
         */
        if ((op == JZ_BIN_DIV || op == JZ_BIN_MOD) && expr->child_count >= 2 && diagnostics) {
            JZASTNode *divisor = expr->children[1];
            if (divisor && divisor->type == JZ_AST_EXPR_LITERAL && divisor->text) {
                int known = 0;
                if (sem_literal_is_const_zero(divisor->text, &known)) {
                    sem_report_rule(diagnostics,
                                    expr->loc,
                                    "DIV_CONST_ZERO",
                                    "division or modulus by compile-time constant zero divisor");
                }
            }
        }

        if (jz_type_binary(op, &lhs, &rhs, out) != 0) {
            if (diagnostics && rule) {
                const char *msg;
                if (strcmp(rule, "LOGICAL_WIDTH_NOT_1") == 0) {
                    msg = "logical operators require width-1 operands";
                } else {
                    msg = "binary operator requires equal operand widths";
                }
                sem_report_rule(diagnostics,
                                expr->loc,
                                rule,
                                msg);
            }
            out->width = 0;
            out->is_signed = 0;
        }
        return;
    }

    case JZ_AST_EXPR_TERNARY: {
        if (expr->child_count < 3) return;
        JZBitvecType cond_t, t_t, f_t;
        infer_expr_type(expr->children[0], mod_scope, project_symbols, diagnostics, &cond_t);
        infer_expr_type(expr->children[1], mod_scope, project_symbols, diagnostics, &t_t);
        infer_expr_type(expr->children[2], mod_scope, project_symbols, diagnostics, &f_t);

        int cond_ok = (cond_t.width == 1);

        /* Handle GND/VCC polymorphic branches: if one branch has width 0
         * (polymorphic special driver), use the other branch's width.
         */
        int t_is_polymorphic = (t_t.width == 0 &&
                                expr->children[1]->type == JZ_AST_EXPR_SPECIAL_DRIVER);
        int f_is_polymorphic = (f_t.width == 0 &&
                                expr->children[2]->type == JZ_AST_EXPR_SPECIAL_DRIVER);

        unsigned result_width = 0;
        int branches_ok = 0;
        if (t_is_polymorphic && f_is_polymorphic) {
            /* Both branches are GND/VCC - result is polymorphic */
            branches_ok = 1;
            result_width = 0;
        } else if (t_is_polymorphic && f_t.width > 0) {
            /* True branch is GND/VCC - use false branch width */
            branches_ok = 1;
            result_width = f_t.width;
        } else if (f_is_polymorphic && t_t.width > 0) {
            /* False branch is GND/VCC - use true branch width */
            branches_ok = 1;
            result_width = t_t.width;
        } else if (t_t.width > 0 && f_t.width > 0 && t_t.width == f_t.width) {
            /* Normal case: both branches have matching non-zero widths */
            branches_ok = 1;
            result_width = t_t.width;
        }

        if (diagnostics) {
            if (cond_t.width > 0 && cond_t.width != 1) {
                sem_report_rule(diagnostics,
                                expr->children[0]->loc,
                                "TERNARY_COND_WIDTH_NOT_1",
                                "ternary condition expression width must be 1");
            }
            if (t_t.width > 0 && f_t.width > 0 && t_t.width != f_t.width &&
                !t_is_polymorphic && !f_is_polymorphic) {
                sem_report_rule(diagnostics,
                                expr->loc,
                                "TERNARY_BRANCH_WIDTH_MISMATCH",
                                "ternary branches must have matching widths");
            }
        }

        if (cond_ok && branches_ok) {
            out->width = result_width;
            out->is_signed = (t_t.is_signed || f_t.is_signed) ? 1 : 0;
        }
        return;
    }

    case JZ_AST_EXPR_CONCAT:
        /* For now, concatenation is only used to propagate widths; width
         * mismatches are enforced at assignment sites in later passes. */
        if (expr->child_count == 0) {
            if (diagnostics) {
                sem_report_rule(diagnostics,
                                expr->loc,
                                "CONCAT_EMPTY",
                                "empty concatenation '{}' is not allowed");
            }
            return;
        } else {
            JZBitvecType elems[16];
            size_t count = expr->child_count;
            if (count > sizeof(elems) / sizeof(elems[0])) count = sizeof(elems) / sizeof(elems[0]);
            size_t actual = 0;
            for (size_t i = 0; i < count; ++i) {
                JZBitvecType tmp;
                infer_expr_type(expr->children[i], mod_scope, project_symbols, diagnostics, &tmp);
                if (tmp.width == 0) {
                    return; /* unknown width for now */
                }
                elems[actual++] = tmp;
            }
            if (actual > 0) {
                jz_type_concat(elems, actual, out);
            }
        }
        return;

    case JZ_AST_EXPR_BUILTIN_CALL: {
        if (!expr->name) return;
        const char *fname = expr->name;

        if (strcmp(fname, "lit") == 0) {
            if (expr->child_count < 2) return;
            long long width_val = 0;
            if (sem_lit_eval_const_expr(expr->children[0],
                                        mod_scope,
                                        project_symbols,
                                        diagnostics,
                                        &width_val) != 0 ||
                width_val <= 0) {
                if (diagnostics) {
                    sem_report_rule(diagnostics,
                                    expr->loc,
                                    "LIT_WIDTH_INVALID",
                                    "lit() width must be a positive integer constant expression");
                }
                return;
            }

            if (width_val > (long long)UINT_MAX) {
                if (diagnostics) {
                    sem_report_rule(diagnostics,
                                    expr->loc,
                                    "LIT_WIDTH_INVALID",
                                    "lit() width must be a positive integer constant expression");
                }
                return;
            }

            long long value_val = 0;
            if (sem_lit_eval_const_expr(expr->children[1],
                                        mod_scope,
                                        project_symbols,
                                        diagnostics,
                                        &value_val) != 0 ||
                value_val < 0) {
                if (diagnostics) {
                    sem_report_rule(diagnostics,
                                    expr->loc,
                                    "LIT_VALUE_INVALID",
                                    "lit() value must be a nonnegative integer constant expression");
                }
                return;
            }

            if (width_val < 64) {
                unsigned long long limit = 1ULL << (unsigned)width_val;
                if ((unsigned long long)value_val >= limit) {
                    if (diagnostics) {
                        sem_report_rule(diagnostics,
                                        expr->loc,
                                        "LIT_VALUE_OVERFLOW",
                                        "lit() value exceeds declared width");
                    }
                    return;
                }
            }

            jz_type_scalar((unsigned)width_val, 0, out);
            return;
        }

        /* clog2() is restricted to compile-time constant integer contexts;
         * any appearance as a runtime expression is a context error. */
        if (strcmp(fname, "clog2") == 0) {
            if (diagnostics) {
                sem_report_rule(diagnostics,
                                expr->loc,
                                "CLOG2_INVALID_CONTEXT",
                                "clog2() may only be used in compile-time constant integer expressions (widths, depths, CONST/CONFIG)");
            }
            return;
        }

        /* widthof() is also restricted to compile-time constant integer
         * contexts (widths, depths, CONST/CONFIG, MEM attributes, OVERRIDE).
         * Any appearance in executable ASYNCHRONOUS/SYNCHRONOUS expressions is
         * a context error; widthof() is not a runtime value. */
        if (strcmp(fname, "widthof") == 0) {
            if (diagnostics) {
                sem_report_rule(diagnostics,
                                expr->loc,
                                "WIDTHOF_INVALID_CONTEXT",
                                "widthof() may only be used in compile-time constant integer expressions (widths, depths, CONST/CONFIG, MEM attributes, OVERRIDE)");
            }
            return;
        }

        /* Arithmetic builtins: uadd/sadd/umul/smul. */
        if (strcmp(fname, "uadd") == 0 || strcmp(fname, "sadd") == 0 ||
            strcmp(fname, "umul") == 0 || strcmp(fname, "smul") == 0) {
            if (expr->child_count < 2) return;
            JZBitvecType a_t, b_t;
            infer_expr_type(expr->children[0], mod_scope, project_symbols, diagnostics, &a_t);
            infer_expr_type(expr->children[1], mod_scope, project_symbols, diagnostics, &b_t);
            if (a_t.width == 0 || b_t.width == 0) return;
            unsigned max_bits = (a_t.width > b_t.width) ? a_t.width : b_t.width;
            if (max_bits == 0) return;

            if (strcmp(fname, "uadd") == 0 || strcmp(fname, "sadd") == 0) {
                /* Result width = max_bits + 1, capturing carry. */
                jz_type_scalar(max_bits + 1u,
                               (strcmp(fname, "sadd") == 0) ? 1 : 0,
                               out);
            } else {
                /* umul/smul: result width = 2 * max_bits. */
                if (max_bits > 0 && max_bits <= (unsigned)(~0u) / 2u) {
                    jz_type_scalar(max_bits * 2u,
                                   (strcmp(fname, "smul") == 0) ? 1 : 0,
                                   out);
                }
            }
            return;
        }

        /* Bit-level builtins: gbit(source, index), sbit(source, index, set). */
        if (strcmp(fname, "gbit") == 0) {
            if (expr->child_count < 2) return;
            JZBitvecType src_t, idx_t;
            infer_expr_type(expr->children[0], mod_scope, project_symbols, diagnostics, &src_t);
            infer_expr_type(expr->children[1], mod_scope, project_symbols, diagnostics, &idx_t);
            if (src_t.width == 0) return;

            /* GBIT_INDEX_OUT_OF_RANGE: const-eval index and compare against src width. */
            if (diagnostics && expr->children[1] &&
                expr->children[1]->type == JZ_AST_EXPR_LITERAL &&
                expr->children[1]->text) {
                unsigned idx_val = 0;
                if (parse_simple_nonnegative_int(expr->children[1]->text, &idx_val) &&
                    idx_val >= src_t.width) {
                    sem_report_rule(diagnostics,
                                    expr->children[1]->loc,
                                    "GBIT_INDEX_OUT_OF_RANGE",
                                    "gbit() index is out of range for source width");
                }
            }

            /* gbit always returns a 1-bit unsigned value. */
            jz_type_scalar(1u, 0, out);
            return;
        }

        if (strcmp(fname, "sbit") == 0) {
            if (expr->child_count < 3) return;
            JZBitvecType src_t, idx_t, set_t;
            infer_expr_type(expr->children[0], mod_scope, project_symbols, diagnostics, &src_t);
            infer_expr_type(expr->children[1], mod_scope, project_symbols, diagnostics, &idx_t);
            infer_expr_type(expr->children[2], mod_scope, project_symbols, diagnostics, &set_t);

            if (src_t.width == 0) return;

            /* SBIT_INDEX_OUT_OF_RANGE: const-eval index and compare against src width. */
            if (diagnostics && expr->children[1] &&
                expr->children[1]->type == JZ_AST_EXPR_LITERAL &&
                expr->children[1]->text) {
                unsigned idx_val = 0;
                if (parse_simple_nonnegative_int(expr->children[1]->text, &idx_val) &&
                    idx_val >= src_t.width) {
                    sem_report_rule(diagnostics,
                                    expr->children[1]->loc,
                                    "SBIT_INDEX_OUT_OF_RANGE",
                                    "sbit() index is out of range for source width");
                }
            }

            /* set must be width-1 per spec. */
            if (set_t.width != 0 && set_t.width != 1 && diagnostics) {
                sem_report_rule(diagnostics,
                                expr->children[2]->loc,
                                "SBIT_SET_WIDTH_NOT_1",
                                "sbit() third argument (set) must be a width-1 expression");
            }

            /* Result width matches the source width and signedness. */
            jz_type_scalar(src_t.width, src_t.is_signed, out);
            return;
        }

        /* Slice builtins: gslice(source, index, width), sslice(source, index, width, value). */
        if (strcmp(fname, "gslice") == 0) {
            /* gslice(source, index, width) */
            if (expr->child_count >= 3 && diagnostics) {
                JZBitvecType src_t;
                infer_expr_type(expr->children[0], mod_scope, project_symbols, diagnostics, &src_t);

                /* GSLICE_WIDTH_INVALID: width param must be positive integer. */
                if (expr->children[2] &&
                    expr->children[2]->type == JZ_AST_EXPR_LITERAL &&
                    expr->children[2]->text) {
                    unsigned w_val = 0;
                    if (!parse_simple_nonnegative_int(expr->children[2]->text, &w_val) ||
                        w_val == 0) {
                        sem_report_rule(diagnostics,
                                        expr->children[2]->loc,
                                        "GSLICE_WIDTH_INVALID",
                                        "gslice() width parameter must be a positive integer");
                    }
                }

                /* GSLICE_INDEX_OUT_OF_RANGE: const-eval index vs src width. */
                if (src_t.width > 0 &&
                    expr->children[1] &&
                    expr->children[1]->type == JZ_AST_EXPR_LITERAL &&
                    expr->children[1]->text) {
                    unsigned idx_val = 0;
                    if (parse_simple_nonnegative_int(expr->children[1]->text, &idx_val) &&
                        idx_val >= src_t.width) {
                        sem_report_rule(diagnostics,
                                        expr->children[1]->loc,
                                        "GSLICE_INDEX_OUT_OF_RANGE",
                                        "gslice() index is out of range for source width");
                    }
                }
            }
            /* Result width is the compile-time constant `width` parameter. That
             * integer is not yet wired through the expression-type layer, so we
             * intentionally leave `out` as unknown-width here and let later
             * constant-eval based passes refine it.
             */
            return;
        }

        if (strcmp(fname, "sslice") == 0) {
            if (expr->child_count < 4) return;
            JZBitvecType src_t;
            infer_expr_type(expr->children[0], mod_scope, project_symbols, diagnostics, &src_t);
            if (src_t.width == 0) return;

            if (diagnostics) {
                /* SSLICE_WIDTH_INVALID: width param must be positive integer. */
                unsigned w_val = 0;
                int w_known = 0;
                if (expr->children[2] &&
                    expr->children[2]->type == JZ_AST_EXPR_LITERAL &&
                    expr->children[2]->text) {
                    if (!parse_simple_nonnegative_int(expr->children[2]->text, &w_val) ||
                        w_val == 0) {
                        sem_report_rule(diagnostics,
                                        expr->children[2]->loc,
                                        "SSLICE_WIDTH_INVALID",
                                        "sslice() width parameter must be a positive integer");
                    } else {
                        w_known = 1;
                    }
                }

                /* SSLICE_INDEX_OUT_OF_RANGE: const-eval index vs src width. */
                if (expr->children[1] &&
                    expr->children[1]->type == JZ_AST_EXPR_LITERAL &&
                    expr->children[1]->text) {
                    unsigned idx_val = 0;
                    if (parse_simple_nonnegative_int(expr->children[1]->text, &idx_val) &&
                        idx_val >= src_t.width) {
                        sem_report_rule(diagnostics,
                                        expr->children[1]->loc,
                                        "SSLICE_INDEX_OUT_OF_RANGE",
                                        "sslice() index is out of range for source width");
                    }
                }

                /* SSLICE_VALUE_WIDTH_MISMATCH: value width must match width param. */
                if (w_known && w_val > 0) {
                    JZBitvecType val_t;
                    infer_expr_type(expr->children[3], mod_scope, project_symbols, diagnostics, &val_t);
                    if (val_t.width > 0 && val_t.width != w_val) {
                        sem_report_rule(diagnostics,
                                        expr->children[3]->loc,
                                        "SSLICE_VALUE_WIDTH_MISMATCH",
                                        "sslice() value argument width does not match width parameter");
                    }
                }
            }

            jz_type_scalar(src_t.width, src_t.is_signed, out);
            return;
        }

        /* oh2b(source) — one-hot to binary encoder. */
        if (strcmp(fname, "oh2b") == 0) {
            if (expr->child_count < 1) return;
            JZBitvecType src_t;
            infer_expr_type(expr->children[0], mod_scope, project_symbols, diagnostics, &src_t);
            if (src_t.width < 2 && diagnostics) {
                sem_report_rule(diagnostics,
                                expr->loc,
                                "OH2B_INPUT_TOO_NARROW",
                                "oh2b() source must be at least 2 bits wide");
                return;
            }
            /* Result width = clog2(source width). */
            unsigned result_width = 0;
            {
                unsigned temp = src_t.width - 1;
                while (temp > 0) { result_width++; temp >>= 1; }
                if (src_t.width == 1) result_width = 1;
            }
            jz_type_scalar(result_width, 0, out);
            return;
        }

        /* b2oh(index, width) — binary to one-hot decoder. */
        if (strcmp(fname, "b2oh") == 0) {
            if (expr->child_count < 2) return;
            /* First arg is index expression — infer its type for width checks. */
            JZBitvecType idx_t;
            infer_expr_type(expr->children[0], mod_scope, project_symbols, diagnostics, &idx_t);
            /* Second arg is width — must be compile-time constant >= 2. */
            unsigned w_val = 0;
            int w_ok = 0;
            if (expr->children[1] && expr->children[1]->text) {
                w_ok = parse_simple_nonnegative_int(expr->children[1]->text, &w_val);
            }
            if (!w_ok || w_val < 2) {
                /* Try const eval from identifier name */
                if (mod_scope && expr->children[1] && expr->children[1]->name) {
                    long long cval = 0;
                    int rc = sem_eval_const_expr_in_module(expr->children[1]->name,
                                                           mod_scope, project_symbols, &cval);
                    if (rc == 0 && cval >= 2) { w_val = (unsigned)cval; w_ok = 1; }
                }
            }
            if (!w_ok || w_val < 2) {
                if (diagnostics) {
                    sem_report_rule(diagnostics,
                                    expr->loc,
                                    "B2OH_WIDTH_INVALID",
                                    "b2oh() width must be a positive constant >= 2");
                }
                return;
            }
            jz_type_scalar(w_val, 0, out);
            return;
        }

        /* prienc(source) — priority encoder (MSB-first). */
        if (strcmp(fname, "prienc") == 0) {
            if (expr->child_count < 1) return;
            JZBitvecType src_t;
            infer_expr_type(expr->children[0], mod_scope, project_symbols, diagnostics, &src_t);
            if (src_t.width < 2 && diagnostics) {
                sem_report_rule(diagnostics,
                                expr->loc,
                                "PRIENC_INPUT_TOO_NARROW",
                                "prienc() source must be at least 2 bits wide");
                return;
            }
            /* Result width = clog2(source width). */
            unsigned result_width = 0;
            {
                unsigned temp = src_t.width - 1;
                while (temp > 0) { result_width++; temp >>= 1; }
                if (src_t.width == 1) result_width = 1;
            }
            jz_type_scalar(result_width, 0, out);
            return;
        }

        /* lzc(source) — leading zero count. */
        if (strcmp(fname, "lzc") == 0) {
            if (expr->child_count < 1) return;
            JZBitvecType src_t;
            infer_expr_type(expr->children[0], mod_scope, project_symbols, diagnostics, &src_t);
            if (src_t.width == 0) return;
            /* Result width = clog2(source_width + 1) because count ranges 0..width. */
            unsigned result_width = 0;
            {
                unsigned temp = src_t.width;
                while (temp > 0) { result_width++; temp >>= 1; }
            }
            jz_type_scalar(result_width, 0, out);
            return;
        }

        /* usub/ssub — unsigned/signed widening subtract. */
        if (strcmp(fname, "usub") == 0 || strcmp(fname, "ssub") == 0) {
            if (expr->child_count < 2) return;
            JZBitvecType a_t, b_t;
            infer_expr_type(expr->children[0], mod_scope, project_symbols, diagnostics, &a_t);
            infer_expr_type(expr->children[1], mod_scope, project_symbols, diagnostics, &b_t);
            if (a_t.width == 0 || b_t.width == 0) return;
            unsigned max_bits = (a_t.width > b_t.width) ? a_t.width : b_t.width;
            jz_type_scalar(max_bits + 1u,
                           (strcmp(fname, "ssub") == 0) ? 1 : 0,
                           out);
            return;
        }

        /* abs(a) — signed absolute value. */
        if (strcmp(fname, "abs") == 0) {
            if (expr->child_count < 1) return;
            JZBitvecType src_t;
            infer_expr_type(expr->children[0], mod_scope, project_symbols, diagnostics, &src_t);
            if (src_t.width == 0) return;
            /* Result width = width(a) + 1 (MSB = overflow). */
            jz_type_scalar(src_t.width + 1u, 0, out);
            return;
        }

        /* umin/umax — unsigned min/max. */
        if (strcmp(fname, "umin") == 0 || strcmp(fname, "umax") == 0) {
            if (expr->child_count < 2) return;
            JZBitvecType a_t, b_t;
            infer_expr_type(expr->children[0], mod_scope, project_symbols, diagnostics, &a_t);
            infer_expr_type(expr->children[1], mod_scope, project_symbols, diagnostics, &b_t);
            if (a_t.width == 0 || b_t.width == 0) return;
            unsigned max_bits = (a_t.width > b_t.width) ? a_t.width : b_t.width;
            jz_type_scalar(max_bits, 0, out);
            return;
        }

        /* smin/smax — signed min/max. */
        if (strcmp(fname, "smin") == 0 || strcmp(fname, "smax") == 0) {
            if (expr->child_count < 2) return;
            JZBitvecType a_t, b_t;
            infer_expr_type(expr->children[0], mod_scope, project_symbols, diagnostics, &a_t);
            infer_expr_type(expr->children[1], mod_scope, project_symbols, diagnostics, &b_t);
            if (a_t.width == 0 || b_t.width == 0) return;
            unsigned max_bits = (a_t.width > b_t.width) ? a_t.width : b_t.width;
            jz_type_scalar(max_bits, 1, out);
            return;
        }

        /* popcount(source) — population count. */
        if (strcmp(fname, "popcount") == 0) {
            if (expr->child_count < 1) return;
            JZBitvecType src_t;
            infer_expr_type(expr->children[0], mod_scope, project_symbols, diagnostics, &src_t);
            if (src_t.width == 0) return;
            /* Result width = clog2(source_width + 1). */
            unsigned result_width = 0;
            {
                unsigned temp = src_t.width;
                while (temp > 0) { result_width++; temp >>= 1; }
            }
            jz_type_scalar(result_width, 0, out);
            return;
        }

        /* reverse(source) — bit reversal. */
        if (strcmp(fname, "reverse") == 0) {
            if (expr->child_count < 1) return;
            JZBitvecType src_t;
            infer_expr_type(expr->children[0], mod_scope, project_symbols, diagnostics, &src_t);
            if (src_t.width == 0) return;
            jz_type_scalar(src_t.width, 0, out);
            return;
        }

        /* bswap(source) — byte swap. */
        if (strcmp(fname, "bswap") == 0) {
            if (expr->child_count < 1) return;
            JZBitvecType src_t;
            infer_expr_type(expr->children[0], mod_scope, project_symbols, diagnostics, &src_t);
            if (src_t.width == 0) return;
            if ((src_t.width % 8) != 0 && diagnostics) {
                sem_report_rule(diagnostics,
                                expr->loc,
                                "BSWAP_WIDTH_NOT_BYTE_ALIGNED",
                                "bswap() source width must be a multiple of 8");
                return;
            }
            jz_type_scalar(src_t.width, 0, out);
            return;
        }

        /* reduce_and/reduce_or/reduce_xor — reduction operators. */
        if (strcmp(fname, "reduce_and") == 0 ||
            strcmp(fname, "reduce_or") == 0 ||
            strcmp(fname, "reduce_xor") == 0) {
            if (expr->child_count < 1) return;
            JZBitvecType src_t;
            infer_expr_type(expr->children[0], mod_scope, project_symbols, diagnostics, &src_t);
            if (src_t.width == 0) return;
            jz_type_scalar(1, 0, out);
            return;
        }

        /* Unknown builtin: leave width unresolved for now. */
        return;
    }

    case JZ_AST_EXPR_SLICE: {
        /* Compute width for literal-index range slices [msb:lsb], but skip
         * MEM port slices where the index denotes an address rather than a
         * bit-select (Section 7.3), and MUX slices where the index selects
         * an entry rather than a bit range (Section 4.6).  We also skip
         * slices whose base is a qualified identifier with a MEM-name
         * prefix (e.g. m.unknown[0]) even when the port name is invalid,
         * so that width inference does not misinterpret the address index
         * as a bit-select range.
         */
        int skip_slice_width = 0;
        if (mod_scope && expr->child_count >= 1) {
            JZASTNode *base = expr->children[0];
            /* Exact MEM port match (valid port name). */
            JZMemPortRef mem_ref;
            memset(&mem_ref, 0, sizeof(mem_ref));
            if (sem_match_mem_port_slice(expr, mod_scope, NULL, &mem_ref)) {
                skip_slice_width = 1;
            }
            /* Catch qualified identifiers whose prefix is a MEM name,
             * even when the port portion is invalid/undefined. */
            if (!skip_slice_width && base &&
                base->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER && base->name) {
                const char *dot = strchr(base->name, '.');
                if (dot && dot != base->name) {
                    char prefix[256];
                    size_t plen = (size_t)(dot - base->name);
                    if (plen > 0 && plen < sizeof(prefix)) {
                        memcpy(prefix, base->name, plen);
                        prefix[plen] = '\0';
                        if (module_scope_lookup_kind(mod_scope, prefix, JZ_SYM_MEM)) {
                            skip_slice_width = 1;
                        }
                    }
                }
            }
            /* MUX selectors: index selects an entry, not a bit range. */
            if (!skip_slice_width && base &&
                base->type == JZ_AST_EXPR_IDENTIFIER && base->name) {
                if (module_scope_lookup_kind(mod_scope, base->name, JZ_SYM_MUX)) {
                    skip_slice_width = 1;
                }
            }
        }
        if (!skip_slice_width) {
            unsigned slice_w = 0;
            if (sem_slice_literal_width(expr, &slice_w)) {
                jz_type_scalar(slice_w, 0, out);
            }
        }
        return;
    }

    default:
        /* BUILTIN_CALL and any other expression forms currently yield unknown
         * width; future semantic passes will refine these. */
        return;
    }
}
