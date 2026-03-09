/*
 * emit_cells.c - Expression-to-cell decomposition for the RTLIL backend.
 *
 * In RTLIL, expressions are not written inline. Instead, each operation
 * becomes a cell ($add, $sub, $mux, etc.) with intermediate wires carrying
 * results. This file recursively walks the IR expression tree, emits cells
 * bottom-up, and returns the sigspec of the result wire.
 */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "rtlil_internal.h"
#include "ir.h"

/* Reuse alias helpers. */
#include "backend/verilog-2005/verilog_internal.h"

/* -------------------------------------------------------------------------
 * Helper: look up an IR signal width from a const module pointer.
 * -------------------------------------------------------------------------
 */
static int signal_width_from_mod(const IR_Module *mod, int signal_id)
{
    if (!mod || !mod->signals || signal_id < 0) return 0;
    for (int i = 0; i < mod->num_signals; ++i) {
        if (mod->signals[i].id == signal_id)
            return mod->signals[i].width;
    }
    return 0;
}

/* -------------------------------------------------------------------------
 * Helper: create an intermediate wire and return its name in out_sigspec
 * -------------------------------------------------------------------------
 */

static void make_temp_wire(FILE *out, int width, char *out_sigspec, int sigspec_size)
{
    int id = rtlil_next_id();
    snprintf(out_sigspec, sigspec_size, "$auto$%d", id);
    rtlil_indent(out, 1);
    fprintf(out, "wire width %d %s\n", width > 0 ? width : 1, out_sigspec);
}

/* -------------------------------------------------------------------------
 * Helper: format a sigspec for a literal
 * -------------------------------------------------------------------------
 */

static void literal_sigspec(char *out_sigspec, int sigspec_size,
                            const IR_Literal *lit)
{
    if (!lit || lit->width <= 0) {
        snprintf(out_sigspec, sigspec_size, "1'0");
        return;
    }

    int width = lit->width;
    int pos = snprintf(out_sigspec, sigspec_size, "%d'", width);

    if (lit->is_z) {
        for (int i = 0; i < width && pos < sigspec_size - 1; ++i) {
            out_sigspec[pos++] = 'z';
        }
        out_sigspec[pos] = '\0';
        return;
    }

    for (int i = width - 1; i >= 0 && pos < sigspec_size - 1; --i) {
        int wi = i / 64;
        int bi = i % 64;
        unsigned bit = (wi < IR_LIT_WORDS) ? (unsigned)((lit->words[wi] >> bi) & 1u) : 0;
        out_sigspec[pos++] = bit ? '1' : '0';
    }
    out_sigspec[pos] = '\0';
}

static void const_val_sigspec(char *out_sigspec, int sigspec_size,
                              int width, uint64_t value)
{
    IR_Literal lit;
    lit.width = width;
    memset(lit.words, 0, sizeof(lit.words));
    lit.words[0] = value;
    lit.is_z = 0;
    literal_sigspec(out_sigspec, sigspec_size, &lit);
}

/* -------------------------------------------------------------------------
 * Helper: emit a binary cell ($add, $sub, etc.)
 * -------------------------------------------------------------------------
 */

static void emit_binary_cell(FILE *out, const char *cell_type,
                              const char *a_sigspec, int a_width, int a_signed,
                              const char *b_sigspec, int b_width, int b_signed,
                              int y_width,
                              char *out_sigspec, int sigspec_size)
{
    char y_wire[128];
    make_temp_wire(out, y_width, y_wire, sizeof(y_wire));

    int id = rtlil_next_id();
    rtlil_indent(out, 1);
    fprintf(out, "cell %s $auto$%d\n", cell_type, id);
    rtlil_indent(out, 2);
    fprintf(out, "parameter \\A_SIGNED %d\n", a_signed);
    rtlil_indent(out, 2);
    fprintf(out, "parameter \\A_WIDTH %d\n", a_width);
    rtlil_indent(out, 2);
    fprintf(out, "parameter \\B_SIGNED %d\n", b_signed);
    rtlil_indent(out, 2);
    fprintf(out, "parameter \\B_WIDTH %d\n", b_width);
    rtlil_indent(out, 2);
    fprintf(out, "parameter \\Y_WIDTH %d\n", y_width);
    rtlil_indent(out, 2);
    fprintf(out, "connect \\A %s\n", a_sigspec);
    rtlil_indent(out, 2);
    fprintf(out, "connect \\B %s\n", b_sigspec);
    rtlil_indent(out, 2);
    fprintf(out, "connect \\Y %s\n", y_wire);
    rtlil_indent(out, 1);
    fprintf(out, "end\n");

    snprintf(out_sigspec, sigspec_size, "%s", y_wire);
}

/* -------------------------------------------------------------------------
 * Helper: emit a unary cell ($not, $neg, etc.)
 * -------------------------------------------------------------------------
 */

static void emit_unary_cell(FILE *out, const char *cell_type,
                             const char *a_sigspec, int a_width, int a_signed,
                             int y_width,
                             char *out_sigspec, int sigspec_size)
{
    char y_wire[128];
    make_temp_wire(out, y_width, y_wire, sizeof(y_wire));

    int id = rtlil_next_id();
    rtlil_indent(out, 1);
    fprintf(out, "cell %s $auto$%d\n", cell_type, id);
    rtlil_indent(out, 2);
    fprintf(out, "parameter \\A_SIGNED %d\n", a_signed);
    rtlil_indent(out, 2);
    fprintf(out, "parameter \\A_WIDTH %d\n", a_width);
    rtlil_indent(out, 2);
    fprintf(out, "parameter \\Y_WIDTH %d\n", y_width);
    rtlil_indent(out, 2);
    fprintf(out, "connect \\A %s\n", a_sigspec);
    rtlil_indent(out, 2);
    fprintf(out, "connect \\Y %s\n", y_wire);
    rtlil_indent(out, 1);
    fprintf(out, "end\n");

    snprintf(out_sigspec, sigspec_size, "%s", y_wire);
}

/* -------------------------------------------------------------------------
 * Helper: emit a $mux cell (for ternary expressions)
 * -------------------------------------------------------------------------
 */

static void emit_mux_cell(FILE *out,
                           const char *s_sigspec,
                           const char *a_sigspec, /* false branch */
                           const char *b_sigspec, /* true branch */
                           int width,
                           char *out_sigspec, int sigspec_size)
{
    char y_wire[128];
    make_temp_wire(out, width, y_wire, sizeof(y_wire));

    int id = rtlil_next_id();
    rtlil_indent(out, 1);
    fprintf(out, "cell $mux $auto$%d\n", id);
    rtlil_indent(out, 2);
    fprintf(out, "parameter \\WIDTH %d\n", width);
    rtlil_indent(out, 2);
    fprintf(out, "connect \\A %s\n", a_sigspec);
    rtlil_indent(out, 2);
    fprintf(out, "connect \\B %s\n", b_sigspec);
    rtlil_indent(out, 2);
    fprintf(out, "connect \\S %s\n", s_sigspec);
    rtlil_indent(out, 2);
    fprintf(out, "connect \\Y %s\n", y_wire);
    rtlil_indent(out, 1);
    fprintf(out, "end\n");

    snprintf(out_sigspec, sigspec_size, "%s", y_wire);
}

/* -------------------------------------------------------------------------
 * Main expression emitter (recursive)
 * -------------------------------------------------------------------------
 */

int rtlil_emit_expr(FILE *out, const IR_Module *mod, const IR_Expr *expr,
                    char *out_sigspec, int sigspec_size)
{
    if (!expr || !out || !out_sigspec || sigspec_size <= 0) {
        snprintf(out_sigspec, sigspec_size, "1'0");
        return 0;
    }

    switch (expr->kind) {
    case EXPR_LITERAL: {
        /* Check for GND/VCC polymorphic constants. */
        if (expr->const_name &&
            (strcmp(expr->const_name, "GND") == 0 ||
             strcmp(expr->const_name, "VCC") == 0)) {
            int width = (expr->width > 0) ? expr->width : 1;
            uint64_t value = (strcmp(expr->const_name, "VCC") == 0)
                           ? ((width < 64) ? ((1ULL << width) - 1) : ~0ULL)
                           : 0ULL;
            const_val_sigspec(out_sigspec, sigspec_size, width, value);
        } else {
            literal_sigspec(out_sigspec, sigspec_size, &expr->u.literal.literal);
        }
        return 0;
    }

    case EXPR_SIGNAL_REF: {
        const IR_Signal *sig = rtlil_find_signal_by_id(mod, expr->u.signal_ref.signal_id);
        if (sig && sig->name) {
            snprintf(out_sigspec, sigspec_size, "\\%s", sig->name);
        } else {
            snprintf(out_sigspec, sigspec_size, "1'0");
        }
        return 0;
    }

    case EXPR_SLICE: {
        if (expr->u.slice.base_expr) {
            /* Expression slice: emit as shift + mask cells. */
            char src_ss[RTLIL_SIGSPEC_MAX];
            rtlil_emit_expr(out, mod, expr->u.slice.base_expr, src_ss, sizeof(src_ss));

            int result_width = expr->u.slice.msb - expr->u.slice.lsb + 1;
            int src_width = expr->u.slice.base_expr->width;
            if (src_width <= 0) src_width = result_width;

            /* Shift right by lsb. */
            char shifted_ss[RTLIL_SIGSPEC_MAX];
            if (expr->u.slice.lsb > 0) {
                char lsb_ss[64];
                const_val_sigspec(lsb_ss, sizeof(lsb_ss), 32,
                                  (uint64_t)expr->u.slice.lsb);
                emit_binary_cell(out, "$shr",
                                 src_ss, src_width, 0,
                                 lsb_ss, 32, 0,
                                 src_width,
                                 shifted_ss, sizeof(shifted_ss));
            } else {
                snprintf(shifted_ss, sizeof(shifted_ss), "%s", src_ss);
            }

            /* Mask to result width. */
            if (result_width < src_width) {
                uint64_t mask = (result_width < 64)
                              ? ((1ULL << result_width) - 1) : ~0ULL;
                char mask_ss[128];
                const_val_sigspec(mask_ss, sizeof(mask_ss), src_width, mask);
                emit_binary_cell(out, "$and",
                                 shifted_ss, src_width, 0,
                                 mask_ss, src_width, 0,
                                 result_width,
                                 out_sigspec, sigspec_size);
            } else {
                snprintf(out_sigspec, sigspec_size, "%s", shifted_ss);
            }
        } else {
            /* Signal slice: use RTLIL bit selection. */
            const IR_Signal *sig = rtlil_find_signal_by_id(mod, expr->u.slice.signal_id);
            if (!sig || !sig->name) {
                int w = expr->u.slice.msb - expr->u.slice.lsb + 1;
                const_val_sigspec(out_sigspec, sigspec_size, w > 0 ? w : 1, 0);
                return 0;
            }
            if (expr->u.slice.msb == expr->u.slice.lsb) {
                snprintf(out_sigspec, sigspec_size, "\\%s [%d]",
                         sig->name, expr->u.slice.msb);
            } else {
                /* RTLIL range: [msb:lsb]. */
                snprintf(out_sigspec, sigspec_size, "\\%s [%d:%d]",
                         sig->name, expr->u.slice.msb,
                         expr->u.slice.lsb);
            }
        }
        return 0;
    }

    case EXPR_CONCAT: {
        /* RTLIL concat: { sigspec sigspec ... } */
        char result[RTLIL_SIGSPEC_MAX];
        int pos = 0;
        pos += snprintf(result + pos, sizeof(result) - (size_t)pos, "{ ");
        for (int i = 0; i < expr->u.concat.num_operands; ++i) {
            char child_ss[RTLIL_SIGSPEC_MAX];
            rtlil_emit_expr(out, mod, expr->u.concat.operands[i],
                            child_ss, sizeof(child_ss));
            if (i > 0) {
                pos += snprintf(result + pos, sizeof(result) - (size_t)pos, " ");
            }
            pos += snprintf(result + pos, sizeof(result) - (size_t)pos, "%s", child_ss);
        }
        pos += snprintf(result + pos, sizeof(result) - (size_t)pos, " }");
        snprintf(out_sigspec, sigspec_size, "%s", result);
        return 0;
    }

    case EXPR_UNARY_NOT: {
        char a_ss[RTLIL_SIGSPEC_MAX];
        rtlil_emit_expr(out, mod, expr->u.unary.operand, a_ss, sizeof(a_ss));
        int a_width = expr->u.unary.operand ? expr->u.unary.operand->width : 1;
        if (a_width <= 0) a_width = 1;
        int y_width = expr->width > 0 ? expr->width : a_width;
        emit_unary_cell(out, "$not", a_ss, a_width, 0, y_width,
                        out_sigspec, sigspec_size);
        return 0;
    }

    case EXPR_UNARY_NEG: {
        char a_ss[RTLIL_SIGSPEC_MAX];
        rtlil_emit_expr(out, mod, expr->u.unary.operand, a_ss, sizeof(a_ss));
        int a_width = expr->u.unary.operand ? expr->u.unary.operand->width : 1;
        if (a_width <= 0) a_width = 1;
        int y_width = expr->width > 0 ? expr->width : a_width;
        emit_unary_cell(out, "$neg", a_ss, a_width, 1, y_width,
                        out_sigspec, sigspec_size);
        return 0;
    }

    case EXPR_LOGICAL_NOT: {
        char a_ss[RTLIL_SIGSPEC_MAX];
        rtlil_emit_expr(out, mod, expr->u.unary.operand, a_ss, sizeof(a_ss));
        int a_width = expr->u.unary.operand ? expr->u.unary.operand->width : 1;
        if (a_width <= 0) a_width = 1;
        emit_unary_cell(out, "$logic_not", a_ss, a_width, 0, 1,
                        out_sigspec, sigspec_size);
        return 0;
    }

    case EXPR_BINARY_ADD:
    case EXPR_BINARY_SUB:
    case EXPR_BINARY_MUL:
    case EXPR_BINARY_DIV:
    case EXPR_BINARY_MOD:
    case EXPR_BINARY_AND:
    case EXPR_BINARY_OR:
    case EXPR_BINARY_XOR:
    case EXPR_BINARY_SHL:
    case EXPR_BINARY_SHR:
    case EXPR_BINARY_ASHR:
    case EXPR_BINARY_EQ:
    case EXPR_BINARY_NEQ:
    case EXPR_BINARY_LT:
    case EXPR_BINARY_GT:
    case EXPR_BINARY_LTE:
    case EXPR_BINARY_GTE:
    case EXPR_LOGICAL_AND:
    case EXPR_LOGICAL_OR: {
        const char *cell_type = "$add";
        switch (expr->kind) {
            case EXPR_BINARY_ADD:  cell_type = "$add"; break;
            case EXPR_BINARY_SUB:  cell_type = "$sub"; break;
            case EXPR_BINARY_MUL:  cell_type = "$mul"; break;
            case EXPR_BINARY_DIV:  cell_type = "$div"; break;
            case EXPR_BINARY_MOD:  cell_type = "$mod"; break;
            case EXPR_BINARY_AND:  cell_type = "$and"; break;
            case EXPR_BINARY_OR:   cell_type = "$or"; break;
            case EXPR_BINARY_XOR:  cell_type = "$xor"; break;
            case EXPR_BINARY_SHL:  cell_type = "$shl"; break;
            case EXPR_BINARY_SHR:  cell_type = "$shr"; break;
            case EXPR_BINARY_ASHR: cell_type = "$sshr"; break;
            case EXPR_BINARY_EQ:   cell_type = "$eq"; break;
            case EXPR_BINARY_NEQ:  cell_type = "$ne"; break;
            case EXPR_BINARY_LT:   cell_type = "$lt"; break;
            case EXPR_BINARY_GT:   cell_type = "$gt"; break;
            case EXPR_BINARY_LTE:  cell_type = "$le"; break;
            case EXPR_BINARY_GTE:  cell_type = "$ge"; break;
            case EXPR_LOGICAL_AND: cell_type = "$logic_and"; break;
            case EXPR_LOGICAL_OR:  cell_type = "$logic_or"; break;
            default: break;
        }

        char a_ss[RTLIL_SIGSPEC_MAX], b_ss[RTLIL_SIGSPEC_MAX];
        rtlil_emit_expr(out, mod, expr->u.binary.left, a_ss, sizeof(a_ss));
        rtlil_emit_expr(out, mod, expr->u.binary.right, b_ss, sizeof(b_ss));

        int a_width = expr->u.binary.left ? expr->u.binary.left->width : 1;
        int b_width = expr->u.binary.right ? expr->u.binary.right->width : 1;
        /* Fall back to IR signal width when inferred width is 0. */
        if (a_width <= 0 && expr->u.binary.left &&
            expr->u.binary.left->kind == EXPR_SIGNAL_REF)
            a_width = signal_width_from_mod(mod, expr->u.binary.left->u.signal_ref.signal_id);
        if (b_width <= 0 && expr->u.binary.right &&
            expr->u.binary.right->kind == EXPR_SIGNAL_REF)
            b_width = signal_width_from_mod(mod, expr->u.binary.right->u.signal_ref.signal_id);
        if (a_width <= 0) a_width = 1;
        if (b_width <= 0) b_width = 1;

        int y_width = expr->width > 0 ? expr->width : 1;

        /* Comparison and logical ops always produce 1-bit result. */
        switch (expr->kind) {
            case EXPR_BINARY_EQ:
            case EXPR_BINARY_NEQ:
            case EXPR_BINARY_LT:
            case EXPR_BINARY_GT:
            case EXPR_BINARY_LTE:
            case EXPR_BINARY_GTE:
            case EXPR_LOGICAL_AND:
            case EXPR_LOGICAL_OR:
                y_width = 1;
                break;
            default:
                /* For arithmetic ops (add, sub, mul, div, mod), Y_WIDTH
                 * must be at least as wide as the widest operand.  The IR
                 * sometimes has expr->width == 0 for these; using Y_WIDTH=1
                 * produces incorrect results and can crash yosys.
                 * For bitwise/shift ops, preserve the requested width. */
                switch (expr->kind) {
                    case EXPR_BINARY_ADD:
                    case EXPR_BINARY_SUB:
                    case EXPR_BINARY_MUL:
                    case EXPR_BINARY_DIV:
                    case EXPR_BINARY_MOD:
                        if (y_width < a_width) y_width = a_width;
                        if (y_width < b_width) y_width = b_width;
                        break;
                    default:
                        break;
                }
                break;
        }

        int is_signed = (expr->kind == EXPR_BINARY_ASHR) ? 1 : 0;
        emit_binary_cell(out, cell_type,
                         a_ss, a_width, is_signed,
                         b_ss, b_width, is_signed,
                         y_width, out_sigspec, sigspec_size);
        return 0;
    }

    case EXPR_TERNARY: {
        char cond_ss[RTLIL_SIGSPEC_MAX];
        char true_ss[RTLIL_SIGSPEC_MAX];
        char false_ss[RTLIL_SIGSPEC_MAX];

        rtlil_emit_expr(out, mod, expr->u.ternary.condition, cond_ss, sizeof(cond_ss));
        rtlil_emit_expr(out, mod, expr->u.ternary.true_val, true_ss, sizeof(true_ss));
        rtlil_emit_expr(out, mod, expr->u.ternary.false_val, false_ss, sizeof(false_ss));

        int width = expr->width;
        /* When ternary width is unknown, derive from branch widths. */
        if (width <= 0) {
            int tw = expr->u.ternary.true_val ? expr->u.ternary.true_val->width : 0;
            int fw = expr->u.ternary.false_val ? expr->u.ternary.false_val->width : 0;
            width = tw > fw ? tw : fw;
        }
        if (width <= 0) width = 1;

        /* If condition is wider than 1 bit, reduce it first. */
        int cond_width = expr->u.ternary.condition
                       ? expr->u.ternary.condition->width : 1;
        if (cond_width > 1) {
            char reduced_ss[RTLIL_SIGSPEC_MAX];
            emit_unary_cell(out, "$reduce_or", cond_ss, cond_width, 0, 1,
                            reduced_ss, sizeof(reduced_ss));
            emit_mux_cell(out, reduced_ss, false_ss, true_ss, width,
                          out_sigspec, sigspec_size);
        } else {
            emit_mux_cell(out, cond_ss, false_ss, true_ss, width,
                          out_sigspec, sigspec_size);
        }
        return 0;
    }

    case EXPR_MEM_READ: {
        /* Memory reads become $memrd_v2 cells. For now, emit a placeholder wire
         * that will be connected by the memory cell emission pass. */
        const char *mem_name = expr->u.mem_read.memory_name;
        const char *port_name = expr->u.mem_read.port_name;

        /* Find the memory to determine if it's a sync or async read. */
        const IR_Memory *mem = NULL;
        const IR_MemoryPort *mp = NULL;
        if (mod && mem_name) {
            for (int i = 0; i < mod->num_memories; ++i) {
                const IR_Memory *m = &mod->memories[i];
                if (m->name && strcmp(m->name, mem_name) == 0) {
                    mem = m;
                    for (int p = 0; p < m->num_ports; ++p) {
                        if (m->ports[p].name && port_name &&
                            strcmp(m->ports[p].name, port_name) == 0) {
                            mp = &m->ports[p];
                            break;
                        }
                    }
                    break;
                }
            }
        }

        /* Use the memory's word width when the expression width is missing. */
        int width = expr->width > 0 ? expr->width : 1;
        if (mem && mem->word_width > 0 && width < mem->word_width) {
            width = mem->word_width;
        }

        /* Emit address sigspec. */
        char addr_ss[RTLIL_SIGSPEC_MAX];
        if (expr->u.mem_read.address) {
            rtlil_emit_expr(out, mod, expr->u.mem_read.address,
                            addr_ss, sizeof(addr_ss));
        } else if (mp && mp->addr_reg_signal_id >= 0) {
            const IR_Signal *addr_reg = rtlil_find_signal_by_id(mod, mp->addr_reg_signal_id);
            if (addr_reg && addr_reg->name) {
                snprintf(addr_ss, sizeof(addr_ss), "\\%s", addr_reg->name);
            } else {
                snprintf(addr_ss, sizeof(addr_ss), "1'0");
            }
        } else if (mp && mp->addr_signal_id >= 0) {
            const IR_Signal *addr_sig = rtlil_find_signal_by_id(mod, mp->addr_signal_id);
            if (addr_sig && addr_sig->name) {
                snprintf(addr_ss, sizeof(addr_ss), "\\%s", addr_sig->name);
            } else {
                snprintf(addr_ss, sizeof(addr_ss), "1'0");
            }
        } else {
            snprintf(addr_ss, sizeof(addr_ss), "1'0");
        }

        /* Emit a $memrd_v2 cell. */
        char y_wire[128];
        make_temp_wire(out, width, y_wire, sizeof(y_wire));

        int id = rtlil_next_id();
        /* Memory reads emitted as expressions are always used inside RTLIL
         * processes which provide their own clock via sync posedge/negedge.
         * Using CLK_ENABLE=1 would add an extra register stage (double-
         * registering the read).  Yosys models Verilog "reg <= mem[addr]"
         * inside always @(posedge clk) as an async $memrd + process latch,
         * so we match that by always using CLK_ENABLE=0 here. */
        int is_sync = 0;
        int addr_width = mem ? mem->address_width : 1;
        if (addr_width <= 0) addr_width = 1;

        /* Determine the clock signal for sync reads. */
        const char *clk_ss = "1'0";
        int clk_polarity = 1;
        if (is_sync && mp && mod) {
            /* Try to find the clock from the memory port's clock domain. */
            if (mp->sync_clock_domain_id >= 0) {
                const IR_ClockDomain *cd = rtlil_find_clock_domain_by_id(
                    mod, mp->sync_clock_domain_id);
                if (cd) {
                    const IR_Signal *clk_sig = rtlil_find_signal_by_id(
                        mod, cd->clock_signal_id);
                    if (clk_sig && clk_sig->name) {
                        static char clk_buf[128];
                        snprintf(clk_buf, sizeof(clk_buf), "\\%s", clk_sig->name);
                        clk_ss = clk_buf;
                        clk_polarity = (cd->edge == EDGE_FALLING) ? 0 : 1;
                    }
                }
            }
            /* Fallback: find any clock domain that references this memory. */
            if (strcmp(clk_ss, "1'0") == 0) {
                for (int cdi = 0; cdi < mod->num_clock_domains; ++cdi) {
                    const IR_ClockDomain *cd = &mod->clock_domains[cdi];
                    const IR_Signal *clk_sig = rtlil_find_signal_by_id(
                        mod, cd->clock_signal_id);
                    if (clk_sig && clk_sig->name) {
                        static char clk_buf2[128];
                        snprintf(clk_buf2, sizeof(clk_buf2), "\\%s", clk_sig->name);
                        clk_ss = clk_buf2;
                        clk_polarity = (cd->edge == EDGE_FALLING) ? 0 : 1;
                        break;
                    }
                }
            }
        }

        rtlil_indent(out, 1);
        fprintf(out, "cell $memrd_v2 $auto$%d\n", id);
        rtlil_indent(out, 2);
        fprintf(out, "parameter \\MEMID \"\\\\%s\"\n", mem_name ? mem_name : "jz_mem");
        rtlil_indent(out, 2);
        fprintf(out, "parameter \\ABITS %d\n", addr_width);
        rtlil_indent(out, 2);
        fprintf(out, "parameter \\WIDTH %d\n", width);
        rtlil_indent(out, 2);
        fprintf(out, "parameter \\CLK_ENABLE %d\n", is_sync);
        rtlil_indent(out, 2);
        fprintf(out, "parameter \\CLK_POLARITY %d\n", clk_polarity);
        rtlil_indent(out, 2);
        fprintf(out, "parameter \\TRANSPARENCY_MASK 0\n");
        rtlil_indent(out, 2);
        fprintf(out, "parameter \\COLLISION_X_MASK 0\n");
        rtlil_indent(out, 2);
        fprintf(out, "parameter \\CE_OVER_SRST 0\n");
        rtlil_indent(out, 2);
        fprintf(out, "parameter \\ARST_VALUE %d'", width);
        for (int b = 0; b < width; ++b) fputc('0', out);
        fputc('\n', out);
        rtlil_indent(out, 2);
        fprintf(out, "parameter \\SRST_VALUE %d'", width);
        for (int b = 0; b < width; ++b) fputc('0', out);
        fputc('\n', out);
        rtlil_indent(out, 2);
        fprintf(out, "parameter \\INIT_VALUE %d'", width);
        for (int b = 0; b < width; ++b) fputc('x', out);
        fputc('\n', out);
        rtlil_indent(out, 2);
        fprintf(out, "connect \\ADDR %s\n", addr_ss);
        rtlil_indent(out, 2);
        fprintf(out, "connect \\DATA %s\n", y_wire);
        rtlil_indent(out, 2);
        fprintf(out, "connect \\EN 1'1\n");
        rtlil_indent(out, 2);
        fprintf(out, "connect \\ARST 1'0\n");
        rtlil_indent(out, 2);
        fprintf(out, "connect \\SRST 1'0\n");
        rtlil_indent(out, 2);
        fprintf(out, "connect \\CLK %s\n", clk_ss);
        rtlil_indent(out, 1);
        fprintf(out, "end\n");

        snprintf(out_sigspec, sigspec_size, "%s", y_wire);
        return 0;
    }

    /* Widening arithmetic intrinsics. */
    case EXPR_INTRINSIC_UADD:
    case EXPR_INTRINSIC_SADD: {
        char a_ss[RTLIL_SIGSPEC_MAX], b_ss[RTLIL_SIGSPEC_MAX];
        rtlil_emit_expr(out, mod, expr->u.binary.left, a_ss, sizeof(a_ss));
        rtlil_emit_expr(out, mod, expr->u.binary.right, b_ss, sizeof(b_ss));
        int a_width = expr->u.binary.left ? expr->u.binary.left->width : 1;
        int b_width = expr->u.binary.right ? expr->u.binary.right->width : 1;
        /* Fall back to IR signal width when inferred width is 0. */
        if (a_width <= 0 && expr->u.binary.left &&
            expr->u.binary.left->kind == EXPR_SIGNAL_REF)
            a_width = signal_width_from_mod(mod, expr->u.binary.left->u.signal_ref.signal_id);
        if (b_width <= 0 && expr->u.binary.right &&
            expr->u.binary.right->kind == EXPR_SIGNAL_REF)
            b_width = signal_width_from_mod(mod, expr->u.binary.right->u.signal_ref.signal_id);
        if (a_width <= 0) a_width = 1;
        if (b_width <= 0) b_width = 1;
        int y_width = expr->width > 0 ? expr->width : a_width + 1;
        int is_signed = (expr->kind == EXPR_INTRINSIC_SADD) ? 1 : 0;
        emit_binary_cell(out, "$add",
                         a_ss, a_width, is_signed,
                         b_ss, b_width, is_signed,
                         y_width, out_sigspec, sigspec_size);
        return 0;
    }

    case EXPR_INTRINSIC_UMUL:
    case EXPR_INTRINSIC_SMUL: {
        char a_ss[RTLIL_SIGSPEC_MAX], b_ss[RTLIL_SIGSPEC_MAX];
        rtlil_emit_expr(out, mod, expr->u.binary.left, a_ss, sizeof(a_ss));
        rtlil_emit_expr(out, mod, expr->u.binary.right, b_ss, sizeof(b_ss));
        int a_width = expr->u.binary.left ? expr->u.binary.left->width : 1;
        int b_width = expr->u.binary.right ? expr->u.binary.right->width : 1;
        /* Fall back to IR signal width when inferred width is 0. */
        if (a_width <= 0 && expr->u.binary.left &&
            expr->u.binary.left->kind == EXPR_SIGNAL_REF)
            a_width = signal_width_from_mod(mod, expr->u.binary.left->u.signal_ref.signal_id);
        if (b_width <= 0 && expr->u.binary.right &&
            expr->u.binary.right->kind == EXPR_SIGNAL_REF)
            b_width = signal_width_from_mod(mod, expr->u.binary.right->u.signal_ref.signal_id);
        if (a_width <= 0) a_width = 1;
        if (b_width <= 0) b_width = 1;
        int y_width = expr->width > 0 ? expr->width : a_width + b_width;
        int is_signed = (expr->kind == EXPR_INTRINSIC_SMUL) ? 1 : 0;
        emit_binary_cell(out, "$mul",
                         a_ss, a_width, is_signed,
                         b_ss, b_width, is_signed,
                         y_width, out_sigspec, sigspec_size);
        return 0;
    }

    case EXPR_INTRINSIC_GBIT: {
        /* gbit(source, index) → shift right, then mask to 1 bit. */
        char src_ss[RTLIL_SIGSPEC_MAX], idx_ss[RTLIL_SIGSPEC_MAX];
        rtlil_emit_expr(out, mod, expr->u.intrinsic.source, src_ss, sizeof(src_ss));
        rtlil_emit_expr(out, mod, expr->u.intrinsic.index, idx_ss, sizeof(idx_ss));
        int src_width = expr->u.intrinsic.source ? expr->u.intrinsic.source->width : 1;
        int idx_width = expr->u.intrinsic.index ? expr->u.intrinsic.index->width : 1;
        if (src_width <= 0) src_width = 1;
        if (idx_width <= 0) idx_width = 1;

        char shifted[RTLIL_SIGSPEC_MAX];
        emit_binary_cell(out, "$shr", src_ss, src_width, 0,
                         idx_ss, idx_width, 0, src_width,
                         shifted, sizeof(shifted));
        char one_ss[32];
        const_val_sigspec(one_ss, sizeof(one_ss), src_width, 1);
        emit_binary_cell(out, "$and", shifted, src_width, 0,
                         one_ss, src_width, 0, 1,
                         out_sigspec, sigspec_size);
        return 0;
    }

    case EXPR_INTRINSIC_GSLICE: {
        /* gslice(source, index, element_width) → shift by index*ew, mask. */
        char src_ss[RTLIL_SIGSPEC_MAX], idx_ss[RTLIL_SIGSPEC_MAX];
        rtlil_emit_expr(out, mod, expr->u.intrinsic.source, src_ss, sizeof(src_ss));
        rtlil_emit_expr(out, mod, expr->u.intrinsic.index, idx_ss, sizeof(idx_ss));
        int src_width = expr->u.intrinsic.source ? expr->u.intrinsic.source->width : 1;
        int idx_width = expr->u.intrinsic.index ? expr->u.intrinsic.index->width : 1;
        int ew = expr->u.intrinsic.element_width;
        if (ew <= 0) ew = expr->width > 0 ? expr->width : 1;
        if (src_width <= 0) src_width = 1;
        if (idx_width <= 0) idx_width = 1;

        /* Multiply index by element_width. */
        char shift_amt[RTLIL_SIGSPEC_MAX];
        int shift_width = 32;
        if (ew != 1) {
            char ew_ss[64];
            const_val_sigspec(ew_ss, sizeof(ew_ss), 32, (uint64_t)ew);
            emit_binary_cell(out, "$mul", idx_ss, idx_width, 0,
                             ew_ss, 32, 0, 32,
                             shift_amt, sizeof(shift_amt));
            shift_width = 32;
        } else {
            snprintf(shift_amt, sizeof(shift_amt), "%s", idx_ss);
            shift_width = idx_width;
        }

        char shifted[RTLIL_SIGSPEC_MAX];
        emit_binary_cell(out, "$shr", src_ss, src_width, 0,
                         shift_amt, shift_width, 0, src_width,
                         shifted, sizeof(shifted));

        uint64_t mask_val = (ew < 64) ? ((1ULL << ew) - 1) : ~0ULL;
        char mask_ss[128];
        const_val_sigspec(mask_ss, sizeof(mask_ss), src_width, mask_val);
        emit_binary_cell(out, "$and", shifted, src_width, 0,
                         mask_ss, src_width, 0, ew,
                         out_sigspec, sigspec_size);
        return 0;
    }

    case EXPR_INTRINSIC_SBIT: {
        /* sbit: clear bit at index, then set it to value. */
        char src_ss[RTLIL_SIGSPEC_MAX], idx_ss[RTLIL_SIGSPEC_MAX], val_ss[RTLIL_SIGSPEC_MAX];
        rtlil_emit_expr(out, mod, expr->u.intrinsic.source, src_ss, sizeof(src_ss));
        rtlil_emit_expr(out, mod, expr->u.intrinsic.index, idx_ss, sizeof(idx_ss));
        rtlil_emit_expr(out, mod, expr->u.intrinsic.value, val_ss, sizeof(val_ss));
        int width = expr->width > 0 ? expr->width : 1;
        int idx_w = expr->u.intrinsic.index ? expr->u.intrinsic.index->width : 1;
        if (idx_w <= 0) idx_w = 1;

        /* mask = 1 << idx */
        char one_ss[64];
        const_val_sigspec(one_ss, sizeof(one_ss), width, 1);
        char mask_ss[RTLIL_SIGSPEC_MAX];
        emit_binary_cell(out, "$shl", one_ss, width, 0, idx_ss, idx_w, 0,
                         width, mask_ss, sizeof(mask_ss));

        /* inv_mask = ~mask */
        char inv_mask[RTLIL_SIGSPEC_MAX];
        emit_unary_cell(out, "$not", mask_ss, width, 0, width,
                        inv_mask, sizeof(inv_mask));

        /* cleared = src & ~mask */
        char cleared[RTLIL_SIGSPEC_MAX];
        emit_binary_cell(out, "$and", src_ss, width, 0, inv_mask, width, 0,
                         width, cleared, sizeof(cleared));

        /* val_shifted = (val_extended & mask) */
        /* First replicate val to width bits, then shift to position */
        /* Actually: val_shifted = (replicated_val) << idx & mask */
        /* Simpler: val_at_pos = val << idx then mask it */
        char val_shifted[RTLIL_SIGSPEC_MAX];
        emit_binary_cell(out, "$shl", val_ss, 1, 0, idx_ss, idx_w, 0,
                         width, val_shifted, sizeof(val_shifted));
        char val_masked[RTLIL_SIGSPEC_MAX];
        emit_binary_cell(out, "$and", val_shifted, width, 0, mask_ss, width, 0,
                         width, val_masked, sizeof(val_masked));

        /* result = cleared | val_masked */
        emit_binary_cell(out, "$or", cleared, width, 0, val_masked, width, 0,
                         width, out_sigspec, sigspec_size);
        return 0;
    }

    case EXPR_INTRINSIC_SSLICE: {
        /* sslice: clear element at index, then insert value. */
        char src_ss[RTLIL_SIGSPEC_MAX], idx_ss[RTLIL_SIGSPEC_MAX], val_ss[RTLIL_SIGSPEC_MAX];
        rtlil_emit_expr(out, mod, expr->u.intrinsic.source, src_ss, sizeof(src_ss));
        rtlil_emit_expr(out, mod, expr->u.intrinsic.index, idx_ss, sizeof(idx_ss));
        rtlil_emit_expr(out, mod, expr->u.intrinsic.value, val_ss, sizeof(val_ss));
        int width = expr->width > 0 ? expr->width : 1;
        int ew = expr->u.intrinsic.element_width;
        if (ew <= 0) ew = expr->u.intrinsic.value ? expr->u.intrinsic.value->width : 1;
        int idx_w = expr->u.intrinsic.index ? expr->u.intrinsic.index->width : 1;
        if (idx_w <= 0) idx_w = 1;

        /* mask = {ew{1'b1}} << (idx * ew) */
        uint64_t mask_val = (ew < 64) ? ((1ULL << ew) - 1) : ~0ULL;
        char elem_mask[64];
        const_val_sigspec(elem_mask, sizeof(elem_mask), width, mask_val);

        char shift_amt[RTLIL_SIGSPEC_MAX];
        int shift_w = 32;
        if (ew != 1) {
            char ew_ss[64];
            const_val_sigspec(ew_ss, sizeof(ew_ss), 32, (uint64_t)ew);
            emit_binary_cell(out, "$mul", idx_ss, idx_w, 0, ew_ss, 32, 0,
                             32, shift_amt, sizeof(shift_amt));
            shift_w = 32;
        } else {
            snprintf(shift_amt, sizeof(shift_amt), "%s", idx_ss);
            shift_w = idx_w;
        }

        char mask_shifted[RTLIL_SIGSPEC_MAX];
        emit_binary_cell(out, "$shl", elem_mask, width, 0, shift_amt, shift_w, 0,
                         width, mask_shifted, sizeof(mask_shifted));

        char inv_mask[RTLIL_SIGSPEC_MAX];
        emit_unary_cell(out, "$not", mask_shifted, width, 0, width,
                        inv_mask, sizeof(inv_mask));

        char cleared[RTLIL_SIGSPEC_MAX];
        emit_binary_cell(out, "$and", src_ss, width, 0, inv_mask, width, 0,
                         width, cleared, sizeof(cleared));

        /* Pad value to source width, shift to position, mask */
        char val_padded[RTLIL_SIGSPEC_MAX];
        snprintf(val_padded, sizeof(val_padded), "%s", val_ss);

        /* Use the actual value's width for A_WIDTH so it matches the sigspec. */
        int val_actual_w = expr->u.intrinsic.value
                         ? expr->u.intrinsic.value->width : ew;
        if (val_actual_w <= 0) val_actual_w = ew;

        char val_shifted[RTLIL_SIGSPEC_MAX];
        emit_binary_cell(out, "$shl", val_padded, val_actual_w, 0,
                         shift_amt, shift_w, 0,
                         width, val_shifted, sizeof(val_shifted));

        char val_masked[RTLIL_SIGSPEC_MAX];
        emit_binary_cell(out, "$and", val_shifted, width, 0,
                         mask_shifted, width, 0, width,
                         val_masked, sizeof(val_masked));

        emit_binary_cell(out, "$or", cleared, width, 0, val_masked, width, 0,
                         width, out_sigspec, sigspec_size);
        return 0;
    }

    case EXPR_INTRINSIC_OH2B: {
        /* One-hot to binary encoder using $reduce_or cells. */
        const IR_Expr *src = expr->u.intrinsic.source;
        int src_w = src ? src->width : 0;
        int result_w = expr->width;
        if (!src || src_w <= 0 || result_w <= 0) {
            snprintf(out_sigspec, sigspec_size, "1'0");
            return 0;
        }

        char src_ss[RTLIL_SIGSPEC_MAX];
        rtlil_emit_expr(out, mod, src, src_ss, sizeof(src_ss));

        /* For each result bit k, OR together source bits whose index has bit k set. */
        char bit_specs[32][128]; /* max 32 result bits */
        for (int k = 0; k < result_w && k < 32; ++k) {
            /* Build a mask of contributing source bits. */
            uint64_t mask = 0;
            for (int i = 0; i < src_w && i < 64; ++i) {
                if ((i >> k) & 1) {
                    mask |= (1ULL << i);
                }
            }

            if (mask == 0) {
                snprintf(bit_specs[k], sizeof(bit_specs[k]), "1'0");
            } else {
                /* AND source with mask, then reduce_or. */
                char mask_ss[128];
                const_val_sigspec(mask_ss, sizeof(mask_ss), src_w, mask);

                char masked[RTLIL_SIGSPEC_MAX];
                emit_binary_cell(out, "$and", src_ss, src_w, 0,
                                 mask_ss, src_w, 0, src_w,
                                 masked, sizeof(masked));

                emit_unary_cell(out, "$reduce_or", masked, src_w, 0, 1,
                                bit_specs[k], sizeof(bit_specs[k]));
            }
        }

        /* Concatenate result bits: { bit[result_w-1] ... bit[0] } */
        if (result_w == 1) {
            snprintf(out_sigspec, sigspec_size, "%s", bit_specs[0]);
        } else {
            int pos = 0;
            pos += snprintf(out_sigspec + pos, sigspec_size - (size_t)pos, "{ ");
            for (int k = result_w - 1; k >= 0; --k) {
                if (k != result_w - 1)
                    pos += snprintf(out_sigspec + pos, sigspec_size - (size_t)pos, " ");
                pos += snprintf(out_sigspec + pos, sigspec_size - (size_t)pos, "%s",
                                k < 32 ? bit_specs[k] : "1'0");
            }
            pos += snprintf(out_sigspec + pos, sigspec_size - (size_t)pos, " }");
        }
        return 0;
    }

    /* ---- Reduction operators ---- */
    case EXPR_INTRINSIC_REDUCE_AND: {
        const IR_Expr *src = expr->u.intrinsic.source;
        if (!src || src->width <= 0) { snprintf(out_sigspec, sigspec_size, "1'0"); return 0; }
        char src_ss[RTLIL_SIGSPEC_MAX];
        rtlil_emit_expr(out, mod, src, src_ss, sizeof(src_ss));
        emit_unary_cell(out, "$reduce_and", src_ss, src->width, 0, 1,
                         out_sigspec, sigspec_size);
        return 0;
    }
    case EXPR_INTRINSIC_REDUCE_OR: {
        const IR_Expr *src = expr->u.intrinsic.source;
        if (!src || src->width <= 0) { snprintf(out_sigspec, sigspec_size, "1'0"); return 0; }
        char src_ss[RTLIL_SIGSPEC_MAX];
        rtlil_emit_expr(out, mod, src, src_ss, sizeof(src_ss));
        emit_unary_cell(out, "$reduce_or", src_ss, src->width, 0, 1,
                         out_sigspec, sigspec_size);
        return 0;
    }
    case EXPR_INTRINSIC_REDUCE_XOR: {
        const IR_Expr *src = expr->u.intrinsic.source;
        if (!src || src->width <= 0) { snprintf(out_sigspec, sigspec_size, "1'0"); return 0; }
        char src_ss[RTLIL_SIGSPEC_MAX];
        rtlil_emit_expr(out, mod, src, src_ss, sizeof(src_ss));
        emit_unary_cell(out, "$reduce_xor", src_ss, src->width, 0, 1,
                         out_sigspec, sigspec_size);
        return 0;
    }

    /* ---- Widening subtract ---- */
    case EXPR_INTRINSIC_USUB:
    case EXPR_INTRINSIC_SSUB: {
        char a_ss[RTLIL_SIGSPEC_MAX], b_ss[RTLIL_SIGSPEC_MAX];
        rtlil_emit_expr(out, mod, expr->u.binary.left, a_ss, sizeof(a_ss));
        rtlil_emit_expr(out, mod, expr->u.binary.right, b_ss, sizeof(b_ss));
        int a_width = expr->u.binary.left ? expr->u.binary.left->width : 1;
        int b_width = expr->u.binary.right ? expr->u.binary.right->width : 1;
        if (a_width <= 0) a_width = 1;
        if (b_width <= 0) b_width = 1;
        int y_width = expr->width > 0 ? expr->width : ((a_width > b_width ? a_width : b_width) + 1);
        int is_signed = (expr->kind == EXPR_INTRINSIC_SSUB) ? 1 : 0;
        emit_binary_cell(out, "$sub",
                         a_ss, a_width, is_signed,
                         b_ss, b_width, is_signed,
                         y_width, out_sigspec, sigspec_size);
        return 0;
    }

    /* ---- Min/Max using $lt/$gt + $mux ---- */
    case EXPR_INTRINSIC_UMIN:
    case EXPR_INTRINSIC_UMAX:
    case EXPR_INTRINSIC_SMIN:
    case EXPR_INTRINSIC_SMAX: {
        char a_ss[RTLIL_SIGSPEC_MAX], b_ss[RTLIL_SIGSPEC_MAX];
        rtlil_emit_expr(out, mod, expr->u.binary.left, a_ss, sizeof(a_ss));
        rtlil_emit_expr(out, mod, expr->u.binary.right, b_ss, sizeof(b_ss));
        int a_width = expr->u.binary.left ? expr->u.binary.left->width : 1;
        int b_width = expr->u.binary.right ? expr->u.binary.right->width : 1;
        if (a_width <= 0) a_width = 1;
        if (b_width <= 0) b_width = 1;
        int result_w = expr->width > 0 ? expr->width : (a_width > b_width ? a_width : b_width);
        int is_signed = (expr->kind == EXPR_INTRINSIC_SMIN || expr->kind == EXPR_INTRINSIC_SMAX) ? 1 : 0;
        int is_max = (expr->kind == EXPR_INTRINSIC_UMAX || expr->kind == EXPR_INTRINSIC_SMAX) ? 1 : 0;
        const char *cmp_cell = is_max ? "$gt" : "$lt";
        /* Compare: cond = (a < b) or (a > b) */
        char cmp_ss[RTLIL_SIGSPEC_MAX];
        emit_binary_cell(out, cmp_cell,
                         a_ss, a_width, is_signed,
                         b_ss, b_width, is_signed,
                         1, cmp_ss, sizeof(cmp_ss));
        /* Mux: cond ? a : b */
        emit_mux_cell(out, cmp_ss, b_ss, a_ss, result_w,
                       out_sigspec, sigspec_size);
        return 0;
    }

    /* ---- Absolute value: mux(sign, neg, val) ---- */
    case EXPR_INTRINSIC_ABS: {
        const IR_Expr *src = expr->u.intrinsic.source;
        if (!src || src->width <= 0) { snprintf(out_sigspec, sigspec_size, "1'0"); return 0; }
        char src_ss[RTLIL_SIGSPEC_MAX];
        rtlil_emit_expr(out, mod, src, src_ss, sizeof(src_ss));
        int src_w = src->width;
        int result_w = expr->width > 0 ? expr->width : src_w + 1;
        /* Negate: neg = -src in result_w bits */
        char zero_ss[64];
        const_val_sigspec(zero_ss, sizeof(zero_ss), result_w, 0);
        char neg_ss[RTLIL_SIGSPEC_MAX];
        emit_binary_cell(out, "$sub",
                         zero_ss, result_w, 0,
                         src_ss, src_w, 0,
                         result_w, neg_ss, sizeof(neg_ss));
        /* Sign bit extraction: shift right by (src_w - 1) */
        char shift_ss[64];
        const_val_sigspec(shift_ss, sizeof(shift_ss), 32, (uint64_t)(src_w - 1));
        char sign_ss[RTLIL_SIGSPEC_MAX];
        emit_binary_cell(out, "$shr",
                         src_ss, src_w, 0,
                         shift_ss, 32, 0,
                         1, sign_ss, sizeof(sign_ss));
        /* Zero-extend src to result_w for the false branch */
        char ext_ss[RTLIL_SIGSPEC_MAX];
        if (src_w < result_w) {
            int pad = result_w - src_w;
            snprintf(ext_ss, sizeof(ext_ss), "{ %d'0 %s }", pad, src_ss);
        } else {
            snprintf(ext_ss, sizeof(ext_ss), "%s", src_ss);
        }
        /* Mux: sign ? neg : {1'b0, src} */
        emit_mux_cell(out, sign_ss, ext_ss, neg_ss, result_w,
                       out_sigspec, sigspec_size);
        return 0;
    }

    /* ---- Population count: adder tree of individual bits ---- */
    case EXPR_INTRINSIC_POPCOUNT: {
        const IR_Expr *src = expr->u.intrinsic.source;
        if (!src || src->width <= 0) { snprintf(out_sigspec, sigspec_size, "1'0"); return 0; }
        char src_ss[RTLIL_SIGSPEC_MAX];
        rtlil_emit_expr(out, mod, src, src_ss, sizeof(src_ss));
        int src_w = src->width;
        int result_w = expr->width > 0 ? expr->width : 1;
        /* Start with bit 0, accumulate with $add */
        char bit_ss[64];
        snprintf(bit_ss, sizeof(bit_ss), "%s [0]", src_ss);
        char acc_ss[RTLIL_SIGSPEC_MAX];
        snprintf(acc_ss, sizeof(acc_ss), "%s", bit_ss);
        int acc_w = 1;
        for (int i = 1; i < src_w; i++) {
            char next_bit[64];
            snprintf(next_bit, sizeof(next_bit), "%s [%d]", src_ss, i);
            char sum_ss[RTLIL_SIGSPEC_MAX];
            int sum_w = acc_w < result_w ? acc_w + 1 : result_w;
            emit_binary_cell(out, "$add",
                             acc_ss, acc_w, 0,
                             next_bit, 1, 0,
                             sum_w, sum_ss, sizeof(sum_ss));
            snprintf(acc_ss, sizeof(acc_ss), "%s", sum_ss);
            acc_w = sum_w;
        }
        snprintf(out_sigspec, sigspec_size, "%s", acc_ss);
        return 0;
    }

    /* ---- Bit reversal ---- */
    case EXPR_INTRINSIC_REVERSE: {
        const IR_Expr *src = expr->u.intrinsic.source;
        if (!src || src->width <= 0) { snprintf(out_sigspec, sigspec_size, "1'0"); return 0; }
        char src_ss[RTLIL_SIGSPEC_MAX];
        rtlil_emit_expr(out, mod, src, src_ss, sizeof(src_ss));
        int w = src->width;
        /* Build reversed concat: { src[0] src[1] ... src[N-1] } */
        int pos = 0;
        pos += snprintf(out_sigspec + pos, sigspec_size - (size_t)pos, "{ ");
        for (int i = 0; i < w; i++) {
            if (i > 0) pos += snprintf(out_sigspec + pos, sigspec_size - (size_t)pos, " ");
            pos += snprintf(out_sigspec + pos, sigspec_size - (size_t)pos, "%s [%d]", src_ss, i);
        }
        pos += snprintf(out_sigspec + pos, sigspec_size - (size_t)pos, " }");
        return 0;
    }

    /* ---- Byte swap ---- */
    case EXPR_INTRINSIC_BSWAP: {
        const IR_Expr *src = expr->u.intrinsic.source;
        if (!src || src->width <= 0 || (src->width % 8) != 0) {
            snprintf(out_sigspec, sigspec_size, "1'0"); return 0;
        }
        char src_ss[RTLIL_SIGSPEC_MAX];
        rtlil_emit_expr(out, mod, src, src_ss, sizeof(src_ss));
        int w = src->width;
        int num_bytes = w / 8;
        /* Build swapped concat: { src[7:0] src[15:8] ... src[N-1:N-8] } */
        int pos = 0;
        pos += snprintf(out_sigspec + pos, sigspec_size - (size_t)pos, "{ ");
        for (int i = 0; i < num_bytes; i++) {
            if (i > 0) pos += snprintf(out_sigspec + pos, sigspec_size - (size_t)pos, " ");
            int lo = i * 8;
            int hi = lo + 7;
            pos += snprintf(out_sigspec + pos, sigspec_size - (size_t)pos,
                            "%s [%d:%d]", src_ss, hi, lo);
        }
        pos += snprintf(out_sigspec + pos, sigspec_size - (size_t)pos, " }");
        return 0;
    }

    /* ---- Binary to one-hot ---- */
    case EXPR_INTRINSIC_B2OH: {
        const IR_Expr *idx = expr->u.intrinsic.source;
        if (!idx || expr->width <= 0) { snprintf(out_sigspec, sigspec_size, "1'0"); return 0; }
        char idx_ss[RTLIL_SIGSPEC_MAX];
        rtlil_emit_expr(out, mod, idx, idx_ss, sizeof(idx_ss));
        int idx_w = idx->width > 0 ? idx->width : 1;
        int result_w = expr->width;
        /* 1 << idx */
        char one_ss[64];
        const_val_sigspec(one_ss, sizeof(one_ss), result_w, 1);
        char shifted_ss[RTLIL_SIGSPEC_MAX];
        emit_binary_cell(out, "$shl",
                         one_ss, result_w, 0,
                         idx_ss, idx_w, 0,
                         result_w, shifted_ss, sizeof(shifted_ss));
        /* Bounds check: idx < result_w.
         * The limit constant may need more bits than idx_w (e.g., result_w=8
         * requires 4 bits but idx_w may be only 3). Use enough bits to hold
         * the value. */
        int limit_w = idx_w;
        {
            unsigned tmp = (unsigned)result_w;
            int needed = 0;
            while (tmp) { needed++; tmp >>= 1; }
            if (needed > limit_w) limit_w = needed;
        }
        char limit_ss[64];
        const_val_sigspec(limit_ss, sizeof(limit_ss), limit_w, (uint64_t)result_w);
        char cmp_ss[RTLIL_SIGSPEC_MAX];
        emit_binary_cell(out, "$lt",
                         idx_ss, idx_w, 0,
                         limit_ss, limit_w, 0,
                         1, cmp_ss, sizeof(cmp_ss));
        /* Mux: in-range ? shifted : 0 */
        char zero_ss[64];
        const_val_sigspec(zero_ss, sizeof(zero_ss), result_w, 0);
        emit_mux_cell(out, cmp_ss, zero_ss, shifted_ss, result_w,
                       out_sigspec, sigspec_size);
        return 0;
    }

    /* ---- Priority encoder (MSB-first) ---- */
    case EXPR_INTRINSIC_PRIENC: {
        const IR_Expr *src = expr->u.intrinsic.source;
        if (!src || src->width <= 0) { snprintf(out_sigspec, sigspec_size, "1'0"); return 0; }
        char src_ss[RTLIL_SIGSPEC_MAX];
        rtlil_emit_expr(out, mod, src, src_ss, sizeof(src_ss));
        int src_w = src->width;
        int result_w = expr->width > 0 ? expr->width : 1;
        /* Cascading mux from bit 0 up to MSB */
        char acc_ss[RTLIL_SIGSPEC_MAX];
        const_val_sigspec(acc_ss, sizeof(acc_ss), result_w, 0);
        for (int i = 1; i < src_w; i++) {
            char bit_ss[64];
            snprintf(bit_ss, sizeof(bit_ss), "%s [%d]", src_ss, i);
            char val_ss[64];
            const_val_sigspec(val_ss, sizeof(val_ss), result_w, (uint64_t)i);
            char mux_ss[RTLIL_SIGSPEC_MAX];
            emit_mux_cell(out, bit_ss, acc_ss, val_ss, result_w,
                           mux_ss, sizeof(mux_ss));
            snprintf(acc_ss, sizeof(acc_ss), "%s", mux_ss);
        }
        snprintf(out_sigspec, sigspec_size, "%s", acc_ss);
        return 0;
    }

    /* ---- Leading zero count ---- */
    case EXPR_INTRINSIC_LZC: {
        const IR_Expr *src = expr->u.intrinsic.source;
        if (!src || src->width <= 0) { snprintf(out_sigspec, sigspec_size, "1'0"); return 0; }
        char src_ss[RTLIL_SIGSPEC_MAX];
        rtlil_emit_expr(out, mod, src, src_ss, sizeof(src_ss));
        int src_w = src->width;
        int result_w = expr->width > 0 ? expr->width : 1;
        /* Cascading mux from LSB to MSB: default = src_w (all zeros) */
        char acc_ss[RTLIL_SIGSPEC_MAX];
        const_val_sigspec(acc_ss, sizeof(acc_ss), result_w, (uint64_t)src_w);
        for (int i = 0; i < src_w; i++) {
            char bit_ss[64];
            snprintf(bit_ss, sizeof(bit_ss), "%s [%d]", src_ss, i);
            char val_ss[64];
            const_val_sigspec(val_ss, sizeof(val_ss), result_w, (uint64_t)(src_w - 1 - i));
            char mux_ss[RTLIL_SIGSPEC_MAX];
            emit_mux_cell(out, bit_ss, acc_ss, val_ss, result_w,
                           mux_ss, sizeof(mux_ss));
            snprintf(acc_ss, sizeof(acc_ss), "%s", mux_ss);
        }
        snprintf(out_sigspec, sigspec_size, "%s", acc_ss);
        return 0;
    }

    default:
        snprintf(out_sigspec, sigspec_size, "1'0");
        return 0;
    }
}
