#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "sem.h"
#include "driver_internal.h"

/* -------------------------------------------------------------------------
 *  Literal typing and simple literal helpers
 * -------------------------------------------------------------------------
 */

void infer_literal_type(JZASTNode *node,
                        JZDiagnosticList *diagnostics,
                        JZBitvecType *out)
{
    if (!out) return;
    out->width = 0;
    out->is_signed = 0;
    if (!node || !node->text) return;

    const char *lex = node->text;
    const char *tick = strchr(lex, '\'');

    if (tick) {
        /* Sized literal: <width>'<base><value> */
        char width_buf[32];
        size_t width_len = (size_t)(tick - lex);
        if (width_len == 0 || width_len >= sizeof(width_buf)) {
            return; /* Defer complex width expressions to later passes. */
        }
        memcpy(width_buf, lex, width_len);
        width_buf[width_len] = '\0';

        unsigned declared_width = 0;
        if (!parse_simple_positive_int(width_buf, &declared_width)) {
            /* Width uses CONST/CONFIG or is otherwise non-simple; skip for now. */
            return;
        }

        char base_ch = tick[1];
        JZNumericBase base = JZ_NUM_BASE_NONE;
        if (base_ch == 'b' || base_ch == 'B') base = JZ_NUM_BASE_BIN;
        else if (base_ch == 'd' || base_ch == 'D') base = JZ_NUM_BASE_DEC;
        else if (base_ch == 'h' || base_ch == 'H') base = JZ_NUM_BASE_HEX;
        else return;

        const char *value_lexeme = tick + 2;
        if (!value_lexeme || !*value_lexeme) return;

        unsigned intrinsic = 0;
        JZLiteralExtKind ext = JZ_LITERAL_EXT_NONE;
        if (jz_literal_analyze(base,
                               value_lexeme,
                               declared_width,
                               &intrinsic,
                               &ext) != 0) {
            if (diagnostics) {
                sem_report_rule(diagnostics,
                                node->loc,
                                "LIT_OVERFLOW",
                                "literal numeric value exceeds declared width");
            }
            return;
        }

        jz_type_scalar(declared_width, 0, out);
        return;
    }

    /* Plain decimal integer literal: infer minimal width from its value. */
    unsigned intrinsic = 0;
    JZLiteralExtKind ext = JZ_LITERAL_EXT_NONE;
    if (jz_literal_analyze(JZ_NUM_BASE_DEC,
                           lex,
                           (unsigned)~0u,
                           &intrinsic,
                           &ext) != 0) {
        return;
    }
    jz_type_scalar(intrinsic, 0, out);
}

/* Simple literal-zero detector for DIV_* rules and dead-code analysis. */
int sem_literal_is_const_zero(const char *lex, int *out_known)
{
    if (out_known) *out_known = 0;
    if (!lex || !*lex) return 0;

    const char *tick = strchr(lex, '\'');
    if (!tick) {
        /* Plain decimal literal: allow digits and underscores. */
        int saw_digit = 0;
        for (const char *p = lex; *p; ++p) {
            char c = *p;
            if (c == '_') continue;
            if (c < '0' || c > '9') {
                return 0; /* not a simple integer literal */
            }
            saw_digit = 1;
            if (c != '0') {
                if (out_known) *out_known = 1;
                return 0; /* non-zero */
            }
        }
        if (!saw_digit) return 0;
        if (out_known) *out_known = 1;
        return 1; /* all digits were '0' */
    }

    /* Sized literal: <width>'<base><value>. Inspect value characters only. */
    const char *value = tick + 2; /* may be past end for malformed lexemes */
    if (!value || !*value) {
        return 0;
    }

    int saw_bit = 0;
    for (const char *p = value; *p; ++p) {
        char c = *p;
        if (c == '_' || c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            continue;
        }
        saw_bit = 1;
        if (c == 'x' || c == 'X' || c == 'z' || c == 'Z') {
            /* Contains unknown/high-impedance bits; not a definite zero. */
            return 0;
        }
        if (c != '0') {
            if (out_known) *out_known = 1;
            return 0; /* non-zero */
        }
    }

    if (!saw_bit) return 0;
    if (out_known) *out_known = 1;
    return 1;
}

/* -------------------------------------------------------------------------
 *  Literal width validation (LIT_* rules)
 * -------------------------------------------------------------------------
 */

/* Parse a very simple signed decimal integer (including zero) from a string
 * that may contain whitespace and an optional leading '+' or '-'. Returns 1 on
 * success, 0 on failure.
 */
static int sem_parse_simple_signed(const char *s, long long *out)
{
    if (!s || !out) return 0;

    const char *p = s;
    /* Skip leading whitespace. */
    while (*p && isspace((unsigned char)*p)) {
        ++p;
    }

    int sign = 1;
    if (*p == '+') {
        ++p;
    } else if (*p == '-') {
        sign = -1;
        ++p;
    }

    long long value = 0;
    int saw_digit = 0;

    for (; *p; ++p) {
        if (isspace((unsigned char)*p)) continue;
        if (*p < '0' || *p > '9') {
            return 0;
        }
        int d = (int)(*p - '0');
        if (value > (LLONG_MAX - d) / 10) {
            return 0; /* overflow */
        }
        value = value * 10 + d;
        saw_digit = 1;
    }

    if (!saw_digit) return 0;

    value *= sign;
    *out = value;
    return 1;
}

/* Check the <width> prefix of a sized literal lexeme and emit LIT_* width
 * diagnostics when we can safely reason about it.
 */
static void sem_check_literal_width(JZASTNode *lit,
                                    const JZModuleScope *mod_scope,
                                    const JZBuffer *project_symbols,
                                    JZDiagnosticList *diagnostics)
{
    if (!lit || !lit->text) return;

    const char *lex = lit->text;
    const char *tick = strchr(lex, '\'');
    if (!tick) {
        /* Plain decimal literal: width is inferred from value, no explicit
         * <width> prefix to validate here.
         */
        return;
    }

    /* Extract width substring before the '\''. */
    size_t width_len = (size_t)(tick - lex);
    if (width_len == 0) {
        /* Unsized literals should already be rejected by the lexer; emit a
         * rule-based error defensively if they slip through.
         */
        sem_report_rule(diagnostics,
                        lit->loc,
                        "LIT_UNSIZED",
                        "unsized literal is not permitted");
        return;
    }

    char buf[64];
    if (width_len >= sizeof(buf)) {
        /* Too complex to analyze safely (likely an expression); defer to
         * future constant-eval based passes.
         */
        return;
    }
    memcpy(buf, lex, width_len);
    buf[width_len] = '\0';

    /* Trim leading/trailing whitespace. */
    char *start = buf;
    while (*start && isspace((unsigned char)*start)) start++;
    char *end = start + strlen(start);
    while (end > start && isspace((unsigned char)end[-1])) {
        *--end = '\0';
    }
    if (*start == '\0') {
        return;
    }

    /* First, handle simple decimal widths (including an optional sign). */
    long long sw = 0;
    if (sem_parse_simple_signed(start, &sw)) {
        if (sw <= 0) {
            sem_report_rule(diagnostics,
                            lit->loc,
                            "LIT_WIDTH_NOT_POSITIVE",
                            "literal width must be a positive integer");
        }
        return;
    }

    /* If not purely decimal digits, treat as a potential CONST/CONFIG name
     * when it looks like a single identifier (or CONFIG.name). More complex
     * expressions are left to future constant-eval integration.
     */

    /* Check for CONFIG.name pattern first. */
    if (strncmp(start, "CONFIG.", 7) == 0 && start[7] != '\0') {
        const char *config_name = start + 7;
        /* Validate the name portion is a simple identifier. */
        const char *cp = config_name;
        if (!((*cp >= 'A' && *cp <= 'Z') || (*cp >= 'a' && *cp <= 'z') || *cp == '_')) {
            goto not_found;
        }
        for (; *cp; ++cp) {
            if (!((*cp >= 'A' && *cp <= 'Z') || (*cp >= 'a' && *cp <= 'z') ||
                  (*cp >= '0' && *cp <= '9') || *cp == '_')) {
                goto not_found;
            }
        }
        if (project_symbols && project_symbols->data) {
            const JZSymbol *syms = (const JZSymbol *)project_symbols->data;
            size_t count = project_symbols->len / sizeof(JZSymbol);
            for (size_t i = 0; i < count; ++i) {
                if (syms[i].kind == JZ_SYM_CONFIG && syms[i].name &&
                    strcmp(syms[i].name, config_name) == 0) {
                    return;
                }
            }
        }
        goto not_found;
    }

    const char *p = start;
    if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || *p == '_')) {
        return; /* not an identifier-like width */
    }
    for (; *p; ++p) {
        if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
              (*p >= '0' && *p <= '9') || *p == '_')) {
            return; /* complex expression, skip for now */
        }
    }

    /* Look for a matching CONST in the current module scope. */
    if (mod_scope) {
        const JZSymbol *sym = module_scope_lookup_kind(mod_scope, start, JZ_SYM_CONST);
        if (sym) {
            return; /* defined CONST; its value will be checked elsewhere. */
        }
    }

    /* Also treat project-level CONFIG names as valid width references. */
    if (project_symbols && project_symbols->data) {
        const JZSymbol *syms = (const JZSymbol *)project_symbols->data;
        size_t count = project_symbols->len / sizeof(JZSymbol);
        for (size_t i = 0; i < count; ++i) {
            if (syms[i].kind == JZ_SYM_CONFIG && syms[i].name &&
                strcmp(syms[i].name, start) == 0) {
                return;
            }
        }
    }

not_found:
    /* No matching CONST/CONFIG name found. */
    sem_report_rule(diagnostics,
                    lit->loc,
                    "LIT_UNDEFINED_CONST_WIDTH",
                    "literal width uses undefined CONST or CONFIG name");
}

static void sem_check_literal_widths_recursive(JZASTNode *node,
                                               const JZModuleScope *mod_scope,
                                               const JZBuffer *project_symbols,
                                               JZDiagnosticList *diagnostics)
{
    if (!node) return;

    if (node->type == JZ_AST_EXPR_LITERAL) {
        sem_check_literal_width(node, mod_scope, project_symbols, diagnostics);
    }

    for (size_t i = 0; i < node->child_count; ++i) {
        sem_check_literal_widths_recursive(node->children[i], mod_scope, project_symbols, diagnostics);
    }
}

void sem_check_module_literal_widths(const JZModuleScope *scope,
                                     const JZBuffer *project_symbols,
                                     JZDiagnosticList *diagnostics)
{
    if (!scope || !scope->node) return;
    sem_check_literal_widths_recursive(scope->node, scope, project_symbols, diagnostics);
}
