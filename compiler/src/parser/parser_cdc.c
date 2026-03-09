/**
 * @file parser_cdc.c
 * @brief Parsing of CDC block bodies.
 *
 * This file implements parsing for CDC (Clock Domain Crossing) block
 * declarations in the JZ HDL. CDC entries describe clock-domain crossings
 * with optional stage counts and bit-selects.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser_internal.h"

/**
 * @brief Parse the body of a CDC block.
 *
 * CDC entries describe clock-domain crossings with optional stage counts.
 *
 * @param p      Active parser
 * @param parent CDC block AST node
 * @return 0 on success, -1 on error
 */
int parse_cdc_block_body(Parser *p, JZASTNode *parent) {
    /* Parse structured CDC entries:
     *   <cdc_type>[n_stages] <source_reg> (<src_clk>) => <dest_alias> (<dest_clk>);
     */
    for (;;) {
        const JZToken *t = peek(p);
        if (t->type == JZ_TOK_RBRACE) {
            advance(p);
            return 0;
        }
        if (t->type == JZ_TOK_EOF) {
            parser_error(p, "unterminated CDC block (missing '}' )");
            return -1;
        }
        if (t->type == JZ_TOK_SEMICOLON) {
            advance(p);
            continue;
        }

        /* cdc_type: BIT | BUS | FIFO (identifier token) */
        const JZToken *type_tok = peek(p);
        if (!is_decl_identifier_token(type_tok) || !type_tok->lexeme) {
            parser_error(p, "expected CDC type BIT/BUS/FIFO");
            return -1;
        }
        advance(p);

        JZASTNode *decl = jz_ast_new(JZ_AST_CDC_DECL, type_tok->loc);
        if (!decl) return -1;
        jz_ast_set_block_kind(decl, type_tok->lexeme); /* store cdc_type keyword */

        /* Optional [n_stages] */
        if (match(p, JZ_TOK_LBRACKET)) {
            size_t start = p->pos;
            while (peek(p)->type != JZ_TOK_EOF && peek(p)->type != JZ_TOK_RBRACKET) {
                advance(p);
            }
            const JZToken *rb = peek(p);
            if (rb->type != JZ_TOK_RBRACKET) {
                jz_ast_free(decl);
                parser_error(p, "expected ']' after CDC [n_stages]");
                return -1;
            }
            size_t end = p->pos;
            advance(p); /* consume ']' */

            char *stages = NULL;
            if (start < end) {
                size_t buf_sz = 0;
                for (size_t i = start; i < end; ++i) {
                    const JZToken *nt = &p->tokens[i];
                    if (nt->lexeme) buf_sz += strlen(nt->lexeme) + 1;
                }
                if (buf_sz > 0) {
                    stages = (char *)malloc(buf_sz + 1);
                    if (!stages) { jz_ast_free(decl); return -1; }
                    stages[0] = '\0';
                    for (size_t i = start; i < end; ++i) {
                        const JZToken *nt = &p->tokens[i];
                        if (!nt->lexeme) continue;
                        strcat(stages, nt->lexeme);
                        strcat(stages, " ");
                    }
                }
            }
            if (stages) {
                jz_ast_set_width(decl, stages); /* reuse width field for n_stages text */
                free(stages);
            }
        }

        /* <source_reg> identifier */
        const JZToken *src_tok = peek(p);
        if (!is_decl_identifier_token(src_tok) || !src_tok->lexeme) {
            jz_ast_free(decl);
            parser_error(p, "expected source REGISTER name in CDC entry");
            return -1;
        }
        JZASTNode *src_id = jz_ast_new(JZ_AST_EXPR_IDENTIFIER, src_tok->loc);
        if (!src_id) { jz_ast_free(decl); return -1; }
        jz_ast_set_name(src_id, src_tok->lexeme);
        advance(p);

        /* Optional bit-select: source_reg[index] or source_reg[msb:lsb] */
        if (peek(p)->type == JZ_TOK_LBRACKET) {
            advance(p); /* consume '[' */
            JZASTNode *msb = parse_simple_index_expr(p);
            if (!msb) {
                jz_ast_free(src_id);
                jz_ast_free(decl);
                parser_error(p, "expected index expression in CDC source bit-select");
                return -1;
            }
            JZASTNode *lsb = NULL;
            if (match(p, JZ_TOK_OP_COLON)) {
                lsb = parse_simple_index_expr(p);
                if (!lsb) {
                    jz_ast_free(msb);
                    jz_ast_free(src_id);
                    jz_ast_free(decl);
                    parser_error(p, "expected lsb expression in CDC source bit-select");
                    return -1;
                }
            }
            if (!match(p, JZ_TOK_RBRACKET)) {
                if (lsb) jz_ast_free(lsb);
                jz_ast_free(msb);
                jz_ast_free(src_id);
                jz_ast_free(decl);
                parser_error(p, "expected ']' after CDC source bit-select");
                return -1;
            }
            /* Build slice node: children = [base_id, msb, lsb] */
            /* For single index [idx], duplicate msb as lsb */
            if (!lsb) {
                /* Create a duplicate of msb for lsb (single-bit select [n] => [n:n]) */
                lsb = jz_ast_new(msb->type, msb->loc);
                if (!lsb) {
                    jz_ast_free(msb);
                    jz_ast_free(src_id);
                    jz_ast_free(decl);
                    return -1;
                }
                if (msb->name) jz_ast_set_name(lsb, msb->name);
                if (msb->width) jz_ast_set_width(lsb, msb->width);
                if (msb->text) jz_ast_set_text(lsb, msb->text);
            }
            JZASTNode *slice = jz_ast_new(JZ_AST_EXPR_SLICE, src_id->loc);
            if (!slice) {
                jz_ast_free(lsb);
                jz_ast_free(msb);
                jz_ast_free(src_id);
                jz_ast_free(decl);
                return -1;
            }
            if (jz_ast_add_child(slice, src_id) != 0 ||
                jz_ast_add_child(slice, msb) != 0 ||
                jz_ast_add_child(slice, lsb) != 0) {
                jz_ast_free(slice);
                jz_ast_free(lsb);
                jz_ast_free(msb);
                jz_ast_free(src_id);
                jz_ast_free(decl);
                return -1;
            }
            src_id = slice; /* replace bare identifier with slice node */
        }

        /* (<src_clk>) */
        if (!match(p, JZ_TOK_LPAREN)) {
            jz_ast_free(src_id);
            jz_ast_free(decl);
            parser_error(p, "expected '(' before CDC source clock");
            return -1;
        }
        const JZToken *clk_src_tok = peek(p);
        if (!is_decl_identifier_token(clk_src_tok) || !clk_src_tok->lexeme) {
            jz_ast_free(src_id);
            jz_ast_free(decl);
            parser_error(p, "expected source clock identifier in CDC entry");
            return -1;
        }
        JZASTNode *src_clk = jz_ast_new(JZ_AST_EXPR_IDENTIFIER, clk_src_tok->loc);
        if (!src_clk) { jz_ast_free(src_id); jz_ast_free(decl); return -1; }
        jz_ast_set_name(src_clk, clk_src_tok->lexeme);
        advance(p);
        if (!match(p, JZ_TOK_RPAREN)) {
            jz_ast_free(src_clk);
            jz_ast_free(src_id);
            jz_ast_free(decl);
            parser_error(p, "expected ')' after CDC source clock");
            return -1;
        }

        /* '=>' */
        const JZToken *arrow = peek(p);
        if (arrow->type != JZ_TOK_OP_DRIVE) {
            jz_ast_free(src_clk);
            jz_ast_free(src_id);
            jz_ast_free(decl);
            parser_error(p, "expected '=>' in CDC entry");
            return -1;
        }
        advance(p);

        /* <dest_alias> identifier */
        const JZToken *dst_tok = peek(p);
        if (!is_decl_identifier_token(dst_tok) || !dst_tok->lexeme) {
            jz_ast_free(src_clk);
            jz_ast_free(src_id);
            jz_ast_free(decl);
            parser_error(p, "expected destination alias identifier in CDC entry");
            return -1;
        }
        JZASTNode *dst_id = jz_ast_new(JZ_AST_EXPR_IDENTIFIER, dst_tok->loc);
        if (!dst_id) { jz_ast_free(src_clk); jz_ast_free(src_id); jz_ast_free(decl); return -1; }
        jz_ast_set_name(dst_id, dst_tok->lexeme);
        advance(p);

        /* (<dest_clk>) */
        if (!match(p, JZ_TOK_LPAREN)) {
            jz_ast_free(dst_id);
            jz_ast_free(src_clk);
            jz_ast_free(src_id);
            jz_ast_free(decl);
            parser_error(p, "expected '(' before CDC destination clock");
            return -1;
        }
        const JZToken *clk_dst_tok = peek(p);
        if (!is_decl_identifier_token(clk_dst_tok) || !clk_dst_tok->lexeme) {
            jz_ast_free(dst_id);
            jz_ast_free(src_clk);
            jz_ast_free(src_id);
            jz_ast_free(decl);
            parser_error(p, "expected destination clock identifier in CDC entry");
            return -1;
        }
        JZASTNode *dst_clk = jz_ast_new(JZ_AST_EXPR_IDENTIFIER, clk_dst_tok->loc);
        if (!dst_clk) {
            jz_ast_free(dst_id);
            jz_ast_free(src_clk);
            jz_ast_free(src_id);
            jz_ast_free(decl);
            return -1;
        }
        jz_ast_set_name(dst_clk, clk_dst_tok->lexeme);
        advance(p);
        if (!match(p, JZ_TOK_RPAREN)) {
            jz_ast_free(dst_clk);
            jz_ast_free(dst_id);
            jz_ast_free(src_clk);
            jz_ast_free(src_id);
            jz_ast_free(decl);
            parser_error(p, "expected ')' after CDC destination clock");
            return -1;
        }

        if (!match(p, JZ_TOK_SEMICOLON)) {
            jz_ast_free(dst_clk);
            jz_ast_free(dst_id);
            jz_ast_free(src_clk);
            jz_ast_free(src_id);
            jz_ast_free(decl);
            parser_error(p, "expected ';' at end of CDC entry");
            return -1;
        }

        if (jz_ast_add_child(decl, src_id) != 0 ||
            jz_ast_add_child(decl, src_clk) != 0 ||
            jz_ast_add_child(decl, dst_id) != 0 ||
            jz_ast_add_child(decl, dst_clk) != 0) {
            jz_ast_free(decl);
            return -1;
        }

        if (jz_ast_add_child(parent, decl) != 0) {
            jz_ast_free(decl);
            return -1;
        }
    }
}
