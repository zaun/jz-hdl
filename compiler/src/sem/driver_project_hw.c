#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>

#include "sem_driver.h"
#include "sem.h"
#include "util.h"
#include "rules.h"
#include "driver_internal.h"

/* -------------------------------------------------------------------------
 *  Hardware-specific project validation: clocks, pins, map, @top, globals
 * -------------------------------------------------------------------------
 */

static int sem_parse_nonnegative_simple(const char *s, unsigned *out)
{
    return parse_simple_nonnegative_int(s, out);
}

static int sem_clock_parse_attrs(const char *attrs,
                                 double *out_period,
                                 char *out_edge,
                                 size_t out_edge_size)
{
    if (!out_period || !out_edge || out_edge_size == 0) return 0;
    *out_period = 0.0;
    out_edge[0] = '\0';
    if (!attrs) return 1;

    const char *p = strstr(attrs, "period");
    if (p) {
        p = strchr(p, '=');
        if (p) {
            ++p;
            while (*p && isspace((unsigned char)*p)) ++p;
            char *endptr = NULL;
            double val = strtod(p, &endptr);
            if (endptr != p) {
                *out_period = val;
            }
        }
    }

    const char *e = strstr(attrs, "edge");
    if (e) {
        e = strchr(e, '=');
        if (e) {
            ++e;
            while (*e && isspace((unsigned char)*e)) ++e;
            size_t len = 0;
            while (e[len] && !isspace((unsigned char)e[len]) && e[len] != ',' && e[len] != ';' && e[len] != '}') {
                ++len;
            }
            if (len >= out_edge_size) len = out_edge_size - 1;
            /* Copy edge value in a case-insensitive way: normalize to
             * canonical "Rising"/"Falling" style so that comparisons
             * later can remain case-sensitive while accepting any
             * casing in the source (e.g. "RISING", "FaLLinG").
             */
            for (size_t i = 0; i < len; ++i) {
                unsigned char ch = (unsigned char)e[i];
                if (i == 0) {
                    out_edge[i] = (char)toupper(ch);
                } else {
                    out_edge[i] = (char)tolower(ch);
                }
            }
            out_edge[len] = '\0';
        }
    }
    return 1;
}

void sem_check_project_clocks(JZASTNode *project,
                              JZBuffer *module_scopes,
                              const JZBuffer *project_symbols,
                              JZDiagnosticList *diagnostics)
{
    (void)module_scopes;

    if (!project || project->type != JZ_AST_PROJECT) return;

    JZASTNode *clocks = NULL;
    for (size_t i = 0; i < project->child_count; ++i) {
        JZASTNode *child = project->children[i];
        if (child && child->type == JZ_AST_CLOCKS_BLOCK) {
            clocks = child;
            break;
        }
    }
    if (!clocks) return;

    for (size_t i = 0; i < clocks->child_count; ++i) {
        JZASTNode *decl = clocks->children[i];
        if (!decl || decl->type != JZ_AST_CONST_DECL || !decl->name) continue;

        double period = 0.0;
        char edge[32];
        sem_clock_parse_attrs(decl->text, &period, edge, sizeof(edge));

        /* Clocks with period are validated here; clocks without period
         * are validated by sem_check_project_clock_gen (must be CLOCK_GEN outputs).
         */
        int has_period = (period > 0.0);

        if (edge[0] != '\0' &&
            strcmp(edge, "Rising") != 0 &&
            strcmp(edge, "Falling") != 0) {
            sem_report_rule(diagnostics,
                            decl->loc,
                            "CLOCK_EDGE_INVALID",
                            "clock edge must be Rising or Falling");
        }

        if (project_symbols && has_period) {
            /* Clock with period: must be in IN_PINS (external clock) */
            const JZSymbol *pin_sym = project_lookup(project_symbols, decl->name, JZ_SYM_PIN);
            JZASTNode *pin_decl = pin_sym ? pin_sym->node : NULL;
            const char *pin_block = (pin_decl && pin_decl->block_kind)
                                  ? pin_decl->block_kind
                                  : "";

            if (!pin_sym || strcmp(pin_block, "IN_PINS") != 0) {
                sem_report_rule(diagnostics,
                                decl->loc,
                                "CLOCK_NAME_NOT_IN_PINS",
                                "clock with period in CLOCKS has no matching IN_PINS declaration");
            } else if (pin_decl && pin_decl->width) {
                if (sem_expr_has_lit_call(pin_decl->width)) {
                    sem_report_rule(diagnostics,
                                    pin_decl->loc,
                                    "LIT_INVALID_CONTEXT",
                                    "lit() may not be used in pin width declarations");
                    continue;
                }
                unsigned w = 0;
                int rc = eval_simple_positive_decl_int(pin_decl->width, &w);
                if (rc == 1 && w != 1u) {
                    sem_report_rule(diagnostics,
                                    pin_decl->loc,
                                    "CLOCK_PORT_WIDTH_NOT_1",
                                    "clock pin width must be [1]");
                }
            }
        }
    }
}

/**
 * @brief Helper to check if a clock name is an output of a CLOCK_GEN block.
 */
static int is_clock_gen_output(JZASTNode *project, const char *clock_name) {
    if (!project || !clock_name) return 0;

    for (size_t i = 0; i < project->child_count; ++i) {
        JZASTNode *child = project->children[i];
        if (!child || child->type != JZ_AST_CLOCK_GEN_BLOCK) continue;

        for (size_t u = 0; u < child->child_count; ++u) {
            JZASTNode *unit = child->children[u];
            if (!unit || unit->type != JZ_AST_CLOCK_GEN_UNIT) continue;

            for (size_t c = 0; c < unit->child_count; ++c) {
                JZASTNode *out = unit->children[c];
                if (!out || (out->type != JZ_AST_CLOCK_GEN_OUT && out->type != JZ_AST_CLOCK_GEN_WIRE)) continue;
                if (out->name && strcmp(out->name, clock_name) == 0) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

/**
 * @brief Helper to check if a clock name is an input of a CLOCK_GEN block.
 */
static int is_clock_gen_input(JZASTNode *project, const char *clock_name) {
    if (!project || !clock_name) return 0;

    for (size_t i = 0; i < project->child_count; ++i) {
        JZASTNode *child = project->children[i];
        if (!child || child->type != JZ_AST_CLOCK_GEN_BLOCK) continue;

        for (size_t u = 0; u < child->child_count; ++u) {
            JZASTNode *unit = child->children[u];
            if (!unit || unit->type != JZ_AST_CLOCK_GEN_UNIT) continue;

            for (size_t c = 0; c < unit->child_count; ++c) {
                JZASTNode *in = unit->children[c];
                if (!in || in->type != JZ_AST_CLOCK_GEN_IN) continue;
                if (in->name && strcmp(in->name, clock_name) == 0) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

/* -------------------------------------------------------------------------
 *  Small double expression evaluator for chip-data derived expressions.
 *  Supports: identifiers, numeric literals, +, -, *, /, parentheses.
 *  Identifier resolution:  first from CONFIG children, then chip defaults,
 *  then special "refclk_period_ns".
 * -------------------------------------------------------------------------
 */
typedef struct CGenEvalCtx {
    JZASTNode *unit;                /* CLOCK_GEN_UNIT node */
    const JZChipData *chip;
    const char *unit_type;          /* e.g., "pll" */
    const JZBuffer *project_symbols;
    JZASTNode *project;
    double refclk_period_ns;        /* resolved from CLOCKS */
    int ok;                         /* set to 0 on error */
} CGenEvalCtx;

static double cgen_eval_expr(const char **pp, CGenEvalCtx *ctx);

static void cgen_skip_ws(const char **pp) {
    while (**pp && isspace((unsigned char)**pp)) ++(*pp);
}

static double cgen_resolve_ident(const char *name, size_t len, CGenEvalCtx *ctx)
{
    char buf[128];
    if (len >= sizeof(buf)) { ctx->ok = 0; return 0.0; }
    memcpy(buf, name, len);
    buf[len] = '\0';

    if (strcmp(buf, "refclk_period_ns") == 0 || strcmp(buf, "REF_CLK_period_ns") == 0) {
        return ctx->refclk_period_ns;
    }

    /* Look up in the unit's CONFIG children */
    if (ctx->unit) {
        for (size_t c = 0; c < ctx->unit->child_count; ++c) {
            JZASTNode *elem = ctx->unit->children[c];
            if (!elem || elem->type != JZ_AST_CLOCK_GEN_CONFIG) continue;
            for (size_t k = 0; k < elem->child_count; ++k) {
                JZASTNode *cfg = elem->children[k];
                if (!cfg || cfg->type != JZ_AST_CONST_DECL || !cfg->name) continue;
                if (strcmp(cfg->name, buf) == 0 && cfg->text) {
                    char *endptr = NULL;
                    double val = strtod(cfg->text, &endptr);
                    if (endptr != cfg->text) return val;
                }
            }
        }
    }

    /* Fall back to chip data defaults */
    if (ctx->chip && ctx->unit_type) {
        const char *def = jz_chip_clock_gen_param_default(ctx->chip,
                                                           ctx->unit_type,
                                                           buf);
        if (def) {
            char *endptr = NULL;
            double val = strtod(def, &endptr);
            if (endptr != def) return val;
        }
    }

    ctx->ok = 0;
    return 0.0;
}

/* primary: NUMBER | IDENT | '(' expr ')' */
static double cgen_eval_primary(const char **pp, CGenEvalCtx *ctx)
{
    cgen_skip_ws(pp);
    if (**pp == '(') {
        ++(*pp);
        double v = cgen_eval_expr(pp, ctx);
        cgen_skip_ws(pp);
        if (**pp == ')') ++(*pp);
        return v;
    }
    if (isalpha((unsigned char)**pp) || **pp == '_') {
        const char *start = *pp;
        while (isalnum((unsigned char)**pp) || **pp == '_') ++(*pp);
        return cgen_resolve_ident(start, (size_t)(*pp - start), ctx);
    }
    /* Number */
    {
        char *endptr = NULL;
        double v = strtod(*pp, &endptr);
        if (endptr == *pp) { ctx->ok = 0; return 0.0; }
        *pp = endptr;
        return v;
    }
}

/* unary: [+-] primary */
static double cgen_eval_unary(const char **pp, CGenEvalCtx *ctx)
{
    cgen_skip_ws(pp);
    if (**pp == '-') { ++(*pp); return -cgen_eval_unary(pp, ctx); }
    if (**pp == '+') { ++(*pp); return cgen_eval_unary(pp, ctx); }
    return cgen_eval_primary(pp, ctx);
}

/* mul_div: unary ( ('*'|'/') unary )* */
static double cgen_eval_mul(const char **pp, CGenEvalCtx *ctx)
{
    double left = cgen_eval_unary(pp, ctx);
    for (;;) {
        cgen_skip_ws(pp);
        if (**pp == '*') { ++(*pp); left *= cgen_eval_unary(pp, ctx); }
        else if (**pp == '/') {
            ++(*pp);
            double d = cgen_eval_unary(pp, ctx);
            if (d == 0.0) { ctx->ok = 0; return 0.0; }
            left /= d;
        }
        else break;
    }
    return left;
}

/* expr: mul_div ( ('+'|'-') mul_div )* */
static double cgen_eval_expr(const char **pp, CGenEvalCtx *ctx)
{
    double left = cgen_eval_mul(pp, ctx);
    for (;;) {
        cgen_skip_ws(pp);
        if (**pp == '+') { ++(*pp); left += cgen_eval_mul(pp, ctx); }
        else if (**pp == '-') { ++(*pp); left -= cgen_eval_mul(pp, ctx); }
        else break;
    }
    return left;
}

static double cgen_evaluate(const char *expr, CGenEvalCtx *ctx)
{
    ctx->ok = 1;
    const char *p = expr;
    double result = cgen_eval_expr(&p, ctx);
    cgen_skip_ws(&p);
    if (*p != '\0') ctx->ok = 0;
    return result;
}

/* Resolve refclk_period_ns for a CLOCK_GEN unit by finding its IN clock's
 * period from the CLOCKS block via project_symbols.
 */
static double cgen_resolve_refclk_period(JZASTNode *unit,
                                          JZASTNode *project,
                                          const JZBuffer *project_symbols)
{
    if (!unit || !project || !project_symbols) return 0.0;

    /* Find the IN clock name */
    const char *in_clk = NULL;
    for (size_t c = 0; c < unit->child_count; ++c) {
        JZASTNode *elem = unit->children[c];
        if (elem && elem->type == JZ_AST_CLOCK_GEN_IN && elem->name) {
            in_clk = elem->name;
            break;
        }
    }
    if (!in_clk) return 0.0;

    /* Find the clock decl in CLOCKS block */
    const JZSymbol *clk_sym = project_lookup(project_symbols, in_clk, JZ_SYM_CLOCK);
    if (!clk_sym || !clk_sym->node) return 0.0;

    double period = 0.0;
    char edge[32];
    sem_clock_parse_attrs(clk_sym->node->text, &period, edge, sizeof(edge));
    return period;
}

void sem_check_project_clock_gen(JZASTNode *project,
                                 const JZBuffer *project_symbols,
                                 const JZChipData *chip,
                                 JZDiagnosticList *diagnostics)
{
    if (!project || project->type != JZ_AST_PROJECT) return;

    /* If no chip data is available, emit an info diagnostic for each CLOCK_GEN block */
    if (!chip) {
        for (size_t i = 0; i < project->child_count; ++i) {
            JZASTNode *cgen = project->children[i];
            if (cgen && cgen->type == JZ_AST_CLOCK_GEN_BLOCK) {
                sem_report_rule(diagnostics,
                                cgen->loc,
                                "CLOCK_GEN_NO_CHIP_DATA",
                                "add CHIP=\"<device>\" to @project to enable "
                                "CLOCK_GEN parameter and frequency validation");
                break;  /* one diagnostic is sufficient */
            }
        }
    }

    /* Find the CLOCKS block for clock validation */
    JZASTNode *clocks = NULL;
    for (size_t i = 0; i < project->child_count; ++i) {
        JZASTNode *child = project->children[i];
        if (child && child->type == JZ_AST_CLOCKS_BLOCK) {
            clocks = child;
            break;
        }
    }

    /* Track clocks driven by CLOCK_GEN to detect multiple drivers */
    JZBuffer driven_clocks = {0};

    /* Validate CLOCK_GEN blocks */
    for (size_t i = 0; i < project->child_count; ++i) {
        JZASTNode *cgen = project->children[i];
        if (!cgen || cgen->type != JZ_AST_CLOCK_GEN_BLOCK) continue;

        /* Collect all outputs from prior units for cross-unit chaining check */
        JZBuffer prior_unit_outputs = {0};

        for (size_t u = 0; u < cgen->child_count; ++u) {
            JZASTNode *unit = cgen->children[u];
            if (!unit || unit->type != JZ_AST_CLOCK_GEN_UNIT) continue;

            /* Validate generator type: PLL, DLL, CLKDIV, OSC, or BUF with optional numeric suffix */
            {
                int valid_type = 0;
                if (unit->name) {
                    const char *rest = NULL;
                    if (strncmp(unit->name, "CLKDIV", 6) == 0) rest = unit->name + 6;
                    else if (strncmp(unit->name, "PLL", 3) == 0) rest = unit->name + 3;
                    else if (strncmp(unit->name, "DLL", 3) == 0) rest = unit->name + 3;
                    else if (strncmp(unit->name, "OSC", 3) == 0) rest = unit->name + 3;
                    else if (strncmp(unit->name, "BUF", 3) == 0) rest = unit->name + 3;
                    if (rest) {
                        valid_type = 1;
                        while (*rest) {
                            if (!isdigit((unsigned char)*rest)) { valid_type = 0; break; }
                            rest++;
                        }
                    }
                }
                if (!valid_type) {
                    sem_report_rule(diagnostics,
                                    unit->loc,
                                    "CLOCK_GEN_INVALID_TYPE",
                                    "CLOCK_GEN generator must be PLL, DLL, CLKDIV, OSC, or BUF (with optional numeric suffix)");
                    continue;
                }
            }

            /* Collect this unit's outputs for self-reference check */
            JZBuffer this_unit_outputs = {0};
            for (size_t c = 0; c < unit->child_count; ++c) {
                JZASTNode *elem = unit->children[c];
                if (elem && (elem->type == JZ_AST_CLOCK_GEN_OUT || elem->type == JZ_AST_CLOCK_GEN_WIRE) && elem->name) {
                    const char *out_clk = elem->name;
                    jz_buf_append(&this_unit_outputs, &out_clk, sizeof(char *));
                }
            }

            int has_input = 0;
            int has_output = 0;

            for (size_t c = 0; c < unit->child_count; ++c) {
                JZASTNode *elem = unit->children[c];
                if (!elem) continue;

                if (elem->type == JZ_AST_CLOCK_GEN_IN) {
                    has_input = 1;
                    const char *in_selector = elem->block_kind; /* e.g. "REF_CLK", "CE" */
                    const char *in_clk = elem->name;            /* signal name */
                    if (!in_clk) continue;

                    /* Look up chip data input definition for this selector */
                    const JZChipClockGenInput *chip_input = NULL;
                    if (chip && unit->name && in_selector) {
                        char tl[32];
                        {
                            size_t tlen = strlen(unit->name);
                            if (tlen >= sizeof(tl)) tlen = sizeof(tl) - 1;
                            for (size_t t = 0; t < tlen; ++t)
                                tl[t] = (char)tolower((unsigned char)unit->name[t]);
                            tl[tlen] = '\0';
                        }
                        chip_input = jz_chip_clock_gen_input(chip, tl, in_selector);
                    }

                    /* Validate input clock exists in CLOCKS */
                    const JZSymbol *clk_sym = project_lookup(project_symbols, in_clk, JZ_SYM_CLOCK);
                    if (!clk_sym) {
                        sem_report_rule(diagnostics,
                                        elem->loc,
                                        "CLOCK_GEN_INPUT_NOT_DECLARED",
                                        "CLOCK_GEN input clock not declared in CLOCKS block");
                        continue;
                    }

                    /* Check if input is an output of a prior unit (cross-unit chaining is OK) */
                    int is_chained = 0;
                    {
                        size_t prior_count = prior_unit_outputs.len / sizeof(char *);
                        char **priors = (char **)prior_unit_outputs.data;
                        for (size_t o = 0; o < prior_count; ++o) {
                            if (priors[o] && strcmp(priors[o], in_clk) == 0) {
                                is_chained = 1;
                                break;
                            }
                        }
                    }

                    /* Input clock must have a period if chip input requires_period (unless chained) */
                    int requires_period = 1; /* default: require period */
                    if (chip_input) {
                        requires_period = chip_input->requires_period;
                    }
                    if (!is_chained && requires_period) {
                        JZASTNode *clk_decl = clk_sym->node;
                        if (clk_decl) {
                            double period = 0.0;
                            char edge[32];
                            sem_clock_parse_attrs(clk_decl->text, &period, edge, sizeof(edge));
                            if (period <= 0.0) {
                                sem_report_rule(diagnostics,
                                                elem->loc,
                                                "CLOCK_GEN_INPUT_NO_PERIOD",
                                                "CLOCK_GEN input clock must have a period in CLOCKS");
                            } else if (chip_input && chip_input->has_min_mhz && chip_input->has_max_mhz) {
                                /* Validate frequency range from chip data input */
                                double freq_mhz = 1000.0 / period;
                                if (freq_mhz < chip_input->min_mhz || freq_mhz > chip_input->max_mhz) {
                                    char msg[512];
                                    snprintf(msg, sizeof(msg),
                                             "CLOCK_GEN input frequency %.1f MHz is outside "
                                             "valid range [%.0f, %.0f] MHz",
                                             freq_mhz, chip_input->min_mhz, chip_input->max_mhz);
                                    sem_report_rule(diagnostics, elem->loc,
                                                    "CLOCK_GEN_INPUT_FREQ_OUT_OF_RANGE", msg);
                                }
                            } else if (chip && unit->name) {
                                /* Fallback: use legacy refclk_range */
                                double freq_mhz = 1000.0 / period;
                                char tl[32];
                                {
                                    size_t tlen = strlen(unit->name);
                                    if (tlen >= sizeof(tl)) tlen = sizeof(tl) - 1;
                                    for (size_t t = 0; t < tlen; ++t)
                                        tl[t] = (char)tolower((unsigned char)unit->name[t]);
                                    tl[tlen] = '\0';
                                }
                                double rmin = 0.0, rmax = 0.0;
                                if (jz_chip_clock_gen_refclk_range(chip, tl, &rmin, &rmax)) {
                                    if (freq_mhz < rmin || freq_mhz > rmax) {
                                        char msg[512];
                                        snprintf(msg, sizeof(msg),
                                                 "CLOCK_GEN input frequency %.1f MHz is outside "
                                                 "valid range [%.0f, %.0f] MHz",
                                                 freq_mhz, rmin, rmax);
                                        sem_report_rule(diagnostics, elem->loc,
                                                        "CLOCK_GEN_INPUT_FREQ_OUT_OF_RANGE", msg);
                                    }
                                }
                            }
                        }
                    }

                    /* Check input is not an output of this SAME unit */
                    size_t this_count = this_unit_outputs.len / sizeof(char *);
                    char **this_outs = (char **)this_unit_outputs.data;
                    for (size_t o = 0; o < this_count; ++o) {
                        if (this_outs[o] && strcmp(this_outs[o], in_clk) == 0) {
                            sem_report_rule(diagnostics,
                                            elem->loc,
                                            "CLOCK_GEN_INPUT_IS_SELF_OUTPUT",
                                            "CLOCK_GEN input clock is an output of the same unit");
                            break;
                        }
                    }
                }

                if (elem->type == JZ_AST_CLOCK_GEN_OUT || elem->type == JZ_AST_CLOCK_GEN_WIRE) {
                    has_output = 1;
                    int is_wire = (elem->type == JZ_AST_CLOCK_GEN_WIRE);
                    const char *out_clk = elem->name;
                    if (!out_clk) continue;

                    /* Validate output selector is valid for this chip's generator type */
                    const char *out_selector = elem->block_kind;
                    char tl[32] = {0};
                    if (unit->name) {
                        size_t tlen = strlen(unit->name);
                        if (tlen >= sizeof(tl)) tlen = sizeof(tl) - 1;
                        for (size_t t = 0; t < tlen; ++t)
                            tl[t] = (char)tolower((unsigned char)unit->name[t]);
                        tl[tlen] = '\0';
                    }
                    if (chip && tl[0] && out_selector) {
                        if (!jz_chip_clock_gen_output_valid(chip, tl, out_selector)) {
                            char msg[256];
                            snprintf(msg, sizeof(msg),
                                     "CLOCK_GEN output selector '%s' not valid for %s",
                                     out_selector, unit->name);
                            sem_report_rule(diagnostics, elem->loc,
                                            "CLOCK_GEN_OUTPUT_INVALID_SELECTOR", msg);
                        } else {
                            /* Validate OUT vs WIRE matches chip data is_clock */
                            int chip_is_clock = jz_chip_clock_gen_output_is_clock(chip, tl, out_selector);
                            if (chip_is_clock >= 0) {
                                if (!is_wire && !chip_is_clock) {
                                    char msg[256];
                                    snprintf(msg, sizeof(msg),
                                             "'%s' is not a clock output; use WIRE instead of OUT",
                                             out_selector);
                                    sem_report_rule(diagnostics, elem->loc,
                                                    "CLOCK_GEN_OUT_NOT_CLOCK", msg);
                                } else if (is_wire && chip_is_clock) {
                                    char msg[256];
                                    snprintf(msg, sizeof(msg),
                                             "'%s' is a clock output; use OUT instead of WIRE",
                                             out_selector);
                                    sem_report_rule(diagnostics, elem->loc,
                                                    "CLOCK_GEN_WIRE_IS_CLOCK", msg);
                                }
                            }
                        }
                    }

                    if (is_wire) {
                        /* WIRE outputs must NOT be in CLOCKS block */
                        const JZSymbol *clk_sym = project_lookup(project_symbols, out_clk, JZ_SYM_CLOCK);
                        if (clk_sym) {
                            sem_report_rule(diagnostics, elem->loc,
                                            "CLOCK_GEN_WIRE_IN_CLOCKS",
                                            "CLOCK_GEN WIRE output must not be declared in CLOCKS block");
                        }
                    } else {
                        /* OUT outputs must be in CLOCKS block */
                        const JZSymbol *clk_sym = project_lookup(project_symbols, out_clk, JZ_SYM_CLOCK);
                        if (!clk_sym) {
                            sem_report_rule(diagnostics,
                                            elem->loc,
                                            "CLOCK_GEN_OUTPUT_NOT_DECLARED",
                                            "CLOCK_GEN output clock not declared in CLOCKS block");
                            continue;
                        }

                        /* Output clock must NOT have a period */
                        JZASTNode *clk_decl = clk_sym->node;
                        if (clk_decl) {
                            double period = 0.0;
                            char edge[32];
                            sem_clock_parse_attrs(clk_decl->text, &period, edge, sizeof(edge));
                            if (period > 0.0) {
                                sem_report_rule(diagnostics,
                                                elem->loc,
                                                "CLOCK_GEN_OUTPUT_HAS_PERIOD",
                                                "CLOCK_GEN output clock must not have period in CLOCKS");
                            }
                        }

                        /* Output clock must NOT be an IN_PINS */
                        const JZSymbol *pin_sym = project_lookup(project_symbols, out_clk, JZ_SYM_PIN);
                        if (pin_sym && pin_sym->node && pin_sym->node->block_kind) {
                            if (strcmp(pin_sym->node->block_kind, "IN_PINS") == 0) {
                                sem_report_rule(diagnostics,
                                                elem->loc,
                                                "CLOCK_GEN_OUTPUT_IS_INPUT_PIN",
                                                "CLOCK_GEN output clock must not be declared as IN_PINS");
                            }
                        }
                    }

                    /* Check for multiple drivers */
                    size_t driven_count = driven_clocks.len / sizeof(char *);
                    char **driven = (char **)driven_clocks.data;
                    int already_driven = 0;
                    for (size_t d = 0; d < driven_count; ++d) {
                        if (driven[d] && strcmp(driven[d], out_clk) == 0) {
                            already_driven = 1;
                            break;
                        }
                    }
                    if (already_driven) {
                        sem_report_rule(diagnostics,
                                        elem->loc,
                                        "CLOCK_GEN_MULTIPLE_DRIVERS",
                                        "clock is driven by multiple CLOCK_GEN outputs");
                    } else {
                        jz_buf_append(&driven_clocks, &out_clk, sizeof(char *));
                    }
                }
            }

            if (!has_input && strncmp(unit->name, "OSC", 3) != 0) {
                sem_report_rule(diagnostics,
                                unit->loc,
                                "CLOCK_GEN_MISSING_INPUT",
                                "CLOCK_GEN PLL/DLL must have an IN clock");
            }
            /* Check for required inputs from chip data */
            if (chip && unit->name) {
                char tl[32];
                {
                    size_t tlen = strlen(unit->name);
                    if (tlen >= sizeof(tl)) tlen = sizeof(tl) - 1;
                    for (size_t t = 0; t < tlen; ++t)
                        tl[t] = (char)tolower((unsigned char)unit->name[t]);
                    tl[tlen] = '\0';
                }
                size_t chip_input_count = jz_chip_clock_gen_input_count(chip, tl);
                for (size_t ci = 0; ci < chip_input_count; ++ci) {
                    const JZChipClockGenInput *chip_inp = jz_chip_clock_gen_input_at(chip, tl, ci);
                    if (!chip_inp || !chip_inp->required || !chip_inp->name) continue;
                    /* Check if this required input was provided */
                    int found = 0;
                    for (size_t c2 = 0; c2 < unit->child_count; ++c2) {
                        JZASTNode *elem2 = unit->children[c2];
                        if (elem2 && elem2->type == JZ_AST_CLOCK_GEN_IN &&
                            elem2->block_kind && strcasecmp(elem2->block_kind, chip_inp->name) == 0) {
                            found = 1;
                            break;
                        }
                    }
                    if (!found) {
                        char msg[512];
                        snprintf(msg, sizeof(msg),
                                 "CLOCK_GEN required input '%s' is not provided",
                                 chip_inp->name);
                        sem_report_rule(diagnostics, unit->loc,
                                        "CLOCK_GEN_REQUIRED_INPUT_MISSING", msg);
                    }
                }
            }
            if (!has_output) {
                sem_report_rule(diagnostics,
                                unit->loc,
                                "CLOCK_GEN_MISSING_OUTPUT",
                                "CLOCK_GEN PLL/DLL must have at least one OUT clock");
            }

            /* Variant dispatch check: compute the facts for this unit and
             * verify exactly one chip variant matches. Only meaningful when
             * chip data is available. */
            if (chip && unit->name && has_output) {
                char type_lower[32];
                {
                    size_t tlen = strlen(unit->name);
                    if (tlen >= sizeof(type_lower)) tlen = sizeof(type_lower) - 1;
                    for (size_t t = 0; t < tlen; ++t)
                        type_lower[t] = (char)tolower((unsigned char)unit->name[t]);
                    type_lower[tlen] = '\0';
                }

                /* Count OUT (clock) outputs only — WIRE outputs are excluded
                 * per S9.3 output.count semantics. */
                int out_count = 0;
                for (size_t c = 0; c < unit->child_count; ++c) {
                    JZASTNode *elem = unit->children[c];
                    if (elem && elem->type == JZ_AST_CLOCK_GEN_OUT) out_count++;
                }

                /* Build per-input source facts (pad|fabric). */
                JZChipClockGenInputFact input_facts[16];
                size_t input_fact_count = 0;
                for (size_t c = 0; c < unit->child_count &&
                                   input_fact_count < 16; ++c) {
                    JZASTNode *elem = unit->children[c];
                    if (!elem || elem->type != JZ_AST_CLOCK_GEN_IN) continue;
                    if (!elem->block_kind || !elem->name) continue;

                    const JZSymbol *pin_sym =
                        project_lookup(project_symbols, elem->name, JZ_SYM_PIN);
                    const char *src = pin_sym ? "pad" : "fabric";
                    input_facts[input_fact_count].input_name = elem->block_kind;
                    input_facts[input_fact_count].source = src;
                    input_fact_count++;
                }

                JZChipClockGenFacts facts;
                facts.inputs = input_facts;
                facts.input_count = input_fact_count;
                facts.output_count = out_count;

                /* We don't care about the template text here — just the
                 * match count. Use a canonical backend name ("verilog-2005");
                 * a successful lookup only requires the variant to exist,
                 * and any backend will do for presence checking. */
                int match_count = 0;
                (void)jz_chip_clock_gen_map_for_facts(chip, type_lower,
                                                      "verilog-2005",
                                                      &facts, &match_count);
                if (match_count == 0) {
                    char msg[512];
                    size_t off = 0;
                    off += (size_t)snprintf(msg + off, sizeof(msg) - off,
                                             "no chip clock_gen variant matches facts { ");
                    for (size_t fi = 0; fi < input_fact_count &&
                                         off + 1 < sizeof(msg); ++fi) {
                        off += (size_t)snprintf(msg + off, sizeof(msg) - off,
                                                 "input.%s.source=\"%s\", ",
                                                 input_facts[fi].input_name,
                                                 input_facts[fi].source);
                    }
                    snprintf(msg + off, sizeof(msg) - off,
                             "output.count=%d }", out_count);
                    sem_report_rule(diagnostics, unit->loc,
                                    "CLOCK_GEN_VARIANT_NO_MATCH", msg);
                } else if (match_count > 1) {
                    char msg[256];
                    snprintf(msg, sizeof(msg),
                             "%d chip clock_gen variants match this unit's "
                             "facts (chip JSON is not disjoint)", match_count);
                    sem_report_rule(diagnostics, unit->loc,
                                    "CLOCK_GEN_VARIANT_AMBIGUOUS", msg);
                }
            }

            /* Move this unit's outputs into prior_unit_outputs for next unit */
            {
                size_t this_count = this_unit_outputs.len / sizeof(char *);
                char **this_outs = (char **)this_unit_outputs.data;
                for (size_t o = 0; o < this_count; ++o) {
                    jz_buf_append(&prior_unit_outputs, &this_outs[o], sizeof(char *));
                }
            }
            jz_buf_free(&this_unit_outputs);
        }

        jz_buf_free(&prior_unit_outputs);
    }

    /* Validate clocks without period are CLOCK_GEN outputs */
    if (clocks) {
        for (size_t i = 0; i < clocks->child_count; ++i) {
            JZASTNode *decl = clocks->children[i];
            if (!decl || decl->type != JZ_AST_CONST_DECL || !decl->name) continue;

            double period = 0.0;
            char edge[32];
            sem_clock_parse_attrs(decl->text, &period, edge, sizeof(edge));

            /* Check if "period" was explicitly specified in the text */
            int has_explicit_period = (decl->text && strstr(decl->text, "period"));

            if (has_explicit_period && period <= 0.0) {
                /* Explicit invalid period (period=0, period=-1) - always an error */
                sem_report_rule(diagnostics,
                                decl->loc,
                                "CLOCK_PERIOD_NONPOSITIVE",
                                "clock period must be a positive number");
            } else if (!has_explicit_period && period <= 0.0) {
                /* No period specified - must be a CLOCK_GEN output or input.
                 * CLOCK_GEN inputs get their own error (CLOCK_GEN_INPUT_NO_PERIOD)
                 * so we skip them here to avoid duplicate/confusing errors.
                 */
                if (!is_clock_gen_output(project, decl->name) &&
                    !is_clock_gen_input(project, decl->name)) {
                    /* Check if it's in IN_PINS - if so, it needs a period */
                    const JZSymbol *pin_sym = project_lookup(project_symbols, decl->name, JZ_SYM_PIN);
                    if (pin_sym && pin_sym->node && pin_sym->node->block_kind &&
                        strcmp(pin_sym->node->block_kind, "IN_PINS") == 0) {
                        sem_report_rule(diagnostics,
                                        decl->loc,
                                        "CLOCK_EXTERNAL_NO_PERIOD",
                                        "external clock (IN_PINS) must have period in CLOCKS");
                    } else {
                        sem_report_rule(diagnostics,
                                        decl->loc,
                                        "CLOCK_PERIOD_NONPOSITIVE",
                                        "clock period must be positive or clock must be CLOCK_GEN output");
                    }
                }
            } else if (period > 0.0) {
                /* Clock with period must NOT be a CLOCK_GEN output */
                if (is_clock_gen_output(project, decl->name)) {
                    sem_report_rule(diagnostics,
                                    decl->loc,
                                    "CLOCK_SOURCE_AMBIGUOUS",
                                    "clock has period but is also a CLOCK_GEN output");
                }
            }
        }
    }

    /* Data-driven parameter and derived value validation using chip data */
    if (chip) {
        for (size_t i = 0; i < project->child_count; ++i) {
            JZASTNode *cgen = project->children[i];
            if (!cgen || cgen->type != JZ_AST_CLOCK_GEN_BLOCK) continue;

            for (size_t u = 0; u < cgen->child_count; ++u) {
                JZASTNode *unit = cgen->children[u];
                if (!unit || unit->type != JZ_AST_CLOCK_GEN_UNIT || !unit->name) continue;

                /* Lowercase type for chip data lookup */
                char type_lower[32];
                {
                    size_t tlen = strlen(unit->name);
                    if (tlen >= sizeof(type_lower)) tlen = sizeof(type_lower) - 1;
                    for (size_t t = 0; t < tlen; ++t)
                        type_lower[t] = (char)tolower((unsigned char)unit->name[t]);
                    type_lower[tlen] = '\0';
                }

                /* Find CONFIG children for this unit */
                JZASTNode *config_block = NULL;
                for (size_t c = 0; c < unit->child_count; ++c) {
                    JZASTNode *elem = unit->children[c];
                    if (elem && elem->type == JZ_AST_CLOCK_GEN_CONFIG) {
                        config_block = elem;
                        break;
                    }
                }

                /* a. Parameter range checks */
                size_t param_count = jz_chip_clock_gen_param_count(chip, type_lower);
                for (size_t pi = 0; pi < param_count; ++pi) {
                    const JZChipClockGenParam *cparam = jz_chip_clock_gen_param_at(chip, type_lower, pi);
                    if (!cparam) continue;

                    /* Find user-specified value */
                    const char *user_val = NULL;
                    JZLocation val_loc = unit->loc;
                    if (config_block) {
                        for (size_t k = 0; k < config_block->child_count; ++k) {
                            JZASTNode *cfg = config_block->children[k];
                            if (!cfg || cfg->type != JZ_AST_CONST_DECL || !cfg->name) continue;
                            if (strcmp(cfg->name, cparam->name) == 0) {
                                user_val = cfg->text;
                                val_loc = cfg->loc;
                                break;
                            }
                        }
                    }

                    /* If not specified, default is guaranteed in-range; skip */
                    if (!user_val) continue;

                    /* Type check: integer parameters must not have decimal values */
                    if (!cparam->is_double && strchr(user_val, '.')) {
                        char msg[512];
                        snprintf(msg, sizeof(msg),
                                 "CLOCK_GEN CONFIG '%s' requires an integer value, got '%s'",
                                 cparam->name, user_val);
                        sem_report_rule(diagnostics, val_loc,
                                        "CLOCK_GEN_PARAM_TYPE_MISMATCH", msg);
                        continue;  /* skip further validation for this param */
                    }

                    if (cparam->has_min && cparam->has_max) {
                        /* Range check (double for fractional params, int otherwise) */
                        char *endptr = NULL;
                        double val = strtod(user_val, &endptr);
                        if (endptr == user_val) continue; /* non-numeric, skip */

                        if (val < cparam->min || val > cparam->max) {
                            char msg[512];
                            if (cparam->is_double) {
                                snprintf(msg, sizeof(msg),
                                         "CLOCK_GEN CONFIG '%s' = %g is outside valid range [%g, %g]",
                                         cparam->name, val, cparam->min, cparam->max);
                            } else {
                                snprintf(msg, sizeof(msg),
                                         "CLOCK_GEN CONFIG '%s' = %ld is outside valid range [%ld, %ld]",
                                         cparam->name, (long)val, (long)cparam->min, (long)cparam->max);
                            }
                            sem_report_rule(diagnostics, val_loc,
                                            "CLOCK_GEN_PARAM_OUT_OF_RANGE", msg);
                        }
                    } else if (cparam->valid_count > 0) {
                        /* Discrete valid values check */
                        int found = 0;
                        for (size_t vi = 0; vi < cparam->valid_count; ++vi) {
                            if (cparam->valid_values[vi] &&
                                strcmp(user_val, cparam->valid_values[vi]) == 0) {
                                found = 1;
                                break;
                            }
                        }
                        if (!found) {
                            char msg[512];
                            char valid_list[384];
                            valid_list[0] = '\0';
                            size_t off = 0;
                            for (size_t vi = 0; vi < cparam->valid_count && off < sizeof(valid_list) - 1; ++vi) {
                                if (vi > 0) {
                                    int n = snprintf(valid_list + off, sizeof(valid_list) - off, ", ");
                                    if (n > 0) off += (size_t)n;
                                }
                                int n = snprintf(valid_list + off, sizeof(valid_list) - off, "%s",
                                                 cparam->valid_values[vi] ? cparam->valid_values[vi] : "?");
                                if (n > 0) off += (size_t)n;
                            }
                            snprintf(msg, sizeof(msg),
                                     "CONFIG '%s' = %s is not a valid value (expected one of: %s)",
                                     cparam->name, user_val, valid_list);
                            sem_report_rule(diagnostics, val_loc,
                                            "CLOCK_GEN_PARAM_OUT_OF_RANGE", msg);
                        }
                    }
                }

                /* b. Derived value checks */
                double refclk_period = cgen_resolve_refclk_period(unit, project, project_symbols);

                size_t derived_count = jz_chip_clock_gen_derived_count(chip, type_lower);
                for (size_t di = 0; di < derived_count; ++di) {
                    const JZChipClockGenDerived *cder = jz_chip_clock_gen_derived_at(chip, type_lower, di);
                    if (!cder || !cder->has_min || !cder->has_max || !cder->expr) continue;

                    /* Skip non-numeric expressions (e.g., toString(...)) */
                    if (strstr(cder->expr, "toString")) continue;

                    CGenEvalCtx ctx;
                    ctx.unit = unit;
                    ctx.chip = chip;
                    ctx.unit_type = type_lower;
                    ctx.project_symbols = project_symbols;
                    ctx.project = project;
                    ctx.refclk_period_ns = refclk_period;
                    ctx.ok = 1;

                    double result = cgen_evaluate(cder->expr, &ctx);
                    if (!ctx.ok) continue; /* couldn't evaluate, skip */

                    if (result < cder->min || result > cder->max) {
                        char msg[512];
                        snprintf(msg, sizeof(msg),
                                 "CLOCK_GEN derived '%s' = %.1f is outside valid range [%.0f, %.0f]",
                                 cder->name, result, cder->min, cder->max);
                        sem_report_rule(diagnostics, unit->loc,
                                        "CLOCK_GEN_DERIVED_OUT_OF_RANGE", msg);
                    }
                }
            }
        }
    }

    jz_buf_free(&driven_clocks);
}

/* ---------- I/O standard classification tables ---------- */

static const char *single_standards[] = {
    "LVTTL", "LVCMOS33", "LVCMOS25", "LVCMOS18", "LVCMOS15", "LVCMOS12",
    "PCI33",
    "SSTL25_I", "SSTL25_II", "SSTL18_I", "SSTL18_II", "SSTL15", "SSTL135",
    "HSTL18_I", "HSTL18_II", "HSTL15_I", "HSTL15_II",
    NULL
};

static const char *diff_standards[] = {
    "LVDS25", "LVDS33", "BLVDS25", "EXT_LVDS25",
    "TMDS33", "RSDS", "MINI_LVDS", "PPDS", "SUB_LVDS", "SLVS", "LVPECL33",
    "DIFF_SSTL25_I", "DIFF_SSTL25_II",
    "DIFF_SSTL18_I", "DIFF_SSTL18_II",
    "DIFF_SSTL15", "DIFF_SSTL135",
    "DIFF_HSTL18_I", "DIFF_HSTL18_II",
    "DIFF_HSTL15_I", "DIFF_HSTL15_II",
    NULL
};

static int is_valid_standard(const char *val) {
    for (const char **p = single_standards; *p; ++p) {
        if (strcmp(val, *p) == 0) return 1;
    }
    for (const char **p = diff_standards; *p; ++p) {
        if (strcmp(val, *p) == 0) return 1;
    }
    return 0;
}

static int is_diff_standard(const char *val) {
    for (const char **p = diff_standards; *p; ++p) {
        if (strcmp(val, *p) == 0) return 1;
    }
    return 0;
}

static int is_single_standard(const char *val) {
    for (const char **p = single_standards; *p; ++p) {
        if (strcmp(val, *p) == 0) return 1;
    }
    return 0;
}

/** Check if a standard supports on-die termination in single-ended mode. */
static int is_term_capable_single(const char *val) {
    return (strncmp(val, "SSTL", 4) == 0 || strncmp(val, "HSTL", 4) == 0);
}

/** Helper: extract a simple identifier value from an attribute string.
 *  Looks for "key=VALUE" and writes VALUE into out_val. Returns 1 if found. */
static int sem_extract_attr(const char *attrs, const char *key,
                            char *out_val, size_t out_size)
{
    out_val[0] = '\0';
    const char *p = strstr(attrs, key);
    if (!p) return 0;
    /* Make sure we matched the key as a whole word, not a substring of
     * another attribute (e.g., "standard" matching inside "substandard").
     */
    if (p != attrs) {
        char before = p[-1];
        if (isalnum((unsigned char)before) || before == '_') return 0;
    }
    p = strchr(p, '=');
    if (!p) return 0;
    ++p;
    while (*p && isspace((unsigned char)*p)) ++p;
    size_t len = 0;
    while (p[len] && !isspace((unsigned char)p[len]) &&
           p[len] != ',' && p[len] != ';' && p[len] != '}') {
        ++len;
    }
    if (len >= out_size) len = out_size - 1;
    memcpy(out_val, p, len);
    out_val[len] = '\0';
    return 1;
}

void sem_check_project_pins(JZASTNode *project,
                            const JZBuffer *project_symbols,
                            const JZChipData *chip,
                            JZDiagnosticList *diagnostics)
{
    (void)project_symbols;

    if (!project || project->type != JZ_AST_PROJECT) return;

    for (size_t i = 0; i < project->child_count; ++i) {
        JZASTNode *blk = project->children[i];
        if (!blk) continue;
        if (blk->type != JZ_AST_IN_PINS_BLOCK &&
            blk->type != JZ_AST_OUT_PINS_BLOCK &&
            blk->type != JZ_AST_INOUT_PINS_BLOCK) {
            continue;
        }

        int is_out_block = (blk->type == JZ_AST_OUT_PINS_BLOCK);
        int require_drive = (blk->type == JZ_AST_OUT_PINS_BLOCK || blk->type == JZ_AST_INOUT_PINS_BLOCK);

        for (size_t j = 0; j < blk->child_count; ++j) {
            JZASTNode *decl = blk->children[j];
            if (!decl || decl->type != JZ_AST_PORT_DECL) continue;

            if (decl->width) {
                if (sem_expr_has_lit_call(decl->width)) {
                    sem_report_rule(diagnostics,
                                    decl->loc,
                                    "LIT_INVALID_CONTEXT",
                                    "lit() may not be used in pin width declarations");
                    continue;
                }
                unsigned w = 0;
                int rc = eval_simple_positive_decl_int(decl->width, &w);
                if (rc == -1 || (rc == 1 && w == 0u)) {
                    sem_report_rule(diagnostics,
                                    decl->loc,
                                    "PIN_BUS_WIDTH_INVALID",
                                    "bus pin width must be a positive integer");
                } else if (rc == 0) {
                    long long sval = 0;
                    if (parse_simple_signed_int(decl->width, &sval) && sval <= 0) {
                        sem_report_rule(diagnostics,
                                        decl->loc,
                                        "PIN_BUS_WIDTH_INVALID",
                                        "bus pin width must be a positive integer");
                    }
                }
            }

            const char *attrs = decl->text;
            if (!attrs) attrs = "";

            /* --- standard --- */
            char standard_val[64];
            int has_standard = sem_extract_attr(attrs, "standard", standard_val, sizeof(standard_val));
            int standard_valid = has_standard && is_valid_standard(standard_val);

            if (!has_standard || !standard_valid) {
                sem_report_rule(diagnostics,
                                decl->loc,
                                "PIN_INVALID_STANDARD",
                                "invalid or missing electrical standard in PIN declaration");
            }

            /* --- drive (accept fractional values like 3.5) --- */
            int has_drive = 0;
            int drive_valid = 0;
            {
                const char *d = strstr(attrs, "drive");
                if (d) {
                    d = strchr(d, '=');
                    if (d) {
                        ++d;
                        while (*d && isspace((unsigned char)*d)) ++d;
                        char *endptr = NULL;
                        double val = strtod(d, &endptr);
                        if (endptr != d) {
                            has_drive = 1;
                            if (val > 0.0) {
                                drive_valid = 1;
                            }
                        }
                    }
                }
            }

            if (require_drive) {
                if (!has_drive || !drive_valid) {
                    sem_report_rule(diagnostics,
                                    decl->loc,
                                    "PIN_DRIVE_MISSING_OR_INVALID",
                                    "missing or nonpositive drive value for output/inout PIN");
                }
            }

            /* --- mode --- */
            char mode_val[32];
            int has_mode = sem_extract_attr(attrs, "mode", mode_val, sizeof(mode_val));
            int is_diff = 0;
            if (has_mode) {
                if (strcmp(mode_val, "SINGLE") == 0) {
                    /* ok */
                } else if (strcmp(mode_val, "DIFFERENTIAL") == 0) {
                    is_diff = 1;
                } else {
                    sem_report_rule(diagnostics,
                                    decl->loc,
                                    "PIN_MODE_INVALID",
                                    "pin mode must be SINGLE or DIFFERENTIAL");
                }
            }

            /* Cross-validate mode vs standard */
            if (standard_valid && has_mode) {
                if (is_diff && is_single_standard(standard_val)) {
                    sem_report_rule(diagnostics,
                                    decl->loc,
                                    "PIN_MODE_STANDARD_MISMATCH",
                                    "mode=DIFFERENTIAL conflicts with single-ended I/O standard");
                } else if (!is_diff && is_diff_standard(standard_val)) {
                    sem_report_rule(diagnostics,
                                    decl->loc,
                                    "PIN_MODE_STANDARD_MISMATCH",
                                    "mode=SINGLE conflicts with differential I/O standard");
                }
            }
            /* If no explicit mode but standard is differential, infer diff */
            if (!has_mode && standard_valid && is_diff_standard(standard_val)) {
                is_diff = 1;
            }

            /* --- pull --- */
            char pull_val[32];
            int has_pull = sem_extract_attr(attrs, "pull", pull_val, sizeof(pull_val));
            if (has_pull) {
                if (strcmp(pull_val, "UP") != 0 &&
                    strcmp(pull_val, "DOWN") != 0 &&
                    strcmp(pull_val, "NONE") != 0) {
                    sem_report_rule(diagnostics,
                                    decl->loc,
                                    "PIN_PULL_INVALID",
                                    "pull must be UP, DOWN, or NONE");
                } else if (is_out_block &&
                           (strcmp(pull_val, "UP") == 0 || strcmp(pull_val, "DOWN") == 0)) {
                    sem_report_rule(diagnostics,
                                    decl->loc,
                                    "PIN_PULL_ON_OUTPUT",
                                    "pull resistor not valid on output-only pins (OUT_PINS)");
                }
            }

            /* --- term --- */
            char term_val[32];
            int has_term = sem_extract_attr(attrs, "term", term_val, sizeof(term_val));
            if (has_term) {
                if (strcmp(term_val, "ON") != 0 &&
                    strcmp(term_val, "OFF") != 0) {
                    sem_report_rule(diagnostics,
                                    decl->loc,
                                    "PIN_TERM_INVALID",
                                    "termination must be ON or OFF");
                } else if (strcmp(term_val, "ON") == 0 && is_out_block) {
                    sem_report_rule(diagnostics,
                                    decl->loc,
                                    "PIN_TERM_ON_OUTPUT",
                                    "termination not valid on output-only pins (OUT_PINS)");
                } else if (strcmp(term_val, "ON") == 0 && standard_valid) {
                    /* term=ON only valid for differential standards or
                     * single-ended SSTL / HSTL standards. */
                    if (!is_diff && !is_term_capable_single(standard_val)) {
                        sem_report_rule(diagnostics,
                                        decl->loc,
                                        "PIN_TERM_INVALID_FOR_STANDARD",
                                        "termination only valid for differential or SSTL/HSTL standards");
                    }
                }
            }

            /* --- width (serialization width, differential only) --- */
            char width_val[32];
            int has_width = sem_extract_attr(attrs, "width", width_val, sizeof(width_val));
            if (has_width && !is_diff) {
                sem_report_rule(diagnostics,
                                decl->loc,
                                "PIN_WIDTH_REQUIRES_DIFFERENTIAL",
                                "width attribute is only valid when mode=DIFFERENTIAL");
            }

            /* --- fclk / pclk / reset: required set depends on chip serializer --- */
            if (is_diff && is_out_block) {
                char fclk_val[64], pclk_val[64], reset_val[64];
                int has_fclk  = sem_extract_attr(attrs, "fclk",  fclk_val,  sizeof(fclk_val));
                int has_pclk  = sem_extract_attr(attrs, "pclk",  pclk_val,  sizeof(pclk_val));
                int has_reset = sem_extract_attr(attrs, "reset", reset_val, sizeof(reset_val));

                /* Determine which clocks this serializer actually needs */
                int need_fclk = 1, need_pclk = 1, need_reset = 1;
                if (chip && has_width) {
                    char *endp = NULL;
                    long wn = strtol(width_val, &endp, 10);
                    if (endp && endp != width_val && *endp == '\0' && wn > 0) {
                        int rf = 1, rp = 1, rr = 1;
                        if (jz_chip_diff_serializer_required_clocks(
                                chip, (int)wn, &rf, &rp, &rr)) {
                            need_fclk = rf;
                            need_pclk = rp;
                            need_reset = rr;
                        }
                    }
                }

                if (need_fclk && !has_fclk) {
                    sem_report_rule(diagnostics, decl->loc,
                                    "PIN_DIFF_OUT_MISSING_FCLK",
                                    "differential output pin requires fclk attribute");
                }
                if (need_pclk && !has_pclk) {
                    sem_report_rule(diagnostics, decl->loc,
                                    "PIN_DIFF_OUT_MISSING_PCLK",
                                    "differential output pin requires pclk attribute");
                }
                if (need_reset && !has_reset) {
                    sem_report_rule(diagnostics, decl->loc,
                                    "PIN_DIFF_OUT_MISSING_RESET",
                                    "differential output pin requires reset attribute");
                }
            }
        }
    }
}

static void sem_trim_copy(const char *src, char *dst, size_t dst_size)
{
    if (!dst || dst_size == 0) return;
    size_t len = 0;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    for (const char *p = src; *p && len + 1 < dst_size; ++p) {
        if (!isspace((unsigned char)*p)) {
            dst[len++] = *p;
        }
    }
    dst[len] = '\0';
}

static int sem_parse_map_lhs(const char *lhs,
                             char *out_pin_name,
                             size_t out_name_size,
                             unsigned *out_bit,
                             int *out_has_bit)
{
    if (!lhs || !out_pin_name || out_name_size == 0) return 0;
    *out_has_bit = 0;
    *out_bit = 0;

    char tmp[256];
    sem_trim_copy(lhs, tmp, sizeof(tmp));
    const char *br = strchr(tmp, '[');
    if (!br) {
        strncpy(out_pin_name, tmp, out_name_size - 1);
        out_pin_name[out_name_size - 1] = '\0';
        return 1;
    }

    const char *br_end = strchr(br, ']');
    if (!br_end || br_end <= br + 1) {
        return 0;
    }

    size_t name_len = (size_t)(br - tmp);
    if (name_len == 0 || name_len >= out_name_size) {
        return 0;
    }
    memcpy(out_pin_name, tmp, name_len);
    out_pin_name[name_len] = '\0';

    char idx_buf[64];
    size_t idx_len = (size_t)(br_end - (br + 1));
    if (idx_len == 0 || idx_len >= sizeof(idx_buf)) {
        return 0;
    }
    memcpy(idx_buf, br + 1, idx_len);
    idx_buf[idx_len] = '\0';

    unsigned bit = 0;
    if (!sem_parse_nonnegative_simple(idx_buf, &bit)) {
        return 0;
    }

    *out_bit = bit;
    *out_has_bit = 1;
    return 1;
}

void sem_check_project_map(JZASTNode *project,
                           const JZBuffer *project_symbols,
                           JZDiagnosticList *diagnostics)
{
    if (!project || project->type != JZ_AST_PROJECT || !project_symbols) return;

    JZASTNode *map_blk = NULL;
    for (size_t i = 0; i < project->child_count; ++i) {
        JZASTNode *child = project->children[i];
        if (child && child->type == JZ_AST_MAP_BLOCK) {
            map_blk = child;
            break;
        }
    }

    size_t pin_count = 0;
    const JZSymbol *syms = (const JZSymbol *)project_symbols->data;
    size_t sym_count = project_symbols->len / sizeof(JZSymbol);
    for (size_t i = 0; i < sym_count; ++i) {
        if (syms[i].kind == JZ_SYM_PIN) {
            ++pin_count;
        }
    }

    if (pin_count == 0) {
        return;
    }

    typedef struct JZPinCoverage {
        const JZSymbol *sym;
        unsigned        width;
        unsigned char  *bit_mapped;
    } JZPinCoverage;

    JZPinCoverage *pins = (JZPinCoverage *)calloc(pin_count, sizeof(JZPinCoverage));
    if (!pins) return;

    size_t pi = 0;
    for (size_t i = 0; i < sym_count && pi < pin_count; ++i) {
        if (syms[i].kind != JZ_SYM_PIN) continue;
        pins[pi].sym = &syms[i];
        pins[pi].width = 1u;
        if (syms[i].node && syms[i].node->width) {
            unsigned w = 0;
            int rc = eval_simple_positive_decl_int(syms[i].node->width, &w);
            if (rc == 1 && w > 0u) {
                pins[pi].width = w;
            } else if (rc == -1) {
                sem_report_rule(diagnostics,
                                syms[i].node->loc,
                                "PIN_BUS_WIDTH_INVALID",
                                "bus pin width must be a positive integer");
            }
        }
        /* Allocate a coverage bitmap for all pins with a positive width,
         * including single-bit pins. This allows MAP entries like
         *   clk = 4;
         * to correctly mark scalar pins as mapped instead of spuriously
         * reporting MAP_PIN_DECLARED_NOT_MAPPED.
         */
        if (pins[pi].width >= 1u) {
            pins[pi].bit_mapped = (unsigned char *)calloc(pins[pi].width, sizeof(unsigned char));
        }
        ++pi;
    }

    typedef struct JZPhysLoc {
        char  id[64];
    } JZPhysLoc;
    JZPhysLoc phys[128];
    size_t phys_count = 0;

    if (map_blk) {
        for (size_t i = 0; i < map_blk->child_count; ++i) {
            JZASTNode *entry = map_blk->children[i];
            if (!entry || entry->type != JZ_AST_CONST_DECL || !entry->name) continue;

            char lhs_name[128];
            unsigned bit = 0;
            int has_bit = 0;
            if (!sem_parse_map_lhs(entry->name, lhs_name, sizeof(lhs_name), &bit, &has_bit)) {
                continue;
            }

            const JZSymbol *pin_sym = project_lookup(project_symbols, lhs_name, JZ_SYM_PIN);
            if (!pin_sym) {
                sem_report_rule(diagnostics,
                                entry->loc,
                                "MAP_PIN_MAPPED_NOT_DECLARED",
                                "MAP entry references undeclared pin");
            }

            for (size_t pidx = 0; pidx < pin_count; ++pidx) {
                if (pins[pidx].sym != pin_sym) continue;
                unsigned w = pins[pidx].width;
                if (!has_bit) {
                    if (w > 0 && pins[pidx].bit_mapped) {
                        if (0u < w) pins[pidx].bit_mapped[0] = 1u;
                    }
                } else if (pins[pidx].bit_mapped && bit < w) {
                    pins[pidx].bit_mapped[bit] = 1u;
                }
                break;
            }

            if (entry->text) {
                char rhs[256];
                sem_trim_copy(entry->text, rhs, sizeof(rhs));

                /* Determine if this pin is differential by checking the
                 * pin declaration's mode/standard attributes.
                 */
                int pin_is_diff = 0;
                if (pin_sym && pin_sym->node && pin_sym->node->text) {
                    const char *pattrs = pin_sym->node->text;
                    char mode_v[32];
                    char std_v[64];
                    int has_explicit_mode = sem_extract_attr(pattrs, "mode", mode_v, sizeof(mode_v));
                    if (has_explicit_mode && strcmp(mode_v, "DIFFERENTIAL") == 0) {
                        pin_is_diff = 1;
                    }
                    /* Only infer from standard when no explicit mode is set */
                    if (!has_explicit_mode &&
                        sem_extract_attr(pattrs, "standard", std_v, sizeof(std_v)) &&
                        is_diff_standard(std_v)) {
                        pin_is_diff = 1;
                    }
                }

                int rhs_is_pair = (rhs[0] == '{');
                char p_pin[64] = {0};
                char n_pin[64] = {0};

                if (rhs_is_pair) {
                    /* Parse { P=<id>, N=<id> } */
                    int has_p = 0, has_n = 0;
                    const char *pp = strstr(rhs, "P");
                    if (pp) {
                        pp = strchr(pp, '=');
                        if (pp) {
                            ++pp;
                            while (*pp && isspace((unsigned char)*pp)) ++pp;
                            size_t plen = 0;
                            while (pp[plen] && pp[plen] != ',' && pp[plen] != '}' &&
                                   !isspace((unsigned char)pp[plen])) ++plen;
                            if (plen > 0 && plen < sizeof(p_pin)) {
                                memcpy(p_pin, pp, plen);
                                p_pin[plen] = '\0';
                                has_p = 1;
                            }
                        }
                    }
                    const char *np = strstr(rhs, "N");
                    if (np) {
                        np = strchr(np, '=');
                        if (np) {
                            ++np;
                            while (*np && isspace((unsigned char)*np)) ++np;
                            size_t nlen = 0;
                            while (np[nlen] && np[nlen] != ',' && np[nlen] != '}' &&
                                   !isspace((unsigned char)np[nlen])) ++nlen;
                            if (nlen > 0 && nlen < sizeof(n_pin)) {
                                memcpy(n_pin, np, nlen);
                                n_pin[nlen] = '\0';
                                has_n = 1;
                            }
                        }
                    }
                    if (!has_p || !has_n) {
                        sem_report_rule(diagnostics, entry->loc,
                                        "MAP_DIFF_MISSING_PN",
                                        "differential MAP entry must have both P and N values");
                    } else if (strcmp(p_pin, n_pin) == 0) {
                        sem_report_rule(diagnostics, entry->loc,
                                        "MAP_DIFF_SAME_PIN",
                                        "differential MAP P and N must be different physical pins");
                    }
                    if (!pin_is_diff) {
                        sem_report_rule(diagnostics, entry->loc,
                                        "MAP_SINGLE_UNEXPECTED_PAIR",
                                        "single-ended pin must not use { P, N } MAP syntax");
                    }
                } else {
                    /* Scalar RHS */
                    if (pin_is_diff) {
                        sem_report_rule(diagnostics, entry->loc,
                                        "MAP_DIFF_EXPECTED_PAIR",
                                        "differential pin must use { P=<id>, N=<id> } MAP syntax");
                    }
                }

                /* MAP_INVALID_BOARD_PIN_ID: validate pin ID format.
                 * A valid board pin ID is a non-empty alphanumeric string
                 * (letters, digits, underscores).
                 */
                {
                    const char *ids_to_check[3] = {NULL, NULL, NULL};
                    int id_count = 0;
                    if (rhs_is_pair) {
                        if (p_pin[0]) ids_to_check[id_count++] = p_pin;
                        if (n_pin[0]) ids_to_check[id_count++] = n_pin;
                    } else {
                        ids_to_check[id_count++] = rhs;
                    }
                    for (int ci = 0; ci < id_count; ++ci) {
                        const char *pid = ids_to_check[ci];
                        if (!pid || !*pid) continue;
                        int valid = 1;
                        for (const char *ch = pid; *ch; ++ch) {
                            if (!((*ch >= 'A' && *ch <= 'Z') ||
                                  (*ch >= 'a' && *ch <= 'z') ||
                                  (*ch >= '0' && *ch <= '9') ||
                                  *ch == '_')) {
                                valid = 0;
                                break;
                            }
                        }
                        if (!valid) {
                            char msg[256];
                            snprintf(msg, sizeof(msg),
                                     "'%s' is not a valid board pin ID\n"
                                     "pin IDs must be alphanumeric (letters, digits, underscores)",
                                     pid);
                            sem_report_rule(diagnostics, entry->loc,
                                            "MAP_INVALID_BOARD_PIN_ID", msg);
                        }
                    }
                }

                /* Physical location duplicate tracking */
                if (rhs_is_pair) {
                    /* Track both P and N pins */
                    const char *pair_pins[2] = { p_pin, n_pin };
                    for (int pp_idx = 0; pp_idx < 2; ++pp_idx) {
                        if (pair_pins[pp_idx][0] == '\0') continue;
                        size_t k;
                        for (k = 0; k < phys_count; ++k) {
                            if (strcmp(phys[k].id, pair_pins[pp_idx]) == 0) {
                                sem_report_rule(diagnostics,
                                                entry->loc,
                                                "MAP_DUP_PHYSICAL_LOCATION",
                                                "two logical pins mapped to same physical board pin");
                                break;
                            }
                        }
                        if (k == phys_count && phys_count < sizeof(phys) / sizeof(phys[0])) {
                            strncpy(phys[phys_count].id, pair_pins[pp_idx], sizeof(phys[phys_count].id) - 1);
                            phys[phys_count].id[sizeof(phys[phys_count].id) - 1] = '\0';
                            ++phys_count;
                        }
                    }
                } else {
                    char phys_id[64];
                    sem_trim_copy(entry->text, phys_id, sizeof(phys_id));
                    if (phys_id[0]) {
                        size_t k;
                        for (k = 0; k < phys_count; ++k) {
                            if (strcmp(phys[k].id, phys_id) == 0) {
                                sem_report_rule(diagnostics,
                                                entry->loc,
                                                "MAP_DUP_PHYSICAL_LOCATION",
                                                "two logical pins mapped to same physical board pin");
                                break;
                            }
                        }
                        if (k == phys_count && phys_count < sizeof(phys) / sizeof(phys[0])) {
                            strncpy(phys[phys_count].id, phys_id, sizeof(phys[phys_count].id) - 1);
                            phys[phys_count].id[sizeof(phys[phys_count].id) - 1] = '\0';
                            ++phys_count;
                        }
                    }
                }
            }
        }
    }

    for (size_t pidx = 0; pidx < pin_count; ++pidx) {
        const JZSymbol *sym = pins[pidx].sym;
        if (!sym || !sym->node) continue;
        unsigned w = pins[pidx].width;
        if (w == 0u) continue;

        if (w == 1u) {
            if (!map_blk) {
                sem_report_rule(diagnostics,
                                sym->node->loc,
                                "MAP_PIN_DECLARED_NOT_MAPPED",
                                "pin declared in PIN blocks but not mapped in MAP");
            } else if (!pins[pidx].bit_mapped || pins[pidx].bit_mapped[0] == 0u) {
                sem_report_rule(diagnostics,
                                sym->node->loc,
                                "MAP_PIN_DECLARED_NOT_MAPPED",
                                "pin declared in PIN blocks but not mapped in MAP");
            }
        } else {
            int any_unmapped = 0;
            if (!pins[pidx].bit_mapped) {
                any_unmapped = 1;
            } else {
                for (unsigned b = 0; b < w; ++b) {
                    if (pins[pidx].bit_mapped[b] == 0u) {
                        any_unmapped = 1;
                        break;
                    }
                }
            }
            if (any_unmapped) {
                sem_report_rule(diagnostics,
                                sym->node->loc,
                                "MAP_PIN_DECLARED_NOT_MAPPED",
                                "pin declared in PIN blocks but not fully mapped in MAP");
            }
        }
    }

    for (size_t pidx = 0; pidx < pin_count; ++pidx) {
        free(pins[pidx].bit_mapped);
    }
    free(pins);
}

static int sem_top_binding_has_literal(const char *expr)
{
    if (!expr || !*expr) {
        return 0;
    }

    /* Sized literal patterns always include a tick. */
    if (strchr(expr, '\'')) {
        return 1;
    }

    /* Catch lit(...) in binding expressions. */
    const char *p = expr;
    while ((p = strstr(p, "lit")) != NULL) {
        char before = (p == expr) ? '\0' : p[-1];
        if (!(isalnum((unsigned char)before) || before == '_')) {
            const char *q = p + 3;
            while (*q && isspace((unsigned char)*q)) {
                ++q;
            }
            if (*q == '(') {
                return 1;
            }
        }
        p += 3;
    }

    /* Treat a bare decimal token as a literal (e.g. "0"). */
    const char *scan = expr;
    while (*scan && isspace((unsigned char)*scan)) {
        ++scan;
    }
    if (*scan && isdigit((unsigned char)*scan)) {
        const char *q = scan;
        while (*q && (isdigit((unsigned char)*q) || *q == '_')) {
            ++q;
        }
        while (*q && isspace((unsigned char)*q)) {
            ++q;
        }
        if (*q == '\0') {
            return 1;
        }
    }

    return 0;
}

void sem_check_project_top_new(JZASTNode *project,
                               JZBuffer *module_scopes,
                               const JZBuffer *project_symbols,
                               JZDiagnosticList *diagnostics)
{
    if (!project || project->type != JZ_AST_PROJECT || !project_symbols) return;

    JZASTNode *top_new = sem_find_project_top_new(project);
    if (!top_new) {
        sem_report_rule(diagnostics,
                        project->loc,
                        "PROJECT_MISSING_TOP_MODULE",
                        "project does not declare a top-level @top module binding");
        return;
    }
    if (!top_new->name) return;

    const JZSymbol *top_sym = project_lookup_module_or_blackbox(project_symbols,
                                                                 top_new->name);
    if (!top_sym || !top_sym->node || top_sym->kind != JZ_SYM_MODULE) {
        sem_report_rule(diagnostics,
                        top_new->loc,
                        "INSTANCE_UNDEFINED_MODULE",
                        "top-level @top module not defined in project");
        return;
    }

    JZASTNode *top_mod = top_sym->node;
    const JZModuleScope *top_scope = find_module_scope_for_node(module_scopes, top_mod);
    (void)top_scope;

    JZASTNode **bindings = top_new->children;
    size_t binding_count = top_new->child_count;

    for (size_t i = 0; i < top_mod->child_count; ++i) {
        JZASTNode *blk = top_mod->children[i];
        if (!blk || blk->type != JZ_AST_PORT_BLOCK) continue;

        for (size_t j = 0; j < blk->child_count; ++j) {
            JZASTNode *pd = blk->children[j];
            if (!pd || pd->type != JZ_AST_PORT_DECL || !pd->name) continue;

            int found = 0;
            JZASTNode *bind = NULL;
            for (size_t k = 0; k < binding_count; ++k) {
                JZASTNode *b = bindings[k];
                if (!b || b->type != JZ_AST_PORT_DECL || !b->name) continue;
                if (strcmp(b->name, pd->name) == 0) {
                    found = 1;
                    bind = b;
                    break;
                }
            }

            if (!found) {
                sem_report_rule(diagnostics,
                                pd->loc,
                                "TOP_PORT_NOT_LISTED",
                                "top module port omitted from project-level @top block");
                continue;
            }

            if (pd->width && bind->width) {
                if (sem_expr_has_lit_call(bind->width)) {
                    sem_report_rule(diagnostics,
                                    bind->loc,
                                    "LIT_INVALID_CONTEXT",
                                    "lit() may not be used in @top width expressions");
                    continue;
                }
                unsigned mod_w = 0, bind_w = 0;
                int mod_rc  = eval_simple_positive_decl_int(pd->width, &mod_w);
                int bind_rc = eval_simple_positive_decl_int(bind->width, &bind_w);
                if (mod_rc == 1 && bind_rc == 1 && mod_w != bind_w) {
                    sem_report_rule(diagnostics,
                                    bind->loc,
                                    "TOP_PORT_WIDTH_MISMATCH",
                                    "instantiated top port width does not match module port width");
                }
            }
        }
    }

    for (size_t k = 0; k < binding_count; ++k) {
        JZASTNode *b = bindings[k];
        if (!b || b->type != JZ_AST_PORT_DECL || !b->name) continue;

        /* Skip BUS ports; they have separate validation rules and their text
         * field contains bus metadata rather than a binding target.
         */
        if (b->block_kind && strcmp(b->block_kind, "BUS") == 0) {
            continue;
        }

        const char *port_name = b->name ? b->name : "";
        const char *target_raw = NULL;
        if (b->text && b->text[0] != '\0') {
            target_raw = b->text;
        } else {
            target_raw = port_name;
        }

        /* Normalize the target expression by stripping whitespace so that
         * no-connect placeholders like "_" are recognized even when the
         * parser stored them with surrounding spaces (e.g. "_ ").
         */
        char target_buf[256];
        sem_trim_copy(target_raw, target_buf, sizeof(target_buf));
        const char *target_expr = target_buf;

        /* Validate CONFIG.<name> usage inside project-level @top width
         * expressions. This flags CONFIG_USE_UNDECLARED when the width
         * expression references a CONFIG entry that does not exist in the
         * project CONFIG block.
         */
        if (b->width) {
            if (sem_expr_has_lit_call(b->width)) {
                sem_report_rule(diagnostics,
                                b->loc,
                                "LIT_INVALID_CONTEXT",
                                "lit() may not be used in @top width expressions");
            }
            sem_check_undeclared_config_in_width(b->width,
                                                 b->loc,
                                                 project_symbols,
                                                 diagnostics);
        }

        if (target_expr[0] == '_' && target_expr[1] == '\0') {
            /* TOP_NO_CONNECT_WITHOUT_WIDTH: port explicitly bound to '_' must
             * have a well-formed, statically positive width. We distinguish
             * three cases:
             *   - Missing/empty width (b->width == NULL or all whitespace):
             *       always an error.
             *   - Digits-only width that is non-positive/overflow (rc == -1):
             *       error.
             *   - More complex expressions (including CONST/CONFIG-based):
             *       width is unknown here, so defer to later constant-eval
             *       passes instead of flagging a spurious error.
             */
            const char *wtext = b->width;
            int missing_or_empty = 1;
            if (wtext) {
                /* Check for any non-whitespace character. */
                for (const char *p = wtext; *p; ++p) {
                    if (!isspace((unsigned char)*p)) {
                        missing_or_empty = 0;
                        break;
                    }
                }
            }

            if (missing_or_empty) {
                sem_report_rule(diagnostics,
                                b->loc,
                                "TOP_NO_CONNECT_WITHOUT_WIDTH",
                                "port bound to '_' must specify explicit positive width");
                continue;
            }

            unsigned w = 0;
            int rc = eval_simple_positive_decl_int(b->width, &w);
            if (rc == -1 || (rc == 1 && w == 0u)) {
                sem_report_rule(diagnostics,
                                b->loc,
                                "TOP_NO_CONNECT_WITHOUT_WIDTH",
                                "port bound to '_' must specify explicit positive width");
            }
            /* rc == 0 (non-simple expression) is treated as unknown here. */
            continue;
        }

        /* Treat complex target expressions (those that include indexing or
         * concatenation syntax) as already validated by MAP/PIN rules.
         * However, we still need to ensure that such expressions ultimately
         * reference at least one declared PIN or CLOCK; otherwise, bugs like
         * `~btn[0]` (where `btn` is not a declared pin bus) would silently
         * pass lint.
         */
        int complex_target = 0;
        if (target_expr && strpbrk(target_expr, "~[]{}.,")) {
            complex_target = 1;
        }

        if (complex_target && project_symbols) {
            /* Scan the target expression for any identifier token that
             * corresponds to a declared PIN or CLOCK. If none of the
             * identifiers resolve, report TOP_PORT_PIN_DECL_MISSING so
             * that cases like `~btn[0]` are diagnosed even though they
             * involve indexing or concatenation syntax.
             */
            int has_pin_or_clock = 0;
            const char *scan = target_expr;
            while (scan && *scan) {
                /* Find the start of the next identifier. */
                while (*scan &&
                       !isalpha((unsigned char)*scan) &&
                       *scan != '_') {
                    ++scan;
                }
                if (!*scan) break;

                const char *start = scan;
                size_t len = 0;
                while (scan[len] &&
                       (isalnum((unsigned char)scan[len]) ||
                        scan[len] == '_')) {
                    ++len;
                }

                if (len > 0) {
                    char ident[64];
                    if (len >= sizeof(ident)) {
                        len = sizeof(ident) - 1;
                    }
                    memcpy(ident, start, len);
                    ident[len] = '\0';

                    const JZSymbol *pin_sym_c = project_lookup(project_symbols,
                                                                ident,
                                                                JZ_SYM_PIN);
                    const JZSymbol *clk_sym_c = project_lookup(project_symbols,
                                                                ident,
                                                                JZ_SYM_CLOCK);
                    if (pin_sym_c || clk_sym_c) {
                        has_pin_or_clock = 1;
                        break;
                    }
                }

                scan = start + len;
            }

            if (!has_pin_or_clock) {
                sem_report_rule(diagnostics,
                                b->loc,
                                "TOP_PORT_PIN_DECL_MISSING",
                                "connected top port has no corresponding pin or clock declaration");
            }
        }

        JZASTNode *mod_port = NULL;
        for (size_t i = 0; i < top_mod->child_count && !mod_port; ++i) {
            JZASTNode *blk = top_mod->children[i];
            if (!blk || blk->type != JZ_AST_PORT_BLOCK) continue;
            for (size_t j = 0; j < blk->child_count; ++j) {
                JZASTNode *pd = blk->children[j];
                if (!pd || pd->type != JZ_AST_PORT_DECL || !pd->name) continue;
                if (strcmp(pd->name, port_name) == 0) {
                    mod_port = pd;
                    break;
                }
            }
        }

        if (!mod_port) {
            sem_report_rule(diagnostics,
                            b->loc,
                            "TOP_PORT_NOT_LISTED",
                            "binding name in project-level @top does not match any module port");
            continue;
        }

        const char *dir = mod_port->block_kind ? mod_port->block_kind : "";
        int is_literal = sem_top_binding_has_literal(target_expr);
        if (strcmp(dir, "OUT") == 0 && is_literal) {
            sem_report_rule(diagnostics,
                            b->loc,
                            "TOP_OUT_LITERAL_BINDING",
                            "module OUT port may not be bound to a literal in @top");
            continue;
        }
        /* IN ports may be bound to literals (constant tie-off). */
        if (strcmp(dir, "IN") == 0 && is_literal) {
            continue;
        }
        const JZSymbol *pin_sym = NULL;
        const JZSymbol *clk_sym = NULL;

        if (!complex_target && target_expr[0] != '\0') {
            pin_sym = project_lookup(project_symbols, target_expr, JZ_SYM_PIN);
            clk_sym = project_lookup(project_symbols, target_expr, JZ_SYM_CLOCK);
        }

        int is_cg_wire = (!complex_target && target_expr[0] != '\0')
                       ? is_clock_gen_output(project, target_expr)
                       : 0;

        if (!complex_target && !pin_sym && !clk_sym && !is_cg_wire) {
            sem_report_rule(diagnostics,
                            b->loc,
                            "TOP_PORT_PIN_DECL_MISSING",
                            "connected top port has no corresponding pin or clock declaration");
            continue;
        }

        if (strcmp(dir, "IN") == 0) {
            if (pin_sym) {
                const char *bk = pin_sym->node && pin_sym->node->block_kind ? pin_sym->node->block_kind : "";
                if (strcmp(bk, "IN_PINS") != 0 && strcmp(bk, "INOUT_PINS") != 0) {
                    sem_report_rule(diagnostics,
                                    b->loc,
                                    "TOP_PORT_PIN_DIRECTION_MISMATCH",
                                    "module IN port connected to non-input PIN category");
                }
            } else if (!clk_sym && !is_cg_wire && !complex_target) {
                sem_report_rule(diagnostics,
                                b->loc,
                                "TOP_PORT_PIN_DECL_MISSING",
                                "IN port must connect to IN_PINS, INOUT_PINS, CLOCKS, or CLOCK_GEN WIRE");
            }
        } else if (strcmp(dir, "OUT") == 0) {
            if (!complex_target && (!pin_sym || !pin_sym->node || !pin_sym->node->block_kind)) {
                sem_report_rule(diagnostics,
                                b->loc,
                                "TOP_PORT_PIN_DIRECTION_MISMATCH",
                                "module OUT port must connect to OUT_PINS or INOUT_PINS");
            } else if (!complex_target) {
                const char *bk = pin_sym->node->block_kind;
                if (strcmp(bk, "OUT_PINS") != 0 && strcmp(bk, "INOUT_PINS") != 0) {
                    sem_report_rule(diagnostics,
                                    b->loc,
                                    "TOP_PORT_PIN_DIRECTION_MISMATCH",
                                    "module OUT port must connect to OUT_PINS or INOUT_PINS");
                }
            }
        } else if (strcmp(dir, "INOUT") == 0) {
            if (!complex_target && (!pin_sym || !pin_sym->node || !pin_sym->node->block_kind ||
                                    strcmp(pin_sym->node->block_kind, "INOUT_PINS") != 0)) {
                sem_report_rule(diagnostics,
                                b->loc,
                                "TOP_PORT_PIN_DIRECTION_MISMATCH",
                                "module INOUT port must connect to INOUT_PINS");
            }
        }

        /* Check that the binding width matches the connected pin/signal width.
         * For simple (non-complex) targets bound to a declared pin, compare
         * the @top binding width against the pin declaration width.
         * If the pin has a `width=N` attribute (differential serialization),
         * use N as the effective width instead of the array/scalar width.
         */
        if (!complex_target && pin_sym && pin_sym->node && b->width) {
            unsigned bind_w = 0;
            int bind_rc = eval_simple_positive_decl_int(b->width, &bind_w);
            unsigned pin_w = 1; /* default scalar */
            int pin_rc = 1;
            if (pin_sym->node->width) {
                pin_rc = eval_simple_positive_decl_int(pin_sym->node->width, &pin_w);
            }
            /* Check for width= attribute on differential pins */
            unsigned effective_w = pin_w;
            if (pin_sym->node->text) {
                char pw_val[32];
                if (sem_extract_attr(pin_sym->node->text, "width", pw_val, sizeof(pw_val))) {
                    unsigned dw = 0;
                    if (eval_simple_positive_decl_int(pw_val, &dw) && dw > 0) {
                        effective_w = dw;
                    }
                }
            }
            if (bind_rc == 1 && pin_rc == 1 && bind_w != effective_w) {
                sem_report_rule(diagnostics,
                                b->loc,
                                "TOP_PORT_SIGNAL_WIDTH_MISMATCH",
                                "binding width does not match connected pin/signal width");
            }
        }
    }
}

static int sem_global_expr_mentions_name(const char *expr,
                                          const char *name)
{
    if (!expr || !name || !*name) return 0;
    size_t nlen = strlen(name);
    const char *p = expr;
    for (;;) {
        const char *hit = strstr(p, name);
        if (!hit) break;

        char before = (hit == expr) ? '\0' : hit[-1];
        const char *after_pos = hit + nlen;
        char after = *after_pos;

        int before_ok = !(isalpha((unsigned char)before) ||
                          isdigit((unsigned char)before) ||
                          before == '_');
        int after_ok = !(isalpha((unsigned char)after) ||
                         isdigit((unsigned char)after) ||
                         after == '_');
        if (before_ok && after_ok) {
            return 1;
        }

        p = hit + nlen;
    }
    return 0;
}

static int sem_global_parse_lit_call(const char *expr,
                                     const JZBuffer *project_symbols,
                                     long long *out_width,
                                     long long *out_value,
                                     JZLocation loc,
                                     JZDiagnosticList *diagnostics)
{
    if (!expr || !out_width || !out_value || !diagnostics) return 0;

    const char *p = expr;
    while (*p && isspace((unsigned char)*p)) {
        ++p;
    }
    if (strncmp(p, "lit", 3) != 0) {
        return 0;
    }
    p += 3;
    while (*p && isspace((unsigned char)*p)) {
        ++p;
    }
    if (*p != '(') {
        return 0;
    }
    ++p;

    int depth = 0;
    const char *arg_start = p;
    const char *comma = NULL;
    const char *end = NULL;
    for (; *p; ++p) {
        if (*p == '(') {
            depth++;
        } else if (*p == ')') {
            if (depth == 0) {
                end = p;
                break;
            }
            depth--;
        } else if (*p == ',' && depth == 0 && !comma) {
            comma = p;
        }
    }
    if (!end || !comma) {
        sem_report_rule(diagnostics,
                        loc,
                        "LIT_WIDTH_INVALID",
                        "lit() width must be a positive integer constant expression");
        return -1;
    }

    const char *tail = end + 1;
    while (*tail && isspace((unsigned char)*tail)) {
        ++tail;
    }
    if (*tail != '\0') {
        sem_report_rule(diagnostics,
                        loc,
                        "LIT_WIDTH_INVALID",
                        "lit() width must be a positive integer constant expression");
        return -1;
    }

    const char *w_start = arg_start;
    const char *w_end = comma;
    const char *v_start = comma + 1;
    const char *v_end = end;

    while (w_start < w_end && isspace((unsigned char)*w_start)) w_start++;
    while (w_end > w_start && isspace((unsigned char)w_end[-1])) w_end--;
    while (v_start < v_end && isspace((unsigned char)*v_start)) v_start++;
    while (v_end > v_start && isspace((unsigned char)v_end[-1])) v_end--;

    if (w_start >= w_end || v_start >= v_end) {
        sem_report_rule(diagnostics,
                        loc,
                        "LIT_WIDTH_INVALID",
                        "lit() width must be a positive integer constant expression");
        return -1;
    }

    size_t w_len = (size_t)(w_end - w_start);
    size_t v_len = (size_t)(v_end - v_start);
    char *w_expr = (char *)malloc(w_len + 1);
    char *v_expr = (char *)malloc(v_len + 1);
    if (!w_expr || !v_expr) {
        free(w_expr);
        free(v_expr);
        return -1;
    }
    memcpy(w_expr, w_start, w_len);
    w_expr[w_len] = '\0';
    memcpy(v_expr, v_start, v_len);
    v_expr[v_len] = '\0';

    if (strstr(w_expr, "widthof") || strstr(v_expr, "widthof")) {
        sem_report_rule(diagnostics,
                        loc,
                        "LIT_INVALID_CONTEXT",
                        "lit() may not use widthof() in project-level @global expressions");
        free(w_expr);
        free(v_expr);
        return -1;
    }

    long long w_val = 0;
    long long v_val = 0;
    if (sem_eval_const_expr_in_project(w_expr, project_symbols, &w_val) != 0 || w_val <= 0) {
        sem_report_rule(diagnostics,
                        loc,
                        "LIT_WIDTH_INVALID",
                        "lit() width must be a positive integer constant expression");
        free(w_expr);
        free(v_expr);
        return -1;
    }

    if (sem_eval_const_expr_in_project(v_expr, project_symbols, &v_val) != 0 || v_val < 0) {
        sem_report_rule(diagnostics,
                        loc,
                        "LIT_VALUE_INVALID",
                        "lit() value must be a nonnegative integer constant expression");
        free(w_expr);
        free(v_expr);
        return -1;
    }

    if (w_val < 64) {
        unsigned long long limit = 1ULL << (unsigned)w_val;
        if ((unsigned long long)v_val >= limit) {
            sem_report_rule(diagnostics,
                            loc,
                            "LIT_VALUE_OVERFLOW",
                            "lit() value exceeds declared width");
            free(w_expr);
            free(v_expr);
            return -1;
        }
    }

    *out_width = w_val;
    *out_value = v_val;
    free(w_expr);
    free(v_expr);
    return 1;
}

/* Validate that a GLOBAL initializer is a sized literal of the form
 *   <width>'<base><value>
 * using the same literal width/overflow rules as other literals.
 * Returns 1 on success, 0 on failure (and emits GLOBAL_INVALID_EXPR_TYPE).
 */
static int sem_global_value_is_valid_sized_literal(const char *expr,
                                                   const JZBuffer *project_symbols,
                                                   JZLocation loc,
                                                   JZDiagnosticList *diagnostics)
{
    if (!expr || !diagnostics) return 0;

    long long lit_w = 0;
    long long lit_v = 0;
    int lit_rc = sem_global_parse_lit_call(expr, project_symbols, &lit_w, &lit_v, loc, diagnostics);
    if (lit_rc > 0) {
        return 1;
    }
    if (lit_rc < 0) {
        return 0;
    }

    const char *lex = expr;
    /* Trim leading whitespace. */
    while (*lex && isspace((unsigned char)*lex)) {
        ++lex;
    }
    if (!*lex) {
        sem_report_rule(diagnostics,
                        loc,
                        "GLOBAL_INVALID_EXPR_TYPE",
                        "GLOBAL value must be a sized literal <width>'<base><value>");
        return 0;
    }

    const char *tick = strchr(lex, '\'');
    if (!tick || !tick[1]) {
        sem_report_rule(diagnostics,
                        loc,
                        "GLOBAL_INVALID_EXPR_TYPE",
                        "GLOBAL value must be a sized literal <width>'<base><value>");
        return 0;
    }

    /* Extract and parse width substring before the '\''. */
    size_t width_len = (size_t)(tick - lex);
    if (width_len == 0 || width_len >= 64) {
        sem_report_rule(diagnostics,
                        loc,
                        "GLOBAL_INVALID_EXPR_TYPE",
                        "GLOBAL value must be a sized literal <width>'<base><value>");
        return 0;
    }
    char width_buf[64];
    memcpy(width_buf, lex, width_len);
    width_buf[width_len] = '\0';

    unsigned declared_width = 0;
    int saw_digit = 0;
    for (const char *p = width_buf; *p; ++p) {
        if (isspace((unsigned char)*p)) continue;
        if (*p < '0' || *p > '9') {
            sem_report_rule(diagnostics,
                            loc,
                            "GLOBAL_INVALID_EXPR_TYPE",
                            "GLOBAL value must be a sized literal <width>'<base><value>");
            return 0;
        }
        unsigned d = (unsigned)(*p - '0');
        if (declared_width > (unsigned)(~0u) / 10u ||
            (declared_width == (unsigned)(~0u) / 10u && d > (unsigned)(~0u) % 10u)) {
            sem_report_rule(diagnostics,
                            loc,
                            "GLOBAL_INVALID_EXPR_TYPE",
                            "GLOBAL value must be a sized literal <width>'<base><value>");
            return 0;
        }
        declared_width = declared_width * 10u + d;
        saw_digit = 1;
    }
    if (!saw_digit || declared_width == 0u) {
        sem_report_rule(diagnostics,
                        loc,
                        "GLOBAL_INVALID_EXPR_TYPE",
                        "GLOBAL value must be a sized literal <width>'<base><value>");
        return 0;
    }

    /* Base character immediately after '\''. */
    char base_ch = tick[1];
    JZNumericBase base = JZ_NUM_BASE_NONE;
    if (base_ch == 'b' || base_ch == 'B') base = JZ_NUM_BASE_BIN;
    else if (base_ch == 'd' || base_ch == 'D') base = JZ_NUM_BASE_DEC;
    else if (base_ch == 'h' || base_ch == 'H') base = JZ_NUM_BASE_HEX;
    else {
        sem_report_rule(diagnostics,
                        loc,
                        "GLOBAL_INVALID_EXPR_TYPE",
                        "GLOBAL value must be a sized literal <width>'<base><value>");
        return 0;
    }

    const char *value_lexeme = tick + 2;
    if (!value_lexeme) {
        sem_report_rule(diagnostics,
                        loc,
                        "GLOBAL_INVALID_EXPR_TYPE",
                        "GLOBAL value must be a sized literal <width>'<base><value>");
        return 0;
    }

    /* Trim whitespace from the value portion before analyzing it. The parser
     * preserves token lexemes verbatim and `parse_global` concatenates them
     * with spaces (e.g. "1'b1 "), whereas `jz_literal_analyze` expects a
     * compact value string.
     */
    char value_buf[128];
    size_t vlen = 0;
    for (const char *p = value_lexeme; *p && vlen + 1 < sizeof(value_buf); ++p) {
        if (isspace((unsigned char)*p)) {
            continue;
        }
        value_buf[vlen++] = *p;
    }
    value_buf[vlen] = '\0';
    if (vlen == 0) {
        sem_report_rule(diagnostics,
                        loc,
                        "GLOBAL_INVALID_EXPR_TYPE",
                        "GLOBAL value must be a sized literal <width>'<base><value>");
        return 0;
    }

    unsigned intrinsic = 0;
    JZLiteralExtKind ext = JZ_LITERAL_EXT_NONE;
    if (jz_literal_analyze(base,
                           value_buf,
                           declared_width,
                           &intrinsic,
                           &ext) != 0) {
        sem_report_rule(diagnostics,
                        loc,
                        "GLOBAL_INVALID_EXPR_TYPE",
                        "GLOBAL value must be a sized literal <width>'<base><value>");
        return 0;
    }

    return 1;
}

void sem_check_globals(JZASTNode *project,
                       const JZBuffer *project_symbols,
                       JZDiagnosticList *diagnostics)
{
    if (!project || project->type != JZ_AST_PROJECT) return;

    /* 1. Validate each @global block independently: duplicate const names,
     *    forward references, cycles, and basic constant-eval.
     */
    for (size_t gi = 0; gi < project->child_count; ++gi) {
        JZASTNode *glob = project->children[gi];
        if (!glob || glob->type != JZ_AST_GLOBAL_BLOCK) continue;

        /* Collect CONST decls in this @global block. */
        size_t decl_count = 0;
        for (size_t j = 0; j < glob->child_count; ++j) {
            JZASTNode *decl = glob->children[j];
            if (!decl || decl->type != JZ_AST_CONST_DECL || !decl->name) continue;
            ++decl_count;
        }
        if (decl_count == 0) {
            continue;
        }

        JZASTNode **decls = (JZASTNode **)calloc(decl_count, sizeof(JZASTNode *));
        if (!decls) {
            return;
        }

        size_t idx = 0;
        for (size_t j = 0; j < glob->child_count && idx < decl_count; ++j) {
            JZASTNode *decl = glob->children[j];
            if (!decl || decl->type != JZ_AST_CONST_DECL || !decl->name) continue;
            decls[idx++] = decl;
        }
        decl_count = idx;
        if (decl_count == 0) {
            free(decls);
            continue;
        }

        /* Duplicate constant names within a single @global block. */
        for (size_t i = 0; i < decl_count; ++i) {
            for (size_t j = i + 1; j < decl_count; ++j) {
                if (decls[i]->name && decls[j]->name &&
                    strcmp(decls[i]->name, decls[j]->name) == 0) {
                    sem_report_rule(diagnostics,
                                    decls[j]->loc,
                                    "GLOBAL_CONST_NAME_DUPLICATE",
                                    "duplicate constant name inside @global block");
                }
            }
        }

        /* Dependency graph for forward-ref and cycle detection. */
        unsigned char *edges = (unsigned char *)calloc(decl_count * decl_count,
                                                       sizeof(unsigned char));
        int *has_forward_ref = (int *)calloc(decl_count, sizeof(int));
        int *has_cycle = (int *)calloc(decl_count, sizeof(int));
        int any_static_error = 0;
        if (!edges || !has_forward_ref || !has_cycle) {
            free(decls);
            free(edges);
            free(has_forward_ref);
            free(has_cycle);
            return;
        }

        for (size_t i = 0; i < decl_count; ++i) {
            JZASTNode *decl = decls[i];
            if (!decl) continue;
            const char *expr_text = decl->text;
            if (!expr_text) continue;

            for (size_t j = 0; j < decl_count; ++j) {
                JZASTNode *dep = decls[j];
                if (!dep || !dep->name) continue;
                if (!sem_global_expr_mentions_name(expr_text, dep->name)) continue;

                size_t idx_e = i * decl_count + j;
                edges[idx_e] = 1u;
                if (j > i && !has_forward_ref[i]) {
                    sem_report_rule(diagnostics,
                                    decl->loc,
                                    "GLOBAL_FORWARD_REF",
                                    "GLOBAL entry references later constant in same @global block");
                    has_forward_ref[i] = 1;
                    any_static_error = 1;
                }
            }
        }

        /* Cycle detection (same pattern as CONFIG). */
        int *visit = (int *)calloc(decl_count, sizeof(int));
        if (visit) {
            for (size_t i = 0; i < decl_count; ++i) {
                if (visit[i] != 0) continue;

                size_t *stack = (size_t *)malloc(decl_count * sizeof(size_t));
                size_t *iter  = (size_t *)malloc(decl_count * sizeof(size_t));
                if (!stack || !iter) {
                    free(stack);
                    free(iter);
                    break;
                }
                size_t sp = 0;
                stack[sp] = i;
                iter[sp] = 0;
                visit[i] = 1;

                while (sp < decl_count) {
                    size_t v = stack[sp];
                    size_t j = iter[sp];
                    if (j >= decl_count) {
                        visit[v] = 2;
                        if (sp == 0) {
                            break;
                        }
                        --sp;
                        continue;
                    }
                    iter[sp] = j + 1;
                    if (!edges[v * decl_count + j]) {
                        continue;
                    }
                    if (visit[j] == 1) {
                        if (!has_cycle[v]) {
                            JZASTNode *decl = decls[v];
                            if (decl) {
                                sem_report_rule(diagnostics,
                                                decl->loc,
                                                "GLOBAL_CIRCULAR_DEP",
                                                "circular dependency between GLOBAL entries");
                            }
                            has_cycle[v] = 1;
                            any_static_error = 1;
                        }
                        continue;
                    }
                    if (visit[j] == 0) {
                        ++sp;
                        stack[sp] = j;
                        iter[sp] = 0;
                        visit[j] = 1;
                    }
                }

                free(stack);
                free(iter);
            }
            free(visit);
        }

        if (!any_static_error) {
            /* Validate that each GLOBAL value is a sized literal <width>'<base><value>. */
            for (size_t i = 0; i < decl_count; ++i) {
                JZASTNode *decl = decls[i];
                if (!decl) continue;
                const char *expr_text = decl->text;
                if (!expr_text) {
                    sem_report_rule(diagnostics,
                                    decl->loc,
                                    "GLOBAL_INVALID_EXPR_TYPE",
                                    "GLOBAL value must be a sized literal <width>'<base><value>");
                    continue;
                }
                (void)sem_global_value_is_valid_sized_literal(expr_text, project_symbols, decl->loc, diagnostics);
            }
        }

        free(edges);
        free(has_forward_ref);
        free(has_cycle);
        free(decls);
    }

    /* 2. Forbid use of GLOBAL.<name> in CONFIG expressions. */
    if (project_symbols && project_symbols->data) {
        for (size_t i = 0; i < project->child_count; ++i) {
            JZASTNode *child = project->children[i];
            if (!child || child->type != JZ_AST_CONFIG_BLOCK) continue;

            for (size_t j = 0; j < child->child_count; ++j) {
                JZASTNode *decl = child->children[j];
                if (!decl || decl->type != JZ_AST_CONST_DECL) continue;
                const char *expr_text = decl->text;
                if (!expr_text) continue;

                if (sem_expr_has_global_ref(expr_text, project_symbols)) {
                    sem_report_rule(diagnostics,
                                    decl->loc,
                                    "GLOBAL_USED_WHERE_FORBIDDEN",
                                    "GLOBAL.<name> may not be used in CONFIG expressions");
                }
            }
        }
    }
}
