/**
 * @file repeat_expand.c
 * @brief Pre-parser @repeat expansion for JZ-HDL.
 *
 * Expands @repeat N ... @end blocks in raw source text before lexing.
 * The body between @repeat N and @end is duplicated N times, with
 * each standalone occurrence of IDX replaced by the iteration index
 * (0 through N-1). Nesting is supported.
 *
 * This runs on raw text before lexing, so it works in any context:
 * @testbench, @simulation, or any future construct.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "../include/repeat_expand.h"
#include "../include/diagnostic.h"

/* ── Dynamic string buffer ──────────────────────────────────────── */

typedef struct {
    char  *data;
    size_t len;
    size_t cap;
} StrBuf;

static void sb_init(StrBuf *sb)
{
    sb->cap = 4096;
    sb->data = malloc(sb->cap);
    sb->len = 0;
    sb->data[0] = '\0';
}

static void sb_append(StrBuf *sb, const char *s, size_t n)
{
    if (n == 0) return;
    while (sb->len + n + 1 > sb->cap) {
        sb->cap *= 2;
        sb->data = realloc(sb->data, sb->cap);
    }
    memcpy(sb->data + sb->len, s, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
}


/* ── Helpers ────────────────────────────────────────────────────── */

/* Check if character is a word boundary (not alphanumeric or underscore). */
static int is_word_char(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

/* Count the line number at position `pos` within `src`. */
static int count_line(const char *src, const char *pos)
{
    int line = 1;
    for (const char *p = src; p < pos; p++) {
        if (*p == '\n') line++;
    }
    return line;
}

/* Skip whitespace (spaces and tabs only, not newlines). */
static const char *skip_hspace(const char *p)
{
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

/**
 * Find the matching @end for a @repeat, handling nesting.
 * Returns pointer to the '@' of @end, or NULL if not found.
 */
static const char *find_matching_end(const char *body_start, const char *src_end)
{
    int depth = 1;
    const char *p = body_start;

    while (p < src_end && depth > 0) {
        /* Skip single-line comments */
        if (*p == '/' && p + 1 < src_end && p[1] == '/') {
            while (p < src_end && *p != '\n') p++;
            continue;
        }
        /* Skip string literals */
        if (*p == '"') {
            p++;
            while (p < src_end && *p != '"' && *p != '\n') {
                if (*p == '\\' && p + 1 < src_end) p++;
                p++;
            }
            if (p < src_end) p++;
            continue;
        }
        if (*p == '@') {
            /* Check for @repeat (nested) */
            if (strncmp(p, "@repeat", 7) == 0 && !is_word_char(p[7])) {
                depth++;
                p += 7;
                continue;
            }
            /* Check for @end — but NOT @endmod, @endtb, @endsim, etc. */
            if (strncmp(p, "@end", 4) == 0 && !is_word_char(p[4])) {
                depth--;
                if (depth == 0) {
                    return p;
                }
                p += 4;
                continue;
            }
        }
        p++;
    }
    return NULL;
}

/**
 * Substitute IDX with a decimal integer in the body text.
 * Only replaces standalone IDX (word boundaries on both sides).
 */
static void substitute_idx(StrBuf *sb, const char *body, size_t body_len, int idx)
{
    char idx_str[16];
    snprintf(idx_str, sizeof(idx_str), "%d", idx);
    size_t idx_str_len = strlen(idx_str);

    const char *p = body;
    const char *end = body + body_len;

    while (p < end) {
        /* Look for "IDX" */
        const char *found = NULL;
        for (const char *s = p; s + 3 <= end; s++) {
            if (s[0] == 'I' && s[1] == 'D' && s[2] == 'X') {
                /* Check word boundaries */
                int left_ok = (s == body) || !is_word_char(s[-1]);
                int right_ok = (s + 3 >= end) || !is_word_char(s[3]);
                if (left_ok && right_ok) {
                    found = s;
                    break;
                }
            }
        }

        if (found) {
            /* Append everything before IDX */
            sb_append(sb, p, (size_t)(found - p));
            /* Append the index value */
            sb_append(sb, idx_str, idx_str_len);
            p = found + 3;
        } else {
            /* No more IDX, append the rest */
            sb_append(sb, p, (size_t)(end - p));
            break;
        }
    }
}

/* ── Recursive expansion ────────────────────────────────────────── */

/**
 * Expand @repeat blocks in [start, src_end).
 * Appends expanded text to `out`.
 * Returns 0 on success, non-zero on error.
 */
static int expand_region(const char *start, const char *src_end,
                         const char *full_src, const char *filename,
                         JZDiagnosticList *diagnostics, StrBuf *out)
{
    const char *p = start;

    while (p < src_end) {
        /* Skip single-line comments */
        if (*p == '/' && p + 1 < src_end && p[1] == '/') {
            const char *eol = p;
            while (eol < src_end && *eol != '\n') eol++;
            sb_append(out, p, (size_t)(eol - p));
            p = eol;
            continue;
        }

        /* Skip string literals */
        if (*p == '"') {
            const char *q = p + 1;
            while (q < src_end && *q != '"' && *q != '\n') {
                if (*q == '\\' && q + 1 < src_end) q++;
                q++;
            }
            if (q < src_end && *q == '"') q++;
            sb_append(out, p, (size_t)(q - p));
            p = q;
            continue;
        }

        /* Scan for @repeat */
        if (*p == '@' && strncmp(p, "@repeat", 7) == 0 && !is_word_char(p[7])) {
            const char *repeat_at = p;
            p += 7;

            /* Skip whitespace after @repeat */
            p = skip_hspace(p);

            /* Parse the count */
            if (!isdigit((unsigned char)*p)) {
                JZLocation loc = { filename, count_line(full_src, repeat_at), 1 };
                jz_diagnostic_report(diagnostics, loc, JZ_SEVERITY_ERROR,
                                     "RPT_COUNT_INVALID",
                                     "RPT-001 @repeat requires a positive integer count");
                return 1;
            }

            int count = 0;
            while (isdigit((unsigned char)*p)) {
                count = count * 10 + (*p - '0');
                p++;
            }

            if (count <= 0) {
                JZLocation loc = { filename, count_line(full_src, repeat_at), 1 };
                jz_diagnostic_report(diagnostics, loc, JZ_SEVERITY_ERROR,
                                     "RPT_COUNT_INVALID",
                                     "RPT-001 @repeat count must be a positive integer");
                return 1;
            }

            /* Skip to end of line (the body starts on the next line or after whitespace) */
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '\n') p++;

            /* Find matching @end */
            const char *end_at = find_matching_end(p, src_end);
            if (!end_at) {
                JZLocation loc = { filename, count_line(full_src, repeat_at), 1 };
                jz_diagnostic_report(diagnostics, loc, JZ_SEVERITY_ERROR,
                                     "RPT_NO_MATCHING_END",
                                     "RPT-002 @repeat without matching @end");
                return 1;
            }

            const char *body_start = p;

            /* For each iteration, recursively expand nested @repeats,
             * then substitute IDX. */
            for (int i = 0; i < count; i++) {
                /* First, recursively expand any nested @repeat in the body */
                StrBuf nested;
                sb_init(&nested);
                if (expand_region(body_start, end_at, full_src, filename,
                                  diagnostics, &nested) != 0) {
                    free(nested.data);
                    return 1;
                }

                /* Then substitute IDX in the expanded body */
                substitute_idx(out, nested.data, nested.len, i);
                free(nested.data);
            }

            /* Skip past @end */
            p = end_at + 4; /* skip "@end" */
            /* Skip trailing whitespace and newline */
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '\n') p++;

        } else {
            /* Regular character — just copy */
            sb_append(out, p, 1);
            p++;
        }
    }

    return 0;
}

/* ── Public API ─────────────────────────────────────────────────── */

char *jz_repeat_expand(const char *source,
                       const char *filename,
                       JZDiagnosticList *diagnostics)
{
    if (!source) return NULL;

    /* Quick check: if no @repeat in source, return a copy as-is */
    if (!strstr(source, "@repeat")) {
        return strdup(source);
    }

    size_t src_len = strlen(source);
    StrBuf out;
    sb_init(&out);

    if (expand_region(source, source + src_len, source, filename,
                      diagnostics, &out) != 0) {
        free(out.data);
        return NULL;
    }

    return out.data;
}
