/**
 * @file parser_port.c
 * @brief Parsing of PORT block bodies.
 *
 * This file implements parsing for PORT block declarations in the JZ HDL,
 * including standard IN/OUT/INOUT port declarations and BUS-based port
 * declarations with optional array widths.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser_internal.h"

/**
 * @brief Parse the body of a PORT block.
 *
 * Supports standard IN/OUT/INOUT port declarations as well as BUS-based
 * port declarations with optional array widths.
 *
 * @param p      Active parser
 * @param parent PORT block AST node
 * @return 0 on success, -1 on error
 */
int parse_port_block_body(Parser *p, JZASTNode *parent) {
    for (;;) {
        const JZToken *t = peek(p);
        if (t->type == JZ_TOK_RBRACE) {
            advance(p); /* consume '}' */
            return 0;
        }
        if (t->type == JZ_TOK_EOF) {
            parser_error(p, "unterminated PORT block (missing '}' )");
            return -1;
        }

        /* Feature guard sentinel — stop when inside a feature body */
        if (t->type == JZ_TOK_KW_FEATURE_ELSE || t->type == JZ_TOK_KW_ENDFEAT) {
            return 0;
        }
        /* Feature guard — parse it */
        if (t->type == JZ_TOK_KW_FEATURE) {
            if (parse_feature_guard_in_block(p, parent, parse_port_block_body) != 0)
                return -1;
            continue;
        }

        /* BUS <bus_id> <ROLE> [N] <port_name>; */
        if (t->type == JZ_TOK_IDENTIFIER && t->lexeme && strcmp(t->lexeme, "BUS") == 0) {
            /* Consume 'BUS'. */
            advance(p);

            const JZToken *bus_id_tok = peek(p);
            if (!is_decl_identifier_token(bus_id_tok)) {
                parser_error(p, "expected BUS identifier after BUS keyword in PORT block");
                return -1;
            }
            advance(p);

            const JZToken *role_tok = peek(p);
            if (!is_decl_identifier_token(role_tok) || !role_tok->lexeme) {
                parser_error(p, "expected BUS role (SOURCE/TARGET) after BUS id in PORT block");
                return -1;
            }
            advance(p);

            /* Optional [N] array count. */
            char *array_width = NULL;
            if (match(p, JZ_TOK_LBRACKET)) {
                size_t width_start = p->pos;
                while (peek(p)->type != JZ_TOK_EOF &&
                       peek(p)->type != JZ_TOK_RBRACKET) {
                    advance(p);
                }
                const JZToken *rb = peek(p);
                if (rb->type != JZ_TOK_RBRACKET) {
                    parser_error(p, "expected ']' after BUS array width expression");
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
                        array_width = (char *)malloc(buf_sz + 1);
                        if (!array_width) return -1;
                        array_width[0] = '\0';
                        for (size_t i = width_start; i < width_end; ++i) {
                            const JZToken *wt = &p->tokens[i];
                            if (!wt->lexeme) continue;
                            strcat(array_width, wt->lexeme);
                            strcat(array_width, " ");
                        }
                    }
                }
            }

            const JZToken *port_name_tok = peek(p);
            if (!is_decl_identifier_token(port_name_tok)) {
                if (array_width) free(array_width);
                parser_error(p, "expected BUS port name after BUS role and optional [N]");
                return -1;
            }
            advance(p);

            if (!match(p, JZ_TOK_SEMICOLON)) {
                if (array_width) free(array_width);
                parser_error(p, "expected ';' after BUS port declaration");
                return -1;
            }

            JZASTNode *decl = jz_ast_new(JZ_AST_PORT_DECL, port_name_tok->loc);
            if (!decl) {
                if (array_width) free(array_width);
                return -1;
            }
            jz_ast_set_name(decl, port_name_tok->lexeme);
            /* Mark this as a BUS port; BUS id and ROLE are encoded in text. */
            jz_ast_set_block_kind(decl, "BUS");

            /* Encode "bus_id ROLE" into the text field for semantic passes. */
            const char *bus_name = bus_id_tok->lexeme ? bus_id_tok->lexeme : "";
            const char *role_name = role_tok->lexeme ? role_tok->lexeme : "";
            size_t bus_len = strlen(bus_name);
            size_t role_len = strlen(role_name);
            char *meta = (char *)malloc(bus_len + 1 + role_len + 1);
            if (!meta) {
                if (array_width) free(array_width);
                jz_ast_free(decl);
                return -1;
            }
            memcpy(meta, bus_name, bus_len);
            meta[bus_len] = ' ';
            memcpy(meta + bus_len + 1, role_name, role_len);
            meta[bus_len + 1 + role_len] = '\0';
            jz_ast_set_text(decl, meta);
            free(meta);

            if (array_width) {
                jz_ast_set_width(decl, array_width);
                free(array_width);
            }

            if (jz_ast_add_child(parent, decl) != 0) {
                jz_ast_free(decl);
                return -1;
            }

            continue;
        }

        /* Direction: IN / OUT / INOUT */
        const char *dir_str = NULL;
        JZTokenType dir_tok = t->type;
        if (dir_tok == JZ_TOK_KW_IN) {
            dir_str = "IN";
        } else if (dir_tok == JZ_TOK_KW_OUT) {
            dir_str = "OUT";
        } else if (dir_tok == JZ_TOK_KW_INOUT) {
            dir_str = "INOUT";
        } else {
            parser_error(p, "expected IN/OUT/INOUT/BUS in PORT block");
            return -1;
        }
        advance(p);

        if (!match(p, JZ_TOK_LBRACKET)) {
            parser_error_rule(p, "PORT_MISSING_WIDTH");
            return -1;
        }

        size_t width_start = p->pos;
        while (peek(p)->type != JZ_TOK_EOF &&
               peek(p)->type != JZ_TOK_RBRACKET) {
            advance(p);
        }
        const JZToken *rb = peek(p);
        if (rb->type != JZ_TOK_RBRACKET) {
            parser_error(p, "expected ']' after port width expression");
            return -1;
        }
        size_t width_end = p->pos; /* [width_start, width_end) is width expr */
        advance(p); /* consume ']' */

        /* Precompute width expr text once; reuse for all declarators on this line. */
        char *width_text = NULL;
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

        /* One or more port names, separated by commas, terminated by ';'. */
        int saw_any_name = 0;
        for (;;) {
            const JZToken *name_tok = peek(p);
            if (!is_decl_identifier_token(name_tok)) {
                if (!saw_any_name) {
                    if (width_text) free(width_text);
                    parser_error(p, "expected port name after width in PORT block");
                    return -1;
                }
                break;
            }
            saw_any_name = 1;
            advance(p);

            JZASTNode *decl = jz_ast_new(JZ_AST_PORT_DECL, name_tok->loc);
            if (!decl) {
                if (width_text) free(width_text);
                return -1;
            }
            jz_ast_set_name(decl, name_tok->lexeme);
            jz_ast_set_block_kind(decl, dir_str);
            if (width_text) {
                jz_ast_set_width(decl, width_text);
            }

            if (jz_ast_add_child(parent, decl) != 0) {
                jz_ast_free(decl);
                if (width_text) free(width_text);
                return -1;
            }

            if (!match(p, JZ_TOK_COMMA)) {
                break;
            }
        }

        if (width_text) {
            free(width_text);
            width_text = NULL;
        }

        if (!match(p, JZ_TOK_SEMICOLON)) {
            parser_error(p, "expected ';' after PORT declaration");
            return -1;
        }
    }
}
