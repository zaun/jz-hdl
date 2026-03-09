#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "sem.h"

static unsigned count_digits_no_underscore(const char *s)
{
    unsigned n = 0;
    if (!s) return 0;
    for (; *s; ++s) {
        if (*s != '_') n++;
    }
    return n;
}

static unsigned intrinsic_width_for_decimal(const char *value)
{
    if (!value || !*value) return 0;
    /* Parse as unsigned long long for width estimation. */
    unsigned long long acc = 0;
    for (const char *p = value; *p; ++p) {
        if (*p == '_') continue;
        if (!isdigit((unsigned char)*p)) return 0; /* lexer should prevent this */
        unsigned d = (unsigned)(*p - '0');
        if (acc > (ULLONG_MAX - d) / 10ULL) {
            /* Clamp; the exact width is huge but we treat as overflow elsewhere. */
            return sizeof(unsigned long long) * 8U;
        }
        acc = acc * 10ULL + d;
    }
    if (acc == 0) return 1; /* represent 0 as width-1 */

    unsigned bits = 0;
    while (acc > 0) {
        acc >>= 1;
        bits++;
    }
    return bits ? bits : 1;
}

/* Compute intrinsic bit-width for a pure-hex (0–9, A–F only) value string. */
static unsigned intrinsic_width_for_hex_magnitude(const char *value)
{
    if (!value || !*value) return 0;

    /* Count non-underscore hex digits. */
    unsigned total_nibbles = 0;
    for (const char *p = value; *p; ++p) {
        if (*p == '_') continue;
        total_nibbles++;
    }
    if (total_nibbles == 0) return 0;

    /* Find the first non-zero hex digit from the left. */
    unsigned idx = 0;
    for (const char *p = value; *p; ++p) {
        char c = *p;
        if (c == '_') continue;

        unsigned v = 0;
        if (c >= '0' && c <= '9') {
            v = (unsigned)(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            v = 10u + (unsigned)(c - 'a');
        } else if (c >= 'A' && c <= 'F') {
            v = 10u + (unsigned)(c - 'A');
        } else {
            /* Caller should not pass x/z digits here. */
            idx++;
            continue;
        }

        if (v == 0u) {
            idx++;
            continue;
        }

        /* Position (1–4) of the highest set bit within this nibble. */
        unsigned msb_pos;
        if (v & 0x8u)      msb_pos = 4u;
        else if (v & 0x4u) msb_pos = 3u;
        else if (v & 0x2u) msb_pos = 2u;
        else               msb_pos = 1u;

        unsigned remaining = total_nibbles - idx;
        return (remaining - 1u) * 4u + msb_pos;
    }

    /* All hex digits were zero; represent 0 with width-1. */
    return 1u;
}

int jz_literal_analyze(JZNumericBase base,
                       const char   *value_lexeme,
                       unsigned      declared_width,
                       unsigned     *out_intrinsic_width,
                       JZLiteralExtKind *out_ext_kind)
{
    if (!value_lexeme || !out_intrinsic_width || !out_ext_kind || declared_width == 0) {
        return -1;
    }

    unsigned intrinsic = 0;
    JZLiteralExtKind ext = JZ_LITERAL_EXT_NONE;

    switch (base) {
    case JZ_NUM_BASE_BIN: {
        unsigned digits = count_digits_no_underscore(value_lexeme);
        intrinsic = digits; /* 1 bit per digit */
        /* MSB is first non-underscore character. */
        const char *p = value_lexeme;
        char msb = '0';
        while (*p && *p == '_') ++p;
        if (*p) msb = *p;
        if (intrinsic < declared_width) {
            if (msb == '0' || msb == '1') ext = JZ_LITERAL_EXT_ZERO;
            else if (msb == 'x' || msb == 'X') ext = JZ_LITERAL_EXT_X;
            else if (msb == 'z' || msb == 'Z') ext = JZ_LITERAL_EXT_Z;
            else ext = JZ_LITERAL_EXT_ZERO;
        }
        break;
    }

    case JZ_NUM_BASE_HEX: {
        const char *p = value_lexeme;
        char msb = '0';

        while (*p && *p == '_') ++p;
        if (*p) msb = *p;

        intrinsic = intrinsic_width_for_hex_magnitude(value_lexeme);
        if (intrinsic == 0) {
            return -1;
        }

        if (intrinsic < declared_width) {
            /* Hex literals can only contain 0–9, A–F digits, so extension
             * is always zero-fill when widening to the declared width.
             */
            if ((msb >= '0' && msb <= '9') ||
                (msb >= 'a' && msb <= 'f') ||
                (msb >= 'A' && msb <= 'F')) {
                ext = JZ_LITERAL_EXT_ZERO;
            } else {
                ext = JZ_LITERAL_EXT_ZERO;
            }
        }
        break;
    }

    case JZ_NUM_BASE_DEC: {
        intrinsic = intrinsic_width_for_decimal(value_lexeme);
        if (intrinsic == 0) return -1;
        /* Decimal literals do not carry x/z; extension is always zero-fill. */
        if (intrinsic < declared_width) {
            ext = JZ_LITERAL_EXT_ZERO;
        }
        break;
    }

    default:
        return -1;
    }

    if (intrinsic > declared_width) {
        /* Overflow before extension. */
        return -1;
    }

    if (intrinsic == declared_width) {
        ext = JZ_LITERAL_EXT_NONE;
    }

    *out_intrinsic_width = intrinsic;
    *out_ext_kind = ext;
    return 0;
}
