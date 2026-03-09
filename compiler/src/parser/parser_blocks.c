/**
 * @file parser_blocks.c
 * @brief Block dispatch and shared block-parsing helpers.
 *
 * This file implements the top-level block dispatcher (parse_block) that
 * routes to block-type-specific parsers, as well as shared helpers for
 * SYNCHRONOUS header parsing, ASYNCHRONOUS/SYNCHRONOUS body wrappers,
 * raw-text block parsing, and CONST/CONFIG block body parsing.
 *
 * Block-type-specific parsers live in their own files:
 *   parser_port.c           - PORT block
 *   parser_wire.c           - WIRE block
 *   parser_register.c       - REGISTER and LATCH blocks
 *   parser_mem.c            - MEM block
 *   parser_mux.c            - MUX block
 *   parser_cdc.c            - CDC block
 *   parser_project_blocks.c - CLOCKS, PIN, MAP, CLOCK_GEN blocks
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser_internal.h"

/**
 * @brief Parse the parameter header of a SYNCHRONOUS block.
 *
 * The SYNCHRONOUS header has the form:
 *   SYNCHRONOUS(key = value, ...)
 *
 * Each key/value pair is parsed and attached to the block as a
 * JZ_AST_SYNC_PARAM child node.
 *
 * @param p     Active parser
 * @param block SYNCHRONOUS block AST node
 * @return 0 on success, -1 on error
 */
static int parse_synchronous_header(Parser *p, JZASTNode *block) {
    if (!match(p, JZ_TOK_LPAREN)) {
        parser_error(p, "expected '(' after SYNCHRONOUS");
        return -1;
    }

    for (;;) {
        const JZToken *t = peek(p);
        if (t->type == JZ_TOK_RPAREN) {
            advance(p); /* consume ')' */
            break;
        }
        if (t->type == JZ_TOK_EOF) {
            parser_error(p, "unterminated SYNCHRONOUS header (missing ')')");
            return -1;
        }

        if (t->type != JZ_TOK_IDENTIFIER || !t->lexeme) {
            parser_error(p, "expected parameter name in SYNCHRONOUS header");
            return -1;
        }
        const JZToken *key_tok = t;
        advance(p);

        if (!match(p, JZ_TOK_OP_ASSIGN)) {
            parser_error(p, "expected '=' after parameter name in SYNCHRONOUS header");
            return -1;
        }

        JZASTNode *value = parse_expression(p);
        if (!value) {
            return -1;
        }

        JZASTNode *param = jz_ast_new(JZ_AST_SYNC_PARAM, key_tok->loc);
        if (!param) {
            jz_ast_free(value);
            return -1;
        }
        jz_ast_set_name(param, key_tok->lexeme);
        if (jz_ast_add_child(param, value) != 0) {
            jz_ast_free(param);
            jz_ast_free(value);
            return -1;
        }
        if (jz_ast_add_child(block, param) != 0) {
            jz_ast_free(param);
            return -1;
        }

        /* Whitespace/newlines are already skipped by the lexer; the next token
           is either another identifier (next parameter) or ')'. */
    }

    return 0;
}

/**
 * @brief Parse a generic braced block as raw text items.
 *
 * This is used for blocks whose internal syntax is not structurally parsed
 * by the front end.
 *
 * @param p                Active parser
 * @param parent           Parent AST node
 * @param unterminated_msg Error message for unterminated blocks
 * @return 0 on success, -1 on error
 */
static int parse_braced_raw_items(Parser *p, JZASTNode *parent, const char *unterminated_msg) {
    size_t item_start = p->pos;
    int depth = 1;

    while (p->pos < p->count && depth > 0) {
        const JZToken *t = &p->tokens[p->pos++];
        if (t->type == JZ_TOK_LBRACE) {
            depth++;
        } else if (t->type == JZ_TOK_RBRACE) {
            depth--;
            if (depth == 0) {
                break;
            }
        }

        if (depth == 1 && t->type == JZ_TOK_SEMICOLON) {
            size_t item_end = p->pos - 1; /* include ';' in raw text */
            JZASTNode *raw = make_raw_text_node(p, item_start, item_end);
            if (!raw) return -1;
            if (jz_ast_add_child(parent, raw) != 0) {
                jz_ast_free(raw);
                return -1;
            }
            item_start = p->pos;
        }
    }

    if (depth != 0) {
        parser_error(p, unterminated_msg);
        return -1;
    }

    /* Capture any trailing tokens before the closing '}' as a final raw-text item. */
    size_t block_end = p->pos - 1; /* index of '}' */
    if (item_start < block_end) {
        JZASTNode *raw = make_raw_text_node(p, item_start, block_end);
        if (raw) {
            if (jz_ast_add_child(parent, raw) != 0) {
                jz_ast_free(raw);
                return -1;
            }
        }
    }

    return 0;
}

/**
 * @brief Parse the body of an ASYNCHRONOUS block.
 *
 * @param p      Active parser
 * @param parent ASYNCHRONOUS block AST node
 * @return 0 on success, -1 on error
 */
static int parse_asynchronous_block_body(Parser *p, JZASTNode *parent) {
    /* Opening '{' has already been consumed by parse_block. */
    return parse_statement_list(p, parent, JZ_TOK_RBRACE, 0);
}

/**
 * @brief Parse the body of a SYNCHRONOUS block.
 *
 * Syntax is identical to ASYNCHRONOUS blocks; semantics differ later.
 *
 * @param p      Active parser
 * @param parent SYNCHRONOUS block AST node
 * @return 0 on success, -1 on error
 */
static int parse_synchronous_block_body(Parser *p, JZASTNode *parent) {
    /* Body syntax is identical; semantics differ in later passes. */
    return parse_statement_list(p, parent, JZ_TOK_RBRACE, 1);
}

/**
 * @brief Parse a structured block.
 *
 * Dispatches to the appropriate block-body parser based on node type
 * and block kind.
 *
 * @param p         Active parser
 * @param block_kw  Block keyword token
 * @param kind      Block kind string
 * @param node_type AST node type to create
 * @return Block AST node, or NULL on error
 */
JZASTNode *parse_block(Parser *p, const JZToken *block_kw, const char *kind, JZASTNodeType node_type) {
    JZLocation loc = block_kw->loc;
    JZASTNode *node = jz_ast_new(node_type, loc);
    if (!node) return NULL;
    jz_ast_set_block_kind(node, kind);

    /* For SYNCHRONOUS blocks, parse the header arguments into SyncParam children
       before consuming the '{' that begins the body. */
    if (kind && strcmp(kind, "SYNCHRONOUS") == 0) {
        if (parse_synchronous_header(p, node) != 0) {
            jz_ast_free(node);
            return NULL;
        }
    }

    /* MEM blocks support an optional header attribute list: MEM(TYPE=...){...} or
       MEM(type=...){...}. Capture the raw attribute text between '(' and ')' on
       the MemBlock node so later semantic passes can interpret TYPE. */
    if (node_type == JZ_AST_MEM_BLOCK && match(p, JZ_TOK_LPAREN)) {
        size_t attr_start = p->pos;
        int depth = 1;
        while (p->pos < p->count && depth > 0) {
            const JZToken *tok = &p->tokens[p->pos++];
            if (tok->type == JZ_TOK_LPAREN) {
                depth++;
            } else if (tok->type == JZ_TOK_RPAREN) {
                depth--;
                if (depth == 0) {
                    break;
                }
            }
        }
        if (depth != 0) {
            parser_error(p, "unterminated MEM header (missing ')')");
            jz_ast_free(node);
            return NULL;
        }

        size_t attr_end = p->pos - 1; /* index of ')' */
        if (attr_start < attr_end) {
            size_t buf_sz = 0;
            for (size_t i = attr_start; i < attr_end; ++i) {
                const JZToken *at = &p->tokens[i];
                if (at->lexeme) buf_sz += strlen(at->lexeme) + 1;
            }
            if (buf_sz > 0) {
                char *buf = (char *)malloc(buf_sz + 1);
                if (!buf) {
                    jz_ast_free(node);
                    return NULL;
                }
                buf[0] = '\0';
                for (size_t i = attr_start; i < attr_end; ++i) {
                    const JZToken *at = &p->tokens[i];
                    if (!at->lexeme) continue;
                    strcat(buf, at->lexeme);
                    strcat(buf, " ");
                }
                jz_ast_set_text(node, buf);
                free(buf);
            }
        }
    }

    if (!match(p, JZ_TOK_LBRACE)) {
        parser_error(p, "expected '{' after block keyword");
        jz_ast_free(node);
        return NULL;
    }

    if (node_type == JZ_AST_CONST_BLOCK) {
        if (parse_const_block_body(p, node) != 0) {
            jz_ast_free(node);
            return NULL;
        }
    } else if (node_type == JZ_AST_PORT_BLOCK) {
        if (parse_port_block_body(p, node) != 0) {
            jz_ast_free(node);
            return NULL;
        }
    } else if (node_type == JZ_AST_WIRE_BLOCK) {
        if (parse_wire_block_body(p, node) != 0) {
            jz_ast_free(node);
            return NULL;
        }
    } else if (node_type == JZ_AST_REGISTER_BLOCK) {
        if (parse_register_block_body(p, node) != 0) {
            jz_ast_free(node);
            return NULL;
        }
    } else if (node_type == JZ_AST_LATCH_BLOCK) {
        if (parse_latch_block_body(p, node) != 0) {
            jz_ast_free(node);
            return NULL;
        }
    } else if (node_type == JZ_AST_MEM_BLOCK) {
        if (parse_mem_block_body(p, node) != 0) {
            jz_ast_free(node);
            return NULL;
        }
    } else if (node_type == JZ_AST_MUX_BLOCK) {
        if (parse_mux_block_body(p, node) != 0) {
            jz_ast_free(node);
            return NULL;
        }
    } else if (kind && strcmp(kind, "CDC") == 0 && node_type == JZ_AST_BLOCK) {
        if (parse_cdc_block_body(p, node) != 0) {
            jz_ast_free(node);
            return NULL;
        }
    } else if (node_type == JZ_AST_CONFIG_BLOCK) {
        if (parse_const_block_body(p, node) != 0) {
            jz_ast_free(node);
            return NULL;
        }
    } else if (node_type == JZ_AST_CLOCKS_BLOCK) {
        if (parse_clocks_block_body(p, node) != 0) {
            jz_ast_free(node);
            return NULL;
        }
    } else if (node_type == JZ_AST_IN_PINS_BLOCK) {
        if (parse_pins_block_body(p, node, "IN_PINS") != 0) {
            jz_ast_free(node);
            return NULL;
        }
    } else if (node_type == JZ_AST_OUT_PINS_BLOCK) {
        if (parse_pins_block_body(p, node, "OUT_PINS") != 0) {
            jz_ast_free(node);
            return NULL;
        }
    } else if (node_type == JZ_AST_INOUT_PINS_BLOCK) {
        if (parse_pins_block_body(p, node, "INOUT_PINS") != 0) {
            jz_ast_free(node);
            return NULL;
        }
    } else if (node_type == JZ_AST_MAP_BLOCK) {
        if (parse_map_block_body(p, node) != 0) {
            jz_ast_free(node);
            return NULL;
        }
    } else if (node_type == JZ_AST_BLOCK && kind && strcmp(kind, "ASYNCHRONOUS") == 0) {
        if (parse_asynchronous_block_body(p, node) != 0) {
            jz_ast_free(node);
            return NULL;
        }
    } else if (node_type == JZ_AST_BLOCK && kind && strcmp(kind, "SYNCHRONOUS") == 0) {
        if (parse_synchronous_block_body(p, node) != 0) {
            jz_ast_free(node);
            return NULL;
        }
    } else {
        if (parse_braced_raw_items(p, node, "unterminated block (missing '}' )") != 0) {
            jz_ast_free(node);
            return NULL;
        }
    }

    return node;
}

/**
 * @brief Parse the body of a CONST block.
 *
 * CONST blocks define named constant expressions.
 *
 * @param p      Active parser
 * @param parent CONST block AST node
 * @return 0 on success, -1 on error
 */
int parse_const_block_body(Parser *p, JZASTNode *parent) {
    for (;;) {
        const JZToken *t = peek(p);
        if (t->type == JZ_TOK_RBRACE) {
            advance(p); /* consume '}' */
            return 0;
        }
        if (t->type == JZ_TOK_EOF) {
            parser_error(p, "unterminated CONST block (missing '}' )");
            return -1;
        }

        /* Feature guard sentinel — stop when inside a feature body */
        if (t->type == JZ_TOK_KW_FEATURE_ELSE || t->type == JZ_TOK_KW_ENDFEAT) {
            return 0;
        }
        /* Feature guard — parse it */
        if (t->type == JZ_TOK_KW_FEATURE) {
            if (parse_feature_guard_in_block(p, parent, parse_const_block_body) != 0)
                return -1;
            continue;
        }

        const JZToken *name_tok = peek(p);
        if (!is_decl_identifier_token(name_tok)) {
            parser_error(p, "expected identifier in CONST block");
            return -1;
        }
        advance(p);

        if (!match(p, JZ_TOK_OP_ASSIGN)) {
            parser_error(p, "expected '=' after CONST name");
            return -1;
        }

        /* String literal value: CONST/CONFIG name = "string"; */
        if (peek(p)->type == JZ_TOK_STRING) {
            const JZToken *str_tok = peek(p);
            advance(p); /* consume string */

            if (!match(p, JZ_TOK_SEMICOLON)) {
                parser_error(p, "expected ';' after string value");
                return -1;
            }

            JZASTNode *decl = jz_ast_new(JZ_AST_CONST_DECL, name_tok->loc);
            if (!decl) return -1;
            jz_ast_set_name(decl, name_tok->lexeme);
            jz_ast_set_text(decl, str_tok->lexeme);
            jz_ast_set_block_kind(decl, "STRING");

            if (jz_ast_add_child(parent, decl) != 0) {
                jz_ast_free(decl);
                return -1;
            }
            continue;
        }

        size_t expr_start = p->pos;
        while (peek(p)->type != JZ_TOK_EOF &&
               peek(p)->type != JZ_TOK_SEMICOLON &&
               peek(p)->type != JZ_TOK_RBRACE) {
            advance(p);
        }

        const JZToken *semi = peek(p);
        if (semi->type != JZ_TOK_SEMICOLON) {
            parser_error(p, "expected ';' after CONST expression");
            return -1;
        }

        size_t expr_end = p->pos; /* tokens [expr_start, expr_end) form the expression */
        advance(p); /* consume ';' */

        JZASTNode *decl = jz_ast_new(JZ_AST_CONST_DECL, name_tok->loc);
        if (!decl) return -1;
        jz_ast_set_name(decl, name_tok->lexeme);

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

        if (jz_ast_add_child(parent, decl) != 0) {
            jz_ast_free(decl);
            return -1;
        }
    }
}
