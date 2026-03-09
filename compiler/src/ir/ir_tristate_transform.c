/**
 * @file ir_tristate_transform.c
 * @brief IR transform pass: eliminate tri-state (z) for FPGA targets.
 *
 * When --tristate-default is specified, this
 * pass eliminates high-impedance values from internal nets so the design
 * can be synthesized on FPGAs that lack internal tri-state buffers.
 *
 * The transform operates at the project level:
 *
 * 1. Identify shared tri-state nets: parent-module wires driven by
 *    multiple child instance INOUT/OUT ports.
 *
 * 2. For each such net, split each driving child port into separate
 *    _out (data) and _oe (output-enable) signals. The child's
 *    z-guarded assignment  "port <= cond ? data : z"  becomes
 *    "port_out <= data; port_oe <= cond;", preserving the existing
 *    statement structure for non-z paths.
 *
 * 3. In the parent module, build a single priority-chained mux:
 *      net <= inst0_oe ? inst0_out : inst1_oe ? inst1_out : ... : default
 *    where default is GND (all-0) or VCC (all-1).
 *
 * 4. Update instance connections so each child's split ports connect
 *    to the per-instance wires in the parent.
 *
 * Signals that are NOT shared across instances (single-driver internal
 * nets) have their z literals replaced with the default value in place.
 *
 * INOUT ports on the top module bound to physical INOUT_PINS are
 * excluded—they correspond to bidirectional pads.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "../../include/ir.h"
#include "../../include/ir_builder.h"
#include "../../include/arena.h"
#include "../../include/diagnostic.h"
#include "ir_internal.h"

/* ======================================================================== */
/*  Small helpers                                                            */
/* ======================================================================== */

static const char *module_source_file(const IR_Design *design, const IR_Module *mod)
{
    if (mod->source_file_id >= 0 && mod->source_file_id < design->num_source_files)
        return design->source_files[mod->source_file_id].path;
    return NULL;
}

static IR_Signal *find_signal(IR_Module *mod, int signal_id)
{
    for (int i = 0; i < mod->num_signals; i++)
        if (mod->signals[i].id == signal_id) return &mod->signals[i];
    return NULL;
}

static const IR_Signal *find_signal_const(const IR_Module *mod, int signal_id)
{
    for (int i = 0; i < mod->num_signals; i++)
        if (mod->signals[i].id == signal_id) return &mod->signals[i];
    return NULL;
}

static bool is_top_inout_pin(const IR_Design *design,
                              int module_id,
                              int signal_id)
{
    if (!design->project) return false;
    if (module_id != design->project->top_module_id) return false;
    const IR_Module *mod = &design->modules[module_id];
    const IR_Signal *sig = find_signal_const(mod, signal_id);
    if (!sig) return false;
    if (sig->kind != SIG_PORT || sig->u.port.direction != PORT_INOUT) return false;
    for (int i = 0; i < design->project->num_top_bindings; i++) {
        IR_TopBinding *b = &design->project->top_bindings[i];
        if (b->top_port_signal_id == signal_id && b->pin_id >= 0) {
            if (design->project->pins[b->pin_id].kind == PIN_INOUT) return true;
        }
    }
    return false;
}

static uint64_t default_value_for_width(IR_TristateDefault mode, int width)
{
    if (mode == TRISTATE_DEFAULT_GND) return 0;
    if (width >= 64) return UINT64_MAX;
    return ((uint64_t)1 << width) - 1;
}

/* ======================================================================== */
/*  Port alias group detection (union-find)                                  */
/* ======================================================================== */

static int uf_find(int *parent, int i)
{
    while (parent[i] != i) {
        parent[i] = parent[parent[i]];
        i = parent[i];
    }
    return i;
}

static void uf_union(int *parent, int *rank, int a, int b)
{
    int ra = uf_find(parent, a);
    int rb = uf_find(parent, b);
    if (ra == rb) return;
    if (rank[ra] < rank[rb]) {
        parent[ra] = rb;
    } else if (rank[ra] > rank[rb]) {
        parent[rb] = ra;
    } else {
        parent[rb] = ra;
        rank[ra]++;
    }
}

/**
 * Find the index (into mod->signals[]) for a given signal ID.
 * Returns -1 if not found.
 */
static int find_signal_index(const IR_Module *mod, int signal_id)
{
    for (int i = 0; i < mod->num_signals; i++)
        if (mod->signals[i].id == signal_id) return i;
    return -1;
}

/**
 * Recursively collect all signal indices referenced in an expression tree.
 * Only collects PORT and NET signals (skips REGISTER).
 * Used to find all port signals that an ASSIGN_ALIAS RHS references,
 * even through complex expressions like gslice(concat(s1, s0), oh2b(...)).
 */
static void collect_signal_refs_from_expr(const IR_Module *mod,
                                            const IR_Expr *expr,
                                            int *out_indices, int *out_count,
                                            int max_count)
{
    if (!expr || *out_count >= max_count) return;
    switch (expr->kind) {
    case EXPR_SIGNAL_REF: {
        int idx = find_signal_index(mod, expr->u.signal_ref.signal_id);
        if (idx >= 0) {
            const IR_Signal *sig = &mod->signals[idx];
            if (sig->kind != SIG_REGISTER) {
                /* Avoid duplicates. */
                for (int i = 0; i < *out_count; i++)
                    if (out_indices[i] == idx) return;
                if (*out_count < max_count)
                    out_indices[(*out_count)++] = idx;
            }
        }
        break;
    }
    case EXPR_UNARY_NOT: case EXPR_UNARY_NEG: case EXPR_LOGICAL_NOT:
        collect_signal_refs_from_expr(mod, expr->u.unary.operand,
                                       out_indices, out_count, max_count);
        break;
    case EXPR_BINARY_ADD: case EXPR_BINARY_SUB: case EXPR_BINARY_MUL:
    case EXPR_BINARY_DIV: case EXPR_BINARY_MOD:
    case EXPR_BINARY_AND: case EXPR_BINARY_OR:  case EXPR_BINARY_XOR:
    case EXPR_BINARY_SHL: case EXPR_BINARY_SHR: case EXPR_BINARY_ASHR:
    case EXPR_BINARY_EQ:  case EXPR_BINARY_NEQ:
    case EXPR_BINARY_LT:  case EXPR_BINARY_GT:
    case EXPR_BINARY_LTE: case EXPR_BINARY_GTE:
    case EXPR_LOGICAL_AND: case EXPR_LOGICAL_OR:
        collect_signal_refs_from_expr(mod, expr->u.binary.left,
                                       out_indices, out_count, max_count);
        collect_signal_refs_from_expr(mod, expr->u.binary.right,
                                       out_indices, out_count, max_count);
        break;
    case EXPR_TERNARY:
        collect_signal_refs_from_expr(mod, expr->u.ternary.condition,
                                       out_indices, out_count, max_count);
        collect_signal_refs_from_expr(mod, expr->u.ternary.true_val,
                                       out_indices, out_count, max_count);
        collect_signal_refs_from_expr(mod, expr->u.ternary.false_val,
                                       out_indices, out_count, max_count);
        break;
    case EXPR_CONCAT:
        for (int i = 0; i < expr->u.concat.num_operands; i++)
            collect_signal_refs_from_expr(mod, expr->u.concat.operands[i],
                                           out_indices, out_count, max_count);
        break;
    case EXPR_SLICE:
        if (expr->u.slice.base_expr) {
            collect_signal_refs_from_expr(mod, expr->u.slice.base_expr,
                                           out_indices, out_count, max_count);
        } else {
            /* Simple slice like signal[msb:lsb] — treat as signal ref. */
            int idx = find_signal_index(mod, expr->u.slice.signal_id);
            if (idx >= 0) {
                const IR_Signal *sig = &mod->signals[idx];
                if (sig->kind != SIG_REGISTER) {
                    int dup = 0;
                    for (int i = 0; i < *out_count; i++)
                        if (out_indices[i] == idx) { dup = 1; break; }
                    if (!dup && *out_count < max_count)
                        out_indices[(*out_count)++] = idx;
                }
            }
        }
        break;
    case EXPR_INTRINSIC_UADD: case EXPR_INTRINSIC_SADD:
    case EXPR_INTRINSIC_UMUL: case EXPR_INTRINSIC_SMUL:
    case EXPR_INTRINSIC_GBIT: case EXPR_INTRINSIC_SBIT:
    case EXPR_INTRINSIC_GSLICE: case EXPR_INTRINSIC_SSLICE:
    case EXPR_INTRINSIC_OH2B:
    case EXPR_INTRINSIC_B2OH: case EXPR_INTRINSIC_PRIENC:
    case EXPR_INTRINSIC_LZC: case EXPR_INTRINSIC_ABS:
    case EXPR_INTRINSIC_POPCOUNT: case EXPR_INTRINSIC_REVERSE:
    case EXPR_INTRINSIC_BSWAP:
    case EXPR_INTRINSIC_REDUCE_AND: case EXPR_INTRINSIC_REDUCE_OR:
    case EXPR_INTRINSIC_REDUCE_XOR:
        collect_signal_refs_from_expr(mod, expr->u.intrinsic.source,
                                       out_indices, out_count, max_count);
        collect_signal_refs_from_expr(mod, expr->u.intrinsic.index,
                                       out_indices, out_count, max_count);
        collect_signal_refs_from_expr(mod, expr->u.intrinsic.value,
                                       out_indices, out_count, max_count);
        break;
    case EXPR_INTRINSIC_USUB: case EXPR_INTRINSIC_SSUB:
    case EXPR_INTRINSIC_UMIN: case EXPR_INTRINSIC_UMAX:
    case EXPR_INTRINSIC_SMIN: case EXPR_INTRINSIC_SMAX:
        collect_signal_refs_from_expr(mod, expr->u.binary.left,
                                       out_indices, out_count, max_count);
        collect_signal_refs_from_expr(mod, expr->u.binary.right,
                                       out_indices, out_count, max_count);
        break;
    case EXPR_MEM_READ:
        collect_signal_refs_from_expr(mod, expr->u.mem_read.address,
                                       out_indices, out_count, max_count);
        break;
    default:
        break;
    }
}

/**
 * Walk statement tree collecting ASSIGN_ALIAS unions between signals.
 *
 * For simple signal-ref RHS (e.g., tgt0_DATA = src_data), unions the
 * LHS and RHS signal indices directly.
 *
 * For complex RHS expressions (e.g., tgt5_DATA = gslice(concat(src1_DATA,
 * src0_DATA), oh2b(active_src), 16)), walks the expression tree to find
 * all referenced PORT/NET signals and unions them with the LHS.  This
 * handles template-expanded aliases where the RHS is a gslice/oh2b
 * expression that selects from concatenated source ports.
 */
static void collect_alias_unions(const IR_Module *mod,
                                  const IR_Stmt *stmt,
                                  int *parent, int *rank)
{
    if (!stmt) return;
    switch (stmt->kind) {
    case STMT_ASSIGNMENT: {
        const IR_Assignment *a = &stmt->u.assign;
        if (a->kind != ASSIGN_ALIAS && a->kind != ASSIGN_ALIAS_ZEXT &&
            a->kind != ASSIGN_ALIAS_SEXT)
            break;
        if (a->is_sliced || !a->rhs) break;

        int lhs_idx = find_signal_index(mod, a->lhs_signal_id);
        if (lhs_idx < 0) break;
        const IR_Signal *lhs_sig = &mod->signals[lhs_idx];
        if (lhs_sig->kind == SIG_REGISTER || lhs_sig->width <= 0) break;

        if (a->rhs->kind == EXPR_SIGNAL_REF) {
            /* Simple case: direct signal reference. */
            int rhs_idx = find_signal_index(mod, a->rhs->u.signal_ref.signal_id);
            if (rhs_idx < 0) break;
            const IR_Signal *rhs_sig = &mod->signals[rhs_idx];
            if (rhs_sig->kind == SIG_REGISTER) break;
            if (lhs_sig->width != rhs_sig->width) break;
            uf_union(parent, rank, lhs_idx, rhs_idx);
        } else {
            /* Complex RHS: walk expression tree to find all referenced
             * PORT signals and union them with the LHS.  This handles
             * template-expanded aliases like:
             *   tgt5_DATA = gslice(concat(src1_DATA, src0_DATA), oh2b(...), 16)
             */
            int refs[64];
            int ref_count = 0;
            collect_signal_refs_from_expr(mod, a->rhs, refs, &ref_count, 64);
            for (int r = 0; r < ref_count; r++) {
                const IR_Signal *ref_sig = &mod->signals[refs[r]];
                /* Only union with PORT signals — NET intermediaries
                 * (like scratch wires) shouldn't form alias groups
                 * with ports. */
                if (ref_sig->kind == SIG_PORT)
                    uf_union(parent, rank, lhs_idx, refs[r]);
            }
        }
        break;
    }
    case STMT_BLOCK:
        for (int i = 0; i < stmt->u.block.count; i++)
            collect_alias_unions(mod, &stmt->u.block.stmts[i], parent, rank);
        break;
    case STMT_IF:
        collect_alias_unions(mod, stmt->u.if_stmt.then_block, parent, rank);
        collect_alias_unions(mod, stmt->u.if_stmt.elif_chain, parent, rank);
        collect_alias_unions(mod, stmt->u.if_stmt.else_block, parent, rank);
        break;
    case STMT_SELECT:
        for (int i = 0; i < stmt->u.select_stmt.num_cases; i++)
            collect_alias_unions(mod, stmt->u.select_stmt.cases[i].body, parent, rank);
        break;
    default:
        break;
    }
}

/**
 * Build port alias groups for a module.
 *
 * Uses union-find on the module's async_block to find groups of 2+ PORT
 * signals that are aliased together (possibly through intermediate NET
 * signals like a shared wire).
 *
 * Populates mod->port_alias_groups and mod->num_port_alias_groups.
 */
static void ir_build_port_alias_groups(IR_Module *mod, JZArena *arena)
{
    mod->port_alias_groups = NULL;
    mod->num_port_alias_groups = 0;

    if (!mod->async_block || mod->num_signals <= 0) return;

    int n = mod->num_signals;
    int *parent = (int *)malloc((size_t)n * sizeof(int));
    int *rank   = (int *)calloc((size_t)n, sizeof(int));
    if (!parent || !rank) { free(parent); free(rank); return; }
    for (int i = 0; i < n; i++) parent[i] = i;

    collect_alias_unions(mod, mod->async_block, parent, rank);

    /* Count groups that contain 2+ PORT signals. */
    /* First, gather the root of each signal. */
    int num_groups = 0;

    /* For each root, count how many PORT signals are in the group. */
    int *port_count_by_root = (int *)calloc((size_t)n, sizeof(int));
    if (!port_count_by_root) { free(parent); free(rank); return; }

    for (int i = 0; i < n; i++) {
        int r = uf_find(parent, i);
        if (mod->signals[i].kind == SIG_PORT)
            port_count_by_root[r]++;
    }

    /* Count how many roots have 2+ ports. */
    for (int i = 0; i < n; i++) {
        if (port_count_by_root[i] >= 2)
            num_groups++;
    }

    if (num_groups == 0) {
        free(parent); free(rank); free(port_count_by_root);
        return;
    }

    /* Allocate groups. */
    IR_PortAliasGroup *groups = (IR_PortAliasGroup *)jz_arena_alloc(
        arena, (size_t)num_groups * sizeof(IR_PortAliasGroup));
    if (!groups) { free(parent); free(rank); free(port_count_by_root); return; }
    memset(groups, 0, (size_t)num_groups * sizeof(IR_PortAliasGroup));

    int gi = 0;
    for (int root = 0; root < n && gi < num_groups; root++) {
        int pc = port_count_by_root[root];
        if (pc < 2) continue;

        IR_PortAliasGroup *g = &groups[gi++];
        g->count = pc;
        g->port_ids = (int *)jz_arena_alloc(arena, (size_t)pc * sizeof(int));
        if (!g->port_ids) { g->count = 0; continue; }

        int pi = 0;
        for (int i = 0; i < n && pi < pc; i++) {
            if (uf_find(parent, i) == root && mod->signals[i].kind == SIG_PORT) {
                g->port_ids[pi++] = mod->signals[i].id;
            }
        }
        g->count = pi;
    }

    mod->port_alias_groups = groups;
    mod->num_port_alias_groups = gi;

    free(parent);
    free(rank);
    free(port_count_by_root);
}

/* ======================================================================== */
/*  Shared-net detection                                                     */
/* ======================================================================== */

/**
 * A record of one instance port that drives a parent signal and can
 * produce z (making it a tri-state driver).
 */
typedef struct {
    int inst_idx;        /* Index into parent module's instances array. */
    int conn_idx;        /* Index into instance's connections array. */
    int child_port_id;   /* Signal ID of the INOUT/OUT port in child. */
    int child_module_id; /* ID of the child module. */
    int parent_msb;      /* Slice bounds on the parent signal. */
    int parent_lsb;
} SharedDriver;

/**
 * A parent signal that is driven by 2+ instance ports with z capability.
 */
typedef struct {
    int           parent_signal_id;
    int           width;            /* Width of the driven slice. */
    int           parent_msb;
    int           parent_lsb;
    SharedDriver *drivers;
    int           num_drivers;
    int           driver_cap;

    /* Fields for merged alias-connected nets. */
    int          *merged_parent_ids;    /* Additional parent signal IDs. */
    int           num_merged_parents;
    int          *passthrough_driver;   /* Per-driver flag: 1 = pass-through (not a real driver). */
    int           merged;               /* Non-zero if this net has been merged into another. */
} SharedNet;

static void shared_net_free(SharedNet *nets, int count)
{
    for (int i = 0; i < count; i++) {
        free(nets[i].drivers);
        free(nets[i].merged_parent_ids);
        free(nets[i].passthrough_driver);
    }
    free(nets);
}

/**
 * For a parent module, find all signals that are driven by 2+ instance
 * ports where the child port can produce z.
 */
static SharedNet *find_shared_tristate_nets(const IR_Design *design,
                                              const IR_Module *parent,
                                              int *out_count)
{
    *out_count = 0;

    /* Temporary: for each (parent_signal_id, msb, lsb) tuple, collect drivers. */
    SharedNet *nets = NULL;
    int net_count = 0;
    int net_cap = 0;

    for (int i = 0; i < parent->num_instances; i++) {
        const IR_Instance *inst = &parent->instances[i];
        if (inst->child_module_id < 0 || inst->child_module_id >= design->num_modules)
            continue;
        const IR_Module *child = &design->modules[inst->child_module_id];

        for (int c = 0; c < inst->num_connections; c++) {
            const IR_InstanceConnection *conn = &inst->connections[c];
            if (conn->parent_signal_id < 0) continue;

            const IR_Signal *child_port = find_signal_const(child, conn->child_port_id);
            if (!child_port) continue;
            if (child_port->kind != SIG_PORT) continue;
            if (child_port->u.port.direction != PORT_INOUT &&
                child_port->u.port.direction != PORT_OUT) continue;
            /* Skip top-module INOUT pins. */
            if (is_top_inout_pin(design, parent->id, conn->parent_signal_id))
                continue;

            /* Find or create the SharedNet entry for this (signal, msb, lsb). */
            SharedNet *net = NULL;
            for (int n = 0; n < net_count; n++) {
                if (nets[n].parent_signal_id == conn->parent_signal_id &&
                    nets[n].parent_msb == conn->parent_msb &&
                    nets[n].parent_lsb == conn->parent_lsb) {
                    net = &nets[n];
                    break;
                }
            }
            if (!net) {
                if (net_count >= net_cap) {
                    net_cap = (net_cap == 0) ? 8 : (net_cap * 2);
                    SharedNet *tmp = (SharedNet *)realloc(nets, (size_t)net_cap * sizeof(SharedNet));
                    if (!tmp) { shared_net_free(nets, net_count); *out_count = 0; return NULL; }
                    nets = tmp;
                }
                net = &nets[net_count++];
                memset(net, 0, sizeof(*net));
                net->parent_signal_id = conn->parent_signal_id;
                net->parent_msb = conn->parent_msb;
                net->parent_lsb = conn->parent_lsb;
                net->width = child_port->width;
            }

            /* Add this driver. */
            if (net->num_drivers >= net->driver_cap) {
                net->driver_cap = (net->driver_cap == 0) ? 4 : (net->driver_cap * 2);
                SharedDriver *tmp = (SharedDriver *)realloc(
                    net->drivers, (size_t)net->driver_cap * sizeof(SharedDriver));
                if (!tmp) { shared_net_free(nets, net_count); *out_count = 0; return NULL; }
                net->drivers = tmp;
            }
            SharedDriver *d = &net->drivers[net->num_drivers++];
            d->inst_idx = i;
            d->conn_idx = c;
            d->child_port_id = conn->child_port_id;
            d->child_module_id = inst->child_module_id;
            d->parent_msb = conn->parent_msb;
            d->parent_lsb = conn->parent_lsb;
        }
    }

    /* Filter: only keep nets with 2+ drivers. */
    int write_idx = 0;
    for (int i = 0; i < net_count; i++) {
        if (nets[i].num_drivers >= 2) {
            nets[write_idx++] = nets[i];
        } else {
            free(nets[i].drivers);
        }
    }
    *out_count = write_idx;
    return nets;
}

/* ======================================================================== */
/*  Merge alias-connected shared nets                                        */
/* ======================================================================== */

/**
 * Check whether child_port_id belongs to a port alias group in child_mod.
 * If so, return the alias group; otherwise return NULL.
 */
static const IR_PortAliasGroup *find_alias_group_for_port(
    const IR_Module *child_mod, int child_port_id)
{
    for (int g = 0; g < child_mod->num_port_alias_groups; g++) {
        const IR_PortAliasGroup *grp = &child_mod->port_alias_groups[g];
        for (int p = 0; p < grp->count; p++) {
            if (grp->port_ids[p] == child_port_id)
                return grp;
        }
    }
    return NULL;
}

/**
 * For an instance, find the parent_signal_id connected to a given child_port_id.
 * Returns -1 if not found.
 */
static int find_parent_signal_for_child_port(const IR_Instance *inst,
                                               int child_port_id)
{
    for (int c = 0; c < inst->num_connections; c++) {
        if (inst->connections[c].child_port_id == child_port_id)
            return inst->connections[c].parent_signal_id;
    }
    return -1;
}

/**
 * Find the SharedNet index for a given parent_signal_id.
 * Returns -1 if not found.
 */
static int find_shared_net_for_signal(SharedNet *nets, int count,
                                        int parent_signal_id,
                                        int msb, int lsb)
{
    for (int i = 0; i < count; i++) {
        if (nets[i].parent_signal_id == parent_signal_id && !nets[i].merged &&
            nets[i].parent_msb == msb && nets[i].parent_lsb == lsb)
            return i;
        /* Also check merged_parent_ids.  Merged parents share the same
         * slice as the primary net. */
        for (int j = 0; j < nets[i].num_merged_parents; j++) {
            if (nets[i].merged_parent_ids[j] == parent_signal_id && !nets[i].merged)
                return i;
        }
    }
    return -1;
}

/**
 * Check if a signal is a dedup sink (created by Phase 0.5 to absorb
 * redundant PORT_OUT drivers).  These should NOT be added as merged
 * parents because they're narrow dummy wires, not real bus signals.
 */
static bool is_dup_sink_signal(const IR_Module *mod, int signal_id)
{
    const IR_Signal *sig = find_signal_const(mod, signal_id);
    if (!sig || !sig->name) return false;
    const char *suffix = strstr(sig->name, "_dup_sink");
    return suffix != NULL && suffix[9] == '\0';
}

/**
 * Add a parent signal ID to the merged_parent_ids list of a SharedNet.
 */
static int add_merged_parent(SharedNet *net, int parent_signal_id)
{
    /* Check if already present. */
    if (parent_signal_id == net->parent_signal_id) return 0;
    for (int i = 0; i < net->num_merged_parents; i++) {
        if (net->merged_parent_ids[i] == parent_signal_id) return 0;
    }
    int *tmp = (int *)realloc(net->merged_parent_ids,
                               (size_t)(net->num_merged_parents + 1) * sizeof(int));
    if (!tmp) return -1;
    net->merged_parent_ids = tmp;
    net->merged_parent_ids[net->num_merged_parents++] = parent_signal_id;
    return 0;
}

/**
 * Add a driver to a SharedNet, growing the array as needed.
 * Returns 0 on success, -1 on failure.
 */
static int add_driver_to_net(SharedNet *net, const SharedDriver *drv)
{
    if (net->num_drivers >= net->driver_cap) {
        net->driver_cap = (net->driver_cap == 0) ? 4 : (net->driver_cap * 2);
        SharedDriver *tmp = (SharedDriver *)realloc(
            net->drivers, (size_t)net->driver_cap * sizeof(SharedDriver));
        if (!tmp) return -1;
        net->drivers = tmp;
    }
    net->drivers[net->num_drivers++] = *drv;
    return 0;
}

/**
 * Ensure the passthrough_driver array is large enough for num_drivers entries.
 * Newly added entries default to 0.
 */
static int ensure_passthrough_array(SharedNet *net)
{
    if (!net->passthrough_driver) {
        net->passthrough_driver = (int *)calloc((size_t)net->num_drivers, sizeof(int));
        if (!net->passthrough_driver) return -1;
    } else {
        int *tmp = (int *)realloc(net->passthrough_driver,
                                    (size_t)net->num_drivers * sizeof(int));
        if (!tmp) return -1;
        net->passthrough_driver = tmp;
    }
    return 0;
}

/**
 * After finding initial SharedNets (with 2+ drivers each on their own
 * parent signals), merge nets that are connected through a pass-through
 * instance's port alias groups.
 *
 * For example, if memory_map has ports src.DATA, tgt0.DATA, tgt1.DATA all
 * aliased together, and the SOC has:
 *   - SharedNet for cpu_bus_DATA with drivers {memory_map, cpu0}
 *   - SharedNet for rom_bus_DATA with drivers {memory_map, rom0}
 * then we merge them into one SharedNet with drivers {cpu0, rom0, ...}
 * and mark memory_map's drivers as pass-through.
 *
 * This function also handles the case where a parent signal has only 1
 * driver (not yet a SharedNet) but needs to be included because it connects
 * to a pass-through port that's aliased with other shared nets.
 */
static void merge_alias_connected_nets(const IR_Design *design,
                                         const IR_Module *parent,
                                         SharedNet **nets_ptr,
                                         int *net_count_ptr)
{
    SharedNet *nets = *nets_ptr;
    int net_count = *net_count_ptr;

    /* For each shared net, check each driver to see if it has aliased ports. */
    for (int n = 0; n < net_count; n++) {
        SharedNet *net = &nets[n];
        if (net->merged) continue;

        for (int d = 0; d < net->num_drivers; d++) {
            SharedDriver *drv = &net->drivers[d];
            const IR_Module *child = &design->modules[drv->child_module_id];
            const IR_Instance *inst = &parent->instances[drv->inst_idx];

            const IR_PortAliasGroup *grp = find_alias_group_for_port(
                child, drv->child_port_id);
            if (!grp) continue;

            /* Save driver fields before the merge loop — add_driver_to_net
             * may realloc net->drivers, invalidating the `drv` pointer. */
            int drv_inst_idx = drv->inst_idx;
            int drv_child_port_id = drv->child_port_id;

            /* This driver's child port is in an alias group. Find the
             * parent signals connected to the OTHER ports in the group. */
            for (int p = 0; p < grp->count; p++) {
                int aliased_port_id = grp->port_ids[p];
                if (aliased_port_id == drv_child_port_id) continue;

                int other_parent_sig = find_parent_signal_for_child_port(
                    inst, aliased_port_id);
                if (other_parent_sig < 0) continue;

                /* Skip top-module INOUT pins. */
                if (is_top_inout_pin(design, parent->id, other_parent_sig))
                    continue;

                /* Get the slice bounds for this aliased port's connection. */
                int other_msb = -1, other_lsb = -1;
                for (int cx = 0; cx < inst->num_connections; cx++) {
                    if (inst->connections[cx].child_port_id == aliased_port_id) {
                        other_msb = inst->connections[cx].parent_msb;
                        other_lsb = inst->connections[cx].parent_lsb;
                        break;
                    }
                }

                /* Find the SharedNet for this other parent signal + slice. */
                int other_idx = find_shared_net_for_signal(nets, net_count,
                                                            other_parent_sig,
                                                            other_msb, other_lsb);

                if (other_idx >= 0 && other_idx != n) {
                    SharedNet *other = &nets[other_idx];

                    /* Merge other's drivers into this net. */
                    for (int od = 0; od < other->num_drivers; od++) {
                        /* Skip drivers from the same instance (pass-through). */
                        if (other->drivers[od].inst_idx == drv_inst_idx)
                            continue;
                        /* Check if driver already exists in target net. */
                        int dup = 0;
                        for (int ed = 0; ed < net->num_drivers; ed++) {
                            if (net->drivers[ed].inst_idx == other->drivers[od].inst_idx &&
                                net->drivers[ed].child_port_id == other->drivers[od].child_port_id) {
                                dup = 1;
                                break;
                            }
                        }
                        if (!dup)
                            add_driver_to_net(net, &other->drivers[od]);
                    }

                    /* Track the other parent signal in merged group. */
                    add_merged_parent(net, other->parent_signal_id);
                    for (int mi = 0; mi < other->num_merged_parents; mi++)
                        add_merged_parent(net, other->merged_parent_ids[mi]);

                    /* Mark the other net as consumed. */
                    other->merged = 1;
                } else if (other_idx < 0) {
                    /* The other parent signal doesn't have a SharedNet yet
                     * (single driver only). Track it as merged parent.
                     * Skip dup_sink signals — they're dummy absorbers from
                     * Phase 0.5 dedup, not real bus wires. */
                    if (!is_dup_sink_signal(parent, other_parent_sig))
                        add_merged_parent(net, other_parent_sig);
                }
            }

            /* Mark this driver as pass-through. */
            ensure_passthrough_array(net);
            if (net->passthrough_driver) {
                /* The passthrough_driver array may need to be
                 * re-allocated after adding drivers above. */
                int *tmp = (int *)realloc(net->passthrough_driver,
                                           (size_t)net->num_drivers * sizeof(int));
                if (tmp) {
                    net->passthrough_driver = tmp;
                    /* Zero out any new entries. */
                    for (int z = 0; z < net->num_drivers; z++) {
                        if (net->drivers[z].inst_idx == drv_inst_idx) {
                            net->passthrough_driver[z] = 1;
                        }
                    }
                }
            }
        }
    }

    /* Second pass: For pass-through instances, check if they have additional
     * port alias groups (e.g., DONE) that also need shared net handling.
     * These groups weren't found in the first pass because the pass-through
     * module's ports have can_be_z=false, but the real drivers (peripherals)
     * DO have can_be_z=true on the aliased parent signals. */
    {
        /* Collect pass-through instances found in the first pass. */
        int *pt_inst = NULL;
        int *pt_child = NULL;
        int pt_count = 0;
        for (int n = 0; n < net_count; n++) {
            if (nets[n].merged) continue;
            if (!nets[n].passthrough_driver) continue;
            for (int d = 0; d < nets[n].num_drivers; d++) {
                if (!nets[n].passthrough_driver[d]) continue;
                /* Check if this instance is already recorded. */
                int already = 0;
                for (int k = 0; k < pt_count; k++) {
                    if (pt_inst[k] == nets[n].drivers[d].inst_idx) {
                        already = 1;
                        break;
                    }
                }
                if (already) continue;
                int *ti = (int *)realloc(pt_inst, (size_t)(pt_count + 1) * sizeof(int));
                int *tc = (int *)realloc(pt_child, (size_t)(pt_count + 1) * sizeof(int));
                if (!ti || !tc) { free(ti); free(tc); break; }
                pt_inst = ti;
                pt_child = tc;
                pt_inst[pt_count] = nets[n].drivers[d].inst_idx;
                pt_child[pt_count] = nets[n].drivers[d].child_module_id;
                pt_count++;
            }
        }

        /* For each pass-through instance, check all alias groups. */
        for (int pi = 0; pi < pt_count; pi++) {
            int inst_idx = pt_inst[pi];
            int child_mod_id = pt_child[pi];
            const IR_Module *child = &design->modules[child_mod_id];
            const IR_Instance *inst = &parent->instances[inst_idx];

            for (int g = 0; g < child->num_port_alias_groups; g++) {
                const IR_PortAliasGroup *grp = &child->port_alias_groups[g];
                if (grp->count < 2) continue;

                /* Check if any port in this group is already in a SharedNet
                 * at the SAME slice.  Different slices of the same parent
                 * signal (e.g., DATA vs DONE on cpu_bus) are independent. */
                int already_handled = 0;
                for (int p = 0; p < grp->count && !already_handled; p++) {
                    int psig = find_parent_signal_for_child_port(inst, grp->port_ids[p]);
                    if (psig < 0) continue;
                    /* Get the slice for this port's connection. */
                    int p_msb = -1, p_lsb = -1;
                    for (int c2 = 0; c2 < inst->num_connections; c2++) {
                        if (inst->connections[c2].child_port_id == grp->port_ids[p]) {
                            p_msb = inst->connections[c2].parent_msb;
                            p_lsb = inst->connections[c2].parent_lsb;
                            break;
                        }
                    }
                    for (int n = 0; n < net_count && !already_handled; n++) {
                        if (nets[n].merged) continue;
                        /* Match on signal ID AND slice — different slices
                         * of the same signal (DATA vs DONE) are separate. */
                        if (nets[n].parent_signal_id == psig &&
                            nets[n].parent_msb == p_msb &&
                            nets[n].parent_lsb == p_lsb)
                            already_handled = 1;
                    }
                }
                if (already_handled) continue;

                /* This alias group's ports are NOT in any SharedNet yet.
                 * Check if other instances on the aliased parent signals
                 * have can_be_z drivers. If so, create a new SharedNet. */

                /* Pick the first port's parent signal as primary. */
                int primary_parent_sig = -1;
                int primary_port_id = -1;
                int primary_msb = -1, primary_lsb = -1;
                int primary_width = 0;
                for (int p = 0; p < grp->count; p++) {
                    int psig = find_parent_signal_for_child_port(inst, grp->port_ids[p]);
                    if (psig >= 0) {
                        primary_parent_sig = psig;
                        primary_port_id = grp->port_ids[p];
                        /* Get slice info from connection. */
                        for (int c2 = 0; c2 < inst->num_connections; c2++) {
                            if (inst->connections[c2].child_port_id == primary_port_id) {
                                primary_msb = inst->connections[c2].parent_msb;
                                primary_lsb = inst->connections[c2].parent_lsb;
                                break;
                            }
                        }
                        const IR_Signal *ps = find_signal_const(child, primary_port_id);
                        if (ps) primary_width = ps->width;
                        break;
                    }
                }
                if (primary_parent_sig < 0 || primary_width <= 0) continue;

                /* Collect real tristate drivers from ALL aliased parent signals. */
                SharedDriver *new_drivers = NULL;
                int new_driver_count = 0;
                int new_driver_cap = 0;
                int *merged_parents = NULL;
                int merged_parent_count = 0;

                for (int p = 0; p < grp->count; p++) {
                    int aliased_port_id = grp->port_ids[p];
                    int aliased_parent_sig = find_parent_signal_for_child_port(
                        inst, aliased_port_id);
                    if (aliased_parent_sig < 0) continue;

                    if (aliased_parent_sig != primary_parent_sig &&
                        !is_dup_sink_signal(parent, aliased_parent_sig)) {
                        /* Track as merged parent. Skip dup_sink signals. */
                        int *mp = (int *)realloc(merged_parents,
                            (size_t)(merged_parent_count + 1) * sizeof(int));
                        if (mp) {
                            merged_parents = mp;
                            merged_parents[merged_parent_count++] = aliased_parent_sig;
                        }
                    }

                    /* Find instances connecting to this parent signal with can_be_z. */
                    int aliased_msb = -1, aliased_lsb = -1;
                    for (int c2 = 0; c2 < inst->num_connections; c2++) {
                        if (inst->connections[c2].child_port_id == aliased_port_id) {
                            aliased_msb = inst->connections[c2].parent_msb;
                            aliased_lsb = inst->connections[c2].parent_lsb;
                            break;
                        }
                    }

                    for (int i2 = 0; i2 < parent->num_instances; i2++) {
                        if (i2 == inst_idx) continue; /* Skip pass-through */
                        const IR_Instance *other_inst = &parent->instances[i2];
                        if (other_inst->child_module_id < 0) continue;
                        const IR_Module *other_child = &design->modules[other_inst->child_module_id];

                        for (int c2 = 0; c2 < other_inst->num_connections; c2++) {
                            const IR_InstanceConnection *oc = &other_inst->connections[c2];
                            if (oc->parent_signal_id != aliased_parent_sig) continue;
                            if (oc->parent_msb != aliased_msb || oc->parent_lsb != aliased_lsb) continue;

                            const IR_Signal *oport = find_signal_const(other_child, oc->child_port_id);
                            if (!oport || oport->kind != SIG_PORT) continue;
                            if (oport->u.port.direction != PORT_INOUT &&
                                oport->u.port.direction != PORT_OUT) continue;
                            if (!oport->can_be_z) continue;

                            /* Found a real tristate driver. */
                            if (new_driver_count >= new_driver_cap) {
                                new_driver_cap = (new_driver_cap == 0) ? 4 : (new_driver_cap * 2);
                                SharedDriver *tmp = (SharedDriver *)realloc(new_drivers,
                                    (size_t)new_driver_cap * sizeof(SharedDriver));
                                if (!tmp) { free(new_drivers); new_drivers = NULL; new_driver_count = 0; break; }
                                new_drivers = tmp;
                            }
                            SharedDriver *nd = &new_drivers[new_driver_count++];
                            nd->inst_idx = i2;
                            nd->conn_idx = c2;
                            nd->child_port_id = oc->child_port_id;
                            nd->child_module_id = other_inst->child_module_id;
                            nd->parent_msb = oc->parent_msb;
                            nd->parent_lsb = oc->parent_lsb;
                        }
                    }
                }

                if (new_driver_count < 1) {
                    free(new_drivers);
                    free(merged_parents);
                    continue;
                }

                /* Create a new SharedNet for this alias group. */
                /* Grow the nets array. */
                int needed = net_count + 1;
                if (needed > *net_count_ptr || !nets) {
                    int new_cap = (net_count == 0) ? 8 : (net_count * 2);
                    if (new_cap < needed) new_cap = needed;
                    SharedNet *tmp2 = (SharedNet *)realloc(nets, (size_t)new_cap * sizeof(SharedNet));
                    if (!tmp2) { free(new_drivers); free(merged_parents); continue; }
                    nets = tmp2;
                    *nets_ptr = nets;
                }

                SharedNet *new_net = &nets[net_count];
                memset(new_net, 0, sizeof(*new_net));
                new_net->parent_signal_id = primary_parent_sig;
                new_net->parent_msb = primary_msb;
                new_net->parent_lsb = primary_lsb;
                new_net->width = primary_width;
                new_net->drivers = new_drivers;
                new_net->num_drivers = new_driver_count;
                new_net->driver_cap = new_driver_cap;
                new_net->merged_parent_ids = merged_parents;
                new_net->num_merged_parents = merged_parent_count;

                /* All drivers are real (the pass-through instance itself
                 * is not added as a driver — it has no _out/_oe). */
                new_net->passthrough_driver = NULL;

                net_count++;

                /* Convert the pass-through instance's ports in this alias
                 * group from OUTPUT to INPUT.  They no longer drive the
                 * parent signals — the mux does instead. */
                for (int p = 0; p < grp->count; p++) {
                    IR_Signal *port_sig = find_signal(
                        (IR_Module *)child, grp->port_ids[p]);
                    if (!port_sig) continue;
                    if (port_sig->kind != SIG_PORT) continue;
                    if (port_sig->u.port.direction == PORT_OUT ||
                        port_sig->u.port.direction == PORT_INOUT) {
                        port_sig->u.port.direction = PORT_IN;
                        port_sig->can_be_z = false;
                    }
                }
            }
        }

        free(pt_inst);
        free(pt_child);
        *nets_ptr = nets;
    }

    /* Remove merged (consumed) nets. */
    int write_idx = 0;
    for (int i = 0; i < net_count; i++) {
        if (!nets[i].merged) {
            nets[write_idx++] = nets[i];
        } else {
            free(nets[i].drivers);
            free(nets[i].merged_parent_ids);
            free(nets[i].passthrough_driver);
            memset(&nets[i], 0, sizeof(SharedNet));
        }
    }
    *net_count_ptr = write_idx;

    /* Re-filter: only keep nets with 2+ real (non-passthrough) drivers. */
    int final_write = 0;
    for (int i = 0; i < write_idx; i++) {
        int real_count = 0;
        for (int d = 0; d < nets[i].num_drivers; d++) {
            if (!nets[i].passthrough_driver ||
                !nets[i].passthrough_driver[d])
                real_count++;
        }
        if (real_count >= 2 ||
            (real_count >= 1 && nets[i].num_merged_parents > 0)) {
            nets[final_write++] = nets[i];
        } else {
            free(nets[i].drivers);
            free(nets[i].merged_parent_ids);
            free(nets[i].passthrough_driver);
        }
    }
    *net_count_ptr = final_write;
}

/* ======================================================================== */
/*  Helpers for allocating new IR nodes                                      */
/* ======================================================================== */

static int next_signal_id(IR_Module *mod)
{
    int max_id = -1;
    for (int i = 0; i < mod->num_signals; i++)
        if (mod->signals[i].id > max_id) max_id = mod->signals[i].id;
    return max_id + 1;
}

static IR_Signal *add_signal(IR_Module *mod, JZArena *arena, int id,
                              const char *name, IR_SignalKind kind,
                              IR_PortDirection dir, int width, int source_line)
{
    /* Grow the signal array (arena-allocated, so we must reallocate). */
    IR_Signal *new_arr = (IR_Signal *)jz_arena_alloc(
        arena, (size_t)(mod->num_signals + 1) * sizeof(IR_Signal));
    if (!new_arr) return NULL;
    if (mod->num_signals > 0)
        memcpy(new_arr, mod->signals, (size_t)mod->num_signals * sizeof(IR_Signal));
    mod->signals = new_arr;

    IR_Signal *sig = &mod->signals[mod->num_signals++];
    memset(sig, 0, sizeof(*sig));
    sig->id = id;
    sig->name = ir_strdup_arena(arena, name);
    sig->kind = kind;
    sig->width = width;
    sig->owner_module_id = mod->id;
    sig->source_line = source_line;
    sig->can_be_z = false;
    if (kind == SIG_PORT) sig->u.port.direction = dir;
    return sig;
}

static IR_Expr *make_literal(JZArena *arena, int width, uint64_t value, int is_z)
{
    IR_Expr *e = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
    if (!e) return NULL;
    memset(e, 0, sizeof(*e));
    e->kind = EXPR_LITERAL;
    e->width = width;
    memset(e->u.literal.literal.words, 0, sizeof(e->u.literal.literal.words));
    e->u.literal.literal.words[0] = value;
    e->u.literal.literal.width = width;
    e->u.literal.literal.is_z = is_z;
    return e;
}

static IR_Expr *make_signal_ref(JZArena *arena, int signal_id, int width)
{
    IR_Expr *e = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
    if (!e) return NULL;
    memset(e, 0, sizeof(*e));
    e->kind = EXPR_SIGNAL_REF;
    e->width = width;
    e->u.signal_ref.signal_id = signal_id;
    return e;
}

static IR_Expr *make_ternary(JZArena *arena, IR_Expr *cond,
                              IR_Expr *true_val, IR_Expr *false_val, int width)
{
    IR_Expr *e = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
    if (!e) return NULL;
    memset(e, 0, sizeof(*e));
    e->kind = EXPR_TERNARY;
    e->width = width;
    e->u.ternary.condition = cond;
    e->u.ternary.true_val = true_val;
    e->u.ternary.false_val = false_val;
    return e;
}

static IR_Stmt *make_assignment_stmt(JZArena *arena, int signal_id,
                                      IR_Expr *rhs, int source_line)
{
    IR_Stmt *s = (IR_Stmt *)jz_arena_alloc(arena, sizeof(IR_Stmt));
    if (!s) return NULL;
    memset(s, 0, sizeof(*s));
    s->kind = STMT_ASSIGNMENT;
    s->source_line = source_line;
    s->u.assign.lhs_signal_id = signal_id;
    s->u.assign.rhs = rhs;
    s->u.assign.kind = ASSIGN_RECEIVE;
    s->u.assign.is_sliced = false;
    return s;
}

/* ======================================================================== */
/*  Deep-copy an IR expression tree (into arena)                             */
/* ======================================================================== */

static IR_Expr *deep_copy_expr(JZArena *arena, const IR_Expr *src)
{
    if (!src) return NULL;
    IR_Expr *dst = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
    if (!dst) return NULL;
    *dst = *src;

    switch (src->kind) {
    case EXPR_LITERAL:
    case EXPR_SIGNAL_REF:
    case EXPR_SLICE:
        /* Leaf nodes: no sub-expressions to copy. */
        break;
    case EXPR_UNARY_NOT: case EXPR_UNARY_NEG: case EXPR_LOGICAL_NOT:
        dst->u.unary.operand = deep_copy_expr(arena, src->u.unary.operand);
        break;
    case EXPR_BINARY_ADD: case EXPR_BINARY_SUB: case EXPR_BINARY_MUL:
    case EXPR_BINARY_DIV: case EXPR_BINARY_MOD:
    case EXPR_BINARY_AND: case EXPR_BINARY_OR:  case EXPR_BINARY_XOR:
    case EXPR_BINARY_SHL: case EXPR_BINARY_SHR: case EXPR_BINARY_ASHR:
    case EXPR_BINARY_EQ:  case EXPR_BINARY_NEQ:
    case EXPR_BINARY_LT:  case EXPR_BINARY_GT:
    case EXPR_BINARY_LTE: case EXPR_BINARY_GTE:
    case EXPR_LOGICAL_AND: case EXPR_LOGICAL_OR:
        dst->u.binary.left  = deep_copy_expr(arena, src->u.binary.left);
        dst->u.binary.right = deep_copy_expr(arena, src->u.binary.right);
        break;
    case EXPR_TERNARY:
        dst->u.ternary.condition = deep_copy_expr(arena, src->u.ternary.condition);
        dst->u.ternary.true_val  = deep_copy_expr(arena, src->u.ternary.true_val);
        dst->u.ternary.false_val = deep_copy_expr(arena, src->u.ternary.false_val);
        break;
    case EXPR_CONCAT:
        if (src->u.concat.num_operands > 0) {
            dst->u.concat.operands = (IR_Expr **)jz_arena_alloc(
                arena, (size_t)src->u.concat.num_operands * sizeof(IR_Expr *));
            if (dst->u.concat.operands) {
                for (int i = 0; i < src->u.concat.num_operands; i++)
                    dst->u.concat.operands[i] = deep_copy_expr(arena, src->u.concat.operands[i]);
            }
        }
        break;
    case EXPR_INTRINSIC_UADD: case EXPR_INTRINSIC_SADD:
    case EXPR_INTRINSIC_UMUL: case EXPR_INTRINSIC_SMUL:
    case EXPR_INTRINSIC_GBIT: case EXPR_INTRINSIC_SBIT:
    case EXPR_INTRINSIC_GSLICE: case EXPR_INTRINSIC_SSLICE:
        dst->u.intrinsic.source = deep_copy_expr(arena, src->u.intrinsic.source);
        dst->u.intrinsic.index  = deep_copy_expr(arena, src->u.intrinsic.index);
        dst->u.intrinsic.value  = deep_copy_expr(arena, src->u.intrinsic.value);
        break;
    case EXPR_MEM_READ:
        dst->u.mem_read.address = deep_copy_expr(arena, src->u.mem_read.address);
        break;
    default:
        break;
    }
    return dst;
}

/* ======================================================================== */
/*  Make a logical NOT expression                                            */
/* ======================================================================== */

static IR_Expr *make_logical_not(JZArena *arena, IR_Expr *operand)
{
    IR_Expr *e = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
    if (!e) return NULL;
    memset(e, 0, sizeof(*e));
    e->kind = EXPR_LOGICAL_NOT;
    e->width = 1;
    e->u.unary.operand = operand;
    return e;
}

/* ======================================================================== */
/*  Extract OE condition from a child module's async_block for a port        */
/* ======================================================================== */

/**
 * Walk a statement tree and find the "output-enable" condition for a given
 * signal.  Handles:
 *   Pattern A:  signal <= cond ? data : z          -> OE = cond
 *   Pattern A': signal <= cond ? z : data          -> OE = !cond
 *   Pattern B:  IF (cond) { signal <= data }       -> OE = cond
 *               ELSE { signal <= z }
 *   Pattern B': IF (cond) { signal <= z }          -> OE = !cond
 *               ELSE { signal <= data }
 *
 * Returns the OE condition (the expression under which the port is NOT z).
 * When the sense is inverted, synthesizes a logical NOT wrapper.
 *
 * The returned expression is a deep copy, safe to use independently of the
 * original statement tree.
 */

/* Recursive helper: does stmt contain an assignment of z to signal_id? */
static bool stmt_assigns_z_to(const IR_Stmt *stmt, int signal_id)
{
    if (!stmt) return false;
    switch (stmt->kind) {
    case STMT_ASSIGNMENT:
        if (stmt->u.assign.lhs_signal_id == signal_id &&
            stmt->u.assign.rhs &&
            stmt->u.assign.rhs->kind == EXPR_LITERAL &&
            stmt->u.assign.rhs->u.literal.literal.is_z) {
            return true;
        }
        return false;
    case STMT_BLOCK:
        for (int i = 0; i < stmt->u.block.count; i++)
            if (stmt_assigns_z_to(&stmt->u.block.stmts[i], signal_id)) return true;
        return false;
    case STMT_IF:
        return stmt_assigns_z_to(stmt->u.if_stmt.then_block, signal_id) ||
               stmt_assigns_z_to(stmt->u.if_stmt.elif_chain, signal_id) ||
               stmt_assigns_z_to(stmt->u.if_stmt.else_block, signal_id);
    case STMT_SELECT:
        for (int i = 0; i < stmt->u.select_stmt.num_cases; i++)
            if (stmt_assigns_z_to(stmt->u.select_stmt.cases[i].body, signal_id)) return true;
        return false;
    default:
        return false;
    }
}

/**
 * Check if there is an ASSIGN_ALIAS (= operator) to signal_id anywhere in
 * the block.  For INOUT ports, an alias means the port is a direct wire
 * pass-through, so OE should be 1 (always driving) with no warning.
 */
static bool has_alias_assignment(const IR_Stmt *block, int signal_id)
{
    if (!block || block->kind != STMT_BLOCK) return false;
    for (int i = 0; i < block->u.block.count; i++) {
        const IR_Stmt *s = &block->u.block.stmts[i];
        if (s->kind == STMT_ASSIGNMENT &&
            s->u.assign.lhs_signal_id == signal_id &&
            (s->u.assign.kind == ASSIGN_ALIAS ||
             s->u.assign.kind == ASSIGN_ALIAS_ZEXT ||
             s->u.assign.kind == ASSIGN_ALIAS_SEXT))
            return true;
        if (s->kind == STMT_BLOCK) {
            if (has_alias_assignment(s, signal_id)) return true;
        }
    }
    return false;
}

/**
 * Helper: check if a literal expression is z or zero.
 */
static bool literal_is_z_or_zero(const IR_Expr *e)
{
    if (!e || e->kind != EXPR_LITERAL) return false;
    if (e->u.literal.literal.is_z) return true;
    for (int i = 0; i < IR_LIT_WORDS; i++) {
        if (e->u.literal.literal.words[i] != 0) return false;
    }
    return true;
}

/**
 * For a ternary pattern:
 *   signal <= cond ? data : z     -> OE = cond
 *   signal <= cond ? z : data     -> OE = !cond
 *   signal <= cond ? data : 0     -> OE = cond   (oh2b expansion default)
 *   signal <= cond ? 0 : data     -> OE = !cond   (oh2b expansion default)
 * Returns a deep-copied OE condition expression, or NULL if not matched.
 * Recurses into nested STMT_BLOCK nodes (e.g. from oh2b expansion).
 */
static IR_Expr *extract_ternary_oe(const IR_Stmt *block, int signal_id,
                                    JZArena *arena)
{
    if (!block || block->kind != STMT_BLOCK) return NULL;
    for (int i = 0; i < block->u.block.count; i++) {
        const IR_Stmt *s = &block->u.block.stmts[i];
        if (s->kind == STMT_ASSIGNMENT &&
            s->u.assign.lhs_signal_id == signal_id &&
            s->u.assign.rhs &&
            s->u.assign.rhs->kind == EXPR_TERNARY) {
            IR_Expr *tern = s->u.assign.rhs;
            /* Pattern A: cond ? data : z_or_zero  ->  OE = cond */
            if (literal_is_z_or_zero(tern->u.ternary.false_val)) {
                return deep_copy_expr(arena, tern->u.ternary.condition);
            }
            /* Pattern A': cond ? z_or_zero : data  ->  OE = !cond */
            if (literal_is_z_or_zero(tern->u.ternary.true_val)) {
                IR_Expr *cond_copy = deep_copy_expr(arena, tern->u.ternary.condition);
                return cond_copy ? make_logical_not(arena, cond_copy) : NULL;
            }
        }
        /* Recurse into nested blocks (e.g. from oh2b expansion). */
        if (s->kind == STMT_BLOCK) {
            IR_Expr *result = extract_ternary_oe(s, signal_id, arena);
            if (result) return result;
        }
    }
    return NULL;
}

/**
 * For an IF/ELSE pattern:
 *   IF (cond) { signal <= data } ELSE { signal <= z }  -> OE = cond
 *   IF (cond) { signal <= z }    ELSE { signal <= data } -> OE = !cond
 * Returns a deep-copied OE condition expression, or NULL if not matched.
 * Recurses into nested STMT_BLOCK nodes.
 */
static IR_Expr *extract_if_else_oe(const IR_Stmt *block, int signal_id,
                                    JZArena *arena)
{
    if (!block || block->kind != STMT_BLOCK) return NULL;
    for (int i = 0; i < block->u.block.count; i++) {
        const IR_Stmt *s = &block->u.block.stmts[i];
        if (s->kind == STMT_IF) {
            const IR_IfStmt *ifs = &s->u.if_stmt;
            bool then_has_z = stmt_assigns_z_to(ifs->then_block, signal_id);
            bool else_has_z = ifs->else_block &&
                              stmt_assigns_z_to(ifs->else_block, signal_id);

            /* Pattern B: z in else, not in then -> OE = cond */
            if (else_has_z && !then_has_z) {
                return deep_copy_expr(arena, ifs->condition);
            }
            /* Pattern B': z in then, not in else -> OE = !cond */
            if (then_has_z && !else_has_z && ifs->else_block) {
                IR_Expr *cond_copy = deep_copy_expr(arena, ifs->condition);
                return cond_copy ? make_logical_not(arena, cond_copy) : NULL;
            }
        }
        /* Recurse into nested blocks. */
        if (s->kind == STMT_BLOCK) {
            IR_Expr *result = extract_if_else_oe(s, signal_id, arena);
            if (result) return result;
        }
    }
    return NULL;
}

/* ======================================================================== */
/*  Simple z-replacement for non-shared signals                              */
/* ======================================================================== */

static int replace_z_in_expr(IR_Expr *expr, IR_TristateDefault mode)
{
    if (!expr) return 0;
    int count = 0;

    if (expr->kind == EXPR_LITERAL && expr->u.literal.literal.is_z) {
        int width = expr->u.literal.literal.width;
        memset(expr->u.literal.literal.words, 0, sizeof(expr->u.literal.literal.words));
        expr->u.literal.literal.words[0] = default_value_for_width(mode, width);
        expr->u.literal.literal.is_z = 0;
        return 1;
    }

    switch (expr->kind) {
    case EXPR_UNARY_NOT: case EXPR_UNARY_NEG: case EXPR_LOGICAL_NOT:
        count += replace_z_in_expr(expr->u.unary.operand, mode);
        break;
    case EXPR_BINARY_ADD: case EXPR_BINARY_SUB: case EXPR_BINARY_MUL:
    case EXPR_BINARY_DIV: case EXPR_BINARY_MOD:
    case EXPR_BINARY_AND: case EXPR_BINARY_OR:  case EXPR_BINARY_XOR:
    case EXPR_BINARY_SHL: case EXPR_BINARY_SHR: case EXPR_BINARY_ASHR:
    case EXPR_BINARY_EQ:  case EXPR_BINARY_NEQ:
    case EXPR_BINARY_LT:  case EXPR_BINARY_GT:
    case EXPR_BINARY_LTE: case EXPR_BINARY_GTE:
    case EXPR_LOGICAL_AND: case EXPR_LOGICAL_OR:
        count += replace_z_in_expr(expr->u.binary.left, mode);
        count += replace_z_in_expr(expr->u.binary.right, mode);
        break;
    case EXPR_TERNARY:
        count += replace_z_in_expr(expr->u.ternary.condition, mode);
        count += replace_z_in_expr(expr->u.ternary.true_val, mode);
        count += replace_z_in_expr(expr->u.ternary.false_val, mode);
        break;
    case EXPR_CONCAT:
        for (int i = 0; i < expr->u.concat.num_operands; i++)
            count += replace_z_in_expr(expr->u.concat.operands[i], mode);
        break;
    case EXPR_INTRINSIC_UADD: case EXPR_INTRINSIC_SADD:
    case EXPR_INTRINSIC_UMUL: case EXPR_INTRINSIC_SMUL:
    case EXPR_INTRINSIC_GBIT: case EXPR_INTRINSIC_SBIT:
    case EXPR_INTRINSIC_GSLICE: case EXPR_INTRINSIC_SSLICE:
        count += replace_z_in_expr(expr->u.intrinsic.source, mode);
        count += replace_z_in_expr(expr->u.intrinsic.index, mode);
        count += replace_z_in_expr(expr->u.intrinsic.value, mode);
        break;
    case EXPR_MEM_READ:
        count += replace_z_in_expr(expr->u.mem_read.address, mode);
        break;
    default:
        break;
    }
    return count;
}

static int replace_z_in_stmt(IR_Stmt *stmt, IR_TristateDefault mode,
                              const int *excluded, int num_excluded)
{
    if (!stmt) return 0;
    int count = 0;
    switch (stmt->kind) {
    case STMT_ASSIGNMENT:
        /* Skip excluded signals. */
        for (int i = 0; i < num_excluded; i++)
            if (excluded[i] == stmt->u.assign.lhs_signal_id) return 0;
        count += replace_z_in_expr(stmt->u.assign.rhs, mode);
        break;
    case STMT_IF:
        count += replace_z_in_stmt(stmt->u.if_stmt.then_block, mode, excluded, num_excluded);
        count += replace_z_in_stmt(stmt->u.if_stmt.elif_chain, mode, excluded, num_excluded);
        count += replace_z_in_stmt(stmt->u.if_stmt.else_block, mode, excluded, num_excluded);
        break;
    case STMT_SELECT:
        for (int i = 0; i < stmt->u.select_stmt.num_cases; i++)
            count += replace_z_in_stmt(stmt->u.select_stmt.cases[i].body, mode, excluded, num_excluded);
        break;
    case STMT_BLOCK:
        for (int i = 0; i < stmt->u.block.count; i++)
            count += replace_z_in_stmt(&stmt->u.block.stmts[i], mode, excluded, num_excluded);
        break;
    case STMT_MEM_WRITE:
        count += replace_z_in_expr(stmt->u.mem_write.data, mode);
        break;
    }
    return count;
}

/* ======================================================================== */
/*  Append a statement to a STMT_BLOCK                                       */
/* ======================================================================== */

static int append_stmt_to_block(IR_Stmt *block, IR_Stmt *new_stmt, JZArena *arena)
{
    if (!block || block->kind != STMT_BLOCK) return -1;
    IR_BlockStmt *blk = &block->u.block;
    IR_Stmt *new_arr = (IR_Stmt *)jz_arena_alloc(arena, (size_t)(blk->count + 1) * sizeof(IR_Stmt));
    if (!new_arr) return -1;
    if (blk->count > 0) memcpy(new_arr, blk->stmts, (size_t)blk->count * sizeof(IR_Stmt));
    new_arr[blk->count] = *new_stmt;
    blk->stmts = new_arr;
    blk->count++;
    return 0;
}

/* ======================================================================== */
/*  Add a new connection to an instance                                      */
/* ======================================================================== */

static int add_instance_connection(IR_Instance *inst, JZArena *arena,
                                    int parent_signal_id, int child_port_id,
                                    int parent_msb, int parent_lsb)
{
    IR_InstanceConnection *new_arr = (IR_InstanceConnection *)jz_arena_alloc(
        arena, (size_t)(inst->num_connections + 1) * sizeof(IR_InstanceConnection));
    if (!new_arr) return -1;
    if (inst->num_connections > 0)
        memcpy(new_arr, inst->connections,
               (size_t)inst->num_connections * sizeof(IR_InstanceConnection));
    inst->connections = new_arr;
    IR_InstanceConnection *c = &inst->connections[inst->num_connections++];
    memset(c, 0, sizeof(*c));
    c->parent_signal_id = parent_signal_id;
    c->child_port_id = child_port_id;
    c->parent_msb = parent_msb;
    c->parent_lsb = parent_lsb;
    return 0;
}

/* ======================================================================== */
/*  Post-transform IR validation: detect multi-driver conflicts              */
/* ======================================================================== */

/**
 * Post-transform validation: for each (parent_signal, msb, lsb) slice,
 * count how many instance INOUT/OUT ports that are NOT can_be_z drive it.
 * If 2+ always-active drivers remain on the same slice, the transform
 * introduced a multi-driver conflict.
 */
typedef struct {
    int parent_signal_id;
    int msb;
    int lsb;
    int active_drivers;
} SliceDriverCount;

static int validate_no_multidriver(const IR_Design *design,
                                    JZDiagnosticList *diagnostics)
{
    int errors = 0;
    for (int m = 0; m < design->num_modules; m++) {
        const IR_Module *parent = &design->modules[m];

        /* Collect per-slice driver counts. */
        SliceDriverCount *slices = NULL;
        int slice_count = 0;
        int slice_cap = 0;

        for (int i = 0; i < parent->num_instances; i++) {
            const IR_Instance *inst = &parent->instances[i];
            if (inst->child_module_id < 0 || inst->child_module_id >= design->num_modules)
                continue;
            const IR_Module *child = &design->modules[inst->child_module_id];

            for (int c = 0; c < inst->num_connections; c++) {
                const IR_InstanceConnection *conn = &inst->connections[c];
                if (conn->parent_signal_id < 0) continue;
                const IR_Signal *cport = find_signal_const(child, conn->child_port_id);
                if (!cport || cport->kind != SIG_PORT) continue;
                if (cport->u.port.direction != PORT_INOUT &&
                    cport->u.port.direction != PORT_OUT) continue;
                if (cport->can_be_z) continue; /* Still z-capable, not a conflict. */

                /* Find or create slice entry. */
                SliceDriverCount *entry = NULL;
                for (int s = 0; s < slice_count; s++) {
                    if (slices[s].parent_signal_id == conn->parent_signal_id &&
                        slices[s].msb == conn->parent_msb &&
                        slices[s].lsb == conn->parent_lsb) {
                        entry = &slices[s];
                        break;
                    }
                }
                if (!entry) {
                    if (slice_count >= slice_cap) {
                        slice_cap = (slice_cap == 0) ? 16 : (slice_cap * 2);
                        SliceDriverCount *tmp = (SliceDriverCount *)realloc(
                            slices, (size_t)slice_cap * sizeof(SliceDriverCount));
                        if (!tmp) { free(slices); return 0; }
                        slices = tmp;
                    }
                    entry = &slices[slice_count++];
                    entry->parent_signal_id = conn->parent_signal_id;
                    entry->msb = conn->parent_msb;
                    entry->lsb = conn->parent_lsb;
                    entry->active_drivers = 0;
                }
                entry->active_drivers++;
            }
        }

        /* Report errors for slices with 2+ active drivers. */
        for (int s = 0; s < slice_count; s++) {
            if (slices[s].active_drivers >= 2) {
                const IR_Signal *psig = find_signal_const(parent, slices[s].parent_signal_id);
                JZLocation loc;
                memset(&loc, 0, sizeof(loc));
                if (psig) loc.line = psig->source_line;
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "post-transform: signal '%s'[%d:%d] has %d always-active "
                         "instance drivers (tri-state elimination created multi-driver conflict)",
                         psig && psig->name ? psig->name : "?",
                         slices[s].msb, slices[s].lsb,
                         slices[s].active_drivers);
                jz_diagnostic_report(diagnostics, loc, JZ_SEVERITY_ERROR,
                                     "TRISTATE_TRANSFORM_MUTUAL_EXCLUSION_FAIL", msg);
                errors++;
            }
        }
        free(slices);
    }
    return errors;
}

/* ======================================================================== */
/*  Track child ports already split (same-module multi-instance dedup)       */
/* ======================================================================== */

/**
 * When the same child module is instantiated multiple times and both
 * instances drive the same parent net, we must not add _out/_oe ports
 * to the child module twice. This structure tracks which (module, port)
 * pairs have already been split so we can reuse the existing port IDs.
 */
typedef struct {
    int child_module_id;
    int original_port_id;
    int out_port_id;
    int oe_port_id;
} SplitPortRecord;

static const SplitPortRecord *find_split_record(const SplitPortRecord *records,
                                                  int count,
                                                  int child_module_id,
                                                  int original_port_id)
{
    for (int i = 0; i < count; i++) {
        if (records[i].child_module_id == child_module_id &&
            records[i].original_port_id == original_port_id)
            return &records[i];
    }
    return NULL;
}

/* ======================================================================== */
/*  Core transform: split shared tri-state nets                              */
/* ======================================================================== */

/**
 * Transform shared tri-state nets for a single parent module.
 *
 * For each shared net:
 *   - In each driving child module, add _out and _oe ports (once per
 *     unique (module, port) pair — shared across instances of the same
 *     module).
 *   - Extract OE condition from the child's async_block and emit the
 *     _oe assignment. If extraction fails, fall back to _oe = 1 and
 *     emit a diagnostic warning.
 *   - Replace z in child async_block for that port.
 *   - In parent, create per-instance wires + OE wires and build a mux.
 *   - Update instance connections.
 *
 * Returns 0 on success, non-zero on failure.
 */
static int transform_shared_nets(IR_Design *design,
                                  IR_Module *parent,
                                  JZArena *arena,
                                  JZDiagnosticList *diagnostics)
{
    int shared_count = 0;
    SharedNet *shared = find_shared_tristate_nets(design, parent, &shared_count);

    /* Merge alias-connected nets before filtering. */
    if (shared && shared_count > 0) {
        merge_alias_connected_nets(design, parent, &shared, &shared_count);
    }

    if (shared_count == 0) {
        free(shared);
        return 0;
    }

    /* Ensure parent has an async_block to add mux assignments. */
    if (!parent->async_block) {
        IR_Stmt *blk = (IR_Stmt *)jz_arena_alloc(arena, sizeof(IR_Stmt));
        if (!blk) { shared_net_free(shared, shared_count); return -1; }
        memset(blk, 0, sizeof(*blk));
        blk->kind = STMT_BLOCK;
        parent->async_block = blk;
    }

    /* Track already-split (child_module, port) pairs to avoid duplicating
     * _out/_oe ports when the same child module is instantiated multiple
     * times on the same shared net. */
    SplitPortRecord *split_records = NULL;
    int split_count = 0;
    int split_cap = 0;

    for (int n = 0; n < shared_count; n++) {
        SharedNet *net = &shared[n];
        IR_Signal *parent_sig = find_signal(parent, net->parent_signal_id);
        if (!parent_sig) continue;

        int data_width = net->width;
        if (data_width <= 0) data_width = parent_sig->width;

        /* Count real (non-passthrough) drivers. */
        int real_driver_count = 0;
        for (int d = 0; d < net->num_drivers; d++) {
            if (!net->passthrough_driver || !net->passthrough_driver[d])
                real_driver_count++;
        }

        /* For each real driver, create _out and _oe in child, and
         * corresponding wires + connections in parent. */
        int *parent_out_ids = (int *)calloc((size_t)net->num_drivers, sizeof(int));
        int *parent_oe_ids  = (int *)calloc((size_t)net->num_drivers, sizeof(int));
        if (!parent_out_ids || !parent_oe_ids) {
            free(parent_out_ids); free(parent_oe_ids);
            free(split_records);
            shared_net_free(shared, shared_count);
            return -1;
        }

        for (int d = 0; d < net->num_drivers; d++) {
            SharedDriver *drv = &net->drivers[d];
            IR_Module *child = &design->modules[drv->child_module_id];
            IR_Instance *inst = &parent->instances[drv->inst_idx];
            IR_Signal *child_port = find_signal(child, drv->child_port_id);
            if (!child_port) continue;

            /* Handle pass-through drivers: convert ports to IN, don't
             * create _out/_oe. */
            if (net->passthrough_driver && net->passthrough_driver[d]) {
                /* Change the pass-through instance's aliased INOUT ports
                 * to PORT_IN. The alias statements in the child module
                 * still work — they become `assign wire = port_input`. */
                const IR_PortAliasGroup *grp = find_alias_group_for_port(
                    child, drv->child_port_id);
                if (grp) {
                    for (int p = 0; p < grp->count; p++) {
                        IR_Signal *alias_port = find_signal(child, grp->port_ids[p]);
                        if (alias_port && alias_port->kind == SIG_PORT &&
                            alias_port->u.port.direction == PORT_INOUT) {
                            alias_port->u.port.direction = PORT_IN;
                            alias_port->can_be_z = false;
                        }
                    }
                } else {
                    /* No alias group — just convert this single port. */
                    child_port = find_signal(child, drv->child_port_id);
                    if (child_port && child_port->u.port.direction == PORT_INOUT) {
                        child_port->u.port.direction = PORT_IN;
                        child_port->can_be_z = false;
                    }
                }
                parent_out_ids[d] = -1;
                parent_oe_ids[d] = -1;
                continue;
            }

            int child_out_id, child_oe_id;

            /* Check if we already split this (module, port) pair. */
            const SplitPortRecord *existing = find_split_record(
                split_records, split_count,
                drv->child_module_id, drv->child_port_id);

            if (existing) {
                /* Reuse existing child _out and _oe port IDs. */
                child_out_id = existing->out_port_id;
                child_oe_id  = existing->oe_port_id;
            } else {
                /* First time seeing this (module, port): create child ports,
                 * extract OE, rewrite LHS, replace z. */

                /* Generate child-side names. */
                char out_name[128], oe_name[128];
                snprintf(out_name, sizeof(out_name), "%s_out", child_port->name);
                snprintf(oe_name,  sizeof(oe_name),  "%s_oe",  child_port->name);

                /* --- Child module: add _out and _oe output ports --- */
                child_out_id = next_signal_id(child);
                IR_Signal *child_out_sig = add_signal(child, arena, child_out_id,
                    out_name, SIG_PORT, PORT_OUT, data_width, child_port->source_line);
                if (!child_out_sig) { free(parent_out_ids); free(parent_oe_ids);
                    free(split_records); shared_net_free(shared, shared_count); return -1; }

                child_oe_id = next_signal_id(child);
                IR_Signal *child_oe_sig = add_signal(child, arena, child_oe_id,
                    oe_name, SIG_PORT, PORT_OUT, 1, child_port->source_line);
                if (!child_oe_sig) { free(parent_out_ids); free(parent_oe_ids);
                    free(split_records); shared_net_free(shared, shared_count); return -1; }

                /* --- Child module: extract OE condition and emit _oe assignment --- */

                /* Alias assignments (= operator) mean the port is a direct
                 * wire pass-through.  OE is always 1 — no warning needed. */
                IR_Expr *oe_cond = NULL;
                if (has_alias_assignment(child->async_block,
                                         drv->child_port_id)) {
                    oe_cond = make_literal(arena, 1, 1, 0);
                }

                if (!oe_cond)
                    oe_cond = extract_ternary_oe(child->async_block,
                                                  drv->child_port_id, arena);
                if (!oe_cond)
                    oe_cond = extract_if_else_oe(child->async_block,
                                                  drv->child_port_id, arena);

                if (!oe_cond) {
                    /* Fallback: drive _oe = 1'b1 (always enabled).
                     * This is safe but conservative — the port is always
                     * driving. Emit a warning so the user knows. */
                    oe_cond = make_literal(arena, 1, 1, 0);

                    /* Re-find child_port — add_signal may have reallocated. */
                    child_port = find_signal(child, drv->child_port_id);

                    JZLocation loc;
                    memset(&loc, 0, sizeof(loc));
                    if (child_port) loc.line = child_port->source_line;
                    char msg[512];
                    snprintf(msg, sizeof(msg),
                             "could not extract output-enable condition from port '%s' "
                             "in module '%s'; _oe driven high as fallback "
                             "(port will always drive the bus)",
                             child_port && child_port->name ? child_port->name : "?",
                             child->name ? child->name : "?");
                    jz_diagnostic_report(diagnostics, loc, JZ_SEVERITY_ERROR,
                                         "TRISTATE_TRANSFORM_OE_EXTRACT_FAIL", msg);
                }

                /* Emit: port_oe <= oe_cond; */
                {
                    IR_Stmt *oe_stmt = make_assignment_stmt(arena, child_oe_id,
                        oe_cond, child_port ? child_port->source_line : 0);
                    if (!oe_stmt) { free(parent_out_ids); free(parent_oe_ids);
                        free(split_records); shared_net_free(shared, shared_count); return -1; }
                    if (append_stmt_to_block(child->async_block, oe_stmt, arena) != 0) {
                        free(parent_out_ids); free(parent_oe_ids);
                        free(split_records); shared_net_free(shared, shared_count); return -1;
                    }
                }

                /* Do NOT replace z here — other ports in the same child
                 * module may still need their z patterns intact for OE
                 * extraction.  Phase 2 handles z-replacement for the
                 * entire module after all shared nets are processed. */

                /* Redirect the original port's assignments to _out.
                 * NOTE: must re-find child_port since add_signal() may have
                 * reallocated the signals array. */
                child_port = find_signal(child, drv->child_port_id);
                if (child_port) {
                    child_port->u.port.direction = PORT_IN;
                    child_port->can_be_z = false;
                }

                /* Rewrite assignments: change target from original port to _out. */
                {
                    void rewrite_lhs(IR_Stmt *s, int old_id, int new_id);
                    rewrite_lhs(child->async_block, drv->child_port_id, child_out_id);
                }

                /* Record this split so subsequent instances of the same module
                 * reuse the same _out/_oe ports. */
                if (split_count >= split_cap) {
                    split_cap = (split_cap == 0) ? 16 : (split_cap * 2);
                    SplitPortRecord *tmp = (SplitPortRecord *)realloc(
                        split_records, (size_t)split_cap * sizeof(SplitPortRecord));
                    if (!tmp) { free(parent_out_ids); free(parent_oe_ids);
                        free(split_records); shared_net_free(shared, shared_count); return -1; }
                    split_records = tmp;
                }
                split_records[split_count].child_module_id = drv->child_module_id;
                split_records[split_count].original_port_id = drv->child_port_id;
                split_records[split_count].out_port_id = child_out_id;
                split_records[split_count].oe_port_id = child_oe_id;
                split_count++;
            }

            /* --- Parent module: add per-instance wires --- */
            char parent_out_name[192], parent_oe_name[192];
            /* Re-find child_port for name (may have been reallocated). */
            child_port = find_signal(child, drv->child_port_id);
            snprintf(parent_out_name, sizeof(parent_out_name),
                     "%s_%s_out", inst->name,
                     child_port && child_port->name ? child_port->name : "port");
            snprintf(parent_oe_name, sizeof(parent_oe_name),
                     "%s_%s_oe", inst->name,
                     child_port && child_port->name ? child_port->name : "port");

            int p_out_id = next_signal_id(parent);
            IR_Signal *p_out = add_signal(parent, arena, p_out_id,
                parent_out_name, SIG_NET, PORT_IN, data_width, parent_sig->source_line);
            if (!p_out) { free(parent_out_ids); free(parent_oe_ids);
                free(split_records); shared_net_free(shared, shared_count); return -1; }
            parent_out_ids[d] = p_out_id;

            int p_oe_id = next_signal_id(parent);
            IR_Signal *p_oe = add_signal(parent, arena, p_oe_id,
                parent_oe_name, SIG_NET, PORT_IN, 1, parent_sig->source_line);
            if (!p_oe) { free(parent_out_ids); free(parent_oe_ids);
                free(split_records); shared_net_free(shared, shared_count); return -1; }
            parent_oe_ids[d] = p_oe_id;

            /* --- Update instance connection: _out and _oe ports --- */
            add_instance_connection(inst, arena, p_out_id, child_out_id, -1, -1);
            add_instance_connection(inst, arena, p_oe_id,  child_oe_id,  -1, -1);
        }

        /* --- Build priority mux in parent (only real drivers) --- */
        uint64_t def_val = default_value_for_width(design->tristate_default, data_width);
        IR_Expr *chain = make_literal(arena, data_width, def_val, 0);
        if (!chain) { free(parent_out_ids); free(parent_oe_ids);
            free(split_records); shared_net_free(shared, shared_count); return -1; }

        for (int d = net->num_drivers - 1; d >= 0; d--) {
            /* Skip pass-through drivers — they don't have _out/_oe. */
            if (net->passthrough_driver && net->passthrough_driver[d])
                continue;
            if (parent_out_ids[d] < 0 || parent_oe_ids[d] < 0)
                continue;
            IR_Expr *oe_ref = make_signal_ref(arena, parent_oe_ids[d], 1);
            IR_Expr *out_ref = make_signal_ref(arena, parent_out_ids[d], data_width);
            chain = make_ternary(arena, oe_ref, out_ref, chain, data_width);
            if (!chain) { free(parent_out_ids); free(parent_oe_ids);
                free(split_records); shared_net_free(shared, shared_count); return -1; }
        }

        /* Assign mux to the primary parent signal.
         * Use ASSIGN_ALIAS so the backend emits this as a continuous
         * "assign" and the bus signal stays as wire — otherwise an
         * ASSIGN_RECEIVE would make the signal reg and only the muxed
         * slice bits would be assigned in always @*, causing unassigned
         * bits (ADDR, CMD, VALID) to latch at 0. */
        IR_Stmt *mux_stmt = make_assignment_stmt(arena, net->parent_signal_id,
            chain, parent_sig->source_line);
        if (!mux_stmt) { free(parent_out_ids); free(parent_oe_ids);
            free(split_records); shared_net_free(shared, shared_count); return -1; }

        mux_stmt->u.assign.kind = ASSIGN_ALIAS;
        if (net->parent_msb >= 0 && net->parent_lsb >= 0) {
            mux_stmt->u.assign.is_sliced = true;
            mux_stmt->u.assign.lhs_msb = net->parent_msb;
            mux_stmt->u.assign.lhs_lsb = net->parent_lsb;
        }

        append_stmt_to_block(parent->async_block, mux_stmt, arena);

        /* Mark parent signal as no longer can_be_z for this slice. */
        parent_sig->can_be_z = false;

        /* --- Per-driver exclusion muxes (break combinational loops) ---
         *
         * When 2+ real drivers share an INOUT net, a module that both
         * reads and writes the bus creates a combinational loop through
         * the full mux.  Real tri-state physics prevents this: a driver
         * on high-Z doesn't see its own output reflected back.
         *
         * For each real driver d, build an exclusion mux that includes
         * every driver EXCEPT d, then redirect d's read-path instance
         * connection to the exclusion wire. */
        if (real_driver_count >= 2) {
            for (int d = 0; d < net->num_drivers; d++) {
                /* Skip pass-through drivers. */
                if (net->passthrough_driver && net->passthrough_driver[d])
                    continue;
                if (parent_out_ids[d] < 0 || parent_oe_ids[d] < 0)
                    continue;

                SharedDriver *drv = &net->drivers[d];
                IR_Instance *inst = &parent->instances[drv->inst_idx];

                /* Build exclusion mux: all drivers except this one. */
                IR_Expr *excl_chain = make_literal(arena, data_width, def_val, 0);
                if (!excl_chain) { free(parent_out_ids); free(parent_oe_ids);
                    free(split_records); shared_net_free(shared, shared_count); return -1; }

                for (int j = net->num_drivers - 1; j >= 0; j--) {
                    if (j == d) continue;  /* exclude self */
                    if (net->passthrough_driver && net->passthrough_driver[j])
                        continue;
                    if (parent_out_ids[j] < 0 || parent_oe_ids[j] < 0)
                        continue;
                    IR_Expr *oe_ref = make_signal_ref(arena, parent_oe_ids[j], 1);
                    IR_Expr *out_ref = make_signal_ref(arena, parent_out_ids[j], data_width);
                    excl_chain = make_ternary(arena, oe_ref, out_ref, excl_chain, data_width);
                    if (!excl_chain) { free(parent_out_ids); free(parent_oe_ids);
                        free(split_records); shared_net_free(shared, shared_count); return -1; }
                }

                /* Create _rd wire in parent. */
                IR_Module *child_rd = &design->modules[drv->child_module_id];
                IR_Signal *child_port_rd = find_signal(child_rd, drv->child_port_id);
                char rd_name[192];
                snprintf(rd_name, sizeof(rd_name), "%s_%s_rd", inst->name,
                         child_port_rd && child_port_rd->name ? child_port_rd->name : "port");

                /* Re-find parent_sig after potential add_signal reallocation. */
                parent_sig = find_signal(parent, net->parent_signal_id);

                int rd_id = next_signal_id(parent);
                IR_Signal *rd_sig = add_signal(parent, arena, rd_id,
                    rd_name, SIG_NET, PORT_IN, data_width,
                    parent_sig ? parent_sig->source_line : 0);
                if (!rd_sig) { free(parent_out_ids); free(parent_oe_ids);
                    free(split_records); shared_net_free(shared, shared_count); return -1; }

                /* Assign exclusion mux to the _rd wire. */
                parent_sig = find_signal(parent, net->parent_signal_id);
                IR_Stmt *rd_stmt = make_assignment_stmt(arena, rd_id,
                    excl_chain, parent_sig ? parent_sig->source_line : 0);
                if (!rd_stmt) { free(parent_out_ids); free(parent_oe_ids);
                    free(split_records); shared_net_free(shared, shared_count); return -1; }
                rd_stmt->u.assign.kind = ASSIGN_ALIAS;
                append_stmt_to_block(parent->async_block, rd_stmt, arena);

                /* Redirect the driver's original instance connection
                 * to read from the exclusion wire instead of the parent signal.
                 * The _rd wire is already data_width, so use full-width (-1,-1). */
                IR_InstanceConnection *conn = &inst->connections[drv->conn_idx];
                conn->parent_signal_id = rd_id;
                conn->parent_msb = -1;
                conn->parent_lsb = -1;
            }

            /* Re-find parent_sig for subsequent merged-net code. */
            parent_sig = find_signal(parent, net->parent_signal_id);
        }

        /* For merged nets: assign the mux result to all additional parent
         * signals in the merged group.  Use sliced assignments so only
         * the DATA portion is aliased — other slices (ADDR, CMD, VALID)
         * remain driven by the pass-through instance's outputs. */
        for (int mi = 0; mi < net->num_merged_parents; mi++) {
            int merged_sig_id = net->merged_parent_ids[mi];
            IR_Signal *msig = find_signal(parent, merged_sig_id);
            if (!msig) continue;

            /* Create: merged_signal[msb:lsb] <= primary_signal[msb:lsb]; */
            IR_Expr *primary_ref;
            if (net->parent_msb >= 0 && net->parent_lsb >= 0) {
                /* Sliced: reference just the relevant slice of the primary. */
                IR_Expr *slice = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
                if (!slice) continue;
                memset(slice, 0, sizeof(*slice));
                slice->kind = EXPR_SLICE;
                slice->width = data_width;
                slice->u.slice.signal_id = net->parent_signal_id;
                slice->u.slice.base_expr = NULL;
                slice->u.slice.msb = net->parent_msb;
                slice->u.slice.lsb = net->parent_lsb;
                primary_ref = slice;
            } else {
                /* Full-width: the shared net spans the entire signal. */
                primary_ref = make_signal_ref(arena, net->parent_signal_id,
                                                data_width);
            }
            if (!primary_ref) continue;

            IR_Stmt *alias_stmt = make_assignment_stmt(arena, merged_sig_id,
                primary_ref, msig->source_line);
            if (!alias_stmt) continue;

            /* Use sliced ASSIGN_ALIAS so the backend emits this as a
             * continuous "assign sig[msb:lsb] = ..." and the bus signal
             * stays as wire.  Sliced aliases do NOT participate in the
             * backend union-find (alias.c line 167 skips is_sliced), so
             * non-shared slices (ADDR, CMD, VALID) are NOT collapsed. */
            alias_stmt->u.assign.kind = ASSIGN_ALIAS;

            if (net->parent_msb >= 0 && net->parent_lsb >= 0) {
                alias_stmt->u.assign.is_sliced = true;
                alias_stmt->u.assign.lhs_msb = net->parent_msb;
                alias_stmt->u.assign.lhs_lsb = net->parent_lsb;
            }

            append_stmt_to_block(parent->async_block, alias_stmt, arena);
            msig->can_be_z = false;
        }

        /* Report an INFO diagnostic about this shared-net transform. */
        {
            JZLocation loc;
            memset(&loc, 0, sizeof(loc));
            loc.filename = module_source_file(design, parent);
            loc.line = parent_sig->source_line;
            char msg[512];
            if (net->num_merged_parents > 0) {
                snprintf(msg, sizeof(msg),
                         "shared tri-state net '%s' (+%d merged) in module '%s' "
                         "transformed to priority mux with %d real driver(s) "
                         "(%d pass-through)",
                         parent_sig->name ? parent_sig->name : "?",
                         net->num_merged_parents,
                         parent->name ? parent->name : "?",
                         real_driver_count,
                         net->num_drivers - real_driver_count);
            } else {
                snprintf(msg, sizeof(msg),
                         "shared tri-state net '%s' in module '%s' transformed to priority mux with %d driver(s)",
                         parent_sig->name ? parent_sig->name : "?",
                         parent->name ? parent->name : "?",
                         net->num_drivers);
            }
            jz_diagnostic_report(diagnostics, loc, JZ_SEVERITY_NOTE,
                                 "INFO_TRISTATE_TRANSFORM", msg);
        }

        free(parent_out_ids);
        free(parent_oe_ids);
    }

    free(split_records);
    shared_net_free(shared, shared_count);
    return 0;
}

/* ======================================================================== */
/*  Eliminate pass-through combinational loops on non-tristate ports          */
/* ======================================================================== */

/**
 * For pass-through modules (like memory_map) whose ports connect to the
 * same parent wire, output ports that echo back to the same signal as a
 * corresponding input create combinational loops.  For example:
 *
 *   memory_map.src_ADDR  (input)  -> cpu_bus[15:0]
 *   memory_map.tgt0_ADDR (output) -> cpu_bus[15:0]
 *
 * Inside memory_map: tgt0_ADDR = src_ADDR;  (creates a loop)
 *
 * Fix: For each instance, detect output ports that connect to the exact
 * same (parent_signal_id, msb, lsb) as an input port on the same instance.
 * Convert those redundant outputs to inputs and redirect their assignments
 * to dummy wires in the child module.
 */
static int eliminate_passthrough_loops(IR_Design *design,
                                        IR_Module *parent,
                                        JZArena *arena)
{
    for (int i = 0; i < parent->num_instances; i++) {
        IR_Instance *inst = &parent->instances[i];
        if (inst->child_module_id < 0 || inst->child_module_id >= design->num_modules)
            continue;
        IR_Module *child = &design->modules[inst->child_module_id];

        /* Build a set of (parent_signal_id, msb, lsb) tuples from INPUT
         * ports on this instance. */
        typedef struct { int sig; int msb; int lsb; } InputSlice;
        int num_inputs = 0;
        InputSlice *inputs = NULL;

        for (int c = 0; c < inst->num_connections; c++) {
            const IR_InstanceConnection *conn = &inst->connections[c];
            if (conn->parent_signal_id < 0) continue;
            const IR_Signal *cp = find_signal_const(child, conn->child_port_id);
            if (!cp || cp->kind != SIG_PORT) continue;
            if (cp->u.port.direction != PORT_IN) continue;

            InputSlice *tmp = (InputSlice *)realloc(inputs,
                (size_t)(num_inputs + 1) * sizeof(InputSlice));
            if (!tmp) { free(inputs); inputs = NULL; num_inputs = 0; break; }
            inputs = tmp;
            inputs[num_inputs].sig = conn->parent_signal_id;
            inputs[num_inputs].msb = conn->parent_msb;
            inputs[num_inputs].lsb = conn->parent_lsb;
            num_inputs++;
        }

        if (num_inputs == 0) { free(inputs); continue; }

        /* For each OUTPUT port connection, check if it maps to the same
         * (parent_signal_id, msb, lsb) as any INPUT port. */
        for (int c = 0; c < inst->num_connections; c++) {
            const IR_InstanceConnection *conn = &inst->connections[c];
            if (conn->parent_signal_id < 0) continue;
            IR_Signal *cp = find_signal(child, conn->child_port_id);
            if (!cp || cp->kind != SIG_PORT) continue;
            if (cp->u.port.direction != PORT_OUT) continue;

            /* Check for loop: same parent signal/slice as an input. */
            int loops = 0;
            for (int j = 0; j < num_inputs; j++) {
                if (inputs[j].sig == conn->parent_signal_id &&
                    inputs[j].msb == conn->parent_msb &&
                    inputs[j].lsb == conn->parent_lsb) {
                    loops = 1;
                    break;
                }
            }
            if (!loops) continue;

            /* This output creates a combinational loop.
             * Create a dummy wire to absorb the assignment. */
            char dummy_name[192];
            snprintf(dummy_name, sizeof(dummy_name),
                     "_passthrough_sink_%s",
                     cp->name ? cp->name : "?");

            int dummy_id = next_signal_id(child);
            IR_Signal *dummy = add_signal(child, arena, dummy_id,
                dummy_name, SIG_NET, PORT_IN, cp->width,
                cp->source_line);
            if (!dummy) continue;

            /* Redirect assignments to the original port → dummy. */
            {
                void rewrite_lhs(IR_Stmt *s, int old_id, int new_id);
                rewrite_lhs(child->async_block, conn->child_port_id, dummy_id);
            }

            /* Convert port to input. Re-find after add_signal realloc. */
            cp = find_signal(child, conn->child_port_id);
            if (cp) {
                cp->u.port.direction = PORT_IN;
            }
        }

        free(inputs);
    }
    return 0;
}

/* ======================================================================== */
/*  Check if any assignment targets a given signal ID                         */
/* ======================================================================== */

static bool stmt_has_any_assignment_to(const IR_Stmt *s, int signal_id)
{
    if (!s) return false;
    switch (s->kind) {
    case STMT_ASSIGNMENT:
        return s->u.assign.lhs_signal_id == signal_id;
    case STMT_BLOCK:
        for (int i = 0; i < s->u.block.count; i++)
            if (stmt_has_any_assignment_to(&s->u.block.stmts[i], signal_id))
                return true;
        return false;
    case STMT_IF:
        return stmt_has_any_assignment_to(s->u.if_stmt.then_block, signal_id) ||
               stmt_has_any_assignment_to(s->u.if_stmt.elif_chain, signal_id) ||
               stmt_has_any_assignment_to(s->u.if_stmt.else_block, signal_id);
    case STMT_SELECT:
        for (int i = 0; i < s->u.select_stmt.num_cases; i++)
            if (stmt_has_any_assignment_to(s->u.select_stmt.cases[i].body, signal_id))
                return true;
        return false;
    default:
        return false;
    }
}

/* ======================================================================== */
/*  Recursive LHS rewrite: change assignment target signal ID                */
/* ======================================================================== */

void rewrite_lhs(IR_Stmt *s, int old_id, int new_id)
{
    if (!s) return;
    switch (s->kind) {
    case STMT_ASSIGNMENT:
        if (s->u.assign.lhs_signal_id == old_id)
            s->u.assign.lhs_signal_id = new_id;
        break;
    case STMT_BLOCK:
        for (int i = 0; i < s->u.block.count; i++)
            rewrite_lhs(&s->u.block.stmts[i], old_id, new_id);
        break;
    case STMT_IF:
        rewrite_lhs(s->u.if_stmt.then_block, old_id, new_id);
        rewrite_lhs(s->u.if_stmt.elif_chain, old_id, new_id);
        rewrite_lhs(s->u.if_stmt.else_block, old_id, new_id);
        break;
    case STMT_SELECT:
        for (int i = 0; i < s->u.select_stmt.num_cases; i++)
            rewrite_lhs(s->u.select_stmt.cases[i].body, old_id, new_id);
        break;
    default:
        break;
    }
}

/* ======================================================================== */
/*  Handle non-shared (single-driver) z signals                              */
/* ======================================================================== */

static bool expr_has_z(const IR_Expr *expr)
{
    if (!expr) return false;
    if (expr->kind == EXPR_LITERAL) return expr->u.literal.literal.is_z != 0;
    switch (expr->kind) {
    case EXPR_UNARY_NOT: case EXPR_UNARY_NEG: case EXPR_LOGICAL_NOT:
        return expr_has_z(expr->u.unary.operand);
    case EXPR_BINARY_ADD: case EXPR_BINARY_SUB: case EXPR_BINARY_MUL:
    case EXPR_BINARY_DIV: case EXPR_BINARY_MOD:
    case EXPR_BINARY_AND: case EXPR_BINARY_OR:  case EXPR_BINARY_XOR:
    case EXPR_BINARY_SHL: case EXPR_BINARY_SHR: case EXPR_BINARY_ASHR:
    case EXPR_BINARY_EQ:  case EXPR_BINARY_NEQ:
    case EXPR_BINARY_LT:  case EXPR_BINARY_GT:
    case EXPR_BINARY_LTE: case EXPR_BINARY_GTE:
    case EXPR_LOGICAL_AND: case EXPR_LOGICAL_OR:
        return expr_has_z(expr->u.binary.left) || expr_has_z(expr->u.binary.right);
    case EXPR_TERNARY:
        return expr_has_z(expr->u.ternary.condition) ||
               expr_has_z(expr->u.ternary.true_val) ||
               expr_has_z(expr->u.ternary.false_val);
    case EXPR_CONCAT:
        for (int i = 0; i < expr->u.concat.num_operands; i++)
            if (expr_has_z(expr->u.concat.operands[i])) return true;
        return false;
    default:
        return false;
    }
}

static bool stmt_signal_still_has_z(const IR_Stmt *stmt, int signal_id)
{
    if (!stmt) return false;
    switch (stmt->kind) {
    case STMT_ASSIGNMENT:
        return (stmt->u.assign.lhs_signal_id == signal_id) && expr_has_z(stmt->u.assign.rhs);
    case STMT_BLOCK:
        for (int i = 0; i < stmt->u.block.count; i++)
            if (stmt_signal_still_has_z(&stmt->u.block.stmts[i], signal_id)) return true;
        return false;
    case STMT_IF:
        return stmt_signal_still_has_z(stmt->u.if_stmt.then_block, signal_id) ||
               stmt_signal_still_has_z(stmt->u.if_stmt.elif_chain, signal_id) ||
               stmt_signal_still_has_z(stmt->u.if_stmt.else_block, signal_id);
    case STMT_SELECT:
        for (int i = 0; i < stmt->u.select_stmt.num_cases; i++)
            if (stmt_signal_still_has_z(stmt->u.select_stmt.cases[i].body, signal_id)) return true;
        return false;
    default:
        return false;
    }
}

static void update_can_be_z(IR_Module *mod)
{
    for (int i = 0; i < mod->num_signals; i++) {
        IR_Signal *sig = &mod->signals[i];
        if (!sig->can_be_z) continue;
        if (mod->async_block && !stmt_signal_still_has_z(mod->async_block, sig->id)) {
            sig->can_be_z = false;
        }
    }
}

/* ======================================================================== */
/*  Deduplicate same-value PORT_OUT drivers on same parent wire              */
/* ======================================================================== */

/**
 * When a BUS SOURCE array has multiple elements mapped to the same parent
 * wire (e.g., arbiter tgt[5] and tgt[6] both on ram_bus), the PORT_OUT
 * signals (VALID, ADDR, CMD, PAGE) create multi-driver conflicts.
 *
 * For PORT_OUT ports in the same alias group (meaning they're all assigned
 * the same value via the = operator), we only need one copy on each parent
 * wire.  The redundant copies are redirected to per-instance dummy wires.
 *
 * PORT_INOUT ports are NOT touched — the merge_alias_connected_nets pass
 * handles those through pass-through detection.
 */
static int deduplicate_alias_group_drivers(IR_Design *design,
                                             IR_Module *parent,
                                             JZArena *arena)
{
    for (int i = 0; i < parent->num_instances; i++) {
        IR_Instance *inst = &parent->instances[i];
        if (inst->child_module_id < 0 || inst->child_module_id >= design->num_modules)
            continue;
        const IR_Module *child = &design->modules[inst->child_module_id];
        if (child->num_port_alias_groups == 0) continue;

        /* For each connection, check if there's a later connection from the
         * same instance on the same parent signal/slice whose child port is
         * in the same alias group (i.e., drives the same value). */
        for (int c1 = 0; c1 < inst->num_connections; c1++) {
            IR_InstanceConnection *conn1 = &inst->connections[c1];
            if (conn1->parent_signal_id < 0) continue;

            const IR_Signal *cp1 = find_signal_const(child, conn1->child_port_id);
            if (!cp1 || cp1->kind != SIG_PORT) continue;
            if (cp1->u.port.direction != PORT_OUT) continue;

            const IR_PortAliasGroup *grp1 = find_alias_group_for_port(
                child, conn1->child_port_id);
            if (!grp1) continue;

            /* Look for later connections in the same alias group on the
             * same parent signal/slice. */
            for (int c2 = c1 + 1; c2 < inst->num_connections; c2++) {
                IR_InstanceConnection *conn2 = &inst->connections[c2];
                if (conn2->parent_signal_id != conn1->parent_signal_id) continue;
                if (conn2->parent_msb != conn1->parent_msb) continue;
                if (conn2->parent_lsb != conn1->parent_lsb) continue;

                const IR_Signal *cp2 = find_signal_const(child, conn2->child_port_id);
                if (!cp2 || cp2->kind != SIG_PORT) continue;
                if (cp2->u.port.direction != PORT_OUT) continue;

                /* Check same alias group. */
                const IR_PortAliasGroup *grp2 = find_alias_group_for_port(
                    child, conn2->child_port_id);
                if (grp2 != grp1) continue;

                /* Redirect conn2 to a dummy wire in the parent. */
                char dummy_name[192];
                snprintf(dummy_name, sizeof(dummy_name),
                         "%s_%s_dup_sink", inst->name,
                         cp2->name ? cp2->name : "?");

                int dummy_id = next_signal_id(parent);
                IR_Signal *dummy = add_signal(parent, arena, dummy_id,
                    dummy_name, SIG_NET, PORT_IN, cp2->width,
                    cp2->source_line);
                if (!dummy) continue;

                conn2->parent_signal_id = dummy_id;
                conn2->parent_msb = -1;
                conn2->parent_lsb = -1;
            }
        }
    }
    return 0;
}

/* ======================================================================== */
/*  Public entry point                                                       */
/* ======================================================================== */

int jz_ir_tristate_transform(IR_Design *design,
                              JZArena *arena,
                              JZDiagnosticList *diagnostics)
{
    if (!design || design->tristate_default == TRISTATE_DEFAULT_NONE) return 0;

    int total_replaced = 0;

    /* Phase 0: Build port alias groups for all modules.
     * This detects groups of PORT signals aliased together (e.g., bus
     * interconnect modules like memory_map). The merge pass in Phase 1
     * uses these groups to unify shared nets. */
    for (int m = 0; m < design->num_modules; m++) {
        ir_build_port_alias_groups(&design->modules[m], arena);
    }

    /* Phase 0.5: Deduplicate same-value PORT_OUT drivers.
     * When BUS SOURCE array ports share a parent wire, PORT_OUT signals
     * (like ADDR, CMD, PAGE) in the same alias group drive the same value.
     * Keep one, redirect the rest to dummy wires. */
    for (int m = 0; m < design->num_modules; m++) {
        deduplicate_alias_group_drivers(design, &design->modules[m], arena);
    }

    /* Phase 1: Transform shared tri-state nets (multi-instance buses).
     * This splits INOUT ports into IN + OUT + OE and builds parent muxes. */
    for (int m = 0; m < design->num_modules; m++) {
        if (transform_shared_nets(design, &design->modules[m], arena, diagnostics) != 0)
            return 1;
    }

    /* Phase 1.5: Eliminate pass-through combinational loops.
     * After phase 1 identifies pass-through instances for tristate nets,
     * this pass handles non-tristate output ports on the same pass-through
     * instances that create combinational loops (e.g., tgt0_ADDR output
     * connected to the same wire as src_ADDR input). */
    for (int m = 0; m < design->num_modules; m++) {
        eliminate_passthrough_loops(design, &design->modules[m], arena);
    }

    /* Phase 1.6: Redirect orphaned assignments to PORT_IN signals.
     * The pass-through merge in phase 1 may convert PORT_OUT/PORT_INOUT
     * to PORT_IN (the parent's merged mux now drives the wire).  But the
     * child module's ASSIGN_ALIAS and procedural assignments that wrote
     * to those ports still exist in the IR.  Emit-time, that would produce
     * invalid Verilog ("assign input_port = expr;" or assigning to an input
     * in an always block).  Redirect them to dummy sink wires. */
    for (int m = 0; m < design->num_modules; m++) {
        IR_Module *mod = &design->modules[m];
        if (!mod->async_block) continue;

        int orig_num_signals = mod->num_signals;
        for (int s = 0; s < orig_num_signals; s++) {
            IR_Signal *sig = &mod->signals[s];
            if (sig->kind != SIG_PORT) continue;
            if (sig->u.port.direction != PORT_IN) continue;
            int sig_id = sig->id;

            /* Check if there's ANY assignment targeting this input port. */
            if (!stmt_has_any_assignment_to(mod->async_block, sig_id))
                continue;

            /* Create a dummy wire and redirect the assignment. */
            char dummy_name[192];
            snprintf(dummy_name, sizeof(dummy_name),
                     "_passthrough_sink_%s",
                     sig->name ? sig->name : "?");

            int dummy_id = next_signal_id(mod);
            add_signal(mod, arena, dummy_id,
                dummy_name, SIG_NET, PORT_IN, sig->width,
                sig->source_line);

            rewrite_lhs(mod->async_block, sig_id, dummy_id);
        }
    }

    /* Phase 2: Simple z-replacement for remaining non-shared signals.
     * After phase 1, shared ports are already split and redirected.
     * Any remaining z literals are on single-driver internal signals. */
    for (int m = 0; m < design->num_modules; m++) {
        IR_Module *mod = &design->modules[m];
        if (!mod->async_block) continue;

        /* Collect signals to exclude from z-replacement:
         * - All INOUT ports (they need tristate for bidirectional I/O).
         *   After phase 1, shared INOUT ports are already converted to
         *   PORT_IN, so only non-shared (single-driver) INOUT ports
         *   remain — these connect to physical bidirectional pads and
         *   must retain their z capability. */
        int num_excluded = 0;
        int *excluded = NULL;
        for (int s = 0; s < mod->num_signals; s++) {
            if (mod->signals[s].kind == SIG_PORT &&
                mod->signals[s].u.port.direction == PORT_INOUT) {
                num_excluded++;
            }
        }
        if (num_excluded > 0) {
            excluded = (int *)calloc((size_t)num_excluded, sizeof(int));
            int idx = 0;
            for (int s = 0; s < mod->num_signals; s++) {
                if (mod->signals[s].kind == SIG_PORT &&
                    mod->signals[s].u.port.direction == PORT_INOUT) {
                    excluded[idx++] = mod->signals[s].id;
                }
            }
        }

        int r = replace_z_in_stmt(mod->async_block, design->tristate_default,
                                   excluded, num_excluded);
        free(excluded);
        total_replaced += r;

        if (r > 0) {
            JZLocation loc;
            memset(&loc, 0, sizeof(loc));
            loc.filename = module_source_file(design, mod);
            loc.line = mod->source_line;
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "replaced %d tri-state (z) literal(s) with %s in module '%s'",
                     r,
                     design->tristate_default == TRISTATE_DEFAULT_GND ? "GND" : "VCC",
                     mod->name ? mod->name : "?");
            jz_diagnostic_report(diagnostics, loc, JZ_SEVERITY_NOTE,
                                 "INFO_TRISTATE_TRANSFORM", msg);
        }

        update_can_be_z(mod);
    }

    /* Phase 3: Post-transform validation. */
    if (validate_no_multidriver(design, diagnostics) > 0) {
        return 1;
    }

    if (total_replaced == 0) {
        /* Check if phase 1 did anything by looking for any transformed shared nets. */
        /* If nothing was done at all, warn. */
        int any_transform = 0;
        for (int m = 0; m < design->num_modules; m++) {
            for (int s = 0; s < design->modules[m].num_signals; s++) {
                const IR_Signal *sig = &design->modules[m].signals[s];
                if (sig->name && (strstr(sig->name, "_oe") || strstr(sig->name, "_out"))) {
                    any_transform = 1;
                    break;
                }
            }
            if (any_transform) break;
        }
        if (!any_transform) {
            JZLocation loc;
            memset(&loc, 0, sizeof(loc));
            jz_diagnostic_report(diagnostics, loc, JZ_SEVERITY_WARNING,
                                 "TRISTATE_TRANSFORM_UNUSED_DEFAULT",
                                 "--tristate-default specified but no internal tri-state nets found to transform");
        }
    }

    return 0;
}
