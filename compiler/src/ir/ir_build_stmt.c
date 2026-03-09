/**
 * @file ir_build_stmt.c
 * @brief Statement and control-flow lowering for IR construction.
 *
 * This file lowers AST statements into IR_Stmt structures, including:
 * - Assignments
 * - IF / ELIF / ELSE chains
 * - SELECT / CASE / DEFAULT statements
 * - Feature guards
 * - Nested blocks
 *
 * It also handles special lowering cases such as memory writes, guarded
 * latch assignments, and SELECT selector fallbacks.
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "ir_internal.h"
#include "../sem/driver_internal.h"

/**
 * @brief Map assignment block_kind strings to IR_AssignmentKind values.
 *
 * @param kind AST block_kind string.
 * @param out  Output assignment kind.
 * @return 0 on success, non-zero on failure.
 */
static int ir_map_assignment_kind(const char *kind, IR_AssignmentKind *out)
{
    if (!kind || !out) return -1;

    if (strcmp(kind, "ALIAS") == 0)       { *out = ASSIGN_ALIAS; return 0; }
    if (strcmp(kind, "ALIAS_Z") == 0)     { *out = ASSIGN_ALIAS_ZEXT; return 0; }
    if (strcmp(kind, "ALIAS_S") == 0)     { *out = ASSIGN_ALIAS_SEXT; return 0; }

    if (strcmp(kind, "DRIVE") == 0)       { *out = ASSIGN_DRIVE; return 0; }
    if (strcmp(kind, "DRIVE_Z") == 0)     { *out = ASSIGN_DRIVE_ZEXT; return 0; }
    if (strcmp(kind, "DRIVE_S") == 0)     { *out = ASSIGN_DRIVE_SEXT; return 0; }

    if (strcmp(kind, "RECEIVE") == 0)       { *out = ASSIGN_RECEIVE; return 0; }
    if (strcmp(kind, "RECEIVE_Z") == 0)     { *out = ASSIGN_RECEIVE_ZEXT; return 0; }
    if (strcmp(kind, "RECEIVE_S") == 0)     { *out = ASSIGN_RECEIVE_SEXT; return 0; }
    if (strcmp(kind, "RECEIVE_LATCH") == 0) { *out = ASSIGN_RECEIVE; return 0; }

    return -1;
}

/**
 * @brief Recursively propagate a target width to width-0 VCC/GND literals.
 *
 * When an assignment has a known LHS width (from slicing or symbol width)
 * but the RHS contains polymorphic VCC/GND literals with width 0 (e.g.,
 * inside a ternary whose type width could not be inferred), this helper
 * walks the expression tree and patches those literals in-place.
 */
static void ir_propagate_vcc_gnd_width(IR_Expr *expr, int target_width)
{
    if (!expr || target_width <= 0) return;

    if (expr->kind == EXPR_LITERAL && expr->const_name &&
        expr->width == 0 &&
        (strcmp(expr->const_name, "GND") == 0 || strcmp(expr->const_name, "VCC") == 0)) {
        expr->width = target_width;
        expr->u.literal.literal.width = target_width;
        return;
    }

    if (expr->kind == EXPR_TERNARY) {
        ir_propagate_vcc_gnd_width(expr->u.ternary.true_val, target_width);
        ir_propagate_vcc_gnd_width(expr->u.ternary.false_val, target_width);
    }
}

/**
 * @brief Build an IR statement for an assignment.
 *
 * Supports:
 * - Simple identifier assignments
 * - Literal-index slices (e.g. a[7:0])
 * - Memory writes of the form mem.port[addr] <= data
 * - Guarded latch assignments (D and SR latches)
 * - BUS member access on LHS (e.g. pbus.DATA <= value)
 *
 * Complex LHS forms such as concatenations are currently not lowered.
 *
 * @param arena           Arena for IR allocation.
 * @param stmt            AST assignment statement.
 * @param mod_scope       Module scope.
 * @param project_symbols Project-level symbols.
 * @param bus_map         BUS signal mapping array (may be NULL).
 * @param bus_map_count   Number of bus mapping entries.
 * @param diagnostics     Diagnostic sink.
 * @param next_assign_id  Monotonic assignment ID counter.
 * @param out             Output IR statement.
 * @return 0 on success, non-zero on failure.
 */
static int ir_build_assignment_stmt(JZArena *arena,
                             JZASTNode *stmt,
                             const JZModuleScope *mod_scope,
                             const JZBuffer *project_symbols,
                             const IR_BusSignalMapping *bus_map,
                             int bus_map_count,
                             JZDiagnosticList *diagnostics,
                             int *next_assign_id,
                             IR_Stmt *out)
{
    if (!arena || !stmt || !mod_scope || !next_assign_id || !out) return -1;
    if (stmt->type != JZ_AST_STMT_ASSIGN || stmt->child_count < 2) return -1;

    JZASTNode *lhs = stmt->children[0];
    JZASTNode *rhs = stmt->children[1];
    if (!lhs || !rhs) return -1;

    /* Special-case memory writes of the form mem.port[addr] <= data; into a
     * dedicated IR_MemWriteStmt so that backends can emit concrete RAM/ROM
     * write statements in the appropriate control-flow context.
     */
    if (lhs->type == JZ_AST_EXPR_SLICE && lhs->child_count >= 2) {
        JZASTNode *base = lhs->children[0];
        if (base && base->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER && base->name) {
            /* Split "mem.port" into memory and port names. */
            const char *full = base->name;
            const char *dot = strchr(full, '.');
            if (dot && dot > full && dot[1] != '\0') {
                size_t mem_len = (size_t)(dot - full);
                const char *port = dot + 1;

                /* Lower address and data expressions via the normal expr
                 * builder so that constructs like ram.write[operand] <= A
                 * or ram.write[operand] <= some_expr are preserved.
                 */
                JZASTNode *addr_node = lhs->children[1];
                if (addr_node) {
                    IR_Expr *addr_ir = ir_build_expr(arena,
                                                    addr_node,
                                                    mod_scope,
                                                    project_symbols,
                                                    bus_map,
                                                    bus_map_count,
                                                    diagnostics);
                    IR_Expr *data_ir = ir_build_expr(arena,
                                                    rhs,
                                                    mod_scope,
                                                    project_symbols,
                                                    bus_map,
                                                    bus_map_count,
                                                    diagnostics);
                    if (addr_ir && data_ir) {
                        char *mem_name = (char *)jz_arena_alloc(arena, mem_len + 1);
                        if (!mem_name) {
                            return -1;
                        }
                        memcpy(mem_name, full, mem_len);
                        mem_name[mem_len] = '\0';

                        char *port_name = ir_strdup_arena(arena, port);
                        if (!port_name) {
                            return -1;
                        }

                        memset(out, 0, sizeof(*out));
                        out->kind = STMT_MEM_WRITE;
                        out->source_line = stmt->loc.line;
                        out->u.mem_write.memory_name = mem_name;
                        out->u.mem_write.port_name = port_name;
                        out->u.mem_write.address = addr_ir;
                        out->u.mem_write.data = data_ir;
                        return 0;
                    }
                }
            }
        }
    }

    const JZSymbol *sym = NULL;
    int lhs_signal_id = -1;  /* For BUS access, we use this instead of sym->id */
    bool is_sliced_lhs = false;
    int lhs_msb = 0;
    int lhs_lsb = 0;

    if (lhs->type == JZ_AST_EXPR_IDENTIFIER && lhs->name) {
        /* Simple identifier LHS. */
        sym = module_scope_lookup(mod_scope, lhs->name);
        if (!sym || sym->id < 0) return -1;
        lhs_signal_id = sym->id;
    } else if (lhs->type == JZ_AST_EXPR_BUS_ACCESS) {
        /* BUS member access LHS: pbus.DATA or pbus[i].DATA */
        if (!bus_map || bus_map_count <= 0) {
            return -1;
        }

        JZBusAccessInfo info;
        memset(&info, 0, sizeof(info));
        if (sem_resolve_bus_access(lhs, mod_scope, project_symbols, &info, NULL) != 1) {
            return -1;
        }

        /* Dynamic bus array index on LHS: expand to per-element conditional
         * assignments. Each element gets: (index == i) ? rhs : {w}'bz
         */
        if (!info.index_known && info.is_array && info.count > 0) {
            /* Build the index expression. */
            IR_Expr *idx_expr = NULL;
            if (lhs->child_count > 0 && lhs->children[0]) {
                idx_expr = ir_build_expr(arena, lhs->children[0],
                                          mod_scope, project_symbols,
                                          bus_map, bus_map_count, diagnostics);
            }
            if (!idx_expr) return -1;

            /* Build the RHS expression. */
            IR_Expr *rhs_expr = ir_build_expr(arena, rhs, mod_scope, project_symbols,
                                               bus_map, bus_map_count, diagnostics);

            /* Resolve assignment kind. */
            IR_AssignmentKind ak = ASSIGN_ALIAS;
            if (stmt->block_kind) {
                if (ir_map_assignment_kind(stmt->block_kind, &ak) != 0) {
                    return -1;
                }
            }

            int count = (int)info.count;
            IR_Stmt *sub_stmts = (IR_Stmt *)jz_arena_alloc(arena,
                                     sizeof(IR_Stmt) * (size_t)count);
            if (!sub_stmts) return -1;
            memset(sub_stmts, 0, sizeof(IR_Stmt) * (size_t)count);

            for (int i = 0; i < count; ++i) {
                /* When array_count == 1, the bus_map stores array_index = -1
                 * (non-array style), so use -1 for single-element arrays. */
                int lookup_idx = (count > 1) ? i : -1;
                int elem_sig_id = ir_lookup_bus_signal_id(bus_map, bus_map_count,
                                                           lhs->name, info.signal_name, lookup_idx);
                if (elem_sig_id < 0) return -1;

                /* Find element width from bus_map. */
                int ew = 0;
                for (int m = 0; m < bus_map_count; ++m) {
                    if (bus_map[m].ir_signal_id == elem_sig_id) {
                        ew = bus_map[m].width;
                        break;
                    }
                }

                /* Build: (index == i) ? rhs : default_val
                 * For ALIAS (=), default is z (tristate).
                 * For DRIVE/RECEIVE (<=), default is 0 (no internal tristate).
                 */
                int use_z = (ak == ASSIGN_ALIAS || ak == ASSIGN_ALIAS_ZEXT ||
                             ak == ASSIGN_ALIAS_SEXT);

                /* Literal i */
                IR_Expr *lit_i = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
                if (!lit_i) return -1;
                memset(lit_i, 0, sizeof(*lit_i));
                lit_i->kind = EXPR_LITERAL;
                lit_i->width = idx_expr->width > 0 ? idx_expr->width : 32;
                lit_i->source_line = lhs->loc.line;
                lit_i->u.literal.literal.words[0] = (uint64_t)i;
                lit_i->u.literal.literal.width = lit_i->width;

                /* index == i */
                IR_Expr *eq = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
                if (!eq) return -1;
                memset(eq, 0, sizeof(*eq));
                eq->kind = EXPR_BINARY_EQ;
                eq->width = 1;
                eq->source_line = lhs->loc.line;
                eq->u.binary.left = idx_expr;
                eq->u.binary.right = lit_i;

                /* Default literal (z or 0) of element width */
                IR_Expr *def_lit = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
                if (!def_lit) return -1;
                memset(def_lit, 0, sizeof(*def_lit));
                def_lit->kind = EXPR_LITERAL;
                def_lit->width = ew;
                def_lit->source_line = lhs->loc.line;
                def_lit->u.literal.literal.width = ew;
                def_lit->u.literal.literal.is_z = use_z ? 1 : 0;
                def_lit->u.literal.literal.words[0] = 0;

                /* ternary: (index == i) ? rhs : default */
                IR_Expr *ternary = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
                if (!ternary) return -1;
                memset(ternary, 0, sizeof(*ternary));
                ternary->kind = EXPR_TERNARY;
                ternary->width = ew;
                ternary->source_line = lhs->loc.line;
                ternary->u.ternary.condition = eq;
                ternary->u.ternary.true_val = rhs_expr;
                ternary->u.ternary.false_val = def_lit;

                sub_stmts[i].kind = STMT_ASSIGNMENT;
                sub_stmts[i].source_line = stmt->loc.line;

                IR_Assignment *a = &sub_stmts[i].u.assign;
                a->id = (*next_assign_id)++;
                a->lhs_signal_id = elem_sig_id;
                a->rhs = ternary;
                a->is_sliced = false;
                a->lhs_msb = 0;
                a->lhs_lsb = 0;
                a->source_line = stmt->loc.line;
                a->kind = ak;
            }

            memset(out, 0, sizeof(*out));
            out->kind = STMT_BLOCK;
            out->source_line = stmt->loc.line;
            out->u.block.stmts = sub_stmts;
            out->u.block.count = count;
            return 0;
        }

        /* When the BUS array has only 1 element, the bus_map stores
         * array_index = -1 (non-array convention), so use -1 for lookup. */
        int array_index = info.index_known
            ? (info.count > 1 ? (int)info.index_value : -1)
            : -1;
        lhs_signal_id = ir_lookup_bus_signal_id(bus_map, bus_map_count,
                                                 lhs->name, info.signal_name,
                                                 array_index);
        if (lhs_signal_id < 0) {
            return -1;
        }
    } else if (lhs->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER && lhs->name) {
        /* Qualified identifier LHS: pbus.DONE (BUS member access via QualifiedIdentifier) */
        if (!bus_map || bus_map_count <= 0) {
            return -1;
        }

        const char *full = lhs->name;
        const char *dot = strchr(full, '.');
        if (!dot || dot == full || dot[1] == '\0') {
            return -1;
        }

        size_t port_len = (size_t)(dot - full);
        char port_name[256];
        if (port_len >= sizeof(port_name)) {
            return -1;
        }
        memcpy(port_name, full, port_len);
        port_name[port_len] = '\0';
        const char *signal_name = dot + 1;

        lhs_signal_id = ir_lookup_bus_signal_id(bus_map, bus_map_count,
                                                 port_name, signal_name, -1);
        if (lhs_signal_id < 0) {
            return -1;
        }
    } else if (lhs->type == JZ_AST_EXPR_SLICE && lhs->child_count >= 3) {
        /* Slice LHS: base must be an identifier bound to a PORT/WIRE/REGISTER
         * symbol, and both bounds must evaluate to non-negative integer
         * constants.  After template expansion, slice bounds may be
         * EXPR_BINARY trees (e.g., 0*11+10) rather than simple literals,
         * so we use ir_eval_slice_bound() which handles both cases.
         */
        JZASTNode *base = lhs->children[0];
        JZASTNode *msb = lhs->children[1];
        JZASTNode *lsb = lhs->children[2];
        if (!base || base->type != JZ_AST_EXPR_IDENTIFIER || !base->name) {
            return -1;
        }
        if (!msb || !lsb) {
            return -1;
        }

        /* Look up the base signal. Reject MUX and other non-signal symbols; the
         * semantic layer already reports appropriate errors for MUX and MEM
         * assignments, and IR currently models only signal and register/latch
         * assignments.
         */
        sym = module_scope_lookup(mod_scope, base->name);
        if (!sym || !sym->node || sym->id < 0) {
            return -1;
        }
        if (sym->kind != JZ_SYM_PORT &&
            sym->kind != JZ_SYM_WIRE &&
            sym->kind != JZ_SYM_REGISTER &&
            sym->kind != JZ_SYM_LATCH) {
            return -1;
        }
        lhs_signal_id = sym->id;

        unsigned msb_val = 0, lsb_val = 0;
        if (!ir_eval_slice_bound(msb, mod_scope, project_symbols, &msb_val) ||
            !ir_eval_slice_bound(lsb, mod_scope, project_symbols, &lsb_val)) {
            return -1;
        }

        is_sliced_lhs = true;
        lhs_msb = (int)msb_val;
        lhs_lsb = (int)lsb_val;
    } else if (lhs->type == JZ_AST_EXPR_CONCAT && lhs->child_count > 0) {
        /* Concat LHS decomposition: {carry, sum} <= rhs
         * Decompose into a STMT_BLOCK containing one sliced assignment per
         * concat element. Each element gets a slice of the RHS result.
         *
         * The concat elements are ordered MSB-first (leftmost element is the
         * most-significant portion). We build the RHS expression once, then
         * create N assignments each selecting the appropriate bit range.
         */
        IR_Expr *rhs_expr = ir_build_expr(arena, rhs, mod_scope, project_symbols,
                                           bus_map, bus_map_count, diagnostics);

        /* Resolve assignment kind from the statement. */
        IR_AssignmentKind ak = ASSIGN_ALIAS;
        if (stmt->block_kind) {
            if (ir_map_assignment_kind(stmt->block_kind, &ak) != 0) {
                return -1;
            }
        }

        /* First pass: resolve all concat element symbols and compute total width. */
        int num_elems = (int)lhs->child_count;
        int total_width = 0;
        for (int ei = 0; ei < num_elems; ++ei) {
            JZASTNode *elem = lhs->children[ei];
            if (!elem || elem->type != JZ_AST_EXPR_IDENTIFIER || !elem->name) {
                return -1;
            }
            const JZSymbol *esym = module_scope_lookup(mod_scope, elem->name);
            if (!esym || esym->id < 0) return -1;
            if (esym->kind != JZ_SYM_PORT &&
                esym->kind != JZ_SYM_WIRE &&
                esym->kind != JZ_SYM_REGISTER &&
                esym->kind != JZ_SYM_LATCH) {
                return -1;
            }
            unsigned ew = 0;
            if (!esym->node || !esym->node->width ||
                sem_eval_width_expr(esym->node->width, mod_scope, project_symbols, &ew) != 0 ||
                ew == 0) {
                return -1;
            }
            total_width += (int)ew;
        }

        /* Build a STMT_BLOCK with one assignment per concat element. */
        IR_Stmt *sub_stmts = (IR_Stmt *)jz_arena_alloc(arena, sizeof(IR_Stmt) * (size_t)num_elems);
        if (!sub_stmts) return -1;
        memset(sub_stmts, 0, sizeof(IR_Stmt) * (size_t)num_elems);

        int bit_offset = total_width; /* Start from MSB */
        for (int ei = 0; ei < num_elems; ++ei) {
            JZASTNode *elem = lhs->children[ei];
            const JZSymbol *esym = module_scope_lookup(mod_scope, elem->name);
            unsigned ew = 0;
            sem_eval_width_expr(esym->node->width, mod_scope, project_symbols, &ew);

            int elem_lsb = bit_offset - (int)ew;
            bit_offset = elem_lsb;

            /* Build a slice expression of the RHS for this element's bit range. */
            IR_Expr *slice_rhs = NULL;
            if (rhs_expr) {
                if ((int)ew == total_width) {
                    /* Single element spanning entire width; no slice needed. */
                    slice_rhs = rhs_expr;
                } else {
                    IR_Expr *s = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
                    if (s) {
                        memset(s, 0, sizeof(*s));
                        s->kind = EXPR_SLICE;
                        s->width = (int)ew;
                        s->source_line = rhs->loc.line;
                        /* Store the RHS expression as a temporary signal-less slice.
                         * We need to use signal_id = -1 to indicate this is an
                         * expression slice, not a signal slice. Use a concat wrapper
                         * to avoid losing the expression. Instead, create a proper
                         * bit selection: for the backend to emit correctly, we wrap
                         * the RHS in a STMT that slices the full expression.
                         *
                         * Actually, EXPR_SLICE only supports signal_id-based slicing.
                         * Instead, if the RHS width equals total_width, we can use
                         * shift + mask. But the simplest approach: build the RHS
                         * once as the full expression and then emit bit-select.
                         *
                         * For now, use a shift-right and mask approach:
                         * element = (rhs >> elem_lsb) & ((1 << ew) - 1)
                         * But IR doesn't have a mask node. Simplest: use the
                         * EXPR_SLICE on a temporary. But that requires a signal.
                         *
                         * Best approach for IR: Create an EXPR_SLICE node that uses
                         * signal_id = -2 as sentinel, and store the full rhs
                         * expression in the intrinsic.source field. But that's hacky.
                         *
                         * Actually the cleanest approach: build a shift + truncation.
                         * (rhs >> lsb)[ew-1:0] — but we can't slice an expression.
                         *
                         * Let me reconsider: the backend already handles EXPR_SLICE
                         * which only references signals. For expression slicing, we
                         * need to use shift+mask via binary ops.
                         */
                        memset(s, 0, sizeof(*s));
                        if (elem_lsb > 0) {
                            /* Build: (rhs >> elem_lsb) — shift right by lsb */
                            IR_Expr *shift_amt = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
                            if (!shift_amt) return -1;
                            memset(shift_amt, 0, sizeof(*shift_amt));
                            shift_amt->kind = EXPR_LITERAL;
                            shift_amt->width = 32;
                            shift_amt->source_line = rhs->loc.line;
                            shift_amt->u.literal.literal.words[0] = (unsigned long long)elem_lsb;
                            shift_amt->u.literal.literal.width = 32;

                            IR_Expr *shifted = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
                            if (!shifted) return -1;
                            memset(shifted, 0, sizeof(*shifted));
                            shifted->kind = EXPR_BINARY_SHR;
                            shifted->width = total_width;
                            shifted->source_line = rhs->loc.line;
                            shifted->u.binary.left = rhs_expr;
                            shifted->u.binary.right = shift_amt;

                            /* Build mask: {ew{1'b1}} as ((1 << ew) - 1) literal */
                            IR_Expr *mask = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
                            if (!mask) return -1;
                            memset(mask, 0, sizeof(*mask));
                            mask->kind = EXPR_LITERAL;
                            mask->width = (int)ew;
                            mask->source_line = rhs->loc.line;
                            mask->u.literal.literal.words[0] = (1ULL << ew) - 1ULL;
                            mask->u.literal.literal.width = (int)ew;

                            /* shifted & mask */
                            s->kind = EXPR_BINARY_AND;
                            s->width = (int)ew;
                            s->source_line = rhs->loc.line;
                            s->u.binary.left = shifted;
                            s->u.binary.right = mask;
                        } else {
                            /* LSB=0: just mask the lower bits */
                            IR_Expr *mask = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
                            if (!mask) return -1;
                            memset(mask, 0, sizeof(*mask));
                            mask->kind = EXPR_LITERAL;
                            mask->width = (int)ew;
                            mask->source_line = rhs->loc.line;
                            mask->u.literal.literal.words[0] = (1ULL << ew) - 1ULL;
                            mask->u.literal.literal.width = (int)ew;

                            s->kind = EXPR_BINARY_AND;
                            s->width = (int)ew;
                            s->source_line = rhs->loc.line;
                            s->u.binary.left = rhs_expr;
                            s->u.binary.right = mask;
                        }
                        slice_rhs = s;
                    }
                }
            }

            sub_stmts[ei].kind = STMT_ASSIGNMENT;
            sub_stmts[ei].source_line = stmt->loc.line;

            IR_Assignment *a = &sub_stmts[ei].u.assign;
            a->id = (*next_assign_id)++;
            a->lhs_signal_id = esym->id;
            a->rhs = slice_rhs;
            a->is_sliced = false;
            a->lhs_msb = 0;
            a->lhs_lsb = 0;
            a->source_line = stmt->loc.line;
            a->kind = ak;
        }

        memset(out, 0, sizeof(*out));
        out->kind = STMT_BLOCK;
        out->source_line = stmt->loc.line;
        out->u.block.stmts = sub_stmts;
        out->u.block.count = num_elems;
        return 0;
    } else {
        /* Other complex LHS forms are not yet lowered into IR assignments.
         * They are either rejected semantically or will be handled by future
         * IR construction work.
         */
        return -1;
    }

    /* Step 4b: Alias with dynamic bus RHS and write-only signal — generate
     * reverse per-element conditional assignments for the write direction.
     *
     * Example: tgt[0].DONE = src[oh2b(active_src)].DONE
     * DONE is write-only from the dynamic side:
     *   src0_DONE = (idx == 0) ? tgt0_DONE : 1'bz
     *   src1_DONE = (idx == 1) ? tgt0_DONE : 1'bz
     *
     * INOUT signals (readable AND writable, e.g. DATA) are NOT handled
     * here — generating per-element reverse conditionals for INOUT would
     * create multiple conflicting drivers (one per template iteration)
     * with identical guard conditions. INOUT falls through to the normal
     * flow, which builds a forward gslice only.
     */
    if (lhs_signal_id >= 0 &&
        rhs && rhs->type == JZ_AST_EXPR_BUS_ACCESS &&
        stmt->block_kind &&
        bus_map && bus_map_count > 0) {
        IR_AssignmentKind check_ak = ASSIGN_ALIAS;
        int is_alias = (ir_map_assignment_kind(stmt->block_kind, &check_ak) == 0 &&
                        (check_ak == ASSIGN_ALIAS ||
                         check_ak == ASSIGN_ALIAS_ZEXT ||
                         check_ak == ASSIGN_ALIAS_SEXT));
        if (is_alias) {
            JZBusAccessInfo rhs_info;
            memset(&rhs_info, 0, sizeof(rhs_info));
            if (sem_resolve_bus_access(rhs, mod_scope, project_symbols,
                                        &rhs_info, NULL) == 1 &&
                !rhs_info.index_known && rhs_info.is_array &&
                rhs_info.count > 0 &&
                rhs_info.writable && !rhs_info.readable) {

                /* Build index expression from the RHS bus access child. */
                IR_Expr *idx_expr = NULL;
                if (rhs->child_count > 0 && rhs->children[0]) {
                    idx_expr = ir_build_expr(arena, rhs->children[0],
                                              mod_scope, project_symbols,
                                              bus_map, bus_map_count, diagnostics);
                }
                if (!idx_expr) return -1;

                int count = (int)rhs_info.count;

                IR_Stmt *sub_stmts = (IR_Stmt *)jz_arena_alloc(arena,
                                         sizeof(IR_Stmt) * (size_t)count);
                if (!sub_stmts) return -1;
                memset(sub_stmts, 0, sizeof(IR_Stmt) * (size_t)count);

                /* Build LHS signal reference (source value for reverse assigns). */
                int lhs_width = 0;
                for (int m = 0; m < bus_map_count; ++m) {
                    if (bus_map[m].ir_signal_id == lhs_signal_id) {
                        lhs_width = bus_map[m].width;
                        break;
                    }
                }
                /* Fallback: try symbol width. */
                if (lhs_width <= 0 && sym && sym->node && sym->node->width) {
                    unsigned sw = 0;
                    if (sem_eval_width_expr(sym->node->width, mod_scope,
                                            project_symbols, &sw) == 0 && sw > 0) {
                        lhs_width = (int)sw;
                    }
                }

                IR_Expr *lhs_ref = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
                if (!lhs_ref) return -1;
                memset(lhs_ref, 0, sizeof(*lhs_ref));
                lhs_ref->kind = EXPR_SIGNAL_REF;
                lhs_ref->width = lhs_width;
                lhs_ref->source_line = lhs->loc.line;
                lhs_ref->u.signal_ref.signal_id = lhs_signal_id;

                /* Reverse: for each RHS element i, generate:
                 *   rhs_elem_i = (index == i) ? lhs_signal : {ew}'bz
                 */
                for (int i = 0; i < count; ++i) {
                    int lookup_idx = (count > 1) ? i : -1;
                    int elem_sig_id = ir_lookup_bus_signal_id(
                        bus_map, bus_map_count,
                        rhs->name, rhs_info.signal_name, lookup_idx);
                    if (elem_sig_id < 0) return -1;

                    int ew = 0;
                    for (int m = 0; m < bus_map_count; ++m) {
                        if (bus_map[m].ir_signal_id == elem_sig_id) {
                            ew = bus_map[m].width;
                            break;
                        }
                    }

                    /* Literal i */
                    IR_Expr *lit_i = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
                    if (!lit_i) return -1;
                    memset(lit_i, 0, sizeof(*lit_i));
                    lit_i->kind = EXPR_LITERAL;
                    lit_i->width = idx_expr->width > 0 ? idx_expr->width : 32;
                    lit_i->source_line = rhs->loc.line;
                    lit_i->u.literal.literal.words[0] = (uint64_t)i;
                    lit_i->u.literal.literal.width = lit_i->width;

                    /* index == i */
                    IR_Expr *eq = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
                    if (!eq) return -1;
                    memset(eq, 0, sizeof(*eq));
                    eq->kind = EXPR_BINARY_EQ;
                    eq->width = 1;
                    eq->source_line = rhs->loc.line;
                    eq->u.binary.left = idx_expr;
                    eq->u.binary.right = lit_i;

                    /* z literal */
                    IR_Expr *z_lit = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
                    if (!z_lit) return -1;
                    memset(z_lit, 0, sizeof(*z_lit));
                    z_lit->kind = EXPR_LITERAL;
                    z_lit->width = ew;
                    z_lit->source_line = rhs->loc.line;
                    z_lit->u.literal.literal.width = ew;
                    z_lit->u.literal.literal.is_z = 1;
                    z_lit->u.literal.literal.words[0] = 0;

                    /* ternary: (index == i) ? lhs_signal : z */
                    IR_Expr *ternary = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
                    if (!ternary) return -1;
                    memset(ternary, 0, sizeof(*ternary));
                    ternary->kind = EXPR_TERNARY;
                    ternary->width = ew;
                    ternary->source_line = rhs->loc.line;
                    ternary->u.ternary.condition = eq;
                    ternary->u.ternary.true_val = lhs_ref;
                    ternary->u.ternary.false_val = z_lit;

                    sub_stmts[i].kind = STMT_ASSIGNMENT;
                    sub_stmts[i].source_line = stmt->loc.line;
                    sub_stmts[i].u.assign.id = (*next_assign_id)++;
                    sub_stmts[i].u.assign.lhs_signal_id = elem_sig_id;
                    sub_stmts[i].u.assign.rhs = ternary;
                    sub_stmts[i].u.assign.is_sliced = false;
                    sub_stmts[i].u.assign.lhs_msb = 0;
                    sub_stmts[i].u.assign.lhs_lsb = 0;
                    sub_stmts[i].u.assign.source_line = stmt->loc.line;
                    sub_stmts[i].u.assign.kind = check_ak;
                }

                memset(out, 0, sizeof(*out));
                out->kind = STMT_BLOCK;
                out->source_line = stmt->loc.line;
                out->u.block.stmts = sub_stmts;
                out->u.block.count = count;
                return 0;
            }
        }
    }

    int is_lhs_latch = (sym && sym->kind == JZ_SYM_LATCH);
    int is_latch_guard = (stmt->block_kind && strcmp(stmt->block_kind, "RECEIVE_LATCH") == 0);

    IR_Expr *rhs_expr = NULL;
    if (is_lhs_latch && is_latch_guard && stmt->child_count >= 3) {
        /* Guarded latch assignment lowering depends on the declared latch type
         * (Section 4.8):
         *   - D latch:  <name> <= enable : data;  →  enable ? data : hold
         *   - SR latch: <name> <= set : reset;   →  (set & ~reset) | (hold & ~(set ^ reset))
         */
        const char *latch_type = (sym->node && sym->node->block_kind)
                               ? sym->node->block_kind
                               : "D";

        if (strcmp(latch_type, "SR") == 0) {
            /* SR latch: child[1] is set_expr, child[2] is reset_expr per
             * parser's guarded assignment layout.
             */
            JZASTNode *set_ast   = stmt->children[1];
            JZASTNode *reset_ast = stmt->children[2];
            IR_Expr *set_expr   = ir_build_expr(arena, set_ast,   mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
            IR_Expr *reset_expr = ir_build_expr(arena, reset_ast, mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
            IR_Expr *hold_expr  = ir_build_expr(arena, lhs,       mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
            if (!set_expr || !reset_expr || !hold_expr) {
                rhs_expr = NULL;
            } else {
                int w = set_expr->width;
                if (w <= 0) {
                    w = hold_expr->width;
                }

                /* term1 = set & ~reset */
                IR_Expr *not_reset = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
                IR_Expr *term1     = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
                /* xor_sr = set ^ reset; not_xor = ~(set ^ reset) */
                IR_Expr *xor_sr    = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
                IR_Expr *not_xor   = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
                /* term2 = hold & ~(set ^ reset) */
                IR_Expr *term2     = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
                /* final = term1 | term2 */
                IR_Expr *final_expr = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));

                if (!not_reset || !term1 || !xor_sr || !not_xor || !term2 || !final_expr) {
                    rhs_expr = NULL;
                } else {
                    memset(not_reset,  0, sizeof(*not_reset));
                    memset(term1,      0, sizeof(*term1));
                    memset(xor_sr,     0, sizeof(*xor_sr));
                    memset(not_xor,    0, sizeof(*not_xor));
                    memset(term2,      0, sizeof(*term2));
                    memset(final_expr, 0, sizeof(*final_expr));

                    not_reset->kind = EXPR_UNARY_NOT;
                    not_reset->width = reset_expr->width;
                    not_reset->source_line = reset_ast->loc.line;
                    not_reset->u.unary.operand = reset_expr;

                    term1->kind = EXPR_BINARY_AND;
                    term1->width = w;
                    term1->source_line = stmt->loc.line;
                    term1->u.binary.left  = set_expr;
                    term1->u.binary.right = not_reset;

                    xor_sr->kind = EXPR_BINARY_XOR;
                    xor_sr->width = w;
                    xor_sr->source_line = stmt->loc.line;
                    xor_sr->u.binary.left  = set_expr;
                    xor_sr->u.binary.right = reset_expr;

                    not_xor->kind = EXPR_UNARY_NOT;
                    not_xor->width = w;
                    not_xor->source_line = stmt->loc.line;
                    not_xor->u.unary.operand = xor_sr;

                    term2->kind = EXPR_BINARY_AND;
                    term2->width = w;
                    term2->source_line = stmt->loc.line;
                    term2->u.binary.left  = hold_expr;
                    term2->u.binary.right = not_xor;

                    final_expr->kind = EXPR_BINARY_OR;
                    final_expr->width = w;
                    final_expr->source_line = stmt->loc.line;
                    final_expr->u.binary.left  = term1;
                    final_expr->u.binary.right = term2;

                    rhs_expr = final_expr;
                }
            }
        } else {
            /* Default/D latch guarded assignment: enable : data. child[2] is
             * enable, child[1] is data per parser layout.
             */
            JZASTNode *enable_ast = stmt->children[2];
            JZASTNode *data_ast   = stmt->children[1];
            IR_Expr *cond_expr  = ir_build_expr(arena, enable_ast, mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
            IR_Expr *data_expr  = ir_build_expr(arena, data_ast,   mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
            IR_Expr *hold_expr  = ir_build_expr(arena, lhs,        mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
            if (!cond_expr || !data_expr || !hold_expr) {
                rhs_expr = NULL;
            } else {
                IR_Expr *tern = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
                if (!tern) {
                    rhs_expr = NULL;
                } else {
                    memset(tern, 0, sizeof(*tern));
                    tern->kind = EXPR_TERNARY;
                    tern->width = (data_expr->width > 0) ? data_expr->width : hold_expr->width;
                    tern->source_line = stmt->loc.line;
                    tern->u.ternary.condition = cond_expr;
                    tern->u.ternary.true_val  = data_expr;
                    tern->u.ternary.false_val = hold_expr;
                    rhs_expr = tern;
                }
            }
        }
    } else {
        rhs_expr = ir_build_expr(arena, rhs, mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
    }

    /* Propagate LHS target width to any width-0 VCC/GND literals in the RHS.
     * This handles cases like data[5:3] <= blink[24] ? VCC : GND where the
     * ternary type width is 0 (polymorphic) but the LHS slice width is known.
     */
    if (rhs_expr) {
        int target_w = 0;
        if (is_sliced_lhs) {
            target_w = lhs_msb - lhs_lsb + 1;
        } else if (sym) {
            unsigned sw = 0;
            if (sym->node && sym->node->width &&
                sem_eval_width_expr(sym->node->width, mod_scope, project_symbols, &sw) == 0 &&
                sw > 0) {
                target_w = (int)sw;
            }
        }
        if (target_w > 0) {
            ir_propagate_vcc_gnd_width(rhs_expr, target_w);
        }
    }

    /* Even if expression lowering fails for the RHS, still record an
     * assignment statement so the IR preserves the presence of the
     * assignment (rhs will appear as null in JSON).
     */

    memset(out, 0, sizeof(*out));
    out->kind = STMT_ASSIGNMENT;
    out->source_line = stmt->loc.line;

    IR_Assignment *a = &out->u.assign;
    a->id = (*next_assign_id)++;
    a->lhs_signal_id = lhs_signal_id;  /* Use tracked ID; already set for all LHS forms */
    a->rhs = rhs_expr; /* may be NULL when expression lowering is incomplete */
    a->is_sliced = is_sliced_lhs;
    a->lhs_msb = lhs_msb;
    a->lhs_lsb = lhs_lsb;
    a->source_line = stmt->loc.line;

    IR_AssignmentKind ak = ASSIGN_ALIAS;
    if (stmt->block_kind) {
        if (ir_map_assignment_kind(stmt->block_kind, &ak) != 0) {
            return -1;
        }
    }
    a->kind = ak;

    return 0;
}

/**
 * @brief Build an IR statement representing an IF/ELIF/ELSE chain.
 *
 * The chain is represented as a single STMT_IF with a linked elif_chain
 * and optional else_block.
 *
 * @param arena           Arena for IR allocation.
 * @param block           Parent AST block.
 * @param start_index     Index of the initial IF node.
 * @param end_index       Index one past the end of the chain.
 * @param mod_scope       Module scope.
 * @param project_symbols Project-level symbols.
 * @param bus_map         BUS signal mapping array (may be NULL).
 * @param bus_map_count   Number of bus mapping entries.
 * @param diagnostics     Diagnostic sink.
 * @param next_assign_id  Assignment ID counter.
 * @param out             Output IR statement.
 * @return 0 on success, non-zero on failure.
 */
static int ir_build_if_chain_stmt(JZArena *arena,
                           JZASTNode *block,
                           size_t start_index,
                           size_t end_index,
                           const JZModuleScope *mod_scope,
                           const JZBuffer *project_symbols,
                           const IR_BusSignalMapping *bus_map,
                           int bus_map_count,
                           JZDiagnosticList *diagnostics,
                           int *next_assign_id,
                           IR_Stmt *out)
{
    if (!arena || !block || !mod_scope || !next_assign_id || !out) return -1;
    if (end_index <= start_index) return -1;

    JZASTNode *if_node = block->children[start_index];
    if (!if_node || if_node->type != JZ_AST_STMT_IF || if_node->child_count == 0) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->kind = STMT_IF;
    out->source_line = if_node->loc.line;

    IR_IfStmt *ir_if = &out->u.if_stmt;

    /* IF condition */
    JZASTNode *cond = if_node->children[0];
    ir_if->condition = ir_build_expr(arena, cond, mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
    if (!ir_if->condition) {
        return -1;
    }

    /* IF body: children[1..] */
    if (if_node->child_count > 1) {
        JZASTNode fake_block = {0};
        fake_block.type = JZ_AST_BLOCK;
        fake_block.loc = if_node->loc;
        fake_block.children = &if_node->children[1];
        fake_block.child_count = if_node->child_count - 1;
        ir_if->then_block = ir_build_block_from_node(arena,
                                                     &fake_block,
                                                     mod_scope,
                                                     project_symbols,
                                                     bus_map,
                                                     bus_map_count,
                                                     diagnostics,
                                                     next_assign_id);
    } else {
        ir_if->then_block = NULL;
    }

    /* ELIF chain and optional ELSE. */
    IR_Stmt *first_elif = NULL;
    IR_Stmt *prev_elif = NULL;
    IR_Stmt *else_block_ir = NULL;

    for (size_t i = start_index + 1; i < end_index; ++i) {
        JZASTNode *node = block->children[i];
        if (!node) continue;

        if (node->type == JZ_AST_STMT_ELIF) {
            if (node->child_count == 0) {
                continue;
            }
            IR_Stmt *elif_stmt = (IR_Stmt *)jz_arena_alloc(arena, sizeof(IR_Stmt));
            if (!elif_stmt) {
                continue;
            }
            memset(elif_stmt, 0, sizeof(*elif_stmt));
            elif_stmt->kind = STMT_IF;
            elif_stmt->source_line = node->loc.line;

            IR_IfStmt *elif_ir = &elif_stmt->u.if_stmt;

            JZASTNode *elif_cond = node->children[0];
            elif_ir->condition = ir_build_expr(arena, elif_cond, mod_scope, project_symbols, bus_map, bus_map_count, diagnostics);
            if (!elif_ir->condition) {
                continue;
            }

            if (node->child_count > 1) {
                JZASTNode fake_block = {0};
                fake_block.type = JZ_AST_BLOCK;
                fake_block.loc = node->loc;
                fake_block.children = &node->children[1];
                fake_block.child_count = node->child_count - 1;
                elif_ir->then_block = ir_build_block_from_node(arena,
                                                                &fake_block,
                                                                mod_scope,
                                                                project_symbols,
                                                                bus_map,
                                                                bus_map_count,
                                                                diagnostics,
                                                                next_assign_id);
            } else {
                elif_ir->then_block = NULL;
            }

            if (!first_elif) {
                first_elif = elif_stmt;
            } else if (prev_elif) {
                prev_elif->u.if_stmt.elif_chain = elif_stmt;
            }
            prev_elif = elif_stmt;
        } else if (node->type == JZ_AST_STMT_ELSE) {
            JZASTNode fake_block = {0};
            fake_block.type = JZ_AST_BLOCK;
            fake_block.loc = node->loc;
            fake_block.children = node->children;
            fake_block.child_count = node->child_count;
            else_block_ir = ir_build_block_from_node(arena,
                                                     &fake_block,
                                                     mod_scope,
                                                     project_symbols,
                                                     bus_map,
                                                     bus_map_count,
                                                     diagnostics,
                                                     next_assign_id);
        }
    }

    ir_if->elif_chain = first_elif;
    ir_if->else_block = else_block_ir;
    return 0;
}

/**
 * @brief Build an IR statement for a SELECT block.
 *
 * Lowers SELECT / CASE / DEFAULT constructs into IR_SelectStmt with
 * case values and per-case statement blocks.
 *
 * @param arena           Arena for IR allocation.
 * @param select_node     AST SELECT node.
 * @param mod_scope       Module scope.
 * @param project_symbols Project-level symbols.
 * @param bus_map         BUS signal mapping array (may be NULL).
 * @param bus_map_count   Number of bus mapping entries.
 * @param diagnostics     Diagnostic sink.
 * @param next_assign_id  Assignment ID counter.
 * @param out             Output IR statement.
 * @return 0 on success, non-zero on failure.
 */
static int ir_build_select_stmt(JZArena *arena,
                         JZASTNode *select_node,
                         const JZModuleScope *mod_scope,
                         const JZBuffer *project_symbols,
                         const IR_BusSignalMapping *bus_map,
                         int bus_map_count,
                         JZDiagnosticList *diagnostics,
                         int *next_assign_id,
                         IR_Stmt *out)
{
    (void)next_assign_id; /* assignments inside cases are handled by nested blocks */
    if (!arena || !select_node || !mod_scope || !out) return -1;
    if (select_node->type != JZ_AST_STMT_SELECT || select_node->child_count == 0) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->kind = STMT_SELECT;
    out->source_line = select_node->loc.line;

    IR_SelectStmt *sel = &out->u.select_stmt;

    /* Selector expression is child[0]. */
    sel->selector = ir_build_expr(arena,
                                  select_node->children[0],
                                  mod_scope,
                                  project_symbols,
                                  bus_map,
                                  bus_map_count,
                                  diagnostics);
    if (!sel->selector) {
        return -1;
    }

    /* Count CASE/DEFAULT arms. */
    int case_count = 0;
    for (size_t i = 1; i < select_node->child_count; ++i) {
        JZASTNode *child = select_node->children[i];
        if (!child) continue;
        if (child->type == JZ_AST_STMT_CASE || child->type == JZ_AST_STMT_DEFAULT) {
            case_count++;
        }
    }

    if (case_count == 0) {
        sel->cases = NULL;
        sel->num_cases = 0;
        return 0;
    }

    sel->cases = (IR_SelectCase *)jz_arena_alloc(arena, sizeof(IR_SelectCase) * (size_t)case_count);
    if (!sel->cases) {
        return -1;
    }
    memset(sel->cases, 0, sizeof(IR_SelectCase) * (size_t)case_count);
    sel->num_cases = case_count;

    int out_idx = 0;
    for (size_t i = 1; i < select_node->child_count && out_idx < case_count; ++i) {
        JZASTNode *child = select_node->children[i];
        if (!child) continue;
        if (child->type != JZ_AST_STMT_CASE && child->type != JZ_AST_STMT_DEFAULT) {
            continue;
        }

        IR_SelectCase *c = &sel->cases[out_idx];
        memset(c, 0, sizeof(*c));

        size_t body_start = 0;
        int has_explicit_body_ast = 0;
        if (child->type == JZ_AST_STMT_CASE) {
            if (child->child_count == 0) {
                continue;
            }
            /* CASE value expression at children[0]. */
            c->case_value = ir_build_expr(arena,
                                          child->children[0],
                                          mod_scope,
                                          project_symbols,
                                          bus_map,
                                          bus_map_count,
                                          diagnostics);
            if (!c->case_value) {
                continue;
            }
            body_start = 1;
            has_explicit_body_ast = (child->child_count > body_start);
        } else {
            /* DEFAULT: represented with NULL case_value. */
            c->case_value = NULL;
            body_start = 0;
            has_explicit_body_ast = (child->child_count > body_start);
        }

        JZASTNode fake_block = {0};
        fake_block.type = JZ_AST_BLOCK;
        fake_block.loc = child->loc;
        if (child->child_count > body_start) {
            fake_block.children = &child->children[body_start];
            fake_block.child_count = child->child_count - body_start;
        } else {
            fake_block.children = NULL;
            fake_block.child_count = 0;
        }

        c->body = ir_build_block_from_node(arena,
                                           &fake_block,
                                           mod_scope,
                                           project_symbols,
                                           bus_map,
                                           bus_map_count,
                                           diagnostics,
                                           next_assign_id);
        if (has_explicit_body_ast && !c->body) {
            IR_Stmt *empty_block = (IR_Stmt *)jz_arena_alloc(arena, sizeof(IR_Stmt));
            if (!empty_block) {
                continue;
            }
            memset(empty_block, 0, sizeof(*empty_block));
            empty_block->kind = STMT_BLOCK;
            empty_block->source_line = child->loc.line;
            empty_block->u.block.stmts = NULL;
            empty_block->u.block.count = 0;
            c->body = empty_block;
        }
        out_idx++;
    }

    sel->num_cases = out_idx;
    return 0;
}

/**
 * @brief Build a SELECT statement using a previously assigned register as selector.
 *
 * This fallback is used when the SELECT selector expression cannot be
 * lowered directly (e.g. mem.read[...] expressions).
 *
 * @param arena           Arena for IR allocation.
 * @param block_node      Parent AST block.
 * @param select_node     AST SELECT node.
 * @param mod_scope       Module scope.
 * @param project_symbols Project-level symbols.
 * @param bus_map         BUS signal mapping array (may be NULL).
 * @param bus_map_count   Number of bus mapping entries.
 * @param diagnostics     Diagnostic sink.
 * @param next_assign_id  Assignment ID counter.
 * @param out             Output IR statement.
 * @return 0 on success, non-zero on failure.
 */
static int ir_build_select_stmt_with_reg_selector(JZArena *arena,
                                           JZASTNode *block_node,
                                           JZASTNode *select_node,
                                           const JZModuleScope *mod_scope,
                                           const JZBuffer *project_symbols,
                                           const IR_BusSignalMapping *bus_map,
                                           int bus_map_count,
                                           JZDiagnosticList *diagnostics,
                                           int *next_assign_id,
                                           IR_Stmt *out)
{
    if (!arena || !block_node || !select_node || !mod_scope || !out) {
        return -1;
    }
    if (select_node->type != JZ_AST_STMT_SELECT || select_node->child_count == 0) {
        return -1;
    }

    /* Identify the selector AST expression. */
    JZASTNode *sel_expr = select_node->children[0];
    if (!sel_expr) {
        return -1;
    }

    /* Find the index of this SELECT node within its parent block. */
    size_t select_index = (size_t)-1;
    for (size_t i = 0; i < block_node->child_count; ++i) {
        if (block_node->children[i] == select_node) {
            select_index = i;
            break;
        }
    }
    if (select_index == (size_t)-1) {
        return -1;
    }

    /* Scan prior statements in the same block for an assignment whose RHS is
     * exactly the same AST node as the selector expression. This covers
     * patterns like:
     *   instr <= rom.read[PC];
     *   SELECT (rom.read[PC]) { ... }
     */
    const JZSymbol *reg_sym = NULL;
    for (size_t i = 0; i < select_index; ++i) {
        JZASTNode *s = block_node->children[i];
        if (!s || s->type != JZ_AST_STMT_ASSIGN || s->child_count < 2) {
            continue;
        }
        JZASTNode *lhs = s->children[0];
        JZASTNode *rhs = s->children[1];
        if (!lhs || !rhs) continue;
        if (rhs != sel_expr) {
            continue;
        }
        if (lhs->type != JZ_AST_EXPR_IDENTIFIER || !lhs->name) {
            continue;
        }
        const JZSymbol *sym = module_scope_lookup(mod_scope, lhs->name);
        if (!sym || sym->id < 0) {
            continue;
        }
        if (sym->kind != JZ_SYM_PORT &&
            sym->kind != JZ_SYM_WIRE &&
            sym->kind != JZ_SYM_REGISTER &&
            sym->kind != JZ_SYM_LATCH) {
            continue;
        }
        reg_sym = sym;
        break;
    }

    if (!reg_sym) {
        return -1;
    }

    /* Build an IR_Expr that simply references the previously assigned
     * register/net as the SELECT selector.
     */
    IR_Expr *selector_ir = (IR_Expr *)jz_arena_alloc(arena, sizeof(IR_Expr));
    if (!selector_ir) {
        return -1;
    }
    memset(selector_ir, 0, sizeof(*selector_ir));
    selector_ir->kind = EXPR_SIGNAL_REF;
    selector_ir->source_line = select_node->loc.line;
    selector_ir->u.signal_ref.signal_id = reg_sym->id;

    /* Infer width for the selector from the symbol's declared width. */
    unsigned w = 0;
    if (reg_sym->node && reg_sym->node->width &&
        sem_eval_width_expr(reg_sym->node->width,
                            mod_scope,
                            project_symbols,
                            &w) == 0 &&
        w > 0u) {
        selector_ir->width = (int)w;
    } else {
        selector_ir->width = 0; /* fall back to type inference in backend */
    }

    /* Now build the SELECT IR_Stmt as in ir_build_select_stmt, but using the
     * preconstructed selector_ir instead of lowering the original selector
     * expression (which may involve unsupported mem.read[...] slices).
     */
    memset(out, 0, sizeof(*out));
    out->kind = STMT_SELECT;
    out->source_line = select_node->loc.line;

    IR_SelectStmt *sel = &out->u.select_stmt;
    sel->selector = selector_ir;

    /* Count CASE/DEFAULT arms. */
    int case_count = 0;
    for (size_t i = 1; i < select_node->child_count; ++i) {
        JZASTNode *child = select_node->children[i];
        if (!child) continue;
        if (child->type == JZ_AST_STMT_CASE || child->type == JZ_AST_STMT_DEFAULT) {
            case_count++;
        }
    }
    if (case_count == 0) {
        sel->cases = NULL;
        sel->num_cases = 0;
        return 0;
    }

    sel->cases = (IR_SelectCase *)jz_arena_alloc(arena, sizeof(IR_SelectCase) * (size_t)case_count);
    if (!sel->cases) {
        return -1;
    }
    memset(sel->cases, 0, sizeof(IR_SelectCase) * (size_t)case_count);
    sel->num_cases = case_count;

    int out_idx = 0;
    for (size_t i = 1; i < select_node->child_count && out_idx < case_count; ++i) {
        JZASTNode *child = select_node->children[i];
        if (!child) continue;
        if (child->type != JZ_AST_STMT_CASE && child->type != JZ_AST_STMT_DEFAULT) {
            continue;
        }

        IR_SelectCase *c = &sel->cases[out_idx];
        memset(c, 0, sizeof(*c));

        size_t body_start = 0;
        int has_explicit_body_ast = 0;
        if (child->type == JZ_AST_STMT_CASE) {
            if (child->child_count == 0) {
                continue;
            }
            /* CASE value expression at children[0]. */
            c->case_value = ir_build_expr(arena,
                                          child->children[0],
                                          mod_scope,
                                          project_symbols,
                                          bus_map,
                                          bus_map_count,
                                          diagnostics);
            if (!c->case_value) {
                continue;
            }
            body_start = 1;
            has_explicit_body_ast = (child->child_count > body_start);
        } else {
            /* DEFAULT: represented with NULL case_value. */
            c->case_value = NULL;
            body_start = 0;
            has_explicit_body_ast = (child->child_count > body_start);
        }

        JZASTNode fake_block = {0};
        fake_block.type = JZ_AST_BLOCK;
        fake_block.loc = child->loc;
        if (child->child_count > body_start) {
            fake_block.children = &child->children[body_start];
            fake_block.child_count = child->child_count - body_start;
        } else {
            fake_block.children = NULL;
            fake_block.child_count = 0;
        }

        c->body = ir_build_block_from_node(arena,
                                           &fake_block,
                                           mod_scope,
                                           project_symbols,
                                           bus_map,
                                           bus_map_count,
                                           diagnostics,
                                           next_assign_id);
        if (has_explicit_body_ast && !c->body) {
            IR_Stmt *empty_block = (IR_Stmt *)jz_arena_alloc(arena, sizeof(IR_Stmt));
            if (!empty_block) {
                continue;
            }
            memset(empty_block, 0, sizeof(*empty_block));
            empty_block->kind = STMT_BLOCK;
            empty_block->source_line = child->loc.line;
            empty_block->u.block.stmts = NULL;
            empty_block->u.block.count = 0;
            c->body = empty_block;
        }
        out_idx++;
    }

    sel->num_cases = out_idx;
    return 0;
}


/**
 * @brief Select the active branch of a feature guard.
 *
 * Evaluates the feature condition as a constant expression and returns
 * the AST block corresponding to the active branch.
 *
 * @param feature         AST feature guard node.
 * @param mod_scope       Module scope.
 * @param project_symbols Project-level symbols.
 * @param diagnostics     Diagnostic sink (unused).
 * @return AST node of the active branch, or NULL if none.
 */
static JZASTNode *ir_feature_active_block(JZASTNode *feature,
                                   const JZModuleScope *mod_scope,
                                   const JZBuffer *project_symbols,
                                   JZDiagnosticList *diagnostics)
{
    (void)diagnostics; /* semantic checks already reported earlier. */
    if (!feature || feature->type != JZ_AST_FEATURE_GUARD) return NULL;
    if (!mod_scope) return NULL;

    if (feature->child_count < 2) {
        return NULL;
    }

    JZASTNode *cond = feature->children[0];
    JZASTNode *then_block = (feature->child_count > 1) ? feature->children[1] : NULL;
    JZASTNode *else_block = (feature->child_count > 2) ? feature->children[2] : NULL;

    if (!cond) {
        return then_block;
    }

    char *expr_text = NULL;
    if (ir_expr_to_const_expr_string(cond, &expr_text) != 0 || !expr_text) {
        if (expr_text) free(expr_text);
        return then_block;
    }

    long long value = 0;
    if (sem_eval_const_expr_in_module(expr_text,
                                      mod_scope,
                                      project_symbols,
                                      &value) != 0) {
        free(expr_text);
        return then_block;
    }
    free(expr_text);

    if (value != 0) {
        return then_block;
    }
    return else_block;
}

/**
 * @brief Lower an AST block node into an IR statement block.
 *
 * Handles assignments, control-flow statements, feature guards, and
 * nested blocks. IF/ELIF/ELSE chains are collapsed into single IR nodes.
 *
 * @param arena           Arena for IR allocation.
 * @param block_node      AST block node.
 * @param mod_scope       Module scope.
 * @param project_symbols Project-level symbols.
 * @param bus_map         BUS signal mapping array (may be NULL).
 * @param bus_map_count   Number of bus mapping entries.
 * @param diagnostics     Diagnostic sink.
 * @param next_assign_id  Assignment ID counter.
 * @return Pointer to IR STMT_BLOCK, or NULL if empty.
 */
IR_Stmt *ir_build_block_from_node(JZArena *arena,
                                  JZASTNode *block_node,
                                  const JZModuleScope *mod_scope,
                                  const JZBuffer *project_symbols,
                                  const IR_BusSignalMapping *bus_map,
                                  int bus_map_count,
                                  JZDiagnosticList *diagnostics,
                                  int *next_assign_id)
{
    if (!arena || !block_node || !mod_scope || !next_assign_id) return NULL;

    /* First pass: count how many IR statements we will emit from this block.
     * We treat an IF/ELIF/ELSE chain as a single IR_Stmt, mirroring the
     * semantic analyses in sem/driver_flow.c.
     */
    size_t stmt_count = 0;
    for (size_t i = 0; i < block_node->child_count; ++i) {
        JZASTNode *stmt = block_node->children[i];
        if (!stmt) continue;

        switch (stmt->type) {
        case JZ_AST_STMT_ASSIGN:
            stmt_count++;
            break;

        case JZ_AST_FEATURE_GUARD:
            /* Feature guards lower to a single nested STMT_BLOCK for the
             * selected branch (or nothing if both branches are empty).
             */
            stmt_count++;
            break;

        case JZ_AST_STMT_IF: {
            size_t start = i;
            size_t end = i + 1;
            while (end < block_node->child_count) {
                JZASTNode *next = block_node->children[end];
                if (!next) {
                    end++;
                    continue;
                }
                if (next->type == JZ_AST_STMT_ELIF || next->type == JZ_AST_STMT_ELSE) {
                    end++;
                    if (next->type == JZ_AST_STMT_ELSE) {
                        break;
                    }
                } else {
                    break;
                }
            }
            (void)start; /* suppress unused warning if logic changes */
            stmt_count++;
            i = end - 1;
            break;
        }

        case JZ_AST_STMT_SELECT:
            stmt_count++;
            break;

        case JZ_AST_BLOCK:
            /* Nested anonymous block. */
            stmt_count++;
            break;

        default:
            break;
        }
    }

    if (stmt_count == 0) {
        return NULL;
    }

    IR_Stmt *block = (IR_Stmt *)jz_arena_alloc(arena, sizeof(IR_Stmt));
    if (!block) return NULL;
    memset(block, 0, sizeof(*block));
    block->kind = STMT_BLOCK;
    block->source_line = block_node->loc.line;

    IR_Stmt *stmts = (IR_Stmt *)jz_arena_alloc(arena, sizeof(IR_Stmt) * stmt_count);
    if (!stmts) return NULL;
    memset(stmts, 0, sizeof(IR_Stmt) * stmt_count);

    block->u.block.stmts = stmts;

    size_t out_idx = 0;
    for (size_t i = 0; i < block_node->child_count && out_idx < stmt_count; ++i) {
        JZASTNode *stmt = block_node->children[i];
        if (!stmt) continue;

        switch (stmt->type) {
        case JZ_AST_STMT_ASSIGN:
            if (ir_build_assignment_stmt(arena,
                                         stmt,
                                         mod_scope,
                                         project_symbols,
                                         bus_map,
                                         bus_map_count,
                                         diagnostics,
                                         next_assign_id,
                                         &stmts[out_idx]) == 0) {
                out_idx++;
            }
            break;

        case JZ_AST_FEATURE_GUARD: {
            /* Evaluate the feature condition in the module/project context and
             * lower only the active branch body into IR. The inactive branch
             * is completely omitted from the IR.
             */
            JZASTNode *active = ir_feature_active_block(stmt,
                                                        mod_scope,
                                                        project_symbols,
                                                        diagnostics);
            if (active) {
                IR_Stmt *nested = ir_build_block_from_node(arena,
                                                           active,
                                                           mod_scope,
                                                           project_symbols,
                                                           bus_map,
                                                           bus_map_count,
                                                           diagnostics,
                                                           next_assign_id);
                if (nested) {
                    stmts[out_idx++] = *nested;
                }
            }
            break;
        }

        case JZ_AST_STMT_IF: {
            size_t start = i;
            size_t end = i + 1;
            while (end < block_node->child_count) {
                JZASTNode *next = block_node->children[end];
                if (!next) {
                    end++;
                    continue;
                }
                if (next->type == JZ_AST_STMT_ELIF || next->type == JZ_AST_STMT_ELSE) {
                    end++;
                    if (next->type == JZ_AST_STMT_ELSE) {
                        break;
                    }
                } else {
                    break;
                }
            }
            if (ir_build_if_chain_stmt(arena,
                                       block_node,
                                       start,
                                       end,
                                       mod_scope,
                                       project_symbols,
                                       bus_map,
                                       bus_map_count,
                                       diagnostics,
                                       next_assign_id,
                                       &stmts[out_idx]) == 0) {
                out_idx++;
            }
            i = end - 1;
            break;
        }

        case JZ_AST_STMT_SELECT:
            if (ir_build_select_stmt(arena,
                                     stmt,
                                     mod_scope,
                                     project_symbols,
                                     bus_map,
                                     bus_map_count,
                                     diagnostics,
                                     next_assign_id,
                                     &stmts[out_idx]) == 0) {
                out_idx++;
            } else if (ir_build_select_stmt_with_reg_selector(arena,
                                                               block_node,
                                                               stmt,
                                                               mod_scope,
                                                               project_symbols,
                                                               bus_map,
                                                               bus_map_count,
                                                               diagnostics,
                                                               next_assign_id,
                                                               &stmts[out_idx]) == 0) {
                out_idx++;
            }
            break;

        case JZ_AST_BLOCK: {
            IR_Stmt *nested = ir_build_block_from_node(arena,
                                                       stmt,
                                                       mod_scope,
                                                       project_symbols,
                                                       bus_map,
                                                       bus_map_count,
                                                       diagnostics,
                                                       next_assign_id);
            if (nested) {
                stmts[out_idx++] = *nested;
            }
            break;
        }

        default:
            break;
        }
    }

    block->u.block.count = (int)out_idx;
    if (out_idx == 0) {
        return NULL;
    }
    return block;
}
