/**
 * @file parser_register.c
 * @brief Parsing of REGISTER and LATCH block bodies.
 *
 * This file implements parsing for REGISTER and LATCH block declarations
 * in the JZ HDL. Registers require a width and a literal initialization
 * value; latches require a width and a latch type (D or SR).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser_internal.h"

/**
 * @brief Parse the body of a REGISTER block.
 *
 * Each register declaration specifies a width and a required literal
 * initialization value.
 *
 * @param p      Active parser
 * @param parent REGISTER block AST node
 * @return 0 on success, -1 on error
 */
int parse_register_block_body(Parser *p, JZASTNode *parent) {
    for (;;) {
        const JZToken *t = peek(p);
        if (t->type == JZ_TOK_RBRACE) {
            advance(p); /* consume '}' */
            return 0;
        }
        if (t->type == JZ_TOK_EOF) {
            parser_error(p, "unterminated REGISTER block (missing '}' )");
            return -1;
        }

        /* Feature guard sentinel — stop when inside a feature body */
        if (t->type == JZ_TOK_KW_FEATURE_ELSE || t->type == JZ_TOK_KW_ENDFEAT) {
            return 0;
        }
        /* Feature guard — parse it */
        if (t->type == JZ_TOK_KW_FEATURE) {
            if (parse_feature_guard_in_block(p, parent, parse_register_block_body) != 0)
                return -1;
            continue;
        }

        const JZToken *name_tok = peek(p);
        if (!is_decl_identifier_token(name_tok)) {
            parser_error(p, "expected register name in REGISTER block");
            return -1;
        }
        advance(p);

        if (!match(p, JZ_TOK_LBRACKET)) {
            parser_error(p, "expected '[' after register name");
            return -1;
        }

        size_t width_start = p->pos;
        while (peek(p)->type != JZ_TOK_EOF &&
               peek(p)->type != JZ_TOK_RBRACKET) {
            advance(p);
        }
        const JZToken *rb = peek(p);
        if (rb->type != JZ_TOK_RBRACKET) {
            parser_error(p, "expected ']' after register width expression");
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

        if (!match(p, JZ_TOK_OP_ASSIGN)) {
            if (width_text) free(width_text);
            parser_error(p, "expected '=' and initialization literal in REGISTER block");
            return -1;
        }

        /* Parse the initializer as a real expression and require that it is a
           literal node per the spec (register reset value must be a literal). */
        JZASTNode *init_expr = parse_expression(p);
        if (!init_expr) {
            if (width_text) free(width_text);
            return -1;
        }
        if (init_expr->type != JZ_AST_EXPR_LITERAL &&
            init_expr->type != JZ_AST_EXPR_SPECIAL_DRIVER &&
            init_expr->type != JZ_AST_EXPR_QUALIFIED_IDENTIFIER &&
            init_expr->type != JZ_AST_EXPR_BUILTIN_CALL &&
            init_expr->type != JZ_AST_EXPR_CONCAT) {
            jz_ast_free(init_expr);
            if (width_text) free(width_text);
            parser_error(p, "register initialization must be a literal, GND/VCC, global constant, lit(), or concatenation");
            return -1;
        }

        if (!match(p, JZ_TOK_SEMICOLON)) {
            jz_ast_free(init_expr);
            if (width_text) free(width_text);
            parser_error(p, "expected ';' after register initialization");
            return -1;
        }

        JZASTNode *decl = jz_ast_new(JZ_AST_REGISTER_DECL, name_tok->loc);
        if (!decl) {
            jz_ast_free(init_expr);
            if (width_text) free(width_text);
            return -1;
        }
        jz_ast_set_name(decl, name_tok->lexeme);
        if (width_text) {
            jz_ast_set_width(decl, width_text);
            free(width_text);
        }

        if (jz_ast_add_child(decl, init_expr) != 0) {
            jz_ast_free(init_expr);
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
 * @brief Parse the body of a LATCH block.
 *
 * Latches require an explicit width and a latch type (D or SR).
 *
 * @param p      Active parser
 * @param parent LATCH block AST node
 * @return 0 on success, -1 on error
 */
int parse_latch_block_body(Parser *p, JZASTNode *parent) {
    for (;;) {
        const JZToken *t = peek(p);
        if (t->type == JZ_TOK_RBRACE) {
            advance(p); /* consume '}' */
            return 0;
        }
        if (t->type == JZ_TOK_EOF) {
            parser_error(p, "unterminated LATCH block (missing '}' )");
            return -1;
        }

        /* Feature guard sentinel — stop when inside a feature body */
        if (t->type == JZ_TOK_KW_FEATURE_ELSE || t->type == JZ_TOK_KW_ENDFEAT) {
            return 0;
        }
        /* Feature guard — parse it */
        if (t->type == JZ_TOK_KW_FEATURE) {
            if (parse_feature_guard_in_block(p, parent, parse_latch_block_body) != 0)
                return -1;
            continue;
        }

        const JZToken *name_tok = peek(p);
        if (!is_decl_identifier_token(name_tok)) {
            parser_error(p, "expected latch name in LATCH block");
            return -1;
        }
        advance(p);

        if (!match(p, JZ_TOK_LBRACKET)) {
            parser_error(p, "expected '[' after latch name");
            return -1;
        }

        size_t width_start = p->pos;
        while (peek(p)->type != JZ_TOK_EOF &&
               peek(p)->type != JZ_TOK_RBRACKET) {
            advance(p);
        }
        const JZToken *rb = peek(p);
        if (rb->type != JZ_TOK_RBRACKET) {
            parser_error(p, "expected ']' after latch width expression");
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

        /* Latch type: D or SR, required per Section 4.8. */
        const JZToken *type_tok = peek(p);
        if (!is_decl_identifier_token(type_tok) || !type_tok->lexeme) {
            if (width_text) free(width_text);
            parser_error(p, "expected latch type D or SR after width in LATCH block");
            return -1;
        }
        const char *type_lex = type_tok->lexeme;
        if (strcmp(type_lex, "D") != 0 && strcmp(type_lex, "SR") != 0) {
            if (width_text) free(width_text);
            {
                char latch_msg[512];
                snprintf(latch_msg, sizeof(latch_msg),
                         "LATCH type `%s` is not valid; only D (data latch) and SR (set-reset)\n"
                         "are supported per Section 4.8",
                         type_lex ? type_lex : "?");
                parser_report_rule(p,
                                   type_tok,
                                   "LATCH_INVALID_TYPE",
                                   latch_msg);
            }
            return -1;
        }
        advance(p);

        if (!match(p, JZ_TOK_SEMICOLON)) {
            if (width_text) free(width_text);
            parser_error(p, "expected ';' after LATCH declaration");
            return -1;
        }

        JZASTNode *decl = jz_ast_new(JZ_AST_LATCH_DECL, name_tok->loc);
        if (!decl) {
            if (width_text) free(width_text);
            return -1;
        }
        jz_ast_set_name(decl, name_tok->lexeme);
        if (width_text) {
            jz_ast_set_width(decl, width_text);
            free(width_text);
        }
        /* Store latch type ("D" or "SR") on block_kind for downstream passes. */
        jz_ast_set_block_kind(decl, type_lex);

        if (jz_ast_add_child(parent, decl) != 0) {
            jz_ast_free(decl);
            return -1;
        }
    }
}
