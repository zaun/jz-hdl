/**
 * @file sim_state.c
 * @brief Simulation state creation, initialization, lookup, and NBA apply.
 */

#include "sim_state.h"
#include "sim_perf.h"
#include <stdlib.h>
#include <string.h>

/* ---- PRNG ---- */

uint32_t sim_rng_next(uint32_t *state) {
    uint32_t x = *state;
    if (x == 0) x = 1; /* avoid zero state */
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

/* ---- Context creation ---- */

SimContext *sim_ctx_create(const IR_Module *module, const IR_Design *design,
                           uint32_t rng_seed) {
    SimContext *ctx = calloc(1, sizeof(SimContext));
    if (!ctx) return NULL;

    ctx->module = module;
    ctx->design = design;
    ctx->rng_state = rng_seed;
    ctx->num_signals = module->num_signals;
    ctx->signals = calloc((size_t)module->num_signals, sizeof(SimSignalEntry));

    for (int i = 0; i < module->num_signals; i++) {
        const IR_Signal *sig = &module->signals[i];
        SimSignalEntry *entry = &ctx->signals[i];
        entry->signal_id = sig->id;
        entry->has_pending = 0;

        switch (sig->kind) {
        case SIG_REGISTER:
        case SIG_LATCH:
            /* Initialize registers and latches with random bits from PRNG */
            {
                uint64_t rv = sim_rng_next(&ctx->rng_state);
                if (sig->width > 32)
                    rv |= (uint64_t)sim_rng_next(&ctx->rng_state) << 32;
                entry->current = sim_val_from_uint(rv, sig->width);
            }
            break;
        case SIG_PORT:
        case SIG_NET:
        default:
            entry->current = sim_val_zero(sig->width);
            break;
        }
        entry->next = sim_val_zero(sig->width);
    }

    /* Initialize memories */
    ctx->num_memories = module->num_memories;
    if (module->num_memories > 0) {
        ctx->memories = calloc((size_t)module->num_memories, sizeof(SimMemEntry));
        for (int i = 0; i < module->num_memories; i++) {
            const IR_Memory *mem = &module->memories[i];
            SimMemEntry *me = &ctx->memories[i];
            me->mem_id = mem->id;
            me->word_width = mem->word_width;
            me->depth = mem->depth;
            me->cells = calloc((size_t)mem->depth, sizeof(SimValue));
            for (int j = 0; j < mem->depth; j++) {
                if (mem->init_is_file) {
                    /* File-initialized memories (ROM) keep their data;
                     * actual file loading is handled elsewhere. Zero here
                     * as a safe default if file data isn't loaded yet. */
                    me->cells[j] = sim_val_zero(mem->word_width);
                } else {
                    /* Initialize MEM with random bits from PRNG per spec
                     * Section 2.6: all storage randomizes at start. */
                    uint64_t rv = sim_rng_next(&ctx->rng_state);
                    if (mem->word_width > 32)
                        rv |= (uint64_t)sim_rng_next(&ctx->rng_state) << 32;
                    me->cells[j] = sim_val_from_uint(rv, mem->word_width);
                }
            }
        }
    }

    /* Recursively create child instance contexts */
    ctx->num_children = module->num_instances;
    ctx->children = NULL;
    if (module->num_instances > 0 && design) {
        ctx->children = calloc((size_t)module->num_instances,
                               sizeof(SimChildInstance));
        for (int i = 0; i < module->num_instances; i++) {
            const IR_Instance *inst = &module->instances[i];
            SimChildInstance *ci = &ctx->children[i];
            ci->inst = inst;
            ci->child_module = NULL;
            ci->ctx = NULL;

            /* Find child module in design */
            for (int m = 0; m < design->num_modules; m++) {
                if (design->modules[m].id == inst->child_module_id) {
                    ci->child_module = &design->modules[m];
                    break;
                }
            }
            if (ci->child_module) {
                ci->ctx = sim_ctx_create(ci->child_module, design, rng_seed + (uint32_t)i + 1);
            }
        }
    }

    return ctx;
}

void sim_ctx_destroy(SimContext *ctx) {
    if (!ctx) return;
    /* Recursively destroy children */
    if (ctx->children) {
        for (int i = 0; i < ctx->num_children; i++) {
            sim_ctx_destroy(ctx->children[i].ctx);
        }
        free(ctx->children);
    }
    free(ctx->signals);
    if (ctx->memories) {
        for (int i = 0; i < ctx->num_memories; i++)
            free(ctx->memories[i].cells);
        free(ctx->memories);
    }
    free(ctx);
}

/* ---- Lookup ---- */

SimSignalEntry *sim_ctx_lookup(SimContext *ctx, int signal_id) {
    for (int i = 0; i < ctx->num_signals; i++) {
        if (ctx->signals[i].signal_id == signal_id)
            return &ctx->signals[i];
    }
    return NULL;
}

SimMemEntry *sim_ctx_lookup_mem(SimContext *ctx, const char *name) {
    for (int i = 0; i < ctx->num_memories; i++) {
        if (strcmp(ctx->module->memories[i].name, name) == 0)
            return &ctx->memories[i];
    }
    return NULL;
}

/* ---- NBA apply ---- */

void sim_ctx_apply_nba(SimContext *ctx) {
    PERF_TIMER_START(PERF_APPLY_NBA);
#ifdef TRACK_PERF
    int pending_count = 0;
#endif
    for (int i = 0; i < ctx->num_signals; i++) {
        SimSignalEntry *e = &ctx->signals[i];
        if (e->has_pending) {
            e->current = e->next;
            e->has_pending = 0;
#ifdef TRACK_PERF
            pending_count++;
#endif
        }
    }
    PERF_STATE_NBA_PENDING(pending_count);
    /* Recursively apply NBA to children */
    for (int c = 0; c < ctx->num_children; c++) {
        if (ctx->children[c].ctx)
            sim_ctx_apply_nba(ctx->children[c].ctx);
    }
    PERF_TIMER_STOP(PERF_APPLY_NBA);
}
