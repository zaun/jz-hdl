/*
 * emit_expr.c - Expression emission for the Verilog-2005 backend.
 *
 * This file handles lowering IR expressions to Verilog syntax with proper
 * operator precedence and intrinsic expansion.
 */
#include <stdio.h>
#include <string.h>

#include "verilog_internal.h"
#include "ir.h"

/* -------------------------------------------------------------------------
 * Operator precedence
 * -------------------------------------------------------------------------
 */

int expr_precedence(IR_ExprKind kind)
{
    /* Verilog-2005 operator precedence (higher number = tighter binding):
     *   14: primary (literals, signals, concat, slice)
     *   13: unary (~, -, !)
     *   12: *, /, %
     *   11: +, -
     *   10: <<, >>, >>>
     *    9: <, <=, >, >=
     *    8: ==, !=
     *    7: & (bitwise AND)
     *    6: ^ (bitwise XOR)
     *    5: | (bitwise OR)
     *    4: && (logical AND)
     *    3: || (logical OR)
     *    1: ?: (ternary)
     */
    switch (kind) {
        case EXPR_LITERAL:
        case EXPR_SIGNAL_REF:
        case EXPR_CONCAT:
        case EXPR_SLICE:
            return 14; /* primary */

        case EXPR_UNARY_NOT:
        case EXPR_UNARY_NEG:
        case EXPR_LOGICAL_NOT:
            return 13; /* unary */

        case EXPR_BINARY_MUL:
        case EXPR_BINARY_DIV:
        case EXPR_BINARY_MOD:
        case EXPR_INTRINSIC_UMUL:
        case EXPR_INTRINSIC_SMUL:
            return 12; /* multiplicative */

        case EXPR_BINARY_ADD:
        case EXPR_BINARY_SUB:
        case EXPR_INTRINSIC_UADD:
        case EXPR_INTRINSIC_SADD:
            return 11; /* additive */

        case EXPR_BINARY_SHL:
        case EXPR_BINARY_SHR:
        case EXPR_BINARY_ASHR:
            return 10; /* shifts */

        case EXPR_BINARY_LT:
        case EXPR_BINARY_GT:
        case EXPR_BINARY_LTE:
        case EXPR_BINARY_GTE:
            return 9;  /* relational */

        case EXPR_BINARY_EQ:
        case EXPR_BINARY_NEQ:
            return 8;  /* equality */

        case EXPR_BINARY_AND:
            return 7;  /* bitwise AND */
        case EXPR_BINARY_XOR:
            return 6;  /* bitwise XOR */
        case EXPR_BINARY_OR:
            return 5;  /* bitwise OR */

        case EXPR_LOGICAL_AND:
            return 4;  /* logical && */
        case EXPR_LOGICAL_OR:
            return 3;  /* logical || */

        case EXPR_TERNARY:
            return 1;  /* lowest among expressions */

        /* Treat intrinsics that expand to shifts/masks as primary expressions
         * once expanded; callers wrap them in parentheses when needed.
         */
        case EXPR_INTRINSIC_GBIT:
        case EXPR_INTRINSIC_SBIT:
        case EXPR_INTRINSIC_GSLICE:
        case EXPR_INTRINSIC_SSLICE:
        case EXPR_INTRINSIC_OH2B:
        case EXPR_INTRINSIC_B2OH:
        case EXPR_INTRINSIC_PRIENC:
        case EXPR_INTRINSIC_LZC:
        case EXPR_INTRINSIC_ABS:
        case EXPR_INTRINSIC_POPCOUNT:
        case EXPR_INTRINSIC_REVERSE:
        case EXPR_INTRINSIC_BSWAP:
        case EXPR_INTRINSIC_REDUCE_AND:
        case EXPR_INTRINSIC_REDUCE_OR:
        case EXPR_INTRINSIC_REDUCE_XOR:
            return 14;

        case EXPR_INTRINSIC_USUB:
        case EXPR_INTRINSIC_SSUB:
            return 11; /* additive */

        case EXPR_INTRINSIC_UMIN:
        case EXPR_INTRINSIC_UMAX:
        case EXPR_INTRINSIC_SMIN:
        case EXPR_INTRINSIC_SMAX:
            return 14; /* ternary expression, wrapped */

        default:
            return 0;
    }
}

/* -------------------------------------------------------------------------
 * Literal emission
 * -------------------------------------------------------------------------
 */

void emit_literal(FILE *out, const IR_Literal *lit)
{
    if (!lit) {
        fprintf(out, "1'b0");
        return;
    }
    if (lit->width <= 0) {
        /* Treat width-less literals as unsized decimal constants. */
        fprintf(out, "%llu", (unsigned long long)lit->words[0]);
        return;
    }

    int width = lit->width;

    /* High-impedance (all-z) literal. */
    if (lit->is_z) {
        fprintf(out, "%d'b", width);
        for (int i = 0; i < width; ++i) {
            fputc('z', out);
        }
        return;
    }

    fprintf(out, "%d'b", width);

    /* Print MSB-first binary with exactly `width` bits (multi-word). */
    for (int i = width - 1; i >= 0; --i) {
        int wi = i / 64;
        int bi = i % 64;
        unsigned bit = (wi < IR_LIT_WORDS)
                     ? (unsigned)((lit->words[wi] >> bi) & 1u)
                     : 0;
        fputc(bit ? '1' : '0', out);
    }
}

/* -------------------------------------------------------------------------
 * Intrinsic helpers
 * -------------------------------------------------------------------------
 */

bool is_intrinsic_expr(IR_ExprKind kind)
{
    switch (kind) {
        case EXPR_INTRINSIC_UADD:
        case EXPR_INTRINSIC_SADD:
        case EXPR_INTRINSIC_UMUL:
        case EXPR_INTRINSIC_SMUL:
        case EXPR_INTRINSIC_GBIT:
        case EXPR_INTRINSIC_SBIT:
        case EXPR_INTRINSIC_GSLICE:
        case EXPR_INTRINSIC_SSLICE:
        case EXPR_INTRINSIC_OH2B:
        case EXPR_INTRINSIC_B2OH:
        case EXPR_INTRINSIC_PRIENC:
        case EXPR_INTRINSIC_LZC:
        case EXPR_INTRINSIC_USUB:
        case EXPR_INTRINSIC_SSUB:
        case EXPR_INTRINSIC_ABS:
        case EXPR_INTRINSIC_UMIN:
        case EXPR_INTRINSIC_UMAX:
        case EXPR_INTRINSIC_SMIN:
        case EXPR_INTRINSIC_SMAX:
        case EXPR_INTRINSIC_POPCOUNT:
        case EXPR_INTRINSIC_REVERSE:
        case EXPR_INTRINSIC_BSWAP:
        case EXPR_INTRINSIC_REDUCE_AND:
        case EXPR_INTRINSIC_REDUCE_OR:
        case EXPR_INTRINSIC_REDUCE_XOR:
            return true;
        default:
            return false;
    }
}

/* Helper: emit a zero- or sign-extended version of `arg` to exactly
 * target_width bits using Verilog concatenation/replication.
 */
void emit_padded_expr(FILE *out,
                      const IR_Module *mod,
                      const IR_Expr *arg,
                      int target_width,
                      bool sign_extend)
{
    if (!arg || arg->width <= 0 || target_width <= arg->width) {
        emit_expr_internal(out, mod, arg, 10);
        return;
    }

    int pad = target_width - arg->width;
    if (pad <= 0) {
        emit_expr_internal(out, mod, arg, 10);
        return;
    }

    fputc('{', out);
    if (sign_extend) {
        /* Replicate the operand's MSB. For literal arguments we materialize
         * the sign bit as a constant to avoid indexing a constant.
         */
        if (arg->kind == EXPR_LITERAL && arg->u.literal.literal.width > 0) {
            const IR_Literal *lit = &arg->u.literal.literal;
            int msb_index = lit->width - 1;
            int wi = msb_index / 64;
            int bi = msb_index % 64;
            unsigned msb_bit = (wi < IR_LIT_WORDS) ? (unsigned)((lit->words[wi] >> bi) & 1u) : 0;
            fprintf(out, "{%d{1'b%d}}, ", pad, msb_bit ? 1 : 0);
        } else {
            fprintf(out, "{%d{", pad);
            emit_expr_internal(out, mod, arg, 10);
            fprintf(out, "[%d]}}, ", arg->width - 1);
        }
    } else {
        fprintf(out, "{%d{1'b0}}, ", pad);
    }
    emit_expr_internal(out, mod, arg, 10);
    fputc('}', out);
}

/* Lower intrinsic expressions to explicit, width-safe Verilog forms. */
static void emit_intrinsic_expr(FILE *out,
                                const IR_Module *mod,
                                const IR_Expr *expr,
                                int parent_prec)
{
    if (!expr) {
        fprintf(out, "1'b0");
        return;
    }

    int my_prec = expr_precedence(expr->kind);
    bool need_parens = my_prec < parent_prec;
    if (need_parens) {
        fputc('(', out);
    }

    switch (expr->kind) {
        case EXPR_INTRINSIC_UADD:
        case EXPR_INTRINSIC_SADD: {
            const IR_Expr *a = expr->u.binary.left;
            const IR_Expr *b = expr->u.binary.right;
            int result_w = expr->width;
            if (!a || !b || result_w <= 0) {
                fprintf(out, "1'b0");
                break;
            }

            bool is_signed = (expr->kind == EXPR_INTRINSIC_SADD);

            fputc('(', out);
            emit_padded_expr(out, mod, a, result_w, is_signed);
            fprintf(out, " + ");
            emit_padded_expr(out, mod, b, result_w, is_signed);
            fputc(')', out);
            break;
        }

        case EXPR_INTRINSIC_UMUL:
        case EXPR_INTRINSIC_SMUL: {
            const IR_Expr *a = expr->u.binary.left;
            const IR_Expr *b = expr->u.binary.right;
            int result_w = expr->width;
            if (!a || !b || result_w <= 0) {
                fprintf(out, "1'b0");
                break;
            }
            int max_bits = result_w / 2;
            if (max_bits <= 0) {
                emit_expr_internal(out, mod, a, 0);
                fprintf(out, " * ");
                emit_expr_internal(out, mod, b, 0);
                break;
            }

            bool is_signed = (expr->kind == EXPR_INTRINSIC_SMUL);

            fputc('(', out);
            if (is_signed) {
                fprintf(out, "$signed(");
                emit_padded_expr(out, mod, a, max_bits, is_signed);
                fprintf(out, ") * $signed(");
                emit_padded_expr(out, mod, b, max_bits, is_signed);
                fputc(')', out);
            } else {
                emit_padded_expr(out, mod, a, max_bits, is_signed);
                fprintf(out, " * ");
                emit_padded_expr(out, mod, b, max_bits, is_signed);
            }
            fputc(')', out);
            break;
        }

        case EXPR_INTRINSIC_GBIT: {
            /* gbit(source, index) → (source >> index) & 1'b1 */
            const IR_Expr *src = expr->u.intrinsic.source;
            const IR_Expr *idx = expr->u.intrinsic.index;
            if (!src || !idx) {
                fprintf(out, "1'b0");
                break;
            }
            fprintf(out, "((");
            emit_expr_internal(out, mod, src, expr_precedence(EXPR_BINARY_SHR));
            fprintf(out, " >> ");
            emit_expr_internal(out, mod, idx, 0);
            fprintf(out, ") & 1'b1)");
            break;
        }

        case EXPR_INTRINSIC_GSLICE: {
            const IR_Expr *src = expr->u.intrinsic.source;
            const IR_Expr *idx = expr->u.intrinsic.index;
            int elem_w = expr->u.intrinsic.element_width;
            if (elem_w <= 0) {
                elem_w = expr->width > 0 ? expr->width : 1;
            }
            if (!src || !idx || elem_w <= 0) {
                fprintf(out, "%d'b0", elem_w > 0 ? elem_w : 1);
                break;
            }
            fprintf(out, "((");
            emit_expr_internal(out, mod, src, expr_precedence(EXPR_BINARY_SHR));
            fprintf(out, " >> (");
            emit_expr_internal(out, mod, idx, expr_precedence(EXPR_BINARY_MUL));
            if (elem_w != 1) {
                fprintf(out, " * %d", elem_w);
            }
            fprintf(out, ")) & {%d{1'b1}})", elem_w);
            break;
        }

        case EXPR_INTRINSIC_SBIT: {
            const IR_Expr *src = expr->u.intrinsic.source;
            const IR_Expr *idx = expr->u.intrinsic.index;
            const IR_Expr *val = expr->u.intrinsic.value;
            int width = expr->width;
            if (!src || !idx || !val || width <= 0) {
                fprintf(out, "1'b0");
                break;
            }

            IR_Literal one_mask;
            memset(one_mask.words, 0, sizeof(one_mask.words));
            one_mask.words[0] = 1ULL;
            one_mask.width = width;
            one_mask.is_z = 0;

            fprintf(out, "((");
            emit_expr_internal(out, mod, src, expr_precedence(EXPR_BINARY_AND));
            fprintf(out, " & ~(");
            emit_literal(out, &one_mask);
            fprintf(out, " << ");
            emit_expr_internal(out, mod, idx, 0);
            fprintf(out, ")) | (({%d{", width);
            emit_expr_internal(out, mod, val, 0);
            fprintf(out, "}} & (");
            emit_literal(out, &one_mask);
            fprintf(out, " << ");
            emit_expr_internal(out, mod, idx, 0);
            fprintf(out, "))))");
            break;
        }

        case EXPR_INTRINSIC_OH2B: {
            /* One-hot to binary encoder: OR-tree.
             * For each result bit k, OR together all source bits whose
             * index i has bit k set.  Uses (source >> i) & 1'b1 pattern
             * to avoid bit-select on complex expressions.
             *
             * Each OR-chain is wrapped in reduction-OR |(...) to force
             * 1-bit width.  Without this, (N-bit >> i) & 1'b1 produces
             * an N-bit result, and the concat {N-bit, N-bit, ...} would
             * be result_w*N bits instead of result_w bits.
             */
            const IR_Expr *src = expr->u.intrinsic.source;
            int src_w = src ? src->width : 0;
            int result_w = expr->width;
            if (!src || src_w <= 0 || result_w <= 0) {
                fprintf(out, "1'b0");
                break;
            }
            if (result_w > 1) fprintf(out, "{");
            for (int k = result_w - 1; k >= 0; k--) {
                /* Collect source bit indices that contribute to result bit k. */
                int first = 1;
                int any = 0;
                fprintf(out, "|(");
                for (int i = 0; i < src_w; i++) {
                    if ((i >> k) & 1) {
                        if (!first) fprintf(out, " | ");
                        fprintf(out, "((");
                        emit_expr_internal(out, mod, src, expr_precedence(EXPR_BINARY_SHR));
                        fprintf(out, " >> %d) & 1'b1)", i);
                        first = 0;
                        any = 1;
                    }
                }
                if (!any) fprintf(out, "1'b0");
                fprintf(out, ")");
                if (k > 0 && result_w > 1) fprintf(out, ", ");
            }
            if (result_w > 1) fprintf(out, "}");
            break;
        }

        case EXPR_INTRINSIC_SSLICE: {
            const IR_Expr *src = expr->u.intrinsic.source;
            const IR_Expr *idx = expr->u.intrinsic.index;
            const IR_Expr *val = expr->u.intrinsic.value;
            int src_w = expr->width;
            int elem_w = expr->u.intrinsic.element_width;
            if (elem_w <= 0) {
                elem_w = val ? val->width : 0;
            }
            if (!src || !idx || !val || src_w <= 0 || elem_w <= 0) {
                fprintf(out, "1'b0");
                break;
            }

            fprintf(out, "((");
            emit_expr_internal(out, mod, src, expr_precedence(EXPR_BINARY_AND));
            fprintf(out, " & ~({%d{1'b1}} << ", elem_w);
            emit_expr_internal(out, mod, idx, 0);
            fprintf(out, ")) | ");
            fprintf(out, "((");
            emit_padded_expr(out, mod, val, src_w, /*sign_extend=*/false);
            fprintf(out, ") << ");
            emit_expr_internal(out, mod, idx, 0);
            fprintf(out, ") & ({%d{1'b1}} << ", elem_w);
            emit_expr_internal(out, mod, idx, 0);
            fprintf(out, "))");
            break;
        }

        /* ---- Reduction operators ---- */
        case EXPR_INTRINSIC_REDUCE_AND: {
            const IR_Expr *src = expr->u.intrinsic.source;
            if (!src || src->width <= 0) { fprintf(out, "1'b0"); break; }
            fprintf(out, "&(");
            emit_expr_internal(out, mod, src, 0);
            fprintf(out, ")");
            break;
        }
        case EXPR_INTRINSIC_REDUCE_OR: {
            const IR_Expr *src = expr->u.intrinsic.source;
            if (!src || src->width <= 0) { fprintf(out, "1'b0"); break; }
            fprintf(out, "|(");
            emit_expr_internal(out, mod, src, 0);
            fprintf(out, ")");
            break;
        }
        case EXPR_INTRINSIC_REDUCE_XOR: {
            const IR_Expr *src = expr->u.intrinsic.source;
            if (!src || src->width <= 0) { fprintf(out, "1'b0"); break; }
            fprintf(out, "^(");
            emit_expr_internal(out, mod, src, 0);
            fprintf(out, ")");
            break;
        }

        /* ---- Bit reversal ---- */
        case EXPR_INTRINSIC_REVERSE: {
            const IR_Expr *src = expr->u.intrinsic.source;
            int src_w = src ? src->width : 0;
            if (!src || src_w <= 0) { fprintf(out, "1'b0"); break; }
            /* Use shift-and-mask instead of bit-select [i] because Verilog
             * does not allow bit-select on concatenation expressions. */
            fprintf(out, "{");
            for (int i = 0; i < src_w; i++) {
                if (i > 0) fprintf(out, ", ");
                fprintf(out, "((");
                emit_expr_internal(out, mod, src, expr_precedence(EXPR_BINARY_SHR));
                fprintf(out, " >> %d) & 1'b1)", i);
            }
            fprintf(out, "}");
            break;
        }

        /* ---- Byte swap ---- */
        case EXPR_INTRINSIC_BSWAP: {
            const IR_Expr *src = expr->u.intrinsic.source;
            int src_w = src ? src->width : 0;
            if (!src || src_w <= 0 || (src_w % 8) != 0) { fprintf(out, "1'b0"); break; }
            int num_bytes = src_w / 8;
            /* Use shift-and-mask instead of part-select [hi:lo] because
             * Verilog does not allow part-select on concatenation expressions. */
            fprintf(out, "{");
            for (int i = 0; i < num_bytes; i++) {
                if (i > 0) fprintf(out, ", ");
                int lo = i * 8;
                fprintf(out, "((");
                emit_expr_internal(out, mod, src, expr_precedence(EXPR_BINARY_SHR));
                fprintf(out, " >> %d) & 8'hFF)", lo);
            }
            fprintf(out, "}");
            break;
        }

        /* ---- Unsigned/signed widening subtract ---- */
        case EXPR_INTRINSIC_USUB:
        case EXPR_INTRINSIC_SSUB: {
            const IR_Expr *a = expr->u.binary.left;
            const IR_Expr *b = expr->u.binary.right;
            int result_w = expr->width;
            if (!a || !b || result_w <= 0) { fprintf(out, "1'b0"); break; }
            bool is_signed = (expr->kind == EXPR_INTRINSIC_SSUB);
            fputc('(', out);
            emit_padded_expr(out, mod, a, result_w, is_signed);
            fprintf(out, " - ");
            emit_padded_expr(out, mod, b, result_w, is_signed);
            fputc(')', out);
            break;
        }

        /* ---- Unsigned/signed min/max ---- */
        case EXPR_INTRINSIC_UMIN:
        case EXPR_INTRINSIC_UMAX:
        case EXPR_INTRINSIC_SMIN:
        case EXPR_INTRINSIC_SMAX: {
            const IR_Expr *a = expr->u.binary.left;
            const IR_Expr *b = expr->u.binary.right;
            int result_w = expr->width;
            if (!a || !b || result_w <= 0) { fprintf(out, "1'b0"); break; }
            bool is_signed = (expr->kind == EXPR_INTRINSIC_SMIN || expr->kind == EXPR_INTRINSIC_SMAX);
            bool is_max = (expr->kind == EXPR_INTRINSIC_UMAX || expr->kind == EXPR_INTRINSIC_SMAX);
            const char *op = is_max ? ">" : "<";
            fprintf(out, "(");
            if (is_signed) {
                fprintf(out, "($signed(");
                emit_padded_expr(out, mod, a, result_w, /*sign_extend=*/true);
                fprintf(out, ") %s $signed(", op);
                emit_padded_expr(out, mod, b, result_w, /*sign_extend=*/true);
                fprintf(out, "))");
            } else {
                fprintf(out, "(");
                emit_padded_expr(out, mod, a, result_w, /*sign_extend=*/false);
                fprintf(out, " %s ", op);
                emit_padded_expr(out, mod, b, result_w, /*sign_extend=*/false);
                fprintf(out, ")");
            }
            fprintf(out, " ? ");
            emit_padded_expr(out, mod, a, result_w, is_signed);
            fprintf(out, " : ");
            emit_padded_expr(out, mod, b, result_w, is_signed);
            fprintf(out, ")");
            break;
        }

        /* ---- Absolute value ---- */
        case EXPR_INTRINSIC_ABS: {
            const IR_Expr *src = expr->u.intrinsic.source;
            int src_w = src ? src->width : 0;
            int result_w = expr->width;
            if (!src || src_w <= 0 || result_w <= 0) { fprintf(out, "1'b0"); break; }
            /* Use shift-and-mask instead of bit-select [N] for sign bit
             * because Verilog does not allow bit-select on concatenations. */
            fprintf(out, "(((");
            emit_expr_internal(out, mod, src, expr_precedence(EXPR_BINARY_SHR));
            fprintf(out, " >> %d) & 1'b1) ? (", src_w - 1);
            fprintf(out, "{%d{1'b0}} - {1'b0, ", result_w);
            emit_expr_internal(out, mod, src, 0);
            fprintf(out, "}) : {1'b0, ");
            emit_expr_internal(out, mod, src, 0);
            fprintf(out, "})");
            break;
        }

        /* ---- Population count ---- */
        case EXPR_INTRINSIC_POPCOUNT: {
            const IR_Expr *src = expr->u.intrinsic.source;
            int src_w = src ? src->width : 0;
            int result_w = expr->width;
            if (!src || src_w <= 0 || result_w <= 0) { fprintf(out, "1'b0"); break; }
            /* Use shift-and-mask instead of bit-select [i] because Verilog
             * does not allow bit-select on concatenation expressions. */
            fprintf(out, "(");
            for (int i = 0; i < src_w; i++) {
                if (i > 0) fprintf(out, " + ");
                if (i == 0) {
                    /* Bit 0: just mask with 1'b1 */
                    fprintf(out, "(");
                    emit_expr_internal(out, mod, src, 0);
                    fprintf(out, " & 1'b1)");
                } else {
                    /* Bit i: shift right then mask */
                    fprintf(out, "((");
                    emit_expr_internal(out, mod, src, 0);
                    fprintf(out, " >> %d) & 1'b1)", i);
                }
            }
            fprintf(out, ")");
            break;
        }

        /* ---- Binary to one-hot ---- */
        case EXPR_INTRINSIC_B2OH: {
            const IR_Expr *idx = expr->u.intrinsic.source;
            int result_w = expr->width;
            if (!idx || result_w <= 0) { fprintf(out, "1'b0"); break; }
            fprintf(out, "((");
            emit_expr_internal(out, mod, idx, 0);
            fprintf(out, " < %d'd%d) ? (%d'b1 << ", result_w, result_w, result_w);
            emit_expr_internal(out, mod, idx, 0);
            fprintf(out, ") : %d'b0)", result_w);
            break;
        }

        /* ---- Priority encoder (MSB-first) ---- */
        case EXPR_INTRINSIC_PRIENC: {
            const IR_Expr *src = expr->u.intrinsic.source;
            int src_w = src ? src->width : 0;
            int result_w = expr->width;
            if (!src || src_w <= 0 || result_w <= 0) { fprintf(out, "1'b0"); break; }
            /* Use shift-and-mask instead of bit-select [i] because Verilog
             * does not allow bit-select on concatenation expressions. */
            for (int i = src_w - 1; i >= 1; i--) {
                fprintf(out, "((");
                emit_expr_internal(out, mod, src, expr_precedence(EXPR_BINARY_SHR));
                fprintf(out, " >> %d) & 1'b1) ? %d'd%d : ", i, result_w, i);
            }
            fprintf(out, "%d'd0", result_w);
            break;
        }

        /* ---- Leading zero count ---- */
        case EXPR_INTRINSIC_LZC: {
            const IR_Expr *src = expr->u.intrinsic.source;
            int src_w = src ? src->width : 0;
            int result_w = expr->width;
            if (!src || src_w <= 0 || result_w <= 0) { fprintf(out, "1'b0"); break; }
            /* Use shift-and-mask instead of bit-select [i] because Verilog
             * does not allow bit-select on concatenation expressions. */
            for (int i = src_w - 1; i >= 0; i--) {
                fprintf(out, "((");
                emit_expr_internal(out, mod, src, expr_precedence(EXPR_BINARY_SHR));
                fprintf(out, " >> %d) & 1'b1) ? %d'd%d : ", i, result_w, src_w - 1 - i);
            }
            fprintf(out, "%d'd%d", result_w, src_w);
            break;
        }

        default:
            fprintf(out, "1'b0");
            break;
    }

    if (need_parens) {
        fputc(')', out);
    }
}

/* -------------------------------------------------------------------------
 * Main expression emitter
 * -------------------------------------------------------------------------
 */

void emit_expr_internal(FILE *out,
                        const IR_Module *mod,
                        const IR_Expr *expr,
                        int parent_prec)
{
    if (!expr) {
        fprintf(out, "1'b0");
        return;
    }

    if (is_intrinsic_expr(expr->kind)) {
        emit_intrinsic_expr(out, mod, expr, parent_prec);
        return;
    }

    int my_prec = expr_precedence(expr->kind);
    bool need_parens = my_prec < parent_prec;

    if (need_parens) {
        fputc('(', out);
    }

    switch (expr->kind) {
        case EXPR_LITERAL:
            /* Check for GND/VCC polymorphic special drivers */
            if (expr->const_name &&
                (strcmp(expr->const_name, "GND") == 0 ||
                 strcmp(expr->const_name, "VCC") == 0)) {
                int width = (expr->width > 0) ? expr->width : 1;
                char bit = (strcmp(expr->const_name, "VCC") == 0) ? '1' : '0';
                fprintf(out, "{%d{1'b%c}}", width, bit);
            } else {
                emit_literal(out, &expr->u.literal.literal);
            }
            break;

        case EXPR_SIGNAL_REF: {
            const IR_Signal *sig = find_signal_by_id(mod, expr->u.signal_ref.signal_id);
            if (sig && sig->name) {
                char esc[256];
                fprintf(out, "%s", verilog_safe_name(sig->name, esc, (int)sizeof(esc)));
            } else {
                fprintf(out, "1'b0");
            }
            break;
        }

        case EXPR_UNARY_NOT:
            fprintf(out, "~");
            emit_expr_internal(out, mod, expr->u.unary.operand, my_prec);
            break;
        case EXPR_UNARY_NEG:
            fprintf(out, "-");
            emit_expr_internal(out, mod, expr->u.unary.operand, my_prec);
            break;
        case EXPR_LOGICAL_NOT:
            fprintf(out, "!");
            emit_expr_internal(out, mod, expr->u.unary.operand, my_prec);
            break;

        case EXPR_BINARY_ADD:
        case EXPR_BINARY_SUB:
        case EXPR_BINARY_MUL:
        case EXPR_BINARY_DIV:
        case EXPR_BINARY_MOD:
        case EXPR_BINARY_AND:
        case EXPR_BINARY_OR:
        case EXPR_BINARY_XOR:
        case EXPR_BINARY_SHL:
        case EXPR_BINARY_SHR:
        case EXPR_BINARY_ASHR:
        case EXPR_BINARY_EQ:
        case EXPR_BINARY_NEQ:
        case EXPR_BINARY_LT:
        case EXPR_BINARY_GT:
        case EXPR_BINARY_LTE:
        case EXPR_BINARY_GTE:
        case EXPR_LOGICAL_AND:
        case EXPR_LOGICAL_OR: {
            const char *op = "";
            switch (expr->kind) {
                case EXPR_BINARY_ADD:  op = "+";  break;
                case EXPR_BINARY_SUB:  op = "-";  break;
                case EXPR_BINARY_MUL:  op = "*";  break;
                case EXPR_BINARY_DIV:  op = "/";  break;
                case EXPR_BINARY_MOD:  op = "%";  break;
                case EXPR_BINARY_AND:  op = "&";  break;
                case EXPR_BINARY_OR:   op = "|";  break;
                case EXPR_BINARY_XOR:  op = "^";  break;
                case EXPR_BINARY_SHL:  op = "<<"; break;
                case EXPR_BINARY_SHR:  op = ">>"; break;
                case EXPR_BINARY_ASHR: op = ">>>";break;
                case EXPR_BINARY_EQ:   op = "=="; break;
                case EXPR_BINARY_NEQ:  op = "!="; break;
                case EXPR_BINARY_LT:   op = "<";  break;
                case EXPR_BINARY_GT:   op = ">";  break;
                case EXPR_BINARY_LTE:  op = "<="; break;
                case EXPR_BINARY_GTE:  op = ">="; break;
                case EXPR_LOGICAL_AND: op = "&&"; break;
                case EXPR_LOGICAL_OR:  op = "||"; break;
                default:               op = "?";  break;
            }

            emit_expr_internal(out, mod, expr->u.binary.left, my_prec);
            fprintf(out, " %s ", op);
            emit_expr_internal(out, mod, expr->u.binary.right, my_prec);
            break;
        }

        case EXPR_TERNARY:
            emit_expr_internal(out, mod, expr->u.ternary.condition, my_prec);
            fprintf(out, " ? ");
            emit_expr_internal(out, mod, expr->u.ternary.true_val, my_prec);
            fprintf(out, " : ");
            emit_expr_internal(out, mod, expr->u.ternary.false_val, my_prec);
            break;

        case EXPR_CONCAT: {
            fprintf(out, "{");
            for (int i = 0; i < expr->u.concat.num_operands; ++i) {
                if (i > 0) {
                    fprintf(out, ", ");
                }
                emit_expr_internal(out, mod, expr->u.concat.operands[i], 0);
            }
            fprintf(out, "}");
            break;
        }

        case EXPR_SLICE: {
            if (expr->u.slice.base_expr) {
                /* Expression slice: (expr)[msb:lsb] - emit as shift-and-mask
                 * since Verilog doesn't support (expr)[msb:lsb] syntax.
                 * Result: (((expr) >> lsb) & {width{1'b1}})
                 * Outer parens required: Verilog == binds tighter than &,
                 * and * binds tighter than &, so without wrapping,
                 * "X & 1'b1 == 1'b0" parses as "X & (1'b1 == 1'b0)" = 0.
                 */
                int width = expr->u.slice.msb - expr->u.slice.lsb + 1;
                fprintf(out, "(((");
                emit_expr_internal(out, mod, expr->u.slice.base_expr, 0);
                fprintf(out, ") >> %d)", expr->u.slice.lsb);
                if (width > 1) {
                    fprintf(out, " & {%d{1'b1}})", width);
                } else {
                    fprintf(out, " & 1'b1)");
                }
            } else {
                /* Signal slice: signal[msb:lsb] */
                const IR_Signal *sig = find_signal_by_id(mod, expr->u.slice.signal_id);
                char esc_sl[256];
                const char *name = (sig && sig->name)
                    ? verilog_safe_name(sig->name, esc_sl, (int)sizeof(esc_sl))
                    : "/*slice*/1'b0";
                if (expr->u.slice.msb == expr->u.slice.lsb) {
                    fprintf(out, "%s[%d]", name, expr->u.slice.msb);
                } else {
                    fprintf(out, "%s[%d:%d]", name, expr->u.slice.msb, expr->u.slice.lsb);
                }
            }
            break;
        }

        case EXPR_MEM_READ: {
            const char *mem_name = expr->u.mem_read.memory_name;
            const char *port_name = expr->u.mem_read.port_name;
            if (!mem_name || !port_name || !mod) {
                fprintf(out, "1'b0");
                break;
            }
            const IR_Memory *mem = NULL;
            const IR_MemoryPort *port = NULL;
            for (int i = 0; i < mod->num_memories && !mem; ++i) {
                const IR_Memory *m = &mod->memories[i];
                if (!m->name || strcmp(m->name, mem_name) != 0) {
                    continue;
                }
                for (int p = 0; p < m->num_ports; ++p) {
                    const IR_MemoryPort *mp = &m->ports[p];
                    if (mp->name && strcmp(mp->name, port_name) == 0) {
                        mem = m;
                        port = mp;
                        break;
                    }
                }
            }
            if (!mem) {
                fprintf(out, "1'b0");
                break;
            }
            /* Compute the safe Verilog memory name (may differ from IR name
             * when it collides with the module name).
             */
            const char *raw_vn = (mem->name && mem->name[0] != '\0') ? mem->name : "jz_mem";
            char vn_safe_buf[256];
            const char *vname = verilog_memory_name(raw_vn, mod->name, vn_safe_buf, sizeof(vn_safe_buf));
            /* BSRAM intermediate substitution: when emitting the main block
             * and this memory has a BSRAM read intermediate, emit the
             * intermediate signal name instead of the direct memory read.
             */
            if (g_skip_block_mem_accesses &&
                mem->kind == MEM_KIND_BLOCK &&
                has_bsram_read_intermediate(vname)) {
                fprintf(out, "%s_bsram_out", vname);
                break;
            }
            fprintf(out, "%s[", vname);
            if (expr->u.mem_read.address) {
                emit_expr_internal(out, mod, expr->u.mem_read.address, my_prec);
                fputc(']', out);
            } else {
                if (port && port->kind == MEM_PORT_READ_SYNC &&
                    port->addr_reg_signal_id >= 0) {
                    /* SYNC read with synthetic address register signal. */
                    const IR_Signal *addr_reg = find_signal_by_id(mod, port->addr_reg_signal_id);
                    if (addr_reg && addr_reg->name) {
                        fprintf(out, "%s]", addr_reg->name);
                    } else {
                        const char *pname = (port->name && port->name[0] != '\0')
                                          ? port->name : "rd";
                        fprintf(out, "%s_%s_addr]", vname, pname);
                    }
                } else if (port && port->kind == MEM_PORT_READ_SYNC &&
                    port->addr_signal_id >= 0) {
                    /* SYNC read: use the implicit registered address. */
                    const char *pname = (port->name && port->name[0] != '\0')
                                      ? port->name : "rd";
                    fprintf(out, "%s_%s_addr]", vname, pname);
                } else {
                    const IR_Signal *addr_sig = NULL;
                    if (port && port->addr_signal_id >= 0) {
                        addr_sig = find_signal_by_id(mod, port->addr_signal_id);
                    }
                    if (addr_sig && addr_sig->name) {
                        char esc_a[256];
                        fputs(verilog_safe_name(addr_sig->name, esc_a, (int)sizeof(esc_a)), out);
                        fputc(']', out);
                    } else {
                        fputs("1'b0]", out);
                    }
                }
            }
            break;
        }

        default:
            fprintf(out, "1'b0");
            break;
    }

    if (need_parens) {
        fputc(')', out);
    }
}

void emit_expr(FILE *out, const IR_Module *mod, const IR_Expr *expr)
{
    emit_expr_internal(out, mod, expr, 0);
}
