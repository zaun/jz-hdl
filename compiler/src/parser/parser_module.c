/**
 * @file parser_module.c
 * @brief Parsing of @module and @blackbox constructs.
 *
 * This file implements parsing logic for module definitions and blackbox
 * bodies in the JZ HDL. It handles structural blocks such as CONST, PORT,
 * WIRE, REGISTER, MEM, MUX, CDC, ASYNCHRONOUS, SYNCHRONOUS, as well as
 * module-level @new instantiations and @check assertions.
 *
 * Context-sensitive validation is intentionally deferred to semantic
 * analysis where possible, allowing the parser to emit precise rule-based
 * diagnostics instead of generic syntax errors.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser_internal.h"

/* Forward declaration for mutual recursion with parse_module_scope_feature_body. */
static int parse_module_scope_feature_body(Parser *p, JZASTNode *parent);

/**
 * @brief Parse a module definition.
 *
 * Expects the @module keyword to have already been consumed. This function
 * parses the module name and all valid module-scope constructs until the
 * matching @endmod directive is encountered.
 *
 * Valid constructs inside a module include:
 * - CONST, PORT, WIRE, REGISTER, LATCH, MEM, MUX blocks
 * - CDC blocks
 * - ASYNCHRONOUS and SYNCHRONOUS blocks
 * - Module-level @new instantiations
 * - Module-level @check assertions
 *
 * Control-flow statements and structural directives that are not valid at
 * module scope are diagnosed but do not immediately terminate parsing.
 *
 * @param p Active parser
 * @return Module AST node, or NULL on error
 */
JZASTNode *parse_module(Parser *p) {
    const JZToken *kw = &p->tokens[p->pos - 1]; /* already consumed @module */

    const JZToken *name_tok = peek(p);
    if (!is_decl_identifier_token(name_tok)) {
        parser_error(p, "expected identifier after @module");
        return NULL;
    }
    advance(p);

    JZASTNode *mod = jz_ast_new(JZ_AST_MODULE, kw->loc);
    if (!mod) return NULL;
    jz_ast_set_name(mod, name_tok->lexeme);

    /* Parse until @endmod */
    while (peek(p)->type != JZ_TOK_EOF && peek(p)->type != JZ_TOK_KW_ENDMOD) {
        const JZToken *t = peek(p);
        if (t->type == JZ_TOK_KW_CONST) {
            advance(p);
            JZASTNode *blk = parse_block(p, t, "CONST", JZ_AST_CONST_BLOCK);
            if (!blk) { jz_ast_free(mod); return NULL; }
            jz_ast_add_child(mod, blk);
        } else if (t->type == JZ_TOK_KW_PORT) {
            advance(p);
            JZASTNode *blk = parse_block(p, t, "PORT", JZ_AST_PORT_BLOCK);
            if (!blk) { jz_ast_free(mod); return NULL; }
            jz_ast_add_child(mod, blk);
        } else if (t->type == JZ_TOK_KW_WIRE) {
            advance(p);
            JZASTNode *blk = parse_block(p, t, "WIRE", JZ_AST_WIRE_BLOCK);
            if (!blk) { jz_ast_free(mod); return NULL; }
            jz_ast_add_child(mod, blk);
        } else if (t->type == JZ_TOK_KW_REGISTER) {
            advance(p);
            JZASTNode *blk = parse_block(p, t, "REGISTER", JZ_AST_REGISTER_BLOCK);
            if (!blk) { jz_ast_free(mod); return NULL; }
            jz_ast_add_child(mod, blk);
        } else if (t->type == JZ_TOK_KW_LATCH) {
            advance(p);
            JZASTNode *blk = parse_block(p, t, "LATCH", JZ_AST_LATCH_BLOCK);
            if (!blk) { jz_ast_free(mod); return NULL; }
            jz_ast_add_child(mod, blk);
        } else if (t->type == JZ_TOK_KW_MEM) {
            advance(p);
            JZASTNode *blk = parse_block(p, t, "MEM", JZ_AST_MEM_BLOCK);
            if (!blk) { jz_ast_free(mod); return NULL; }
            jz_ast_add_child(mod, blk);
        } else if (t->type == JZ_TOK_KW_MUX) {
            advance(p);
            JZASTNode *blk = parse_block(p, t, "MUX", JZ_AST_MUX_BLOCK);
            if (!blk) { jz_ast_free(mod); return NULL; }
            jz_ast_add_child(mod, blk);
        } else if (t->type == JZ_TOK_KW_CDC) {
            advance(p);
            JZASTNode *blk = parse_block(p, t, "CDC", JZ_AST_BLOCK);
            if (!blk) { jz_ast_free(mod); return NULL; }
            jz_ast_add_child(mod, blk);
        } else if (t->type == JZ_TOK_KW_ASYNC) {
            advance(p);
            JZASTNode *blk = parse_block(p, t, "ASYNCHRONOUS", JZ_AST_BLOCK);
            if (!blk) { jz_ast_free(mod); return NULL; }
            jz_ast_add_child(mod, blk);
        } else if (t->type == JZ_TOK_KW_SYNC) {
            advance(p);
            JZASTNode *blk = parse_block(p, t, "SYNCHRONOUS", JZ_AST_BLOCK);
            if (!blk) { jz_ast_free(mod); return NULL; }
            jz_ast_add_child(mod, blk);
        } else if (t->type == JZ_TOK_KW_NEW) {
            /* Module-level @new instantiation. */
            advance(p);
            JZASTNode *inst = parse_module_instantiation(p);
            if (!inst) { jz_ast_free(mod); return NULL; }
            jz_ast_add_child(mod, inst);
        } else if (t->type == JZ_TOK_KW_CHECK) {
            /* Module-level @check compile-time assertion. */
            advance(p);
            JZASTNode *chk = parse_check(p);
            if (!chk) { jz_ast_free(mod); return NULL; }
            jz_ast_add_child(mod, chk);
        } else if (t->type == JZ_TOK_KW_TEMPLATE) {
            /* Module-scoped template definition. */
            advance(p);
            JZASTNode *tmpl = parse_template_def(p);
            if (!tmpl) { jz_ast_free(mod); return NULL; }
            jz_ast_add_child(mod, tmpl);
        } else if (t->type == JZ_TOK_KW_FEATURE) {
            /* Module-scope @feature guard wrapping blocks and instances. */
            if (parse_feature_guard_in_block(p, mod, parse_module_scope_feature_body) != 0) {
                jz_ast_free(mod);
                return NULL;
            }
        } else if (t->type == JZ_TOK_KW_APPLY) {
            /* @apply outside ASYNCHRONOUS/SYNCHRONOUS block. */
            parser_report_rule(p, t, "TEMPLATE_APPLY_OUTSIDE_BLOCK",
                               "@apply found at module scope; @apply may only appear\n"
                               "inside ASYNCHRONOUS or SYNCHRONOUS blocks");
            /* Skip past the semicolon to recover */
            advance(p);
            while (peek(p)->type != JZ_TOK_EOF &&
                   peek(p)->type != JZ_TOK_SEMICOLON &&
                   peek(p)->type != JZ_TOK_KW_ENDMOD) {
                advance(p);
            }
            if (peek(p)->type == JZ_TOK_SEMICOLON) advance(p);
        } else if (t->type == JZ_TOK_KW_SCRATCH) {
            /* @scratch outside template body. */
            parser_report_rule(p, t, "TEMPLATE_SCRATCH_OUTSIDE",
                               "@scratch found at module scope; @scratch declares temporary wires\n"
                               "and may only be used inside a @template body");
            /* Skip past the semicolon to recover */
            advance(p);
            while (peek(p)->type != JZ_TOK_EOF &&
                   peek(p)->type != JZ_TOK_SEMICOLON &&
                   peek(p)->type != JZ_TOK_KW_ENDMOD) {
                advance(p);
            }
            if (peek(p)->type == JZ_TOK_SEMICOLON) advance(p);
        } else if (t->type == JZ_TOK_KW_IF ||
                   t->type == JZ_TOK_KW_ELIF ||
                   t->type == JZ_TOK_KW_ELSE ||
                   t->type == JZ_TOK_KW_SELECT ||
                   t->type == JZ_TOK_KW_CASE ||
                   t->type == JZ_TOK_KW_DEFAULT) {
            /* Control-flow keywords are only valid inside ASYNCHRONOUS or
             * SYNCHRONOUS blocks, not at module scope.
             */
            parser_report_rule(p,
                               t,
                               "CONTROL_FLOW_OUTSIDE_BLOCK",
                               "control-flow statements are only allowed inside ASYNCHRONOUS or SYNCHRONOUS blocks");
            advance(p);
        } else if (t->type == JZ_TOK_KW_PROJECT ||
                   t->type == JZ_TOK_KW_ENDPROJ ||
                   t->type == JZ_TOK_KW_MODULE ||
                   t->type == JZ_TOK_KW_ENDMOD ||
                   t->type == JZ_TOK_KW_BLACKBOX ||
                   t->type == JZ_TOK_KW_IMPORT ||
                   t->type == JZ_TOK_KW_GLOBAL ||
                   t->type == JZ_TOK_KW_ENDGLOB) {
            /* Structural directives are not allowed inside module bodies. */
            parser_report_rule(p, t,
                               "DIRECTIVE_INVALID_CONTEXT",
                               "structural directive used in invalid location inside module");
            advance(p);
        } else if (t->type == JZ_TOK_SEMICOLON) {
            /* Tolerate stray semicolons (e.g. after @new blocks). */
            advance(p);
        } else if (t->type == JZ_TOK_IDENTIFIER && t->lexeme && t->lexeme[0] == '@') {
            /* Unrecognized @directive — likely a typo like @feature_else. */
            parser_error(p, "unrecognized directive in module body");
            advance(p);
        } else {
            /* Skip unknown tokens silently for error recovery. */
            advance(p);
        }
    }

    if (!match(p, JZ_TOK_KW_ENDMOD)) {
        parser_error(p, "missing @endmod for module");
        jz_ast_free(mod);
        return NULL;
    }

    return mod;
}

/**
 * @brief Parse module-scope items inside a @feature guard body.
 *
 * This function iterates module-scope items (blocks, @new, @check) until
 * it encounters @else or @endfeat sentinel tokens, at which point it returns 0.
 *
 * @param p      Active parser
 * @param parent Feature branch AST node
 * @return 0 on success, -1 on error
 */
static int parse_module_scope_feature_body(Parser *p, JZASTNode *parent)
{
    for (;;) {
        const JZToken *t = peek(p);

        /* Sentinel tokens — stop here, let the caller handle them. */
        if (t->type == JZ_TOK_KW_FEATURE_ELSE || t->type == JZ_TOK_KW_ENDFEAT) {
            return 0;
        }
        if (t->type == JZ_TOK_EOF || t->type == JZ_TOK_KW_ENDMOD) {
            parser_error(p, "unterminated @feature block in module (missing @endfeat)");
            return -1;
        }

        if (t->type == JZ_TOK_KW_CONST) {
            advance(p);
            JZASTNode *blk = parse_block(p, t, "CONST", JZ_AST_CONST_BLOCK);
            if (!blk) return -1;
            jz_ast_add_child(parent, blk);
        } else if (t->type == JZ_TOK_KW_PORT) {
            advance(p);
            JZASTNode *blk = parse_block(p, t, "PORT", JZ_AST_PORT_BLOCK);
            if (!blk) return -1;
            jz_ast_add_child(parent, blk);
        } else if (t->type == JZ_TOK_KW_WIRE) {
            advance(p);
            JZASTNode *blk = parse_block(p, t, "WIRE", JZ_AST_WIRE_BLOCK);
            if (!blk) return -1;
            jz_ast_add_child(parent, blk);
        } else if (t->type == JZ_TOK_KW_REGISTER) {
            advance(p);
            JZASTNode *blk = parse_block(p, t, "REGISTER", JZ_AST_REGISTER_BLOCK);
            if (!blk) return -1;
            jz_ast_add_child(parent, blk);
        } else if (t->type == JZ_TOK_KW_LATCH) {
            advance(p);
            JZASTNode *blk = parse_block(p, t, "LATCH", JZ_AST_LATCH_BLOCK);
            if (!blk) return -1;
            jz_ast_add_child(parent, blk);
        } else if (t->type == JZ_TOK_KW_MEM) {
            advance(p);
            JZASTNode *blk = parse_block(p, t, "MEM", JZ_AST_MEM_BLOCK);
            if (!blk) return -1;
            jz_ast_add_child(parent, blk);
        } else if (t->type == JZ_TOK_KW_MUX) {
            advance(p);
            JZASTNode *blk = parse_block(p, t, "MUX", JZ_AST_MUX_BLOCK);
            if (!blk) return -1;
            jz_ast_add_child(parent, blk);
        } else if (t->type == JZ_TOK_KW_CDC) {
            advance(p);
            JZASTNode *blk = parse_block(p, t, "CDC", JZ_AST_BLOCK);
            if (!blk) return -1;
            jz_ast_add_child(parent, blk);
        } else if (t->type == JZ_TOK_KW_ASYNC) {
            advance(p);
            JZASTNode *blk = parse_block(p, t, "ASYNCHRONOUS", JZ_AST_BLOCK);
            if (!blk) return -1;
            jz_ast_add_child(parent, blk);
        } else if (t->type == JZ_TOK_KW_SYNC) {
            advance(p);
            JZASTNode *blk = parse_block(p, t, "SYNCHRONOUS", JZ_AST_BLOCK);
            if (!blk) return -1;
            jz_ast_add_child(parent, blk);
        } else if (t->type == JZ_TOK_KW_NEW) {
            advance(p);
            JZASTNode *inst = parse_module_instantiation(p);
            if (!inst) return -1;
            jz_ast_add_child(parent, inst);
        } else if (t->type == JZ_TOK_KW_CHECK) {
            advance(p);
            JZASTNode *chk = parse_check(p);
            if (!chk) return -1;
            jz_ast_add_child(parent, chk);
        } else if (t->type == JZ_TOK_SEMICOLON) {
            advance(p);
        } else if (t->type == JZ_TOK_IDENTIFIER && t->lexeme && t->lexeme[0] == '@') {
            /* Unrecognized @directive — likely a typo like @feature_else. */
            parser_error(p, "unrecognized directive in @feature block");
            advance(p);
        } else {
            /* Skip unknown tokens silently for error recovery. */
            advance(p);
        }
    }
}

/**
 * @brief Parse the body of a @blackbox definition.
 *
 * A blackbox body is restricted to structural declarations only and may
 * contain:
 * - CONST blocks
 * - PORT blocks
 *
 * No executable logic or other block types are permitted. Parsing continues
 * until the closing '}' brace is encountered.
 *
 * @param p  Active parser
 * @param bb Blackbox AST node
 * @return 0 on success, -1 on error
 */
int parse_blackbox_body(Parser *p, JZASTNode *bb) {
    for (;;) {
        const JZToken *t = peek(p);
        if (t->type == JZ_TOK_RBRACE) {
            advance(p); /* consume '}' */
            return 0;
        }
        if (t->type == JZ_TOK_EOF) {
            parser_error(p, "unterminated @blackbox body (missing '}' )");
            return -1;
        }

        if (t->type == JZ_TOK_KW_CONST) {
            advance(p);
            JZASTNode *blk = parse_block(p, t, "CONST", JZ_AST_CONST_BLOCK);
            if (!blk) {
                return -1;
            }
            if (jz_ast_add_child(bb, blk) != 0) {
                jz_ast_free(blk);
                return -1;
            }
        } else if (t->type == JZ_TOK_KW_PORT) {
            advance(p);
            JZASTNode *blk = parse_block(p, t, "PORT", JZ_AST_PORT_BLOCK);
            if (!blk) {
                return -1;
            }
            if (jz_ast_add_child(bb, blk) != 0) {
                jz_ast_free(blk);
                return -1;
            }
        } else {
            parser_error(p, "unexpected token in @blackbox body; expected CONST or PORT");
            return -1;
        }
    }
}
