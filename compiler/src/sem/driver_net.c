#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

#include "sem_driver.h"
#include "sem.h"
#include "util.h"
#include "rules.h"
#include "driver_internal.h"
#include <time.h>

/* Flag set by jz_sem_set_tristate_default() to suppress WARN_INTERNAL_TRISTATE
 * when the IR tri-state transform will handle internal tri-state nets.
 */
static int g_tristate_default_active = 0;

void jz_sem_set_tristate_default(int active)
{
    g_tristate_default_active = (active != 0);
}

/* -------------------------------------------------------------------------
 *  Net graph construction and simple net-usage rules
 * -------------------------------------------------------------------------
 */

static void sem_net_free(JZNet *net)
{
    if (!net) return;
    jz_buf_free(&net->atoms);
    jz_buf_free(&net->driver_stmts);
    jz_buf_free(&net->sink_stmts);
    jz_buf_free(&net->alias_edges);
}

static int sem_net_add_decl(JZBuffer *nets,
                            size_t net_ix,
                            JZASTNode *decl)
{
    if (!nets || !decl) return 0;
    if (net_ix >= (nets->len / sizeof(JZNet))) return -1;
    JZNet *arr = (JZNet *)nets->data;
    JZNet *net = &arr[net_ix];
    return jz_buf_append(&net->atoms, &decl, sizeof(JZASTNode *));
}

static int sem_net_add_binding(JZBuffer *bindings,
                               JZASTNode *decl,
                               size_t net_ix)
{
    if (!bindings || !decl) return 0;
    JZNetBinding b;
    b.decl = decl;
    b.net_ix = net_ix;
    return jz_buf_append(bindings, &b, sizeof(b));
}

JZNetBinding *sem_net_find_binding(JZBuffer *bindings,
                                          JZASTNode *decl)
{
    if (!bindings || !decl) return NULL;
    size_t count = bindings->len / sizeof(JZNetBinding);
    JZNetBinding *arr = (JZNetBinding *)bindings->data;
    for (size_t i = 0; i < count; ++i) {
        if (arr[i].decl == decl) {
            return &arr[i];
        }
    }
    return NULL;
}

/* sem_comb_collect_targets_from_lhs and sem_comb_collect_sources_from_expr
 * are declared in driver_internal.h (implemented in driver_comb.c). */

static void sem_bus_signal_access_dirs_flow(const char *bus_dir,
                                            const char *role,
                                            int *out_readable,
                                            int *out_writable)
{
    if (out_readable) *out_readable = 0;
    if (out_writable) *out_writable = 0;
    if (!bus_dir || !role) return;

    if (strcmp(bus_dir, "INOUT") == 0) {
        if (out_readable) *out_readable = 1;
        if (out_writable) *out_writable = 1;
        return;
    }

    int is_source = (strcmp(role, "SOURCE") == 0);
    int is_target = (strcmp(role, "TARGET") == 0);
    if (!is_source && !is_target) return;

    if (strcmp(bus_dir, "OUT") == 0) {
        if (is_source) {
            if (out_writable) *out_writable = 1;
        } else {
            if (out_readable) *out_readable = 1;
        }
    } else if (strcmp(bus_dir, "IN") == 0) {
        if (is_source) {
            if (out_readable) *out_readable = 1;
        } else {
            if (out_writable) *out_writable = 1;
        }
    }
}

static int sem_bus_binding_access_dirs(const JZASTNode *bind,
                                       const JZBuffer *project_symbols,
                                       int *out_readable,
                                       int *out_writable)
{
    if (out_readable) *out_readable = 0;
    if (out_writable) *out_writable = 0;
    if (!bind || !bind->text || !project_symbols || !project_symbols->data) return 0;

    char bus_id[128];
    char role[128];
    if (sscanf(bind->text, "%127s %127s", bus_id, role) != 2) {
        return 0;
    }

    const JZSymbol *syms = (const JZSymbol *)project_symbols->data;
    size_t count = project_symbols->len / sizeof(JZSymbol);
    const JZSymbol *bus_sym = NULL;
    for (size_t i = 0; i < count; ++i) {
        if (syms[i].kind == JZ_SYM_BUS && syms[i].name &&
            strcmp(syms[i].name, bus_id) == 0) {
            bus_sym = &syms[i];
            break;
        }
    }
    if (!bus_sym || !bus_sym->node) {
        return 0;
    }

    int readable = 0;
    int writable = 0;
    for (size_t i = 0; i < bus_sym->node->child_count; ++i) {
        JZASTNode *decl = bus_sym->node->children[i];
        if (!decl || decl->type != JZ_AST_BUS_DECL || !decl->block_kind) continue;
        int r = 0;
        int w = 0;
        sem_bus_signal_access_dirs_flow(decl->block_kind, role, &r, &w);
        if (r) readable = 1;
        if (w) writable = 1;
        if (readable && writable) break;
    }

    if (out_readable) *out_readable = readable;
    if (out_writable) *out_writable = writable;
    return (readable || writable);
}

static JZASTNode *sem_net_bus_port_decl_from_expr(const JZASTNode *expr,
                                                  const JZModuleScope *scope)
{
    if (!expr || !scope) return NULL;

    const char *port_name = NULL;
    char buf[256];

    if (expr->type == JZ_AST_EXPR_BUS_ACCESS) {
        port_name = expr->name;
    } else if (expr->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER && expr->name) {
        const char *dot = strchr(expr->name, '.');
        if (!dot || dot == expr->name) return NULL;
        size_t len = (size_t)(dot - expr->name);
        if (len == 0 || len >= sizeof(buf)) return NULL;
        memcpy(buf, expr->name, len);
        buf[len] = '\0';
        port_name = buf;
    } else {
        return NULL;
    }

    if (!port_name) return NULL;
    const JZSymbol *sym = module_scope_lookup_kind(scope, port_name, JZ_SYM_PORT);
    if (!sym || !sym->node || !sym->node->block_kind) return NULL;
    if (strcmp(sym->node->block_kind, "BUS") != 0) return NULL;
    return sym->node;
}


static int sem_net_merge(JZBuffer *nets,
                         JZBuffer *bindings,
                         size_t a_ix,
                         size_t b_ix)
{
    if (!nets || !bindings) return 0;
    if (a_ix == b_ix) return 0;
    size_t count = nets->len / sizeof(JZNet);
    if (a_ix >= count || b_ix >= count) return 0;

    size_t root_ix = (a_ix < b_ix) ? a_ix : b_ix;
    size_t other_ix = (a_ix < b_ix) ? b_ix : a_ix;

    JZNet *arr = (JZNet *)nets->data;
    JZNet *root = &arr[root_ix];
    JZNet *other = &arr[other_ix];

    /* Move atoms. */
    JZASTNode **atoms = (JZASTNode **)other->atoms.data;
    size_t atom_count = other->atoms.len / sizeof(JZASTNode *);
    for (size_t i = 0; i < atom_count; ++i) {
        (void)jz_buf_append(&root->atoms, &atoms[i], sizeof(JZASTNode *));
    }

    /* Move driver and sink statement references. */
    JZASTNode **drv = (JZASTNode **)other->driver_stmts.data;
    size_t drv_count = other->driver_stmts.len / sizeof(JZASTNode *);
    for (size_t i = 0; i < drv_count; ++i) {
        (void)jz_buf_append(&root->driver_stmts, &drv[i], sizeof(JZASTNode *));
    }
    JZASTNode **snk = (JZASTNode **)other->sink_stmts.data;
    size_t snk_count = other->sink_stmts.len / sizeof(JZASTNode *);
    for (size_t i = 0; i < snk_count; ++i) {
        (void)jz_buf_append(&root->sink_stmts, &snk[i], sizeof(JZASTNode *));
    }

    /* Move alias edges. */
    JZAliasEdge *edges = (JZAliasEdge *)other->alias_edges.data;
    size_t edge_count = other->alias_edges.len / sizeof(JZAliasEdge);
    for (size_t i = 0; i < edge_count; ++i) {
        (void)jz_buf_append(&root->alias_edges, &edges[i], sizeof(JZAliasEdge));
    }

    /* Update bindings so future lookups see the merged net. */
    size_t bind_count = bindings->len / sizeof(JZNetBinding);
    JZNetBinding *barr = (JZNetBinding *)bindings->data;
    for (size_t i = 0; i < bind_count; ++i) {
        if (barr[i].net_ix == other_ix) {
            barr[i].net_ix = root_ix;
        }
    }

    /* Free buffers on the now-empty net. */
    sem_net_free(other);
    return 0;
}

/* Conservative fallback: check whether a declaration name appears in any
 * assignment (LHS or RHS) within the given module. This is used to avoid
 * reporting NET_DANGLING_UNUSED when the AST clearly contains assignments
 * involving this identifier but flow analysis failed to record drivers/sinks
 * (for example, due to earlier semantic errors on the assignment).
 */
static int sem_expr_contains_name(const JZASTNode *expr, const char *name)
{
    if (!expr || !name) return 0;

    switch (expr->type) {
    case JZ_AST_EXPR_IDENTIFIER:
        return (expr->name && strcmp(expr->name, name) == 0);

    case JZ_AST_EXPR_SLICE:
    case JZ_AST_EXPR_CONCAT:
    case JZ_AST_EXPR_UNARY:
    case JZ_AST_EXPR_BINARY:
    case JZ_AST_EXPR_TERNARY:
    case JZ_AST_EXPR_BUILTIN_CALL:
        for (size_t i = 0; i < expr->child_count; ++i) {
            if (sem_expr_contains_name(expr->children[i], name)) {
                return 1;
            }
        }
        return 0;

    default:
        return 0;
    }
}

static int sem_node_has_assignment_for_name(const JZASTNode *node,
                                             const char *name)
{
    if (!node || !name) return 0;

    if (node->type == JZ_AST_STMT_ASSIGN && node->child_count >= 2) {
        const JZASTNode *lhs = node->children[0];
        const JZASTNode *rhs = node->children[1];
        if (sem_expr_contains_name(lhs, name) ||
            sem_expr_contains_name(rhs, name)) {
            return 1;
        }
    }

    for (size_t i = 0; i < node->child_count; ++i) {
        if (sem_node_has_assignment_for_name(node->children[i], name)) {
            return 1;
        }
    }

    return 0;
}

/* Conservative AST-based check: does this block contain any *read* of the
 * given identifier name (as opposed to just assignments)? We approximate reads
 * as occurrences in expression contexts: RHS of assignments, IF/ELIF
 * conditions, SELECT/CASE expressions, etc. This is used to avoid reporting
 * ASYNC_UNDEFINED_PATH_NO_DRIVER when a net is written in some but not all
 * paths but its value is never inspected.
 */
int sem_block_reads_name(const JZASTNode *node, const char *name)
{
    if (!node || !name) return 0;

    switch (node->type) {
    case JZ_AST_STMT_ASSIGN:
        if (node->child_count >= 2) {
            const JZASTNode *rhs = node->children[1];
            if (sem_expr_contains_name(rhs, name)) {
                return 1;
            }
        }
        break;

    case JZ_AST_STMT_IF:
    case JZ_AST_STMT_ELIF:
        /* Child[0] is the condition expression. */
        if (node->child_count > 0 && node->children[0] &&
            sem_expr_contains_name(node->children[0], name)) {
            return 1;
        }
        /* Bodies. */
        for (size_t j = 1; j < node->child_count; ++j) {
            if (sem_block_reads_name(node->children[j], name)) {
                return 1;
            }
        }
        return 0;

    case JZ_AST_STMT_SELECT:
        /* SELECT(condition) { ... } – condition is child[0]. */
        if (node->child_count > 0 && node->children[0] &&
            sem_expr_contains_name(node->children[0], name)) {
            return 1;
        }
        for (size_t j = 1; j < node->child_count; ++j) {
            if (sem_block_reads_name(node->children[j], name)) {
                return 1;
            }
        }
        return 0;

    case JZ_AST_STMT_CASE:
    case JZ_AST_STMT_DEFAULT:
    case JZ_AST_STMT_ELSE:
    case JZ_AST_BLOCK:
        for (size_t j = 0; j < node->child_count; ++j) {
            if (sem_block_reads_name(node->children[j], name)) {
                return 1;
            }
        }
        return 0;

    default:
        break;
    }

    /* Generic fallback: scan children. */
    for (size_t i = 0; i < node->child_count; ++i) {
        if (sem_block_reads_name(node->children[i], name)) {
            return 1;
        }
    }

    return 0;
}

static int sem_net_record_driver(JZBuffer *nets,
                                 JZNetBinding *binding,
                                 JZASTNode *stmt)
{
    if (!nets || !binding || !stmt) return 0;
    size_t count = nets->len / sizeof(JZNet);
    if (binding->net_ix >= count) return 0;
    JZNet *arr = (JZNet *)nets->data;
    JZNet *net = &arr[binding->net_ix];
    return jz_buf_append(&net->driver_stmts, &stmt, sizeof(JZASTNode *));
}

static int sem_net_record_sink(JZBuffer *nets,
                               JZNetBinding *binding,
                               JZASTNode *stmt)
{
    if (!nets || !binding || !stmt) return 0;
    size_t count = nets->len / sizeof(JZNet);
    if (binding->net_ix >= count) return 0;
    JZNet *arr = (JZNet *)nets->data;
    JZNet *net = &arr[binding->net_ix];
    return jz_buf_append(&net->sink_stmts, &stmt, sizeof(JZASTNode *));
}

static int sem_net_build_initial_for_module(const JZModuleScope *scope,
                                            JZBuffer *nets,
                                            JZBuffer *bindings)
{
    if (!scope || !nets || !bindings) return 0;

    size_t sym_count = scope->symbols.len / sizeof(JZSymbol);
    const JZSymbol *syms = (const JZSymbol *)scope->symbols.data;
    for (size_t i = 0; i < sym_count; ++i) {
        const JZSymbol *sym = &syms[i];
        if (!sym->node || !sym->name) continue;
        if (sym->kind != JZ_SYM_PORT &&
            sym->kind != JZ_SYM_WIRE &&
            sym->kind != JZ_SYM_REGISTER &&
            sym->kind != JZ_SYM_LATCH) {
            continue;
        }

        JZNet net = (JZNet){0};
        size_t net_ix = nets->len / sizeof(JZNet);
        if (jz_buf_append(nets, &net, sizeof(net)) != 0) {
            return -1;
        }
        if (sem_net_add_decl(nets, net_ix, sym->node) != 0) {
            return -1;
        }
        if (sem_net_add_binding(bindings, sym->node, net_ix) != 0) {
            return -1;
        }
    }

    return 0;
}

static void sem_net_mark_expr_sinks(JZASTNode *expr,
                                    const JZModuleScope *scope,
                                    JZBuffer *nets,
                                    JZBuffer *bindings,
                                    JZASTNode *stmt)
{
    if (!expr || !scope || !nets || !bindings) return;
    /* If no explicit statement context is provided, fall back to using the
     * expression node itself as the sink location. This is useful for
     * contexts like CDC clock expressions where we want the net to be
     * considered "used" but do not have a dedicated statement node.
     */
    if (!stmt) {
        stmt = expr;
    }

    switch (expr->type) {
    case JZ_AST_EXPR_IDENTIFIER: {
        if (!expr->name) break;
        const JZSymbol *sym = module_scope_lookup(scope, expr->name);
        if (!sym || !sym->node) break;
        if (sym->kind != JZ_SYM_PORT &&
            sym->kind != JZ_SYM_WIRE &&
            sym->kind != JZ_SYM_REGISTER &&
            sym->kind != JZ_SYM_LATCH) {
            break;
        }
        JZNetBinding *binding = sem_net_find_binding(bindings, sym->node);
        if (binding) {
            (void)sem_net_record_sink(nets, binding, stmt);
        }
        break;
    }

    case JZ_AST_EXPR_BUS_ACCESS:
    case JZ_AST_EXPR_QUALIFIED_IDENTIFIER: {
        JZASTNode *bus_port = sem_net_bus_port_decl_from_expr(expr, scope);
        if (!bus_port) break;
        JZNetBinding *binding = sem_net_find_binding(bindings, bus_port);
        if (binding) {
            (void)sem_net_record_sink(nets, binding, stmt);
        }
        break;
    }

    case JZ_AST_EXPR_SLICE:
    case JZ_AST_EXPR_CONCAT:
    case JZ_AST_EXPR_UNARY:
    case JZ_AST_EXPR_BINARY:
    case JZ_AST_EXPR_TERNARY:
    case JZ_AST_EXPR_BUILTIN_CALL:
        for (size_t i = 0; i < expr->child_count; ++i) {
            sem_net_mark_expr_sinks(expr->children[i], scope, nets, bindings, stmt);
        }
        break;

    default:
        break;
    }
}

static void sem_net_mark_lvalue_drivers(JZASTNode *expr,
                                        const JZModuleScope *scope,
                                        JZBuffer *nets,
                                        JZBuffer *bindings,
                                        JZASTNode *stmt)
{
    if (!expr || !scope || !nets || !bindings || !stmt) return;

    switch (expr->type) {
    case JZ_AST_EXPR_CONCAT:
        for (size_t i = 0; i < expr->child_count; ++i) {
            sem_net_mark_lvalue_drivers(expr->children[i], scope, nets, bindings, stmt);
        }
        break;

    case JZ_AST_EXPR_SLICE:
        if (expr->child_count >= 1) {
            sem_net_mark_lvalue_drivers(expr->children[0], scope, nets, bindings, stmt);
        }
        break;

    case JZ_AST_EXPR_IDENTIFIER: {
        if (!expr->name) break;
        const JZSymbol *sym = module_scope_lookup(scope, expr->name);
        if (!sym || !sym->node) break;
        if (sym->kind != JZ_SYM_PORT &&
            sym->kind != JZ_SYM_WIRE &&
            sym->kind != JZ_SYM_REGISTER &&
            sym->kind != JZ_SYM_LATCH) {
            break;
        }
        JZNetBinding *binding = sem_net_find_binding(bindings, sym->node);
        if (binding) {
            (void)sem_net_record_driver(nets, binding, stmt);
        }
        break;
    }

    case JZ_AST_EXPR_BUS_ACCESS:
    case JZ_AST_EXPR_QUALIFIED_IDENTIFIER: {
        JZASTNode *bus_port = sem_net_bus_port_decl_from_expr(expr, scope);
        if (!bus_port) break;
        JZNetBinding *binding = sem_net_find_binding(bindings, bus_port);
        if (binding) {
            (void)sem_net_record_driver(nets, binding, stmt);
        }
        break;
    }

    default:
        break;
    }
}

static void sem_net_collect_aliases_in_block(JZASTNode *block,
                                             const JZModuleScope *scope,
                                             JZBuffer *nets,
                                             JZBuffer *bindings,
                                             int is_sync)
{
    if (!block || !scope || !nets || !bindings) return;

    for (size_t i = 0; i < block->child_count; ++i) {
        JZASTNode *stmt = block->children[i];
        if (!stmt) continue;

        switch (stmt->type) {
        case JZ_AST_STMT_ASSIGN: {
            if (is_sync || stmt->child_count < 2) {
                break;
            }
            const char *op = stmt->block_kind ? stmt->block_kind : "";
            int is_alias = (strcmp(op, "ALIAS") == 0 ||
                            strcmp(op, "ALIAS_Z") == 0 ||
                            strcmp(op, "ALIAS_S") == 0);
            if (!is_alias) {
                break;
            }

            JZASTNode *lhs = stmt->children[0];
            JZASTNode *rhs = stmt->children[1];

            /* Collect all base declarations on each side of the alias,
             * including identifiers that appear inside concatenations and
             * slices. This lets the alias report reflect concatenation-based
             * aliasing instead of only simple `id = id;` forms.
             */
            JZBuffer lhs_decls = {0};
            JZBuffer rhs_decls = {0};
            sem_comb_collect_targets_from_lhs(lhs, scope, &lhs_decls);
            sem_comb_collect_sources_from_expr(rhs, scope, &rhs_decls);

            size_t lhs_count = lhs_decls.len / sizeof(JZASTNode *);
            size_t rhs_count = rhs_decls.len / sizeof(JZASTNode *);
            if (lhs_count == 0 || rhs_count == 0) {
                jz_buf_free(&lhs_decls);
                jz_buf_free(&rhs_decls);
                break;
            }

            size_t net_count = nets->len / sizeof(JZNet);

            JZASTNode **lhs_nodes = (JZASTNode **)lhs_decls.data;
            JZASTNode **rhs_nodes = (JZASTNode **)rhs_decls.data;

            for (size_t li = 0; li < lhs_count; ++li) {
                JZASTNode *lhs_decl = lhs_nodes[li];
                if (!lhs_decl) continue;
                JZNetBinding *lhs_bind = sem_net_find_binding(bindings, lhs_decl);
                if (!lhs_bind) continue;

                for (size_t ri = 0; ri < rhs_count; ++ri) {
                    JZASTNode *rhs_decl = rhs_nodes[ri];
                    if (!rhs_decl) continue;
                    JZNetBinding *rhs_bind = sem_net_find_binding(bindings, rhs_decl);
                    if (!rhs_bind) continue;

                    /* Record alias edge on the root net (min index) before merging. */
                    size_t a_ix = lhs_bind->net_ix;
                    size_t b_ix = rhs_bind->net_ix;
                    size_t root_ix = (a_ix < b_ix) ? a_ix : b_ix;
                    if (root_ix < net_count) {
                        JZNet *net_arr = (JZNet *)nets->data;
                        JZNet *root = &net_arr[root_ix];
                        JZAliasEdge edge;
                        edge.lhs_decl = lhs_decl;
                        edge.rhs_decl = rhs_decl;
                        edge.stmt     = stmt;
                        (void)jz_buf_append(&root->alias_edges, &edge, sizeof(edge));
                    }

                    (void)sem_net_merge(nets, bindings, lhs_bind->net_ix, rhs_bind->net_ix);
                }
            }

            jz_buf_free(&lhs_decls);
            jz_buf_free(&rhs_decls);
            break;
        }

        case JZ_AST_STMT_IF:
        case JZ_AST_STMT_ELIF:
            for (size_t j = 1; j < stmt->child_count; ++j) {
                sem_net_collect_aliases_in_block(stmt->children[j], scope, nets, bindings, is_sync);
            }
            break;

        case JZ_AST_STMT_ELSE:
        case JZ_AST_STMT_SELECT:
        case JZ_AST_STMT_CASE:
        case JZ_AST_STMT_DEFAULT:
            for (size_t j = 0; j < stmt->child_count; ++j) {
                sem_net_collect_aliases_in_block(stmt->children[j], scope, nets, bindings, is_sync);
            }
            break;

        case JZ_AST_FEATURE_GUARD:
            for (size_t j = 1; j < stmt->child_count; ++j) {
                JZASTNode *branch = stmt->children[j];
                if (!branch) continue;
                sem_net_collect_aliases_in_block(branch, scope, nets, bindings, is_sync);
            }
            break;

        default:
            break;
        }
    }
}

static void sem_net_collect_usage_for_stmt(JZASTNode *stmt,
                                           const JZModuleScope *scope,
                                           JZBuffer *nets,
                                           JZBuffer *bindings,
                                           int is_sync)
{
    if (!stmt || !scope || !nets || !bindings) return;

    switch (stmt->type) {
    case JZ_AST_SYNC_PARAM:
        for (size_t j = 0; j < stmt->child_count; ++j) {
            JZASTNode *expr = stmt->children[j];
            sem_net_mark_expr_sinks(expr, scope, nets, bindings, stmt);
        }
        break;

    case JZ_AST_STMT_ASSIGN: {
        if (stmt->child_count < 2) break;
        JZASTNode *lhs = stmt->children[0];
        JZASTNode *rhs = stmt->children[1];

        const char *op = stmt->block_kind ? stmt->block_kind : "";
        int is_alias   = (strcmp(op, "ALIAS") == 0 ||
                           strcmp(op, "ALIAS_Z") == 0 ||
                           strcmp(op, "ALIAS_S") == 0);
        int is_drive   = (strncmp(op, "DRIVE", 5) == 0);
        int is_receive = (strncmp(op, "RECEIVE", 7) == 0);
        int is_latch_guard = (strcmp(op, "RECEIVE_LATCH") == 0);

        if (is_sync && is_alias) {
            is_alias = 0;
        }

        /* Determine which side is actually being driven, based on the
         * directional operator semantics:
         *   - DRIVE*:  source => sink   (RHS is driven)
         *   - RECEIVE*: sink <= source  (LHS is driven)
         *   - plain '=' or anything else: LHS is driven.
         */
        if (!is_alias) {
            JZASTNode *driver_target = NULL;
            if (is_drive) {
                driver_target = rhs;
            } else {
                /* For RECEIVE_* and bare '=', LHS is the driven net. */
                driver_target = lhs;
            }
            if (driver_target) {
                sem_net_mark_lvalue_drivers(driver_target, scope, nets, bindings, stmt);
            }
        }

        /* Mark sinks according to which side is being read:
         *   - DRIVE*: source (LHS) is read, sink (RHS) may also be read by
         *     later logic.
         *   - RECEIVE*: source (RHS) is read, sink (LHS) may also be read.
         *   - plain '=': conservatively treat RHS as the source.
         */
        if (is_drive) {
            sem_net_mark_expr_sinks(lhs, scope, nets, bindings, stmt);
        } else if (is_receive) {
            sem_net_mark_expr_sinks(rhs, scope, nets, bindings, stmt);
        } else {
            /* Plain '=' or unknown: treat RHS as the value source. */
            sem_net_mark_expr_sinks(rhs, scope, nets, bindings, stmt);
        }

        /* Always mark the RHS as a potential sink to remain conservative for
         * complex expressions and future analyses.
         */
        sem_net_mark_expr_sinks(rhs, scope, nets, bindings, stmt);
        if (is_latch_guard && stmt->child_count >= 3 && stmt->children[2]) {
            sem_net_mark_expr_sinks(stmt->children[2], scope, nets, bindings, stmt);
        }
        break;
    }

    case JZ_AST_STMT_IF:
    case JZ_AST_STMT_ELIF:
        /* Child[0] is the condition expression; its operands are sinks. */
        if (stmt->child_count > 0 && stmt->children[0]) {
            sem_net_mark_expr_sinks(stmt->children[0], scope, nets, bindings, stmt);
        }
        /* Remaining children are the statement bodies for this branch. */
        for (size_t j = 1; j < stmt->child_count; ++j) {
            JZASTNode *body = stmt->children[j];
            sem_net_collect_usage_for_stmt(body, scope, nets, bindings, is_sync);
        }
        break;

    case JZ_AST_STMT_SELECT:
        /* SELECT(condition) { ... } – condition is child[0] and should mark
         * any referenced nets as sinks, just like IF/ELIF conditions. This
         * ensures selector nets are treated as "used" for NET_DANGLING_UNUSED
         * and related net-usage diagnostics.
         */
        if (stmt->child_count > 0 && stmt->children[0]) {
            sem_net_mark_expr_sinks(stmt->children[0], scope, nets, bindings, stmt);
        }
        /* Children after the condition are CASE/DEFAULT blocks or nested
         * statements. Recurse into them to collect further usage.
         */
        for (size_t j = 1; j < stmt->child_count; ++j) {
            JZASTNode *body = stmt->children[j];
            sem_net_collect_usage_for_stmt(body, scope, nets, bindings, is_sync);
        }
        break;

    case JZ_AST_STMT_ELSE:
    case JZ_AST_STMT_CASE:
    case JZ_AST_STMT_DEFAULT:
        for (size_t j = 0; j < stmt->child_count; ++j) {
            JZASTNode *body = stmt->children[j];
            sem_net_collect_usage_for_stmt(body, scope, nets, bindings, is_sync);
        }
        break;

    case JZ_AST_FEATURE_GUARD:
        /* Recurse into both THEN (children[1]) and ELSE (children[2]) branches
         * so that assignments inside @feature/@else blocks are recorded as
         * drivers/sinks. children[0] is the condition expression.
         */
        for (size_t j = 1; j < stmt->child_count; ++j) {
            JZASTNode *branch = stmt->children[j];
            if (!branch) continue;
            for (size_t k = 0; k < branch->child_count; ++k) {
                sem_net_collect_usage_for_stmt(branch->children[k], scope, nets, bindings, is_sync);
            }
        }
        break;

    default:
        break;
    }
}

static void sem_net_collect_usage_in_block(JZASTNode *block,
                                           const JZModuleScope *scope,
                                           JZBuffer *nets,
                                           JZBuffer *bindings,
                                           int is_sync)
{
    if (!block || !scope || !nets || !bindings) return;

    for (size_t i = 0; i < block->child_count; ++i) {
        JZASTNode *stmt = block->children[i];
        if (!stmt) continue;
        sem_net_collect_usage_for_stmt(stmt, scope, nets, bindings, is_sync);
    }
}

static int sem_expr_is_all_z_literal(const JZASTNode *expr)
{
    if (!expr) return 0;

    if (expr->type == JZ_AST_EXPR_LITERAL && expr->text) {
        const char *lex = expr->text;
        const char *tick = strchr(lex, '\'');
        if (!tick) {
            return 0;
        }
        const char *value = tick + 2;
        if (!*value) return 0;
        int saw_bit = 0;
        for (const char *p = value; *p; ++p) {
            char c = *p;
            if (c == '_' || isspace((unsigned char)c)) continue;
            saw_bit = 1;
            if (c != 'z' && c != 'Z') {
                return 0;
            }
        }
        return saw_bit ? 1 : 0;
    }

    if (expr->type == JZ_AST_EXPR_CONCAT) {
        if (expr->child_count == 0) return 0;
        for (size_t i = 0; i < expr->child_count; ++i) {
            if (!sem_expr_is_all_z_literal(expr->children[i])) {
                return 0;
            }
        }
        return 1;
    }

    return 0;
}

/* AST helper: for a given identifier name, detect whether there are any
 * assignments "name = <rhs>" in the module AST, and classify RHS as
 * all-z vs non-z. Used as a fallback when the net graph fails to record
 * drivers for INOUT nets.
 */
static void sem_ast_driver_z_info_for_name(const JZASTNode *node,
                                           const char *name,
                                           int *out_has_driver,
                                           int *out_all_z,
                                           int *out_any_non_z)
{
    if (!node || !name) return;

    if (node->type == JZ_AST_STMT_ASSIGN && node->child_count >= 2) {
        const char *op = node->block_kind ? node->block_kind : "";
        int is_alias = (strcmp(op, "ALIAS") == 0 ||
                        strcmp(op, "ALIAS_Z") == 0 ||
                        strcmp(op, "ALIAS_S") == 0);

        if (!is_alias) {
            const JZASTNode *lhs = node->children[0];
            const JZASTNode *rhs = node->children[1];

            /* Peel slices from LHS to find base identifier. */
            while (lhs && lhs->type == JZ_AST_EXPR_SLICE && lhs->child_count >= 1) {
                lhs = lhs->children[0];
            }
            if (lhs && lhs->type == JZ_AST_EXPR_IDENTIFIER && lhs->name &&
                strcmp(lhs->name, name) == 0) {
                if (out_has_driver) *out_has_driver = 1;
                if (sem_expr_is_all_z_literal(rhs)) {
                    if (out_all_z) *out_all_z = 1;
                } else {
                    if (out_any_non_z) *out_any_non_z = 1;
                }
            }
        } else {
            /* For alias assignments (=), a wire appearing on the RHS is
             * implicitly driven through the bidirectional alias connection.
             * e.g., "src.DATA = data;" means data is driven by src.DATA.
             */
            const JZASTNode *rhs = node->children[1];
            if (rhs && rhs->type == JZ_AST_EXPR_IDENTIFIER && rhs->name &&
                strcmp(rhs->name, name) == 0) {
                if (out_has_driver) *out_has_driver = 1;
                if (out_any_non_z) *out_any_non_z = 1;
            }
        }
    }

    for (size_t i = 0; i < node->child_count; ++i) {
        sem_ast_driver_z_info_for_name(node->children[i], name,
                                       out_has_driver, out_all_z, out_any_non_z);
    }
}

/* Return non-zero if an expression tree contains any literal that assigns
 * one or more 'z' bits (high-impedance) anywhere in the value.
 */
static int sem_expr_contains_z_literal_anywhere(const JZASTNode *expr)
{
    if (!expr) return 0;

    if (expr->type == JZ_AST_EXPR_LITERAL && expr->text) {
        const char *lex = expr->text;
        const char *tick = strchr(lex, '\'');
        if (!tick) {
            return 0;
        }
        const char *value = tick + 2;
        if (!*value) return 0;
        for (const char *p = value; *p; ++p) {
            char c = *p;
            if (c == '_' || isspace((unsigned char)c)) continue;
            if (c == 'z' || c == 'Z') {
                return 1;
            }
        }
        return 0;
    }

    for (size_t i = 0; i < expr->child_count; ++i) {
        if (sem_expr_contains_z_literal_anywhere(expr->children[i])) {
            return 1;
        }
    }
    return 0;
}

/* Conservative helper: does this assignment statement ever drive 'z' to
 * any of its targets? We approximate by checking the RHS expression tree
 * for any 'z' literal.
 */
static int sem_stmt_can_drive_z(const JZASTNode *stmt)
{
    if (!stmt || stmt->type != JZ_AST_STMT_ASSIGN || stmt->child_count < 2) return 0;
    const JZASTNode *rhs = stmt->children[1];
    return sem_expr_contains_z_literal_anywhere(rhs);
}

/* Check if any assignment to `name` in the AST subtree can produce 'z'.
 * This is used to determine if a module's output port is tri-state capable.
 */
static int sem_port_can_drive_z_in_ast(const JZASTNode *node, const char *name)
{
    if (!node || !name) return 0;

    if (node->type == JZ_AST_STMT_ASSIGN && node->child_count >= 2) {
        const char *op = node->block_kind ? node->block_kind : "";
        int is_alias = (strcmp(op, "ALIAS") == 0 ||
                        strcmp(op, "ALIAS_Z") == 0 ||
                        strcmp(op, "ALIAS_S") == 0);

        if (!is_alias) {
            const JZASTNode *lhs = node->children[0];
            const JZASTNode *rhs = node->children[1];

            /* Peel slices from LHS to find base identifier. */
            while (lhs && lhs->type == JZ_AST_EXPR_SLICE && lhs->child_count >= 1) {
                lhs = lhs->children[0];
            }
            if (lhs && lhs->type == JZ_AST_EXPR_IDENTIFIER && lhs->name &&
                strcmp(lhs->name, name) == 0) {
                if (sem_expr_contains_z_literal_anywhere(rhs)) {
                    return 1;
                }
            }
        }
    }

    for (size_t i = 0; i < node->child_count; ++i) {
        if (sem_port_can_drive_z_in_ast(node->children[i], name)) {
            return 1;
        }
    }
    return 0;
}

/* Check if any assignment to `name` in the AST subtree can produce non-z values.
 * Returns 1 if there's any assignment that doesn't just produce z.
 */
static int sem_port_can_drive_non_z_in_ast(const JZASTNode *node, const char *name)
{
    if (!node || !name) return 0;

    if (node->type == JZ_AST_STMT_ASSIGN && node->child_count >= 2) {
        const char *op = node->block_kind ? node->block_kind : "";
        int is_alias = (strcmp(op, "ALIAS") == 0 ||
                        strcmp(op, "ALIAS_Z") == 0 ||
                        strcmp(op, "ALIAS_S") == 0);

        if (!is_alias) {
            const JZASTNode *lhs = node->children[0];
            const JZASTNode *rhs = node->children[1];

            /* Peel slices from LHS to find base identifier. */
            while (lhs && lhs->type == JZ_AST_EXPR_SLICE && lhs->child_count >= 1) {
                lhs = lhs->children[0];
            }
            if (lhs && lhs->type == JZ_AST_EXPR_IDENTIFIER && lhs->name &&
                strcmp(lhs->name, name) == 0) {
                /* Check if RHS is NOT all-z. We approximate by checking if
                 * it's not a pure z literal or contains any non-z values.
                 */
                if (!sem_expr_is_all_z_literal(rhs)) {
                    return 1;
                }
            }
        }
    }

    for (size_t i = 0; i < node->child_count; ++i) {
        if (sem_port_can_drive_non_z_in_ast(node->children[i], name)) {
            return 1;
        }
    }
    return 0;
}

/* Find the module scope for a given module name in the scopes buffer. */
static const JZModuleScope *sem_find_scope_by_name(const JZBuffer *module_scopes,
                                                    const char *name)
{
    if (!module_scopes || !name) return NULL;
    size_t count = module_scopes->len / sizeof(JZModuleScope);
    const JZModuleScope *scopes = (const JZModuleScope *)module_scopes->data;
    for (size_t i = 0; i < count; ++i) {
        if (scopes[i].name && strcmp(scopes[i].name, name) == 0) {
            return &scopes[i];
        }
    }
    return NULL;
}

/* Check if a module instance driver is tri-state capable for driving a specific
 * net. Returns 1 if the instance's output port can produce 'z', 0 otherwise.
 */
static int sem_instance_driver_can_produce_z(const JZASTNode *inst_node,
                                              const char *net_name,
                                              const JZBuffer *module_scopes)
{
    if (!inst_node || inst_node->type != JZ_AST_MODULE_INSTANCE ||
        !net_name || !module_scopes) {
        return 0;
    }

    /* Find the port binding that drives this net. */
    const char *child_port_name = NULL;
    for (size_t bi = 0; bi < inst_node->child_count; ++bi) {
        const JZASTNode *bind = inst_node->children[bi];
        if (!bind || bind->type != JZ_AST_PORT_DECL || !bind->block_kind) continue;
        const char *dir = bind->block_kind;
        int is_out   = (strcmp(dir, "OUT") == 0);
        int is_inout = (strcmp(dir, "INOUT") == 0);
        if (!is_out && !is_inout) continue;
        if (bind->child_count == 0) continue;

        /* Check if the binding's RHS is/contains the net identifier. */
        const JZASTNode *rhs = bind->children[0];
        /* Handle simple identifier case. */
        if (rhs && rhs->type == JZ_AST_EXPR_IDENTIFIER && rhs->name &&
            strcmp(rhs->name, net_name) == 0) {
            child_port_name = bind->name;
            break;
        }
        /* Handle slice case - peel to base. */
        const JZASTNode *base = rhs;
        while (base && base->type == JZ_AST_EXPR_SLICE && base->child_count >= 1) {
            base = base->children[0];
        }
        if (base && base->type == JZ_AST_EXPR_IDENTIFIER && base->name &&
            strcmp(base->name, net_name) == 0) {
            child_port_name = bind->name;
            break;
        }
    }

    if (!child_port_name) return 0;

    /* Look up the child module. */
    const char *child_mod_name = inst_node->text;
    if (!child_mod_name) return 0;

    const JZModuleScope *child_scope = sem_find_scope_by_name(module_scopes, child_mod_name);
    if (!child_scope || !child_scope->node) return 0;

    /* Check if the port can produce 'z' in the child module. */
    return sem_port_can_drive_z_in_ast(child_scope->node, child_port_name);
}

/* Check if a module instance driver can produce non-z values for a specific net.
 * Returns 1 if the instance's output port can produce non-z values, 0 otherwise.
 */
static int sem_instance_driver_can_produce_non_z(const JZASTNode *inst_node,
                                                  const char *net_name,
                                                  const JZBuffer *module_scopes)
{
    if (!inst_node || inst_node->type != JZ_AST_MODULE_INSTANCE ||
        !net_name || !module_scopes) {
        return 0;
    }

    /* Find the port binding that drives this net. */
    const char *child_port_name = NULL;
    for (size_t bi = 0; bi < inst_node->child_count; ++bi) {
        const JZASTNode *bind = inst_node->children[bi];
        if (!bind || bind->type != JZ_AST_PORT_DECL || !bind->block_kind) continue;
        const char *dir = bind->block_kind;
        int is_out   = (strcmp(dir, "OUT") == 0);
        int is_inout = (strcmp(dir, "INOUT") == 0);
        if (!is_out && !is_inout) continue;
        if (bind->child_count == 0) continue;

        /* Check if the binding's RHS is/contains the net identifier. */
        const JZASTNode *rhs = bind->children[0];
        /* Handle simple identifier case. */
        if (rhs && rhs->type == JZ_AST_EXPR_IDENTIFIER && rhs->name &&
            strcmp(rhs->name, net_name) == 0) {
            child_port_name = bind->name;
            break;
        }
        /* Handle slice case - peel to base. */
        const JZASTNode *base = rhs;
        while (base && base->type == JZ_AST_EXPR_SLICE && base->child_count >= 1) {
            base = base->children[0];
        }
        if (base && base->type == JZ_AST_EXPR_IDENTIFIER && base->name &&
            strcmp(base->name, net_name) == 0) {
            child_port_name = bind->name;
            break;
        }
    }

    if (!child_port_name) return 0;

    /* Look up the child module. */
    const char *child_mod_name = inst_node->text;
    if (!child_mod_name) return 0;

    const JZModuleScope *child_scope = sem_find_scope_by_name(module_scopes, child_mod_name);
    if (!child_scope || !child_scope->node) return 0;

    /* Check if the port can produce non-z values in the child module. */
    return sem_port_can_drive_non_z_in_ast(child_scope->node, child_port_name);
}


/* Collapse whitespace from a substring and return a newly allocated identifier-
 * like string (used for MUX source lists). Returns NULL for empty results.
 */
static char *sem_mux_normalize_name_segment(const char *start, size_t len)
{
    if (!start || len == 0) return NULL;
    size_t out_len = 0;
    for (size_t i = 0; i < len; ++i) {
        if (!isspace((unsigned char)start[i])) {
            out_len++;
        }
    }
    if (out_len == 0) return NULL;

    char *buf = (char *)malloc(out_len + 1);
    if (!buf) return NULL;
    size_t pos = 0;
    for (size_t i = 0; i < len; ++i) {
        if (!isspace((unsigned char)start[i])) {
            buf[pos++] = start[i];
        }
    }
    buf[pos] = '\0';
    return buf;
}

/* Return non-zero if the given identifier name appears as a source in any
 * MUX aggregation or slice declaration within this module. This is used as a
 * conservative fallback to treat nets that are only used as MUX sources as
 * "used" for NET_DANGLING_UNUSED.
 */
static int sem_module_uses_name_in_mux(const JZModuleScope *scope,
                                       const char *name)
{
    if (!scope || !scope->node || !name) return 0;
    JZASTNode *mod = scope->node;

    for (size_t ci = 0; ci < mod->child_count; ++ci) {
        JZASTNode *blk = mod->children[ci];
        if (!blk || blk->type != JZ_AST_MUX_BLOCK) continue;

        for (size_t mi = 0; mi < blk->child_count; ++mi) {
            JZASTNode *mux = blk->children[mi];
            if (!mux || mux->type != JZ_AST_MUX_DECL || mux->child_count == 0) {
                continue;
            }
            JZASTNode *rhs = mux->children[0];
            if (!rhs || rhs->type != JZ_AST_RAW_TEXT || !rhs->text) {
                continue;
            }

            const char *p = rhs->text;
            while (*p) {
                const char *seg_start = p;
                const char *comma = strchr(p, ',');
                size_t seg_len = 0;
                if (comma) {
                    seg_len = (size_t)(comma - seg_start);
                    p = comma + 1;
                } else {
                    seg_len = strlen(seg_start);
                    p = seg_start + seg_len;
                }

                char *seg = sem_mux_normalize_name_segment(seg_start, seg_len);
                if (!seg) {
                    continue; /* empty or whitespace-only segment */
                }
                int match = (strcmp(seg, name) == 0);
                free(seg);
                if (match) {
                    return 1;
                }
            }
        }
    }

    return 0;
}

/* Return non-zero if any declaration in this net corresponds to the
 * destination alias identifier of a CDC entry in this module. CDC aliases
 * are modeled as logical wires fed by dedicated synchronizers, so they
 * should not trigger NET_FLOATING_WITH_SINK even if the flow graph does not
 * record explicit drivers for them.
 */
/* Check whether any atom in the net is the source register of a CDC declaration.
 * The CDC source (children[0]) logically sinks the register value into the
 * synchroniser, so the register should not be flagged as "never read".
 */
static int sem_net_is_cdc_source_reg(const JZModuleScope *scope,
                                     JZASTNode **atoms,
                                     size_t atom_count)
{
    if (!scope || !scope->node || !atoms || atom_count == 0) return 0;
    JZASTNode *mod = scope->node;

    for (size_t ci = 0; ci < mod->child_count; ++ci) {
        JZASTNode *blk = mod->children[ci];
        if (!blk || blk->type != JZ_AST_BLOCK || !blk->block_kind) continue;
        if (strcmp(blk->block_kind, "CDC") != 0) continue;

        for (size_t j = 0; j < blk->child_count; ++j) {
            JZASTNode *cdc = blk->children[j];
            if (!cdc || cdc->type != JZ_AST_CDC_DECL) continue;
            if (cdc->child_count < 4) continue;

            JZASTNode *src_id = cdc->children[0];
            if (!src_id || !src_id->name) continue;

            for (size_t ai = 0; ai < atom_count; ++ai) {
                if (atoms[ai] && atoms[ai]->name &&
                    strcmp(atoms[ai]->name, src_id->name) == 0) {
                    return 1;
                }
            }
        }
    }

    return 0;
}

static int sem_net_is_cdc_alias_net(const JZModuleScope *scope,
                                    JZASTNode **atoms,
                                    size_t atom_count)
{
    if (!scope || !scope->node || !atoms || atom_count == 0) return 0;
    JZASTNode *mod = scope->node;

    for (size_t ci = 0; ci < mod->child_count; ++ci) {
        JZASTNode *blk = mod->children[ci];
        if (!blk || blk->type != JZ_AST_BLOCK || !blk->block_kind) continue;
        if (strcmp(blk->block_kind, "CDC") != 0) continue;

        for (size_t j = 0; j < blk->child_count; ++j) {
            JZASTNode *cdc = blk->children[j];
            if (!cdc || cdc->type != JZ_AST_CDC_DECL) continue;
            if (cdc->child_count < 4) continue;

            JZASTNode *dst_id = cdc->children[2];
            if (!dst_id) continue;

            for (size_t ai = 0; ai < atom_count; ++ai) {
                if (atoms[ai] == dst_id) {
                    return 1;
                }
            }
        }
    }

    return 0;
}

/* -------------------------------------------------------------------------
 *  Per-field tristate check for BUS instance drivers.
 *
 *  When multiple module instances connect to the same bus wire via BUS ports,
 *  the whole-wire tristate check fails because different fields (ADDR, CMD,
 *  VALID, DATA, DONE, ...) are driven by different instances.  This function
 *  checks each bus field independently:
 *
 *    - Non-INOUT fields: at most one instance may produce non-z.
 *    - INOUT fields: first try the full tristate proof; if that fails,
 *      accept if every non-z driver is also z-capable (i.e., uses
 *      z-conditional driving).  The bus protocol ensures mutual exclusion
 *      at runtime; the linter verifies structural correctness.
 *
 *  Returns 1 if all fields pass, 0 otherwise.
 * -------------------------------------------------------------------------
 */
static int sem_tristate_check_bus_per_field(
    const JZNet *net,
    const char *net_name,
    const JZModuleScope *scope,
    const JZBuffer *module_scopes,
    const JZBuffer *project_symbols,
    JZASTNode **instance_drivers,
    size_t instance_driver_count)
{
    if (!net || !net_name || !module_scopes || !project_symbols ||
        instance_driver_count < 2) {
        return 0;
    }

    /* Verify all instance drivers connect via BUS ports and share the same
     * bus type. */
    char bus_name[128] = {0};

    for (size_t i = 0; i < instance_driver_count; ++i) {
        const JZASTNode *inst = instance_drivers[i];
        if (!inst || inst->type != JZ_AST_MODULE_INSTANCE) return 0;

        int found_bus = 0;
        for (size_t bi = 0; bi < inst->child_count; ++bi) {
            const JZASTNode *bind = inst->children[bi];
            if (!bind || bind->type != JZ_AST_PORT_DECL) continue;
            if (!bind->block_kind || strcmp(bind->block_kind, "BUS") != 0) continue;
            if (bind->child_count == 0) continue;

            /* Check if this BUS binding connects to our net. */
            const JZASTNode *rhs = bind->children[0];
            const JZASTNode *base = rhs;
            while (base && base->type == JZ_AST_EXPR_SLICE && base->child_count >= 1)
                base = base->children[0];
            if (!base || base->type != JZ_AST_EXPR_IDENTIFIER ||
                !base->name || strcmp(base->name, net_name) != 0) {
                continue;
            }

            /* Extract bus type name from text (e.g., "PARALLEL_BUS SOURCE"). */
            if (bind->text) {
                char this_bus[128] = {0};
                if (sscanf(bind->text, "%127s", this_bus) == 1) {
                    if (bus_name[0] == '\0') {
                        strncpy(bus_name, this_bus, sizeof(bus_name) - 1);
                    } else if (strcmp(bus_name, this_bus) != 0) {
                        return 0; /* Different bus types on same wire */
                    }
                }
            }
            found_bus = 1;
            break;
        }
        if (!found_bus) return 0;
    }

    if (bus_name[0] == '\0') return 0;

    /* Look up bus definition from project symbols. */
    const JZSymbol *syms = (const JZSymbol *)project_symbols->data;
    size_t sym_count = project_symbols->len / sizeof(JZSymbol);
    const JZASTNode *bus_def = NULL;

    for (size_t i = 0; i < sym_count; ++i) {
        if (syms[i].kind == JZ_SYM_BUS && syms[i].name &&
            strcmp(syms[i].name, bus_name) == 0 && syms[i].node) {
            bus_def = syms[i].node;
            break;
        }
    }
    if (!bus_def) return 0;

    /* Check each bus field independently. */
    for (size_t fi = 0; fi < bus_def->child_count; ++fi) {
        const JZASTNode *field = bus_def->children[fi];
        if (!field || field->type != JZ_AST_BUS_DECL ||
            !field->name || !field->block_kind) {
            continue;
        }

        int is_inout = (strcmp(field->block_kind, "INOUT") == 0);

        JZTristateNetInfo info;
        jz_tristate_analyze_net(&info, net, net_name, field->name,
                                scope, module_scopes);

        size_t driver_count = info.drivers.len / sizeof(JZTristateDriver);
        JZTristateDriver *drivers = (JZTristateDriver *)info.drivers.data;

        int field_safe = 0;

        if (is_inout) {
            /* For INOUT fields, try the full tristate proof first. */
            if (info.result == JZ_TRISTATE_PROVEN) {
                field_safe = 1;
            } else {
                /* Fallback: accept if every non-z driver is also z-capable.
                 * This verifies structural correctness (z-conditional driving)
                 * while the bus protocol handles runtime mutual exclusion. */
                field_safe = 1;
                for (size_t di = 0; di < driver_count; ++di) {
                    if (drivers[di].can_produce_non_z && !drivers[di].can_produce_z) {
                        field_safe = 0;
                        break;
                    }
                }
            }
        } else {
            /* For non-INOUT fields (OUT/IN), at most one driver may produce
             * non-z values.  SOURCE and TARGET drive complementary OUT fields,
             * so this naturally passes for properly typed bus instances. */
            size_t non_z = 0;
            for (size_t di = 0; di < driver_count; ++di) {
                if (drivers[di].can_produce_non_z) non_z++;
            }
            field_safe = (non_z <= 1);
        }

        jz_buf_free(&info.drivers);
        jz_buf_free(&info.sinks);

        if (!field_safe) return 0;
    }

    return 1;
}

static void sem_net_apply_simple_rules_for_module(const JZModuleScope *scope,
                                                   JZBuffer *nets,
                                                   JZBuffer *bindings,
                                                   const JZBuffer *module_scopes,
                                                   const JZBuffer *project_symbols,
                                                   JZDiagnosticList *diagnostics)
{
    (void)bindings;
    if (!scope || !scope->node || !nets) return;

    size_t net_count = nets->len / sizeof(JZNet);
    JZNet *arr = (JZNet *)nets->data;

    for (size_t i = 0; i < net_count; ++i) {
        JZNet *net = &arr[i];
        if (!net->atoms.data) continue;

        int has_driver = (net->driver_stmts.len > 0);
        int has_sink   = (net->sink_stmts.len > 0);

        JZASTNode **atoms = (JZASTNode **)net->atoms.data;
        size_t atom_count = net->atoms.len / sizeof(JZASTNode *);
        if (atom_count == 0 || !atoms[0]) continue;

        int is_cdc_alias_net = sem_net_is_cdc_alias_net(scope, atoms, atom_count);

        int has_input_port     = 0;
        int has_output_port    = 0;
        int has_inout_port     = 0;
        int has_register_atom  = 0;
        int has_wire_or_port   = 0;

        for (size_t j = 0; j < atom_count; ++j) {
            JZASTNode *decl = atoms[j];
            if (!decl) continue;

            if (decl->type == JZ_AST_REGISTER_DECL) {
                has_register_atom = 1;
            } else if (decl->type == JZ_AST_PORT_DECL) {
                has_wire_or_port = 1;
                if (decl->block_kind) {
                    if (strcmp(decl->block_kind, "IN") == 0) {
                        has_input_port = 1;
                    } else if (strcmp(decl->block_kind, "OUT") == 0) {
                        has_output_port = 1;
                    } else if (strcmp(decl->block_kind, "INOUT") == 0) {
                        has_inout_port = 1;
                    } else if (strcmp(decl->block_kind, "BUS") == 0) {
                        /* BUS ports have their own driver/sink semantics managed
                         * by the bus infrastructure. Treat them as having implicit
                         * external drivers to avoid spurious NET_FLOATING_WITH_SINK
                         * errors on bus signals.
                         */
                        has_input_port = 1;
                    }
                }
            } else if (decl->type == JZ_AST_WIRE_DECL) {
                has_wire_or_port = 1;
            }
        }

        /* Compute can_be_z for all atoms on this net. A net is tri-stateable if
         * it has any INOUT port or if any of its driver statements can assign 'z'
         * in the RHS expression tree.
         */
        int net_can_be_z = 0;
        if (has_inout_port) {
            net_can_be_z = 1;
        } else if (net->driver_stmts.len > 0) {
            JZASTNode **drv = (JZASTNode **)net->driver_stmts.data;
            size_t drv_count = net->driver_stmts.len / sizeof(JZASTNode *);
            for (size_t di = 0; di < drv_count; ++di) {
                if (sem_stmt_can_drive_z(drv[di])) {
                    net_can_be_z = 1;
                    break;
                }
            }
        }

        if (net_can_be_z) {
            size_t sym_count = scope->symbols.len / sizeof(JZSymbol);
            JZSymbol *syms = (JZSymbol *)scope->symbols.data;
            for (size_t j = 0; j < atom_count; ++j) {
                JZASTNode *decl = atoms[j];
                if (!decl) continue;
                for (size_t si = 0; si < sym_count; ++si) {
                    if (syms[si].node == decl) {
                        syms[si].can_be_z = 1;
                        break;
                    }
                }
            }

            /* Warn about internal tri-state usage (not tied to an INOUT port).
             * INOUT ports are expected to carry tri-state and may be connected
             * to project-level INOUT_PINS with physical bidirectional buffers.
             * Any other net using z is internal tri-state that is not
             * FPGA-compatible without --tristate-default.
             */
            if (!has_inout_port && !g_tristate_default_active) {
                sem_report_rule(diagnostics,
                                atoms[0]->loc,
                                "WARN_INTERNAL_TRISTATE",
                                "internal tri-state logic is not FPGA-compatible; "
                                "use --tristate-default");
            }
        }

        /* OUT ports: warn only if they are never driven inside this module.
         * "Driven" is determined first from the flow graph (has_driver), and
         * conservatively from the AST: if the identifier appears in any
         * assignment in this module, we also treat it as driven.
         */
        int treated_as_driven = has_driver;
        if (!treated_as_driven && has_output_port && scope && scope->node && atoms[0]->name) {
            if (sem_node_has_assignment_for_name(scope->node, atoms[0]->name)) {
                treated_as_driven = 1;
            }
        }

        if (has_output_port && !treated_as_driven) {
            sem_report_rule(diagnostics,
                            atoms[0]->loc,
                            "WARN_UNCONNECTED_OUTPUT",
                            "output port is never driven inside the design (unconnected output)");
        }

        /* General "unused" signal warning.
         *
         * A signal is considered "used" if it either feeds some sink
         * (has_sink) or is driven at least once inside the design. This
         * matches the intuition that even a write-only net is still
         * "participating" in the design, and avoids spurious
         * WARN_UNUSED_REGISTER on nets that are assigned but never
         * read (e.g. simple drive-only test patterns).
         */
        int is_used = has_sink;
        if (!is_used && (has_output_port || has_inout_port || has_wire_or_port) &&
            (treated_as_driven || has_driver)) {
            is_used = 1;
        }
        /* As an additional conservative fallback, if the identifier appears
         * in any assignment (LHS or RHS) anywhere in the module AST, treat the
         * signal as "used" even if flow tracking failed to record explicit
         * drivers/sinks (e.g. alias-style assignments that only merge nets).
         */
        if (!is_used && scope && scope->node && atoms[0]->name &&
            sem_node_has_assignment_for_name(scope->node, atoms[0]->name)) {
            is_used = 1;
        }

        /* Register-only unused signals: registers declared but never read or
         * written anywhere in the design. We restrict this to nets that contain
         * only REGISTER declarations (no PORT/WIRE atoms) so that nets shared
         * between a register and ports/wires continue to be handled by the
         * general net-usage rules below.
         */
        if (has_register_atom &&
            !has_wire_or_port && !has_output_port && !has_inout_port &&
            !has_input_port &&
            !has_driver && !has_sink) {
            sem_report_rule(diagnostics,
                            atoms[0]->loc,
                            "WARN_UNUSED_REGISTER",
                            "register is declared but its value is never read or written inside the design");
            continue;
        }

        /* Register written but never read: has at least one driver but no
         * sinks. Restrict to pure register nets (no port/wire atoms) to
         * avoid false positives on registers aliased to output ports.
         * Skip CDC source registers — the CDC synchroniser implicitly
         * sinks the register value.
         * Also skip registers used as MUX sources — reading from a MUX that
         * includes this register counts as an implicit sink.
         */
        if (has_register_atom &&
            !has_wire_or_port && !has_output_port && !has_inout_port &&
            !has_input_port &&
            has_driver && !has_sink &&
            !sem_net_is_cdc_source_reg(scope, atoms, atom_count) &&
            !sem_module_uses_name_in_mux(scope, atoms[0]->name)) {
            sem_report_rule(diagnostics,
                            atoms[0]->loc,
                            "WARN_UNSINKED_REGISTER",
                            "register is written but its value is never read");
            continue;
        }

        /* Register read but never written: has at least one sink but no
         * drivers. The register will hold its reset value forever.
         * Restrict to pure register nets to avoid false positives.
         */
        if (has_register_atom &&
            !has_wire_or_port && !has_output_port && !has_inout_port &&
            !has_input_port &&
            !has_driver && has_sink) {
            sem_report_rule(diagnostics,
                            atoms[0]->loc,
                            "WARN_UNDRIVEN_REGISTER",
                            "register is read but never written");
            continue;
        }

        if (!has_register_atom && !is_used &&
            (has_wire_or_port || has_output_port || has_inout_port) &&
            !has_input_port) {
            sem_report_rule(diagnostics,
                            atoms[0]->loc,
                            "WARN_UNUSED_REGISTER",
                            "signal is declared but its value is never used inside the design");
        }

        if (has_register_atom) {
            continue;
        }

        if (!has_driver && !has_sink) {
            const char *name = atoms[0]->name;
            if (scope && scope->node && name &&
                (sem_node_has_assignment_for_name(scope->node, name) ||
                 sem_module_uses_name_in_mux(scope, name))) {
                /* The identifier either appears in at least one assignment in
                 * this module or is used as a source in a MUX declaration, so
                 * treat it as conceptually used and do not report
                 * NET_DANGLING_UNUSED even if flow tracking failed to record
                 * drivers/sinks.
                 */
                continue;
            }

            sem_report_rule(diagnostics,
                            atoms[0]->loc,
                            "NET_DANGLING_UNUSED",
                            "net has no drivers and no sinks");
            continue;
        }

        /* Fallback using the AST: if flow analysis did not record any drivers
         * but the module AST clearly contains assignments to this identifier,
         * treat the net as driven and, where possible, classify those drivers
         * as z-only vs non-z. This prevents false NET_FLOATING_WITH_SINK on
         * ports/nets like `data = 8'bzzzz_zzzz; out_a = data;` when the flow
         * graph under-approximates drivers.
         */
        int ast_has_driver   = 0;
        int ast_all_z        = 0;
        int ast_any_non_z    = 0;
        if (!has_driver && atoms[0]->name && scope && scope->node) {
            sem_ast_driver_z_info_for_name(scope->node, atoms[0]->name,
                                           &ast_has_driver,
                                           &ast_all_z,
                                           &ast_any_non_z);
            if (ast_has_driver) {
                has_driver = 1;
            }
        }

        if (!has_driver && has_sink && !has_input_port && !has_inout_port && !is_cdc_alias_net) {
            sem_report_rule(diagnostics,
                            atoms[0]->loc,
                            "NET_FLOATING_WITH_SINK",
                            "net has sinks but no drivers inside design");
            continue;
        }

        /* For driver classification, we need to know if there are readers of
         * this net. OUT and INOUT ports have implicit external readers.
         */
        int effective_has_sink = has_sink || has_output_port || has_inout_port;

        if (has_driver) {
            int non_z_assign_driver_count = 0;
            int has_z_only_driver = 0;

            /* Get net name for instance driver analysis. */
            const char *net_name = atoms[0]->name;

            /* Collect instance drivers for pairwise comparison. */
            JZASTNode *instance_drivers[16];
            int instance_driver_can_z[16];
            size_t instance_driver_count = 0;

            JZASTNode **drv = (JZASTNode **)net->driver_stmts.data;
            size_t drv_count = net->driver_stmts.len / sizeof(JZASTNode *);
            for (size_t d = 0; d < drv_count; ++d) {
                JZASTNode *stmt = drv[d];
                if (!stmt) continue;

                if (stmt->type == JZ_AST_MODULE_INSTANCE) {
                    /* Module instance OUT port driver. */
                    if (instance_driver_count < 16) {
                        int can_z = (net_name && module_scopes &&
                                     sem_instance_driver_can_produce_z(stmt, net_name, module_scopes));
                        instance_drivers[instance_driver_count] = stmt;
                        instance_driver_can_z[instance_driver_count] = can_z;
                        instance_driver_count++;
                        if (can_z) {
                            has_z_only_driver = 1;
                        }
                    }
                    continue;
                }

                if (stmt->type != JZ_AST_STMT_ASSIGN || stmt->child_count < 2) {
                    /* Other non-assignment driver types. */
                    non_z_assign_driver_count++;
                    continue;
                }

                JZASTNode *rhs = stmt->children[1];
                if (sem_expr_is_all_z_literal(rhs)) {
                    has_z_only_driver = 1;
                } else {
                    non_z_assign_driver_count++;
                }
            }

            /* Merge in AST-based classification for INOUT nets. */
            if (ast_all_z) {
                has_z_only_driver = 1;
            }
            if (ast_any_non_z) {
                non_z_assign_driver_count++;
            }

            /* Check for multiple active drivers using the tristate proof engine.
             * This replaces the old inline check with a full analysis that
             * considers guard conditions, distinct constants, IF/ELSE branches,
             * and pairwise mutual exclusion proofs.
             */
            if (instance_driver_count > 1 && net_name && module_scopes) {
                if (!sem_tristate_check_net(net, net_name, scope, module_scopes)) {
                    /* Fallback: for BUS instance drivers, check per-field.
                     * Different bus fields (ADDR, CMD, DATA, ...) are driven by
                     * different instances (SOURCE vs TARGET), so the whole-wire
                     * check fails even though no individual field has a conflict.
                     */
                    int bus_ok = 0;
                    if (project_symbols) {
                        bus_ok = sem_tristate_check_bus_per_field(
                            net, net_name, scope, module_scopes,
                            project_symbols, instance_drivers,
                            instance_driver_count);
                    }
                    if (!bus_ok) {
                        sem_report_rule(diagnostics,
                                        atoms[0]->loc,
                                        "NET_MULTIPLE_ACTIVE_DRIVERS",
                                        "net has multiple active (non-z) drivers; tri-state requires all but one driver to assign z");
                    }
                }
            }

            /* Count instance drivers that can produce non-z for floating check.
             * An instance that uses `cond ? non_z : z` CAN produce z but also
             * CAN produce non_z, so it shouldn't trigger the floating check.
             */
            int instance_non_z_capable_count = 0;
            for (size_t ii = 0; ii < instance_driver_count; ++ii) {
                JZASTNode *inst = instance_drivers[ii];
                if (net_name && module_scopes &&
                    sem_instance_driver_can_produce_non_z(inst, net_name, module_scopes)) {
                    instance_non_z_capable_count++;
                }
            }
            int total_non_z = non_z_assign_driver_count + instance_non_z_capable_count;
            if (total_non_z == 0 && has_z_only_driver && effective_has_sink) {
                /* Report ASYNC_FLOATING_Z_READ at the point where the net is
                 * actually read, not at its declaration. This makes the
                 * diagnostic much more actionable (e.g. points at
                 *   out_a = data;  // read of fully released tri-state bus
                 * instead of the PORT/WIRE declaration).
                 */
                JZLocation loc = atoms[0]->loc;
                if (net->sink_stmts.len >= sizeof(JZASTNode *)) {
                    JZASTNode **snk = (JZASTNode **)net->sink_stmts.data;
                    if (snk[0]) {
                        loc = snk[0]->loc;
                    }
                }

                sem_report_rule(diagnostics,
                                loc,
                                "ASYNC_FLOATING_Z_READ",
                                "net has sinks but all drivers assign 'z' (tri-state bus fully released while read)");
            }
        }
    }
}


static void sem_net_collect_instance_usage(JZASTNode *inst_node,
                                           const JZModuleScope *scope,
                                           JZBuffer *nets,
                                           JZBuffer *bindings,
                                           const JZBuffer *project_symbols)
{
    if (!inst_node || inst_node->type != JZ_AST_MODULE_INSTANCE) return;

    for (size_t bi = 0; bi < inst_node->child_count; ++bi) {
        JZASTNode *bind = inst_node->children[bi];
        if (!bind || bind->type != JZ_AST_PORT_DECL || !bind->block_kind) continue;
        if (bind->child_count == 0) continue;
        JZASTNode *rhs = bind->children[0];
        const char *dir = bind->block_kind;
        int is_out   = (strcmp(dir, "OUT") == 0);
        int is_inout = (strcmp(dir, "INOUT") == 0);
        int is_in    = (strcmp(dir, "IN") == 0);
        int is_bus   = (strcmp(dir, "BUS") == 0);

        if (is_bus) {
            int readable = 0;
            int writable = 0;
            if (sem_bus_binding_access_dirs(bind, project_symbols, &readable, &writable)) {
                if (writable) {
                    sem_net_mark_lvalue_drivers(rhs, scope, nets, bindings, inst_node);
                }
                if (readable) {
                    sem_net_mark_expr_sinks(rhs, scope, nets, bindings, inst_node);
                }
            }
            continue;
        }

        if (is_out || is_inout) {
            sem_net_mark_lvalue_drivers(rhs, scope, nets, bindings, inst_node);
        }
        if (is_in) {
            sem_net_mark_expr_sinks(rhs, scope, nets, bindings, inst_node);
        }
    }
}

void sem_build_net_graphs(JZASTNode *root,
                          JZBuffer *module_scopes,
                          const JZBuffer *project_symbols,
                          JZDiagnosticList *diagnostics)
{
    if (!root || root->type != JZ_AST_PROJECT || !module_scopes) return;

    /* Set project symbols context for tristate analysis so that qualified
     * identifiers (e.g., DEV.ROM) can be resolved to their literal values.
     */
    jz_tristate_set_project_symbols(project_symbols);

    size_t scope_count = module_scopes->len / sizeof(JZModuleScope);
    JZModuleScope *scopes = (JZModuleScope *)module_scopes->data;

    for (size_t i = 0; i < scope_count; ++i) {
        const JZModuleScope *scope = &scopes[i];
        if (!scope->node) continue;
        /* Blackboxes describe only an external interface signature. They do not
         * contain executable logic inside this project, so net usage and
         * combinational loop checks are not meaningful for them. Skipping them
         * here prevents spurious NET_DANGLING_UNUSED and related diagnostics on
         * blackbox ports.
         */
        if (scope->node->type == JZ_AST_BLACKBOX) {
            continue;
        }

        /* Set parent scope context for tristate analysis so that module CONST
         * values (e.g., DEV_A) can be resolved to their literal values.
         */
        jz_tristate_set_parent_scope(scope);

        JZBuffer nets = (JZBuffer){0};
        JZBuffer bindings = (JZBuffer){0};

        if (sem_net_build_initial_for_module(scope, &nets, &bindings) != 0) {
            jz_buf_free(&nets);
            jz_buf_free(&bindings);
            continue;
        }

        JZASTNode *mod = scope->node;
        for (size_t ci = 0; ci < mod->child_count; ++ci) {
            JZASTNode *child = mod->children[ci];
            if (!child || child->type != JZ_AST_BLOCK || !child->block_kind) continue;
            int is_async = (strcmp(child->block_kind, "ASYNCHRONOUS") == 0);
            int is_sync  = (strcmp(child->block_kind, "SYNCHRONOUS") == 0);
            if (!is_async && !is_sync) continue;

            sem_net_collect_aliases_in_block(child, scope, &nets, &bindings, is_sync);
        }

        for (size_t ci = 0; ci < mod->child_count; ++ci) {
            JZASTNode *child = mod->children[ci];
            if (!child || child->type != JZ_AST_BLOCK || !child->block_kind) continue;
            int is_async = (strcmp(child->block_kind, "ASYNCHRONOUS") == 0);
            int is_sync  = (strcmp(child->block_kind, "SYNCHRONOUS") == 0);
            if (!is_async && !is_sync) continue;

            sem_net_collect_usage_in_block(child, scope, &nets, &bindings, is_sync);
        }

        /* Treat CDC clock expressions as sinks so that CDC clocks are
         * considered "used" for net diagnostics (e.g. NET_DANGLING_UNUSED),
         * matching the treatment of clocks referenced by SYNCHRONOUS(CLK=...).
         */
        for (size_t ci = 0; ci < mod->child_count; ++ci) {
            JZASTNode *blk = mod->children[ci];
            if (!blk || blk->type != JZ_AST_BLOCK || !blk->block_kind) continue;
            if (strcmp(blk->block_kind, "CDC") != 0) continue;

            for (size_t j = 0; j < blk->child_count; ++j) {
                JZASTNode *cdc = blk->children[j];
                if (!cdc || cdc->type != JZ_AST_CDC_DECL) continue;
                if (cdc->child_count < 4) continue;

                JZASTNode *src_clk = cdc->children[1];
                JZASTNode *dst_clk = cdc->children[3];

                if (src_clk) {
                    sem_net_mark_expr_sinks(src_clk, scope, &nets, &bindings, NULL);
                }
                if (dst_clk) {
                    sem_net_mark_expr_sinks(dst_clk, scope, &nets, &bindings, NULL);
                }
            }
        }

        /* Collect net usage from module instances (including those inside
         * @feature guards). Instances inside feature guards are walked for
         * BOTH branches so that net analysis stays consistent with the
         * declaration scanning (which also registers both branches).
         */
        for (size_t ci = 0; ci < mod->child_count; ++ci) {
            JZASTNode *child = mod->children[ci];
            if (!child) continue;

            /* Collect instances from a feature guard's branches. */
            if (child->type == JZ_AST_FEATURE_GUARD) {
                for (size_t fi = 1; fi < child->child_count; ++fi) {
                    JZASTNode *branch = child->children[fi];
                    if (!branch) continue;
                    for (size_t gi = 0; gi < branch->child_count; ++gi) {
                        JZASTNode *inst_node = branch->children[gi];
                        if (!inst_node || inst_node->type != JZ_AST_MODULE_INSTANCE) continue;
                        sem_net_collect_instance_usage(inst_node, scope, &nets, &bindings, project_symbols);
                    }
                }
                continue;
            }

            if (child->type != JZ_AST_MODULE_INSTANCE) continue;
            sem_net_collect_instance_usage(child, scope, &nets, &bindings, project_symbols);
        }

        sem_net_apply_simple_rules_for_module(scope, &nets, &bindings, module_scopes, project_symbols, diagnostics);
        sem_check_combinational_loops_for_module(scope, &nets, &bindings, diagnostics);

        /* Always delegate to alias-report module; it is a no-op when
         * alias reporting is not enabled.
         */
        sem_emit_alias_report_for_module(scope, &nets, module_scopes, project_symbols, root);

        /* Always delegate to tristate-report module; it is a no-op when
         * tristate reporting is not enabled.
         */
        sem_emit_tristate_report_for_module(scope, &nets, module_scopes, project_symbols, root);

        size_t net_count = nets.len / sizeof(JZNet);
        JZNet *net_arr = (JZNet *)nets.data;
        for (size_t ni = 0; ni < net_count; ++ni) {
            sem_net_free(&net_arr[ni]);
        }
        jz_buf_free(&nets);
        jz_buf_free(&bindings);
    }

    sem_emit_alias_report_finalize();
    sem_emit_tristate_report_finalize();
}
