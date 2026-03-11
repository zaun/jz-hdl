/**
 * @file sim_state.c
 * @brief Simulation state creation, initialization, lookup, and NBA apply.
 */

#include "sim_state.h"
#include "sim_perf.h"
#include <stdlib.h>
#include <string.h>

/* ---- PRNG ---- */

uint32_t sim_rng_next(uint32_t *state) {
    uint32_t x = *state;
    if (x == 0) x = 1; /* avoid zero state */
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

/* ---- Signal set collector for dependency analysis ---- */

typedef struct {
    int *ids;
    int  count;
    int  cap;
} SigIdSet;

static void sigset_add(SigIdSet *s, int id) {
    for (int i = 0; i < s->count; i++)
        if (s->ids[i] == id) return;
    if (s->count >= s->cap) {
        s->cap = s->cap ? s->cap * 2 : 8;
        s->ids = realloc(s->ids, (size_t)s->cap * sizeof(int));
    }
    s->ids[s->count++] = id;
}

static void collect_expr_reads(const IR_Expr *expr, SigIdSet *reads) {
    if (!expr) return;
    switch (expr->kind) {
    case EXPR_LITERAL:
        break;
    case EXPR_SIGNAL_REF:
        sigset_add(reads, expr->u.signal_ref.signal_id);
        break;
    case EXPR_UNARY_NOT:
    case EXPR_UNARY_NEG:
    case EXPR_LOGICAL_NOT:
        collect_expr_reads(expr->u.unary.operand, reads);
        break;
    case EXPR_BINARY_ADD: case EXPR_BINARY_SUB: case EXPR_BINARY_MUL:
    case EXPR_BINARY_DIV: case EXPR_BINARY_MOD:
    case EXPR_BINARY_AND: case EXPR_BINARY_OR: case EXPR_BINARY_XOR:
    case EXPR_BINARY_SHL: case EXPR_BINARY_SHR: case EXPR_BINARY_ASHR:
    case EXPR_BINARY_EQ: case EXPR_BINARY_NEQ:
    case EXPR_BINARY_LT: case EXPR_BINARY_GT:
    case EXPR_BINARY_LTE: case EXPR_BINARY_GTE:
    case EXPR_LOGICAL_AND: case EXPR_LOGICAL_OR:
        collect_expr_reads(expr->u.binary.left, reads);
        collect_expr_reads(expr->u.binary.right, reads);
        break;
    case EXPR_TERNARY:
        collect_expr_reads(expr->u.ternary.condition, reads);
        collect_expr_reads(expr->u.ternary.true_val, reads);
        collect_expr_reads(expr->u.ternary.false_val, reads);
        break;
    case EXPR_CONCAT:
        for (int i = 0; i < expr->u.concat.num_operands; i++)
            collect_expr_reads(expr->u.concat.operands[i], reads);
        break;
    case EXPR_SLICE:
        if (expr->u.slice.base_expr)
            collect_expr_reads(expr->u.slice.base_expr, reads);
        else
            sigset_add(reads, expr->u.slice.signal_id);
        break;
    /* Intrinsics using u.intrinsic layout */
    case EXPR_INTRINSIC_UADD: case EXPR_INTRINSIC_SADD:
    case EXPR_INTRINSIC_UMUL: case EXPR_INTRINSIC_SMUL:
    case EXPR_INTRINSIC_GBIT: case EXPR_INTRINSIC_SBIT:
    case EXPR_INTRINSIC_GSLICE: case EXPR_INTRINSIC_SSLICE:
    case EXPR_INTRINSIC_OH2B: case EXPR_INTRINSIC_B2OH:
    case EXPR_INTRINSIC_PRIENC: case EXPR_INTRINSIC_LZC:
    case EXPR_INTRINSIC_ABS:
    case EXPR_INTRINSIC_POPCOUNT: case EXPR_INTRINSIC_REVERSE:
    case EXPR_INTRINSIC_BSWAP:
    case EXPR_INTRINSIC_REDUCE_AND: case EXPR_INTRINSIC_REDUCE_OR:
    case EXPR_INTRINSIC_REDUCE_XOR:
        collect_expr_reads(expr->u.intrinsic.source, reads);
        collect_expr_reads(expr->u.intrinsic.index, reads);
        collect_expr_reads(expr->u.intrinsic.value, reads);
        break;
    /* Intrinsics using u.binary layout */
    case EXPR_INTRINSIC_USUB: case EXPR_INTRINSIC_SSUB:
    case EXPR_INTRINSIC_UMIN: case EXPR_INTRINSIC_UMAX:
    case EXPR_INTRINSIC_SMIN: case EXPR_INTRINSIC_SMAX:
        collect_expr_reads(expr->u.binary.left, reads);
        collect_expr_reads(expr->u.binary.right, reads);
        break;
    case EXPR_MEM_READ:
        collect_expr_reads(expr->u.mem_read.address, reads);
        break;
    }
}

static void collect_stmt_deps(const IR_Stmt *stmt, SigIdSet *reads, SigIdSet *writes) {
    if (!stmt) return;
    switch (stmt->kind) {
    case STMT_ASSIGNMENT:
        sigset_add(writes, stmt->u.assign.lhs_signal_id);
        collect_expr_reads(stmt->u.assign.rhs, reads);
        break;
    case STMT_IF:
        collect_expr_reads(stmt->u.if_stmt.condition, reads);
        collect_stmt_deps(stmt->u.if_stmt.then_block, reads, writes);
        collect_stmt_deps(stmt->u.if_stmt.elif_chain, reads, writes);
        collect_stmt_deps(stmt->u.if_stmt.else_block, reads, writes);
        break;
    case STMT_SELECT:
        collect_expr_reads(stmt->u.select_stmt.selector, reads);
        for (int i = 0; i < stmt->u.select_stmt.num_cases; i++) {
            collect_expr_reads(stmt->u.select_stmt.cases[i].case_value, reads);
            collect_stmt_deps(stmt->u.select_stmt.cases[i].body, reads, writes);
        }
        break;
    case STMT_BLOCK:
        for (int i = 0; i < stmt->u.block.count; i++)
            collect_stmt_deps(&stmt->u.block.stmts[i], reads, writes);
        break;
    case STMT_MEM_WRITE:
        collect_expr_reads(stmt->u.mem_write.address, reads);
        collect_expr_reads(stmt->u.mem_write.data, reads);
        break;
    }
}

/**
 * Convert signal IDs to ctx->signals[] indices, deduplicating.
 * Returns allocated array and sets *out_count. Caller frees.
 */
static int *sigids_to_indices(SimContext *ctx, const SigIdSet *ids, int *out_count) {
    if (ids->count == 0) { *out_count = 0; return NULL; }
    int *indices = malloc((size_t)ids->count * sizeof(int));
    int n = 0;
    for (int i = 0; i < ids->count; i++) {
        int sig_id = ids->ids[i];
        if (sig_id >= 0 && sig_id <= ctx->max_sig_id) {
            int idx = ctx->sig_id_map[sig_id];
            if (idx >= 0) {
                /* Deduplicate indices */
                int dup = 0;
                for (int j = 0; j < n; j++)
                    if (indices[j] == idx) { dup = 1; break; }
                if (!dup)
                    indices[n++] = idx;
            }
        }
    }
    *out_count = n;
    if (n == 0) { free(indices); return NULL; }
    return indices;
}

/**
 * Build async chunks from the module's async_block.
 * Splits top-level STMT_BLOCK into individual chunks with dependency sets.
 */
static void build_async_chunks(SimContext *ctx) {
    const IR_Stmt *async = ctx->module->async_block;
    if (!async) {
        ctx->async_chunks = NULL;
        ctx->num_async_chunks = 0;
        return;
    }

    /* Determine how many top-level statements to split into */
    int n;
    const IR_Stmt *stmts_base;
    if (async->kind == STMT_BLOCK) {
        n = async->u.block.count;
        stmts_base = async->u.block.stmts;
    } else {
        n = 1;
        stmts_base = async;
    }

    ctx->async_chunks = calloc((size_t)n, sizeof(SimAsyncChunk));
    ctx->num_async_chunks = n;

    for (int i = 0; i < n; i++) {
        const IR_Stmt *stmt = (async->kind == STMT_BLOCK) ? &stmts_base[i] : stmts_base;
        SigIdSet reads = {NULL, 0, 0};
        SigIdSet writes = {NULL, 0, 0};

        collect_stmt_deps(stmt, &reads, &writes);

        SimAsyncChunk *chunk = &ctx->async_chunks[i];
        chunk->stmt = stmt;
        chunk->read_indices = sigids_to_indices(ctx, &reads, &chunk->num_reads);
        chunk->write_indices = sigids_to_indices(ctx, &writes, &chunk->num_writes);

        free(reads.ids);
        free(writes.ids);
    }
}

/* ---- Context creation ---- */

SimContext *sim_ctx_create(const IR_Module *module, const IR_Design *design,
                           uint32_t rng_seed) {
    SimContext *ctx = calloc(1, sizeof(SimContext));
    if (!ctx) return NULL;

    ctx->module = module;
    ctx->design = design;
    ctx->rng_state = rng_seed;
    ctx->num_signals = module->num_signals;
    ctx->signals = calloc((size_t)module->num_signals, sizeof(SimSignalEntry));

    /* Find max signal ID to size the lookup map */
    int max_id = -1;
    for (int i = 0; i < module->num_signals; i++) {
        if (module->signals[i].id > max_id)
            max_id = module->signals[i].id;
    }
    ctx->max_sig_id = max_id;
    ctx->sig_id_map = NULL;
    if (max_id >= 0) {
        ctx->sig_id_map = malloc((size_t)(max_id + 1) * sizeof(int));
        for (int i = 0; i <= max_id; i++)
            ctx->sig_id_map[i] = -1;
    }

    for (int i = 0; i < module->num_signals; i++) {
        const IR_Signal *sig = &module->signals[i];
        SimSignalEntry *entry = &ctx->signals[i];
        entry->signal_id = sig->id;
        entry->has_pending = 0;

        /* Populate O(1) lookup map */
        if (sig->id >= 0 && sig->id <= max_id)
            ctx->sig_id_map[sig->id] = i;

        switch (sig->kind) {
        case SIG_REGISTER:
        case SIG_LATCH:
            /* Initialize registers and latches with random bits from PRNG */
            {
                uint64_t rv = sim_rng_next(&ctx->rng_state);
                if (sig->width > 32)
                    rv |= (uint64_t)sim_rng_next(&ctx->rng_state) << 32;
                entry->current = sim_val_from_uint(rv, sig->width);
            }
            break;
        case SIG_PORT:
        case SIG_NET:
        default:
            entry->current = sim_val_zero(sig->width);
            break;
        }
        entry->next = sim_val_zero(sig->width);
    }

    /* Allocate per-signal dirty flags (all dirty initially for first settle) */
    ctx->sig_dirty = calloc((size_t)module->num_signals, sizeof(uint8_t));
    for (int i = 0; i < module->num_signals; i++)
        ctx->sig_dirty[i] = 1;

    /* Initialize memories */
    ctx->num_memories = module->num_memories;
    if (module->num_memories > 0) {
        ctx->memories = calloc((size_t)module->num_memories, sizeof(SimMemEntry));
        for (int i = 0; i < module->num_memories; i++) {
            const IR_Memory *mem = &module->memories[i];
            SimMemEntry *me = &ctx->memories[i];
            me->mem_id = mem->id;
            me->word_width = mem->word_width;
            me->depth = mem->depth;
            me->cells = calloc((size_t)mem->depth, sizeof(SimValue));
            for (int j = 0; j < mem->depth; j++) {
                if (mem->init_is_file) {
                    /* File-initialized memories (ROM) keep their data;
                     * actual file loading is handled elsewhere. Zero here
                     * as a safe default if file data isn't loaded yet. */
                    me->cells[j] = sim_val_zero(mem->word_width);
                } else {
                    /* Initialize MEM with random bits from PRNG per spec
                     * Section 2.6: all storage randomizes at start. */
                    uint64_t rv = sim_rng_next(&ctx->rng_state);
                    if (mem->word_width > 32)
                        rv |= (uint64_t)sim_rng_next(&ctx->rng_state) << 32;
                    me->cells[j] = sim_val_from_uint(rv, mem->word_width);
                }
            }
        }
    }

    /* Recursively create child instance contexts */
    ctx->num_children = module->num_instances;
    ctx->children = NULL;
    if (module->num_instances > 0 && design) {
        ctx->children = calloc((size_t)module->num_instances,
                               sizeof(SimChildInstance));
        for (int i = 0; i < module->num_instances; i++) {
            const IR_Instance *inst = &module->instances[i];
            SimChildInstance *ci = &ctx->children[i];
            ci->inst = inst;
            ci->child_module = NULL;
            ci->ctx = NULL;

            /* Find child module in design */
            for (int m = 0; m < design->num_modules; m++) {
                if (design->modules[m].id == inst->child_module_id) {
                    ci->child_module = &design->modules[m];
                    break;
                }
            }
            if (ci->child_module) {
                ci->ctx = sim_ctx_create(ci->child_module, design, rng_seed + (uint32_t)i + 1);
            }
        }

        /* Build precomputed port mappings for each child */
        for (int i = 0; i < module->num_instances; i++) {
            SimChildInstance *ci = &ctx->children[i];
            if (!ci->ctx || !ci->inst) continue;

            const IR_Instance *inst = ci->inst;
            int num_conn = inst->num_connections;

            /* Count input and output mappings */
            int n_in = 0, n_out = 0;
            for (int p = 0; p < num_conn; p++) {
                const IR_InstanceConnection *conn = &inst->connections[p];
                if (conn->const_expr) continue;

                int child_port_id = conn->child_port_id;
                const IR_Signal *child_sig = NULL;
                if (child_port_id >= 0 && child_port_id <= ci->ctx->max_sig_id) {
                    int idx = ci->ctx->sig_id_map[child_port_id];
                    if (idx >= 0)
                        child_sig = &ci->child_module->signals[idx];
                }
                if (!child_sig || child_sig->kind != SIG_PORT) continue;

                SimSignalEntry *pe = sim_ctx_lookup(ctx, conn->parent_signal_id);
                SimSignalEntry *ce = sim_ctx_lookup(ci->ctx, child_port_id);
                if (!pe || !ce) continue;

                if (child_sig->u.port.direction == PORT_IN ||
                    child_sig->u.port.direction == PORT_INOUT)
                    n_in++;
                if (child_sig->u.port.direction == PORT_OUT ||
                    child_sig->u.port.direction == PORT_INOUT)
                    n_out++;
            }

            ci->input_maps = n_in > 0 ? malloc((size_t)n_in * sizeof(SimPortMapping)) : NULL;
            ci->num_input_maps = 0;
            ci->output_maps = n_out > 0 ? malloc((size_t)n_out * sizeof(SimPortMapping)) : NULL;
            ci->num_output_maps = 0;

            for (int p = 0; p < num_conn; p++) {
                const IR_InstanceConnection *conn = &inst->connections[p];
                if (conn->const_expr) continue;

                int child_port_id = conn->child_port_id;
                const IR_Signal *child_sig = NULL;
                if (child_port_id >= 0 && child_port_id <= ci->ctx->max_sig_id) {
                    int idx = ci->ctx->sig_id_map[child_port_id];
                    if (idx >= 0)
                        child_sig = &ci->child_module->signals[idx];
                }
                if (!child_sig || child_sig->kind != SIG_PORT) continue;

                SimSignalEntry *pe = sim_ctx_lookup(ctx, conn->parent_signal_id);
                SimSignalEntry *ce = sim_ctx_lookup(ci->ctx, child_port_id);
                if (!pe || !ce) continue;

                int is_inout = (child_sig->u.port.direction == PORT_INOUT);

                if (child_sig->u.port.direction == PORT_IN || is_inout) {
                    SimPortMapping *m = &ci->input_maps[ci->num_input_maps++];
                    m->parent_entry = pe;
                    m->child_entry = ce;
                    m->parent_msb = conn->parent_msb;
                    m->parent_lsb = conn->parent_lsb;
                    m->child_width = child_sig->width;
                    m->is_inout = is_inout;
                }
                if (child_sig->u.port.direction == PORT_OUT || is_inout) {
                    SimPortMapping *m = &ci->output_maps[ci->num_output_maps++];
                    m->parent_entry = pe;
                    m->child_entry = ce;
                    m->parent_msb = conn->parent_msb;
                    m->parent_lsb = conn->parent_lsb;
                    m->child_width = child_sig->width;
                    m->is_inout = is_inout;
                }
            }
        }
    }

    /* Build dependency-analyzed async chunks */
    build_async_chunks(ctx);

    return ctx;
}

void sim_ctx_destroy(SimContext *ctx) {
    if (!ctx) return;
    /* Recursively destroy children */
    if (ctx->children) {
        for (int i = 0; i < ctx->num_children; i++) {
            free(ctx->children[i].input_maps);
            free(ctx->children[i].output_maps);
            sim_ctx_destroy(ctx->children[i].ctx);
        }
        free(ctx->children);
    }
    free(ctx->signals);
    free(ctx->sig_id_map);
    free(ctx->sig_dirty);
    if (ctx->async_chunks) {
        for (int i = 0; i < ctx->num_async_chunks; i++) {
            free(ctx->async_chunks[i].read_indices);
            free(ctx->async_chunks[i].write_indices);
        }
        free(ctx->async_chunks);
    }
    if (ctx->memories) {
        for (int i = 0; i < ctx->num_memories; i++)
            free(ctx->memories[i].cells);
        free(ctx->memories);
    }
    free(ctx);
}

/* ---- Lookup ---- */

SimSignalEntry *sim_ctx_lookup(SimContext *ctx, int signal_id) {
    if (signal_id >= 0 && signal_id <= ctx->max_sig_id) {
        int idx = ctx->sig_id_map[signal_id];
        if (idx >= 0)
            return &ctx->signals[idx];
    }
    return NULL;
}

SimMemEntry *sim_ctx_lookup_mem(SimContext *ctx, const char *name) {
    for (int i = 0; i < ctx->num_memories; i++) {
        if (strcmp(ctx->module->memories[i].name, name) == 0)
            return &ctx->memories[i];
    }
    return NULL;
}

/* ---- Dirty tracking ---- */

void sim_ctx_clear_dirty(SimContext *ctx) {
    ctx->settle_dirty = 0;
    for (int c = 0; c < ctx->num_children; c++) {
        if (ctx->children[c].ctx)
            sim_ctx_clear_dirty(ctx->children[c].ctx);
    }
}

/* ---- NBA apply ---- */

void sim_ctx_apply_nba(SimContext *ctx) {
    PERF_TIMER_START(PERF_APPLY_NBA);
#ifdef TRACK_PERF
    int pending_count = 0;
#endif
    for (int i = 0; i < ctx->num_signals; i++) {
        SimSignalEntry *e = &ctx->signals[i];
        if (e->has_pending) {
            e->current = e->next;
            e->has_pending = 0;
            ctx->settle_dirty = 1;
            ctx->sig_dirty[i] = 1;
#ifdef TRACK_PERF
            pending_count++;
#endif
        }
    }
    PERF_STATE_NBA_PENDING(pending_count);
    /* Recursively apply NBA to children */
    for (int c = 0; c < ctx->num_children; c++) {
        if (ctx->children[c].ctx)
            sim_ctx_apply_nba(ctx->children[c].ctx);
    }
    PERF_TIMER_STOP(PERF_APPLY_NBA);
}
