/**
 * @file parser_instance.c
 * @brief Parsing of module instantiations (@new) and instance port bindings.
 *
 * This file implements parsing logic for module instantiation statements
 * using the @new directive, including optional instance arrays, per-instance
 * constant overrides, and structured port-to-signal bindings.
 *
 * The grammar handled here is intentionally restricted to ensure that
 * instance bindings reference only valid parent signals and slices, with
 * more complex validation deferred to semantic analysis.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser_internal.h"

static JZASTNode *clone_expr_tree(const JZASTNode *src) {
    if (!src) return NULL;
    JZASTNode *copy = jz_ast_new(src->type, src->loc);
    if (!copy) return NULL;

    if (src->name) {
        jz_ast_set_name(copy, src->name);
    }
    if (src->text) {
        jz_ast_set_text(copy, src->text);
    }
    if (src->width) {
        jz_ast_set_width(copy, src->width);
    }
    if (src->block_kind) {
        jz_ast_set_block_kind(copy, src->block_kind);
    }

    for (size_t i = 0; i < src->child_count; ++i) {
        JZASTNode *child_copy = clone_expr_tree(src->children[i]);
        if (!child_copy) {
            jz_ast_free(copy);
            return NULL;
        }
        if (jz_ast_add_child(copy, child_copy) != 0) {
            jz_ast_free(child_copy);
            jz_ast_free(copy);
            return NULL;
        }
    }
    return copy;
}

/**
 * @brief Parse a restricted parent signal expression for @new bindings.
 *
 * This helper parses the right-hand side of a module instance port binding.
 * Only signal-like expressions are permitted:
 * - A simple identifier
 * - A qualified identifier using '.'
 * - An optional single slice [msb:lsb] or index [idx]
 *
 * CONFIG-qualified identifiers and arbitrary expressions are deliberately
 * disallowed so that illegal connections are caught during parsing.
 *
 * @param p Active parser
 * @return Expression AST node, or NULL on error
 */
static JZASTNode *parse_instance_parent_signal_expr(Parser *p)
{
    /* Parse a restricted expression used on the RHS of @new bindings:
       - identifier (including CONFIG is *not* allowed here)
       - qualified identifier via '.'
       - optional single [msb:lsb] or [idx] slice.
    */
    const JZToken *t = peek(p);
    if (t->type == JZ_TOK_NUMBER || t->type == JZ_TOK_SIZED_NUMBER) {
        JZASTNode *lit = jz_ast_new(JZ_AST_EXPR_LITERAL, t->loc);
        if (!lit) return NULL;
        if (t->lexeme) {
            jz_ast_set_text(lit, t->lexeme);
        }
        advance(p);
        return lit;
    }

    /* Concatenation: { expr, expr, ... } */
    if (t->type == JZ_TOK_LBRACE) {
        JZLocation loc = t->loc;
        advance(p); /* consume '{' */
        JZASTNode *concat = jz_ast_new(JZ_AST_EXPR_CONCAT, loc);
        if (!concat) return NULL;
        if (peek(p)->type != JZ_TOK_RBRACE) {
            for (;;) {
                JZASTNode *elem = parse_instance_parent_signal_expr(p);
                if (!elem) {
                    jz_ast_free(concat);
                    return NULL;
                }
                if (jz_ast_add_child(concat, elem) != 0) {
                    jz_ast_free(elem);
                    jz_ast_free(concat);
                    return NULL;
                }
                if (!match(p, JZ_TOK_COMMA)) break;
            }
        }
        if (!match(p, JZ_TOK_RBRACE)) {
            jz_ast_free(concat);
            parser_error(p, "expected '}' at end of concatenation in @new instance binding");
            return NULL;
        }
        return concat;
    }

    if (t->type != JZ_TOK_IDENTIFIER) {
        parser_error(p, "expected signal name or literal on right-hand side of @new instance binding");
        return NULL;
    }

    /* Build qualified name: id(.id)* into a buffer. */
    JZLocation loc = t->loc;
    char *buf = NULL;
    size_t buf_sz = 0;

    for (;;) {
        const JZToken *id = peek(p);
        if (id->type != JZ_TOK_IDENTIFIER || !id->lexeme) break;
        size_t len = strlen(id->lexeme);
        char *new_buf = (char *)realloc(buf, buf_sz + len + 2);
        if (!new_buf) {
            free(buf);
            return NULL;
        }
        buf = new_buf;
        memcpy(buf + buf_sz, id->lexeme, len);
        buf_sz += len;
        buf[buf_sz] = '\0';
        advance(p);
        if (!match(p, JZ_TOK_DOT)) {
            break;
        }
        new_buf = (char *)realloc(buf, buf_sz + 2);
        if (!new_buf) {
            free(buf);
            return NULL;
        }
        buf = new_buf;
        buf[buf_sz++] = '.';
        buf[buf_sz] = '\0';
    }

    if (!buf) {
        parser_error(p, "expected identifier in @new instance binding");
        return NULL;
    }

    JZASTNodeType base_type = (strchr(buf, '.') != NULL)
        ? JZ_AST_EXPR_QUALIFIED_IDENTIFIER
        : JZ_AST_EXPR_IDENTIFIER;
    JZASTNode *base = jz_ast_new(base_type, loc);
    if (!base) {
        free(buf);
        return NULL;
    }
    jz_ast_set_name(base, buf);
    free(buf);

    /* Optional single slice: [msb:lsb] or [idx]. */
    if (!match(p, JZ_TOK_LBRACKET)) {
        return base;
    }

    int is_wildcard = 0;
    JZASTNode *msb = NULL;
    if (peek(p)->type == JZ_TOK_OP_STAR) {
        is_wildcard = 1;
        advance(p);
    } else {
        msb = parse_simple_index_expr(p);
        if (!msb) {
            jz_ast_free(base);
            return NULL;
        }
    }

    JZASTNode *lsb = NULL;
    int is_slice = 0;
    if (!is_wildcard && match(p, JZ_TOK_OP_COLON)) {
        is_slice = 1;
        lsb = parse_simple_index_expr(p);
        if (!lsb) {
            jz_ast_free(base);
            jz_ast_free(msb);
            return NULL;
        }
    }

    if (!match(p, JZ_TOK_RBRACKET)) {
        jz_ast_free(base);
        jz_ast_free(msb);
        jz_ast_free(lsb);
        parser_error(p, "expected ']' after slice/index in @new instance binding");
        return NULL;
    }

    if (!is_slice && match(p, JZ_TOK_DOT)) {
        const JZToken *member_tok = peek(p);
        if (!is_decl_identifier_token(member_tok) || !member_tok->lexeme) {
            jz_ast_free(base);
            if (msb) jz_ast_free(msb);
            parser_error(p, "expected BUS member identifier after '.'");
            return NULL;
        }

        if (!base || base->type != JZ_AST_EXPR_IDENTIFIER || !base->name) {
            jz_ast_free(base);
            if (msb) jz_ast_free(msb);
            parser_error(p, "expected BUS port identifier before member access");
            return NULL;
        }

        JZASTNode *bus = jz_ast_new(JZ_AST_EXPR_BUS_ACCESS, base->loc);
        if (!bus) {
            jz_ast_free(base);
            if (msb) jz_ast_free(msb);
            return NULL;
        }
        jz_ast_set_name(bus, base->name);
        jz_ast_set_text(bus, member_tok->lexeme);
        if (is_wildcard) {
            jz_ast_set_block_kind(bus, "WILDCARD");
        } else if (msb) {
            if (jz_ast_add_child(bus, msb) != 0) {
                jz_ast_free(bus);
                jz_ast_free(msb);
                jz_ast_free(base);
                return NULL;
            }
        }
        advance(p); /* consume member identifier */
        jz_ast_free(base);
        base = bus;

        /* Optional slice after BUS member access. */
        if (!match(p, JZ_TOK_LBRACKET)) {
            return base;
        }

        JZASTNode *msb2 = parse_simple_index_expr(p);
        if (!msb2) {
            jz_ast_free(base);
            return NULL;
        }
        JZASTNode *lsb2 = NULL;
        if (match(p, JZ_TOK_OP_COLON)) {
            lsb2 = parse_simple_index_expr(p);
            if (!lsb2) {
                jz_ast_free(base);
                jz_ast_free(msb2);
                return NULL;
            }
        } else {
            lsb2 = clone_expr_tree(msb2);
            if (!lsb2) {
                jz_ast_free(base);
                jz_ast_free(msb2);
                return NULL;
            }
        }
        if (!match(p, JZ_TOK_RBRACKET)) {
            jz_ast_free(base);
            jz_ast_free(msb2);
            jz_ast_free(lsb2);
            parser_error(p, "expected ']' after slice/index in @new instance binding");
            return NULL;
        }

        JZASTNode *slice = jz_ast_new(JZ_AST_EXPR_SLICE, base->loc);
        if (!slice) {
            jz_ast_free(base);
            jz_ast_free(msb2);
            jz_ast_free(lsb2);
            return NULL;
        }
        if (jz_ast_add_child(slice, base) != 0 ||
            jz_ast_add_child(slice, msb2) != 0 ||
            jz_ast_add_child(slice, lsb2) != 0) {
            jz_ast_free(slice);
            jz_ast_free(base);
            jz_ast_free(msb2);
            jz_ast_free(lsb2);
            return NULL;
        }
        return slice;
    }

    if (is_wildcard) {
        jz_ast_free(base);
        parser_error(p, "wildcard index is only valid for BUS member access");
        return NULL;
    }

    if (!is_slice) {
        lsb = jz_ast_new(msb->type, msb->loc);
        if (!lsb) {
            jz_ast_free(base);
            jz_ast_free(msb);
            return NULL;
        }
        if (msb->type == JZ_AST_EXPR_IDENTIFIER) {
            if (msb->name) jz_ast_set_name(lsb, msb->name);
        } else if (msb->type == JZ_AST_EXPR_LITERAL) {
            if (msb->text) jz_ast_set_text(lsb, msb->text);
        }
    }

    JZASTNode *slice = jz_ast_new(JZ_AST_EXPR_SLICE, base->loc);
    if (!slice) {
        jz_ast_free(base);
        jz_ast_free(msb);
        jz_ast_free(lsb);
        return NULL;
    }
    if (jz_ast_add_child(slice, base) != 0 ||
        jz_ast_add_child(slice, msb) != 0 ||
        jz_ast_add_child(slice, lsb) != 0) {
        jz_ast_free(slice);
        jz_ast_free(base);
        jz_ast_free(msb);
        jz_ast_free(lsb);
        return NULL;
    }
    return slice;
}

/**
 * @brief Parse the body of a module instance (@new) declaration.
 *
 * The instance body consists of structured port-binding declarations:
 *
 * @code
 * IN    [width] port = parent_signal;
 * OUT   [width] port = parent_signal;
 * INOUT [width] port = parent_signal;
 * BUS <bus_id> <ROLE> [N] port = parent_signal;
 * @endcode
 *
 * @param p    Active parser
 * @param inst Module instance AST node
 * @return 0 on success, -1 on error
 */
static int parse_instance_binding_list(Parser *p, JZASTNode *inst) {
    for (;;) {
        const JZToken *t = peek(p);
        if (t->type == JZ_TOK_RBRACE) {
            /* End of instance body. */
            advance(p);
            return 0;
        }
        if (t->type == JZ_TOK_EOF) {
            parser_error(p, "unterminated @new instance body (missing '}' )");
            return -1;
        }
        if (t->type == JZ_TOK_SEMICOLON) {
            /* Allow stray semicolons as empty statements. */
            advance(p);
            continue;
        }

        /* BUS <bus_id> <ROLE> [N] <port_name> = <parent_signal>; */
        if (t->type == JZ_TOK_IDENTIFIER && t->lexeme && strcmp(t->lexeme, "BUS") == 0) {
            advance(p); /* consume 'BUS' */

            const JZToken *bus_id_tok = peek(p);
            if (!is_decl_identifier_token(bus_id_tok)) {
                parser_error(p, "expected BUS identifier after BUS keyword in @new instance body");
                return -1;
            }
            advance(p);

            const JZToken *role_tok = peek(p);
            if (!is_decl_identifier_token(role_tok) || !role_tok->lexeme) {
                parser_error(p, "expected BUS role (SOURCE/TARGET) after BUS id in @new instance body");
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
                    parser_error(p, "expected ']' after BUS array width expression in @new instance body");
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
                parser_error(p, "expected BUS port name after BUS role and optional [N] in @new instance body");
                return -1;
            }
            advance(p);

            if (!match(p, JZ_TOK_OP_ASSIGN)) {
                if (array_width) free(array_width);
                parser_error(p, "expected '=' after BUS port name in @new instance body");
                return -1;
            }

            JZASTNode *rhs = parse_instance_parent_signal_expr(p);
            if (!rhs) {
                if (array_width) free(array_width);
                return -1;
            }

            if (!match(p, JZ_TOK_SEMICOLON)) {
                jz_ast_free(rhs);
                if (array_width) free(array_width);
                parser_error(p, "expected ';' after BUS @new instance binding");
                return -1;
            }

            JZASTNode *decl = jz_ast_new(JZ_AST_PORT_DECL, port_name_tok->loc);
            if (!decl) {
                jz_ast_free(rhs);
                if (array_width) free(array_width);
                return -1;
            }
            jz_ast_set_name(decl, port_name_tok->lexeme);
            jz_ast_set_block_kind(decl, "BUS");

            /* Encode "bus_id ROLE" into the text field for semantic passes. */
            const char *bus_name = bus_id_tok->lexeme ? bus_id_tok->lexeme : "";
            const char *role_name = role_tok->lexeme ? role_tok->lexeme : "";
            size_t bus_len = strlen(bus_name);
            size_t role_len = strlen(role_name);
            char *meta = (char *)malloc(bus_len + 1 + role_len + 1);
            if (!meta) {
                if (array_width) free(array_width);
                jz_ast_free(rhs);
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

        /* Direction: IN / OUT / INOUT */
        const char *dir_str = NULL;
        if (t->type == JZ_TOK_KW_IN) {
            dir_str = "IN";
        } else if (t->type == JZ_TOK_KW_OUT) {
            dir_str = "OUT";
        } else if (t->type == JZ_TOK_KW_INOUT) {
            dir_str = "INOUT";
        } else {
            parser_error(p, "expected IN/OUT/INOUT/BUS in @new instance body");
            return -1;
        }
        advance(p);

        if (!match(p, JZ_TOK_LBRACKET)) {
            parser_error(p, "expected '[' after port direction in @new instance body");
            return -1;
        }

        /* Capture width expression tokens between '[' and ']'. */
        size_t width_start = p->pos;
        while (peek(p)->type != JZ_TOK_EOF &&
               peek(p)->type != JZ_TOK_RBRACKET) {
            advance(p);
        }
        const JZToken *rb = peek(p);
        if (rb->type != JZ_TOK_RBRACKET) {
            parser_error(p, "expected ']' after port width expression in @new instance body");
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

        /* Child port name. */
        const JZToken *name_tok = peek(p);
        if (!is_decl_identifier_token(name_tok)) {
            if (width_text) free(width_text);
            parser_error(p, "expected port name after width in @new instance body");
            return -1;
        }
        advance(p);

        if (!match(p, JZ_TOK_OP_ASSIGN)) {
            if (width_text) free(width_text);
            parser_error(p, "expected '=' after port name in @new instance body");
            return -1;
        }

        /* RHS parent signal expression: restrict to signal-like forms
           (identifier, qualified identifier, optional single slice). */
        JZASTNode *rhs = parse_instance_parent_signal_expr(p);
        if (!rhs) {
            if (width_text) free(width_text);
            return -1;
        }

        if (!match(p, JZ_TOK_SEMICOLON)) {
            jz_ast_free(rhs);
            if (width_text) free(width_text);
            parser_error(p, "expected ';' after @new instance binding");
            return -1;
        }

        JZASTNode *decl = jz_ast_new(JZ_AST_PORT_DECL, name_tok->loc);
        if (!decl) {
            jz_ast_free(rhs);
            if (width_text) free(width_text);
            return -1;
        }
        jz_ast_set_name(decl, name_tok->lexeme);
        jz_ast_set_block_kind(decl, dir_str);
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
 * @brief Parse a module instantiation (@new) statement.
 *
 * Supported forms:
 * @code
 * @new inst module { ... }
 * @new inst[count] module { ... }
 * @endcode
 *
 * An optional OVERRIDE block may appear at the start of the instance body
 * to supply per-instance constant overrides.
 *
 * @param p Active parser
 * @return Module instance AST node, or NULL on error
 */
JZASTNode *parse_module_instantiation(Parser *p) {
    const JZToken *kw = &p->tokens[p->pos - 1]; /* @new already consumed */

    const JZToken *inst_name = peek(p);
    if (!is_decl_identifier_token(inst_name)) {
        parser_error(p, "expected instance name after @new in module");
        return NULL;
    }
    advance(p);

    /* Optional [count] for instance arrays: @new inst[count] module { ... } */
    char *count_text = NULL;
    if (match(p, JZ_TOK_LBRACKET)) {
        size_t start = p->pos;
        while (peek(p)->type != JZ_TOK_EOF &&
               peek(p)->type != JZ_TOK_RBRACKET) {
            advance(p);
        }
        const JZToken *rb = peek(p);
        if (rb->type != JZ_TOK_RBRACKET) {
            parser_error(p, "expected ']' after instance array count");
            return NULL;
        }
        size_t end = p->pos;
        advance(p); /* consume ']' */

        if (start < end) {
            size_t buf_sz = 0;
            for (size_t i = start; i < end; ++i) {
                const JZToken *t = &p->tokens[i];
                if (t->lexeme) buf_sz += strlen(t->lexeme) + 1;
            }
            if (buf_sz > 0) {
                count_text = (char *)malloc(buf_sz + 1);
                if (!count_text) return NULL;
                count_text[0] = '\0';
                for (size_t i = start; i < end; ++i) {
                    const JZToken *t = &p->tokens[i];
                    if (!t->lexeme) continue;
                    strcat(count_text, t->lexeme);
                    strcat(count_text, " ");
                }
            }
        }
    }

    const JZToken *mod_name = peek(p);
    if (!is_decl_identifier_token(mod_name)) {
        if (count_text) free(count_text);
        parser_error(p, "expected module or blackbox name after instance name");
        return NULL;
    }
    advance(p);

    JZASTNode *inst = jz_ast_new(JZ_AST_MODULE_INSTANCE, kw->loc);
    if (!inst) {
        if (count_text) free(count_text);
        return NULL;
    }
    /* Store instance name in node->name and referenced module/blackbox in node->text for now. */
    jz_ast_set_name(inst, inst_name->lexeme);
    jz_ast_set_text(inst, mod_name->lexeme);
    if (count_text) {
        /* Reuse width field on ModuleInstance to carry optional array count expression. */
        jz_ast_set_width(inst, count_text);
        free(count_text);
    }

    if (!match(p, JZ_TOK_LBRACE)) {
        parser_error(p, "expected '{' after @new <inst> <module> in module body");
        jz_ast_free(inst);
        return NULL;
    }

    /* Optional OVERRIDE { ... } block for per-instance CONST overrides. */
    if (peek(p)->type == JZ_TOK_KW_OVERRIDE) {
        const JZToken *ov_kw = peek(p);
        advance(p); /* consume OVERRIDE */

        JZASTNode *ov_block = jz_ast_new(JZ_AST_CONST_BLOCK, ov_kw->loc);
        if (!ov_block) {
            jz_ast_free(inst);
            return NULL;
        }
        jz_ast_set_block_kind(ov_block, "OVERRIDE");

        if (!match(p, JZ_TOK_LBRACE)) {
            parser_error(p, "expected '{' after OVERRIDE in @new instance body");
            jz_ast_free(ov_block);
            jz_ast_free(inst);
            return NULL;
        }

        if (parse_const_block_body(p, ov_block) != 0) {
            jz_ast_free(ov_block);
            jz_ast_free(inst);
            return NULL;
        }

        if (jz_ast_add_child(inst, ov_block) != 0) {
            jz_ast_free(ov_block);
            jz_ast_free(inst);
            return NULL;
        }
    }

    /* Parse the remaining instance body as structured port-binding declarations
       using the updated 4.12 syntax:
         IN    [width] port = parent_signal;
         OUT   [width] port = parent_signal;
         INOUT [width] port = parent_signal;
    */
    if (parse_instance_binding_list(p, inst) != 0) {
        jz_ast_free(inst);
        return NULL;
    }

    return inst;
}
