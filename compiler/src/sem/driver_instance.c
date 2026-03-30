#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

#include "sem_driver.h"
#include "sem.h"
#include "util.h"
#include "rules.h"
#include "driver_internal.h"

/* Note: full MEM declaration checks are implemented in driver_mem.c via
 * sem_check_module_mem_and_mux_decls().
 */

/*
 * MODULE_AND_INSTANTIATION checks for module-level @new instances.
 */
static int sem_expr_contains_idx(JZASTNode *expr)
{
    if (!expr) return 0;
    if ((expr->type == JZ_AST_EXPR_IDENTIFIER ||
         expr->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER) &&
        expr->name && strcmp(expr->name, "IDX") == 0) {
        return 1;
    }
    for (size_t i = 0; i < expr->child_count; ++i) {
        if (sem_expr_contains_idx(expr->children[i])) {
            return 1;
        }
    }
    return 0;
}

/* Evaluate a simple index node (literal or identifier) as a nonnegative integer.
 * This is intentionally conservative: we only support cases where the index is a
 * simple number-like token that eval_simple_positive_decl_int can handle.
 */
int sem_eval_simple_index_literal(JZASTNode *idx, unsigned *out)
{
    if (!idx || !out) return 0;

    /* Literal index: accept nonnegative integers, including 0, with simple
     * decimal syntax. This is slightly more permissive than
     * eval_simple_positive_decl_int(), which rejects 0.
     */
    if (idx->type == JZ_AST_EXPR_LITERAL && idx->text) {
        const char *p = idx->text;
        while (*p == ' ' || *p == '\t') {
            ++p;
        }
        if (!*p) return 0;
        unsigned v = 0;
        int saw_digit = 0;
        for (; *p; ++p) {
            if (*p >= '0' && *p <= '9') {
                saw_digit = 1;
                v = (unsigned)(v * 10u + (unsigned)(*p - '0'));
            } else {
                /* Stop at first non-digit; keep semantics simple and strict. */
                return 0;
            }
        }
        if (!saw_digit) return 0;
        *out = v;
        return 1;
    }

    /* Identifier/qualified identifier index: delegate to existing helper so
     * CONST/CONFIG-based indices still work when they evaluate to a positive
     * integer.
     */
    if ((idx->type == JZ_AST_EXPR_IDENTIFIER ||
         idx->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER) &&
        idx->name) {
        unsigned v = 0;
        int rc = eval_simple_positive_decl_int(idx->name, &v);
        if (rc != 1) return 0;
        *out = v;
        return 1;
    }

    return 0;
}

/* Recursively evaluate an AST expression node with a given IDX value.
 * Returns 0 on success (result stored in *out), -1 if the expression
 * cannot be statically evaluated (unknown node type, non-IDX identifier, etc.).
 */
static int sem_eval_idx_expr(JZASTNode *expr, unsigned idx_value, unsigned *out)
{
    if (!expr || !out) return -1;

    /* Literal integer: parse from text field. */
    if (expr->type == JZ_AST_EXPR_LITERAL && expr->text) {
        const char *p = expr->text;
        while (*p == ' ' || *p == '\t') ++p;
        if (!*p) return -1;
        unsigned v = 0;
        for (; *p; ++p) {
            if (*p >= '0' && *p <= '9') {
                v = v * 10u + (unsigned)(*p - '0');
            } else {
                return -1;
            }
        }
        *out = v;
        return 0;
    }

    /* IDX identifier. */
    if ((expr->type == JZ_AST_EXPR_IDENTIFIER ||
         expr->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER) &&
        expr->name && strcmp(expr->name, "IDX") == 0) {
        *out = idx_value;
        return 0;
    }

    /* Builtin calls: uadd, umul, sadd, smul. */
    if (expr->type == JZ_AST_EXPR_BUILTIN_CALL && expr->name &&
        expr->child_count == 2) {
        unsigned a = 0, b = 0;
        if (sem_eval_idx_expr(expr->children[0], idx_value, &a) != 0) return -1;
        if (sem_eval_idx_expr(expr->children[1], idx_value, &b) != 0) return -1;

        if (strcmp(expr->name, "uadd") == 0 || strcmp(expr->name, "sadd") == 0) {
            *out = a + b;
            return 0;
        }
        if (strcmp(expr->name, "umul") == 0 || strcmp(expr->name, "smul") == 0) {
            *out = a * b;
            return 0;
        }
        if (strcmp(expr->name, "usub") == 0 || strcmp(expr->name, "ssub") == 0) {
            *out = a - b;
            return 0;
        }
    }

    return -1;
}

void sem_check_module_instantiations(const JZModuleScope *scope,
                                            JZBuffer *module_scopes,
                                            const JZBuffer *project_symbols,
                                            JZDiagnosticList *diagnostics)
{
    (void)module_scopes;

    if (!scope || !scope->node) return;
    if (!project_symbols) return;

    JZASTNode *mod = scope->node;

    /* Collect all MODULE_INSTANCE nodes, including those inside FEATURE_GUARD. */
    const size_t MAX_INSTANCES = 256;
    JZASTNode *all_instances[256];
    size_t all_inst_count = 0;
    for (size_t i = 0; i < mod->child_count; ++i) {
        JZASTNode *child = mod->children[i];
        if (!child) continue;
        if (child->type == JZ_AST_MODULE_INSTANCE) {
            if (all_inst_count < MAX_INSTANCES) all_instances[all_inst_count++] = child;
        } else if (child->type == JZ_AST_FEATURE_GUARD) {
            /* Walk both branches of module-scope FEATURE_GUARD */
            for (size_t bi = 1; bi < child->child_count; ++bi) {
                JZASTNode *branch = child->children[bi];
                if (!branch) continue;
                for (size_t bj = 0; bj < branch->child_count; ++bj) {
                    JZASTNode *bchild = branch->children[bj];
                    if (bchild && bchild->type == JZ_AST_MODULE_INSTANCE) {
                        if (all_inst_count < MAX_INSTANCES) all_instances[all_inst_count++] = bchild;
                    }
                }
            }
        }
    }

    for (size_t i = 0; i < all_inst_count; ++i) {
        JZASTNode *inst = all_instances[i];
        if (!inst || inst->type != JZ_AST_MODULE_INSTANCE) continue;

        /* Determine instance array count (optional [count] on @new). */
        unsigned array_count = 1;
        int is_array = 0;
        if (inst->width && *inst->width) {
            unsigned c = 0;
            int rc = eval_simple_positive_decl_int(inst->width, &c);
            if (rc == -1) {
                /* Digits-only but invalid (zero or overflow). */
                sem_report_rule(diagnostics,
                                inst->loc,
                                "INSTANCE_ARRAY_COUNT_INVALID",
                                "instance array count must be a positive integer expression");
                /* Fall back to treating this as a scalar instance to keep analysis going. */
            } else if (rc == 0) {
                /* Non-simple expression; try full constant evaluation which
                 * handles CONST references, CONFIG.<name>, arithmetic, and
                 * clog2().
                 */
                int resolved = 0;
                long long cval = 0;
                if (sem_eval_const_expr_in_module(inst->width,
                                                   scope,
                                                   project_symbols,
                                                   &cval) == 0) {
                    if (cval > 0) {
                        array_count = (unsigned)cval;
                        is_array = 1;
                        resolved = 1;
                    } else {
                        sem_report_rule(diagnostics,
                                        inst->loc,
                                        "INSTANCE_ARRAY_COUNT_INVALID",
                                        "instance array count must be a positive integer expression");
                        resolved = 1;
                    }
                }
                if (!resolved) {
                    /* Unresolvable expression; treat as unknown-count array. */
                    is_array = 1;
                }
            } else {
                array_count = c;
                is_array = 1;
            }
        }

        /* Resolve referenced module/blackbox. */
        const char *child_name = inst->text;
        const JZSymbol *child_sym = NULL;
        if (child_name && project_symbols && project_symbols->data) {
            const JZSymbol *syms = (const JZSymbol *)project_symbols->data;
            size_t count = project_symbols->len / sizeof(JZSymbol);
            for (size_t i = 0; i < count; ++i) {
                if (syms[i].name && strcmp(syms[i].name, child_name) == 0 &&
                    (syms[i].kind == JZ_SYM_MODULE || syms[i].kind == JZ_SYM_BLACKBOX)) {
                    child_sym = &syms[i];
                    break;
                }
            }
        }
        if (!child_sym || !child_sym->node) {
            /* Distinguish module-not-found from blackbox-not-found.
             * If the project declares any @blackbox, the undefined name was
             * likely intended as a blackbox reference; otherwise treat it as
             * an undefined module. */
            int has_blackbox = 0;
            if (project_symbols && project_symbols->data) {
                const JZSymbol *ps = (const JZSymbol *)project_symbols->data;
                size_t pc = project_symbols->len / sizeof(JZSymbol);
                for (size_t k = 0; k < pc; ++k) {
                    if (ps[k].kind == JZ_SYM_BLACKBOX) {
                        has_blackbox = 1;
                        break;
                    }
                }
            }
            if (has_blackbox) {
                sem_report_rule(diagnostics,
                                inst->loc,
                                "BLACKBOX_UNDEFINED_IN_NEW",
                                "@new references undefined blackbox name");
            } else {
                sem_report_rule(diagnostics,
                                inst->loc,
                                "INSTANCE_UNDEFINED_MODULE",
                                "instantiation references non-existent module");
            }
            continue;
        }

        JZASTNode *child_mod = child_sym->node;
        const JZModuleScope *child_scope = NULL; /* child scopes are managed in driver_project.c; fall back to AST scan here */

        /* Optional OVERRIDE { ... } block.
         *
         * For module children, ensure every overridden CONST exists in the
         * child module. For blackbox children, do not validate OVERRIDE
         * entries; instead, emit an INFO diagnostic to indicate that the
         * override values are passed through to vendor IP.
         */
        for (size_t c = 0; c < inst->child_count; ++c) {
            JZASTNode *child = inst->children[c];
            if (!child || child->type != JZ_AST_CONST_BLOCK ||
                !child->block_kind || strcmp(child->block_kind, "OVERRIDE") != 0) {
                continue;
            }

            if (child_sym->kind == JZ_SYM_BLACKBOX) {
                sem_report_rule(diagnostics,
                                child->loc,
                                "BLACKBOX_OVERRIDE_UNCHECKED",
                                "OVERRIDE in blackbox instantiation is not validated and will be passed through to vendor IP");
                continue;
            }

            for (size_t j = 0; j < child->child_count; ++j) {
                JZASTNode *ov = child->children[j];
                if (!ov || ov->type != JZ_AST_CONST_DECL || !ov->name) continue;

                /* Forbid GLOBAL.<name> in OVERRIDE expressions. */
                if (project_symbols && ov->text &&
                    sem_expr_has_global_ref(ov->text, project_symbols)) {
                    sem_report_rule(diagnostics,
                                    ov->loc,
                                    "GLOBAL_USED_WHERE_FORBIDDEN",
                                    "GLOBAL.<name> may not be used in OVERRIDE expressions");
                }

                if (ov->text && sem_expr_has_lit_call(ov->text)) {
                    sem_report_rule(diagnostics,
                                    ov->loc,
                                    "LIT_INVALID_CONTEXT",
                                    "lit() may not be used in OVERRIDE expressions");
                }

                int found = 0;
                if (child_scope) {
                    const JZSymbol *c_sym = module_scope_lookup_kind(child_scope,
                                                                      ov->name,
                                                                      JZ_SYM_CONST);
                    if (c_sym) {
                        found = 1;
                    }
                } else {
                    /* Fallback: scan child module CONST blocks directly. */
                    for (size_t mi = 0; mi < child_mod->child_count && !found; ++mi) {
                        JZASTNode *blk = child_mod->children[mi];
                        if (!blk || blk->type != JZ_AST_CONST_BLOCK) continue;
                        for (size_t dj = 0; dj < blk->child_count; ++dj) {
                            JZASTNode *cd = blk->children[dj];
                            if (cd && cd->type == JZ_AST_CONST_DECL &&
                                cd->name && strcmp(cd->name, ov->name) == 0) {
                                found = 1;
                                break;
                            }
                        }
                    }
                }

                if (!found) {
                    sem_report_rule(diagnostics,
                                    ov->loc,
                                    "INSTANCE_OVERRIDE_CONST_UNDEFINED",
                                    "OVERRIDE sets CONST that is not declared in child module or blackbox");
                }
            }
        }

        /* Build a simple view of the instance's port bindings (PORT_DECL
         * children). The parser guarantees block_kind is the declared
         * direction (IN/OUT/INOUT) and width is the instantiation width
         * expression in parent scope.
         */
        const size_t MAX_BINDINGS = 128;
        JZASTNode *bindings[MAX_BINDINGS];
        size_t binding_count = 0;
        for (size_t c = 0; c < inst->child_count && binding_count < MAX_BINDINGS; ++c) {
            JZASTNode *b = inst->children[c];
            if (!b || b->type != JZ_AST_PORT_DECL || !b->name) continue;
            bindings[binding_count++] = b;
        }

        /* Enforce that IDX is only used in array instance bindings, not scalar @new
         * or OVERRIDE expressions.
         */
        for (size_t c = 0; c < inst->child_count; ++c) {
            JZASTNode *child = inst->children[c];
            if (!child) continue;

            /* OVERRIDE block: its RHS expressions are stored as text; forbid IDX here. */
            if (child->type == JZ_AST_CONST_BLOCK &&
                child->block_kind && strcmp(child->block_kind, "OVERRIDE") == 0) {
                for (size_t j = 0; j < child->child_count; ++j) {
                    JZASTNode *ov = child->children[j];
                    if (!ov || ov->type != JZ_AST_CONST_DECL || !ov->text) continue;
                    if (strstr(ov->text, "IDX") != NULL) {
                        sem_report_rule(diagnostics,
                                        ov->loc,
                                        "INSTANCE_ARRAY_IDX_INVALID_CONTEXT",
                                        "IDX may not be used inside OVERRIDE expressions");
                    }
                }
            }
        }

        /* For each child port, ensure there is a corresponding binding and
         * that direction and (where both sides are simple) widths match.
         */
        for (size_t ci = 0; ci < child_mod->child_count; ++ci) {
            JZASTNode *blk = child_mod->children[ci];
            if (!blk || blk->type != JZ_AST_PORT_BLOCK) continue;

            for (size_t pj = 0; pj < blk->child_count; ++pj) {
                JZASTNode *pd = blk->children[pj];
                if (!pd || pd->type != JZ_AST_PORT_DECL || !pd->name) continue;

                JZASTNode *bind = NULL;
                for (size_t bi = 0; bi < binding_count; ++bi) {
                    if (bindings[bi] && bindings[bi]->name &&
                        strcmp(bindings[bi]->name, pd->name) == 0) {
                        bind = bindings[bi];
                        break;
                    }
                }

                if (!bind) {
                    sem_report_rule(diagnostics,
                                    inst->loc,
                                    "INSTANCE_MISSING_PORT",
                                    "not all child module ports are listed in @new instance body");
                    continue;
                }

                /* Direction must match exactly (IN/OUT/INOUT). */
                const char *child_dir = pd->block_kind ? pd->block_kind : "";
                const char *bind_dir  = bind->block_kind ? bind->block_kind : "";
                if (child_dir[0] && bind_dir[0] && strcmp(child_dir, bind_dir) != 0) {
                    sem_report_rule(diagnostics,
                                    bind->loc,
                                    "INSTANCE_PORT_DIRECTION_MISMATCH",
                                    "instance port direction does not match child module port direction");
                }

                /* BUS ports: validate BUS metadata and array count on @new. */
                int child_is_bus = (child_dir[0] && strcmp(child_dir, "BUS") == 0);
                if (child_is_bus) {
                    const char *child_meta = pd->text ? pd->text : "";
                    const char *bind_meta = bind->text ? bind->text : "";
                    char child_bus[128] = {0};
                    char child_role[128] = {0};
                    char bind_bus[128] = {0};
                    char bind_role[128] = {0};

                    if (child_meta && *child_meta) {
                        sscanf(child_meta, "%127s %127s", child_bus, child_role);
                    }
                    if (bind_meta && *bind_meta) {
                        sscanf(bind_meta, "%127s %127s", bind_bus, bind_role);
                    }

                    if (child_bus[0] && bind_bus[0] &&
                        strcmp(child_bus, bind_bus) != 0) {
                        sem_report_rule(diagnostics,
                                        bind->loc,
                                        "INSTANCE_BUS_MISMATCH",
                                        "instance BUS binding does not match child BUS port BUS id");
                    }
                    if (child_role[0] && bind_role[0] &&
                        strcmp(child_role, bind_role) != 0) {
                        sem_report_rule(diagnostics,
                                        bind->loc,
                                        "INSTANCE_BUS_MISMATCH",
                                        "instance BUS binding does not match child BUS port role");
                    }

                    if (bind->width) {
                        unsigned count = 0;
                        if (sem_eval_width_expr(bind->width,
                                                scope,
                                                project_symbols,
                                                &count) != 0) {
                            sem_report_rule(diagnostics,
                                            bind->loc,
                                            "BUS_PORT_ARRAY_COUNT_INVALID",
                                            "BUS array count must be a positive integer constant expression");
                        }
                    }
                }

                /* Width equality when both sides are simple positive integers.
                 * If instantiation width is not a simple positive int or refers
                 * to an invalid CONST/CONFIG, report INSTANCE_PORT_WIDTH_EXPR_INVALID.
                 */
                if (!child_is_bus && pd->width && bind->width) {
                    if (sem_expr_has_lit_call(bind->width)) {
                        sem_report_rule(diagnostics,
                                        bind->loc,
                                        "LIT_INVALID_CONTEXT",
                                        "lit() may not be used in instance port width expressions");
                        continue;
                    }
                    unsigned child_w = 0, inst_w = 0;
                    int child_rc = eval_simple_positive_decl_int(pd->width, &child_w);
                    int inst_rc  = eval_simple_positive_decl_int(bind->width, &inst_w);
                    if (child_rc == 1 && inst_rc == 1 && child_w != inst_w) {
                        sem_report_rule(diagnostics,
                                        bind->loc,
                                        "INSTANCE_PORT_WIDTH_MISMATCH",
                                        "instantiated port width does not match child module effective port width");
                    } else if (inst_rc == -1 ||
                               sem_instance_width_expr_is_invalid(bind->width, scope, project_symbols)) {
                        sem_report_rule(diagnostics,
                                        bind->loc,
                                        "INSTANCE_PORT_WIDTH_EXPR_INVALID",
                                        "width expression in instance port list uses undefined CONST/CONFIG or invalid integer expression");
                    }
                }

                /* INSTANCE_OUT_PORT_LITERAL: OUT port binding must not be a literal. */
                if (bind->child_count > 0 &&
                    (strcmp(child_dir, "OUT") == 0 || strcmp(child_dir, "INOUT") == 0)) {
                    JZASTNode *rhs = bind->children[0];
                    if (rhs && rhs->type == JZ_AST_EXPR_LITERAL) {
                        sem_report_rule(diagnostics,
                                        rhs->loc,
                                        "INSTANCE_OUT_PORT_LITERAL",
                                        "OUT port binding in @new may not be a literal value");
                    }
                }

                /* Basic IDX context check for binding RHS: only allowed when this
                 * instantiation is an array. For scalars, flag use of IDX.
                 */
                if (bind->child_count > 0) {
                    JZASTNode *rhs = bind->children[0];
                    if (sem_expr_contains_idx(rhs) && !is_array) {
                        sem_report_rule(diagnostics,
                                        rhs->loc,
                                        "INSTANCE_ARRAY_IDX_INVALID_CONTEXT",
                                        "IDX may only appear in parent-signal bindings of instance arrays");
                    }

                    /* Simple Non-Overlap check for instance arrays:
                     * if this binding is for an OUT port of an arrayed instance and
                     * the RHS is a fixed slice (indices do not depend on IDX), then
                     * more than one instance would drive the same parent bits.
                     * Flag this as INSTANCE_ARRAY_PARENT_BIT_OVERLAP.
                     */
                    if (is_array && array_count > 1 &&
                        rhs->type == JZ_AST_EXPR_SLICE &&
                        (strcmp(child_dir, "OUT") == 0 ||
                         strcmp(child_dir, "INOUT") == 0)) {
                        if (rhs->child_count >= 3) {
                            JZASTNode *msb = rhs->children[1];
                            JZASTNode *lsb = rhs->children[2];
                            unsigned msb_v = 0, lsb_v = 0;
                            /* Only enforce when indices are simple constants; if
                             * they cannot be evaluated, defer to generic net rules.
                             */
                            if (sem_eval_simple_index_literal(msb, &msb_v) &&
                                sem_eval_simple_index_literal(lsb, &lsb_v) &&
                                msb_v >= lsb_v) {
                                sem_report_rule(diagnostics,
                                                bind->loc,
                                                "INSTANCE_ARRAY_PARENT_BIT_OVERLAP",
                                                "instance array OUT mappings drive overlapping bits of the same parent signal");
                            }
                        }
                    }

                    /* IDX-dependent slice range check: for array instances where
                     * the binding RHS is a slice whose indices depend on IDX,
                     * evaluate MSB/LSB for every IDX=0..array_count-1 and verify
                     * they fall within the parent signal's declared width.
                     */
                    if (is_array && array_count > 0 &&
                        rhs->type == JZ_AST_EXPR_SLICE &&
                        rhs->child_count >= 3 &&
                        sem_expr_contains_idx(rhs)) {
                        JZASTNode *base = rhs->children[0];
                        JZASTNode *msb_expr = rhs->children[1];
                        JZASTNode *lsb_expr = rhs->children[2];

                        /* Resolve the parent signal width from the base identifier. */
                        unsigned parent_w = 0;
                        int have_parent_w = 0;
                        if (base && base->type == JZ_AST_EXPR_IDENTIFIER && base->name) {
                            const JZSymbol *psym = module_scope_lookup(scope, base->name);
                            if (psym && psym->node &&
                                (psym->kind == JZ_SYM_PORT ||
                                 psym->kind == JZ_SYM_WIRE ||
                                 psym->kind == JZ_SYM_REGISTER) &&
                                psym->node->width) {
                                int rc = eval_simple_positive_decl_int(psym->node->width, &parent_w);
                                if (rc == 1) have_parent_w = 1;
                            }
                        }

                        if (have_parent_w) {
                            for (unsigned idx = 0; idx < array_count; ++idx) {
                                unsigned msb_v = 0, lsb_v = 0;
                                int msb_ok = sem_eval_idx_expr(msb_expr, idx, &msb_v);
                                int lsb_ok = sem_eval_idx_expr(lsb_expr, idx, &lsb_v);
                                if (msb_ok == 0 && lsb_ok == 0) {
                                    if (msb_v >= parent_w || lsb_v >= parent_w) {
                                        sem_report_rule(diagnostics,
                                                        bind->loc,
                                                        "INSTANCE_ARRAY_IDX_SLICE_OUT_OF_RANGE",
                                                        "IDX-dependent slice index exceeds parent signal width for some instance index");
                                        break; /* one diagnostic per binding is sufficient */
                                    }
                                }
                            }
                        }
                    }
                }

                /* Parent signal width vs instantiation width: when the bound
                 * parent expression is a simple identifier referring to a
                 * PORT/WIRE/REGISTER in the parent scope, and both widths are
                 * simple positive integers, require them to match.
                 */
                if (bind->child_count > 0 && bind->width) {
                    JZASTNode *rhs = bind->children[0];
                    if (rhs && rhs->type == JZ_AST_EXPR_IDENTIFIER &&
                        rhs->name && strcmp(rhs->name, "_") == 0) {
                        /* Explicit no-connect: skip parent-signal width checks. */
                    } else if (rhs && rhs->type == JZ_AST_EXPR_IDENTIFIER && rhs->name) {
                        const JZSymbol *psym = module_scope_lookup(scope, rhs->name);
                        if (psym && psym->node &&
                            (psym->kind == JZ_SYM_PORT ||
                             psym->kind == JZ_SYM_WIRE ||
                             psym->kind == JZ_SYM_REGISTER)) {
                            JZASTNode *pdecl = psym->node;
                            if (pdecl->width) {
                                unsigned parent_w = 0, inst_w2 = 0;
                                int parent_rc = eval_simple_positive_decl_int(pdecl->width, &parent_w);
                                int inst_rc2  = eval_simple_positive_decl_int(bind->width, &inst_w2);
                                if (parent_rc == 1 && inst_rc2 == 1 && parent_w != inst_w2) {
                                    sem_report_rule(diagnostics,
                                                    bind->loc,
                                                    "INSTANCE_PARENT_SIGNAL_WIDTH_MISMATCH",
                                                    "bound parent signal width does not match instantiation port width");
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

/*
 * Module-level width checks for PORT/WIRE/REGISTER blocks.
 */
