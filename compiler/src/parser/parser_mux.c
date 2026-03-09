/**
 * @file parser_mux.c
 * @brief Parsing of MUX block bodies.
 *
 * This file implements parsing for MUX block declarations in the JZ HDL.
 * Supports aggregate and slice-based MUX declarations.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser_internal.h"

/**
 * @brief Parse the body of a MUX block.
 *
 * Supports aggregate and slice-based MUX declarations.
 *
 * @param p      Active parser
 * @param parent MUX block AST node
 * @return 0 on success, -1 on error
 */
int parse_mux_block_body(Parser *p, JZASTNode *parent) {
    /* Parse structured MUX declarations:
     *   mux_id = src0, src1, ...;
     *   mux_id [elem_width] = wide_source;
     *
     * Each line becomes a JZ_AST_MUX_DECL child of the MuxBlock with:
     *   - name       = mux identifier
     *   - block_kind = "AGGREGATE" or "SLICE"
     *   - width      = element width expression text for SLICE form
     *   - children[0]= RawText node containing RHS source list or wide source.
     */
    for (;;) {
        const JZToken *t = peek(p);
        if (t->type == JZ_TOK_RBRACE) {
            /* End of MUX block. */
            advance(p);
            return 0;
        }
        if (t->type == JZ_TOK_EOF) {
            parser_error(p, "unterminated MUX block (missing '}' )");
            return -1;
        }
        if (t->type == JZ_TOK_SEMICOLON) {
            /* Allow stray semicolons between declarations. */
            advance(p);
            continue;
        }

        /* Feature guard sentinel — stop when inside a feature body */
        if (t->type == JZ_TOK_KW_FEATURE_ELSE || t->type == JZ_TOK_KW_ENDFEAT) {
            return 0;
        }
        /* Feature guard — parse it */
        if (t->type == JZ_TOK_KW_FEATURE) {
            if (parse_feature_guard_in_block(p, parent, parse_mux_block_body) != 0)
                return -1;
            continue;
        }

        /* MUX identifier. */
        if (!is_decl_identifier_token(t) || !t->lexeme) {
            parser_error(p, "expected MUX identifier");
            return -1;
        }
        const JZToken *id_tok = t;
        advance(p);

        JZASTNode *mux = jz_ast_new(JZ_AST_MUX_DECL, id_tok->loc);
        if (!mux) {
            return -1;
        }
        jz_ast_set_name(mux, id_tok->lexeme);

        int is_slice = 0;

        /* Optional [elem_width] for SLICE form. */
        t = peek(p);
        if (t->type == JZ_TOK_LBRACKET) {
            is_slice = 1;
            advance(p); /* consume '[' */

            size_t start = p->pos;
            while (peek(p)->type != JZ_TOK_EOF && peek(p)->type != JZ_TOK_RBRACKET) {
                advance(p);
            }
            const JZToken *rb = peek(p);
            if (rb->type != JZ_TOK_RBRACKET) {
                jz_ast_free(mux);
                parser_error(p, "expected ']' after MUX element width");
                return -1;
            }
            size_t end = p->pos;
            advance(p); /* consume ']' */

            char *width = NULL;
            if (start < end) {
                size_t buf_sz = 0;
                for (size_t i = start; i < end; ++i) {
                    const JZToken *wt = &p->tokens[i];
                    if (wt->lexeme) buf_sz += strlen(wt->lexeme) + 1;
                }
                if (buf_sz > 0) {
                    width = (char *)malloc(buf_sz + 1);
                    if (!width) {
                        jz_ast_free(mux);
                        return -1;
                    }
                    width[0] = '\0';
                    for (size_t i = start; i < end; ++i) {
                        const JZToken *wt = &p->tokens[i];
                        if (!wt->lexeme) continue;
                        strcat(width, wt->lexeme);
                        strcat(width, " ");
                    }
                }
            }
            if (width) {
                jz_ast_set_width(mux, width);
                free(width);
            }
        }

        /* Mandatory '=' token. */
        if (!match(p, JZ_TOK_OP_ASSIGN)) {
            jz_ast_free(mux);
            parser_error(p, "expected '=' in MUX declaration");
            return -1;
        }

        /* Collect RHS tokens up to ';' or '}' into a RawText child. */
        size_t rhs_start = p->pos;
        while (p->pos < p->count) {
            const JZToken *rt = &p->tokens[p->pos];
            if (rt->type == JZ_TOK_SEMICOLON ||
                rt->type == JZ_TOK_RBRACE ||
                rt->type == JZ_TOK_EOF) {
                break;
            }
            p->pos++;
        }
        size_t rhs_end = p->pos;
        if (rhs_start >= rhs_end) {
            jz_ast_free(mux);
            parser_error(p, "expected MUX source expression after '='");
            return -1;
        }

        JZASTNode *rhs = make_raw_text_node(p, rhs_start, rhs_end);
        if (!rhs) {
            jz_ast_free(mux);
            return -1;
        }
        if (jz_ast_add_child(mux, rhs) != 0) {
            jz_ast_free(rhs);
            jz_ast_free(mux);
            return -1;
        }

        /* Tag declaration kind. */
        if (is_slice) {
            jz_ast_set_block_kind(mux, "SLICE");
        } else {
            jz_ast_set_block_kind(mux, "AGGREGATE");
        }

        if (jz_ast_add_child(parent, mux) != 0) {
            jz_ast_free(mux);
            return -1;
        }

        /* Optional ';' terminator; closing '}' is handled at top of loop. */
        t = peek(p);
        if (t->type == JZ_TOK_SEMICOLON) {
            advance(p);
        } else if (t->type == JZ_TOK_EOF) {
            parser_error(p, "unterminated MUX block (missing '}' )");
            return -1;
        }
    }
}
