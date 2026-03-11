/**
 * @file sim_exec.c
 * @brief IR statement executor with NBA/immediate modes,
 *        combinational settling, and sync domain execution.
 *        Supports hierarchical sub-instance simulation.
 */

#include "sim_exec.h"
#include "sim_eval.h"
#include "sim_perf.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Apply an assignment to a signal entry */
static void apply_assign(SimContext *ctx, const IR_Assignment *asgn, int is_nba) {
    SimSignalEntry *entry = sim_ctx_lookup(ctx, asgn->lhs_signal_id);
    if (!entry) return;

    SimValue rhs_val = sim_eval_expr(ctx, asgn->rhs);

    /* Handle extension kinds */
    int target_width = entry->current.width;
    switch (asgn->kind) {
    case ASSIGN_ALIAS_ZEXT:
    case ASSIGN_DRIVE_ZEXT:
    case ASSIGN_RECEIVE_ZEXT:
        rhs_val = sim_val_zext(rhs_val, target_width);
        break;
    case ASSIGN_ALIAS_SEXT:
    case ASSIGN_DRIVE_SEXT:
    case ASSIGN_RECEIVE_SEXT:
        rhs_val = sim_val_sext(rhs_val, target_width);
        break;
    default:
        /* Truncate or zero-extend to match target width */
        if (rhs_val.width != target_width) {
            rhs_val.width = target_width;
            rhs_val = sim_val_mask(rhs_val);
        }
        break;
    }

    if (asgn->is_sliced) {
        /* Part-select write (multi-word aware) */
        int msb = asgn->lhs_msb;
        int lsb = asgn->lhs_lsb;
        int slice_w = msb - lsb + 1;

        SimValue *target = is_nba ? &entry->next : &entry->current;
        if (is_nba && !entry->has_pending) {
            /* Start from current value for partial updates */
            entry->next = entry->current;
        }

        /* Snapshot affected words for dirty tracking (immediate mode) */
        int lo_wi = lsb / 64;
        int hi_wi = msb / 64;
        uint64_t snap_val[SIM_VAL_WORDS], snap_x[SIM_VAL_WORDS], snap_z[SIM_VAL_WORDS];
        if (!is_nba) {
            for (int w = lo_wi; w <= hi_wi; w++) {
                snap_val[w] = target->val[w];
                snap_x[w] = target->xmask[w];
                snap_z[w] = target->zmask[w];
            }
        }

        /* Write each bit of the slice */
        for (int b = 0; b < slice_w; b++) {
            int dst_bit = lsb + b;
            int dst_wi = dst_bit / 64;
            int dst_bi = dst_bit % 64;
            int src_wi = b / 64;
            int src_bi = b % 64;
            uint64_t bmask = (uint64_t)1 << dst_bi;

            target->val[dst_wi]   = (target->val[dst_wi]   & ~bmask) | (((rhs_val.val[src_wi]   >> src_bi) & 1) << dst_bi);
            target->xmask[dst_wi] = (target->xmask[dst_wi] & ~bmask) | (((rhs_val.xmask[src_wi] >> src_bi) & 1) << dst_bi);
            target->zmask[dst_wi] = (target->zmask[dst_wi] & ~bmask) | (((rhs_val.zmask[src_wi] >> src_bi) & 1) << dst_bi);
        }

        if (is_nba) {
            entry->has_pending = 1;
        } else {
            /* Dirty tracking: compare affected words */
            for (int w = lo_wi; w <= hi_wi; w++) {
                if (target->val[w] != snap_val[w] ||
                    target->xmask[w] != snap_x[w] ||
                    target->zmask[w] != snap_z[w]) {
                    ctx->settle_dirty = 1;
                    int sig_idx = (int)(entry - ctx->signals);
                    ctx->sig_dirty[sig_idx] = 1;
                    break;
                }
            }
        }
    } else {
        /* Full-width write */
        if (is_nba) {
            entry->next = rhs_val;
            entry->has_pending = 1;
        } else {
            /* Dirty tracking: only write and mark dirty if value changed */
            int nw = (target_width + 63) / 64;
            int same = 1;
            for (int w = 0; w < nw; w++) {
                if (entry->current.val[w] != rhs_val.val[w] ||
                    entry->current.xmask[w] != rhs_val.xmask[w] ||
                    entry->current.zmask[w] != rhs_val.zmask[w]) {
                    same = 0;
                    break;
                }
            }
            if (!same) {
                entry->current = rhs_val;
                ctx->settle_dirty = 1;
                int sig_idx = (int)(entry - ctx->signals);
                ctx->sig_dirty[sig_idx] = 1;
            }
        }
    }
}

void sim_exec_stmt(SimContext *ctx, const IR_Stmt *stmt, int is_nba) {
    if (!stmt) return;
    PERF_COUNT(PERF_EXEC_STMT);

    switch (stmt->kind) {

    case STMT_ASSIGNMENT:
        apply_assign(ctx, &stmt->u.assign, is_nba);
        break;

    case STMT_IF: {
        const IR_IfStmt *ifs = &stmt->u.if_stmt;
        SimValue cond = sim_eval_expr(ctx, ifs->condition);
        int tc = sim_val_is_true(cond);

        if (tc == -1) {
            /* z reached a non-tristate expression (IF condition).
             * Per spec: x is not a runtime value; z in non-tristate
             * expression is runtime error SE-008. */
            if (!ctx->runtime_error) {
                ctx->runtime_error = 1;
                fprintf(stderr, "RUNTIME ERROR: z reached IF condition "
                        "(SE-008)\n");
            }
            break;
        }

        if (tc == 1) {
            sim_exec_stmt(ctx, ifs->then_block, is_nba);
        } else {
            /* Try elif chain */
            const IR_Stmt *elif = ifs->elif_chain;
            while (elif && elif->kind == STMT_IF) {
                SimValue ec = sim_eval_expr(ctx, elif->u.if_stmt.condition);
                int etc = sim_val_is_true(ec);
                if (etc == -1) {
                    /* z in elif condition — same runtime error */
                    if (!ctx->runtime_error) {
                        ctx->runtime_error = 1;
                        fprintf(stderr, "RUNTIME ERROR: z reached ELIF "
                                "condition (SE-008)\n");
                    }
                    return;
                }
                if (etc == 1) {
                    sim_exec_stmt(ctx, elif->u.if_stmt.then_block, is_nba);
                    return;
                }
                elif = elif->u.if_stmt.elif_chain;
            }
            /* Else block */
            if (ifs->else_block) {
                sim_exec_stmt(ctx, ifs->else_block, is_nba);
            }
        }
        break;
    }

    case STMT_SELECT: {
        const IR_SelectStmt *sel = &stmt->u.select_stmt;
        SimValue selector = sim_eval_expr(ctx, sel->selector);

        /* z in SELECT selector is a runtime error (SE-008) */
        if (sim_val_has_xz(selector)) {
            if (!ctx->runtime_error) {
                ctx->runtime_error = 1;
                fprintf(stderr, "RUNTIME ERROR: z reached SELECT selector "
                        "(SE-008)\n");
            }
            break;
        }

        int matched = 0;

        for (int i = 0; i < sel->num_cases; i++) {
            if (sel->cases[i].case_value == NULL) {
                /* DEFAULT case */
                if (!matched) {
                    sim_exec_stmt(ctx, sel->cases[i].body, is_nba);
                    matched = 1;
                }
                break;
            }
            SimValue cv = sim_eval_expr(ctx, sel->cases[i].case_value);
            SimValue eq = sim_val_eq(selector, cv);
            if (sim_val_is_true(eq) == 1) {
                sim_exec_stmt(ctx, sel->cases[i].body, is_nba);
                matched = 1;
                break;
            }
        }
        break;
    }

    case STMT_BLOCK: {
        const IR_BlockStmt *blk = &stmt->u.block;
        for (int i = 0; i < blk->count; i++) {
            sim_exec_stmt(ctx, &blk->stmts[i], is_nba);
        }
        break;
    }

    case STMT_MEM_WRITE: {
        const IR_MemWriteStmt *mw = &stmt->u.mem_write;
        SimMemEntry *me = sim_ctx_lookup_mem(ctx, mw->memory_name);
        if (!me) break;
        SimValue addr = sim_eval_expr(ctx, mw->address);
        SimValue data = sim_eval_expr(ctx, mw->data);
        if (sim_val_has_xz(addr)) break;
        uint64_t idx = addr.val[0];
        if (idx >= (uint64_t)me->depth) break;
        me->cells[idx] = data;
        break;
    }

    } /* end switch */
}

/* ---- Instance port propagation ---- */

/**
 * O(1) signal lookup using the SimContext's prebuilt index map.
 */
static const IR_Signal *find_signal_in_ctx(SimContext *ctx, int signal_id) {
    if (signal_id >= 0 && signal_id <= ctx->max_sig_id) {
        int idx = ctx->sig_id_map[signal_id];
        if (idx >= 0)
            return &ctx->module->signals[idx];
    }
    return NULL;
}

/**
 * Compare two SimValues for exact equality (fast, no masking).
 * Assumes unused bits above width are already zeroed.
 */
static inline int sim_val_same(SimValue a, SimValue b, int nw) {
    for (int i = 0; i < nw; i++) {
        if (a.val[i] != b.val[i] || a.xmask[i] != b.xmask[i] ||
            a.zmask[i] != b.zmask[i])
            return 0;
    }
    return 1;
}

/**
 * Propagate values from parent context to child instance inputs.
 * Uses precomputed port mappings for zero-lookup propagation.
 * Returns 1 if any child input actually changed, 0 otherwise.
 */
static int propagate_to_child(SimContext *parent, SimChildInstance *ci) {
    (void)parent;
    int changed = 0;
    for (int i = 0; i < ci->num_input_maps; i++) {
        SimPortMapping *m = &ci->input_maps[i];
        SimValue old = m->child_entry->current;

        if (m->parent_msb >= 0) {
            /* Sliced connection: extract slice from parent */
            m->child_entry->current = sim_val_slice(m->parent_entry->current,
                                                     m->parent_msb,
                                                     m->parent_lsb);
        } else {
            /* Full-width connection */
            m->child_entry->current = m->parent_entry->current;
            if (m->child_entry->current.width != m->child_width) {
                m->child_entry->current.width = m->child_width;
                m->child_entry->current = sim_val_mask(m->child_entry->current);
            }
        }

        {
            int nw = (m->child_width + 63) / 64;
            if (!sim_val_same(old, m->child_entry->current, nw)) {
                changed = 1;
                int sig_idx = (int)(m->child_entry - ci->ctx->signals);
                ci->ctx->sig_dirty[sig_idx] = 1;
            }
        }
    }
    return changed;
}

/**
 * Propagate values from child instance outputs back to parent context.
 * Uses precomputed port mappings for zero-lookup propagation.
 * Sets parent settle_dirty if any parent signal changes.
 */
static void propagate_from_child(SimContext *parent, SimChildInstance *ci) {
    for (int i = 0; i < ci->num_output_maps; i++) {
        SimPortMapping *m = &ci->output_maps[i];

        /* For INOUT: don't overwrite parent with z */
        if (m->is_inout && sim_val_is_all_z(m->child_entry->current))
            continue;

        if (m->parent_msb >= 0) {
            /* Sliced: write child value into parent's slice (multi-word) */
            int msb = m->parent_msb;
            int lsb = m->parent_lsb;
            int sw = msb - lsb + 1;
            SimValue old = m->parent_entry->current;
            for (int b = 0; b < sw; b++) {
                int dst_bit = lsb + b;
                int dst_wi = dst_bit / 64;
                int dst_bi = dst_bit % 64;
                int src_wi = b / 64;
                int src_bi = b % 64;
                uint64_t bmask = (uint64_t)1 << dst_bi;
                m->parent_entry->current.val[dst_wi] = (m->parent_entry->current.val[dst_wi] & ~bmask) |
                    (((m->child_entry->current.val[src_wi] >> src_bi) & 1) << dst_bi);
                m->parent_entry->current.xmask[dst_wi] = (m->parent_entry->current.xmask[dst_wi] & ~bmask) |
                    (((m->child_entry->current.xmask[src_wi] >> src_bi) & 1) << dst_bi);
                m->parent_entry->current.zmask[dst_wi] = (m->parent_entry->current.zmask[dst_wi] & ~bmask) |
                    (((m->child_entry->current.zmask[src_wi] >> src_bi) & 1) << dst_bi);
            }
            int nw = (m->parent_entry->current.width + 63) / 64;
            if (!sim_val_same(old, m->parent_entry->current, nw)) {
                parent->settle_dirty = 1;
                int sig_idx = (int)(m->parent_entry - parent->signals);
                parent->sig_dirty[sig_idx] = 1;
            }
        } else {
            /* Full-width */
            int nw = (m->child_entry->current.width + 63) / 64;
            if (!sim_val_same(m->parent_entry->current, m->child_entry->current, nw)) {
                m->parent_entry->current = m->child_entry->current;
                parent->settle_dirty = 1;
                int sig_idx = (int)(m->parent_entry - parent->signals);
                parent->sig_dirty[sig_idx] = 1;
            }
        }
    }
}

/**
 * Propagate inputs to all children, recursively.
 */
static void propagate_to_all_children(SimContext *ctx) {
    for (int c = 0; c < ctx->num_children; c++) {
        if (!ctx->children[c].ctx) continue;
        propagate_to_child(ctx, &ctx->children[c]);
        propagate_to_all_children(ctx->children[c].ctx);
    }
}

/* ---- Combinational settling (hierarchical) ---- */

/**
 * Execute one round of async settling across the entire hierarchy:
 * 1. Run parent async block
 * 2. Propagate parent → children (skip if inputs unchanged)
 * 3. Run children async blocks (recursively)
 * 4. Propagate children → parent
 *
 * Uses dirty tracking to skip unchanged subtrees.
 */
static void settle_hierarchy_once(SimContext *ctx) {
    PERF_TIMER_START(PERF_SETTLE_HIERARCHY_ONCE);
    /* Execute this context's async block (chunk-based if available) */
    if (ctx->async_chunks && ctx->num_async_chunks > 0) {
        for (int i = 0; i < ctx->num_async_chunks; i++) {
            SimAsyncChunk *chunk = &ctx->async_chunks[i];
            /* Check if any read signal is dirty */
            int needs_exec = (chunk->num_reads == 0); /* always run constant chunks */
            for (int r = 0; r < chunk->num_reads; r++) {
                if (ctx->sig_dirty[chunk->read_indices[r]]) {
                    needs_exec = 1;
                    break;
                }
            }
            if (needs_exec) {
                sim_exec_stmt(ctx, chunk->stmt, 0);
            }
        }
    } else if (ctx->module->async_block) {
        sim_exec_stmt(ctx, ctx->module->async_block, 0);
    }

    /* Propagate to children and settle them */
    for (int c = 0; c < ctx->num_children; c++) {
        SimChildInstance *ci = &ctx->children[c];
        if (!ci->ctx) continue;

        int inputs_changed = propagate_to_child(ctx, ci);

        /* Skip child if its inputs didn't change AND it has no internal
         * dirty state (e.g., from NBA apply or previous iteration). */
        if (!inputs_changed && !ci->ctx->settle_dirty) {
            /* Still propagate runtime_error */
            if (ci->ctx->runtime_error)
                ctx->runtime_error = 1;
            continue;
        }

        ci->ctx->settle_dirty = 0; /* clear before recursing */
        settle_hierarchy_once(ci->ctx);

        /* Propagate runtime_error from child to parent */
        if (ci->ctx->runtime_error)
            ctx->runtime_error = 1;
        propagate_from_child(ctx, ci);
    }
    PERF_TIMER_STOP(PERF_SETTLE_HIERARCHY_ONCE);
}

/**
 * Clear per-signal dirty flags for this context and all children.
 */
static void clear_sig_dirty(SimContext *ctx) {
    if (ctx->sig_dirty)
        memset(ctx->sig_dirty, 0, (size_t)ctx->num_signals * sizeof(uint8_t));
    for (int c = 0; c < ctx->num_children; c++) {
        if (ctx->children[c].ctx)
            clear_sig_dirty(ctx->children[c].ctx);
    }
}

int sim_settle_combinational(SimContext *ctx, int max_iters) {
    PERF_TIMER_START(PERF_SETTLE_COMBINATIONAL);
    int has_any_async = (ctx->module->async_block != NULL);
    for (int c = 0; c < ctx->num_children; c++) {
        if (ctx->children[c].ctx && ctx->children[c].child_module &&
            ctx->children[c].child_module->async_block) {
            has_any_async = 1;
        }
    }
    if (!has_any_async) {
        PERF_TIMER_STOP(PERF_SETTLE_COMBINATIONAL);
        return 0;
    }

#ifdef TRACK_PERF
    int total_iters = 0;
    int total_changed = 0;
#endif

    for (int iter = 0; iter < max_iters; iter++) {
#ifdef TRACK_PERF
        total_iters++;
#endif

        /* Clear dirty flags before settling */
        ctx->settle_dirty = 0;

        /* Settle entire hierarchy once */
        settle_hierarchy_once(ctx);

        /* Convergence: if no signal changed anywhere, we're done */
        int changed = ctx->settle_dirty;

#ifdef TRACK_PERF
        total_changed += changed;
#endif

        if (!changed) {
            clear_sig_dirty(ctx);
            PERF_STATE_SETTLE_ITER(total_iters, total_changed);
            PERF_TIMER_STOP(PERF_SETTLE_COMBINATIONAL);
            return 0; /* converged */
        }
    }

    PERF_STATE_SETTLE_ITER(total_iters, total_changed);
    PERF_TIMER_STOP(PERF_SETTLE_COMBINATIONAL);
    return -1; /* oscillation */
}

/* ---- Synchronous domain execution (hierarchical) ---- */

/**
 * Execute sync domains in child instances that match the parent's clock.
 * The parent clock signal is connected to child clock ports via instance
 * connections. We find matching child clock domains and execute them.
 */
static void exec_children_sync_for_clock(SimContext *parent, int parent_clock_sig_id,
                                          int reset_active) {
    (void)reset_active;
    for (int c = 0; c < parent->num_children; c++) {
        SimChildInstance *ci = &parent->children[c];
        if (!ci->ctx || !ci->inst) continue;

        /* Find which child port is connected to the parent clock signal */
        int child_clock_port_id = -1;

        for (int p = 0; p < ci->inst->num_connections; p++) {
            const IR_InstanceConnection *conn = &ci->inst->connections[p];
            if (conn->parent_signal_id == parent_clock_sig_id) {
                child_clock_port_id = conn->child_port_id;
            }
        }
        if (child_clock_port_id < 0) continue;

        /* Find the child clock domain that uses this clock port */
        for (int d = 0; d < ci->child_module->num_clock_domains; d++) {
            const IR_ClockDomain *cd = &ci->child_module->clock_domains[d];
            if (cd->clock_signal_id != child_clock_port_id) continue;

            /* Check child reset */
            int child_reset = 0;
            if (cd->reset_signal_id >= 0) {
                SimSignalEntry *rst_entry = sim_ctx_lookup(ci->ctx, cd->reset_signal_id);
                if (rst_entry) {
                    uint64_t rv = rst_entry->current.val[0] & 1;
                    if (cd->reset_active == RESET_ACTIVE_HIGH && rv == 1)
                        child_reset = 1;
                    else if (cd->reset_active == RESET_ACTIVE_LOW && rv == 0)
                        child_reset = 1;
                }
            }

            if (child_reset) {
                /* Force child domain registers to reset values */
                for (int r = 0; r < cd->num_registers; r++) {
                    int reg_id = cd->register_ids[r];
                    SimSignalEntry *re = sim_ctx_lookup(ci->ctx, reg_id);
                    if (!re) continue;

                    const IR_Signal *sig = find_signal_in_ctx(ci->ctx, reg_id);
                    if (sig && sig->kind == SIG_REGISTER) {
                        SimValue rv = sim_val_from_words(
                            sig->u.reg.reset_value.words,
                            IR_LIT_WORDS,
                            sig->u.reg.reset_value.width);
                        if (rv.width != sig->width)
                            rv = sim_val_zext(rv, sig->width);
                        re->current = rv;
                        ci->ctx->settle_dirty = 1;
                        int sig_idx = (int)(re - ci->ctx->signals);
                        ci->ctx->sig_dirty[sig_idx] = 1;
                    }
                }
            } else {
                /* Execute child sync domain */
                sim_exec_sync_domain(ci->ctx, d);
            }

            /* Recursively handle grandchildren */
            exec_children_sync_for_clock(ci->ctx, child_clock_port_id, child_reset);

            /* Propagate runtime_error from child to parent */
            if (ci->ctx->runtime_error)
                parent->runtime_error = 1;
        }
    }
}

void sim_exec_sync_domain(SimContext *ctx, int domain_idx) {
    if (domain_idx < 0 || domain_idx >= ctx->module->num_clock_domains)
        return;

    const IR_ClockDomain *cd = &ctx->module->clock_domains[domain_idx];
    if (!cd->statements) return;

    PERF_TIMER_START(PERF_EXEC_SYNC_DOMAIN);
    sim_exec_stmt(ctx, cd->statements, 1); /* NBA mode */
    PERF_TIMER_STOP(PERF_EXEC_SYNC_DOMAIN);
}

void sim_exec_sync_domain_with_children(SimContext *ctx, int domain_idx,
                                         int reset_active) {
    if (domain_idx < 0 || domain_idx >= ctx->module->num_clock_domains)
        return;

    const IR_ClockDomain *cd = &ctx->module->clock_domains[domain_idx];

    /* Propagate inputs to children first */
    propagate_to_all_children(ctx);

    if (!reset_active) {
        /* Execute parent sync block */
        if (cd->statements)
            sim_exec_stmt(ctx, cd->statements, 1);
    }

    /* Execute matching child sync domains */
    exec_children_sync_for_clock(ctx, cd->clock_signal_id, reset_active);
}
