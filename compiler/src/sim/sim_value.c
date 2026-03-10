/**
 * @file sim_value.c
 * @brief 4-state bit-vector arithmetic, logic, comparison, and formatting.
 *
 * Multi-word support: values up to SIM_VAL_WORDS * 64 bits (256 bits).
 * Word 0 is the least-significant 64 bits.
 */

#include "sim_value.h"
#include <stdio.h>
#include <string.h>

/* ---- helpers ---- */

/** Return the number of words needed for w bits (1..SIM_VAL_WORDS). */
static int num_words(int w) {
    if (w <= 0) return 1;
    return (w + 63) / 64;
}

/** Return a mask for the top word of a w-bit value. */
static uint64_t top_word_mask(int w) {
    if (w <= 0) return 0;
    int bits_in_top = ((w - 1) % 64) + 1;
    if (bits_in_top == 64) return UINT64_MAX;
    return ((uint64_t)1 << bits_in_top) - 1;
}

/** Return a full 64-bit mask for word index wi of a w-bit value. */
static uint64_t word_mask(int w, int wi) {
    int nw = num_words(w);
    if (wi < 0 || wi >= nw) return 0;
    if (wi < nw - 1) return UINT64_MAX;
    return top_word_mask(w);
}

/* ---- construction ---- */

SimValue sim_val_zero(int width) {
    SimValue v;
    memset(&v, 0, sizeof(v));
    v.width = width;
    return v;
}

SimValue sim_val_ones(int width) {
    SimValue v;
    memset(&v, 0, sizeof(v));
    v.width = width;
    int nw = num_words(width);
    for (int i = 0; i < nw - 1; i++)
        v.val[i] = UINT64_MAX;
    if (nw > 0)
        v.val[nw - 1] = top_word_mask(width);
    return v;
}

SimValue sim_val_from_uint(uint64_t val, int width) {
    SimValue v;
    memset(&v, 0, sizeof(v));
    v.width = width;
    if (width > 0 && width <= 64) {
        uint64_t m = (width == 64) ? UINT64_MAX : ((uint64_t)1 << width) - 1;
        v.val[0] = val & m;
    } else {
        v.val[0] = val;
    }
    return v;
}

SimValue sim_val_from_words(const uint64_t *words, int nw, int width) {
    SimValue v;
    memset(&v, 0, sizeof(v));
    v.width = width;
    int copy = nw < SIM_VAL_WORDS ? nw : SIM_VAL_WORDS;
    for (int i = 0; i < copy; i++)
        v.val[i] = words[i];
    return sim_val_mask(v);
}

SimValue sim_val_all_x(int width) {
    SimValue v;
    memset(&v, 0, sizeof(v));
    v.width = width;
    int nw = num_words(width);
    for (int i = 0; i < nw - 1; i++)
        v.xmask[i] = UINT64_MAX;
    if (nw > 0)
        v.xmask[nw - 1] = top_word_mask(width);
    return v;
}

SimValue sim_val_all_z(int width) {
    SimValue v;
    memset(&v, 0, sizeof(v));
    v.width = width;
    int nw = num_words(width);
    for (int i = 0; i < nw - 1; i++)
        v.zmask[i] = UINT64_MAX;
    if (nw > 0)
        v.zmask[nw - 1] = top_word_mask(width);
    return v;
}

SimValue sim_val_mask(SimValue v) {
    int nw = num_words(v.width);
    if (nw > 0) {
        uint64_t m = top_word_mask(v.width);
        v.val[nw - 1]   &= m;
        v.xmask[nw - 1] &= m;
        v.zmask[nw - 1] &= m;
    }
    /* Clear words above the used ones */
    for (int i = nw; i < SIM_VAL_WORDS; i++) {
        v.val[i] = 0;
        v.xmask[i] = 0;
        v.zmask[i] = 0;
    }
    return v;
}

/* ---- per-bit access ---- */

int sim_val_get_bit(SimValue v, int bit) {
    if (bit < 0 || bit >= v.width) return 0;
    int wi = bit / 64;
    int bi = bit % 64;
    return (int)((v.val[wi] >> bi) & 1);
}

void sim_val_set_bit(SimValue *v, int bit, int b) {
    if (bit < 0 || bit >= v->width) return;
    int wi = bit / 64;
    int bi = bit % 64;
    uint64_t mask = (uint64_t)1 << bi;
    if (b)
        v->val[wi] |= mask;
    else
        v->val[wi] &= ~mask;
}

/* ---- query ---- */

int sim_val_has_xz(SimValue v) {
    int nw = num_words(v.width);
    for (int i = 0; i < nw; i++) {
        uint64_t m = word_mask(v.width, i);
        if (((v.xmask[i] | v.zmask[i]) & m) != 0) return 1;
    }
    return 0;
}

int sim_val_is_all_z(SimValue v) {
    int nw = num_words(v.width);
    for (int i = 0; i < nw; i++) {
        uint64_t m = word_mask(v.width, i);
        if ((v.zmask[i] & m) != m) return 0;
        if ((v.xmask[i] & m) != 0) return 0;
    }
    return 1;
}

int sim_val_is_true(SimValue v) {
    int nw = num_words(v.width);
    int has_xz = 0;
    int has_val = 0;
    for (int i = 0; i < nw; i++) {
        uint64_t m = word_mask(v.width, i);
        if ((v.xmask[i] | v.zmask[i]) & m) has_xz = 1;
        if (v.val[i] & m) has_val = 1;
    }
    if (has_xz) return -1;
    return has_val ? 1 : 0;
}

int sim_val_equal(SimValue a, SimValue b) {
    if (a.width != b.width) return 0;
    int nw = num_words(a.width);
    for (int i = 0; i < nw; i++) {
        uint64_t m = word_mask(a.width, i);
        if ((a.val[i] & m) != (b.val[i] & m)) return 0;
        if ((a.xmask[i] & m) != (b.xmask[i] & m)) return 0;
        if ((a.zmask[i] & m) != (b.zmask[i] & m)) return 0;
    }
    return 1;
}

/* ---- arithmetic (word[0] only, widen later if needed) ---- */

static SimValue arith_xz_check(SimValue a, SimValue b, int rw) {
    if (sim_val_has_xz(a) || sim_val_has_xz(b))
        return sim_val_all_x(rw);
    SimValue dummy;
    memset(&dummy, 0, sizeof(dummy));
    dummy.width = -1; /* sentinel */
    return dummy;
}

SimValue sim_val_add(SimValue a, SimValue b) {
    int rw = a.width > b.width ? a.width : b.width;
    SimValue x = arith_xz_check(a, b, rw);
    if (x.width != -1) return x;
    return sim_val_from_uint(a.val[0] + b.val[0], rw);
}

SimValue sim_val_sub(SimValue a, SimValue b) {
    int rw = a.width > b.width ? a.width : b.width;
    SimValue x = arith_xz_check(a, b, rw);
    if (x.width != -1) return x;
    return sim_val_from_uint(a.val[0] - b.val[0], rw);
}

SimValue sim_val_mul(SimValue a, SimValue b) {
    int rw = a.width > b.width ? a.width : b.width;
    SimValue x = arith_xz_check(a, b, rw);
    if (x.width != -1) return x;
    return sim_val_from_uint(a.val[0] * b.val[0], rw);
}

SimValue sim_val_div(SimValue a, SimValue b) {
    int rw = a.width > b.width ? a.width : b.width;
    SimValue x = arith_xz_check(a, b, rw);
    if (x.width != -1) return x;
    if (b.val[0] == 0) return sim_val_all_x(rw);
    return sim_val_from_uint(a.val[0] / b.val[0], rw);
}

SimValue sim_val_mod(SimValue a, SimValue b) {
    int rw = a.width > b.width ? a.width : b.width;
    SimValue x = arith_xz_check(a, b, rw);
    if (x.width != -1) return x;
    if (b.val[0] == 0) return sim_val_all_x(rw);
    return sim_val_from_uint(a.val[0] % b.val[0], rw);
}

SimValue sim_val_neg(SimValue a) {
    if (sim_val_has_xz(a)) return sim_val_all_x(a.width);
    return sim_val_from_uint((~a.val[0] + 1), a.width);
}

/* ---- bitwise (4-state truth tables, multi-word) ---- */

SimValue sim_val_and(SimValue a, SimValue b) {
    int rw = a.width > b.width ? a.width : b.width;
    int nw = num_words(rw);
    SimValue r;
    memset(&r, 0, sizeof(r));
    r.width = rw;

    for (int i = 0; i < nw; i++) {
        uint64_t m = word_mask(rw, i);
        uint64_t a_xz = (a.xmask[i] | a.zmask[i]) & m;
        uint64_t b_xz = (b.xmask[i] | b.zmask[i]) & m;
        uint64_t a_known_zero = (~a.val[i] & ~a_xz) & m;
        uint64_t b_known_zero = (~b.val[i] & ~b_xz) & m;
        uint64_t force_zero = a_known_zero | b_known_zero;
        uint64_t result_val = a.val[i] & b.val[i] & m;
        uint64_t result_x = ((a_xz | b_xz) & ~force_zero) & m;
        r.val[i] = result_val & ~result_x;
        r.xmask[i] = result_x;
    }
    return r;
}

SimValue sim_val_or(SimValue a, SimValue b) {
    int rw = a.width > b.width ? a.width : b.width;
    int nw = num_words(rw);
    SimValue r;
    memset(&r, 0, sizeof(r));
    r.width = rw;

    for (int i = 0; i < nw; i++) {
        uint64_t m = word_mask(rw, i);
        uint64_t a_xz = (a.xmask[i] | a.zmask[i]) & m;
        uint64_t b_xz = (b.xmask[i] | b.zmask[i]) & m;
        uint64_t a_known_one = (a.val[i] & ~a_xz) & m;
        uint64_t b_known_one = (b.val[i] & ~b_xz) & m;
        uint64_t force_one = a_known_one | b_known_one;
        uint64_t result_val = (a.val[i] | b.val[i]) & m;
        uint64_t result_x = ((a_xz | b_xz) & ~force_one) & m;
        r.val[i] = (result_val | force_one) & ~result_x;
        r.xmask[i] = result_x;
    }
    return r;
}

SimValue sim_val_xor(SimValue a, SimValue b) {
    int rw = a.width > b.width ? a.width : b.width;
    int nw = num_words(rw);
    SimValue r;
    memset(&r, 0, sizeof(r));
    r.width = rw;

    for (int i = 0; i < nw; i++) {
        uint64_t m = word_mask(rw, i);
        uint64_t a_xz = (a.xmask[i] | a.zmask[i]) & m;
        uint64_t b_xz = (b.xmask[i] | b.zmask[i]) & m;
        uint64_t any_xz = (a_xz | b_xz) & m;
        r.val[i] = (a.val[i] ^ b.val[i]) & m & ~any_xz;
        r.xmask[i] = any_xz;
    }
    return r;
}

SimValue sim_val_not(SimValue a) {
    int nw = num_words(a.width);
    SimValue r;
    memset(&r, 0, sizeof(r));
    r.width = a.width;

    for (int i = 0; i < nw; i++) {
        uint64_t m = word_mask(a.width, i);
        uint64_t xz = (a.xmask[i] | a.zmask[i]) & m;
        r.val[i] = (~a.val[i] & m) & ~xz;
        r.xmask[i] = xz;
    }
    return r;
}

/* ---- comparison ---- */

static SimValue cmp_result(int result_bit) {
    SimValue r;
    memset(&r, 0, sizeof(r));
    r.width = 1;
    r.val[0] = result_bit ? 1 : 0;
    return r;
}

static SimValue cmp_x(void) {
    return sim_val_all_x(1);
}

SimValue sim_val_eq(SimValue a, SimValue b) {
    if (sim_val_has_xz(a) || sim_val_has_xz(b)) return cmp_x();
    int w = a.width > b.width ? a.width : b.width;
    int nw = num_words(w);
    for (int i = 0; i < nw; i++) {
        uint64_t m = word_mask(w, i);
        if ((a.val[i] & m) != (b.val[i] & m)) return cmp_result(0);
    }
    return cmp_result(1);
}

SimValue sim_val_neq(SimValue a, SimValue b) {
    if (sim_val_has_xz(a) || sim_val_has_xz(b)) return cmp_x();
    int w = a.width > b.width ? a.width : b.width;
    int nw = num_words(w);
    for (int i = 0; i < nw; i++) {
        uint64_t m = word_mask(w, i);
        if ((a.val[i] & m) != (b.val[i] & m)) return cmp_result(1);
    }
    return cmp_result(0);
}

SimValue sim_val_lt(SimValue a, SimValue b) {
    if (sim_val_has_xz(a) || sim_val_has_xz(b)) return cmp_x();
    int w = a.width > b.width ? a.width : b.width;
    int nw = num_words(w);
    /* Compare from MSW to LSW */
    for (int i = nw - 1; i >= 0; i--) {
        uint64_t m = word_mask(w, i);
        uint64_t av = a.val[i] & m;
        uint64_t bv = b.val[i] & m;
        if (av < bv) return cmp_result(1);
        if (av > bv) return cmp_result(0);
    }
    return cmp_result(0); /* equal */
}

SimValue sim_val_gt(SimValue a, SimValue b) {
    return sim_val_lt(b, a);
}

SimValue sim_val_lte(SimValue a, SimValue b) {
    if (sim_val_has_xz(a) || sim_val_has_xz(b)) return cmp_x();
    int w = a.width > b.width ? a.width : b.width;
    int nw = num_words(w);
    for (int i = nw - 1; i >= 0; i--) {
        uint64_t m = word_mask(w, i);
        uint64_t av = a.val[i] & m;
        uint64_t bv = b.val[i] & m;
        if (av < bv) return cmp_result(1);
        if (av > bv) return cmp_result(0);
    }
    return cmp_result(1); /* equal -> true for LTE */
}

SimValue sim_val_gte(SimValue a, SimValue b) {
    return sim_val_lte(b, a);
}

/* ---- logical ---- */

SimValue sim_val_logical_and(SimValue a, SimValue b) {
    int ta = sim_val_is_true(a);
    int tb = sim_val_is_true(b);
    if (ta == 0 || tb == 0) return cmp_result(0);
    if (ta == -1 || tb == -1) return cmp_x();
    return cmp_result(1);
}

SimValue sim_val_logical_or(SimValue a, SimValue b) {
    int ta = sim_val_is_true(a);
    int tb = sim_val_is_true(b);
    if (ta == 1 || tb == 1) return cmp_result(1);
    if (ta == -1 || tb == -1) return cmp_x();
    return cmp_result(0);
}

SimValue sim_val_logical_not(SimValue a) {
    int ta = sim_val_is_true(a);
    if (ta == -1) return cmp_x();
    return cmp_result(ta == 0 ? 1 : 0);
}

/* ---- shift ---- */

SimValue sim_val_shl(SimValue a, SimValue b) {
    if (sim_val_has_xz(a) || sim_val_has_xz(b))
        return sim_val_all_x(a.width);
    uint64_t shift = b.val[0];
    if (shift >= (uint64_t)a.width) return sim_val_zero(a.width);

    int s = (int)shift;
    int nw = num_words(a.width);
    SimValue r;
    memset(&r, 0, sizeof(r));
    r.width = a.width;

    int word_shift = s / 64;
    int bit_shift = s % 64;

    for (int i = nw - 1; i >= 0; i--) {
        int src_lo = i - word_shift;
        int src_hi = i - word_shift - 1;
        uint64_t lo_part = (src_lo >= 0 && src_lo < nw) ? a.val[src_lo] : 0;
        uint64_t hi_part = (src_hi >= 0 && src_hi < nw) ? a.val[src_hi] : 0;
        if (bit_shift == 0) {
            r.val[i] = lo_part;
        } else {
            r.val[i] = (lo_part << bit_shift) |
                        (hi_part >> (64 - bit_shift));
        }
    }
    return sim_val_mask(r);
}

SimValue sim_val_shr(SimValue a, SimValue b) {
    if (sim_val_has_xz(a) || sim_val_has_xz(b))
        return sim_val_all_x(a.width);
    uint64_t shift = b.val[0];
    if (shift >= (uint64_t)a.width) return sim_val_zero(a.width);

    SimValue am = sim_val_mask(a);
    int s = (int)shift;
    int nw = num_words(a.width);
    SimValue r;
    memset(&r, 0, sizeof(r));
    r.width = a.width;

    int word_shift = s / 64;
    int bit_shift = s % 64;

    for (int i = 0; i < nw; i++) {
        int src_lo = i + word_shift;
        int src_hi = i + word_shift + 1;
        uint64_t lo_part = (src_lo >= 0 && src_lo < nw) ? am.val[src_lo] : 0;
        uint64_t hi_part = (src_hi >= 0 && src_hi < nw) ? am.val[src_hi] : 0;
        if (bit_shift == 0) {
            r.val[i] = lo_part;
        } else {
            r.val[i] = (lo_part >> bit_shift) |
                        (hi_part << (64 - bit_shift));
        }
    }
    return sim_val_mask(r);
}

SimValue sim_val_ashr(SimValue a, SimValue b) {
    if (sim_val_has_xz(a) || sim_val_has_xz(b))
        return sim_val_all_x(a.width);
    uint64_t shift = b.val[0];
    int sign_bit = sim_val_get_bit(a, a.width - 1);
    if (shift >= (uint64_t)a.width) {
        return sign_bit ? sim_val_ones(a.width) : sim_val_zero(a.width);
    }

    /* Logical shift right first */
    SimValue r = sim_val_shr(a, b);

    /* Fill upper bits with sign */
    if (sign_bit && shift > 0) {
        int s = (int)shift;
        for (int bit = a.width - s; bit < a.width; bit++) {
            sim_val_set_bit(&r, bit, 1);
        }
    }
    return r;
}

/* ---- concat / slice (multi-word) ---- */

SimValue sim_val_concat(const SimValue *vals, int count) {
    /* vals[0] is MSB, vals[count-1] is LSB (Verilog convention) */
    int total_width = 0;
    for (int i = 0; i < count; i++)
        total_width += vals[i].width;
    int max_w = SIM_VAL_WORDS * 64;
    if (total_width > max_w) total_width = max_w;

    SimValue r;
    memset(&r, 0, sizeof(r));
    r.width = total_width;

    int shift = 0;
    for (int i = count - 1; i >= 0; i--) {
        int w = vals[i].width;
        int src_nw = num_words(w);
        for (int si = 0; si < src_nw; si++) {
            uint64_t m = word_mask(w, si);
            uint64_t vb = vals[i].val[si] & m;
            uint64_t xb = vals[i].xmask[si] & m;
            uint64_t zb = vals[i].zmask[si] & m;

            int bit_base = shift + si * 64;
            int dst_wi = bit_base / 64;
            int dst_bi = bit_base % 64;

            if (dst_wi < SIM_VAL_WORDS) {
                r.val[dst_wi]   |= vb << dst_bi;
                r.xmask[dst_wi] |= xb << dst_bi;
                r.zmask[dst_wi] |= zb << dst_bi;
            }
            if (dst_bi > 0 && dst_wi + 1 < SIM_VAL_WORDS) {
                r.val[dst_wi + 1]   |= vb >> (64 - dst_bi);
                r.xmask[dst_wi + 1] |= xb >> (64 - dst_bi);
                r.zmask[dst_wi + 1] |= zb >> (64 - dst_bi);
            }
        }
        shift += w;
    }
    return sim_val_mask(r);
}

SimValue sim_val_slice(SimValue v, int msb, int lsb) {
    int rw = msb - lsb + 1;
    if (rw <= 0) return sim_val_zero(1);

    SimValue r;
    memset(&r, 0, sizeof(r));
    r.width = rw;

    int r_nw = num_words(rw);
    for (int ri = 0; ri < r_nw; ri++) {
        int src_bit = lsb + ri * 64;
        int src_wi = src_bit / 64;
        int src_bi = src_bit % 64;

        if (src_wi < SIM_VAL_WORDS) {
            r.val[ri]   = v.val[src_wi]   >> src_bi;
            r.xmask[ri] = v.xmask[src_wi] >> src_bi;
            r.zmask[ri] = v.zmask[src_wi] >> src_bi;
        }
        if (src_bi > 0 && src_wi + 1 < SIM_VAL_WORDS) {
            r.val[ri]   |= v.val[src_wi + 1]   << (64 - src_bi);
            r.xmask[ri] |= v.xmask[src_wi + 1] << (64 - src_bi);
            r.zmask[ri] |= v.zmask[src_wi + 1] << (64 - src_bi);
        }
    }
    return sim_val_mask(r);
}

/* ---- ternary ---- */

SimValue sim_val_ternary(SimValue cond, SimValue t, SimValue f) {
    int tc = sim_val_is_true(cond);
    if (tc == 1) return t;
    if (tc == 0) return f;
    /* x/z condition: merge both results */
    int rw = t.width > f.width ? t.width : f.width;
    int nw = num_words(rw);
    SimValue r;
    memset(&r, 0, sizeof(r));
    r.width = rw;
    for (int i = 0; i < nw; i++) {
        uint64_t m = word_mask(rw, i);
        uint64_t differ = ((t.val[i] ^ f.val[i]) | t.xmask[i] | f.xmask[i] |
                           t.zmask[i] | f.zmask[i]) & m;
        r.val[i] = t.val[i] & ~differ;
        r.xmask[i] = differ;
    }
    return r;
}

/* ---- extension ---- */

SimValue sim_val_zext(SimValue v, int new_width) {
    if (new_width <= v.width) return v;
    SimValue r = v;
    r.width = new_width;
    /* upper bits are already 0 from construction */
    return r;
}

SimValue sim_val_sext(SimValue v, int new_width) {
    if (new_width <= v.width) return v;
    int sign_bit = sim_val_get_bit(v, v.width - 1);
    int sign_x = 0;
    if (v.width > 0) {
        int wi = (v.width - 1) / 64;
        int bi = (v.width - 1) % 64;
        sign_x = (int)((v.xmask[wi] >> bi) & 1);
    }

    SimValue r = v;
    r.width = new_width;

    if (sign_bit) {
        /* Fill upper bits with 1 */
        for (int bit = v.width; bit < new_width; bit++) {
            sim_val_set_bit(&r, bit, 1);
        }
    }
    if (sign_x) {
        /* Fill upper bits with x */
        for (int bit = v.width; bit < new_width; bit++) {
            int wi = bit / 64;
            int bi = bit % 64;
            r.xmask[wi] |= (uint64_t)1 << bi;
        }
    }
    return r;
}

/* ---- formatting ---- */

char *sim_val_to_hex(SimValue v, char *buf, int buflen) {
    if (buflen < 2) { buf[0] = '\0'; return buf; }
    int nib_count = (v.width + 3) / 4;
    if (nib_count == 0) nib_count = 1;

    int pos = 0;
    for (int i = nib_count - 1; i >= 0 && pos < buflen - 1; i--) {
        int bit_lo = i * 4;
        int wi = bit_lo / 64;
        int bi = bit_lo % 64;

        uint64_t nibble_val = 0, nibble_x = 0, nibble_z = 0;
        if (wi < SIM_VAL_WORDS) {
            nibble_val = (v.val[wi] >> bi) & 0xF;
            nibble_x = (v.xmask[wi] >> bi) & 0xF;
            nibble_z = (v.zmask[wi] >> bi) & 0xF;
        }
        /* Handle cross-word nibble (when bi > 60) */
        if (bi > 60 && wi + 1 < SIM_VAL_WORDS) {
            int carry = 64 - bi;
            nibble_val |= (v.val[wi + 1] << carry) & 0xF;
            nibble_x |= (v.xmask[wi + 1] << carry) & 0xF;
            nibble_z |= (v.zmask[wi + 1] << carry) & 0xF;
        }

        if (nibble_x == 0xF)
            buf[pos++] = 'x';
        else if (nibble_z == 0xF)
            buf[pos++] = 'z';
        else if (nibble_x || nibble_z)
            buf[pos++] = 'X'; /* mixed x/z in nibble */
        else
            buf[pos++] = "0123456789abcdef"[nibble_val];
    }
    buf[pos] = '\0';
    return buf;
}

char *sim_val_to_bin(SimValue v, char *buf, int buflen) {
    if (buflen < 2) { buf[0] = '\0'; return buf; }
    int pos = 0;
    for (int i = v.width - 1; i >= 0 && pos < buflen - 1; i--) {
        int wi = i / 64;
        int bi = i % 64;
        int x_bit = (wi < SIM_VAL_WORDS) ? (int)((v.xmask[wi] >> bi) & 1) : 0;
        int z_bit = (wi < SIM_VAL_WORDS) ? (int)((v.zmask[wi] >> bi) & 1) : 0;
        int v_bit = (wi < SIM_VAL_WORDS) ? (int)((v.val[wi] >> bi) & 1) : 0;
        if (x_bit) buf[pos++] = 'x';
        else if (z_bit) buf[pos++] = 'z';
        else buf[pos++] = '0' + v_bit;
    }
    buf[pos] = '\0';
    return buf;
}

char *sim_val_to_dec(SimValue v, char *buf, int buflen) {
    if (buflen < 2) { buf[0] = '\0'; return buf; }
    if (sim_val_has_xz(v)) {
        snprintf(buf, (size_t)buflen, "x");
        return buf;
    }
    /* For values that fit in 64 bits, use simple conversion */
    if (v.width <= 64) {
        snprintf(buf, (size_t)buflen, "%llu", (unsigned long long)v.val[0]);
    } else {
        /* For wider values, fall back to hex representation */
        snprintf(buf, (size_t)buflen, "0x");
        int prefix_len = 2;
        sim_val_to_hex(v, buf + prefix_len, buflen - prefix_len);
    }
    return buf;
}

char *sim_val_format_literal(SimValue v, char *buf, int buflen) {
    char hex[80];
    sim_val_to_hex(v, hex, sizeof(hex));
    snprintf(buf, (size_t)buflen, "%d'h%s", v.width, hex);
    return buf;
}
