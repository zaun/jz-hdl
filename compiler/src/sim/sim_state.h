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

/* ---- Precomputed port mapping for fast propagation ---- */

typedef struct SimPortMapping {
    SimSignalEntry *parent_entry;  /* Direct pointer into parent signals[] */
    SimSignalEntry *child_entry;   /* Direct pointer into child signals[] */
    int             parent_msb;    /* Slice MSB, or -1 for full-width */
    int             parent_lsb;    /* Slice LSB, or -1 for full-width */
    int             child_width;   /* Child port width (for width fixup) */
    int             is_inout;      /* 1 if child port is INOUT */
} SimPortMapping;

/* ---- Async chunk for dependency-driven re-execution ---- */

typedef struct SimAsyncChunk {
    const IR_Stmt *stmt;           /* Statement to execute */
    int           *read_indices;   /* Indices into ctx->signals[] that this chunk reads */
    int            num_reads;
    int           *write_indices;  /* Indices into ctx->signals[] that this chunk writes */
    int            num_writes;
} SimAsyncChunk;

/* ---- Sub-instance context ---- */

typedef struct SimChildInstance {
    struct SimContext   *ctx;           /* Child simulation context */
    const IR_Instance  *inst;          /* IR instance descriptor */
    const IR_Module    *child_module;  /* Child module IR */
    SimPortMapping     *input_maps;    /* Precomputed parent->child mappings */
    int                 num_input_maps;
    SimPortMapping     *output_maps;   /* Precomputed child->parent mappings */
    int                 num_output_maps;
} SimChildInstance;

/* ---- DUT simulation context ---- */

typedef struct SimContext {
    const IR_Module    *module;
    const IR_Design    *design;
    SimSignalEntry     *signals;
    int                 num_signals;
    int                *sig_id_map;    /* signal_id -> signals[] index, size max_sig_id+1 */
    int                 max_sig_id;    /* largest signal_id in this context */
    SimMemEntry        *memories;
    int                 num_memories;
    SimChildInstance   *children;
    int                 num_children;
    uint32_t            rng_state;
    int                 runtime_error; /* z reached non-tristate expression (SE-008) */
    int                 settle_dirty;  /* non-zero if any signal changed during settle */
    SimAsyncChunk      *async_chunks;  /* Dependency-analyzed async block chunks */
    int                 num_async_chunks;
    uint8_t            *sig_dirty;     /* Per-signal dirty flags (indexed by signals[] position) */
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

/* ---- Per-test/simulation state (shared by both systems) ---- */

typedef struct SimTestState {
    /* Shared: wires and port bindings */
    SimTbWire       *tb_wires;
    int              num_tb_wires;
    SimPortBinding  *bindings;
    int              num_bindings;
    SimContext       *dut;

    /* Shared: execution control */
    int              verbose;
    int              runtime_error; /* non-zero if z observed; abort test */
    JZDiagnosticList *diagnostics;
    const char       *filename;

    /* Shared: failure reporting */
    int              num_failed;
    char            **failure_msgs;
    int              num_failure_msgs;
    int              cap_failure_msgs;

    /* Shared: time tracking (for @print %tick / %ms) */
    uint64_t         current_time_ps;  /* simulation time in picoseconds */
    uint64_t         tick_ps;          /* tick resolution in picoseconds */

    /* Testbench only: assertion tracking */
    int              test_passed;
    int              num_expects;
    int              num_passed;
    uint64_t         cycle_count;
} SimTestState;

/* ---- API ---- */

SimContext *sim_ctx_create(const IR_Module *module, const IR_Design *design,
                           uint32_t rng_seed);
void       sim_ctx_destroy(SimContext *ctx);

SimSignalEntry *sim_ctx_lookup(SimContext *ctx, int signal_id);
SimMemEntry    *sim_ctx_lookup_mem(SimContext *ctx, const char *name);
void            sim_ctx_apply_nba(SimContext *ctx);
void            sim_ctx_clear_dirty(SimContext *ctx);

/* Simple xorshift32 PRNG */
uint32_t sim_rng_next(uint32_t *state);

#endif /* JZ_SIM_STATE_H */
