/**
 * @file parser_expression.c
 * @brief Expression parsing with full operator precedence for the JZ HDL.
 *
 * This file implements the recursive-descent expression parser used throughout
 * the JZ HDL compiler. Expressions are parsed with standard precedence and
 * associativity, producing a structured AST suitable for later semantic and
 * code-generation passes.
 *
 * Supported features include:
 * - Unary, binary, and ternary operators
 * - Bitwise and logical operations
 * - Slices and indexing
 * - Concatenation and replication
 * - Built-in function calls
 * - Qualified identifiers
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser_internal.h"

/**
 * @brief Parse postfix expressions such as slicing and indexing.
 *
 * This handles constructs of the form:
 *   base[idx]
 *   base[msb:lsb]
 *
 * Multiple slices may be chained. Single indices are normalized into
 * [idx:idx] slices using distinct AST nodes to preserve tree ownership.
 *
 * @param p Active parser
 * @return Expression AST node, or NULL on error
 */
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

static JZASTNode *parse_postfix_expr(Parser *p) {
    JZASTNode *expr = parse_primary_expr(p);
    if (!expr) return NULL;

    for (;;) {
        if (!match(p, JZ_TOK_LBRACKET)) {
            break;
        }
        int is_wildcard = 0;
        JZASTNode *msb = NULL;
        if (peek(p)->type == JZ_TOK_OP_STAR) {
            is_wildcard = 1;
            advance(p);
        } else {
            msb = parse_simple_index_expr(p);
            if (!msb) {
                jz_ast_free(expr);
                return NULL;
            }
        }

        JZASTNode *lsb = NULL;
        int is_slice = 0;
        if (!is_wildcard && match(p, JZ_TOK_OP_COLON)) {
            /* Full slice [msb:lsb]. */
            is_slice = 1;
            lsb = parse_simple_index_expr(p);
            if (!lsb) {
                jz_ast_free(expr);
                jz_ast_free(msb);
                return NULL;
            }
        }

        if (!match(p, JZ_TOK_RBRACKET)) {
            if (lsb) jz_ast_free(lsb);
            if (msb) jz_ast_free(msb);
            jz_ast_free(expr);
            parser_error(p, "expected ']' after slice/index expression");
            return NULL;
        }

        if (!is_slice && match(p, JZ_TOK_DOT)) {
            const JZToken *member_tok = peek(p);
            if (!is_decl_identifier_token(member_tok) || !member_tok->lexeme) {
                if (lsb) jz_ast_free(lsb);
                if (msb) jz_ast_free(msb);
                jz_ast_free(expr);
                parser_error(p, "expected BUS member identifier after '.'");
                return NULL;
            }

            if (!expr || expr->type != JZ_AST_EXPR_IDENTIFIER || !expr->name) {
                if (lsb) jz_ast_free(lsb);
                if (msb) jz_ast_free(msb);
                jz_ast_free(expr);
                parser_error(p, "expected BUS port identifier before member access");
                return NULL;
            }

            JZASTNode *bus = jz_ast_new(JZ_AST_EXPR_BUS_ACCESS, expr->loc);
            if (!bus) {
                if (lsb) jz_ast_free(lsb);
                if (msb) jz_ast_free(msb);
                jz_ast_free(expr);
                return NULL;
            }
            jz_ast_set_name(bus, expr->name);
            jz_ast_set_text(bus, member_tok->lexeme);
            if (is_wildcard) {
                jz_ast_set_block_kind(bus, "WILDCARD");
            } else if (msb) {
                if (jz_ast_add_child(bus, msb) != 0) {
                    jz_ast_free(bus);
                    jz_ast_free(msb);
                    jz_ast_free(expr);
                    return NULL;
                }
            }
            advance(p); /* consume member identifier */
            jz_ast_free(expr);
            expr = bus;
            continue;
        }

        if (is_wildcard) {
            if (lsb) jz_ast_free(lsb);
            jz_ast_free(expr);
            parser_error(p, "wildcard index is only valid for BUS member access");
            return NULL;
        }

        if (!is_slice) {
            /* Single index [idx] -> treat as [idx:idx] but with two distinct nodes
             * so that AST ownership remains tree-shaped (no shared children). */
            lsb = clone_expr_tree(msb);
            if (!lsb) {
                jz_ast_free(expr);
                jz_ast_free(msb);
                return NULL;
            }
        }

        JZASTNode *slice = jz_ast_new(JZ_AST_EXPR_SLICE, expr->loc);
        if (!slice) {
            if (lsb) jz_ast_free(lsb);
            jz_ast_free(expr);
            jz_ast_free(msb);
            return NULL;
        }
        if (jz_ast_add_child(slice, expr) != 0 ||
            jz_ast_add_child(slice, msb) != 0 ||
            jz_ast_add_child(slice, lsb) != 0) {
            jz_ast_free(slice);
            jz_ast_free(lsb);
            jz_ast_free(expr);
            jz_ast_free(msb);
            return NULL;
        }
        expr = slice;
    }

    return expr;
}

/**
 * @brief Parse either a concatenation or a primary/postfix expression.
 *
 * Supports:
 * - Standard concatenation: { expr, expr, ... }
 * - Replication: { N{expr} }
 *
 * @param p Active parser
 * @return Expression AST node, or NULL on error
 */
static JZASTNode *parse_concat_or_primary_expr(Parser *p) {
    if (peek(p)->type == JZ_TOK_LBRACE) {
        /* Concatenation: { expr, expr, ... } or replication {N{expr}}. */
        const JZToken *lbrace = peek(p);
        advance(p); /* consume '{' */

        /* Check for replication form: NUMBER '{' expr '}' '}'. */
        if (peek(p)->type == JZ_TOK_NUMBER || peek(p)->type == JZ_TOK_SIZED_NUMBER) {
            /* Look ahead for the inner '{' without consuming too far on error. */
            size_t save_pos = p->pos;
            const JZToken *rep_count_tok = peek(p);
            advance(p); /* consume count */
            if (match(p, JZ_TOK_LBRACE)) {
                /* Treat as replication: { N{expr} } -> Concat where first child is
                 * the literal N and second child is the replicated expression. */
                JZASTNode *concat = jz_ast_new(JZ_AST_EXPR_CONCAT, lbrace->loc);
                if (!concat) {
                    return NULL;
                }

                /* N as a literal child. */
                JZASTNode *count_lit = jz_ast_new(JZ_AST_EXPR_LITERAL, rep_count_tok->loc);
                if (!count_lit) {
                    jz_ast_free(concat);
                    return NULL;
                }
                if (rep_count_tok->lexeme) {
                    jz_ast_set_text(count_lit, rep_count_tok->lexeme);
                }
                if (jz_ast_add_child(concat, count_lit) != 0) {
                    jz_ast_free(count_lit);
                    jz_ast_free(concat);
                    return NULL;
                }

                /* Inner expression. */
                JZASTNode *elem = parse_expression(p);
                if (!elem) {
                    jz_ast_free(concat);
                    return NULL;
                }
                if (!match(p, JZ_TOK_RBRACE)) {
                    jz_ast_free(elem);
                    jz_ast_free(concat);
                    parser_error(p, "expected '}' after replication expression");
                    return NULL;
                }

                if (jz_ast_add_child(concat, elem) != 0) {
                    jz_ast_free(elem);
                    jz_ast_free(concat);
                    return NULL;
                }

                if (!match(p, JZ_TOK_RBRACE)) {
                    jz_ast_free(concat);
                    parser_error(p, "expected '}' at end of concatenation");
                    return NULL;
                }
                return concat;
            }

            /* Not actually replication; rewind and parse as normal concatenation. */
            p->pos = save_pos;
        }

        /* Standard concatenation { expr, expr, ... }. */
        JZASTNode *concat = jz_ast_new(JZ_AST_EXPR_CONCAT, lbrace->loc);
        if (!concat) return NULL;

        if (peek(p)->type != JZ_TOK_RBRACE) {
            for (;;) {
                JZASTNode *elem = parse_expression(p);
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
            parser_error(p, "expected '}' at end of concatenation");
            return NULL;
        }
        return concat;
    }

    return parse_postfix_expr(p);
}

/**
 * @brief Parse unary expressions.
 *
 * Supported operators:
 *   ~  !  +  -
 *
 * @param p Active parser
 * @return Expression AST node, or NULL on error
 */
static JZASTNode *parse_unary_expr(Parser *p) {
    const JZToken *t = peek(p);
    if (t->type == JZ_TOK_OP_TILDE ||
        t->type == JZ_TOK_OP_BANG  ||
        t->type == JZ_TOK_OP_PLUS ||
        t->type == JZ_TOK_OP_MINUS) {
        JZASTNode *operand;
        JZASTNode *un;
        const char *kind = NULL;

        if (t->type == JZ_TOK_OP_TILDE) kind = "BIT_NOT";
        else if (t->type == JZ_TOK_OP_BANG) kind = "LOG_NOT";
        else if (t->type == JZ_TOK_OP_PLUS) kind = "POS";
        else if (t->type == JZ_TOK_OP_MINUS) kind = "NEG";

        JZLocation loc = t->loc;
        advance(p);
        operand = parse_unary_expr(p);
        if (!operand) return NULL;

        un = jz_ast_new(JZ_AST_EXPR_UNARY, loc);
        if (!un) {
            jz_ast_free(operand);
            return NULL;
        }
        if (kind) jz_ast_set_block_kind(un, kind);
        if (jz_ast_add_child(un, operand) != 0) {
            jz_ast_free(un);
            jz_ast_free(operand);
            return NULL;
        }
        return un;
    }

    return parse_concat_or_primary_expr(p);
}

/**
 * @brief Parse multiplicative expressions.
 *
 * This function implements the precedence level for multiplicative operators:
 *
 *   *   /   %
 *
 * Expressions are parsed left-associatively. Operands are parsed using the
 * unary-expression level, making multiplication, division, and modulo bind
 * more tightly than addition and subtraction.
 *
 * @param p Active parser
 * @return Expression AST node, or NULL on error
 */
static JZASTNode *parse_multiplicative_expr(Parser *p) {
    JZASTNode *left = parse_unary_expr(p);
    if (!left) return NULL;

    for (;;) {
        const JZToken *t = peek(p);
        const char *op = NULL;
        if (t->type == JZ_TOK_OP_STAR) op = "MUL";
        else if (t->type == JZ_TOK_OP_SLASH) op = "DIV";
        else if (t->type == JZ_TOK_OP_PERCENT) op = "MOD";
        else break;

        JZLocation loc = t->loc;
        advance(p);
        JZASTNode *right = parse_unary_expr(p);
        if (!right) {
            jz_ast_free(left);
            return NULL;
        }
        JZASTNode *bin = jz_ast_new(JZ_AST_EXPR_BINARY, loc);
        if (!bin) {
            jz_ast_free(left);
            jz_ast_free(right);
            return NULL;
        }
        jz_ast_set_block_kind(bin, op);
        if (jz_ast_add_child(bin, left) != 0 ||
            jz_ast_add_child(bin, right) != 0) {
            jz_ast_free(bin);
            jz_ast_free(left);
            jz_ast_free(right);
            return NULL;
        }
        left = bin;
    }
    return left;
}

/**
 * @brief Parse additive expressions.
 *
 * This function implements the precedence level for addition and subtraction:
 *
 *   +   -
 *
 * Expressions are parsed left-associatively. Operands are parsed using the
 * multiplicative-expression level.
 *
 * @param p Active parser
 * @return Expression AST node, or NULL on error
 */
static JZASTNode *parse_additive_expr(Parser *p) {
    JZASTNode *left = parse_multiplicative_expr(p);
    if (!left) return NULL;

    for (;;) {
        const JZToken *t = peek(p);
        const char *op = NULL;
        if (t->type == JZ_TOK_OP_PLUS) op = "ADD";
        else if (t->type == JZ_TOK_OP_MINUS) op = "SUB";
        else break;

        JZLocation loc = t->loc;
        advance(p);
        JZASTNode *right = parse_multiplicative_expr(p);
        if (!right) {
            jz_ast_free(left);
            return NULL;
        }
        JZASTNode *bin = jz_ast_new(JZ_AST_EXPR_BINARY, loc);
        if (!bin) {
            jz_ast_free(left);
            jz_ast_free(right);
            return NULL;
        }
        jz_ast_set_block_kind(bin, op);
        if (jz_ast_add_child(bin, left) != 0 ||
            jz_ast_add_child(bin, right) != 0) {
            jz_ast_free(bin);
            jz_ast_free(left);
            jz_ast_free(right);
            return NULL;
        }
        left = bin;
    }
    return left;
}

/**
 * @brief Parse shift expressions.
 *
 * This function implements the precedence level for shift operators:
 *
 *   <<   >>   >>>
 *
 * Expressions are parsed left-associatively. Operands are parsed using the
 * additive-expression level, making shifts bind more weakly than addition
 * and subtraction.
 *
 * @param p Active parser
 * @return Expression AST node, or NULL on error
 */
static JZASTNode *parse_shift_expr(Parser *p) {
    JZASTNode *left = parse_additive_expr(p);
    if (!left) return NULL;

    for (;;) {
        const JZToken *t = peek(p);
        const char *op = NULL;
        if (t->type == JZ_TOK_OP_SHL) op = "SHL";
        else if (t->type == JZ_TOK_OP_SHR) op = "SHR";
        else if (t->type == JZ_TOK_OP_ASHR) op = "ASHR";
        else break;

        JZLocation loc = t->loc;
        advance(p);
        JZASTNode *right = parse_additive_expr(p);
        if (!right) {
            jz_ast_free(left);
            return NULL;
        }
        JZASTNode *bin = jz_ast_new(JZ_AST_EXPR_BINARY, loc);
        if (!bin) {
            jz_ast_free(left);
            jz_ast_free(right);
            return NULL;
        }
        jz_ast_set_block_kind(bin, op);
        if (jz_ast_add_child(bin, left) != 0 ||
            jz_ast_add_child(bin, right) != 0) {
            jz_ast_free(bin);
            jz_ast_free(left);
            jz_ast_free(right);
            return NULL;
        }
        left = bin;
    }
    return left;
}

/**
 * @brief Parse relational comparison expressions.
 *
 * This function implements the precedence level for relational operators:
 *
 *   <   <=   >   >=
 *
 * Expressions are parsed left-associatively. Operands are parsed using the
 * shift-expression level, making relational operators bind more weakly than
 * shifts and more tightly than equality operators.
 *
 * @param p Active parser
 * @return Expression AST node, or NULL on error
 */
static JZASTNode *parse_relational_expr(Parser *p) {
    JZASTNode *left = parse_shift_expr(p);
    if (!left) return NULL;

    for (;;) {
        const JZToken *t = peek(p);
        const char *op = NULL;
        if (t->type == JZ_TOK_OP_LT) op = "LT";
        else if (t->type == JZ_TOK_OP_LE) op = "LE";
        else if (t->type == JZ_TOK_OP_GT) op = "GT";
        else if (t->type == JZ_TOK_OP_GE) op = "GE";
        else break;

        JZLocation loc = t->loc;
        advance(p);
        JZASTNode *right = parse_shift_expr(p);
        if (!right) {
            jz_ast_free(left);
            return NULL;
        }
        JZASTNode *bin = jz_ast_new(JZ_AST_EXPR_BINARY, loc);
        if (!bin) {
            jz_ast_free(left);
            jz_ast_free(right);
            return NULL;
        }
        jz_ast_set_block_kind(bin, op);
        if (jz_ast_add_child(bin, left) != 0 ||
            jz_ast_add_child(bin, right) != 0) {
            jz_ast_free(bin);
            jz_ast_free(left);
            jz_ast_free(right);
            return NULL;
        }
        left = bin;
    }
    return left;
}

/**
 * @brief Parse equality comparison expressions.
 *
 * This function implements the precedence level for equality operators:
 *
 *   ==   !=
 *
 * Expressions are parsed left-associatively. Operands are parsed using the
 * relational-expression level, making equality operators bind more weakly
 * than relational operators.
 *
 * @param p Active parser
 * @return Expression AST node, or NULL on error
 */
static JZASTNode *parse_equality_expr(Parser *p) {
    JZASTNode *left = parse_relational_expr(p);
    if (!left) return NULL;

    for (;;) {
        const JZToken *t = peek(p);
        const char *op = NULL;
        if (t->type == JZ_TOK_OP_EQ) op = "EQ";
        else if (t->type == JZ_TOK_OP_NEQ) op = "NEQ";
        else break;

        JZLocation loc = t->loc;
        advance(p);
        JZASTNode *right = parse_relational_expr(p);
        if (!right) {
            jz_ast_free(left);
            return NULL;
        }
        JZASTNode *bin = jz_ast_new(JZ_AST_EXPR_BINARY, loc);
        if (!bin) {
            jz_ast_free(left);
            jz_ast_free(right);
            return NULL;
        }
        jz_ast_set_block_kind(bin, op);
        if (jz_ast_add_child(bin, left) != 0 ||
            jz_ast_add_child(bin, right) != 0) {
            jz_ast_free(bin);
            jz_ast_free(left);
            jz_ast_free(right);
            return NULL;
        }
        left = bin;
    }
    return left;
}

/**
 * @brief Parse bitwise AND expressions ('&').
 *
 * This function implements the precedence level for bitwise AND operations.
 * It parses left-associative chains of the form:
 *
 *   expr & expr & expr ...
 *
 * The operands are parsed using the equality-expression level, making
 * bitwise AND bind more tightly than equality operators and less tightly
 * than XOR.
 *
 * @param p Active parser
 * @return Expression AST node, or NULL on error
 */
static JZASTNode *parse_bitwise_and_expr(Parser *p) {
    JZASTNode *left = parse_equality_expr(p);
    if (!left) return NULL;
    for (;;) {
        const JZToken *t = peek(p);
        if (t->type != JZ_TOK_OP_AMP) break;
        JZLocation loc = t->loc;
        advance(p);
        JZASTNode *right = parse_equality_expr(p);
        if (!right) {
            jz_ast_free(left);
            return NULL;
        }
        JZASTNode *bin = jz_ast_new(JZ_AST_EXPR_BINARY, loc);
        if (!bin) {
            jz_ast_free(left);
            jz_ast_free(right);
            return NULL;
        }
        jz_ast_set_block_kind(bin, "BIT_AND");
        if (jz_ast_add_child(bin, left) != 0 ||
            jz_ast_add_child(bin, right) != 0) {
            jz_ast_free(bin);
            jz_ast_free(left);
            jz_ast_free(right);
            return NULL;
        }
        left = bin;
    }
    return left;
}

/**
 * @brief Parse bitwise AND expressions ('&').
 *
 * This precedence level sits above equality comparisons and below XOR.
 *
 * @param p Active parser
 * @return Expression AST node, or NULL on error
 */
static JZASTNode *parse_bitwise_xor_expr(Parser *p) {
    JZASTNode *left = parse_bitwise_and_expr(p);
    if (!left) return NULL;
    for (;;) {
        const JZToken *t = peek(p);
        if (t->type != JZ_TOK_OP_CARET) break;
        JZLocation loc = t->loc;
        advance(p);
        JZASTNode *right = parse_bitwise_and_expr(p);
        if (!right) {
            jz_ast_free(left);
            return NULL;
        }
        JZASTNode *bin = jz_ast_new(JZ_AST_EXPR_BINARY, loc);
        if (!bin) {
            jz_ast_free(left);
            jz_ast_free(right);
            return NULL;
        }
        jz_ast_set_block_kind(bin, "BIT_XOR");
        if (jz_ast_add_child(bin, left) != 0 ||
            jz_ast_add_child(bin, right) != 0) {
            jz_ast_free(bin);
            jz_ast_free(left);
            jz_ast_free(right);
            return NULL;
        }
        left = bin;
    }
    return left;
}

/**
 * @brief Parse bitwise OR expressions ('|').
 *
 * @param p Active parser
 * @return Expression AST node, or NULL on error
 */
static JZASTNode *parse_bitwise_or_expr(Parser *p) {
    JZASTNode *left = parse_bitwise_xor_expr(p);
    if (!left) return NULL;
    for (;;) {
        const JZToken *t = peek(p);
        if (t->type != JZ_TOK_OP_PIPE) break;
        JZLocation loc = t->loc;
        advance(p);
        JZASTNode *right = parse_bitwise_xor_expr(p);
        if (!right) {
            jz_ast_free(left);
            return NULL;
        }
        JZASTNode *bin = jz_ast_new(JZ_AST_EXPR_BINARY, loc);
        if (!bin) {
            jz_ast_free(left);
            jz_ast_free(right);
            return NULL;
        }
        jz_ast_set_block_kind(bin, "BIT_OR");
        if (jz_ast_add_child(bin, left) != 0 ||
            jz_ast_add_child(bin, right) != 0) {
            jz_ast_free(bin);
            jz_ast_free(left);
            jz_ast_free(right);
            return NULL;
        }
        left = bin;
    }
    return left;
}

/**
 * @brief Parse logical AND expressions ('&&').
 *
 * @param p Active parser
 * @return Expression AST node, or NULL on error
 */
static JZASTNode *parse_logical_and_expr(Parser *p) {
    JZASTNode *left = parse_bitwise_or_expr(p);
    if (!left) return NULL;
    for (;;) {
        const JZToken *t = peek(p);
        if (t->type != JZ_TOK_OP_AND_AND) break;
        JZLocation loc = t->loc;
        advance(p);
        JZASTNode *right = parse_bitwise_or_expr(p);
        if (!right) {
            jz_ast_free(left);
            return NULL;
        }
        JZASTNode *bin = jz_ast_new(JZ_AST_EXPR_BINARY, loc);
        if (!bin) {
            jz_ast_free(left);
            jz_ast_free(right);
            return NULL;
        }
        jz_ast_set_block_kind(bin, "LOG_AND");
        if (jz_ast_add_child(bin, left) != 0 ||
            jz_ast_add_child(bin, right) != 0) {
            jz_ast_free(bin);
            jz_ast_free(left);
            jz_ast_free(right);
            return NULL;
        }
        left = bin;
    }
    return left;
}

/**
 * @brief Parse logical OR expressions ('||').
 *
 * @param p Active parser
 * @return Expression AST node, or NULL on error
 */
static JZASTNode *parse_logical_or_expr(Parser *p) {
    JZASTNode *left = parse_logical_and_expr(p);
    if (!left) return NULL;
    for (;;) {
        const JZToken *t = peek(p);
        if (t->type != JZ_TOK_OP_OR_OR) break;
        JZLocation loc = t->loc;
        advance(p);
        JZASTNode *right = parse_logical_and_expr(p);
        if (!right) {
            jz_ast_free(left);
            return NULL;
        }
        JZASTNode *bin = jz_ast_new(JZ_AST_EXPR_BINARY, loc);
        if (!bin) {
            jz_ast_free(left);
            jz_ast_free(right);
            return NULL;
        }
        jz_ast_set_block_kind(bin, "LOG_OR");
        if (jz_ast_add_child(bin, left) != 0 ||
            jz_ast_add_child(bin, right) != 0) {
            jz_ast_free(bin);
            jz_ast_free(left);
            jz_ast_free(right);
            return NULL;
        }
        left = bin;
    }
    return left;
}

/**
 * @brief Parse ternary conditional expressions.
 *
 * Grammar:
 *   cond ? true_expr : false_expr
 *
 * Ternary expressions are right-associative and have the lowest precedence
 * in the expression grammar.
 *
 * @param p Active parser
 * @return Expression AST node, or NULL on error
 */
static JZASTNode *parse_ternary_expr(Parser *p) {
    JZASTNode *cond = parse_logical_or_expr(p);
    if (!cond) return NULL;

    if (!match(p, JZ_TOK_OP_QUESTION)) {
        return cond;
    }

    JZLocation loc = cond->loc;
    JZASTNode *true_expr = parse_expression(p);
    if (!true_expr) {
        jz_ast_free(cond);
        return NULL;
    }
    if (!match(p, JZ_TOK_OP_COLON)) {
        jz_ast_free(cond);
        jz_ast_free(true_expr);
        parser_error(p, "expected ':' in ternary expression");
        return NULL;
    }
    JZASTNode *false_expr = parse_ternary_expr(p);
    if (!false_expr) {
        jz_ast_free(cond);
        jz_ast_free(true_expr);
        return NULL;
    }

    JZASTNode *tern = jz_ast_new(JZ_AST_EXPR_TERNARY, loc);
    if (!tern) {
        jz_ast_free(cond);
        jz_ast_free(true_expr);
        jz_ast_free(false_expr);
        return NULL;
    }
    if (jz_ast_add_child(tern, cond) != 0 ||
        jz_ast_add_child(tern, true_expr) != 0 ||
        jz_ast_add_child(tern, false_expr) != 0) {
        jz_ast_free(tern);
        jz_ast_free(cond);
        jz_ast_free(true_expr);
        jz_ast_free(false_expr);
        return NULL;
    }
    return tern;
}

/**
 * @brief Parse a primary expression.
 *
 * Primary expressions are the leaves of the expression grammar and include:
 * - Numeric literals
 * - Identifiers and qualified identifiers
 * - Built-in function calls
 * - Parenthesized expressions
 *
 * Reserved keywords are intentionally treated as identifier-like in this
 * context so that semantic analysis can emit KEYWORD_AS_IDENTIFIER instead
 * of triggering a generic parse error.
 *
 * @param p Active parser
 * @return Expression AST node, or NULL on error
 */
JZASTNode *parse_primary_expr(Parser *p) {
    const JZToken *t = peek(p);

    if (t->type == JZ_TOK_NUMBER || t->type == JZ_TOK_SIZED_NUMBER) {
        /* Literal expression: store lexeme in text. */
        JZASTNode *lit = jz_ast_new(JZ_AST_EXPR_LITERAL, t->loc);
        if (!lit) return NULL;
        if (t->lexeme) {
            jz_ast_set_text(lit, t->lexeme);
        }
        advance(p);
        return lit;
    }

    /* Special semantic drivers: GND and VCC (polymorphic width constants) */
    if (t->type == JZ_TOK_KW_GND || t->type == JZ_TOK_KW_VCC) {
        JZASTNode *special = jz_ast_new(JZ_AST_EXPR_SPECIAL_DRIVER, t->loc);
        if (!special) return NULL;
        jz_ast_set_block_kind(special, t->type == JZ_TOK_KW_GND ? "GND" : "VCC");
        advance(p);
        return special;
    }

    /* Treat reserved keywords as identifier-like when they appear in
     * expression position, so that semantic analysis can report
     * KEYWORD_AS_IDENTIFIER instead of the parser failing.
     */
    if (is_decl_identifier_token(t) || t->type == JZ_TOK_KW_CONFIG) {
        /* Built-in functions: uadd, sadd, umul, smul, gbit, sbit, gslice, sslice,
         * lit(...), clog2(...), widthof(...)
         */
        const char *name = t->lexeme;
        const JZToken *next = NULL;
        if (t->type == JZ_TOK_IDENTIFIER &&
            name &&
            (!strcmp(name, "uadd") || !strcmp(name, "sadd") ||
             !strcmp(name, "umul") || !strcmp(name, "smul") ||
             !strcmp(name, "gbit") || !strcmp(name, "sbit") ||
             !strcmp(name, "gslice") || !strcmp(name, "sslice") ||
             !strcmp(name, "oh2b") ||
             !strcmp(name, "b2oh") || !strcmp(name, "prienc") ||
             !strcmp(name, "lzc") ||
             !strcmp(name, "usub") || !strcmp(name, "ssub") ||
             !strcmp(name, "abs") ||
             !strcmp(name, "umin") || !strcmp(name, "umax") ||
             !strcmp(name, "smin") || !strcmp(name, "smax") ||
             !strcmp(name, "popcount") ||
             !strcmp(name, "reverse") || !strcmp(name, "bswap") ||
             !strcmp(name, "reduce_and") || !strcmp(name, "reduce_or") ||
             !strcmp(name, "reduce_xor") ||
             !strcmp(name, "lit") || !strcmp(name, "clog2") ||
             !strcmp(name, "widthof") || !strcmp(name, "bwidth"))) {
            next = &p->tokens[p->pos + 1 < p->count ? p->pos + 1 : p->count - 1];
            if (next->type == JZ_TOK_LPAREN) {
                /* Builtin call */
                JZASTNode *call = jz_ast_new(JZ_AST_EXPR_BUILTIN_CALL, t->loc);
                if (!call) return NULL;
                jz_ast_set_name(call, name);
                advance(p); /* ident */
                advance(p); /* '(' */
                /* Parse zero or more comma-separated arguments. */
                if (peek(p)->type != JZ_TOK_RPAREN) {
                    for (;;) {
                        JZASTNode *arg = parse_expression(p);
                        if (!arg) { jz_ast_free(call); return NULL; }
                        if (jz_ast_add_child(call, arg) != 0) {
                            jz_ast_free(arg);
                            jz_ast_free(call);
                            return NULL;
                        }
                        if (!match(p, JZ_TOK_COMMA)) break;
                    }
                }
                if (!match(p, JZ_TOK_RPAREN)) {
                    jz_ast_free(call);
                    parser_error(p, "expected ')' after builtin call arguments");
                    return NULL;
                }
                return call;
            }
        }

        /* Qualified identifier: id ('.' id)*. The head may be a regular
         * identifier or the CONFIG keyword (for CONFIG.<name> usage).
         */
        JZLocation loc = t->loc;
        char *buf = NULL;
        size_t buf_sz = 0;
        for (;;) {
            const JZToken *id = peek(p);
            if ((!is_decl_identifier_token(id) && id->type != JZ_TOK_KW_CONFIG) ||
                !id->lexeme) {
                break;
            }
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
            /* append '.' */
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
            parser_error(p, "expected identifier");
            return NULL;
        }

        /* If there was only one identifier and no dot, treat as simple Identifier. */
        JZASTNodeType type = (strchr(buf, '.') != NULL) ?
            JZ_AST_EXPR_QUALIFIED_IDENTIFIER : JZ_AST_EXPR_IDENTIFIER;
        JZASTNode *id_node = jz_ast_new(type, loc);
        if (!id_node) {
            free(buf);
            return NULL;
        }
        jz_ast_set_name(id_node, buf);
        free(buf);
        return id_node;
    }

    if (t->type == JZ_TOK_LPAREN) {
        advance(p);
        JZASTNode *inner = parse_expression(p);
        if (!inner) return NULL;
        if (!match(p, JZ_TOK_RPAREN)) {
            jz_ast_free(inner);
            parser_error(p, "expected ')' after expression");
            return NULL;
        }
        return inner;
    }

    parser_error(p, "expected primary expression");
    return NULL;
}

/**
 * @brief Parse a simple index expression used in slices and array indexing.
 *
 * This parser is intentionally restricted and is used for expressions inside
 * brackets such as:
 *   base[idx]
 *   base[msb:lsb]
 *
 * Accepted forms:
 * - Numeric literals (including sized numbers)
 * - Identifiers
 * - Concatenation expressions beginning with '{'
 *
 * When the index begins with '{', parsing is delegated to the full expression
 * parser to support forms like `{buf, addr}`. Parsing stops before the closing
 * ']' or ':' delimiter, which is consumed by the caller.
 *
 * @param p Active parser
 * @return Expression AST node representing the index, or NULL on error
 */
JZASTNode *parse_simple_index_expr(Parser *p) {
    const JZToken *t = peek(p);

    /* When the index starts with '{', delegate to the full expression parser so
     * that forms like {buf, addr} are accepted. The expression parser will
     * stop at the closing ']' or ':' delimiter, leaving it for the caller.
     */
    if (t->type == JZ_TOK_LBRACE ||
        t->type == JZ_TOK_LPAREN ||
        t->type == JZ_TOK_OP_TILDE ||
        t->type == JZ_TOK_OP_BANG ||
        t->type == JZ_TOK_OP_PLUS ||
        t->type == JZ_TOK_OP_MINUS ||
        t->type == JZ_TOK_NUMBER ||
        t->type == JZ_TOK_SIZED_NUMBER ||
        t->type == JZ_TOK_IDENTIFIER ||
        t->type == JZ_TOK_KW_CONFIG) {
        return parse_expression(p);
    }

    parser_error(p, "expected expression in index");
    return NULL;
}

/**
 * @brief Parse a full expression.
 *
 * This is the public entry point for expression parsing and begins at the
 * lowest-precedence level (ternary expressions).
 *
 * @param p Active parser
 * @return Expression AST node, or NULL on error
 */
JZASTNode *parse_expression(Parser *p) {
    return parse_ternary_expr(p);
}
