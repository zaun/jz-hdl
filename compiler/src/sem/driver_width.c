#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <limits.h>

#include "sem_driver.h"
#include "sem.h"
#include "util.h"
#include "rules.h"
#include "driver_internal.h"

/* Forward declaration for internal recursive helper. */
static int sem_eval_width_expr_internal(const char *expr,
                                        const JZModuleScope *scope,
                                        const JZBuffer *project_symbols,
                                        unsigned *out_width,
                                        int depth);

int sem_instance_width_expr_is_invalid(const char *expr,
                                              const JZModuleScope *parent_scope,
                                              const JZBuffer *project_symbols)
{
    if (!expr) return 0;

    /* Digits-only but invalid (<= 0 / overflow). */
    unsigned tmp = 0;
    int rc = eval_simple_positive_decl_int(expr, &tmp);
    if (rc == -1) {
        return 1;
    }
    if (rc == 1) {
        /* Simple positive integer literal. */
        return 0;
    }

    /* Single identifier that must resolve either as a module CONST or a
     * project CONFIG entry.
     */
    char ident[64];
    if (sem_extract_identifier_like(expr, ident, sizeof(ident))) {
        const JZSymbol *c_sym = parent_scope
                              ? module_scope_lookup_kind(parent_scope, ident, JZ_SYM_CONST)
                              : NULL;
        const JZSymbol *cfg_sym = NULL;
        if (project_symbols && project_symbols->data) {
            const JZSymbol *syms = (const JZSymbol *)project_symbols->data;
            size_t count = project_symbols->len / sizeof(JZSymbol);
            for (size_t i = 0; i < count; ++i) {
                if (syms[i].kind == JZ_SYM_CONFIG && syms[i].name &&
                    strcmp(syms[i].name, ident) == 0) {
                    cfg_sym = &syms[i];
                    break;
                }
            }
        }
        if (!c_sym && !cfg_sym) {
            return 1;
        }
    }

    /* CONFIG.<name> references: scan token stream for CONFIG '.' name triples
     * and ensure each <name> exists as a CONFIG symbol.
     */
    if (project_symbols) {
        const char *p = expr;
        int expecting_dot = 0;
        int expecting_name = 0;
        char token[128];

        while (*p) {
            /* Skip whitespace. */
            while (*p && isspace((unsigned char)*p)) {
                ++p;
            }
            if (!*p) break;

            const char *start = p;
            while (*p && !isspace((unsigned char)*p)) {
                ++p;
            }
            size_t len = (size_t)(p - start);
            if (len >= sizeof(token)) len = sizeof(token) - 1;
            memcpy(token, start, len);
            token[len] = '\0';

            if (expecting_name) {
                expecting_name = 0;
                if (token[0] != '\0') {
                    const JZSymbol *cfg = NULL;
                    if (project_symbols && project_symbols->data) {
                        const JZSymbol *syms = (const JZSymbol *)project_symbols->data;
                        size_t count = project_symbols->len / sizeof(JZSymbol);
                        for (size_t i = 0; i < count; ++i) {
                            if (syms[i].kind == JZ_SYM_CONFIG && syms[i].name &&
                                strcmp(syms[i].name, token) == 0) {
                                cfg = &syms[i];
                                break;
                            }
                        }
                    }
                    if (!cfg) {
                        return 1;
                    }
                }
                continue;
            }

            if (expecting_dot) {
                expecting_dot = 0;
                if (strcmp(token, ".") == 0) {
                    expecting_name = 1;
                }
                continue;
            }

            if (strcmp(token, "CONFIG") == 0) {
                expecting_dot = 1;
                continue;
            }
        }
    }

    return 0;
}

/*
 * Scan a raw width expression text for CONFIG.<name> references and emit
 * CONFIG_USE_UNDECLARED when any referenced <name> does not exist in the
 * project CONFIG symbol table.
 */

void sem_check_undeclared_config_in_width(const char *expr,
                                          JZLocation loc,
                                          const JZBuffer *project_symbols,
                                          JZDiagnosticList *diagnostics)
{
    if (!expr || !project_symbols || !project_symbols->data || !diagnostics) {
        return;
    }

    const JZSymbol *syms = (const JZSymbol *)project_symbols->data;
    size_t count = project_symbols->len / sizeof(JZSymbol);

    const char *p = expr;
    int expecting_dot = 0;
    int expecting_name = 0;
    char token[128];

    while (*p) {
        /* Skip whitespace. */
        while (*p && isspace((unsigned char)*p)) {
            ++p;
        }
        if (!*p) break;

        const char *start = p;
        while (*p && !isspace((unsigned char)*p)) {
            ++p;
        }
        size_t len = (size_t)(p - start);
        if (len >= sizeof(token)) len = sizeof(token) - 1;
        memcpy(token, start, len);
        token[len] = '\0';

        if (expecting_name) {
            expecting_name = 0;
            if (token[0] != '\0') {
                int found = 0;
                int is_string = 0;
                for (size_t i = 0; i < count; ++i) {
                    if (syms[i].kind == JZ_SYM_CONFIG && syms[i].name &&
                        strcmp(syms[i].name, token) == 0) {
                        found = 1;
                        if (syms[i].node && syms[i].node->block_kind &&
                            strcmp(syms[i].node->block_kind, "STRING") == 0) {
                            is_string = 1;
                        }
                        break;
                    }
                }
                if (found && is_string) {
                    sem_report_rule(diagnostics,
                                    loc,
                                    "CONST_STRING_IN_NUMERIC_CONTEXT",
                                    "string CONFIG value used where a numeric expression is expected");
                } else if (!found) {
                    sem_report_rule(diagnostics,
                                    loc,
                                    "CONFIG_USE_UNDECLARED",
                                    "Use of CONFIG.<name> not declared in project CONFIG");
                }
            }
            continue;
        }

        if (expecting_dot) {
            expecting_dot = 0;
            if (strcmp(token, ".") == 0) {
                expecting_name = 1;
            } else {
                /* Malformed CONFIG reference like "CONFIG x"; treat as
                 * undeclared CONFIG.<name> as well.
                 */
                sem_report_rule(diagnostics,
                                loc,
                                "CONFIG_USE_UNDECLARED",
                                "Use of CONFIG.<name> not declared in project CONFIG");
            }
            continue;
        }

        if (strcmp(token, "CONFIG") == 0) {
            expecting_dot = 1;
            continue;
        }
    }
}



/*
 * Scan a CONST initializer text for identifier-based literal widths of the
 * form "NAME'..." where NAME is not a known CONST or CONFIG.
 *
 * In such cases, the literal-width pass (sem_check_literal_width) will emit
 * LIT_UNDEFINED_CONST_WIDTH based on the AST. If we attempted to
 * constant-evaluate these expressions here, the generic constant-eval
 * failure would be reported as CONST_NEGATIVE_OR_NONINT on the CONST, which
 * is misleading and duplicates the more precise literal-width diagnostic.
 *
 * Returns 1 if such an undefined-width identifier is found, 0 otherwise.
 */
int sem_expr_has_undefined_width_ident(const char *expr_text,
                                              const JZModuleScope *scope,
                                              const JZBuffer *project_symbols)
{
    if (!expr_text) return 0;

    const char *p = expr_text;
    while (*p) {
        /* Find start of next identifier candidate. */
        while (*p && !((*p >= 'A' && *p <= 'Z') ||
                       (*p >= 'a' && *p <= 'z') ||
                       *p == '_')) {
            ++p;
        }
        if (!*p) break;

        const char *start = p;
        ++p;
        while (*p && ((*p >= 'A' && *p <= 'Z') ||
                      (*p >= 'a' && *p <= 'z') ||
                      (*p >= '0' && *p <= '9') ||
                      *p == '_')) {
            ++p;
        }

        /* Optional whitespace between identifier and '\'' is allowed. */
        const char *q = p;
        while (*q && isspace((unsigned char)*q)) {
            ++q;
        }
        if (*q != '\'') {
            /* Not a width prefix; continue scanning from current position. */
            continue;
        }

        /* We have NAME'... candidate: extract NAME. */
        size_t len = (size_t)(p - start);
        if (len == 0) {
            continue;
        }

        char name_buf[64];
        if (len >= sizeof(name_buf)) {
            /* Too long to be a simple CONST/CONFIG name; skip. */
            continue;
        }
        memcpy(name_buf, start, len);
        name_buf[len] = '\0';

        /* If NAME matches a module-level CONST, it's fine. */
        if (scope) {
            const JZSymbol *sym = module_scope_lookup_kind(scope, name_buf, JZ_SYM_CONST);
            if (sym) {
                continue;
            }
        }

        /* If NAME matches a project-level CONFIG, it's also fine. */
        if (project_symbols && project_symbols->data) {
            const JZSymbol *syms = (const JZSymbol *)project_symbols->data;
            size_t count = project_symbols->len / sizeof(JZSymbol);
            int found_config = 0;
            for (size_t i = 0; i < count; ++i) {
                if (syms[i].kind == JZ_SYM_CONFIG && syms[i].name &&
                    strcmp(syms[i].name, name_buf) == 0) {
                    found_config = 1;
                    break;
                }
            }
            if (found_config) {
                continue;
            }
        }

        /* NAME'... where NAME is neither CONST nor CONFIG: let the
         * literal-width pass own this diagnostic as LIT_UNDEFINED_CONST_WIDTH
         * instead of emitting CONST_NEGATIVE_OR_NONINT here.
         */
        return 1;
    }

    return 0;
}

/*
 * Detect simple sized literals in an expression where the width is a
 * non-positive simple decimal constant (e.g. 0'h0, -1'hFF). We do a
 * text-based scan for "<width>'<base><value>" patterns and parse the
 * <width> portion as a signed decimal.
 */
int sem_expr_has_nonpositive_simple_width_literal(const char *expr_text)
{
    if (!expr_text) return 0;

    const char *p = expr_text;
    while ((p = strchr(p, '\'')) != NULL) {
        const char *tick = p; /* points at '\'' */

        /* Walk backwards over optional whitespace and digits/sign to
         * reconstruct the width token immediately preceding the tick.
         */
        const char *end = tick;
        /* Skip whitespace before the tick. */
        while (end > expr_text && isspace((unsigned char)end[-1])) {
            --end;
        }
        const char *start = end;
        while (start > expr_text) {
            char c = start[-1];
            if ((c >= '0' && c <= '9') || c == '+' || c == '-') {
                --start;
                continue;
            }
            if (isspace((unsigned char)c)) {
                --start;
                continue;
            }
            break;
        }

        if (end > start) {
            char buf[64];
            size_t len = (size_t)(end - start);
            if (len > 0 && len < sizeof(buf)) {
                memcpy(buf, start, len);
                buf[len] = '\0';

                /* Parse as simple signed decimal. */
                long long sw = 0;
                const char *q = buf;
                while (*q && isspace((unsigned char)*q)) ++q;
                int sign = 1;
                if (*q == '+') {
                    ++q;
                } else if (*q == '-') {
                    sign = -1;
                    ++q;
                }
                int saw_digit = 0;
                long long val = 0;
                for (; *q; ++q) {
                    if (isspace((unsigned char)*q)) continue;
                    if (*q < '0' || *q > '9') {
                        saw_digit = 0;
                        break;
                    }
                    int d = (int)(*q - '0');
                    if (val > (LLONG_MAX - d) / 10) {
                        saw_digit = 0;
                        break;
                    }
                    val = val * 10 + d;
                    saw_digit = 1;
                }
                if (saw_digit) {
                    sw = val * sign;
                    if (sw <= 0) {
                        return 1;
                    }
                }
            }
        }

        /* Move past this tick for the next search. */
        ++p;
    }

    return 0;
}

int sem_expand_widthof_in_width_expr(const char *expr,
                                     const JZModuleScope *scope,
                                     const JZBuffer *project_symbols,
                                     char **out_expanded,
                                     int depth)
{
    /* project_symbols is used below for BUS signal CONFIG width resolution */
    if (!expr || !scope || !out_expanded) {
        return -1;
    }

    *out_expanded = NULL;

    const char *p = expr;
    int saw_widthof = 0;

    while (*p) {
        if (*p == 'w' && strncmp(p, "widthof", 7) == 0) {
            saw_widthof = 1;
            break;
        }
        ++p;
    }

    if (!saw_widthof) {
        return 0; /* no expansion needed */
    }

    size_t expr_len = strlen(expr);
    /* Allocate a buffer somewhat larger than the original to accommodate
     * decimal expansions of widths.
     */
    size_t cap = expr_len * 4u + 64u;
    char *buf = (char *)malloc(cap);
    if (!buf) {
        return -1;
    }
    size_t out_len = 0;

    p = expr;
    while (*p) {
        if (*p == 'w' && strncmp(p, "widthof", 7) == 0) {
            const char *start = p; /* for error recovery if needed */
            p += 7; /* consume "widthof" */

            /* Skip optional whitespace. */
            while (*p && isspace((unsigned char)*p)) {
                ++p;
            }
            if (*p != '(') {
                /* Not actually a well-formed widthof-call; treat "widthof" as
                 * ordinary identifier text and continue copying.
                 */
                p = start;
            } else {
                ++p; /* consume '(' */
                while (*p && isspace((unsigned char)*p)) {
                    ++p;
                }
                /* Parse identifier argument. */
                const char *id_start = p;
                if (!((*p >= 'A' && *p <= 'Z') ||
                      (*p >= 'a' && *p <= 'z') ||
                      *p == '_')) {
                    free(buf);
                    return -1;
                }
                ++p;
                while ((*p >= 'A' && *p <= 'Z') ||
                       (*p >= 'a' && *p <= 'z') ||
                       (*p >= '0' && *p <= '9') ||
                       *p == '_') {
                    ++p;
                }
                const char *id_end = p;
                while (*p && isspace((unsigned char)*p)) {
                    ++p;
                }
                if (*p != ')') {
                    free(buf);
                    return -1;
                }
                ++p; /* consume ')' */

                size_t id_len = (size_t)(id_end - id_start);
                char ident[256];
                if (id_len >= sizeof(ident)) {
                    id_len = sizeof(ident) - 1u;
                }
                memcpy(ident, id_start, id_len);
                ident[id_len] = '\0';

                /* Look up the identifier first in the module scope (WIRE/REGISTER)
                 * and, if not found there, as a BUS definition in the project
                 * symbol table. BUS support replaces the legacy bwidth()
                 * intrinsic.
                 */
                const JZSymbol *sym = module_scope_lookup_kind(scope, ident, JZ_SYM_WIRE);
                if (!sym) {
                    sym = module_scope_lookup_kind(scope, ident, JZ_SYM_REGISTER);
                }

                unsigned target_width = 0u;
                if (sym && sym->node && sym->node->width) {
                    if (sem_eval_width_expr_internal(sym->node->width,
                                                     scope,
                                                     project_symbols,
                                                     &target_width,
                                                     depth + 1) != 0) {
                        free(buf);
                        return -1;
                    }
                } else {
                    /* Fall back to BUS definition lookup: widthof(<bus_id>) is
                     * defined as the sum of member signal widths in the BUS
                     * block. This currently requires each BUS signal width to
                     * be a simple positive integer literal, matching the
                     * previous bwidth() behavior.
                     */
                    if (!project_symbols || !project_symbols->data) {
                        free(buf);
                        return -1;
                    }

                    const JZSymbol *syms = (const JZSymbol *)project_symbols->data;
                    size_t sym_count = project_symbols->len / sizeof(JZSymbol);
                    const JZSymbol *bus_sym = NULL;
                    for (size_t i = 0; i < sym_count; ++i) {
                        if (syms[i].kind == JZ_SYM_BUS && syms[i].name &&
                            strcmp(syms[i].name, ident) == 0) {
                            bus_sym = &syms[i];
                            break;
                        }
                    }
                    if (!bus_sym || !bus_sym->node || bus_sym->node->type != JZ_AST_BUS_BLOCK) {
                        free(buf);
                        return -1;
                    }

                    JZASTNode *bus = bus_sym->node;
                    for (size_t bi = 0; bi < bus->child_count; ++bi) {
                        JZASTNode *decl = bus->children[bi];
                        if (!decl || decl->type != JZ_AST_BUS_DECL || !decl->width) {
                            continue;
                        }
                        unsigned w = 0u;
                        int rc = eval_simple_positive_decl_int(decl->width, &w);
                        if (rc != 1 || w == 0u) {
                            /* Fall back to const-expr evaluation for CONFIG-based
                             * widths (e.g., CONFIG.XLEN in BUS signal decls). */
                            long long cval = 0;
                            if (sem_eval_const_expr_in_project(decl->width,
                                                               project_symbols,
                                                               &cval) == 0 &&
                                cval > 0) {
                                w = (unsigned)cval;
                            } else {
                                free(buf);
                                return -1;
                            }
                        }
                        target_width += w;
                    }
                }

                char num[32];
                snprintf(num, sizeof(num), "%u", target_width);
                size_t num_len = strlen(num);
                if (out_len + num_len + 1 >= cap) {
                    size_t new_cap = cap * 2u + num_len + 32u;
                    char *new_buf = (char *)realloc(buf, new_cap);
                    if (!new_buf) {
                        free(buf);
                        return -1;
                    }
                    buf = new_buf;
                    cap = new_cap;
                }
                memcpy(buf + out_len, num, num_len);
                out_len += num_len;
                continue;
            }
        }

        /* Default: copy one character. */
        if (out_len + 2 >= cap) {
            size_t new_cap = cap * 2u + 32u;
            char *new_buf = (char *)realloc(buf, new_cap);
            if (!new_buf) {
                free(buf);
                return -1;
            }
            buf = new_buf;
            cap = new_cap;
        }
        buf[out_len++] = *p++;
    }

    buf[out_len] = '\0';
    *out_expanded = buf;
    return 0;
}


static int sem_eval_width_expr_internal(const char *expr,
                                        const JZModuleScope *scope,
                                        const JZBuffer *project_symbols,
                                        unsigned *out_width,
                                        int depth)
{
    if (!expr || !scope || !out_width) {
        return -1;
    }
    if (depth > 16) {
        /* Prevent unbounded recursion in pathological widthof() cycles. */
        return -1;
    }

    char *expanded = NULL;
    if (sem_expand_widthof_in_width_expr(expr,
                                         scope,
                                         project_symbols,
                                         &expanded,
                                         depth) != 0) {
        if (expanded) free(expanded);
        return -1;
    }

    const char *to_eval = expanded ? expanded : expr;
    long long v = 0;
    if (sem_eval_const_expr_in_module(to_eval, scope, project_symbols, &v) != 0) {
        if (expanded) free(expanded);
        return -1;
    }
    if (expanded) {
        free(expanded);
    }
    if (v <= 0) {
        return -1;
    }

    *out_width = (unsigned)v;
    return 0;
}

int sem_eval_width_expr(const char *expr,
                        const JZModuleScope *scope,
                        const JZBuffer *project_symbols,
                        unsigned *out_width)
{
    return sem_eval_width_expr_internal(expr, scope, project_symbols, out_width, 0);
}

void sem_check_module_decl_widths(const JZModuleScope *scope,
                                         const JZBuffer *project_symbols,
                                         JZDiagnosticList *diagnostics)
{
    if (!scope || !scope->node) return;

    JZASTNode *mod = scope->node;

    /* First, enforce module-level PORT presence and direction mix rules.
     *
     * - MODULE_MISSING_PORT (error): no PORT block at all, or PORT block present
     *   but declares zero ports.
     * - MODULE_PORT_IN_ONLY (warning): PORT block exists and all declared ports
     *   are IN (no OUT or INOUT ports).
     */
    int has_port_block = 0;
    int port_count = 0;
    int has_out_or_inout = 0;

    for (size_t i = 0; i < mod->child_count; ++i) {
        JZASTNode *blk = mod->children[i];
        if (!blk || blk->type != JZ_AST_PORT_BLOCK) continue;
        has_port_block = 1;

        for (size_t j = 0; j < blk->child_count; ++j) {
            JZASTNode *decl = blk->children[j];
            if (!decl || decl->type != JZ_AST_PORT_DECL) continue;
            ++port_count;
            if (decl->block_kind &&
                (strcmp(decl->block_kind, "OUT") == 0 ||
                 strcmp(decl->block_kind, "INOUT") == 0)) {
                has_out_or_inout = 1;
            } else if (decl->block_kind &&
                       strcmp(decl->block_kind, "BUS") == 0 &&
                       sem_bus_port_has_writable_signal(decl, project_symbols)) {
                has_out_or_inout = 1;
            }
        }
    }

    if (!has_port_block || port_count == 0) {
        /* Use module location if we have it; otherwise, fall back to first
         * child PORT block (if any were seen but were empty).
         */
        JZLocation loc = mod->loc;
        if (has_port_block && mod->child_count > 0) {
            for (size_t i = 0; i < mod->child_count; ++i) {
                JZASTNode *blk = mod->children[i];
                if (blk && blk->type == JZ_AST_PORT_BLOCK) {
                    loc = blk->loc;
                    break;
                }
            }
        }
        sem_report_rule(diagnostics,
                        loc,
                        "MODULE_MISSING_PORT",
                        "module must declare at least one PORT; missing or empty PORT block");
    } else if (!has_out_or_inout) {
        sem_report_rule(diagnostics,
                        mod->loc,
                        "MODULE_PORT_IN_ONLY",
                        "module declares only IN ports (no OUT/INOUT); may be dead logic");
    }

    /* Then perform per-declaration width checks for PORT/WIRE/REGISTER blocks. */
    for (size_t i = 0; i < mod->child_count; ++i) {
        JZASTNode *blk = mod->children[i];
        if (!blk) continue;

        if (blk->type == JZ_AST_PORT_BLOCK ||
            blk->type == JZ_AST_WIRE_BLOCK ||
            blk->type == JZ_AST_REGISTER_BLOCK ||
            blk->type == JZ_AST_LATCH_BLOCK) {
            for (size_t j = 0; j < blk->child_count; ++j) {
                JZASTNode *decl = blk->children[j];
                if (!decl) continue;
                if (decl->type != JZ_AST_PORT_DECL &&
                    decl->type != JZ_AST_WIRE_DECL &&
                    decl->type != JZ_AST_REGISTER_DECL &&
                    decl->type != JZ_AST_LATCH_DECL) {
                    continue;
                }

                /* BUS ports: validate BUS id and role (SOURCE/TARGET). */
                if (decl->type == JZ_AST_PORT_DECL &&
                    decl->block_kind && strcmp(decl->block_kind, "BUS") == 0) {
                    const char *meta = decl->text ? decl->text : "";
                    char bus_name[128] = {0};
                    char role_name[128] = {0};
                    if (meta && *meta) {
                        /* meta format: "<bus_id> <ROLE>" */
                        sscanf(meta, "%127s %127s", bus_name, role_name);
                    }

                    if (bus_name[0] == '\0') {
                        sem_report_rule(diagnostics,
                                        decl->loc,
                                        "BUS_PORT_UNKNOWN_BUS",
                                        "BUS port missing BUS identifier");
                    } else if (project_symbols && project_symbols->data) {
                        const JZSymbol *syms = (const JZSymbol *)project_symbols->data;
                        size_t count = project_symbols->len / sizeof(JZSymbol);
                        const JZSymbol *bus_sym = NULL;
                        for (size_t bi = 0; bi < count; ++bi) {
                            if (syms[bi].kind == JZ_SYM_BUS && syms[bi].name &&
                                strcmp(syms[bi].name, bus_name) == 0) {
                                bus_sym = &syms[bi];
                                break;
                            }
                        }
                        if (!bus_sym) {
                            sem_report_rule(diagnostics,
                                            decl->loc,
                                            "BUS_PORT_UNKNOWN_BUS",
                                            "BUS port references BUS name that is not declared in project");
                        }
                    }

                    if (role_name[0] == '\0' ||
                        (strcmp(role_name, "SOURCE") != 0 &&
                         strcmp(role_name, "TARGET") != 0)) {
                        sem_report_rule(diagnostics,
                                        decl->loc,
                                        "BUS_PORT_INVALID_ROLE",
                                        "BUS port role must be SOURCE or TARGET");
                    }

                    /* BUS array count (optional). */
                    if (decl->width) {
                        unsigned count = 0;
                        if (sem_eval_width_expr(decl->width,
                                                scope,
                                                project_symbols,
                                                &count) != 0) {
                            sem_report_rule(diagnostics,
                                            decl->loc,
                                            "BUS_PORT_ARRAY_COUNT_INVALID",
                                            "BUS array count must be a positive integer constant expression");
                        }
                    }
                }

                /* Skip standard port width checks for BUS ports. */
                if (decl->type == JZ_AST_PORT_DECL &&
                    decl->block_kind && strcmp(decl->block_kind, "BUS") == 0) {
                    continue;
                }

                /* PORT_MISSING_WIDTH: module ports must declare a width. */
                if (decl->type == JZ_AST_PORT_DECL && !decl->width) {
                    sem_report_rule(diagnostics,
                                    decl->loc,
                                    "PORT_MISSING_WIDTH",
                                    "port declaration must specify [width]");
                    continue;
                }

                /* REG_INIT_CONTAINS_X: register initialization literals must
                 * not contain x bits.
                 */
                if (decl->type == JZ_AST_REGISTER_DECL &&
                    decl->child_count >= 1 && decl->children[0] &&
                    decl->children[0]->type == JZ_AST_EXPR_LITERAL &&
                    decl->children[0]->text &&
                    sem_literal_has_x_bits(decl->children[0]->text)) {
                    sem_report_rule(diagnostics,
                                    decl->children[0]->loc.line ? decl->children[0]->loc : decl->loc,
                                    "REG_INIT_CONTAINS_X",
                                    "register initialization literal must not contain x bits");
                }

                /* REG_INIT_CONTAINS_Z: register initialization literals must
                 * not contain z bits.
                 */
                if (decl->type == JZ_AST_REGISTER_DECL &&
                    decl->child_count >= 1 && decl->children[0] &&
                    decl->children[0]->type == JZ_AST_EXPR_LITERAL &&
                    decl->children[0]->text &&
                    sem_literal_has_z_bits(decl->children[0]->text)) {
                    sem_report_rule(diagnostics,
                                    decl->children[0]->loc.line ? decl->children[0]->loc : decl->loc,
                                    "REG_INIT_CONTAINS_Z",
                                    "register initialization literal must not contain z bits");
                }

                /* REG_INIT_WIDTH_MISMATCH: register initialization literal
                 * width must match the declared register width.
                 */
                if (decl->type == JZ_AST_REGISTER_DECL &&
                    decl->width &&
                    decl->child_count >= 1 && decl->children[0] &&
                    decl->children[0]->type == JZ_AST_EXPR_LITERAL &&
                    decl->children[0]->text) {
                    unsigned reg_w = 0;
                    int rrc = eval_simple_positive_decl_int(decl->width, &reg_w);
                    if (rrc == 1 && reg_w > 0) {
                        const char *lit = decl->children[0]->text;
                        const char *tick = strchr(lit, '\'');
                        if (tick && tick > lit) {
                            char wbuf[32];
                            size_t wlen = (size_t)(tick - lit);
                            if (wlen > 0 && wlen < sizeof(wbuf)) {
                                memcpy(wbuf, lit, wlen);
                                wbuf[wlen] = '\0';
                                unsigned lit_w = 0;
                                if (parse_simple_positive_int(wbuf, &lit_w) &&
                                    lit_w != reg_w) {
                                    sem_report_rule(diagnostics,
                                                    decl->children[0]->loc.line ? decl->children[0]->loc : decl->loc,
                                                    "REG_INIT_WIDTH_MISMATCH",
                                                    "register initialization literal width does not match declared register width");
                                }
                            }
                        }
                    }
                }

                /* LATCH_INVALID_TYPE: latch type must be D or SR. */
                if (decl->type == JZ_AST_LATCH_DECL &&
                    decl->block_kind) {
                    if (strcmp(decl->block_kind, "D") != 0 &&
                        strcmp(decl->block_kind, "SR") != 0) {
                        sem_report_rule(diagnostics,
                                        decl->loc,
                                        "LATCH_INVALID_TYPE",
                                        "LATCH type must be D or SR");
                    }
                }

                /* LATCH_WIDTH_INVALID: latch width must be a positive integer. */
                if (decl->type == JZ_AST_LATCH_DECL &&
                    decl->width) {
                    unsigned lw = 0;
                    int lrc = eval_simple_positive_decl_int(decl->width, &lw);
                    if (lrc == -1) {
                        sem_report_rule(diagnostics,
                                        decl->loc,
                                        "LATCH_WIDTH_INVALID",
                                        "LATCH width must be a positive integer");
                    }
                }

                if (!decl->width) continue;

                if (sem_expr_has_lit_call(decl->width)) {
                    sem_report_rule(diagnostics,
                                    decl->loc,
                                    "LIT_INVALID_CONTEXT",
                                    "lit() may not be used in width declarations");
                    continue;
                }

                unsigned w = 0;
                int rc = eval_simple_positive_decl_int(decl->width, &w);
                if (rc == -1) {
                    sem_report_rule(diagnostics,
                                    decl->loc,
                                    "WIDTH_NONPOSITIVE_OR_NONINT",
                                    "declared width must be a positive integer");
                } else if (rc == 0 && project_symbols) {
                    /* Non-simple width: if it is a single identifier-like
                     * token, ensure it refers to a declared CONST or CONFIG.
                     */
                    char ident[64];
                    if (sem_extract_identifier_like(decl->width, ident, sizeof(ident))) {
                        const JZSymbol *c_sym = module_scope_lookup_kind(scope, ident, JZ_SYM_CONST);
                        const JZSymbol *cfg_sym = NULL;
                        if (project_symbols && project_symbols->data) {
                            const JZSymbol *syms = (const JZSymbol *)project_symbols->data;
                            size_t count = project_symbols->len / sizeof(JZSymbol);
                            for (size_t k = 0; k < count; ++k) {
                                if (syms[k].kind == JZ_SYM_CONFIG && syms[k].name &&
                                    strcmp(syms[k].name, ident) == 0) {
                                    cfg_sym = &syms[k];
                                    break;
                                }
                            }
                        }
                        if (c_sym && c_sym->node && c_sym->node->block_kind &&
                            strcmp(c_sym->node->block_kind, "STRING") == 0) {
                            sem_report_rule(diagnostics,
                                            decl->loc,
                                            "CONST_STRING_IN_NUMERIC_CONTEXT",
                                            "string CONST value used where a numeric expression is expected");
                        } else if (!c_sym && !cfg_sym) {
                            sem_report_rule(diagnostics,
                                            decl->loc,
                                            "CONST_UNDEFINED_IN_WIDTH_OR_SLICE",
                                            "width expression uses undefined CONST/CONFIG name");
                        }
                    }

                    /* Additionally, flag any CONFIG.<name> references in the
                     * width text that do not correspond to declared CONFIG
                     * entries in the project.
                     */
                    sem_check_undeclared_config_in_width(decl->width,
                                                         decl->loc,
                                                         project_symbols,
                                                         diagnostics);
                }
            }
        }
    }
}

/* -------------------------------------------------------------------------
 *  Literal width checks and simple clog2() argument validation in CONST
 *  expressions: LIT_UNDEFINED_CONST_WIDTH / LIT_WIDTH_NOT_POSITIVE /
 *  CLOG2_NONPOSITIVE_ARG / CLOG2_NONCONST_ARG
 * -------------------------------------------------------------------------
 */

/* Scan a constant expression string for a simple "clog2(<arg>)" call where
 * <arg> is a plain decimal integer literal. When such a literal is found and
 * is non-positive, we emit CLOG2_NONPOSITIVE_ARG. More complex arguments
 * (including CONST/CONFIG-based expressions) are left to the general
 * constant-eval engine, which already enforces compile-time semantics.
 *
 * Returns 1 if a clog2-related diagnostic was emitted, 0 otherwise.
 */
int sem_check_clog2_expr_simple(const char *expr_text,
                                       JZLocation loc,
                                       JZDiagnosticList *diagnostics)
{
    if (!expr_text || !diagnostics) return 0;
    const char *p = strstr(expr_text, "clog2");
    if (!p) return 0;

    /* Require exact "clog2(" sequence. */
    p += 5; /* skip "clog2" */
    while (*p && isspace((unsigned char)*p)) ++p;
    if (*p != '(') return 0;
    ++p; /* past '(' */

    const char *arg_start = p;
    int depth = 1;
    while (*p && depth > 0) {
        if (*p == '(') depth++;
        else if (*p == ')') depth--;
        if (depth == 0) break;
        ++p;
    }
    if (depth != 0) {
        /* Unbalanced parentheses; let constant-eval report generic error. */
        return 0;
    }
    const char *arg_end = p;
    if (arg_end <= arg_start) return 0;

    /* Trim whitespace around argument. */
    while (arg_start < arg_end && isspace((unsigned char)*arg_start)) arg_start++;
    while (arg_end > arg_start && isspace((unsigned char)arg_end[-1])) arg_end--;
    if (arg_start >= arg_end) return 0;

    size_t len = (size_t)(arg_end - arg_start);
    char buf[64];
    if (len >= sizeof(buf)) {
        /* Too complex for this simple checker; defer to full constant eval. */
        return 0;
    }
    memcpy(buf, arg_start, len);
    buf[len] = '\0';

    /* Attempt to parse as decimal integer. If this fails (identifiers,
     * operators, etc.), we treat it as a general compile-time expression
     * and leave validation to the constant-eval pass instead of flagging
     * CLOG2_NONCONST_ARG here.
     */
    char *endptr = NULL;
    long long val = strtoll(buf, &endptr, 10);
    if (endptr == buf) {
        return 0;
    }
    while (*endptr && isspace((unsigned char)*endptr)) ++endptr;
    if (*endptr != '\0') {
        return 0;
    }

    if (val <= 0) {
        sem_report_rule(diagnostics,
                        loc,
                        "CLOG2_NONPOSITIVE_ARG",
                        "argument to clog2() must be a positive integer");
        return 1;
    }

    return 0;
}
