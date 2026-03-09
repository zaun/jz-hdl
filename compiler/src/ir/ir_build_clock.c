/**
 * @file ir_build_clock.c
 * @brief Clock domain and CDC IR construction.
 *
 * This file contains helpers for lowering SYNCHRONOUS and CDC blocks from
 * the AST into IR_ClockDomain and IR_CDC structures.
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "ir_internal.h"

static IR_Expr *ir_make_signal_ref_expr(JZArena *arena,
                                        int signal_id,
                                        int width,
                                        int source_line)
{
    if (!arena) {
        return NULL;
    }
    IR_Expr *expr = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
    if (!expr) {
        return NULL;
    }
    memset(expr, 0, sizeof(*expr));
    expr->kind = EXPR_SIGNAL_REF;
    expr->width = width;
    expr->source_line = source_line;
    expr->u.signal_ref.signal_id = signal_id;
    return expr;
}

static IR_Expr *ir_make_literal_expr(JZArena *arena,
                                     IR_Literal lit,
                                     int source_line)
{
    if (!arena) {
        return NULL;
    }
    IR_Expr *expr = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
    if (!expr) {
        return NULL;
    }
    memset(expr, 0, sizeof(*expr));
    expr->kind = EXPR_LITERAL;
    expr->width = lit.width;
    expr->source_line = source_line;
    expr->u.literal.literal = lit;
    return expr;
}

static IR_Expr *ir_build_reset_condition(JZArena *arena,
                                         IR_Module *mod,
                                         const IR_ClockDomain *cd,
                                         int source_line)
{
    if (!arena || !mod || !cd || cd->reset_signal_id < 0) {
        return NULL;
    }

    int width = 1;
    IR_Signal *sig = ir_find_signal_by_id(mod, cd->reset_signal_id);
    if (sig && sig->width > 0) {
        width = sig->width;
    }

    IR_Expr *rst = ir_make_signal_ref_expr(arena,
                                           cd->reset_signal_id,
                                           width,
                                           source_line);
    if (!rst) {
        return NULL;
    }

    if (cd->reset_active == RESET_ACTIVE_LOW) {
        IR_Expr *not_rst = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
        if (!not_rst) {
            return rst;
        }
        memset(not_rst, 0, sizeof(*not_rst));
        not_rst->kind = EXPR_LOGICAL_NOT;
        not_rst->width = 1;
        not_rst->source_line = source_line;
        not_rst->u.unary.operand = rst;
        return not_rst;
    }

    return rst;
}

static IR_Stmt *ir_build_reset_assignments_block(JZArena *arena,
                                                 IR_Module *mod,
                                                 const IR_ClockDomain *cd,
                                                 int *next_assign_id,
                                                 int source_line)
{
    if (!arena || !mod || !cd || !next_assign_id) {
        return NULL;
    }
    if (cd->num_registers <= 0) {
        return NULL;
    }

    IR_Stmt *block = (IR_Stmt *)jz_arena_alloc(arena, sizeof(IR_Stmt));
    if (!block) {
        return NULL;
    }
    memset(block, 0, sizeof(*block));
    block->kind = STMT_BLOCK;
    block->source_line = source_line;

    IR_Stmt *stmts = (IR_Stmt *)jz_arena_alloc(arena, sizeof(IR_Stmt) * (size_t)cd->num_registers);
    if (!stmts) {
        return NULL;
    }
    memset(stmts, 0, sizeof(IR_Stmt) * (size_t)cd->num_registers);
    block->u.block.stmts = stmts;
    block->u.block.count = cd->num_registers;

    for (int i = 0; i < cd->num_registers; ++i) {
        int reg_id = cd->register_ids[i];
        const IR_Signal *sig = ir_find_signal_by_id(mod, reg_id);

        IR_Stmt *stmt = &stmts[i];
        stmt->kind = STMT_ASSIGNMENT;
        stmt->source_line = source_line;

        IR_Assignment *a = &stmt->u.assign;
        a->id = (*next_assign_id)++;
        a->lhs_signal_id = reg_id;
        a->rhs = sig ? ir_make_literal_expr(arena, sig->u.reg.reset_value, source_line) : NULL;
        a->kind = ASSIGN_RECEIVE;
        a->is_sliced = false;
        a->lhs_msb = 0;
        a->lhs_lsb = 0;
        a->source_line = source_line;
    }

    return block;
}

static IR_Stmt *ir_wrap_sync_block_with_reset(JZArena *arena,
                                              IR_Module *mod,
                                              const IR_ClockDomain *cd,
                                              IR_Stmt *body,
                                              int *next_assign_id,
                                              int source_line)
{
    if (!arena || !mod || !cd || cd->reset_signal_id < 0) {
        return body;
    }

    IR_Expr *cond = ir_build_reset_condition(arena, mod, cd, source_line);
    if (!cond) {
        return body;
    }

    IR_Stmt *reset_block = ir_build_reset_assignments_block(arena,
                                                            mod,
                                                            cd,
                                                            next_assign_id,
                                                            source_line);
    IR_Stmt *if_stmt = (IR_Stmt *)jz_arena_alloc(arena, sizeof(IR_Stmt));
    if (!if_stmt) {
        return body;
    }
    memset(if_stmt, 0, sizeof(*if_stmt));
    if_stmt->kind = STMT_IF;
    if_stmt->source_line = source_line;

    if_stmt->u.if_stmt.condition = cond;
    if_stmt->u.if_stmt.then_block = reset_block;
    if_stmt->u.if_stmt.elif_chain = NULL;
    if_stmt->u.if_stmt.else_block = body;

    return if_stmt;
}

/**
 * @brief Parse SYNCHRONOUS(...) header parameters into IR_ClockDomain fields.
 *
 * Mirrors the semantic expectations for clock domains, but performs parsing
 * locally so that IR construction remains independent of the semantic
 * clock-domain pass.
 *
 * @param scope               Module scope for symbol resolution.
 * @param block               AST block node representing SYNCHRONOUS.
 * @param out_clock_signal_id Output clock signal semantic ID.
 * @param out_edge            Output clock edge.
 * @param out_reset_signal_id Output reset signal semantic ID.
 * @param out_reset_active    Output reset polarity.
 * @param out_reset_type      Output reset type.
 */
static void ir_parse_sync_header(const JZModuleScope *scope,
                          JZASTNode *block,
                          int *out_clock_signal_id,
                          IR_ClockEdge *out_edge,
                          int *out_reset_signal_id,
                          IR_ResetPolarity *out_reset_active,
                          IR_ResetType *out_reset_type)
{
    if (!scope || !block) return;

    for (size_t i = 0; i < block->child_count; ++i) {
        JZASTNode *child = block->children[i];
        if (!child || child->type != JZ_AST_SYNC_PARAM || !child->name) continue;
        if (child->child_count == 0) continue;
        JZASTNode *val = child->children[0];
        if (!val) continue;

        const char *param_name = child->name;

        if (strcmp(param_name, "CLK") == 0) {
            if (val->type == JZ_AST_EXPR_IDENTIFIER && val->name) {
                const JZSymbol *sym = module_scope_lookup(scope, val->name);
                if (sym && sym->id >= 0) {
                    if (sym->kind == JZ_SYM_PORT ||
                        sym->kind == JZ_SYM_WIRE ||
                        sym->kind == JZ_SYM_REGISTER ||
                        sym->kind == JZ_SYM_LATCH) {
                        if (out_clock_signal_id) {
                            *out_clock_signal_id = sym->id;
                        }
                    }
                }
            }
        } else if (strcmp(param_name, "EDGE") == 0) {
            if ((val->type == JZ_AST_EXPR_IDENTIFIER ||
                 val->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER) &&
                val->name && out_edge) {
                const char *s = val->name;
                /* Accept both spec spellings (Rising/Falling/Both) and the
                 * legacy all-caps forms (RISING/FALLING/BOTH).
                 */
                if (!strcmp(s, "RISING") || !strcmp(s, "Rising")) {
                    *out_edge = EDGE_RISING;
                } else if (!strcmp(s, "FALLING") || !strcmp(s, "Falling")) {
                    *out_edge = EDGE_FALLING;
                } else if (!strcmp(s, "BOTH") || !strcmp(s, "Both")) {
                    *out_edge = EDGE_BOTH;
                }
            }
        } else if (strcmp(param_name, "RESET") == 0) {
            if (val->type == JZ_AST_EXPR_IDENTIFIER && val->name && out_reset_signal_id) {
                const JZSymbol *sym = module_scope_lookup(scope, val->name);
                if (sym && sym->id >= 0) {
                    if (sym->kind == JZ_SYM_PORT ||
                        sym->kind == JZ_SYM_WIRE ||
                        sym->kind == JZ_SYM_REGISTER ||
                        sym->kind == JZ_SYM_LATCH) {
                        *out_reset_signal_id = sym->id;
                    }
                }
            }
        } else if (strcmp(param_name, "RESET_ACTIVE") == 0) {
            if ((val->type == JZ_AST_EXPR_IDENTIFIER ||
                 val->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER) &&
                val->name && out_reset_active) {
                const char *s = val->name;
                /* Accept spec spelling (High/Low) plus legacy ACTIVE_* and
                 * all-caps LOW/HIGH for backward compatibility.
                 */
                if (!strcmp(s, "ACTIVE_LOW") || !strcmp(s, "LOW") || !strcmp(s, "Low")) {
                    *out_reset_active = RESET_ACTIVE_LOW;
                } else if (!strcmp(s, "ACTIVE_HIGH") || !strcmp(s, "HIGH") || !strcmp(s, "High")) {
                    *out_reset_active = RESET_ACTIVE_HIGH;
                }
            }
        } else if (strcmp(param_name, "RESET_TYPE") == 0) {
            if ((val->type == JZ_AST_EXPR_IDENTIFIER ||
                 val->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER) &&
                val->name && out_reset_type) {
                const char *s = val->name;
                if (!strcmp(s, "CLOCKED") || !strcmp(s, "Clocked")) {
                    *out_reset_type = RESET_CLOCKED;
                } else if (!strcmp(s, "IMMEDIATE") || !strcmp(s, "Immediate")) {
                    *out_reset_type = RESET_IMMEDIATE;
                }
            }
        }
    }
}

/**
 * @brief Walk an IR statement tree and collect registers written in a clock domain.
 *
 * Registers discovered here are:
 * - Added to the clock domain's register list
 * - Assigned a home_clock_domain_id if previously unset
 *
 * @param stmt             IR statement to inspect.
 * @param mod              Owning IR module.
 * @param clock_domain_id  Clock domain identifier.
 * @param ids_buf          Buffer to append unique register IDs into.
 */
static void ir_collect_register_ids_from_stmt(const IR_Stmt *stmt,
                                      IR_Module *mod,
                                      int clock_domain_id,
                                      JZBuffer *ids_buf)
{
    if (!stmt || !mod || !ids_buf) return;

    switch (stmt->kind) {
    case STMT_ASSIGNMENT: {
        const IR_Assignment *a = &stmt->u.assign;
        IR_Signal *sig = ir_find_signal_by_id(mod, a->lhs_signal_id);
        if (!sig || sig->kind != SIG_REGISTER) {
            return;
        }
        if (sig->u.reg.home_clock_domain_id == -1) {
            sig->u.reg.home_clock_domain_id = clock_domain_id;
        }
        int id = sig->id;
        int *arr = (int *)ids_buf->data;
        size_t count = ids_buf->len / sizeof(int);
        for (size_t i = 0; i < count; ++i) {
            if (arr[i] == id) {
                return; /* already recorded */
            }
        }
        (void)jz_buf_append(ids_buf, &id, sizeof(int));
        return;
    }

    case STMT_IF: {
        const IR_IfStmt *ifs = &stmt->u.if_stmt;
        if (ifs->then_block) {
            ir_collect_register_ids_from_stmt(ifs->then_block, mod, clock_domain_id, ids_buf);
        }
        /* Walk the elif chain iteratively.  Each elif is a STMT_IF whose
         * then_block holds that branch's body.  We must NOT call the full
         * recursive function on the elif node itself, because that would
         * re-walk the remaining elif_chain inside the recursion AND then
         * the while loop would walk it again — leading to exponential
         * (O(2^N)) processing time for N elif branches.
         */
        const IR_Stmt *elif = ifs->elif_chain;
        while (elif) {
            if (elif->kind == STMT_IF) {
                if (elif->u.if_stmt.then_block) {
                    ir_collect_register_ids_from_stmt(elif->u.if_stmt.then_block,
                                                      mod, clock_domain_id, ids_buf);
                }
                if (elif->u.if_stmt.else_block) {
                    ir_collect_register_ids_from_stmt(elif->u.if_stmt.else_block,
                                                      mod, clock_domain_id, ids_buf);
                }
                elif = elif->u.if_stmt.elif_chain;
            } else {
                /* Non-IF node in elif chain (shouldn't happen, but handle safely). */
                ir_collect_register_ids_from_stmt(elif, mod, clock_domain_id, ids_buf);
                break;
            }
        }
        if (ifs->else_block) {
            ir_collect_register_ids_from_stmt(ifs->else_block, mod, clock_domain_id, ids_buf);
        }
        return;
    }

    case STMT_SELECT: {
        const IR_SelectStmt *sel = &stmt->u.select_stmt;
        for (int i = 0; i < sel->num_cases; ++i) {
            if (sel->cases[i].body) {
                ir_collect_register_ids_from_stmt(sel->cases[i].body,
                                                  mod,
                                                  clock_domain_id,
                                                  ids_buf);
            }
        }
        return;
    }

    case STMT_BLOCK:
        for (int i = 0; i < stmt->u.block.count; ++i) {
            ir_collect_register_ids_from_stmt(&stmt->u.block.stmts[i],
                                              mod,
                                              clock_domain_id,
                                              ids_buf);
        }
        return;

    default:
        return;
    }
}

/**
 * @brief Build IR clock domains for a module.
 *
 * Walks SYNCHRONOUS blocks in the module AST, lowers each to an
 * IR_ClockDomain, builds its statement tree, and collects all registers
 * written under that clock.
 *
 * @param scope            Module scope.
 * @param mod              IR module.
 * @param arena            Arena for allocation.
 * @param project_symbols  Project-level symbols.
 * @param bus_map          BUS signal mapping array (may be NULL).
 * @param bus_map_count    Number of bus mapping entries.
 * @param diagnostics      Diagnostic sink.
 * @return 0 on success, non-zero on failure.
 */
int ir_build_clock_domains_for_module(const JZModuleScope *scope,
                                     IR_Module *mod,
                                     JZArena *arena,
                                     const JZBuffer *project_symbols,
                                     const IR_BusSignalMapping *bus_map,
                                     int bus_map_count,
                                     JZDiagnosticList *diagnostics)
{
    if (!scope || !scope->node || !mod || !arena) {
        return -1;
    }

    JZASTNode *ast_mod = scope->node;

    /* First pass: count SYNCHRONOUS blocks. */
    int domain_count = 0;
    for (size_t i = 0; i < ast_mod->child_count; ++i) {
        JZASTNode *child = ast_mod->children[i];
        if (!child || child->type != JZ_AST_BLOCK || !child->block_kind) continue;
        if (strcmp(child->block_kind, "SYNCHRONOUS") == 0) {
            domain_count++;
        }
    }

    if (domain_count == 0) {
        mod->clock_domains = NULL;
        mod->num_clock_domains = 0;
        return 0;
    }

    IR_ClockDomain *domains =
        (IR_ClockDomain *)jz_arena_alloc(arena, sizeof(IR_ClockDomain) * (size_t)domain_count);
    if (!domains) {
        return -1;
    }
    memset(domains, 0, sizeof(IR_ClockDomain) * (size_t)domain_count);

    /* Per-domain bookkeeping for deferred reset wrapping. */
    int *domain_next_assign_ids = (int *)jz_arena_alloc(
        arena, sizeof(int) * (size_t)domain_count);
    int *domain_source_lines = (int *)jz_arena_alloc(
        arena, sizeof(int) * (size_t)domain_count);
    if (!domain_next_assign_ids || !domain_source_lines) {
        return -1;
    }

    /* First pass: build each clock domain's statements, collect written
       registers, and build sensitivity lists. Reset wrapping is deferred
       so that orphan registers can be adopted first. */
    int di = 0;
    for (size_t i = 0; i < ast_mod->child_count && di < domain_count; ++i) {
        JZASTNode *blk = ast_mod->children[i];
        if (!blk || blk->type != JZ_AST_BLOCK || !blk->block_kind) continue;
        if (strcmp(blk->block_kind, "SYNCHRONOUS") != 0) continue;

        IR_ClockDomain *cd = &domains[di];
        cd->id = di;
        cd->clock_signal_id = -1;
        cd->edge = EDGE_RISING;
        cd->register_ids = NULL;
        cd->num_registers = 0;
        cd->reset_signal_id = -1;
        cd->reset_sync_signal_id = -1;
        cd->reset_active = RESET_ACTIVE_LOW;
        cd->reset_type = RESET_CLOCKED;
        cd->statements = NULL;

        ir_parse_sync_header(scope,
                             blk,
                             &cd->clock_signal_id,
                             &cd->edge,
                             &cd->reset_signal_id,
                             &cd->reset_active,
                             &cd->reset_type);

        int next_assign_id = 0;
        cd->statements = ir_build_block_from_node(arena,
                                                  blk,
                                                  scope,
                                                  project_symbols,
                                                  bus_map,
                                                  bus_map_count,
                                                  diagnostics,
                                                  &next_assign_id);

        /* Collect registers written in this domain and update home_clock_domain_id. */
        if (cd->statements) {
            JZBuffer reg_ids = (JZBuffer){0};
            ir_collect_register_ids_from_stmt(cd->statements,
                                              mod,
                                              cd->id,
                                              &reg_ids);
            size_t reg_count = reg_ids.len / sizeof(int);
            if (reg_count > 0) {
                cd->register_ids = (int *)jz_arena_alloc(arena, sizeof(int) * reg_count);
                if (!cd->register_ids) {
                    jz_buf_free(&reg_ids);
                    return -1;
                }
                memcpy(cd->register_ids, reg_ids.data, sizeof(int) * reg_count);
                cd->num_registers = (int)reg_count;
            }
            jz_buf_free(&reg_ids);
        }

        domain_next_assign_ids[di] = next_assign_id;
        domain_source_lines[di] = blk->loc.line;

        /* Build the sensitivity list for this clock domain. */
        {
            int max_entries = (cd->edge == EDGE_BOTH) ? 3 : 2;
            IR_SensitivityEntry *sens = (IR_SensitivityEntry *)jz_arena_alloc(
                arena, sizeof(IR_SensitivityEntry) * (size_t)max_entries);
            if (!sens) {
                return -1;
            }
            int ns = 0;

            if (cd->edge == EDGE_BOTH) {
                /* EDGE_BOTH: posedge + negedge on the same clock */
                sens[ns].signal_id = cd->clock_signal_id;
                sens[ns].edge = EDGE_RISING;
                ns++;
                sens[ns].signal_id = cd->clock_signal_id;
                sens[ns].edge = EDGE_FALLING;
                ns++;
            } else {
                sens[ns].signal_id = cd->clock_signal_id;
                sens[ns].edge = cd->edge;
                ns++;
            }

            /* Add reset to sensitivity list for async (IMMEDIATE) resets. */
            if (cd->reset_signal_id >= 0 && cd->reset_type == RESET_IMMEDIATE) {
                sens[ns].signal_id = cd->reset_signal_id;
                sens[ns].edge = (cd->reset_active == RESET_ACTIVE_HIGH)
                              ? EDGE_RISING : EDGE_FALLING;
                ns++;
            }

            cd->sensitivity_list = sens;
            cd->num_sensitivity = ns;
        }

        di++;
    }

    mod->clock_domains = domains;
    mod->num_clock_domains = di;

    /* Adopt orphan registers: any register with a reset value that was never
       written in a SYNCHRONOUS block still needs a clock domain so that its
       reset assignment is emitted.  Assign it to the first domain that has
       a reset signal. */
    for (int si = 0; si < mod->num_signals; ++si) {
        IR_Signal *sig = &mod->signals[si];
        if (sig->kind != SIG_REGISTER) continue;
        if (sig->u.reg.home_clock_domain_id != -1) continue;
        if (sig->u.reg.reset_value.width <= 0) continue;
        if (sig->source_line <= 0) continue; /* skip auto-generated signals */

        /* Find the first clock domain with a reset signal. */
        for (int ci = 0; ci < di; ++ci) {
            if (domains[ci].reset_signal_id >= 0) {
                sig->u.reg.home_clock_domain_id = domains[ci].id;

                /* Append to that domain's register_ids. */
                int new_count = domains[ci].num_registers + 1;
                int *new_ids = (int *)jz_arena_alloc(arena, sizeof(int) * (size_t)new_count);
                if (!new_ids) return -1;
                if (domains[ci].num_registers > 0) {
                    memcpy(new_ids, domains[ci].register_ids,
                           sizeof(int) * (size_t)domains[ci].num_registers);
                }
                new_ids[new_count - 1] = sig->id;
                domains[ci].register_ids = new_ids;
                domains[ci].num_registers = new_count;
                break;
            }
        }
    }

    /* Wrap each domain's statements with reset logic (deferred to here so
       that orphan registers are included in the reset block). */
    for (int ci = 0; ci < di; ++ci) {
        IR_ClockDomain *cd = &domains[ci];
        if (cd->reset_signal_id >= 0) {
            cd->statements = ir_wrap_sync_block_with_reset(arena,
                                                           mod,
                                                           cd,
                                                           cd->statements,
                                                           &domain_next_assign_ids[ci],
                                                           domain_source_lines[ci]);
        }
    }

    return 0;
}

/**
 * @brief Build CDC crossings for a module.
 *
 * Lowers CDC blocks into IR_CDC entries, resolves source/destination
 * clock domains, and ensures alias widths are materialized.
 *
 * @param scope  Module scope.
 * @param mod    IR module.
 * @param arena  Arena for allocation.
 * @return 0 on success, non-zero on failure.
 */
int ir_build_cdc_for_module(const JZModuleScope *scope,
                           IR_Module *mod,
                           JZArena *arena)
{
    if (!scope || !scope->node || !mod || !arena) {
        return -1;
    }

    JZASTNode *ast_mod = scope->node;

    /* First pass: count CDC declarations. */
    int cdc_count = 0;
    for (size_t ci = 0; ci < ast_mod->child_count; ++ci) {
        JZASTNode *blk = ast_mod->children[ci];
        if (!blk || blk->type != JZ_AST_BLOCK || !blk->block_kind) continue;
        if (strcmp(blk->block_kind, "CDC") != 0) continue;
        for (size_t j = 0; j < blk->child_count; ++j) {
            JZASTNode *cdc = blk->children[j];
            if (!cdc || cdc->type != JZ_AST_CDC_DECL) continue;
            cdc_count++;
        }
    }

    if (cdc_count == 0) {
        mod->cdc_crossings = NULL;
        mod->num_cdc_crossings = 0;
        return 0;
    }

    IR_CDC *cdcs = (IR_CDC *)jz_arena_alloc(arena, sizeof(IR_CDC) * (size_t)cdc_count);
    if (!cdcs) {
        return -1;
    }
    memset(cdcs, 0, sizeof(IR_CDC) * (size_t)cdc_count);

    int idx = 0;
    for (size_t ci = 0; ci < ast_mod->child_count && idx < cdc_count; ++ci) {
        JZASTNode *blk = ast_mod->children[ci];
        if (!blk || blk->type != JZ_AST_BLOCK || !blk->block_kind) continue;
        if (strcmp(blk->block_kind, "CDC") != 0) continue;

        for (size_t j = 0; j < blk->child_count && idx < cdc_count; ++j) {
            JZASTNode *cdc = blk->children[j];
            if (!cdc || cdc->type != JZ_AST_CDC_DECL) continue;

            IR_CDC *ir_cdc = &cdcs[idx];
            ir_cdc->id = idx;

            /* Map CDC kind from block_kind. */
            ir_cdc->type = CDC_BUS;
            if (cdc->block_kind) {
                if (strcmp(cdc->block_kind, "BIT") == 0) {
                    ir_cdc->type = CDC_BIT;
                } else if (strcmp(cdc->block_kind, "BUS") == 0) {
                    ir_cdc->type = CDC_BUS;
                } else if (strcmp(cdc->block_kind, "FIFO") == 0) {
                    ir_cdc->type = CDC_FIFO;
                } else if (strcmp(cdc->block_kind, "HANDSHAKE") == 0) {
                    ir_cdc->type = CDC_HANDSHAKE;
                } else if (strcmp(cdc->block_kind, "PULSE") == 0) {
                    ir_cdc->type = CDC_PULSE;
                } else if (strcmp(cdc->block_kind, "MCP") == 0) {
                    ir_cdc->type = CDC_MCP;
                } else if (strcmp(cdc->block_kind, "RAW") == 0) {
                    ir_cdc->type = CDC_RAW;
                }
            }

            /* Source register: child[0] identifier or slice. */
            ir_cdc->source_reg_id = -1;
            ir_cdc->source_msb = -1;
            ir_cdc->source_lsb = -1;
            if (cdc->child_count >= 1) {
                JZASTNode *src_node = cdc->children[0];
                JZASTNode *src_id = src_node;

                /* If the source is a slice, extract base identifier and bit indices. */
                if (src_node && src_node->type == JZ_AST_EXPR_SLICE &&
                    src_node->child_count >= 3) {
                    src_id = src_node->children[0]; /* base identifier */
                    JZASTNode *msb_node = src_node->children[1];
                    JZASTNode *lsb_node = src_node->children[2];
                    if (msb_node && msb_node->text) {
                        char *end = NULL;
                        long v = strtol(msb_node->text, &end, 10);
                        if (end && *end == '\0' && v >= 0) {
                            ir_cdc->source_msb = (int)v;
                        }
                    }
                    if (lsb_node && lsb_node->text) {
                        char *end = NULL;
                        long v = strtol(lsb_node->text, &end, 10);
                        if (end && *end == '\0' && v >= 0) {
                            ir_cdc->source_lsb = (int)v;
                        }
                    }
                }

                if (src_id && src_id->type == JZ_AST_EXPR_IDENTIFIER && src_id->name) {
                    const JZSymbol *sym = module_scope_lookup_kind(scope,
                                                                    src_id->name,
                                                                    JZ_SYM_REGISTER);
                    if (sym && sym->id >= 0) {
                        ir_cdc->source_reg_id = sym->id;
                    }
                }
            }

            /* Destination alias: child[2] identifier name. */
            ir_cdc->dest_alias_name = NULL;
            if (cdc->child_count >= 3) {
                JZASTNode *dst_id = cdc->children[2];
                if (dst_id && dst_id->type == JZ_AST_EXPR_IDENTIFIER && dst_id->name) {
                    ir_cdc->dest_alias_name = ir_strdup_arena(arena, dst_id->name);
                }
            }
            if (!ir_cdc->dest_alias_name) {
                ir_cdc->dest_alias_name = ir_strdup_arena(arena, "");
            }

            /* Map source and destination clocks to IR_ClockDomain IDs when
             * possible by matching the referenced clock port to
             * IR_ClockDomain.clock_signal_id.
             */
            ir_cdc->source_clock_id = -1;
            ir_cdc->dest_clock_id = -1;

            /*
             * Ensure the CDC alias nets in IR_Signal have concrete widths that
             * match the source register. The semantic layer currently records
             * some CDC alias wires (e.g., io_view/cpu_view) with width 0,
             * relying on use sites to define the width. For IR consumers and
             * backends it is more robust to materialize the width here.
             */
            if (ir_cdc->source_reg_id >= 0 && ir_cdc->dest_alias_name && ir_cdc->dest_alias_name[0] != '\0') {
                IR_Signal *src_sig = ir_find_signal_by_id(mod, ir_cdc->source_reg_id);
                if (src_sig && src_sig->width > 0) {
                    /* When a bit-select is present, use the slice width
                     * instead of the full source register width. */
                    int effective_width = src_sig->width;
                    if (ir_cdc->source_msb >= 0 && ir_cdc->source_lsb >= 0) {
                        effective_width = ir_cdc->source_msb - ir_cdc->source_lsb + 1;
                    }
                    const JZSymbol *dst_sym = module_scope_lookup_kind(scope,
                                                                        ir_cdc->dest_alias_name,
                                                                        JZ_SYM_WIRE);
                    if (dst_sym && dst_sym->id >= 0) {
                        IR_Signal *alias_sig = ir_find_signal_by_id(mod, dst_sym->id);
                        if (alias_sig && alias_sig->width <= 0) {
                            alias_sig->width = effective_width;
                        }
                    }
                }
            }

            int src_clk_sig_id = -1;
            int dst_clk_sig_id = -1;

            if (cdc->child_count >= 2) {
                JZASTNode *src_clk = cdc->children[1];
                if (src_clk && src_clk->type == JZ_AST_EXPR_IDENTIFIER && src_clk->name) {
                    const JZSymbol *clk_sym = module_scope_lookup_kind(scope,
                                                                       src_clk->name,
                                                                       JZ_SYM_PORT);
                    if (clk_sym && clk_sym->id >= 0) {
                        src_clk_sig_id = clk_sym->id;
                    }
                }
            }

            if (cdc->child_count >= 4) {
                JZASTNode *dst_clk = cdc->children[3];
                if (dst_clk && dst_clk->type == JZ_AST_EXPR_IDENTIFIER && dst_clk->name) {
                    const JZSymbol *clk_sym = module_scope_lookup_kind(scope,
                                                                       dst_clk->name,
                                                                       JZ_SYM_PORT);
                    if (clk_sym && clk_sym->id >= 0) {
                        dst_clk_sig_id = clk_sym->id;
                    }
                }
            }

            for (int di = 0; di < mod->num_clock_domains; ++di) {
                IR_ClockDomain *cd = &mod->clock_domains[di];
                if (src_clk_sig_id >= 0 && cd->clock_signal_id == src_clk_sig_id &&
                    ir_cdc->source_clock_id == -1) {
                    ir_cdc->source_clock_id = cd->id;
                }
                if (dst_clk_sig_id >= 0 && cd->clock_signal_id == dst_clk_sig_id &&
                    ir_cdc->dest_clock_id == -1) {
                    ir_cdc->dest_clock_id = cd->id;
                }
            }

            idx++;
        }
    }

    mod->cdc_crossings = cdcs;
    mod->num_cdc_crossings = idx;
    return 0;
}

/**
 * @brief Parse CLOCKS attributes into clock period and edge.
 *
 * Accepts attributes such as:
 *   period=37.04 edge=Rising
 *
 * Preserves fractional period values even when whitespace is present.
 *
 * @param attrs          Attribute string.
 * @param out_period_ns  Output period in nanoseconds.
 * @param out_edge       Output clock edge.
 * @return 1 on success, 0 on failure.
 */
int ir_clock_parse_attrs(const char *attrs,
                        double *out_period_ns,
                        IR_ClockEdge *out_edge)
{
    if (!out_period_ns || !out_edge) return 0;
    *out_period_ns = 0.0;
    *out_edge = EDGE_RISING;
    if (!attrs) return 1;

    /* Parse period using strtod, mirroring sem_clock_parse_attrs. */
    const char *p = strstr(attrs, "period");
    if (p) {
        p = strchr(p, '=');
        if (p) {
            ++p;
            while (*p && isspace((unsigned char)*p)) ++p;
            /*
             * The AST pretty-printer may introduce spaces around the decimal
             * point (e.g., "37 . 04"), which confuses strtod. To preserve the
             * original fractional period, copy the numeric token into a
             * scratch buffer while stripping internal whitespace.
             */
            char numbuf[64];
            size_t n = 0;
            const char *q = p;
            while (*q && *q != ',' && *q != ';' && *q != '}') {
                if (!isspace((unsigned char)*q) && n + 1 < sizeof(numbuf)) {
                    numbuf[n++] = *q;
                }
                ++q;
            }
            numbuf[n] = '\0';
            if (n > 0) {
                char *endptr = NULL;
                double val = strtod(numbuf, &endptr);
                if (endptr != numbuf && val > 0.0) {
                    *out_period_ns = val;
                }
            }
        }
    }

    /* Parse edge. Attributes use case-insensitive Rising/Falling naming. */
    char edge_buf[32];
    edge_buf[0] = '\0';
    const char *e = strstr(attrs, "edge");
    if (e) {
        e = strchr(e, '=');
        if (e) {
            ++e;
            while (*e && isspace((unsigned char)*e)) ++e;
            size_t len = 0;
            while (e[len] && !isspace((unsigned char)e[len]) &&
                   e[len] != ',' && e[len] != ';' && e[len] != '}') {
                ++len;
            }
            if (len >= sizeof(edge_buf)) len = sizeof(edge_buf) - 1u;
            for (size_t i = 0; i < len; ++i) {
                unsigned char ch = (unsigned char)e[i];
                if (i == 0) {
                    edge_buf[i] = (char)toupper(ch);
                } else {
                    edge_buf[i] = (char)tolower(ch);
                }
            }
            edge_buf[len] = '\0';
        }
    }

    if (edge_buf[0] != '\0') {
        if (strcmp(edge_buf, "Rising") == 0) {
            *out_edge = EDGE_RISING;
        } else if (strcmp(edge_buf, "Falling") == 0) {
            *out_edge = EDGE_FALLING;
        } else {
            /* Unknown edge: leave default EDGE_RISING. */
        }
    }

    return 1;
}
