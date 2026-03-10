/**
 * @file sim_value.h
 * @brief 4-state bit-vector type for cycle-accurate simulation.
 *
 * Encoding per bit:
 *   val=0, xmask=0, zmask=0 -> logic 0
 *   val=1, xmask=0, zmask=0 -> logic 1
 *   xmask=1                 -> x (unknown)
 *   zmask=1                 -> z (high-impedance)
 *
 * Maximum supported width: 256 bits (SIM_VAL_WORDS * 64).
 */

#ifndef JZ_SIM_VALUE_H
#define JZ_SIM_VALUE_H

#include <stdint.h>

/** Number of 64-bit words per SimValue (supports up to 256 bits). */
#define SIM_VAL_WORDS 4

typedef struct SimValue {
    uint64_t val[SIM_VAL_WORDS];    /* 0/1 bits */
    uint64_t xmask[SIM_VAL_WORDS];  /* 1 = bit is x (unknown) */
    uint64_t zmask[SIM_VAL_WORDS];  /* 1 = bit is z (high-impedance) */
    int      width;                 /* 1..256 */
} SimValue;

/* Construction */
SimValue sim_val_zero(int width);
SimValue sim_val_ones(int width);
SimValue sim_val_from_uint(uint64_t v, int width);
SimValue sim_val_from_words(const uint64_t *words, int num_words, int width);
SimValue sim_val_all_x(int width);
SimValue sim_val_all_z(int width);

/* Mask bits above width to zero */
SimValue sim_val_mask(SimValue v);

/* Per-bit access */
int sim_val_get_bit(SimValue v, int bit);       /* returns 0 or 1 (ignores x/z) */
void sim_val_set_bit(SimValue *v, int bit, int b); /* sets val bit only */

/* Query */
int sim_val_has_xz(SimValue v);
int sim_val_is_all_z(SimValue v);  /* 1 if all bits within width are z */
int sim_val_is_true(SimValue v);   /* non-zero val and no x/z -> 1; x/z -> -1; zero -> 0 */
int sim_val_equal(SimValue a, SimValue b); /* exact 4-state compare */

/* Arithmetic (x/z inputs -> all-x result) */
SimValue sim_val_add(SimValue a, SimValue b);
SimValue sim_val_sub(SimValue a, SimValue b);
SimValue sim_val_mul(SimValue a, SimValue b);
SimValue sim_val_div(SimValue a, SimValue b);
SimValue sim_val_mod(SimValue a, SimValue b);
SimValue sim_val_neg(SimValue a);

/* Bitwise (per-bit 4-state truth tables) */
SimValue sim_val_and(SimValue a, SimValue b);
SimValue sim_val_or(SimValue a, SimValue b);
SimValue sim_val_xor(SimValue a, SimValue b);
SimValue sim_val_not(SimValue a);

/* Comparison (return width-1 SimValue; x if any input has x/z) */
SimValue sim_val_eq(SimValue a, SimValue b);
SimValue sim_val_neq(SimValue a, SimValue b);
SimValue sim_val_lt(SimValue a, SimValue b);
SimValue sim_val_gt(SimValue a, SimValue b);
SimValue sim_val_lte(SimValue a, SimValue b);
SimValue sim_val_gte(SimValue a, SimValue b);

/* Logical */
SimValue sim_val_logical_and(SimValue a, SimValue b);
SimValue sim_val_logical_or(SimValue a, SimValue b);
SimValue sim_val_logical_not(SimValue a);

/* Shift */
SimValue sim_val_shl(SimValue a, SimValue b);
SimValue sim_val_shr(SimValue a, SimValue b);
SimValue sim_val_ashr(SimValue a, SimValue b);

/* Concatenation and slicing */
SimValue sim_val_concat(const SimValue *vals, int count);
SimValue sim_val_slice(SimValue v, int msb, int lsb);

/* Ternary */
SimValue sim_val_ternary(SimValue cond, SimValue t, SimValue f);

/* Extension */
SimValue sim_val_zext(SimValue v, int new_width);
SimValue sim_val_sext(SimValue v, int new_width);

/* Formatting (writes into caller-provided buffer, returns buf) */
char *sim_val_to_hex(SimValue v, char *buf, int buflen);
char *sim_val_to_bin(SimValue v, char *buf, int buflen);
char *sim_val_to_dec(SimValue v, char *buf, int buflen);

/* Format as sized literal string, e.g. "8'hFF" */
char *sim_val_format_literal(SimValue v, char *buf, int buflen);

#endif /* JZ_SIM_VALUE_H */
