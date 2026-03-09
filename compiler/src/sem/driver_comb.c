#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

#include "sem_driver.h"
#include "sem.h"
#include "util.h"
#include "rules.h"
#include "driver_internal.h"

/* -------------------------------------------------------------------------
 *  Combinational loop detection (COMB_LOOP_*)
 * -------------------------------------------------------------------------
 */

typedef struct JZCombEdge {
    size_t src_net_ix;
    size_t dst_net_ix;
} JZCombEdge;

typedef struct JZCombPathState {
    JZBuffer edges; /* array of JZCombEdge */
} JZCombPathState;

static void sem_comb_free_paths(JZBuffer *paths)
{
    if (!paths) return;
    size_t count = paths->len / sizeof(JZCombPathState);
    JZCombPathState *arr = (JZCombPathState *)paths->data;
    for (size_t i = 0; i < count; ++i) {
        jz_buf_free(&arr[i].edges);
    }
    jz_buf_free(paths);
}

static int sem_comb_clone_path_state(const JZCombPathState *src,
                                     JZCombPathState *dst)
{
    if (!dst) return -1;
    memset(dst, 0, sizeof(*dst));
    if (!src) return 0;
    if (src->edges.len == 0) return 0;
    if (jz_buf_append(&dst->edges, src->edges.data, src->edges.len) != 0) {
        jz_buf_free(&dst->edges);
        return -1;
    }
    return 0;
}

void sem_comb_collect_targets_from_lhs(JZASTNode *lhs,
                                              const JZModuleScope *scope,
                                              JZBuffer *out_decls)
{
    if (!lhs || !scope || !out_decls) return;

    switch (lhs->type) {
    case JZ_AST_EXPR_CONCAT:
        for (size_t i = 0; i < lhs->child_count; ++i) {
            sem_comb_collect_targets_from_lhs(lhs->children[i], scope, out_decls);
        }
        break;

    case JZ_AST_EXPR_SLICE:
        if (lhs->child_count >= 1) {
            JZASTNode *base = lhs->children[0];
            if (base && base->type == JZ_AST_EXPR_IDENTIFIER && base->name) {
                const JZSymbol *sym = module_scope_lookup(scope, base->name);
                if (sym && sym->node &&
                    (sym->kind == JZ_SYM_PORT || sym->kind == JZ_SYM_WIRE ||
                     sym->kind == JZ_SYM_REGISTER || sym->kind == JZ_SYM_LATCH)) {
                    (void)jz_buf_append(out_decls, &sym->node, sizeof(JZASTNode *));
                }
            }
        }
        break;

    case JZ_AST_EXPR_IDENTIFIER:
        if (lhs->name) {
            const JZSymbol *sym = module_scope_lookup(scope, lhs->name);
            if (sym && sym->node &&
                (sym->kind == JZ_SYM_PORT || sym->kind == JZ_SYM_WIRE || sym->kind == JZ_SYM_REGISTER)) {
                (void)jz_buf_append(out_decls, &sym->node, sizeof(JZASTNode *));
            }
        }
        break;

    default:
        break;
    }
}

void sem_comb_collect_sources_from_expr(JZASTNode *expr,
                                               const JZModuleScope *scope,
                                               JZBuffer *out_decls)
{
    if (!expr || !scope || !out_decls) return;

    switch (expr->type) {
    case JZ_AST_EXPR_IDENTIFIER:
        if (expr->name) {
            const JZSymbol *sym = module_scope_lookup(scope, expr->name);
            if (sym && sym->node &&
                (sym->kind == JZ_SYM_PORT || sym->kind == JZ_SYM_WIRE || sym->kind == JZ_SYM_REGISTER)) {
                (void)jz_buf_append(out_decls, &sym->node, sizeof(JZASTNode *));
            }
        }
        break;

    case JZ_AST_EXPR_BUS_ACCESS:
    case JZ_AST_EXPR_QUALIFIED_IDENTIFIER: {
        /* Resolve BUS port declarations from qualified names like "pbus.DATA"
         * so that alias assignments with bus-field sources are tracked.
         */
        const char *port_name = NULL;
        char buf[256];
        if (expr->type == JZ_AST_EXPR_BUS_ACCESS) {
            port_name = expr->name;
        } else if (expr->name) {
            const char *dot = strchr(expr->name, '.');
            if (dot && dot != expr->name) {
                size_t len = (size_t)(dot - expr->name);
                if (len > 0 && len < sizeof(buf)) {
                    memcpy(buf, expr->name, len);
                    buf[len] = '\0';
                    port_name = buf;
                }
            }
        }
        if (port_name) {
            const JZSymbol *sym = module_scope_lookup(scope, port_name);
            if (sym && sym->node &&
                (sym->kind == JZ_SYM_PORT || sym->kind == JZ_SYM_WIRE || sym->kind == JZ_SYM_REGISTER)) {
                (void)jz_buf_append(out_decls, &sym->node, sizeof(JZASTNode *));
            }
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
            sem_comb_collect_sources_from_expr(expr->children[i], scope, out_decls);
        }
        break;

    default:
        break;
    }
}

static void sem_comb_record_edges_for_assign(JZASTNode *stmt,
                                             const JZModuleScope *scope,
                                             JZBuffer *nets,
                                             JZBuffer *bindings,
                                             JZBuffer *paths,
                                             int *had_unconditional_cycle,
                                             JZDiagnosticList *diagnostics)
{
    (void)had_unconditional_cycle;
    (void)diagnostics;
    if (!stmt || !scope || !nets || !bindings || !paths) return;
    if (stmt->child_count < 2) return;

    JZASTNode *lhs = stmt->children[0];
    JZASTNode *rhs = stmt->children[1];

    const char *op = stmt->block_kind ? stmt->block_kind : "";
    int is_alias   = (strcmp(op, "ALIAS") == 0 ||
                      strcmp(op, "ALIAS_Z") == 0 ||
                      strcmp(op, "ALIAS_S") == 0);

    if (is_alias) return;

    int is_drive   = (strncmp(op, "DRIVE", 5) == 0);
    int is_receive = (strncmp(op, "RECEIVE", 7) == 0);

    JZBuffer lhs_decls = {0};
    JZBuffer rhs_decls = {0};
    sem_comb_collect_targets_from_lhs(lhs, scope, &lhs_decls);
    sem_comb_collect_sources_from_expr(rhs, scope, &rhs_decls);

    size_t lhs_count = lhs_decls.len / sizeof(JZASTNode *);
    size_t rhs_count = rhs_decls.len / sizeof(JZASTNode *);

    if (lhs_count == 0 || rhs_count == 0) {
        jz_buf_free(&lhs_decls);
        jz_buf_free(&rhs_decls);
        return;
    }

    size_t net_count = nets->len / sizeof(JZNet);

    JZASTNode **lhs_nodes = (JZASTNode **)lhs_decls.data;
    JZASTNode **rhs_nodes = (JZASTNode **)rhs_decls.data;

    size_t path_count = paths->len / sizeof(JZCombPathState);
    JZCombPathState *path_arr = (JZCombPathState *)paths->data;

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

            size_t src_net_ix = 0;
            size_t dst_net_ix = 0;

            if (is_drive) {
                src_net_ix = lhs_bind->net_ix;
                dst_net_ix = rhs_bind->net_ix;
            } else if (is_receive) {
                src_net_ix = rhs_bind->net_ix;
                dst_net_ix = lhs_bind->net_ix;
            } else {
                src_net_ix = rhs_bind->net_ix;
                dst_net_ix = lhs_bind->net_ix;
            }

            if (src_net_ix >= net_count || dst_net_ix >= net_count) continue;

            for (size_t pi = 0; pi < path_count; ++pi) {
                JZCombPathState *ps = &path_arr[pi];
                JZCombEdge edge;
                edge.src_net_ix = src_net_ix;
                edge.dst_net_ix = dst_net_ix;
                (void)jz_buf_append(&ps->edges, &edge, sizeof(edge));
            }
        }
    }

    jz_buf_free(&lhs_decls);
    jz_buf_free(&rhs_decls);
}

static void sem_comb_analyze_stmt(JZASTNode *stmt,
                                  const JZModuleScope *scope,
                                  JZBuffer *nets,
                                  JZBuffer *bindings,
                                  JZBuffer *paths,
                                  int *had_unconditional_cycle,
                                  JZDiagnosticList *diagnostics);

static void sem_comb_analyze_if_branch_body(JZASTNode *stmt,
                                            const JZModuleScope *scope,
                                            JZBuffer *nets,
                                            JZBuffer *bindings,
                                            JZBuffer *paths,
                                            int *had_unconditional_cycle,
                                            JZDiagnosticList *diagnostics)
{
    if (!stmt || !paths) return;

    if (stmt->type == JZ_AST_STMT_IF || stmt->type == JZ_AST_STMT_ELIF) {
        for (size_t j = 1; j < stmt->child_count; ++j) {
            JZASTNode *body = stmt->children[j];
            sem_comb_analyze_stmt(body, scope, nets, bindings, paths, had_unconditional_cycle, diagnostics);
        }
    } else if (stmt->type == JZ_AST_STMT_ELSE) {
        for (size_t j = 0; j < stmt->child_count; ++j) {
            JZASTNode *body = stmt->children[j];
            sem_comb_analyze_stmt(body, scope, nets, bindings, paths, had_unconditional_cycle, diagnostics);
        }
    }
}

/* Maximum number of tracked execution paths before collapsing into a single
   union.  Prevents exponential blowup from deeply-nested IF/SELECT chains. */
#define COMB_MAX_PATHS 256

/* Collapse all paths into a single path containing the deduplicated union of
 * all edges.  Without dedup, template-expanded IF/ELSE chains can accumulate
 * millions of duplicate (src, dst) pairs, making the downstream DFS extremely
 * slow.  With N nets the unique edge count is at most N², so dedup keeps the
 * graph small regardless of path count.
 */
static void sem_comb_collapse_paths(JZBuffer *paths)
{
    if (!paths) return;
    size_t path_count = paths->len / sizeof(JZCombPathState);
    if (path_count <= 1) return;

    JZCombPathState *arr = (JZCombPathState *)paths->data;
    /* Merge all edges into path 0 */
    for (size_t i = 1; i < path_count; ++i) {
        if (arr[i].edges.len > 0) {
            (void)jz_buf_append(&arr[0].edges, arr[i].edges.data, arr[i].edges.len);
        }
        jz_buf_free(&arr[i].edges);
    }

    /* Deduplicate edges in the merged path.  Since net indices are small
     * (typically < 1000), we use a simple seen-set based on (src, dst) pairs.
     * For very large net counts, fall back to a hash-based approach.
     */
    size_t edge_count = arr[0].edges.len / sizeof(JZCombEdge);
    if (edge_count > 1) {
        JZCombEdge *edges = (JZCombEdge *)arr[0].edges.data;

        /* Find max net index to size the seen bitmap */
        size_t max_net = 0;
        for (size_t i = 0; i < edge_count; ++i) {
            if (edges[i].src_net_ix > max_net) max_net = edges[i].src_net_ix;
            if (edges[i].dst_net_ix > max_net) max_net = edges[i].dst_net_ix;
        }
        size_t n = max_net + 1;

        if (n <= 4096) {
            /* Bitmap approach: one bit per (src, dst) pair */
            size_t bitmap_size = (n * n + 7) / 8;
            unsigned char *seen = (unsigned char *)calloc(bitmap_size, 1);
            if (seen) {
                size_t out = 0;
                for (size_t i = 0; i < edge_count; ++i) {
                    size_t key = edges[i].src_net_ix * n + edges[i].dst_net_ix;
                    size_t byte_ix = key / 8;
                    unsigned char bit = (unsigned char)(1u << (key % 8));
                    if (!(seen[byte_ix] & bit)) {
                        seen[byte_ix] |= bit;
                        edges[out++] = edges[i];
                    }
                }
                arr[0].edges.len = out * sizeof(JZCombEdge);
                free(seen);
            }
        } else {
            /* For large net counts, sort and compact */
            /* Simple insertion-sort-style dedup (rare case) */
            size_t out = 0;
            for (size_t i = 0; i < edge_count; ++i) {
                int dup = 0;
                for (size_t j = 0; j < out; ++j) {
                    if (edges[j].src_net_ix == edges[i].src_net_ix &&
                        edges[j].dst_net_ix == edges[i].dst_net_ix) {
                        dup = 1;
                        break;
                    }
                }
                if (!dup) edges[out++] = edges[i];
            }
            arr[0].edges.len = out * sizeof(JZCombEdge);
        }
    }

    paths->len = sizeof(JZCombPathState);
}

static void sem_comb_analyze_if_chain(JZASTNode *block,
                                      size_t start_index,
                                      size_t end_index,
                                      const JZModuleScope *scope,
                                      JZBuffer *nets,
                                      JZBuffer *bindings,
                                      JZBuffer *paths,
                                      int *had_unconditional_cycle,
                                      JZDiagnosticList *diagnostics)
{
    if (!block || !scope || !paths) return;
    if (end_index <= start_index) return;

    size_t branch_count = end_index - start_index;
    int has_else = 0;
    JZASTNode *last = block->children[end_index - 1];
    if (last && last->type == JZ_AST_STMT_ELSE) {
        has_else = 1;
    }

    JZBuffer new_paths = (JZBuffer){0};

    size_t path_count = paths->len / sizeof(JZCombPathState);
    JZCombPathState *base_arr = (JZCombPathState *)paths->data;

    for (size_t pi = 0; pi < path_count; ++pi) {
        JZCombPathState *base = &base_arr[pi];

        for (size_t bi = 0; bi < branch_count; ++bi) {
            JZASTNode *stmt = block->children[start_index + bi];
            int reuse_base = (has_else && bi == branch_count - 1);

            if (reuse_base) {
                JZBuffer branch_paths = (JZBuffer){0};
                (void)jz_buf_append(&branch_paths, base, sizeof(JZCombPathState));
                sem_comb_analyze_if_branch_body(stmt, scope, nets, bindings, &branch_paths, had_unconditional_cycle, diagnostics);
                (void)jz_buf_append(&new_paths, branch_paths.data, branch_paths.len);
                jz_buf_free(&branch_paths);
            } else {
                JZCombPathState clone;
                if (sem_comb_clone_path_state(base, &clone) != 0) {
                    continue;
                }
                JZBuffer branch_paths = (JZBuffer){0};
                (void)jz_buf_append(&branch_paths, &clone, sizeof(JZCombPathState));
                sem_comb_analyze_if_branch_body(stmt, scope, nets, bindings, &branch_paths, had_unconditional_cycle, diagnostics);
                (void)jz_buf_append(&new_paths, branch_paths.data, branch_paths.len);
                jz_buf_free(&branch_paths);
            }
        }

        if (!has_else) {
            (void)jz_buf_append(&new_paths, base, sizeof(JZCombPathState));
        }
    }

    jz_buf_free(paths);
    *paths = new_paths;

    /* Prevent exponential path explosion */
    if (paths->len / sizeof(JZCombPathState) > COMB_MAX_PATHS) {
        sem_comb_collapse_paths(paths);
    }
}

static void sem_comb_analyze_select(JZASTNode *select_stmt,
                                    const JZModuleScope *scope,
                                    JZBuffer *nets,
                                    JZBuffer *bindings,
                                    JZBuffer *paths,
                                    int *had_unconditional_cycle,
                                    JZDiagnosticList *diagnostics)
{
    if (!select_stmt || !paths) return;
    if (select_stmt->child_count < 2) return;

    JZBuffer new_paths = (JZBuffer){0};

    size_t path_count = paths->len / sizeof(JZCombPathState);
    JZCombPathState *base_arr = (JZCombPathState *)paths->data;

    size_t case_start = 1;
    size_t case_end = select_stmt->child_count;

    /* Check if the SELECT has a DEFAULT case -- if so, all values are covered
       and we should NOT append the base path (same logic as IF with ELSE). */
    int has_default = 0;
    for (size_t ci = case_start; ci < case_end; ++ci) {
        JZASTNode *case_node = select_stmt->children[ci];
        if (case_node && case_node->type == JZ_AST_STMT_DEFAULT) {
            has_default = 1;
            break;
        }
    }

    for (size_t pi = 0; pi < path_count; ++pi) {
        JZCombPathState *base = &base_arr[pi];

        for (size_t ci = case_start; ci < case_end; ++ci) {
            JZASTNode *case_node = select_stmt->children[ci];
            if (!case_node) continue;

            size_t body_start = (case_node->type == JZ_AST_STMT_CASE) ? 1u : 0u;
            if (case_node->child_count <= body_start) {
                continue;
            }

            JZCombPathState clone;
            if (sem_comb_clone_path_state(base, &clone) != 0) {
                continue;
            }

            JZBuffer branch_paths = (JZBuffer){0};
            (void)jz_buf_append(&branch_paths, &clone, sizeof(JZCombPathState));

            for (size_t bj = body_start; bj < case_node->child_count; ++bj) {
                JZASTNode *body = case_node->children[bj];
                sem_comb_analyze_stmt(body, scope, nets, bindings, &branch_paths, had_unconditional_cycle, diagnostics);
            }

            (void)jz_buf_append(&new_paths, branch_paths.data, branch_paths.len);
            jz_buf_free(&branch_paths);
        }

        if (!has_default) {
            (void)jz_buf_append(&new_paths, base, sizeof(JZCombPathState));
        }
    }

    jz_buf_free(paths);
    *paths = new_paths;

    /* Prevent exponential path explosion */
    if (paths->len / sizeof(JZCombPathState) > COMB_MAX_PATHS) {
        sem_comb_collapse_paths(paths);
    }
}

static void sem_comb_analyze_stmt(JZASTNode *stmt,
                                  const JZModuleScope *scope,
                                  JZBuffer *nets,
                                  JZBuffer *bindings,
                                  JZBuffer *paths,
                                  int *had_unconditional_cycle,
                                  JZDiagnosticList *diagnostics)
{
    if (!stmt || !scope || !paths) return;

    switch (stmt->type) {
    case JZ_AST_STMT_ASSIGN:
        sem_comb_record_edges_for_assign(stmt, scope, nets, bindings, paths,
                                         had_unconditional_cycle,
                                         diagnostics);
        break;

    case JZ_AST_STMT_IF: {
        JZASTNode fake_block;
        memset(&fake_block, 0, sizeof(fake_block));
        fake_block.type = JZ_AST_BLOCK;
        fake_block.children = &stmt;
        fake_block.child_count = 1;
        sem_comb_analyze_if_chain(&fake_block, 0, 1, scope, nets, bindings, paths,
                                  had_unconditional_cycle,
                                  diagnostics);
        break;
    }

    case JZ_AST_STMT_ELIF:
    case JZ_AST_STMT_ELSE:
        break;

    case JZ_AST_STMT_SELECT:
        sem_comb_analyze_select(stmt, scope, nets, bindings, paths,
                                had_unconditional_cycle,
                                diagnostics);
        break;

    case JZ_AST_BLOCK:
        for (size_t i = 0; i < stmt->child_count; ++i) {
            JZASTNode *child = stmt->children[i];
            if (!child) continue;
            sem_comb_analyze_stmt(child, scope, nets, bindings, paths,
                                   had_unconditional_cycle,
                                   diagnostics);
        }
        break;

    default:
        break;
    }
}

/* Check if a set of edges contains a cycle using O(V+E) DFS with adjacency lists */
static int sem_comb_edgeset_has_cycle(const JZCombEdge *edges,
                                       size_t edge_count,
                                       size_t net_count)
{
    if (!edges || edge_count == 0 || net_count == 0) return 0;

    /* Build CSR-style adjacency: count neighbors, then fill */
    size_t *adj_count = (size_t *)calloc(net_count, sizeof(size_t));
    if (!adj_count) return 0;
    for (size_t ei = 0; ei < edge_count; ++ei) {
        if (edges[ei].src_net_ix < net_count) adj_count[edges[ei].src_net_ix]++;
    }
    size_t *adj_offset = (size_t *)calloc(net_count + 1, sizeof(size_t));
    if (!adj_offset) { free(adj_count); return 0; }
    for (size_t i = 0; i < net_count; ++i) adj_offset[i + 1] = adj_offset[i] + adj_count[i];
    size_t *adj_dst = (size_t *)malloc(edge_count * sizeof(size_t));
    if (!adj_dst) { free(adj_offset); free(adj_count); return 0; }
    memset(adj_count, 0, net_count * sizeof(size_t));
    for (size_t ei = 0; ei < edge_count; ++ei) {
        size_t s = edges[ei].src_net_ix;
        if (s < net_count) {
            adj_dst[adj_offset[s] + adj_count[s]] = edges[ei].dst_net_ix;
            adj_count[s]++;
        }
    }

    /* DFS with 3-color marking: 0=white, 1=gray (on stack), 2=black (done) */
    int *color = (int *)calloc(net_count, sizeof(int));
    size_t *stack = (size_t *)malloc(net_count * sizeof(size_t));
    int has_cycle = 0;
    if (color && stack) {
        for (size_t start = 0; start < net_count && !has_cycle; ++start) {
            if (color[start] != 0) continue;
            if (adj_offset[start + 1] == adj_offset[start]) { color[start] = 2; continue; }
            size_t sp = 0;
            stack[sp++] = start;
            color[start] = 1;
            while (sp > 0 && !has_cycle) {
                size_t v = stack[sp - 1];
                int pushed = 0;
                for (size_t ni = adj_offset[v]; ni < adj_offset[v + 1]; ++ni) {
                    size_t w = adj_dst[ni];
                    if (w >= net_count) continue;
                    if (color[w] == 1) { has_cycle = 1; break; }
                    if (color[w] == 0) {
                        color[w] = 1;
                        stack[sp++] = w;
                        pushed = 1;
                        break;
                    }
                }
                if (!pushed && !has_cycle) { color[v] = 2; sp--; }
            }
        }
    }
    free(stack);
    free(color);
    free(adj_dst);
    free(adj_offset);
    free(adj_count);
    return has_cycle;
}

static int sem_comb_union_has_cycle(const JZBuffer *paths,
                                     size_t net_count)
{
    if (!paths || net_count == 0) return 0;

    JZBuffer all_edges = (JZBuffer){0};
    size_t path_count = paths->len / sizeof(JZCombPathState);
    const JZCombPathState *path_arr = (const JZCombPathState *)paths->data;
    for (size_t pi = 0; pi < path_count; ++pi) {
        const JZCombPathState *ps = &path_arr[pi];
        if (ps->edges.len == 0) continue;
        (void)jz_buf_append(&all_edges, ps->edges.data, ps->edges.len);
    }

    size_t edge_count = all_edges.len / sizeof(JZCombEdge);
    int result = sem_comb_edgeset_has_cycle(
        (const JZCombEdge *)all_edges.data, edge_count, net_count);
    jz_buf_free(&all_edges);
    return result;
}

/* Check each individual path for cycles. Returns 1 if ANY path has a cycle. */
static int sem_comb_any_path_has_cycle(const JZBuffer *paths,
                                        size_t net_count)
{
    if (!paths || net_count == 0) return 0;
    size_t path_count = paths->len / sizeof(JZCombPathState);
    const JZCombPathState *path_arr = (const JZCombPathState *)paths->data;
    for (size_t pi = 0; pi < path_count; ++pi) {
        const JZCombPathState *ps = &path_arr[pi];
        size_t edge_count = ps->edges.len / sizeof(JZCombEdge);
        if (edge_count == 0) continue;
        if (sem_comb_edgeset_has_cycle(
                (const JZCombEdge *)ps->edges.data, edge_count, net_count)) {
            return 1;
        }
    }
    return 0;
}

void sem_check_combinational_loops_for_module(const JZModuleScope *scope,
                                                     JZBuffer *nets,
                                                     JZBuffer *bindings,
                                                     JZDiagnosticList *diagnostics)
{
    if (!scope || !scope->node) return;
    JZASTNode *mod = scope->node;

    for (size_t ci = 0; ci < mod->child_count; ++ci) {
        JZASTNode *child = mod->children[ci];
        if (!child || child->type != JZ_AST_BLOCK || !child->block_kind) continue;
        if (strcmp(child->block_kind, "ASYNCHRONOUS") != 0) continue;

        JZBuffer paths = (JZBuffer){0};
        JZCombPathState initial = (JZCombPathState){0};
        (void)jz_buf_append(&paths, &initial, sizeof(initial));

        int had_unconditional_cycle = 0;

        for (size_t i = 0; i < child->child_count; ++i) {
            JZASTNode *stmt = child->children[i];
            if (!stmt) continue;

            if (stmt->type == JZ_AST_STMT_IF) {
                size_t start = i;
                size_t end = i + 1;
                while (end < child->child_count) {
                    JZASTNode *next = child->children[end];
                    if (!next) {
                        end++;
                        continue;
                    }
                    if (next->type == JZ_AST_STMT_ELIF || next->type == JZ_AST_STMT_ELSE) {
                        end++;
                        if (next->type == JZ_AST_STMT_ELSE) {
                            break;
                        }
                    } else {
                        break;
                    }
                }
                sem_comb_analyze_if_chain(child, start, end, scope, nets, bindings,
                                          &paths,
                                          &had_unconditional_cycle,
                                          diagnostics);
                i = end - 1;
            } else {
                sem_comb_analyze_stmt(stmt, scope, nets, bindings, &paths,
                                       &had_unconditional_cycle,
                                       diagnostics);
            }
        }

        {
            size_t net_count = nets->len / sizeof(JZNet);
            if (sem_comb_any_path_has_cycle(&paths, net_count)) {
                const char *mod_name = (mod->name) ? mod->name : "?";
                char explain[256];
                snprintf(explain, sizeof(explain),
                         "ASYNCHRONOUS block in module '%s' contains an unconditional\n"
                         "combinational loop — a signal's value depends on itself with\n"
                         "no intervening register. This will cause simulation oscillation\n"
                         "and is unsynthesizable.",
                         mod_name);
                sem_report_rule(diagnostics,
                                child->loc,
                                "COMB_LOOP_UNCONDITIONAL",
                                explain);
            } else if (sem_comb_union_has_cycle(&paths, net_count)) {
                const char *mod_name = (mod->name) ? mod->name : "?";
                char explain[256];
                snprintf(explain, sizeof(explain),
                         "ASYNCHRONOUS block in module '%s' has a combinational dependency\n"
                         "cycle that exists only across mutually exclusive branches (e.g.\n"
                         "IF/ELSE). No single execution path forms a loop, so this is safe.",
                         mod_name);
                sem_report_rule(diagnostics,
                                child->loc,
                                "COMB_LOOP_CONDITIONAL_SAFE",
                                explain);
            }
        }

        sem_comb_free_paths(&paths);
    }
}


