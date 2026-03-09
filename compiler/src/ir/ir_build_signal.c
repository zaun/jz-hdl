/**
 * @file ir_build_signal.c
 * @brief Signal construction and constant/width evaluation helpers.
 *
 * This file contains helpers for constructing IR_Signal entries from
 * semantic symbols, resolving signals by ID, and evaluating constant
 * and width expressions with specialization overrides applied.
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#include "ir_internal.h"

/* ============================================================================
 * BUS Signal Expansion Helpers
 * ============================================================================
 */

/**
 * @brief Parse BUS port metadata from the text field.
 *
 * BUS ports encode "bus_id ROLE" in their text field. This helper extracts
 * both components.
 *
 * @param port_decl      BUS port declaration AST node.
 * @param out_bus_id     Output buffer for BUS identifier.
 * @param bus_id_size    Size of bus_id buffer.
 * @param out_role       Output buffer for role (SOURCE/TARGET).
 * @param role_size      Size of role buffer.
 * @return 1 on success, 0 on failure.
 */
static int ir_parse_bus_port_meta(const JZASTNode *port_decl,
                                  char *out_bus_id,
                                  size_t bus_id_size,
                                  char *out_role,
                                  size_t role_size)
{
    if (!port_decl || !out_bus_id || !out_role || bus_id_size == 0 || role_size == 0) {
        return 0;
    }
    out_bus_id[0] = '\0';
    out_role[0] = '\0';

    const char *meta = port_decl->text;
    if (!meta || !*meta) {
        return 0;
    }

    /* Skip leading whitespace. */
    while (*meta && isspace((unsigned char)*meta)) {
        ++meta;
    }

    /* Extract bus_id (first token). */
    const char *bus_start = meta;
    while (*meta && !isspace((unsigned char)*meta)) {
        ++meta;
    }
    size_t bus_len = (size_t)(meta - bus_start);
    if (bus_len == 0 || bus_len >= bus_id_size) {
        return 0;
    }
    memcpy(out_bus_id, bus_start, bus_len);
    out_bus_id[bus_len] = '\0';

    /* Skip whitespace between bus_id and role. */
    while (*meta && isspace((unsigned char)*meta)) {
        ++meta;
    }

    /* Extract role (second token). */
    const char *role_start = meta;
    while (*meta && !isspace((unsigned char)*meta)) {
        ++meta;
    }
    size_t role_len = (size_t)(meta - role_start);
    if (role_len == 0 || role_len >= role_size) {
        return 0;
    }
    memcpy(out_role, role_start, role_len);
    out_role[role_len] = '\0';

    return 1;
}

/**
 * @brief Compute IR port direction from BUS signal direction and role.
 *
 * The direction mapping follows the BUS semantics:
 * - INOUT remains INOUT regardless of role.
 * - For SOURCE role: OUT -> OUTPUT, IN -> INPUT
 * - For TARGET role: OUT -> INPUT, IN -> OUTPUT (reversed)
 *
 * @param signal_dir  Signal direction from BUS definition ("IN", "OUT", "INOUT").
 * @param role        Port role ("SOURCE" or "TARGET").
 * @return Computed IR_PortDirection.
 */
static IR_PortDirection ir_compute_bus_signal_direction(const char *signal_dir,
                                                         const char *role)
{
    if (!signal_dir || !role) {
        return PORT_INOUT;
    }

    if (strcmp(signal_dir, "INOUT") == 0) {
        return PORT_INOUT;
    }

    int is_source = (strcmp(role, "SOURCE") == 0);

    if (strcmp(signal_dir, "OUT") == 0) {
        return is_source ? PORT_OUT : PORT_IN;
    }
    if (strcmp(signal_dir, "IN") == 0) {
        return is_source ? PORT_IN : PORT_OUT;
    }

    return PORT_INOUT;
}

/**
 * @brief Look up a BUS definition in the project symbols.
 *
 * @param project_symbols  Project symbol table.
 * @param bus_id           BUS identifier name.
 * @return Pointer to BUS block AST node, or NULL if not found.
 */
static const JZASTNode *ir_lookup_bus_def(const JZBuffer *project_symbols,
                                           const char *bus_id)
{
    if (!project_symbols || !project_symbols->data || !bus_id) {
        return NULL;
    }

    const JZSymbol *syms = (const JZSymbol *)project_symbols->data;
    size_t count = project_symbols->len / sizeof(JZSymbol);

    for (size_t i = 0; i < count; ++i) {
        const JZSymbol *s = &syms[i];
        if (s->kind != JZ_SYM_BUS || !s->name || !s->node) {
            continue;
        }
        if (strcmp(s->name, bus_id) == 0) {
            return s->node;
        }
    }
    return NULL;
}

/**
 * @brief Count the number of signals in a BUS definition.
 *
 * @param bus_def  BUS block AST node.
 * @return Number of signal declarations in the BUS.
 */
static int ir_count_bus_signals(const JZASTNode *bus_def)
{
    if (!bus_def) {
        return 0;
    }
    int count = 0;
    for (size_t i = 0; i < bus_def->child_count; ++i) {
        JZASTNode *child = bus_def->children[i];
        if (child && child->type == JZ_AST_BUS_DECL) {
            ++count;
        }
    }
    return count;
}

/**
 * @brief Look up an expanded BUS signal's IR ID from a BUS access.
 */
int ir_lookup_bus_signal_id(const IR_BusSignalMapping *map,
                            int map_count,
                            const char *port_name,
                            const char *signal_name,
                            int array_index)
{
    if (!map || map_count <= 0 || !port_name || !signal_name) {
        return -1;
    }

    for (int i = 0; i < map_count; ++i) {
        const IR_BusSignalMapping *m = &map[i];
        if (!m->bus_port_name || !m->signal_name) {
            continue;
        }
        if (strcmp(m->bus_port_name, port_name) != 0) {
            continue;
        }
        if (strcmp(m->signal_name, signal_name) != 0) {
            continue;
        }
        if (m->array_index != array_index) {
            continue;
        }
        return m->ir_signal_id;
    }
    return -1;
}

/**
 * @brief Build IR_Signal entries for a module.
 *
 * Converts semantic PORT, WIRE, REGISTER, and LATCH symbols into IR_Signal
 * structures with stable IDs, names, widths, and metadata.
 *
 * BUS ports are expanded into individual IR_Signal entries for each member
 * signal in the BUS definition. The bus signal mapping is returned so that
 * expression/statement builders can resolve BUS access expressions to the
 * expanded signal IDs.
 *
 * @param scope             Module scope containing semantic symbols.
 * @param owner_module_id   Owning IR module ID.
 * @param arena             Arena for IR allocation.
 * @param project_symbols   Project-level symbols (for BUS definitions).
 * @param out_signals       Output array of IR_Signal pointers.
 * @param out_count         Output number of signals.
 * @param out_bus_map       Output BUS signal mapping array (may be NULL).
 * @param out_bus_map_count Output BUS signal mapping count (may be NULL).
 * @return 0 on success, non-zero on failure.
 */
int ir_build_signals_for_module(const JZModuleScope *scope,
                                int owner_module_id,
                                JZArena *arena,
                                const JZBuffer *project_symbols,
                                IR_Signal **out_signals,
                                int *out_count,
                                IR_BusSignalMapping **out_bus_map,
                                int *out_bus_map_count)
{
    if (!scope || !arena || !out_signals || !out_count) {
        return -1;
    }

    /* Initialize optional output parameters. */
    if (out_bus_map) *out_bus_map = NULL;
    if (out_bus_map_count) *out_bus_map_count = 0;

    size_t sym_count = scope->symbols.len / sizeof(JZSymbol);
    const JZSymbol *syms = (const JZSymbol *)scope->symbols.data;

    /* First pass: count signals including expanded BUS signals.
     * For BUS ports, we count the number of signals in the BUS definition
     * multiplied by the array count (default 1).
     */
    int count = 0;
    int bus_map_count = 0;
    for (size_t i = 0; i < sym_count; ++i) {
        const JZSymbol *sym = &syms[i];
        if (sym->kind == JZ_SYM_PORT) {
            /* Check if this is a BUS port. */
            if (sym->node && sym->node->block_kind &&
                strcmp(sym->node->block_kind, "BUS") == 0) {
                /* BUS port: look up the BUS definition and count signals. */
                char bus_id[128];
                char role[128];
                if (ir_parse_bus_port_meta(sym->node, bus_id, sizeof(bus_id),
                                           role, sizeof(role))) {
                    const JZASTNode *bus_def = ir_lookup_bus_def(project_symbols, bus_id);
                    if (bus_def) {
                        int bus_sig_count = ir_count_bus_signals(bus_def);
                        /* Determine array count from port width (default 1). */
                        unsigned array_count = 1;
                        if (sym->node->width) {
                            unsigned tmp = 0;
                            if (eval_simple_positive_decl_int(sym->node->width, &tmp) == 1 && tmp > 0) {
                                array_count = tmp;
                            } else {
                                /* Try resolving CONST/CONFIG expression. */
                                long long val = 0;
                                if (sem_eval_const_expr_in_module(sym->node->width,
                                        scope, project_symbols, &val) == 0 && val > 0) {
                                    array_count = (unsigned)val;
                                }
                            }
                        }
                        count += bus_sig_count * (int)array_count;
                        bus_map_count += bus_sig_count * (int)array_count;
                    }
                }
            } else {
                /* Regular port. */
                count++;
            }
        } else if (sym->kind == JZ_SYM_WIRE ||
                   sym->kind == JZ_SYM_REGISTER ||
                   sym->kind == JZ_SYM_LATCH) {
            count++;
        }
    }

    if (count == 0) {
        *out_signals = NULL;
        *out_count = 0;
        return 0;
    }

    IR_Signal *signals = (IR_Signal *)jz_arena_alloc(arena, sizeof(IR_Signal) * (size_t)count);
    if (!signals) {
        return -1;
    }
    memset(signals, 0, sizeof(IR_Signal) * (size_t)count);

    /* Allocate bus signal mapping if needed. */
    IR_BusSignalMapping *bus_map = NULL;
    if (bus_map_count > 0) {
        bus_map = (IR_BusSignalMapping *)jz_arena_alloc(
            arena, sizeof(IR_BusSignalMapping) * (size_t)bus_map_count);
        if (!bus_map) {
            return -1;
        }
        memset(bus_map, 0, sizeof(IR_BusSignalMapping) * (size_t)bus_map_count);
    }

    /* Use a synthetic signal ID base that won't conflict with semantic IDs.
     * Semantic IDs are typically small positive integers. We start from a
     * high value to avoid collisions.
     */
    int next_synthetic_id = 100000;
    int idx = 0;
    int bus_map_idx = 0;

    for (size_t i = 0; i < sym_count; ++i) {
        const JZSymbol *sym = &syms[i];

        if (sym->kind == JZ_SYM_PORT) {
            /* Check if this is a BUS port. */
            if (sym->node && sym->node->block_kind &&
                strcmp(sym->node->block_kind, "BUS") == 0) {
                /* BUS port: expand into individual signals. */
                char bus_id[128];
                char role[128];
                if (!ir_parse_bus_port_meta(sym->node, bus_id, sizeof(bus_id),
                                            role, sizeof(role))) {
                    continue;
                }
                const JZASTNode *bus_def = ir_lookup_bus_def(project_symbols, bus_id);
                if (!bus_def) {
                    continue;
                }

                /* Determine array count. */
                unsigned array_count = 1;
                if (sym->node->width) {
                    unsigned tmp = 0;
                    if (eval_simple_positive_decl_int(sym->node->width, &tmp) == 1 && tmp > 0) {
                        array_count = tmp;
                    } else {
                        /* Try resolving CONST/CONFIG expression. */
                        long long val = 0;
                        if (sem_eval_const_expr_in_module(sym->node->width,
                                scope, project_symbols, &val) == 0 && val > 0) {
                            array_count = (unsigned)val;
                        }
                    }
                }

                const char *port_name = sym->name ? sym->name : "";

                /* Iterate over array elements and BUS signals. */
                for (unsigned elem = 0; elem < array_count; ++elem) {
                    for (size_t bi = 0; bi < bus_def->child_count; ++bi) {
                        JZASTNode *sig_decl = bus_def->children[bi];
                        if (!sig_decl || sig_decl->type != JZ_AST_BUS_DECL ||
                            !sig_decl->name) {
                            continue;
                        }
                        if (idx >= count) {
                            break;
                        }

                        IR_Signal *sig = &signals[idx];
                        sig->id = next_synthetic_id++;
                        sig->owner_module_id = owner_module_id;
                        sig->source_line = sig_decl->loc.line;
                        sig->kind = SIG_PORT;

                        /* Build signal name: portname_SIGNAL or portname[i]_SIGNAL. */
                        char name_buf[256];
                        if (array_count > 1) {
                            snprintf(name_buf, sizeof(name_buf), "%s%u_%s",
                                     port_name, elem, sig_decl->name);
                        } else {
                            snprintf(name_buf, sizeof(name_buf), "%s_%s",
                                     port_name, sig_decl->name);
                        }
                        sig->name = ir_strdup_arena(arena, name_buf);

                        /* Compute direction based on signal dir and port role. */
                        const char *signal_dir = sig_decl->block_kind ? sig_decl->block_kind : "INOUT";
                        sig->u.port.direction = ir_compute_bus_signal_direction(signal_dir, role);

                        /* can_be_z: true for INOUT signals or if port has can_be_z. */
                        sig->can_be_z = (sig->u.port.direction == PORT_INOUT) ||
                                        (sym->can_be_z != 0);

                        /* Evaluate signal width. */
                        sig->width = 1;
                        if (sig_decl->width) {
                            unsigned w = 0;
                            if (eval_simple_positive_decl_int(sig_decl->width, &w) == 1 && w > 0) {
                                sig->width = (int)w;
                            } else {
                                long long cval = 0;
                                if (sem_eval_const_expr_in_module(sig_decl->width,
                                        scope, project_symbols, &cval) == 0 && cval > 0) {
                                    sig->width = (int)cval;
                                }
                            }
                        }

                        /* Record in bus signal mapping. */
                        if (bus_map && bus_map_idx < bus_map_count) {
                            IR_BusSignalMapping *m = &bus_map[bus_map_idx++];
                            m->bus_port_name = ir_strdup_arena(arena, port_name);
                            m->signal_name = ir_strdup_arena(arena, sig_decl->name);
                            m->array_index = (array_count > 1) ? (int)elem : -1;
                            m->ir_signal_id = sig->id;
                            m->width = sig->width;
                        }

                        idx++;
                    }
                }
                continue;
            }

            /* Regular port (non-BUS). */
            if (idx >= count) {
                break;
            }
            IR_Signal *sig = &signals[idx++];
            sig->id = sym->id;
            sig->name = sym->name ? ir_strdup_arena(arena, sym->name) : NULL;
            sig->owner_module_id = owner_module_id;
            sig->source_line = sym->node ? sym->node->loc.line : 0;
            sig->can_be_z = (sym->can_be_z != 0);
            sig->kind = SIG_PORT;

            if (sym->node && sym->node->block_kind) {
                const char *dir = sym->node->block_kind;
                if (strcmp(dir, "IN") == 0) {
                    sig->u.port.direction = PORT_IN;
                } else if (strcmp(dir, "OUT") == 0) {
                    sig->u.port.direction = PORT_OUT;
                } else if (strcmp(dir, "INOUT") == 0) {
                    sig->u.port.direction = PORT_INOUT;
                }
            }

            unsigned width = 0;
            if (sym->node && sym->node->width) {
                if (sem_eval_width_expr(sym->node->width, scope, project_symbols, &width) == 0) {
                    sig->width = (int)width;
                }
            }
            continue;
        }

        if (sym->kind != JZ_SYM_WIRE &&
            sym->kind != JZ_SYM_REGISTER &&
            sym->kind != JZ_SYM_LATCH) {
            continue;
        }
        if (idx >= count) {
            break;
        }

        IR_Signal *sig = &signals[idx++];
        /* Use the semantic symbol ID so that expressions can reference
         * signals by this stable identifier.
         */
        sig->id = sym->id;
        sig->name = sym->name ? ir_strdup_arena(arena, sym->name) : NULL;
        sig->owner_module_id = owner_module_id;
        sig->source_line = sym->node ? sym->node->loc.line : 0;
        sig->can_be_z = (sym->can_be_z != 0);

        /* Map symbol kind to IR_SignalKind and port/register/latch metadata. */
        if (sym->kind == JZ_SYM_WIRE) {
            sig->kind = SIG_NET;
        } else if (sym->kind == JZ_SYM_REGISTER) {
            sig->kind = SIG_REGISTER;
            sig->u.reg.home_clock_domain_id = -1; /* clock domains not yet modeled */
            /* Initialize reset value from any literal or GND/VCC initializer on the
             * register declaration (e.g., counter [25] = 25'h0 or data [8] <= GND).
             */
            memset(sig->u.reg.reset_value.words, 0, sizeof(sig->u.reg.reset_value.words));
            sig->u.reg.reset_value.width = 0;
            sig->u.reg.reset_value_gnd_vcc = NULL; /* for GND/VCC polymorphic reset */
            if (sym->node && sym->node->child_count > 0) {
                JZASTNode *init = sym->node->children[0];
                if (init && init->type == JZ_AST_EXPR_LITERAL && init->text) {
                    IR_Literal lit;
                    if (ir_decode_sized_literal(init->text,
                                                scope,
                                                project_symbols,
                                                &lit) == 0) {
                        sig->u.reg.reset_value = lit;
                    }
                } else if (init && init->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER && init->name) {
                    /* Global constant reference (e.g., PROTOCOL.SYNC_WORD) */
                    IR_Literal glit;
                    if (ir_eval_global_const_qualified(init->name,
                                                       scope,
                                                       project_symbols,
                                                       &glit) == 0) {
                        sig->u.reg.reset_value = glit;
                    }
                } else if (init && init->type == JZ_AST_EXPR_SPECIAL_DRIVER) {
                    /* GND/VCC: store the driver name for backend to expand */
                    const char *drv = (init->block_kind && strcmp(init->block_kind, "VCC") == 0)
                                    ? "VCC" : "GND";
                    sig->u.reg.reset_value_gnd_vcc = ir_strdup_arena(arena, drv);
                }
            }
        } else if (sym->kind == JZ_SYM_LATCH) {
            sig->kind = SIG_LATCH;
            /* Initialize reset value from any literal or GND/VCC initializer. */
            memset(sig->u.reg.reset_value.words, 0, sizeof(sig->u.reg.reset_value.words));
            sig->u.reg.reset_value.width = 0;
            sig->u.reg.reset_value_gnd_vcc = NULL;
            if (sym->node && sym->node->child_count > 0) {
                JZASTNode *init = sym->node->children[0];
                if (init && init->type == JZ_AST_EXPR_LITERAL && init->text) {
                    IR_Literal lit;
                    if (ir_decode_sized_literal(init->text,
                                                scope,
                                                project_symbols,
                                                &lit) == 0) {
                        sig->u.reg.reset_value = lit;
                    }
                } else if (init && init->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER && init->name) {
                    IR_Literal glit;
                    if (ir_eval_global_const_qualified(init->name,
                                                       scope,
                                                       project_symbols,
                                                       &glit) == 0) {
                        sig->u.reg.reset_value = glit;
                    }
                } else if (init && init->type == JZ_AST_EXPR_SPECIAL_DRIVER) {
                    const char *drv = (init->block_kind && strcmp(init->block_kind, "VCC") == 0)
                                    ? "VCC" : "GND";
                    sig->u.reg.reset_value_gnd_vcc = ir_strdup_arena(arena, drv);
                }
            }
        }

        /* Compute final signal width using the declaration's width expression
         * when available. Width expressions are already validated by semantic
         * analysis; here we simply evaluate them to a concrete integer.
         */
        unsigned width = 0;
        if (sym->node && sym->node->width) {
            if (sem_eval_width_expr(sym->node->width,
                                    scope,
                                    project_symbols,
                                    &width) == 0) {
                sig->width = (int)width;
            }
        }
    }

    *out_signals = signals;
    *out_count = idx;

    if (out_bus_map) *out_bus_map = bus_map;
    if (out_bus_map_count) *out_bus_map_count = bus_map_idx;

    return 0;
}

/**
 * @brief Find an IR_Signal by semantic signal ID.
 *
 * @param mod       IR module.
 * @param signal_id Semantic signal ID.
 * @return Pointer to IR_Signal, or NULL if not found.
 */
IR_Signal *ir_find_signal_by_id(IR_Module *mod, int signal_id)
{
    if (!mod || !mod->signals) return NULL;
    for (int i = 0; i < mod->num_signals; ++i) {
        if (mod->signals[i].id == signal_id) {
            return &mod->signals[i];
        }
    }
    return NULL;
}

/**
 * @brief Find the semantic symbol corresponding to an IR signal ID.
 *
 * Used when re-evaluating widths under specialization overrides so that
 * the original width expression can be recovered.
 *
 * @param scope     Module scope.
 * @param signal_id Semantic signal ID.
 * @return Pointer to JZSymbol, or NULL if not found.
 */
const JZSymbol *ir_find_symbol_by_signal_id(const JZModuleScope *scope, int signal_id)
{
    if (!scope) return NULL;
    size_t sym_count = scope->symbols.len / sizeof(JZSymbol);
    const JZSymbol *syms = (const JZSymbol *)scope->symbols.data;
    for (size_t i = 0; i < sym_count; ++i) {
        const JZSymbol *sym = &syms[i];
        if (sym->id == signal_id &&
            (sym->kind == JZ_SYM_PORT ||
             sym->kind == JZ_SYM_WIRE ||
             sym->kind == JZ_SYM_REGISTER ||
             sym->kind == JZ_SYM_LATCH)) {
            return sym;
        }
    }
    return NULL;
}

/**
 * @brief Evaluate a width expression with specialization overrides applied.
 *
 * The resulting width must be strictly positive.
 *
 * @param expr             Width expression text.
 * @param scope            Module scope.
 * @param project_symbols  Project-level symbols.
 * @param overrides        Override set.
 * @param num_overrides    Number of overrides.
 * @param out_width        Output width.
 * @return 0 on success, non-zero on failure.
 */
int ir_eval_width_with_overrides(const char *expr,
                                 const JZModuleScope *scope,
                                 const JZBuffer *project_symbols,
                                 const IR_ModuleSpecOverride *overrides,
                                 int num_overrides,
                                 unsigned *out_width)
{
    if (!expr || !scope || !out_width) {
        return -1;
    }
    long long v = 0;
    if (ir_eval_const_with_overrides(expr,
                                     scope,
                                     project_symbols,
                                     overrides,
                                     num_overrides,
                                     &v) != 0) {
        return -1;
    }
    if (v <= 0) {
        return -1;
    }
    *out_width = (unsigned)v;
    return 0;
}

/**
 * @brief Evaluate a constant expression with specialization overrides applied.
 *
 * This mirrors sem_eval_const_expr_in_module(), but replaces overridden
 * CONST definitions with concrete decimal literals before evaluation.
 *
 * @param expr             Expression to evaluate.
 * @param scope            Module scope.
 * @param project_symbols  Project-level symbols.
 * @param overrides        Override set.
 * @param num_overrides    Number of overrides.
 * @param out_value        Output value.
 * @return 0 on success, non-zero on failure.
 */
int ir_eval_const_with_overrides(const char *expr,
                                 const JZModuleScope *scope,
                                 const JZBuffer *project_symbols,
                                 const IR_ModuleSpecOverride *overrides,
                                 int num_overrides,
                                 long long *out_value)
{
    if (!expr || !scope || !scope->node || !out_value) {
        return -1;
    }

    /* Count named CONST (module) + CONFIG (project) entries. */
    size_t named_count = 0;
    {
        size_t sym_count = scope->symbols.len / sizeof(JZSymbol);
        const JZSymbol *syms = (const JZSymbol *)scope->symbols.data;
        for (size_t i = 0; i < sym_count; ++i) {
            if (syms[i].kind == JZ_SYM_CONST && syms[i].node && syms[i].node->text) {
                ++named_count;
            }
        }
        if (project_symbols && project_symbols->data) {
            const JZSymbol *psyms = (const JZSymbol *)project_symbols->data;
            size_t pcount = project_symbols->len / sizeof(JZSymbol);
            for (size_t i = 0; i < pcount; ++i) {
                if (psyms[i].kind == JZ_SYM_CONFIG && psyms[i].node && psyms[i].node->text) {
                    ++named_count;
                }
            }
        }
    }

    size_t total = named_count + 1; /* +1 for anonymous expression */
    if (total == 1) {
        /* No named CONST/CONFIG in scope: fall back to plain expression eval. */
        return jz_const_eval_expr(expr, NULL, out_value);
    }

    JZConstDef *defs = (JZConstDef *)calloc(total, sizeof(JZConstDef));
    long long  *vals = (long long *)calloc(total, sizeof(long long));
    if (!defs || !vals) {
        free(defs);
        free(vals);
        return -1;
    }

    /* We may need to synthesize decimal literal strings for overridden CONSTs. */
    char **owned_exprs = (char **)calloc(named_count, sizeof(char *));
    size_t owned_count = 0;

    size_t idx = 0;
    /* Module-level CONST definitions (with overrides applied). */
    size_t sym_count = scope->symbols.len / sizeof(JZSymbol);
    const JZSymbol *syms = (const JZSymbol *)scope->symbols.data;
    for (size_t i = 0; i < sym_count; ++i) {
        const JZSymbol *s = &syms[i];
        if (s->kind != JZ_SYM_CONST || !s->node || !s->node->name || !s->node->text) {
            continue;
        }

        const char *expr_text = s->node->text;
        /* Check for override of this CONST name. */
        for (int ov_i = 0; ov_i < num_overrides; ++ov_i) {
            const IR_ModuleSpecOverride *ov = &overrides[ov_i];
            if (ov->name && strcmp(ov->name, s->node->name) == 0) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%lld", (long long)ov->value);
                size_t len = strlen(buf);
                char *dup = (char *)malloc(len + 1);
                if (dup) {
                    memcpy(dup, buf, len + 1);
                    expr_text = dup;
                    if (owned_exprs && owned_count < named_count) {
                        owned_exprs[owned_count++] = dup;
                    }
                }
                break;
            }
        }

        defs[idx].name = s->node->name;
        defs[idx].expr = expr_text;
        ++idx;
    }

    /* Project-level CONFIG entries (no overrides). */
    if (project_symbols && project_symbols->data) {
        const JZSymbol *psyms = (const JZSymbol *)project_symbols->data;
        size_t pcount = project_symbols->len / sizeof(JZSymbol);
        for (size_t i = 0; i < pcount; ++i) {
            const JZSymbol *s = &psyms[i];
            if (s->kind != JZ_SYM_CONFIG || !s->node ||
                !s->node->name || !s->node->text) {
                continue;
            }
            defs[idx].name = s->node->name;
            defs[idx].expr = s->node->text;
            ++idx;
        }
    }

    /* Anonymous expression to evaluate. */
    defs[idx].name = NULL;
    defs[idx].expr = expr;

    JZConstEvalOptions opts;
    memset(&opts, 0, sizeof(opts));
    if (scope->node->loc.filename) {
        opts.filename = scope->node->loc.filename;
    }

    int rc = jz_const_eval_all(defs, total, &opts, vals);
    if (rc != 0) {
        for (size_t i = 0; i < owned_count; ++i) {
            free(owned_exprs[i]);
        }
        free(owned_exprs);
        free(defs);
        free(vals);
        return -1;
    }

    *out_value = vals[total - 1];

    for (size_t i = 0; i < owned_count; ++i) {
        free(owned_exprs[i]);
    }
    free(owned_exprs);
    free(defs);
    free(vals);
    return 0;
}
