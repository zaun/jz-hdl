/**
 * @file sim_eval.h
 * @brief IR expression evaluator for simulation.
 */

#ifndef JZ_SIM_EVAL_H
#define JZ_SIM_EVAL_H

#include "sim_state.h"

SimValue sim_eval_expr(SimContext *ctx, const IR_Expr *expr);

#endif /* JZ_SIM_EVAL_H */
