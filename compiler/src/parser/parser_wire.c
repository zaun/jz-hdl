/**
 * @file parser_wire.c
 * @brief Parsing of WIRE block bodies.
 *
 * This file implements parsing for WIRE block declarations in the JZ HDL.
 * Each declaration defines a named wire with an explicit width expression.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser_internal.h"

/**
 * @brief Parse the body of a WIRE block.
 *
 * Each declaration defines a named wire with an explicit width expression.
 *
 * @param p      Active parser
 * @param parent WIRE block AST node
 * @return 0 on success, -1 on error
 */
int parse_wire_block_body(Parser *p, JZASTNode *parent) {
    for (;;) {
        const JZToken *t = peek(p);
        if (t->type == JZ_TOK_RBRACE) {
            advance(p); /* consume '}' */
            return 0;
        }
        if (t->type == JZ_TOK_EOF) {
            parser_error(p, "unterminated WIRE block (missing '}' )");
            return -1;
        }

        /* Feature guard sentinel — stop when inside a feature body */
        if (t->type == JZ_TOK_KW_FEATURE_ELSE || t->type == JZ_TOK_KW_ENDFEAT) {
            return 0;
        }
        /* Feature guard — parse it */
        if (t->type == JZ_TOK_KW_FEATURE) {
            if (parse_feature_guard_in_block(p, parent, parse_wire_block_body) != 0)
                return -1;
            continue;
        }

        const JZToken *name_tok = peek(p);
        if (!is_decl_identifier_token(name_tok)) {
            parser_error(p, "expected wire name in WIRE block");
            return -1;
        }
        advance(p);

        if (!match(p, JZ_TOK_LBRACKET)) {
            parser_error(p, "expected '[' after wire name");
            return -1;
        }

        size_t width_start = p->pos;
        while (peek(p)->type != JZ_TOK_EOF &&
               peek(p)->type != JZ_TOK_RBRACKET) {
            advance(p);
        }
        const JZToken *rb = peek(p);
        if (rb->type != JZ_TOK_RBRACKET) {
            parser_error(p, "expected ']' after wire width expression");
            return -1;
        }
        size_t width_end = p->pos;
        advance(p); /* consume ']' */

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

        if (!match(p, JZ_TOK_SEMICOLON)) {
            if (width_text) free(width_text);
            if (peek(p)->type == JZ_TOK_LBRACKET) {
                parser_error_rule(p, "WIRE_MULTI_DIMENSIONAL");
            } else {
                parser_error(p, "expected ';' after WIRE declaration");
            }
            return -1;
        }

        JZASTNode *decl = jz_ast_new(JZ_AST_WIRE_DECL, name_tok->loc);
        if (!decl) {
            if (width_text) free(width_text);
            return -1;
        }
        jz_ast_set_name(decl, name_tok->lexeme);
        if (width_text) {
            jz_ast_set_width(decl, width_text);
            free(width_text);
        }

        if (jz_ast_add_child(parent, decl) != 0) {
            jz_ast_free(decl);
            return -1;
        }
    }
}
