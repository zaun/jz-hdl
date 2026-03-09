#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <time.h>

#include "sem_driver.h"
#include "sem.h"
#include "../../sem/driver_internal.h"
#include "util.h"
#include "rules.h"

/* -------------------------------------------------------------------------
 *  Alias-report global state
 * -------------------------------------------------------------------------
 */

static int g_alias_report_enabled = 0;
static FILE *g_alias_report_out = NULL;
static char g_alias_report_generated[64];
static const char *g_alias_report_version = NULL;
static const char *g_alias_report_input = NULL;

/* Cross-module summary accumulator for finalize. */
typedef struct {
    char   module_name[128];
    unsigned net_count;
    unsigned clock_domains;
    unsigned register_count;
    unsigned tri_state_nets;
    unsigned alias_stmts;
} JZAliasSummaryEntry;

static JZBuffer g_alias_summary = {0}; /* Array of JZAliasSummaryEntry */

void jz_sem_enable_alias_report(FILE *out,
                                const char *tool_version,
                                const char *input_filename)
{
    g_alias_report_enabled = (out != NULL);
    g_alias_report_out = out;
    g_alias_report_version = tool_version;
    g_alias_report_input = input_filename;
    jz_buf_free(&g_alias_summary);
    memset(&g_alias_summary, 0, sizeof(g_alias_summary));

    time_t now = time(NULL);
    struct tm tm_info;
    if (localtime_r(&now, &tm_info) != NULL) {
        if (strftime(g_alias_report_generated,
                     sizeof(g_alias_report_generated),
                     "%Y-%m-%d %H:%M %Z",
                     &tm_info) == 0) {
            snprintf(g_alias_report_generated,
                     sizeof(g_alias_report_generated),
                     "<unknown>");
        }
    } else {
        snprintf(g_alias_report_generated,
                 sizeof(g_alias_report_generated),
                 "<unknown>");
    }
}

/* -------------------------------------------------------------------------
 *  Small helpers reused across alias-report implementation
 * -------------------------------------------------------------------------
 */

/* Resolve declaration width using the full CONFIG/CONST evaluator when
 * possible, falling back to simple integer parsing.
 */
static unsigned sem_decl_width_resolved(JZASTNode *decl,
                                        const JZModuleScope *scope,
                                        const JZBuffer *project_symbols)
{
    if (!decl || !decl->width) {
        return 0;
    }
    /* Try the full evaluator first (handles CONFIG, CONST, clog2, etc.). */
    if (scope && project_symbols) {
        unsigned w = 0;
        if (sem_eval_width_expr(decl->width, scope, project_symbols, &w) == 0 && w > 0) {
            return w;
        }
    }
    /* Fall back to simple integer parsing. */
    unsigned w = 0;
    int rc = eval_simple_positive_decl_int(decl->width, &w);
    return (rc == 1) ? w : 0;
}

static int sem_decl_is_inout_port(const JZASTNode *decl)
{
    if (!decl) return 0;
    if (decl->type != JZ_AST_PORT_DECL || !decl->block_kind) return 0;
    return strcmp(decl->block_kind, "INOUT") == 0;
}

/* Check whether an expression subtree references a given identifier name. */
static int sem_expr_references_name(const JZASTNode *expr, const char *name)
{
    if (!expr || !name) return 0;
    if ((expr->type == JZ_AST_EXPR_IDENTIFIER ||
         expr->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER) &&
        expr->name && strcmp(expr->name, name) == 0) {
        return 1;
    }
    for (size_t i = 0; i < expr->child_count; ++i) {
        if (sem_expr_references_name(expr->children[i], name)) return 1;
    }
    return 0;
}

/* Check if a net has any driver that can produce Z (bus port, Z-capable
 * symbol, or wire connected to instances via BUS bindings).
 */
static int sem_net_has_z_drivers(const JZNet *net,
                                 const JZModuleScope *scope)
{
    if (!net || !scope) return 0;

    /* Check atoms: INOUT port declarations are inherently tri-state. */
    JZASTNode **atoms = (JZASTNode **)net->atoms.data;
    size_t atom_count = net->atoms.len / sizeof(JZASTNode *);
    for (size_t ai = 0; ai < atom_count; ++ai) {
        JZASTNode *decl = atoms[ai];
        if (!decl) continue;
        if (sem_decl_is_inout_port(decl)) return 1;
        /* Check the symbol table for can_be_z flag. */
        if (decl->name) {
            const JZSymbol *sym = module_scope_lookup(scope, decl->name);
            if (sym && sym->can_be_z) return 1;
        }
    }

    /* Also check: is it a bus port? (type with no explicit INOUT but
     * used as bus). Bus ports without explicit direction are tri-state. */
    if (jz_tristate_net_is_bus_port(net)) return 1;

    /* Check if any driver is a MODULE_INSTANCE with a BUS binding whose
     * RHS references one of the net's atom names. A wire connected to
     * instances via BUS bindings carries tri-state signals.
     */
    if (net->driver_stmts.len > 0) {
        JZASTNode **drv = (JZASTNode **)net->driver_stmts.data;
        size_t drv_count = net->driver_stmts.len / sizeof(JZASTNode *);
        for (size_t di = 0; di < drv_count; ++di) {
            JZASTNode *stmt = drv[di];
            if (!stmt || stmt->type != JZ_AST_MODULE_INSTANCE) continue;

            for (size_t bi = 0; bi < stmt->child_count; ++bi) {
                JZASTNode *bind = stmt->children[bi];
                if (!bind || bind->type != JZ_AST_PORT_DECL || !bind->block_kind) continue;
                if (strcmp(bind->block_kind, "BUS") != 0) continue;
                if (bind->child_count == 0) continue;

                /* The BUS binding's RHS is the wire expression. Check if it
                 * references any atom name on this net.
                 */
                JZASTNode *rhs = bind->children[0];
                for (size_t ai = 0; ai < atom_count; ++ai) {
                    JZASTNode *decl = atoms[ai];
                    if (!decl || !decl->name) continue;
                    if (sem_expr_references_name(rhs, decl->name)) {
                        return 1;
                    }
                }
            }
        }
    }

    return 0;
}

static void sem_count_module_alias_stats(const JZASTNode *node,
                                         unsigned *out_user_identifiers,
                                         unsigned *out_internal_identifiers,
                                         unsigned *out_alias_stmts)
{
    if (!node) return;

    if (node->name) {
        int is_user = 0;
        switch (node->type) {
        case JZ_AST_PORT_DECL:
        case JZ_AST_WIRE_DECL:
        case JZ_AST_REGISTER_DECL:
        case JZ_AST_MEM_DECL:
        case JZ_AST_MUX_DECL:
        case JZ_AST_MODULE:
            is_user = 1;
            break;
        default:
            break;
        }
        if (is_user) {
            if (out_user_identifiers) (*out_user_identifiers)++;
        } else {
            if (out_internal_identifiers) (*out_internal_identifiers)++;
        }
    }

    if (node->type == JZ_AST_STMT_ASSIGN && node->block_kind) {
        const char *op = node->block_kind;
        if (strcmp(op, "ALIAS") == 0 ||
            strcmp(op, "ALIAS_Z") == 0 ||
            strcmp(op, "ALIAS_S") == 0) {
            if (out_alias_stmts) (*out_alias_stmts)++;
        }
    }

    for (size_t i = 0; i < node->child_count; ++i) {
        sem_count_module_alias_stats(node->children[i],
                                     out_user_identifiers,
                                     out_internal_identifiers,
                                     out_alias_stmts);
    }
}

/* Return non-zero if this module has any SYNCHRONOUS block that uses the
 * given identifier name as its CLK parameter. Collects all matching block
 * line numbers into out_lines (a JZBuffer of int). Optionally returns the
 * first line number via out_first_line.
 */
static int sem_module_uses_clock(const JZModuleScope *scope,
                                 const char *clk_name,
                                 int *out_first_line)
{
    if (!scope || !scope->node || !clk_name) return 0;
    JZASTNode *mod = scope->node;
    int found = 0;
    int first_line = 0;

    for (size_t i = 0; i < mod->child_count; ++i) {
        JZASTNode *blk = mod->children[i];
        if (!blk || blk->type != JZ_AST_BLOCK || !blk->block_kind) continue;
        if (strcmp(blk->block_kind, "SYNCHRONOUS") != 0) continue;

        for (size_t j = 0; j < blk->child_count; ++j) {
            JZASTNode *p = blk->children[j];
            if (!p || p->type != JZ_AST_SYNC_PARAM || !p->name) continue;
            if (strcmp(p->name, "CLK") != 0) continue;
            if (p->child_count == 0) continue;
            JZASTNode *expr = p->children[0];
            if (!expr) continue;

            const char *id = NULL;
            if ((expr->type == JZ_AST_EXPR_IDENTIFIER ||
                 expr->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER) &&
                expr->name) {
                id = expr->name;
            }
            if (!id) continue;

            if (strcmp(id, clk_name) == 0) {
                if (!found) {
                    found = 1;
                    first_line = blk->loc.line;
                }
            }
        }
    }

    if (found && out_first_line) {
        *out_first_line = first_line;
    }
    return found;
}

/* Collect all SYNCHRONOUS block line numbers that use the given clock name.
 * Returns the number of matching blocks found.
 */
static size_t sem_module_collect_clock_consumers(const JZModuleScope *scope,
                                                  const char *clk_name,
                                                  JZBuffer *out_lines)
{
    if (!scope || !scope->node || !clk_name || !out_lines) return 0;
    JZASTNode *mod = scope->node;
    size_t count = 0;

    for (size_t i = 0; i < mod->child_count; ++i) {
        JZASTNode *blk = mod->children[i];
        if (!blk || blk->type != JZ_AST_BLOCK || !blk->block_kind) continue;
        if (strcmp(blk->block_kind, "SYNCHRONOUS") != 0) continue;

        for (size_t j = 0; j < blk->child_count; ++j) {
            JZASTNode *p = blk->children[j];
            if (!p || p->type != JZ_AST_SYNC_PARAM || !p->name) continue;
            if (strcmp(p->name, "CLK") != 0) continue;
            if (p->child_count == 0) continue;
            JZASTNode *expr = p->children[0];
            if (!expr) continue;

            const char *id = NULL;
            if ((expr->type == JZ_AST_EXPR_IDENTIFIER ||
                 expr->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER) &&
                expr->name) {
                id = expr->name;
            }
            if (!id) continue;

            if (strcmp(id, clk_name) == 0) {
                int line = blk->loc.line;
                (void)jz_buf_append(out_lines, &line, sizeof(line));
                count++;
                break; /* one match per block is enough */
            }
        }
    }

    return count;
}

/* Lightweight project-symbol lookup used by alias reporting to attach
 * CLOCKS/IN_PINS/MAP context to nets without depending on driver_project.c
 * internals.
 */
static const JZSymbol *sem_alias_project_lookup(const JZBuffer *project_symbols,
                                                const char *name,
                                                JZSymbolKind kind)
{
    if (!project_symbols || !name) return NULL;
    size_t count = project_symbols->len / sizeof(JZSymbol);
    const JZSymbol *syms = (const JZSymbol *)project_symbols->data;
    for (size_t i = 0; i < count; ++i) {
        const JZSymbol *s = &syms[i];
        if (!s->name || s->kind != kind) continue;
        if (strcmp(s->name, name) == 0) {
            return s;
        }
    }
    return NULL;
}

/* Simple helper to trim leading/trailing whitespace from a small attribute
 * string into a fixed-size buffer.
 */
static void sem_alias_trim_copy(const char *src, char *dst, size_t dst_size)
{
    if (!dst || dst_size == 0) return;
    dst[0] = '\0';
    if (!src) return;

    const char *start = src;
    while (*start && isspace((unsigned char)*start)) {
        ++start;
    }
    const char *end = start + strlen(start);
    while (end > start && isspace((unsigned char)end[-1])) {
        --end;
    }

    size_t len = (size_t)(end - start);
    if (len >= dst_size) len = dst_size - 1;
    memcpy(dst, start, len);
    dst[len] = '\0';
}

/* Parse a CLOCKS attribute string of the form "period=<number> ... edge=..."
 * and extract the numeric period (if present). This mirrors the logic used in
 * sem_check_project_clocks for diagnostics but is kept local here so that the
 * alias report can surface the same information without depending on that
 * TU's static helpers.
 */
static int sem_alias_parse_clock_period(const char *attrs, double *out_period)
{
    if (!out_period) return 0;
    *out_period = 0.0;
    if (!attrs) return 0;

    const char *p = strstr(attrs, "period");
    if (!p) return 0;
    p = strchr(p, '=');
    if (!p) return 0;
    ++p;
    while (*p && isspace((unsigned char)*p)) ++p;

    char *endptr = NULL;
    double val = strtod(p, &endptr);
    if (endptr == p) return 0;
    *out_period = val;
    return 1;
}

/* Locate the enclosing ASYNCHRONOUS/SYNCHRONOUS block for a given statement
 * so that alias-reporting can distinguish combinational vs. clocked drivers
 * without changing the underlying net graph representation.
 */
static int sem_alias_find_enclosing_block_rec(const JZASTNode *node,
                                              const JZASTNode *target,
                                              const JZASTNode *current_block,
                                              const char *current_kind,
                                              const JZASTNode **out_block,
                                              const char **out_kind)
{
    if (!node || !target) return 0;

    const JZASTNode *next_block = current_block;
    const char *next_kind = current_kind;

    if (node->type == JZ_AST_BLOCK && node->block_kind) {
        next_block = node;
        next_kind = node->block_kind;
    }

    if (node == target) {
        if (out_block) *out_block = next_block;
        if (out_kind) *out_kind = next_kind;
        return 1;
    }

    for (size_t i = 0; i < node->child_count; ++i) {
        if (sem_alias_find_enclosing_block_rec(node->children[i],
                                               target,
                                               next_block,
                                               next_kind,
                                               out_block,
                                               out_kind)) {
            return 1;
        }
    }

    return 0;
}

static void sem_alias_find_enclosing_block(const JZModuleScope *scope,
                                           const JZASTNode *stmt,
                                           const JZASTNode **out_block,
                                           const char **out_kind)
{
    if (!out_block || !out_kind) return;
    *out_block = NULL;
    *out_kind = NULL;
    if (!scope || !scope->node || !stmt) return;
    (void)sem_alias_find_enclosing_block_rec(scope->node,
                                             stmt,
                                             NULL,
                                             NULL,
                                             out_block,
                                             out_kind);
}

/* Return a human-readable string for the declaration kind of an AST node. */
static const char *sem_alias_decl_kind_str(const JZASTNode *decl)
{
    if (!decl) return "UNKNOWN";
    switch (decl->type) {
    case JZ_AST_PORT_DECL:
        if (decl->block_kind) {
            if (strcmp(decl->block_kind, "IN") == 0) return "PORT IN";
            if (strcmp(decl->block_kind, "OUT") == 0) return "PORT OUT";
            if (strcmp(decl->block_kind, "INOUT") == 0) return "PORT INOUT";
        }
        return "PORT";
    case JZ_AST_WIRE_DECL: return "WIRE";
    case JZ_AST_REGISTER_DECL: return "REGISTER";
    case JZ_AST_MEM_DECL: return "MEM";
    case JZ_AST_MUX_DECL: return "MUX";
    default: return "UNKNOWN";
    }
}

/* Extract the CLK identifier name from a SYNCHRONOUS block node. */
static const char *sem_alias_sync_block_clock_name(const JZASTNode *block)
{
    if (!block || block->type != JZ_AST_BLOCK || !block->block_kind ||
        strcmp(block->block_kind, "SYNCHRONOUS") != 0) {
        return NULL;
    }
    for (size_t j = 0; j < block->child_count; ++j) {
        JZASTNode *p = block->children[j];
        if (!p || p->type != JZ_AST_SYNC_PARAM || !p->name) continue;
        if (strcmp(p->name, "CLK") != 0) continue;
        if (p->child_count == 0) continue;
        JZASTNode *expr = p->children[0];
        if (!expr) continue;
        if ((expr->type == JZ_AST_EXPR_IDENTIFIER ||
             expr->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER) &&
            expr->name) {
            return expr->name;
        }
    }
    return NULL;
}

/* Return a malloc'd string containing the trimmed source line for a statement.
 * Caller must free() the result. Returns NULL on failure.
 */
static char *sem_alias_get_stmt_source_line(const JZASTNode *stmt)
{
    if (!stmt || !stmt->loc.filename || stmt->loc.line <= 0) return NULL;

    size_t size = 0;
    char *contents = jz_read_entire_file(stmt->loc.filename, &size);
    if (!contents || size == 0) {
        if (contents) free(contents);
        return NULL;
    }

    const char *p = contents;
    const char *line_start = contents;
    int current_line = 1;

    while (*p && current_line < stmt->loc.line) {
        if (*p == '\n') {
            ++current_line;
            line_start = p + 1;
        }
        ++p;
    }

    char *result = NULL;
    if (current_line == stmt->loc.line) {
        const char *line_end = line_start;
        while (*line_end && *line_end != '\n' && *line_end != '\r') {
            ++line_end;
        }
        while (line_start < line_end && isspace((unsigned char)*line_start)) {
            ++line_start;
        }
        while (line_end > line_start && isspace((unsigned char)line_end[-1])) {
            --line_end;
        }
        size_t len = (size_t)(line_end - line_start);
        if (len > 0) {
            result = malloc(len + 1);
            if (result) {
                memcpy(result, line_start, len);
                result[len] = '\0';
            }
        }
    }

    free(contents);
    return result;
}

/* Print a statement location and source line in the format:
 *     <file>@<line>: <source>
 */
static void sem_alias_print_loc_source(FILE *out, const JZASTNode *stmt)
{
    if (!out || !stmt) return;
    char *src = sem_alias_get_stmt_source_line(stmt);
    if (stmt->loc.filename) {
        fprintf(out, "    %s@%d: %s\n",
                stmt->loc.filename, stmt->loc.line,
                src ? src : "<source unavailable>");
    } else {
        fprintf(out, "    line %d: %s\n",
                stmt->loc.line,
                src ? src : "<source unavailable>");
    }
    free(src);
}

/* -------------------------------------------------------------------------
 *  Project @top bindings helpers
 * -------------------------------------------------------------------------
 */

typedef struct JZTopPortBinding {
    JZASTNode      *port_decl;    /* module PORT_DECL node */
    const char     *dir;          /* "IN" / "OUT" / "INOUT" */
    const char     *target_name;  /* project-level binding target (pin/clock name) */
    const JZSymbol *pin_sym;      /* IN_PINS/OUT_PINS/INOUT_PINS entry, if any */
    const JZSymbol *clk_sym;      /* CLOCKS entry, if any */
} JZTopPortBinding;

static void sem_alias_collect_top_bindings_for_module(JZASTNode *project_root,
                                                      const JZModuleScope *scope,
                                                      const JZBuffer *project_symbols,
                                                      JZBuffer *out_bindings)
{
    if (!out_bindings) return;
    memset(out_bindings, 0, sizeof(*out_bindings));
    if (!project_root || project_root->type != JZ_AST_PROJECT || !scope || !scope->node || !scope->name) {
        return;
    }

    /* Locate the single @top block in the project, if any. */
    JZASTNode *top_new = NULL;
    for (size_t i = 0; i < project_root->child_count; ++i) {
        JZASTNode *child = project_root->children[i];
        if (!child || child->type != JZ_AST_PROJECT_TOP_INSTANCE) continue;
        top_new = child;
        break;
    }
    if (!top_new || !top_new->name) {
        return;
    }

    /* Only attach bindings for the module that is actually selected as @top. */
    if (strcmp(top_new->name, scope->name) != 0) {
        return;
    }

    JZASTNode *top_mod = scope->node;

    JZASTNode **bindings = top_new->children;
    size_t binding_count = top_new->child_count;

    for (size_t k = 0; k < binding_count; ++k) {
        JZASTNode *b = bindings[k];
        if (!b || b->type != JZ_AST_PORT_DECL || !b->name) continue;

        const char *port_name = b->name;
        const char *target_name = NULL;
        if (b->text && b->text[0] != '\0') {
            target_name = b->text;
        } else {
            target_name = port_name;
        }

        /* Find the corresponding PORT_DECL inside the top module. */
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
        if (!mod_port) continue;

        JZTopPortBinding rec;
        memset(&rec, 0, sizeof(rec));
        rec.port_decl = mod_port;
        rec.dir = mod_port->block_kind;
        rec.target_name = target_name;
        if (project_symbols && target_name[0] != '\0') {
            rec.pin_sym = sem_alias_project_lookup(project_symbols, target_name, JZ_SYM_PIN);
            rec.clk_sym = sem_alias_project_lookup(project_symbols, target_name, JZ_SYM_CLOCK);
        }

        (void)jz_buf_append(out_bindings, &rec, sizeof(rec));
    }
}

static const JZTopPortBinding *sem_alias_lookup_top_binding_for_net(const JZBuffer *top_bindings,
                                                                    JZASTNode **atoms,
                                                                    size_t atom_count)
{
    if (!top_bindings || top_bindings->len == 0 || !atoms || atom_count == 0) return NULL;

    const JZTopPortBinding *arr = (const JZTopPortBinding *)top_bindings->data;
    size_t count = top_bindings->len / sizeof(JZTopPortBinding);

    for (size_t ai = 0; ai < atom_count; ++ai) {
        JZASTNode *decl = atoms[ai];
        if (!decl) continue;
        for (size_t i = 0; i < count; ++i) {
            const JZTopPortBinding *tb = &arr[i];
            if (tb->port_decl == decl) {
                return tb;
            }
        }
    }
    return NULL;
}

/* -------------------------------------------------------------------------
 *  Alias report emission
 * -------------------------------------------------------------------------
 */

/* JZAliasEdge / JZNet are defined in driver_internal.h and populated by the
 * flow pass (sem_build_net_graphs). We only consume them here.
 */

void sem_emit_alias_report_for_module(const JZModuleScope *scope,
                                      JZBuffer *nets,
                                      const JZBuffer *module_scopes,
                                      const JZBuffer *project_symbols,
                                      JZASTNode *project_root)
{
    (void)module_scopes; /* reserved for future cross-module analysis */
    if (!g_alias_report_enabled || !g_alias_report_out || !scope || !scope->node) {
        return;
    }

    FILE *out = g_alias_report_out;

    unsigned user_identifiers = 0;
    unsigned internal_identifiers = 0;
    unsigned total_alias_stmts = 0;
    sem_count_module_alias_stats(scope->node,
                                 &user_identifiers,
                                 &internal_identifiers,
                                 &total_alias_stmts);

    size_t raw_net_count = nets->len / sizeof(JZNet);
    unsigned tri_state_nets = 0;
    unsigned extended_width_aliases = 0;

    /* Collect project-level @top bindings, if any, for this module. */
    JZBuffer top_bindings = (JZBuffer){0};
    if (project_root && project_root->type == JZ_AST_PROJECT) {
        sem_alias_collect_top_bindings_for_module(project_root,
                                                  scope,
                                                  project_symbols,
                                                  &top_bindings);
    }

    /* Map each declaration name to its primary net index for the identifier index. */
    JZBuffer ident_index = (JZBuffer){0}; /* array of struct { const char *name; size_t net_ix; } */

    typedef struct JZIdentIndexEntry {
        const char *name;
        size_t      net_ix;
    } JZIdentIndexEntry;

    size_t net_count = 0; /* number of non-empty physical nets */

    for (size_t ni = 0; ni < raw_net_count; ++ni) {
        JZNet *net = &((JZNet *)nets->data)[ni];
        if (net->atoms.len == 0) continue;

        ++net_count;

        JZASTNode **atoms = (JZASTNode **)net->atoms.data;
        size_t atom_count = net->atoms.len / sizeof(JZASTNode *);

        if (sem_net_has_z_drivers(net, scope)) {
            tri_state_nets++;
        }

        for (size_t ai = 0; ai < atom_count; ++ai) {
            JZASTNode *decl = atoms[ai];
            if (!decl || !decl->name) continue;

            /* Track one (name, net_ix) pair per identifier for the index. */
            int seen = 0;
            JZIdentIndexEntry *arr = (JZIdentIndexEntry *)ident_index.data;
            size_t count = ident_index.len / sizeof(JZIdentIndexEntry);
            for (size_t k = 0; k < count; ++k) {
                if (arr[k].name == decl->name) {
                    seen = 1;
                    break;
                }
            }
            if (!seen) {
                JZIdentIndexEntry e;
                e.name  = decl->name;
                e.net_ix = ni;
                (void)jz_buf_append(&ident_index, &e, sizeof(e));
            }
        }

        JZAliasEdge *edges = (JZAliasEdge *)net->alias_edges.data;
        size_t edge_count = net->alias_edges.len / sizeof(JZAliasEdge);
        for (size_t ei = 0; ei < edge_count; ++ei) {
            JZAliasEdge *e = &edges[ei];
            unsigned lhs_w = sem_decl_width_resolved(e->lhs_decl, scope, project_symbols);
            unsigned rhs_w = sem_decl_width_resolved(e->rhs_decl, scope, project_symbols);
            if (lhs_w != 0 && rhs_w != 0 && lhs_w != rhs_w) {
                extended_width_aliases++;
            }
        }
    }

    if (g_alias_report_input) {
        fprintf(out, "Input file: %s\n", g_alias_report_input);
    }
    fprintf(out, "Module: %s\n", scope->name ? scope->name : "<anonymous>");
    fprintf(out, "\n");

    unsigned total_identifiers = user_identifiers + internal_identifiers;

    fprintf(out, "User-visible identifiers:  %u\n", user_identifiers);
    fprintf(out, "Internal elaboration nodes:%u\n", internal_identifiers);
    fprintf(out, "Total identifiers:        %u\n", total_identifiers);
    fprintf(out, "Total alias statements:   %u\n", total_alias_stmts);
    fprintf(out, "Resolved physical nets:   %zu\n", net_count);
    fprintf(out, "Tri-state nets:           %u\n", tri_state_nets);
    fprintf(out, "Extended-width aliases:   %u\n", extended_width_aliases);
    fprintf(out, "\n");

    /* Detailed per-net information. */
    for (size_t ni = 0; ni < raw_net_count; ++ni) {
        JZNet *net = &((JZNet *)nets->data)[ni];
        if (net->atoms.len == 0) continue;

        JZASTNode **atoms = (JZASTNode **)net->atoms.data;
        size_t atom_count = net->atoms.len / sizeof(JZASTNode *);
        unsigned width = 0;
        int tri = 0;
        int is_clock = 0;
        int clock_line = 0;
        for (size_t ai = 0; ai < atom_count; ++ai) {
            JZASTNode *decl = atoms[ai];
            if (!decl) continue;
            if (!width) {
                width = sem_decl_width_resolved(decl, scope, project_symbols);
            }
        }
        tri = sem_net_has_z_drivers(net, scope);

        /* Recognize simple clock nets: IN port used as CLK in a SYNCHRONOUS block. */
        const char *clk_name_for_net = NULL;
        for (size_t ai = 0; ai < atom_count && !is_clock; ++ai) {
            JZASTNode *decl = atoms[ai];
            if (!decl || decl->type != JZ_AST_PORT_DECL || !decl->block_kind ||
                !decl->name) {
                continue;
            }
            if (strcmp(decl->block_kind, "IN") != 0) continue;
            if (sem_module_uses_clock(scope, decl->name, &clock_line)) {
                is_clock = 1;
                clk_name_for_net = decl->name;
            }
        }

        /* Detect simple width-reducing slice aliases so that we can label
         * both the member list and the alias edges with explicit slice
         * ranges and, when widths are purely numeric, derive a per-bit
         * mapping description.
         */
        int have_slice_alias = 0;
        JZASTNode *slice_rhs_decl = NULL;
        unsigned slice_msb = 0;
        unsigned slice_lsb = 0;

        if (net->alias_edges.len >= sizeof(JZAliasEdge)) {
            JZAliasEdge *edges = (JZAliasEdge *)net->alias_edges.data;
            size_t edge_count = net->alias_edges.len / sizeof(JZAliasEdge);
            for (size_t ei = 0; ei < edge_count && !have_slice_alias; ++ei) {
                JZAliasEdge *e = &edges[ei];
                JZASTNode *stmt = e->stmt;
                if (!stmt || stmt->type != JZ_AST_STMT_ASSIGN || stmt->child_count < 2) {
                    continue;
                }
                const char *op = stmt->block_kind ? stmt->block_kind : "";
                int is_alias = (strcmp(op, "ALIAS") == 0 ||
                                strcmp(op, "ALIAS_Z") == 0 ||
                                strcmp(op, "ALIAS_S") == 0);
                if (!is_alias) continue;

                JZASTNode *lhs = stmt->children[0];
                JZASTNode *rhs = stmt->children[1];
                if (!lhs || !rhs) continue;

                if (lhs->type != JZ_AST_EXPR_IDENTIFIER) {
                    continue;
                }
                if (rhs->type != JZ_AST_EXPR_SLICE || rhs->child_count < 3) {
                    continue;
                }

                JZASTNode *base = rhs->children[0];
                JZASTNode *msb = rhs->children[1];
                JZASTNode *lsb = rhs->children[2];
                if (!base || !base->name ||
                    (base->type != JZ_AST_EXPR_IDENTIFIER &&
                     base->type != JZ_AST_EXPR_QUALIFIED_IDENTIFIER)) {
                    continue;
                }
                if (!msb || !lsb || msb->type != JZ_AST_EXPR_LITERAL ||
                    lsb->type != JZ_AST_EXPR_LITERAL ||
                    !msb->text || !lsb->text) {
                    continue;
                }

                unsigned msb_val = 0, lsb_val = 0;
                if (!parse_simple_nonnegative_int(msb->text, &msb_val) ||
                    !parse_simple_nonnegative_int(lsb->text, &lsb_val)) {
                    continue;
                }

                slice_rhs_decl = e->rhs_decl;
                slice_msb = msb_val;
                slice_lsb = lsb_val;
                have_slice_alias = 1;
            }
        }

        /* --- Net header: single line with width and tri-state --- */
        if (is_clock) {
            fprintf(out, "PHYSICAL NET: NET_%04zu    Width: %u    Kind: CLOCK\n", ni, width);
        } else {
            fprintf(out, "PHYSICAL NET: NET_%04zu    Width: %u    Tri-state: %s\n",
                    ni, width, tri ? "Yes" : "No");
        }
        fprintf(out, "------------------------------------------------------------\n");

        /* Declared as / Declared at */
        if (atom_count > 0 && atoms[0]) {
            JZASTNode *primary = atoms[0];
            fprintf(out, "Declared as: %s\n", sem_alias_decl_kind_str(primary));
            if (primary->loc.filename) {
                fprintf(out, "Declared at: %s@%d\n",
                        primary->loc.filename, primary->loc.line);
            }
        }
        fprintf(out, "\n");

        /* Members (Aliased Identifiers) - show alias edges as equalities */
        {
            JZAliasEdge *edges = (JZAliasEdge *)net->alias_edges.data;
            size_t edge_count = net->alias_edges.len / sizeof(JZAliasEdge);
            if (edge_count > 0) {
                fprintf(out, "Members (Aliased Identifiers):\n");
                for (size_t ei = 0; ei < edge_count; ++ei) {
                    JZAliasEdge *e = &edges[ei];
                    const char *lhs_name = (e->lhs_decl && e->lhs_decl->name)
                        ? e->lhs_decl->name : "<unnamed>";
                    const char *rhs_name = (e->rhs_decl && e->rhs_decl->name)
                        ? e->rhs_decl->name : "<unnamed>";

                    char rhs_buf[128];
                    const char *rhs_print = rhs_name;
                    if (have_slice_alias && e->rhs_decl == slice_rhs_decl) {
                        if (slice_msb >= slice_lsb) {
                            snprintf(rhs_buf, sizeof(rhs_buf), "%s[%u:%u]",
                                     rhs_name, slice_msb, slice_lsb);
                        } else {
                            snprintf(rhs_buf, sizeof(rhs_buf), "%s[%u:%u]",
                                     rhs_name, slice_lsb, slice_msb);
                        }
                        rhs_print = rhs_buf;
                    }

                    if (e->stmt && e->stmt->loc.filename) {
                        fprintf(out, "    %s@%d: %s == %s\n",
                                e->stmt->loc.filename, e->stmt->loc.line,
                                lhs_name, rhs_print);
                    } else if (e->stmt) {
                        fprintf(out, "    line %d: %s == %s\n",
                                e->stmt->loc.line, lhs_name, rhs_print);
                    } else {
                        fprintf(out, "    %s == %s\n", lhs_name, rhs_print);
                    }
                }
                fprintf(out, "\n");
            }
        }

        /* Drivers - split into ASYNCHRONOUS, SYNCHRONOUS(CLK=...), and other */
        if (!is_clock && net->driver_stmts.len > 0) {
            JZASTNode **drv = (JZASTNode **)net->driver_stmts.data;
            size_t drv_count = net->driver_stmts.len / sizeof(JZASTNode *);

            /* ASYNCHRONOUS Drivers */
            {
                int printed_header = 0;
                for (size_t di = 0; di < drv_count; ++di) {
                    JZASTNode *stmt = drv[di];
                    if (!stmt) continue;
                    int seen = 0;
                    for (size_t dj = 0; dj < di; ++dj) {
                        if (drv[dj] == stmt) { seen = 1; break; }
                    }
                    if (seen) continue;

                    const JZASTNode *blk = NULL;
                    const char *blk_kind = NULL;
                    sem_alias_find_enclosing_block(scope, stmt, &blk, &blk_kind);

                    if (blk_kind && strcmp(blk_kind, "ASYNCHRONOUS") == 0) {
                        if (!printed_header) {
                            fprintf(out, "ASYNCHRONOUS Drivers:\n");
                            printed_header = 1;
                        }
                        sem_alias_print_loc_source(out, stmt);
                    }
                }
                if (printed_header) fprintf(out, "\n");
            }

            /* SYNCHRONOUS Drivers - grouped by clock domain.
             * First pass: collect unique clock names from sync drivers.
             * Second pass: for each clock, print header + matching drivers.
             */
            {
                const char *clk_names[32];
                size_t clk_count = 0;

                for (size_t di = 0; di < drv_count && clk_count < 32; ++di) {
                    JZASTNode *stmt = drv[di];
                    if (!stmt) continue;
                    int seen = 0;
                    for (size_t dj = 0; dj < di; ++dj) {
                        if (drv[dj] == stmt) { seen = 1; break; }
                    }
                    if (seen) continue;

                    const JZASTNode *blk = NULL;
                    const char *blk_kind = NULL;
                    sem_alias_find_enclosing_block(scope, stmt, &blk, &blk_kind);
                    if (!blk_kind || strcmp(blk_kind, "SYNCHRONOUS") != 0) continue;

                    const char *sync_clk = sem_alias_sync_block_clock_name(blk);
                    if (!sync_clk) sync_clk = "";

                    int dup = 0;
                    for (size_t ci = 0; ci < clk_count; ++ci) {
                        if (strcmp(clk_names[ci], sync_clk) == 0) {
                            dup = 1;
                            break;
                        }
                    }
                    if (!dup) {
                        clk_names[clk_count++] = sync_clk;
                    }
                }

                for (size_t ci = 0; ci < clk_count; ++ci) {
                    const char *target_clk = clk_names[ci];
                    if (target_clk[0]) {
                        fprintf(out, "SYNCHRONOUS(CLK=%s) Drivers:\n", target_clk);
                    } else {
                        fprintf(out, "SYNCHRONOUS Drivers:\n");
                    }

                    for (size_t di = 0; di < drv_count; ++di) {
                        JZASTNode *stmt = drv[di];
                        if (!stmt) continue;
                        int seen = 0;
                        for (size_t dj = 0; dj < di; ++dj) {
                            if (drv[dj] == stmt) { seen = 1; break; }
                        }
                        if (seen) continue;

                        const JZASTNode *blk = NULL;
                        const char *blk_kind = NULL;
                        sem_alias_find_enclosing_block(scope, stmt, &blk, &blk_kind);
                        if (!blk_kind || strcmp(blk_kind, "SYNCHRONOUS") != 0) continue;

                        const char *sync_clk = sem_alias_sync_block_clock_name(blk);
                        if (!sync_clk) sync_clk = "";
                        if (strcmp(sync_clk, target_clk) != 0) continue;

                        sem_alias_print_loc_source(out, stmt);
                    }
                    fprintf(out, "\n");
                }
            }

            /* Other drivers (instance bindings, etc.) */
            {
                int printed_header = 0;
                for (size_t di = 0; di < drv_count; ++di) {
                    JZASTNode *stmt = drv[di];
                    if (!stmt) continue;
                    int seen = 0;
                    for (size_t dj = 0; dj < di; ++dj) {
                        if (drv[dj] == stmt) { seen = 1; break; }
                    }
                    if (seen) continue;

                    const JZASTNode *blk = NULL;
                    const char *blk_kind = NULL;
                    sem_alias_find_enclosing_block(scope, stmt, &blk, &blk_kind);

                    int in_async = (blk_kind && strcmp(blk_kind, "ASYNCHRONOUS") == 0);
                    int in_sync  = (blk_kind && strcmp(blk_kind, "SYNCHRONOUS") == 0);
                    if (in_async || in_sync) continue;

                    if (!printed_header) {
                        fprintf(out, "Drivers:\n");
                        printed_header = 1;
                    }
                    sem_alias_print_loc_source(out, stmt);
                }
                if (printed_header) fprintf(out, "\n");
            }
        }

        /* Clock consumers */
        if (is_clock && clock_line > 0) {
            fprintf(out, "Consumers:\n");
            const char *clk_name = clk_name_for_net;

            /* Collect all synchronous blocks using this clock. */
            JZBuffer consumer_lines = (JZBuffer){0};
            size_t consumer_count = sem_module_collect_clock_consumers(scope, clk_name, &consumer_lines);
            if (consumer_count > 0) {
                int *lines = (int *)consumer_lines.data;
                for (size_t ci = 0; ci < consumer_count; ++ci) {
                    fprintf(out, "    SYNCHRONOUS block (CLK=%s) at line %d\n",
                            clk_name ? clk_name : "<clk>", lines[ci]);
                }
            } else {
                fprintf(out, "    SYNCHRONOUS block (CLK=%s) at line %d\n",
                        clk_name ? clk_name : "<clk>", clock_line);
            }
            jz_buf_free(&consumer_lines);
            fprintf(out, "\n");

            if (project_symbols && clk_name) {
                const JZSymbol *clk_sym = sem_alias_project_lookup(project_symbols,
                                                                    clk_name,
                                                                    JZ_SYM_CLOCK);
                const JZSymbol *map_sym = NULL;

                if (project_symbols) {
                    size_t sym_count = project_symbols->len / sizeof(JZSymbol);
                    const JZSymbol *syms = (const JZSymbol *)project_symbols->data;
                    for (size_t i = 0; i < sym_count; ++i) {
                        const JZSymbol *s = &syms[i];
                        if (!s->node || s->kind != JZ_SYM_MAP_ENTRY || !s->node->name) {
                            continue;
                        }
                        if (strcmp(s->node->name, clk_name) == 0) {
                            map_sym = s;
                            break;
                        }
                    }
                }

                double period = 0.0;
                int have_period = 0;
                if (clk_sym && clk_sym->node && clk_sym->node->text) {
                    have_period = sem_alias_parse_clock_period(clk_sym->node->text,
                                                                &period);
                }

                char phys_id[64] = "";
                if (map_sym && map_sym->node && map_sym->node->text) {
                    sem_alias_trim_copy(map_sym->node->text,
                                         phys_id,
                                         sizeof(phys_id));
                }

                if (phys_id[0] || have_period) {
                    fprintf(out, "Clock Source:\n");
                    if (phys_id[0]) {
                        fprintf(out, "    Input pin %s\n", phys_id);
                    }
                    if (have_period && period > 0.0) {
                        fprintf(out, "    Period: %.3f ns\n", period);
                    }
                    fprintf(out, "\n");
                }
            }
        }

        /* Sinks */
        if (!is_clock && net->sink_stmts.len > 0) {
            fprintf(out, "Sinks:\n");
            JZASTNode **snk = (JZASTNode **)net->sink_stmts.data;
            size_t snk_count = net->sink_stmts.len / sizeof(JZASTNode *);

            for (size_t si = 0; si < snk_count; ++si) {
                JZASTNode *stmt = snk[si];
                if (!stmt) continue;

                int seen = 0;
                for (size_t sj = 0; sj < si; ++sj) {
                    if (snk[sj] == stmt) { seen = 1; break; }
                }
                if (seen) continue;

                sem_alias_print_loc_source(out, stmt);
            }
            fprintf(out, "\n");
        }

        /* Top-level binding */
        if (!is_clock && top_bindings.len > 0) {
            const JZTopPortBinding *tb = sem_alias_lookup_top_binding_for_net(&top_bindings,
                                                                              atoms,
                                                                              atom_count);
            if (tb && tb->target_name) {
                fprintf(out, "Top-level binding:\n");
                const char *dir = tb->dir ? tb->dir : "";
                const char *port_name = tb->port_decl && tb->port_decl->name
                                      ? tb->port_decl->name
                                      : "<port>";
                fprintf(out,
                        "    PORT binding: %s %s -> %s",
                        dir,
                        port_name,
                        tb->target_name);
                if (tb->clk_sym) {
                    fprintf(out, " (CLOCKS)\n");
                } else if (tb->pin_sym && tb->pin_sym->node && tb->pin_sym->node->block_kind) {
                    fprintf(out, " (%s)\n", tb->pin_sym->node->block_kind);
                } else {
                    fputc('\n', out);
                }
                fprintf(out, "\n");
            }
        }
    }

    /* Identifier index: simple name -> NET_xxxx mapping. */
    if (ident_index.len > 0) {
        fprintf(out, "------------------------------------------------------------\n");
        fprintf(out, "IDENTIFIER INDEX\n");
        fprintf(out, "------------------------------------------------------------\n");
        JZIdentIndexEntry *arr = (JZIdentIndexEntry *)ident_index.data;
        size_t count = ident_index.len / sizeof(JZIdentIndexEntry);
        for (size_t i = 0; i < count; ++i) {
            fprintf(out, "%s  -> NET_%04zu\n", arr[i].name, arr[i].net_ix);
        }
        fprintf(out, "\n");
    }

    /* Hardware summary: basic high-level view derived from the above. */
    fprintf(out, "------------------------------------------------------------\n");
    fprintf(out, "HARDWARE SUMMARY\n");
    fprintf(out, "------------------------------------------------------------\n");
    if (net_count > 0) {
        /* Clock domains: count nets we classified as clocks. */
        unsigned clock_nets = 0;
        for (size_t ni = 0; ni < raw_net_count; ++ni) {
            JZNet *net = &((JZNet *)nets->data)[ni];
            if (net->atoms.len == 0) continue;
            JZASTNode **atoms = (JZASTNode **)net->atoms.data;
            size_t atom_count = net->atoms.len / sizeof(JZASTNode *);
            int is_clock = 0;
            int dummy_line = 0;
            for (size_t ai = 0; ai < atom_count && !is_clock; ++ai) {
                JZASTNode *decl = atoms[ai];
                if (!decl || decl->type != JZ_AST_PORT_DECL || !decl->block_kind ||
                    !decl->name) {
                    continue;
                }
                if (strcmp(decl->block_kind, "IN") != 0) continue;
                if (sem_module_uses_clock(scope, decl->name, &dummy_line)) {
                    is_clock = 1;
                }
            }
            if (is_clock) clock_nets++;
        }

        fprintf(out, "- %u clock domain%s\n", clock_nets, clock_nets == 1 ? "" : "s");

        /* Register banks: count REGISTER declarations inside REGISTER blocks. */
        unsigned reg_count = 0;
        JZASTNode *mod = scope->node;
        for (size_t i = 0; i < mod->child_count; ++i) {
            JZASTNode *blk = mod->children[i];
            if (!blk) continue;
            if (blk->type == JZ_AST_REGISTER_BLOCK) {
                for (size_t j = 0; j < blk->child_count; ++j) {
                    JZASTNode *decl = blk->children[j];
                    if (decl && decl->type == JZ_AST_REGISTER_DECL) {
                        reg_count++;
                    }
                }
            }
        }
        fprintf(out, "- %u register%s\n", reg_count, reg_count == 1 ? "" : "s");

        fprintf(out, "- Alias net merges: %s\n", extended_width_aliases > 0 || total_alias_stmts > 0 ? "present" : "none");
        fprintf(out, "- Tri-state logic: %s\n", tri_state_nets > 0 ? "present" : "none");
    } else {
        fprintf(out, "- No nets in module\n");
    }
    fprintf(out, "------------------------------------------------------------\n\n");

    /* Accumulate into cross-module summary. */
    {
        JZAliasSummaryEntry entry;
        memset(&entry, 0, sizeof(entry));
        if (scope->name) {
            strncpy(entry.module_name, scope->name, sizeof(entry.module_name) - 1);
            entry.module_name[sizeof(entry.module_name) - 1] = '\0';
        }
        entry.net_count = (unsigned)net_count;
        entry.tri_state_nets = tri_state_nets;
        entry.alias_stmts = total_alias_stmts;

        /* Recount clocks and registers for the summary. */
        unsigned clock_nets_sum = 0;
        for (size_t ni2 = 0; ni2 < raw_net_count; ++ni2) {
            JZNet *n2 = &((JZNet *)nets->data)[ni2];
            if (n2->atoms.len == 0) continue;
            JZASTNode **a2 = (JZASTNode **)n2->atoms.data;
            size_t ac2 = n2->atoms.len / sizeof(JZASTNode *);
            int ic = 0;
            int dl = 0;
            for (size_t ai2 = 0; ai2 < ac2 && !ic; ++ai2) {
                JZASTNode *d2 = a2[ai2];
                if (!d2 || d2->type != JZ_AST_PORT_DECL || !d2->block_kind || !d2->name) continue;
                if (strcmp(d2->block_kind, "IN") != 0) continue;
                if (sem_module_uses_clock(scope, d2->name, &dl)) ic = 1;
            }
            if (ic) clock_nets_sum++;
        }
        entry.clock_domains = clock_nets_sum;

        unsigned rc = 0;
        JZASTNode *m2 = scope->node;
        for (size_t i2 = 0; i2 < m2->child_count; ++i2) {
            JZASTNode *b2 = m2->children[i2];
            if (!b2 || b2->type != JZ_AST_REGISTER_BLOCK) continue;
            for (size_t j2 = 0; j2 < b2->child_count; ++j2) {
                if (b2->children[j2] && b2->children[j2]->type == JZ_AST_REGISTER_DECL) rc++;
            }
        }
        entry.register_count = rc;

        (void)jz_buf_append(&g_alias_summary, &entry, sizeof(entry));
    }

    jz_buf_free(&ident_index);
    jz_buf_free(&top_bindings);
}

/* -------------------------------------------------------------------------
 *  Cross-module summary (finalize)
 * -------------------------------------------------------------------------
 */

void sem_emit_alias_report_finalize(void)
{
    if (!g_alias_report_enabled || !g_alias_report_out) {
        jz_buf_free(&g_alias_summary);
        return;
    }

    size_t entry_count = g_alias_summary.len / sizeof(JZAliasSummaryEntry);
    if (entry_count == 0) {
        jz_buf_free(&g_alias_summary);
        return;
    }

    FILE *out = g_alias_report_out;
    JZAliasSummaryEntry *entries = (JZAliasSummaryEntry *)g_alias_summary.data;

    fprintf(out, "============================================================\n");
    fprintf(out, "CROSS-MODULE SUMMARY\n");
    fprintf(out, "============================================================\n");
    fprintf(out, "Total modules analyzed: %zu\n\n", entry_count);

    unsigned total_nets = 0;
    unsigned total_clocks = 0;
    unsigned total_regs = 0;
    unsigned total_tristate = 0;
    unsigned total_aliases = 0;

    fprintf(out, "%-20s %6s %6s %6s %6s %6s\n",
            "Module", "Nets", "Clocks", "Regs", "Tri-St", "Alias");
    fprintf(out, "%-20s %6s %6s %6s %6s %6s\n",
            "--------------------", "------", "------", "------", "------", "------");

    for (size_t i = 0; i < entry_count; ++i) {
        JZAliasSummaryEntry *e = &entries[i];
        fprintf(out, "%-20s %6u %6u %6u %6u %6u\n",
                e->module_name,
                e->net_count,
                e->clock_domains,
                e->register_count,
                e->tri_state_nets,
                e->alias_stmts);
        total_nets += e->net_count;
        total_clocks += e->clock_domains;
        total_regs += e->register_count;
        total_tristate += e->tri_state_nets;
        total_aliases += e->alias_stmts;
    }

    fprintf(out, "%-20s %6s %6s %6s %6s %6s\n",
            "--------------------", "------", "------", "------", "------", "------");
    fprintf(out, "%-20s %6u %6u %6u %6u %6u\n",
            "TOTAL",
            total_nets,
            total_clocks,
            total_regs,
            total_tristate,
            total_aliases);
    fprintf(out, "============================================================\n");

    jz_buf_free(&g_alias_summary);
}
