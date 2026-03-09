#include <stdio.h>
#include <string.h>

#include "sem.h"

static int test_single_expr(const char *expr, long long expected)
{
    long long value = 0;
    JZConstEvalOptions opts = { 0 };
    int rc = jz_const_eval_expr(expr, &opts, &value);
    if (rc != 0) {
        fprintf(stderr, "jz_const_eval_expr failed for '%s' (rc=%d)\\n", expr, rc);
        return 1;
    }
    if (value != expected) {
        fprintf(stderr, "jz_const_eval_expr('%s') => %lld, expected %lld\\n",
                expr, value, expected);
        return 1;
    }
    return 0;
}

static int test_single_expr_fail(const char *expr)
{
    long long value = 0;
    JZConstEvalOptions opts = { 0 };
    int rc = jz_const_eval_expr(expr, &opts, &value);
    if (rc == 0) {
        fprintf(stderr, "expected jz_const_eval_expr to fail for '%s' but it succeeded with %lld\\n",
                expr, value);
        return 1;
    }
    return 0;
}

static int test_all_defs(void)
{
    JZConstDef defs[] = {
        { "WIDTH", "8" },
        { "DEPTH", "16" },
        { "ADDR",  "clog2(DEPTH)" },
    };
    long long values[3] = {0, 0, 0};
    JZConstEvalOptions opts = { 0 };

    int rc = jz_const_eval_all(defs, 3, &opts, values);
    if (rc != 0) {
        fprintf(stderr, "jz_const_eval_all failed (rc=%d)\n", rc);
        return 1;
    }
    if (values[0] != 8 || values[1] != 16 || values[2] != 4) {
        fprintf(stderr, "unexpected values: WIDTH=%lld DEPTH=%lld ADDR=%lld\n",
                values[0], values[1], values[2]);
        return 1;
    }

    /* Now create a circular dependency to ensure detection. */
    JZConstDef cyclic_defs[] = {
        { "A", "B + 1" },
        { "B", "A + 1" },
    };
    long long cyclic_values[2] = {0, 0};
    rc = jz_const_eval_all(cyclic_defs, 2, &opts, cyclic_values);
    if (rc == 0) {
        fprintf(stderr, "expected cycle detection to fail but it succeeded\n");
        return 1;
    }

    /* DAC-style environment with anonymous @check expression. */
    JZConstDef dac_defs[] = {
        { "CLK_FREQ_HZ", "27000000" },
        { "VALUE_BITS",  "8" },
        { "MIN_PWM_HZ",  "50000" },
        { NULL, "(CLK_FREQ_HZ / (1 << VALUE_BITS)) < MIN_PWM_HZ" },
    };
    long long dac_values[4] = {0, 0, 0, 0};
    rc = jz_const_eval_all(dac_defs, 4, &opts, dac_values);
    if (rc != 0) {
        fprintf(stderr, "jz_const_eval_all failed for DAC-style defs (rc=%d)\n", rc);
        return 1;
    }
    if (dac_values[3] != 0) {
        fprintf(stderr, "DAC-style check value = %lld, expected 0\n", dac_values[3]);
        return 1;
    }

    return 0;
}

int main(void)
{
    int failures = 0;
    failures += test_single_expr("1 + 2 * 3", 7);
    failures += test_single_expr("(1 + 2) * 3", 9);
    failures += test_single_expr("clog2(1)", 0);
    failures += test_single_expr("clog2(2)", 1);
    failures += test_single_expr("clog2(3)", 2);
    failures += test_single_expr("clog2(16)", 4);
    failures += test_single_expr("4 == 4", 1);
    failures += test_single_expr("4 != 5", 1);
    failures += test_single_expr("4 < 5", 1);
    failures += test_single_expr("5 <= 5", 1);
    failures += test_single_expr("6 > 5", 1);
    failures += test_single_expr("6 >= 6", 1);
    failures += test_single_expr("10 / 3", 3);
    failures += test_single_expr("10 % 3", 1);
    /* Underscore-separated decimal literals and comparison results. */
    failures += test_single_expr("27_000_000 / 256", 105468);
    failures += test_single_expr("(27_000_000 / 256) < 50_000", 0);
    /* Shift operators in constant expressions. */
    failures += test_single_expr("1 << 3", 8);
    failures += test_single_expr("(27_000_000 / (1 << 8)) < 50_000", 0);

    /* Non-negative and error conditions. */
    failures += test_single_expr_fail("-1");
    failures += test_single_expr_fail("1 - 2");
    failures += test_single_expr_fail("1 / 0");
    failures += test_single_expr_fail("clog2(0)");

    failures += test_all_defs();

    if (failures != 0) {
        fprintf(stderr, "%d constant-eval tests failed\n", failures);
        return 1;
    }

    return 0;
}
