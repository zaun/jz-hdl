/*
 * emit_memory.c - Memory cell emission for the RTLIL backend.
 *
 * Memory accesses in RTLIL are represented by $memrd_v2 and $memwr_v2
 * cells. These cells reference a named memory declared in the module.
 *
 * Note: Memory reads that appear inline in expressions are handled by
 * emit_cells.c (EXPR_MEM_READ). This file handles the standalone
 * memory write cells that correspond to STMT_MEM_WRITE in the IR.
 */
#include <stdio.h>
#include <string.h>

#include "rtlil_internal.h"
#include "ir.h"

/* Reuse alias helpers from Verilog backend. */
#include "backend/verilog-2005/verilog_internal.h"

/* Forward declaration. */
static void const_val_sigspec_zero(char *buf, int buf_size, int width);

/* -------------------------------------------------------------------------
 * Collect memory write statements from the statement tree
 * -------------------------------------------------------------------------
 */

typedef struct {
    const IR_MemWriteStmt *write;
    const IR_Expr *guard_condition;  /* NULL if unconditional. */
    int clock_domain_id;
} MemWriteInfo;

#define MAX_MEM_WRITES 64

static int s_mem_writes_count = 0;
static MemWriteInfo s_mem_writes[MAX_MEM_WRITES];

static void collect_mem_writes_from_stmt(const IR_Stmt *stmt,
                                          const IR_Expr *guard,
                                          int cd_id)
{
    if (!stmt) return;

    switch (stmt->kind) {
    case STMT_MEM_WRITE:
        if (s_mem_writes_count < MAX_MEM_WRITES) {
            s_mem_writes[s_mem_writes_count].write = &stmt->u.mem_write;
            s_mem_writes[s_mem_writes_count].guard_condition = guard;
            s_mem_writes[s_mem_writes_count].clock_domain_id = cd_id;
            s_mem_writes_count++;
        }
        break;

    case STMT_BLOCK: {
        const IR_BlockStmt *blk = &stmt->u.block;
        for (int i = 0; i < blk->count; ++i) {
            collect_mem_writes_from_stmt(&blk->stmts[i], guard, cd_id);
        }
        break;
    }

    case STMT_IF: {
        const IR_IfStmt *ifs = &stmt->u.if_stmt;
        /* Simplification: pass the IF condition as the guard.
         * A more complete implementation would AND the condition chain. */
        collect_mem_writes_from_stmt(ifs->then_block, ifs->condition, cd_id);
        if (ifs->elif_chain)
            collect_mem_writes_from_stmt(ifs->elif_chain, guard, cd_id);
        if (ifs->else_block)
            collect_mem_writes_from_stmt(ifs->else_block, guard, cd_id);
        break;
    }

    case STMT_SELECT: {
        const IR_SelectStmt *sel = &stmt->u.select_stmt;
        for (int i = 0; i < sel->num_cases; ++i) {
            collect_mem_writes_from_stmt(sel->cases[i].body, guard, cd_id);
        }
        break;
    }

    default:
        break;
    }
}

/* -------------------------------------------------------------------------
 * Emit $memwr_v2 cells for collected writes
 * -------------------------------------------------------------------------
 */

static void emit_memwr_cell(FILE *out, const IR_Module *mod,
                              const MemWriteInfo *info)
{
    const IR_MemWriteStmt *mw = info->write;
    if (!mw || !mw->memory_name) return;

    /* Find the memory to get dimensions. */
    const IR_Memory *mem = NULL;
    for (int i = 0; i < mod->num_memories; ++i) {
        const IR_Memory *m = &mod->memories[i];
        if (m->name && strcmp(m->name, mw->memory_name) == 0) {
            mem = m;
            break;
        }
    }
    if (!mem) return;

    int width = mem->word_width > 0 ? mem->word_width : 1;
    int addr_width = mem->address_width > 0 ? mem->address_width : 1;

    /* Emit address and data sigspecs. */
    char addr_ss[RTLIL_SIGSPEC_MAX];
    if (mw->address) {
        rtlil_emit_expr(out, mod, mw->address, addr_ss, sizeof(addr_ss));
    } else {
        const_val_sigspec_zero(addr_ss, sizeof(addr_ss), addr_width);
    }

    char data_ss[RTLIL_SIGSPEC_MAX];
    if (mw->data) {
        rtlil_emit_expr(out, mod, mw->data, data_ss, sizeof(data_ss));
    } else {
        const_val_sigspec_zero(data_ss, sizeof(data_ss), width);
    }

    /* Write enable: gate with guard condition if present. */
    char en_ss[RTLIL_SIGSPEC_MAX];
    if (info->guard_condition) {
        /* Emit the guard expression as cells, get a 1-bit sigspec. */
        char guard_ss[RTLIL_SIGSPEC_MAX];
        rtlil_emit_expr(out, mod, info->guard_condition,
                         guard_ss, sizeof(guard_ss));

        /* Replicate the 1-bit enable to match the data width.
         * Build a concat: { guard guard ... guard } */
        int pos = snprintf(en_ss, sizeof(en_ss), "{ ");
        for (int b = 0; b < width && pos < (int)sizeof(en_ss) - 4; ++b) {
            if (b > 0) {
                en_ss[pos++] = ' ';
            }
            int n = snprintf(en_ss + pos, sizeof(en_ss) - (size_t)pos,
                             "%s", guard_ss);
            pos += n;
        }
        pos += snprintf(en_ss + pos, sizeof(en_ss) - (size_t)pos, " }");
        en_ss[pos] = '\0';
    } else {
        /* Unconditional write: all bits enabled. */
        int pos = snprintf(en_ss, sizeof(en_ss), "%d'", width);
        for (int b = 0; b < width && pos < (int)sizeof(en_ss) - 1; ++b) {
            en_ss[pos++] = '1';
        }
        en_ss[pos] = '\0';
    }

    /* Find clock signal for this write. */
    const char *clk_name = "1'0";
    int clk_enable = 0;
    int clk_polarity = 1;

    if (info->clock_domain_id >= 0) {
        const IR_ClockDomain *cd = rtlil_find_clock_domain_by_id(
            mod, info->clock_domain_id);
        if (cd) {
            const IR_Signal *clk_sig = rtlil_find_signal_by_id(
                mod, cd->clock_signal_id);
            if (clk_sig && clk_sig->name) {
                static char clk_buf[128];
                snprintf(clk_buf, sizeof(clk_buf), "\\%s", clk_sig->name);
                clk_name = clk_buf;
                clk_enable = 1;
                clk_polarity = (cd->edge == EDGE_FALLING) ? 0 : 1;
            }
        }
    }

    int id = rtlil_next_id();
    rtlil_indent(out, 1);
    fprintf(out, "cell $memwr_v2 $auto$%d\n", id);
    rtlil_indent(out, 2);
    fprintf(out, "parameter \\MEMID \"\\\\%s\"\n", mw->memory_name);
    rtlil_indent(out, 2);
    fprintf(out, "parameter \\ABITS %d\n", addr_width);
    rtlil_indent(out, 2);
    fprintf(out, "parameter \\WIDTH %d\n", width);
    rtlil_indent(out, 2);
    fprintf(out, "parameter \\CLK_ENABLE %d\n", clk_enable);
    rtlil_indent(out, 2);
    fprintf(out, "parameter \\CLK_POLARITY %d\n", clk_polarity);
    rtlil_indent(out, 2);
    fprintf(out, "parameter \\PORTID 0\n");
    rtlil_indent(out, 2);
    fprintf(out, "parameter \\PRIORITY_MASK 0\n");
    rtlil_indent(out, 2);
    fprintf(out, "connect \\ADDR %s\n", addr_ss);
    rtlil_indent(out, 2);
    fprintf(out, "connect \\DATA %s\n", data_ss);
    rtlil_indent(out, 2);
    fprintf(out, "connect \\EN %s\n", en_ss);
    rtlil_indent(out, 2);
    fprintf(out, "connect \\CLK %s\n", clk_name);
    rtlil_indent(out, 1);
    fprintf(out, "end\n");
}

/* Helper to build a zero sigspec string. */
static void const_val_sigspec_zero(char *buf, int buf_size, int width)
{
    if (width <= 0) width = 1;
    int pos = snprintf(buf, (size_t)buf_size, "%d'", width);
    for (int i = 0; i < width && pos < buf_size - 1; ++i) {
        buf[pos++] = '0';
    }
    buf[pos] = '\0';
}

/* -------------------------------------------------------------------------
 * Main entry point
 * -------------------------------------------------------------------------
 */

void rtlil_emit_memory_cells(FILE *out, const IR_Module *mod)
{
    if (!out || !mod) return;
    if (mod->num_memories <= 0) return;

    /* Collect all memory write statements from clock domains. */
    s_mem_writes_count = 0;

    for (int cd = 0; cd < mod->num_clock_domains; ++cd) {
        const IR_ClockDomain *domain = &mod->clock_domains[cd];
        if (domain->statements) {
            collect_mem_writes_from_stmt(domain->statements, NULL, domain->id);
        }
    }

    /* Also check async block for async writes. */
    if (mod->async_block) {
        collect_mem_writes_from_stmt(mod->async_block, NULL, -1);
    }

    /* Emit $memwr_v2 cells for collected writes. */
    for (int i = 0; i < s_mem_writes_count; ++i) {
        emit_memwr_cell(out, mod, &s_mem_writes[i]);
    }
}
