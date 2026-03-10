/**
 * @file parser_simulation.c
 * @brief Parsing of @simulation blocks.
 *
 * Handles the simulation-specific grammar including auto-toggling CLOCK
 * declarations (with periods), TAP blocks for internal signal monitoring,
 * and @run directives for time-based advancement. Reuses WIRE, @setup,
 * @update, @new, and @import parsing from testbench infrastructure.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser_internal.h"

/* Forward declarations for TB helpers we reuse */
extern int parse_tb_wire_block_body_impl(Parser *p, JZASTNode *parent);

/* ---------- helpers ---------- */

/**
 * @brief Parse a simulation CLOCK block body.
 *
 * CLOCK {
 *     <id> = { period=<float> };
 *     ...
 * }
 *
 * Each child is a SIM_CLOCK_DECL node with name=clock_id, text=period string.
 */
static int parse_sim_clock_block_body(Parser *p, JZASTNode *parent)
{
    for (;;) {
        const JZToken *t = peek(p);
        if (t->type == JZ_TOK_RBRACE) {
            advance(p);
            return 0;
        }
        if (t->type == JZ_TOK_EOF) {
            parser_error(p, "unterminated simulation CLOCK block (missing '}')");
            return -1;
        }

        if (!is_decl_identifier_token(t)) {
            parser_error(p, "expected clock identifier in simulation CLOCK block");
            return -1;
        }

        const char *clock_name = t->lexeme;
        JZLocation decl_loc = t->loc;
        advance(p);

        /* = */
        if (!match(p, JZ_TOK_OP_ASSIGN)) {
            parser_error(p, "expected '=' after clock identifier in simulation CLOCK block");
            return -1;
        }

        /* { */
        if (!match(p, JZ_TOK_LBRACE)) {
            parser_error(p, "expected '{' after '=' in simulation CLOCK declaration");
            return -1;
        }

        /* period=<value> */
        const JZToken *param = peek(p);
        if (!param->lexeme || strcmp(param->lexeme, "period") != 0) {
            parser_error(p, "expected 'period=' in simulation CLOCK declaration");
            return -1;
        }
        advance(p);

        if (!match(p, JZ_TOK_OP_ASSIGN)) {
            parser_error(p, "expected '=' after 'period' in simulation CLOCK declaration");
            return -1;
        }

        /* Collect period value tokens until '}' */
        size_t val_start = p->pos;
        while (peek(p)->type != JZ_TOK_EOF &&
               peek(p)->type != JZ_TOK_RBRACE) {
            advance(p);
        }
        size_t val_end = p->pos;

        if (!match(p, JZ_TOK_RBRACE)) {
            parser_error(p, "expected '}' after period value in simulation CLOCK declaration");
            return -1;
        }

        /* Build period text */
        char *period_text = NULL;
        if (val_start < val_end) {
            size_t buf_sz = 0;
            for (size_t i = val_start; i < val_end; ++i) {
                const JZToken *vt = &p->tokens[i];
                if (vt->lexeme) buf_sz += strlen(vt->lexeme) + 1;
            }
            if (buf_sz > 0) {
                period_text = (char *)malloc(buf_sz + 1);
                if (!period_text) return -1;
                period_text[0] = '\0';
                for (size_t i = val_start; i < val_end; ++i) {
                    const JZToken *vt = &p->tokens[i];
                    if (!vt->lexeme) continue;
                    strcat(period_text, vt->lexeme);
                }
            }
        }

        /* ; */
        if (!match(p, JZ_TOK_SEMICOLON)) {
            free(period_text);
            parser_error(p, "expected ';' after simulation CLOCK declaration");
            return -1;
        }

        JZASTNode *decl = jz_ast_new(JZ_AST_SIM_CLOCK_DECL, decl_loc);
        if (!decl) {
            free(period_text);
            return -1;
        }
        jz_ast_set_name(decl, clock_name);
        if (period_text) {
            jz_ast_set_text(decl, period_text);
            free(period_text);
        }

        if (jz_ast_add_child(parent, decl) != 0) {
            jz_ast_free(decl);
            return -1;
        }
    }
}

/**
 * @brief Parse a TAP block body.
 *
 * TAP {
 *     <instance>.<signal>;
 *     <instance>.<sub>.<signal>;
 *     ...
 * }
 */
static int parse_sim_tap_block_body(Parser *p, JZASTNode *parent)
{
    for (;;) {
        const JZToken *t = peek(p);
        if (t->type == JZ_TOK_RBRACE) {
            advance(p);
            return 0;
        }
        if (t->type == JZ_TOK_EOF) {
            parser_error(p, "unterminated TAP block (missing '}')");
            return -1;
        }

        /* Collect hierarchical path tokens: id.id.id... until ; */
        if (!is_decl_identifier_token(t)) {
            parser_error(p, "expected signal path in TAP block");
            return -1;
        }

        JZLocation tap_loc = t->loc;
        size_t path_start = p->pos;
        advance(p);

        while (peek(p)->type == JZ_TOK_DOT) {
            advance(p); /* consume . */
            if (!is_decl_identifier_token(peek(p))) {
                parser_error(p, "expected identifier after '.' in TAP path");
                return -1;
            }
            advance(p);
        }
        size_t path_end = p->pos;

        if (!match(p, JZ_TOK_SEMICOLON)) {
            parser_error(p, "expected ';' after TAP signal path");
            return -1;
        }

        /* Build path text */
        size_t buf_sz = 0;
        for (size_t i = path_start; i < path_end; ++i) {
            const JZToken *pt = &p->tokens[i];
            if (pt->lexeme) buf_sz += strlen(pt->lexeme);
        }
        char *path_text = (char *)malloc(buf_sz + 1);
        if (!path_text) return -1;
        path_text[0] = '\0';
        for (size_t i = path_start; i < path_end; ++i) {
            const JZToken *pt = &p->tokens[i];
            if (pt->lexeme) strcat(path_text, pt->lexeme);
        }

        JZASTNode *decl = jz_ast_new(JZ_AST_SIM_TAP_DECL, tap_loc);
        if (!decl) {
            free(path_text);
            return -1;
        }
        jz_ast_set_name(decl, path_text);
        free(path_text);

        if (jz_ast_add_child(parent, decl) != 0) {
            jz_ast_free(decl);
            return -1;
        }
    }
}

/**
 * @brief Parse @run(unit=value) directive.
 *
 * Supports:
 *   @run(ticks=N)
 *   @run(ns=N)
 *   @run(ms=N)
 *
 * Returns a SIM_RUN node with text=unit, name=value.
 */
static JZASTNode *parse_sim_run(Parser *p)
{
    const JZToken *kw = &p->tokens[p->pos - 1]; /* @run already consumed */

    if (!match(p, JZ_TOK_LPAREN)) {
        parser_error(p, "expected '(' after @run");
        return NULL;
    }

    /* Parse unit keyword: ticks, ns, ms */
    const JZToken *unit_tok = peek(p);
    if (!unit_tok->lexeme ||
        (strcmp(unit_tok->lexeme, "ticks") != 0 &&
         strcmp(unit_tok->lexeme, "ns") != 0 &&
         strcmp(unit_tok->lexeme, "ms") != 0)) {
        parser_error(p, "expected 'ticks=', 'ns=', or 'ms=' in @run directive");
        return NULL;
    }
    const char *unit = unit_tok->lexeme;
    advance(p);

    if (!match(p, JZ_TOK_OP_ASSIGN)) {
        parser_error(p, "expected '=' after unit in @run directive");
        return NULL;
    }

    /* Collect value tokens until ')' */
    size_t val_start = p->pos;
    while (peek(p)->type != JZ_TOK_EOF &&
           peek(p)->type != JZ_TOK_RPAREN) {
        advance(p);
    }
    size_t val_end = p->pos;

    if (!match(p, JZ_TOK_RPAREN)) {
        parser_error(p, "expected ')' after @run directive");
        return NULL;
    }

    /* Build value text */
    char *val_text = NULL;
    if (val_start < val_end) {
        size_t buf_sz = 0;
        for (size_t i = val_start; i < val_end; ++i) {
            const JZToken *vt = &p->tokens[i];
            if (vt->lexeme) buf_sz += strlen(vt->lexeme) + 1;
        }
        if (buf_sz > 0) {
            val_text = (char *)malloc(buf_sz + 1);
            if (!val_text) return NULL;
            val_text[0] = '\0';
            for (size_t i = val_start; i < val_end; ++i) {
                const JZToken *vt = &p->tokens[i];
                if (!vt->lexeme) continue;
                strcat(val_text, vt->lexeme);
            }
        }
    }

    JZASTNode *node = jz_ast_new(JZ_AST_SIM_RUN, kw->loc);
    if (!node) {
        free(val_text);
        return NULL;
    }
    jz_ast_set_text(node, unit);
    if (val_text) {
        jz_ast_set_name(node, val_text);
        free(val_text);
    }

    return node;
}

/**
 * @brief Parse @print or @print_if directive.
 *
 * @print("format", arg1, arg2, ...)
 * @print_if(condition, "format", arg1, arg2, ...)
 */
JZASTNode *parse_print_directive(Parser *p, int is_print_if)
{
    const JZToken *kw = &p->tokens[p->pos - 1]; /* keyword already consumed */
    JZASTNodeType node_type = is_print_if ? JZ_AST_PRINT_IF : JZ_AST_PRINT;

    if (!match(p, JZ_TOK_LPAREN)) {
        parser_error(p, is_print_if
            ? "expected '(' after @print_if"
            : "expected '(' after @print");
        return NULL;
    }

    JZASTNode *node = jz_ast_new(node_type, kw->loc);
    if (!node) return NULL;

    /* For @print_if, the first argument is the condition expression */
    if (is_print_if) {
        JZASTNode *cond = parse_expression(p);
        if (!cond) {
            jz_ast_free(node);
            return NULL;
        }
        jz_ast_add_child(node, cond);

        if (!match(p, JZ_TOK_COMMA)) {
            parser_error(p, "expected ',' after condition in @print_if");
            jz_ast_free(node);
            return NULL;
        }
    }

    /* Format string */
    const JZToken *fmt_tok = peek(p);
    if (fmt_tok->type != JZ_TOK_STRING || !fmt_tok->lexeme) {
        parser_error(p, "expected format string in @print/@print_if");
        jz_ast_free(node);
        return NULL;
    }
    jz_ast_set_text(node, fmt_tok->lexeme);
    advance(p);

    /* Optional arguments: , arg1, arg2, ... */
    while (peek(p)->type == JZ_TOK_COMMA) {
        advance(p); /* consume comma */
        JZASTNode *arg = parse_expression(p);
        if (!arg) {
            jz_ast_free(node);
            return NULL;
        }
        jz_ast_add_child(node, arg);
    }

    if (!match(p, JZ_TOK_RPAREN)) {
        parser_error(p, "expected ')' after @print/@print_if");
        jz_ast_free(node);
        return NULL;
    }

    return node;
}

/**
 * @brief Parse @run_until / @run_while directive.
 *
 * Syntax:
 *   @run_until(<signal> == <value>, timeout=<unit>=<amount>)
 *   @run_until(<signal> != <value>, timeout=<unit>=<amount>)
 *   @run_while(<signal> == <value>, timeout=<unit>=<amount>)
 *   @run_while(<signal> != <value>, timeout=<unit>=<amount>)
 *
 * Returns a SIM_RUN_UNTIL or SIM_RUN_WHILE node with:
 *   children[0] = signal identifier
 *   children[1] = expected value (literal)
 *   block_kind  = "==" or "!="
 *   text        = timeout unit ("ns", "ms", "ticks")
 *   name        = timeout value
 */
static JZASTNode *parse_sim_run_cond(Parser *p, JZASTNodeType node_type)
{
    const JZToken *kw = &p->tokens[p->pos - 1]; /* keyword already consumed */

    if (!match(p, JZ_TOK_LPAREN)) {
        parser_error(p, "expected '(' after @run_until/@run_while");
        return NULL;
    }

    /* Signal identifier */
    const JZToken *sig_tok = peek(p);
    if (!is_decl_identifier_token(sig_tok)) {
        parser_error(p, "expected signal name in @run_until/@run_while condition");
        return NULL;
    }
    JZASTNode *sig = jz_ast_new(JZ_AST_EXPR_IDENTIFIER, sig_tok->loc);
    if (!sig) return NULL;
    jz_ast_set_name(sig, sig_tok->lexeme);
    advance(p);

    /* == or != */
    const char *op = NULL;
    const JZToken *op_tok = peek(p);
    if (op_tok->type == JZ_TOK_OP_EQ) {
        op = "==";
        advance(p);
    } else if (op_tok->type == JZ_TOK_OP_NEQ) {
        op = "!=";
        advance(p);
    } else {
        parser_error(p, "expected '==' or '!=' in @run_until/@run_while condition");
        jz_ast_free(sig);
        return NULL;
    }

    /* Value expression (literal) */
    JZASTNode *val = parse_expression(p);
    if (!val) {
        jz_ast_free(sig);
        return NULL;
    }

    /* Comma */
    if (!match(p, JZ_TOK_COMMA)) {
        parser_error(p, "expected ',' before 'timeout=' in @run_until/@run_while");
        jz_ast_free(sig);
        jz_ast_free(val);
        return NULL;
    }

    /* timeout= */
    const JZToken *timeout_kw = peek(p);
    if (!timeout_kw->lexeme || strcmp(timeout_kw->lexeme, "timeout") != 0) {
        parser_error(p, "expected 'timeout=' in @run_until/@run_while");
        jz_ast_free(sig);
        jz_ast_free(val);
        return NULL;
    }
    advance(p);

    if (!match(p, JZ_TOK_OP_ASSIGN)) {
        parser_error(p, "expected '=' after 'timeout' in @run_until/@run_while");
        jz_ast_free(sig);
        jz_ast_free(val);
        return NULL;
    }

    /* Time unit: ns, ms, ticks */
    const JZToken *unit_tok = peek(p);
    if (!unit_tok->lexeme ||
        (strcmp(unit_tok->lexeme, "ns") != 0 &&
         strcmp(unit_tok->lexeme, "ms") != 0 &&
         strcmp(unit_tok->lexeme, "ticks") != 0)) {
        parser_error(p, "expected 'ns=', 'ms=', or 'ticks=' after 'timeout=' in @run_until/@run_while");
        jz_ast_free(sig);
        jz_ast_free(val);
        return NULL;
    }
    const char *unit = unit_tok->lexeme;
    advance(p);

    if (!match(p, JZ_TOK_OP_ASSIGN)) {
        parser_error(p, "expected '=' after unit in timeout");
        jz_ast_free(sig);
        jz_ast_free(val);
        return NULL;
    }

    /* Collect timeout value tokens until ')' */
    size_t tval_start = p->pos;
    while (peek(p)->type != JZ_TOK_EOF &&
           peek(p)->type != JZ_TOK_RPAREN) {
        advance(p);
    }
    size_t tval_end = p->pos;

    if (!match(p, JZ_TOK_RPAREN)) {
        parser_error(p, "expected ')' after @run_until/@run_while");
        jz_ast_free(sig);
        jz_ast_free(val);
        return NULL;
    }

    /* Build timeout value text */
    char *tval_text = NULL;
    if (tval_start < tval_end) {
        size_t buf_sz = 0;
        for (size_t i = tval_start; i < tval_end; ++i) {
            const JZToken *vt = &p->tokens[i];
            if (vt->lexeme) buf_sz += strlen(vt->lexeme) + 1;
        }
        if (buf_sz > 0) {
            tval_text = (char *)malloc(buf_sz + 1);
            if (!tval_text) { jz_ast_free(sig); jz_ast_free(val); return NULL; }
            tval_text[0] = '\0';
            for (size_t i = tval_start; i < tval_end; ++i) {
                const JZToken *vt = &p->tokens[i];
                if (!vt->lexeme) continue;
                strcat(tval_text, vt->lexeme);
            }
        }
    }

    JZASTNode *node = jz_ast_new(node_type, kw->loc);
    if (!node) {
        jz_ast_free(sig);
        jz_ast_free(val);
        free(tval_text);
        return NULL;
    }
    jz_ast_set_block_kind(node, op);
    jz_ast_set_text(node, unit);
    if (tval_text) {
        jz_ast_set_name(node, tval_text);
        free(tval_text);
    }
    jz_ast_add_child(node, sig);
    jz_ast_add_child(node, val);

    return node;
}

/**
 * @brief Parse the binding list inside a simulation @new block.
 *
 * Same as testbench @new — direction-less bindings.
 * Reuses parse_tb_binding_list logic inline.
 */
static int parse_sim_binding_list(Parser *p, JZASTNode *inst)
{
    for (;;) {
        const JZToken *t = peek(p);
        if (t->type == JZ_TOK_RBRACE) {
            advance(p);
            return 0;
        }
        if (t->type == JZ_TOK_EOF) {
            parser_error(p, "unterminated @new instance body (missing '}')");
            return -1;
        }
        if (t->type == JZ_TOK_SEMICOLON) {
            advance(p);
            continue;
        }

        /* BUS port binding: BUS <bus_id> <port_name> = <wire_name>; */
        if (t->type == JZ_TOK_IDENTIFIER && t->lexeme && strcmp(t->lexeme, "BUS") == 0) {
            advance(p);

            const JZToken *bus_id_tok = peek(p);
            if (!is_decl_identifier_token(bus_id_tok)) {
                parser_error(p, "expected BUS type name after BUS in @new binding");
                return -1;
            }
            advance(p);

            const JZToken *port_tok = peek(p);
            if (!is_decl_identifier_token(port_tok)) {
                parser_error(p, "expected port name after BUS type in @new binding");
                return -1;
            }
            advance(p);

            if (!match(p, JZ_TOK_OP_ASSIGN)) {
                parser_error(p, "expected '=' after BUS port name in @new binding");
                return -1;
            }

            const JZToken *wire_tok = peek(p);
            if (!is_decl_identifier_token(wire_tok)) {
                parser_error(p, "expected wire name on RHS of BUS @new binding");
                return -1;
            }
            JZASTNode *rhs = jz_ast_new(JZ_AST_EXPR_IDENTIFIER, wire_tok->loc);
            if (!rhs) return -1;
            jz_ast_set_name(rhs, wire_tok->lexeme);
            advance(p);

            if (!match(p, JZ_TOK_SEMICOLON)) {
                jz_ast_free(rhs);
                parser_error(p, "expected ';' after BUS @new binding");
                return -1;
            }

            JZASTNode *decl = jz_ast_new(JZ_AST_PORT_DECL, t->loc);
            if (!decl) { jz_ast_free(rhs); return -1; }
            jz_ast_set_name(decl, port_tok->lexeme);
            jz_ast_set_block_kind(decl, "BUS");
            jz_ast_set_text(decl, bus_id_tok->lexeme);
            jz_ast_add_child(decl, rhs);
            jz_ast_add_child(inst, decl);
            continue;
        }

        /* Port name */
        if (!is_decl_identifier_token(t)) {
            parser_error(p, "expected port name in @new instance body");
            return -1;
        }
        const JZToken *name_tok = t;
        advance(p);

        /* [width] */
        char *width_text = NULL;
        if (match(p, JZ_TOK_LBRACKET)) {
            size_t ws = p->pos;
            while (peek(p)->type != JZ_TOK_EOF &&
                   peek(p)->type != JZ_TOK_RBRACKET)
                advance(p);
            if (peek(p)->type != JZ_TOK_RBRACKET) {
                parser_error(p, "expected ']' after port width");
                return -1;
            }
            size_t we = p->pos;
            advance(p);
            if (ws < we) {
                size_t buf_sz = 0;
                for (size_t i = ws; i < we; ++i) {
                    const JZToken *wt = &p->tokens[i];
                    if (wt->lexeme) buf_sz += strlen(wt->lexeme) + 1;
                }
                if (buf_sz > 0) {
                    width_text = (char *)malloc(buf_sz + 1);
                    if (!width_text) return -1;
                    width_text[0] = '\0';
                    for (size_t i = ws; i < we; ++i) {
                        const JZToken *wt = &p->tokens[i];
                        if (!wt->lexeme) continue;
                        strcat(width_text, wt->lexeme);
                        strcat(width_text, " ");
                    }
                }
            }
        }

        if (!match(p, JZ_TOK_OP_ASSIGN)) {
            free(width_text);
            parser_error(p, "expected '=' after port name in @new binding");
            return -1;
        }

        const JZToken *rhs_tok = peek(p);
        if (!is_decl_identifier_token(rhs_tok)) {
            free(width_text);
            parser_error(p, "expected signal name on right-hand side of @new binding");
            return -1;
        }
        JZASTNode *rhs = jz_ast_new(JZ_AST_EXPR_IDENTIFIER, rhs_tok->loc);
        if (!rhs) { free(width_text); return -1; }
        jz_ast_set_name(rhs, rhs_tok->lexeme);
        advance(p);

        if (!match(p, JZ_TOK_SEMICOLON)) {
            jz_ast_free(rhs);
            free(width_text);
            parser_error(p, "expected ';' after @new binding");
            return -1;
        }

        JZASTNode *decl = jz_ast_new(JZ_AST_PORT_DECL, name_tok->loc);
        if (!decl) { jz_ast_free(rhs); free(width_text); return -1; }
        jz_ast_set_name(decl, name_tok->lexeme);
        if (width_text) {
            jz_ast_set_width(decl, width_text);
            free(width_text);
        }
        jz_ast_add_child(decl, rhs);
        jz_ast_add_child(inst, decl);
    }
}

/**
 * @brief Parse a simulation @new instantiation.
 */
static JZASTNode *parse_sim_instantiation(Parser *p)
{
    const JZToken *kw = &p->tokens[p->pos - 1]; /* @new already consumed */

    const JZToken *inst_name = peek(p);
    if (!is_decl_identifier_token(inst_name)) {
        parser_error(p, "expected instance name after @new in simulation");
        return NULL;
    }
    advance(p);

    const JZToken *mod_name = peek(p);
    if (!is_decl_identifier_token(mod_name)) {
        parser_error(p, "expected module name after instance name in @new");
        return NULL;
    }
    advance(p);

    JZASTNode *inst = jz_ast_new(JZ_AST_MODULE_INSTANCE, kw->loc);
    if (!inst) return NULL;
    jz_ast_set_name(inst, inst_name->lexeme);
    jz_ast_set_text(inst, mod_name->lexeme);

    if (!match(p, JZ_TOK_LBRACE)) {
        parser_error(p, "expected '{' after @new <inst> <module> in simulation");
        jz_ast_free(inst);
        return NULL;
    }

    if (parse_sim_binding_list(p, inst) != 0) {
        jz_ast_free(inst);
        return NULL;
    }

    return inst;
}

/**
 * @brief Parse a @setup or @update block body for simulation.
 *
 * Same as testbench: wire <= expr;
 */
static int parse_sim_stimulus_body(Parser *p, JZASTNode *parent)
{
    for (;;) {
        const JZToken *t = peek(p);
        if (t->type == JZ_TOK_RBRACE) {
            advance(p);
            return 0;
        }
        if (t->type == JZ_TOK_EOF) {
            parser_error(p, "unterminated @setup/@update block (missing '}')");
            return -1;
        }

        if (!is_decl_identifier_token(t)) {
            parser_error(p, "expected wire identifier in @setup/@update block");
            return -1;
        }

        JZASTNode *lhs = jz_ast_new(JZ_AST_EXPR_IDENTIFIER, t->loc);
        if (!lhs) return -1;
        jz_ast_set_name(lhs, t->lexeme);
        advance(p);

        if (!match(p, JZ_TOK_OP_LE)) {
            parser_error(p, "expected '<=' in @setup/@update assignment");
            jz_ast_free(lhs);
            return -1;
        }

        JZASTNode *rhs = parse_expression(p);
        if (!rhs) {
            jz_ast_free(lhs);
            return -1;
        }

        if (!match(p, JZ_TOK_SEMICOLON)) {
            parser_error(p, "expected ';' after @setup/@update assignment");
            jz_ast_free(lhs);
            jz_ast_free(rhs);
            return -1;
        }

        JZASTNode *assign = jz_ast_new(JZ_AST_STMT_ASSIGN, lhs->loc);
        if (!assign) {
            jz_ast_free(lhs);
            jz_ast_free(rhs);
            return -1;
        }
        jz_ast_set_text(assign, "<=");
        jz_ast_add_child(assign, lhs);
        jz_ast_add_child(assign, rhs);

        if (jz_ast_add_child(parent, assign) != 0) {
            jz_ast_free(assign);
            return -1;
        }
    }
}

/* ---------- public API ---------- */

JZASTNode *parse_simulation(Parser *p)
{
    const JZToken *kw = &p->tokens[p->pos - 1]; /* @simulation already consumed */

    /* Module name */
    const JZToken *mod_name = peek(p);
    if (!is_decl_identifier_token(mod_name)) {
        parser_error(p, "expected module name after @simulation");
        return NULL;
    }
    advance(p);

    JZASTNode *sim = jz_ast_new(JZ_AST_SIMULATION, kw->loc);
    if (!sim) return NULL;
    jz_ast_set_name(sim, mod_name->lexeme);

    /* Parse body until @endsim */
    while (peek(p)->type != JZ_TOK_EOF &&
           peek(p)->type != JZ_TOK_KW_ENDSIM) {
        const JZToken *t = peek(p);

        if (t->type == JZ_TOK_IDENTIFIER && t->lexeme && strcmp(t->lexeme, "CLOCK") == 0) {
            advance(p);
            JZASTNode *clk_block = jz_ast_new(JZ_AST_SIM_CLOCK_BLOCK, t->loc);
            if (!clk_block) { jz_ast_free(sim); return NULL; }
            if (!match(p, JZ_TOK_LBRACE)) {
                parser_error(p, "expected '{' after CLOCK in simulation");
                jz_ast_free(clk_block);
                jz_ast_free(sim);
                return NULL;
            }
            if (parse_sim_clock_block_body(p, clk_block) != 0) {
                jz_ast_free(clk_block);
                jz_ast_free(sim);
                return NULL;
            }
            jz_ast_add_child(sim, clk_block);
        } else if (t->type == JZ_TOK_KW_WIRE) {
            advance(p);
            JZASTNode *wire_block = jz_ast_new(JZ_AST_TB_WIRE_BLOCK, t->loc);
            if (!wire_block) { jz_ast_free(sim); return NULL; }
            if (!match(p, JZ_TOK_LBRACE)) {
                parser_error(p, "expected '{' after WIRE in simulation");
                jz_ast_free(wire_block);
                jz_ast_free(sim);
                return NULL;
            }
            /* Reuse the TB wire block parser */
            /* We need to call the static function from parser_testbench.c.
             * Since it's static, we duplicate the simple wire parsing here. */
            {
                for (;;) {
                    const JZToken *wt = peek(p);
                    if (wt->type == JZ_TOK_RBRACE) {
                        advance(p);
                        break;
                    }
                    if (wt->type == JZ_TOK_EOF) {
                        parser_error(p, "unterminated WIRE block (missing '}')");
                        jz_ast_free(wire_block);
                        jz_ast_free(sim);
                        return NULL;
                    }

                    /* BUS wire declaration */
                    if (wt->type == JZ_TOK_IDENTIFIER && wt->lexeme && strcmp(wt->lexeme, "BUS") == 0) {
                        advance(p);
                        const JZToken *bus_id_tok = peek(p);
                        if (!is_decl_identifier_token(bus_id_tok)) {
                            parser_error(p, "expected BUS type name");
                            jz_ast_free(wire_block); jz_ast_free(sim);
                            return NULL;
                        }
                        advance(p);

                        char *count_text = NULL;
                        if (match(p, JZ_TOK_LBRACKET)) {
                            size_t cs = p->pos;
                            while (peek(p)->type != JZ_TOK_EOF && peek(p)->type != JZ_TOK_RBRACKET) advance(p);
                            if (peek(p)->type != JZ_TOK_RBRACKET) {
                                parser_error(p, "expected ']' after BUS array count");
                                jz_ast_free(wire_block); jz_ast_free(sim); return NULL;
                            }
                            size_t ce = p->pos;
                            advance(p);
                            if (cs < ce) {
                                size_t bsz = 0;
                                for (size_t i = cs; i < ce; ++i) { if (p->tokens[i].lexeme) bsz += strlen(p->tokens[i].lexeme) + 1; }
                                if (bsz > 0) {
                                    count_text = (char *)malloc(bsz + 1);
                                    if (!count_text) { jz_ast_free(wire_block); jz_ast_free(sim); return NULL; }
                                    count_text[0] = '\0';
                                    for (size_t i = cs; i < ce; ++i) { if (p->tokens[i].lexeme) strcat(count_text, p->tokens[i].lexeme); }
                                }
                            }
                        }

                        const JZToken *nm = peek(p);
                        if (!is_decl_identifier_token(nm)) {
                            free(count_text);
                            parser_error(p, "expected wire name after BUS type");
                            jz_ast_free(wire_block); jz_ast_free(sim); return NULL;
                        }
                        advance(p);
                        if (!match(p, JZ_TOK_SEMICOLON)) {
                            free(count_text);
                            parser_error(p, "expected ';' after BUS wire declaration");
                            jz_ast_free(wire_block); jz_ast_free(sim); return NULL;
                        }

                        JZASTNode *wd = jz_ast_new(JZ_AST_TB_WIRE_DECL, wt->loc);
                        if (!wd) { free(count_text); jz_ast_free(wire_block); jz_ast_free(sim); return NULL; }
                        jz_ast_set_name(wd, nm->lexeme);
                        jz_ast_set_block_kind(wd, "BUS");
                        jz_ast_set_text(wd, bus_id_tok->lexeme);
                        if (count_text) { jz_ast_set_width(wd, count_text); free(count_text); }
                        jz_ast_add_child(wire_block, wd);
                        continue;
                    }

                    /* Scalar wire */
                    if (!is_decl_identifier_token(wt)) {
                        parser_error(p, "expected wire identifier in WIRE block");
                        jz_ast_free(wire_block); jz_ast_free(sim); return NULL;
                    }

                    JZASTNode *wd = jz_ast_new(JZ_AST_TB_WIRE_DECL, wt->loc);
                    if (!wd) { jz_ast_free(wire_block); jz_ast_free(sim); return NULL; }
                    jz_ast_set_name(wd, wt->lexeme);
                    advance(p);

                    if (match(p, JZ_TOK_LBRACKET)) {
                        size_t ws = p->pos;
                        while (peek(p)->type != JZ_TOK_EOF && peek(p)->type != JZ_TOK_RBRACKET) advance(p);
                        if (peek(p)->type != JZ_TOK_RBRACKET) {
                            parser_error(p, "expected ']' after wire width");
                            jz_ast_free(wd); jz_ast_free(wire_block); jz_ast_free(sim); return NULL;
                        }
                        size_t we = p->pos;
                        advance(p);
                        if (ws < we) {
                            size_t bsz = 0;
                            for (size_t i = ws; i < we; ++i) { if (p->tokens[i].lexeme) bsz += strlen(p->tokens[i].lexeme) + 1; }
                            if (bsz > 0) {
                                char *wtxt = (char *)malloc(bsz + 1);
                                if (!wtxt) { jz_ast_free(wd); jz_ast_free(wire_block); jz_ast_free(sim); return NULL; }
                                wtxt[0] = '\0';
                                for (size_t i = ws; i < we; ++i) {
                                    if (!p->tokens[i].lexeme) continue;
                                    strcat(wtxt, p->tokens[i].lexeme);
                                    strcat(wtxt, " ");
                                }
                                jz_ast_set_width(wd, wtxt);
                                free(wtxt);
                            }
                        }
                    }

                    if (!match(p, JZ_TOK_SEMICOLON)) {
                        parser_error(p, "expected ';' after wire declaration");
                        jz_ast_free(wd); jz_ast_free(wire_block); jz_ast_free(sim); return NULL;
                    }
                    jz_ast_add_child(wire_block, wd);
                }
            }
            jz_ast_add_child(sim, wire_block);
        } else if (t->type == JZ_TOK_KW_TAP) {
            advance(p);
            JZASTNode *tap_block = jz_ast_new(JZ_AST_SIM_TAP_BLOCK, t->loc);
            if (!tap_block) { jz_ast_free(sim); return NULL; }
            if (!match(p, JZ_TOK_LBRACE)) {
                parser_error(p, "expected '{' after TAP in simulation");
                jz_ast_free(tap_block);
                jz_ast_free(sim);
                return NULL;
            }
            if (parse_sim_tap_block_body(p, tap_block) != 0) {
                jz_ast_free(tap_block);
                jz_ast_free(sim);
                return NULL;
            }
            jz_ast_add_child(sim, tap_block);
        } else if (t->type == JZ_TOK_KW_IMPORT) {
            advance(p);
            const JZToken *path_tok = peek(p);
            if (path_tok->type != JZ_TOK_STRING || !path_tok->lexeme) {
                parser_error(p, "expected string after @import in @simulation");
                jz_ast_free(sim);
                return NULL;
            }
            const char *path = path_tok->lexeme;
            advance(p);
            if (import_modules_from_path(p, sim, path, t) != 0) {
                jz_ast_free(sim);
                return NULL;
            }
            match(p, JZ_TOK_SEMICOLON); /* optional */
        } else if (t->type == JZ_TOK_IDENTIFIER && t->lexeme && strcmp(t->lexeme, "BUS") == 0) {
            advance(p);
            JZASTNode *bus = parse_bus_definition(p, t);
            if (!bus) { jz_ast_free(sim); return NULL; }
            jz_ast_add_child(sim, bus);
        } else if (t->type == JZ_TOK_KW_NEW) {
            advance(p);
            JZASTNode *inst = parse_sim_instantiation(p);
            if (!inst) { jz_ast_free(sim); return NULL; }
            jz_ast_add_child(sim, inst);
        } else if (t->type == JZ_TOK_KW_SETUP) {
            advance(p);
            JZASTNode *setup = jz_ast_new(JZ_AST_TB_SETUP, t->loc);
            if (!setup) { jz_ast_free(sim); return NULL; }
            if (!match(p, JZ_TOK_LBRACE)) {
                parser_error(p, "expected '{' after @setup");
                jz_ast_free(setup);
                jz_ast_free(sim);
                return NULL;
            }
            if (parse_sim_stimulus_body(p, setup) != 0) {
                jz_ast_free(setup);
                jz_ast_free(sim);
                return NULL;
            }
            jz_ast_add_child(sim, setup);
        } else if (t->type == JZ_TOK_KW_UPDATE) {
            advance(p);
            JZASTNode *update = jz_ast_new(JZ_AST_TB_UPDATE, t->loc);
            if (!update) { jz_ast_free(sim); return NULL; }
            if (!match(p, JZ_TOK_LBRACE)) {
                parser_error(p, "expected '{' after @update");
                jz_ast_free(update);
                jz_ast_free(sim);
                return NULL;
            }
            if (parse_sim_stimulus_body(p, update) != 0) {
                jz_ast_free(update);
                jz_ast_free(sim);
                return NULL;
            }
            jz_ast_add_child(sim, update);
        } else if (t->type == JZ_TOK_KW_RUN) {
            advance(p);
            JZASTNode *run = parse_sim_run(p);
            if (!run) { jz_ast_free(sim); return NULL; }
            jz_ast_add_child(sim, run);
        } else if (t->type == JZ_TOK_KW_RUN_UNTIL) {
            advance(p);
            JZASTNode *ru = parse_sim_run_cond(p, JZ_AST_SIM_RUN_UNTIL);
            if (!ru) { jz_ast_free(sim); return NULL; }
            jz_ast_add_child(sim, ru);
        } else if (t->type == JZ_TOK_KW_RUN_WHILE) {
            advance(p);
            JZASTNode *rw = parse_sim_run_cond(p, JZ_AST_SIM_RUN_WHILE);
            if (!rw) { jz_ast_free(sim); return NULL; }
            jz_ast_add_child(sim, rw);
        } else if (t->type == JZ_TOK_KW_PRINT) {
            advance(p);
            JZASTNode *pr = parse_print_directive(p, 0);
            if (!pr) { jz_ast_free(sim); return NULL; }
            jz_ast_add_child(sim, pr);
        } else if (t->type == JZ_TOK_KW_PRINT_IF) {
            advance(p);
            JZASTNode *pr = parse_print_directive(p, 1);
            if (!pr) { jz_ast_free(sim); return NULL; }
            jz_ast_add_child(sim, pr);
        } else {
            parser_error(p, "unexpected token in @simulation block");
            jz_ast_free(sim);
            return NULL;
        }
    }

    if (!match(p, JZ_TOK_KW_ENDSIM)) {
        parser_error(p, "missing @endsim for simulation block");
        jz_ast_free(sim);
        return NULL;
    }

    return sim;
}
