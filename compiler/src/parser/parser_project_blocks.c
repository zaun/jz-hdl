/**
 * @file parser_project_blocks.c
 * @brief Parsing of project-level block bodies.
 *
 * This file implements parsing for project-level block constructs in the
 * JZ HDL, including CLOCKS, PIN (IN_PINS, OUT_PINS, INOUT_PINS), MAP,
 * and CLOCK_GEN blocks.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser_internal.h"

/**
 * @brief Check if a name is a valid CLOCK_GEN generator type.
 *
 * Valid types are PLL, DLL, CLKDIV, OSC, or BUF, optionally followed by
 * digits (e.g., PLL2, CLKDIV2, BUF3).
 */
static int is_valid_clock_gen_type(const char *name) {
    if (!name) return 0;
    const char *rest = NULL;
    if (strncmp(name, "CLKDIV", 6) == 0) rest = name + 6;
    else if (strncmp(name, "PLL", 3) == 0) rest = name + 3;
    else if (strncmp(name, "DLL", 3) == 0) rest = name + 3;
    else if (strncmp(name, "OSC", 3) == 0) rest = name + 3;
    else if (strncmp(name, "BUF", 3) == 0) rest = name + 3;
    else return 0;
    /* Optional trailing digits only */
    while (*rest) {
        if (!isdigit((unsigned char)*rest)) return 0;
        rest++;
    }
    return 1;
}

/**
 * @brief Parse the body of a CLOCKS block.
 *
 * CLOCKS blocks associate clock names with attribute dictionaries.
 *
 * @param p      Active parser
 * @param parent CLOCKS block AST node
 * @return 0 on success, -1 on error
 */
int parse_clocks_block_body(Parser *p, JZASTNode *parent) {
    for (;;) {
        const JZToken *t = peek(p);
        if (t->type == JZ_TOK_RBRACE) {
            advance(p); /* consume '}' */
            return 0;
        }
        if (t->type == JZ_TOK_EOF) {
            parser_error(p, "unterminated CLOCKS block (missing '}' )");
            return -1;
        }

        const JZToken *name_tok = peek(p);
        if (name_tok->type != JZ_TOK_IDENTIFIER) {
            parser_error(p, "expected clock name in CLOCKS block");
            return -1;
        }
        advance(p);

        /* Optional single-bit index, e.g. clks[0]. We treat the base
         * identifier as the clock/pin name and ignore the explicit bit
         * index here; MAP and PIN rules enforce bit-level coverage.
         */
        if (match(p, JZ_TOK_LBRACKET)) {
            while (peek(p)->type != JZ_TOK_EOF &&
                   peek(p)->type != JZ_TOK_RBRACKET) {
                advance(p);
            }
            if (!match(p, JZ_TOK_RBRACKET)) {
                parser_error(p, "expected ']' after clock index in CLOCKS block");
                return -1;
            }
        }

        JZASTNode *decl = jz_ast_new(JZ_AST_CONST_DECL, name_tok->loc);
        if (!decl) {
            return -1;
        }
        jz_ast_set_name(decl, name_tok->lexeme);

        /* Clocks can have optional attributes: either 'clock_name;' (for
         * CLOCK_GEN-driven clocks) or 'clock_name = { period=..., edge=... };'
         * (for external IN_PINS clocks).
         */
        if (match(p, JZ_TOK_OP_ASSIGN)) {
            if (!match(p, JZ_TOK_LBRACE)) {
                jz_ast_free(decl);
                parser_error(p, "expected '{' after '=' in CLOCKS block");
                return -1;
            }

            size_t attr_start = p->pos;
            int depth = 1;
            while (p->pos < p->count && depth > 0) {
                const JZToken *tok = &p->tokens[p->pos++];
                if (tok->type == JZ_TOK_LBRACE) {
                    depth++;
                } else if (tok->type == JZ_TOK_RBRACE) {
                    depth--;
                    if (depth == 0) {
                        break;
                    }
                }
            }
            if (depth != 0) {
                jz_ast_free(decl);
                parser_error(p, "unterminated '{' in CLOCKS block");
                return -1;
            }
            size_t attr_end = p->pos - 1; /* index of closing '}' */

            if (attr_start < attr_end) {
                size_t buf_sz = 0;
                for (size_t i = attr_start; i < attr_end; ++i) {
                    const JZToken *at = &p->tokens[i];
                    if (at->lexeme) buf_sz += strlen(at->lexeme) + 1;
                }
                if (buf_sz > 0) {
                    char *buf = (char *)malloc(buf_sz + 1);
                    if (!buf) {
                        jz_ast_free(decl);
                        return -1;
                    }
                    buf[0] = '\0';
                    for (size_t i = attr_start; i < attr_end; ++i) {
                        const JZToken *at = &p->tokens[i];
                        if (!at->lexeme) continue;
                        strcat(buf, at->lexeme);
                        strcat(buf, " ");
                    }
                    jz_ast_set_text(decl, buf);
                    free(buf);
                }
            }
        }

        if (!match(p, JZ_TOK_SEMICOLON)) {
            jz_ast_free(decl);
            parser_error(p, "expected ';' after CLOCKS entry");
            return -1;
        }

        if (jz_ast_add_child(parent, decl) != 0) {
            jz_ast_free(decl);
            return -1;
        }
    }
}

/**
 * @brief Parse the body of a PIN block.
 *
 * Handles IN_PINS, OUT_PINS, and INOUT_PINS blocks with optional widths
 * and attribute dictionaries.
 *
 * @param p          Active parser
 * @param parent     PIN block AST node
 * @param block_kind Block kind string (IN_PINS, OUT_PINS, INOUT_PINS)
 * @return 0 on success, -1 on error
 */
int parse_pins_block_body(Parser *p, JZASTNode *parent, const char *block_kind) {
    for (;;) {
        const JZToken *t = peek(p);
        if (t->type == JZ_TOK_RBRACE) {
            advance(p); /* consume '}' */
            return 0;
        }
        if (t->type == JZ_TOK_EOF) {
            parser_error(p, "unterminated PIN block (missing '}' )");
            return -1;
        }

        const JZToken *name_tok = peek(p);
        if (!is_decl_identifier_token(name_tok)) {
            parser_error(p, "expected pin name in PIN block");
            return -1;
        }
        advance(p);

        /* Optional bus width [..] */
        char *width_text = NULL;
        if (match(p, JZ_TOK_LBRACKET)) {
            size_t width_start = p->pos;
            while (peek(p)->type != JZ_TOK_EOF &&
                   peek(p)->type != JZ_TOK_RBRACKET) {
                advance(p);
            }
            const JZToken *rb = peek(p);
            if (rb->type != JZ_TOK_RBRACKET) {
                if (width_text) free(width_text);
                parser_error(p, "expected ']' after pin width expression");
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

        if (!match(p, JZ_TOK_OP_ASSIGN)) {
            if (width_text) free(width_text);
            parser_error(p, "expected '=' after pin name in PIN block");
            return -1;
        }

        if (!match(p, JZ_TOK_LBRACE)) {
            if (width_text) free(width_text);
            parser_error(p, "expected '{' after '=' in PIN block");
            return -1;
        }

        size_t attr_start = p->pos;
        int depth = 1;
        while (p->pos < p->count && depth > 0) {
            const JZToken *tok = &p->tokens[p->pos++];
            if (tok->type == JZ_TOK_LBRACE) {
                depth++;
            } else if (tok->type == JZ_TOK_RBRACE) {
                depth--;
                if (depth == 0) {
                    break;
                }
            }
        }
        if (depth != 0) {
            if (width_text) free(width_text);
            parser_error(p, "unterminated '{' in PIN block");
            return -1;
        }
        size_t attr_end = p->pos - 1; /* index of closing '}' */

        if (!match(p, JZ_TOK_SEMICOLON)) {
            if (width_text) free(width_text);
            parser_error(p, "expected ';' after PIN entry");
            return -1;
        }

        JZASTNode *decl = jz_ast_new(JZ_AST_PORT_DECL, name_tok->loc);
        if (!decl) {
            if (width_text) free(width_text);
            return -1;
        }
        jz_ast_set_name(decl, name_tok->lexeme);
        if (block_kind) {
            jz_ast_set_block_kind(decl, block_kind);
        }
        if (width_text) {
            jz_ast_set_width(decl, width_text);
            free(width_text);
        }

        if (attr_start < attr_end) {
            size_t buf_sz = 0;
            for (size_t i = attr_start; i < attr_end; ++i) {
                const JZToken *at = &p->tokens[i];
                if (at->lexeme) buf_sz += strlen(at->lexeme) + 1;
            }
            if (buf_sz > 0) {
                char *buf = (char *)malloc(buf_sz + 1);
                if (!buf) {
                    jz_ast_free(decl);
                    return -1;
                }
                buf[0] = '\0';
                for (size_t i = attr_start; i < attr_end; ++i) {
                    const JZToken *at = &p->tokens[i];
                    if (!at->lexeme) continue;
                    strcat(buf, at->lexeme);
                    strcat(buf, " ");
                }
                jz_ast_set_text(decl, buf);
                free(buf);
            }
        }

        if (jz_ast_add_child(parent, decl) != 0) {
            jz_ast_free(decl);
            return -1;
        }
    }
}

/**
 * @brief Parse the body of a MAP block.
 *
 * MAP blocks associate pins or pin bits with numeric locations.
 *
 * @param p      Active parser
 * @param parent MAP block AST node
 * @return 0 on success, -1 on error
 */
int parse_map_block_body(Parser *p, JZASTNode *parent) {
    for (;;) {
        const JZToken *t = peek(p);
        if (t->type == JZ_TOK_RBRACE) {
            advance(p); /* consume '}' */
            return 0;
        }
        if (t->type == JZ_TOK_EOF) {
            parser_error(p, "unterminated MAP block (missing '}' )");
            return -1;
        }

        size_t lhs_start = p->pos;
        while (peek(p)->type != JZ_TOK_EOF &&
               peek(p)->type != JZ_TOK_OP_ASSIGN &&
               peek(p)->type != JZ_TOK_SEMICOLON &&
               peek(p)->type != JZ_TOK_RBRACE) {
            advance(p);
        }
        size_t lhs_end = p->pos;

        const JZToken *eq_tok = peek(p);
        if (eq_tok->type != JZ_TOK_OP_ASSIGN) {
            parser_error(p, "expected '=' in MAP entry");
            return -1;
        }
        advance(p); /* consume '=' */

        size_t rhs_start = p->pos;
        {
            int brace_depth = 0;
            while (peek(p)->type != JZ_TOK_EOF) {
                if (peek(p)->type == JZ_TOK_LBRACE) {
                    brace_depth++;
                    advance(p);
                } else if (peek(p)->type == JZ_TOK_RBRACE) {
                    if (brace_depth > 0) {
                        brace_depth--;
                        advance(p);
                    } else {
                        break; /* block-closing brace */
                    }
                } else if (peek(p)->type == JZ_TOK_SEMICOLON && brace_depth == 0) {
                    break;
                } else {
                    advance(p);
                }
            }
        }
        size_t rhs_end = p->pos;

        if (peek(p)->type != JZ_TOK_SEMICOLON) {
            parser_error(p, "expected ';' after MAP entry");
            return -1;
        }
        advance(p); /* consume ';' */

        if (lhs_start >= lhs_end) {
            parser_error(p, "expected pin name or pin bit on left-hand side of MAP entry");
            return -1;
        }

        const JZToken *lhs_first = &p->tokens[lhs_start];
        JZASTNode *entry = jz_ast_new(JZ_AST_CONST_DECL, lhs_first->loc);
        if (!entry) {
            return -1;
        }

        /* Build LHS name string (e.g., clk, led[0]). */
        size_t buf_sz = 0;
        for (size_t i = lhs_start; i < lhs_end; ++i) {
            const JZToken *lt = &p->tokens[i];
            if (lt->lexeme) buf_sz += strlen(lt->lexeme) + 1;
        }
        if (buf_sz > 0) {
            char *buf = (char *)malloc(buf_sz + 1);
            if (!buf) {
                jz_ast_free(entry);
                return -1;
            }
            buf[0] = '\0';
            for (size_t i = lhs_start; i < lhs_end; ++i) {
                const JZToken *lt = &p->tokens[i];
                if (!lt->lexeme) continue;
                strcat(buf, lt->lexeme);
                strcat(buf, " ");
            }
            jz_ast_set_name(entry, buf);
            free(buf);
        }

        /* Build RHS text (e.g., 4, 79). */
        if (rhs_start < rhs_end) {
            size_t rhs_buf_sz = 0;
            for (size_t i = rhs_start; i < rhs_end; ++i) {
                const JZToken *rt = &p->tokens[i];
                if (rt->lexeme) rhs_buf_sz += strlen(rt->lexeme) + 1;
            }
            if (rhs_buf_sz > 0) {
                char *buf = (char *)malloc(rhs_buf_sz + 1);
                if (!buf) {
                    jz_ast_free(entry);
                    return -1;
                }
                buf[0] = '\0';
                for (size_t i = rhs_start; i < rhs_end; ++i) {
                    const JZToken *rt = &p->tokens[i];
                    if (!rt->lexeme) continue;
                    strcat(buf, rt->lexeme);
                    strcat(buf, " ");
                }
                jz_ast_set_text(entry, buf);
                free(buf);
            }
        }

        if (jz_ast_add_child(parent, entry) != 0) {
            jz_ast_free(entry);
            return -1;
        }
    }
}

/**
 * @brief Parse a CLOCK_GEN block inside a project.
 *
 * CLOCK_GEN blocks define PLLs/DLLs that generate clocks from other clocks.
 * The chip information comes from the PROJECT-level CHIP declaration.
 * The syntax is:
 *   CLOCK_GEN {
 *     PLL {
 *       IN <clock>;
 *       OUT <pll_output> <clock>;
 *       CONFIG { <param>=<value>; ... };
 *     };
 *   }
 *
 * @param p        Active parser
 * @param block_kw CLOCK_GEN keyword token
 * @return CLOCK_GEN block AST node, or NULL on error
 */
JZASTNode *parse_clock_gen_block(Parser *p, const JZToken *block_kw) {
    JZASTNode *cgen = jz_ast_new(JZ_AST_CLOCK_GEN_BLOCK, block_kw->loc);
    if (!cgen) return NULL;
    jz_ast_set_block_kind(cgen, "CLOCK_GEN");

    /* Reject attributes — chip information comes from the PROJECT */
    if (match(p, JZ_TOK_LPAREN)) {
        parser_error(p, "CLOCK_GEN does not accept attributes; chip information comes from the PROJECT");
        jz_ast_free(cgen);
        return NULL;
    }

    if (!match(p, JZ_TOK_LBRACE)) {
        parser_error(p, "expected '{' after CLOCK_GEN");
        jz_ast_free(cgen);
        return NULL;
    }

    /* Parse generator units (PLL or DLL) */
    for (;;) {
        const JZToken *t = peek(p);
        if (t->type == JZ_TOK_RBRACE) {
            advance(p); /* consume '}' */
            return cgen;
        }
        if (t->type == JZ_TOK_EOF) {
            parser_error(p, "unterminated CLOCK_GEN block (missing '}')");
            jz_ast_free(cgen);
            return NULL;
        }

        /* Expect generator type: PLL, DLL, CLKDIV, OSC, or BUF (with optional numeric suffix) */
        if (t->type != JZ_TOK_IDENTIFIER) {
            parser_error(p, "expected PLL, DLL, CLKDIV, OSC, or BUF generator type in CLOCK_GEN block");
            jz_ast_free(cgen);
            return NULL;
        }
        const char *gen_type = t->lexeme;
        if (!is_valid_clock_gen_type(gen_type)) {
            parser_error(p, "CLOCK_GEN generator must be PLL, DLL, CLKDIV, OSC, or BUF (with optional numeric suffix)");
            jz_ast_free(cgen);
            return NULL;
        }
        advance(p);

        JZASTNode *unit = jz_ast_new(JZ_AST_CLOCK_GEN_UNIT, t->loc);
        if (!unit) {
            jz_ast_free(cgen);
            return NULL;
        }
        jz_ast_set_name(unit, gen_type);

        if (!match(p, JZ_TOK_LBRACE)) {
            parser_error(p, "expected '{' after PLL/DLL/CLKDIV/OSC in CLOCK_GEN block");
            jz_ast_free(unit);
            jz_ast_free(cgen);
            return NULL;
        }

        /* Parse unit contents: IN, OUT, CONFIG */
        for (;;) {
            const JZToken *ut = peek(p);
            if (ut->type == JZ_TOK_RBRACE) {
                advance(p); /* consume '}' */
                break;
            }
            if (ut->type == JZ_TOK_EOF) {
                parser_error(p, "unterminated PLL/DLL block (missing '}')");
                jz_ast_free(unit);
                jz_ast_free(cgen);
                return NULL;
            }

            /* IN <input_name> <signal>; */
            if (ut->type == JZ_TOK_KW_IN) {
                advance(p);
                const JZToken *sel_tok = peek(p);
                if (!is_decl_identifier_token(sel_tok)) {
                    parser_error(p, "expected input name (e.g. REF_CLK) after IN in CLOCK_GEN");
                    jz_ast_free(unit);
                    jz_ast_free(cgen);
                    return NULL;
                }
                advance(p);

                const JZToken *sig_tok = peek(p);
                if (!is_decl_identifier_token(sig_tok)) {
                    parser_error(p, "expected signal name after input name in CLOCK_GEN IN");
                    jz_ast_free(unit);
                    jz_ast_free(cgen);
                    return NULL;
                }
                advance(p);

                JZASTNode *in_node = jz_ast_new(JZ_AST_CLOCK_GEN_IN, ut->loc);
                if (!in_node) {
                    jz_ast_free(unit);
                    jz_ast_free(cgen);
                    return NULL;
                }
                jz_ast_set_block_kind(in_node, sel_tok->lexeme);
                jz_ast_set_name(in_node, sig_tok->lexeme);

                if (!match(p, JZ_TOK_SEMICOLON)) {
                    parser_error(p, "expected ';' after IN clock in CLOCK_GEN");
                    jz_ast_free(in_node);
                    jz_ast_free(unit);
                    jz_ast_free(cgen);
                    return NULL;
                }

                if (jz_ast_add_child(unit, in_node) != 0) {
                    jz_ast_free(in_node);
                    jz_ast_free(unit);
                    jz_ast_free(cgen);
                    return NULL;
                }
                continue;
            }

            /* OUT <pll_output_name> <clock>; */
            if (ut->type == JZ_TOK_KW_OUT) {
                advance(p);
                const JZToken *pll_out_tok = peek(p);
                if (!is_decl_identifier_token(pll_out_tok)) {
                    parser_error(p, "expected PLL output name after OUT in CLOCK_GEN");
                    jz_ast_free(unit);
                    jz_ast_free(cgen);
                    return NULL;
                }
                advance(p);

                const JZToken *clk_tok = peek(p);
                if (!is_decl_identifier_token(clk_tok)) {
                    parser_error(p, "expected clock name after PLL output name in CLOCK_GEN OUT");
                    jz_ast_free(unit);
                    jz_ast_free(cgen);
                    return NULL;
                }
                advance(p);

                JZASTNode *out_node = jz_ast_new(JZ_AST_CLOCK_GEN_OUT, ut->loc);
                if (!out_node) {
                    jz_ast_free(unit);
                    jz_ast_free(cgen);
                    return NULL;
                }
                jz_ast_set_name(out_node, clk_tok->lexeme);
                jz_ast_set_block_kind(out_node, pll_out_tok->lexeme);  /* PLL output selector */

                if (!match(p, JZ_TOK_SEMICOLON)) {
                    parser_error(p, "expected ';' after OUT declaration in CLOCK_GEN");
                    jz_ast_free(out_node);
                    jz_ast_free(unit);
                    jz_ast_free(cgen);
                    return NULL;
                }

                if (jz_ast_add_child(unit, out_node) != 0) {
                    jz_ast_free(out_node);
                    jz_ast_free(unit);
                    jz_ast_free(cgen);
                    return NULL;
                }
                continue;
            }

            /* CONFIG { ... }; */
            if (ut->type == JZ_TOK_KW_CONFIG) {
                advance(p);
                if (!match(p, JZ_TOK_LBRACE)) {
                    parser_error(p, "expected '{' after CONFIG in CLOCK_GEN");
                    jz_ast_free(unit);
                    jz_ast_free(cgen);
                    return NULL;
                }

                JZASTNode *config = jz_ast_new(JZ_AST_CLOCK_GEN_CONFIG, ut->loc);
                if (!config) {
                    jz_ast_free(unit);
                    jz_ast_free(cgen);
                    return NULL;
                }

                /* Parse CONFIG contents as key=value pairs */
                for (;;) {
                    const JZToken *ct = peek(p);
                    if (ct->type == JZ_TOK_RBRACE) {
                        advance(p);
                        break;
                    }
                    if (ct->type == JZ_TOK_EOF) {
                        parser_error(p, "unterminated CONFIG block in CLOCK_GEN");
                        jz_ast_free(config);
                        jz_ast_free(unit);
                        jz_ast_free(cgen);
                        return NULL;
                    }

                    if (!is_decl_identifier_token(ct)) {
                        parser_error(p, "expected parameter name in CLOCK_GEN CONFIG");
                        jz_ast_free(config);
                        jz_ast_free(unit);
                        jz_ast_free(cgen);
                        return NULL;
                    }
                    const char *param_name = ct->lexeme;
                    JZLocation param_loc = ct->loc;
                    advance(p);

                    if (!match(p, JZ_TOK_OP_ASSIGN)) {
                        parser_error(p, "expected '=' after CONFIG parameter name");
                        jz_ast_free(config);
                        jz_ast_free(unit);
                        jz_ast_free(cgen);
                        return NULL;
                    }

                    const JZToken *val_tok = peek(p);
                    if (val_tok->type != JZ_TOK_NUMBER && val_tok->type != JZ_TOK_IDENTIFIER) {
                        parser_error(p, "expected value after '=' in CLOCK_GEN CONFIG");
                        jz_ast_free(config);
                        jz_ast_free(unit);
                        jz_ast_free(cgen);
                        return NULL;
                    }
                    const char *param_value = val_tok->lexeme;
                    advance(p);

                    if (!match(p, JZ_TOK_SEMICOLON)) {
                        parser_error(p, "expected ';' after CONFIG parameter");
                        jz_ast_free(config);
                        jz_ast_free(unit);
                        jz_ast_free(cgen);
                        return NULL;
                    }

                    /* Create a CONST_DECL node for the parameter */
                    JZASTNode *param = jz_ast_new(JZ_AST_CONST_DECL, param_loc);
                    if (!param) {
                        jz_ast_free(config);
                        jz_ast_free(unit);
                        jz_ast_free(cgen);
                        return NULL;
                    }
                    jz_ast_set_name(param, param_name);
                    jz_ast_set_text(param, param_value);

                    if (jz_ast_add_child(config, param) != 0) {
                        jz_ast_free(param);
                        jz_ast_free(config);
                        jz_ast_free(unit);
                        jz_ast_free(cgen);
                        return NULL;
                    }
                }

                /* Optional trailing semicolon after CONFIG block */
                match(p, JZ_TOK_SEMICOLON);

                if (jz_ast_add_child(unit, config) != 0) {
                    jz_ast_free(config);
                    jz_ast_free(unit);
                    jz_ast_free(cgen);
                    return NULL;
                }
                continue;
            }

            /* Unknown content - skip */
            parser_error(p, "unexpected token in CLOCK_GEN PLL/DLL block");
            jz_ast_free(unit);
            jz_ast_free(cgen);
            return NULL;
        }

        /* Optional trailing semicolon after PLL/DLL block */
        match(p, JZ_TOK_SEMICOLON);

        if (jz_ast_add_child(cgen, unit) != 0) {
            jz_ast_free(unit);
            jz_ast_free(cgen);
            return NULL;
        }
    }
}
