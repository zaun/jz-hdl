/**
 * @file sim_exec.h
 * @brief IR statement executor for simulation.
 */

#ifndef JZ_SIM_EXEC_H
#define JZ_SIM_EXEC_H

#include "sim_state.h"

/** Maximum delta cycles for combinational settling before reporting
 *  a combinational loop runtime error (SE-001). */
#define SIM_SETTLE_MAX_ITERS 100

/**
 * Execute an IR statement.
 * @param is_nba 1 for synchronous (NBA) mode, 0 for asynchronous (immediate).
 */
void sim_exec_stmt(SimContext *ctx, const IR_Stmt *stmt, int is_nba);

/**
 * Repeatedly execute the module's async_block until quiescence.
 * @return 0 on convergence, -1 on oscillation (after max_iters).
 */
int sim_settle_combinational(SimContext *ctx, int max_iters);

/**
 * Execute a clock domain's synchronous block in NBA mode.
 */
void sim_exec_sync_domain(SimContext *ctx, int domain_idx);

/**
 * Execute a clock domain's synchronous block and recursively execute
 * matching sync blocks in child instances.
 */
void sim_exec_sync_domain_with_children(SimContext *ctx, int domain_idx,
                                         int reset_active);

#endif /* JZ_SIM_EXEC_H */
