/**
 * @file template_expand.c
 * @brief Template expansion pass — AST-to-AST rewrite.
 *
 * Implements Section 10 of the JZ-HDL specification. After parsing
 * produces an AST with JZ_AST_TEMPLATE_DEF / JZ_AST_TEMPLATE_APPLY /
 * JZ_AST_SCRATCH_DECL nodes, this pass:
 *
 *   1. Collects all template definitions (file-scoped and module-scoped).
 *   2. For each @apply, resolves the template, evaluates the optional
 *      count expression, and for each iteration clones the template
 *      body, substitutes parameters with argument expressions, replaces
 *      IDX with the iteration literal, and renames scratch wires.
 *   3. Materializes scratch wires as module-level WIRE declarations.
 *   4. Removes template definition nodes from the AST.
 *
 * After this pass, the AST contains no template-related nodes and can
 * proceed directly to semantic analysis.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "template_expand.h"
#include "rules.h"

/* ── Random suffix for scratch wire names ────────────────────────── */

static int rng_seeded = 0;

static void generate_random_suffix(char out[7])
{
    static const char chars[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    for (int i = 0; i < 6; i++)
        out[i] = chars[rand() % 62];
    out[6] = '\0';
}

/* ── Template registry ───────────────────────────────────────────── */

typedef struct TemplateEntry {
    const char  *name;          /* template identifier */
    JZASTNode   *def_node;      /* pointer into AST (JZ_AST_TEMPLATE_DEF) */
    JZASTNode   *scope_module;  /* owning module, or NULL for file-scoped */
} TemplateEntry;

typedef struct ExpandContext {
    TemplateEntry  *templates;
    size_t          template_count;
    size_t          template_cap;
    JZDiagnosticList *diagnostics;
    const char      *filename;

    /* Lightweight CONST/CONFIG table for count evaluation */
    struct { const char *name; long value; } *consts;
    size_t const_count;
    size_t const_cap;

    /* Per-module scratch wire collection */
    JZASTNode **scratch_wires;
    size_t      scratch_wire_count;
    size_t      scratch_wire_cap;

    int apply_counter;  /* unique callsite id */
} ExpandContext;

static void report_rule(ExpandContext *ctx, JZLocation loc,
                         const char *rule_id, const char *fallback)
{
    if (!ctx || !ctx->diagnostics || !rule_id) return;
    const JZRuleInfo *rule = jz_rule_lookup(rule_id);
    JZSeverity sev = JZ_SEVERITY_ERROR;
    const char *msg = fallback;
    if (rule) {
        sev = (rule->mode == JZ_RULE_MODE_WRN) ? JZ_SEVERITY_WARNING : JZ_SEVERITY_ERROR;
        if (rule->description) msg = rule->description;
    }
    if (!msg) msg = rule_id;
    jz_diagnostic_report(ctx->diagnostics, loc, sev, rule_id, msg);
}

/* ── CONST/CONFIG table for count evaluation ─────────────────────── */

static void collect_const_value(ExpandContext *ctx, const char *name, long value)
{
    if (ctx->const_count == ctx->const_cap) {
        size_t new_cap = ctx->const_cap ? ctx->const_cap * 2 : 64;
        void *p = realloc(ctx->consts, new_cap * sizeof(ctx->consts[0]));
        if (!p) return;
        ctx->consts = p;
        ctx->const_cap = new_cap;
    }
    ctx->consts[ctx->const_count].name = name;
    ctx->consts[ctx->const_count].value = value;
    ctx->const_count++;
}

static int lookup_const(ExpandContext *ctx, const char *name, long *out)
{
    for (size_t i = 0; i < ctx->const_count; i++) {
        if (strcmp(ctx->consts[i].name, name) == 0) {
            *out = ctx->consts[i].value;
            return 1;
        }
    }
    return 0;
}

/**
 * Try to parse a simple integer from a string.
 */
static int parse_int_str(const char *s, long *out)
{
    if (!s || !*s) return 0;
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end && *end == '\0') {
        *out = v;
        return 1;
    }
    return 0;
}

/**
 * Walk the AST and collect CONST and CONFIG values for count evaluation.
 */
static void collect_consts_from_ast(ExpandContext *ctx, JZASTNode *node)
{
    if (!node) return;

    if (node->type == JZ_AST_CONST_DECL && node->name && node->text) {
        long v = 0;
        if (parse_int_str(node->text, &v)) {
            collect_const_value(ctx, node->name, v);
        }
    }

    /* CONFIG entries are also CONST_DECL children of CONFIG_BLOCK */
    for (size_t i = 0; i < node->child_count; i++) {
        collect_consts_from_ast(ctx, node->children[i]);
    }
}

/* ── Evaluate count expression ───────────────────────────────────── */

/**
 * Evaluate a simple count expression. Supports:
 * - Integer literals
 * - CONST names
 * - CONFIG.name (as "CONFIG.name")
 * - Simple arithmetic (+, -, *)
 *
 * Returns 1 on success with *out set, 0 on failure.
 */
static int eval_count_expr(ExpandContext *ctx, const char *expr, long *out)
{
    if (!expr || !*expr) return 0;

    /* Strip whitespace (there shouldn't be any from token concatenation) */
    while (*expr && isspace((unsigned char)*expr)) expr++;
    if (!*expr) return 0;

    /* Try plain integer */
    if (parse_int_str(expr, out)) return 1;

    /* Try CONST/CONFIG lookup */
    long v = 0;
    if (lookup_const(ctx, expr, &v)) {
        *out = v;
        return 1;
    }

    /* Try CONFIG.name format */
    if (strncmp(expr, "CONFIG.", 7) == 0) {
        if (lookup_const(ctx, expr, &v)) {
            *out = v;
            return 1;
        }
        /* Also try just the config name after "CONFIG." */
        const char *config_name = expr + 7;
        if (lookup_const(ctx, config_name, &v)) {
            *out = v;
            return 1;
        }
    }

    /* Try simple binary operations: find + or - not inside parens */
    int paren_depth = 0;
    const char *last_plus = NULL;
    const char *last_minus = NULL;
    for (const char *p = expr; *p; p++) {
        if (*p == '(') paren_depth++;
        else if (*p == ')') paren_depth--;
        else if (paren_depth == 0) {
            if (*p == '+') last_plus = p;
            else if (*p == '-' && p != expr) last_minus = p;
        }
    }

    if (last_plus) {
        char *left = (char *)malloc((size_t)(last_plus - expr) + 1);
        if (!left) return 0;
        memcpy(left, expr, (size_t)(last_plus - expr));
        left[last_plus - expr] = '\0';
        const char *right = last_plus + 1;
        long lv = 0, rv = 0;
        int ok = eval_count_expr(ctx, left, &lv) && eval_count_expr(ctx, right, &rv);
        free(left);
        if (ok) { *out = lv + rv; return 1; }
    }

    if (last_minus) {
        char *left = (char *)malloc((size_t)(last_minus - expr) + 1);
        if (!left) return 0;
        memcpy(left, expr, (size_t)(last_minus - expr));
        left[last_minus - expr] = '\0';
        const char *right = last_minus + 1;
        long lv = 0, rv = 0;
        int ok = eval_count_expr(ctx, left, &lv) && eval_count_expr(ctx, right, &rv);
        free(left);
        if (ok) { *out = lv - rv; return 1; }
    }

    /* Try * */
    const char *last_star = NULL;
    paren_depth = 0;
    for (const char *p = expr; *p; p++) {
        if (*p == '(') paren_depth++;
        else if (*p == ')') paren_depth--;
        else if (paren_depth == 0 && *p == '*') last_star = p;
    }
    if (last_star) {
        char *left = (char *)malloc((size_t)(last_star - expr) + 1);
        if (!left) return 0;
        memcpy(left, expr, (size_t)(last_star - expr));
        left[last_star - expr] = '\0';
        const char *right = last_star + 1;
        long lv = 0, rv = 0;
        int ok = eval_count_expr(ctx, left, &lv) && eval_count_expr(ctx, right, &rv);
        free(left);
        if (ok) { *out = lv * rv; return 1; }
    }

    return 0;
}

/* ── Deep clone ─────────────────────────────────────────────────── */

static JZASTNode *ast_deep_clone(const JZASTNode *src)
{
    if (!src) return NULL;

    JZASTNode *dst = jz_ast_new(src->type, src->loc);
    if (!dst) return NULL;

    if (src->name)       jz_ast_set_name(dst, src->name);
    if (src->block_kind) jz_ast_set_block_kind(dst, src->block_kind);
    if (src->text)       jz_ast_set_text(dst, src->text);
    if (src->width)      jz_ast_set_width(dst, src->width);

    for (size_t i = 0; i < src->child_count; i++) {
        JZASTNode *child_clone = ast_deep_clone(src->children[i]);
        if (!child_clone) {
            jz_ast_free(dst);
            return NULL;
        }
        if (jz_ast_add_child(dst, child_clone) != 0) {
            jz_ast_free(child_clone);
            jz_ast_free(dst);
            return NULL;
        }
    }

    return dst;
}

/* ── String substitution helpers ─────────────────────────────────── */

/**
 * Whole-word replacement in a string. Replaces all occurrences of `old`
 * with `new_str` where `old` is bounded by non-identifier characters.
 * Returns a new heap-allocated string.
 */
static char *string_replace_whole_word(const char *src, const char *old,
                                       const char *new_str)
{
    if (!src || !old || !new_str) return src ? strdup(src) : NULL;

    size_t old_len = strlen(old);
    size_t new_len = strlen(new_str);
    if (old_len == 0) return strdup(src);

    /* Count occurrences to pre-allocate */
    size_t count = 0;
    const char *p = src;
    while ((p = strstr(p, old)) != NULL) {
        /* Check word boundary before */
        if (p > src) {
            char before = *(p - 1);
            if (isalnum((unsigned char)before) || before == '_') {
                p += old_len;
                continue;
            }
        }
        /* Check word boundary after */
        char after = *(p + old_len);
        if (isalnum((unsigned char)after) || after == '_') {
            p += old_len;
            continue;
        }
        count++;
        p += old_len;
    }

    if (count == 0) return strdup(src);

    size_t src_len = strlen(src);
    size_t result_len = src_len + count * (new_len - old_len);
    char *result = (char *)malloc(result_len + 1);
    if (!result) return NULL;

    char *dst = result;
    p = src;
    while (*p) {
        const char *found = strstr(p, old);
        if (!found) {
            strcpy(dst, p);
            break;
        }

        /* Check word boundaries */
        int is_word = 1;
        if (found > src) {
            char before = *(found - 1);
            if (isalnum((unsigned char)before) || before == '_') is_word = 0;
        }
        char after = *(found + old_len);
        if (isalnum((unsigned char)after) || after == '_') is_word = 0;

        if (!is_word) {
            size_t skip = (size_t)(found - p) + old_len;
            memcpy(dst, p, skip);
            dst += skip;
            p = found + old_len;
            continue;
        }

        size_t prefix = (size_t)(found - p);
        memcpy(dst, p, prefix);
        dst += prefix;
        memcpy(dst, new_str, new_len);
        dst += new_len;
        p = found + old_len;
    }

    if (*p == '\0' && dst != result + result_len) {
        /* Already handled by strcpy above */
    }
    result[result_len] = '\0';

    return result;
}

/* ── Parameter substitution ──────────────────────────────────────── */

/**
 * Recursively walk a cloned AST subtree and replace:
 * - Identifier nodes matching param names with cloned arg expressions
 * - "IDX" identifiers with integer literal for current iteration
 * - Scratch wire names with mangled unique names
 * - Width strings with param/IDX substitution
 */
static void substitute_in_node(JZASTNode *node,
                                const char **param_names,
                                JZASTNode **arg_exprs,
                                size_t param_count,
                                int idx_value,
                                int has_idx,
                                const char *tmpl_name,
                                int callsite_id,
                                const char **scratch_names,
                                size_t scratch_count,
                                const char (*scratch_suffixes)[7])
{
    if (!node) return;

    /* Substitute width strings (string-level replacement) */
    if (node->width) {
        char *w = strdup(node->width);
        for (size_t i = 0; i < param_count && w; i++) {
            char *nw = string_replace_whole_word(w, param_names[i],
                                                  arg_exprs[i]->name ? arg_exprs[i]->name :
                                                  (arg_exprs[i]->text ? arg_exprs[i]->text : "0"));
            free(w);
            w = nw;
        }
        if (has_idx && w) {
            char idx_str[32];
            snprintf(idx_str, sizeof(idx_str), "%d", idx_value);
            char *nw = string_replace_whole_word(w, "IDX", idx_str);
            free(w);
            w = nw;
        }
        if (w) {
            jz_ast_set_width(node, w);
            free(w);
        }
    }

    /* Substitute text strings (e.g. CONST_DECL values, config text) */
    if (node->text && node->type != JZ_AST_TEMPLATE_APPLY) {
        char *t = strdup(node->text);
        for (size_t i = 0; i < param_count && t; i++) {
            char *nt = string_replace_whole_word(t, param_names[i],
                                                  arg_exprs[i]->name ? arg_exprs[i]->name :
                                                  (arg_exprs[i]->text ? arg_exprs[i]->text : "0"));
            free(t);
            t = nt;
        }
        if (has_idx && t) {
            char idx_str[32];
            snprintf(idx_str, sizeof(idx_str), "%d", idx_value);
            char *nt = string_replace_whole_word(t, "IDX", idx_str);
            free(t);
            t = nt;
        }
        if (t) {
            jz_ast_set_text(node, t);
            free(t);
        }
    }

    /* Handle identifier and expression nodes */
    if (node->type == JZ_AST_EXPR_IDENTIFIER && node->name) {
        /* Check if this is IDX */
        if (has_idx && strcmp(node->name, "IDX") == 0) {
            char idx_str[32];
            snprintf(idx_str, sizeof(idx_str), "%d", idx_value);
            node->type = JZ_AST_EXPR_LITERAL;
            jz_ast_set_name(node, NULL);
            jz_ast_set_text(node, idx_str);
            return;
        }

        /* Check if this is a param name */
        for (size_t i = 0; i < param_count; i++) {
            if (strcmp(node->name, param_names[i]) == 0) {
                /* Replace this node's content with the arg expression.
                 * We do a shallow copy of the arg expression's fields. */
                JZASTNode *arg_clone = ast_deep_clone(arg_exprs[i]);
                if (arg_clone) {
                    node->type = arg_clone->type;
                    jz_ast_set_name(node, arg_clone->name);
                    jz_ast_set_text(node, arg_clone->text);
                    jz_ast_set_width(node, arg_clone->width);
                    jz_ast_set_block_kind(node, arg_clone->block_kind);
                    /* Move children */
                    for (size_t c = 0; c < node->child_count; c++) {
                        jz_ast_free(node->children[c]);
                    }
                    free(node->children);
                    node->children = arg_clone->children;
                    node->child_count = arg_clone->child_count;
                    node->child_capacity = arg_clone->child_capacity;
                    /* Detach children from arg_clone so they aren't freed */
                    arg_clone->children = NULL;
                    arg_clone->child_count = 0;
                    arg_clone->child_capacity = 0;
                    jz_ast_free(arg_clone);
                }
                return;
            }
        }

        /* Check if this is a scratch wire name */
        for (size_t i = 0; i < scratch_count; i++) {
            if (strcmp(node->name, scratch_names[i]) == 0) {
                char mangled[512];
                if (scratch_suffixes) {
                    snprintf(mangled, sizeof(mangled), "%s__%s__%d_%d_%s",
                             tmpl_name, scratch_names[i], callsite_id,
                             idx_value, scratch_suffixes[i]);
                } else {
                    snprintf(mangled, sizeof(mangled), "%s__%s__%d_%d",
                             tmpl_name, scratch_names[i], callsite_id, idx_value);
                }
                jz_ast_set_name(node, mangled);
                return;
            }
        }
    }

    /* For qualified identifiers, check if the first part matches a param */
    if (node->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER && node->name) {
        for (size_t i = 0; i < param_count; i++) {
            size_t plen = strlen(param_names[i]);
            if (strncmp(node->name, param_names[i], plen) == 0 &&
                node->name[plen] == '.') {
                /* Save suffix to local buffer before any jz_ast_set_name
                 * call frees node->name (suffix points into it). */
                char suffix_buf[256];
                strncpy(suffix_buf, node->name + plen + 1, sizeof(suffix_buf) - 1);
                suffix_buf[sizeof(suffix_buf) - 1] = '\0';
                const char *suffix = suffix_buf;
                JZASTNode *arg = arg_exprs[i];

                if (arg->type == JZ_AST_EXPR_IDENTIFIER && arg->name) {
                    /* Case 1: simple identifier arg (e.g., "src")
                     * -> keep as QualifiedIdentifier("src.ADDR") */
                    size_t alen = strlen(arg->name);
                    size_t suffix_len = strlen(node->name + plen);
                    char *new_name = (char *)malloc(alen + suffix_len + 1);
                    if (new_name) {
                        memcpy(new_name, arg->name, alen);
                        memcpy(new_name + alen, node->name + plen, suffix_len + 1);
                        jz_ast_set_name(node, new_name);
                        free(new_name);
                    }
                } else if (arg->type == JZ_AST_EXPR_SLICE &&
                           arg->child_count >= 2 &&
                           arg->children[0] &&
                           arg->children[0]->type == JZ_AST_EXPR_IDENTIFIER &&
                           arg->children[0]->name) {
                    /* Case 2: slice with identifier base (e.g., "src[oh2b(x)]")
                     * SLICE children: [0]=base ident, [1]=MSB index, [2]=LSB index
                     * -> transform to BUS_ACCESS(name="src", text="ADDR",
                     *    children=[cloned MSB index]) */
                    node->type = JZ_AST_EXPR_BUS_ACCESS;
                    jz_ast_set_name(node, arg->children[0]->name);
                    jz_ast_set_text(node, suffix);
                    /* Clear existing children */
                    for (size_t c = 0; c < node->child_count; c++) {
                        jz_ast_free(node->children[c]);
                    }
                    free(node->children);
                    node->children = NULL;
                    node->child_count = 0;
                    node->child_capacity = 0;
                    /* Clone MSB index as the BUS_ACCESS child */
                    JZASTNode *idx_clone = ast_deep_clone(arg->children[1]);
                    if (idx_clone) {
                        jz_ast_add_child(node, idx_clone);
                    }
                    /* Recurse into newly-added children for further substitution */
                    for (size_t c = 0; c < node->child_count; c++) {
                        substitute_in_node(node->children[c], param_names,
                                           arg_exprs, param_count, idx_value,
                                           has_idx, tmpl_name, callsite_id,
                                           scratch_names, scratch_count,
                                           scratch_suffixes);
                    }
                } else if (arg->type == JZ_AST_EXPR_BUS_ACCESS && arg->name) {
                    /* Case 3: arg is already a BUS_ACCESS (e.g., "src[0]")
                     * -> transform to BUS_ACCESS(name=arg.name, text=suffix,
                     *    children=[cloned arg children]) */
                    node->type = JZ_AST_EXPR_BUS_ACCESS;
                    jz_ast_set_name(node, arg->name);
                    jz_ast_set_text(node, suffix);
                    /* Clear existing children */
                    for (size_t c = 0; c < node->child_count; c++) {
                        jz_ast_free(node->children[c]);
                    }
                    free(node->children);
                    node->children = NULL;
                    node->child_count = 0;
                    node->child_capacity = 0;
                    /* Clone arg's children */
                    for (size_t c = 0; c < arg->child_count; c++) {
                        JZASTNode *child_clone = ast_deep_clone(arg->children[c]);
                        if (child_clone) {
                            jz_ast_add_child(node, child_clone);
                        }
                    }
                    /* Recurse into newly-added children for further substitution */
                    for (size_t c = 0; c < node->child_count; c++) {
                        substitute_in_node(node->children[c], param_names,
                                           arg_exprs, param_count, idx_value,
                                           has_idx, tmpl_name, callsite_id,
                                           scratch_names, scratch_count,
                                           scratch_suffixes);
                    }
                }
                return;
            }
        }
    }

    /* For bus access, check if the base name matches a param */
    if (node->type == JZ_AST_EXPR_BUS_ACCESS && node->name) {
        for (size_t i = 0; i < param_count; i++) {
            if (strcmp(node->name, param_names[i]) == 0 && arg_exprs[i]->name) {
                jz_ast_set_name(node, arg_exprs[i]->name);
                break;
            }
        }
    }

    /* Also substitute in node->name for non-expression nodes that might have
     * scratch wire names in them (e.g., lvalue identifiers used in assignments) */
    if (node->name && node->type != JZ_AST_EXPR_IDENTIFIER &&
        node->type != JZ_AST_EXPR_QUALIFIED_IDENTIFIER &&
        node->type != JZ_AST_EXPR_BUS_ACCESS) {
        for (size_t i = 0; i < scratch_count; i++) {
            if (strcmp(node->name, scratch_names[i]) == 0) {
                char mangled[512];
                if (scratch_suffixes) {
                    snprintf(mangled, sizeof(mangled), "%s__%s__%d_%d_%s",
                             tmpl_name, scratch_names[i], callsite_id,
                             idx_value, scratch_suffixes[i]);
                } else {
                    snprintf(mangled, sizeof(mangled), "%s__%s__%d_%d",
                             tmpl_name, scratch_names[i], callsite_id, idx_value);
                }
                jz_ast_set_name(node, mangled);
                break;
            }
        }
    }

    /* Recurse into children */
    for (size_t i = 0; i < node->child_count; i++) {
        substitute_in_node(node->children[i], param_names, arg_exprs,
                           param_count, idx_value, has_idx, tmpl_name,
                           callsite_id, scratch_names, scratch_count,
                           scratch_suffixes);
    }
}

/* ── Template collection ─────────────────────────────────────────── */

static void register_template(ExpandContext *ctx, JZASTNode *def, JZASTNode *scope_module)
{
    if (ctx->template_count == ctx->template_cap) {
        size_t new_cap = ctx->template_cap ? ctx->template_cap * 2 : 16;
        void *p = realloc(ctx->templates, new_cap * sizeof(TemplateEntry));
        if (!p) return;
        ctx->templates = p;
        ctx->template_cap = new_cap;
    }

    /* Check for duplicates in same scope */
    for (size_t i = 0; i < ctx->template_count; i++) {
        if (ctx->templates[i].scope_module == scope_module &&
            ctx->templates[i].name && def->name &&
            strcmp(ctx->templates[i].name, def->name) == 0) {
            report_rule(ctx, def->loc, "TEMPLATE_DUP_NAME",
                        "duplicate template name in the same scope");
            return;
        }
    }

    /* Check for duplicate parameter names */
    for (size_t i = 0; i < def->child_count; i++) {
        if (def->children[i]->type != JZ_AST_TEMPLATE_PARAM) continue;
        for (size_t j = i + 1; j < def->child_count; j++) {
            if (def->children[j]->type != JZ_AST_TEMPLATE_PARAM) continue;
            if (def->children[i]->name && def->children[j]->name &&
                strcmp(def->children[i]->name, def->children[j]->name) == 0) {
                report_rule(ctx, def->children[j]->loc, "TEMPLATE_DUP_PARAM",
                            "duplicate parameter name in template definition");
            }
        }
    }

    ctx->templates[ctx->template_count].name = def->name;
    ctx->templates[ctx->template_count].def_node = def;
    ctx->templates[ctx->template_count].scope_module = scope_module;
    ctx->template_count++;
}

static void collect_templates(ExpandContext *ctx, JZASTNode *node, JZASTNode *current_module)
{
    if (!node) return;

    if (node->type == JZ_AST_TEMPLATE_DEF) {
        register_template(ctx, node, current_module);
        return; /* Don't recurse into template bodies */
    }

    JZASTNode *mod = current_module;
    if (node->type == JZ_AST_MODULE) {
        mod = node;
    }

    for (size_t i = 0; i < node->child_count; i++) {
        collect_templates(ctx, node->children[i], mod);
    }
}

static TemplateEntry *find_template(ExpandContext *ctx, const char *name, JZASTNode *scope_module)
{
    /* Module-scoped templates shadow file-scoped ones */
    TemplateEntry *file_scoped = NULL;
    for (size_t i = 0; i < ctx->template_count; i++) {
        if (strcmp(ctx->templates[i].name, name) == 0) {
            if (ctx->templates[i].scope_module == scope_module) {
                return &ctx->templates[i];
            }
            if (ctx->templates[i].scope_module == NULL) {
                file_scoped = &ctx->templates[i];
            }
        }
    }
    return file_scoped;
}

/* ── Scratch wire materialization ────────────────────────────────── */

static void add_scratch_wire(ExpandContext *ctx, JZASTNode *wire_decl)
{
    if (ctx->scratch_wire_count == ctx->scratch_wire_cap) {
        size_t new_cap = ctx->scratch_wire_cap ? ctx->scratch_wire_cap * 2 : 16;
        void *p = realloc(ctx->scratch_wires, new_cap * sizeof(JZASTNode *));
        if (!p) return;
        ctx->scratch_wires = p;
        ctx->scratch_wire_cap = new_cap;
    }
    ctx->scratch_wires[ctx->scratch_wire_count++] = wire_decl;
}

/* ── Template body identifier validation ─────────────────────────── */

/**
 * Check whether `name` is allowed inside a template body.
 * Allowed: parameter names, scratch wire names, "IDX" (when has_idx),
 * and compile-time constants known to the expansion context.
 */
static int is_allowed_template_ident(ExpandContext *ctx,
                                      const char *name,
                                      const char **param_names,
                                      size_t param_count,
                                      const char **scratch_names,
                                      size_t scratch_count,
                                      int has_idx)
{
    if (!name) return 1;  /* no name → nothing to check */

    if (has_idx && strcmp(name, "IDX") == 0) return 1;

    for (size_t i = 0; i < param_count; i++) {
        if (strcmp(name, param_names[i]) == 0) return 1;
    }
    for (size_t i = 0; i < scratch_count; i++) {
        if (strcmp(name, scratch_names[i]) == 0) return 1;
    }

    /* Compile-time constants (CONST, CONFIG) are allowed in expressions */
    long v;
    if (lookup_const(ctx, name, &v)) return 1;

    return 0;
}

/**
 * Walk template body AST and report any identifier that references a signal
 * outside the template's parameter list or scratch wires.
 * Called once per template definition to enforce S10.3 isolation.
 */
static void validate_template_body_refs(ExpandContext *ctx,
                                         JZASTNode *node,
                                         const char **param_names,
                                         size_t param_count,
                                         const char **scratch_names,
                                         size_t scratch_count,
                                         int has_idx)
{
    if (!node) return;

    /* Skip scratch declarations and template param nodes */
    if (node->type == JZ_AST_SCRATCH_DECL) return;
    if (node->type == JZ_AST_TEMPLATE_PARAM) return;

    /* Check plain identifiers (signal references on LHS and RHS) */
    if (node->type == JZ_AST_EXPR_IDENTIFIER && node->name) {
        if (!is_allowed_template_ident(ctx, node->name, param_names,
                                        param_count, scratch_names,
                                        scratch_count, has_idx)) {
            report_rule(ctx, node->loc, "TEMPLATE_EXTERNAL_REF",
                        "identifier in template body must be a parameter, "
                        "@scratch wire, or compile-time constant");
        }
    }

    /* Check bus access base names (e.g., `some_signal.FIELD`) */
    if (node->type == JZ_AST_EXPR_BUS_ACCESS && node->name) {
        if (!is_allowed_template_ident(ctx, node->name, param_names,
                                        param_count, scratch_names,
                                        scratch_count, has_idx)) {
            report_rule(ctx, node->loc, "TEMPLATE_EXTERNAL_REF",
                        "identifier in template body must be a parameter, "
                        "@scratch wire, or compile-time constant");
        }
    }

    /* Recurse into children */
    for (size_t i = 0; i < node->child_count; i++) {
        validate_template_body_refs(ctx, node->children[i],
                                     param_names, param_count,
                                     scratch_names, scratch_count,
                                     has_idx);
    }
}

/* ── Expand a single @apply ──────────────────────────────────────── */

static int expand_apply(ExpandContext *ctx, JZASTNode *apply,
                         JZASTNode *parent, size_t apply_index,
                         JZASTNode *scope_module)
{
    if (!apply || !apply->name) return -1;

    TemplateEntry *tmpl = find_template(ctx, apply->name, scope_module);
    if (!tmpl) {
        report_rule(ctx, apply->loc, "TEMPLATE_UNDEFINED",
                    "@apply references undefined template");
        /* Remove the @apply node so downstream analysis doesn't see it */
        goto remove_apply;
    }

    JZASTNode *def = tmpl->def_node;

    /* Count parameters and body statements */
    size_t param_count = 0;
    const char *param_names[64]; /* reasonable limit */
    size_t body_start = 0;

    for (size_t i = 0; i < def->child_count; i++) {
        if (def->children[i]->type == JZ_AST_TEMPLATE_PARAM) {
            if (param_count < 64) {
                param_names[param_count] = def->children[i]->name;
            }
            param_count++;
            body_start = i + 1;
        }
    }

    /* Collect scratch names from body */
    const char *scratch_names[64];
    size_t scratch_count = 0;
    for (size_t i = body_start; i < def->child_count; i++) {
        if (def->children[i]->type == JZ_AST_SCRATCH_DECL && def->children[i]->name) {
            if (scratch_count < 64) {
                scratch_names[scratch_count++] = def->children[i]->name;
            }
        }
    }

    /* Check arg count */
    size_t arg_count = apply->child_count;
    if (arg_count != param_count) {
        report_rule(ctx, apply->loc, "TEMPLATE_ARG_COUNT_MISMATCH",
                    "@apply argument count does not match template parameter count");
        goto remove_apply;
    }

    /* Evaluate count */
    long count = 1;
    int has_idx = 0;
    if (apply->text) {
        has_idx = 1;
        if (!eval_count_expr(ctx, apply->text, &count)) {
            report_rule(ctx, apply->loc, "TEMPLATE_COUNT_NOT_NONNEG_INT",
                        "@apply count expression does not resolve to a non-negative integer");
            goto remove_apply;
        }
        if (count < 0) {
            report_rule(ctx, apply->loc, "TEMPLATE_COUNT_NOT_NONNEG_INT",
                        "@apply count expression does not resolve to a non-negative integer");
            goto remove_apply;
        }
    }

    /* Validate template body: all identifiers must be params, scratch, or IDX */
    for (size_t i = body_start; i < def->child_count; i++) {
        if (def->children[i]->type == JZ_AST_SCRATCH_DECL) continue;
        if (def->children[i]->type == JZ_AST_TEMPLATE_PARAM) continue;
        validate_template_body_refs(ctx, def->children[i],
                                     param_names, param_count,
                                     scratch_names, scratch_count,
                                     has_idx);
    }

    int callsite_id = ctx->apply_counter++;

    /* For each iteration, clone template body and substitute */
    /* We'll collect expanded statements and insert them into the parent
     * at the position of the @apply node */
    JZASTNode **expanded = NULL;
    size_t expanded_count = 0;
    size_t expanded_cap = 0;

    for (long idx = 0; idx < count; idx++) {
        /* Generate random suffixes for scratch wires in this iteration */
        char (*suffixes)[7] = NULL;
        if (scratch_count > 0) {
            suffixes = (char (*)[7])malloc(scratch_count * 7);
            if (suffixes) {
                for (size_t s = 0; s < scratch_count; s++)
                    generate_random_suffix(suffixes[s]);
            }
        }

        /* Materialize scratch wires for this iteration */
        for (size_t s = 0; s < scratch_count; s++) {
            /* Find the scratch decl */
            JZASTNode *scratch_def = NULL;
            for (size_t i = body_start; i < def->child_count; i++) {
                if (def->children[i]->type == JZ_AST_SCRATCH_DECL &&
                    def->children[i]->name &&
                    strcmp(def->children[i]->name, scratch_names[s]) == 0) {
                    scratch_def = def->children[i];
                    break;
                }
            }
            if (!scratch_def) continue;

            char mangled_name[512];
            if (suffixes) {
                snprintf(mangled_name, sizeof(mangled_name), "%s__%s__%d_%ld_%s",
                         def->name, scratch_names[s], callsite_id, idx,
                         suffixes[s]);
            } else {
                snprintf(mangled_name, sizeof(mangled_name), "%s__%s__%d_%ld",
                         def->name, scratch_names[s], callsite_id, idx);
            }

            JZASTNode *wire = jz_ast_new(JZ_AST_WIRE_DECL, scratch_def->loc);
            if (!wire) continue;
            jz_ast_set_name(wire, mangled_name);

            /* Substitute params and IDX in width expression */
            if (scratch_def->width) {
                char *w = strdup(scratch_def->width);
                for (size_t pi = 0; pi < param_count && w; pi++) {
                    char *arg_text = apply->children[pi]->name
                                     ? apply->children[pi]->name
                                     : (apply->children[pi]->text
                                        ? apply->children[pi]->text : "0");
                    char *nw = string_replace_whole_word(w, param_names[pi], arg_text);
                    free(w);
                    w = nw;
                }
                if (has_idx && w) {
                    char idx_str[32];
                    snprintf(idx_str, sizeof(idx_str), "%ld", idx);
                    char *nw = string_replace_whole_word(w, "IDX", idx_str);
                    free(w);
                    w = nw;
                }
                if (w) {
                    jz_ast_set_width(wire, w);
                    free(w);
                }
            }

            add_scratch_wire(ctx, wire);
        }

        /* Clone and substitute body statements (skip params and scratch decls) */
        for (size_t i = body_start; i < def->child_count; i++) {
            if (def->children[i]->type == JZ_AST_SCRATCH_DECL) continue;
            if (def->children[i]->type == JZ_AST_TEMPLATE_PARAM) continue;

            JZASTNode *clone = ast_deep_clone(def->children[i]);
            if (!clone) continue;

            substitute_in_node(clone, param_names, apply->children,
                               param_count, (int)idx, has_idx, def->name,
                               callsite_id, scratch_names, scratch_count,
                               (const char (*)[7])suffixes);

            if (expanded_count == expanded_cap) {
                size_t new_cap = expanded_cap ? expanded_cap * 2 : 16;
                void *p = realloc(expanded, new_cap * sizeof(JZASTNode *));
                if (!p) { jz_ast_free(clone); continue; }
                expanded = p;
                expanded_cap = new_cap;
            }
            expanded[expanded_count++] = clone;
        }

        free(suffixes);
    }

    /* Replace the @apply node in parent with expanded statements */
    if (expanded_count > 0) {
        /* Build new children array */
        size_t new_count = parent->child_count - 1 + expanded_count;
        JZASTNode **new_children = (JZASTNode **)malloc(new_count * sizeof(JZASTNode *));
        if (!new_children) {
            for (size_t i = 0; i < expanded_count; i++) jz_ast_free(expanded[i]);
            free(expanded);
            return -1;
        }

        size_t dst = 0;
        for (size_t i = 0; i < parent->child_count; i++) {
            if (i == apply_index) {
                for (size_t j = 0; j < expanded_count; j++) {
                    new_children[dst++] = expanded[j];
                }
            } else {
                new_children[dst++] = parent->children[i];
            }
        }

        /* Free the old @apply node */
        jz_ast_free(parent->children[apply_index]);

        free(parent->children);
        parent->children = new_children;
        parent->child_count = new_count;
        parent->child_capacity = new_count;
    } else {
        /* count == 0: just remove the @apply node */
        jz_ast_free(parent->children[apply_index]);
        for (size_t i = apply_index; i + 1 < parent->child_count; i++) {
            parent->children[i] = parent->children[i + 1];
        }
        parent->child_count--;
    }

    free(expanded);
    return 0;

remove_apply:
    /* Error path: remove the @apply node from parent to avoid downstream confusion */
    jz_ast_free(parent->children[apply_index]);
    for (size_t i = apply_index; i + 1 < parent->child_count; i++) {
        parent->children[i] = parent->children[i + 1];
    }
    parent->child_count--;
    return 0; /* Return 0 so expansion continues for other @apply nodes */
}

/* ── Expand all @apply nodes in a subtree ────────────────────────── */

static int expand_applies_in_node(ExpandContext *ctx, JZASTNode *node, JZASTNode *scope_module)
{
    if (!node) return 0;

    JZASTNode *mod = scope_module;
    if (node->type == JZ_AST_MODULE) {
        mod = node;
    }

    /* Process children, watching for @apply nodes */
    for (size_t i = 0; i < node->child_count; /* incremented conditionally */) {
        JZASTNode *child = node->children[i];
        if (!child) { i++; continue; }

        if (child->type == JZ_AST_TEMPLATE_APPLY) {
            size_t old_count = node->child_count;
            if (expand_apply(ctx, child, node, i, mod) != 0) {
                return -1;
            }
            /* After expansion, re-check from the same index since new nodes
             * were inserted (or the apply was removed). Don't increment i. */
            /* But avoid infinite loop if nothing changed */
            if (node->child_count == old_count) {
                i++;
            }
            continue;
        }

        /* Recurse into child */
        if (expand_applies_in_node(ctx, child, mod) != 0) {
            return -1;
        }
        i++;
    }

    return 0;
}

/* ── Remove template defs from AST ───────────────────────────────── */

static void remove_template_defs(JZASTNode *node)
{
    if (!node) return;

    size_t dst = 0;
    for (size_t i = 0; i < node->child_count; i++) {
        if (node->children[i]->type == JZ_AST_TEMPLATE_DEF) {
            jz_ast_free(node->children[i]);
        } else {
            node->children[dst++] = node->children[i];
        }
    }
    node->child_count = dst;

    for (size_t i = 0; i < node->child_count; i++) {
        remove_template_defs(node->children[i]);
    }
}

/* ── Insert scratch wires into module ────────────────────────────── */

static void insert_scratch_wires(JZASTNode *module, JZASTNode **wires, size_t count)
{
    if (!module || count == 0) return;

    /* Find existing WIRE block */
    JZASTNode *wire_block = NULL;
    for (size_t i = 0; i < module->child_count; i++) {
        if (module->children[i]->type == JZ_AST_WIRE_BLOCK) {
            wire_block = module->children[i];
            break;
        }
    }

    if (!wire_block) {
        /* Create a new WIRE block */
        wire_block = jz_ast_new(JZ_AST_WIRE_BLOCK, module->loc);
        if (!wire_block) return;
        jz_ast_set_block_kind(wire_block, "WIRE");
        jz_ast_add_child(module, wire_block);
    }

    for (size_t i = 0; i < count; i++) {
        jz_ast_add_child(wire_block, wires[i]);
    }
}

/* ── Public API ──────────────────────────────────────────────────── */

int jz_template_expand(JZASTNode *root,
                        JZDiagnosticList *diagnostics,
                        const char *filename)
{
    if (!root) return 0;

    if (!rng_seeded) {
        srand((unsigned)time(NULL));
        rng_seeded = 1;
    }

    ExpandContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.diagnostics = diagnostics;
    ctx.filename = filename;
    ctx.apply_counter = 0;

    /* Collect CONST/CONFIG values for count evaluation */
    collect_consts_from_ast(&ctx, root);

    /* Collect all template definitions */
    collect_templates(&ctx, root, NULL);

    /* Expand all @apply nodes, processing each module */
    for (size_t i = 0; i < root->child_count; i++) {
        JZASTNode *child = root->children[i];
        if (!child) continue;

        if (child->type == JZ_AST_MODULE) {
            ctx.scratch_wire_count = 0;

            if (expand_applies_in_node(&ctx, child, child) != 0) {
                /* Error already reported */
            }

            /* Insert scratch wires into module */
            if (ctx.scratch_wire_count > 0) {
                insert_scratch_wires(child, ctx.scratch_wires, ctx.scratch_wire_count);
                /* Don't free the wire nodes — they're now owned by the AST */
                ctx.scratch_wire_count = 0;
            }
        }
    }

    /* Remove template definition nodes */
    remove_template_defs(root);

    /* Cleanup */
    free(ctx.templates);
    free(ctx.consts);
    free(ctx.scratch_wires);

    return 0;
}
