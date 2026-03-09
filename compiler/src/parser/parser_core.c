/**
 * @file parser_utils.c
 * @brief Core parser utility functions for token navigation and diagnostics.
 *
 * This file provides low-level helper routines used throughout the
 * recursive-descent parser, including token matching, lookahead,
 * error reporting, raw-text AST construction, and rule-based diagnostics.
 *
 * These functions form the foundation of the parser control flow and are
 * intentionally minimal and predictable to support precise error handling
 * and recovery.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser_internal.h"

/**
 * @brief Conditionally consume the next token if it matches a given type.
 *
 * If the current token matches @p type, it is consumed and the parser
 * advances. Otherwise, the parser state is unchanged.
 *
 * @param p     Active parser
 * @param type  Token type to match
 * @return 1 if the token matched and was consumed, 0 otherwise
 */
int match(Parser *p, JZTokenType type) {
    if (peek(p)->type == type) {
        advance(p);
        return 1;
    }
    return 0;
}

/**
 * @brief Peek at the current token without consuming it.
 *
 * If the parser position is beyond the end of the token stream, this
 * function always returns the final EOF token.
 *
 * @param p Active parser
 * @return Pointer to the current token (never NULL)
 */
const JZToken *peek(const Parser *p) {
    if (p->pos < p->count) return &p->tokens[p->pos];
    return &p->tokens[p->count - 1]; /* EOF */
}

/**
 * @brief Advance the parser to the next token.
 *
 * If already positioned at EOF, the parser remains at EOF.
 *
 * @param p Active parser
 * @return Pointer to the new current token
 */
const JZToken *advance(Parser *p) {
    if (p->pos < p->count) p->pos++;
    return peek(p);
}

/**
 * @brief Emit a generic parse error at the current token location.
 *
 * This function reports a non-rule-based parse error directly to stderr.
 * It is intended for unrecoverable syntax errors where rule-based
 * diagnostics are not applicable.
 *
 * @param p   Active parser
 * @param msg Human-readable error message
 */
void parser_error(const Parser *p, const char *msg) {
    const JZToken *t = peek(p);
    fprintf(stderr, "%s:%d:%d: parse error near token '%s': %s\n",
            t->loc.filename ? t->loc.filename : "<input>",
            t->loc.line, t->loc.column,
            t->lexeme ? t->lexeme : "<eof>",
            msg);
}

/**
 * @brief Construct a RAW_TEXT AST node from a range of tokens.
 *
 * Tokens in the half-open range [start, end) are concatenated by lexeme,
 * separated by spaces, and stored in the node's text field. This is used
 * for grammar regions that are intentionally parsed as opaque text and
 * interpreted later by semantic passes.
 *
 * @param p     Active parser
 * @param start Starting token index (inclusive)
 * @param end   Ending token index (exclusive)
 * @return Newly allocated RAW_TEXT AST node, or NULL on failure
 */
JZASTNode *make_raw_text_node(const Parser *p, size_t start, size_t end) {
    if (start >= end) {
        return NULL;
    }

    const JZToken *first = &p->tokens[start];
    JZLocation loc = first->loc;
    JZASTNode *node = jz_ast_new(JZ_AST_RAW_TEXT, loc);
    if (!node) return NULL;

    size_t buf_sz = 0;
    for (size_t i = start; i < end; ++i) {
        const JZToken *t = &p->tokens[i];
        if (t->lexeme) buf_sz += strlen(t->lexeme) + 1;
    }

    if (buf_sz > 0) {
        char *buf = (char *)malloc(buf_sz + 1);
        if (!buf) {
            jz_ast_free(node);
            return NULL;
        }
        buf[0] = '\0';
        for (size_t i = start; i < end; ++i) {
            const JZToken *t = &p->tokens[i];
            if (!t->lexeme) continue;
            strcat(buf, t->lexeme);
            strcat(buf, " ");
        }
        jz_ast_set_text(node, buf);
        free(buf);
    }

    return node;
}

/**
 * @brief Determine whether a token may act as an identifier in declarations.
 *
 * In declaration contexts (module names, CONST/PORT/WIRE/REGISTER/MEM names,
 * project names, etc.), reserved keywords such as IF, ELSE, or SELECT are
 * intentionally allowed to parse as identifiers. This enables semantic
 * analysis to emit KEYWORD_AS_IDENTIFIER diagnostics instead of the parser
 * failing with a generic syntax error.
 *
 * @param tok Token to test
 * @return 1 if the token is identifier-like, 0 otherwise
 */
int is_decl_identifier_token(const JZToken *tok) {
    if (!tok || !tok->lexeme) return 0;
    if (tok->type == JZ_TOK_IDENTIFIER) return 1;

    const char *name = tok->lexeme;

    /* Keep this list in sync with sem_is_reserved_keyword() in sem/driver.c. */
    if (!strcmp(name, "IF") || !strcmp(name, "ELIF") || !strcmp(name, "ELSE") ||
        !strcmp(name, "SELECT") || !strcmp(name, "CASE") || !strcmp(name, "DEFAULT") ||
        !strcmp(name, "CONST") || !strcmp(name, "PORT") || !strcmp(name, "REGISTER") ||
        !strcmp(name, "LATCH") ||
        !strcmp(name, "WIRE") || !strcmp(name, "MEM") || !strcmp(name, "MUX") ||
        !strcmp(name, "CDC") ||
        !strcmp(name, "IN") || !strcmp(name, "OUT") || !strcmp(name, "INOUT") ||
        !strcmp(name, "ASYNCHRONOUS") || !strcmp(name, "SYNCHRONOUS") ||
        !strcmp(name, "OVERRIDE") || !strcmp(name, "CONFIG") ||
        !strcmp(name, "CLOCKS") || !strcmp(name, "IN_PINS") ||
        !strcmp(name, "OUT_PINS") || !strcmp(name, "INOUT_PINS") ||
        !strcmp(name, "MAP") ||
        !strcmp(name, "IDX")) {
        return 1;
    }

    return 0;
}

/**
 * @brief Report a rule-based diagnostic associated with a specific token.
 *
 * This function looks up a rule by ID and emits a diagnostic using the
 * rule's severity and description if available. If the rule is not found,
 * the provided fallback message is used.
 *
 * If no diagnostic list is attached to the parser, this function does
 * nothing.
 *
 * @param p                Active parser
 * @param t                Token associated with the diagnostic
 * @param rule_id          Rule identifier string
 * @param fallback_message Message used if the rule has no description
 */
void parser_report_rule(const Parser *p,
                        const JZToken *t,
                        const char *rule_id,
                        const char *fallback_message)
{
    if (!p || !p->diagnostics || !t || !rule_id) return;

    const JZRuleInfo *rule = jz_rule_lookup(rule_id);
    JZSeverity sev = JZ_SEVERITY_ERROR;

    if (rule) {
        sev = (rule->mode == JZ_RULE_MODE_WRN) ? JZ_SEVERITY_WARNING : JZ_SEVERITY_ERROR;
    }

    /* Store the caller's explanation as d->message so that --explain can
     * show it underneath the rule description on the main diagnostic line. */
    const char *msg = fallback_message ? fallback_message : rule_id;
    jz_diagnostic_report(p->diagnostics, t->loc, sev, rule_id, msg);
}
