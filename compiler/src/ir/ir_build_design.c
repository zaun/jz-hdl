/**
 * @file ir_build_design.c
 * @brief Top-level IR design construction and project-level helpers.
 *
 * This file contains the main IR construction entry point along with
 * project-level parsing helpers (pins, maps, clocks, top bindings) and
 * utility routines used during design elaboration.
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "ir_internal.h"
#include "ir_library.h"
#include "chip_data.h"
#include "compiler.h"

/**
 * @brief Copy a string while trimming leading and trailing whitespace.
 *
 * Used during project-level parsing to normalize identifiers and attribute
 * values.
 *
 * @param src      Source string (may be NULL).
 * @param dst      Destination buffer.
 * @param dst_size Size of destination buffer in bytes.
 */
/** Sort comparator for IR_ModuleSpecOverride by name (for qsort). */
static int ir_override_name_cmp(const void *a, const void *b)
{
    const IR_ModuleSpecOverride *oa = (const IR_ModuleSpecOverride *)a;
    const IR_ModuleSpecOverride *ob = (const IR_ModuleSpecOverride *)b;
    if (!oa->name && !ob->name) return 0;
    if (!oa->name) return -1;
    if (!ob->name) return 1;
    return strcmp(oa->name, ob->name);
}

static void ir_trim_copy(const char *src, char *dst, size_t dst_size)
{
    if (!dst || dst_size == 0) return;
    dst[0] = '\0';
    if (!src) return;

    const char *start = src;
    while (*start && isspace((unsigned char)*start)) {
        ++start;
    }
    const char *end = src + strlen(src);
    while (end > start && isspace((unsigned char)end[-1])) {
        --end;
    }
    size_t len = (size_t)(end - start);
    if (len >= dst_size) {
        len = dst_size - 1u;
    }
    memcpy(dst, start, len);
    dst[len] = '\0';
}

/**
 * @brief Build the ASYNCHRONOUS statement block for a module.
 *
 * Locates the ASYNCHRONOUS block in the module AST and lowers its contents
 * into a single IR STMT_BLOCK, including assignments, IF/ELIF/ELSE chains,
 * SELECT statements, and nested blocks.
 *
 * @param arena           Arena for IR allocation.
 * @param mod_scope       Module scope.
 * @param project_symbols Project-level symbols.
 * @param bus_map         BUS signal mapping array (may be NULL).
 * @param bus_map_count   Number of bus mapping entries.
 * @param diagnostics     Diagnostic sink.
 * @return Pointer to IR_Stmt block, or NULL if none exists.
 */
static IR_Stmt *ir_build_async_block(JZArena *arena,
                                     const JZModuleScope *mod_scope,
                                     const JZBuffer *project_symbols,
                                     const IR_BusSignalMapping *bus_map,
                                     int bus_map_count,
                                     JZDiagnosticList *diagnostics)
{
    if (!arena || !mod_scope || !mod_scope->node) return NULL;
    JZASTNode *mod = mod_scope->node;

    JZASTNode *async_blk = NULL;
    for (size_t i = 0; i < mod->child_count; ++i) {
        JZASTNode *child = mod->children[i];
        if (!child || child->type != JZ_AST_BLOCK || !child->block_kind) continue;
        if (strcmp(child->block_kind, "ASYNCHRONOUS") == 0) {
            async_blk = child;
            break;
        }
    }
    if (!async_blk) return NULL;

    int next_assign_id = 0;
    return ir_build_block_from_node(arena,
                                    async_blk,
                                    mod_scope,
                                    project_symbols,
                                    bus_map,
                                    bus_map_count,
                                    diagnostics,
                                    &next_assign_id);
}

/**
 * @brief Count the number of lines in a source file.
 *
 * An empty file reports 0 lines. A non-empty file without a trailing newline
 * still counts as one line.
 *
 * @param path Path to the source file.
 * @return Number of lines, or 0 if the file cannot be opened.
 */
static int ir_count_file_lines(const char *path)
{
    if (!path || !*path) {
        return 0;
    }
    FILE *f = fopen(path, "rb");
    if (!f) {
        return 0;
    }
    int ch = 0;
    int last = '\0';
    int lines = 0;
    int saw_any = 0;
    while ((ch = fgetc(f)) != EOF) {
        saw_any = 1;
        if (ch == '\n') {
            lines++;
        }
        last = ch;
    }
    fclose(f);
    if (saw_any && last != '\n') {
        lines++;
    }
    return lines;
}

/**
 * @brief Parse a pin slice of the form "pin[msb:lsb]".
 *
 * Extracts the logical pin name, MSB, and LSB indices.
 *
 * @param expr           Input expression text.
 * @param out_pin_name   Output buffer for pin name.
 * @param out_name_size  Size of pin-name buffer.
 * @param out_msb        Output MSB index.
 * @param out_lsb        Output LSB index.
 * @return 1 on success, 0 on failure or if no colon is present.
 */
static int ir_parse_pin_slice(const char *expr,
                              char *out_pin_name,
                              size_t out_name_size,
                              int *out_msb,
                              int *out_lsb)
{
    if (!expr || !out_pin_name || out_name_size == 0 || !out_msb || !out_lsb) return 0;
    *out_pin_name = '\0';
    *out_msb = -1;
    *out_lsb = -1;

    char tmp[256];
    ir_trim_copy(expr, tmp, sizeof(tmp));
    const char *br = strchr(tmp, '[');
    if (!br) return 0;
    const char *br_end = strchr(br, ']');
    if (!br_end || br_end <= br + 1) return 0;

    /* Must contain a colon for this to be a slice. */
    const char *colon = NULL;
    for (const char *p = br + 1; p < br_end; ++p) {
        if (*p == ':') { colon = p; break; }
    }
    if (!colon) return 0;

    size_t name_len = (size_t)(br - tmp);
    if (name_len == 0 || name_len >= out_name_size) return 0;
    memcpy(out_pin_name, tmp, name_len);
    out_pin_name[name_len] = '\0';
    while (name_len > 0 && isspace((unsigned char)out_pin_name[name_len - 1])) {
        out_pin_name[--name_len] = '\0';
    }

    /* Parse MSB. */
    char msb_buf[64];
    size_t msb_len = (size_t)(colon - (br + 1));
    if (msb_len == 0 || msb_len >= sizeof(msb_buf)) return 0;
    memcpy(msb_buf, br + 1, msb_len);
    msb_buf[msb_len] = '\0';
    unsigned msb_val = 0;
    if (!parse_simple_nonnegative_int(msb_buf, &msb_val)) return 0;

    /* Parse LSB. */
    char lsb_buf[64];
    size_t lsb_len = (size_t)(br_end - (colon + 1));
    if (lsb_len == 0 || lsb_len >= sizeof(lsb_buf)) return 0;
    memcpy(lsb_buf, colon + 1, lsb_len);
    lsb_buf[lsb_len] = '\0';
    unsigned lsb_val = 0;
    if (!parse_simple_nonnegative_int(lsb_buf, &lsb_val)) return 0;

    if (msb_val < lsb_val) return 0;

    *out_msb = (int)msb_val;
    *out_lsb = (int)lsb_val;
    return 1;
}

/**
 * @brief Parse a MAP left-hand side of the form "pin" or "pin[bit]".
 *
 * Extracts the logical pin name and optional bit index.
 *
 * @param lhs             Left-hand side text.
 * @param out_pin_name    Output buffer for pin name.
 * @param out_name_size   Size of pin-name buffer.
 * @param out_bit_index   Output bit index, or -1 when not present.
 * @return 1 on success, 0 on failure.
 */
static int ir_parse_map_lhs(const char *lhs,
                            char *out_pin_name,
                            size_t out_name_size,
                            int *out_bit_index)
{
    if (!lhs || !out_pin_name || out_name_size == 0 || !out_bit_index) return 0;
    *out_pin_name = '\0';
    *out_bit_index = -1;

    char tmp[256];
    ir_trim_copy(lhs, tmp, sizeof(tmp));
    const char *br = strchr(tmp, '[');
    if (!br) {
        /* Scalar pin mapping. */
        size_t name_len = strlen(tmp);
        if (name_len == 0 || name_len >= out_name_size) return 0;
        memcpy(out_pin_name, tmp, name_len);
        out_pin_name[name_len] = '\0';
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
    /* Trim any trailing whitespace that may precede the '[' so that
     * logical_pin_name is canonicalized (e.g., "led " -> "led").
     */
    while (name_len > 0 && isspace((unsigned char)out_pin_name[name_len - 1])) {
        out_pin_name[--name_len] = '\0';
    }

    char idx_buf[64];
    size_t idx_len = (size_t)(br_end - (br + 1));
    if (idx_len == 0 || idx_len >= sizeof(idx_buf)) {
        return 0;
    }
    memcpy(idx_buf, br + 1, idx_len);
    idx_buf[idx_len] = '\0';

    unsigned bit = 0;
    if (!parse_simple_nonnegative_int(idx_buf, &bit)) {
        return 0;
    }

    *out_bit_index = (int)bit;
    return 1;
}

/* -------------------------------------------------------------------------
 * Small expression evaluator for chip-data clock-gen output frequency
 * expressions.  Works with IR_ClockGenUnit (CONFIG values) + chip data
 * (defaults, derived expressions).  Mirrors the AST-level evaluator in
 * driver_project_hw.c but operates on IR data structures.
 * -------------------------------------------------------------------------
 */
typedef struct IRCGenEvalCtx {
    const IR_ClockGenUnit *unit;
    const JZChipData      *chip;
    const char            *unit_type;   /* e.g., "pll" */
    double                 refclk_period_ns;
    int                    depth;       /* recursion guard */
    int                    ok;
} IRCGenEvalCtx;

static double ir_cgen_eval_expr(const char **pp, IRCGenEvalCtx *ctx);

static void ir_cgen_skip_ws(const char **pp) {
    while (**pp && isspace((unsigned char)**pp)) ++(*pp);
}

static double ir_cgen_resolve_ident(const char *name, size_t len, IRCGenEvalCtx *ctx)
{
    char buf[128];
    if (len >= sizeof(buf)) { ctx->ok = 0; return 0.0; }
    memcpy(buf, name, len);
    buf[len] = '\0';

    if (strcmp(buf, "refclk_period_ns") == 0 || strcmp(buf, "REF_CLK_period_ns") == 0) {
        return ctx->refclk_period_ns;
    }

    /* Look up in the unit's CONFIG entries. */
    if (ctx->unit) {
        for (int i = 0; i < ctx->unit->num_configs; ++i) {
            const IR_ClockGenConfig *cfg = &ctx->unit->configs[i];
            if (cfg->param_name && strcmp(cfg->param_name, buf) == 0 && cfg->param_value) {
                char *endptr = NULL;
                double val = strtod(cfg->param_value, &endptr);
                if (endptr != cfg->param_value) return val;
            }
        }
    }

    /* Fall back to chip data defaults. */
    if (ctx->chip && ctx->unit_type) {
        const char *def = jz_chip_clock_gen_param_default(ctx->chip, ctx->unit_type, buf);
        if (def) {
            char *endptr = NULL;
            double val = strtod(def, &endptr);
            if (endptr != def) return val;
        }
    }

    /* Try derived expressions (e.g., FVCO). */
    if (ctx->chip && ctx->unit_type && ctx->depth < 8) {
        const char *expr = jz_chip_clock_gen_derived_expr(ctx->chip, ctx->unit_type, buf);
        if (expr && !strstr(expr, "toString")) {
            ctx->depth++;
            int saved_ok = ctx->ok;
            ctx->ok = 1;
            const char *p = expr;
            double val = ir_cgen_eval_expr(&p, ctx);
            int eval_ok = ctx->ok;
            ctx->ok = saved_ok & eval_ok;
            ctx->depth--;
            if (eval_ok) return val;
        }
    }

    ctx->ok = 0;
    return 0.0;
}

static double ir_cgen_eval_primary(const char **pp, IRCGenEvalCtx *ctx)
{
    ir_cgen_skip_ws(pp);
    if (**pp == '(') {
        ++(*pp);
        double v = ir_cgen_eval_expr(pp, ctx);
        ir_cgen_skip_ws(pp);
        if (**pp == ')') ++(*pp);
        return v;
    }
    if (isalpha((unsigned char)**pp) || **pp == '_') {
        const char *start = *pp;
        while (isalnum((unsigned char)**pp) || **pp == '_') ++(*pp);
        return ir_cgen_resolve_ident(start, (size_t)(*pp - start), ctx);
    }
    {
        char *endptr = NULL;
        double v = strtod(*pp, &endptr);
        if (endptr == *pp) { ctx->ok = 0; return 0.0; }
        *pp = endptr;
        return v;
    }
}

static double ir_cgen_eval_unary(const char **pp, IRCGenEvalCtx *ctx)
{
    ir_cgen_skip_ws(pp);
    if (**pp == '-') { ++(*pp); return -ir_cgen_eval_unary(pp, ctx); }
    if (**pp == '+') { ++(*pp); return ir_cgen_eval_unary(pp, ctx); }
    return ir_cgen_eval_primary(pp, ctx);
}

static double ir_cgen_eval_mul(const char **pp, IRCGenEvalCtx *ctx)
{
    double left = ir_cgen_eval_unary(pp, ctx);
    for (;;) {
        ir_cgen_skip_ws(pp);
        if (**pp == '*') { ++(*pp); left *= ir_cgen_eval_unary(pp, ctx); }
        else if (**pp == '/') {
            ++(*pp);
            double d = ir_cgen_eval_unary(pp, ctx);
            if (d == 0.0) { ctx->ok = 0; return 0.0; }
            left /= d;
        }
        else break;
    }
    return left;
}

static double ir_cgen_eval_expr(const char **pp, IRCGenEvalCtx *ctx)
{
    double left = ir_cgen_eval_mul(pp, ctx);
    for (;;) {
        ir_cgen_skip_ws(pp);
        if (**pp == '+') { ++(*pp); left += ir_cgen_eval_mul(pp, ctx); }
        else if (**pp == '-') { ++(*pp); left -= ir_cgen_eval_mul(pp, ctx); }
        else break;
    }
    return left;
}

/**
 * @brief Evaluate a chip-data clock-gen expression to a double.
 * @return The result in MHz, or 0.0 on failure.
 */
static double ir_eval_cgen_expr(const char *expr,
                                const IR_ClockGenUnit *unit,
                                double refclk_period_ns,
                                const JZChipData *chip,
                                const char *unit_type)
{
    IRCGenEvalCtx ctx;
    ctx.unit = unit;
    ctx.chip = chip;
    ctx.unit_type = unit_type;
    ctx.refclk_period_ns = refclk_period_ns;
    ctx.depth = 0;
    ctx.ok = 1;
    const char *p = expr;
    double result = ir_cgen_eval_expr(&p, &ctx);
    ir_cgen_skip_ws(&p);
    if (*p != '\0') ctx.ok = 0;
    return ctx.ok ? result : 0.0;
}

/* -------------------------------------------------------------------------
 * IOB latch marking
 * -------------------------------------------------------------------------
 * After top bindings are built, walk the top module's async block looking
 * for direct `PORT_OUT <= LATCH` assignments where the port is bound to a
 * real output pin.  Mark those latch signals with iob=true so backends can
 * emit the appropriate placement attribute.
 */

static void ir_mark_iob_latch_stmt(const IR_Stmt *stmt,
                                    IR_Module *mod,
                                    const IR_Project *proj)
{
    if (!stmt) return;

    switch (stmt->kind) {
    case STMT_ASSIGNMENT: {
        const IR_Assignment *a = &stmt->u.assign;
        if (a->kind != ASSIGN_RECEIVE) break;
        if (a->is_sliced) break;

        const IR_Expr *rhs = a->rhs;
        if (!rhs || rhs->kind != EXPR_SIGNAL_REF) break;

        IR_Signal *rhs_sig = ir_find_signal_by_id(mod, rhs->u.signal_ref.signal_id);
        if (!rhs_sig || rhs_sig->kind != SIG_LATCH) break;

        IR_Signal *lhs_sig = ir_find_signal_by_id(mod, a->lhs_signal_id);
        if (!lhs_sig || lhs_sig->kind != SIG_PORT) break;
        if (lhs_sig->u.port.direction != PORT_OUT &&
            lhs_sig->u.port.direction != PORT_INOUT) break;

        if (!proj->top_bindings || !proj->pins) break;
        for (int i = 0; i < proj->num_top_bindings; ++i) {
            const IR_TopBinding *tb = &proj->top_bindings[i];
            if (tb->top_port_signal_id != lhs_sig->id) continue;
            if (tb->pin_id < 0 || tb->pin_id >= proj->num_pins) continue;
            const IR_Pin *pin = &proj->pins[tb->pin_id];
            if (pin->kind == PIN_OUT || pin->kind == PIN_INOUT) {
                rhs_sig->iob = true;
                return;
            }
        }
        break;
    }
    case STMT_IF: {
        const IR_IfStmt *ifs = &stmt->u.if_stmt;
        ir_mark_iob_latch_stmt(ifs->then_block, mod, proj);
        if (ifs->elif_chain) ir_mark_iob_latch_stmt(ifs->elif_chain, mod, proj);
        if (ifs->else_block) ir_mark_iob_latch_stmt(ifs->else_block, mod, proj);
        break;
    }
    case STMT_SELECT: {
        const IR_SelectStmt *sel = &stmt->u.select_stmt;
        for (int i = 0; i < sel->num_cases; ++i) {
            ir_mark_iob_latch_stmt(sel->cases[i].body, mod, proj);
        }
        break;
    }
    case STMT_BLOCK: {
        const IR_BlockStmt *blk = &stmt->u.block;
        for (int i = 0; i < blk->count; ++i) {
            ir_mark_iob_latch_stmt(&blk->stmts[i], mod, proj);
        }
        break;
    }
    default:
        break;
    }
}

static void ir_mark_iob_latches(IR_Module *top_mod, const IR_Project *proj)
{
    if (!top_mod || !proj || !top_mod->async_block) return;
    ir_mark_iob_latch_stmt(top_mod->async_block, top_mod, proj);
}

/**
 * @brief Build a complete IR_Design from the project AST.
 *
 * This is the primary entry point for IR construction. It rebuilds symbol
 * tables, elaborates modules and specializations, lowers statements,
 * constructs clocks, pins, mappings, instances, and finalizes memory
 * bindings.
 *
 * @param root        Root AST node.
 * @param out_design  Output pointer to constructed IR design.
 * @param arena       Arena for IR allocation.
 * @param diagnostics Diagnostic sink.
 * @return 0 on success, non-zero on failure.
 */
int jz_ir_build_design(JZASTNode *root,
                       IR_Design **out_design,
                       JZArena *arena,
                       JZDiagnosticList *diagnostics)
{
    if (!root || !out_design || !arena) {
        return -1;
    }

    /* Rebuild symbol tables and net graphs to obtain module scopes and
     * per-symbol metadata (stable IDs, can_be_z, etc.). This mirrors the
     * early stages of jz_sem_run but is kept separate so IR construction can
     * treat internal failures distinctly.
     */
    JZBuffer module_scopes = (JZBuffer){0};
    JZBuffer project_symbols = (JZBuffer){0};

    /* Temporary specialization metadata used while constructing the IR. */
    IR_ModuleSpec *specs = NULL;
    int spec_count = 0;

    clock_t ir_t0 = clock();
    if (build_symbol_tables(root, &module_scopes, &project_symbols, diagnostics) != 0) {
        /* Cleanup on failure. */
        size_t scope_count = module_scopes.len / sizeof(JZModuleScope);
        JZModuleScope *scopes = (JZModuleScope *)module_scopes.data;
        for (size_t i = 0; i < scope_count; ++i) {
            jz_buf_free(&scopes[i].symbols);
        }
        jz_buf_free(&module_scopes);
        jz_buf_free(&project_symbols);
        if (diagnostics && root->loc.filename) {
            JZLocation loc = root->loc;
            jz_diagnostic_report(diagnostics,
                                 loc,
                                 JZ_SEVERITY_ERROR,
                                 "IR_INTERNAL",
                                 "failed to build symbol tables for IR construction");
        }
        return -1;
    }

    if (jz_verbose) fprintf(stderr, "[verbose]   ir: build_symbol_tables: %.1f ms\n",
                             (double)(clock() - ir_t0) / CLOCKS_PER_SEC * 1000.0);

    /* Build net graphs so that symbol.can_be_z is populated for each signal. */
    ir_t0 = clock();
    sem_build_net_graphs(root, &module_scopes, &project_symbols, diagnostics);
    if (jz_verbose) fprintf(stderr, "[verbose]   ir: net_graphs: %.1f ms\n",
                             (double)(clock() - ir_t0) / CLOCKS_PER_SEC * 1000.0);

    /* Build a minimal IR_Design: modules and unified signal lists. Other IR
     * features (expressions, statements, memories, clock domains, project
     * metadata) will be added incrementally.
     */
    IR_Design *design = (IR_Design *)jz_arena_alloc(arena, sizeof(IR_Design));
    if (!design) {
        goto ir_fail;
    }
    memset(design, 0, sizeof(*design));

    /* Design name: use project name if present, otherwise empty string. */
    design->name = root->name ? ir_strdup_arena(arena, root->name) : ir_strdup_arena(arena, "");

    size_t scope_count = module_scopes.len / sizeof(JZModuleScope);
    JZModuleScope *scopes = (JZModuleScope *)module_scopes.data;

    /* Discover all module specializations (if any) before allocating the IR
     * module array so that we know the total module count.
     */
    if (ir_collect_module_specializations(scopes,
                                          scope_count,
                                          &project_symbols,
                                          arena,
                                          &specs,
                                          &spec_count) != 0) {
        goto ir_fail;
    }

    /* Assign final IR module indices for specializations. Base modules occupy
     * [0, scope_count), specializations follow in declaration order.
     */
    size_t total_modules = scope_count + (size_t)spec_count;
    for (int i = 0; i < spec_count; ++i) {
        specs[i].spec_module_index = (int)(scope_count + (size_t)i);
    }

    /* Build IR_SourceFile[] from all distinct filenames referenced by the
     * project root and module scopes.
     */
    JZBuffer file_names = (JZBuffer){0}; /* array of const char* */
    if (root->loc.filename) {
        const char *path = root->loc.filename;
        (void)jz_buf_append(&file_names, &path, sizeof(path));
    }
    for (size_t i = 0; i < scope_count; ++i) {
        JZModuleScope *scope = &scopes[i];
        if (!scope->node || !scope->node->loc.filename) continue;
        const char *path = scope->node->loc.filename;
        /* Check if already present. */
        const char **arr = (const char **)file_names.data;
        size_t count = file_names.len / sizeof(const char *);
        int found = 0;
        for (size_t j = 0; j < count; ++j) {
            if (arr[j] && strcmp(arr[j], path) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            (void)jz_buf_append(&file_names, &path, sizeof(path));
        }
    }

    const char **fn_arr = (const char **)file_names.data;
    size_t file_count = file_names.len / sizeof(const char *);
    design->num_source_files = (int)file_count;
    if (file_count > 0) {
        design->source_files = (IR_SourceFile *)jz_arena_alloc(arena, sizeof(IR_SourceFile) * file_count);
        if (!design->source_files) {
            goto ir_fail;
        }
        memset(design->source_files, 0, sizeof(IR_SourceFile) * file_count);
        for (size_t i = 0; i < file_count; ++i) {
            const char *path = fn_arr[i];
            design->source_files[i].path = path ? ir_strdup_arena(arena, path) : ir_strdup_arena(arena, "");
            design->source_files[i].line_count = ir_count_file_lines(path);
        }
    }

    if (total_modules > 0) {
        design->modules = (IR_Module *)jz_arena_alloc(arena, sizeof(IR_Module) * total_modules);
        if (!design->modules) {
            goto ir_fail;
        }
        memset(design->modules, 0, sizeof(IR_Module) * total_modules);
        design->num_modules = (int)total_modules;

        /* Temporary storage for bus_map per module (needed for second pass). */
        IR_BusSignalMapping **all_bus_maps = NULL;
        int *all_bus_map_counts = NULL;
        if (scope_count > 0) {
            all_bus_maps = (IR_BusSignalMapping **)jz_arena_alloc(
                arena, sizeof(IR_BusSignalMapping *) * scope_count);
            all_bus_map_counts = (int *)jz_arena_alloc(
                arena, sizeof(int) * scope_count);
            if (!all_bus_maps || !all_bus_map_counts) {
                goto ir_fail;
            }
            memset(all_bus_maps, 0, sizeof(IR_BusSignalMapping *) * scope_count);
            memset(all_bus_map_counts, 0, sizeof(int) * scope_count);
        }

        /* First pass: build IR modules (signals, memories, async, clocks, CDCs)
         * for all generic/base modules. Instance connections are deferred to a
         * second pass after all module signals are available.
         */
        ir_t0 = clock();
        for (size_t i = 0; i < scope_count; ++i) {
            clock_t mod_t0 = clock();
            IR_Module *mod = &design->modules[i];
            const JZModuleScope *scope = &scopes[i];

            mod->id = (int)i;
            mod->name = scope->name ? ir_strdup_arena(arena, scope->name) : ir_strdup_arena(arena, "");
            if (jz_verbose) fprintf(stderr, "[verbose]     ir: building module '%s' ...\n",
                                     mod->name ? mod->name : "?");
            mod->base_module_id = -1; /* generic/base module */
            mod->source_line = scope->node ? scope->node->loc.line : 0;
            /* Map module to its source file index. */
            mod->source_file_id = -1;
            if (scope->node && scope->node->loc.filename && file_count > 0) {
                const char *mod_path = scope->node->loc.filename;
                for (size_t fi = 0; fi < file_count; ++fi) {
                    if (fn_arr[fi] && strcmp(fn_arr[fi], mod_path) == 0) {
                        mod->source_file_id = (int)fi;
                        break;
                    }
                }
            } else if (file_count > 0) {
                /* Fall back to the first source file (project file) if the
                 * module node lacks an explicit filename.
                 */
                mod->source_file_id = 0;
            }

            clock_t step_t0;
            IR_BusSignalMapping *bus_map = NULL;
            int bus_map_count = 0;
            step_t0 = clock();
            if (ir_build_signals_for_module(scope,
                                            mod->id,
                                            arena,
                                            &project_symbols,
                                            &mod->signals,
                                            &mod->num_signals,
                                            &bus_map,
                                            &bus_map_count) != 0) {
                goto ir_fail;
            }
            if (jz_verbose) {
                double ms = (double)(clock() - step_t0) / CLOCKS_PER_SEC * 1000.0;
                if (ms > 1.0) fprintf(stderr, "[verbose]       signals: %.1f ms\n", ms);
            }

            /* Store bus_map for second pass. */
            all_bus_maps[i] = bus_map;
            all_bus_map_counts[i] = bus_map_count;

            /* MEM declarations for this module → IR_Memory[]. */
            step_t0 = clock();
            if (ir_build_memories_for_module(scope,
                                             arena,
                                             &project_symbols,
                                             &mod->memories,
                                             &mod->num_memories) != 0) {
                goto ir_fail;
            }
            if (jz_verbose) {
                double ms = (double)(clock() - step_t0) / CLOCKS_PER_SEC * 1000.0;
                if (ms > 1.0) fprintf(stderr, "[verbose]       memories: %.1f ms\n", ms);
            }

            /* Create synthetic address register signals for SYNC read ports
             * so that qualified identifiers like "rom.read.addr" resolve via
             * the bus_map during statement lowering.
             */
            if (ir_create_mem_addr_signals(arena, mod,
                                            &bus_map, &bus_map_count) != 0) {
                goto ir_fail;
            }
            /* Update stored bus_map after possible reallocation. */
            all_bus_maps[i] = bus_map;
            all_bus_map_counts[i] = bus_map_count;

            /* Initial lowering of ASYNCHRONOUS block into IR_Stmt tree. */
            step_t0 = clock();
            mod->async_block = ir_build_async_block(arena,
                                                    scope,
                                                    &project_symbols,
                                                    bus_map,
                                                    bus_map_count,
                                                    diagnostics);
            if (jz_verbose) {
                double ms = (double)(clock() - step_t0) / CLOCKS_PER_SEC * 1000.0;
                if (ms > 1.0) fprintf(stderr, "[verbose]       async_block: %.1f ms\n", ms);
            }

            /* SYNCHRONOUS blocks → IR_ClockDomain[] with per-domain statements
             * and register home clock-domain IDs.
             */
            step_t0 = clock();
            if (ir_build_clock_domains_for_module(scope,
                                                  mod,
                                                  arena,
                                                  &project_symbols,
                                                  bus_map,
                                                  bus_map_count,
                                                  diagnostics) != 0) {
                goto ir_fail;
            }
            if (jz_verbose) {
                double ms = (double)(clock() - step_t0) / CLOCKS_PER_SEC * 1000.0;
                if (ms > 1.0) fprintf(stderr, "[verbose]       clock_domains: %.1f ms\n", ms);
            }

            /* CDC declarations → IR_CDC[]. */
            if (ir_build_cdc_for_module(scope,
                                        mod,
                                        arena) != 0) {
                goto ir_fail;
            }

            if (jz_verbose) {
                double mod_ms = (double)(clock() - mod_t0) / CLOCKS_PER_SEC * 1000.0;
                if (mod_ms > 1.0) {
                    fprintf(stderr, "[verbose]     ir: module '%s': %.1f ms\n",
                            mod->name ? mod->name : "?", mod_ms);
                }
            }
        }

        if (jz_verbose) fprintf(stderr, "[verbose]   ir: first_pass (signals/mem/async/clocks): %.1f ms\n",
                                 (double)(clock() - ir_t0) / CLOCKS_PER_SEC * 1000.0);

        /* Second pass: build instances for all modules now that all signals
         * are available for child module lookups.
         */
        ir_t0 = clock();
        for (size_t i = 0; i < scope_count; ++i) {
            IR_Module *mod = &design->modules[i];
            const JZModuleScope *scope = &scopes[i];
            IR_BusSignalMapping *bus_map = all_bus_maps[i];
            int bus_map_count = all_bus_map_counts[i];

            if (ir_build_instances_for_module(scope,
                                              mod,
                                              scopes,
                                              scope_count,
                                              &project_symbols,
                                              specs,
                                              spec_count,
                                              design->modules,
                                              bus_map,
                                              bus_map_count,
                                              arena) != 0) {
                goto ir_fail;
            }
        }

        if (jz_verbose) fprintf(stderr, "[verbose]   ir: second_pass (instances): %.1f ms\n",
                                 (double)(clock() - ir_t0) / CLOCKS_PER_SEC * 1000.0);

        /* Next, materialize specialized modules by cloning their base modules
         * and updating identity, ownership, and width/depth metadata using the
         * override set for each specialization.
         */
        for (int si = 0; si < spec_count; ++si) {
            IR_ModuleSpec *spec = &specs[si];
            int base_index = spec->base_scope_index;
            int spec_index = spec->spec_module_index;
            if (base_index < 0 || spec_index < 0) {
                continue;
            }
            if ((size_t)base_index >= scope_count || (size_t)spec_index >= total_modules) {
                continue;
            }

            IR_Module *base_mod = &design->modules[base_index];
            IR_Module *mod      = &design->modules[spec_index];
            const JZModuleScope *child_scope = &scopes[(size_t)base_index];

            /* Copy structural data from the base module. */
            memcpy(mod, base_mod, sizeof(IR_Module));

            mod->id             = spec_index;
            mod->name           = spec->spec_name ? spec->spec_name : ir_strdup_arena(arena, "");
            mod->base_module_id = base_index;

            /* Give the specialization its own IR_Signal array so that
             * owner_module_id reflects the specialized module id and widths can
             * be re-elaborated under the OVERRIDE environment.
             */
            if (base_mod->num_signals > 0 && base_mod->signals) {
                mod->signals = (IR_Signal *)jz_arena_alloc(arena,
                                                           sizeof(IR_Signal) * (size_t)base_mod->num_signals);
                if (!mod->signals) {
                    goto ir_fail;
                }
                memcpy(mod->signals,
                       base_mod->signals,
                       sizeof(IR_Signal) * (size_t)base_mod->num_signals);
                mod->num_signals = base_mod->num_signals;
                for (int si2 = 0; si2 < mod->num_signals; ++si2) {
                    IR_Signal *sig = &mod->signals[si2];
                    sig->owner_module_id = mod->id;

                    /* Re-evaluate signal width using the original declaration's
                     * width expression and the specialization's override set.
                     */
                    const JZSymbol *sym = ir_find_symbol_by_signal_id(child_scope,
                                                                       sig->id);
                    if (sym && sym->node && sym->node->width) {
                        unsigned new_w = 0;
                        if (ir_eval_width_with_overrides(sym->node->width,
                                                         child_scope,
                                                         &project_symbols,
                                                         spec->overrides,
                                                         spec->num_overrides,
                                                         &new_w) == 0) {
                            sig->width = (int)new_w;
                        }
                    }
                }
            } else {
                mod->signals = NULL;
                mod->num_signals = 0;
            }

            /* Give the specialization its own IR_Memory array so that
             * word_width/depth/address_width can be re-elaborated under
             * OVERRIDE as well.
             */
            if (base_mod->num_memories > 0 && base_mod->memories) {
                mod->memories = (IR_Memory *)jz_arena_alloc(arena,
                                                            sizeof(IR_Memory) * (size_t)base_mod->num_memories);
                if (!mod->memories) {
                    goto ir_fail;
                }
                memcpy(mod->memories,
                       base_mod->memories,
                       sizeof(IR_Memory) * (size_t)base_mod->num_memories);
                mod->num_memories = base_mod->num_memories;

                /* For each memory, find the corresponding MEM_DECL in the child
                 * module AST by name and re-evaluate width/depth.
                 */
                JZASTNode *mod_node = child_scope->node;
                for (int mi2 = 0; mi2 < mod->num_memories; ++mi2) {
                    IR_Memory *m = &mod->memories[mi2];
                    JZASTNode *mem_decl = NULL;
                    if (mod_node) {
                        for (size_t ci = 0; ci < mod_node->child_count && !mem_decl; ++ci) {
                            JZASTNode *blk = mod_node->children[ci];
                            if (!blk || blk->type != JZ_AST_MEM_BLOCK) {
                                continue;
                            }
                            for (size_t dj = 0; dj < blk->child_count; ++dj) {
                                JZASTNode *md = blk->children[dj];
                                if (!md || md->type != JZ_AST_MEM_DECL || !md->name) {
                                    continue;
                                }
                                if (m->name && strcmp(md->name, m->name) == 0) {
                                    mem_decl = md;
                                    break;
                                }
                            }
                        }
                    }

                    if (mem_decl) {
                        /* Re-evaluate word width. */
                        unsigned word_w = 0;
                        if (mem_decl->width &&
                            ir_eval_width_with_overrides(mem_decl->width,
                                                         child_scope,
                                                         &project_symbols,
                                                         spec->overrides,
                                                         spec->num_overrides,
                                                         &word_w) == 0 &&
                            word_w > 0u) {
                            m->word_width = (int)word_w;
                        }

                        /* Re-evaluate depth. */
                        long long depth_val = 0;
                        if (mem_decl->text &&
                            ir_eval_const_with_overrides(mem_decl->text,
                                                         child_scope,
                                                         &project_symbols,
                                                         spec->overrides,
                                                         spec->num_overrides,
                                                         &depth_val) == 0 &&
                            depth_val > 0) {
                            m->depth = (int)depth_val;
                        }

                        /* Recompute address_width from depth. */
                        m->address_width = 0;
                        if (m->depth > 0) {
                            int aw = 0;
                            int d = m->depth - 1;
                            while (d > 0) {
                                aw++;
                                d >>= 1;
                            }
                            if (aw == 0) aw = 1;
                            m->address_width = aw;
                        }

                        /* Propagate new address_width into MEM ports. */
                        if (m->ports && m->num_ports > 0) {
                            for (int pi = 0; pi < m->num_ports; ++pi) {
                                m->ports[pi].address_width = m->address_width;
                            }
                        }

                        /* Re-resolve @file() initialization path for string
                         * overrides.  If the MEM's init is a FILE_REF whose
                         * CONST name matches a string override in this spec,
                         * update the file_path to the overridden value.
                         */
                        if (m->init_is_file && m->init.file_path) {
                            JZASTNode *init_expr = NULL;
                            if (mem_decl->child_count > 0) {
                                JZASTNode *first = mem_decl->children[0];
                                if (first && first->type != JZ_AST_MEM_PORT) {
                                    init_expr = first;
                                }
                            }
                            if (init_expr && init_expr->block_kind &&
                                strcmp(init_expr->block_kind, "FILE_REF") == 0 &&
                                init_expr->name) {
                                for (int oi = 0; oi < spec->num_overrides; ++oi) {
                                    if (spec->overrides[oi].string_value &&
                                        spec->overrides[oi].name &&
                                        strcmp(spec->overrides[oi].name,
                                               init_expr->name) == 0) {
                                        m->init.file_path = ir_strdup_arena(
                                            arena,
                                            spec->overrides[oi].string_value);
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            } else {
                mod->memories = NULL;
                mod->num_memories = 0;
            }

            /* Other members (clock_domains, instances, CDC, async block,
             * source file/line) already copied via memcpy above and are shared
             * read-only with the base module.
             */
        }

        /* Re-map sub-instance child_module_ids for specializations that have
         * string overrides.  When tone_gen is specialized with DATA_FILE=X,
         * its mel0 instance should point to melodies__SPEC_DATA_FILE_X, not
         * whichever melodies spec the base module happened to match.
         *
         * For each specialized module, clone its instances and re-match
         * sub-instances against sub-specializations using the parent spec's
         * string overrides.
         */
        for (int si = 0; si < spec_count; ++si) {
            IR_ModuleSpec *spec = &specs[si];
            int spec_index = spec->spec_module_index;
            if (spec_index < 0 || (size_t)spec_index >= total_modules) {
                continue;
            }

            /* Only process specs with string overrides. */
            int has_string_ov = 0;
            for (int k = 0; k < spec->num_overrides; ++k) {
                if (spec->overrides[k].string_value) {
                    has_string_ov = 1;
                    break;
                }
            }
            if (!has_string_ov) {
                continue;
            }

            IR_Module *mod = &design->modules[spec_index];
            if (!mod->instances || mod->num_instances <= 0) {
                continue;
            }

            /* Clone the instances array so we can modify child_module_ids. */
            IR_Instance *new_insts = (IR_Instance *)jz_arena_alloc(
                arena, sizeof(IR_Instance) * (size_t)mod->num_instances);
            if (!new_insts) {
                goto ir_fail;
            }
            memcpy(new_insts, mod->instances,
                   sizeof(IR_Instance) * (size_t)mod->num_instances);
            mod->instances = new_insts;

            /* For each instance, check if it has a matching sub-specialization
             * under this parent spec's override values.
             */
            const JZASTNode *parent_mod_ast = spec->base_mod_node;
            if (!parent_mod_ast) continue;

            for (int ii = 0; ii < mod->num_instances; ++ii) {
                IR_Instance *inst = &mod->instances[ii];
                int old_child = inst->child_module_id;
                if (old_child < 0) continue;

                /* Find the corresponding AST instance node by name. */
                JZASTNode *inst_ast = NULL;
                for (size_t ci = 0; ci < parent_mod_ast->child_count; ++ci) {
                    JZASTNode *ch = parent_mod_ast->children[ci];
                    if (!ch || ch->type != JZ_AST_MODULE_INSTANCE) continue;
                    if (ch->name && inst->name &&
                        strcmp(ch->name, inst->name) == 0) {
                        inst_ast = ch;
                        break;
                    }
                }
                if (!inst_ast) continue;

                /* Check for OVERRIDE block. */
                JZASTNode *ov_block = NULL;
                for (size_t c = 0; c < inst_ast->child_count; ++c) {
                    JZASTNode *child = inst_ast->children[c];
                    if (child && child->type == JZ_AST_CONST_BLOCK &&
                        child->block_kind &&
                        strcmp(child->block_kind, "OVERRIDE") == 0) {
                        ov_block = child;
                        break;
                    }
                }
                if (!ov_block) continue;

                /* Build the overrides with parent spec values substituted. */
                int sub_count = 0;
                for (size_t j = 0; j < ov_block->child_count; ++j) {
                    JZASTNode *ov = ov_block->children[j];
                    if (ov && ov->type == JZ_AST_CONST_DECL && ov->name)
                        sub_count++;
                }
                if (sub_count == 0) continue;

                IR_ModuleSpecOverride *sub_ov = (IR_ModuleSpecOverride *)
                    malloc(sizeof(IR_ModuleSpecOverride) * (size_t)sub_count);
                if (!sub_ov) goto ir_fail;
                memset(sub_ov, 0,
                       sizeof(IR_ModuleSpecOverride) * (size_t)sub_count);

                int sub_idx = 0;
                for (size_t j = 0; j < ov_block->child_count && sub_idx < sub_count; ++j) {
                    JZASTNode *ov = ov_block->children[j];
                    if (!ov || ov->type != JZ_AST_CONST_DECL || !ov->name)
                        continue;

                    if (ov->block_kind &&
                        strcmp(ov->block_kind, "STRING") == 0) {
                        /* Substitute parent spec's string override. */
                        const char *val = ov->text;
                        for (int k = 0; k < spec->num_overrides; ++k) {
                            if (spec->overrides[k].string_value &&
                                spec->overrides[k].name &&
                                strcmp(spec->overrides[k].name,
                                       ov->name) == 0) {
                                val = spec->overrides[k].string_value;
                                break;
                            }
                        }
                        sub_ov[sub_idx].name = ov->name;
                        sub_ov[sub_idx].string_value = val;
                    } else {
                        sub_ov[sub_idx].name = ov->name;
                        sub_ov[sub_idx].string_value = NULL;
                        /* Integer value: use the base module's resolved value.
                         * We can't easily re-evaluate here, so just try to
                         * match by name only for integer overrides.
                         */
                    }
                    sub_idx++;
                }

                if (sub_idx > 1) {
                    qsort(sub_ov, (size_t)sub_idx,
                          sizeof(IR_ModuleSpecOverride),
                          ir_override_name_cmp);
                }

                /* Find the child module's base scope index. */
                int child_base = -1;
                if ((size_t)old_child < total_modules) {
                    int bm = design->modules[old_child].base_module_id;
                    child_base = (bm >= 0) ? bm : old_child;
                }

                /* Match against sub-specializations. */
                for (int ssi = 0; ssi < spec_count; ++ssi) {
                    const IR_ModuleSpec *sub_spec = &specs[ssi];
                    if (sub_spec->base_scope_index != child_base) continue;
                    if (sub_spec->num_overrides != sub_idx) continue;
                    int match = 1;
                    for (int k = 0; k < sub_idx; ++k) {
                        const IR_ModuleSpecOverride *a = &sub_spec->overrides[k];
                        const IR_ModuleSpecOverride *b = &sub_ov[k];
                        if ((!a->name && b->name) ||
                            (a->name && !b->name) ||
                            (a->name && b->name &&
                             strcmp(a->name, b->name) != 0)) {
                            match = 0; break;
                        }
                        if (a->string_value || b->string_value) {
                            if (!a->string_value || !b->string_value ||
                                strcmp(a->string_value,
                                       b->string_value) != 0) {
                                match = 0; break;
                            }
                        } else if (a->value != b->value) {
                            match = 0; break;
                        }
                    }
                    if (match && sub_spec->spec_module_index >= 0) {
                        inst->child_module_id =
                            sub_spec->spec_module_index;
                        break;
                    }
                }

                free(sub_ov);
            }
        }
    }

    /* Source files array already populated above from project/modules.
     * line_count values remain 0 until we wire in file-length metadata.
     */
    /* Project-level IR: build IR_Project when root is a @project node. */
    design->project = NULL;
    if (root->type == JZ_AST_PROJECT) {
        IR_Project *proj = (IR_Project *)jz_arena_alloc(arena, sizeof(IR_Project));
        if (!proj) {
            goto ir_fail;
        }
        memset(proj, 0, sizeof(*proj));

        /* Project name and chip ID. */
        proj->name = root->name ? ir_strdup_arena(arena, root->name) : ir_strdup_arena(arena, "");
        proj->chip_id = (root->text && root->text[0]) ? ir_strdup_arena(arena, root->text) : NULL;

        /* CLOCKS block → IR_Clock[]. */
        JZASTNode *clocks_blk = NULL;
        for (size_t i = 0; i < root->child_count; ++i) {
            JZASTNode *child = root->children[i];
            if (child && child->type == JZ_AST_CLOCKS_BLOCK) {
                clocks_blk = child;
                break;
            }
        }
        if (clocks_blk && clocks_blk->child_count > 0) {
            int clock_count = 0;
            for (size_t i = 0; i < clocks_blk->child_count; ++i) {
                JZASTNode *decl = clocks_blk->children[i];
                if (decl && decl->type == JZ_AST_CONST_DECL && decl->name) {
                    clock_count++;
                }
            }
            if (clock_count > 0) {
                IR_Clock *clocks = (IR_Clock *)jz_arena_alloc(arena, sizeof(IR_Clock) * (size_t)clock_count);
                if (!clocks) {
                    goto ir_fail;
                }
                memset(clocks, 0, sizeof(IR_Clock) * (size_t)clock_count);
                int ci = 0;
                for (size_t i = 0; i < clocks_blk->child_count && ci < clock_count; ++i) {
                    JZASTNode *decl = clocks_blk->children[i];
                    if (!decl || decl->type != JZ_AST_CONST_DECL || !decl->name) {
                        continue;
                    }
                    IR_Clock *c = &clocks[ci++];
                    c->name = ir_strdup_arena(arena, decl->name);
                    double period_ns = 0.0;
                    IR_ClockEdge edge = EDGE_RISING;
                    ir_clock_parse_attrs(decl->text, &period_ns, &edge);
                    c->period_ns = period_ns;
                    c->edge = edge;
                }
                proj->clocks = clocks;
                proj->num_clocks = clock_count;
            }
        }

        /* CLOCK_GEN blocks → IR_ClockGen[]. */
        int clock_gen_count = 0;
        for (size_t i = 0; i < root->child_count; ++i) {
            JZASTNode *child = root->children[i];
            if (child && child->type == JZ_AST_CLOCK_GEN_BLOCK) {
                clock_gen_count++;
            }
        }
        if (clock_gen_count > 0) {
            IR_ClockGen *clock_gens = (IR_ClockGen *)jz_arena_alloc(arena, sizeof(IR_ClockGen) * (size_t)clock_gen_count);
            if (!clock_gens) {
                goto ir_fail;
            }
            memset(clock_gens, 0, sizeof(IR_ClockGen) * (size_t)clock_gen_count);
            int cgi = 0;
            for (size_t i = 0; i < root->child_count && cgi < clock_gen_count; ++i) {
                JZASTNode *cgen_blk = root->children[i];
                if (!cgen_blk || cgen_blk->type != JZ_AST_CLOCK_GEN_BLOCK) {
                    continue;
                }

                IR_ClockGen *cg = &clock_gens[cgi++];

                /* Count units (PLL/DLL) */
                int unit_count = 0;
                for (size_t u = 0; u < cgen_blk->child_count; ++u) {
                    JZASTNode *unit = cgen_blk->children[u];
                    if (unit && unit->type == JZ_AST_CLOCK_GEN_UNIT) {
                        unit_count++;
                    }
                }

                if (unit_count > 0) {
                    IR_ClockGenUnit *units = (IR_ClockGenUnit *)jz_arena_alloc(arena, sizeof(IR_ClockGenUnit) * (size_t)unit_count);
                    if (!units) {
                        goto ir_fail;
                    }
                    memset(units, 0, sizeof(IR_ClockGenUnit) * (size_t)unit_count);

                    int ui = 0;
                    for (size_t u = 0; u < cgen_blk->child_count && ui < unit_count; ++u) {
                        JZASTNode *unit_ast = cgen_blk->children[u];
                        if (!unit_ast || unit_ast->type != JZ_AST_CLOCK_GEN_UNIT) {
                            continue;
                        }

                        IR_ClockGenUnit *unit = &units[ui++];
                        /* Store generator type as lowercase string */
                        if (unit_ast->name) {
                            size_t nlen = strlen(unit_ast->name);
                            char *lower = (char *)jz_arena_alloc(arena, nlen + 1);
                            if (!lower) goto ir_fail;
                            for (size_t li = 0; li < nlen; ++li)
                                lower[li] = (char)tolower((unsigned char)unit_ast->name[li]);
                            lower[nlen] = '\0';
                            unit->type = lower;
                        } else {
                            unit->type = ir_strdup_arena(arena, "pll");
                        }

                        /* Count inputs, outputs and configs */
                        int input_count = 0;
                        int output_count = 0;
                        int config_count = 0;
                        for (size_t c = 0; c < unit_ast->child_count; ++c) {
                            JZASTNode *elem = unit_ast->children[c];
                            if (!elem) continue;
                            if (elem->type == JZ_AST_CLOCK_GEN_IN) {
                                input_count++;
                            } else if (elem->type == JZ_AST_CLOCK_GEN_OUT || elem->type == JZ_AST_CLOCK_GEN_WIRE) {
                                output_count++;
                            } else if (elem->type == JZ_AST_CLOCK_GEN_CONFIG) {
                                for (size_t p = 0; p < elem->child_count; ++p) {
                                    if (elem->children[p] && elem->children[p]->type == JZ_AST_CONST_DECL) {
                                        config_count++;
                                    }
                                }
                            }
                        }

                        /* Build inputs array */
                        if (input_count > 0) {
                            IR_ClockGenInput *inputs = (IR_ClockGenInput *)jz_arena_alloc(arena, sizeof(IR_ClockGenInput) * (size_t)input_count);
                            if (!inputs) {
                                goto ir_fail;
                            }
                            memset(inputs, 0, sizeof(IR_ClockGenInput) * (size_t)input_count);
                            int ii = 0;
                            for (size_t c = 0; c < unit_ast->child_count && ii < input_count; ++c) {
                                JZASTNode *in_ast = unit_ast->children[c];
                                if (!in_ast || in_ast->type != JZ_AST_CLOCK_GEN_IN) continue;
                                IR_ClockGenInput *inp = &inputs[ii++];
                                inp->selector = in_ast->block_kind
                                              ? ir_strdup_arena(arena, in_ast->block_kind) : NULL;
                                inp->signal_name = in_ast->name
                                                 ? ir_strdup_arena(arena, in_ast->name) : NULL;
                            }
                            unit->inputs = inputs;
                            unit->num_inputs = input_count;
                        }

                        /* Build outputs array */
                        if (output_count > 0) {
                            IR_ClockGenOutput *outputs = (IR_ClockGenOutput *)jz_arena_alloc(arena, sizeof(IR_ClockGenOutput) * (size_t)output_count);
                            if (!outputs) {
                                goto ir_fail;
                            }
                            memset(outputs, 0, sizeof(IR_ClockGenOutput) * (size_t)output_count);
                            int oi = 0;
                            for (size_t c = 0; c < unit_ast->child_count && oi < output_count; ++c) {
                                JZASTNode *out_ast = unit_ast->children[c];
                                if (!out_ast || (out_ast->type != JZ_AST_CLOCK_GEN_OUT &&
                                                 out_ast->type != JZ_AST_CLOCK_GEN_WIRE)) continue;
                                IR_ClockGenOutput *out = &outputs[oi++];
                                out->selector = out_ast->block_kind
                                              ? ir_strdup_arena(arena, out_ast->block_kind) : NULL;
                                out->clock_name = out_ast->name
                                                ? ir_strdup_arena(arena, out_ast->name) : NULL;
                                out->is_clock = (out_ast->type == JZ_AST_CLOCK_GEN_OUT) ? 1 : 0;
                            }
                            unit->outputs = outputs;
                            unit->num_outputs = output_count;
                        }

                        /* Build configs array */
                        if (config_count > 0) {
                            IR_ClockGenConfig *configs = (IR_ClockGenConfig *)jz_arena_alloc(arena, sizeof(IR_ClockGenConfig) * (size_t)config_count);
                            if (!configs) {
                                goto ir_fail;
                            }
                            memset(configs, 0, sizeof(IR_ClockGenConfig) * (size_t)config_count);
                            int cfg_i = 0;
                            for (size_t c = 0; c < unit_ast->child_count; ++c) {
                                JZASTNode *cfg_blk = unit_ast->children[c];
                                if (!cfg_blk || cfg_blk->type != JZ_AST_CLOCK_GEN_CONFIG) continue;
                                for (size_t p = 0; p < cfg_blk->child_count && cfg_i < config_count; ++p) {
                                    JZASTNode *param = cfg_blk->children[p];
                                    if (!param || param->type != JZ_AST_CONST_DECL) continue;
                                    IR_ClockGenConfig *cfg = &configs[cfg_i++];
                                    cfg->param_name = param->name
                                                    ? ir_strdup_arena(arena, param->name) : NULL;
                                    cfg->param_value = param->text
                                                     ? ir_strdup_arena(arena, param->text) : NULL;
                                }
                            }
                            unit->configs = configs;
                            unit->num_configs = config_count;
                        }
                    }
                    cg->units = units;
                    cg->num_units = unit_count;
                }
            }
            proj->clock_gens = clock_gens;
            proj->num_clock_gens = clock_gen_count;
        }

        /* Compute PLL/DLL output clock periods from chip data expressions. */
        for (int cgi = 0; cgi < proj->num_clock_gens; ++cgi) {
            const IR_ClockGen *cg = &proj->clock_gens[cgi];

            /* Load chip data from the project-level chip ID. */
            JZChipData chip_data;
            memset(&chip_data, 0, sizeof(chip_data));
            int have_chip = 0;
            if (proj->chip_id) {
                JZChipLoadStatus st = jz_chip_data_load(proj->chip_id, NULL, &chip_data);
                if (st == JZ_CHIP_LOAD_OK) have_chip = 1;
            }

            for (int ui = 0; ui < cg->num_units; ++ui) {
                const IR_ClockGenUnit *unit = &cg->units[ui];
                int is_osc_type = (unit->type && strncmp(unit->type, "osc", 3) == 0);

                /* Find REF_CLK input signal name */
                const char *ref_clk_name = NULL;
                for (int ii = 0; ii < unit->num_inputs; ++ii) {
                    if (unit->inputs[ii].selector &&
                        strcmp(unit->inputs[ii].selector, "REF_CLK") == 0) {
                        ref_clk_name = unit->inputs[ii].signal_name;
                        break;
                    }
                }
                if (!ref_clk_name && !is_osc_type) continue;

                const char *unit_type = unit->type ? unit->type : "pll";

                /* Find input clock period. OSC has no input clock. */
                double input_period = 0.0;
                if (ref_clk_name) {
                    for (int ci = 0; ci < proj->num_clocks; ++ci) {
                        if (proj->clocks[ci].name &&
                            strcmp(proj->clocks[ci].name, ref_clk_name) == 0) {
                            input_period = proj->clocks[ci].period_ns;
                            break;
                        }
                    }
                }
                if (input_period <= 0.0 && !is_osc_type) continue;

                /* Set output clock periods and mark as generated. */
                for (int oi = 0; oi < unit->num_outputs; ++oi) {
                    const IR_ClockGenOutput *out = &unit->outputs[oi];
                    if (!out->clock_name || !out->selector) continue;

                    /* Look up the frequency expression from chip data. */
                    const char *freq_expr = have_chip
                        ? jz_chip_clock_gen_output_freq_expr(&chip_data, unit_type, out->selector)
                        : NULL;
                    if (!freq_expr) continue;

                    /* Evaluate the frequency expression. The expression may
                     * reference CONFIG params, chip defaults, derived values
                     * (like FVCO), and refclk_period_ns.  We use a simple
                     * recursive-descent evaluator matching driver_project_hw.c.
                     */
                    double freq_mhz = ir_eval_cgen_expr(
                        freq_expr, unit, input_period,
                        have_chip ? &chip_data : NULL, unit_type);
                    if (freq_mhz <= 0.0) continue;

                    double period_ns = 1000.0 / freq_mhz;

                    /* Evaluate phase expression if present. */
                    double phase_deg = 0.0;
                    const char *phase_expr = have_chip
                        ? jz_chip_clock_gen_output_phase_expr(&chip_data, unit_type, out->selector)
                        : NULL;
                    if (phase_expr) {
                        phase_deg = ir_eval_cgen_expr(
                            phase_expr, unit, input_period,
                            have_chip ? &chip_data : NULL, unit_type);
                    }

                    for (int ci = 0; ci < proj->num_clocks; ++ci) {
                        IR_Clock *clk = &proj->clocks[ci];
                        if (clk->name &&
                            strcmp(clk->name, out->clock_name) == 0) {
                            clk->period_ns = period_ns;
                            clk->phase_deg = phase_deg;
                            clk->is_generated = true;
                            break;
                        }
                    }
                }
            }

            if (have_chip) jz_chip_data_free(&chip_data);
        }

        /* Project pins (IN_PINS/OUT_PINS/INOUT_PINS) from project_symbols. */
        int pin_count = 0;
        if (project_symbols.data) {
            const JZSymbol *syms = (const JZSymbol *)project_symbols.data;
            size_t sym_count = project_symbols.len / sizeof(JZSymbol);
            for (size_t i = 0; i < sym_count; ++i) {
                if (syms[i].kind == JZ_SYM_PIN) {
                    pin_count++;
                }
            }
            if (pin_count > 0) {
                IR_Pin *pins = (IR_Pin *)jz_arena_alloc(arena, sizeof(IR_Pin) * (size_t)pin_count);
                if (!pins) {
                    goto ir_fail;
                }
                memset(pins, 0, sizeof(IR_Pin) * (size_t)pin_count);

                int pi = 0;
                for (size_t i = 0; i < sym_count && pi < pin_count; ++i) {
                    const JZSymbol *s = &syms[i];
                    if (s->kind != JZ_SYM_PIN || !s->node || !s->name) {
                        continue;
                    }
                    IR_Pin *p = &pins[pi];
                    p->id = pi;
                    p->name = ir_strdup_arena(arena, s->name);

                    const char *blk = s->node->block_kind ? s->node->block_kind : "";
                    if (strcmp(blk, "IN_PINS") == 0) {
                        p->kind = PIN_IN;
                    } else if (strcmp(blk, "OUT_PINS") == 0) {
                        p->kind = PIN_OUT;
                    } else if (strcmp(blk, "INOUT_PINS") == 0) {
                        p->kind = PIN_INOUT;
                    } else {
                        p->kind = PIN_IN;
                    }

                    /* Width: default 1 when unspecified; try to parse simple
                     * positive integer width when present.  Fall back to
                     * full CONFIG-aware const evaluation for expressions
                     * like CONFIG.DATA_WIDTH.
                     */
                    p->width = 1;
                    if (s->node->width) {
                        unsigned w = 0;
                        int rc = eval_simple_positive_decl_int(s->node->width, &w);
                        if (rc == 1 && w > 0u) {
                            p->width = (int)w;
                        } else if (project_symbols.data) {
                            /* Count CONFIG entries. */
                            const JZSymbol *psyms2 = (const JZSymbol *)project_symbols.data;
                            size_t pcount2 = project_symbols.len / sizeof(JZSymbol);
                            size_t cfg_count = 0;
                            for (size_t ci = 0; ci < pcount2; ++ci) {
                                if (psyms2[ci].kind == JZ_SYM_CONFIG &&
                                    psyms2[ci].node && psyms2[ci].node->name) {
                                    ++cfg_count;
                                }
                            }
                            if (cfg_count > 0) {
                                size_t cfg_total = cfg_count + 1; /* +1 anonymous */
                                JZConstDef *cfg_defs = (JZConstDef *)calloc(cfg_total, sizeof(JZConstDef));
                                long long  *cfg_vals = (long long *)calloc(cfg_total, sizeof(long long));
                                if (cfg_defs && cfg_vals) {
                                    size_t ci2 = 0;
                                    for (size_t k = 0; k < pcount2; ++k) {
                                        if (psyms2[k].kind == JZ_SYM_CONFIG &&
                                            psyms2[k].node && psyms2[k].node->name) {
                                            cfg_defs[ci2].name = psyms2[k].node->name;
                                            cfg_defs[ci2].expr = psyms2[k].node->width
                                                                  ? psyms2[k].node->width
                                                                  : psyms2[k].node->text;
                                            ++ci2;
                                        }
                                    }
                                    /* Strip CONFIG. prefix from the width expr. */
                                    const char *wexpr = s->node->width;
                                    char wbuf[256];
                                    {
                                        const char *sp2 = wexpr;
                                        size_t wo = 0;
                                        while (*sp2 && wo < sizeof(wbuf) - 1) {
                                            if (sp2[0] == 'C' && strncmp(sp2, "CONFIG", 6) == 0) {
                                                int ws = (sp2 == wexpr) ||
                                                    !((sp2[-1] >= 'A' && sp2[-1] <= 'Z') ||
                                                      (sp2[-1] >= 'a' && sp2[-1] <= 'z') ||
                                                      (sp2[-1] >= '0' && sp2[-1] <= '9') ||
                                                      sp2[-1] == '_');
                                                if (ws) {
                                                    const char *rr = sp2 + 6;
                                                    while (*rr && isspace((unsigned char)*rr)) ++rr;
                                                    if (*rr == '.') {
                                                        ++rr;
                                                        while (*rr && isspace((unsigned char)*rr)) ++rr;
                                                        sp2 = rr;
                                                        continue;
                                                    }
                                                }
                                            }
                                            wbuf[wo++] = *sp2++;
                                        }
                                        wbuf[wo] = '\0';
                                    }
                                    cfg_defs[ci2].name = NULL;
                                    cfg_defs[ci2].expr = wbuf;
                                    if (jz_const_eval_all(cfg_defs, cfg_total, NULL, cfg_vals) == 0 &&
                                        cfg_vals[cfg_total - 1] > 0) {
                                        p->width = (int)cfg_vals[cfg_total - 1];
                                    }
                                }
                                free(cfg_defs);
                                free(cfg_vals);
                            }
                        }
                    }

                    /* Electrical standard and drive strength come from attrs. */
                    const char *attrs = s->node->text ? s->node->text : "";
                    char standard[64];
                    standard[0] = '\0';
                    int drive_ma = -1;
                    double drive_frac = -1.0;

                    const char *s_attr = strstr(attrs, "standard");
                    if (s_attr) {
                        s_attr = strchr(s_attr, '=');
                        if (s_attr) {
                            ++s_attr;
                            while (*s_attr && isspace((unsigned char)*s_attr)) ++s_attr;
                            char val[64];
                            size_t len = 0;
                            while (s_attr[len] && !isspace((unsigned char)s_attr[len]) &&
                                   s_attr[len] != ',' && s_attr[len] != ';' && s_attr[len] != '}') {
                                ++len;
                            }
                            if (len >= sizeof(val)) len = sizeof(val) - 1u;
                            memcpy(val, s_attr, len);
                            val[len] = '\0';
                            ir_trim_copy(val, standard, sizeof(standard));
                        }
                    }

                    const char *d_attr = strstr(attrs, "drive");
                    if (d_attr) {
                        d_attr = strchr(d_attr, '=');
                        if (d_attr) {
                            ++d_attr;
                            while (*d_attr && isspace((unsigned char)*d_attr)) ++d_attr;
                            char *endptr = NULL;
                            double dv = strtod(d_attr, &endptr);
                            if (endptr != d_attr && dv > 0.0) {
                                drive_frac = dv;
                                drive_ma = (int)dv;
                            }
                        }
                    }

                    p->standard = ir_strdup_arena(arena, standard);
                    p->drive_ma = drive_ma;
                    p->drive = drive_frac;

                    /* Mode (SINGLE / DIFFERENTIAL) */
                    p->mode = PIN_MODE_SINGLE;
                    {
                        const char *m_attr = strstr(attrs, "mode");
                        if (m_attr) {
                            m_attr = strchr(m_attr, '=');
                            if (m_attr) {
                                ++m_attr;
                                while (*m_attr && isspace((unsigned char)*m_attr)) ++m_attr;
                                if (strncmp(m_attr, "DIFFERENTIAL", 12) == 0) {
                                    p->mode = PIN_MODE_DIFFERENTIAL;
                                }
                            }
                        }
                    }

                    /* Termination (ON / OFF) */
                    p->term = 0;
                    {
                        const char *t_attr = strstr(attrs, "term");
                        if (t_attr) {
                            t_attr = strchr(t_attr, '=');
                            if (t_attr) {
                                ++t_attr;
                                while (*t_attr && isspace((unsigned char)*t_attr)) ++t_attr;
                                if (strncmp(t_attr, "ON", 2) == 0) {
                                    p->term = 1;
                                }
                            }
                        }
                    }

                    /* Pull (UP / DOWN / NONE) */
                    p->pull = PULL_NONE;
                    {
                        const char *pl_attr = strstr(attrs, "pull");
                        if (pl_attr) {
                            pl_attr = strchr(pl_attr, '=');
                            if (pl_attr) {
                                ++pl_attr;
                                while (*pl_attr && isspace((unsigned char)*pl_attr)) ++pl_attr;
                                if (strncmp(pl_attr, "UP", 2) == 0) {
                                    p->pull = PULL_UP;
                                } else if (strncmp(pl_attr, "DOWN", 4) == 0) {
                                    p->pull = PULL_DOWN;
                                }
                            }
                        }
                    }

                    /* fclk (fast/serializer clock) */
                    p->fclk_name = NULL;
                    {
                        const char *fc_attr = strstr(attrs, "fclk");
                        if (fc_attr) {
                            fc_attr = strchr(fc_attr, '=');
                            if (fc_attr) {
                                ++fc_attr;
                                while (*fc_attr && isspace((unsigned char)*fc_attr)) ++fc_attr;
                                char val[64];
                                size_t len2 = 0;
                                while (fc_attr[len2] && !isspace((unsigned char)fc_attr[len2]) &&
                                       fc_attr[len2] != ',' && fc_attr[len2] != ';' && fc_attr[len2] != '}') {
                                    ++len2;
                                }
                                if (len2 > 0 && len2 < sizeof(val)) {
                                    memcpy(val, fc_attr, len2);
                                    val[len2] = '\0';
                                    p->fclk_name = ir_strdup_arena(arena, val);
                                }
                            }
                        }
                    }

                    /* pclk (parallel data clock) */
                    p->pclk_name = NULL;
                    {
                        const char *pc_attr = strstr(attrs, "pclk");
                        if (pc_attr) {
                            pc_attr = strchr(pc_attr, '=');
                            if (pc_attr) {
                                ++pc_attr;
                                while (*pc_attr && isspace((unsigned char)*pc_attr)) ++pc_attr;
                                char val[64];
                                size_t len2 = 0;
                                while (pc_attr[len2] && !isspace((unsigned char)pc_attr[len2]) &&
                                       pc_attr[len2] != ',' && pc_attr[len2] != ';' && pc_attr[len2] != '}') {
                                    ++len2;
                                }
                                if (len2 > 0 && len2 < sizeof(val)) {
                                    memcpy(val, pc_attr, len2);
                                    val[len2] = '\0';
                                    p->pclk_name = ir_strdup_arena(arena, val);
                                }
                            }
                        }
                    }

                    /* reset (serializer reset signal, typically PLL lock) */
                    p->reset_name = NULL;
                    {
                        const char *rs_attr = strstr(attrs, "reset");
                        if (rs_attr) {
                            rs_attr = strchr(rs_attr, '=');
                            if (rs_attr) {
                                ++rs_attr;
                                while (*rs_attr && isspace((unsigned char)*rs_attr)) ++rs_attr;
                                char val[64];
                                size_t len2 = 0;
                                while (rs_attr[len2] && !isspace((unsigned char)rs_attr[len2]) &&
                                       rs_attr[len2] != ',' && rs_attr[len2] != ';' && rs_attr[len2] != '}') {
                                    ++len2;
                                }
                                if (len2 > 0 && len2 < sizeof(val)) {
                                    memcpy(val, rs_attr, len2);
                                    val[len2] = '\0';
                                    p->reset_name = ir_strdup_arena(arena, val);
                                }
                            }
                        }
                    }

                    ++pi;
                }

                proj->pins = pins;
                proj->num_pins = pin_count;
            }
        }

        /* MAP block → IR_PinMapping[]. */
        JZASTNode *map_blk = NULL;
        for (size_t i = 0; i < root->child_count; ++i) {
            JZASTNode *child = root->children[i];
            if (child && child->type == JZ_AST_MAP_BLOCK) {
                map_blk = child;
                break;
            }
        }
        if (map_blk && map_blk->child_count > 0) {
            int map_count = 0;
            for (size_t i = 0; i < map_blk->child_count; ++i) {
                JZASTNode *entry = map_blk->children[i];
                if (entry && entry->type == JZ_AST_CONST_DECL && entry->name) {
                    map_count++;
                }
            }
            if (map_count > 0) {
                IR_PinMapping *maps = (IR_PinMapping *)jz_arena_alloc(arena, sizeof(IR_PinMapping) * (size_t)map_count);
                if (!maps) {
                    goto ir_fail;
                }
                memset(maps, 0, sizeof(IR_PinMapping) * (size_t)map_count);

                int mi2 = 0;
                for (size_t i = 0; i < map_blk->child_count && mi2 < map_count; ++i) {
                    JZASTNode *entry = map_blk->children[i];
                    if (!entry || entry->type != JZ_AST_CONST_DECL || !entry->name) {
                        continue;
                    }
                    IR_PinMapping *pm = &maps[mi2++];
                    char pin_name[128];
                    int bit_index = -1;
                    if (!ir_parse_map_lhs(entry->name, pin_name, sizeof(pin_name), &bit_index)) {
                        pin_name[0] = '\0';
                        bit_index = -1;
                    }
                    pm->logical_pin_name = ir_strdup_arena(arena, pin_name);
                    pm->bit_index = bit_index;
                    pm->board_pin_n_id = NULL;

                    char rhs[256];
                    ir_trim_copy(entry->text ? entry->text : "", rhs, sizeof(rhs));

                    if (rhs[0] == '{') {
                        /* Differential pair: { P=<id>, N=<id> } */
                        char p_val[64] = {0};
                        char n_val[64] = {0};
                        const char *pp = strstr(rhs, "P");
                        if (pp) {
                            pp = strchr(pp, '=');
                            if (pp) {
                                ++pp;
                                while (*pp && isspace((unsigned char)*pp)) ++pp;
                                size_t plen = 0;
                                while (pp[plen] && pp[plen] != ',' && pp[plen] != '}' &&
                                       !isspace((unsigned char)pp[plen])) ++plen;
                                if (plen > 0 && plen < sizeof(p_val)) {
                                    memcpy(p_val, pp, plen);
                                    p_val[plen] = '\0';
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
                                if (nlen > 0 && nlen < sizeof(n_val)) {
                                    memcpy(n_val, np, nlen);
                                    n_val[nlen] = '\0';
                                }
                            }
                        }
                        pm->board_pin_id = ir_strdup_arena(arena, p_val);
                        pm->board_pin_n_id = ir_strdup_arena(arena, n_val);
                    } else {
                        pm->board_pin_id = ir_strdup_arena(arena, rhs);
                    }
                }

                proj->mappings = maps;
                proj->num_mappings = map_count;
            }
        }

        /* Top module id and bindings. */
        JZASTNode *top_new = NULL;
        for (size_t i = 0; i < root->child_count; ++i) {
            JZASTNode *child = root->children[i];
            if (child && child->type == JZ_AST_PROJECT_TOP_INSTANCE) {
                top_new = child;
                break;
            }
        }
        proj->top_module_id = -1;
        proj->top_bindings = NULL;
        proj->num_top_bindings = 0;

        if (top_new && top_new->name) {
            const JZSymbol *top_sym = ir_project_lookup_module_or_blackbox(&project_symbols,
                                                                           top_new->name);
            if (top_sym && top_sym->node && top_sym->kind == JZ_SYM_MODULE) {
                int top_mod_index = ir_find_module_index_for_node(scopes,
                                                                   scope_count,
                                                                   top_sym->node);
                if (top_mod_index >= 0) {
                    proj->top_module_id = top_mod_index;

                    /* Build top bindings. For bus ports and bus pins, expand
                     * bindings per bit using IR_PinMapping so that consumers
                     * can unambiguously relate top port bits to board pins.
                     */
                    int bind_count = 0;
                    for (size_t i = 0; i < top_new->child_count; ++i) {
                        JZASTNode *b = top_new->children[i];
                        if (b && b->type == JZ_AST_PORT_DECL && b->name) {
                            bind_count++;
                        }
                    }
                    if (bind_count > 0) {
                        /* Upper bound on bindings: one per bit of each top
                         * module port. This is sufficient even when
                         * concatenation expressions are used in @top bindings.
                         */
                        IR_Module *top_mod = &design->modules[top_mod_index];
                        /* We'll identify top_scope (module scope for the top
                         * module) below once and reuse it both for width and
                         * binding construction.
                         */
                        const JZModuleScope *top_scope = NULL;
                        for (size_t si = 0; si < scope_count; ++si) {
                            if (scopes[si].node == top_sym->node) {
                                top_scope = &scopes[si];
                                break;
                            }
                        }

                        int max_bindings = 0;
                        for (size_t i = 0; i < top_new->child_count; ++i) {
                            JZASTNode *b = top_new->children[i];
                            if (!b || b->type != JZ_AST_PORT_DECL || !b->name) {
                                continue;
                            }
                            int port_id_tmp = -1;
                            int width_tmp = 1;
                            if (top_scope) {
                                const JZSymbol *psym = module_scope_lookup_kind(top_scope,
                                                                                 b->name,
                                                                                 JZ_SYM_PORT);
                                if (psym) {
                                    port_id_tmp = psym->id;
                                }
                            }
                            if (top_mod && port_id_tmp >= 0) {
                                for (int si2 = 0; si2 < top_mod->num_signals; ++si2) {
                                    const IR_Signal *sig = &top_mod->signals[si2];
                                    if (sig->id == port_id_tmp) {
                                        width_tmp = (sig->width > 0) ? sig->width : 1;
                                        break;
                                    }
                                }
                            }
                            max_bindings += width_tmp;
                        }
                        if (max_bindings <= 0) {
                            max_bindings = bind_count;
                        }

                        IR_TopBinding *bindings = (IR_TopBinding *)jz_arena_alloc(
                            arena, sizeof(IR_TopBinding) * (size_t)max_bindings);
                        if (!bindings) {
                            goto ir_fail;
                        }
                        memset(bindings, 0, sizeof(IR_TopBinding) * (size_t)max_bindings);

                        /* top_scope already computed above for this top
                         * module; reuse it to map port names to IR_Signal ids.
                         */

                        int bi = 0;
                        for (size_t i = 0; i < top_new->child_count && bi < max_bindings; ++i) {
                            JZASTNode *b = top_new->children[i];
                            if (!b || b->type != JZ_AST_PORT_DECL || !b->name) {
                                continue;
                            }

                            /* top_port_signal_id: semantic id and width of the module port. */
                            int port_id = -1;
                            int port_width = 1;
                            if (top_scope) {
                                const JZSymbol *psym = module_scope_lookup_kind(top_scope,
                                                                                b->name,
                                                                                JZ_SYM_PORT);
                                if (psym) {
                                    port_id = psym->id;
                                    /* Prefer IR signal width when available. */
                                    if (top_mod) {
                                        for (int si2 = 0; si2 < top_mod->num_signals; ++si2) {
                                            const IR_Signal *sig = &top_mod->signals[si2];
                                            if (sig->id == port_id) {
                                                if (sig->width > 0) {
                                                    port_width = sig->width;
                                                }
                                                break;
                                            }
                                        }
                                    }
                                }
                            }

                            /* Determine target pin name for this binding.
                             * The parser stores the raw text in b->text;
                             * trim whitespace so that names like "clk_27mhz "
                             * still match the IN_PINS/OUT_PINS entries.
                             */
                            const char *target_raw = NULL;
                            if (b->text && b->text[0] != '\0') {
                                target_raw = b->text;
                            } else {
                                target_raw = b->name;
                            }

                            char target_buf[256];
                            ir_trim_copy(target_raw ? target_raw : "", target_buf, sizeof(target_buf));
                            const char *target_expr = target_buf;

                            /* Support simple active-low bindings of the form
                             *   IN [1] rst_n = ~btns[0];
                             * by stripping a leading '~' and recording the
                             * inversion in IR_TopBinding. More complex
                             * expressions are not interpreted here.
                             */
                            int invert = 0;
                            if (target_expr[0] == '~') {
                                const char *p = target_expr + 1;
                                while (*p && isspace((unsigned char)*p)) {
                                    ++p;
                                }
                                ir_trim_copy(p, target_buf, sizeof(target_buf));
                                target_expr = target_buf;
                                invert = 1;
                            }

                            /* Handle simple explicit no-connect. */
                            if (target_expr[0] == '_' && target_expr[1] == '\0') {
                                /* Explicit no-connect: single binding with
                                 * top_bit_index/pin_bit_index set to -1.
                                 */
                                if (bi < max_bindings) {
                                    IR_TopBinding *tb = &bindings[bi++];
                                    tb->top_port_signal_id = port_id;
                                    tb->top_bit_index = -1;
                                    tb->pin_id = -1;
                                    tb->pin_bit_index = -1;
                                }
                                continue;
                            }

                            /* Concatenation binding: support for patterns
                             * like:
                             *   IN  [8] btns = { btns[1], 7'b0 };
                             *   OUT [8] leds = { leds, 2'b11 };
                             *   OUT [6] leds = { LED0_DONE, LED };
                             * Each element may be a pin reference (scalar or
                             * bus) or a sized binary literal.  Elements are
                             * assigned to top-port bits from MSB downwards in
                             * left-to-right order.
                             *
                             * A leading '~' is not interpreted for
                             * concatenations; such expressions are currently
                             * ignored for inversion semantics.
                             */
                            if (target_expr[0] == '{') {
                                const char *lbrace = strchr(target_expr, '{');
                                const char *rbrace = strrchr(target_expr, '}');
                                if (lbrace && rbrace && rbrace > lbrace) {
                                    ++lbrace;
                                    size_t inner_len = (size_t)(rbrace - lbrace);
                                    char inner[256];
                                    if (inner_len >= sizeof(inner)) inner_len = sizeof(inner) - 1u;
                                    memcpy(inner, lbrace, inner_len);
                                    inner[inner_len] = '\0';

                                    /* Walk comma-separated elements from left
                                     * to right, assigning top-port bits from
                                     * MSB downward.
                                     */
                                    int top_bit_cursor = port_width - 1;
                                    char *saveptr = NULL;
                                    char *elem = NULL;
                                    char inner_copy[256];
                                    memcpy(inner_copy, inner, inner_len + 1);

                                    for (elem = strtok_r(inner_copy, ",", &saveptr);
                                         elem != NULL && top_bit_cursor >= 0 && bi < max_bindings;
                                         elem = strtok_r(NULL, ",", &saveptr)) {
                                        char elem_trim[128];
                                        ir_trim_copy(elem, elem_trim, sizeof(elem_trim));

                                        /* Check if this element is a sized
                                         * binary literal (e.g. 7'b0, 2'b11).
                                         */
                                        char *quote = strchr(elem_trim, '\'');
                                        if (quote && quote[1] != '\0' &&
                                            (quote[1] == 'b' || quote[1] == 'B') &&
                                            quote[2] != '\0') {
                                            /* Parse literal width from prefix. */
                                            int lit_width = 0;
                                            for (const char *wp = elem_trim; wp < quote; ++wp) {
                                                if (*wp >= '0' && *wp <= '9')
                                                    lit_width = lit_width * 10 + (*wp - '0');
                                            }
                                            if (lit_width <= 0) lit_width = 1;
                                            char *digits = quote + 2;
                                            size_t dlen = strlen(digits);
                                            /* Record const-1 bindings for '1'
                                             * digits; '0' digits are left
                                             * unbound (emitted as 1'b0).
                                             */
                                            for (int ci = 0;
                                                 ci < lit_width && top_bit_cursor >= 0 && bi < max_bindings;
                                                 ++ci) {
                                                int digit_idx = (int)dlen - 1 - ci;
                                                char ch = (digit_idx >= 0) ? digits[digit_idx] : '0';
                                                if (ch == '1') {
                                                    IR_TopBinding *tb = &bindings[bi++];
                                                    tb->top_port_signal_id = port_id;
                                                    tb->top_bit_index = top_bit_cursor;
                                                    tb->pin_id = -1;
                                                    tb->pin_bit_index = -1;
                                                    tb->const_value = 1;
                                                }
                                                top_bit_cursor--;
                                            }
                                            continue;
                                        }

                                        /* Otherwise treat as a pin reference. */
                                        char pin_name[128];
                                        int pin_bit_index = -1;
                                        if (!ir_parse_map_lhs(elem_trim, pin_name, sizeof(pin_name), &pin_bit_index) ||
                                            pin_name[0] == '\0') {
                                            continue;
                                        }
                                        int pin_id = -1;
                                        if (proj->pins && proj->num_pins > 0) {
                                            for (int pi2 = 0; pi2 < proj->num_pins; ++pi2) {
                                                if (proj->pins[pi2].name &&
                                                    strcmp(proj->pins[pi2].name, pin_name) == 0) {
                                                    pin_id = pi2;
                                                    break;
                                                }
                                            }
                                        }
                                        if (pin_id < 0) continue;

                                        if (pin_bit_index >= 0) {
                                            /* Single-bit pin reference. */
                                            if (bi < max_bindings && top_bit_cursor >= 0) {
                                                IR_TopBinding *tb = &bindings[bi++];
                                                tb->top_port_signal_id = port_id;
                                                tb->top_bit_index = top_bit_cursor;
                                                tb->pin_id = pin_id;
                                                tb->pin_bit_index = pin_bit_index;
                                                tb->const_value = 0;
                                                top_bit_cursor--;
                                            }
                                        } else {
                                            /* Bus pin: expand MSB-first. */
                                            int pin_width = proj->pins[pin_id].width > 0
                                                          ? proj->pins[pin_id].width
                                                          : 1;
                                            for (int bidx = 0;
                                                 bidx < pin_width && top_bit_cursor >= 0 && bi < max_bindings;
                                                 ++bidx) {
                                                IR_TopBinding *tb = &bindings[bi++];
                                                tb->top_port_signal_id = port_id;
                                                tb->top_bit_index = top_bit_cursor;
                                                tb->pin_id = pin_id;
                                                tb->pin_bit_index = (pin_width - 1) - bidx;
                                                tb->const_value = 0;
                                                top_bit_cursor--;
                                            }
                                        }
                                    }
                                }

                                continue;
                            }

                            /* Try to parse as a slice expression first,
                             * e.g. "LED[4:0]".  If successful, expand into
                             * per-bit bindings and continue to the next port.
                             */
                            {
                                char slice_pin_name[128];
                                int slice_msb = -1, slice_lsb = -1;
                                if (ir_parse_pin_slice(target_expr, slice_pin_name,
                                                       sizeof(slice_pin_name),
                                                       &slice_msb, &slice_lsb) &&
                                    slice_pin_name[0] != '\0') {
                                    int slice_pin_id = -1;
                                    if (proj->pins && proj->num_pins > 0) {
                                        for (int pi = 0; pi < proj->num_pins; ++pi) {
                                            if (proj->pins[pi].name &&
                                                strcmp(proj->pins[pi].name, slice_pin_name) == 0) {
                                                slice_pin_id = pi;
                                                break;
                                            }
                                        }
                                    }
                                    if (slice_pin_id >= 0) {
                                        /* Expand slice into per-bit bindings.
                                         * top port bit 0 -> pin bit lsb,
                                         * top port bit 1 -> pin bit lsb+1, etc.
                                         */
                                        int top_bit = 0;
                                        for (int pbit = slice_lsb;
                                             pbit <= slice_msb && bi < max_bindings;
                                             ++pbit, ++top_bit) {
                                            IR_TopBinding *tb = &bindings[bi++];
                                            tb->top_port_signal_id = port_id;
                                            tb->top_bit_index = top_bit;
                                            tb->pin_id = slice_pin_id;
                                            tb->pin_bit_index = pbit;
                                            tb->inverted = invert;
                                        }
                                        continue;
                                    }
                                }
                            }

                            /* Parse the target expression as a pin name with
                             * optional bit index, e.g. "btns" or "btns[0]".
                             * This reuses the MAP LHS parser so that slices
                             * bind cleanly to individual pin bits.
                             */
                            char pin_name[128];
                            int pin_bit_index = -1;
                            if (!ir_parse_map_lhs(target_expr, pin_name, sizeof(pin_name), &pin_bit_index) ||
                                pin_name[0] == '\0') {
                                /* Fallback: treat the entire expression as a
                                 * bare pin name with no explicit bit index.
                                 */
                                ir_trim_copy(target_expr, pin_name, sizeof(pin_name));
                                pin_bit_index = -1;
                            }

                            /* Resolve target pin id, if any. */
                            int pin_id = -1;
                            if (proj->pins && proj->num_pins > 0 && pin_name[0] != '\0') {
                                for (int pi = 0; pi < proj->num_pins; ++pi) {
                                    if (proj->pins[pi].name &&
                                        strcmp(proj->pins[pi].name, pin_name) == 0) {
                                        pin_id = pi;
                                        break;
                                    }
                                }
                            }

                            if (pin_id < 0) {
                                /* Check if target is a literal constant
                                 * (e.g., "1'b1", "1'b0").  If so, record a
                                 * const binding with the appropriate value.
                                 */
                                {
                                    const char *tick = strchr(target_expr, '\'');
                                    if (tick && tick[1] != '\0' &&
                                        (tick[1] == 'b' || tick[1] == 'B') &&
                                        tick[2] != '\0') {
                                        char *digits = (char *)(tick + 2);
                                        size_t dlen = strlen(digits);
                                        int lit_width = 0;
                                        for (const char *wp = target_expr; wp < tick; ++wp) {
                                            if (*wp >= '0' && *wp <= '9')
                                                lit_width = lit_width * 10 + (*wp - '0');
                                        }
                                        if (lit_width <= 0) lit_width = 1;
                                        for (int ci = 0;
                                             ci < lit_width && ci < port_width && bi < max_bindings;
                                             ++ci) {
                                            int digit_idx = (int)dlen - 1 - ci;
                                            char ch = (digit_idx >= 0) ? digits[digit_idx] : '0';
                                            if (bi < max_bindings) {
                                                IR_TopBinding *tb = &bindings[bi++];
                                                tb->top_port_signal_id = port_id;
                                                tb->top_bit_index = ci;
                                                tb->pin_id = -1;
                                                tb->pin_bit_index = -1;
                                                tb->const_value = (ch == '1') ? 1 : 0;
                                            }
                                        }
                                        continue;
                                    }
                                }

                                /* Check if the target is a clock gen output
                                 * (e.g., a PLL-generated clock like CLK_A).
                                 */
                                const char *resolved_clock = NULL;
                                for (int cg = 0; cg < proj->num_clock_gens && !resolved_clock; ++cg) {
                                    const IR_ClockGen *clock_gen = &proj->clock_gens[cg];
                                    for (int cu = 0; cu < clock_gen->num_units && !resolved_clock; ++cu) {
                                        const IR_ClockGenUnit *unit = &clock_gen->units[cu];
                                        for (int co = 0; co < unit->num_outputs; ++co) {
                                            const IR_ClockGenOutput *out_clk = &unit->outputs[co];
                                            if (out_clk->clock_name &&
                                                strcmp(out_clk->clock_name, pin_name) == 0) {
                                                resolved_clock = out_clk->clock_name;
                                                break;
                                            }
                                        }
                                    }
                                }

                                /* Record a binding so that the IR reflects the
                                 * project-level connection intent.
                                 */
                                if (bi < max_bindings) {
                                    IR_TopBinding *tb = &bindings[bi++];
                                    tb->top_port_signal_id = port_id;
                                    tb->top_bit_index = -1;
                                    tb->pin_id = -1;
                                    tb->pin_bit_index = -1;
                                    tb->clock_name = resolved_clock
                                        ? ir_strdup_arena(arena, resolved_clock)
                                        : NULL;
                                }
                                continue;
                            }

                            /* If the target expression specified an explicit
                             * pin bit (e.g. "btns[0]"), bind that single bit
                             * directly to the (possibly scalar) top port.
                             * Board/package mapping is handled later via
                             * IR_PinMapping when emitting comments.
                             */
                            if (pin_bit_index >= 0) {
                                if (bi < max_bindings) {
                                    IR_TopBinding *tb = &bindings[bi++];
                                    tb->top_port_signal_id = port_id;
                                    tb->top_bit_index = -1; /* scalar top-port bit */
                                    tb->pin_id = pin_id;
                                    tb->pin_bit_index = pin_bit_index;
                                    tb->inverted = invert;
                                }
                                continue;
                            }

                            /* Emit per-bit bindings based on IR_PinMapping
                             * entries for this pin. When no mappings are
                             * present, fall back to a single whole-bus
                             * binding.
                             */
                            int any_mapping = 0;
                            if (proj->mappings && proj->num_mappings > 0) {
                                const char *logical_pin_name = proj->pins[pin_id].name;
                                for (int mi = 0; mi < proj->num_mappings && bi < max_bindings; ++mi) {
                                    const IR_PinMapping *pm = &proj->mappings[mi];
                                    if (!pm->logical_pin_name || !logical_pin_name ||
                                        strcmp(pm->logical_pin_name, logical_pin_name) != 0) {
                                        continue;
                                    }
                                    IR_TopBinding *tb = &bindings[bi++];
                                    tb->top_port_signal_id = port_id;
                                    tb->inverted = invert;
                                    if (pm->bit_index >= 0) {
                                        tb->top_bit_index = pm->bit_index;
                                        tb->pin_id = pin_id;
                                        tb->pin_bit_index = pm->bit_index;
                                    } else {
                                        /* Scalar pin mapping. */
                                        tb->top_bit_index = -1;
                                        tb->pin_id = pin_id;
                                        tb->pin_bit_index = -1;
                                    }
                                    any_mapping = 1;
                                }
                            }

                            if (!any_mapping && bi < max_bindings) {
                                /* No explicit MAP entries: represent as a
                                 * single whole-signal binding.
                                 */
                                IR_TopBinding *tb = &bindings[bi++];
                                tb->top_port_signal_id = port_id;
                                tb->top_bit_index = -1;
                                tb->pin_id = pin_id;
                                tb->pin_bit_index = -1;
                                tb->inverted = invert;
                            }
                        }

                        proj->top_bindings = bindings;
                        proj->num_top_bindings = bi;
                    }
                }
            }
        }

        design->project = proj;

        /* Mark latch signals that directly drive output pins as IOB. */
        if (proj->top_module_id >= 0 && proj->top_module_id < design->num_modules) {
            IR_Module *top_mod = &design->modules[proj->top_module_id];
            ir_mark_iob_latches(top_mod, proj);
        }
    }

    /* Run memory-port binding while module scopes and project symbols are
     * still available so IR_MemoryPort fields can be populated.
     */
    if (jz_ir_bind_memory_ports(design,
                                &module_scopes,
                                &project_symbols,
                                diagnostics) != 0) {
        goto ir_fail;
    }

    /* Lower memory writes into clock domain statement trees.
     * This must run after memory port binding is complete.
     */
    if (ir_lower_memory_writes(design, arena) != 0) {
        goto ir_fail;
    }

    /* Build CDC library modules and lower CDC entries into instances.
     * This must run after all modules, specializations, instances, and
     * memory bindings are complete.
     */
    if (ir_build_library_modules(design, arena) != 0) {
        goto ir_fail;
    }

    /* Cleanup temporary symbol tables and transient filename buffer. */
    for (size_t i = 0; i < scope_count; ++i) {
        jz_buf_free(&scopes[i].symbols);
    }
    jz_buf_free(&module_scopes);
    jz_buf_free(&project_symbols);
    jz_buf_free(&file_names);

    /* Eliminate modules not reachable from @top. */
    jz_ir_eliminate_dead_modules(design);

    *out_design = design;
    return 0;

ir_fail:
    {
        /* Free any temporary specialization metadata. The spec_name strings
         * themselves live in the IR arena and must not be freed here.
         */
        if (specs) {
            for (int i = 0; i < spec_count; ++i) {
                free(specs[i].overrides);
            }
            free(specs);
        }

        size_t sc = module_scopes.len / sizeof(JZModuleScope);
        JZModuleScope *scopes2 = (JZModuleScope *)module_scopes.data;
        for (size_t i = 0; i < sc; ++i) {
            jz_buf_free(&scopes2[i].symbols);
        }
        jz_buf_free(&module_scopes);
        jz_buf_free(&project_symbols);
    }
    if (diagnostics && root->loc.filename) {
        JZLocation loc = root->loc;
        jz_diagnostic_report(diagnostics,
                             loc,
                             JZ_SEVERITY_ERROR,
                             "IR_INTERNAL",
                             "failed to construct IR design");
    }
    return -1;
}

/**
 * @brief Look up a module or blackbox symbol by name.
 *
 * Mirrors project-level semantic lookup logic, restricted to modules and
 * blackboxes for IR construction.
 *
 * @param project_symbols Project symbol table.
 * @param name            Module or blackbox name.
 * @return Matching symbol, or NULL if not found.
 */
const JZSymbol *ir_project_lookup_module_or_blackbox(const JZBuffer *project_symbols,
                                                            const char *name)
{
    if (!project_symbols || !project_symbols->data || !name) {
        return NULL;
    }
    const JZSymbol *syms = (const JZSymbol *)project_symbols->data;
    size_t count = project_symbols->len / sizeof(JZSymbol);
    for (size_t i = 0; i < count; ++i) {
        if (!syms[i].name || strcmp(syms[i].name, name) != 0) {
            continue;
        }
        if (syms[i].kind == JZ_SYM_MODULE || syms[i].kind == JZ_SYM_BLACKBOX) {
            return &syms[i];
        }
    }
    return NULL;
}

/**
 * @brief Find the IR module index corresponding to an AST module node.
 *
 * Relies on the invariant that IR modules are created in the same order
 * as JZModuleScope entries.
 *
 * @param scopes      Module scope array.
 * @param scope_count Number of scopes.
 * @param node        AST module or blackbox node.
 * @return IR module index, or -1 if not found.
 */
int ir_find_module_index_for_node(const JZModuleScope *scopes,
                                         size_t scope_count,
                                         const JZASTNode *node)
{
    if (!scopes || !node) {
        return -1;
    }
    for (size_t i = 0; i < scope_count; ++i) {
        if (scopes[i].node == node) {
            return (int)i;
        }
    }
    return -1;
}

/**
 * @brief Duplicate a string into the IR arena.
 *
 * @param arena IR allocation arena.
 * @param s     Source string.
 * @return Arena-allocated copy of the string, or NULL on failure.
 */
char *ir_strdup_arena(JZArena *arena, const char *s)
{
    if (!arena || !s) {
        return NULL;
    }
    size_t len = strlen(s);
    char *dst = (char *)jz_arena_alloc(arena, len + 1);
    if (!dst) {
        return NULL;
    }
    memcpy(dst, s, len + 1);
    return dst;
}

/* -------------------------------------------------------------------------
 * Dead module elimination
 * -------------------------------------------------------------------------
 */

void jz_ir_eliminate_dead_modules(IR_Design *design)
{
    if (!design || design->num_modules <= 0) {
        return;
    }

    const int n = design->num_modules;

    int top = -1;
    if (design->project) {
        top = design->project->top_module_id;
        if (top < 0 || top >= n) {
            top = -1;
        }
    }

    if (top < 0) {
        /* No top module — keep everything. */
        return;
    }

    /* BFS from top module through instance edges. */
    int *reachable = (int *)calloc((size_t)n, sizeof(int));
    int *queue     = (int *)calloc((size_t)n, sizeof(int));
    if (!reachable || !queue) {
        free(reachable);
        free(queue);
        return;
    }

    int head = 0, tail = 0;
    reachable[top] = 1;
    queue[tail++] = top;

    while (head < tail) {
        int mid = queue[head++];
        const IR_Module *mod = &design->modules[mid];
        for (int j = 0; j < mod->num_instances; ++j) {
            int cid = mod->instances[j].child_module_id;
            if (cid >= 0 && cid < n && !reachable[cid]) {
                reachable[cid] = 1;
                queue[tail++] = cid;
            }
        }
    }

    /* Mark unreachable modules as eliminated. */
    for (int i = 0; i < n; ++i) {
        if (!reachable[i]) {
            design->modules[i].eliminated = true;
        }
    }

    free(reachable);
    free(queue);
}
