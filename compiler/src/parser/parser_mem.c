/**
 * @file parser_mem.c
 * @brief Parsing of MEM block bodies.
 *
 * This file implements parsing for MEM block declarations in the JZ HDL.
 * MEM blocks define memories with word width, depth, optional initialization,
 * and one or more IN/OUT ports with optional attributes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser_internal.h"

/**
 * @brief Parse the body of a MEM block.
 *
 * MEM blocks define memories with word width, depth, optional initialization,
 * and one or more IN/OUT ports with optional attributes.
 *
 * @param p      Active parser
 * @param parent MEM block AST node
 * @return 0 on success, -1 on error
 */
int parse_mem_block_body(Parser *p, JZASTNode *parent) {
    /* MEM { name [word_width] [depth] = init { IN ...; OUT ...; ... }; ... } */
    for (;;) {
        const JZToken *t = peek(p);
        if (t->type == JZ_TOK_RBRACE) {
            advance(p); /* consume '}' */
            return 0;
        }
        if (t->type == JZ_TOK_EOF) {
            parser_error(p, "unterminated MEM block (missing '}' )");
            return -1;
        }

        /* Feature guard sentinel — stop when inside a feature body */
        if (t->type == JZ_TOK_KW_FEATURE_ELSE || t->type == JZ_TOK_KW_ENDFEAT) {
            return 0;
        }
        /* Feature guard — parse it */
        if (t->type == JZ_TOK_KW_FEATURE) {
            if (parse_feature_guard_in_block(p, parent, parse_mem_block_body) != 0)
                return -1;
            continue;
        }

        const JZToken *name_tok = peek(p);
        if (!is_decl_identifier_token(name_tok)) {
            parser_error(p, "expected memory name in MEM block");
            return -1;
        }
        advance(p);

        if (!match(p, JZ_TOK_LBRACKET)) {
            parser_error(p, "expected '[' after MEM name for word_width");
            return -1;
        }

        size_t word_start = p->pos;
        while (peek(p)->type != JZ_TOK_EOF &&
               peek(p)->type != JZ_TOK_RBRACKET) {
            advance(p);
        }
        const JZToken *rb1 = peek(p);
        if (rb1->type != JZ_TOK_RBRACKET) {
            parser_error(p, "expected ']' after MEM word_width expression");
            return -1;
        }
        size_t word_end = p->pos;
        advance(p); /* consume ']' */

        if (!match(p, JZ_TOK_LBRACKET)) {
            parser_error(p, "expected '[' before MEM depth expression");
            return -1;
        }

        size_t depth_start = p->pos;
        while (peek(p)->type != JZ_TOK_EOF &&
               peek(p)->type != JZ_TOK_RBRACKET) {
            advance(p);
        }
        const JZToken *rb2 = peek(p);
        if (rb2->type != JZ_TOK_RBRACKET) {
            parser_error(p, "expected ']' after MEM depth expression");
            return -1;
        }
        size_t depth_end = p->pos;
        advance(p); /* consume ']' */

        /* Optional initializer:
         *   name [word_width] [depth] = <expr> { ... };
         *   name [word_width] [depth] = @file("path") { ... };
         *   name [word_width] [depth] { ... };
         *
         * In the third form the MEM is considered "uninitialized" and
         * semantic analysis is responsible for emitting MEM_MISSING_INIT
         * instead of this being a parse error.
         */
        int has_init = 0;
        if (match(p, JZ_TOK_OP_ASSIGN)) {
            has_init = 1;
        }

        /* Construct the MemDecl node before parsing the initializer so that any
           initializer expression or @file(...) form can be attached as a
           structured child instead of a RawText blob. */
        JZASTNode *mem = jz_ast_new(JZ_AST_MEM_DECL, name_tok->loc);
        if (!mem) return -1;
        jz_ast_set_name(mem, name_tok->lexeme);

        /* Parse the initializer (if present):
           - Literal/constant expression via the general expression parser.
           - Or the special @file("...") form parsed explicitly here.
        */
        JZASTNode *init_expr = NULL;

        if (has_init) {
            const JZToken *init_tok = peek(p);

            /* Support two lexical shapes for the @file initializer:
               - Older/ideal form where '@' is its own JZ_TOK_AT token followed by
                 identifier 'file'.
               - Current lexer behavior where the entire "@file" sequence is
                 lexed as a single JZ_TOK_IDENTIFIER with lexeme "@file".
            */
            int is_file_init = 0;
            JZLocation init_loc = init_tok->loc;

            if (init_tok->type == JZ_TOK_AT) {
                is_file_init = 1;
                advance(p); /* consume '@' */

                const JZToken *id = peek(p);
                if (id->type != JZ_TOK_IDENTIFIER || !id->lexeme || strcmp(id->lexeme, "file") != 0) {
                    jz_ast_free(mem);
                    parser_error(p, "expected file initializer '@file(""path"")' in MEM block");
                    return -1;
                }
                advance(p); /* consume 'file' */
            } else if (init_tok->type == JZ_TOK_IDENTIFIER &&
                       init_tok->lexeme && strcmp(init_tok->lexeme, "@file") == 0) {
                is_file_init = 1;
                advance(p); /* consume '@file' as a single identifier token */
            }

            if (is_file_init) {
                if (!match(p, JZ_TOK_LPAREN)) {
                    jz_ast_free(mem);
                    parser_error(p, "expected '(' after @file in MEM initializer");
                    return -1;
                }

                const JZToken *path_tok = peek(p);
                if (path_tok->type == JZ_TOK_STRING && path_tok->lexeme) {
                    /* Literal string path: @file("path/to/file.bin") */
                    init_expr = jz_ast_new(JZ_AST_EXPR_LITERAL, init_loc);
                    if (!init_expr) {
                        jz_ast_free(mem);
                        return -1;
                    }
                    jz_ast_set_text(init_expr, path_tok->lexeme);
                    advance(p); /* consume string */
                } else if (path_tok->type == JZ_TOK_IDENTIFIER && path_tok->lexeme) {
                    /* CONST or CONFIG.NAME reference: @file(MY_PATH) or @file(CONFIG.SAMPLE) */
                    char ref_name[256];
                    ref_name[0] = '\0';
                    if (strcmp(path_tok->lexeme, "CONFIG") == 0) {
                        advance(p); /* consume 'CONFIG' */
                        if (!match(p, JZ_TOK_DOT)) {
                            jz_ast_free(mem);
                            parser_error(p, "expected '.' after CONFIG in @file(CONFIG.NAME)");
                            return -1;
                        }
                        const JZToken *name_tok2 = peek(p);
                        if (name_tok2->type != JZ_TOK_IDENTIFIER || !name_tok2->lexeme) {
                            jz_ast_free(mem);
                            parser_error(p, "expected identifier after CONFIG. in @file()");
                            return -1;
                        }
                        snprintf(ref_name, sizeof(ref_name), "CONFIG.%s", name_tok2->lexeme);
                        advance(p); /* consume name */
                    } else {
                        snprintf(ref_name, sizeof(ref_name), "%s", path_tok->lexeme);
                        advance(p); /* consume identifier */
                    }
                    init_expr = jz_ast_new(JZ_AST_EXPR_IDENTIFIER, init_loc);
                    if (!init_expr) {
                        jz_ast_free(mem);
                        return -1;
                    }
                    jz_ast_set_name(init_expr, ref_name);
                    jz_ast_set_block_kind(init_expr, "FILE_REF");
                } else {
                    jz_ast_free(mem);
                    parser_error(p, "expected string path or CONST/CONFIG reference in @file(...) MEM initializer");
                    return -1;
                }

                if (!match(p, JZ_TOK_RPAREN)) {
                    jz_ast_free(init_expr);
                    jz_ast_free(mem);
                    parser_error(p, "expected ')' after @file path in MEM initializer");
                    return -1;
                }
            } else {
                init_expr = parse_expression(p);
                if (!init_expr) {
                    jz_ast_free(mem);
                    return -1;
                }
            }
        }

        if (!match(p, JZ_TOK_LBRACE)) {
            jz_ast_free(init_expr);
            jz_ast_free(mem);
            parser_error(p, "expected '{' to begin MEM port list");
            return -1;
        }

        /* word_width into width field */
        if (word_start < word_end) {
            size_t buf_sz = 0;
            for (size_t i = word_start; i < word_end; ++i) {
                const JZToken *wt = &p->tokens[i];
                if (wt->lexeme) buf_sz += strlen(wt->lexeme) + 1;
            }
            if (buf_sz > 0) {
                char *buf = (char *)malloc(buf_sz + 1);
                if (!buf) {
                    jz_ast_free(init_expr);
                    jz_ast_free(mem);
                    return -1;
                }
                buf[0] = '\0';
                for (size_t i = word_start; i < word_end; ++i) {
                    const JZToken *wt = &p->tokens[i];
                    if (!wt->lexeme) continue;
                    strcat(buf, wt->lexeme);
                    strcat(buf, " ");
                }
                jz_ast_set_width(mem, buf);
                free(buf);
            }
        }

        /* depth expression stored in text for now (no generic width slot left) */
        if (depth_start < depth_end) {
            size_t buf_sz = 0;
            for (size_t i = depth_start; i < depth_end; ++i) {
                const JZToken *dt = &p->tokens[i];
                if (dt->lexeme) buf_sz += strlen(dt->lexeme) + 1;
            }
            if (buf_sz > 0) {
                char *buf = (char *)malloc(buf_sz + 1);
                if (!buf) {
                    jz_ast_free(init_expr);
                    jz_ast_free(mem);
                    return -1;
                }
                buf[0] = '\0';
                for (size_t i = depth_start; i < depth_end; ++i) {
                    const JZToken *dt = &p->tokens[i];
                    if (!dt->lexeme) continue;
                    strcat(buf, dt->lexeme);
                    strcat(buf, " ");
                }
                jz_ast_set_text(mem, buf);
                free(buf);
            }
        }

        /* Attach the parsed initializer expression (literal or @file form) as
           the first child of the MemDecl. */
        if (init_expr) {
            if (jz_ast_add_child(mem, init_expr) != 0) {
                jz_ast_free(init_expr);
                jz_ast_free(mem);
                return -1;
            }
        }

        /* Parse MEM ports inside inner braces until '}' then require ';'. */
        for (;;) {
            const JZToken *pt = peek(p);
            if (pt->type == JZ_TOK_RBRACE) {
                advance(p); /* end of port list */
                break;
            }
            if (pt->type == JZ_TOK_EOF) {
                jz_ast_free(mem);
                parser_error(p, "unterminated MEM port list (missing '}' )");
                return -1;
            }

            if (pt->type == JZ_TOK_KW_IN) {
                /* IN = write port. Optional write-mode attribute block:
                 *   IN <name> {WRITE_MODE = <mode>;};
                 */
                advance(p);
                const JZToken *port_name = peek(p);
                if (!is_decl_identifier_token(port_name)) {
                    jz_ast_free(mem);
                    parser_error(p, "expected port name after IN in MEM block");
                    return -1;
                }
                advance(p);

                const JZToken *next = peek(p);
                const char *write_mode_value = NULL;

                /*
                 * Error-recovery for a very common mistake: using the read-port
                 * timing qualifiers on a write (IN) port, e.g. `IN wr ASYNC;`
                 * or `IN wr SYNC;`. Instead of treating this as a syntax error
                 * (which would fall back to PARSE000/MISSING_RULE_FIXME), emit
                 * the MEM_INVALID_PORT_TYPE rule at the qualifier token and
                 * then continue parsing so the rest of the MEM block can still
                 * be analyzed.
                 */
                if (next->type == JZ_TOK_IDENTIFIER && next->lexeme &&
                    (strcmp(next->lexeme, "ASYNC") == 0 ||
                     strcmp(next->lexeme, "SYNC") == 0)) {
                    parser_report_rule(p,
                                       next,
                                       "MEM_INVALID_PORT_TYPE",
                                       "invalid MEM IN port qualifier; expected WRITE_MODE attribute block or shorthand write mode");
                    advance(p); /* consume the unexpected qualifier */
                    /* Refresh lookahead for attribute/semicolon/shorthand handling. */
                    next = peek(p);
                }

                /* Shorthand form from S7.4:
                 *   IN <name> WRITE_FIRST;
                 *   IN <name> READ_FIRST;
                 *   IN <name> NO_CHANGE;
                 */
                if (next->type == JZ_TOK_IDENTIFIER && next->lexeme &&
                    (!strcmp(next->lexeme, "WRITE_FIRST") ||
                     !strcmp(next->lexeme, "READ_FIRST")  ||
                     !strcmp(next->lexeme, "NO_CHANGE"))) {
                    write_mode_value = next->lexeme;
                    advance(p); /* consume shorthand mode identifier */
                    next = peek(p);
                }

                if (next->type == JZ_TOK_LBRACE) {
                    advance(p); /* consume '{' */

                    const JZToken *attr_key = peek(p);
                    if (attr_key->type != JZ_TOK_IDENTIFIER || !attr_key->lexeme ||
                        strcmp(attr_key->lexeme, "WRITE_MODE") != 0) {
                        jz_ast_free(mem);
                        parser_error(p, "expected WRITE_MODE attribute in MEM IN port");
                        return -1;
                    }
                    advance(p); /* consume WRITE_MODE */

                    if (!match(p, JZ_TOK_OP_ASSIGN)) {
                        jz_ast_free(mem);
                        parser_error(p, "expected '=' after WRITE_MODE in MEM IN port");
                        return -1;
                    }

                    const JZToken *mode_tok = peek(p);
                    if (mode_tok->type != JZ_TOK_IDENTIFIER || !mode_tok->lexeme) {
                        jz_ast_free(mem);
                        parser_error(p, "expected write mode identifier after WRITE_MODE = in MEM IN port");
                        return -1;
                    }
                    write_mode_value = mode_tok->lexeme;
                    advance(p); /* consume mode identifier */

                    /* Optional ';' inside the attribute block before '}'. */
                    if (match(p, JZ_TOK_SEMICOLON)) {
                        /* nothing else expected before '}' */
                    }

                    if (!match(p, JZ_TOK_RBRACE)) {
                        jz_ast_free(mem);
                        parser_error(p, "expected '}' after WRITE_MODE attribute in MEM IN port");
                        return -1;
                    }
                }

                if (!match(p, JZ_TOK_SEMICOLON)) {
                    jz_ast_free(mem);
                    parser_error(p, "expected ';' after MEM IN port");
                    return -1;
                }

                JZASTNode *pnode = jz_ast_new(JZ_AST_MEM_PORT, port_name->loc);
                if (!pnode) { jz_ast_free(mem); return -1; }
                jz_ast_set_block_kind(pnode, "IN");
                jz_ast_set_name(pnode, port_name->lexeme);
                if (write_mode_value) {
                    jz_ast_set_text(pnode, write_mode_value);
                }
                if (jz_ast_add_child(mem, pnode) != 0) {
                    jz_ast_free(pnode);
                    jz_ast_free(mem);
                    return -1;
                }
            } else if (pt->type == JZ_TOK_KW_OUT) {
                /* OUT = read port. Optional timing qualifier:
                 *   OUT <name> ASYNC;
                 *   OUT <name> SYNC;
                 */
                advance(p);
                const JZToken *port_name = peek(p);
                if (!is_decl_identifier_token(port_name)) {
                    jz_ast_free(mem);
                    parser_error(p, "expected port name after OUT in MEM block");
                    return -1;
                }
                advance(p);

                const JZToken *mode_tok = peek(p);
                const char *mode = NULL;
                if (mode_tok->type == JZ_TOK_IDENTIFIER && mode_tok->lexeme) {
                    if (strcmp(mode_tok->lexeme, "ASYNC") == 0 ||
                        strcmp(mode_tok->lexeme, "SYNC") == 0) {
                        mode = mode_tok->lexeme;
                        advance(p);
                    }
                }

                if (!match(p, JZ_TOK_SEMICOLON)) {
                    jz_ast_free(mem);
                    parser_error(p, "expected ';' after MEM OUT port");
                    return -1;
                }

                JZASTNode *pnode = jz_ast_new(JZ_AST_MEM_PORT, port_name->loc);
                if (!pnode) { jz_ast_free(mem); return -1; }
                jz_ast_set_block_kind(pnode, "OUT");
                jz_ast_set_name(pnode, port_name->lexeme);
                if (mode) {
                    jz_ast_set_text(pnode, mode);
                }
                if (jz_ast_add_child(mem, pnode) != 0) {
                    jz_ast_free(pnode);
                    jz_ast_free(mem);
                    return -1;
                }
            } else if (pt->type == JZ_TOK_KW_INOUT) {
                /* INOUT = shared read/write port. Write mode allowed, no ASYNC/SYNC. */
                advance(p);
                const JZToken *port_name = peek(p);
                if (!is_decl_identifier_token(port_name)) {
                    jz_ast_free(mem);
                    parser_error(p, "expected port name after INOUT in MEM block");
                    return -1;
                }
                advance(p);

                const JZToken *next = peek(p);
                const char *write_mode_value = NULL;

                /* Error recovery for ASYNC/SYNC on INOUT port */
                if (next->type == JZ_TOK_IDENTIFIER && next->lexeme &&
                    (strcmp(next->lexeme, "ASYNC") == 0 ||
                     strcmp(next->lexeme, "SYNC") == 0)) {
                    parser_report_rule(p, next, "MEM_INOUT_ASYNC",
                                       "INOUT ports are always synchronous; ASYNC/SYNC not permitted");
                    advance(p);
                    next = peek(p);
                }

                /* Shorthand write mode form */
                if (next->type == JZ_TOK_IDENTIFIER && next->lexeme &&
                    (!strcmp(next->lexeme, "WRITE_FIRST") ||
                     !strcmp(next->lexeme, "READ_FIRST")  ||
                     !strcmp(next->lexeme, "NO_CHANGE"))) {
                    write_mode_value = next->lexeme;
                    advance(p);
                    next = peek(p);
                }

                /* Attribute block form {WRITE_MODE = ...;} - same logic as IN port */
                if (next->type == JZ_TOK_LBRACE) {
                    advance(p);
                    const JZToken *attr_key = peek(p);
                    if (attr_key->type != JZ_TOK_IDENTIFIER || !attr_key->lexeme ||
                        strcmp(attr_key->lexeme, "WRITE_MODE") != 0) {
                        jz_ast_free(mem);
                        parser_error(p, "expected WRITE_MODE attribute in MEM INOUT port");
                        return -1;
                    }
                    advance(p);
                    if (!match(p, JZ_TOK_OP_ASSIGN)) {
                        jz_ast_free(mem);
                        parser_error(p, "expected '=' after WRITE_MODE in MEM INOUT port");
                        return -1;
                    }
                    const JZToken *mode_tok = peek(p);
                    if (mode_tok->type != JZ_TOK_IDENTIFIER || !mode_tok->lexeme) {
                        jz_ast_free(mem);
                        parser_error(p, "expected write mode identifier in MEM INOUT port");
                        return -1;
                    }
                    write_mode_value = mode_tok->lexeme;
                    advance(p);
                    if (match(p, JZ_TOK_SEMICOLON)) { /* optional ';' before '}' */ }
                    if (!match(p, JZ_TOK_RBRACE)) {
                        jz_ast_free(mem);
                        parser_error(p, "expected '}' after WRITE_MODE in MEM INOUT port");
                        return -1;
                    }
                }

                if (!match(p, JZ_TOK_SEMICOLON)) {
                    jz_ast_free(mem);
                    parser_error(p, "expected ';' after MEM INOUT port");
                    return -1;
                }

                JZASTNode *pnode = jz_ast_new(JZ_AST_MEM_PORT, port_name->loc);
                if (!pnode) { jz_ast_free(mem); return -1; }
                jz_ast_set_block_kind(pnode, "INOUT");
                jz_ast_set_name(pnode, port_name->lexeme);
                if (write_mode_value) {
                    jz_ast_set_text(pnode, write_mode_value);
                }
                if (jz_ast_add_child(mem, pnode) != 0) {
                    jz_ast_free(pnode);
                    jz_ast_free(mem);
                    return -1;
                }
            } else {
                jz_ast_free(mem);
                parser_error(p, "expected IN/OUT/INOUT port in MEM block");
                return -1;
            }
        }

        if (!match(p, JZ_TOK_SEMICOLON)) {
            jz_ast_free(mem);
            parser_error(p, "expected ';' after MEM declaration");
            return -1;
        }

        if (jz_ast_add_child(parent, mem) != 0) {
            jz_ast_free(mem);
            return -1;
        }
    }
}
