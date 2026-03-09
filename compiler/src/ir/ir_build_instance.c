/**
 * @file ir_build_instance.c
 * @brief Module specialization and instance elaboration for IR construction.
 *
 * This file implements module specialization based on OVERRIDE blocks and
 * elaborates @new module instances into IR_Instance structures, including
 * array instances and port connections.
 */

#include <stdlib.h>
#include <string.h>

#include "ir_internal.h"

/**
 * @brief Look up a BUS definition in the project symbol table.
 *
 * @param project_symbols Project symbol buffer.
 * @param name            BUS name.
 * @return Matching symbol, or NULL if not found.
 */
static const JZSymbol *ir_lookup_bus_def(const JZBuffer *project_symbols,
                                          const char *name)
{
    if (!project_symbols || !project_symbols->data || !name) {
        return NULL;
    }
    const JZSymbol *syms = (const JZSymbol *)project_symbols->data;
    size_t count = project_symbols->len / sizeof(JZSymbol);
    for (size_t i = 0; i < count; ++i) {
        if (!syms[i].name || strcmp(syms[i].name, name) != 0) {
            continue;
        }
        if (syms[i].kind == JZ_SYM_BUS) {
            return &syms[i];
        }
    }
    return NULL;
}

/**
 * @brief qsort comparator for module-spec override entries.
 *
 * Sorts IR_ModuleSpecOverride entries lexicographically by CONST name in
 * order to canonicalize OVERRIDE sets.
 *
 * @param a First override.
 * @param b Second override.
 * @return Negative, zero, or positive as per strcmp semantics.
 */
static int ir_override_name_cmp(const void *a, const void *b)
{
    const IR_ModuleSpecOverride *oa = (const IR_ModuleSpecOverride *)a;
    const IR_ModuleSpecOverride *ob = (const IR_ModuleSpecOverride *)b;
    if (!oa->name && !ob->name) return 0;
    if (!oa->name) return -1;
    if (!ob->name) return 1;
    return strcmp(oa->name, ob->name);
}

/**
 * @brief Collect and evaluate OVERRIDE entries for a single module instance.
 *
 * Searches for an OVERRIDE { ... } block inside an @new instance declaration
 * and evaluates each CONST override expression in the context of the child
 * module.
 *
 * The resulting override set is sorted by CONST name to enable canonical
 * comparison between instances.
 *
 * @param inst_node            AST node for the module instance.
 * @param child_scope          Module scope of the instantiated child module.
 * @param project_symbols      Project-level symbol table.
 * @param out_overrides        Output array of overrides (heap-allocated).
 * @param out_overrides_count  Output number of overrides.
 * @return 0 on success, non-zero on failure.
 *
 * @note When no overrides are present, *out_overrides_count is set to 0 and
 *       *out_overrides is set to NULL. The caller must not free it in this case.
 */
static int ir_collect_instance_overrides(const JZASTNode *inst_node,
                                 const JZModuleScope *child_scope,
                                 const JZBuffer *project_symbols,
                                 IR_ModuleSpecOverride **out_overrides,
                                 int *out_overrides_count)
{
    if (!inst_node || !child_scope || !out_overrides || !out_overrides_count) {
        return -1;
    }

    *out_overrides = NULL;
    *out_overrides_count = 0;

    /* Locate OVERRIDE { ... } block, if any. */
    JZASTNode *override_block = NULL;
    for (size_t c = 0; c < inst_node->child_count; ++c) {
        JZASTNode *child = inst_node->children[c];
        if (!child || child->type != JZ_AST_CONST_BLOCK) {
            continue;
        }
        if (!child->block_kind || strcmp(child->block_kind, "OVERRIDE") != 0) {
            continue;
        }
        override_block = child;
        break;
    }

    if (!override_block) {
        /* No OVERRIDE block on this instance. */
        return 0;
    }

    /* Count CONST entries inside OVERRIDE. */
    int count = 0;
    for (size_t j = 0; j < override_block->child_count; ++j) {
        JZASTNode *ov = override_block->children[j];
        if (!ov || ov->type != JZ_AST_CONST_DECL || !ov->name) {
            continue;
        }
        count++;
    }

    if (count == 0) {
        return 0;
    }

    IR_ModuleSpecOverride *ov_arr = (IR_ModuleSpecOverride *)malloc(sizeof(IR_ModuleSpecOverride) * (size_t)count);
    if (!ov_arr) {
        return -1;
    }
    memset(ov_arr, 0, sizeof(IR_ModuleSpecOverride) * (size_t)count);

    int idx = 0;
    for (size_t j = 0; j < override_block->child_count && idx < count; ++j) {
        JZASTNode *ov = override_block->children[j];
        if (!ov || ov->type != JZ_AST_CONST_DECL || !ov->name) {
            continue;
        }

        if (ov->block_kind && strcmp(ov->block_kind, "STRING") == 0) {
            /* String OVERRIDE (e.g. DATA_FILE = "path").
             * String overrides affect file paths (MEM initialization)
             * and require separate specializations when values differ.
             */
            ov_arr[idx].name = ov->name;
            ov_arr[idx].value = 0;
            ov_arr[idx].string_value = ov->text;
            idx++;
            continue;
        }

        const char *expr = ov->text ? ov->text : "0";
        long long value = 0;
        if (sem_eval_const_expr_in_module(expr,
                                          child_scope,
                                          project_symbols,
                                          &value) != 0) {
            free(ov_arr);
            return -1;
        }

        ov_arr[idx].name = ov->name;
        ov_arr[idx].value = value;
        ov_arr[idx].string_value = NULL;
        idx++;
    }

    /* Sort overrides by CONST name to canonicalize the set. */
    if (idx > 1) {
        qsort(ov_arr, (size_t)idx, sizeof(IR_ModuleSpecOverride), ir_override_name_cmp);
    }

    *out_overrides = ov_arr;
    *out_overrides_count = idx;
    return 0;
}

/**
 * @brief Collect all module specializations in the design.
 *
 * Walks all module instances across all module scopes and identifies unique
 * {base_module, OVERRIDE set} pairs. Each unique pair is recorded as an
 * IR_ModuleSpec entry.
 *
 * @param scopes          Module scope array.
 * @param scope_count     Number of module scopes.
 * @param project_symbols Project-level symbol table.
 * @param arena           Arena for allocating specialization names.
 * @param out_specs       Output array of module specializations.
 * @param out_spec_count  Output number of specializations.
 * @return 0 on success, non-zero on failure.
 *
 * @note The returned specs array and each specs[i].overrides must be freed
 *       by the caller. spec_name strings live in the arena.
 */
int ir_collect_module_specializations(const JZModuleScope *scopes,
                                     size_t scope_count,
                                     const JZBuffer *project_symbols,
                                     JZArena *arena,
                                     IR_ModuleSpec **out_specs,
                                     int *out_spec_count)
{
    if (!scopes || !out_specs || !out_spec_count) {
        return -1;
    }

    *out_specs = NULL;
    *out_spec_count = 0;

    if (!project_symbols || !project_symbols->data) {
        return 0;
    }

    IR_ModuleSpec *specs = NULL;
    int            spec_count = 0;
    int            spec_cap = 0;

    const JZSymbol *proj_syms = (const JZSymbol *)project_symbols->data;
    size_t proj_sym_count = project_symbols->len / sizeof(JZSymbol);

    for (size_t mi = 0; mi < scope_count; ++mi) {
        const JZModuleScope *parent_scope = &scopes[mi];
        JZASTNode *mod = parent_scope->node;
        if (!mod) {
            continue;
        }

        /* Collect instances including those inside module-scope FEATURE_GUARD.
         * For specialization, we need to consider both branches since either
         * could reference a module that needs specialization.
         */
        JZASTNode *spec_inst_nodes[512];
        int spec_inst_count = 0;
        for (size_t ci = 0; ci < mod->child_count; ++ci) {
            JZASTNode *ch = mod->children[ci];
            if (!ch) continue;
            if (ch->type == JZ_AST_MODULE_INSTANCE) {
                if (spec_inst_count < 512) spec_inst_nodes[spec_inst_count++] = ch;
            } else if (ch->type == JZ_AST_FEATURE_GUARD) {
                /* Walk both branches for specialization (both might be used) */
                for (size_t bi = 1; bi < ch->child_count; ++bi) {
                    JZASTNode *branch = ch->children[bi];
                    if (!branch) continue;
                    for (size_t bj = 0; bj < branch->child_count; ++bj) {
                        JZASTNode *bch = branch->children[bj];
                        if (bch && bch->type == JZ_AST_MODULE_INSTANCE) {
                            if (spec_inst_count < 512) spec_inst_nodes[spec_inst_count++] = bch;
                        }
                    }
                }
            }
        }

        for (int sii = 0; sii < spec_inst_count; ++sii) {
            JZASTNode *inst = spec_inst_nodes[sii];
            if (!inst || inst->type != JZ_AST_MODULE_INSTANCE || !inst->text) {
                continue;
            }

            /* Resolve referenced module/blackbox in project symbol table. */
            const char *child_name = inst->text;
            const JZSymbol *child_sym = NULL;
            for (size_t si = 0; si < proj_sym_count; ++si) {
                const JZSymbol *s = &proj_syms[si];
                if (!s->name || strcmp(s->name, child_name) != 0) {
                    continue;
                }
                if (s->kind == JZ_SYM_MODULE || s->kind == JZ_SYM_BLACKBOX) {
                    child_sym = s;
                    break;
                }
            }
            if (!child_sym || !child_sym->node) {
                continue;
            }
            if (child_sym->kind != JZ_SYM_MODULE) {
                /* OVERRIDE on blackboxes is passed through to vendor IP and
                 * does not produce a specialized IR module.
                 */
                continue;
            }

            /* Find the child module scope so overrides can be evaluated. */
            const JZASTNode *child_mod = child_sym->node;
            int base_scope_index = -1;
            for (size_t si = 0; si < scope_count; ++si) {
                if (scopes[si].node == child_mod) {
                    base_scope_index = (int)si;
                    break;
                }
            }
            if (base_scope_index < 0) {
                continue;
            }

            const JZModuleScope *child_scope = &scopes[(size_t)base_scope_index];

            IR_ModuleSpecOverride *ov = NULL;
            int ov_count = 0;
            /* Evaluate OVERRIDE expressions in the parent scope, since
             * override values may reference parent-scope CONSTs (e.g.
             * MY_WIDTH) or CONFIG entries (CONFIG.DATA_WIDTH).  The child
             * scope only contains the child's own CONSTs and would fail to
             * resolve parent identifiers.
             */
            if (ir_collect_instance_overrides(inst,
                                              parent_scope,
                                              project_symbols,
                                              &ov,
                                              &ov_count) != 0) {
                /* Treat override evaluation failures as internal IR errors. */
                if (ov) free(ov);
                return -1;
            }
            if (!ov || ov_count == 0) {
                if (ov) free(ov);
                continue; /* no OVERRIDE on this instance */
            }

            /* Check if this {base, override_set} already exists. */
            int found_index = -1;
            for (int si = 0; si < spec_count; ++si) {
                IR_ModuleSpec *spec = &specs[si];
                if (spec->base_scope_index != base_scope_index) {
                    continue;
                }
                if (spec->num_overrides != ov_count) {
                    continue;
                }
                int match = 1;
                for (int k = 0; k < ov_count; ++k) {
                    const IR_ModuleSpecOverride *a = &spec->overrides[k];
                    const IR_ModuleSpecOverride *b = &ov[k];
                    if ((!a->name && b->name) || (a->name && !b->name)) {
                        match = 0;
                        break;
                    }
                    if (a->name && b->name && strcmp(a->name, b->name) != 0) {
                        match = 0;
                        break;
                    }
                    if (a->string_value || b->string_value) {
                        /* String override: compare string values. */
                        if (!a->string_value || !b->string_value ||
                            strcmp(a->string_value, b->string_value) != 0) {
                            match = 0;
                            break;
                        }
                    } else if (a->value != b->value) {
                        match = 0;
                        break;
                    }
                }
                if (match) {
                    found_index = si;
                    break;
                }
            }

            if (found_index >= 0) {
                /* Specialization already registered; discard temporary overrides. */
                free(ov);
                continue;
            }

            /* New specialization: grow specs array if needed. */
            if (spec_count >= spec_cap) {
                int new_cap = (spec_cap == 0) ? 4 : spec_cap * 2;
                IR_ModuleSpec *new_specs = (IR_ModuleSpec *)realloc(specs, sizeof(IR_ModuleSpec) * (size_t)new_cap);
                if (!new_specs) {
                    free(ov);
                    free(specs);
                    return -1;
                }
                specs = new_specs;
                spec_cap = new_cap;
            }

            IR_ModuleSpec *spec = &specs[spec_count];
            memset(spec, 0, sizeof(*spec));
            spec->base_mod_node = child_mod;
            spec->base_scope_index = base_scope_index;
            spec->overrides = ov;      /* take ownership */
            spec->num_overrides = ov_count;

            /* Build specialized module name: <base>__SPEC_<CONST>_<value>... */
            const char *base_name = child_mod->name ? child_mod->name : "";
            size_t base_len = strlen(base_name);
            size_t est_len = base_len + 7; /* __SPEC_ */
            for (int k = 0; k < ov_count; ++k) {
                size_t nlen = ov[k].name ? strlen(ov[k].name) : 0;
                size_t vlen = ov[k].string_value ? strlen(ov[k].string_value) : 32;
                est_len += 1 + nlen + 1 + vlen; /* '_' + name + '_' + value */
            }

            char *tmp = (char *)malloc(est_len + 1);
            if (!tmp) {
                free(specs);
                return -1;
            }
            size_t pos = 0;
            memcpy(tmp + pos, base_name, base_len);
            pos += base_len;
            memcpy(tmp + pos, "__SPEC", 6);
            pos += 6;
            for (int k = 0; k < ov_count; ++k) {
                int written;
                if (ov[k].string_value) {
                    /* Sanitize string value for module name: replace
                     * non-alphanumeric chars with underscores.
                     */
                    written = snprintf(tmp + pos, est_len + 1 - pos,
                                       "_%s_",
                                       ov[k].name ? ov[k].name : "");
                    if (written > 0) {
                        pos += (size_t)written;
                    }
                    const char *sv = ov[k].string_value;
                    for (size_t si = 0; sv[si] && pos < est_len; ++si) {
                        char c = sv[si];
                        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                            (c >= '0' && c <= '9')) {
                            tmp[pos++] = c;
                        } else {
                            tmp[pos++] = '_';
                        }
                    }
                    written = 0; /* already handled */
                } else {
                    written = snprintf(tmp + pos, est_len + 1 - pos,
                                       "_%s_%lld",
                                       ov[k].name ? ov[k].name : "",
                                       (long long)ov[k].value);
                }
                if (written < 0) {
                    tmp[pos] = '\0';
                    break;
                }
                pos += (size_t)written;
            }
            tmp[pos] = '\0';

            spec->spec_name = ir_strdup_arena(arena, tmp);
            free(tmp);

            spec->spec_module_index = -1; /* filled in later using scope_count */
            spec_count++;
        }
    }

    /* Recursive pass: for each specialization that has string overrides,
     * walk the base module's sub-instances and create sub-specializations
     * with the parent spec's string override values substituted.
     *
     * This handles patterns like:
     *   dvi_top -> tone_gen { OVERRIDE { DATA_FILE = "track0.bin"; } }
     *   tone_gen -> melodies { OVERRIDE { DATA_FILE = DATA_FILE; } }
     *
     * Without this pass, all melodies instances would use the same file
     * because Pass 2b resolves the base scope with the last override value.
     */
    int first_level_count = spec_count;
    for (int psi = 0; psi < first_level_count; ++psi) {
        const IR_ModuleSpec *parent_spec = &specs[psi];

        /* Only process specs with string overrides — integer overrides
         * are already handled correctly by the base scope.
         */
        int has_string_ov = 0;
        for (int k = 0; k < parent_spec->num_overrides; ++k) {
            if (parent_spec->overrides[k].string_value) {
                has_string_ov = 1;
                break;
            }
        }
        if (!has_string_ov) {
            continue;
        }

        /* Walk the parent spec's base module for sub-instances. */
        const JZASTNode *parent_mod = parent_spec->base_mod_node;
        if (!parent_mod) {
            continue;
        }

        for (size_t ci = 0; ci < parent_mod->child_count; ++ci) {
            JZASTNode *inst = parent_mod->children[ci];
            if (!inst || inst->type != JZ_AST_MODULE_INSTANCE || !inst->text) {
                continue;
            }

            /* Find OVERRIDE block on this sub-instance. */
            JZASTNode *override_block = NULL;
            for (size_t c = 0; c < inst->child_count; ++c) {
                JZASTNode *child = inst->children[c];
                if (child && child->type == JZ_AST_CONST_BLOCK &&
                    child->block_kind && strcmp(child->block_kind, "OVERRIDE") == 0) {
                    override_block = child;
                    break;
                }
            }
            if (!override_block) {
                continue;
            }

            /* Check if any override entry is a STRING type whose CONST name
             * matches a string override in the parent spec. If so, we need
             * a sub-specialization with the parent's value substituted.
             */
            int needs_sub_spec = 0;
            for (size_t j = 0; j < override_block->child_count; ++j) {
                JZASTNode *ov = override_block->children[j];
                if (!ov || ov->type != JZ_AST_CONST_DECL || !ov->name) {
                    continue;
                }
                if (!ov->block_kind || strcmp(ov->block_kind, "STRING") != 0) {
                    continue;
                }
                /* Check if this CONST name has a string override in the parent. */
                for (int k = 0; k < parent_spec->num_overrides; ++k) {
                    if (parent_spec->overrides[k].string_value &&
                        parent_spec->overrides[k].name &&
                        strcmp(parent_spec->overrides[k].name, ov->name) == 0) {
                        needs_sub_spec = 1;
                        break;
                    }
                }
                if (needs_sub_spec) break;
            }
            if (!needs_sub_spec) {
                continue;
            }

            /* Resolve the child module. */
            const char *child_name = inst->text;
            const JZASTNode *child_mod = NULL;
            int base_scope_index = -1;
            for (size_t si = 0; si < proj_sym_count; ++si) {
                const JZSymbol *s = &proj_syms[si];
                if (!s->name || strcmp(s->name, child_name) != 0) {
                    continue;
                }
                if (s->kind == JZ_SYM_MODULE && s->node) {
                    child_mod = s->node;
                    break;
                }
            }
            if (!child_mod) {
                continue;
            }
            for (size_t si = 0; si < scope_count; ++si) {
                if (scopes[si].node == child_mod) {
                    base_scope_index = (int)si;
                    break;
                }
            }
            if (base_scope_index < 0) {
                continue;
            }

            /* Build override set with parent spec values substituted. */
            int ov_count = 0;
            for (size_t j = 0; j < override_block->child_count; ++j) {
                JZASTNode *ov = override_block->children[j];
                if (ov && ov->type == JZ_AST_CONST_DECL && ov->name) {
                    ov_count++;
                }
            }
            if (ov_count == 0) {
                continue;
            }

            IR_ModuleSpecOverride *sub_ov =
                (IR_ModuleSpecOverride *)malloc(sizeof(IR_ModuleSpecOverride) * (size_t)ov_count);
            if (!sub_ov) {
                free(specs);
                return -1;
            }
            memset(sub_ov, 0, sizeof(IR_ModuleSpecOverride) * (size_t)ov_count);

            int sub_idx = 0;
            const JZModuleScope *child_scope = &scopes[(size_t)base_scope_index];
            for (size_t j = 0; j < override_block->child_count && sub_idx < ov_count; ++j) {
                JZASTNode *ov = override_block->children[j];
                if (!ov || ov->type != JZ_AST_CONST_DECL || !ov->name) {
                    continue;
                }

                if (ov->block_kind && strcmp(ov->block_kind, "STRING") == 0) {
                    /* Look for a matching parent spec override. */
                    const char *resolved_str = ov->text; /* default: Pass 2b value */
                    for (int k = 0; k < parent_spec->num_overrides; ++k) {
                        if (parent_spec->overrides[k].string_value &&
                            parent_spec->overrides[k].name &&
                            strcmp(parent_spec->overrides[k].name, ov->name) == 0) {
                            resolved_str = parent_spec->overrides[k].string_value;
                            break;
                        }
                    }
                    sub_ov[sub_idx].name = ov->name;
                    sub_ov[sub_idx].value = 0;
                    sub_ov[sub_idx].string_value = resolved_str;
                    sub_idx++;
                } else {
                    /* Integer override: evaluate as before. */
                    const char *expr = ov->text ? ov->text : "0";
                    long long value = 0;
                    if (sem_eval_const_expr_in_module(expr,
                                                      child_scope,
                                                      project_symbols,
                                                      &value) != 0) {
                        free(sub_ov);
                        continue;
                    }
                    sub_ov[sub_idx].name = ov->name;
                    sub_ov[sub_idx].value = value;
                    sub_ov[sub_idx].string_value = NULL;
                    sub_idx++;
                }
            }

            if (sub_idx > 1) {
                qsort(sub_ov, (size_t)sub_idx, sizeof(IR_ModuleSpecOverride),
                      ir_override_name_cmp);
            }

            /* Check if this sub-spec already exists. */
            int found_index = -1;
            for (int si = 0; si < spec_count; ++si) {
                IR_ModuleSpec *existing = &specs[si];
                if (existing->base_scope_index != base_scope_index) continue;
                if (existing->num_overrides != sub_idx) continue;
                int match = 1;
                for (int k = 0; k < sub_idx; ++k) {
                    const IR_ModuleSpecOverride *a = &existing->overrides[k];
                    const IR_ModuleSpecOverride *b = &sub_ov[k];
                    if ((!a->name && b->name) || (a->name && !b->name) ||
                        (a->name && b->name && strcmp(a->name, b->name) != 0)) {
                        match = 0; break;
                    }
                    if (a->string_value || b->string_value) {
                        if (!a->string_value || !b->string_value ||
                            strcmp(a->string_value, b->string_value) != 0) {
                            match = 0; break;
                        }
                    } else if (a->value != b->value) {
                        match = 0; break;
                    }
                }
                if (match) { found_index = si; break; }
            }

            if (found_index >= 0) {
                free(sub_ov);
                continue;
            }

            /* Register new sub-specialization. */
            if (spec_count >= spec_cap) {
                int new_cap = (spec_cap == 0) ? 4 : spec_cap * 2;
                IR_ModuleSpec *new_specs = (IR_ModuleSpec *)realloc(
                    specs, sizeof(IR_ModuleSpec) * (size_t)new_cap);
                if (!new_specs) {
                    free(sub_ov);
                    free(specs);
                    return -1;
                }
                specs = new_specs;
                spec_cap = new_cap;
                /* Re-fetch parent_spec after realloc. */
            }

            IR_ModuleSpec *sub_spec = &specs[spec_count];
            memset(sub_spec, 0, sizeof(*sub_spec));
            sub_spec->base_mod_node = child_mod;
            sub_spec->base_scope_index = base_scope_index;
            sub_spec->overrides = sub_ov;
            sub_spec->num_overrides = sub_idx;

            /* Build sub-spec name. */
            const char *sub_base_name = child_mod->name ? child_mod->name : "";
            size_t sbn_len = strlen(sub_base_name);
            size_t sub_est = sbn_len + 7;
            for (int k = 0; k < sub_idx; ++k) {
                size_t nlen = sub_ov[k].name ? strlen(sub_ov[k].name) : 0;
                size_t vlen = sub_ov[k].string_value ? strlen(sub_ov[k].string_value) : 32;
                sub_est += 1 + nlen + 1 + vlen;
            }
            char *sub_tmp = (char *)malloc(sub_est + 1);
            if (!sub_tmp) {
                free(specs);
                return -1;
            }
            size_t spos = 0;
            memcpy(sub_tmp + spos, sub_base_name, sbn_len);
            spos += sbn_len;
            memcpy(sub_tmp + spos, "__SPEC", 6);
            spos += 6;
            for (int k = 0; k < sub_idx; ++k) {
                if (sub_ov[k].string_value) {
                    int w = snprintf(sub_tmp + spos, sub_est + 1 - spos,
                                     "_%s_", sub_ov[k].name ? sub_ov[k].name : "");
                    if (w > 0) spos += (size_t)w;
                    const char *sv = sub_ov[k].string_value;
                    for (size_t si2 = 0; sv[si2] && spos < sub_est; ++si2) {
                        char c = sv[si2];
                        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                            (c >= '0' && c <= '9')) {
                            sub_tmp[spos++] = c;
                        } else {
                            sub_tmp[spos++] = '_';
                        }
                    }
                } else {
                    int w = snprintf(sub_tmp + spos, sub_est + 1 - spos,
                                     "_%s_%lld",
                                     sub_ov[k].name ? sub_ov[k].name : "",
                                     (long long)sub_ov[k].value);
                    if (w > 0) spos += (size_t)w;
                }
            }
            sub_tmp[spos] = '\0';
            sub_spec->spec_name = ir_strdup_arena(arena, sub_tmp);
            free(sub_tmp);

            sub_spec->spec_module_index = -1;
            spec_count++;
        }
    }

    *out_specs = specs;
    *out_spec_count = spec_count;
    return 0;
}

/**
 * @brief Find an IR signal by name within a module.
 *
 * @param mod         IR module to search.
 * @param name        Signal name to find.
 * @return IR signal ID, or -1 if not found.
 */
static int ir_find_signal_id_by_name(const IR_Module *mod, const char *name)
{
    if (!mod || !name || !mod->signals) {
        return -1;
    }
    for (int i = 0; i < mod->num_signals; ++i) {
        const IR_Signal *sig = &mod->signals[i];
        if (sig->name && strcmp(sig->name, name) == 0) {
            return sig->id;
        }
    }
    return -1;
}

/**
 * @brief Compute the bit offset for a BUS signal within a widthof() wire.
 *
 * BUS signals are packed LSB-first (first signal occupies the lowest bits).
 *
 * @param bus_def      BUS definition AST node.
 * @param signal_name  Target signal name.
 * @param mod_scope    Module scope for width evaluation.
 * @param project_symbols Project symbols.
 * @param out_lsb      Output LSB offset.
 * @param out_msb      Output MSB offset.
 * @return 1 on success, 0 on failure.
 */
static int ir_compute_bus_signal_slice(const JZASTNode *bus_def,
                                       const char *signal_name,
                                       const JZModuleScope *mod_scope,
                                       const JZBuffer *project_symbols,
                                       int *out_lsb,
                                       int *out_msb)
{
    if (!bus_def || !signal_name || !out_lsb || !out_msb) {
        return 0;
    }

    int offset = 0;
    for (size_t i = 0; i < bus_def->child_count; ++i) {
        JZASTNode *sig = bus_def->children[i];
        if (!sig || sig->type != JZ_AST_BUS_DECL || !sig->name) {
            continue;
        }

        /* Evaluate signal width. */
        unsigned sig_width = 1;
        if (sig->width && *sig->width) {
            long long w = 0;
            if (sem_eval_const_expr_in_module(sig->width, mod_scope,
                                              project_symbols, &w) == 0 && w > 0) {
                sig_width = (unsigned)w;
            }
        }

        if (strcmp(sig->name, signal_name) == 0) {
            *out_lsb = offset;
            *out_msb = offset + (int)sig_width - 1;
            return 1;
        }
        offset += (int)sig_width;
    }
    return 0;
}

/**
 * @brief Evaluate a @feature guard condition and return the active branch.
 *
 * This is a local equivalent of ir_feature_active_block from ir_build_stmt.c,
 * used here because that function is static.
 */
static JZASTNode *ir_instance_feature_active_block(JZASTNode *feature,
                                                    const JZModuleScope *mod_scope,
                                                    const JZBuffer *project_symbols)
{
    if (!feature || feature->type != JZ_AST_FEATURE_GUARD) return NULL;
    if (!mod_scope) return NULL;
    if (feature->child_count < 2) return NULL;

    JZASTNode *cond = feature->children[0];
    JZASTNode *then_block = (feature->child_count > 1) ? feature->children[1] : NULL;
    JZASTNode *else_block = (feature->child_count > 2) ? feature->children[2] : NULL;

    if (!cond) return then_block;

    char *expr_text = NULL;
    if (ir_expr_to_const_expr_string(cond, &expr_text) != 0 || !expr_text) {
        if (expr_text) free(expr_text);
        return then_block;
    }

    long long value = 0;
    if (sem_eval_const_expr_in_module(expr_text, mod_scope, project_symbols, &value) != 0) {
        free(expr_text);
        return then_block;
    }
    free(expr_text);

    return (value != 0) ? then_block : else_block;
}

/**
 * @brief Collect active MODULE_INSTANCE nodes from a module, evaluating
 *        module-scope @feature guards to select the active branch.
 */
static void collect_active_instances(JZASTNode *ast_mod,
                                      const JZModuleScope *mod_scope,
                                      const JZBuffer *project_symbols,
                                      JZASTNode **out_nodes,
                                      int *out_count,
                                      int max_count)
{
    *out_count = 0;
    if (!ast_mod) return;

    for (size_t i = 0; i < ast_mod->child_count; ++i) {
        JZASTNode *child = ast_mod->children[i];
        if (!child) continue;

        if (child->type == JZ_AST_MODULE_INSTANCE) {
            if (*out_count < max_count) {
                out_nodes[(*out_count)++] = child;
            }
        } else if (child->type == JZ_AST_FEATURE_GUARD) {
            JZASTNode *active = ir_instance_feature_active_block(child, mod_scope, project_symbols);
            if (!active) continue;
            for (size_t j = 0; j < active->child_count; ++j) {
                JZASTNode *bchild = active->children[j];
                if (bchild && bchild->type == JZ_AST_MODULE_INSTANCE) {
                    if (*out_count < max_count) {
                        out_nodes[(*out_count)++] = bchild;
                    }
                }
            }
        }
    }
}

/**
 * @brief Build IR_Instance entries for a module.
 *
 * Walks @new instance declarations in the module AST and elaborates them
 * into IR_Instance entries, including:
 * - Instance arrays
 * - Port connections
 * - OVERRIDE-based specialization selection
 *
 * When a matching module specialization exists, instances reference the
 * specialized IR module instead of the generic base module.
 *
 * @param scope           Parent module scope.
 * @param mod             IR module being populated.
 * @param all_scopes      All module scopes in the design.
 * @param scope_count     Number of module scopes.
 * @param project_symbols Project-level symbol table.
 * @param specs           Module specialization table.
 * @param spec_count      Number of specializations.
 * @param all_modules     All IR modules (for BUS signal lookup).
 * @param parent_bus_map  Parent module's BUS signal mapping.
 * @param parent_bus_map_count Number of entries in parent_bus_map.
 * @param arena           Arena for IR allocation.
 * @return 0 on success, non-zero on failure.
 */
int ir_build_instances_for_module(const JZModuleScope *scope,
                                  IR_Module *mod,
                                  const JZModuleScope *all_scopes,
                                  size_t scope_count,
                                  const JZBuffer *project_symbols,
                                  const IR_ModuleSpec *specs,
                                  int spec_count,
                                  IR_Module *all_modules,
                                  const IR_BusSignalMapping *parent_bus_map,
                                  int parent_bus_map_count,
                                  JZArena *arena)
{
    if (!scope || !scope->node || !mod || !all_scopes || scope_count == 0 || !arena) {
        return -1;
    }

    JZASTNode *ast_mod = scope->node;

    /* Collect all active MODULE_INSTANCE nodes (evaluating feature guards). */
    JZASTNode *active_inst_nodes[512];
    int active_inst_node_count = 0;
    collect_active_instances(ast_mod, scope, project_symbols,
                             active_inst_nodes, &active_inst_node_count, 512);

    /* First pass: count @new instances.
     *
     * For instance arrays (@new u[N] Child { ... }), we allocate one IR_Instance
     * per element, so the total count is the sum of all per-instance array
     * lengths. The array length is stored in inst->width as a simple integer
     * expression; we reuse eval_simple_positive_decl_int here. If the count
     * expression is invalid or nonpositive, we conservatively treat the
     * instance as scalar (count = 1) and rely on the semantic pass to have
     * already reported INSTANCE_ARRAY_COUNT_INVALID.
     */
    int instance_count = 0;
    for (int ai = 0; ai < active_inst_node_count; ++ai) {
        JZASTNode *child = active_inst_nodes[ai];
        if (!child || child->type != JZ_AST_MODULE_INSTANCE) {
            continue;
        }
        unsigned count = 1;
        if (child->width && *child->width) {
            unsigned tmp = 0;
            int rc = eval_simple_positive_decl_int(child->width, &tmp);
            if (rc == 1 && tmp > 0) {
                count = tmp;
            }
        }
        instance_count += (int)count;
    }

    if (instance_count == 0) {
        mod->instances = NULL;
        mod->num_instances = 0;
        return 0;
    }

    IR_Instance *instances = (IR_Instance *)jz_arena_alloc(arena, sizeof(IR_Instance) * (size_t)instance_count);
    if (!instances) {
        return -1;
    }
    memset(instances, 0, sizeof(IR_Instance) * (size_t)instance_count);

    int inst_index = 0;
    for (int ai = 0; ai < active_inst_node_count && inst_index < instance_count; ++ai) {
        JZASTNode *inst_node = active_inst_nodes[ai];
        if (!inst_node || inst_node->type != JZ_AST_MODULE_INSTANCE) {
            continue;
        }

        /* Determine how many IR instances to create for this AST instance.
         * For @new u[N] Child { ... }, we create N IR_Instance entries with
         * distinct names (e.g., "u[0]", "u[1]", ...). If the count cannot be
         * resolved to a positive integer, we fall back to a single instance
         * and rely on the semantic layer to have reported the error.
         */
        unsigned array_count = 1;
        if (inst_node->width && *inst_node->width) {
            unsigned tmp = 0;
            int rc = eval_simple_positive_decl_int(inst_node->width, &tmp);
            if (rc == 1 && tmp > 0) {
                array_count = tmp;
            }
        }

        /* Resolve the child module/blackbox and map it to an IR_Module id. */
        const JZSymbol *child_sym = NULL;
        if (project_symbols && inst_node->text) {
            child_sym = ir_project_lookup_module_or_blackbox(project_symbols,
                                                              inst_node->text);
        }
        const JZModuleScope *child_scope = NULL;
        int child_mod_index = -1;
        if (child_sym && child_sym->node) {
            child_mod_index = ir_find_module_index_for_node(all_scopes,
                                                            scope_count,
                                                            child_sym->node);
            if (child_mod_index >= 0) {
                child_scope = &all_scopes[(size_t)child_mod_index];
            }
        }

        /* If this instance has an OVERRIDE set and a matching specialization
         * exists for the resolved child module, compute the final child
         * module id once; all elements of the array share the same child.
         */
        int effective_child_mod_id = child_mod_index;
        if (specs && spec_count > 0 && child_scope && child_sym &&
            child_sym->kind == JZ_SYM_MODULE) {
            IR_ModuleSpecOverride *ov = NULL;
            int ov_count = 0;
            /* Use parent scope for override evaluation so that parent
             * CONST references (e.g. MY_WIDTH) resolve correctly.
             */
            if (ir_collect_instance_overrides(inst_node,
                                              scope,
                                              project_symbols,
                                              &ov,
                                              &ov_count) != 0) {
                if (ov) free(ov);
                return -1;
            }
            if (ov && ov_count > 0) {
                int matched_index = -1;
                for (int si = 0; si < spec_count; ++si) {
                    const IR_ModuleSpec *spec = &specs[si];
                    if (spec->base_scope_index != child_mod_index) {
                        continue;
                    }
                    if (spec->num_overrides != ov_count) {
                        continue;
                    }
                    int match = 1;
                    for (int k = 0; k < ov_count; ++k) {
                        const IR_ModuleSpecOverride *a = &spec->overrides[k];
                        const IR_ModuleSpecOverride *b = &ov[k];
                        if ((!a->name && b->name) || (a->name && !b->name)) {
                            match = 0;
                            break;
                        }
                        if (a->name && b->name && strcmp(a->name, b->name) != 0) {
                            match = 0;
                            break;
                        }
                        if (a->string_value || b->string_value) {
                            if (!a->string_value || !b->string_value ||
                                strcmp(a->string_value, b->string_value) != 0) {
                                match = 0;
                                break;
                            }
                        } else if (a->value != b->value) {
                            match = 0;
                            break;
                        }
                    }
                    if (match) {
                        matched_index = spec->spec_module_index;
                        break;
                    }
                }
                if (matched_index >= 0) {
                    effective_child_mod_id = matched_index;
                }
            }
            if (ov) free(ov);
        }

        /* Collect structured IN/OUT/INOUT bindings from the instance body once;
         * all array elements share the same binding pattern.
         *
         * For BUS bindings, we expand each binding into multiple connections
         * (one per signal in the BUS definition).
         */
        int connection_count = 0;
        for (size_t c = 0; c < inst_node->child_count; ++c) {
            JZASTNode *b = inst_node->children[c];
            if (!b || b->type != JZ_AST_PORT_DECL || !b->name) {
                continue;
            }
            if (b->block_kind && strcmp(b->block_kind, "BUS") == 0 && b->text) {
                /* BUS binding: count signals in the BUS definition. */
                char bus_id[256] = {0};
                const char *space = strchr(b->text, ' ');
                if (space) {
                    size_t len = (size_t)(space - b->text);
                    if (len >= sizeof(bus_id)) len = sizeof(bus_id) - 1;
                    memcpy(bus_id, b->text, len);
                    bus_id[len] = '\0';
                } else {
                    size_t len = strlen(b->text);
                    if (len >= sizeof(bus_id)) len = sizeof(bus_id) - 1;
                    memcpy(bus_id, b->text, len);
                    bus_id[len] = '\0';
                }
                const JZSymbol *bus_sym = ir_lookup_bus_def(project_symbols, bus_id);
                if (bus_sym && bus_sym->node) {
                    /* Determine array count from binding width. */
                    unsigned bus_array_count = 1;
                    if (b->width) {
                        unsigned tmp = 0;
                        if (eval_simple_positive_decl_int(b->width, &tmp) == 1 && tmp > 0) {
                            bus_array_count = tmp;
                        } else {
                            long long cval = 0;
                            if (sem_eval_const_expr_in_module(b->width,
                                    child_scope, project_symbols, &cval) == 0 && cval > 0) {
                                bus_array_count = (unsigned)cval;
                            }
                        }
                    }
                    for (size_t si = 0; si < bus_sym->node->child_count; ++si) {
                        if (bus_sym->node->children[si] &&
                            bus_sym->node->children[si]->type == JZ_AST_BUS_DECL) {
                            connection_count += (int)bus_array_count;
                        }
                    }
                }
            } else {
                /* Non-BUS binding: 1 connection. */
                connection_count++;
            }
        }

        IR_InstanceConnection *template_conns = NULL;
        int template_conn_count = 0;
        if (connection_count > 0 && child_scope && effective_child_mod_id >= 0) {
            template_conns = (IR_InstanceConnection *)jz_arena_alloc(
                arena, sizeof(IR_InstanceConnection) * (size_t)connection_count);
            if (!template_conns) {
                return -1;
            }
            memset(template_conns, 0, sizeof(IR_InstanceConnection) * (size_t)connection_count);

            /* Get child IR module for signal lookup.
             *
             * When the instance uses a specialization (effective_child_mod_id
             * differs from child_mod_index), the specialization module slot
             * has not been materialized yet at this point in the pipeline.
             * Fall back to the base module for signal-name lookups since
             * specializations share the same signal names as their base.
             */
            IR_Module *child_ir_mod = NULL;
            if (all_modules && effective_child_mod_id >= 0) {
                child_ir_mod = &all_modules[effective_child_mod_id];
                if (child_ir_mod->num_signals == 0 && child_mod_index >= 0 &&
                    effective_child_mod_id != child_mod_index) {
                    child_ir_mod = &all_modules[child_mod_index];
                }
            }

            int conn_index = 0;
            for (size_t c = 0; c < inst_node->child_count && conn_index < connection_count; ++c) {
                JZASTNode *bind = inst_node->children[c];
                if (!bind || bind->type != JZ_AST_PORT_DECL || !bind->name) {
                    continue;
                }

                /* Check for BUS binding. */
                if (bind->block_kind && strcmp(bind->block_kind, "BUS") == 0 && bind->text) {
                    /* Parse "bus_id ROLE" from bind->text. */
                    char bus_id[256] = {0};
                    const char *space = strchr(bind->text, ' ');
                    if (space) {
                        size_t len = (size_t)(space - bind->text);
                        if (len >= sizeof(bus_id)) len = sizeof(bus_id) - 1;
                        memcpy(bus_id, bind->text, len);
                        bus_id[len] = '\0';
                    }

                    /* Look up BUS definition. */
                    const JZSymbol *bus_sym = ir_lookup_bus_def(project_symbols, bus_id);
                    if (!bus_sym || !bus_sym->node) {
                        continue;
                    }
                    const JZASTNode *bus_def = bus_sym->node;

                    /* Determine array count from binding width. */
                    unsigned bus_array_count = 1;
                    if (bind->width) {
                        unsigned tmp = 0;
                        if (eval_simple_positive_decl_int(bind->width, &tmp) == 1 && tmp > 0) {
                            bus_array_count = tmp;
                        } else {
                            long long cval = 0;
                            if (sem_eval_const_expr_in_module(bind->width,
                                    child_scope, project_symbols, &cval) == 0 && cval > 0) {
                                bus_array_count = (unsigned)cval;
                            }
                        }
                    }

                    /* Get parent signal info from RHS expression.
                     * For non-array: single identifier (e.g., "cpu_bus")
                     * For array: concatenation of identifiers (e.g., {btn_bus, led_bus, ...})
                     */
                    JZASTNode *rhs_expr = (bind->child_count > 0) ? bind->children[0] : NULL;

                    /* Build an array of parent signal names, one per array element.
                     * For concatenation: elements are MSB-first in source, but array
                     * element 0 is the last (LSB) element.
                     */
                    const char *parent_names[256];
                    unsigned parent_name_count = 0;
                    memset(parent_names, 0, sizeof(parent_names));

                    if (rhs_expr && rhs_expr->type == JZ_AST_EXPR_CONCAT) {
                        /* Concatenation: {elem_MSB, ..., elem_LSB}
                         * Array element 0 maps to the last concat child (LSB). */
                        for (size_t ri = 0; ri < rhs_expr->child_count && parent_name_count < 256; ++ri) {
                            JZASTNode *elem = rhs_expr->children[rhs_expr->child_count - 1 - ri];
                            if (elem && elem->type == JZ_AST_EXPR_IDENTIFIER && elem->name) {
                                parent_names[parent_name_count++] = elem->name;
                            }
                        }
                    } else if (rhs_expr && rhs_expr->type == JZ_AST_EXPR_IDENTIFIER && rhs_expr->name) {
                        /* Single identifier: used for all elements (non-array case). */
                        parent_names[0] = rhs_expr->name;
                        parent_name_count = 1;
                    }

                    if (parent_name_count == 0) {
                        continue;
                    }

                    /* Iterate over array elements. */
                    for (unsigned elem = 0; elem < bus_array_count; ++elem) {
                        /* Determine parent signal name for this element. */
                        const char *parent_sig_name = NULL;
                        if (bus_array_count > 1 && elem < parent_name_count) {
                            parent_sig_name = parent_names[elem];
                        } else {
                            parent_sig_name = parent_names[0];
                        }
                        if (!parent_sig_name) continue;

                        /* Check if parent signal is a wide wire or expanded BUS. */
                        const JZSymbol *parent_sym = module_scope_lookup(scope, parent_sig_name);
                        int parent_is_wide_wire = 0;
                        int parent_wide_wire_id = -1;
                        if (parent_sym && parent_sym->kind == JZ_SYM_WIRE) {
                            int found_in_bus_map = 0;
                            for (int bi = 0; bi < parent_bus_map_count; ++bi) {
                                if (parent_bus_map[bi].bus_port_name &&
                                    strcmp(parent_bus_map[bi].bus_port_name, parent_sig_name) == 0) {
                                    found_in_bus_map = 1;
                                    break;
                                }
                            }
                            if (!found_in_bus_map) {
                                parent_is_wide_wire = 1;
                                parent_wide_wire_id = parent_sym->id;
                            }
                        }

                        /* For each signal in the BUS definition, create a connection. */
                        for (size_t si = 0; si < bus_def->child_count; ++si) {
                            JZASTNode *bus_sig = bus_def->children[si];
                            if (!bus_sig || bus_sig->type != JZ_AST_BUS_DECL || !bus_sig->name) {
                                continue;
                            }

                            /* Build child expanded signal name.
                             * Array: "{port_name}{elem}_{signal_name}" (e.g., "tgt0_ADDR")
                             * Non-array: "{port_name}_{signal_name}" (e.g., "src_ADDR")
                             */
                            char child_sig_name[512];
                            if (bus_array_count > 1) {
                                if (snprintf(child_sig_name, sizeof(child_sig_name), "%s%u_%s",
                                             bind->name, elem, bus_sig->name) < 0) {
                                    continue;
                                }
                            } else {
                                if (snprintf(child_sig_name, sizeof(child_sig_name), "%s_%s",
                                             bind->name, bus_sig->name) < 0) {
                                    continue;
                                }
                            }

                            /* Find child signal ID in child module's IR signals. */
                            int child_port_id = -1;
                            if (child_ir_mod) {
                                child_port_id = ir_find_signal_id_by_name(child_ir_mod, child_sig_name);
                            }
                            if (child_port_id < 0) {
                                continue;
                            }

                            int parent_signal_id = -1;
                            int parent_msb = -1;
                            int parent_lsb = -1;

                            if (parent_is_wide_wire) {
                                /* Parent is a wide wire: compute slice offset. */
                                parent_signal_id = parent_wide_wire_id;
                                ir_compute_bus_signal_slice(bus_def, bus_sig->name,
                                                            scope, project_symbols,
                                                            &parent_lsb, &parent_msb);
                            } else {
                                /* Parent has expanded BUS signals: look up by name. */
                                int parent_array_idx = (bus_array_count > 1) ? (int)elem : -1;
                                parent_signal_id = ir_lookup_bus_signal_id(
                                    parent_bus_map, parent_bus_map_count,
                                    parent_sig_name, bus_sig->name, parent_array_idx);
                            }

                            if (child_port_id >= 0 && parent_signal_id >= 0) {
                                IR_InstanceConnection *ic = &template_conns[conn_index++];
                                ic->parent_signal_id = parent_signal_id;
                                ic->child_port_id = child_port_id;
                                ic->parent_msb = parent_msb;
                                ic->parent_lsb = parent_lsb;
                                ic->const_expr = NULL;
                            }
                        }
                    }
                    continue;
                }

                /* Non-BUS binding: original handling. */

                /* Child port: resolve within child module scope as a PORT symbol
                 * and use its stable semantic id as child_port_id.
                 */
                const JZSymbol *child_port_sym = module_scope_lookup_kind(child_scope,
                                                                          bind->name,
                                                                          JZ_SYM_PORT);
                int child_port_id = child_port_sym ? child_port_sym->id : -1;

                /* Parent signal: instance RHS expression is restricted to
                 * signal-like forms (identifier or optional single literal
                 * slice). Map down to the base signal and use its stable
                 * semantic id as parent_signal_id. When a literal slice is
                 * present (e.g., led[5:3]), also record the parent_msb/lsb
                 * range so the IR preserves the bit-range mapping.
                 * Qualified identifiers (GLOBAL./CONFIG.) are not modeled as
                 * connections in the current IR.
                 */
                int parent_signal_id = -1;
                int parent_msb = -1;
                int parent_lsb = -1;
                const char *const_expr = NULL;
                if (bind->child_count > 0) {
                    JZASTNode *rhs = bind->children[0];
                    JZASTNode *base = rhs;
                    if (rhs && rhs->type == JZ_AST_EXPR_LITERAL && rhs->text) {
                        const_expr = rhs->text;
                    } else if (rhs && rhs->type == JZ_AST_EXPR_CONCAT && rhs->child_count > 0) {
                        /* Concatenation of literals: build "{lit, lit, ...}" string. */
                        int all_literals = 1;
                        for (size_t ri = 0; ri < rhs->child_count; ++ri) {
                            JZASTNode *elem = rhs->children[ri];
                            if (!elem || elem->type != JZ_AST_EXPR_LITERAL || !elem->text) {
                                all_literals = 0;
                                break;
                            }
                        }
                        if (all_literals) {
                            char concat_buf[512];
                            int pos = 0;
                            concat_buf[pos++] = '{';
                            for (size_t ri = 0; ri < rhs->child_count; ++ri) {
                                if (ri > 0) {
                                    concat_buf[pos++] = ',';
                                    concat_buf[pos++] = ' ';
                                }
                                const char *lt = rhs->children[ri]->text;
                                size_t lt_len = strlen(lt);
                                if (pos + (int)lt_len + 2 >= (int)sizeof(concat_buf)) break;
                                memcpy(concat_buf + pos, lt, lt_len);
                                pos += (int)lt_len;
                            }
                            concat_buf[pos++] = '}';
                            concat_buf[pos] = '\0';
                            const_expr = ir_strdup_arena(arena, concat_buf);
                        } else {
                            /* Concatenation of signals (and/or literals):
                             * build "{name, name, ...}" Verilog expression.
                             * Each element must be a literal or a valid
                             * signal identifier in the parent scope.
                             */
                            int all_valid = 1;
                            for (size_t ri = 0; ri < rhs->child_count; ++ri) {
                                JZASTNode *elem = rhs->children[ri];
                                if (!elem) { all_valid = 0; break; }
                                if (elem->type == JZ_AST_EXPR_LITERAL && elem->text) continue;
                                if (elem->type == JZ_AST_EXPR_IDENTIFIER && elem->name) {
                                    const JZSymbol *esym = module_scope_lookup(scope, elem->name);
                                    if (!esym || (esym->kind != JZ_SYM_PORT &&
                                                  esym->kind != JZ_SYM_WIRE &&
                                                  esym->kind != JZ_SYM_REGISTER)) {
                                        all_valid = 0;
                                        break;
                                    }
                                    continue;
                                }
                                all_valid = 0;
                                break;
                            }
                            if (all_valid) {
                                char concat_buf[512];
                                int pos = 0;
                                concat_buf[pos++] = '{';
                                for (size_t ri = 0; ri < rhs->child_count; ++ri) {
                                    JZASTNode *elem = rhs->children[ri];
                                    if (ri > 0) {
                                        concat_buf[pos++] = ',';
                                        concat_buf[pos++] = ' ';
                                    }
                                    const char *t = (elem->type == JZ_AST_EXPR_LITERAL)
                                                    ? elem->text : elem->name;
                                    size_t t_len = strlen(t);
                                    if (pos + (int)t_len + 2 >= (int)sizeof(concat_buf)) break;
                                    memcpy(concat_buf + pos, t, t_len);
                                    pos += (int)t_len;
                                }
                                concat_buf[pos++] = '}';
                                concat_buf[pos] = '\0';
                                const_expr = ir_strdup_arena(arena, concat_buf);
                            }
                        }
                    } else if (rhs && rhs->type == JZ_AST_EXPR_SLICE && rhs->child_count >= 3) {
                        base = rhs->children[0];
                        JZASTNode *msb_node = rhs->children[1];
                        JZASTNode *lsb_node = rhs->children[2];
                        if (msb_node && lsb_node &&
                            msb_node->type == JZ_AST_EXPR_LITERAL && msb_node->text &&
                            lsb_node->type == JZ_AST_EXPR_LITERAL && lsb_node->text) {
                            unsigned msb_val = 0, lsb_val = 0;
                            if (parse_simple_nonnegative_int(msb_node->text, &msb_val) &&
                                parse_simple_nonnegative_int(lsb_node->text, &lsb_val)) {
                                parent_msb = (int)msb_val;
                                parent_lsb = (int)lsb_val;
                            }
                        }
                    }
                    if (base && base->type == JZ_AST_EXPR_IDENTIFIER && base->name) {
                        const JZSymbol *psym = module_scope_lookup(scope, base->name);
                        if (psym && (psym->kind == JZ_SYM_PORT ||
                                     psym->kind == JZ_SYM_WIRE ||
                                     psym->kind == JZ_SYM_REGISTER)) {
                            parent_signal_id = psym->id;
                        }
                    }
                    /* Handle qualified identifiers (e.g., DEV.ROM, GLOBAL.CONST)
                     * as constant expressions for port bindings.
                     */
                    if (rhs && rhs->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER && rhs->name &&
                        project_symbols && project_symbols->data) {
                        const char *full = rhs->name;
                        const char *dot = strchr(full, '.');
                        if (dot && dot > full && *(dot + 1)) {
                            /* Split into namespace and constant name. */
                            char ns_name[128];
                            size_t ns_len = (size_t)(dot - full);
                            if (ns_len < sizeof(ns_name)) {
                                memcpy(ns_name, full, ns_len);
                                ns_name[ns_len] = '\0';
                                const char *cname = dot + 1;

                                /* Look up the @global namespace. */
                                const JZSymbol *proj_syms = (const JZSymbol *)project_symbols->data;
                                size_t proj_count = project_symbols->len / sizeof(JZSymbol);
                                const JZSymbol *glob_sym = NULL;
                                for (size_t gi = 0; gi < proj_count; ++gi) {
                                    if (proj_syms[gi].kind == JZ_SYM_GLOBAL &&
                                        proj_syms[gi].name &&
                                        strcmp(proj_syms[gi].name, ns_name) == 0) {
                                        glob_sym = &proj_syms[gi];
                                        break;
                                    }
                                }

                                if (glob_sym && glob_sym->node) {
                                    /* Find the constant within the @global block. */
                                    for (size_t ci = 0; ci < glob_sym->node->child_count; ++ci) {
                                        JZASTNode *gc = glob_sym->node->children[ci];
                                        if (!gc || !gc->name || !gc->text) continue;
                                        if (strcmp(gc->name, cname) != 0) continue;

                                        /* gc->text is a sized literal like "3'b000".
                                         * Use it directly as the const_expr.
                                         */
                                        const_expr = ir_strdup_arena(arena, gc->text);
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }

                if ((parent_signal_id >= 0 || const_expr) && child_port_id >= 0) {
                    IR_InstanceConnection *ic = &template_conns[conn_index++];
                    ic->parent_signal_id = parent_signal_id;
                    ic->child_port_id = child_port_id;
                    ic->parent_msb = parent_msb;
                    ic->parent_lsb = parent_lsb;
                    ic->const_expr = const_expr ? ir_strdup_arena(arena, const_expr) : NULL;
                }
            }

            template_conn_count = conn_index;
        }

        /* Finally, materialize one IR_Instance per array element, copying the
         * shared connection template and assigning distinct instance names. */
        for (unsigned elem = 0; elem < array_count && inst_index < instance_count; ++elem) {
            IR_Instance *ir_inst = &instances[inst_index];
            ir_inst->id = inst_index;

            /* Name: for arrays, synthesize "u[elem]"; for scalars, keep the
             * original instance name.
             */
            const char *base_name = inst_node->name ? inst_node->name : "";
            if (array_count > 1) {
                char buf[256];
                if (snprintf(buf, sizeof(buf), "%s[%u]", base_name, elem) < 0) {
                    return -1;
                }
                ir_inst->name = ir_strdup_arena(arena, buf);
            } else {
                ir_inst->name = ir_strdup_arena(arena, base_name);
            }

            ir_inst->child_module_id = effective_child_mod_id >= 0 ? effective_child_mod_id : -1;
            ir_inst->connections = NULL;
            ir_inst->num_connections = 0;

            if (template_conn_count > 0) {
                IR_InstanceConnection *conns = (IR_InstanceConnection *)jz_arena_alloc(
                    arena, sizeof(IR_InstanceConnection) * (size_t)template_conn_count);
                if (!conns) {
                    return -1;
                }
                memcpy(conns,
                       template_conns,
                       sizeof(IR_InstanceConnection) * (size_t)template_conn_count);
                ir_inst->connections = conns;
                ir_inst->num_connections = template_conn_count;
            }

            inst_index++;
        }
    }

    mod->instances = instances;
    mod->num_instances = inst_index;
    return 0;
}
