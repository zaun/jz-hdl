#include <stddef.h>

#include "sem.h"

static int type_is_valid(const JZBitvecType *t)
{
    return t && t->width > 0;
}

void jz_type_scalar(unsigned width, int is_signed, JZBitvecType *out)
{
    if (!out) return;
    out->width = width;
    out->is_signed = is_signed ? 1 : 0;
}

int jz_type_unary(JZUnaryOp op, const JZBitvecType *operand, JZBitvecType *out)
{
    if (!type_is_valid(operand) || !out) return -1;

    switch (op) {
    case JZ_UNARY_PLUS:
    case JZ_UNARY_MINUS:
        /* Unary arithmetic: result width = input width. */
        out->width = operand->width;
        out->is_signed = operand->is_signed;
        return 0;

    case JZ_UNARY_BIT_NOT:
        /* Bitwise NOT preserves width and signedness. */
        *out = *operand;
        return 0;

    case JZ_UNARY_LOGICAL_NOT:
        /* Logical NOT operates on width-1 and returns width-1. */
        if (operand->width != 1) {
            return -1;
        }
        out->width = 1;
        out->is_signed = 0;
        return 0;
    }

    return -1;
}

int jz_type_binary(JZBinaryOp op,
                   const JZBitvecType *lhs,
                   const JZBitvecType *rhs,
                   JZBitvecType *out)
{
    if (!type_is_valid(lhs) || !type_is_valid(rhs) || !out) return -1;

    switch (op) {
    /* Binary arithmetic: add/sub */
    case JZ_BIN_ADD:
    case JZ_BIN_SUB:
        if (lhs->width != rhs->width) return -1;
        out->width = lhs->width;
        out->is_signed = lhs->is_signed || rhs->is_signed;
        return 0;

    /* Multiply: full-width product (2 * max_bits). */
    case JZ_BIN_MUL: {
        if (lhs->width != rhs->width) return -1;
        unsigned w = lhs->width;
        if (w > 0 && w > (unsigned)(~0u) / 2) return -1; /* overflow guard */
        out->width = w * 2;
        out->is_signed = lhs->is_signed || rhs->is_signed;
        return 0;
    }

    /* Division and modulus: result width = dividend width. */
    case JZ_BIN_DIV:
    case JZ_BIN_MOD:
        if (lhs->width != rhs->width) return -1;
        out->width = lhs->width;
        out->is_signed = lhs->is_signed || rhs->is_signed;
        return 0;

    /* Bitwise ops: &, |, ^ */
    case JZ_BIN_BIT_AND:
    case JZ_BIN_BIT_OR:
    case JZ_BIN_BIT_XOR:
        if (lhs->width != rhs->width) return -1;
        out->width = lhs->width;
        out->is_signed = lhs->is_signed || rhs->is_signed;
        return 0;

    /* Logical ops: &&, || : width-1 inputs, width-1 result. */
    case JZ_BIN_LOG_AND:
    case JZ_BIN_LOG_OR:
        if (lhs->width != 1 || rhs->width != 1) return -1;
        out->width = 1;
        out->is_signed = 0;
        return 0;

    /* Comparisons: result width = 1, operands equal width. */
    case JZ_BIN_EQ:
    case JZ_BIN_NE:
    case JZ_BIN_LT:
    case JZ_BIN_LE:
    case JZ_BIN_GT:
    case JZ_BIN_GE:
        if (lhs->width != rhs->width) return -1;
        out->width = 1;
        out->is_signed = 0;
        return 0;

    /* Shifts: result width = LHS width. RHS may be any positive width. */
    case JZ_BIN_SHL:
    case JZ_BIN_SHR:
    case JZ_BIN_ASHR:
        if (rhs->width == 0) return -1;
        out->width = lhs->width;
        out->is_signed = lhs->is_signed;
        return 0;
    }

    return -1;
}

int jz_type_ternary(const JZBitvecType *cond,
                    const JZBitvecType *if_true,
                    const JZBitvecType *if_false,
                    JZBitvecType *out)
{
    if (!type_is_valid(cond) || !type_is_valid(if_true) || !type_is_valid(if_false) || !out)
        return -1;

    if (cond->width != 1) return -1;
    if (if_true->width != if_false->width) return -1;

    out->width = if_true->width;
    out->is_signed = if_true->is_signed || if_false->is_signed;
    return 0;
}

int jz_type_concat(const JZBitvecType *elems,
                   size_t count,
                   JZBitvecType *out)
{
    if (!elems || !out || count == 0) return -1;

    unsigned total = 0;
    for (size_t i = 0; i < count; ++i) {
        if (!type_is_valid(&elems[i])) return -1;
        if (elems[i].width > 0 && total > (unsigned)(~0u) - elems[i].width) {
            return -1; /* overflow */
        }
        total += elems[i].width;
    }

    out->width = total;
    out->is_signed = 0; /* concatenations are treated as unsigned bit-vectors */
    return 0;
}

int jz_type_slice(const JZBitvecType *base,
                  unsigned msb,
                  unsigned lsb,
                  JZBitvecType *out)
{
    if (!type_is_valid(base) || !out) return -1;
    if (msb < lsb) return -1;

    unsigned width = msb - lsb + 1;
    if (msb >= base->width) return -1;

    out->width = width;
    out->is_signed = 0; /* slices are plain bit-vectors */
    return 0;
}
