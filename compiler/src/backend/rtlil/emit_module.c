/*
 * emit_module.c - Module, wire, and memory declaration emission for RTLIL.
 *
 * RTLIL modules contain:
 *   - attribute declarations (e.g., \top 1)
 *   - wire declarations (ports, nets, registers - all are "wire" in RTLIL)
 *   - memory declarations
 *   - connect statements for aliases
 */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "rtlil_internal.h"
#include "ir.h"

/* Reuse alias helpers from Verilog backend. */
#include "backend/verilog-2005/verilog_internal.h"

/* -------------------------------------------------------------------------
 * Module header emission
 * -------------------------------------------------------------------------
 */

void rtlil_emit_module_header(FILE *out, const IR_Module *mod, int is_top)
{
    const char *name = (mod->name && mod->name[0] != '\0')
                     ? mod->name : "jz_unnamed_module";

    if (is_top) {
        fprintf(out, "attribute \\top 1\n");
    }
    fprintf(out, "attribute \\src \"%s:%d\"\n",
            mod->source_file_id >= 0 ? "jz-hdl" : "unknown",
            mod->source_line > 0 ? mod->source_line : 1);
    fprintf(out, "module \\%s\n", name);
}

/* -------------------------------------------------------------------------
 * Wire declarations
 * -------------------------------------------------------------------------
 */

void rtlil_emit_wires(FILE *out, const IR_Module *mod)
{
    if (!out || !mod) return;

    int port_idx = 1; /* RTLIL port indices start at 1 */

    for (int i = 0; i < mod->num_signals; ++i) {
        const IR_Signal *sig = &mod->signals[i];

        /* Skip non-representative aliases (same as Verilog backend). */
        if (sig->kind == SIG_NET && !alias_ctx_is_representative(mod, i)) {
            continue;
        }

        const char *name = sig->name ? sig->name : "jz_unnamed";
        int width = sig->width > 0 ? sig->width : 1;

        /* Emit IOB placement attribute for IOB-based latches. */
        if (sig->kind == SIG_LATCH && sig->iob) {
            rtlil_indent(out, 1);
            fprintf(out, "attribute \\IOBFF 1\n");
        }

        rtlil_indent(out, 1);
        fprintf(out, "wire width %d", width);

        if (sig->kind == SIG_PORT) {
            switch (sig->u.port.direction) {
                case PORT_IN:
                    fprintf(out, " input %d", port_idx++);
                    break;
                case PORT_OUT:
                    fprintf(out, " output %d", port_idx++);
                    break;
                case PORT_INOUT:
                    fprintf(out, " inout %d", port_idx++);
                    break;
                default:
                    fprintf(out, " input %d", port_idx++);
                    break;
            }
        }

        fprintf(out, " \\%s\n", name);
    }
}

/* -------------------------------------------------------------------------
 * Memory declarations
 * -------------------------------------------------------------------------
 */

void rtlil_emit_memories(FILE *out, const IR_Module *mod)
{
    if (!out || !mod || mod->num_memories <= 0) return;

    for (int i = 0; i < mod->num_memories; ++i) {
        const IR_Memory *m = &mod->memories[i];
        const char *name = (m->name && m->name[0] != '\0') ? m->name : "jz_mem";
        int word_width = m->word_width > 0 ? m->word_width : 1;
        int depth = m->depth > 0 ? m->depth : 1;

        rtlil_indent(out, 1);
        fprintf(out, "memory width %d size %d \\%s\n", word_width, depth, name);

        /* Declare implicit address registers for SYNC read ports. */
        for (int p = 0; p < m->num_ports; ++p) {
            const IR_MemoryPort *mp = &m->ports[p];
            if (mp->kind != MEM_PORT_READ_SYNC || mp->addr_signal_id < 0) {
                continue;
            }
            if (mp->addr_reg_signal_id >= 0) {
                continue; /* Already declared as a normal signal. */
            }
            const char *port_name = (mp->name && mp->name[0] != '\0')
                                  ? mp->name : "rd";
            int addr_w = mp->address_width > 0 ? mp->address_width : m->address_width;
            if (addr_w <= 0) addr_w = 1;
            rtlil_indent(out, 1);
            fprintf(out, "wire width %d \\%s_%s_addr\n", addr_w, name, port_name);
        }
    }
}

/* -------------------------------------------------------------------------
 * Alias connect emission
 *
 * ASSIGN_ALIAS sites from the ASYNCHRONOUS block are emitted as RTLIL
 * "connect" statements (direct wire-to-wire connections).
 * -------------------------------------------------------------------------
 */

static void emit_alias_connect(FILE *out, const IR_Module *mod,
                                const IR_Assignment *a)
{
    if (!out || !mod || !a) return;

    const IR_Signal *lhs_sig = rtlil_find_signal_by_id(mod, a->lhs_signal_id);
    if (!lhs_sig || !lhs_sig->name) return;

    /* Self-referential check. */
    if (a->rhs && a->rhs->kind == EXPR_SIGNAL_REF && !a->is_sliced) {
        const IR_Signal *rhs_sig = rtlil_find_signal_by_id(mod, a->rhs->u.signal_ref.signal_id);
        if (rhs_sig && rhs_sig == lhs_sig) return;
    }

    /* Direction fix for port-to-port aliases (same logic as Verilog backend). */
    const IR_Signal *emit_lhs = lhs_sig;
    const IR_Expr *emit_rhs = a->rhs;
    int swapped = 0;

    if (emit_rhs && emit_rhs->kind == EXPR_SIGNAL_REF && !a->is_sliced) {
        const IR_Signal *rhs_sig = rtlil_find_signal_by_id(mod, emit_rhs->u.signal_ref.signal_id);
        if (lhs_sig->kind == SIG_PORT && rhs_sig && rhs_sig->kind == SIG_PORT) {
            int lhs_is_input = (lhs_sig->u.port.direction == PORT_IN);
            int rhs_is_input = (rhs_sig->u.port.direction == PORT_IN);
            int rhs_is_output = (rhs_sig->u.port.direction == PORT_OUT);

            if (lhs_is_input && rhs_is_input) return;

            if (lhs_is_input && rhs_is_output) {
                emit_lhs = rhs_sig;
                swapped = 1;
            }
        }
    }

    /* Pre-compute RHS sigspec. This may emit cells/wires to 'out' as side
     * effects, so it MUST happen BEFORE we start writing the connect line. */
    char rhs_ss[RTLIL_SIGSPEC_MAX];
    if (swapped) {
        snprintf(rhs_ss, sizeof(rhs_ss), "\\%s", lhs_sig->name);
    } else if (!a->rhs) {
        int width = emit_lhs->width > 0 ? emit_lhs->width : 1;
        int pos = snprintf(rhs_ss, sizeof(rhs_ss), "%d'", width);
        for (int b = 0; b < width && pos < (int)sizeof(rhs_ss) - 1; ++b)
            rhs_ss[pos++] = '0';
        rhs_ss[pos] = '\0';
    } else {
        /* Use rtlil_emit_expr for signal refs, literals, and complex exprs.
         * For signal refs and literals, no side effects occur. For complex
         * expressions, cells/wires are emitted to 'out' before the connect. */
        rtlil_emit_expr(out, mod, a->rhs, rhs_ss, sizeof(rhs_ss));
    }

    /* Now emit the connect statement — no more side effects. */
    rtlil_indent(out, 1);
    fputs("connect ", out);

    /* LHS sigspec */
    if (a->is_sliced && !swapped) {
        fprintf(out, "\\%s [%d:%d]", emit_lhs->name,
                a->lhs_msb, a->lhs_lsb);
    } else {
        fprintf(out, "\\%s", emit_lhs->name);
    }

    fprintf(out, " %s\n", rhs_ss);
}

static void emit_alias_connects_from_stmt(FILE *out, const IR_Module *mod,
                                            const IR_Stmt *stmt)
{
    if (!out || !mod || !stmt) return;

    switch (stmt->kind) {
    case STMT_ASSIGNMENT:
        if (assignment_kind_is_alias(stmt->u.assign.kind)) {
            emit_alias_connect(out, mod, &stmt->u.assign);
        }
        break;
    case STMT_IF: {
        const IR_IfStmt *ifs = &stmt->u.if_stmt;
        if (ifs->then_block) emit_alias_connects_from_stmt(out, mod, ifs->then_block);
        const IR_Stmt *elif = ifs->elif_chain;
        while (elif && elif->kind == STMT_IF) {
            const IR_IfStmt *eifs = &elif->u.if_stmt;
            if (eifs->then_block) emit_alias_connects_from_stmt(out, mod, eifs->then_block);
            elif = eifs->elif_chain;
        }
        if (ifs->else_block) emit_alias_connects_from_stmt(out, mod, ifs->else_block);
        break;
    }
    case STMT_SELECT: {
        const IR_SelectStmt *sel = &stmt->u.select_stmt;
        for (int i = 0; i < sel->num_cases; ++i) {
            if (sel->cases[i].body)
                emit_alias_connects_from_stmt(out, mod, sel->cases[i].body);
        }
        break;
    }
    case STMT_BLOCK: {
        const IR_BlockStmt *blk = &stmt->u.block;
        for (int i = 0; i < blk->count; ++i) {
            emit_alias_connects_from_stmt(out, mod, &blk->stmts[i]);
        }
        break;
    }
    default:
        break;
    }
}

void rtlil_emit_alias_connects(FILE *out, const IR_Module *mod)
{
    if (!out || !mod || !mod->async_block) return;
    emit_alias_connects_from_stmt(out, mod, mod->async_block);
}
