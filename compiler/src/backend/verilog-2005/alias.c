/*
 * alias.c - Alias context and union-find for the Verilog-2005 backend.
 *
 * This file implements signal alias resolution using disjoint-set (union-find)
 * data structures. It also handles emission of continuous alias assignments.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "verilog_internal.h"
#include "ir.h"

/* -------------------------------------------------------------------------
 * Alias context globals
 * -------------------------------------------------------------------------
 */

typedef struct AliasContext {
    const IR_Module *mod;
    int             *canonical_index; /* length = mod->num_signals */
    int             *is_repr;         /* non-zero if signals[i] is canonical */
    int              size;
} AliasContext;

static AliasContext g_alias_ctx = {0};

/* -------------------------------------------------------------------------
 * Union-find helpers
 * -------------------------------------------------------------------------
 */

static int uf_find(int *parent, int i)
{
    while (parent[i] != i) {
        parent[i] = parent[parent[i]];
        i = parent[i];
    }
    return i;
}

static void uf_union(int *parent, int *rank, int a, int b)
{
    int ra = uf_find(parent, a);
    int rb = uf_find(parent, b);
    if (ra == rb) return;
    if (rank[ra] < rank[rb]) {
        parent[ra] = rb;
    } else if (rank[ra] > rank[rb]) {
        parent[rb] = ra;
    } else {
        parent[rb] = ra;
        rank[ra]++;
    }
}

/* -------------------------------------------------------------------------
 * Assignment kind helpers
 * -------------------------------------------------------------------------
 */

int assignment_kind_is_alias(IR_AssignmentKind kind)
{
    switch (kind) {
        case ASSIGN_ALIAS:
        case ASSIGN_ALIAS_ZEXT:
        case ASSIGN_ALIAS_SEXT:
            return 1;
        default:
            return 0;
    }
}

/* -------------------------------------------------------------------------
 * Alias context interface
 * -------------------------------------------------------------------------
 */

void alias_ctx_set(const IR_Module *mod,
                   int *canonical_index,
                   int *is_repr,
                   int size)
{
    g_alias_ctx.mod = mod;
    g_alias_ctx.canonical_index = canonical_index;
    g_alias_ctx.is_repr = is_repr;
    g_alias_ctx.size = size;
}

void alias_ctx_clear(void)
{
    g_alias_ctx.mod = NULL;
    g_alias_ctx.canonical_index = NULL;
    g_alias_ctx.is_repr = NULL;
    g_alias_ctx.size = 0;
}

int alias_ctx_get_canonical_index(const IR_Module *mod, int signal_index)
{
    if (!g_alias_ctx.canonical_index || g_alias_ctx.mod != mod) {
        return signal_index;
    }
    if (signal_index < 0 || signal_index >= g_alias_ctx.size) {
        return signal_index;
    }
    int canon = g_alias_ctx.canonical_index[signal_index];
    if (canon < 0 || canon >= g_alias_ctx.size) {
        return signal_index;
    }
    return canon;
}

int alias_ctx_is_representative(const IR_Module *mod, int signal_index)
{
    if (!g_alias_ctx.is_repr || g_alias_ctx.mod != mod) {
        return 1;
    }
    if (signal_index < 0 || signal_index >= g_alias_ctx.size) {
        return 1;
    }
    return g_alias_ctx.is_repr[signal_index] != 0;
}

/* -------------------------------------------------------------------------
 * Scoring for canonical representative selection
 * -------------------------------------------------------------------------
 */

static int alias_canonical_score(const IR_Module *mod, int index)
{
    const IR_Signal *s = &mod->signals[index];
    int kind_score = 3;
    switch (s->kind) {
        case SIG_PORT:     kind_score = 0; break;
        case SIG_NET:      kind_score = 1; break;
        case SIG_REGISTER: kind_score = 2; break;
        case SIG_LATCH:    kind_score = 2; break;
        default:           kind_score = 3; break;
    }
    return kind_score * 1024 + index;
}

static int choose_canonical_index(const IR_Module *mod, int a, int b)
{
    int sa = alias_canonical_score(mod, a);
    int sb = alias_canonical_score(mod, b);
    return (sa <= sb) ? a : b;
}

/* -------------------------------------------------------------------------
 * Alias union collection from statements
 * -------------------------------------------------------------------------
 */

static void collect_alias_unions_from_stmt(const IR_Module *mod,
                                           const IR_Stmt *stmt,
                                           int *parent,
                                           int *rank)
{
    if (!mod || !stmt || !parent || !rank) {
        return;
    }

    switch (stmt->kind) {
    case STMT_ASSIGNMENT: {
        const IR_Assignment *a = &stmt->u.assign;
        if (!assignment_kind_is_alias(a->kind) || a->is_sliced || !a->rhs) {
            break;
        }
        if (a->rhs->kind != EXPR_SIGNAL_REF) {
            break; /* only simple a = b aliases participate in union-find */
        }

        int lhs_idx = find_signal_index_by_id_raw(mod, a->lhs_signal_id);
        int rhs_idx = find_signal_index_by_id_raw(mod, a->rhs->u.signal_ref.signal_id);
        if (lhs_idx < 0 || rhs_idx < 0) {
            break;
        }
        const IR_Signal *lhs = &mod->signals[lhs_idx];
        const IR_Signal *rhs = &mod->signals[rhs_idx];

        /* Registers always represent storage and are never unified as nets. */
        if (lhs->kind == SIG_REGISTER || rhs->kind == SIG_REGISTER) {
            break;
        }
        /* When both sides are ports, they must remain as distinct signals
         * in the Verilog port list. Merging them would cause the
         * self-referential check to drop the required assign statement
         * and redirect one port name to the other. */
        if (lhs->kind == SIG_PORT && rhs->kind == SIG_PORT) {
            break;
        }
        if (lhs->width <= 0 || lhs->width != rhs->width) {
            break;
        }

        uf_union(parent, rank, lhs_idx, rhs_idx);
        break;
    }
    case STMT_IF: {
        const IR_IfStmt *ifs = &stmt->u.if_stmt;
        if (ifs->then_block) {
            collect_alias_unions_from_stmt(mod, ifs->then_block, parent, rank);
        }
        const IR_Stmt *elif = ifs->elif_chain;
        while (elif && elif->kind == STMT_IF) {
            const IR_IfStmt *eifs = &elif->u.if_stmt;
            if (eifs->then_block) {
                collect_alias_unions_from_stmt(mod, eifs->then_block, parent, rank);
            }
            elif = eifs->elif_chain;
        }
        if (ifs->else_block) {
            collect_alias_unions_from_stmt(mod, ifs->else_block, parent, rank);
        }
        break;
    }
    case STMT_SELECT: {
        const IR_SelectStmt *sel = &stmt->u.select_stmt;
        for (int i = 0; i < sel->num_cases; ++i) {
            if (sel->cases[i].body) {
                collect_alias_unions_from_stmt(mod, sel->cases[i].body, parent, rank);
            }
        }
        break;
    }
    case STMT_BLOCK: {
        const IR_BlockStmt *blk = &stmt->u.block;
        for (int i = 0; i < blk->count; ++i) {
            collect_alias_unions_from_stmt(mod, &blk->stmts[i], parent, rank);
        }
        break;
    }
    default:
        break;
    }
}

/* -------------------------------------------------------------------------
 * Alias context preparation
 * -------------------------------------------------------------------------
 */

void prepare_alias_context_for_module(const IR_Module *mod,
                                      int **out_canonical_index,
                                      int **out_is_repr)
{
    *out_canonical_index = NULL;
    *out_is_repr = NULL;
    if (!mod || mod->num_signals <= 0) {
        return;
    }

    int n = mod->num_signals;
    int *parent = (int *)malloc((size_t)n * sizeof(int));
    int *rank   = (int *)calloc((size_t)n, sizeof(int));
    if (!parent || !rank) {
        free(parent);
        free(rank);
        return;
    }
    for (int i = 0; i < n; ++i) {
        parent[i] = i;
        rank[i] = 0;
    }

    /* Collect alias unions from ASYNCHRONOUS and SYNCHRONOUS bodies. */
    if (mod->async_block) {
        collect_alias_unions_from_stmt(mod, mod->async_block, parent, rank);
    }
    for (int i = 0; i < mod->num_clock_domains; ++i) {
        const IR_ClockDomain *cd = &mod->clock_domains[i];
        if (cd->statements) {
            collect_alias_unions_from_stmt(mod, cd->statements, parent, rank);
        }
    }

    int has_alias_merges = 0;
    for (int i = 0; i < n; ++i) {
        if (uf_find(parent, i) != i) {
            has_alias_merges = 1;
            break;
        }
    }
    if (!has_alias_merges) {
        free(parent);
        free(rank);
        return; /* no alias groups; keep default identity mapping */
    }

    int *root_canonical = (int *)malloc((size_t)n * sizeof(int));
    if (!root_canonical) {
        free(parent);
        free(rank);
        return;
    }
    for (int i = 0; i < n; ++i) {
        root_canonical[i] = -1;
    }

    for (int i = 0; i < n; ++i) {
        int r = uf_find(parent, i);
        if (root_canonical[r] < 0) {
            root_canonical[r] = i;
        } else {
            root_canonical[r] = choose_canonical_index(mod, root_canonical[r], i);
        }
    }

    int *canonical_index = (int *)malloc((size_t)n * sizeof(int));
    int *is_repr         = (int *)calloc((size_t)n, sizeof(int));
    if (!canonical_index || !is_repr) {
        free(parent);
        free(rank);
        free(root_canonical);
        free(canonical_index);
        free(is_repr);
        return;
    }

    for (int i = 0; i < n; ++i) {
        int r = uf_find(parent, i);
        int canon = root_canonical[r];
        if (canon < 0) {
            canon = i;
        }
        canonical_index[i] = canon;
    }
    for (int i = 0; i < n; ++i) {
        if (canonical_index[i] == i) {
            is_repr[i] = 1;
        }
    }

    free(parent);
    free(rank);
    free(root_canonical);

    *out_canonical_index = canonical_index;
    *out_is_repr = is_repr;
}

/* -------------------------------------------------------------------------
 * Continuous alias assignment emission
 * -------------------------------------------------------------------------
 */

/* Forward declarations to break circular dependency */
void emit_assignment_zext_rhs(FILE *out,
                              const IR_Module *mod,
                              const IR_Signal *lhs_sig,
                              const IR_Expr *rhs);
void emit_assignment_sext_rhs(FILE *out,
                              const IR_Module *mod,
                              const IR_Signal *lhs_sig,
                              const IR_Expr *rhs);

/* Emit a single continuous alias assignment as a top-level Verilog "assign"
 * statement. This is used for ASSIGN_ALIAS* sites from ASYNCHRONOUS blocks
 * where the semantics are pure wiring/aliasing.
 */
static void emit_continuous_alias_assignment(FILE *out,
                                             const IR_Module *mod,
                                             const IR_Assignment *a)
{
    if (!out || !mod || !a) {
        return;
    }

    const IR_Signal *lhs_sig = find_signal_by_id(mod, a->lhs_signal_id);
    if (!lhs_sig || !lhs_sig->name) {
        return;
    }

    /* Check for self-referential alias: if the RHS resolves to the same
     * canonical signal as the LHS (via union-find aliasing), skip emission
     * to avoid generating "assign x = x;" which is a no-op/combinational loop.
     */
    if (a->rhs && a->rhs->kind == EXPR_SIGNAL_REF && !a->is_sliced) {
        const IR_Signal *rhs_sig = find_signal_by_id(mod, a->rhs->u.signal_ref.signal_id);
        if (rhs_sig && rhs_sig == lhs_sig) {
            return;
        }
    }

    /* Direction fix for port-to-port aliases: An alias is semantically
     * bidirectional, but Verilog "assign" is unidirectional. When the IR has
     * LHS=INPUT_PORT and RHS=OUTPUT_PORT (e.g., tgt.DONE = src.DONE where
     * tgt.DONE is an input from a peripheral and src.DONE is an output to
     * the CPU), the correct Verilog is "assign OUTPUT = INPUT" so that the
     * output port is driven by the input port's externally-supplied value.
     * Swap LHS and RHS when needed.
     */
    const IR_Signal *emit_lhs = lhs_sig;
    const IR_Expr   *emit_rhs = a->rhs;
    int              emit_sliced = a->is_sliced;
    int              emit_msb = a->lhs_msb;
    int              emit_lsb = a->lhs_lsb;
    int              swapped = 0;

    if (emit_rhs && emit_rhs->kind == EXPR_SIGNAL_REF && !a->is_sliced) {
        const IR_Signal *rhs_sig = find_signal_by_id(mod, emit_rhs->u.signal_ref.signal_id);
        if (lhs_sig->kind == SIG_PORT && rhs_sig &&
            rhs_sig->kind == SIG_PORT) {
            int lhs_is_input = (lhs_sig->u.port.direction == PORT_IN);
            int rhs_is_input = (rhs_sig->u.port.direction == PORT_IN);
            int rhs_is_output = (rhs_sig->u.port.direction == PORT_OUT);

            /* When both ports are INPUT (e.g., after tristate elimination
             * converts INOUT ports to INPUT), skip emission entirely.
             * The routing is handled at the parent level by the tristate
             * transform's propagation assigns. Emitting an assign to an
             * input port from inside the module creates a multi-driver
             * conflict on the external wire. */
            if (lhs_is_input && rhs_is_input) {
                return;
            }

            if (lhs_is_input && rhs_is_output) {
                /* Swap: emit "assign rhs_name = lhs_name" */
                emit_lhs = rhs_sig;
                emit_rhs = NULL;  /* we'll emit lhs_sig name manually */
                swapped = 1;
            }
        }
    }

    /* LHS */
    emit_indent(out, 1);
    fputs("assign ", out);
    {
        char esc_al[256];
        fputs(verilog_safe_name(emit_lhs->name, esc_al, (int)sizeof(esc_al)), out);
    }
    if (emit_sliced && !swapped) {
        if (emit_msb == emit_lsb) {
            fprintf(out, "[%d]", emit_msb);
        } else {
            fprintf(out, "[%d:%d]", emit_msb, emit_lsb);
        }
    }

    fputs(" = ", out);

    if (swapped) {
        /* RHS is the original LHS signal */
        char esc_al2[256];
        fputs(verilog_safe_name(lhs_sig->name, esc_al2, (int)sizeof(esc_al2)), out);
    } else if (!a->rhs) {
        /* Try to interpret this alias as an asynchronous memory read. If that
         * does not succeed, fall back to a safe constant.
         */
        if (!emit_mem_read_for_null(out, mod, lhs_sig, MEM_PORT_READ_ASYNC)) {
            fputs("1'b0", out);
        }
    } else if (assignment_kind_is_zext(a->kind)) {
        emit_assignment_zext_rhs(out, mod, lhs_sig, a->rhs);
    } else if (assignment_kind_is_sext(a->kind)) {
        emit_assignment_sext_rhs(out, mod, lhs_sig, a->rhs);
    } else {
        emit_expr(out, mod, a->rhs);
    }

    fputs(";\n", out);
}

/* Walk a statement tree and emit continuous alias assignments for any
 * ASSIGN_ALIAS* nodes found.
 */
static void emit_continuous_alias_assignments_from_stmt(FILE *out,
                                                        const IR_Module *mod,
                                                        const IR_Stmt *stmt)
{
    if (!out || !mod || !stmt) {
        return;
    }

    switch (stmt->kind) {
    case STMT_ASSIGNMENT:
        if (assignment_kind_is_alias(stmt->u.assign.kind)) {
            emit_continuous_alias_assignment(out, mod, &stmt->u.assign);
        }
        break;

    case STMT_IF: {
        const IR_IfStmt *ifs = &stmt->u.if_stmt;
        if (ifs->then_block) {
            emit_continuous_alias_assignments_from_stmt(out, mod, ifs->then_block);
        }
        const IR_Stmt *elif = ifs->elif_chain;
        while (elif && elif->kind == STMT_IF) {
            const IR_IfStmt *eifs = &elif->u.if_stmt;
            if (eifs->then_block) {
                emit_continuous_alias_assignments_from_stmt(out, mod, eifs->then_block);
            }
            elif = eifs->elif_chain;
        }
        if (ifs->else_block) {
            emit_continuous_alias_assignments_from_stmt(out, mod, ifs->else_block);
        }
        break;
    }

    case STMT_SELECT: {
        const IR_SelectStmt *sel = &stmt->u.select_stmt;
        for (int i = 0; i < sel->num_cases; ++i) {
            if (sel->cases[i].body) {
                emit_continuous_alias_assignments_from_stmt(out, mod, sel->cases[i].body);
            }
        }
        break;
    }

    case STMT_BLOCK: {
        const IR_BlockStmt *blk = &stmt->u.block;
        for (int i = 0; i < blk->count; ++i) {
            emit_continuous_alias_assignments_from_stmt(out, mod, &blk->stmts[i]);
        }
        break;
    }

    default:
        break;
    }
}

void emit_continuous_alias_assignments(FILE *out, const IR_Module *mod)
{
    if (!out || !mod || !mod->async_block) {
        return;
    }
    emit_continuous_alias_assignments_from_stmt(out, mod, mod->async_block);
}

/* -------------------------------------------------------------------------
 * Wire tieoff for partially-driven sliced aliases
 *
 * When a wire is declared with width N but only some bit positions are
 * assigned via sliced continuous alias assignments (e.g., template expansion
 * assigns src_req[0] but src_req is 2 bits), the undriven bits are X in
 * simulation and undefined on FPGA.  This pass detects such gaps and emits
 * explicit `assign wire[bit] = 1'b0;` for each undriven position.
 * -------------------------------------------------------------------------
 */

/* Per-signal bit-coverage tracker. */
typedef struct {
    int  signal_id;
    int  width;
    bool has_full_alias;      /* true if any non-sliced alias covers all bits */
    bool has_any_sliced_alias;
    char *driven;             /* driven[bit] = 1 if covered by a sliced alias */
} SignalCoverage;

static SignalCoverage *s_cov     = NULL;
static int             s_cov_cap = 0;
static int             s_cov_len = 0;

static SignalCoverage *find_or_create_coverage(int signal_id, int width)
{
    for (int i = 0; i < s_cov_len; ++i) {
        if (s_cov[i].signal_id == signal_id) {
            return &s_cov[i];
        }
    }
    if (s_cov_len >= s_cov_cap) {
        s_cov_cap = s_cov_cap ? s_cov_cap * 2 : 16;
        s_cov = realloc(s_cov, (size_t)s_cov_cap * sizeof(*s_cov));
    }
    SignalCoverage *c = &s_cov[s_cov_len++];
    c->signal_id = signal_id;
    c->width = width;
    c->has_full_alias = false;
    c->has_any_sliced_alias = false;
    c->driven = calloc((size_t)width, 1);
    return c;
}

static void collect_alias_coverage_from_stmt(const IR_Module *mod,
                                              const IR_Stmt *stmt)
{
    if (!stmt) return;

    switch (stmt->kind) {
    case STMT_ASSIGNMENT: {
        const IR_Assignment *a = &stmt->u.assign;
        if (!assignment_kind_is_alias(a->kind)) break;

        const IR_Signal *sig = find_signal_by_id_raw(mod, a->lhs_signal_id);
        if (!sig) break;
        /* Only process wires (SIG_NET), skip ports/regs/tristate. */
        if (sig->kind != SIG_NET) break;
        if (sig->can_be_z) break;

        SignalCoverage *c = find_or_create_coverage(sig->id, sig->width);
        if (a->is_sliced) {
            c->has_any_sliced_alias = true;
            int lo = a->lhs_lsb;
            int hi = a->lhs_msb;
            if (lo < 0) lo = 0;
            if (hi >= sig->width) hi = sig->width - 1;
            for (int b = lo; b <= hi; ++b) {
                c->driven[b] = 1;
            }
        } else {
            c->has_full_alias = true;
        }
        break;
    }
    case STMT_IF: {
        const IR_IfStmt *ifs = &stmt->u.if_stmt;
        if (ifs->then_block)
            collect_alias_coverage_from_stmt(mod, ifs->then_block);
        const IR_Stmt *elif = ifs->elif_chain;
        while (elif && elif->kind == STMT_IF) {
            const IR_IfStmt *eifs = &elif->u.if_stmt;
            if (eifs->then_block)
                collect_alias_coverage_from_stmt(mod, eifs->then_block);
            elif = eifs->elif_chain;
        }
        if (ifs->else_block)
            collect_alias_coverage_from_stmt(mod, ifs->else_block);
        break;
    }
    case STMT_SELECT: {
        const IR_SelectStmt *sel = &stmt->u.select_stmt;
        for (int i = 0; i < sel->num_cases; ++i) {
            if (sel->cases[i].body)
                collect_alias_coverage_from_stmt(mod, sel->cases[i].body);
        }
        break;
    }
    case STMT_BLOCK: {
        const IR_BlockStmt *blk = &stmt->u.block;
        for (int i = 0; i < blk->count; ++i) {
            collect_alias_coverage_from_stmt(mod, &blk->stmts[i]);
        }
        break;
    }
    default:
        break;
    }
}

void emit_wire_tieoff_assignments(FILE *out, const IR_Module *mod)
{
    if (!out || !mod || !mod->async_block) return;

    /* Reset coverage state. */
    s_cov_len = 0;

    /* Collect bit coverage from alias assignments. */
    collect_alias_coverage_from_stmt(mod, mod->async_block);

    /* Emit tieoff for any gaps. */
    char safe_buf[256];
    for (int i = 0; i < s_cov_len; ++i) {
        SignalCoverage *c = &s_cov[i];
        /* Skip if fully driven by a non-sliced alias. */
        if (c->has_full_alias) goto cleanup_entry;
        /* Only act if there's at least one sliced alias. */
        if (!c->has_any_sliced_alias) goto cleanup_entry;

        const IR_Signal *sig = find_signal_by_id_raw(mod, c->signal_id);
        if (!sig) goto cleanup_entry;
        const char *name = verilog_safe_name(sig->name, safe_buf,
                                             (int)sizeof(safe_buf));

        for (int b = 0; b < c->width; ++b) {
            if (!c->driven[b]) {
                fprintf(out, "    assign %s[%d] = 1'b0;\n", name, b);
            }
        }

    cleanup_entry:
        free(c->driven);
        c->driven = NULL;
    }
    s_cov_len = 0;
}
