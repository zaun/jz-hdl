/**
 * @file ir_library.c
 * @brief CDC library module construction and CDC-to-instance lowering.
 *
 * This file creates real IR_Module entries for CDC primitives (BIT, BUS, FIFO)
 * so the Verilog backend can emit them like any other module, removing the
 * need for hardcoded Verilog strings.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "ir_library.h"
#include "ir_internal.h"

/* -------------------------------------------------------------------------
 * Signal ID allocation for library modules
 *
 * Library modules use a simple incrementing ID scheme starting from 0.
 * These IDs are local to the library module.
 * -------------------------------------------------------------------------
 */

/**
 * @brief Allocate an IR_Signal in an arena-owned array.
 */
static IR_Signal *alloc_signals(JZArena *arena, int count)
{
    IR_Signal *sigs = (IR_Signal *)jz_arena_alloc(arena,
                                                   sizeof(IR_Signal) * (size_t)count);
    if (sigs) {
        memset(sigs, 0, sizeof(IR_Signal) * (size_t)count);
    }
    return sigs;
}

/**
 * @brief Initialize a port signal.
 */
static void init_port_signal(IR_Signal *sig, int id, const char *name,
                             int width, IR_PortDirection dir,
                             int owner_module_id, JZArena *arena)
{
    sig->id = id;
    sig->name = ir_strdup_arena(arena, name);
    sig->kind = SIG_PORT;
    sig->width = width;
    sig->owner_module_id = owner_module_id;
    sig->u.port.direction = dir;
}

/**
 * @brief Initialize a register signal.
 */
static void init_reg_signal(IR_Signal *sig, int id, const char *name,
                            int width, int home_clock_domain_id,
                            int owner_module_id, JZArena *arena)
{
    sig->id = id;
    sig->name = ir_strdup_arena(arena, name);
    sig->kind = SIG_REGISTER;
    sig->width = width;
    sig->owner_module_id = owner_module_id;
    sig->u.reg.home_clock_domain_id = home_clock_domain_id;
    memset(sig->u.reg.reset_value.words, 0, sizeof(sig->u.reg.reset_value.words));
    sig->u.reg.reset_value.width = width;
}

/**
 * @brief Initialize a net/wire signal.
 */
static void init_net_signal(IR_Signal *sig, int id, const char *name,
                            int width, int owner_module_id, JZArena *arena)
{
    sig->id = id;
    sig->name = ir_strdup_arena(arena, name);
    sig->kind = SIG_NET;
    sig->width = width;
    sig->owner_module_id = owner_module_id;
}

/* -------------------------------------------------------------------------
 * Statement construction helpers
 * -------------------------------------------------------------------------
 */

/**
 * @brief Create a non-blocking assignment statement: lhs <= rhs.
 */
static IR_Stmt make_assign_stmt(int lhs_id, IR_Expr *rhs)
{
    IR_Stmt stmt;
    memset(&stmt, 0, sizeof(stmt));
    stmt.kind = STMT_ASSIGNMENT;
    stmt.u.assign.lhs_signal_id = lhs_id;
    stmt.u.assign.rhs = rhs;
    stmt.u.assign.kind = ASSIGN_DRIVE;
    return stmt;
}

/**
 * @brief Create a signal reference expression.
 */
static IR_Expr *make_sig_ref(JZArena *arena, int signal_id, int width)
{
    IR_Expr *e = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
    if (!e) return NULL;
    memset(e, 0, sizeof(*e));
    e->kind = EXPR_SIGNAL_REF;
    e->width = width;
    e->u.signal_ref.signal_id = signal_id;
    return e;
}

/**
 * @brief Create a binary expression.
 */
static IR_Expr *make_binary(JZArena *arena, IR_ExprKind kind,
                            IR_Expr *left, IR_Expr *right, int width)
{
    IR_Expr *e = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
    if (!e) return NULL;
    memset(e, 0, sizeof(*e));
    e->kind = kind;
    e->width = width;
    e->u.binary.left = left;
    e->u.binary.right = right;
    return e;
}

/**
 * @brief Create a slice expression: signal[msb:lsb].
 */
static IR_Expr *make_slice(JZArena *arena, int signal_id, int msb, int lsb)
{
    IR_Expr *e = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
    if (!e) return NULL;
    memset(e, 0, sizeof(*e));
    e->kind = EXPR_SLICE;
    e->width = msb - lsb + 1;
    e->u.slice.signal_id = signal_id;
    e->u.slice.msb = msb;
    e->u.slice.lsb = lsb;
    return e;
}

/**
 * @brief Create a literal expression.
 */
static IR_Expr *make_literal(JZArena *arena, uint64_t value, int width)
{
    IR_Expr *e = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
    if (!e) return NULL;
    memset(e, 0, sizeof(*e));
    e->kind = EXPR_LITERAL;
    e->width = width;
    memset(e->u.literal.literal.words, 0, sizeof(e->u.literal.literal.words));
    e->u.literal.literal.words[0] = value;
    e->u.literal.literal.width = width;
    return e;
}

/**
 * @brief Allocate a block of statements in the arena.
 */
static IR_Stmt *alloc_stmts(JZArena *arena, int count)
{
    IR_Stmt *s = (IR_Stmt *)jz_arena_alloc(arena, sizeof(IR_Stmt) * (size_t)count);
    if (s) {
        memset(s, 0, sizeof(IR_Stmt) * (size_t)count);
    }
    return s;
}

/**
 * @brief Create a STMT_BLOCK wrapping an array of statements.
 */
static IR_Stmt *make_block(JZArena *arena, IR_Stmt *stmts, int count)
{
    IR_Stmt *blk = (IR_Stmt *)jz_arena_alloc(arena, sizeof(IR_Stmt));
    if (!blk) return NULL;
    memset(blk, 0, sizeof(*blk));
    blk->kind = STMT_BLOCK;
    blk->u.block.stmts = stmts;
    blk->u.block.count = count;
    return blk;
}

/**
 * @brief Build the sensitivity list for a clock domain.
 */
static int build_sensitivity_list(IR_ClockDomain *cd, JZArena *arena)
{
    int max_entries = (cd->edge == EDGE_BOTH) ? 3 : 2;
    IR_SensitivityEntry *sens = (IR_SensitivityEntry *)jz_arena_alloc(
        arena, sizeof(IR_SensitivityEntry) * (size_t)max_entries);
    if (!sens) return -1;
    int ns = 0;

    if (cd->edge == EDGE_BOTH) {
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

    if (cd->reset_signal_id >= 0 && cd->reset_type == RESET_IMMEDIATE) {
        sens[ns].signal_id = cd->reset_signal_id;
        sens[ns].edge = (cd->reset_active == RESET_ACTIVE_HIGH)
                      ? EDGE_RISING : EDGE_FALLING;
        ns++;
    }

    cd->sensitivity_list = sens;
    cd->num_sensitivity = ns;
    return 0;
}

/**
 * @brief Allocate a clock domain array.
 */
static IR_ClockDomain *alloc_clock_domains(JZArena *arena, int count)
{
    IR_ClockDomain *cds = (IR_ClockDomain *)jz_arena_alloc(arena,
                                                            sizeof(IR_ClockDomain) * (size_t)count);
    if (cds) {
        memset(cds, 0, sizeof(IR_ClockDomain) * (size_t)count);
    }
    return cds;
}

/* -------------------------------------------------------------------------
 * CDC BIT library module builder
 * -------------------------------------------------------------------------
 *
 * Ports: clk_dest (IN 1), data_in (IN 1), data_out (OUT 1)
 * Regs:  sync_ff1, sync_ff2 (width 1)
 * Clock domain (posedge clk_dest): sync_ff1 <= data_in; sync_ff2 <= sync_ff1;
 * Async: data_out = sync_ff2;
 */
static int build_cdc_bit_module(IR_Module *mod, int module_id, JZArena *arena)
{
    mod->id = module_id;
    mod->name = ir_strdup_arena(arena, "JZHDL_LIB_CDC_BIT");
    mod->base_module_id = -1;
    mod->source_file_id = -1;

    /* Signal IDs */
    enum { SID_CLK_DEST = 0, SID_DATA_IN, SID_DATA_OUT,
           SID_SYNC_FF1, SID_SYNC_FF2, NUM_SIGS };

    IR_Signal *sigs = alloc_signals(arena, NUM_SIGS);
    if (!sigs) return -1;

    init_port_signal(&sigs[SID_CLK_DEST], SID_CLK_DEST, "clk_dest", 1, PORT_IN, module_id, arena);
    init_port_signal(&sigs[SID_DATA_IN], SID_DATA_IN, "data_in", 1, PORT_IN, module_id, arena);
    init_port_signal(&sigs[SID_DATA_OUT], SID_DATA_OUT, "data_out", 1, PORT_OUT, module_id, arena);
    init_reg_signal(&sigs[SID_SYNC_FF1], SID_SYNC_FF1, "sync_ff1", 1, 0, module_id, arena);
    init_reg_signal(&sigs[SID_SYNC_FF2], SID_SYNC_FF2, "sync_ff2", 1, 0, module_id, arena);

    mod->signals = sigs;
    mod->num_signals = NUM_SIGS;

    /* Clock domain: posedge clk_dest */
    IR_ClockDomain *cd = alloc_clock_domains(arena, 1);
    if (!cd) return -1;

    cd[0].id = 0;
    cd[0].clock_signal_id = SID_CLK_DEST;
    cd[0].edge = EDGE_RISING;
    cd[0].reset_signal_id = -1;
    cd[0].reset_sync_signal_id = -1;

    /* Register list for clock domain */
    int *reg_ids = (int *)jz_arena_alloc(arena, sizeof(int) * 2);
    if (!reg_ids) return -1;
    reg_ids[0] = SID_SYNC_FF1;
    reg_ids[1] = SID_SYNC_FF2;
    cd[0].register_ids = reg_ids;
    cd[0].num_registers = 2;

    /* Statements: sync_ff1 <= data_in; sync_ff2 <= sync_ff1; */
    IR_Stmt *cd_stmts = alloc_stmts(arena, 2);
    if (!cd_stmts) return -1;
    cd_stmts[0] = make_assign_stmt(SID_SYNC_FF1, make_sig_ref(arena, SID_DATA_IN, 1));
    cd_stmts[1] = make_assign_stmt(SID_SYNC_FF2, make_sig_ref(arena, SID_SYNC_FF1, 1));
    cd[0].statements = make_block(arena, cd_stmts, 2);

    if (build_sensitivity_list(&cd[0], arena) != 0) return -1;

    mod->clock_domains = cd;
    mod->num_clock_domains = 1;

    /* Async block: data_out = sync_ff2; */
    IR_Stmt *async_stmts = alloc_stmts(arena, 1);
    if (!async_stmts) return -1;
    IR_Stmt assign_out;
    memset(&assign_out, 0, sizeof(assign_out));
    assign_out.kind = STMT_ASSIGNMENT;
    assign_out.u.assign.lhs_signal_id = SID_DATA_OUT;
    assign_out.u.assign.rhs = make_sig_ref(arena, SID_SYNC_FF2, 1);
    assign_out.u.assign.kind = ASSIGN_ALIAS;
    async_stmts[0] = assign_out;
    mod->async_block = make_block(arena, async_stmts, 1);

    return 0;
}

/* -------------------------------------------------------------------------
 * CDC BUS library module builder
 * -------------------------------------------------------------------------
 *
 * Ports: clk_src (IN 1), clk_dest (IN 1), data_in (IN W), data_out (OUT W)
 * Regs:  gray_src, gray_sync1, gray_sync2 (width W)
 * Net:   bin_dest (width W)
 * Clock domain 1 (posedge clk_src): gray_src <= (data_in >> 1) ^ data_in;
 * Clock domain 2 (posedge clk_dest): gray_sync1 <= gray_src; gray_sync2 <= gray_sync1;
 * Async: Gray-to-binary decode, data_out = bin_dest;
 */
static int build_cdc_bus_module(IR_Module *mod, int module_id,
                                int width, JZArena *arena)
{
    char name_buf[64];
    snprintf(name_buf, sizeof(name_buf), "JZHDL_LIB_CDC_BUS__W%d", width);

    mod->id = module_id;
    mod->name = ir_strdup_arena(arena, name_buf);
    mod->base_module_id = -1;
    mod->source_file_id = -1;

    /* Signal IDs */
    enum { SID_CLK_SRC = 0, SID_CLK_DEST, SID_DATA_IN, SID_DATA_OUT,
           SID_GRAY_SRC, SID_GRAY_SYNC1, SID_GRAY_SYNC2, SID_BIN_DEST,
           NUM_SIGS };

    IR_Signal *sigs = alloc_signals(arena, NUM_SIGS);
    if (!sigs) return -1;

    init_port_signal(&sigs[SID_CLK_SRC], SID_CLK_SRC, "clk_src", 1, PORT_IN, module_id, arena);
    init_port_signal(&sigs[SID_CLK_DEST], SID_CLK_DEST, "clk_dest", 1, PORT_IN, module_id, arena);
    init_port_signal(&sigs[SID_DATA_IN], SID_DATA_IN, "data_in", width, PORT_IN, module_id, arena);
    init_port_signal(&sigs[SID_DATA_OUT], SID_DATA_OUT, "data_out", width, PORT_OUT, module_id, arena);
    init_reg_signal(&sigs[SID_GRAY_SRC], SID_GRAY_SRC, "gray_src", width, 0, module_id, arena);
    init_reg_signal(&sigs[SID_GRAY_SYNC1], SID_GRAY_SYNC1, "gray_sync1", width, 1, module_id, arena);
    init_reg_signal(&sigs[SID_GRAY_SYNC2], SID_GRAY_SYNC2, "gray_sync2", width, 1, module_id, arena);
    init_net_signal(&sigs[SID_BIN_DEST], SID_BIN_DEST, "bin_dest", width, module_id, arena);

    mod->signals = sigs;
    mod->num_signals = NUM_SIGS;

    /* Clock domain 0: posedge clk_src
     * gray_src <= (data_in >> 1) ^ data_in;
     */
    IR_ClockDomain *cds = alloc_clock_domains(arena, 2);
    if (!cds) return -1;

    cds[0].id = 0;
    cds[0].clock_signal_id = SID_CLK_SRC;
    cds[0].edge = EDGE_RISING;
    cds[0].reset_signal_id = -1;
    cds[0].reset_sync_signal_id = -1;
    int *reg_ids0 = (int *)jz_arena_alloc(arena, sizeof(int));
    if (!reg_ids0) return -1;
    reg_ids0[0] = SID_GRAY_SRC;
    cds[0].register_ids = reg_ids0;
    cds[0].num_registers = 1;

    {
        IR_Stmt *stmts = alloc_stmts(arena, 1);
        if (!stmts) return -1;
        /* (data_in >> 1) ^ data_in */
        IR_Expr *shift = make_binary(arena, EXPR_BINARY_SHR,
                                     make_sig_ref(arena, SID_DATA_IN, width),
                                     make_literal(arena, 1, width),
                                     width);
        IR_Expr *gray = make_binary(arena, EXPR_BINARY_XOR,
                                    shift,
                                    make_sig_ref(arena, SID_DATA_IN, width),
                                    width);
        stmts[0] = make_assign_stmt(SID_GRAY_SRC, gray);
        cds[0].statements = make_block(arena, stmts, 1);
    }

    /* Clock domain 1: posedge clk_dest
     * gray_sync1 <= gray_src; gray_sync2 <= gray_sync1;
     */
    cds[1].id = 1;
    cds[1].clock_signal_id = SID_CLK_DEST;
    cds[1].edge = EDGE_RISING;
    cds[1].reset_signal_id = -1;
    cds[1].reset_sync_signal_id = -1;
    int *reg_ids1 = (int *)jz_arena_alloc(arena, sizeof(int) * 2);
    if (!reg_ids1) return -1;
    reg_ids1[0] = SID_GRAY_SYNC1;
    reg_ids1[1] = SID_GRAY_SYNC2;
    cds[1].register_ids = reg_ids1;
    cds[1].num_registers = 2;

    {
        IR_Stmt *stmts = alloc_stmts(arena, 2);
        if (!stmts) return -1;
        stmts[0] = make_assign_stmt(SID_GRAY_SYNC1, make_sig_ref(arena, SID_GRAY_SRC, width));
        stmts[1] = make_assign_stmt(SID_GRAY_SYNC2, make_sig_ref(arena, SID_GRAY_SYNC1, width));
        cds[1].statements = make_block(arena, stmts, 2);
    }

    if (build_sensitivity_list(&cds[0], arena) != 0) return -1;
    if (build_sensitivity_list(&cds[1], arena) != 0) return -1;

    mod->clock_domains = cds;
    mod->num_clock_domains = 2;

    /* Async block: Gray-to-binary decode and data_out assignment.
     *
     * The Verilog-2005 approach uses a for loop with integer variable,
     * but our IR doesn't have loops. Instead we build explicit per-bit
     * XOR cascade assignments into bin_dest, then alias data_out = bin_dest.
     *
     * bin_dest[W-1] = gray_sync2[W-1];
     * bin_dest[i] = bin_dest[i+1] ^ gray_sync2[i];  for i = W-2 .. 0
     * data_out = bin_dest;
     *
     * We represent this as:
     *   bin_dest = gray_sync2[W-1:W-1] (MSB only, then cascade XOR down)
     *
     * Actually, the simplest approach that produces correct Verilog is to
     * keep bin_dest as a reg assigned in an always@* block. However, our IR
     * uses ASSIGN_ALIAS for continuous assignments in async blocks.
     *
     * For the bus module, we'll use a simpler approach: assign each bit of
     * bin_dest individually would be complex. Instead, we'll build a
     * combinational block similar to the original Verilog's always @(*) block.
     *
     * Since the original uses blocking assignments in always @(*), and our
     * async block emits always @(*), we can build the same logic:
     *   bin_dest[W-1] = gray_sync2[W-1];
     *   bin_dest[i] = bin_dest[i+1] ^ gray_sync2[i];
     *
     * But IR assignments are by signal, with optional slicing. Let's build
     * sliced assignments for each bit.
     */
    {
        /* W sliced assignments for bin_dest + 1 alias for data_out = bin_dest */
        int num_async = width + 1;
        IR_Stmt *async_stmts = alloc_stmts(arena, num_async);
        if (!async_stmts) return -1;

        /* bin_dest[W-1] = gray_sync2[W-1]; */
        {
            IR_Stmt *s = &async_stmts[0];
            s->kind = STMT_ASSIGNMENT;
            s->u.assign.lhs_signal_id = SID_BIN_DEST;
            s->u.assign.rhs = make_slice(arena, SID_GRAY_SYNC2, width - 1, width - 1);
            s->u.assign.kind = ASSIGN_DRIVE;
            s->u.assign.is_sliced = true;
            s->u.assign.lhs_msb = width - 1;
            s->u.assign.lhs_lsb = width - 1;
        }

        /* bin_dest[i] = bin_dest[i+1] ^ gray_sync2[i]; for i = W-2 down to 0 */
        for (int i = width - 2; i >= 0; --i) {
            int stmt_idx = (width - 2 - i) + 1;
            IR_Stmt *s = &async_stmts[stmt_idx];
            s->kind = STMT_ASSIGNMENT;
            s->u.assign.lhs_signal_id = SID_BIN_DEST;
            s->u.assign.rhs = make_binary(arena, EXPR_BINARY_XOR,
                                          make_slice(arena, SID_BIN_DEST, i + 1, i + 1),
                                          make_slice(arena, SID_GRAY_SYNC2, i, i),
                                          1);
            s->u.assign.kind = ASSIGN_DRIVE;
            s->u.assign.is_sliced = true;
            s->u.assign.lhs_msb = i;
            s->u.assign.lhs_lsb = i;
        }

        /* data_out = bin_dest; */
        {
            IR_Stmt *s = &async_stmts[width];
            s->kind = STMT_ASSIGNMENT;
            s->u.assign.lhs_signal_id = SID_DATA_OUT;
            s->u.assign.rhs = make_sig_ref(arena, SID_BIN_DEST, width);
            s->u.assign.kind = ASSIGN_ALIAS;
        }

        mod->async_block = make_block(arena, async_stmts, num_async);
    }

    return 0;
}

/* -------------------------------------------------------------------------
 * CDC FIFO library module builder
 * -------------------------------------------------------------------------
 *
 * Complete async FIFO with Gray-coded pointer synchronization,
 * distributed RAM with combinational read, full/empty flags,
 * and async-assert reset on all registers.
 */
static int build_cdc_fifo_module(IR_Module *mod, int module_id,
                                 int width, JZArena *arena)
{
    char name_buf[64];
    snprintf(name_buf, sizeof(name_buf), "JZHDL_LIB_CDC_FIFO__W%d", width);

    mod->id = module_id;
    mod->name = ir_strdup_arena(arena, name_buf);
    mod->base_module_id = -1;
    mod->source_file_id = -1;

    /* FIFO hardcoded parameters */
    int depth = 16;
    int addr_width = 4;
    int ptr_width = addr_width + 1;

    /* Signal IDs - ports */
    enum {
        SID_CLK_WR = 0, SID_CLK_RD, SID_RST_WR, SID_RST_RD,
        SID_DATA_IN, SID_WRITE_EN, SID_READ_EN,
        SID_DATA_OUT, SID_FULL, SID_EMPTY,
        /* Internal registers */
        SID_WR_PTR_BIN, SID_WR_PTR_GRAY,
        SID_RD_PTR_BIN, SID_RD_PTR_GRAY,
        SID_WR_PTR_GRAY_SYNC1, SID_WR_PTR_GRAY_SYNC2,
        SID_RD_PTR_GRAY_SYNC1, SID_RD_PTR_GRAY_SYNC2,
        NUM_SIGS
    };

    IR_Signal *sigs = alloc_signals(arena, NUM_SIGS);
    if (!sigs) return -1;

    init_port_signal(&sigs[SID_CLK_WR], SID_CLK_WR, "clk_wr", 1, PORT_IN, module_id, arena);
    init_port_signal(&sigs[SID_CLK_RD], SID_CLK_RD, "clk_rd", 1, PORT_IN, module_id, arena);
    init_port_signal(&sigs[SID_RST_WR], SID_RST_WR, "rst_wr", 1, PORT_IN, module_id, arena);
    init_port_signal(&sigs[SID_RST_RD], SID_RST_RD, "rst_rd", 1, PORT_IN, module_id, arena);
    init_port_signal(&sigs[SID_DATA_IN], SID_DATA_IN, "data_in", width, PORT_IN, module_id, arena);
    init_port_signal(&sigs[SID_WRITE_EN], SID_WRITE_EN, "write_en", 1, PORT_IN, module_id, arena);
    init_port_signal(&sigs[SID_READ_EN], SID_READ_EN, "read_en", 1, PORT_IN, module_id, arena);
    init_port_signal(&sigs[SID_DATA_OUT], SID_DATA_OUT, "data_out", width, PORT_OUT, module_id, arena);
    init_port_signal(&sigs[SID_FULL], SID_FULL, "full", 1, PORT_OUT, module_id, arena);
    init_port_signal(&sigs[SID_EMPTY], SID_EMPTY, "empty", 1, PORT_OUT, module_id, arena);

    /* Internal pointer registers */
    init_reg_signal(&sigs[SID_WR_PTR_BIN], SID_WR_PTR_BIN, "wr_ptr_bin", ptr_width, 0, module_id, arena);
    init_reg_signal(&sigs[SID_WR_PTR_GRAY], SID_WR_PTR_GRAY, "wr_ptr_gray", ptr_width, 0, module_id, arena);
    init_reg_signal(&sigs[SID_RD_PTR_BIN], SID_RD_PTR_BIN, "rd_ptr_bin", ptr_width, 1, module_id, arena);
    init_reg_signal(&sigs[SID_RD_PTR_GRAY], SID_RD_PTR_GRAY, "rd_ptr_gray", ptr_width, 1, module_id, arena);
    init_reg_signal(&sigs[SID_WR_PTR_GRAY_SYNC1], SID_WR_PTR_GRAY_SYNC1, "wr_ptr_gray_sync1", ptr_width, 1, module_id, arena);
    init_reg_signal(&sigs[SID_WR_PTR_GRAY_SYNC2], SID_WR_PTR_GRAY_SYNC2, "wr_ptr_gray_sync2", ptr_width, 1, module_id, arena);
    init_reg_signal(&sigs[SID_RD_PTR_GRAY_SYNC1], SID_RD_PTR_GRAY_SYNC1, "rd_ptr_gray_sync1", ptr_width, 0, module_id, arena);
    init_reg_signal(&sigs[SID_RD_PTR_GRAY_SYNC2], SID_RD_PTR_GRAY_SYNC2, "rd_ptr_gray_sync2", ptr_width, 0, module_id, arena);

    mod->signals = sigs;
    mod->num_signals = NUM_SIGS;

    /* Memory with read (async) and write ports */
    IR_Memory *ir_mem = (IR_Memory *)jz_arena_alloc(arena, sizeof(IR_Memory));
    if (!ir_mem) return -1;
    memset(ir_mem, 0, sizeof(*ir_mem));
    ir_mem->id = 0;
    ir_mem->name = ir_strdup_arena(arena, "mem");
    ir_mem->kind = MEM_KIND_DISTRIBUTED;
    ir_mem->word_width = width;
    ir_mem->depth = depth;
    ir_mem->address_width = addr_width;

    IR_MemoryPort *mports = (IR_MemoryPort *)jz_arena_alloc(arena, sizeof(IR_MemoryPort) * 2);
    if (!mports) return -1;
    memset(mports, 0, sizeof(IR_MemoryPort) * 2);
    mports[0].name = ir_strdup_arena(arena, "write");
    mports[0].kind = MEM_PORT_WRITE;
    mports[0].address_width = addr_width;
    mports[0].addr_signal_id = -1;
    mports[0].data_in_signal_id = -1;
    mports[0].data_out_signal_id = -1;
    mports[0].enable_signal_id = -1;
    mports[0].wdata_signal_id = -1;
    mports[0].output_signal_id = -1;
    mports[0].addr_reg_signal_id = -1;
    mports[0].sync_clock_domain_id = -1;
    mports[1].name = ir_strdup_arena(arena, "read");
    mports[1].kind = MEM_PORT_READ_ASYNC;
    mports[1].address_width = addr_width;
    mports[1].addr_signal_id = -1;
    mports[1].data_in_signal_id = -1;
    mports[1].data_out_signal_id = -1;
    mports[1].enable_signal_id = -1;
    mports[1].wdata_signal_id = -1;
    mports[1].output_signal_id = -1;
    mports[1].addr_reg_signal_id = -1;
    mports[1].sync_clock_domain_id = -1;
    ir_mem->ports = mports;
    ir_mem->num_ports = 2;
    mod->memories = ir_mem;
    mod->num_memories = 1;

    /* ---------------------------------------------------------------
     * Clock domains with explicit reset guards.
     * Library modules don't go through the user-module reset-wrapping
     * pass, so we build the if(rst){...}else{...} structure manually.
     * --------------------------------------------------------------- */
    IR_ClockDomain *cds = alloc_clock_domains(arena, 4);
    if (!cds) return -1;

    /* CD0: write pointer + memory write on clk_wr, async reset rst_wr */
    cds[0].id = 0;
    cds[0].clock_signal_id = SID_CLK_WR;
    cds[0].edge = EDGE_RISING;
    cds[0].reset_signal_id = SID_RST_WR;
    cds[0].reset_sync_signal_id = -1;
    cds[0].reset_active = RESET_ACTIVE_HIGH;
    cds[0].reset_type = RESET_IMMEDIATE;
    {
        int *rids = (int *)jz_arena_alloc(arena, sizeof(int) * 2);
        if (!rids) return -1;
        rids[0] = SID_WR_PTR_BIN;
        rids[1] = SID_WR_PTR_GRAY;
        cds[0].register_ids = rids;
        cds[0].num_registers = 2;

        /* Reset branch: wr_ptr_bin <= 0; wr_ptr_gray <= 0; */
        IR_Stmt *rst_stmts = alloc_stmts(arena, 2);
        if (!rst_stmts) return -1;
        rst_stmts[0] = make_assign_stmt(SID_WR_PTR_BIN, make_literal(arena, 0, ptr_width));
        rst_stmts[1] = make_assign_stmt(SID_WR_PTR_GRAY, make_literal(arena, 0, ptr_width));

        /* Body: mem write + pointer updates, guarded by write_en && ~full */
        IR_Stmt *body_stmts = alloc_stmts(arena, 3);
        if (!body_stmts) return -1;

        /* mem[wr_ptr_bin[3:0]] <= data_in */
        body_stmts[0].kind = STMT_MEM_WRITE;
        body_stmts[0].u.mem_write.memory_name = ir_strdup_arena(arena, "mem");
        body_stmts[0].u.mem_write.port_name = ir_strdup_arena(arena, "write");
        body_stmts[0].u.mem_write.address = make_slice(arena, SID_WR_PTR_BIN, addr_width - 1, 0);
        body_stmts[0].u.mem_write.data = make_sig_ref(arena, SID_DATA_IN, width);

        /* wr_ptr_bin <= wr_ptr_bin + 1 */
        body_stmts[1] = make_assign_stmt(SID_WR_PTR_BIN,
            make_binary(arena, EXPR_BINARY_ADD,
                        make_sig_ref(arena, SID_WR_PTR_BIN, ptr_width),
                        make_literal(arena, 1, ptr_width), ptr_width));

        /* wr_ptr_gray <= (wr_ptr_bin+1) >> 1 ^ (wr_ptr_bin+1) */
        IR_Expr *wr_next = make_binary(arena, EXPR_BINARY_ADD,
                                        make_sig_ref(arena, SID_WR_PTR_BIN, ptr_width),
                                        make_literal(arena, 1, ptr_width), ptr_width);
        IR_Expr *wr_shr = make_binary(arena, EXPR_BINARY_SHR, wr_next,
                                       make_literal(arena, 1, ptr_width), ptr_width);
        IR_Expr *wr_next2 = make_binary(arena, EXPR_BINARY_ADD,
                                         make_sig_ref(arena, SID_WR_PTR_BIN, ptr_width),
                                         make_literal(arena, 1, ptr_width), ptr_width);
        body_stmts[2] = make_assign_stmt(SID_WR_PTR_GRAY,
            make_binary(arena, EXPR_BINARY_XOR, wr_shr, wr_next2, ptr_width));

        /* Guard: if (write_en && ~full) */
        IR_Expr *full_n = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
        if (!full_n) return -1;
        memset(full_n, 0, sizeof(*full_n));
        full_n->kind = EXPR_UNARY_NOT;
        full_n->width = 1;
        full_n->u.unary.operand = make_sig_ref(arena, SID_FULL, 1);

        IR_Expr *wr_cond = make_binary(arena, EXPR_LOGICAL_AND,
                                        make_sig_ref(arena, SID_WRITE_EN, 1),
                                        full_n, 1);

        IR_Stmt *wr_if = alloc_stmts(arena, 1);
        if (!wr_if) return -1;
        wr_if->kind = STMT_IF;
        wr_if->u.if_stmt.condition = wr_cond;
        wr_if->u.if_stmt.then_block = make_block(arena, body_stmts, 3);
        wr_if->u.if_stmt.elif_chain = NULL;
        wr_if->u.if_stmt.else_block = NULL;

        /* Top-level: if (rst_wr) { reset } else { guarded body } */
        IR_Stmt *top_if = (IR_Stmt *)jz_arena_alloc(arena, sizeof(IR_Stmt));
        if (!top_if) return -1;
        memset(top_if, 0, sizeof(*top_if));
        top_if->kind = STMT_IF;
        top_if->u.if_stmt.condition = make_sig_ref(arena, SID_RST_WR, 1);
        top_if->u.if_stmt.then_block = make_block(arena, rst_stmts, 2);
        top_if->u.if_stmt.elif_chain = NULL;
        top_if->u.if_stmt.else_block = make_block(arena, wr_if, 1);

        cds[0].statements = top_if;
    }

    /* CD1: read pointer logic on clk_rd, async reset rst_rd */
    cds[1].id = 1;
    cds[1].clock_signal_id = SID_CLK_RD;
    cds[1].edge = EDGE_RISING;
    cds[1].reset_signal_id = SID_RST_RD;
    cds[1].reset_sync_signal_id = -1;
    cds[1].reset_active = RESET_ACTIVE_HIGH;
    cds[1].reset_type = RESET_IMMEDIATE;
    {
        /* data_out is combinational (async mem read), not registered */
        int *rids = (int *)jz_arena_alloc(arena, sizeof(int) * 2);
        if (!rids) return -1;
        rids[0] = SID_RD_PTR_BIN;
        rids[1] = SID_RD_PTR_GRAY;
        cds[1].register_ids = rids;
        cds[1].num_registers = 2;

        /* Reset branch: rd_ptr_bin <= 0; rd_ptr_gray <= 0; */
        IR_Stmt *rst_stmts = alloc_stmts(arena, 2);
        if (!rst_stmts) return -1;
        rst_stmts[0] = make_assign_stmt(SID_RD_PTR_BIN, make_literal(arena, 0, ptr_width));
        rst_stmts[1] = make_assign_stmt(SID_RD_PTR_GRAY, make_literal(arena, 0, ptr_width));

        /* Body: pointer updates guarded by read_en && ~empty */
        IR_Stmt *body_stmts = alloc_stmts(arena, 2);
        if (!body_stmts) return -1;

        body_stmts[0] = make_assign_stmt(SID_RD_PTR_BIN,
            make_binary(arena, EXPR_BINARY_ADD,
                        make_sig_ref(arena, SID_RD_PTR_BIN, ptr_width),
                        make_literal(arena, 1, ptr_width), ptr_width));

        IR_Expr *rd_next = make_binary(arena, EXPR_BINARY_ADD,
                                        make_sig_ref(arena, SID_RD_PTR_BIN, ptr_width),
                                        make_literal(arena, 1, ptr_width), ptr_width);
        IR_Expr *rd_shr = make_binary(arena, EXPR_BINARY_SHR, rd_next,
                                       make_literal(arena, 1, ptr_width), ptr_width);
        IR_Expr *rd_next2 = make_binary(arena, EXPR_BINARY_ADD,
                                         make_sig_ref(arena, SID_RD_PTR_BIN, ptr_width),
                                         make_literal(arena, 1, ptr_width), ptr_width);
        body_stmts[1] = make_assign_stmt(SID_RD_PTR_GRAY,
            make_binary(arena, EXPR_BINARY_XOR, rd_shr, rd_next2, ptr_width));

        /* Guard: if (read_en && ~empty) */
        IR_Expr *empty_n = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
        if (!empty_n) return -1;
        memset(empty_n, 0, sizeof(*empty_n));
        empty_n->kind = EXPR_UNARY_NOT;
        empty_n->width = 1;
        empty_n->u.unary.operand = make_sig_ref(arena, SID_EMPTY, 1);

        IR_Expr *rd_cond = make_binary(arena, EXPR_LOGICAL_AND,
                                        make_sig_ref(arena, SID_READ_EN, 1),
                                        empty_n, 1);

        IR_Stmt *rd_if = alloc_stmts(arena, 1);
        if (!rd_if) return -1;
        rd_if->kind = STMT_IF;
        rd_if->u.if_stmt.condition = rd_cond;
        rd_if->u.if_stmt.then_block = make_block(arena, body_stmts, 2);
        rd_if->u.if_stmt.elif_chain = NULL;
        rd_if->u.if_stmt.else_block = NULL;

        /* Top-level: if (rst_rd) { reset } else { guarded body } */
        IR_Stmt *top_if = (IR_Stmt *)jz_arena_alloc(arena, sizeof(IR_Stmt));
        if (!top_if) return -1;
        memset(top_if, 0, sizeof(*top_if));
        top_if->kind = STMT_IF;
        top_if->u.if_stmt.condition = make_sig_ref(arena, SID_RST_RD, 1);
        top_if->u.if_stmt.then_block = make_block(arena, rst_stmts, 2);
        top_if->u.if_stmt.elif_chain = NULL;
        top_if->u.if_stmt.else_block = make_block(arena, rd_if, 1);

        cds[1].statements = top_if;
    }

    /* CD2: rd pointer sync into write clock domain */
    cds[2].id = 2;
    cds[2].clock_signal_id = SID_CLK_WR;
    cds[2].edge = EDGE_RISING;
    cds[2].reset_signal_id = SID_RST_WR;
    cds[2].reset_sync_signal_id = -1;
    cds[2].reset_active = RESET_ACTIVE_HIGH;
    cds[2].reset_type = RESET_IMMEDIATE;
    {
        int *rids = (int *)jz_arena_alloc(arena, sizeof(int) * 2);
        if (!rids) return -1;
        rids[0] = SID_RD_PTR_GRAY_SYNC1;
        rids[1] = SID_RD_PTR_GRAY_SYNC2;
        cds[2].register_ids = rids;
        cds[2].num_registers = 2;

        IR_Stmt *rst_stmts = alloc_stmts(arena, 2);
        if (!rst_stmts) return -1;
        rst_stmts[0] = make_assign_stmt(SID_RD_PTR_GRAY_SYNC1, make_literal(arena, 0, ptr_width));
        rst_stmts[1] = make_assign_stmt(SID_RD_PTR_GRAY_SYNC2, make_literal(arena, 0, ptr_width));

        IR_Stmt *norm_stmts = alloc_stmts(arena, 2);
        if (!norm_stmts) return -1;
        norm_stmts[0] = make_assign_stmt(SID_RD_PTR_GRAY_SYNC1, make_sig_ref(arena, SID_RD_PTR_GRAY, ptr_width));
        norm_stmts[1] = make_assign_stmt(SID_RD_PTR_GRAY_SYNC2, make_sig_ref(arena, SID_RD_PTR_GRAY_SYNC1, ptr_width));

        IR_Stmt *top_if = (IR_Stmt *)jz_arena_alloc(arena, sizeof(IR_Stmt));
        if (!top_if) return -1;
        memset(top_if, 0, sizeof(*top_if));
        top_if->kind = STMT_IF;
        top_if->u.if_stmt.condition = make_sig_ref(arena, SID_RST_WR, 1);
        top_if->u.if_stmt.then_block = make_block(arena, rst_stmts, 2);
        top_if->u.if_stmt.elif_chain = NULL;
        top_if->u.if_stmt.else_block = make_block(arena, norm_stmts, 2);

        cds[2].statements = top_if;
    }

    /* CD3: wr pointer sync into read clock domain */
    cds[3].id = 3;
    cds[3].clock_signal_id = SID_CLK_RD;
    cds[3].edge = EDGE_RISING;
    cds[3].reset_signal_id = SID_RST_RD;
    cds[3].reset_sync_signal_id = -1;
    cds[3].reset_active = RESET_ACTIVE_HIGH;
    cds[3].reset_type = RESET_IMMEDIATE;
    {
        int *rids = (int *)jz_arena_alloc(arena, sizeof(int) * 2);
        if (!rids) return -1;
        rids[0] = SID_WR_PTR_GRAY_SYNC1;
        rids[1] = SID_WR_PTR_GRAY_SYNC2;
        cds[3].register_ids = rids;
        cds[3].num_registers = 2;

        IR_Stmt *rst_stmts = alloc_stmts(arena, 2);
        if (!rst_stmts) return -1;
        rst_stmts[0] = make_assign_stmt(SID_WR_PTR_GRAY_SYNC1, make_literal(arena, 0, ptr_width));
        rst_stmts[1] = make_assign_stmt(SID_WR_PTR_GRAY_SYNC2, make_literal(arena, 0, ptr_width));

        IR_Stmt *norm_stmts = alloc_stmts(arena, 2);
        if (!norm_stmts) return -1;
        norm_stmts[0] = make_assign_stmt(SID_WR_PTR_GRAY_SYNC1, make_sig_ref(arena, SID_WR_PTR_GRAY, ptr_width));
        norm_stmts[1] = make_assign_stmt(SID_WR_PTR_GRAY_SYNC2, make_sig_ref(arena, SID_WR_PTR_GRAY_SYNC1, ptr_width));

        IR_Stmt *top_if = (IR_Stmt *)jz_arena_alloc(arena, sizeof(IR_Stmt));
        if (!top_if) return -1;
        memset(top_if, 0, sizeof(*top_if));
        top_if->kind = STMT_IF;
        top_if->u.if_stmt.condition = make_sig_ref(arena, SID_RST_RD, 1);
        top_if->u.if_stmt.then_block = make_block(arena, rst_stmts, 2);
        top_if->u.if_stmt.elif_chain = NULL;
        top_if->u.if_stmt.else_block = make_block(arena, norm_stmts, 2);

        cds[3].statements = top_if;
    }

    for (int cdi = 0; cdi < 4; ++cdi) {
        if (build_sensitivity_list(&cds[cdi], arena) != 0) return -1;
    }

    mod->clock_domains = cds;
    mod->num_clock_domains = 4;

    /* Async block: combinational data_out, full, and empty */
    {
        IR_Stmt *async_stmts = alloc_stmts(arena, 3);
        if (!async_stmts) return -1;

        /* assign data_out = mem[rd_ptr_bin[3:0]] */
        IR_Expr *mem_rd = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
        if (!mem_rd) return -1;
        memset(mem_rd, 0, sizeof(*mem_rd));
        mem_rd->kind = EXPR_MEM_READ;
        mem_rd->width = width;
        mem_rd->u.mem_read.memory_name = ir_strdup_arena(arena, "mem");
        mem_rd->u.mem_read.port_name = ir_strdup_arena(arena, "read");
        mem_rd->u.mem_read.address = make_slice(arena, SID_RD_PTR_BIN, addr_width - 1, 0);
        async_stmts[0] = make_assign_stmt(SID_DATA_OUT, mem_rd);
        async_stmts[0].u.assign.kind = ASSIGN_ALIAS;

        /* assign full = (wr_ptr_gray == {~rd_ptr_gray_sync2[4:3], rd_ptr_gray_sync2[2:0]}) */
        IR_Expr *rd_sync_hi = make_slice(arena, SID_RD_PTR_GRAY_SYNC2, ptr_width - 1, ptr_width - 2);
        IR_Expr *rd_sync_hi_inv = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
        if (!rd_sync_hi_inv) return -1;
        memset(rd_sync_hi_inv, 0, sizeof(*rd_sync_hi_inv));
        rd_sync_hi_inv->kind = EXPR_UNARY_NOT;
        rd_sync_hi_inv->width = 2;
        rd_sync_hi_inv->u.unary.operand = rd_sync_hi;

        IR_Expr *rd_sync_lo = make_slice(arena, SID_RD_PTR_GRAY_SYNC2, ptr_width - 3, 0);

        IR_Expr *full_concat = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
        if (!full_concat) return -1;
        memset(full_concat, 0, sizeof(*full_concat));
        full_concat->kind = EXPR_CONCAT;
        full_concat->width = ptr_width;
        IR_Expr **concat_ops = (IR_Expr **)jz_arena_alloc(arena, sizeof(IR_Expr *) * 2);
        if (!concat_ops) return -1;
        concat_ops[0] = rd_sync_hi_inv;
        concat_ops[1] = rd_sync_lo;
        full_concat->u.concat.operands = concat_ops;
        full_concat->u.concat.num_operands = 2;

        IR_Expr *full_cmp = make_binary(arena, EXPR_BINARY_EQ,
                                         make_sig_ref(arena, SID_WR_PTR_GRAY, ptr_width),
                                         full_concat, 1);
        async_stmts[1] = make_assign_stmt(SID_FULL, full_cmp);
        async_stmts[1].u.assign.kind = ASSIGN_ALIAS;

        /* assign empty = (rd_ptr_gray == wr_ptr_gray_sync2) */
        IR_Expr *empty_cmp = make_binary(arena, EXPR_BINARY_EQ,
                                          make_sig_ref(arena, SID_RD_PTR_GRAY, ptr_width),
                                          make_sig_ref(arena, SID_WR_PTR_GRAY_SYNC2, ptr_width), 1);
        async_stmts[2] = make_assign_stmt(SID_EMPTY, empty_cmp);
        async_stmts[2].u.assign.kind = ASSIGN_ALIAS;

        mod->async_block = make_block(arena, async_stmts, 3);
    }

    return 0;
}

/* -------------------------------------------------------------------------
 * CDC HANDSHAKE library module builder
 * -------------------------------------------------------------------------
 *
 * Req/ack handshake protocol for multi-bit transfers.
 * Ports: clk_src (IN 1), clk_dest (IN 1), data_in (IN W), data_out (OUT W)
 * Regs:  src_data (W), req_src (1), req_sync1 (1), req_sync2 (1),
 *        ack_dest (1), ack_sync1 (1), ack_sync2 (1)
 * Net:   data_out_net (W)
 *
 * Source domain (clk_src):
 *   if (!req_src && !ack_sync2) { src_data <= data_in; req_src <= 1; }
 *   if (ack_sync2) { req_src <= 0; }
 *
 * Dest domain (clk_dest):
 *   req_sync1 <= req_src; req_sync2 <= req_sync1;
 *   if (req_sync2 && !ack_dest) { ack_dest <= 1; }
 *   if (!req_sync2) { ack_dest <= 0; }
 *
 * Source domain (ack sync):
 *   ack_sync1 <= ack_dest; ack_sync2 <= ack_sync1;
 *
 * Async: data_out = req_sync2 ? src_data : data_out;  (hold output)
 */
static int build_cdc_handshake_module(IR_Module *mod, int module_id,
                                       int width, JZArena *arena)
{
    char name_buf[64];
    snprintf(name_buf, sizeof(name_buf), "JZHDL_LIB_CDC_HANDSHAKE__W%d", width);
    mod->id = module_id;
    mod->name = ir_strdup_arena(arena, name_buf);
    mod->base_module_id = -1;
    mod->source_file_id = -1;

    enum {
        SID_CLK_SRC = 0, SID_CLK_DEST, SID_DATA_IN, SID_DATA_OUT,
        SID_SRC_DATA, SID_REQ_SRC, SID_REQ_SYNC1, SID_REQ_SYNC2,
        SID_ACK_DEST, SID_ACK_SYNC1, SID_ACK_SYNC2, NUM_SIGS
    };

    IR_Signal *sigs = alloc_signals(arena, NUM_SIGS);
    if (!sigs) return -1;

    init_port_signal(&sigs[SID_CLK_SRC],  SID_CLK_SRC,  "clk_src",  1, PORT_IN, module_id, arena);
    init_port_signal(&sigs[SID_CLK_DEST], SID_CLK_DEST, "clk_dest", 1, PORT_IN, module_id, arena);
    init_port_signal(&sigs[SID_DATA_IN],  SID_DATA_IN,  "data_in",  width, PORT_IN, module_id, arena);
    init_port_signal(&sigs[SID_DATA_OUT], SID_DATA_OUT, "data_out", width, PORT_OUT, module_id, arena);

    init_reg_signal(&sigs[SID_SRC_DATA],  SID_SRC_DATA,  "src_data",  width, 0, module_id, arena);
    init_reg_signal(&sigs[SID_REQ_SRC],   SID_REQ_SRC,   "req_src",   1, 0, module_id, arena);
    init_reg_signal(&sigs[SID_REQ_SYNC1], SID_REQ_SYNC1, "req_sync1", 1, 1, module_id, arena);
    init_reg_signal(&sigs[SID_REQ_SYNC2], SID_REQ_SYNC2, "req_sync2", 1, 1, module_id, arena);
    init_reg_signal(&sigs[SID_ACK_DEST],  SID_ACK_DEST,  "ack_dest",  1, 1, module_id, arena);
    init_reg_signal(&sigs[SID_ACK_SYNC1], SID_ACK_SYNC1, "ack_sync1", 1, 0, module_id, arena);
    init_reg_signal(&sigs[SID_ACK_SYNC2], SID_ACK_SYNC2, "ack_sync2", 1, 0, module_id, arena);

    mod->signals = sigs;
    mod->num_signals = NUM_SIGS;

    /* 3 clock domains:
     * CD0: clk_src — source data latch + ack sync
     * CD1: clk_dest — req sync + ack generation
     * (ack sync merged into CD0 for simplicity)
     */
    IR_ClockDomain *cds = alloc_clock_domains(arena, 2);
    if (!cds) return -1;

    /* CD0: source domain (clk_src) */
    cds[0].id = 0;
    cds[0].clock_signal_id = SID_CLK_SRC;
    cds[0].edge = EDGE_RISING;
    cds[0].reset_signal_id = -1;
    cds[0].reset_sync_signal_id = -1;
    {
        int *rids = (int *)jz_arena_alloc(arena, sizeof(int) * 4);
        if (!rids) return -1;
        rids[0] = SID_SRC_DATA;
        rids[1] = SID_REQ_SRC;
        rids[2] = SID_ACK_SYNC1;
        rids[3] = SID_ACK_SYNC2;
        cds[0].register_ids = rids;
        cds[0].num_registers = 4;
    }

    /* Source domain statements:
     * ack_sync1 <= ack_dest;
     * ack_sync2 <= ack_sync1;
     * if (!req_src && !ack_sync2) { src_data <= data_in; req_src <= 1; }
     * if (ack_sync2) { req_src <= 0; }
     */
    {
        IR_Stmt *stmts = alloc_stmts(arena, 4);
        if (!stmts) return -1;

        stmts[0] = make_assign_stmt(SID_ACK_SYNC1, make_sig_ref(arena, SID_ACK_DEST, 1));
        stmts[1] = make_assign_stmt(SID_ACK_SYNC2, make_sig_ref(arena, SID_ACK_SYNC1, 1));

        /* if (!req_src && !ack_sync2) */
        IR_Expr *not_req = make_binary(arena, EXPR_LOGICAL_NOT, make_sig_ref(arena, SID_REQ_SRC, 1), NULL, 1);
        IR_Expr *not_ack = make_binary(arena, EXPR_LOGICAL_NOT, make_sig_ref(arena, SID_ACK_SYNC2, 1), NULL, 1);
        IR_Expr *cond1 = make_binary(arena, EXPR_LOGICAL_AND, not_req, not_ack, 1);

        IR_Stmt *then1 = alloc_stmts(arena, 2);
        if (!then1) return -1;
        then1[0] = make_assign_stmt(SID_SRC_DATA, make_sig_ref(arena, SID_DATA_IN, width));
        then1[1] = make_assign_stmt(SID_REQ_SRC, make_literal(arena, 1, 1));

        stmts[2].kind = STMT_IF;
        stmts[2].u.if_stmt.condition = cond1;
        stmts[2].u.if_stmt.then_block = make_block(arena, then1, 2);
        stmts[2].u.if_stmt.else_block = NULL;

        /* if (ack_sync2) { req_src <= 0; } */
        IR_Stmt *then2 = alloc_stmts(arena, 1);
        if (!then2) return -1;
        then2[0] = make_assign_stmt(SID_REQ_SRC, make_literal(arena, 0, 1));

        stmts[3].kind = STMT_IF;
        stmts[3].u.if_stmt.condition = make_sig_ref(arena, SID_ACK_SYNC2, 1);
        stmts[3].u.if_stmt.then_block = make_block(arena, then2, 1);
        stmts[3].u.if_stmt.else_block = NULL;

        cds[0].statements = make_block(arena, stmts, 4);
    }
    if (build_sensitivity_list(&cds[0], arena) != 0) return -1;

    /* CD1: destination domain (clk_dest) */
    cds[1].id = 1;
    cds[1].clock_signal_id = SID_CLK_DEST;
    cds[1].edge = EDGE_RISING;
    cds[1].reset_signal_id = -1;
    cds[1].reset_sync_signal_id = -1;
    {
        int *rids = (int *)jz_arena_alloc(arena, sizeof(int) * 3);
        if (!rids) return -1;
        rids[0] = SID_REQ_SYNC1;
        rids[1] = SID_REQ_SYNC2;
        rids[2] = SID_ACK_DEST;
        cds[1].register_ids = rids;
        cds[1].num_registers = 3;
    }

    /* Dest domain statements:
     * req_sync1 <= req_src;
     * req_sync2 <= req_sync1;
     * if (req_sync2 && !ack_dest) { ack_dest <= 1; }
     * if (!req_sync2) { ack_dest <= 0; }
     */
    {
        IR_Stmt *stmts = alloc_stmts(arena, 4);
        if (!stmts) return -1;

        stmts[0] = make_assign_stmt(SID_REQ_SYNC1, make_sig_ref(arena, SID_REQ_SRC, 1));
        stmts[1] = make_assign_stmt(SID_REQ_SYNC2, make_sig_ref(arena, SID_REQ_SYNC1, 1));

        /* if (req_sync2 && !ack_dest) */
        IR_Expr *not_ack_d = make_binary(arena, EXPR_LOGICAL_NOT, make_sig_ref(arena, SID_ACK_DEST, 1), NULL, 1);
        IR_Expr *cond = make_binary(arena, EXPR_LOGICAL_AND, make_sig_ref(arena, SID_REQ_SYNC2, 1), not_ack_d, 1);

        IR_Stmt *then1 = alloc_stmts(arena, 1);
        if (!then1) return -1;
        then1[0] = make_assign_stmt(SID_ACK_DEST, make_literal(arena, 1, 1));

        stmts[2].kind = STMT_IF;
        stmts[2].u.if_stmt.condition = cond;
        stmts[2].u.if_stmt.then_block = make_block(arena, then1, 1);
        stmts[2].u.if_stmt.else_block = NULL;

        /* if (!req_sync2) { ack_dest <= 0; } */
        IR_Stmt *then2 = alloc_stmts(arena, 1);
        if (!then2) return -1;
        then2[0] = make_assign_stmt(SID_ACK_DEST, make_literal(arena, 0, 1));

        stmts[3].kind = STMT_IF;
        stmts[3].u.if_stmt.condition = make_binary(arena, EXPR_LOGICAL_NOT, make_sig_ref(arena, SID_REQ_SYNC2, 1), NULL, 1);
        stmts[3].u.if_stmt.then_block = make_block(arena, then2, 1);
        stmts[3].u.if_stmt.else_block = NULL;

        cds[1].statements = make_block(arena, stmts, 4);
    }
    if (build_sensitivity_list(&cds[1], arena) != 0) return -1;

    mod->clock_domains = cds;
    mod->num_clock_domains = 2;

    /* Async block: data_out = src_data (when req_sync2 is high, data is stable) */
    {
        IR_Stmt *async_stmts = alloc_stmts(arena, 1);
        if (!async_stmts) return -1;
        async_stmts[0] = make_assign_stmt(SID_DATA_OUT, make_sig_ref(arena, SID_SRC_DATA, width));
        async_stmts[0].u.assign.kind = ASSIGN_ALIAS;
        mod->async_block = make_block(arena, async_stmts, 1);
    }

    return 0;
}

/* -------------------------------------------------------------------------
 * CDC PULSE library module builder
 * -------------------------------------------------------------------------
 *
 * Toggle-based pulse synchronizer (width-1 only).
 * Ports: clk_src (IN 1), clk_dest (IN 1), pulse_in (IN 1), pulse_out (OUT 1)
 * Regs:  toggle_src (1), toggle_sync1 (1), toggle_sync2 (1), toggle_last (1)
 *
 * Source domain (clk_src):
 *   if (pulse_in) toggle_src <= ~toggle_src;
 *
 * Dest domain (clk_dest):
 *   toggle_sync1 <= toggle_src;
 *   toggle_sync2 <= toggle_sync1;
 *   toggle_last  <= toggle_sync2;
 *
 * Async: pulse_out = toggle_sync2 ^ toggle_last;
 */
static int build_cdc_pulse_module(IR_Module *mod, int module_id, JZArena *arena)
{
    mod->id = module_id;
    mod->name = ir_strdup_arena(arena, "JZHDL_LIB_CDC_PULSE");
    mod->base_module_id = -1;
    mod->source_file_id = -1;

    enum {
        SID_CLK_SRC = 0, SID_CLK_DEST, SID_PULSE_IN, SID_PULSE_OUT,
        SID_TOGGLE_SRC, SID_TOGGLE_SYNC1, SID_TOGGLE_SYNC2, SID_TOGGLE_LAST,
        NUM_SIGS
    };

    IR_Signal *sigs = alloc_signals(arena, NUM_SIGS);
    if (!sigs) return -1;

    init_port_signal(&sigs[SID_CLK_SRC],    SID_CLK_SRC,    "clk_src",   1, PORT_IN, module_id, arena);
    init_port_signal(&sigs[SID_CLK_DEST],   SID_CLK_DEST,   "clk_dest",  1, PORT_IN, module_id, arena);
    init_port_signal(&sigs[SID_PULSE_IN],   SID_PULSE_IN,   "pulse_in",  1, PORT_IN, module_id, arena);
    init_port_signal(&sigs[SID_PULSE_OUT],  SID_PULSE_OUT,  "pulse_out", 1, PORT_OUT, module_id, arena);

    init_reg_signal(&sigs[SID_TOGGLE_SRC],   SID_TOGGLE_SRC,   "toggle_src",   1, 0, module_id, arena);
    init_reg_signal(&sigs[SID_TOGGLE_SYNC1], SID_TOGGLE_SYNC1, "toggle_sync1", 1, 1, module_id, arena);
    init_reg_signal(&sigs[SID_TOGGLE_SYNC2], SID_TOGGLE_SYNC2, "toggle_sync2", 1, 1, module_id, arena);
    init_reg_signal(&sigs[SID_TOGGLE_LAST],  SID_TOGGLE_LAST,  "toggle_last",  1, 1, module_id, arena);

    mod->signals = sigs;
    mod->num_signals = NUM_SIGS;

    /* 2 clock domains */
    IR_ClockDomain *cds = alloc_clock_domains(arena, 2);
    if (!cds) return -1;

    /* CD0: source domain (clk_src) */
    cds[0].id = 0;
    cds[0].clock_signal_id = SID_CLK_SRC;
    cds[0].edge = EDGE_RISING;
    cds[0].reset_signal_id = -1;
    cds[0].reset_sync_signal_id = -1;
    {
        int *rids = (int *)jz_arena_alloc(arena, sizeof(int));
        if (!rids) return -1;
        rids[0] = SID_TOGGLE_SRC;
        cds[0].register_ids = rids;
        cds[0].num_registers = 1;
    }

    /* if (pulse_in) toggle_src <= ~toggle_src; */
    {
        IR_Stmt *stmts = alloc_stmts(arena, 1);
        if (!stmts) return -1;

        IR_Expr *not_toggle = make_binary(arena, EXPR_UNARY_NOT,
                                          make_sig_ref(arena, SID_TOGGLE_SRC, 1), NULL, 1);
        IR_Stmt *then_s = alloc_stmts(arena, 1);
        if (!then_s) return -1;
        then_s[0] = make_assign_stmt(SID_TOGGLE_SRC, not_toggle);

        stmts[0].kind = STMT_IF;
        stmts[0].u.if_stmt.condition = make_sig_ref(arena, SID_PULSE_IN, 1);
        stmts[0].u.if_stmt.then_block = make_block(arena, then_s, 1);
        stmts[0].u.if_stmt.else_block = NULL;

        cds[0].statements = make_block(arena, stmts, 1);
    }
    if (build_sensitivity_list(&cds[0], arena) != 0) return -1;

    /* CD1: destination domain (clk_dest) */
    cds[1].id = 1;
    cds[1].clock_signal_id = SID_CLK_DEST;
    cds[1].edge = EDGE_RISING;
    cds[1].reset_signal_id = -1;
    cds[1].reset_sync_signal_id = -1;
    {
        int *rids = (int *)jz_arena_alloc(arena, sizeof(int) * 3);
        if (!rids) return -1;
        rids[0] = SID_TOGGLE_SYNC1;
        rids[1] = SID_TOGGLE_SYNC2;
        rids[2] = SID_TOGGLE_LAST;
        cds[1].register_ids = rids;
        cds[1].num_registers = 3;
    }

    /* toggle_sync1 <= toggle_src; toggle_sync2 <= toggle_sync1; toggle_last <= toggle_sync2; */
    {
        IR_Stmt *stmts = alloc_stmts(arena, 3);
        if (!stmts) return -1;
        stmts[0] = make_assign_stmt(SID_TOGGLE_SYNC1, make_sig_ref(arena, SID_TOGGLE_SRC, 1));
        stmts[1] = make_assign_stmt(SID_TOGGLE_SYNC2, make_sig_ref(arena, SID_TOGGLE_SYNC1, 1));
        stmts[2] = make_assign_stmt(SID_TOGGLE_LAST,  make_sig_ref(arena, SID_TOGGLE_SYNC2, 1));
        cds[1].statements = make_block(arena, stmts, 3);
    }
    if (build_sensitivity_list(&cds[1], arena) != 0) return -1;

    mod->clock_domains = cds;
    mod->num_clock_domains = 2;

    /* Async: pulse_out = toggle_sync2 ^ toggle_last; */
    {
        IR_Stmt *async_stmts = alloc_stmts(arena, 1);
        if (!async_stmts) return -1;
        IR_Expr *xor_e = make_binary(arena, EXPR_BINARY_XOR,
                                     make_sig_ref(arena, SID_TOGGLE_SYNC2, 1),
                                     make_sig_ref(arena, SID_TOGGLE_LAST, 1), 1);
        async_stmts[0] = make_assign_stmt(SID_PULSE_OUT, xor_e);
        async_stmts[0].u.assign.kind = ASSIGN_ALIAS;
        mod->async_block = make_block(arena, async_stmts, 1);
    }

    return 0;
}

/* -------------------------------------------------------------------------
 * CDC MCP library module builder
 * -------------------------------------------------------------------------
 *
 * Multi-cycle path with synchronized enable.
 * Ports: clk_src (IN 1), clk_dest (IN 1), data_in (IN W), data_out (OUT W)
 * Regs:  src_data (W), en_src (1), en_sync1 (1), en_sync2 (1),
 *        ack_dest (1), ack_sync1 (1), ack_sync2 (1)
 *
 * Source domain:
 *   ack_sync1 <= ack_dest; ack_sync2 <= ack_sync1;
 *   if (!en_src && !ack_sync2) { src_data <= data_in; en_src <= 1; }
 *   if (ack_sync2) { en_src <= 0; }
 *
 * Dest domain:
 *   en_sync1 <= en_src; en_sync2 <= en_sync1;
 *   if (en_sync2 && !ack_dest) { ack_dest <= 1; }
 *   if (!en_sync2) { ack_dest <= 0; }
 *
 * Async: data_out = src_data;
 */
static int build_cdc_mcp_module(IR_Module *mod, int module_id,
                                 int width, JZArena *arena)
{
    char name_buf[64];
    snprintf(name_buf, sizeof(name_buf), "JZHDL_LIB_CDC_MCP__W%d", width);
    mod->id = module_id;
    mod->name = ir_strdup_arena(arena, name_buf);
    mod->base_module_id = -1;
    mod->source_file_id = -1;

    enum {
        SID_CLK_SRC = 0, SID_CLK_DEST, SID_DATA_IN, SID_DATA_OUT,
        SID_SRC_DATA, SID_EN_SRC, SID_EN_SYNC1, SID_EN_SYNC2,
        SID_ACK_DEST, SID_ACK_SYNC1, SID_ACK_SYNC2, NUM_SIGS
    };

    IR_Signal *sigs = alloc_signals(arena, NUM_SIGS);
    if (!sigs) return -1;

    init_port_signal(&sigs[SID_CLK_SRC],  SID_CLK_SRC,  "clk_src",  1, PORT_IN, module_id, arena);
    init_port_signal(&sigs[SID_CLK_DEST], SID_CLK_DEST, "clk_dest", 1, PORT_IN, module_id, arena);
    init_port_signal(&sigs[SID_DATA_IN],  SID_DATA_IN,  "data_in",  width, PORT_IN, module_id, arena);
    init_port_signal(&sigs[SID_DATA_OUT], SID_DATA_OUT, "data_out", width, PORT_OUT, module_id, arena);

    init_reg_signal(&sigs[SID_SRC_DATA],  SID_SRC_DATA,  "src_data",  width, 0, module_id, arena);
    init_reg_signal(&sigs[SID_EN_SRC],    SID_EN_SRC,    "en_src",    1, 0, module_id, arena);
    init_reg_signal(&sigs[SID_EN_SYNC1],  SID_EN_SYNC1,  "en_sync1",  1, 1, module_id, arena);
    init_reg_signal(&sigs[SID_EN_SYNC2],  SID_EN_SYNC2,  "en_sync2",  1, 1, module_id, arena);
    init_reg_signal(&sigs[SID_ACK_DEST],  SID_ACK_DEST,  "ack_dest",  1, 1, module_id, arena);
    init_reg_signal(&sigs[SID_ACK_SYNC1], SID_ACK_SYNC1, "ack_sync1", 1, 0, module_id, arena);
    init_reg_signal(&sigs[SID_ACK_SYNC2], SID_ACK_SYNC2, "ack_sync2", 1, 0, module_id, arena);

    mod->signals = sigs;
    mod->num_signals = NUM_SIGS;

    /* 2 clock domains */
    IR_ClockDomain *cds = alloc_clock_domains(arena, 2);
    if (!cds) return -1;

    /* CD0: source domain (clk_src) */
    cds[0].id = 0;
    cds[0].clock_signal_id = SID_CLK_SRC;
    cds[0].edge = EDGE_RISING;
    cds[0].reset_signal_id = -1;
    cds[0].reset_sync_signal_id = -1;
    {
        int *rids = (int *)jz_arena_alloc(arena, sizeof(int) * 4);
        if (!rids) return -1;
        rids[0] = SID_SRC_DATA;
        rids[1] = SID_EN_SRC;
        rids[2] = SID_ACK_SYNC1;
        rids[3] = SID_ACK_SYNC2;
        cds[0].register_ids = rids;
        cds[0].num_registers = 4;
    }

    {
        IR_Stmt *stmts = alloc_stmts(arena, 4);
        if (!stmts) return -1;

        stmts[0] = make_assign_stmt(SID_ACK_SYNC1, make_sig_ref(arena, SID_ACK_DEST, 1));
        stmts[1] = make_assign_stmt(SID_ACK_SYNC2, make_sig_ref(arena, SID_ACK_SYNC1, 1));

        /* if (!en_src && !ack_sync2) { src_data <= data_in; en_src <= 1; } */
        IR_Expr *not_en = make_binary(arena, EXPR_LOGICAL_NOT, make_sig_ref(arena, SID_EN_SRC, 1), NULL, 1);
        IR_Expr *not_ack = make_binary(arena, EXPR_LOGICAL_NOT, make_sig_ref(arena, SID_ACK_SYNC2, 1), NULL, 1);
        IR_Expr *cond1 = make_binary(arena, EXPR_LOGICAL_AND, not_en, not_ack, 1);

        IR_Stmt *then1 = alloc_stmts(arena, 2);
        if (!then1) return -1;
        then1[0] = make_assign_stmt(SID_SRC_DATA, make_sig_ref(arena, SID_DATA_IN, width));
        then1[1] = make_assign_stmt(SID_EN_SRC, make_literal(arena, 1, 1));

        stmts[2].kind = STMT_IF;
        stmts[2].u.if_stmt.condition = cond1;
        stmts[2].u.if_stmt.then_block = make_block(arena, then1, 2);
        stmts[2].u.if_stmt.else_block = NULL;

        /* if (ack_sync2) { en_src <= 0; } */
        IR_Stmt *then2 = alloc_stmts(arena, 1);
        if (!then2) return -1;
        then2[0] = make_assign_stmt(SID_EN_SRC, make_literal(arena, 0, 1));

        stmts[3].kind = STMT_IF;
        stmts[3].u.if_stmt.condition = make_sig_ref(arena, SID_ACK_SYNC2, 1);
        stmts[3].u.if_stmt.then_block = make_block(arena, then2, 1);
        stmts[3].u.if_stmt.else_block = NULL;

        cds[0].statements = make_block(arena, stmts, 4);
    }
    if (build_sensitivity_list(&cds[0], arena) != 0) return -1;

    /* CD1: destination domain (clk_dest) */
    cds[1].id = 1;
    cds[1].clock_signal_id = SID_CLK_DEST;
    cds[1].edge = EDGE_RISING;
    cds[1].reset_signal_id = -1;
    cds[1].reset_sync_signal_id = -1;
    {
        int *rids = (int *)jz_arena_alloc(arena, sizeof(int) * 3);
        if (!rids) return -1;
        rids[0] = SID_EN_SYNC1;
        rids[1] = SID_EN_SYNC2;
        rids[2] = SID_ACK_DEST;
        cds[1].register_ids = rids;
        cds[1].num_registers = 3;
    }

    {
        IR_Stmt *stmts = alloc_stmts(arena, 4);
        if (!stmts) return -1;

        stmts[0] = make_assign_stmt(SID_EN_SYNC1, make_sig_ref(arena, SID_EN_SRC, 1));
        stmts[1] = make_assign_stmt(SID_EN_SYNC2, make_sig_ref(arena, SID_EN_SYNC1, 1));

        IR_Expr *not_ack_d = make_binary(arena, EXPR_LOGICAL_NOT, make_sig_ref(arena, SID_ACK_DEST, 1), NULL, 1);
        IR_Expr *cond = make_binary(arena, EXPR_LOGICAL_AND, make_sig_ref(arena, SID_EN_SYNC2, 1), not_ack_d, 1);

        IR_Stmt *then1 = alloc_stmts(arena, 1);
        if (!then1) return -1;
        then1[0] = make_assign_stmt(SID_ACK_DEST, make_literal(arena, 1, 1));

        stmts[2].kind = STMT_IF;
        stmts[2].u.if_stmt.condition = cond;
        stmts[2].u.if_stmt.then_block = make_block(arena, then1, 1);
        stmts[2].u.if_stmt.else_block = NULL;

        IR_Stmt *then2 = alloc_stmts(arena, 1);
        if (!then2) return -1;
        then2[0] = make_assign_stmt(SID_ACK_DEST, make_literal(arena, 0, 1));

        stmts[3].kind = STMT_IF;
        stmts[3].u.if_stmt.condition = make_binary(arena, EXPR_LOGICAL_NOT, make_sig_ref(arena, SID_EN_SYNC2, 1), NULL, 1);
        stmts[3].u.if_stmt.then_block = make_block(arena, then2, 1);
        stmts[3].u.if_stmt.else_block = NULL;

        cds[1].statements = make_block(arena, stmts, 4);
    }
    if (build_sensitivity_list(&cds[1], arena) != 0) return -1;

    mod->clock_domains = cds;
    mod->num_clock_domains = 2;

    /* Async: data_out = src_data; */
    {
        IR_Stmt *async_stmts = alloc_stmts(arena, 1);
        if (!async_stmts) return -1;
        async_stmts[0] = make_assign_stmt(SID_DATA_OUT, make_sig_ref(arena, SID_SRC_DATA, width));
        async_stmts[0].u.assign.kind = ASSIGN_ALIAS;
        mod->async_block = make_block(arena, async_stmts, 1);
    }

    return 0;
}

/* -------------------------------------------------------------------------
 * Reset synchronizer library module builder
 * -------------------------------------------------------------------------
 *
 * Async assert, sync deassert reset synchronizer.
 * Ports: clk (IN 1), rst_async_n (IN 1), rst_sync_n (OUT 1)
 * Regs:  sync_ff1 (1), sync_ff2 (1)
 *
 * always @(posedge clk or negedge rst_async_n)
 *   if (!rst_async_n) { sync_ff1 <= 0; sync_ff2 <= 0; }
 *   else { sync_ff1 <= 1; sync_ff2 <= sync_ff1; }
 * assign rst_sync_n = sync_ff2;
 */
static int build_reset_sync_module(IR_Module *mod, int module_id, JZArena *arena)
{
    mod->id = module_id;
    mod->name = ir_strdup_arena(arena, "JZHDL_LIB_RESET_SYNC");
    mod->base_module_id = -1;
    mod->source_file_id = -1;

    enum {
        SID_CLK = 0, SID_RST_ASYNC_N, SID_RST_SYNC_N,
        SID_SYNC_FF1, SID_SYNC_FF2, NUM_SIGS
    };

    IR_Signal *sigs = alloc_signals(arena, NUM_SIGS);
    if (!sigs) return -1;

    init_port_signal(&sigs[SID_CLK],         SID_CLK,         "clk",         1, PORT_IN, module_id, arena);
    init_port_signal(&sigs[SID_RST_ASYNC_N], SID_RST_ASYNC_N, "rst_async_n", 1, PORT_IN, module_id, arena);
    init_port_signal(&sigs[SID_RST_SYNC_N],  SID_RST_SYNC_N,  "rst_sync_n",  1, PORT_OUT, module_id, arena);

    init_reg_signal(&sigs[SID_SYNC_FF1], SID_SYNC_FF1, "sync_ff1", 1, 0, module_id, arena);
    init_reg_signal(&sigs[SID_SYNC_FF2], SID_SYNC_FF2, "sync_ff2", 1, 0, module_id, arena);

    mod->signals = sigs;
    mod->num_signals = NUM_SIGS;

    /* 1 clock domain: posedge clk, async reset on rst_async_n (active-low) */
    IR_ClockDomain *cd = alloc_clock_domains(arena, 1);
    if (!cd) return -1;

    cd[0].id = 0;
    cd[0].clock_signal_id = SID_CLK;
    cd[0].edge = EDGE_RISING;
    cd[0].reset_signal_id = SID_RST_ASYNC_N;
    cd[0].reset_sync_signal_id = -1;
    cd[0].reset_active = RESET_ACTIVE_LOW;
    cd[0].reset_type = RESET_IMMEDIATE;

    {
        int *rids = (int *)jz_arena_alloc(arena, sizeof(int) * 2);
        if (!rids) return -1;
        rids[0] = SID_SYNC_FF1;
        rids[1] = SID_SYNC_FF2;
        cd[0].register_ids = rids;
        cd[0].num_registers = 2;
    }

    /* Build: if (!rst_async_n) { ff1<=0; ff2<=0; } else { ff1<=1; ff2<=ff1; }
     * This explicitly creates the reset if-else rather than relying on
     * the user-module wrapping pass (which doesn't run for library modules).
     */
    {
        /* Reset branch */
        IR_Stmt *rst_stmts = alloc_stmts(arena, 2);
        if (!rst_stmts) return -1;
        rst_stmts[0] = make_assign_stmt(SID_SYNC_FF1, make_literal(arena, 0, 1));
        rst_stmts[1] = make_assign_stmt(SID_SYNC_FF2, make_literal(arena, 0, 1));

        /* Normal branch */
        IR_Stmt *norm_stmts = alloc_stmts(arena, 2);
        if (!norm_stmts) return -1;
        norm_stmts[0] = make_assign_stmt(SID_SYNC_FF1, make_literal(arena, 1, 1));
        norm_stmts[1] = make_assign_stmt(SID_SYNC_FF2, make_sig_ref(arena, SID_SYNC_FF1, 1));

        /* Condition: !rst_async_n */
        IR_Expr *rst_ref = make_sig_ref(arena, SID_RST_ASYNC_N, 1);
        IR_Expr *not_rst = make_binary(arena, EXPR_LOGICAL_NOT, rst_ref, NULL, 1);

        /* Build if-else statement */
        IR_Stmt *if_stmt = (IR_Stmt *)jz_arena_alloc(arena, sizeof(IR_Stmt));
        if (!if_stmt) return -1;
        memset(if_stmt, 0, sizeof(*if_stmt));
        if_stmt->kind = STMT_IF;
        if_stmt->u.if_stmt.condition = not_rst;
        if_stmt->u.if_stmt.then_block = make_block(arena, rst_stmts, 2);
        if_stmt->u.if_stmt.elif_chain = NULL;
        if_stmt->u.if_stmt.else_block = make_block(arena, norm_stmts, 2);

        cd[0].statements = if_stmt;
    }
    if (build_sensitivity_list(&cd[0], arena) != 0) return -1;

    mod->clock_domains = cd;
    mod->num_clock_domains = 1;

    /* Async: rst_sync_n = sync_ff2; */
    {
        IR_Stmt *async_stmts = alloc_stmts(arena, 1);
        if (!async_stmts) return -1;
        async_stmts[0] = make_assign_stmt(SID_RST_SYNC_N, make_sig_ref(arena, SID_SYNC_FF2, 1));
        async_stmts[0].u.assign.kind = ASSIGN_ALIAS;
        mod->async_block = make_block(arena, async_stmts, 1);
    }

    return 0;
}

/* -------------------------------------------------------------------------
 * CDC entry -> Instance lowering
 * -------------------------------------------------------------------------
 */

/**
 * @brief Find a signal in a module by name.
 */
static const IR_Signal *lib_find_signal_by_name(const IR_Module *mod,
                                                 const char *name)
{
    if (!mod || !name) return NULL;
    for (int i = 0; i < mod->num_signals; ++i) {
        if (mod->signals[i].name && strcmp(mod->signals[i].name, name) == 0) {
            return &mod->signals[i];
        }
    }
    return NULL;
}

/**
 * @brief Find a signal in a module by ID.
 */
static const IR_Signal *lib_find_signal_by_id(const IR_Module *mod, int id)
{
    if (!mod || id < 0) return NULL;
    for (int i = 0; i < mod->num_signals; ++i) {
        if (mod->signals[i].id == id) {
            return &mod->signals[i];
        }
    }
    return NULL;
}

/**
 * @brief Create an IR_InstanceConnection binding a parent signal to a child port.
 */
static IR_InstanceConnection make_conn(int parent_sig_id, int child_port_id,
                                       int parent_msb, int parent_lsb)
{
    IR_InstanceConnection c;
    memset(&c, 0, sizeof(c));
    c.parent_signal_id = parent_sig_id;
    c.child_port_id = child_port_id;
    c.parent_msb = parent_msb;
    c.parent_lsb = parent_lsb;
    return c;
}

/**
 * @brief Lower a CDC_RAW entry into a direct wire assignment (no instance).
 *
 * Appends `assign dest_alias = source_reg;` (or a slice thereof) to the
 * module's async block.  Returns 0 on success, -1 on failure.
 */
static int lower_cdc_raw(const IR_CDC *cdc, IR_Module *mod, JZArena *arena)
{
    /* Find alias signal in parent */
    const IR_Signal *alias_sig = lib_find_signal_by_name(mod, cdc->dest_alias_name);
    if (!alias_sig) return -1;

    /* Build RHS expression: source_reg or source_reg[msb:lsb] */
    int eff_width = 1;
    const IR_Signal *src_reg = lib_find_signal_by_id(mod, cdc->source_reg_id);
    if (src_reg) {
        if (cdc->source_msb >= 0 && cdc->source_lsb >= 0) {
            eff_width = cdc->source_msb - cdc->source_lsb + 1;
        } else {
            eff_width = src_reg->width;
        }
    }
    if (eff_width <= 0) eff_width = 1;

    IR_Expr *rhs;
    if (cdc->source_msb >= 0 && cdc->source_lsb >= 0) {
        rhs = make_slice(arena, cdc->source_reg_id,
                         cdc->source_msb, cdc->source_lsb);
    } else {
        rhs = make_sig_ref(arena, cdc->source_reg_id, eff_width);
    }
    if (!rhs) return -1;

    /* Build the assignment statement: dest_alias = source_reg */
    IR_Stmt new_stmt;
    memset(&new_stmt, 0, sizeof(new_stmt));
    new_stmt.kind = STMT_ASSIGNMENT;
    new_stmt.u.assign.lhs_signal_id = alias_sig->id;
    new_stmt.u.assign.rhs = rhs;
    new_stmt.u.assign.kind = ASSIGN_ALIAS;

    /* Append to the module's async block */
    if (mod->async_block && mod->async_block->kind == STMT_BLOCK) {
        int old_count = mod->async_block->u.block.count;
        IR_Stmt *new_stmts = (IR_Stmt *)jz_arena_alloc(
            arena, sizeof(IR_Stmt) * (size_t)(old_count + 1));
        if (!new_stmts) return -1;
        if (old_count > 0) {
            memcpy(new_stmts, mod->async_block->u.block.stmts,
                   sizeof(IR_Stmt) * (size_t)old_count);
        }
        new_stmts[old_count] = new_stmt;
        mod->async_block->u.block.stmts = new_stmts;
        mod->async_block->u.block.count = old_count + 1;
    } else {
        /* No async block yet — create one */
        IR_Stmt *stmts = alloc_stmts(arena, 1);
        if (!stmts) return -1;
        stmts[0] = new_stmt;
        mod->async_block = make_block(arena, stmts, 1);
        if (!mod->async_block) return -1;
    }

    return 0;
}

/**
 * @brief Lower a CDC_BIT entry into an IR_Instance.
 */
static int lower_cdc_bit(const IR_CDC *cdc, const IR_Module *parent,
                         const IR_Module *lib_mod, IR_Instance *inst,
                         int inst_id, JZArena *arena)
{
    char name_buf[128];
    snprintf(name_buf, sizeof(name_buf), "u_cdc_bit_%s",
             cdc->dest_alias_name ? cdc->dest_alias_name : "anon");

    inst->id = inst_id;
    inst->name = ir_strdup_arena(arena, name_buf);
    inst->child_module_id = lib_mod->id;

    /* 3 connections: clk_dest, data_in, data_out */
    IR_InstanceConnection *conns = (IR_InstanceConnection *)jz_arena_alloc(
        arena, sizeof(IR_InstanceConnection) * 3);
    if (!conns) return -1;
    memset(conns, 0, sizeof(IR_InstanceConnection) * 3);

    /* Find destination clock signal */
    const IR_ClockDomain *dst_cd = NULL;
    if (cdc->dest_clock_id >= 0 && cdc->dest_clock_id < parent->num_clock_domains) {
        dst_cd = &parent->clock_domains[cdc->dest_clock_id];
    }
    int dst_clk_sig = dst_cd ? dst_cd->clock_signal_id : -1;

    /* Find child port IDs */
    const IR_Signal *child_clk_dest = lib_find_signal_by_name(lib_mod, "clk_dest");
    const IR_Signal *child_data_in = lib_find_signal_by_name(lib_mod, "data_in");
    const IR_Signal *child_data_out = lib_find_signal_by_name(lib_mod, "data_out");
    if (!child_clk_dest || !child_data_in || !child_data_out) return -1;

    /* clk_dest connection */
    conns[0] = make_conn(dst_clk_sig, child_clk_dest->id, -1, -1);

    /* data_in connection (with optional slice) */
    conns[1] = make_conn(cdc->source_reg_id, child_data_in->id,
                         cdc->source_msb, cdc->source_lsb);

    /* data_out connection: find alias signal in parent */
    const IR_Signal *alias_sig = lib_find_signal_by_name(parent, cdc->dest_alias_name);
    int alias_id = alias_sig ? alias_sig->id : -1;
    conns[2] = make_conn(alias_id, child_data_out->id, -1, -1);

    inst->connections = conns;
    inst->num_connections = 3;
    return 0;
}

/**
 * @brief Lower a CDC_BUS entry into an IR_Instance.
 */
static int lower_cdc_bus(const IR_CDC *cdc, const IR_Module *parent,
                         const IR_Module *lib_mod, IR_Instance *inst,
                         int inst_id, JZArena *arena)
{
    char name_buf[128];
    snprintf(name_buf, sizeof(name_buf), "u_cdc_bus_%s",
             cdc->dest_alias_name ? cdc->dest_alias_name : "anon");

    inst->id = inst_id;
    inst->name = ir_strdup_arena(arena, name_buf);
    inst->child_module_id = lib_mod->id;

    /* 4 connections: clk_src, clk_dest, data_in, data_out */
    IR_InstanceConnection *conns = (IR_InstanceConnection *)jz_arena_alloc(
        arena, sizeof(IR_InstanceConnection) * 4);
    if (!conns) return -1;
    memset(conns, 0, sizeof(IR_InstanceConnection) * 4);

    /* Find clock signals */
    const IR_ClockDomain *src_cd = NULL;
    const IR_ClockDomain *dst_cd = NULL;
    if (cdc->source_clock_id >= 0 && cdc->source_clock_id < parent->num_clock_domains) {
        src_cd = &parent->clock_domains[cdc->source_clock_id];
    }
    if (cdc->dest_clock_id >= 0 && cdc->dest_clock_id < parent->num_clock_domains) {
        dst_cd = &parent->clock_domains[cdc->dest_clock_id];
    }

    int src_clk_sig = src_cd ? src_cd->clock_signal_id : -1;
    int dst_clk_sig = dst_cd ? dst_cd->clock_signal_id : -1;

    const IR_Signal *child_clk_src = lib_find_signal_by_name(lib_mod, "clk_src");
    const IR_Signal *child_clk_dest = lib_find_signal_by_name(lib_mod, "clk_dest");
    const IR_Signal *child_data_in = lib_find_signal_by_name(lib_mod, "data_in");
    const IR_Signal *child_data_out = lib_find_signal_by_name(lib_mod, "data_out");
    if (!child_clk_src || !child_clk_dest || !child_data_in || !child_data_out) return -1;

    conns[0] = make_conn(src_clk_sig, child_clk_src->id, -1, -1);
    conns[1] = make_conn(dst_clk_sig, child_clk_dest->id, -1, -1);
    conns[2] = make_conn(cdc->source_reg_id, child_data_in->id,
                         cdc->source_msb, cdc->source_lsb);

    const IR_Signal *alias_sig = lib_find_signal_by_name(parent, cdc->dest_alias_name);
    int alias_id = alias_sig ? alias_sig->id : -1;
    conns[3] = make_conn(alias_id, child_data_out->id, -1, -1);

    inst->connections = conns;
    inst->num_connections = 4;
    return 0;
}

/* -------------------------------------------------------------------------
 * FIFO CDC helpers: derive write_en / read_en from statement trees
 * -------------------------------------------------------------------------
 */

/** Check whether an expression tree references a given signal ID. */
static bool fifo_expr_refs(const IR_Expr *expr, int sig_id)
{
    if (!expr) return false;
    switch (expr->kind) {
    case EXPR_SIGNAL_REF:
        return expr->u.signal_ref.signal_id == sig_id;
    case EXPR_SLICE:
        return expr->u.slice.signal_id == sig_id;
    case EXPR_UNARY_NOT:
    case EXPR_UNARY_NEG:
    case EXPR_LOGICAL_NOT:
        return fifo_expr_refs(expr->u.unary.operand, sig_id);
    case EXPR_CONCAT:
        for (int i = 0; i < expr->u.concat.num_operands; ++i)
            if (fifo_expr_refs(expr->u.concat.operands[i], sig_id)) return true;
        return false;
    case EXPR_TERNARY:
        return fifo_expr_refs(expr->u.ternary.condition, sig_id) ||
               fifo_expr_refs(expr->u.ternary.true_val, sig_id) ||
               fifo_expr_refs(expr->u.ternary.false_val, sig_id);
    case EXPR_MEM_READ:
        return fifo_expr_refs(expr->u.mem_read.address, sig_id);
    case EXPR_LITERAL:
        return false;
    default:
        /* Binary ops and others with left/right */
        if (expr->u.binary.left && fifo_expr_refs(expr->u.binary.left, sig_id)) return true;
        if (expr->u.binary.right && fifo_expr_refs(expr->u.binary.right, sig_id)) return true;
        return false;
    }
}

/**
 * Check whether a statement subtree writes to (is_write=true) or
 * reads from (is_write=false) the given signal ID.
 */
static bool fifo_stmt_touches(const IR_Stmt *stmt, int sig_id, bool is_write)
{
    if (!stmt) return false;
    switch (stmt->kind) {
    case STMT_ASSIGNMENT:
        if (is_write)
            return stmt->u.assign.lhs_signal_id == sig_id;
        else
            return fifo_expr_refs(stmt->u.assign.rhs, sig_id);
    case STMT_IF: {
        const IR_IfStmt *ifs = &stmt->u.if_stmt;
        if (!is_write && fifo_expr_refs(ifs->condition, sig_id)) return true;
        if (fifo_stmt_touches(ifs->then_block, sig_id, is_write)) return true;
        const IR_Stmt *elif = ifs->elif_chain;
        while (elif && elif->kind == STMT_IF) {
            if (!is_write && fifo_expr_refs(elif->u.if_stmt.condition, sig_id)) return true;
            if (fifo_stmt_touches(elif->u.if_stmt.then_block, sig_id, is_write)) return true;
            elif = elif->u.if_stmt.elif_chain;
        }
        if (fifo_stmt_touches(ifs->else_block, sig_id, is_write)) return true;
        return false;
    }
    case STMT_SELECT: {
        const IR_SelectStmt *sel = &stmt->u.select_stmt;
        if (!is_write && fifo_expr_refs(sel->selector, sig_id)) return true;
        for (int i = 0; i < sel->num_cases; ++i)
            if (fifo_stmt_touches(sel->cases[i].body, sig_id, is_write)) return true;
        return false;
    }
    case STMT_BLOCK: {
        const IR_BlockStmt *blk = &stmt->u.block;
        for (int i = 0; i < blk->count; ++i)
            if (fifo_stmt_touches(&blk->stmts[i], sig_id, is_write)) return true;
        return false;
    }
    case STMT_MEM_WRITE:
        if (!is_write) {
            return fifo_expr_refs(stmt->u.mem_write.address, sig_id) ||
                   fifo_expr_refs(stmt->u.mem_write.data, sig_id);
        }
        return false;
    default:
        return false;
    }
}

/**
 * Derive an enable expression from a statement tree.
 * Returns an IR_Expr that is 1 when the target signal is written/read,
 * or NULL if the target is not found in the tree.
 *
 * For unconditional access, returns literal 1'b1.
 * For conditional access (inside IF/SELECT), returns the gating condition.
 */
static IR_Expr *fifo_derive_enable(const IR_Stmt *stmt, int sig_id,
                                    bool is_write, JZArena *arena)
{
    if (!stmt) return NULL;

    switch (stmt->kind) {
    case STMT_ASSIGNMENT:
        if (is_write && stmt->u.assign.lhs_signal_id == sig_id)
            return make_literal(arena, 1, 1);
        if (!is_write && fifo_expr_refs(stmt->u.assign.rhs, sig_id))
            return make_literal(arena, 1, 1);
        return NULL;

    case STMT_IF: {
        const IR_IfStmt *ifs = &stmt->u.if_stmt;
        IR_Expr *result = NULL;

        /* then branch */
        if (fifo_stmt_touches(ifs->then_block, sig_id, is_write)) {
            IR_Expr *inner = fifo_derive_enable(ifs->then_block, sig_id, is_write, arena);
            IR_Expr *contrib = ifs->condition;
            if (inner && inner->kind != EXPR_LITERAL)
                contrib = make_binary(arena, EXPR_LOGICAL_AND, ifs->condition, inner, 1);
            result = contrib;
        }

        /* elif chain */
        const IR_Stmt *elif = ifs->elif_chain;
        while (elif && elif->kind == STMT_IF) {
            const IR_IfStmt *eifs = &elif->u.if_stmt;
            if (fifo_stmt_touches(eifs->then_block, sig_id, is_write)) {
                IR_Expr *inner = fifo_derive_enable(eifs->then_block, sig_id, is_write, arena);
                IR_Expr *contrib = eifs->condition;
                if (inner && inner->kind != EXPR_LITERAL)
                    contrib = make_binary(arena, EXPR_LOGICAL_AND, eifs->condition, inner, 1);
                result = result ? make_binary(arena, EXPR_LOGICAL_OR, result, contrib, 1)
                                : contrib;
            }
            elif = eifs->elif_chain;
        }

        /* else branch — recurse to get inner condition (not just 1'b1) */
        if (fifo_stmt_touches(ifs->else_block, sig_id, is_write)) {
            IR_Expr *inner = fifo_derive_enable(ifs->else_block, sig_id, is_write, arena);
            if (inner) {
                /* else is reached when all prior conditions are false;
                 * for now, contribute the inner condition directly since
                 * the else context is implicit */
                result = result ? make_binary(arena, EXPR_LOGICAL_OR, result, inner, 1)
                                : inner;
            }
        }

        return result;
    }

    case STMT_SELECT: {
        const IR_SelectStmt *sel = &stmt->u.select_stmt;
        IR_Expr *result = NULL;

        for (int i = 0; i < sel->num_cases; ++i) {
            if (!fifo_stmt_touches(sel->cases[i].body, sig_id, is_write))
                continue;
            if (!sel->cases[i].case_value) {
                /* DEFAULT case — conservative: always enabled */
                return make_literal(arena, 1, 1);
            }
            IR_Expr *match = make_binary(arena, EXPR_BINARY_EQ,
                                          sel->selector, sel->cases[i].case_value, 1);
            result = result ? make_binary(arena, EXPR_LOGICAL_OR, result, match, 1) : match;
        }
        return result;
    }

    case STMT_BLOCK: {
        const IR_BlockStmt *blk = &stmt->u.block;
        IR_Expr *result = NULL;
        for (int i = 0; i < blk->count; ++i) {
            IR_Expr *en = fifo_derive_enable(&blk->stmts[i], sig_id, is_write, arena);
            if (en) {
                result = result ? make_binary(arena, EXPR_LOGICAL_OR, result, en, 1)
                                : en;
            }
        }
        return result;
    }

    default:
        return NULL;
    }
}

/**
 * Add a wire signal to a module, growing the signals array.
 * Returns the new signal's ID, or -1 on failure.
 */
static int fifo_add_wire(IR_Module *mod, JZArena *arena, const char *name, int width)
{
    int new_id = mod->num_signals;
    IR_Signal *new_arr = (IR_Signal *)jz_arena_alloc(
        arena, (size_t)(mod->num_signals + 1) * sizeof(IR_Signal));
    if (!new_arr) return -1;
    if (mod->num_signals > 0)
        memcpy(new_arr, mod->signals, (size_t)mod->num_signals * sizeof(IR_Signal));
    mod->signals = new_arr;

    IR_Signal *sig = &mod->signals[mod->num_signals++];
    memset(sig, 0, sizeof(*sig));
    sig->id = new_id;
    sig->name = ir_strdup_arena(arena, name);
    sig->kind = SIG_NET;
    sig->width = width;
    sig->owner_module_id = mod->id;
    return new_id;
}

/** Append an assignment statement to the module's async block. */
static int fifo_add_async_assign(IR_Module *mod, JZArena *arena,
                                  int lhs_id, IR_Expr *rhs, IR_AssignmentKind kind)
{
    IR_Stmt new_stmt;
    memset(&new_stmt, 0, sizeof(new_stmt));
    new_stmt.kind = STMT_ASSIGNMENT;
    new_stmt.u.assign.lhs_signal_id = lhs_id;
    new_stmt.u.assign.rhs = rhs;
    new_stmt.u.assign.kind = kind;

    if (mod->async_block && mod->async_block->kind == STMT_BLOCK) {
        int old_count = mod->async_block->u.block.count;
        IR_Stmt *new_stmts = (IR_Stmt *)jz_arena_alloc(
            arena, sizeof(IR_Stmt) * (size_t)(old_count + 1));
        if (!new_stmts) return -1;
        if (old_count > 0)
            memcpy(new_stmts, mod->async_block->u.block.stmts,
                   sizeof(IR_Stmt) * (size_t)old_count);
        new_stmts[old_count] = new_stmt;
        mod->async_block->u.block.stmts = new_stmts;
        mod->async_block->u.block.count = old_count + 1;
    } else {
        IR_Stmt *stmts = alloc_stmts(arena, 1);
        if (!stmts) return -1;
        stmts[0] = new_stmt;
        mod->async_block = make_block(arena, stmts, 1);
        if (!mod->async_block) return -1;
    }
    return 0;
}

/**
 * @brief Lower a CDC_FIFO entry into an IR_Instance.
 *
 * Handles:
 * - Reset polarity: inverts active-low parent resets for FIFO's active-high ports
 * - write_en: derived from conditions gating assignments to the source register
 * - read_en: derived from conditions gating reads of the dest alias
 */
static int lower_cdc_fifo(const IR_CDC *cdc, IR_Module *parent,
                          const IR_Module *lib_mod, IR_Instance *inst,
                          int inst_id, JZArena *arena)
{
    char name_buf[128];
    snprintf(name_buf, sizeof(name_buf), "u_cdc_fifo_%s",
             cdc->dest_alias_name ? cdc->dest_alias_name : "anon");

    inst->id = inst_id;
    inst->name = ir_strdup_arena(arena, name_buf);
    inst->child_module_id = lib_mod->id;

    const IR_ClockDomain *src_cd = NULL;
    const IR_ClockDomain *dst_cd = NULL;
    if (cdc->source_clock_id >= 0 && cdc->source_clock_id < parent->num_clock_domains) {
        src_cd = &parent->clock_domains[cdc->source_clock_id];
    }
    if (cdc->dest_clock_id >= 0 && cdc->dest_clock_id < parent->num_clock_domains) {
        dst_cd = &parent->clock_domains[cdc->dest_clock_id];
    }

    int src_clk_sig = src_cd ? src_cd->clock_signal_id : -1;
    int dst_clk_sig = dst_cd ? dst_cd->clock_signal_id : -1;
    int src_rst_sig = src_cd ? src_cd->reset_signal_id : -1;
    int dst_rst_sig = dst_cd ? dst_cd->reset_signal_id : -1;

    /* Find child ports */
    const IR_Signal *c_clk_wr   = lib_find_signal_by_name(lib_mod, "clk_wr");
    const IR_Signal *c_clk_rd   = lib_find_signal_by_name(lib_mod, "clk_rd");
    const IR_Signal *c_rst_wr   = lib_find_signal_by_name(lib_mod, "rst_wr");
    const IR_Signal *c_rst_rd   = lib_find_signal_by_name(lib_mod, "rst_rd");
    const IR_Signal *c_data_in  = lib_find_signal_by_name(lib_mod, "data_in");
    const IR_Signal *c_write_en = lib_find_signal_by_name(lib_mod, "write_en");
    const IR_Signal *c_read_en  = lib_find_signal_by_name(lib_mod, "read_en");
    const IR_Signal *c_data_out = lib_find_signal_by_name(lib_mod, "data_out");
    if (!c_clk_wr || !c_clk_rd || !c_data_in || !c_data_out) return -1;

    /* --- Derive write_en from source clock domain --- */
    int wr_en_sig = -1;

    if (src_cd && src_cd->statements) {

        IR_Expr *wr_en_expr = fifo_derive_enable(
            src_cd->statements, cdc->source_reg_id, true, arena);

        if (wr_en_expr && wr_en_expr->kind == EXPR_LITERAL &&
            wr_en_expr->u.literal.literal.words[0] == 1) {
            /* Unconditional write — keep 1'b1 (no wire needed) */
            wr_en_sig = -1;
        } else if (wr_en_expr) {
            snprintf(name_buf, sizeof(name_buf), "_fifo_%s_wr_en",
                     cdc->dest_alias_name ? cdc->dest_alias_name : "anon");

            wr_en_sig = fifo_add_wire(parent, arena, name_buf, 1);

            if (wr_en_sig >= 0) {
                fifo_add_async_assign(parent, arena, wr_en_sig, wr_en_expr, ASSIGN_ALIAS);

            }
        }
    }

    /* --- Derive read_en from dest clock domain --- */

    /* Note: must look up alias AFTER any fifo_add_wire calls since they realloc signals */
    int alias_id = -1;
    {
        const IR_Signal *alias_sig = lib_find_signal_by_name(parent, cdc->dest_alias_name);
        alias_id = alias_sig ? alias_sig->id : -1;
    }
    int rd_en_sig = -1;
    if (dst_cd && dst_cd->statements && alias_id >= 0) {

        IR_Expr *rd_en_expr = fifo_derive_enable(
            dst_cd->statements, alias_id, false, arena);

        if (rd_en_expr && rd_en_expr->kind == EXPR_LITERAL &&
            rd_en_expr->u.literal.literal.words[0] == 1) {
            /* Unconditional read — keep 1'b1 */
            rd_en_sig = -1;
        } else if (rd_en_expr) {
            snprintf(name_buf, sizeof(name_buf), "_fifo_%s_rd_en",
                     cdc->dest_alias_name ? cdc->dest_alias_name : "anon");
            rd_en_sig = fifo_add_wire(parent, arena, name_buf, 1);
            if (rd_en_sig >= 0)
                fifo_add_async_assign(parent, arena, rd_en_sig, rd_en_expr, ASSIGN_ALIAS);
        }
    }


    /* --- Reset polarity: FIFO uses active-high, invert if parent is active-low --- */
    int rst_wr_sig = -1;
    if (src_rst_sig >= 0 && src_cd && src_cd->reset_active == RESET_ACTIVE_LOW) {
        snprintf(name_buf, sizeof(name_buf), "_fifo_%s_rst_wr",
                 cdc->dest_alias_name ? cdc->dest_alias_name : "anon");
        rst_wr_sig = fifo_add_wire(parent, arena, name_buf, 1);
        if (rst_wr_sig >= 0) {
            IR_Expr *not_rst = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
            if (not_rst) {
                memset(not_rst, 0, sizeof(*not_rst));
                not_rst->kind = EXPR_UNARY_NOT;
                not_rst->width = 1;
                not_rst->u.unary.operand = make_sig_ref(arena, src_rst_sig, 1);
                fifo_add_async_assign(parent, arena, rst_wr_sig, not_rst, ASSIGN_ALIAS);
            }
        }
    } else {
        rst_wr_sig = src_rst_sig; /* active-high or no reset */
    }

    int rst_rd_sig = -1;
    if (dst_rst_sig >= 0 && dst_cd && dst_cd->reset_active == RESET_ACTIVE_LOW) {
        snprintf(name_buf, sizeof(name_buf), "_fifo_%s_rst_rd",
                 cdc->dest_alias_name ? cdc->dest_alias_name : "anon");
        rst_rd_sig = fifo_add_wire(parent, arena, name_buf, 1);
        if (rst_rd_sig >= 0) {
            IR_Expr *not_rst = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
            if (not_rst) {
                memset(not_rst, 0, sizeof(*not_rst));
                not_rst->kind = EXPR_UNARY_NOT;
                not_rst->width = 1;
                not_rst->u.unary.operand = make_sig_ref(arena, dst_rst_sig, 1);
                fifo_add_async_assign(parent, arena, rst_rd_sig, not_rst, ASSIGN_ALIAS);
            }
        }
    } else {
        rst_rd_sig = dst_rst_sig;
    }

    /* --- Connect 8 ports --- */
    int num_conns = 8;
    IR_InstanceConnection *conns = (IR_InstanceConnection *)jz_arena_alloc(
        arena, sizeof(IR_InstanceConnection) * num_conns);
    if (!conns) return -1;
    memset(conns, 0, sizeof(IR_InstanceConnection) * num_conns);

    conns[0] = make_conn(src_clk_sig, c_clk_wr->id, -1, -1);
    conns[1] = make_conn(dst_clk_sig, c_clk_rd->id, -1, -1);

    /* Reset connections (polarity-corrected) */
    conns[2] = make_conn(rst_wr_sig, c_rst_wr ? c_rst_wr->id : -1, -1, -1);
    if (rst_wr_sig < 0 && c_rst_wr) {
        conns[2].parent_signal_id = -1;
        conns[2].child_port_id = c_rst_wr->id;
        conns[2].const_expr = ir_strdup_arena(arena, "1'b0");
    }
    conns[3] = make_conn(rst_rd_sig, c_rst_rd ? c_rst_rd->id : -1, -1, -1);
    if (rst_rd_sig < 0 && c_rst_rd) {
        conns[3].parent_signal_id = -1;
        conns[3].child_port_id = c_rst_rd->id;
        conns[3].const_expr = ir_strdup_arena(arena, "1'b0");
    }

    conns[4] = make_conn(cdc->source_reg_id, c_data_in->id,
                         cdc->source_msb, cdc->source_lsb);

    /* write_en: use derived signal or 1'b1 */
    if (wr_en_sig >= 0) {
        conns[5] = make_conn(wr_en_sig, c_write_en ? c_write_en->id : -1, -1, -1);
    } else {
        conns[5].parent_signal_id = -1;
        conns[5].child_port_id = c_write_en ? c_write_en->id : -1;
        conns[5].parent_msb = -1;
        conns[5].parent_lsb = -1;
        conns[5].const_expr = ir_strdup_arena(arena, "1'b1");
    }

    /* read_en: use derived signal or 1'b1 */
    if (rd_en_sig >= 0) {
        conns[6] = make_conn(rd_en_sig, c_read_en ? c_read_en->id : -1, -1, -1);
    } else {
        conns[6].parent_signal_id = -1;
        conns[6].child_port_id = c_read_en ? c_read_en->id : -1;
        conns[6].parent_msb = -1;
        conns[6].parent_lsb = -1;
        conns[6].const_expr = ir_strdup_arena(arena, "1'b1");
    }

    /* Re-lookup alias_id since fifo_add_wire may have reallocated signals */
    {
        const IR_Signal *alias_relookup = lib_find_signal_by_name(parent, cdc->dest_alias_name);
        alias_id = alias_relookup ? alias_relookup->id : -1;
    }
    conns[7] = make_conn(alias_id, c_data_out->id, -1, -1);

    inst->connections = conns;
    inst->num_connections = num_conns;
    return 0;
}

/**
 * @brief Lower a CDC_HANDSHAKE entry into an IR_Instance.
 */
static int lower_cdc_handshake(const IR_CDC *cdc, const IR_Module *parent,
                                const IR_Module *lib_mod, IR_Instance *inst,
                                int inst_id, JZArena *arena)
{
    char name_buf[128];
    snprintf(name_buf, sizeof(name_buf), "u_cdc_handshake_%s",
             cdc->dest_alias_name ? cdc->dest_alias_name : "anon");

    inst->id = inst_id;
    inst->name = ir_strdup_arena(arena, name_buf);
    inst->child_module_id = lib_mod->id;

    /* 4 connections: clk_src, clk_dest, data_in, data_out */
    IR_InstanceConnection *conns = (IR_InstanceConnection *)jz_arena_alloc(
        arena, sizeof(IR_InstanceConnection) * 4);
    if (!conns) return -1;
    memset(conns, 0, sizeof(IR_InstanceConnection) * 4);

    const IR_ClockDomain *src_cd = NULL;
    const IR_ClockDomain *dst_cd = NULL;
    if (cdc->source_clock_id >= 0 && cdc->source_clock_id < parent->num_clock_domains)
        src_cd = &parent->clock_domains[cdc->source_clock_id];
    if (cdc->dest_clock_id >= 0 && cdc->dest_clock_id < parent->num_clock_domains)
        dst_cd = &parent->clock_domains[cdc->dest_clock_id];

    int src_clk_sig = src_cd ? src_cd->clock_signal_id : -1;
    int dst_clk_sig = dst_cd ? dst_cd->clock_signal_id : -1;

    const IR_Signal *child_clk_src  = lib_find_signal_by_name(lib_mod, "clk_src");
    const IR_Signal *child_clk_dest = lib_find_signal_by_name(lib_mod, "clk_dest");
    const IR_Signal *child_data_in  = lib_find_signal_by_name(lib_mod, "data_in");
    const IR_Signal *child_data_out = lib_find_signal_by_name(lib_mod, "data_out");
    if (!child_clk_src || !child_clk_dest || !child_data_in || !child_data_out) return -1;

    conns[0] = make_conn(src_clk_sig, child_clk_src->id, -1, -1);
    conns[1] = make_conn(dst_clk_sig, child_clk_dest->id, -1, -1);
    conns[2] = make_conn(cdc->source_reg_id, child_data_in->id,
                         cdc->source_msb, cdc->source_lsb);

    const IR_Signal *alias_sig = lib_find_signal_by_name(parent, cdc->dest_alias_name);
    int alias_id = alias_sig ? alias_sig->id : -1;
    conns[3] = make_conn(alias_id, child_data_out->id, -1, -1);

    inst->connections = conns;
    inst->num_connections = 4;
    return 0;
}

/**
 * @brief Lower a CDC_PULSE entry into an IR_Instance.
 */
static int lower_cdc_pulse(const IR_CDC *cdc, const IR_Module *parent,
                            const IR_Module *lib_mod, IR_Instance *inst,
                            int inst_id, JZArena *arena)
{
    char name_buf[128];
    snprintf(name_buf, sizeof(name_buf), "u_cdc_pulse_%s",
             cdc->dest_alias_name ? cdc->dest_alias_name : "anon");

    inst->id = inst_id;
    inst->name = ir_strdup_arena(arena, name_buf);
    inst->child_module_id = lib_mod->id;

    /* 4 connections: clk_src, clk_dest, pulse_in, pulse_out */
    IR_InstanceConnection *conns = (IR_InstanceConnection *)jz_arena_alloc(
        arena, sizeof(IR_InstanceConnection) * 4);
    if (!conns) return -1;
    memset(conns, 0, sizeof(IR_InstanceConnection) * 4);

    const IR_ClockDomain *src_cd = NULL;
    const IR_ClockDomain *dst_cd = NULL;
    if (cdc->source_clock_id >= 0 && cdc->source_clock_id < parent->num_clock_domains)
        src_cd = &parent->clock_domains[cdc->source_clock_id];
    if (cdc->dest_clock_id >= 0 && cdc->dest_clock_id < parent->num_clock_domains)
        dst_cd = &parent->clock_domains[cdc->dest_clock_id];

    int src_clk_sig = src_cd ? src_cd->clock_signal_id : -1;
    int dst_clk_sig = dst_cd ? dst_cd->clock_signal_id : -1;

    const IR_Signal *child_clk_src   = lib_find_signal_by_name(lib_mod, "clk_src");
    const IR_Signal *child_clk_dest  = lib_find_signal_by_name(lib_mod, "clk_dest");
    const IR_Signal *child_pulse_in  = lib_find_signal_by_name(lib_mod, "pulse_in");
    const IR_Signal *child_pulse_out = lib_find_signal_by_name(lib_mod, "pulse_out");
    if (!child_clk_src || !child_clk_dest || !child_pulse_in || !child_pulse_out) return -1;

    conns[0] = make_conn(src_clk_sig, child_clk_src->id, -1, -1);
    conns[1] = make_conn(dst_clk_sig, child_clk_dest->id, -1, -1);
    conns[2] = make_conn(cdc->source_reg_id, child_pulse_in->id,
                         cdc->source_msb, cdc->source_lsb);

    const IR_Signal *alias_sig = lib_find_signal_by_name(parent, cdc->dest_alias_name);
    int alias_id = alias_sig ? alias_sig->id : -1;
    conns[3] = make_conn(alias_id, child_pulse_out->id, -1, -1);

    inst->connections = conns;
    inst->num_connections = 4;
    return 0;
}

/**
 * @brief Lower a CDC_MCP entry into an IR_Instance.
 */
static int lower_cdc_mcp(const IR_CDC *cdc, const IR_Module *parent,
                          const IR_Module *lib_mod, IR_Instance *inst,
                          int inst_id, JZArena *arena)
{
    char name_buf[128];
    snprintf(name_buf, sizeof(name_buf), "u_cdc_mcp_%s",
             cdc->dest_alias_name ? cdc->dest_alias_name : "anon");

    inst->id = inst_id;
    inst->name = ir_strdup_arena(arena, name_buf);
    inst->child_module_id = lib_mod->id;

    /* 4 connections: clk_src, clk_dest, data_in, data_out */
    IR_InstanceConnection *conns = (IR_InstanceConnection *)jz_arena_alloc(
        arena, sizeof(IR_InstanceConnection) * 4);
    if (!conns) return -1;
    memset(conns, 0, sizeof(IR_InstanceConnection) * 4);

    const IR_ClockDomain *src_cd = NULL;
    const IR_ClockDomain *dst_cd = NULL;
    if (cdc->source_clock_id >= 0 && cdc->source_clock_id < parent->num_clock_domains)
        src_cd = &parent->clock_domains[cdc->source_clock_id];
    if (cdc->dest_clock_id >= 0 && cdc->dest_clock_id < parent->num_clock_domains)
        dst_cd = &parent->clock_domains[cdc->dest_clock_id];

    int src_clk_sig = src_cd ? src_cd->clock_signal_id : -1;
    int dst_clk_sig = dst_cd ? dst_cd->clock_signal_id : -1;

    const IR_Signal *child_clk_src  = lib_find_signal_by_name(lib_mod, "clk_src");
    const IR_Signal *child_clk_dest = lib_find_signal_by_name(lib_mod, "clk_dest");
    const IR_Signal *child_data_in  = lib_find_signal_by_name(lib_mod, "data_in");
    const IR_Signal *child_data_out = lib_find_signal_by_name(lib_mod, "data_out");
    if (!child_clk_src || !child_clk_dest || !child_data_in || !child_data_out) return -1;

    conns[0] = make_conn(src_clk_sig, child_clk_src->id, -1, -1);
    conns[1] = make_conn(dst_clk_sig, child_clk_dest->id, -1, -1);
    conns[2] = make_conn(cdc->source_reg_id, child_data_in->id,
                         cdc->source_msb, cdc->source_lsb);

    const IR_Signal *alias_sig = lib_find_signal_by_name(parent, cdc->dest_alias_name);
    int alias_id = alias_sig ? alias_sig->id : -1;
    conns[3] = make_conn(alias_id, child_data_out->id, -1, -1);

    inst->connections = conns;
    inst->num_connections = 4;
    return 0;
}

/* -------------------------------------------------------------------------
 * Unique width collection
 * -------------------------------------------------------------------------
 */

typedef struct WidthSet {
    int *widths;
    int  count;
    int  capacity;
} WidthSet;

static void width_set_init(WidthSet *ws)
{
    ws->widths = NULL;
    ws->count = 0;
    ws->capacity = 0;
}

static void width_set_free(WidthSet *ws)
{
    free(ws->widths);
    ws->widths = NULL;
    ws->count = 0;
    ws->capacity = 0;
}

static int width_set_add(WidthSet *ws, int w)
{
    for (int i = 0; i < ws->count; ++i) {
        if (ws->widths[i] == w) return 0;
    }
    if (ws->count >= ws->capacity) {
        int new_cap = ws->capacity ? ws->capacity * 2 : 8;
        int *new_arr = (int *)realloc(ws->widths, sizeof(int) * (size_t)new_cap);
        if (!new_arr) return -1;
        ws->widths = new_arr;
        ws->capacity = new_cap;
    }
    ws->widths[ws->count++] = w;
    return 0;
}

/* -------------------------------------------------------------------------
 * Main entry point
 * -------------------------------------------------------------------------
 */

int ir_build_library_modules(IR_Design *design, JZArena *arena)
{
    if (!design || !arena) return -1;
    if (design->num_modules <= 0 || !design->modules) return 0;

    /* Phase 1: Scan all modules for CDC crossings and collect requirements. */
    int need_bit = 0;
    int need_pulse = 0;
    int need_raw = 0;
    int need_reset_sync = 0;
    WidthSet bus_widths;
    WidthSet fifo_widths;
    WidthSet handshake_widths;
    WidthSet mcp_widths;
    width_set_init(&bus_widths);
    width_set_init(&fifo_widths);
    width_set_init(&handshake_widths);
    width_set_init(&mcp_widths);

    for (int mi = 0; mi < design->num_modules; ++mi) {
        const IR_Module *mod = &design->modules[mi];
        for (int ci = 0; ci < mod->num_cdc_crossings; ++ci) {
            const IR_CDC *cdc = &mod->cdc_crossings[ci];

            /* Compute effective width */
            int eff_width = 1;
            const IR_Signal *src_reg = lib_find_signal_by_id(mod, cdc->source_reg_id);
            if (src_reg) {
                if (cdc->source_msb >= 0 && cdc->source_lsb >= 0) {
                    eff_width = cdc->source_msb - cdc->source_lsb + 1;
                } else {
                    eff_width = src_reg->width;
                }
            }
            if (eff_width <= 0) eff_width = 1;

            switch (cdc->type) {
            case CDC_BIT:
                need_bit = 1;
                break;
            case CDC_BUS:
                if (width_set_add(&bus_widths, eff_width) != 0) goto fail;
                break;
            case CDC_FIFO:
                if (width_set_add(&fifo_widths, eff_width) != 0) goto fail;
                break;
            case CDC_HANDSHAKE:
                if (width_set_add(&handshake_widths, eff_width) != 0) goto fail;
                break;
            case CDC_PULSE:
                need_pulse = 1;
                break;
            case CDC_MCP:
                if (width_set_add(&mcp_widths, eff_width) != 0) goto fail;
                break;
            case CDC_RAW:
                /* RAW needs no library module — lowered to a direct assign. */
                need_raw = 1;
                break;
            }
        }

        /* Check for Immediate resets needing reset synchronizer */
        for (int cdi = 0; cdi < mod->num_clock_domains; ++cdi) {
            if (mod->clock_domains[cdi].reset_signal_id >= 0 &&
                mod->clock_domains[cdi].reset_type == RESET_IMMEDIATE) {
                need_reset_sync = 1;
            }
        }
    }

    int num_lib = need_bit + need_pulse + need_reset_sync +
                  bus_widths.count + fifo_widths.count +
                  handshake_widths.count + mcp_widths.count;
    if (num_lib == 0 && !need_raw) {
        width_set_free(&bus_widths);
        width_set_free(&fifo_widths);
        width_set_free(&handshake_widths);
        width_set_free(&mcp_widths);
        return 0;
    }

    /* Phase 2: Allocate expanded module array. */
    int old_count = design->num_modules;
    int new_count = old_count + num_lib;
    IR_Module *new_modules = (IR_Module *)jz_arena_alloc(arena,
                                                          sizeof(IR_Module) * (size_t)new_count);
    if (!new_modules) goto fail;
    memcpy(new_modules, design->modules, sizeof(IR_Module) * (size_t)old_count);
    memset(&new_modules[old_count], 0, sizeof(IR_Module) * (size_t)num_lib);

    design->modules = new_modules;
    design->num_modules = new_count;

    /* Phase 3: Build library modules. */
    int lib_idx = old_count;

    /* CDC BIT module (single, width always 1) */
    int bit_module_id = -1;
    if (need_bit) {
        bit_module_id = lib_idx;
        if (build_cdc_bit_module(&new_modules[lib_idx], lib_idx, arena) != 0) goto fail;
        ++lib_idx;
    }

    /* CDC PULSE module (single, width always 1) */
    int pulse_module_id = -1;
    if (need_pulse) {
        pulse_module_id = lib_idx;
        if (build_cdc_pulse_module(&new_modules[lib_idx], lib_idx, arena) != 0) goto fail;
        ++lib_idx;
    }

    /* Reset synchronizer module */
    int reset_sync_module_id = -1;
    if (need_reset_sync) {
        reset_sync_module_id = lib_idx;
        if (build_reset_sync_module(&new_modules[lib_idx], lib_idx, arena) != 0) goto fail;
        ++lib_idx;
    }

    /* CDC BUS modules (one per unique width) */
    int *bus_module_ids = NULL;
    if (bus_widths.count > 0) {
        bus_module_ids = (int *)malloc(sizeof(int) * (size_t)bus_widths.count);
        if (!bus_module_ids) goto fail;
        for (int i = 0; i < bus_widths.count; ++i) {
            bus_module_ids[i] = lib_idx;
            if (build_cdc_bus_module(&new_modules[lib_idx], lib_idx,
                                     bus_widths.widths[i], arena) != 0) {
                free(bus_module_ids);
                goto fail;
            }
            ++lib_idx;
        }
    }

    /* CDC FIFO modules (one per unique width) */
    int *fifo_module_ids = NULL;
    if (fifo_widths.count > 0) {
        fifo_module_ids = (int *)malloc(sizeof(int) * (size_t)fifo_widths.count);
        if (!fifo_module_ids) goto fail;
        for (int i = 0; i < fifo_widths.count; ++i) {
            fifo_module_ids[i] = lib_idx;
            if (build_cdc_fifo_module(&new_modules[lib_idx], lib_idx,
                                      fifo_widths.widths[i], arena) != 0) {
                free(bus_module_ids);
                free(fifo_module_ids);
                goto fail;
            }
            ++lib_idx;
        }
    }

    /* CDC HANDSHAKE modules (one per unique width) */
    int *handshake_module_ids = NULL;
    if (handshake_widths.count > 0) {
        handshake_module_ids = (int *)malloc(sizeof(int) * (size_t)handshake_widths.count);
        if (!handshake_module_ids) goto fail;
        for (int i = 0; i < handshake_widths.count; ++i) {
            handshake_module_ids[i] = lib_idx;
            if (build_cdc_handshake_module(&new_modules[lib_idx], lib_idx,
                                            handshake_widths.widths[i], arena) != 0) {
                free(bus_module_ids);
                free(fifo_module_ids);
                free(handshake_module_ids);
                goto fail;
            }
            ++lib_idx;
        }
    }

    /* CDC MCP modules (one per unique width) */
    int *mcp_module_ids = NULL;
    if (mcp_widths.count > 0) {
        mcp_module_ids = (int *)malloc(sizeof(int) * (size_t)mcp_widths.count);
        if (!mcp_module_ids) goto fail;
        for (int i = 0; i < mcp_widths.count; ++i) {
            mcp_module_ids[i] = lib_idx;
            if (build_cdc_mcp_module(&new_modules[lib_idx], lib_idx,
                                      mcp_widths.widths[i], arena) != 0) {
                free(bus_module_ids);
                free(fifo_module_ids);
                free(handshake_module_ids);
                free(mcp_module_ids);
                goto fail;
            }
            ++lib_idx;
        }
    }

    /* Phase 4: Lower each CDC entry into an IR_Instance.
     * For each module, we grow the instances array by the number of
     * CDC crossings, create instances, then clear cdc_crossings.
     */
    for (int mi = 0; mi < old_count; ++mi) {
        IR_Module *mod = &new_modules[mi];
        if (mod->num_cdc_crossings <= 0) continue;

        int old_inst_count = mod->num_instances;
        int new_inst_count = old_inst_count + mod->num_cdc_crossings;
        IR_Instance *new_instances = (IR_Instance *)jz_arena_alloc(
            arena, sizeof(IR_Instance) * (size_t)new_inst_count);
        if (!new_instances) {
            free(bus_module_ids);
            free(fifo_module_ids);
            free(handshake_module_ids);
            free(mcp_module_ids);
            goto fail;
        }
        memset(new_instances, 0, sizeof(IR_Instance) * (size_t)new_inst_count);
        if (mod->instances && old_inst_count > 0) {
            memcpy(new_instances, mod->instances,
                   sizeof(IR_Instance) * (size_t)old_inst_count);
        }

        int next_id = old_inst_count;
        for (int ci = 0; ci < mod->num_cdc_crossings; ++ci) {
            const IR_CDC *cdc = &mod->cdc_crossings[ci];
            IR_Instance *inst = &new_instances[next_id];

            /* Compute effective width for library module lookup */
            int eff_width = 1;
            const IR_Signal *src_reg = lib_find_signal_by_id(mod, cdc->source_reg_id);
            if (src_reg) {
                if (cdc->source_msb >= 0 && cdc->source_lsb >= 0) {
                    eff_width = cdc->source_msb - cdc->source_lsb + 1;
                } else {
                    eff_width = src_reg->width;
                }
            }
            if (eff_width <= 0) eff_width = 1;

            int rc = -1;
            switch (cdc->type) {
            case CDC_BIT:
                if (bit_module_id >= 0) {
                    rc = lower_cdc_bit(cdc, mod, &new_modules[bit_module_id],
                                       inst, next_id, arena);
                }
                break;
            case CDC_BUS: {
                int bus_mod_id = -1;
                for (int i = 0; i < bus_widths.count; ++i) {
                    if (bus_widths.widths[i] == eff_width) {
                        bus_mod_id = bus_module_ids[i];
                        break;
                    }
                }
                if (bus_mod_id >= 0) {
                    rc = lower_cdc_bus(cdc, mod, &new_modules[bus_mod_id],
                                       inst, next_id, arena);
                }
                break;
            }
            case CDC_FIFO: {
                int fifo_mod_id = -1;
                for (int i = 0; i < fifo_widths.count; ++i) {
                    if (fifo_widths.widths[i] == eff_width) {
                        fifo_mod_id = fifo_module_ids[i];
                        break;
                    }
                }
                if (fifo_mod_id >= 0) {
                    rc = lower_cdc_fifo(cdc, mod, &new_modules[fifo_mod_id],
                                        inst, next_id, arena);
                }
                break;
            }
            case CDC_HANDSHAKE: {
                int hs_mod_id = -1;
                for (int i = 0; i < handshake_widths.count; ++i) {
                    if (handshake_widths.widths[i] == eff_width) {
                        hs_mod_id = handshake_module_ids[i];
                        break;
                    }
                }
                if (hs_mod_id >= 0) {
                    rc = lower_cdc_handshake(cdc, mod, &new_modules[hs_mod_id],
                                              inst, next_id, arena);
                }
                break;
            }
            case CDC_PULSE:
                if (pulse_module_id >= 0) {
                    rc = lower_cdc_pulse(cdc, mod, &new_modules[pulse_module_id],
                                         inst, next_id, arena);
                }
                break;
            case CDC_MCP: {
                int mcp_mod_id = -1;
                for (int i = 0; i < mcp_widths.count; ++i) {
                    if (mcp_widths.widths[i] == eff_width) {
                        mcp_mod_id = mcp_module_ids[i];
                        break;
                    }
                }
                if (mcp_mod_id >= 0) {
                    rc = lower_cdc_mcp(cdc, mod, &new_modules[mcp_mod_id],
                                        inst, next_id, arena);
                }
                break;
            }
            case CDC_RAW:
                /* RAW: no instance — emit direct assign in async block */
                if (lower_cdc_raw(cdc, mod, arena) == 0) {
                    rc = 1; /* success, but no instance created */
                }
                break;
            }

            if (rc != 0) {
                continue;
            }
            ++next_id;
        }

        mod->instances = new_instances;
        mod->num_instances = next_id;

        mod->cdc_crossings = NULL;
        mod->num_cdc_crossings = 0;
    }

    /* Phase 5: Auto-instantiate reset synchronizers for Immediate resets.
     * For each user module with RESET_TYPE=Immediate clock domains,
     * add a JZHDL_LIB_RESET_SYNC instance and update the clock domain
     * to use the synchronized reset in its body guard.
     */
    if (reset_sync_module_id >= 0) {
        for (int mi = 0; mi < old_count; ++mi) {
            IR_Module *mod = &new_modules[mi];
            int num_imm_resets = 0;

            /* Count immediate resets */
            for (int cdi = 0; cdi < mod->num_clock_domains; ++cdi) {
                if (mod->clock_domains[cdi].reset_signal_id >= 0 &&
                    mod->clock_domains[cdi].reset_type == RESET_IMMEDIATE) {
                    ++num_imm_resets;
                }
            }
            if (num_imm_resets == 0) continue;

            /* Grow instances array */
            int old_inst_count = mod->num_instances;
            int new_inst_count = old_inst_count + num_imm_resets;
            IR_Instance *new_insts = (IR_Instance *)jz_arena_alloc(
                arena, sizeof(IR_Instance) * (size_t)new_inst_count);
            if (!new_insts) goto fail_cleanup;
            memset(new_insts, 0, sizeof(IR_Instance) * (size_t)new_inst_count);
            if (mod->instances && old_inst_count > 0) {
                memcpy(new_insts, mod->instances,
                       sizeof(IR_Instance) * (size_t)old_inst_count);
            }

            /* Grow signals array for rst_sync wires */
            int old_sig_count = mod->num_signals;
            int new_sig_count = old_sig_count + num_imm_resets;
            IR_Signal *new_sigs = (IR_Signal *)jz_arena_alloc(
                arena, sizeof(IR_Signal) * (size_t)new_sig_count);
            if (!new_sigs) goto fail_cleanup;
            memset(new_sigs, 0, sizeof(IR_Signal) * (size_t)new_sig_count);
            if (mod->signals && old_sig_count > 0) {
                memcpy(new_sigs, mod->signals,
                       sizeof(IR_Signal) * (size_t)old_sig_count);
            }

            int next_inst = old_inst_count;
            int next_sig = old_sig_count;

            for (int cdi = 0; cdi < mod->num_clock_domains; ++cdi) {
                IR_ClockDomain *cd = &mod->clock_domains[cdi];
                if (cd->reset_signal_id < 0 || cd->reset_type != RESET_IMMEDIATE) continue;

                /* Create a wire signal for the synchronized reset */
                char sig_name[64];
                snprintf(sig_name, sizeof(sig_name), "rst_sync_%d", cdi);
                init_net_signal(&new_sigs[next_sig], next_sig, sig_name, 1, mod->id, arena);
                int sync_sig_id = next_sig;
                ++next_sig;

                /* Create instance */
                IR_Instance *inst = &new_insts[next_inst];
                char inst_name[64];
                snprintf(inst_name, sizeof(inst_name), "u_rst_sync_%d", cdi);
                inst->id = next_inst;
                inst->name = ir_strdup_arena(arena, inst_name);
                inst->child_module_id = reset_sync_module_id;

                const IR_Module *lib_mod_rs = &new_modules[reset_sync_module_id];
                const IR_Signal *c_clk = lib_find_signal_by_name(lib_mod_rs, "clk");
                const IR_Signal *c_rst_in = lib_find_signal_by_name(lib_mod_rs, "rst_async_n");
                const IR_Signal *c_rst_out = lib_find_signal_by_name(lib_mod_rs, "rst_sync_n");
                if (!c_clk || !c_rst_in || !c_rst_out) goto fail_cleanup;

                IR_InstanceConnection *conns = (IR_InstanceConnection *)jz_arena_alloc(
                    arena, sizeof(IR_InstanceConnection) * 3);
                if (!conns) goto fail_cleanup;
                memset(conns, 0, sizeof(IR_InstanceConnection) * 3);

                conns[0] = make_conn(cd->clock_signal_id, c_clk->id, -1, -1);
                conns[1] = make_conn(cd->reset_signal_id, c_rst_in->id, -1, -1);
                conns[2] = make_conn(sync_sig_id, c_rst_out->id, -1, -1);

                inst->connections = conns;
                inst->num_connections = 3;
                ++next_inst;

                /* Update clock domain to use synchronized reset in body guard.
                 * Patch the statement tree's reset condition to reference
                 * the synchronized signal instead of the raw async reset.
                 * The statement tree is: if (!reset) { reset_vals } else { body }
                 * We need to replace the signal ref in the condition.
                 */
                cd->reset_sync_signal_id = sync_sig_id;
                if (cd->statements && cd->statements->kind == STMT_IF) {
                    IR_Expr *cond = cd->statements->u.if_stmt.condition;
                    /* For active-low: cond is EXPR_LOGICAL_NOT wrapping a signal ref */
                    if (cond && cond->kind == EXPR_LOGICAL_NOT && cond->u.unary.operand) {
                        IR_Expr *inner = cond->u.unary.operand;
                        if (inner->kind == EXPR_SIGNAL_REF &&
                            inner->u.signal_ref.signal_id == cd->reset_signal_id) {
                            inner->u.signal_ref.signal_id = sync_sig_id;
                        }
                    }
                    /* For active-high: cond is a direct signal ref */
                    else if (cond && cond->kind == EXPR_SIGNAL_REF &&
                             cond->u.signal_ref.signal_id == cd->reset_signal_id) {
                        cond->u.signal_ref.signal_id = sync_sig_id;
                    }
                }
            }

            mod->instances = new_insts;
            mod->num_instances = next_inst;
            mod->signals = new_sigs;
            mod->num_signals = next_sig;
        }
    }

    free(bus_module_ids);
    free(fifo_module_ids);
    free(handshake_module_ids);
    free(mcp_module_ids);
    width_set_free(&bus_widths);
    width_set_free(&fifo_widths);
    width_set_free(&handshake_widths);
    width_set_free(&mcp_widths);
    return 0;

fail_cleanup:
    free(bus_module_ids);
    free(fifo_module_ids);
    free(handshake_module_ids);
    free(mcp_module_ids);
fail:
    width_set_free(&bus_widths);
    width_set_free(&fifo_widths);
    width_set_free(&handshake_widths);
    width_set_free(&mcp_widths);
    return -1;
}

/* -------------------------------------------------------------------------
 * Memory write lowering into clock domain statement trees
 * -------------------------------------------------------------------------
 */

int ir_lower_memory_writes(IR_Design *design, JZArena *arena)
{
    if (!design || !arena) return -1;

    for (int mi = 0; mi < design->num_modules; ++mi) {
        IR_Module *mod = &design->modules[mi];
        if (mod->num_memories <= 0 || mod->num_clock_domains <= 0) {
            continue;
        }

        for (int cdi = 0; cdi < mod->num_clock_domains; ++cdi) {
            IR_ClockDomain *cd = &mod->clock_domains[cdi];

            /* Collect memory write statements for this clock domain,
             * mirroring the logic from emit_memory_writes_for_clock_domain.
             */
            int write_count = 0;
            for (int mei = 0; mei < mod->num_memories; ++mei) {
                const IR_Memory *m = &mod->memories[mei];
                for (int pi = 0; pi < m->num_ports; ++pi) {
                    const IR_MemoryPort *mp = &m->ports[pi];
                    if (mp->kind != MEM_PORT_WRITE) continue;
                    if (mp->addr_signal_id < 0 || mp->data_in_signal_id < 0) continue;
                    if (mp->enable_signal_id < 0) continue;

                    /* Check if this write port belongs to this clock domain. */
                    int addr_cd = -1, data_cd = -1;
                    for (int si = 0; si < mod->num_signals; ++si) {
                        if (mod->signals[si].id == mp->addr_signal_id &&
                            mod->signals[si].kind == SIG_REGISTER) {
                            addr_cd = mod->signals[si].u.reg.home_clock_domain_id;
                        }
                        if (mod->signals[si].id == mp->data_in_signal_id &&
                            mod->signals[si].kind == SIG_REGISTER) {
                            data_cd = mod->signals[si].u.reg.home_clock_domain_id;
                        }
                    }
                    if (addr_cd >= 0 && addr_cd != cd->id &&
                        data_cd >= 0 && data_cd != cd->id) {
                        continue;
                    }

                    /* Verify the enable signal has a name. */
                    const IR_Signal *en_sig = NULL;
                    for (int si = 0; si < mod->num_signals; ++si) {
                        if (mod->signals[si].id == mp->enable_signal_id) {
                            en_sig = &mod->signals[si];
                            break;
                        }
                    }
                    if (!en_sig || !en_sig->name) continue;

                    /* Verify addr and data signals have names. */
                    const IR_Signal *addr_sig = NULL, *data_sig = NULL;
                    for (int si = 0; si < mod->num_signals; ++si) {
                        if (mod->signals[si].id == mp->addr_signal_id)
                            addr_sig = &mod->signals[si];
                        if (mod->signals[si].id == mp->data_in_signal_id)
                            data_sig = &mod->signals[si];
                    }
                    if (!addr_sig || !addr_sig->name || !data_sig || !data_sig->name) continue;

                    write_count++;
                }
            }

            if (write_count == 0) continue;

            /* Build memory write statements. */
            IR_Stmt *mem_stmts = (IR_Stmt *)jz_arena_alloc(arena,
                sizeof(IR_Stmt) * (size_t)write_count);
            if (!mem_stmts) return -1;
            memset(mem_stmts, 0, sizeof(IR_Stmt) * (size_t)write_count);

            int ws = 0;
            for (int mei = 0; mei < mod->num_memories; ++mei) {
                const IR_Memory *m = &mod->memories[mei];
                for (int pi = 0; pi < m->num_ports; ++pi) {
                    const IR_MemoryPort *mp = &m->ports[pi];
                    if (mp->kind != MEM_PORT_WRITE) continue;
                    if (mp->addr_signal_id < 0 || mp->data_in_signal_id < 0) continue;
                    if (mp->enable_signal_id < 0) continue;

                    int addr_cd = -1, data_cd = -1;
                    for (int si = 0; si < mod->num_signals; ++si) {
                        if (mod->signals[si].id == mp->addr_signal_id &&
                            mod->signals[si].kind == SIG_REGISTER) {
                            addr_cd = mod->signals[si].u.reg.home_clock_domain_id;
                        }
                        if (mod->signals[si].id == mp->data_in_signal_id &&
                            mod->signals[si].kind == SIG_REGISTER) {
                            data_cd = mod->signals[si].u.reg.home_clock_domain_id;
                        }
                    }
                    if (addr_cd >= 0 && addr_cd != cd->id &&
                        data_cd >= 0 && data_cd != cd->id) {
                        continue;
                    }

                    const IR_Signal *en_sig = NULL;
                    for (int si = 0; si < mod->num_signals; ++si) {
                        if (mod->signals[si].id == mp->enable_signal_id) {
                            en_sig = &mod->signals[si];
                            break;
                        }
                    }
                    if (!en_sig || !en_sig->name) continue;

                    const IR_Signal *addr_sig = NULL, *data_sig = NULL;
                    for (int si = 0; si < mod->num_signals; ++si) {
                        if (mod->signals[si].id == mp->addr_signal_id)
                            addr_sig = &mod->signals[si];
                        if (mod->signals[si].id == mp->data_in_signal_id)
                            data_sig = &mod->signals[si];
                    }
                    if (!addr_sig || !addr_sig->name || !data_sig || !data_sig->name) continue;

                    /* Build: if (enable) mem[addr] <= data; */
                    IR_Stmt *mw_inner = (IR_Stmt *)jz_arena_alloc(arena, sizeof(IR_Stmt));
                    if (!mw_inner) return -1;
                    memset(mw_inner, 0, sizeof(*mw_inner));
                    mw_inner->kind = STMT_MEM_WRITE;
                    mw_inner->u.mem_write.memory_name = ir_strdup_arena(arena,
                        (m->name && m->name[0] != '\0') ? m->name : "jz_mem");
                    mw_inner->u.mem_write.port_name = ir_strdup_arena(arena,
                        mp->name ? mp->name : "");
                    mw_inner->u.mem_write.address = make_sig_ref(arena,
                        mp->addr_signal_id, addr_sig->width);
                    mw_inner->u.mem_write.data = make_sig_ref(arena,
                        mp->data_in_signal_id, data_sig->width);

                    /* Wrap in if(enable) */
                    IR_Stmt *if_en = &mem_stmts[ws++];
                    if_en->kind = STMT_IF;
                    IR_Expr *en_cond = make_sig_ref(arena, mp->enable_signal_id, 1);
                    if_en->u.if_stmt.condition = en_cond;
                    if_en->u.if_stmt.then_block = mw_inner;
                    if_en->u.if_stmt.elif_chain = NULL;
                    if_en->u.if_stmt.else_block = NULL;
                }
            }

            /* If the clock domain has a reset, wrap memory writes in a
             * NOT-reset guard so they only execute when not in reset.
             */
            IR_Stmt *mem_tree = make_block(arena, mem_stmts, ws);
            if (!mem_tree) return -1;

            if (cd->reset_signal_id >= 0) {
                /* Build the NOT-reset condition:
                 * ACTIVE_HIGH -> if (!rst) { mem_writes }
                 * ACTIVE_LOW  -> if (rst)  { mem_writes }
                 */
                IR_Expr *rst_ref = make_sig_ref(arena, cd->reset_signal_id, 1);
                IR_Expr *guard_cond;
                if (cd->reset_active == RESET_ACTIVE_HIGH) {
                    /* !rst */
                    IR_Expr *not_rst = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
                    if (!not_rst) return -1;
                    memset(not_rst, 0, sizeof(*not_rst));
                    not_rst->kind = EXPR_UNARY_NOT;
                    not_rst->width = 1;
                    not_rst->u.unary.operand = rst_ref;
                    guard_cond = not_rst;
                } else {
                    /* rst (active low: memory writes happen when rst is high) */
                    guard_cond = rst_ref;
                }

                IR_Stmt *guard_if = (IR_Stmt *)jz_arena_alloc(arena, sizeof(IR_Stmt));
                if (!guard_if) return -1;
                memset(guard_if, 0, sizeof(*guard_if));
                guard_if->kind = STMT_IF;
                guard_if->u.if_stmt.condition = guard_cond;
                guard_if->u.if_stmt.then_block = mem_tree;
                guard_if->u.if_stmt.elif_chain = NULL;
                guard_if->u.if_stmt.else_block = NULL;

                mem_tree = guard_if;
            }

            /* Append mem_tree to the clock domain's existing statements. */
            if (!cd->statements) {
                cd->statements = mem_tree;
            } else if (cd->statements->kind == STMT_BLOCK) {
                /* Extend the existing block. */
                int old_count = cd->statements->u.block.count;
                int new_count = old_count + 1;
                IR_Stmt *new_stmts = (IR_Stmt *)jz_arena_alloc(arena,
                    sizeof(IR_Stmt) * (size_t)new_count);
                if (!new_stmts) return -1;
                memcpy(new_stmts, cd->statements->u.block.stmts,
                       sizeof(IR_Stmt) * (size_t)old_count);
                new_stmts[old_count] = *mem_tree;
                cd->statements->u.block.stmts = new_stmts;
                cd->statements->u.block.count = new_count;
            } else {
                /* Create a new STMT_BLOCK containing original + mem writes. */
                IR_Stmt *new_stmts = (IR_Stmt *)jz_arena_alloc(arena,
                    sizeof(IR_Stmt) * 2);
                if (!new_stmts) return -1;
                new_stmts[0] = *cd->statements;
                new_stmts[1] = *mem_tree;
                cd->statements = make_block(arena, new_stmts, 2);
            }
        }
    }

    return 0;
}
