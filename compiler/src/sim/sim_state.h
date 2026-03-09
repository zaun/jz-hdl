/**
 * @file sim_state.h
 * @brief Simulation state: per-signal storage, memory, and testbench context.
 */

#ifndef JZ_SIM_STATE_H
#define JZ_SIM_STATE_H

#include "sim_value.h"
#include "../../include/ir.h"
#include "../../include/diagnostic.h"

/* ---- Per-signal entry ---- */

typedef struct SimSignalEntry {
    int      signal_id;
    SimValue current;
    SimValue next;        /* NBA staging for registers */
    int      has_pending; /* non-zero if next is valid */
} SimSignalEntry;

/* ---- Per-memory entry ---- */

typedef struct SimMemEntry {
    int       mem_id;
    SimValue *cells;      /* array[depth] */
    int       word_width;
    int       depth;
} SimMemEntry;

/* ---- Sub-instance context ---- */

typedef struct SimChildInstance {
    struct SimContext   *ctx;           /* Child simulation context */
    const IR_Instance  *inst;          /* IR instance descriptor */
    const IR_Module    *child_module;  /* Child module IR */
} SimChildInstance;

/* ---- DUT simulation context ---- */

typedef struct SimContext {
    const IR_Module    *module;
    const IR_Design    *design;
    SimSignalEntry     *signals;
    int                 num_signals;
    SimMemEntry        *memories;
    int                 num_memories;
    SimChildInstance   *children;
    int                 num_children;
    uint32_t            rng_state;
    int                 runtime_error; /* z reached non-tristate expression (SE-008) */
} SimContext;

/* ---- Testbench wire ---- */

typedef struct SimTbWire {
    const char *name;
    SimValue    value;
    int         width;
    int         is_clock;
    int         owns_name; /* 1 if name was strdup'd and needs free */
} SimTbWire;

/* ---- Port binding: tb wire <-> DUT port ---- */

typedef struct SimPortBinding {
    int port_signal_id;   /* module port IR signal ID */
    int tb_wire_index;    /* index into tb_wires array */
} SimPortBinding;

/* ---- Per-test state ---- */

typedef struct SimTestState {
    SimTbWire       *tb_wires;
    int              num_tb_wires;
    SimPortBinding  *bindings;
    int              num_bindings;
    SimContext       *dut;
    int              test_passed;
    int              num_expects;
    int              num_passed;
    int              num_failed;
    int              verbose;
    int              runtime_error; /* non-zero if z observed; abort test */
    uint64_t         cycle_count;
    JZDiagnosticList *diagnostics;
    const char       *filename;
    /* Failure message buffer for reporting */
    char            **failure_msgs;
    int              num_failure_msgs;
    int              cap_failure_msgs;
} SimTestState;

/* ---- API ---- */

SimContext *sim_ctx_create(const IR_Module *module, const IR_Design *design,
                           uint32_t rng_seed);
void       sim_ctx_destroy(SimContext *ctx);

SimSignalEntry *sim_ctx_lookup(SimContext *ctx, int signal_id);
SimMemEntry    *sim_ctx_lookup_mem(SimContext *ctx, const char *name);
void            sim_ctx_apply_nba(SimContext *ctx);

/* Simple xorshift32 PRNG */
uint32_t sim_rng_next(uint32_t *state);

#endif /* JZ_SIM_STATE_H */
