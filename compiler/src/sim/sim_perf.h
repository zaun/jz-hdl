/**
 * @file sim_perf.h
 * @brief Performance tracking for the simulation engine.
 *
 * Enabled by compiling with -DTRACK_PERF.
 * Tracks wall-clock time and call counts for key simulation phases,
 * plus current/future state statistics (NBA pending, signal changes).
 */

#ifndef JZ_SIM_PERF_H
#define JZ_SIM_PERF_H

#ifdef TRACK_PERF

#include <stdint.h>
#include <time.h>

/* ---- Timer / counter IDs ---- */

typedef enum {
    PERF_PROPAGATE_INPUTS,
    PERF_PROPAGATE_OUTPUTS,
    PERF_SETTLE_COMBINATIONAL,
    PERF_SETTLE_HIERARCHY_ONCE,
    PERF_RESOLVE_INOUT_Z,
    PERF_FIRE_DOMAINS,
    PERF_EXEC_SYNC_DOMAIN,
    PERF_APPLY_NBA,
    PERF_EXEC_STMT,
    PERF_EVAL_EXPR,
    PERF_WAVEFORM_DUMP,
    PERF_FULL_SETTLE,
    PERF_CLOCK_TOGGLE,
    PERF__COUNT   /* sentinel — must be last */
} PerfTimerId;

/* ---- Per-timer accumulator ---- */

typedef struct {
    uint64_t calls;
    uint64_t total_ns;    /* cumulative wall-clock nanoseconds */
    uint64_t max_ns;      /* worst single invocation */
} PerfTimer;

/* ---- State tracking counters ---- */

typedef struct {
    uint64_t nba_applies;          /* number of sim_ctx_apply_nba calls */
    uint64_t nba_pending_total;    /* sum of has_pending entries across all applies */
    uint64_t nba_pending_max;      /* max pending in a single apply */
    uint64_t settle_iterations;    /* total delta-cycle iterations */
    uint64_t settle_max_iters;     /* worst-case iterations in one settle */
    uint64_t signals_changed;      /* total signal changes detected during settling */
    uint64_t tick_count;           /* total simulation ticks processed */
} PerfStateCounters;

/* ---- Global perf context ---- */

typedef struct {
    PerfTimer timers[PERF__COUNT];
    PerfStateCounters state;
} PerfContext;

/* Single global instance (defined in sim_perf.c) */
extern PerfContext g_perf;

/* ---- Timing helpers ---- */

static inline uint64_t perf_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* ---- Macros for instrumentation ---- */

#define PERF_TIMER_START(id) \
    uint64_t _perf_start_##id = perf_now_ns()

#define PERF_TIMER_STOP(id) \
    do { \
        uint64_t _perf_elapsed = perf_now_ns() - _perf_start_##id; \
        g_perf.timers[id].calls++; \
        g_perf.timers[id].total_ns += _perf_elapsed; \
        if (_perf_elapsed > g_perf.timers[id].max_ns) \
            g_perf.timers[id].max_ns = _perf_elapsed; \
    } while (0)

#define PERF_COUNT(id) \
    do { g_perf.timers[id].calls++; } while (0)

#define PERF_STATE_NBA_PENDING(n) \
    do { \
        g_perf.state.nba_applies++; \
        g_perf.state.nba_pending_total += (uint64_t)(n); \
        if ((uint64_t)(n) > g_perf.state.nba_pending_max) \
            g_perf.state.nba_pending_max = (uint64_t)(n); \
    } while (0)

#define PERF_STATE_SETTLE_ITER(iters, changed) \
    do { \
        g_perf.state.settle_iterations += (uint64_t)(iters); \
        if ((uint64_t)(iters) > g_perf.state.settle_max_iters) \
            g_perf.state.settle_max_iters = (uint64_t)(iters); \
        g_perf.state.signals_changed += (uint64_t)(changed); \
    } while (0)

#define PERF_STATE_TICK() \
    do { g_perf.state.tick_count++; } while (0)

/* ---- API ---- */

void perf_reset(void);
void perf_print_summary(void);

#else /* !TRACK_PERF */

/* No-op stubs when TRACK_PERF is not defined */
#define PERF_TIMER_START(id)              ((void)0)
#define PERF_TIMER_STOP(id)               ((void)0)
#define PERF_COUNT(id)                    ((void)0)
#define PERF_STATE_NBA_PENDING(n)         ((void)0)
#define PERF_STATE_SETTLE_ITER(iters, changed) ((void)0)
#define PERF_STATE_TICK()                 ((void)0)

static inline void perf_reset(void) {}
static inline void perf_print_summary(void) {}

#endif /* TRACK_PERF */

#endif /* JZ_SIM_PERF_H */
