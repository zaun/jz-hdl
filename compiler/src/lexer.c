#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "util.h"
#include "rules.h"

typedef struct LexerState {
    const char      *filename;
    const char      *src;
    size_t           len;
    size_t           pos;
    int              line;
    int              column;
    JZTokenStream   *out;
    JZDiagnosticList *diagnostics;
    int              had_error;
    size_t           last_token_end;
    int              last_token_valid;
} LexerState;

static int lexer_ensure_capacity(JZTokenStream *s) {
    if (s->count == s->capacity) {
        size_t new_cap = s->capacity ? s->capacity * 2 : 128;
        JZToken *new_tokens = (JZToken *)realloc(s->tokens, new_cap * sizeof(JZToken));
        if (!new_tokens) return -1;
        s->tokens = new_tokens;
        s->capacity = new_cap;
    }
    return 0;
}

static void lexer_report_parse_error(LexerState *st,
                                     JZLocation loc,
                                     const char *code,
                                     const char *message)
{
    /* Legacy helper for non-rule-based parse errors in the lexer. Prefer
     * lexer_report_rule for rule IDs defined in rules.c.
     */
    st->had_error = 1;
    if (!st->diagnostics) {
        return;
    }
    jz_diagnostic_report(st->diagnostics, loc, JZ_SEVERITY_ERROR, code, message);
}

static void emit_token(LexerState *st, JZTokenType type, const char *start, size_t length, JZLocation loc) {
    if (lexer_ensure_capacity(st->out) != 0) return;
    JZToken *t = &st->out->tokens[st->out->count++];
    memset(t, 0, sizeof(*t));
    t->type = type;
    t->loc = loc;
    if (start && length > 0) {
        char *lex = (char *)malloc(length + 1);
        if (lex) {
            memcpy(lex, start, length);
            lex[length] = '\0';
        }
        t->lexeme = lex;
    } else {
        t->lexeme = NULL;
    }

    if (type != JZ_TOK_EOF) {
        st->last_token_end = st->pos;
        st->last_token_valid = 1;
    }
}

static int is_identifier_start(int c) {
    return isalpha(c) || c == '_';
}

static int is_identifier_char(int c) {
    return isalnum(c) || c == '_';
}

/*
 * Helper to report a rule-based diagnostic from the lexer using JZRuleInfo.
 */
static void lexer_report_rule(LexerState *st,
                              JZLocation loc,
                              const char *rule_id,
                              const char *fallback_message)
{
    if (!st || !st->diagnostics || !rule_id) {
        return;
    }

    const JZRuleInfo *rule = jz_rule_lookup(rule_id);
    JZSeverity sev = JZ_SEVERITY_ERROR;

    if (rule) {
        switch (rule->mode) {
        case JZ_RULE_MODE_WRN:
            sev = JZ_SEVERITY_WARNING;
            break;
        case JZ_RULE_MODE_INF:
            sev = JZ_SEVERITY_NOTE;
            break;
        case JZ_RULE_MODE_ERR:
        default:
            sev = JZ_SEVERITY_ERROR;
            break;
        }
    }

    /* Store the caller's explanation as d->message so that --explain can
     * show it underneath the rule description on the main diagnostic line. */
    const char *msg = fallback_message ? fallback_message : rule_id;
    st->had_error = 1;
    jz_diagnostic_report(st->diagnostics, loc, sev, rule_id, msg);
}

/* Fine-grained error causes for sized numeric literal lexing. */
typedef enum JZNumericErrorCause {
    JZ_NUM_ERR_NONE = 0,
    JZ_NUM_ERR_UNDERSCORE_EDGES,
    JZ_NUM_ERR_INVALID_DIGIT,
    JZ_NUM_ERR_DECIMAL_HAS_XZ,
    JZ_NUM_ERR_OTHER
} JZNumericErrorCause;

static int parse_numeric_literal(const char *lexeme,
                                 size_t len,
                                 JZNumericInfo *out_num,
                                 JZNumericErrorCause *out_err)
{
    if (out_err) {
        *out_err = JZ_NUM_ERR_NONE;
    }
    memset(out_num, 0, sizeof(*out_num));

    /* Check for sized literal of the form <width>'<base><value>. */
    const char *quote = memchr(lexeme, '\'', len);
    if (!quote) {
        /* Plain integer (no base prefix) */
        return 0;
    }

    size_t width_len = (size_t)(quote - lexeme);
    if (width_len == 0) {
        /* Unsized literal like 'hFF is illegal; handled separately by caller. */
        if (out_err) *out_err = JZ_NUM_ERR_OTHER;
        return -1;
    }

    size_t pos = width_len + 1; /* skip width and '\'' */
    if (pos >= len) {
        /* Missing base/value. */
        if (out_err) *out_err = JZ_NUM_ERR_OTHER;
        return -1;
    }

    char base_ch = lexeme[pos++];
    JZNumericBase base = JZ_NUM_BASE_NONE;
    if (base_ch == 'b' || base_ch == 'B') base = JZ_NUM_BASE_BIN;
    else if (base_ch == 'd' || base_ch == 'D') base = JZ_NUM_BASE_DEC;
    else if (base_ch == 'h' || base_ch == 'H') base = JZ_NUM_BASE_HEX;
    else {
        if (out_err) *out_err = JZ_NUM_ERR_OTHER;
        return -1; /* invalid base */
    }

    if (pos >= len) {
        /* Empty value portion. */
        if (out_err) *out_err = JZ_NUM_ERR_OTHER;
        return -1;
    }

    size_t value_len = len - pos;
    const char *value = lexeme + pos;

    /* Enforce underscore placement rules: not first or last. */
    if (value[0] == '_' || value[value_len - 1] == '_') {
        if (out_err) *out_err = JZ_NUM_ERR_UNDERSCORE_EDGES;
        return -1;
    }

    /* Validate digits per base. */
    for (size_t i = 0; i < value_len; ++i) {
        char ch = value[i];
        if (ch == '_') {
            continue;
        }
        switch (base) {
        case JZ_NUM_BASE_BIN:
            if (!(ch == '0' || ch == '1' || ch == 'x' || ch == 'X' || ch == 'z' || ch == 'Z')) {
                if (out_err) *out_err = JZ_NUM_ERR_INVALID_DIGIT;
                return -1;
            }
            break;
        case JZ_NUM_BASE_DEC:
            if (ch == 'x' || ch == 'X' || ch == 'z' || ch == 'Z') {
                if (out_err) *out_err = JZ_NUM_ERR_DECIMAL_HAS_XZ;
                return -1;
            }
            if (!(ch >= '0' && ch <= '9')) {
                if (out_err) *out_err = JZ_NUM_ERR_INVALID_DIGIT;
                return -1;
            }
            break;
    case JZ_NUM_BASE_HEX:
            if (!((ch >= '0' && ch <= '9') ||
                  (ch >= 'a' && ch <= 'f') ||
                  (ch >= 'A' && ch <= 'F'))) {
                if (out_err) *out_err = JZ_NUM_ERR_INVALID_DIGIT;
                return -1;
            }
            break;
        default:
            if (out_err) *out_err = JZ_NUM_ERR_OTHER;
            return -1;
        }
    }

    out_num->is_sized = 1;
    out_num->base = base;
    out_num->width_chars = width_len;
    return 1;
}

static int match_keyword(const char *lexeme, JZTokenType *out_type) {
    /* Directives (begin with '@') */
    if (strcmp(lexeme, "@module") == 0)   { *out_type = JZ_TOK_KW_MODULE;       return 1; }
    if (strcmp(lexeme, "@endmod") == 0)   { *out_type = JZ_TOK_KW_ENDMOD;       return 1; }
    if (strcmp(lexeme, "@project") == 0)  { *out_type = JZ_TOK_KW_PROJECT;      return 1; }
    if (strcmp(lexeme, "@endproj") == 0)  { *out_type = JZ_TOK_KW_ENDPROJ;      return 1; }
    if (strcmp(lexeme, "@blackbox") == 0) { *out_type = JZ_TOK_KW_BLACKBOX;     return 1; }
    if (strcmp(lexeme, "@new") == 0)      { *out_type = JZ_TOK_KW_NEW;          return 1; }
    if (strcmp(lexeme, "@top") == 0)      { *out_type = JZ_TOK_KW_TOP;          return 1; }
    if (strcmp(lexeme, "@import") == 0)   { *out_type = JZ_TOK_KW_IMPORT;       return 1; }
    if (strcmp(lexeme, "@global") == 0)   { *out_type = JZ_TOK_KW_GLOBAL;       return 1; }
    if (strcmp(lexeme, "@endglob") == 0)  { *out_type = JZ_TOK_KW_ENDGLOB;      return 1; }
    if (strcmp(lexeme, "@feature") == 0)  { *out_type = JZ_TOK_KW_FEATURE;      return 1; }
    if (strcmp(lexeme, "@endfeat") == 0)  { *out_type = JZ_TOK_KW_ENDFEAT;      return 1; }
    if (strcmp(lexeme, "@else") == 0)     { *out_type = JZ_TOK_KW_FEATURE_ELSE; return 1; }
    if (strcmp(lexeme, "@check") == 0)    { *out_type = JZ_TOK_KW_CHECK;        return 1; }
    if (strcmp(lexeme, "@template") == 0)    { *out_type = JZ_TOK_KW_TEMPLATE;    return 1; }
    if (strcmp(lexeme, "@endtemplate") == 0) { *out_type = JZ_TOK_KW_ENDTEMPLATE; return 1; }
    if (strcmp(lexeme, "@apply") == 0)       { *out_type = JZ_TOK_KW_APPLY;       return 1; }
    if (strcmp(lexeme, "@scratch") == 0)     { *out_type = JZ_TOK_KW_SCRATCH;     return 1; }
    if (strcmp(lexeme, "@testbench") == 0)   { *out_type = JZ_TOK_KW_TESTBENCH;   return 1; }
    if (strcmp(lexeme, "@endtb") == 0)       { *out_type = JZ_TOK_KW_ENDTB;       return 1; }
    if (strcmp(lexeme, "@setup") == 0)       { *out_type = JZ_TOK_KW_SETUP;       return 1; }
    if (strcmp(lexeme, "@update") == 0)      { *out_type = JZ_TOK_KW_UPDATE;      return 1; }
    if (strcmp(lexeme, "@clock") == 0)       { *out_type = JZ_TOK_KW_TB_CLOCK;    return 1; }
    if (strcmp(lexeme, "@expect_equal") == 0)     { *out_type = JZ_TOK_KW_EXPECT_EQ;  return 1; }
    if (strcmp(lexeme, "@expect_not_equal") == 0) { *out_type = JZ_TOK_KW_EXPECT_NEQ; return 1; }
    if (strcmp(lexeme, "@expect_tristate") == 0)  { *out_type = JZ_TOK_KW_EXPECT_TRI; return 1; }
    if (strcmp(lexeme, "@simulation") == 0)      { *out_type = JZ_TOK_KW_SIMULATION; return 1; }
    if (strcmp(lexeme, "@endsim") == 0)          { *out_type = JZ_TOK_KW_ENDSIM;     return 1; }
    if (strcmp(lexeme, "@run") == 0)             { *out_type = JZ_TOK_KW_RUN;        return 1; }
    if (strcmp(lexeme, "@run_until") == 0)       { *out_type = JZ_TOK_KW_RUN_UNTIL; return 1; }
    if (strcmp(lexeme, "@run_while") == 0)       { *out_type = JZ_TOK_KW_RUN_WHILE; return 1; }
    if (strcmp(lexeme, "@print") == 0)           { *out_type = JZ_TOK_KW_PRINT;    return 1; }
    if (strcmp(lexeme, "@print_if") == 0)        { *out_type = JZ_TOK_KW_PRINT_IF; return 1; }
    if (strcmp(lexeme, "@trace") == 0)           { *out_type = JZ_TOK_KW_TRACE;    return 1; }
    if (strcmp(lexeme, "@mark") == 0)            { *out_type = JZ_TOK_KW_MARK;     return 1; }
    if (strcmp(lexeme, "@mark_if") == 0)         { *out_type = JZ_TOK_KW_MARK_IF;  return 1; }
    if (strcmp(lexeme, "@alert") == 0)           { *out_type = JZ_TOK_KW_ALERT;    return 1; }
    if (strcmp(lexeme, "@alert_if") == 0)        { *out_type = JZ_TOK_KW_ALERT_IF; return 1; }

    /* Block and structural keywords */
    if (strcmp(lexeme, "CONST") == 0)         { *out_type = JZ_TOK_KW_CONST;      return 1; }
    if (strcmp(lexeme, "PORT") == 0)          { *out_type = JZ_TOK_KW_PORT;       return 1; }
    if (strcmp(lexeme, "WIRE") == 0)          { *out_type = JZ_TOK_KW_WIRE;       return 1; }
    if (strcmp(lexeme, "REGISTER") == 0)      { *out_type = JZ_TOK_KW_REGISTER;   return 1; }
    if (strcmp(lexeme, "LATCH") == 0)         { *out_type = JZ_TOK_KW_LATCH;      return 1; }
    if (strcmp(lexeme, "MEM") == 0)           { *out_type = JZ_TOK_KW_MEM;        return 1; }
    if (strcmp(lexeme, "MUX") == 0)           { *out_type = JZ_TOK_KW_MUX;        return 1; }
    if (strcmp(lexeme, "CDC") == 0)           { *out_type = JZ_TOK_KW_CDC;        return 1; }
    if (strcmp(lexeme, "ASYNCHRONOUS") == 0)  { *out_type = JZ_TOK_KW_ASYNC;      return 1; }
    if (strcmp(lexeme, "SYNCHRONOUS") == 0)   { *out_type = JZ_TOK_KW_SYNC;       return 1; }
    if (strcmp(lexeme, "CONFIG") == 0)        { *out_type = JZ_TOK_KW_CONFIG;     return 1; }
    if (strcmp(lexeme, "CLOCKS") == 0)        { *out_type = JZ_TOK_KW_CLOCKS;     return 1; }
    if (strcmp(lexeme, "IN_PINS") == 0)       { *out_type = JZ_TOK_KW_IN_PINS;    return 1; }
    if (strcmp(lexeme, "OUT_PINS") == 0)      { *out_type = JZ_TOK_KW_OUT_PINS;   return 1; }
    if (strcmp(lexeme, "INOUT_PINS") == 0)    { *out_type = JZ_TOK_KW_INOUT_PINS; return 1; }
    if (strcmp(lexeme, "MAP") == 0)           { *out_type = JZ_TOK_KW_MAP;        return 1; }
    if (strcmp(lexeme, "CLOCK_GEN") == 0)     { *out_type = JZ_TOK_KW_CLOCK_GEN;  return 1; }
    if (strcmp(lexeme, "TAP") == 0)            { *out_type = JZ_TOK_KW_TAP;        return 1; }

    /* Statement-level and direction/type keywords from the spec */
    if (strcmp(lexeme, "IF") == 0)       { *out_type = JZ_TOK_KW_IF;       return 1; }
    if (strcmp(lexeme, "ELIF") == 0)     { *out_type = JZ_TOK_KW_ELIF;     return 1; }
    if (strcmp(lexeme, "ELSE") == 0)     { *out_type = JZ_TOK_KW_ELSE;     return 1; }
    if (strcmp(lexeme, "SELECT") == 0)   { *out_type = JZ_TOK_KW_SELECT;   return 1; }
    if (strcmp(lexeme, "CASE") == 0)     { *out_type = JZ_TOK_KW_CASE;     return 1; }
    if (strcmp(lexeme, "DEFAULT") == 0)  { *out_type = JZ_TOK_KW_DEFAULT;  return 1; }
    if (strcmp(lexeme, "IN") == 0)       { *out_type = JZ_TOK_KW_IN;       return 1; }
    if (strcmp(lexeme, "OUT") == 0)      { *out_type = JZ_TOK_KW_OUT;      return 1; }
    if (strcmp(lexeme, "INOUT") == 0)    { *out_type = JZ_TOK_KW_INOUT;    return 1; }
    if (strcmp(lexeme, "OVERRIDE") == 0) { *out_type = JZ_TOK_KW_OVERRIDE; return 1; }

    /* Special semantic drivers */
    if (strcmp(lexeme, "GND") == 0) { *out_type = JZ_TOK_KW_GND; return 1; }
    if (strcmp(lexeme, "VCC") == 0) { *out_type = JZ_TOK_KW_VCC; return 1; }

    return 0;
}

static void skip_line_comment(LexerState *st) {
    while (st->pos < st->len) {
        char c = st->src[st->pos];
        if (c == '\n') {
            st->pos++;
            st->line++;
            st->column = 1;
            return;
        }
        st->pos++;
        st->column++;
    }
}

static int skip_block_comment(LexerState *st, JZLocation start_loc) {
    /* Block comments are non-nested; detect nested openings and unterminated blocks. */
    while (st->pos < st->len) {
        char c = st->src[st->pos++];
        if (c == '\n') {
            st->line++;
            st->column = 1;
        } else {
            st->column++;
        }
        if (c == '/' && st->pos < st->len && st->src[st->pos] == '*') {
            /* Nested block comments are not allowed by the spec. */
            JZLocation loc = start_loc;
            lexer_report_parse_error(st, loc, "COMMENT_NESTED_BLOCK",
                                     "nested block comment detected inside another block comment");
            return -1;
        }
        if (c == '*' && st->pos < st->len && st->src[st->pos] == '/') {
            st->pos++;
            st->column++;
            return 0;
        }
    }
    /* Unterminated block comment. */
    JZLocation loc = start_loc;
    lexer_report_parse_error(st, loc, "COMMENT_NESTED_BLOCK", "unterminated block comment");
    return -1;
}

static void lex_one_token(LexerState *st) {
    const char *src = st->src;

    while (st->pos < st->len) {
        char c = src[st->pos];
        if (c == ' ' || c == '\t' || c == '\r') {
            st->pos++;
            st->column++;
            continue;
        }
        if (c == '\n') {
            st->pos++;
            st->line++;
            st->column = 1;
            continue;
        }
        break;
    }

    if (st->pos >= st->len) {
        JZLocation loc = { st->filename, st->line, st->column };
        emit_token(st, JZ_TOK_EOF, NULL, 0, loc);
        return;
    }

    JZLocation loc = { st->filename, st->line, st->column };
    char c = src[st->pos];

    /* Comments */
    if (c == '/' && st->pos + 1 < st->len) {
        char n = src[st->pos + 1];
        if (n == '/' || n == '*') {
            int in_token = st->last_token_valid && (st->last_token_end == st->pos);
            if (in_token) {
                JZLocation loc_comment = loc;
                lexer_report_parse_error(st, loc_comment, "COMMENT_IN_TOKEN",
                                         "comment appears immediately adjacent to a token (inside token)");
            }
        }
        if (n == '/') {
            st->pos += 2;
            st->column += 2;
            skip_line_comment(st);
            if (!st->had_error) {
                lex_one_token(st);
            }
            return;
        } else if (n == '*') {
            JZLocation comment_loc = loc;
            st->pos += 2;
            st->column += 2;
            if (skip_block_comment(st, comment_loc) != 0) {
                /* Nested or unterminated block comment: error already reported. */
                return;
            }
            if (!st->had_error) {
                lex_one_token(st);
            }
            return;
        }
    }

    /* String literal with minimal escape support (\\, \", \n). */
    if (c == '"') {
        size_t content_start = st->pos + 1;
        int line = st->line;
        int col  = st->column;
        st->pos++;
        st->column++;
        while (st->pos < st->len && st->src[st->pos] != '"') {
            char ch = st->src[st->pos++];
            if (ch == '\n') {
                st->line++;
                st->column = 1;
            } else {
                st->column++;
            }
        }
        if (st->pos >= st->len) {
            /* Unterminated string literal. */
            return;
        }
        size_t content_end = st->pos;
        /* Consume closing quote. */
        st->pos++;
        st->column++;

        /* Decode escapes into a temporary buffer. */
        size_t raw_len = content_end - content_start;
        char *buf = (char *)malloc(raw_len + 1);
        if (!buf) return;
        size_t out_len = 0;
        for (size_t i = content_start; i < content_end; ++i) {
            char ch = src[i];
            if (ch == '\\' && i + 1 < content_end) {
                char n = src[i + 1];
                if (n == '\\' || n == '"') {
                    buf[out_len++] = n;
                    i++;
                    continue;
                } else if (n == 'n') {
                    buf[out_len++] = '\n';
                    i++;
                    continue;
                }
                /* Unknown escape: leave the backslash literal. */
            }
            buf[out_len++] = ch;
        }
        emit_token(st, JZ_TOK_STRING, buf, out_len, (JZLocation){ st->filename, line, col });
        free(buf);
        return;
    }

    /* Identifier, keyword, or directive (including @module, @project, etc. as single tokens) */
    if (is_identifier_start((unsigned char)c) || c == '@') {
        size_t start_pos = st->pos;
        int line = st->line;
        int col  = st->column;
        st->pos++;
        st->column++;
        while (st->pos < st->len && is_identifier_char((unsigned char)st->src[st->pos])) {
            st->pos++;
            st->column++;
        }
        size_t end_pos = st->pos;

        /*
         * Special case: identifier immediately followed by '\'' is treated
         * as the <width> part of a sized literal, e.g. WIDTH'b0001. The spec
         * (S2.1) allows a CONST/CONFIG name as the literal width, so we lex
         * the whole WIDTH'... sequence as a single JZ_TOK_SIZED_NUMBER token
         * instead of IDENT + stray quote (LIT_UNSIZED).
         */
        if (st->pos < st->len && st->src[st->pos] == '\'') {
            /* Scan through base/value portion similarly to the numeric-literal
             * branch used for digit-starting literals.
             */
            st->pos++;
            st->column++;
            while (st->pos < st->len &&
                   (isalnum((unsigned char)st->src[st->pos]) ||
                    st->src[st->pos] == '\'' ||
                    st->src[st->pos] == '_')) {
                st->pos++;
                st->column++;
            }
            size_t len = st->pos - start_pos;
            char *lex = (char *)malloc(len + 1);
            if (!lex) return;
            memcpy(lex, src + start_pos, len);
            lex[len] = '\0';

            JZNumericInfo info;
            JZNumericErrorCause err = JZ_NUM_ERR_NONE;
            int sized = parse_numeric_literal(lex, len, &info, &err);
            if (sized < 0) {
                /* Invalid sized literal; mirror the handling in the decimal
                 * numeric branch so we still attribute a useful rule ID.
                 * Allow multiple such diagnostics per file.
                 */
                const char *rule_id = NULL;
                const char *fallback = NULL;
                switch (err) {
                case JZ_NUM_ERR_UNDERSCORE_EDGES:
                    rule_id = "LIT_UNDERSCORE_AT_EDGES";
                    fallback = "literal has underscore as first or last character of value";
                    break;
                case JZ_NUM_ERR_DECIMAL_HAS_XZ:
                    rule_id = "LIT_DECIMAL_HAS_XZ";
                    fallback = "decimal literal may not contain x/z digits";
                    break;
                case JZ_NUM_ERR_INVALID_DIGIT:
                    rule_id = "LIT_INVALID_DIGIT_FOR_BASE";
                    fallback = "literal contains digit not allowed for its base";
                    break;
                case JZ_NUM_ERR_OTHER:
                default:
                    lexer_report_parse_error(st,
                                             (JZLocation){ st->filename, line, col },
                                             "LEX_NUMERIC",
                                             "invalid numeric literal");
                    break;
                }
                if (rule_id) {
                    lexer_report_rule(st,
                                      (JZLocation){ st->filename, line, col },
                                      rule_id,
                                      fallback);
                }
                free(lex);
                return;
            }

            JZTokenType ttype = sized > 0 ? JZ_TOK_SIZED_NUMBER : JZ_TOK_NUMBER;
            emit_token(st, ttype, lex, len, (JZLocation){ st->filename, line, col });
            if (sized > 0) {
                JZToken *tok = &st->out->tokens[st->out->count - 1];
                tok->num = info;
            }
            free(lex);
            return;
        }

        size_t len = end_pos - start_pos;
        char *lex = (char *)malloc(len + 1);
        if (!lex) return;
        memcpy(lex, src + start_pos, len);
        lex[len] = '\0';

        /* Enforce identifier rules (regex and max length) for non-directive identifiers.
         * Regex is handled lexically by is_identifier_start/char; all semantic uses of
         * '_' as no-connect vs. illegal identifier are enforced later over the AST.
         */
        (void)len;
        (void)line;
        (void)col;
        (void)st;

        JZTokenType tt;
        if (match_keyword(lex, &tt)) {
            emit_token(st, tt, lex, len, (JZLocation){ st->filename, line, col });
            free(lex); /* emit_token will copy if needed; we passed value only for keyword mapping */
        } else {
            emit_token(st, JZ_TOK_IDENTIFIER, lex, len, (JZLocation){ st->filename, line, col });
            free(lex);
        }
        return;
    }

    /* Disallow stray quote characters that would indicate unsized literals like 'hFF.
     *
     * IMPORTANT: we must still advance the lexer position after reporting the
     * error; otherwise, jz_lex_source() will repeatedly re-lex the same
     * character and hang in an infinite loop when encountering an unsized
     * literal. We conservatively consume just the quote here and let the
     * remaining characters be tokenized normally.
     */
    if (c == '\'') {
        /* Report as a rule-based literal error so validation can attribute it
         * to LIT_UNSIZED.
         */
        lexer_report_rule(st,
                          loc,
                          "LIT_UNSIZED",
                          "unsized literal (e.g. 'hFF) is not permitted");
        /* Consume the stray quote so that lexing can make forward progress. */
        st->pos++;
        st->column++;
        return;
    }

    /* Number literal: plain integers and sized bit-vector literals. */
    if (isdigit((unsigned char)c)) {
        size_t start_pos = st->pos;
        int line = st->line;
        int col  = st->column;
        st->pos++;
        st->column++;
        while (st->pos < st->len && (isalnum((unsigned char)st->src[st->pos]) || st->src[st->pos] == '\'' || st->src[st->pos] == '_')) {
            st->pos++;
            st->column++;
        }

        /* Check for a decimal point followed by a digit, making this a
         * floating-point literal (e.g., 5.0, 5.125).  Only allow this
         * for plain integers—not sized literals that contain a tick. */
        int is_float = 0;
        if (st->pos < st->len && st->src[st->pos] == '.' &&
            st->pos + 1 < st->len && isdigit((unsigned char)st->src[st->pos + 1])) {
            /* Verify no tick in what we scanned so far */
            int has_tick = 0;
            for (size_t ci = start_pos; ci < st->pos; ++ci) {
                if (src[ci] == '\'') { has_tick = 1; break; }
            }
            if (!has_tick) {
                is_float = 1;
                st->pos++;  /* consume '.' */
                st->column++;
                while (st->pos < st->len && isdigit((unsigned char)st->src[st->pos])) {
                    st->pos++;
                    st->column++;
                }
            }
        }

        size_t len = st->pos - start_pos;
        char *lex = (char *)malloc(len + 1);
        if (!lex) return;
        memcpy(lex, src + start_pos, len);
        lex[len] = '\0';

        if (is_float) {
            emit_token(st, JZ_TOK_NUMBER, lex, len, (JZLocation){ st->filename, line, col });
            free(lex);
            return;
        }

        JZNumericInfo info;
        JZNumericErrorCause err = JZ_NUM_ERR_NONE;
        int sized = parse_numeric_literal(lex, len, &info, &err);
        if (sized < 0) {
            /* Invalid numeric literal; map to specific LITERALS_AND_TYPES rules
             * where possible. We now allow multiple such diagnostics per file
             * instead of suppressing after the first.
             */
            const char *rule_id = NULL;
            const char *fallback = NULL;
            switch (err) {
            case JZ_NUM_ERR_UNDERSCORE_EDGES:
                rule_id = "LIT_UNDERSCORE_AT_EDGES";
                fallback = "literal has underscore as first or last character of value";
                break;
            case JZ_NUM_ERR_DECIMAL_HAS_XZ:
                rule_id = "LIT_DECIMAL_HAS_XZ";
                fallback = "decimal literal may not contain x/z digits";
                break;
            case JZ_NUM_ERR_INVALID_DIGIT:
                rule_id = "LIT_INVALID_DIGIT_FOR_BASE";
                fallback = "literal contains digit not allowed for its base";
                break;
            case JZ_NUM_ERR_OTHER:
            default:
                /* Fall back to a generic lexical error code when we cannot
                 * attribute a more specific rule ID.
                 */
                lexer_report_parse_error(st,
                                         (JZLocation){ st->filename, line, col },
                                         "LEX_NUMERIC",
                                         "invalid numeric literal");
                break;
            }
            if (rule_id) {
                lexer_report_rule(st,
                                  (JZLocation){ st->filename, line, col },
                                  rule_id,
                                  fallback);
            }
            free(lex);
            return;
        }

        JZTokenType ttype = sized > 0 ? JZ_TOK_SIZED_NUMBER : JZ_TOK_NUMBER;
        emit_token(st, ttype, lex, len, (JZLocation){ st->filename, line, col });
        if (sized > 0) {
            JZToken *t = &st->out->tokens[st->out->count - 1];
            t->num = info;
        }
        free(lex);
        return;
    }

    /* Operators and punctuation with longest-match rules. */
    switch (c) {
    case '{':
        st->pos++; st->column++;
        emit_token(st, JZ_TOK_LBRACE, &c, 1, loc);
        break;
    case '}':
        st->pos++; st->column++;
        emit_token(st, JZ_TOK_RBRACE, &c, 1, loc);
        break;
    case '(':
        st->pos++; st->column++;
        emit_token(st, JZ_TOK_LPAREN, &c, 1, loc);
        break;
    case ')':
        st->pos++; st->column++;
        emit_token(st, JZ_TOK_RPAREN, &c, 1, loc);
        break;
    case '[':
        st->pos++; st->column++;
        emit_token(st, JZ_TOK_LBRACKET, &c, 1, loc);
        break;
    case ']':
        st->pos++; st->column++;
        emit_token(st, JZ_TOK_RBRACKET, &c, 1, loc);
        break;
    case ';':
        st->pos++; st->column++;
        emit_token(st, JZ_TOK_SEMICOLON, &c, 1, loc);
        break;
    case ',':
        st->pos++; st->column++;
        emit_token(st, JZ_TOK_COMMA, &c, 1, loc);
        break;
    case '.':
        st->pos++; st->column++;
        emit_token(st, JZ_TOK_DOT, &c, 1, loc);
        break;
    case '@':
        st->pos++; st->column++;
        emit_token(st, JZ_TOK_AT, &c, 1, loc);
        break;

    case '=': {
        if (st->pos + 1 < st->len && src[st->pos + 1] == '=') {
            /* '==' */
            st->pos += 2; st->column += 2;
            const char *lex = "==";
            emit_token(st, JZ_TOK_OP_EQ, lex, 2, loc);
        } else if (st->pos + 1 < st->len && src[st->pos + 1] == '>') {
            /* '=>' or '=>z' / '=>s' */
            if (st->pos + 2 < st->len && (src[st->pos + 2] == 'z' || src[st->pos + 2] == 's') &&
                !(st->pos + 3 < st->len && (isalnum((unsigned char)src[st->pos + 3]) || src[st->pos + 3] == '_'))) {
                JZTokenType ttype = (src[st->pos + 2] == 'z') ? JZ_TOK_OP_DRIVE_Z : JZ_TOK_OP_DRIVE_S;
                st->pos += 3; st->column += 3;
                const char *lex = (ttype == JZ_TOK_OP_DRIVE_Z) ? "=>z" : "=>s";
                emit_token(st, ttype, lex, 3, loc);
            } else {
                st->pos += 2; st->column += 2;
                const char *lex = "=>";
                emit_token(st, JZ_TOK_OP_DRIVE, lex, 2, loc);
            }
        } else if (st->pos + 1 < st->len && (src[st->pos + 1] == 'z' || src[st->pos + 1] == 's') &&
                   !(st->pos + 2 < st->len && (isalnum((unsigned char)src[st->pos + 2]) || src[st->pos + 2] == '_'))) {
            /* =z or =s (only if not followed by identifier chars) */
            JZTokenType ttype = (src[st->pos + 1] == 'z') ? JZ_TOK_OP_ASSIGN_Z : JZ_TOK_OP_ASSIGN_S;
            st->pos += 2; st->column += 2;
            const char *lex = (ttype == JZ_TOK_OP_ASSIGN_Z) ? "=z" : "=s";
            emit_token(st, ttype, lex, 2, loc);
        } else {
            st->pos++; st->column++;
            emit_token(st, JZ_TOK_OP_ASSIGN, &c, 1, loc);
        }
        break;
    }

    case '+':
        st->pos++; st->column++;
        emit_token(st, JZ_TOK_OP_PLUS, &c, 1, loc);
        break;
    case '-':
        st->pos++; st->column++;
        emit_token(st, JZ_TOK_OP_MINUS, &c, 1, loc);
        break;
    case '*':
        st->pos++; st->column++;
        emit_token(st, JZ_TOK_OP_STAR, &c, 1, loc);
        break;
    case '%':
        st->pos++; st->column++;
        emit_token(st, JZ_TOK_OP_PERCENT, &c, 1, loc);
        break;
    case '~':
        st->pos++; st->column++;
        emit_token(st, JZ_TOK_OP_TILDE, &c, 1, loc);
        break;
    case '^':
        st->pos++; st->column++;
        emit_token(st, JZ_TOK_OP_CARET, &c, 1, loc);
        break;

    case '&':
        if (st->pos + 1 < st->len && src[st->pos + 1] == '&') {
            st->pos += 2; st->column += 2;
            const char *lex = "&&";
            emit_token(st, JZ_TOK_OP_AND_AND, lex, 2, loc);
        } else {
            st->pos++; st->column++;
            emit_token(st, JZ_TOK_OP_AMP, &c, 1, loc);
        }
        break;

    case '|':
        if (st->pos + 1 < st->len && src[st->pos + 1] == '|') {
            st->pos += 2; st->column += 2;
            const char *lex = "||";
            emit_token(st, JZ_TOK_OP_OR_OR, lex, 2, loc);
        } else {
            st->pos++; st->column++;
            emit_token(st, JZ_TOK_OP_PIPE, &c, 1, loc);
        }
        break;

    case '!':
        if (st->pos + 1 < st->len && src[st->pos + 1] == '=') {
            st->pos += 2; st->column += 2;
            const char *lex = "!=";
            emit_token(st, JZ_TOK_OP_NEQ, lex, 2, loc);
        } else {
            st->pos++; st->column++;
            emit_token(st, JZ_TOK_OP_BANG, &c, 1, loc);
        }
        break;

    case '<':
        if (st->pos + 1 < st->len && src[st->pos + 1] == '<') {
            /* '<<' */
            st->pos += 2; st->column += 2;
            const char *lex = "<<";
            emit_token(st, JZ_TOK_OP_SHL, lex, 2, loc);
        } else if (st->pos + 1 < st->len && src[st->pos + 1] == '=') {
            if (st->pos + 2 < st->len && (src[st->pos + 2] == 'z' || src[st->pos + 2] == 's')) {
                JZTokenType ttype = (src[st->pos + 2] == 'z') ? JZ_TOK_OP_LE_Z : JZ_TOK_OP_LE_S;
                st->pos += 3; st->column += 3;
                const char *lex = (ttype == JZ_TOK_OP_LE_Z) ? "<=z" : "<=s";
                emit_token(st, ttype, lex, 3, loc);
            } else {
                st->pos += 2; st->column += 2;
                const char *lex = "<=";
                emit_token(st, JZ_TOK_OP_LE, lex, 2, loc);
            }
        } else {
            st->pos++; st->column++;
            emit_token(st, JZ_TOK_OP_LT, &c, 1, loc);
        }
        break;

    case '>':
        if (st->pos + 2 < st->len && src[st->pos + 1] == '>' && src[st->pos + 2] == '>') {
            /* '>>>' arithmetic shift right */
            st->pos += 3; st->column += 3;
            const char *lex = ">>>";
            emit_token(st, JZ_TOK_OP_ASHR, lex, 3, loc);
        } else if (st->pos + 1 < st->len && src[st->pos + 1] == '>' ) {
            /* '>>' logical shift right */
            st->pos += 2; st->column += 2;
            const char *lex = ">>";
            emit_token(st, JZ_TOK_OP_SHR, lex, 2, loc);
        } else if (st->pos + 1 < st->len && src[st->pos + 1] == '=') {
            /* '>=' */
            st->pos += 2; st->column += 2;
            const char *lex = ">=";
            emit_token(st, JZ_TOK_OP_GE, lex, 2, loc);
        } else {
            st->pos++; st->column++;
            emit_token(st, JZ_TOK_OP_GT, &c, 1, loc);
        }
        break;

    case '?':
        st->pos++; st->column++;
        emit_token(st, JZ_TOK_OP_QUESTION, &c, 1, loc);
        break;
    case ':':
        st->pos++; st->column++;
        emit_token(st, JZ_TOK_OP_COLON, &c, 1, loc);
        break;

    case '/':
        /* At this point we've ruled out comments; treat as arithmetic divide. */
        st->pos++; st->column++;
        emit_token(st, JZ_TOK_OP_SLASH, &c, 1, loc);
        break;

    default:
        st->pos++; st->column++;
        emit_token(st, JZ_TOK_OTHER, &c, 1, loc);
        break;
    }
}

int jz_lex_source(const char *filename,
                  const char *source,
                  JZTokenStream *out_stream,
                  JZDiagnosticList *diagnostics) {
    memset(out_stream, 0, sizeof(*out_stream));

    LexerState st;
    st.filename = filename;
    st.src = source;
    st.len = strlen(source);
    st.pos = 0;
    st.line = 1;
    st.column = 1;
    st.out = out_stream;
    st.diagnostics = diagnostics;
    st.had_error = 0;
    st.last_token_end = 0;
    st.last_token_valid = 0;

    while (1) {
        lex_one_token(&st);
        if (st.out->count > 0 &&
            st.out->tokens[st.out->count - 1].type == JZ_TOK_EOF) {
            break;
        }
    }

    /* Propagate an overall failure status when any lexical error was
     * encountered, but only after we've scanned the entire source so that
     * multiple diagnostics (e.g. multiple bad literals) can be collected.
     */
    return st.had_error ? -1 : 0;
}

void jz_token_stream_free(JZTokenStream *stream) {
    if (!stream) return;
    for (size_t i = 0; i < stream->count; ++i) {
        free(stream->tokens[i].lexeme);
    }
    free(stream->tokens);
    stream->tokens = NULL;
    stream->count = 0;
    stream->capacity = 0;
}
