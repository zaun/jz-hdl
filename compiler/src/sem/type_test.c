#include <stdio.h>

#include "sem.h"

static int expect_unary(JZUnaryOp op, unsigned in_width, unsigned out_width, int should_fail)
{
    JZBitvecType in, out;
    jz_type_scalar(in_width, 0, &in);
    int rc = jz_type_unary(op, &in, &out);
    if (should_fail) {
        if (rc == 0) {
            fprintf(stderr, "expected unary op to fail for width %u but it succeeded\n", in_width);
            return 1;
        }
        return 0;
    }
    if (rc != 0) {
        fprintf(stderr, "unexpected unary failure for width %u\n", in_width);
        return 1;
    }
    if (out.width != out_width) {
        fprintf(stderr, "unary result width %u, expected %u\n", out.width, out_width);
        return 1;
    }
    return 0;
}

static int expect_binary(JZBinaryOp op,
                         unsigned lhs_width,
                         unsigned rhs_width,
                         unsigned expected_width,
                         int should_fail)
{
    JZBitvecType lhs, rhs, out;
    jz_type_scalar(lhs_width, 0, &lhs);
    jz_type_scalar(rhs_width, 0, &rhs);
    int rc = jz_type_binary(op, &lhs, &rhs, &out);
    if (should_fail) {
        if (rc == 0) {
            fprintf(stderr, "expected binary op to fail for %u vs %u but it succeeded\n",
                    lhs_width, rhs_width);
            return 1;
        }
        return 0;
    }
    if (rc != 0) {
        fprintf(stderr, "unexpected binary failure for %u vs %u\n",
                lhs_width, rhs_width);
        return 1;
    }
    if (out.width != expected_width) {
        fprintf(stderr, "binary result width %u, expected %u\n",
                out.width, expected_width);
        return 1;
    }
    return 0;
}

static int expect_ternary(unsigned cond_width,
                          unsigned t_width,
                          unsigned f_width,
                          unsigned expected_width,
                          int should_fail)
{
    JZBitvecType cond, t, f, out;
    jz_type_scalar(cond_width, 0, &cond);
    jz_type_scalar(t_width, 0, &t);
    jz_type_scalar(f_width, 0, &f);
    int rc = jz_type_ternary(&cond, &t, &f, &out);
    if (should_fail) {
        if (rc == 0) {
            fprintf(stderr, "expected ternary to fail (cond=%u, t=%u, f=%u) but it succeeded\n",
                    cond_width, t_width, f_width);
            return 1;
        }
        return 0;
    }
    if (rc != 0) {
        fprintf(stderr, "unexpected ternary failure (cond=%u, t=%u, f=%u)\n",
                cond_width, t_width, f_width);
        return 1;
    }
    if (out.width != expected_width) {
        fprintf(stderr, "ternary result width %u, expected %u\n",
                out.width, expected_width);
        return 1;
    }
    return 0;
}

static int expect_concat(const unsigned *widths,
                         size_t count,
                         unsigned expected_width,
                         int should_fail)
{
    JZBitvecType elems[8];
    if (count > 8) return 1;
    for (size_t i = 0; i < count; ++i) {
        jz_type_scalar(widths[i], 0, &elems[i]);
    }
    JZBitvecType out;
    int rc = jz_type_concat(elems, count, &out);
    if (should_fail) {
        if (rc == 0) {
            fprintf(stderr, "expected concat to fail but it succeeded\n");
            return 1;
        }
        return 0;
    }
    if (rc != 0) {
        fprintf(stderr, "unexpected concat failure\n");
        return 1;
    }
    if (out.width != expected_width) {
        fprintf(stderr, "concat result width %u, expected %u\n",
                out.width, expected_width);
        return 1;
    }
    return 0;
}

static int expect_slice(unsigned base_width,
                        unsigned msb,
                        unsigned lsb,
                        unsigned expected_width,
                        int should_fail)
{
    JZBitvecType base, out;
    jz_type_scalar(base_width, 0, &base);
    int rc = jz_type_slice(&base, msb, lsb, &out);
    if (should_fail) {
        if (rc == 0) {
            fprintf(stderr, "expected slice [%u:%u] of %u to fail but it succeeded\n",
                    msb, lsb, base_width);
            return 1;
        }
        return 0;
    }
    if (rc != 0) {
        fprintf(stderr, "unexpected slice failure [%u:%u] of %u\n",
                msb, lsb, base_width);
        return 1;
    }
    if (out.width != expected_width) {
        fprintf(stderr, "slice result width %u, expected %u\n",
                out.width, expected_width);
        return 1;
    }
    return 0;
}

int main(void)
{
    int failures = 0;

    /* Unary arithmetic and logical rules. */
    failures += expect_unary(JZ_UNARY_PLUS, 1, 1, 0);
    failures += expect_unary(JZ_UNARY_MINUS, 1, 1, 0);
    failures += expect_unary(JZ_UNARY_PLUS, 8, 8, 0);
    failures += expect_unary(JZ_UNARY_MINUS, 8, 8, 0);
    failures += expect_unary(JZ_UNARY_LOGICAL_NOT, 1, 1, 0);
    failures += expect_unary(JZ_UNARY_LOGICAL_NOT, 4, 0, 1);

    /* Binary arithmetic. */
    failures += expect_binary(JZ_BIN_ADD, 8, 8, 8, 0);
    failures += expect_binary(JZ_BIN_SUB, 8, 8, 8, 0);
    failures += expect_binary(JZ_BIN_ADD, 8, 4, 0, 1);
    failures += expect_binary(JZ_BIN_MUL, 8, 8, 16, 0);
    failures += expect_binary(JZ_BIN_DIV, 8, 8, 8, 0);
    failures += expect_binary(JZ_BIN_MOD, 8, 8, 8, 0);

    /* Bitwise and logical operators. */
    failures += expect_binary(JZ_BIN_BIT_AND, 4, 4, 4, 0);
    failures += expect_binary(JZ_BIN_BIT_OR, 4, 2, 0, 1);
    failures += expect_binary(JZ_BIN_LOG_AND, 1, 1, 1, 0);
    failures += expect_binary(JZ_BIN_LOG_AND, 1, 2, 0, 1);

    /* Comparisons and shifts. */
    failures += expect_binary(JZ_BIN_EQ, 8, 8, 1, 0);
    failures += expect_binary(JZ_BIN_LT, 8, 4, 0, 1);
    failures += expect_binary(JZ_BIN_SHL, 8, 3, 8, 0);

    /* Ternary width rules. */
    failures += expect_ternary(1, 8, 8, 8, 0);
    failures += expect_ternary(2, 8, 8, 0, 1);
    failures += expect_ternary(1, 8, 4, 0, 1);

    /* Concatenation. */
    {
        unsigned ws1[] = { 4, 4 };
        failures += expect_concat(ws1, 2, 8, 0);
        unsigned ws2[] = { 1, 2, 3 };
        failures += expect_concat(ws2, 3, 6, 0);
    }

    /* Slicing. */
    failures += expect_slice(8, 7, 0, 8, 0);
    failures += expect_slice(8, 3, 1, 3, 0);
    failures += expect_slice(8, 8, 0, 0, 1); /* msb out of range */
    failures += expect_slice(8, 2, 3, 0, 1); /* msb < lsb */

    if (failures != 0) {
        fprintf(stderr, "%d type/width tests failed\n", failures);
        return 1;
    }

    return 0;
}
