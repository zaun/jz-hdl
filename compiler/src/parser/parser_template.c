/**
 * @file parser_template.c
 * @brief Parsing of @template definitions, @scratch declarations, and @apply statements.
 *
 * This file implements parsing for the template system described in Section 10
 * of the JZ-HDL specification. Templates are parsed into AST nodes and later
 * expanded by the template expansion pass before semantic analysis.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser_internal.h"

/**
 * @brief Parse a @scratch declaration inside a template body.
 *
 * Syntax: @scratch <id> [<width_expr>];
 *
 * @param p Active parser (positioned after @scratch token was consumed)
 * @return SCRATCH_DECL AST node, or NULL on error
 */
JZASTNode *parse_scratch_decl(Parser *p) {
    const JZToken *scratch_tok = &p->tokens[p->pos - 1]; /* already consumed @scratch */

    const JZToken *name_tok = peek(p);
    if (name_tok->type != JZ_TOK_IDENTIFIER || !name_tok->lexeme) {
        parser_error(p, "expected identifier after @scratch");
        return NULL;
    }
    advance(p);

    /* Parse [width_expr] */
    if (!match(p, JZ_TOK_LBRACKET)) {
        parser_error(p, "expected '[' after @scratch identifier for width");
        return NULL;
    }

    /* Collect width expression as a string until ']' */
    size_t width_start = p->pos;
    int bracket_depth = 1;
    while (peek(p)->type != JZ_TOK_EOF && bracket_depth > 0) {
        if (peek(p)->type == JZ_TOK_LBRACKET) bracket_depth++;
        else if (peek(p)->type == JZ_TOK_RBRACKET) {
            bracket_depth--;
            if (bracket_depth == 0) break;
        }
        advance(p);
    }

    /* Build width string from tokens */
    size_t width_end = p->pos;
    char *width_str = NULL;
    size_t width_len = 0;
    for (size_t i = width_start; i < width_end; i++) {
        const JZToken *wt = &p->tokens[i];
        if (wt->lexeme) {
            size_t tl = strlen(wt->lexeme);
            char *nw = (char *)realloc(width_str, width_len + tl + 1);
            if (!nw) { free(width_str); return NULL; }
            width_str = nw;
            memcpy(width_str + width_len, wt->lexeme, tl);
            width_len += tl;
            width_str[width_len] = '\0';
        }
    }

    if (!match(p, JZ_TOK_RBRACKET)) {
        free(width_str);
        parser_error(p, "expected ']' after @scratch width expression");
        return NULL;
    }

    if (!match(p, JZ_TOK_SEMICOLON)) {
        free(width_str);
        parser_error(p, "expected ';' after @scratch declaration");
        return NULL;
    }

    JZASTNode *node = jz_ast_new(JZ_AST_SCRATCH_DECL, scratch_tok->loc);
    if (!node) { free(width_str); return NULL; }
    jz_ast_set_name(node, name_tok->lexeme);
    if (width_str) {
        jz_ast_set_width(node, width_str);
        free(width_str);
    }

    return node;
}

/**
 * @brief Parse a @template definition.
 *
 * Syntax: @template <id> (<param0>, <param1>, ...) <body> @endtemplate
 *
 * The body may contain @scratch declarations, assignment statements,
 * IF/ELIF/ELSE chains, and SELECT/CASE statements. Declaration blocks,
 * alias assignments, and structural directives are diagnosed as errors.
 *
 * @param p Active parser (positioned after @template token was consumed)
 * @return TEMPLATE_DEF AST node, or NULL on error
 */
JZASTNode *parse_template_def(Parser *p) {
    const JZToken *kw = &p->tokens[p->pos - 1]; /* already consumed @template */

    /* Template name */
    const JZToken *name_tok = peek(p);
    if (name_tok->type != JZ_TOK_IDENTIFIER || !name_tok->lexeme) {
        parser_error(p, "expected identifier after @template");
        return NULL;
    }
    advance(p);

    JZASTNode *tmpl = jz_ast_new(JZ_AST_TEMPLATE_DEF, kw->loc);
    if (!tmpl) return NULL;
    jz_ast_set_name(tmpl, name_tok->lexeme);

    /* Parameter list */
    if (!match(p, JZ_TOK_LPAREN)) {
        parser_error(p, "expected '(' after template name");
        jz_ast_free(tmpl);
        return NULL;
    }

    if (peek(p)->type != JZ_TOK_RPAREN) {
        for (;;) {
            const JZToken *param_tok = peek(p);
            if (param_tok->type != JZ_TOK_IDENTIFIER || !param_tok->lexeme) {
                parser_error(p, "expected parameter name in template parameter list");
                jz_ast_free(tmpl);
                return NULL;
            }
            advance(p);

            JZASTNode *param = jz_ast_new(JZ_AST_TEMPLATE_PARAM, param_tok->loc);
            if (!param) { jz_ast_free(tmpl); return NULL; }
            jz_ast_set_name(param, param_tok->lexeme);
            jz_ast_add_child(tmpl, param);

            if (!match(p, JZ_TOK_COMMA)) break;
        }
    }

    if (!match(p, JZ_TOK_RPAREN)) {
        parser_error(p, "expected ')' after template parameters");
        jz_ast_free(tmpl);
        return NULL;
    }

    /* Template body — parse until @endtemplate */
    while (peek(p)->type != JZ_TOK_EOF && peek(p)->type != JZ_TOK_KW_ENDTEMPLATE) {
        const JZToken *t = peek(p);

        if (t->type == JZ_TOK_SEMICOLON) {
            advance(p);
            continue;
        }

        /* @scratch declarations */
        if (t->type == JZ_TOK_KW_SCRATCH) {
            advance(p);
            JZASTNode *scratch = parse_scratch_decl(p);
            if (!scratch) { jz_ast_free(tmpl); return NULL; }
            jz_ast_add_child(tmpl, scratch);
            continue;
        }

        /* Forbidden: nested @template */
        if (t->type == JZ_TOK_KW_TEMPLATE) {
            parser_report_rule(p, t, "TEMPLATE_NESTED_DEF",
                               "found @template inside another @template body\n"
                               "move the inner template to module scope alongside the outer one");
            advance(p);
            /* Skip to matching @endtemplate, respecting nesting depth. */
            int nest = 1;
            while (peek(p)->type != JZ_TOK_EOF && nest > 0) {
                if (peek(p)->type == JZ_TOK_KW_TEMPLATE) nest++;
                else if (peek(p)->type == JZ_TOK_KW_ENDTEMPLATE) nest--;
                if (nest > 0) advance(p);
            }
            if (peek(p)->type == JZ_TOK_KW_ENDTEMPLATE) advance(p);
            continue;
        }

        /* Forbidden: declaration blocks */
        if (t->type == JZ_TOK_KW_WIRE || t->type == JZ_TOK_KW_REGISTER ||
            t->type == JZ_TOK_KW_PORT || t->type == JZ_TOK_KW_MEM ||
            t->type == JZ_TOK_KW_MUX || t->type == JZ_TOK_KW_LATCH ||
            t->type == JZ_TOK_KW_CONST || t->type == JZ_TOK_KW_CDC) {
            parser_report_rule(p, t, "TEMPLATE_FORBIDDEN_DECL",
                               "declaration blocks are not allowed inside template body; use @scratch for temporary signals");
            /* Skip the entire block to recover */
            advance(p);
            if (match(p, JZ_TOK_LBRACE)) {
                int depth = 1;
                while (peek(p)->type != JZ_TOK_EOF && depth > 0) {
                    if (peek(p)->type == JZ_TOK_LBRACE) depth++;
                    else if (peek(p)->type == JZ_TOK_RBRACE) depth--;
                    advance(p);
                }
            }
            continue;
        }

        /* Forbidden: block headers */
        if (t->type == JZ_TOK_KW_ASYNC || t->type == JZ_TOK_KW_SYNC) {
            parser_report_rule(p, t, "TEMPLATE_FORBIDDEN_BLOCK_HEADER",
                               "templates contain only statements, not block headers\n"
                               "@apply the template from inside a SYNCHRONOUS or ASYNCHRONOUS block");
            advance(p);
            /* Skip optional (CLK=...) parameter list */
            if (peek(p)->type == JZ_TOK_LPAREN) {
                advance(p);
                while (peek(p)->type != JZ_TOK_EOF &&
                       peek(p)->type != JZ_TOK_RPAREN &&
                       peek(p)->type != JZ_TOK_KW_ENDTEMPLATE) {
                    advance(p);
                }
                if (peek(p)->type == JZ_TOK_RPAREN) advance(p);
            }
            /* Skip block body { ... } if present */
            if (peek(p)->type == JZ_TOK_LBRACE) {
                int depth = 1;
                advance(p);
                while (peek(p)->type != JZ_TOK_EOF &&
                       peek(p)->type != JZ_TOK_KW_ENDTEMPLATE && depth > 0) {
                    if (peek(p)->type == JZ_TOK_LBRACE) depth++;
                    else if (peek(p)->type == JZ_TOK_RBRACE) depth--;
                    advance(p);
                }
            }
            continue;
        }

        /* Forbidden: structural directives */
        if (t->type == JZ_TOK_KW_NEW || t->type == JZ_TOK_KW_MODULE ||
            t->type == JZ_TOK_KW_ENDMOD || t->type == JZ_TOK_KW_PROJECT ||
            t->type == JZ_TOK_KW_ENDPROJ || t->type == JZ_TOK_KW_BLACKBOX ||
            t->type == JZ_TOK_KW_IMPORT || t->type == JZ_TOK_KW_FEATURE ||
            t->type == JZ_TOK_KW_ENDFEAT || t->type == JZ_TOK_KW_FEATURE_ELSE ||
            t->type == JZ_TOK_KW_GLOBAL || t->type == JZ_TOK_KW_ENDGLOB ||
            t->type == JZ_TOK_KW_CHECK || t->type == JZ_TOK_KW_APPLY) {
            parser_report_rule(p, t, "TEMPLATE_FORBIDDEN_DIRECTIVE",
                               "structural directives (@new, @module, @feature, etc.) cannot appear\n"
                               "inside a template body; templates may only contain assignment statements");
            advance(p);
            /* Skip until semicolon or @endtemplate, handling balanced braces */
            {
                int depth = 0;
                while (peek(p)->type != JZ_TOK_EOF &&
                       peek(p)->type != JZ_TOK_KW_ENDTEMPLATE) {
                    if (peek(p)->type == JZ_TOK_LBRACE) depth++;
                    else if (peek(p)->type == JZ_TOK_RBRACE) {
                        if (depth > 0) depth--;
                    }
                    if (peek(p)->type == JZ_TOK_SEMICOLON && depth == 0) {
                        advance(p);
                        break;
                    }
                    advance(p);
                }
            }
            continue;
        }

        /* IF statement */
        if (t->type == JZ_TOK_KW_IF) {
            /* Use parse_statement_list with a temporary parent to collect
             * the IF chain, but we need to handle it inline. Since
             * parse_statement_list wants a terminator, we'll use the
             * existing parse_if pattern by delegating to parse_statement_list
             * for a single statement.
             *
             * Actually, we can just use parse_statement_list on the whole
             * template body — but we need special terminator handling.
             * Instead, let's create a temporary wrapper.
             */
            if (parse_statement_list(p, tmpl, JZ_TOK_KW_ENDTEMPLATE, 0) != 0) {
                jz_ast_free(tmpl);
                return NULL;
            }
            /* parse_statement_list consumed the @endtemplate terminator */
            return tmpl;
        }

        /* SELECT statement */
        if (t->type == JZ_TOK_KW_SELECT) {
            if (parse_statement_list(p, tmpl, JZ_TOK_KW_ENDTEMPLATE, 0) != 0) {
                jz_ast_free(tmpl);
                return NULL;
            }
            return tmpl;
        }

        /* Assignment statement (default) */
        if (t->type == JZ_TOK_IDENTIFIER || t->type == JZ_TOK_KW_CONFIG ||
            t->type == JZ_TOK_LBRACE) {
            /* Delegate remaining body to parse_statement_list */
            if (parse_statement_list(p, tmpl, JZ_TOK_KW_ENDTEMPLATE, 0) != 0) {
                jz_ast_free(tmpl);
                return NULL;
            }
            return tmpl;
        }

        /* Unknown token in template body — skip */
        advance(p);
    }

    if (!match(p, JZ_TOK_KW_ENDTEMPLATE)) {
        parser_error(p, "missing @endtemplate for template definition");
        jz_ast_free(tmpl);
        return NULL;
    }

    return tmpl;
}

/**
 * @brief Parse an @apply statement.
 *
 * Syntax:
 *   @apply <template_id> (<arg0>, <arg1>, ...);
 *   @apply [count_expr] <template_id> (<arg0>, <arg1>, ...);
 *
 * @param p Active parser (positioned after @apply token was consumed)
 * @return TEMPLATE_APPLY AST node, or NULL on error
 */
JZASTNode *parse_apply_stmt(Parser *p) {
    const JZToken *kw = &p->tokens[p->pos - 1]; /* already consumed @apply */

    JZASTNode *apply = jz_ast_new(JZ_AST_TEMPLATE_APPLY, kw->loc);
    if (!apply) return NULL;

    /* Optional [count_expr] */
    if (match(p, JZ_TOK_LBRACKET)) {
        /* Collect count expression as string until ']' */
        size_t count_start = p->pos;
        int bracket_depth = 1;
        while (peek(p)->type != JZ_TOK_EOF && bracket_depth > 0) {
            if (peek(p)->type == JZ_TOK_LBRACKET) bracket_depth++;
            else if (peek(p)->type == JZ_TOK_RBRACKET) {
                bracket_depth--;
                if (bracket_depth == 0) break;
            }
            advance(p);
        }

        size_t count_end = p->pos;
        char *count_str = NULL;
        size_t count_len = 0;
        for (size_t i = count_start; i < count_end; i++) {
            const JZToken *ct = &p->tokens[i];
            if (ct->lexeme) {
                size_t tl = strlen(ct->lexeme);
                char *nc = (char *)realloc(count_str, count_len + tl + 1);
                if (!nc) { free(count_str); jz_ast_free(apply); return NULL; }
                count_str = nc;
                memcpy(count_str + count_len, ct->lexeme, tl);
                count_len += tl;
                count_str[count_len] = '\0';
            }
        }

        if (!match(p, JZ_TOK_RBRACKET)) {
            free(count_str);
            jz_ast_free(apply);
            parser_error(p, "expected ']' after @apply count expression");
            return NULL;
        }

        if (count_str) {
            jz_ast_set_text(apply, count_str);
            free(count_str);
        }
    }

    /* Template name */
    const JZToken *name_tok = peek(p);
    if (name_tok->type != JZ_TOK_IDENTIFIER || !name_tok->lexeme) {
        parser_error(p, "expected template name after @apply");
        jz_ast_free(apply);
        return NULL;
    }
    jz_ast_set_name(apply, name_tok->lexeme);
    advance(p);

    /* Argument list */
    if (!match(p, JZ_TOK_LPAREN)) {
        parser_error(p, "expected '(' after template name in @apply");
        jz_ast_free(apply);
        return NULL;
    }

    if (peek(p)->type != JZ_TOK_RPAREN) {
        for (;;) {
            JZASTNode *arg = parse_expression(p);
            if (!arg) {
                jz_ast_free(apply);
                return NULL;
            }
            jz_ast_add_child(apply, arg);
            if (!match(p, JZ_TOK_COMMA)) break;
        }
    }

    if (!match(p, JZ_TOK_RPAREN)) {
        parser_error(p, "expected ')' after @apply arguments");
        jz_ast_free(apply);
        return NULL;
    }

    if (!match(p, JZ_TOK_SEMICOLON)) {
        parser_error(p, "expected ';' after @apply statement");
        jz_ast_free(apply);
        return NULL;
    }

    return apply;
}
