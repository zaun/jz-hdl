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

        if (is_nba) entry->has_pending = 1;
    } else {
        /* Full-width write */
        if (is_nba) {
            entry->next = rhs_val;
            entry->has_pending = 1;
        } else {
            entry->current = rhs_val;
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

static const IR_Signal *find_signal_in_module(const IR_Module *mod, int signal_id) {
    for (int i = 0; i < mod->num_signals; i++) {
        if (mod->signals[i].id == signal_id)
            return &mod->signals[i];
    }
    return NULL;
}

/**
 * Propagate values from parent context to child instance inputs.
 * For each instance connection where the child port is INPUT or INOUT,
 * copy the parent signal value (possibly sliced) to the child port entry.
 */
static void propagate_to_child(SimContext *parent, SimChildInstance *ci) {
    if (!ci->ctx || !ci->inst) return;

    for (int p = 0; p < ci->inst->num_connections; p++) {
        const IR_InstanceConnection *conn = &ci->inst->connections[p];
        if (conn->const_expr) continue; /* constant binding, skip */

        const IR_Signal *child_sig = find_signal_in_module(ci->child_module, conn->child_port_id);
        if (!child_sig || child_sig->kind != SIG_PORT) continue;

        if (child_sig->u.port.direction == PORT_IN ||
            child_sig->u.port.direction == PORT_INOUT) {
            SimSignalEntry *parent_entry = sim_ctx_lookup(parent, conn->parent_signal_id);
            SimSignalEntry *child_entry = sim_ctx_lookup(ci->ctx, conn->child_port_id);
            if (!parent_entry || !child_entry) continue;

            if (conn->parent_msb >= 0 && conn->parent_lsb >= 0) {
                /* Sliced connection: extract slice from parent */
                child_entry->current = sim_val_slice(parent_entry->current,
                                                     conn->parent_msb,
                                                     conn->parent_lsb);
            } else {
                /* Full-width connection */
                child_entry->current = parent_entry->current;
                if (child_entry->current.width != child_sig->width) {
                    child_entry->current.width = child_sig->width;
                    child_entry->current = sim_val_mask(child_entry->current);
                }
            }
        }
    }
}

/**
 * Propagate values from child instance outputs back to parent context.
 * For each instance connection where the child port is OUTPUT or INOUT,
 * copy the child port value back to the parent signal.
 */
static void propagate_from_child(SimContext *parent, SimChildInstance *ci) {
    if (!ci->ctx || !ci->inst) return;

    for (int p = 0; p < ci->inst->num_connections; p++) {
        const IR_InstanceConnection *conn = &ci->inst->connections[p];
        if (conn->const_expr) continue;

        const IR_Signal *child_sig = find_signal_in_module(ci->child_module, conn->child_port_id);
        if (!child_sig || child_sig->kind != SIG_PORT) continue;

        if (child_sig->u.port.direction == PORT_OUT ||
            child_sig->u.port.direction == PORT_INOUT) {
            SimSignalEntry *parent_entry = sim_ctx_lookup(parent, conn->parent_signal_id);
            SimSignalEntry *child_entry = sim_ctx_lookup(ci->ctx, conn->child_port_id);
            if (!parent_entry || !child_entry) continue;

            /* For INOUT: don't overwrite parent with z */
            if (child_sig->u.port.direction == PORT_INOUT &&
                sim_val_is_all_z(child_entry->current)) {
                continue;
            }

            if (conn->parent_msb >= 0 && conn->parent_lsb >= 0) {
                /* Sliced: write child value into parent's slice (multi-word) */
                int msb = conn->parent_msb;
                int lsb = conn->parent_lsb;
                int sw = msb - lsb + 1;
                for (int b = 0; b < sw; b++) {
                    int dst_bit = lsb + b;
                    int dst_wi = dst_bit / 64;
                    int dst_bi = dst_bit % 64;
                    int src_wi = b / 64;
                    int src_bi = b % 64;
                    uint64_t bmask = (uint64_t)1 << dst_bi;
                    parent_entry->current.val[dst_wi] = (parent_entry->current.val[dst_wi] & ~bmask) |
                        (((child_entry->current.val[src_wi] >> src_bi) & 1) << dst_bi);
                    parent_entry->current.xmask[dst_wi] = (parent_entry->current.xmask[dst_wi] & ~bmask) |
                        (((child_entry->current.xmask[src_wi] >> src_bi) & 1) << dst_bi);
                    parent_entry->current.zmask[dst_wi] = (parent_entry->current.zmask[dst_wi] & ~bmask) |
                        (((child_entry->current.zmask[src_wi] >> src_bi) & 1) << dst_bi);
                }
            } else {
                /* Full-width */
                parent_entry->current = child_entry->current;
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
 * 2. Propagate parent → children
 * 3. Run children async blocks (recursively)
 * 4. Propagate children → parent
 */
static void settle_hierarchy_once(SimContext *ctx) {
    PERF_TIMER_START(PERF_SETTLE_HIERARCHY_ONCE);
    /* Execute this context's async block */
    if (ctx->module->async_block) {
        sim_exec_stmt(ctx, ctx->module->async_block, 0);
    }

    /* Propagate to children and settle them */
    for (int c = 0; c < ctx->num_children; c++) {
        if (!ctx->children[c].ctx) continue;
        propagate_to_child(ctx, &ctx->children[c]);
        settle_hierarchy_once(ctx->children[c].ctx);
        /* Propagate runtime_error from child to parent */
        if (ctx->children[c].ctx->runtime_error)
            ctx->runtime_error = 1;
        propagate_from_child(ctx, &ctx->children[c]);
    }
    PERF_TIMER_STOP(PERF_SETTLE_HIERARCHY_ONCE);
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

        /* Snapshot current values (parent only for convergence check) */
        int n = ctx->num_signals;
        SimValue *snap = (SimValue *)malloc((size_t)n * sizeof(SimValue));
        if (!snap) {
            PERF_TIMER_STOP(PERF_SETTLE_COMBINATIONAL);
            return -1;
        }

        for (int i = 0; i < n; i++) {
            snap[i] = ctx->signals[i].current;
        }

        /* Settle entire hierarchy once */
        settle_hierarchy_once(ctx);

        /* Check convergence (parent signals) */
        int changed = 0;
        for (int i = 0; i < n; i++) {
            if (!sim_val_equal(ctx->signals[i].current, snap[i])) {
                changed++;
#ifndef TRACK_PERF
                break;
#endif
            }
        }

#ifdef TRACK_PERF
        total_changed += changed;
#endif
        free(snap);

        if (!changed) {
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

                    const IR_Signal *sig = find_signal_in_module(ci->child_module, reg_id);
                    if (sig && sig->kind == SIG_REGISTER) {
                        SimValue rv = sim_val_from_words(
                            sig->u.reg.reset_value.words,
                            IR_LIT_WORDS,
                            sig->u.reg.reset_value.width);
                        if (rv.width != sig->width)
                            rv = sim_val_zext(rv, sig->width);
                        re->current = rv;
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
