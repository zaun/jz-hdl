/**
 * @file parser_project.c
 * @brief Parsing of @project blocks, @top bindings, BUS definitions, globals,
 *        and compilation-root structure.
 *
 * This file implements the highest-level structural parsing in the JZ HDL
 * compiler. It handles project definitions, top-level instantiations, BUS
 * declarations, global constants, and the orchestration of modules and
 * globals into a single project AST.
 *
 * The parser here is intentionally permissive in some contexts so that
 * semantic analysis can emit precise, rule-based diagnostics rather than
 * generic parse errors.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "parser_internal.h"

/**
 * @brief Parse the binding list inside a project-level @top block.
 *
 * This function parses structured port bindings of the form:
 *
 *   IN    [width] port = target;
 *   OUT   [width] port;
 *   INOUT [width] port = _;
 *
 * An optional leading OVERRIDE { ... } block is allowed and mirrors the
 * semantics of per-instance overrides in module-level @new instantiations.
 *
 * Parsing continues until the closing '}' of the @top block is encountered.
 *
 * @param p   Active parser
 * @param top Project top-instance AST node
 * @return 0 on success, -1 on error
 */
static int parse_project_top_binding_list(Parser *p, JZASTNode *top) {
    for (;;) {
        const JZToken *t = peek(p);
        if (t->type == JZ_TOK_RBRACE) {
            /* End of binding list. */
            advance(p); /* consume '}' */
            return 0;
        }
        if (t->type == JZ_TOK_EOF) {
            parser_error(p, "unterminated @top{...} port binding list (missing '}' )");
            return -1;
        }

        /* Optional leading OVERRIDE { ... } block, mirroring module-level @new. */
        if (t->type == JZ_TOK_KW_OVERRIDE) {
            const JZToken *ov_kw = t;
            advance(p); /* consume OVERRIDE */

            JZASTNode *ov_block = jz_ast_new(JZ_AST_CONST_BLOCK, ov_kw->loc);
            if (!ov_block) {
                return -1;
            }
            jz_ast_set_block_kind(ov_block, "OVERRIDE");

            if (!match(p, JZ_TOK_LBRACE)) {
                parser_error(p, "expected '{' after OVERRIDE in project @top binding list");
                jz_ast_free(ov_block);
                return -1;
            }

            if (parse_const_block_body(p, ov_block) != 0) {
                jz_ast_free(ov_block);
                return -1;
            }

            if (jz_ast_add_child(top, ov_block) != 0) {
                jz_ast_free(ov_block);
                return -1;
            }

            /* Continue parsing remaining bindings after OVERRIDE block. */
            continue;
        }

        /* BUS <bus_id> <ROLE> [N] <port_name> = <target>; */
        if (t->type == JZ_TOK_IDENTIFIER && t->lexeme && strcmp(t->lexeme, "BUS") == 0) {
            /* Consume 'BUS'. */
            advance(p);

            const JZToken *bus_id_tok = peek(p);
            if (!is_decl_identifier_token(bus_id_tok)) {
                parser_error(p, "expected BUS identifier after BUS keyword in project @top binding");
                return -1;
            }
            advance(p);

            const JZToken *role_tok = peek(p);
            if (!is_decl_identifier_token(role_tok) || !role_tok->lexeme) {
                parser_error(p, "expected BUS role (SOURCE/TARGET) after BUS id in project @top binding");
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
                    parser_error(p, "expected ']' after BUS array width expression in project @top binding");
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
                parser_error(p, "expected BUS port name after BUS role and optional [N] in project @top binding");
                return -1;
            }
            advance(p);

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

            /* Optional '= target' part for BUS binding (similar to regular ports). */
            if (match(p, JZ_TOK_OP_ASSIGN)) {
                const JZToken *target_tok = peek(p);
                /* Check for no-connect: _ is lexed as JZ_TOK_IDENTIFIER with lexeme "_" */
                if (target_tok->type == JZ_TOK_IDENTIFIER &&
                    target_tok->lexeme && strcmp(target_tok->lexeme, "_") == 0) {
                    /* Explicit no-connect: BUS ... = _; */
                    advance(p);
                    /* No-connect is already implied by absence of target; nothing to set. */
                } else {
                    /* TODO: handle non-trivial BUS binding targets if needed. */
                    parser_error(p, "BUS port bindings in @top currently only support '= _' (no-connect)");
                    jz_ast_free(decl);
                    return -1;
                }
            }

            if (!match(p, JZ_TOK_SEMICOLON)) {
                jz_ast_free(decl);
                parser_error(p, "expected ';' after BUS port declaration in project @top binding");
                return -1;
            }

            if (jz_ast_add_child(top, decl) != 0) {
                jz_ast_free(decl);
                return -1;
            }

            continue;
        }

        /* Direction: IN / OUT / INOUT */
        const char *dir_str = NULL;
        if (t->type == JZ_TOK_KW_IN) {
            dir_str = "IN";
        } else if (t->type == JZ_TOK_KW_OUT) {
            dir_str = "OUT";
        } else if (t->type == JZ_TOK_KW_INOUT) {
            dir_str = "INOUT";
        } else {
            parser_error(p, "expected IN/OUT/INOUT in project @top binding list");
            return -1;
        }
        advance(p);

        if (!match(p, JZ_TOK_LBRACKET)) {
            parser_error(p, "expected '[' after port direction in project @top binding");
            return -1;
        }

        size_t width_start = p->pos;
        while (peek(p)->type != JZ_TOK_EOF &&
               peek(p)->type != JZ_TOK_RBRACKET) {
            advance(p);
        }
        const JZToken *rb = peek(p);
        if (rb->type != JZ_TOK_RBRACKET) {
            parser_error(p, "expected ']' after port width expression in project @top binding");
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

        const JZToken *name_tok = peek(p);
        if (name_tok->type == JZ_TOK_NO_CONNECT) {
            /* Pure no-connect binding: IN [width] _; */
            advance(p);

            JZASTNode *decl = jz_ast_new(JZ_AST_PORT_DECL, name_tok->loc);
            if (!decl) {
                if (width_text) free(width_text);
                return -1;
            }
            jz_ast_set_name(decl, name_tok->lexeme ? name_tok->lexeme : "_");
            jz_ast_set_block_kind(decl, dir_str);
            if (width_text) {
                jz_ast_set_width(decl, width_text);
                free(width_text);
            }

            if (jz_ast_add_child(top, decl) != 0) {
                jz_ast_free(decl);
                return -1;
            }
        } else if (is_decl_identifier_token(name_tok)) {
            /* Structured binding: IN/OUT/INOUT [width] <port_name> [= <pin_expr_or_>]; */
            advance(p);

            JZASTNode *decl = jz_ast_new(JZ_AST_PORT_DECL, name_tok->loc);
            if (!decl) {
                if (width_text) free(width_text);
                return -1;
            }
            jz_ast_set_name(decl, name_tok->lexeme ? name_tok->lexeme : "");
            jz_ast_set_block_kind(decl, dir_str);
            if (width_text) {
                jz_ast_set_width(decl, width_text);
                /* width_text copied into decl; free local buffer. */
                free(width_text);
                width_text = NULL;
            }

            /* Optional '= target' part: target may be a pin/clock name, a
             * bus bit (e.g. btns[0]), or a concatenation expression using
             * braces, slices, and literals.
             */
            if (match(p, JZ_TOK_OP_ASSIGN)) {
                const JZToken *target_tok = peek(p);
                if (target_tok->type == JZ_TOK_NO_CONNECT) {
                    /* Explicit no-connect: IN [width] port = _; */
                    advance(p);
                    jz_ast_set_text(decl, "_");
                } else {
                    size_t expr_start = p->pos;
                    int brace_depth = 0;

                    /*
                     * Capture tokens for the RHS expression, allowing nested
                     * '{' ... '}' for concatenation. Stop at a semicolon or
                     * closing '}' that belongs to the @top block (brace_depth
                     * == 0), but leave that terminator unconsumed for the
                     * outer parser loop.
                     */
                    for (;;) {
                        const JZToken *cur = peek(p);
                        if (cur->type == JZ_TOK_EOF) {
                            break;
                        }
                        if (cur->type == JZ_TOK_LBRACE) {
                            ++brace_depth;
                            advance(p);
                            continue;
                        }
                        if (cur->type == JZ_TOK_RBRACE) {
                            if (brace_depth == 0) {
                                break; /* end of @top block or binding without ';' */
                            }
                            --brace_depth;
                            advance(p);
                            continue;
                        }
                        if (brace_depth == 0) {
                            if (cur->type == JZ_TOK_SEMICOLON) {
                                /* End of this binding expression. */
                                break;
                            }
                            if (cur->type == JZ_TOK_KW_IN ||
                                cur->type == JZ_TOK_KW_OUT ||
                                cur->type == JZ_TOK_KW_INOUT) {
                                /* Start of the next binding; leave this
                                 * token for the outer loop.
                                 */
                                break;
                            }
                        }
                        advance(p);
                    }

                    size_t expr_end = p->pos;
                    if (expr_start >= expr_end) {
                        jz_ast_free(decl);
                        parser_error(p, "expected pin expression after '=' in project @top binding");
                        return -1;
                    }

                    size_t buf_sz = 0;
                    for (size_t i = expr_start; i < expr_end; ++i) {
                        const JZToken *et = &p->tokens[i];
                        if (et->lexeme) buf_sz += strlen(et->lexeme) + 1;
                    }
                    if (buf_sz > 0) {
                        char *buf = (char *)malloc(buf_sz + 1);
                        if (!buf) {
                            jz_ast_free(decl);
                            return -1;
                        }
                        buf[0] = '\0';
                        for (size_t i = expr_start; i < expr_end; ++i) {
                            const JZToken *et = &p->tokens[i];
                            if (!et->lexeme) continue;
                            strcat(buf, et->lexeme);
                            strcat(buf, " ");
                        }
                        jz_ast_set_text(decl, buf);
                        free(buf);
                    }
                }
            } else {
                /* No explicit target; default to same name (port connected to
                 * identically named pin/clock).
                 */
                if (name_tok->lexeme) {
                    jz_ast_set_text(decl, name_tok->lexeme);
                }
            }

            if (jz_ast_add_child(top, decl) != 0) {
                jz_ast_free(decl);
                return -1;
            }
        } else {
            if (width_text) free(width_text);
            parser_error(p, "expected port name or '_' in project @top binding");
            return -1;
        }

        /* Semicolons are optional in the examples; consume if present. */
        match(p, JZ_TOK_SEMICOLON);
    }
}

static int parse_project_header_chip_attr(Parser *p, char **out_chip_id) {
    if (!out_chip_id) return -1;
    *out_chip_id = NULL;

    const JZToken *key = peek(p);
    if (key->type != JZ_TOK_IDENTIFIER || !key->lexeme) {
        parser_error(p, "expected CHIP attribute name after @project(");
        return -1;
    }

    int is_chip = 0;
    if (key->lexeme) {
        const char *a = key->lexeme;
        const char *b = "CHIP";
        while (*a && *b) {
            if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
                break;
            }
            ++a;
            ++b;
        }
        if (*a == '\0' && *b == '\0') {
            is_chip = 1;
        }
    }

    if (!is_chip) {
        parser_error(p, "unknown @project header attribute (expected CHIP)");
        return -1;
    }
    advance(p);

    if (!match(p, JZ_TOK_OP_ASSIGN)) {
        parser_error(p, "expected '=' after CHIP in @project header");
        return -1;
    }

    size_t scan_pos = p->pos;
    size_t value_len = 0;
    while (scan_pos < p->count) {
        const JZToken *t = &p->tokens[scan_pos];
        if (t->type == JZ_TOK_RPAREN) break;
        if (t->type == JZ_TOK_IDENTIFIER ||
            t->type == JZ_TOK_NUMBER ||
            t->type == JZ_TOK_STRING ||
            t->type == JZ_TOK_OP_MINUS ||
            t->type == JZ_TOK_DOT) {
            if (t->lexeme) {
                value_len += strlen(t->lexeme);
            }
            scan_pos++;
            continue;
        }
        parser_error(p, "invalid CHIP value in @project header");
        return -1;
    }

    if (scan_pos >= p->count || p->tokens[scan_pos].type != JZ_TOK_RPAREN) {
        parser_error(p, "unterminated @project header (missing ')')");
        return -1;
    }
    if (value_len == 0) {
        parser_error(p, "missing CHIP value in @project header");
        return -1;
    }

    char *buf = (char *)malloc(value_len + 1);
    if (!buf) return -1;
    buf[0] = '\0';

    while (peek(p)->type != JZ_TOK_RPAREN && peek(p)->type != JZ_TOK_EOF) {
        const JZToken *t = peek(p);
        if (t->lexeme) {
            strcat(buf, t->lexeme);
        }
        advance(p);
    }

    if (!match(p, JZ_TOK_RPAREN)) {
        free(buf);
        parser_error(p, "unterminated @project header (missing ')')");
        return -1;
    }

    *out_chip_id = buf;
    return 0;
}

/**
 * @brief Parse a project-level @top instantiation.
 *
 * Supported forms:
 *   @top <module> { ... }
 *   @top <instance> <module> { ... }
 *
 * @param p Active parser
 * @return Project top-instance AST node, or NULL on error
 */
static JZASTNode *parse_project_top_new(Parser *p) {
    const JZToken *kw = &p->tokens[p->pos - 1]; /* @top already consumed */

    const JZToken *first_id = peek(p);
    if (!is_decl_identifier_token(first_id)) {
        parser_error(p, "expected module or instance name after @top in project");
        return NULL;
    }
    advance(p);

    /* Support both forms:
       - @top <top_module> { ... }
       - @top <instance_name> <top_module> { ... }
    */
    const JZToken *maybe_second = peek(p);
    const JZToken *module_tok = NULL;
    if (maybe_second->type == JZ_TOK_LBRACE) {
        /* Simple form: first identifier is the top module name. */
        module_tok = first_id;
    } else if (is_decl_identifier_token(maybe_second)) {
        /* Two identifiers: instance then module. The second is the top module. */
        module_tok = maybe_second;
        advance(p); /* consume module identifier */
    } else {
        parser_error(p, "expected module name or '{' after @top identifier in project");
        return NULL;
    }

    JZASTNode *top = jz_ast_new(JZ_AST_PROJECT_TOP_INSTANCE, kw->loc);
    if (!top) return NULL;
    /* Store the top module name as the node name; instance name (if any) can be recovered from bindings later. */
    jz_ast_set_name(top, module_tok->lexeme);

    if (!match(p, JZ_TOK_LBRACE)) {
        parser_error(p, "expected '{' after @top <top_module> in project");
        jz_ast_free(top);
        return NULL;
    }

    /* Parse structured IN/OUT/INOUT bindings until the closing '}'. */
    if (parse_project_top_binding_list(p, top) != 0) {
        jz_ast_free(top);
        return NULL;
    }

    return top;
}

/**
 * @brief Parse a BUS definition inside a project.
 *
 * BUS blocks define named signal groups with directional members.
 *
 * @param p      Active parser
 * @param bus_kw BUS keyword token
 * @return BUS AST node, or NULL on error
 */
JZASTNode *parse_bus_definition(Parser *p, const JZToken *bus_kw) {
    if (!bus_kw) return NULL;

    const JZToken *name_tok = peek(p);
    if (!is_decl_identifier_token(name_tok)) {
        parser_error(p, "expected BUS identifier after BUS keyword in project");
        return NULL;
    }
    advance(p);

    JZASTNode *bus = jz_ast_new(JZ_AST_BUS_BLOCK, bus_kw->loc);
    if (!bus) return NULL;
    jz_ast_set_name(bus, name_tok->lexeme);
    jz_ast_set_block_kind(bus, "BUS");

    if (!match(p, JZ_TOK_LBRACE)) {
        parser_error(p, "expected '{' after BUS <name> in project");
        jz_ast_free(bus);
        return NULL;
    }

    for (;;) {
        const JZToken *t = peek(p);
        if (t->type == JZ_TOK_RBRACE) {
            advance(p);
            return bus;
        }
        if (t->type == JZ_TOK_EOF) {
            parser_error(p, "unterminated BUS block (missing '}' )");
            jz_ast_free(bus);
            return NULL;
        }

        /* Direction: IN / OUT / INOUT */
        const char *dir_str = NULL;
        if (t->type == JZ_TOK_KW_IN) {
            dir_str = "IN";
        } else if (t->type == JZ_TOK_KW_OUT) {
            dir_str = "OUT";
        } else if (t->type == JZ_TOK_KW_INOUT) {
            dir_str = "INOUT";
        } else {
            parser_error(p, "expected IN/OUT/INOUT in BUS block");
            jz_ast_free(bus);
            return NULL;
        }
        advance(p);

        if (!match(p, JZ_TOK_LBRACKET)) {
            parser_error(p, "expected '[' after BUS signal direction");
            jz_ast_free(bus);
            return NULL;
        }

        size_t width_start = p->pos;
        while (peek(p)->type != JZ_TOK_EOF &&
               peek(p)->type != JZ_TOK_RBRACKET) {
            advance(p);
        }
        const JZToken *rb = peek(p);
        if (rb->type != JZ_TOK_RBRACKET) {
            parser_error(p, "expected ']' after BUS signal width expression");
            jz_ast_free(bus);
            return NULL;
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
                if (!width_text) {
                    jz_ast_free(bus);
                    return NULL;
                }
                width_text[0] = '\0';
                for (size_t i = width_start; i < width_end; ++i) {
                    const JZToken *wt = &p->tokens[i];
                    if (!wt->lexeme) continue;
                    strcat(width_text, wt->lexeme);
                    strcat(width_text, " ");
                }
            }
        }

        const JZToken *name_tok = peek(p);
        if (!is_decl_identifier_token(name_tok)) {
            if (width_text) free(width_text);
            parser_error(p, "expected BUS signal name after width expression");
            jz_ast_free(bus);
            return NULL;
        }
        advance(p);

        if (!match(p, JZ_TOK_SEMICOLON)) {
            if (width_text) free(width_text);
            parser_error(p, "expected ';' after BUS signal declaration");
            jz_ast_free(bus);
            return NULL;
        }

        JZASTNode *decl = jz_ast_new(JZ_AST_BUS_DECL, name_tok->loc);
        if (!decl) {
            if (width_text) free(width_text);
            jz_ast_free(bus);
            return NULL;
        }
        jz_ast_set_name(decl, name_tok->lexeme);
        jz_ast_set_block_kind(decl, dir_str);
        if (width_text) {
            jz_ast_set_width(decl, width_text);
            free(width_text);
        }

        if (jz_ast_add_child(bus, decl) != 0) {
            jz_ast_free(decl);
            jz_ast_free(bus);
            return NULL;
        }
    }
}

/**
 * @brief Parse a @global block.
 *
 * Expects the @global keyword to have already been consumed. A global block
 * defines named constant expressions that are visible across the entire
 * project and all modules.
 *
 * The body of a @global block consists of one or more constant assignments:
 *
 *   <name> = <expression>;
 *
 * Parsing continues until the matching @endglob directive is encountered.
 * Global blocks are only permitted at the compilation-root level; any use
 * inside a @project or @module is diagnosed as an invalid context elsewhere.
 *
 * @param p Active parser
 * @return Global block AST node, or NULL on error
 */
JZASTNode *parse_global(Parser *p)
{
    const JZToken *kw = &p->tokens[p->pos - 1]; /* already consumed @global */

    const JZToken *name_tok = peek(p);
    if (!is_decl_identifier_token(name_tok)) {
        parser_error(p, "expected identifier after @global");
        return NULL;
    }
    advance(p);

    JZASTNode *glob = jz_ast_new(JZ_AST_GLOBAL_BLOCK, kw->loc);
    if (!glob) return NULL;
    jz_ast_set_name(glob, name_tok->lexeme);

    while (peek(p)->type != JZ_TOK_EOF &&
           peek(p)->type != JZ_TOK_KW_ENDGLOB) {
        const JZToken *t = peek(p);
        if (t->type == JZ_TOK_SEMICOLON) {
            advance(p);
            continue;
        }

        const JZToken *id_tok = peek(p);
        if (!is_decl_identifier_token(id_tok)) {
            parser_error(p, "expected identifier in @global block");
            jz_ast_free(glob);
            return NULL;
        }
        advance(p);

        if (!match(p, JZ_TOK_OP_ASSIGN)) {
            parser_error(p, "expected '=' after global constant name");
            jz_ast_free(glob);
            return NULL;
        }

        size_t expr_start = p->pos;
        while (peek(p)->type != JZ_TOK_EOF &&
               peek(p)->type != JZ_TOK_SEMICOLON &&
               peek(p)->type != JZ_TOK_KW_ENDGLOB) {
            advance(p);
        }

        const JZToken *semi_or_end = peek(p);
        if (semi_or_end->type == JZ_TOK_SEMICOLON) {
            /* expression ends before ';' */
        } else if (semi_or_end->type == JZ_TOK_KW_ENDGLOB) {
            /* expression ends immediately before @endglob */
        } else {
            parser_error(p, "expected ';' after global constant expression");
            jz_ast_free(glob);
            return NULL;
        }

        size_t expr_end = p->pos; /* tokens [expr_start, expr_end) form the expression */
        if (semi_or_end->type == JZ_TOK_SEMICOLON) {
            advance(p); /* consume ';' */
        }

        JZASTNode *decl = jz_ast_new(JZ_AST_CONST_DECL, id_tok->loc);
        if (!decl) {
            jz_ast_free(glob);
            return NULL;
        }
        jz_ast_set_name(decl, id_tok->lexeme);

        if (expr_start < expr_end) {
            size_t buf_sz = 0;
            for (size_t i = expr_start; i < expr_end; ++i) {
                const JZToken *et = &p->tokens[i];
                if (et->lexeme) buf_sz += strlen(et->lexeme) + 1;
            }
            if (buf_sz > 0) {
                char *buf = (char *)malloc(buf_sz + 1);
                if (!buf) {
                    jz_ast_free(decl);
                    jz_ast_free(glob);
                    return NULL;
                }
                buf[0] = '\0';
                for (size_t i = expr_start; i < expr_end; ++i) {
                    const JZToken *et = &p->tokens[i];
                    if (!et->lexeme) continue;
                    strcat(buf, et->lexeme);
                    strcat(buf, " ");
                }
                jz_ast_set_text(decl, buf);
                free(buf);
            }
        }

        if (jz_ast_add_child(glob, decl) != 0) {
            jz_ast_free(decl);
            jz_ast_free(glob);
            return NULL;
        }
    }

    if (!match(p, JZ_TOK_KW_ENDGLOB)) {
        parser_error(p, "missing @endglob for global block");
        jz_ast_free(glob);
        return NULL;
    }

    return glob;
}

/**
 * @brief Parse an entire compilation unit.
 *
 * This is the top-level entry point for parsing a JZ HDL source file.
 * It consumes a pre-tokenized input stream and produces a fully-formed
 * project AST.
 *
 * Responsibilities:
 * - Parse top-level @module and @global blocks
 * - Enforce that exactly one @project block exists
 * - Attach parsed modules and globals to the project
 * - Report invalid top-level directives
 *
 * The returned AST node is the @project node; any temporary root used
 * during parsing is discarded before returning.
 *
 * @param filename     Source filename (used for diagnostics)
 * @param stream       Token stream produced by the lexer
 * @param diagnostics  Diagnostic sink for rule-based errors
 * @return Project AST node, or NULL on error
 */
JZASTNode *jz_parse_file(const char *filename,
                         const JZTokenStream *stream,
                         JZDiagnosticList *diagnostics) {
    if (!stream || stream->count == 0) return NULL;

    Parser p;
    p.filename = filename;
    p.tokens = stream->tokens;
    p.count = stream->count;
    p.pos = 0;
    p.diagnostics = diagnostics;

    JZLocation root_loc = { filename, 1, 1 };
    JZASTNode *root = jz_ast_new(JZ_AST_PROJECT, root_loc);
    if (!root) return NULL;

    int saw_project = 0;
    int saw_testbench = 0;
    JZASTNode *proj_node = NULL;

    while (peek(&p)->type != JZ_TOK_EOF) {
        const JZToken *t = peek(&p);
        if (t->type == JZ_TOK_KW_MODULE) {
            advance(&p);
            JZASTNode *mod = parse_module(&p);
            if (!mod) { jz_ast_free(root); return NULL; }
            jz_ast_add_child(root, mod);
        } else if (t->type == JZ_TOK_KW_GLOBAL) {
            advance(&p);
            JZASTNode *glob = parse_global(&p);
            if (!glob) { jz_ast_free(root); return NULL; }
            jz_ast_add_child(root, glob);
        } else if (t->type == JZ_TOK_KW_PROJECT) {
            if (saw_testbench) {
                parser_report_rule(&p, t, "TB_PROJECT_MIXED",
                                   "this file already contains a @testbench; @project and @testbench\n"
                                   "cannot coexist in the same file");
                jz_ast_free(root);
                return NULL;
            }
            if (saw_project) {
                /* PROJECT_MULTIPLE_PER_FILE: multiple @project directives in the
                 * same compilation root are forbidden by the spec.
                 */
                parser_report_rule(&p,
                                   t,
                                   "PROJECT_MULTIPLE_PER_FILE",
                                   "a second @project was found but only one is allowed per file;\n"
                                   "merge the project definitions or split into separate files");
                jz_ast_free(root);
                return NULL;
            }
            saw_project = 1;
            advance(&p);
            JZASTNode *proj = parse_project(&p);
            if (!proj) { jz_ast_free(root); return NULL; }
            jz_ast_add_child(root, proj);
            proj_node = proj;
        } else if (t->type == JZ_TOK_KW_TESTBENCH) {
            if (saw_project) {
                parser_report_rule(&p, t, "TB_PROJECT_MIXED",
                                   "this file already contains a @project; @project and @testbench\n"
                                   "cannot coexist in the same file");
                jz_ast_free(root);
                return NULL;
            }
            saw_testbench = 1;
            advance(&p);
            JZASTNode *tb = parse_testbench(&p);
            if (!tb) { jz_ast_free(root); return NULL; }
            /* Lift MODULE, GLOBAL, and BUS_BLOCK children from tb to root
             * so they are visible for symbol table building and IR construction. */
            for (size_t ci = 0; ci < tb->child_count; ) {
                JZASTNode *c = tb->children[ci];
                if (c && (c->type == JZ_AST_MODULE ||
                          c->type == JZ_AST_GLOBAL_BLOCK ||
                          c->type == JZ_AST_BUS_BLOCK)) {
                    jz_ast_add_child(root, c);
                    for (size_t j = ci; j + 1 < tb->child_count; j++)
                        tb->children[j] = tb->children[j + 1];
                    tb->child_count--;
                } else {
                    ci++;
                }
            }
            jz_ast_add_child(root, tb);
        } else if (t->type == JZ_TOK_KW_SIMULATION) {
            if (saw_project) {
                parser_report_rule(&p, t, "SIM_PROJECT_MIXED",
                                   "this file already contains a @project; @project and @simulation\n"
                                   "cannot coexist in the same file");
                jz_ast_free(root);
                return NULL;
            }
            saw_testbench = 1; /* reuse flag for sim/tb files */
            advance(&p);
            JZASTNode *sim = parse_simulation(&p);
            if (!sim) { jz_ast_free(root); return NULL; }
            /* Lift MODULE, GLOBAL, and BUS_BLOCK children from sim to root */
            for (size_t ci = 0; ci < sim->child_count; ) {
                JZASTNode *c = sim->children[ci];
                if (c && (c->type == JZ_AST_MODULE ||
                          c->type == JZ_AST_GLOBAL_BLOCK ||
                          c->type == JZ_AST_BUS_BLOCK)) {
                    jz_ast_add_child(root, c);
                    for (size_t j = ci; j + 1 < sim->child_count; j++)
                        sim->children[j] = sim->children[j + 1];
                    sim->child_count--;
                } else {
                    ci++;
                }
            }
            jz_ast_add_child(root, sim);
        } else if (t->type == JZ_TOK_IDENTIFIER && t->lexeme && strcmp(t->lexeme, "BUS") == 0) {
            /* File-level BUS definition (used by testbench files). */
            advance(&p);
            JZASTNode *bus = parse_bus_definition(&p, t);
            if (!bus) { jz_ast_free(root); return NULL; }
            jz_ast_add_child(root, bus);
        } else if (t->type == JZ_TOK_KW_TEMPLATE) {
            /* File-scoped template definition. */
            advance(&p);
            JZASTNode *tmpl = parse_template_def(&p);
            if (!tmpl) { jz_ast_free(root); return NULL; }
            jz_ast_add_child(root, tmpl);
        } else if (t->type == JZ_TOK_KW_SCRATCH) {
            /* @scratch outside template body. */
            parser_report_rule(&p, t, "TEMPLATE_SCRATCH_OUTSIDE",
                               "@scratch found at file top level; @scratch declares temporary wires\n"
                               "and may only be used inside a @template body");
            advance(&p);
        } else if (t->type == JZ_TOK_KW_ENDMOD ||
                   t->type == JZ_TOK_KW_ENDPROJ ||
                   t->type == JZ_TOK_KW_ENDGLOB ||
                   t->type == JZ_TOK_KW_ENDSIM ||
                   t->type == JZ_TOK_KW_BLACKBOX ||
                   t->type == JZ_TOK_KW_NEW ||
                   t->type == JZ_TOK_KW_IMPORT ||
                   t->type == JZ_TOK_KW_CHECK) {
            /* Structural directives at top level that are not part of a
             * well-formed @module/@project are considered invalid context.
             * For @import specifically, we surface IMPORT_OUTSIDE_PROJECT.
             */
            const char *rule_id = (t->type == JZ_TOK_KW_IMPORT)
                                ? "IMPORT_OUTSIDE_PROJECT"
                                : "DIRECTIVE_INVALID_CONTEXT";
            const char *fallback = (t->type == JZ_TOK_KW_IMPORT)
                                 ? "@import may only be used inside a @project block;\n"
                                   "wrap your modules and imports in @project ... @endproj"
                                 : "this directive is only valid inside a @module or @project block;\n"
                                   "it cannot appear at file top level";
            parser_report_rule(&p, t, rule_id, fallback);
            advance(&p);
        } else {
            /* Skip unknown top-level tokens for now. */
            advance(&p);
        }
    }

    /* Testbench/simulation files: return the synthetic root directly. */
    if (saw_testbench) {
        /* The root is a synthetic PROJECT node — repurpose it as a container.
         * Move modules and globals as children for lookup during semantic checks.
         */
        return root;
    }

    if (!proj_node) {
        /* If the file contains modules but no @project, return the
         * synthetic root.  This allows standalone module files (used via
         * @import) to be parsed and analysed individually — useful for
         * the LSP and other tooling that opens single files. */
        if (root->child_count > 0) {
            return root;
        }
        parser_error(&p, "missing @project definition in compilation root");
        jz_ast_free(root);
        return NULL;
    }

    /* Move all top-level modules under the project and discard the synthetic root. */
    for (size_t i = 0; i < root->child_count; ++i) {
        JZASTNode *child = root->children[i];
        root->children[i] = NULL; /* detach from temporary root */
        if (!child || child == proj_node) {
            continue;
        }
        if (child->type == JZ_AST_MODULE ||
            child->type == JZ_AST_GLOBAL_BLOCK ||
            child->type == JZ_AST_TEMPLATE_DEF ||
            child->type == JZ_AST_BUS_BLOCK) {
            if (jz_ast_add_child(proj_node, child) != 0) {
                jz_ast_free(child);
                jz_ast_free(proj_node);
                jz_ast_free(root);
                return NULL;
            }
        } else {
            /* Unexpected top-level child; free it to avoid leaks. */
            jz_ast_free(child);
        }
    }
    root->child_count = 0;
    jz_ast_free(root);
    return proj_node;
}

/**
 * @brief Parse a compile-time @check directive.
 *
 * The @check directive has the form:
 *
 *   @check(<condition>, "message");
 *
 * The condition is parsed as a full expression and stored as both a
 * structured AST child and a reconstructed raw-text expression so that
 * existing constant-evaluation logic can be reused during semantic checks.
 *
 * @param p Active parser
 * @return Check AST node, or NULL on error
 */
JZASTNode *parse_check(Parser *p) {
    const JZToken *kw = &p->tokens[p->pos - 1]; /* @check already consumed */

    if (!match(p, JZ_TOK_LPAREN)) {
        parser_error(p, "expected '(' after @check");
        return NULL;
    }

    size_t expr_start = p->pos;
    JZASTNode *expr = parse_expression(p);
    if (!expr) {
        return NULL;
    }
    size_t expr_end = p->pos;

    if (!match(p, JZ_TOK_COMMA)) {
        jz_ast_free(expr);
        parser_error(p, "expected ',' after @check condition expression");
        return NULL;
    }

    const JZToken *msg_tok = peek(p);
    if (msg_tok->type != JZ_TOK_STRING || !msg_tok->lexeme) {
        jz_ast_free(expr);
        parser_error(p, "expected string literal message in @check");
        return NULL;
    }
    const char *msg = msg_tok->lexeme;
    advance(p);

    if (!match(p, JZ_TOK_RPAREN)) {
        jz_ast_free(expr);
        parser_error(p, "expected ')' after @check arguments");
        return NULL;
    }

    if (!match(p, JZ_TOK_SEMICOLON)) {
        jz_ast_free(expr);
        parser_error(p, "expected ';' after @check");
        return NULL;
    }

    /* Reconstruct the condition expression text from the original tokens so
     * that semantic passes can reuse the existing constant-eval helpers that
     * operate on raw strings.
     */
    char *expr_text = NULL;
    if (expr_start < expr_end) {
        size_t buf_sz = 0;
        for (size_t i = expr_start; i < expr_end; ++i) {
            const JZToken *t = &p->tokens[i];
            if (t->lexeme) buf_sz += strlen(t->lexeme) + 1;
        }
        if (buf_sz > 0) {
            expr_text = (char *)malloc(buf_sz + 1);
            if (!expr_text) {
                jz_ast_free(expr);
                return NULL;
            }
            expr_text[0] = '\0';
            for (size_t i = expr_start; i < expr_end; ++i) {
                const JZToken *t = &p->tokens[i];
                if (!t->lexeme) continue;
                strcat(expr_text, t->lexeme);
                strcat(expr_text, " ");
            }
        }
    }

    JZASTNode *check = jz_ast_new(JZ_AST_CHECK, kw->loc);
    if (!check) {
        if (expr_text) free(expr_text);
        jz_ast_free(expr);
        return NULL;
    }

    if (expr_text) {
        jz_ast_set_width(check, expr_text);
        free(expr_text);
    }
    if (msg) {
        jz_ast_set_text(check, msg);
    }

    if (jz_ast_add_child(check, expr) != 0) {
        jz_ast_free(check);
        jz_ast_free(expr);
        return NULL;
    }

    return check;
}

/**
 * @brief Parse a @project block.
 *
 * Expects the @project keyword to have already been consumed. A project
 * block defines the structural composition of a design, including imports,
 * configuration, clocks, pins, maps, blackboxes, BUS definitions, and a
 * single top-level instantiation.
 *
 * Enforced rules include:
 * - Imports must appear at the top of the project
 * - At most one CONFIG block is allowed
 * - At most one top-level @top instantiation is allowed
 * - Control-flow statements are forbidden at project scope
 *
 * Parsing continues until the matching @endproj directive is encountered.
 *
 * @param p Active parser
 * @return Project AST node, or NULL on error
 */
JZASTNode *parse_project(Parser *p) {
    const JZToken *kw = &p->tokens[p->pos - 1]; /* already consumed @project */

    char *chip_id = NULL;
    if (match(p, JZ_TOK_LPAREN)) {
        if (parse_project_header_chip_attr(p, &chip_id) != 0) {
            free(chip_id);
            return NULL;
        }
    }

    const JZToken *name_tok = peek(p);
    if (!is_decl_identifier_token(name_tok)) {
        parser_error(p, "expected identifier after @project");
        return NULL;
    }
    advance(p);

    JZASTNode *proj = jz_ast_new(JZ_AST_PROJECT, kw->loc);
    if (!proj) return NULL;
    jz_ast_set_name(proj, name_tok->lexeme);
    if (chip_id) {
        jz_ast_set_text(proj, chip_id);
        free(chip_id);
    }

    int saw_non_import = 0;
    int saw_config = 0;
    int saw_top_new = 0;

    while (peek(p)->type != JZ_TOK_EOF && peek(p)->type != JZ_TOK_KW_ENDPROJ) {
        const JZToken *t = peek(p);

        if (t->type == JZ_TOK_KW_IMPORT) {
            if (saw_non_import) {
                /* IMPORT_NOT_AT_PROJECT_TOP: imports must appear immediately
                 * after @project and before CONFIG/CLOCKS/PIN/MAP/blackbox/top-level @new.
                 */
                parser_report_rule(p,
                                   t,
                                   "IMPORT_NOT_AT_PROJECT_TOP",
                                   "@import must appear before any CONFIG, CLOCKS, PIN_MAP, @blackbox,\n"
                                   "or @top blocks; move this @import to the top of the @project body");
                jz_ast_free(proj);
                return NULL;
            }
            advance(p);
            const JZToken *path_tok = peek(p);
            if (path_tok->type != JZ_TOK_STRING || !path_tok->lexeme) {
                parser_error(p, "expected string after @import");
                jz_ast_free(proj);
                return NULL;
            }
            const char *path = path_tok->lexeme;
            advance(p);
            if (import_modules_from_path(p, proj, path, t) != 0) {
                jz_ast_free(proj);
                return NULL;
            }
            match(p, JZ_TOK_SEMICOLON); /* optional */
            continue;
        }

        /* Any non-import token marks the end of the import region. */
        saw_non_import = 1;

        if (t->type == JZ_TOK_KW_CONFIG) {
            if (saw_config) {
                parser_error(p, "multiple CONFIG blocks in a single project are not allowed");
                jz_ast_free(proj);
                return NULL;
            }
            saw_config = 1;
            advance(p);
            JZASTNode *blk = parse_block(p, t, "CONFIG", JZ_AST_CONFIG_BLOCK);
            if (!blk) { jz_ast_free(proj); return NULL; }
            jz_ast_add_child(proj, blk);
        } else if (t->type == JZ_TOK_IDENTIFIER && t->lexeme && strcmp(t->lexeme, "BUS") == 0) {
            advance(p);
            JZASTNode *bus = parse_bus_definition(p, t);
            if (!bus) { jz_ast_free(proj); return NULL; }
            jz_ast_add_child(proj, bus);
        } else if (t->type == JZ_TOK_KW_CLOCKS) {
            advance(p);
            JZASTNode *blk = parse_block(p, t, "CLOCKS", JZ_AST_CLOCKS_BLOCK);
            if (!blk) { jz_ast_free(proj); return NULL; }
            jz_ast_add_child(proj, blk);
        } else if (t->type == JZ_TOK_KW_IN_PINS) {
            advance(p);
            JZASTNode *blk = parse_block(p, t, "IN_PINS", JZ_AST_IN_PINS_BLOCK);
            if (!blk) { jz_ast_free(proj); return NULL; }
            jz_ast_add_child(proj, blk);
        } else if (t->type == JZ_TOK_KW_OUT_PINS) {
            advance(p);
            JZASTNode *blk = parse_block(p, t, "OUT_PINS", JZ_AST_OUT_PINS_BLOCK);
            if (!blk) { jz_ast_free(proj); return NULL; }
            jz_ast_add_child(proj, blk);
        } else if (t->type == JZ_TOK_KW_INOUT_PINS) {
            advance(p);
            JZASTNode *blk = parse_block(p, t, "INOUT_PINS", JZ_AST_INOUT_PINS_BLOCK);
            if (!blk) { jz_ast_free(proj); return NULL; }
            jz_ast_add_child(proj, blk);
        } else if (t->type == JZ_TOK_KW_MAP) {
            advance(p);
            JZASTNode *blk = parse_block(p, t, "MAP", JZ_AST_MAP_BLOCK);
            if (!blk) { jz_ast_free(proj); return NULL; }
            jz_ast_add_child(proj, blk);
        } else if (t->type == JZ_TOK_KW_CLOCK_GEN) {
            advance(p);
            JZASTNode *cgen = parse_clock_gen_block(p, t);
            if (!cgen) { jz_ast_free(proj); return NULL; }
            jz_ast_add_child(proj, cgen);
        } else if (t->type == JZ_TOK_KW_CHECK) {
            /* Project-level @check compile-time assertion. */
            advance(p);
            JZASTNode *chk = parse_check(p);
            if (!chk) { jz_ast_free(proj); return NULL; }
            jz_ast_add_child(proj, chk);
        } else if (t->type == JZ_TOK_KW_BLACKBOX) {
            advance(p);
            const JZToken *name = peek(p);
            if (!is_decl_identifier_token(name)) {
                parser_error(p, "expected identifier after @blackbox");
                jz_ast_free(proj);
                return NULL;
            }
            advance(p);
            JZASTNode *bb = jz_ast_new(JZ_AST_BLACKBOX, t->loc);
            if (!bb) { jz_ast_free(proj); return NULL; }
            jz_ast_set_name(bb, name->lexeme);
            if (!match(p, JZ_TOK_LBRACE)) {
                parser_error(p, "expected '{' after @blackbox name");
                jz_ast_free(bb);
                jz_ast_free(proj);
                return NULL;
            }

            if (parse_blackbox_body(p, bb) != 0) {
                jz_ast_free(bb);
                jz_ast_free(proj);
                return NULL;
            }

            jz_ast_add_child(proj, bb);
        } else if (t->type == JZ_TOK_KW_TOP) {
            /* Top-level @top <top_module_name> { ... } inside a project. */
            if (saw_top_new) {
                parser_error(p, "multiple top-level @top instantiations in a single project are not allowed");
                jz_ast_free(proj);
                return NULL;
            }
            saw_top_new = 1;
            advance(p);
            JZASTNode *top = parse_project_top_new(p);
            if (!top) { jz_ast_free(proj); return NULL; }
            jz_ast_add_child(proj, top);
        } else if (t->type == JZ_TOK_KW_GLOBAL ||
                   t->type == JZ_TOK_KW_ENDGLOB) {
            /* Global blocks are only allowed at compilation-root top level,
             * not inside a @project body.
             */
            parser_report_rule(p,
                               t,
                               "DIRECTIVE_INVALID_CONTEXT",
                               "@global/@endglob may only appear at file top level, not inside\n"
                               "a @project body; move the @global block outside @project...@endproj");
            advance(p);
        } else if (t->type == JZ_TOK_KW_IF ||
                   t->type == JZ_TOK_KW_ELIF ||
                   t->type == JZ_TOK_KW_ELSE ||
                   t->type == JZ_TOK_KW_SELECT ||
                   t->type == JZ_TOK_KW_CASE ||
                   t->type == JZ_TOK_KW_DEFAULT) {
            /* Control-flow is forbidden at project scope; only structural
             * declarations and the top-level @new are valid here.
             */
            parser_report_rule(p,
                               t,
                               "CONTROL_FLOW_OUTSIDE_BLOCK",
                               "IF/ELIF/ELSE/SELECT/CASE/DEFAULT are only valid inside\n"
                               "ASYNCHRONOUS or SYNCHRONOUS blocks, not at @project scope");
            advance(p);
        } else if (t->type == JZ_TOK_KW_CHECK) {
            /* @check is only allowed at module or project scope, not nested
             * inside executable blocks or other directives.
             */
            parser_report_rule(p,
                               t,
                               "DIRECTIVE_INVALID_CONTEXT",
                               "@check is not valid at this location inside the @project body;\n"
                               "place @check at module scope or directly inside @project before @endproj");
            advance(p);
        } else {
            advance(p);
        }
    }

    if (!match(p, JZ_TOK_KW_ENDPROJ)) {
        parser_report_rule(p,
                           peek(p),
                           "PROJECT_MISSING_ENDPROJ",
                           "@project block was not closed; add @endproj after the last\n"
                           "declaration to terminate the project definition");
        jz_ast_free(proj);
        return NULL;
    }

    return proj;
}
