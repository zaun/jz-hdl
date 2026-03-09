/*
 * emit_stmt.c - Statement emission for the Verilog-2005 backend.
 *
 * This file handles lowering IR statements (assignments, if/else, select/case)
 * to Verilog procedural statements.
 */
#include <stdio.h>
#include <string.h>

#include "verilog_internal.h"
#include "ir.h"

/* -------------------------------------------------------------------------
 * Assignment kind helpers
 * -------------------------------------------------------------------------
 */

int assignment_kind_is_zext(IR_AssignmentKind kind)
{
    switch (kind) {
        case ASSIGN_ALIAS_ZEXT:
        case ASSIGN_DRIVE_ZEXT:
        case ASSIGN_RECEIVE_ZEXT:
            return 1;
        default:
            return 0;
    }
}

int assignment_kind_is_sext(IR_AssignmentKind kind)
{
    switch (kind) {
        case ASSIGN_ALIAS_SEXT:
        case ASSIGN_DRIVE_SEXT:
        case ASSIGN_RECEIVE_SEXT:
            return 1;
        default:
            return 0;
    }
}

/* -------------------------------------------------------------------------
 * Width extension helpers for assignments
 * -------------------------------------------------------------------------
 */

void emit_assignment_zext_rhs(FILE *out,
                              const IR_Module *mod,
                              const IR_Signal *lhs_sig,
                              const IR_Expr *rhs)
{
    if (!lhs_sig || !rhs || lhs_sig->width <= 0 || rhs->width <= 0) {
        emit_expr(out, mod, rhs);
        return;
    }

    int lhs_w = lhs_sig->width;
    int rhs_w = rhs->width;
    if (lhs_w <= rhs_w) {
        emit_expr(out, mod, rhs);
        return;
    }

    int pad = lhs_w - rhs_w;
    fputc('{', out);
    if (lhs_sig->can_be_z) {
        fprintf(out, "{%d{1'bz}}, ", pad);
    } else {
        fprintf(out, "{%d{1'b0}}, ", pad);
    }
    emit_expr(out, mod, rhs);
    fputc('}', out);
}

void emit_assignment_sext_rhs(FILE *out,
                              const IR_Module *mod,
                              const IR_Signal *lhs_sig,
                              const IR_Expr *rhs)
{
    if (!lhs_sig || !rhs || lhs_sig->width <= 0 || rhs->width <= 0) {
        emit_expr(out, mod, rhs);
        return;
    }

    int lhs_w = lhs_sig->width;
    if (lhs_w <= rhs->width) {
        emit_expr(out, mod, rhs);
        return;
    }

    emit_padded_expr(out, mod, rhs, lhs_w, /*sign_extend=*/true);
}

/* -------------------------------------------------------------------------
 * Memory read emission for null RHS
 * -------------------------------------------------------------------------
 */

int emit_mem_read_for_null(FILE *out,
                           const IR_Module *mod,
                           const IR_Signal *lhs_sig,
                           IR_MemPortKind required_port_kind)
{
    if (!out || !mod || !lhs_sig) {
        return 0;
    }

    /* Preferred path: use explicit memory-port bindings. */
    const IR_Memory *chosen_mem = NULL;
    const IR_MemoryPort *chosen_port = NULL;

    for (int i = 0; i < mod->num_memories && !chosen_mem; ++i) {
        const IR_Memory *m = &mod->memories[i];
        for (int p = 0; p < m->num_ports; ++p) {
            const IR_MemoryPort *mp = &m->ports[p];
            if (mp->kind != required_port_kind) {
                continue;
            }
            if (mp->data_out_signal_id == lhs_sig->id && mp->addr_signal_id >= 0) {
                chosen_mem = m;
                chosen_port = mp;
                break;
            }
        }
    }

    if (chosen_mem && chosen_port) {
        const char *raw_mem = (chosen_mem->name && chosen_mem->name[0] != '\0')
                             ? chosen_mem->name
                             : "jz_mem";
        char mem_safe_buf[256];
        const char *mem_name = verilog_memory_name(raw_mem, mod->name, mem_safe_buf, sizeof(mem_safe_buf));
        if (chosen_port->kind == MEM_PORT_READ_SYNC) {
            /* BSRAM intermediate substitution: when emitting the main block
             * and this memory has a BSRAM read intermediate, emit the
             * intermediate signal name instead of the direct memory read.
             */
            if (g_skip_block_mem_accesses &&
                chosen_mem->kind == MEM_KIND_BLOCK &&
                has_bsram_read_intermediate(mem_name)) {
                fprintf(out, "%s_bsram_out", mem_name);
                return 1;
            }
            /* SYNC read: use implicit registered address. */
            const char *pname = (chosen_port->name && chosen_port->name[0] != '\0')
                              ? chosen_port->name : "rd";
            fprintf(out, "%s[%s_%s_addr]", mem_name, mem_name, pname);
        } else {
            const IR_Signal *addr_sig = find_signal_by_id(mod, chosen_port->addr_signal_id);
            if (!addr_sig || !addr_sig->name) {
                return 0;
            }
            fprintf(out, "%s[%s]", mem_name, addr_sig->name);
        }
        return 1;
    }

    /* Secondary path for READ_SYNC ports. */
    if (required_port_kind == MEM_PORT_READ_SYNC && lhs_sig->width > 0) {
        const IR_Memory *single_mem = NULL;
        const IR_MemoryPort *single_port = NULL;
        int candidate_count = 0;

        for (int i = 0; i < mod->num_memories; ++i) {
            const IR_Memory *m = &mod->memories[i];
            if (m->word_width != lhs_sig->width) {
                continue;
            }
            for (int p = 0; p < m->num_ports; ++p) {
                const IR_MemoryPort *mp = &m->ports[p];
                if (mp->kind != MEM_PORT_READ_SYNC) {
                    continue;
                }
                if (mp->addr_signal_id < 0) {
                    continue;
                }
                if (mp->data_out_signal_id >= 0) {
                    continue;
                }
                single_mem = m;
                single_port = mp;
                candidate_count++;
                if (candidate_count > 1) {
                    break;
                }
            }
            if (candidate_count > 1) {
                break;
            }
        }

        if (candidate_count == 1 && single_mem && single_port) {
            const char *raw_mem2 = (single_mem->name && single_mem->name[0] != '\0')
                                 ? single_mem->name
                                 : "jz_mem";
            char mem_safe_buf2[256];
            const char *mem_name = verilog_memory_name(raw_mem2, mod->name, mem_safe_buf2, sizeof(mem_safe_buf2));
            const char *pname = (single_port->name && single_port->name[0] != '\0')
                              ? single_port->name : "rd";
            fprintf(out, "%s[%s_%s_addr]", mem_name, mem_name, pname);
            return 1;
        }
    }

    /* Fallback heuristic for legacy IR. */
    const IR_Memory *heur_mem = NULL;
    for (int i = 0; i < mod->num_memories && !heur_mem; ++i) {
        const IR_Memory *m = &mod->memories[i];
        if (m->word_width != lhs_sig->width) {
            continue;
        }
        for (int p = 0; p < m->num_ports; ++p) {
            const IR_MemoryPort *mp = &m->ports[p];
            if (mp->kind == required_port_kind) {
                heur_mem = m;
                break;
            }
        }
    }

    if (!heur_mem) {
        return 0;
    }

    const IR_Signal *addr_sig = NULL;
    for (int i = 0; i < mod->num_signals; ++i) {
        const IR_Signal *sig = &mod->signals[i];
        if (sig->width == heur_mem->address_width && sig->name && sig->name[0] != '\0') {
            addr_sig = sig;
            break;
        }
    }

    if (!addr_sig) {
        return 0;
    }

    const char *raw_mem3 = (heur_mem->name && heur_mem->name[0] != '\0')
                         ? heur_mem->name
                         : "jz_mem";
    char mem_safe_buf3[256];
    const char *mem_name = verilog_memory_name(raw_mem3, mod->name, mem_safe_buf3, sizeof(mem_safe_buf3));

    fprintf(out, "%s[%s]", mem_name, addr_sig->name);
    return 1;
}

/* -------------------------------------------------------------------------
 * Latch guard pattern recognition
 * -------------------------------------------------------------------------
 */

static int emit_latch_guard_if_possible(FILE *out,
                                        const IR_Module *mod,
                                        const IR_Assignment *a,
                                        const IR_Signal *lhs_sig,
                                        int indent_level)
{
    if (!out || !mod || !a || !lhs_sig) {
        return 0;
    }
    if (g_in_sequential) {
        return 0;
    }

    const IR_Signal *lhs_raw = find_signal_by_id_raw(mod, a->lhs_signal_id);
    if (!lhs_raw || lhs_raw->kind != SIG_LATCH) {
        return 0;
    }
    const IR_Expr *rhs = a->rhs;
    if (!rhs || rhs->kind != EXPR_TERNARY) {
        return 0;
    }

    const IR_Expr *cond = rhs->u.ternary.condition;
    const IR_Expr *tval = rhs->u.ternary.true_val;
    const IR_Expr *fval = rhs->u.ternary.false_val;
    if (!cond || !tval || !fval) {
        return 0;
    }

    /* Check that the false branch is a "hold" on the same latch. */
    if (!a->is_sliced) {
        if (fval->kind != EXPR_SIGNAL_REF ||
            fval->u.signal_ref.signal_id != lhs_raw->id) {
            return 0;
        }
    } else {
        if (fval->kind != EXPR_SLICE ||
            fval->u.slice.signal_id != lhs_raw->id ||
            fval->u.slice.msb != a->lhs_msb ||
            fval->u.slice.lsb != a->lhs_lsb) {
            return 0;
        }
    }

    /* Emit: if (cond) lhs <= data; */
    emit_indent(out, indent_level);
    fputs("if (", out);
    emit_expr(out, mod, cond);
    fputs(") ", out);

    {
        char esc_lhs[256];
        fprintf(out, "%s", verilog_safe_name(lhs_sig->name, esc_lhs, (int)sizeof(esc_lhs)));
    }
    if (a->is_sliced) {
        if (a->lhs_msb == a->lhs_lsb) {
            fprintf(out, "[%d]", a->lhs_msb);
        } else {
            fprintf(out, "[%d:%d]", a->lhs_msb, a->lhs_lsb);
        }
    }

    fputs(" <= ", out);
    if (assignment_kind_is_zext(a->kind)) {
        emit_assignment_zext_rhs(out, mod, lhs_sig, tval);
    } else if (assignment_kind_is_sext(a->kind)) {
        emit_assignment_sext_rhs(out, mod, lhs_sig, tval);
    } else {
        emit_expr(out, mod, tval);
    }
    fputs(";\n", out);
    return 1;
}

/* -------------------------------------------------------------------------
 * Assignment statement emission
 * -------------------------------------------------------------------------
 */

void emit_assignment_stmt(FILE *out,
                          const IR_Module *mod,
                          const IR_Assignment *a,
                          int indent_level)
{
    if (!a) {
        return;
    }

    /* ASSIGN_ALIAS* sites are promoted to continuous assigns. */
    if (!g_in_sequential && assignment_kind_is_alias(a->kind)) {
        return;
    }

    /* INOUT port assignments in async blocks are emitted as continuous
     * assigns (not procedural), since inout ports cannot be declared reg. */
    if (g_skip_inout_port_assigns && !g_in_sequential) {
        const IR_Signal *raw_lhs = find_signal_by_id_raw(mod, a->lhs_signal_id);
        if (raw_lhs && raw_lhs->kind == SIG_PORT &&
            raw_lhs->u.port.direction == PORT_INOUT) {
            return;
        }
    }

    const IR_Signal *lhs_sig = find_signal_by_id(mod, a->lhs_signal_id);
    if (!lhs_sig || !lhs_sig->name) {
        emit_indent(out, indent_level);
        fprintf(out, "/* unknown LHS signal id %d */", a->lhs_signal_id);
        fputc('\n', out);
        return;
    }

    /* Try to recognize guarded latch assignments. */
    if (emit_latch_guard_if_possible(out, mod, a, lhs_sig, indent_level)) {
        return;
    }

    emit_indent(out, indent_level);
    {
        char esc_lhs2[256];
        fprintf(out, "%s", verilog_safe_name(lhs_sig->name, esc_lhs2, (int)sizeof(esc_lhs2)));
    }
    if (a->is_sliced) {
        if (a->lhs_msb == a->lhs_lsb) {
            fprintf(out, "[%d]", a->lhs_msb);
        } else {
            fprintf(out, "[%d:%d]", a->lhs_msb, a->lhs_lsb);
        }
    }

    int is_latch_lhs = (lhs_sig->kind == SIG_LATCH);
    fprintf(out, (g_in_sequential || is_latch_lhs) ? " <= " : " = ");

    if (!a->rhs) {
        if (g_in_sequential) {
            if (!emit_mem_read_for_null(out, mod, lhs_sig, MEM_PORT_READ_SYNC)) {
                fprintf(out, "1'b0");
            }
        } else {
            if (!emit_mem_read_for_null(out, mod, lhs_sig, MEM_PORT_READ_ASYNC)) {
                fprintf(out, "1'b0");
            }
        }
    } else if (assignment_kind_is_zext(a->kind)) {
        emit_assignment_zext_rhs(out, mod, lhs_sig, a->rhs);
    } else if (assignment_kind_is_sext(a->kind)) {
        emit_assignment_sext_rhs(out, mod, lhs_sig, a->rhs);
    } else if (a->rhs && a->rhs->kind == EXPR_LITERAL &&
               a->rhs->const_name &&
               (strcmp(a->rhs->const_name, "GND") == 0 ||
                strcmp(a->rhs->const_name, "VCC") == 0)) {
        int width = lhs_sig->width;
        if (a->is_sliced) {
            width = a->lhs_msb - a->lhs_lsb + 1;
        }
        if (width <= 0) width = 1;
        char bit = (strcmp(a->rhs->const_name, "VCC") == 0) ? '1' : '0';
        fprintf(out, "{%d{1'b%c}}", width, bit);
    } else {
        emit_expr(out, mod, a->rhs);
    }

    fputs(";\n", out);
}

/* -------------------------------------------------------------------------
 * IF/ELIF/ELSE chain emission
 * -------------------------------------------------------------------------
 */

void emit_if_chain(FILE *out,
                   const IR_Module *mod,
                   const IR_Stmt *if_stmt_node,
                   int indent_level)
{
    const IR_Stmt *current = if_stmt_node;
    int first = 1;

    while (current && current->kind == STMT_IF) {
        const IR_IfStmt *ifs = &current->u.if_stmt;

        emit_indent(out, indent_level);
        if (first) {
            fputs("if (", out);
            first = 0;
        } else {
            fputs("else if (", out);
        }
        emit_expr(out, mod, ifs->condition);
        fputs(") ", out);

        if (ifs->then_block) {
            if (ifs->then_block->kind == STMT_BLOCK) {
                fputs("begin\n", out);
                emit_block_stmt(out, mod, ifs->then_block, indent_level + 1, 0);
                emit_indent(out, indent_level);
                fputs("end\n", out);
            } else {
                fputs("begin\n", out);
                emit_stmt(out, mod, ifs->then_block, indent_level + 1);
                emit_indent(out, indent_level);
                fputs("end\n", out);
            }
        } else {
            fputs("begin\n", out);
            emit_indent(out, indent_level);
            fputs("end\n", out);
        }

        current = ifs->elif_chain;
    }

    const IR_IfStmt *root_ifs = &if_stmt_node->u.if_stmt;
    if (root_ifs->else_block) {
        emit_indent(out, indent_level);
        fputs("else ", out);
        const IR_Stmt *else_block = root_ifs->else_block;
        if (else_block->kind == STMT_BLOCK) {
            fputs("begin\n", out);
            emit_block_stmt(out, mod, else_block, indent_level + 1, 0);
            emit_indent(out, indent_level);
            fputs("end\n", out);
        } else {
            fputs("begin\n", out);
            emit_stmt(out, mod, else_block, indent_level + 1);
            emit_indent(out, indent_level);
            fputs("end\n", out);
        }
    }
}

/* -------------------------------------------------------------------------
 * SELECT/CASE statement emission
 * -------------------------------------------------------------------------
 */

static const char *expr_const_comment(const IR_Expr *expr)
{
    if (!expr) {
        return NULL;
    }
    if (expr->kind != EXPR_LITERAL) {
        return NULL;
    }
    if (!expr->const_name || expr->const_name[0] == '\0') {
        return NULL;
    }
    return expr->const_name;
}

void emit_select_stmt(FILE *out,
                      const IR_Module *mod,
                      const IR_Stmt *select_stmt_node,
                      int indent_level)
{
    if (!select_stmt_node || select_stmt_node->kind != STMT_SELECT) {
        return;
    }
    const IR_SelectStmt *sel = &select_stmt_node->u.select_stmt;
    if (!sel || !sel->selector) {
        return;
    }

    emit_indent(out, indent_level);
    fputs("case (", out);
    emit_expr(out, mod, sel->selector);
    fputs(")\n", out);

    /* Determine if fall-through should be allowed. */
    int allow_fallthrough = 1;
    if (sel->selector->kind == EXPR_SIGNAL_REF) {
        const IR_Signal *sel_sig = find_signal_by_id(mod, sel->selector->u.signal_ref.signal_id);
        if (sel_sig && sel_sig->name && strcmp(sel_sig->name, "state") == 0) {
            allow_fallthrough = 0;
        }
    }

    int i = 0;
    while (i < sel->num_cases) {
        const IR_SelectCase *sc = &sel->cases[i];

        if (!sc->case_value) {
            emit_indent(out, indent_level + 1);
            fputs("default: ", out);

            const IR_Stmt *body_to_emit = sc->body;
            if (body_to_emit) {
                if (body_to_emit->kind == STMT_BLOCK) {
                    fputs("begin\n", out);
                    emit_block_stmt(out, mod, body_to_emit, indent_level + 2, 0);
                    emit_indent(out, indent_level + 1);
                    fputs("end\n", out);
                } else {
                    fputs("begin\n", out);
                    emit_stmt(out, mod, body_to_emit, indent_level + 2);
                    emit_indent(out, indent_level + 1);
                    fputs("end\n", out);
                }
            } else {
                fputs("begin end\n", out);
            }

            ++i;
            continue;
        }

        const IR_Stmt *body_to_emit = sc->body;
        if (allow_fallthrough && !body_to_emit) {
            for (int j = i + 1; j < sel->num_cases; ++j) {
                const IR_SelectCase *next = &sel->cases[j];
                if (!next->case_value) {
                    break;
                }
                if (next->body) {
                    body_to_emit = next->body;
                    break;
                }
            }
        }

        int group_end = i + 1;
        if (body_to_emit && allow_fallthrough) {
            while (group_end < sel->num_cases) {
                const IR_SelectCase *next = &sel->cases[group_end];
                if (!next->case_value) {
                    break;
                }

                const IR_Stmt *next_body = next->body;
                if (!next_body) {
                    for (int k = group_end + 1; k < sel->num_cases && !next_body; ++k) {
                        const IR_SelectCase *lookahead = &sel->cases[k];
                        if (!lookahead->case_value) {
                            break;
                        }
                        if (lookahead->body) {
                            next_body = lookahead->body;
                            break;
                        }
                    }
                }

                if (next_body != body_to_emit) {
                    break;
                }

                ++group_end;
            }
        }

        const char *case_comment = NULL;
        if (group_end == i + 1) {
            emit_indent(out, indent_level + 1);
            emit_expr(out, mod, sc->case_value);
            fputs(": ", out);
            case_comment = expr_const_comment(sc->case_value);
        } else {
            for (int idx = i; idx < group_end - 1; ++idx) {
                const IR_SelectCase *lbl = &sel->cases[idx];
                emit_indent(out, indent_level + 1);
                emit_expr(out, mod, lbl->case_value);
                fputs(",", out);
                const char *lbl_comment = expr_const_comment(lbl->case_value);
                if (lbl_comment) {
                    fputs("  // ", out);
                    fputs(lbl_comment, out);
                }
                fputs("\n", out);
            }
            const IR_SelectCase *last_lbl = &sel->cases[group_end - 1];
            emit_indent(out, indent_level + 1);
            emit_expr(out, mod, last_lbl->case_value);
            fputs(": ", out);
            case_comment = expr_const_comment(last_lbl->case_value);
        }

        if (body_to_emit) {
            if (body_to_emit->kind == STMT_BLOCK) {
                fputs("begin", out);
                if (case_comment) {
                    fputs("  // ", out);
                    fputs(case_comment, out);
                }
                fputs("\n", out);
                emit_block_stmt(out, mod, body_to_emit, indent_level + 2, 0);
                emit_indent(out, indent_level + 1);
                fputs("end\n", out);
            } else {
                fputs("begin", out);
                if (case_comment) {
                    fputs("  // ", out);
                    fputs(case_comment, out);
                }
                fputs("\n", out);
                emit_stmt(out, mod, body_to_emit, indent_level + 2);
                emit_indent(out, indent_level + 1);
                fputs("end\n", out);
            }
        } else {
            fputs("begin end", out);
            if (case_comment) {
                fputs("  // ", out);
                fputs(case_comment, out);
            }
            fputs("\n", out);
        }

        i = group_end;
    }

    emit_indent(out, indent_level);
    fputs("endcase\n", out);
}

/* -------------------------------------------------------------------------
 * Block and generic statement emission
 * -------------------------------------------------------------------------
 */

void emit_block_stmt(FILE *out,
                     const IR_Module *mod,
                     const IR_Stmt *block_stmt,
                     int indent_level,
                     int is_root_block)
{
    if (!block_stmt || block_stmt->kind != STMT_BLOCK) {
        return;
    }

    const IR_BlockStmt *blk = &block_stmt->u.block;
    for (int i = 0; i < blk->count; ++i) {
        IR_Stmt *child = &blk->stmts[i];
        (void)is_root_block;
        emit_stmt(out, mod, child, indent_level);
    }
}

void emit_stmt(FILE *out, const IR_Module *mod, const IR_Stmt *stmt, int indent_level)
{
    if (!stmt) {
        return;
    }

    switch (stmt->kind) {
        case STMT_ASSIGNMENT:
            emit_assignment_stmt(out, mod, &stmt->u.assign, indent_level);
            break;

        case STMT_IF:
            emit_if_chain(out, mod, stmt, indent_level);
            break;

        case STMT_SELECT:
            emit_select_stmt(out, mod, stmt, indent_level);
            break;

        case STMT_BLOCK:
            emit_block_stmt(out, mod, stmt, indent_level, 0);
            break;

        case STMT_MEM_WRITE: {
            /* Skip BLOCK memory writes if they've been emitted in separate blocks. */
            if (stmt_is_skipped_block_mem_write(mod, stmt)) {
                break;
            }
            const char *mem_name = stmt->u.mem_write.memory_name;
            if (!mem_name || !mod) {
                break;
            }
            const IR_Memory *mem = NULL;
            for (int i = 0; i < mod->num_memories; ++i) {
                const IR_Memory *m = &mod->memories[i];
                if (m->name && strcmp(m->name, mem_name) == 0) {
                    mem = m;
                    break;
                }
            }
            if (!mem) {
                break;
            }
            const char *raw_vname = (mem->name && mem->name[0] != '\0') ? mem->name : "jz_mem";
            char vname_buf[256];
            const char *vname = verilog_memory_name(raw_vname, mod->name, vname_buf, sizeof(vname_buf));
            emit_indent(out, indent_level);
            fprintf(out, "%s[", vname);
            emit_expr(out, mod, stmt->u.mem_write.address);
            fputs("] <= ", out);
            emit_expr(out, mod, stmt->u.mem_write.data);
            fputs(";\n", out);
            break;
        }

        default:
            emit_indent(out, indent_level);
            fprintf(out, "/* unsupported stmt kind %d */\n", (int)stmt->kind);
            break;
    }
}
