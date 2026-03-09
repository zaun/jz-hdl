/*
 * emit_blocks.c - Always block emission for the Verilog-2005 backend.
 *
 * This file handles emitting ASYNCHRONOUS (always @*) and SYNCHRONOUS
 * (always @posedge/negedge) blocks.
 *
 * For BLOCK-type memories (BSRAM), both writes AND reads are emitted
 * in separate always blocks to enable proper inference by synthesis tools.
 * BSRAM reads use an intermediate signal (e.g., rom_bsram_out) that is
 * read unconditionally using the bus address. The main always block then
 * references this intermediate instead of the memory array directly.
 * This avoids placing reset logic in the read always block, which would
 * prevent yosys/Gowin from inferring BSRAM.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "verilog_internal.h"
#include "ir.h"

/* Global flag for skipping BLOCK memory accesses in main block. */
int g_skip_block_mem_accesses = 0;

/* Global flag for skipping INOUT port assignments in async block. */
int g_skip_inout_port_assigns = 0;

/* -------------------------------------------------------------------------
 * BLOCK memory helpers for BSRAM inference
 * -------------------------------------------------------------------------
 */

int memory_is_block_type(const IR_Module *mod, const char *mem_name)
{
    if (!mod || !mem_name) {
        return 0;
    }
    for (int i = 0; i < mod->num_memories; ++i) {
        const IR_Memory *m = &mod->memories[i];
        if (m->name && strcmp(m->name, mem_name) == 0) {
            return m->kind == MEM_KIND_BLOCK;
        }
    }
    return 0;
}

int stmt_is_skipped_block_mem_write(const IR_Module *mod, const IR_Stmt *stmt)
{
    if (!g_skip_block_mem_accesses || !mod || !stmt) {
        return 0;
    }
    if (stmt->kind != STMT_MEM_WRITE) {
        return 0;
    }
    return memory_is_block_type(mod, stmt->u.mem_write.memory_name);
}

/* Check if an expression contains a BLOCK memory read. */
static int expr_has_block_mem_read(const IR_Module *mod, const IR_Expr *expr)
{
    if (!mod || !expr) {
        return 0;
    }
    if (expr->kind == EXPR_MEM_READ) {
        return memory_is_block_type(mod, expr->u.mem_read.memory_name);
    }
    /* Recursively check subexpressions. */
    switch (expr->kind) {
        case EXPR_TERNARY:
            return expr_has_block_mem_read(mod, expr->u.ternary.condition) ||
                   expr_has_block_mem_read(mod, expr->u.ternary.true_val) ||
                   expr_has_block_mem_read(mod, expr->u.ternary.false_val);
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
        case EXPR_LOGICAL_OR:
            return expr_has_block_mem_read(mod, expr->u.binary.left) ||
                   expr_has_block_mem_read(mod, expr->u.binary.right);
        case EXPR_UNARY_NOT:
        case EXPR_UNARY_NEG:
        case EXPR_LOGICAL_NOT:
            return expr_has_block_mem_read(mod, expr->u.unary.operand);
        case EXPR_CONCAT:
            for (int i = 0; i < expr->u.concat.num_operands; ++i) {
                if (expr_has_block_mem_read(mod, expr->u.concat.operands[i])) {
                    return 1;
                }
            }
            return 0;
        default:
            return 0;
    }
}

int assignment_has_skipped_block_mem_read(const IR_Module *mod, const IR_Assignment *a)
{
    if (!g_skip_block_mem_accesses || !mod || !a) {
        return 0;
    }
    return expr_has_block_mem_read(mod, a->rhs);
}

/* -------------------------------------------------------------------------
 * BSRAM read intermediate signal mappings
 *
 * When a BLOCK memory has a SYNC read port, we emit a separate always
 * block for the read (without reset) and an intermediate signal. The
 * main always block then references the intermediate instead of the
 * memory array directly. This mapping tracks which memories have been
 * given intermediates so emit_expr and emit_stmt can substitute.
 * -------------------------------------------------------------------------
 */

#define MAX_BSRAM_READ_MAPPINGS 32

static struct {
    const char *mem_name;
} s_bsram_read_mappings[MAX_BSRAM_READ_MAPPINGS];
static int s_bsram_read_mapping_count = 0;

void bsram_mappings_clear(void)
{
    for (int i = 0; i < s_bsram_read_mapping_count; ++i) {
        free((void *)s_bsram_read_mappings[i].mem_name);
        s_bsram_read_mappings[i].mem_name = NULL;
    }
    s_bsram_read_mapping_count = 0;
}

static void bsram_mappings_add(const char *mem_name)
{
    if (s_bsram_read_mapping_count >= MAX_BSRAM_READ_MAPPINGS) {
        return;
    }
    s_bsram_read_mappings[s_bsram_read_mapping_count++].mem_name = strdup(mem_name);
}

int has_bsram_read_intermediate(const char *mem_name)
{
    if (!mem_name) {
        return 0;
    }
    for (int i = 0; i < s_bsram_read_mapping_count; ++i) {
        if (s_bsram_read_mappings[i].mem_name &&
            strcmp(s_bsram_read_mappings[i].mem_name, mem_name) == 0) {
            return 1;
        }
    }
    return 0;
}

/* -------------------------------------------------------------------------
 * BLOCK memory access collection structures
 * -------------------------------------------------------------------------
 */

typedef struct {
    const IR_Stmt *stmt;           /* The memory access statement or assignment. */
    const IR_Expr *condition;      /* Guarding condition (NULL if unconditional). */
    int is_write;                  /* 1 for write, 0 for read. */
    const char *mem_name;          /* Memory name. */
    const char *port_name;         /* Port name. */
    /* SELECT/CASE context (for memory accesses inside case branches). */
    const IR_Expr *select_selector; /* Selector expression (NULL if not in SELECT). */
    const IR_Expr *case_value;      /* Case value expression (NULL for default). */
    int lhs_signal_id;             /* LHS signal ID for read assignments (-1 for writes). */
    int lhs_width;                 /* Width of LHS signal for read assignments. */
    int emitted;                   /* 1 if already emitted (combined with read block). */
} BlockMemAccess;


typedef struct {
    BlockMemAccess *items;
    int count;
    int capacity;
} BlockMemAccessList;

static void access_list_init(BlockMemAccessList *list)
{
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

static void access_list_free(BlockMemAccessList *list)
{
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

static void access_list_add(BlockMemAccessList *list, BlockMemAccess access)
{
    if (list->count >= list->capacity) {
        int new_cap = list->capacity == 0 ? 8 : list->capacity * 2;
        BlockMemAccess *new_items = realloc(list->items, new_cap * sizeof(BlockMemAccess));
        if (!new_items) return;
        list->items = new_items;
        list->capacity = new_cap;
    }
    list->items[list->count++] = access;
}

/* -------------------------------------------------------------------------
 * Collect BLOCK memory accesses from statement tree
 * -------------------------------------------------------------------------
 */

/* Forward declaration. */
static void collect_block_mem_accesses_internal(const IR_Module *mod, const IR_Stmt *stmt,
                                                const IR_Expr *cond,
                                                const IR_Expr *select_selector,
                                                const IR_Expr *case_value,
                                                BlockMemAccessList *list);

static void collect_from_block_internal(const IR_Module *mod, const IR_BlockStmt *blk,
                                        const IR_Expr *cond,
                                        const IR_Expr *select_selector,
                                        const IR_Expr *case_value,
                                        BlockMemAccessList *list)
{
    if (!blk) return;
    for (int i = 0; i < blk->count; ++i) {
        collect_block_mem_accesses_internal(mod, &blk->stmts[i], cond,
                                            select_selector, case_value, list);
    }
}

static void collect_block_mem_accesses_internal(const IR_Module *mod, const IR_Stmt *stmt,
                                                const IR_Expr *cond,
                                                const IR_Expr *select_selector,
                                                const IR_Expr *case_value,
                                                BlockMemAccessList *list)
{
    if (!mod || !stmt) return;

    switch (stmt->kind) {
        case STMT_MEM_WRITE: {
            if (!memory_is_block_type(mod, stmt->u.mem_write.memory_name)) {
                return;
            }
            BlockMemAccess acc = {0};
            acc.stmt = stmt;
            acc.condition = cond;
            acc.is_write = 1;
            acc.mem_name = stmt->u.mem_write.memory_name;
            acc.port_name = stmt->u.mem_write.port_name;
            acc.select_selector = select_selector;
            acc.case_value = case_value;
            access_list_add(list, acc);
            break;
        }

        case STMT_ASSIGNMENT: {
            const IR_Assignment *a = &stmt->u.assign;
            if (expr_has_block_mem_read(mod, a->rhs)) {
                /* Find the memory and port name from the expression. */
                const char *mem_name = NULL;
                const char *port_name = NULL;
                if (a->rhs && a->rhs->kind == EXPR_MEM_READ) {
                    mem_name = a->rhs->u.mem_read.memory_name;
                    port_name = a->rhs->u.mem_read.port_name;
                }
                if (mem_name && memory_is_block_type(mod, mem_name)) {
                    BlockMemAccess acc = {0};
                    acc.stmt = stmt;
                    acc.condition = cond;
                    acc.is_write = 0;
                    acc.mem_name = mem_name;
                    acc.port_name = port_name;
                    acc.select_selector = select_selector;
                    acc.case_value = case_value;
                    acc.lhs_signal_id = a->lhs_signal_id;
                    /* Get LHS signal width. */
                    const IR_Signal *lhs_sig = find_signal_by_id(mod, a->lhs_signal_id);
                    acc.lhs_width = lhs_sig ? lhs_sig->width : 1;
                    access_list_add(list, acc);
                }
            }
            break;
        }

        case STMT_BLOCK:
            collect_from_block_internal(mod, &stmt->u.block, cond,
                                        select_selector, case_value, list);
            break;

        case STMT_IF: {
            const IR_IfStmt *ifs = &stmt->u.if_stmt;
            /* Build combined condition: outer_cond && inner_condition.
             * This preserves enclosing IF guards when extracting BSRAM
             * write ports into separate always blocks.
             */
            const IR_Expr *then_cond;
            if (cond) {
                IR_Expr *and_node = calloc(1, sizeof(IR_Expr));
                and_node->kind = EXPR_LOGICAL_AND;
                and_node->width = 1;
                and_node->u.binary.left  = (IR_Expr *)cond;
                and_node->u.binary.right = ifs->condition;
                then_cond = and_node;
            } else {
                then_cond = ifs->condition;
            }
            collect_block_mem_accesses_internal(mod, ifs->then_block, then_cond,
                                                select_selector, case_value, list);
            /* For elif chain (another STMT_IF), pass combined outer cond
             * so it can merge its own condition on top. */
            if (ifs->elif_chain) {
                collect_block_mem_accesses_internal(mod, ifs->elif_chain, cond,
                                                    select_selector, case_value, list);
            }
            /* For else block, build condition: outer_cond && !(inner_condition).
             * The else branch is structurally guarded by the negation of the
             * inner IF condition, which must be explicit for BSRAM extraction. */
            if (ifs->else_block) {
                IR_Expr *not_node = calloc(1, sizeof(IR_Expr));
                not_node->kind = EXPR_LOGICAL_NOT;
                not_node->width = 1;
                not_node->u.unary.operand = ifs->condition;
                const IR_Expr *else_cond;
                if (cond) {
                    IR_Expr *and_node = calloc(1, sizeof(IR_Expr));
                    and_node->kind = EXPR_LOGICAL_AND;
                    and_node->width = 1;
                    and_node->u.binary.left  = (IR_Expr *)cond;
                    and_node->u.binary.right = not_node;
                    else_cond = and_node;
                } else {
                    else_cond = not_node;
                }
                collect_block_mem_accesses_internal(mod, ifs->else_block, else_cond,
                                                    select_selector, case_value, list);
            }
            break;
        }

        case STMT_SELECT: {
            const IR_SelectStmt *sel = &stmt->u.select_stmt;
            for (int i = 0; i < sel->num_cases; ++i) {
                if (select_selector && case_value) {
                    /* We're already inside an outer SELECT/CASE. Build a combined
                     * condition that includes the outer guard so it isn't lost
                     * when the inner SELECT overwrites select_selector/case_value.
                     */
                    IR_Expr *eq_node = calloc(1, sizeof(IR_Expr));
                    eq_node->kind = EXPR_BINARY_EQ;
                    eq_node->width = 1;
                    eq_node->u.binary.left  = (IR_Expr *)select_selector;
                    eq_node->u.binary.right = (IR_Expr *)case_value;

                    const IR_Expr *merged_cond;
                    if (cond) {
                        IR_Expr *and_node = calloc(1, sizeof(IR_Expr));
                        and_node->kind = EXPR_LOGICAL_AND;
                        and_node->width = 1;
                        and_node->u.binary.left  = (IR_Expr *)cond;
                        and_node->u.binary.right = eq_node;
                        merged_cond = and_node;
                    } else {
                        merged_cond = eq_node;
                    }
                    collect_block_mem_accesses_internal(mod, sel->cases[i].body, merged_cond,
                                                        sel->selector, sel->cases[i].case_value, list);
                } else {
                    collect_block_mem_accesses_internal(mod, sel->cases[i].body, cond,
                                                        sel->selector, sel->cases[i].case_value, list);
                }
            }
            break;
        }

        default:
            break;
    }
}

static void collect_block_mem_accesses(const IR_Module *mod, const IR_Stmt *stmt,
                                       const IR_Expr *cond, BlockMemAccessList *list)
{
    collect_block_mem_accesses_internal(mod, stmt, cond, NULL, NULL, list);
}

/* -------------------------------------------------------------------------
 * Emit sensitivity list helper
 * -------------------------------------------------------------------------
 */

static void emit_sensitivity_list(FILE *out, const IR_Module *mod, const IR_ClockDomain *cd)
{
    fprintf(out, "    always @(");
    int first = 1;
    for (int i = 0; i < cd->num_sensitivity; ++i) {
        /* Skip the raw reset signal when a synchronizer is used — the body
         * references the synchronized signal, not the raw one, so including
         * the raw reset causes a sensitivity/body mismatch that yosys
         * proc_arst cannot reconcile. */
        if (cd->reset_sync_signal_id >= 0 &&
            cd->sensitivity_list[i].signal_id == cd->reset_signal_id) {
            continue;
        }
        if (!first) fprintf(out, " or ");
        first = 0;
        const char *edge_str = (cd->sensitivity_list[i].edge == EDGE_FALLING)
                             ? "negedge" : "posedge";
        const IR_Signal *sig = find_signal_by_id(mod, cd->sensitivity_list[i].signal_id);
        fprintf(out, "%s %s", edge_str, sig && sig->name ? sig->name : "/*unknown*/");
    }
    fprintf(out, ") begin\n");
}

/* Forward declarations for helpers used by emit_bsram_read_blocks. */
static void emit_bsram_condition(FILE *out, const IR_Module *mod, BlockMemAccess *acc);
static void emit_mem_access_stmt(FILE *out, const IR_Module *mod,
                                  BlockMemAccess *acc, int indent_level);

/* -------------------------------------------------------------------------
 * Emit BSRAM read blocks (separate always blocks for BLOCK memory reads)
 * -------------------------------------------------------------------------
 */

/**
 * Find the RHS expression of an address capture assignment in the IR tree.
 *
 * Walks the statement tree looking for assignments where the LHS signal
 * matches the given addr_reg_signal_id and the RHS is NOT a literal
 * (to skip reset assignments like addr_reg <= 0). Returns the first
 * non-literal RHS found.
 */
static const IR_Expr *find_addr_capture_rhs(const IR_Stmt *stmt,
                                             int addr_reg_signal_id)
{
    if (!stmt || addr_reg_signal_id < 0) {
        return NULL;
    }

    switch (stmt->kind) {
        case STMT_ASSIGNMENT: {
            const IR_Assignment *a = &stmt->u.assign;
            /* Match assignments to the addr_reg signal with non-literal RHS.
             * Literal RHS means reset value (addr_reg <= 0), which we skip.
             */
            if (a->lhs_signal_id == addr_reg_signal_id && a->rhs &&
                a->rhs->kind != EXPR_LITERAL) {
                return a->rhs;
            }
            break;
        }

        case STMT_BLOCK: {
            const IR_BlockStmt *blk = &stmt->u.block;
            for (int i = 0; i < blk->count; ++i) {
                const IR_Expr *result = find_addr_capture_rhs(&blk->stmts[i],
                                                               addr_reg_signal_id);
                if (result) {
                    return result;
                }
            }
            break;
        }

        case STMT_IF: {
            const IR_IfStmt *ifs = &stmt->u.if_stmt;
            if (ifs->then_block) {
                const IR_Expr *result = find_addr_capture_rhs(ifs->then_block,
                                                               addr_reg_signal_id);
                if (result) {
                    return result;
                }
            }
            if (ifs->elif_chain) {
                const IR_Expr *result = find_addr_capture_rhs(ifs->elif_chain,
                                                               addr_reg_signal_id);
                if (result) {
                    return result;
                }
            }
            if (ifs->else_block) {
                const IR_Expr *result = find_addr_capture_rhs(ifs->else_block,
                                                               addr_reg_signal_id);
                if (result) {
                    return result;
                }
            }
            break;
        }

        case STMT_SELECT: {
            const IR_SelectStmt *sel = &stmt->u.select_stmt;
            for (int i = 0; i < sel->num_cases; ++i) {
                if (sel->cases[i].body) {
                    const IR_Expr *result = find_addr_capture_rhs(sel->cases[i].body,
                                                                   addr_reg_signal_id);
                    if (result) {
                        return result;
                    }
                }
            }
            break;
        }

        default:
            break;
    }

    return NULL;
}

/**
 * Emit separate always blocks for BLOCK memory SYNC read ports.
 *
 * For each BLOCK memory with a SYNC read port in this clock domain:
 *   1. Find the address capture expression (the RHS of addr_reg <= bus_addr)
 *   2. Declare an intermediate reg: {mem}_bsram_out
 *   3. Emit an always block that reads unconditionally using the bus address:
 *        always @(posedge clk) begin
 *            {mem}_bsram_out <= {mem}[{bus_addr_expr}];
 *        end
 *
 * The intermediate signal holds its value between clock edges (it's a reg),
 * so the main always block can read it one cycle later when the pipeline
 * latch fires. This avoids placing reset logic in the BSRAM read block,
 * which would prevent synthesis tools from inferring BSRAM.
 */
static void emit_bsram_read_blocks(FILE *out, const IR_Module *mod,
                                    const IR_ClockDomain *cd)
{
    if (!out || !mod || !cd || !cd->statements) {
        return;
    }

    for (int mi = 0; mi < mod->num_memories; ++mi) {
        const IR_Memory *m = &mod->memories[mi];
        if (m->kind != MEM_KIND_BLOCK) {
            continue;
        }

        const char *raw_mem_name = (m->name && m->name[0] != '\0') ? m->name : "jz_mem";
        char mem_safe_buf[256];
        const char *mem_name = verilog_memory_name(raw_mem_name, mod->name, mem_safe_buf, sizeof(mem_safe_buf));

        for (int pi = 0; pi < m->num_ports; ++pi) {
            const IR_MemoryPort *mp = &m->ports[pi];
            if (mp->kind != MEM_PORT_READ_SYNC) {
                continue;
            }
            if (mp->addr_reg_signal_id < 0) {
                continue;
            }
            /* Match by clock domain ID if set, otherwise try to find the
             * address capture in this clock domain's statement tree.
             */
            if (mp->sync_clock_domain_id >= 0 && mp->sync_clock_domain_id != cd->id) {
                continue;
            }

            /* Find the address capture expression by walking the IR tree
             * for assignments to the addr_reg signal outside reset.
             */
            const IR_Expr *addr_rhs = find_addr_capture_rhs(cd->statements,
                                                              mp->addr_reg_signal_id);
            if (!addr_rhs) {
                continue;
            }

            /* Register this memory as having a BSRAM read intermediate. */
            bsram_mappings_add(mem_name);

            /* Declare intermediate signal. */
            fprintf(out, "\n    // BSRAM read port: %s.%s (no reset - required for BSRAM inference)\n",
                    mem_name,
                    (mp->name && mp->name[0] != '\0') ? mp->name : "read");
            if (m->word_width > 1) {
                fprintf(out, "    reg [%d:0] %s_bsram_out;\n", m->word_width - 1, mem_name);
            } else {
                fprintf(out, "    reg %s_bsram_out;\n", mem_name);
            }

            /* Emit always block with unconditional read from bus address.
             * Uses a separate always block (no writes) so yosys maps to
             * $__GOWIN_SDP_ (semi-dual-port) instead of $__GOWIN_DP_.
             * This avoids a yosys bug where $__GOWIN_DP_ brams_map.v uses
             * PORT_B_WR_BE for ADA byte enables instead of PORT_A_WR_BE,
             * causing all writes to be silently discarded.
             */
            emit_sensitivity_list(out, mod, cd);
            int prev = g_in_sequential;
            g_in_sequential = 1;
            fprintf(out, "        %s_bsram_out <= %s[", mem_name, mem_name);
            emit_expr(out, mod, addr_rhs);
            fprintf(out, "];\n");

            g_in_sequential = prev;
            fprintf(out, "    end\n");
        }
    }
}

/* -------------------------------------------------------------------------
 * Emit separate BLOCK memory write port blocks
 * -------------------------------------------------------------------------
 */

/* Helper to emit the condition for a BSRAM access. */
static void emit_bsram_condition(FILE *out, const IR_Module *mod, BlockMemAccess *acc)
{
    if (acc->condition && acc->select_selector && acc->case_value) {
        /* Both an IF guard and a SELECT/CASE condition — emit both ANDed. */
        emit_expr(out, mod, acc->condition);
        fprintf(out, " && ");
        emit_expr(out, mod, acc->select_selector);
        fprintf(out, " == ");
        emit_expr(out, mod, acc->case_value);
    } else if (acc->select_selector && acc->case_value) {
        emit_expr(out, mod, acc->select_selector);
        fprintf(out, " == ");
        emit_expr(out, mod, acc->case_value);
    } else if (acc->condition) {
        emit_expr(out, mod, acc->condition);
    }
}

/* Helper to emit memory access statement. */
static void emit_mem_access_stmt(FILE *out, const IR_Module *mod,
                                  BlockMemAccess *acc, int indent_level)
{
    if (acc->is_write) {
        const IR_MemWriteStmt *mw = &acc->stmt->u.mem_write;
        const char *raw_vn = acc->mem_name ? acc->mem_name : "jz_mem";
        char vn_buf[256];
        const char *vname = verilog_memory_name(raw_vn, mod->name, vn_buf, sizeof(vn_buf));
        emit_indent(out, indent_level);
        fprintf(out, "%s[", vname);
        emit_expr(out, mod, mw->address);
        fprintf(out, "] <= ");
        emit_expr(out, mod, mw->data);
        fprintf(out, ";\n");
    } else {
        /* Temporarily disable skip flag to allow emitting the assignment. */
        int prev_skip = g_skip_block_mem_accesses;
        g_skip_block_mem_accesses = 0;
        emit_assignment_stmt(out, mod, &acc->stmt->u.assign, indent_level);
        g_skip_block_mem_accesses = prev_skip;
    }
}

static void emit_block_mem_port_blocks(FILE *out, const IR_Module *mod,
                                       const IR_ClockDomain *cd,
                                       BlockMemAccessList *list)
{
    if (!out || !mod || !cd || !list || list->count == 0) {
        return;
    }

    /* Emit separate always blocks for BLOCK memory write accesses only.
     * Read accesses remain in the main always block to preserve the user's
     * pipeline timing (reads share the same always block with reset and
     * other sequential logic).
     */
    for (int i = 0; i < list->count; ++i) {
        BlockMemAccess *acc = &list->items[i];
        if (!acc->is_write) continue;

        fprintf(out, "\n    // BSRAM write port: %s.%s\n",
                acc->mem_name ? acc->mem_name : "mem",
                acc->port_name ? acc->port_name : "write");

        emit_sensitivity_list(out, mod, cd);

        int prev = g_in_sequential;
        g_in_sequential = 1;

        int has_condition = (acc->select_selector && acc->case_value) || acc->condition;

        if (has_condition) {
            fprintf(out, "        if (");
            emit_bsram_condition(out, mod, acc);
            fprintf(out, ") begin\n");
            emit_mem_access_stmt(out, mod, acc, 3);
            fprintf(out, "        end\n");
        } else {
            emit_mem_access_stmt(out, mod, acc, 2);
        }

        g_in_sequential = prev;
        fprintf(out, "    end\n");
    }
}

/* -------------------------------------------------------------------------
 * Async block helpers
 * -------------------------------------------------------------------------
 */

/* Return non-zero if the assignment targets a PORT_INOUT signal. */
static int assignment_targets_inout_port(const IR_Module *mod,
                                          const IR_Assignment *a)
{
    if (!mod || !a) return 0;
    const IR_Signal *lhs = find_signal_by_id_raw(mod, a->lhs_signal_id);
    return (lhs && lhs->kind == SIG_PORT &&
            lhs->u.port.direction == PORT_INOUT);
}

static int async_block_has_non_alias_logic(const IR_Stmt *async_block)
{
    if (!async_block || async_block->kind != STMT_BLOCK) {
        return 0;
    }
    const IR_BlockStmt *blk = &async_block->u.block;
    for (int i = 0; i < blk->count; ++i) {
        const IR_Stmt *child = &blk->stmts[i];
        if (!child) {
            continue;
        }
        if (child->kind != STMT_ASSIGNMENT) {
            return 1;
        }
        const IR_Assignment *a = &child->u.assign;
        if (!assignment_kind_is_alias(a->kind)) {
            return 1;
        }
    }
    return 0;
}

/* -------------------------------------------------------------------------
 * Async block emission
 * -------------------------------------------------------------------------
 */

/* Emit INOUT port assignments from the async block as continuous assign
 * statements.  INOUT ports cannot be declared 'reg' in Verilog, so they
 * must use continuous assign rather than procedural assignment inside an
 * always @* block.
 */
static void emit_inout_continuous_assigns(FILE *out, const IR_Module *mod)
{
    if (!mod || !mod->async_block ||
        mod->async_block->kind != STMT_BLOCK) {
        return;
    }

    const IR_BlockStmt *blk = &mod->async_block->u.block;
    for (int i = 0; i < blk->count; ++i) {
        const IR_Stmt *child = &blk->stmts[i];
        if (!child || child->kind != STMT_ASSIGNMENT) {
            continue;
        }
        const IR_Assignment *a = &child->u.assign;
        if (assignment_kind_is_alias(a->kind)) {
            continue;
        }
        if (!assignment_targets_inout_port(mod, a)) {
            continue;
        }

        const IR_Signal *lhs_sig = find_signal_by_id(mod, a->lhs_signal_id);
        if (!lhs_sig || !lhs_sig->name) {
            continue;
        }

        emit_indent(out, 1);
        fputs("assign ", out);
        {
            char esc[256];
            fputs(verilog_safe_name(lhs_sig->name, esc, (int)sizeof(esc)), out);
        }
        if (a->is_sliced) {
            if (a->lhs_msb == a->lhs_lsb) {
                fprintf(out, "[%d]", a->lhs_msb);
            } else {
                fprintf(out, "[%d:%d]", a->lhs_msb, a->lhs_lsb);
            }
        }
        fputs(" = ", out);
        if (a->rhs) {
            emit_expr(out, mod, a->rhs);
        } else {
            fprintf(out, "1'b0");
        }
        fputs(";\n", out);
    }
}

void emit_async_block(FILE *out, const IR_Module *mod)
{
    if (!mod || !mod->async_block) {
        return;
    }

    if (!async_block_has_non_alias_logic(mod->async_block)) {
        return;
    }

    /* Emit INOUT port assignments as continuous assigns (outside always). */
    emit_inout_continuous_assigns(out, mod);

    /* Emit remaining async logic in always @* block, skipping INOUT. */
    g_skip_inout_port_assigns = 1;
    fprintf(out, "\n    always @* begin\n");
    emit_block_stmt(out, mod, mod->async_block, 2, 1);
    fprintf(out, "    end\n");
    g_skip_inout_port_assigns = 0;
}

/* -------------------------------------------------------------------------
 * Clock domain emission
 * -------------------------------------------------------------------------
 */

void emit_clock_domain(FILE *out,
                       const IR_Module *mod,
                       const IR_ClockDomain *cd)
{
    if (!mod || !cd) {
        return;
    }

    bsram_mappings_clear();

    /* Use the pre-built sensitivity list from the IR. */
    if (cd->num_sensitivity <= 0 || !cd->sensitivity_list) {
        const IR_Signal *clk = find_signal_by_id(mod, cd->clock_signal_id);
        fprintf(out, "    /* clock domain %d: no sensitivity list */\n",
                cd->id);
        (void)clk;
        return;
    }

    /* Collect BLOCK memory accesses for separate emission (BSRAM inference). */
    BlockMemAccessList block_mem_accesses;
    access_list_init(&block_mem_accesses);
    if (cd->statements) {
        collect_block_mem_accesses(mod, cd->statements, NULL, &block_mem_accesses);
    }

    /* Check if this clock domain has any BLOCK memories that need BSRAM
     * read intermediates (even if no accesses were collected, e.g., read-only
     * memories with NULL-RHS port binding resolution).
     */
    int has_block_mem_sync_reads = 0;
    for (int mi = 0; mi < mod->num_memories && !has_block_mem_sync_reads; ++mi) {
        const IR_Memory *m = &mod->memories[mi];
        if (m->kind != MEM_KIND_BLOCK) continue;
        for (int pi = 0; pi < m->num_ports; ++pi) {
            if (m->ports[pi].kind == MEM_PORT_READ_SYNC &&
                m->ports[pi].addr_reg_signal_id >= 0) {
                has_block_mem_sync_reads = 1;
                break;
            }
        }
    }

    /* Emit separate always blocks for BLOCK memory port accesses. */
    if (has_block_mem_sync_reads) {
        /* Try to emit BSRAM read blocks (intermediate signals for SYNC read ports).
         * This may not emit anything if address captures can't be found.
         */
        emit_bsram_read_blocks(out, mod, cd);
    }
    if (block_mem_accesses.count > 0) {
        /* Emit BSRAM write blocks. */
        emit_block_mem_port_blocks(out, mod, cd, &block_mem_accesses);
    }
    int has_bsram_blocks = s_bsram_read_mapping_count > 0 || block_mem_accesses.count > 0;
    if (has_bsram_blocks) {
        fprintf(out, "\n    // Main logic (BLOCK memory ports in separate blocks above)\n");
    }

    emit_sensitivity_list(out, mod, cd);

    if (cd->statements) {
        int prev_seq = g_in_sequential;
        int prev_skip = g_skip_block_mem_accesses;
        g_in_sequential = 1;
        /* Enable skipping/substitution of BLOCK memory accesses in main block. */
        g_skip_block_mem_accesses = has_bsram_blocks ? 1 : 0;
        emit_stmt(out, mod, cd->statements, 2);
        g_in_sequential = prev_seq;
        g_skip_block_mem_accesses = prev_skip;
    }

    fprintf(out, "    end\n");

    access_list_free(&block_mem_accesses);

    /* Emit implicit SYNC read address capture always blocks for this clock domain.
     * These are only needed for SYNC read ports without explicit addr_reg_signal_id.
     */
    for (int mi = 0; mi < mod->num_memories; ++mi) {
        const IR_Memory *m = &mod->memories[mi];
        for (int pi = 0; pi < m->num_ports; ++pi) {
            const IR_MemoryPort *mp = &m->ports[pi];
            if (mp->kind != MEM_PORT_READ_SYNC || mp->addr_signal_id < 0) {
                continue;
            }
            /* Skip ports with synthetic addr register signals; these are
             * handled inside the clock domain's statement tree instead.
             */
            if (mp->addr_reg_signal_id >= 0) {
                continue;
            }
            if (mp->sync_clock_domain_id != cd->id) {
                continue;
            }

            const char *raw_mem_name2 = (m->name && m->name[0] != '\0') ? m->name : "jz_mem";
            char mem_safe_buf2[256];
            const char *mem_name = verilog_memory_name(raw_mem_name2, mod->name, mem_safe_buf2, sizeof(mem_safe_buf2));
            const char *port_name = (mp->name && mp->name[0] != '\0') ? mp->name : "rd";
            const IR_Signal *addr_sig = find_signal_by_id(mod, mp->addr_signal_id);
            if (!addr_sig || !addr_sig->name) {
                continue;
            }

            int addr_w = mp->address_width > 0 ? mp->address_width : m->address_width;

            /* Use the same sensitivity list as the parent clock domain. */
            fprintf(out, "\n");
            emit_sensitivity_list(out, mod, cd);

            /* Reset logic if the clock domain has a reset.
             * Use synchronized reset signal for body guard when available. */
            if (cd->reset_signal_id >= 0) {
                int body_rst_id = (cd->reset_sync_signal_id >= 0)
                                ? cd->reset_sync_signal_id : cd->reset_signal_id;
                const IR_Signal *rst_sig = find_signal_by_id(mod, body_rst_id);
                const char *rst_name = (rst_sig && rst_sig->name) ? rst_sig->name : "rst";
                if (cd->reset_active == RESET_ACTIVE_LOW) {
                    fprintf(out, "        if (!%s) begin\n", rst_name);
                } else {
                    fprintf(out, "        if (%s) begin\n", rst_name);
                }
                fprintf(out, "            %s_%s_addr <= %d'b0;\n",
                        mem_name, port_name, addr_w);
                fprintf(out, "        end else begin\n");
                fprintf(out, "            %s_%s_addr <= %s;\n",
                        mem_name, port_name, addr_sig->name);
                fprintf(out, "        end\n");
            } else {
                fprintf(out, "        %s_%s_addr <= %s;\n",
                        mem_name, port_name, addr_sig->name);
            }

            fprintf(out, "    end\n");
        }
    }

}

void emit_clock_domains(FILE *out, const IR_Module *mod)
{
    if (!mod || mod->num_clock_domains <= 0) {
        return;
    }
    for (int i = 0; i < mod->num_clock_domains; ++i) {
        if (i != 0) fputc('\n', out);
        emit_clock_domain(out, mod, &mod->clock_domains[i]);
    }
}
