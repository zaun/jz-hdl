/**
 * @file sim_eval.c
 * @brief Recursive IR expression evaluator for cycle-accurate simulation.
 */

#include "sim_eval.h"
#include <string.h>
#include <stdio.h>

SimValue sim_eval_expr(SimContext *ctx, const IR_Expr *expr) {
    if (!expr) return sim_val_all_x(1);

    switch (expr->kind) {

    case EXPR_LITERAL: {
        SimValue v = sim_val_from_words(expr->u.literal.literal.words,
                                         IR_LIT_WORDS,
                                         expr->u.literal.literal.width);
        if (expr->u.literal.literal.is_z)
            v = sim_val_all_z(expr->u.literal.literal.width);
        if (v.width == 0) v.width = expr->width > 0 ? expr->width : 1;
        return v;
    }

    case EXPR_SIGNAL_REF: {
        SimSignalEntry *e = sim_ctx_lookup(ctx, expr->u.signal_ref.signal_id);
        if (!e) return sim_val_all_x(expr->width > 0 ? expr->width : 1);
        return e->current;
    }

    /* ---- Unary ---- */
    case EXPR_UNARY_NOT: {
        SimValue op = sim_eval_expr(ctx, expr->u.unary.operand);
        SimValue r = sim_val_not(op);
        r.width = expr->width > 0 ? expr->width : r.width;
        return r;
    }
    case EXPR_UNARY_NEG: {
        SimValue op = sim_eval_expr(ctx, expr->u.unary.operand);
        SimValue r = sim_val_neg(op);
        r.width = expr->width > 0 ? expr->width : r.width;
        return r;
    }
    case EXPR_LOGICAL_NOT: {
        SimValue op = sim_eval_expr(ctx, expr->u.unary.operand);
        return sim_val_logical_not(op);
    }

    /* ---- Binary arithmetic ---- */
    case EXPR_BINARY_ADD: {
        SimValue l = sim_eval_expr(ctx, expr->u.binary.left);
        SimValue r = sim_eval_expr(ctx, expr->u.binary.right);
        SimValue res = sim_val_add(l, r);
        res.width = expr->width > 0 ? expr->width : res.width;
        return sim_val_mask(res);
    }
    case EXPR_BINARY_SUB: {
        SimValue l = sim_eval_expr(ctx, expr->u.binary.left);
        SimValue r = sim_eval_expr(ctx, expr->u.binary.right);
        SimValue res = sim_val_sub(l, r);
        res.width = expr->width > 0 ? expr->width : res.width;
        return sim_val_mask(res);
    }
    case EXPR_BINARY_MUL: {
        SimValue l = sim_eval_expr(ctx, expr->u.binary.left);
        SimValue r = sim_eval_expr(ctx, expr->u.binary.right);
        SimValue res = sim_val_mul(l, r);
        res.width = expr->width > 0 ? expr->width : res.width;
        return sim_val_mask(res);
    }
    case EXPR_BINARY_DIV: {
        SimValue l = sim_eval_expr(ctx, expr->u.binary.left);
        SimValue r = sim_eval_expr(ctx, expr->u.binary.right);
        SimValue res = sim_val_div(l, r);
        res.width = expr->width > 0 ? expr->width : res.width;
        return sim_val_mask(res);
    }
    case EXPR_BINARY_MOD: {
        SimValue l = sim_eval_expr(ctx, expr->u.binary.left);
        SimValue r = sim_eval_expr(ctx, expr->u.binary.right);
        SimValue res = sim_val_mod(l, r);
        res.width = expr->width > 0 ? expr->width : res.width;
        return sim_val_mask(res);
    }

    /* ---- Bitwise ---- */
    case EXPR_BINARY_AND: {
        SimValue l = sim_eval_expr(ctx, expr->u.binary.left);
        SimValue r = sim_eval_expr(ctx, expr->u.binary.right);
        SimValue res = sim_val_and(l, r);
        res.width = expr->width > 0 ? expr->width : res.width;
        return res;
    }
    case EXPR_BINARY_OR: {
        SimValue l = sim_eval_expr(ctx, expr->u.binary.left);
        SimValue r = sim_eval_expr(ctx, expr->u.binary.right);
        SimValue res = sim_val_or(l, r);
        res.width = expr->width > 0 ? expr->width : res.width;
        return res;
    }
    case EXPR_BINARY_XOR: {
        SimValue l = sim_eval_expr(ctx, expr->u.binary.left);
        SimValue r = sim_eval_expr(ctx, expr->u.binary.right);
        SimValue res = sim_val_xor(l, r);
        res.width = expr->width > 0 ? expr->width : res.width;
        return res;
    }

    /* ---- Shifts ---- */
    case EXPR_BINARY_SHL: {
        SimValue l = sim_eval_expr(ctx, expr->u.binary.left);
        SimValue r = sim_eval_expr(ctx, expr->u.binary.right);
        SimValue res = sim_val_shl(l, r);
        res.width = expr->width > 0 ? expr->width : res.width;
        return sim_val_mask(res);
    }
    case EXPR_BINARY_SHR: {
        SimValue l = sim_eval_expr(ctx, expr->u.binary.left);
        SimValue r = sim_eval_expr(ctx, expr->u.binary.right);
        SimValue res = sim_val_shr(l, r);
        res.width = expr->width > 0 ? expr->width : res.width;
        return sim_val_mask(res);
    }
    case EXPR_BINARY_ASHR: {
        SimValue l = sim_eval_expr(ctx, expr->u.binary.left);
        SimValue r = sim_eval_expr(ctx, expr->u.binary.right);
        SimValue res = sim_val_ashr(l, r);
        res.width = expr->width > 0 ? expr->width : res.width;
        return sim_val_mask(res);
    }

    /* ---- Comparisons ---- */
    case EXPR_BINARY_EQ: {
        SimValue l = sim_eval_expr(ctx, expr->u.binary.left);
        SimValue r = sim_eval_expr(ctx, expr->u.binary.right);
        return sim_val_eq(l, r);
    }
    case EXPR_BINARY_NEQ: {
        SimValue l = sim_eval_expr(ctx, expr->u.binary.left);
        SimValue r = sim_eval_expr(ctx, expr->u.binary.right);
        return sim_val_neq(l, r);
    }
    case EXPR_BINARY_LT: {
        SimValue l = sim_eval_expr(ctx, expr->u.binary.left);
        SimValue r = sim_eval_expr(ctx, expr->u.binary.right);
        return sim_val_lt(l, r);
    }
    case EXPR_BINARY_GT: {
        SimValue l = sim_eval_expr(ctx, expr->u.binary.left);
        SimValue r = sim_eval_expr(ctx, expr->u.binary.right);
        return sim_val_gt(l, r);
    }
    case EXPR_BINARY_LTE: {
        SimValue l = sim_eval_expr(ctx, expr->u.binary.left);
        SimValue r = sim_eval_expr(ctx, expr->u.binary.right);
        return sim_val_lte(l, r);
    }
    case EXPR_BINARY_GTE: {
        SimValue l = sim_eval_expr(ctx, expr->u.binary.left);
        SimValue r = sim_eval_expr(ctx, expr->u.binary.right);
        return sim_val_gte(l, r);
    }

    /* ---- Logical ---- */
    case EXPR_LOGICAL_AND: {
        SimValue l = sim_eval_expr(ctx, expr->u.binary.left);
        SimValue r = sim_eval_expr(ctx, expr->u.binary.right);
        return sim_val_logical_and(l, r);
    }
    case EXPR_LOGICAL_OR: {
        SimValue l = sim_eval_expr(ctx, expr->u.binary.left);
        SimValue r = sim_eval_expr(ctx, expr->u.binary.right);
        return sim_val_logical_or(l, r);
    }

    /* ---- Ternary ---- */
    case EXPR_TERNARY: {
        SimValue cond = sim_eval_expr(ctx, expr->u.ternary.condition);
        SimValue tv = sim_eval_expr(ctx, expr->u.ternary.true_val);
        SimValue fv = sim_eval_expr(ctx, expr->u.ternary.false_val);
        /* z in ternary condition is a runtime error (SE-008).
         * x is not a runtime value; z in a non-tristate expression
         * means z leaked past compile-time structural checks. */
        if (sim_val_has_xz(cond) && !ctx->runtime_error) {
            ctx->runtime_error = 1;
            fprintf(stderr, "RUNTIME ERROR: z reached ternary condition "
                    "(SE-008)\n");
        }
        return sim_val_ternary(cond, tv, fv);
    }

    /* ---- Concat ---- */
    case EXPR_CONCAT: {
        int n = expr->u.concat.num_operands;
        SimValue parts[64];
        if (n > 64) n = 64;
        for (int i = 0; i < n; i++)
            parts[i] = sim_eval_expr(ctx, expr->u.concat.operands[i]);
        return sim_val_concat(parts, n);
    }

    /* ---- Slice ---- */
    case EXPR_SLICE: {
        SimValue base;
        if (expr->u.slice.base_expr) {
            base = sim_eval_expr(ctx, expr->u.slice.base_expr);
        } else {
            SimSignalEntry *e = sim_ctx_lookup(ctx, expr->u.slice.signal_id);
            if (!e) return sim_val_all_x(expr->width > 0 ? expr->width : 1);
            base = e->current;
        }
        return sim_val_slice(base, expr->u.slice.msb, expr->u.slice.lsb);
    }

    /* ---- Memory read ---- */
    case EXPR_MEM_READ: {
        SimMemEntry *me = sim_ctx_lookup_mem(ctx, expr->u.mem_read.memory_name);
        if (!me) return sim_val_all_x(expr->width > 0 ? expr->width : 1);
        SimValue addr = sim_eval_expr(ctx, expr->u.mem_read.address);
        if (sim_val_has_xz(addr)) return sim_val_all_x(me->word_width);
        uint64_t idx = addr.val[0];
        if (idx >= (uint64_t)me->depth) return sim_val_all_x(me->word_width);
        return me->cells[idx];
    }

    /* ---- Intrinsics ---- */
    case EXPR_INTRINSIC_UADD: {
        SimValue src = sim_eval_expr(ctx, expr->u.intrinsic.source);
        SimValue idx = sim_eval_expr(ctx, expr->u.intrinsic.index);
        SimValue res = sim_val_add(src, idx);
        res.width = expr->width > 0 ? expr->width : res.width;
        return sim_val_mask(res);
    }
    case EXPR_INTRINSIC_SADD: {
        SimValue src = sim_eval_expr(ctx, expr->u.intrinsic.source);
        SimValue idx = sim_eval_expr(ctx, expr->u.intrinsic.index);
        SimValue res = sim_val_add(src, idx);
        res.width = expr->width > 0 ? expr->width : res.width;
        return sim_val_mask(res);
    }
    case EXPR_INTRINSIC_UMUL: {
        SimValue src = sim_eval_expr(ctx, expr->u.intrinsic.source);
        SimValue idx = sim_eval_expr(ctx, expr->u.intrinsic.index);
        SimValue res = sim_val_mul(src, idx);
        res.width = expr->width > 0 ? expr->width : res.width;
        return sim_val_mask(res);
    }
    case EXPR_INTRINSIC_SMUL: {
        SimValue src = sim_eval_expr(ctx, expr->u.intrinsic.source);
        SimValue idx = sim_eval_expr(ctx, expr->u.intrinsic.index);
        SimValue res = sim_val_mul(src, idx);
        res.width = expr->width > 0 ? expr->width : res.width;
        return sim_val_mask(res);
    }
    case EXPR_INTRINSIC_GBIT: {
        SimValue src = sim_eval_expr(ctx, expr->u.intrinsic.source);
        SimValue idx = sim_eval_expr(ctx, expr->u.intrinsic.index);
        if (sim_val_has_xz(idx)) return sim_val_all_x(1);
        int bit = (int)(idx.val[0]);
        if (bit < 0 || bit >= src.width) return sim_val_zero(1);
        return sim_val_slice(src, bit, bit);
    }
    case EXPR_INTRINSIC_SBIT: {
        SimValue src = sim_eval_expr(ctx, expr->u.intrinsic.source);
        SimValue idx = sim_eval_expr(ctx, expr->u.intrinsic.index);
        SimValue val = sim_eval_expr(ctx, expr->u.intrinsic.value);
        if (sim_val_has_xz(idx)) return sim_val_all_x(src.width);
        int bit = (int)(idx.val[0]);
        if (bit < 0 || bit >= src.width) return src;
        int wi = bit / 64;
        int bi = bit % 64;
        uint64_t mask = (uint64_t)1 << bi;
        src.val[wi] = (src.val[wi] & ~mask) | ((val.val[0] & 1) << bi);
        src.xmask[wi] = (src.xmask[wi] & ~mask) | ((val.xmask[0] & 1) << bi);
        src.zmask[wi] = (src.zmask[wi] & ~mask) | ((val.zmask[0] & 1) << bi);
        return src;
    }
    case EXPR_INTRINSIC_GSLICE: {
        SimValue src = sim_eval_expr(ctx, expr->u.intrinsic.source);
        SimValue idx = sim_eval_expr(ctx, expr->u.intrinsic.index);
        int ew = expr->u.intrinsic.element_width;
        if (ew <= 0) ew = expr->width > 0 ? expr->width : 1;
        if (sim_val_has_xz(idx))
            return sim_val_all_x(ew);
        /* Index is an element index; multiply by element width for bit offset */
        int lo = (int)(idx.val[0]) * ew;
        int hi = lo + ew - 1;
        if (lo < 0 || hi >= src.width)
            return sim_val_all_x(ew);
        return sim_val_slice(src, hi, lo);
    }
    case EXPR_INTRINSIC_SSLICE: {
        SimValue src = sim_eval_expr(ctx, expr->u.intrinsic.source);
        SimValue idx = sim_eval_expr(ctx, expr->u.intrinsic.index);
        SimValue val = sim_eval_expr(ctx, expr->u.intrinsic.value);
        if (sim_val_has_xz(idx)) return sim_val_all_x(src.width);
        int lo = (int)(idx.val[0]);
        int ew = expr->u.intrinsic.element_width;
        int hi = lo + ew - 1;
        if (lo < 0 || hi >= src.width) return src;
        /* Use slice-based approach for multi-word support */
        SimValue slice_val = sim_val_slice(val, ew - 1, 0);
        /* Build result by shifting slice into position */
        SimValue shift_amt = sim_val_from_uint((uint64_t)lo, 32);
        SimValue shifted_val = sim_val_shl(sim_val_zext(slice_val, src.width), shift_amt);
        /* Build position mask */
        SimValue ones_ew = sim_val_ones(ew);
        SimValue shifted_mask = sim_val_shl(sim_val_zext(ones_ew, src.width), shift_amt);
        SimValue inv_mask = sim_val_not(shifted_mask);
        SimValue cleared = sim_val_and(src, inv_mask);
        return sim_val_or(cleared, shifted_val);
    }

    case EXPR_INTRINSIC_OH2B: {
        SimValue src = sim_eval_expr(ctx, expr->u.intrinsic.source);
        if (sim_val_has_xz(src)) return sim_val_all_x(expr->width > 0 ? expr->width : 1);
        int result_w = expr->width > 0 ? expr->width : 1;
        uint64_t result = 0;
        for (int i = 0; i < src.width; i++) {
            if (sim_val_get_bit(src, i)) result |= (uint64_t)i;
        }
        return sim_val_from_uint(result, result_w);
    }

    case EXPR_INTRINSIC_B2OH: {
        SimValue idx = sim_eval_expr(ctx, expr->u.intrinsic.source);
        if (sim_val_has_xz(idx)) return sim_val_all_x(expr->width > 0 ? expr->width : 1);
        int result_w = expr->width > 0 ? expr->width : 1;
        if (idx.val[0] < (uint64_t)result_w) {
            SimValue r = sim_val_zero(result_w);
            sim_val_set_bit(&r, (int)idx.val[0], 1);
            return r;
        }
        return sim_val_zero(result_w);
    }

    case EXPR_INTRINSIC_PRIENC: {
        SimValue src = sim_eval_expr(ctx, expr->u.intrinsic.source);
        if (sim_val_has_xz(src)) return sim_val_all_x(expr->width > 0 ? expr->width : 1);
        int result_w = expr->width > 0 ? expr->width : 1;
        uint64_t result = 0;
        for (int i = src.width - 1; i >= 0; i--) {
            if (sim_val_get_bit(src, i)) { result = (uint64_t)i; break; }
        }
        return sim_val_from_uint(result, result_w);
    }

    case EXPR_INTRINSIC_LZC: {
        SimValue src = sim_eval_expr(ctx, expr->u.intrinsic.source);
        if (sim_val_has_xz(src)) return sim_val_all_x(expr->width > 0 ? expr->width : 1);
        int result_w = expr->width > 0 ? expr->width : 1;
        uint64_t count = (uint64_t)src.width;
        for (int i = src.width - 1; i >= 0; i--) {
            if (sim_val_get_bit(src, i)) { count = (uint64_t)(src.width - 1 - i); break; }
        }
        return sim_val_from_uint(count, result_w);
    }

    case EXPR_INTRINSIC_USUB:
    case EXPR_INTRINSIC_SSUB: {
        SimValue a = sim_eval_expr(ctx, expr->u.binary.left);
        SimValue b = sim_eval_expr(ctx, expr->u.binary.right);
        SimValue res = sim_val_sub(a, b);
        res.width = expr->width > 0 ? expr->width : res.width;
        return sim_val_mask(res);
    }

    case EXPR_INTRINSIC_ABS: {
        SimValue src = sim_eval_expr(ctx, expr->u.intrinsic.source);
        if (sim_val_has_xz(src)) return sim_val_all_x(expr->width > 0 ? expr->width : 1);
        int result_w = expr->width > 0 ? expr->width : 1;
        int src_w = src.width > 0 ? src.width : 1;
        /* Check sign bit */
        int is_neg = sim_val_get_bit(src, src_w - 1);
        uint64_t magnitude;
        uint64_t overflow = 0;
        if (is_neg) {
            /* Two's complement negate in src_w+1 bits */
            uint64_t extended = src.val[0];
            uint64_t mask_full = (result_w >= 64) ? ~0ULL : ((1ULL << result_w) - 1);
            magnitude = ((~extended) + 1) & mask_full;
            /* Overflow when input is most-negative (e.g., 0x80 for 8-bit) */
            uint64_t most_neg = (uint64_t)1 << (src_w - 1);
            if (src.val[0] == most_neg) overflow = 1;
        } else {
            magnitude = src.val[0];
        }
        uint64_t result = (overflow << (result_w - 1)) | magnitude;
        return sim_val_from_uint(result, result_w);
    }

    case EXPR_INTRINSIC_UMIN:
    case EXPR_INTRINSIC_UMAX: {
        SimValue a = sim_eval_expr(ctx, expr->u.binary.left);
        SimValue b = sim_eval_expr(ctx, expr->u.binary.right);
        if (sim_val_has_xz(a) || sim_val_has_xz(b))
            return sim_val_all_x(expr->width > 0 ? expr->width : 1);
        int result_w = expr->width > 0 ? expr->width : 1;
        bool pick_a = (expr->kind == EXPR_INTRINSIC_UMIN) ? (a.val[0] < b.val[0]) : (a.val[0] > b.val[0]);
        return sim_val_from_uint(pick_a ? a.val[0] : b.val[0], result_w);
    }

    case EXPR_INTRINSIC_SMIN:
    case EXPR_INTRINSIC_SMAX: {
        SimValue a = sim_eval_expr(ctx, expr->u.binary.left);
        SimValue b = sim_eval_expr(ctx, expr->u.binary.right);
        if (sim_val_has_xz(a) || sim_val_has_xz(b))
            return sim_val_all_x(expr->width > 0 ? expr->width : 1);
        int result_w = expr->width > 0 ? expr->width : 1;
        /* Sign-extend to 64 bits for comparison */
        int wa = a.width > 0 ? a.width : 1;
        int wb = b.width > 0 ? b.width : 1;
        int64_t sa = (int64_t)a.val[0];
        if (wa < 64 && ((a.val[0] >> (wa - 1)) & 1)) sa |= ~((int64_t)((1ULL << wa) - 1));
        int64_t sb = (int64_t)b.val[0];
        if (wb < 64 && ((b.val[0] >> (wb - 1)) & 1)) sb |= ~((int64_t)((1ULL << wb) - 1));
        bool pick_a = (expr->kind == EXPR_INTRINSIC_SMIN) ? (sa < sb) : (sa > sb);
        return sim_val_from_uint(pick_a ? a.val[0] : b.val[0], result_w);
    }

    case EXPR_INTRINSIC_POPCOUNT: {
        SimValue src = sim_eval_expr(ctx, expr->u.intrinsic.source);
        if (sim_val_has_xz(src)) return sim_val_all_x(expr->width > 0 ? expr->width : 1);
        int result_w = expr->width > 0 ? expr->width : 1;
        uint64_t count = 0;
        for (int i = 0; i < src.width; i++) {
            if (sim_val_get_bit(src, i)) count++;
        }
        return sim_val_from_uint(count, result_w);
    }

    case EXPR_INTRINSIC_REVERSE: {
        SimValue src = sim_eval_expr(ctx, expr->u.intrinsic.source);
        if (sim_val_has_xz(src)) return sim_val_all_x(expr->width > 0 ? expr->width : 1);
        int w = src.width > 0 ? src.width : 1;
        SimValue result = sim_val_zero(w);
        for (int i = 0; i < w; i++) {
            if (sim_val_get_bit(src, i))
                sim_val_set_bit(&result, w - 1 - i, 1);
        }
        return result;
    }

    case EXPR_INTRINSIC_BSWAP: {
        SimValue src = sim_eval_expr(ctx, expr->u.intrinsic.source);
        if (sim_val_has_xz(src)) return sim_val_all_x(expr->width > 0 ? expr->width : 1);
        int w = src.width > 0 ? src.width : 8;
        int num_bytes = w / 8;
        SimValue result = sim_val_zero(w);
        for (int i = 0; i < num_bytes; i++) {
            for (int b = 0; b < 8; b++) {
                if (sim_val_get_bit(src, i * 8 + b))
                    sim_val_set_bit(&result, (num_bytes - 1 - i) * 8 + b, 1);
            }
        }
        return result;
    }

    case EXPR_INTRINSIC_REDUCE_AND: {
        SimValue src = sim_eval_expr(ctx, expr->u.intrinsic.source);
        if (sim_val_has_xz(src)) return sim_val_all_x(1);
        /* Check if all bits are 1 */
        SimValue ones = sim_val_ones(src.width);
        return sim_val_eq(src, ones);
    }

    case EXPR_INTRINSIC_REDUCE_OR: {
        SimValue src = sim_eval_expr(ctx, expr->u.intrinsic.source);
        if (sim_val_has_xz(src)) return sim_val_all_x(1);
        SimValue zero = sim_val_zero(src.width);
        return sim_val_neq(src, zero);
    }

    case EXPR_INTRINSIC_REDUCE_XOR: {
        SimValue src = sim_eval_expr(ctx, expr->u.intrinsic.source);
        if (sim_val_has_xz(src)) return sim_val_all_x(1);
        int w = src.width > 0 ? src.width : 1;
        uint64_t result = 0;
        for (int i = 0; i < w; i++) {
            result ^= (uint64_t)sim_val_get_bit(src, i);
        }
        return sim_val_from_uint(result, 1);
    }

    } /* end switch */

    /* Unreachable for known expression kinds */
    return sim_val_all_x(expr->width > 0 ? expr->width : 1);
}
