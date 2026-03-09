/**
 * @file parser_testbench.c
 * @brief Parsing of @testbench blocks.
 *
 * Handles the testbench-specific grammar including CLOCK/WIRE declarations,
 * TEST blocks, @new instantiations, @setup/@update blocks, @clock directives,
 * and @expect_equal/@expect_not_equal/@expect_tristate assertions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser_internal.h"

/* ---------- forward declarations ---------- */
static JZASTNode *parse_tb_instantiation(Parser *p);

/* ---------- helpers ---------- */

/**
 * @brief Parse a testbench CLOCK block body.
 *
 * CLOCK { <id>; ... }
 *
 * Each child is a TB_CLOCK_DECL node with the clock name.
 */
static int parse_tb_clock_block_body(Parser *p, JZASTNode *parent)
{
    for (;;) {
        const JZToken *t = peek(p);
        if (t->type == JZ_TOK_RBRACE) {
            advance(p);
            return 0;
        }
        if (t->type == JZ_TOK_EOF) {
            parser_error(p, "unterminated testbench CLOCK block (missing '}')");
            return -1;
        }

        if (!is_decl_identifier_token(t)) {
            parser_error(p, "expected clock identifier in testbench CLOCK block");
            return -1;
        }

        JZASTNode *decl = jz_ast_new(JZ_AST_TB_CLOCK_DECL, t->loc);
        if (!decl) return -1;
        jz_ast_set_name(decl, t->lexeme);
        advance(p);

        if (!match(p, JZ_TOK_SEMICOLON)) {
            parser_error(p, "expected ';' after clock declaration");
            jz_ast_free(decl);
            return -1;
        }

        if (jz_ast_add_child(parent, decl) != 0) {
            jz_ast_free(decl);
            return -1;
        }
    }
}

/**
 * @brief Parse a testbench WIRE block body.
 *
 * WIRE { <id> [<width>]; ... }
 */
static int parse_tb_wire_block_body(Parser *p, JZASTNode *parent)
{
    for (;;) {
        const JZToken *t = peek(p);
        if (t->type == JZ_TOK_RBRACE) {
            advance(p);
            return 0;
        }
        if (t->type == JZ_TOK_EOF) {
            parser_error(p, "unterminated testbench WIRE block (missing '}')");
            return -1;
        }

        /* BUS wire declaration: BUS <bus_id> [<count>] <name>; */
        if (t->type == JZ_TOK_IDENTIFIER && t->lexeme && strcmp(t->lexeme, "BUS") == 0) {
            advance(p); /* consume BUS */

            const JZToken *bus_id_tok = peek(p);
            if (!is_decl_identifier_token(bus_id_tok)) {
                parser_error(p, "expected BUS type name after BUS in WIRE block");
                return -1;
            }
            advance(p);

            /* Optional [<count>] */
            char *count_text = NULL;
            if (match(p, JZ_TOK_LBRACKET)) {
                size_t cs = p->pos;
                while (peek(p)->type != JZ_TOK_EOF &&
                       peek(p)->type != JZ_TOK_RBRACKET)
                    advance(p);
                if (peek(p)->type != JZ_TOK_RBRACKET) {
                    parser_error(p, "expected ']' after BUS array count");
                    return -1;
                }
                size_t ce = p->pos;
                advance(p); /* consume ']' */
                if (cs < ce) {
                    size_t buf_sz = 0;
                    for (size_t i = cs; i < ce; ++i) {
                        const JZToken *wt = &p->tokens[i];
                        if (wt->lexeme) buf_sz += strlen(wt->lexeme) + 1;
                    }
                    if (buf_sz > 0) {
                        count_text = (char *)malloc(buf_sz + 1);
                        if (!count_text) return -1;
                        count_text[0] = '\0';
                        for (size_t i = cs; i < ce; ++i) {
                            const JZToken *wt = &p->tokens[i];
                            if (!wt->lexeme) continue;
                            strcat(count_text, wt->lexeme);
                        }
                    }
                }
            }

            const JZToken *name_tok = peek(p);
            if (!is_decl_identifier_token(name_tok)) {
                if (count_text) free(count_text);
                parser_error(p, "expected wire name after BUS type in WIRE block");
                return -1;
            }
            advance(p);

            if (!match(p, JZ_TOK_SEMICOLON)) {
                if (count_text) free(count_text);
                parser_error(p, "expected ';' after BUS wire declaration");
                return -1;
            }

            JZASTNode *decl = jz_ast_new(JZ_AST_TB_WIRE_DECL, t->loc);
            if (!decl) {
                if (count_text) free(count_text);
                return -1;
            }
            jz_ast_set_name(decl, name_tok->lexeme);
            jz_ast_set_block_kind(decl, "BUS");
            jz_ast_set_text(decl, bus_id_tok->lexeme);
            if (count_text) {
                jz_ast_set_width(decl, count_text);
                free(count_text);
            }

            if (jz_ast_add_child(parent, decl) != 0) {
                jz_ast_free(decl);
                return -1;
            }
            continue;
        }

        if (!is_decl_identifier_token(t)) {
            parser_error(p, "expected wire identifier in testbench WIRE block");
            return -1;
        }

        JZASTNode *decl = jz_ast_new(JZ_AST_TB_WIRE_DECL, t->loc);
        if (!decl) return -1;
        jz_ast_set_name(decl, t->lexeme);
        advance(p);

        /* Optional [<width>] */
        if (match(p, JZ_TOK_LBRACKET)) {
            size_t width_start = p->pos;
            while (peek(p)->type != JZ_TOK_EOF &&
                   peek(p)->type != JZ_TOK_RBRACKET) {
                advance(p);
            }
            if (peek(p)->type != JZ_TOK_RBRACKET) {
                parser_error(p, "expected ']' after wire width expression");
                jz_ast_free(decl);
                return -1;
            }
            size_t width_end = p->pos;
            advance(p); /* consume ']' */

            if (width_start < width_end) {
                size_t buf_sz = 0;
                for (size_t i = width_start; i < width_end; ++i) {
                    const JZToken *wt = &p->tokens[i];
                    if (wt->lexeme) buf_sz += strlen(wt->lexeme) + 1;
                }
                if (buf_sz > 0) {
                    char *width_text = (char *)malloc(buf_sz + 1);
                    if (!width_text) {
                        jz_ast_free(decl);
                        return -1;
                    }
                    width_text[0] = '\0';
                    for (size_t i = width_start; i < width_end; ++i) {
                        const JZToken *wt = &p->tokens[i];
                        if (!wt->lexeme) continue;
                        strcat(width_text, wt->lexeme);
                        strcat(width_text, " ");
                    }
                    jz_ast_set_width(decl, width_text);
                    free(width_text);
                }
            }
        }

        if (!match(p, JZ_TOK_SEMICOLON)) {
            parser_error(p, "expected ';' after wire declaration");
            jz_ast_free(decl);
            return -1;
        }

        if (jz_ast_add_child(parent, decl) != 0) {
            jz_ast_free(decl);
            return -1;
        }
    }
}

/**
 * @brief Parse a @setup or @update block body.
 *
 * @setup { <wire> <= <expr>; ... }
 * @update { <wire> <= <expr>; ... }
 *
 * Assignments are stored as STMT_ASSIGN children.
 */
static int parse_tb_stimulus_body(Parser *p, JZASTNode *parent)
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

        /* Parse LHS identifier */
        if (!is_decl_identifier_token(t)) {
            parser_error(p, "expected wire identifier in @setup/@update block");
            return -1;
        }

        JZASTNode *lhs = jz_ast_new(JZ_AST_EXPR_IDENTIFIER, t->loc);
        if (!lhs) return -1;
        jz_ast_set_name(lhs, t->lexeme);
        advance(p);

        /* Expect <= operator */
        if (!match(p, JZ_TOK_OP_LE)) {
            parser_error(p, "expected '<=' in @setup/@update assignment");
            jz_ast_free(lhs);
            return -1;
        }

        /* Parse RHS expression */
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

        /* Create assignment node */
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

/**
 * @brief Parse a @clock(clk, cycle=N) directive.
 *
 * Expects the @clock keyword to have already been consumed.
 * Returns a TB_CLOCK_ADV node with name=clock_id, text=cycle_count.
 */
static JZASTNode *parse_tb_clock_adv(Parser *p)
{
    const JZToken *kw = &p->tokens[p->pos - 1]; /* @clock already consumed */

    if (!match(p, JZ_TOK_LPAREN)) {
        parser_error(p, "expected '(' after @clock");
        return NULL;
    }

    const JZToken *clk_tok = peek(p);
    if (!is_decl_identifier_token(clk_tok)) {
        parser_error(p, "expected clock identifier in @clock directive");
        return NULL;
    }
    advance(p);

    if (!match(p, JZ_TOK_COMMA)) {
        parser_error(p, "expected ',' after clock identifier in @clock directive");
        return NULL;
    }

    /* Expect "cycle" "=" <expr> */
    const JZToken *cycle_kw = peek(p);
    if (!cycle_kw->lexeme || strcmp(cycle_kw->lexeme, "cycle") != 0) {
        parser_error(p, "expected 'cycle=' in @clock directive");
        return NULL;
    }
    advance(p);

    if (!match(p, JZ_TOK_OP_ASSIGN)) {
        parser_error(p, "expected '=' after 'cycle' in @clock directive");
        return NULL;
    }

    /* Collect cycle count expression tokens until ')' */
    size_t expr_start = p->pos;
    while (peek(p)->type != JZ_TOK_EOF &&
           peek(p)->type != JZ_TOK_RPAREN) {
        advance(p);
    }
    size_t expr_end = p->pos;

    if (!match(p, JZ_TOK_RPAREN)) {
        parser_error(p, "expected ')' after @clock directive");
        return NULL;
    }

    /* Build cycle count text */
    char *cycle_text = NULL;
    if (expr_start < expr_end) {
        size_t buf_sz = 0;
        for (size_t i = expr_start; i < expr_end; ++i) {
            const JZToken *et = &p->tokens[i];
            if (et->lexeme) buf_sz += strlen(et->lexeme) + 1;
        }
        if (buf_sz > 0) {
            cycle_text = (char *)malloc(buf_sz + 1);
            if (!cycle_text) return NULL;
            cycle_text[0] = '\0';
            for (size_t i = expr_start; i < expr_end; ++i) {
                const JZToken *et = &p->tokens[i];
                if (!et->lexeme) continue;
                strcat(cycle_text, et->lexeme);
                if (i + 1 < expr_end) strcat(cycle_text, " ");
            }
        }
    }

    JZASTNode *node = jz_ast_new(JZ_AST_TB_CLOCK_ADV, kw->loc);
    if (!node) {
        free(cycle_text);
        return NULL;
    }
    jz_ast_set_name(node, clk_tok->lexeme);
    if (cycle_text) {
        jz_ast_set_text(node, cycle_text);
        free(cycle_text);
    }

    return node;
}

/**
 * @brief Parse @expect_equal(signal, value) or @expect_not_equal(signal, value).
 *
 * Expects the @expect_equal or @expect_not_equal keyword to have already been consumed.
 * The first child is the signal expression, the second is the expected value expression.
 */
static JZASTNode *parse_tb_expect(Parser *p, JZASTNodeType node_type)
{
    const JZToken *kw = &p->tokens[p->pos - 1]; /* keyword already consumed */

    if (!match(p, JZ_TOK_LPAREN)) {
        parser_error(p, "expected '(' after @expect directive");
        return NULL;
    }

    /* Parse signal expression (identifier, qualified id, or hierarchical ref) */
    JZASTNode *signal = parse_expression(p);
    if (!signal) return NULL;

    if (!match(p, JZ_TOK_COMMA)) {
        parser_error(p, "expected ',' in @expect directive");
        jz_ast_free(signal);
        return NULL;
    }

    /* Parse expected value expression */
    JZASTNode *value = parse_expression(p);
    if (!value) {
        jz_ast_free(signal);
        return NULL;
    }

    if (!match(p, JZ_TOK_RPAREN)) {
        parser_error(p, "expected ')' after @expect directive");
        jz_ast_free(signal);
        jz_ast_free(value);
        return NULL;
    }

    JZASTNode *node = jz_ast_new(node_type, kw->loc);
    if (!node) {
        jz_ast_free(signal);
        jz_ast_free(value);
        return NULL;
    }
    jz_ast_add_child(node, signal);
    jz_ast_add_child(node, value);
    return node;
}

/**
 * @brief Parse @expect_tristate(signal).
 *
 * Expects the @expect_tristate keyword to have already been consumed.
 * The single child is the signal expression.
 */
static JZASTNode *parse_tb_expect_tristate(Parser *p)
{
    const JZToken *kw = &p->tokens[p->pos - 1]; /* keyword already consumed */

    if (!match(p, JZ_TOK_LPAREN)) {
        parser_error(p, "expected '(' after @expect_tristate");
        return NULL;
    }

    JZASTNode *signal = parse_expression(p);
    if (!signal) return NULL;

    if (!match(p, JZ_TOK_RPAREN)) {
        parser_error(p, "expected ')' after @expect_tristate");
        jz_ast_free(signal);
        return NULL;
    }

    JZASTNode *node = jz_ast_new(JZ_AST_TB_EXPECT_TRI, kw->loc);
    if (!node) {
        jz_ast_free(signal);
        return NULL;
    }
    jz_ast_add_child(node, signal);
    return node;
}

/**
 * @brief Parse the binding list inside a testbench @new block.
 *
 * Testbench @new uses direction-less bindings:
 *   <port_name> [<width>] = <signal_id>;
 *
 * This differs from module-level @new which requires IN/OUT/INOUT prefixes.
 */
static int parse_tb_binding_list(Parser *p, JZASTNode *inst)
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
            advance(p); /* consume BUS */

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

            if (jz_ast_add_child(decl, rhs) != 0) {
                jz_ast_free(rhs);
                jz_ast_free(decl);
                return -1;
            }
            if (jz_ast_add_child(inst, decl) != 0) {
                jz_ast_free(decl);
                return -1;
            }
            continue;
        }

        /* Port name */
        if (!is_decl_identifier_token(t)) {
            parser_error(p, "expected port name in testbench @new instance body");
            return -1;
        }
        const JZToken *name_tok = t;
        advance(p);

        /* [width] */
        char *width_text = NULL;
        if (match(p, JZ_TOK_LBRACKET)) {
            size_t width_start = p->pos;
            while (peek(p)->type != JZ_TOK_EOF &&
                   peek(p)->type != JZ_TOK_RBRACKET) {
                advance(p);
            }
            if (peek(p)->type != JZ_TOK_RBRACKET) {
                parser_error(p, "expected ']' after port width in testbench @new binding");
                return -1;
            }
            size_t width_end = p->pos;
            advance(p); /* consume ']' */

            if (width_start < width_end) {
                size_t buf_sz = 0;
                for (size_t i = width_start; i < width_end; ++i) {
                    const JZToken *wt = &p->tokens[i];
                    if (wt->lexeme) buf_sz += strlen(wt->lexeme) + 1;
                }
                if (buf_sz > 0) {
                    width_text = (char *)malloc(buf_sz + 1);
                    if (!width_text) return -1;
                    width_text[0] = '\0';
                    for (size_t i = width_start; i < width_end; ++i) {
                        const JZToken *wt = &p->tokens[i];
                        if (!wt->lexeme) continue;
                        strcat(width_text, wt->lexeme);
                        strcat(width_text, " ");
                    }
                }
            }
        }

        /* = */
        if (!match(p, JZ_TOK_OP_ASSIGN)) {
            if (width_text) free(width_text);
            parser_error(p, "expected '=' after port name in testbench @new binding");
            return -1;
        }

        /* RHS: simple identifier (testbench wire or clock) */
        const JZToken *rhs_tok = peek(p);
        if (!is_decl_identifier_token(rhs_tok)) {
            if (width_text) free(width_text);
            parser_error(p, "expected signal name on right-hand side of testbench @new binding");
            return -1;
        }
        JZASTNode *rhs = jz_ast_new(JZ_AST_EXPR_IDENTIFIER, rhs_tok->loc);
        if (!rhs) {
            if (width_text) free(width_text);
            return -1;
        }
        jz_ast_set_name(rhs, rhs_tok->lexeme);
        advance(p);

        /* ; */
        if (!match(p, JZ_TOK_SEMICOLON)) {
            jz_ast_free(rhs);
            if (width_text) free(width_text);
            parser_error(p, "expected ';' after testbench @new binding");
            return -1;
        }

        /* Create PORT_DECL node (no direction — testbench bindings are directionless) */
        JZASTNode *decl = jz_ast_new(JZ_AST_PORT_DECL, name_tok->loc);
        if (!decl) {
            jz_ast_free(rhs);
            if (width_text) free(width_text);
            return -1;
        }
        jz_ast_set_name(decl, name_tok->lexeme);
        if (width_text) {
            jz_ast_set_width(decl, width_text);
            free(width_text);
        }

        if (jz_ast_add_child(decl, rhs) != 0) {
            jz_ast_free(rhs);
            jz_ast_free(decl);
            return -1;
        }

        if (jz_ast_add_child(inst, decl) != 0) {
            jz_ast_free(decl);
            return -1;
        }
    }
}

/**
 * @brief Parse a testbench @new instantiation.
 *
 * @new <inst_name> <module_name> {
 *     <port> [<width>] = <signal>;
 *     ...
 * }
 *
 * Uses direction-less port bindings unlike module-level @new.
 */
static JZASTNode *parse_tb_instantiation(Parser *p)
{
    const JZToken *kw = &p->tokens[p->pos - 1]; /* @new already consumed */

    const JZToken *inst_name = peek(p);
    if (!is_decl_identifier_token(inst_name)) {
        parser_error(p, "expected instance name after @new in testbench");
        return NULL;
    }
    advance(p);

    const JZToken *mod_name = peek(p);
    if (!is_decl_identifier_token(mod_name)) {
        parser_error(p, "expected module name after instance name in testbench @new");
        return NULL;
    }
    advance(p);

    JZASTNode *inst = jz_ast_new(JZ_AST_MODULE_INSTANCE, kw->loc);
    if (!inst) return NULL;
    jz_ast_set_name(inst, inst_name->lexeme);
    jz_ast_set_text(inst, mod_name->lexeme);

    if (!match(p, JZ_TOK_LBRACE)) {
        parser_error(p, "expected '{' after @new <inst> <module> in testbench");
        jz_ast_free(inst);
        return NULL;
    }

    if (parse_tb_binding_list(p, inst) != 0) {
        jz_ast_free(inst);
        return NULL;
    }

    return inst;
}

/**
 * @brief Parse the body of a TEST block.
 *
 * TEST "<desc>" {
 *     @new ...
 *     @setup { ... }
 *     <directives: @clock, @update, @expect_equal, @expect_not_equal>
 * }
 */
static int parse_test_body(Parser *p, JZASTNode *test_node)
{
    for (;;) {
        const JZToken *t = peek(p);
        if (t->type == JZ_TOK_RBRACE) {
            advance(p);
            return 0;
        }
        if (t->type == JZ_TOK_EOF) {
            parser_error(p, "unterminated TEST block (missing '}')");
            return -1;
        }

        if (t->type == JZ_TOK_KW_NEW) {
            advance(p);
            JZASTNode *inst = parse_tb_instantiation(p);
            if (!inst) return -1;
            jz_ast_add_child(test_node, inst);
        } else if (t->type == JZ_TOK_KW_SETUP) {
            advance(p);
            JZASTNode *setup = jz_ast_new(JZ_AST_TB_SETUP, t->loc);
            if (!setup) return -1;
            if (!match(p, JZ_TOK_LBRACE)) {
                parser_error(p, "expected '{' after @setup");
                jz_ast_free(setup);
                return -1;
            }
            if (parse_tb_stimulus_body(p, setup) != 0) {
                jz_ast_free(setup);
                return -1;
            }
            jz_ast_add_child(test_node, setup);
        } else if (t->type == JZ_TOK_KW_UPDATE) {
            advance(p);
            JZASTNode *update = jz_ast_new(JZ_AST_TB_UPDATE, t->loc);
            if (!update) return -1;
            if (!match(p, JZ_TOK_LBRACE)) {
                parser_error(p, "expected '{' after @update");
                jz_ast_free(update);
                return -1;
            }
            if (parse_tb_stimulus_body(p, update) != 0) {
                jz_ast_free(update);
                return -1;
            }
            jz_ast_add_child(test_node, update);
        } else if (t->type == JZ_TOK_KW_TB_CLOCK) {
            advance(p);
            JZASTNode *clk_adv = parse_tb_clock_adv(p);
            if (!clk_adv) return -1;
            jz_ast_add_child(test_node, clk_adv);
        } else if (t->type == JZ_TOK_KW_EXPECT_EQ) {
            advance(p);
            JZASTNode *expect = parse_tb_expect(p, JZ_AST_TB_EXPECT_EQ);
            if (!expect) return -1;
            jz_ast_add_child(test_node, expect);
        } else if (t->type == JZ_TOK_KW_EXPECT_NEQ) {
            advance(p);
            JZASTNode *expect = parse_tb_expect(p, JZ_AST_TB_EXPECT_NEQ);
            if (!expect) return -1;
            jz_ast_add_child(test_node, expect);
        } else if (t->type == JZ_TOK_KW_EXPECT_TRI) {
            advance(p);
            JZASTNode *expect = parse_tb_expect_tristate(p);
            if (!expect) return -1;
            jz_ast_add_child(test_node, expect);
        } else {
            parser_error(p, "unexpected token in TEST block");
            return -1;
        }
    }
}

/* ---------- public API ---------- */

JZASTNode *parse_testbench(Parser *p)
{
    const JZToken *kw = &p->tokens[p->pos - 1]; /* @testbench already consumed */

    /* Module name */
    const JZToken *mod_name = peek(p);
    if (!is_decl_identifier_token(mod_name)) {
        parser_error(p, "expected module name after @testbench");
        return NULL;
    }
    advance(p);

    JZASTNode *tb = jz_ast_new(JZ_AST_TESTBENCH, kw->loc);
    if (!tb) return NULL;
    jz_ast_set_name(tb, mod_name->lexeme);

    /* Parse body until @endtb */
    while (peek(p)->type != JZ_TOK_EOF &&
           peek(p)->type != JZ_TOK_KW_ENDTB) {
        const JZToken *t = peek(p);

        if (t->type == JZ_TOK_KW_CLOCKS) {
            /* CLOCK block (reusing CLOCKS token since CLOCK is not a separate keyword;
             * but per the spec it's "CLOCK {" not "CLOCKS {". We'll check for an identifier
             * "CLOCK" as well. Actually the spec says "CLOCK" which isn't a keyword. Let's
             * handle this: the lexer tokenizes "CLOCK" as an identifier. */
            parser_error(p, "unexpected CLOCKS keyword in testbench; use CLOCK { ... } instead");
            jz_ast_free(tb);
            return NULL;
        } else if (t->type == JZ_TOK_IDENTIFIER && t->lexeme && strcmp(t->lexeme, "CLOCK") == 0) {
            advance(p);
            JZASTNode *clk_block = jz_ast_new(JZ_AST_TB_CLOCK_BLOCK, t->loc);
            if (!clk_block) { jz_ast_free(tb); return NULL; }
            if (!match(p, JZ_TOK_LBRACE)) {
                parser_error(p, "expected '{' after CLOCK in testbench");
                jz_ast_free(clk_block);
                jz_ast_free(tb);
                return NULL;
            }
            if (parse_tb_clock_block_body(p, clk_block) != 0) {
                jz_ast_free(clk_block);
                jz_ast_free(tb);
                return NULL;
            }
            jz_ast_add_child(tb, clk_block);
        } else if (t->type == JZ_TOK_KW_WIRE) {
            advance(p);
            JZASTNode *wire_block = jz_ast_new(JZ_AST_TB_WIRE_BLOCK, t->loc);
            if (!wire_block) { jz_ast_free(tb); return NULL; }
            if (!match(p, JZ_TOK_LBRACE)) {
                parser_error(p, "expected '{' after WIRE in testbench");
                jz_ast_free(wire_block);
                jz_ast_free(tb);
                return NULL;
            }
            if (parse_tb_wire_block_body(p, wire_block) != 0) {
                jz_ast_free(wire_block);
                jz_ast_free(tb);
                return NULL;
            }
            jz_ast_add_child(tb, wire_block);
        } else if (t->type == JZ_TOK_KW_IMPORT) {
            /* @import "path"; inside @testbench — imports modules into tb node */
            advance(p);
            const JZToken *path_tok = peek(p);
            if (path_tok->type != JZ_TOK_STRING || !path_tok->lexeme) {
                parser_error(p, "expected string after @import in @testbench");
                jz_ast_free(tb);
                return NULL;
            }
            const char *path = path_tok->lexeme;
            advance(p);
            if (import_modules_from_path(p, tb, path, t) != 0) {
                jz_ast_free(tb);
                return NULL;
            }
            match(p, JZ_TOK_SEMICOLON); /* optional */
        } else if (t->type == JZ_TOK_IDENTIFIER && t->lexeme && strcmp(t->lexeme, "BUS") == 0) {
            /* BUS definition inside @testbench */
            advance(p);
            JZASTNode *bus = parse_bus_definition(p, t);
            if (!bus) { jz_ast_free(tb); return NULL; }
            jz_ast_add_child(tb, bus);
        } else if (t->type == JZ_TOK_KW_TEST ||
                   (t->type == JZ_TOK_IDENTIFIER && t->lexeme && strcmp(t->lexeme, "TEST") == 0)) {
            advance(p);
            const JZToken *desc = peek(p);
            if (desc->type != JZ_TOK_STRING || !desc->lexeme) {
                parser_error(p, "expected string description after TEST");
                jz_ast_free(tb);
                return NULL;
            }
            advance(p);

            JZASTNode *test = jz_ast_new(JZ_AST_TB_TEST, t->loc);
            if (!test) { jz_ast_free(tb); return NULL; }
            jz_ast_set_text(test, desc->lexeme);

            if (!match(p, JZ_TOK_LBRACE)) {
                parser_error(p, "expected '{' after TEST description");
                jz_ast_free(test);
                jz_ast_free(tb);
                return NULL;
            }

            if (parse_test_body(p, test) != 0) {
                jz_ast_free(test);
                jz_ast_free(tb);
                return NULL;
            }

            jz_ast_add_child(tb, test);
        } else {
            /* Skip unknown tokens with an error */
            parser_error(p, "unexpected token in @testbench block");
            jz_ast_free(tb);
            return NULL;
        }
    }

    if (!match(p, JZ_TOK_KW_ENDTB)) {
        parser_error(p, "missing @endtb for testbench block");
        jz_ast_free(tb);
        return NULL;
    }

    return tb;
}
