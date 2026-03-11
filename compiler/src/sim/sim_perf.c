/**
 * @file sim_perf.c
 * @brief Performance tracking implementation for the simulation engine.
 *
 * Only compiled when -DTRACK_PERF is set.
 */

#ifdef TRACK_PERF

#include "sim_perf.h"
#include <stdio.h>
#include <string.h>

/* Global perf context */
PerfContext g_perf;

static const char *timer_names[PERF__COUNT] = {
    [PERF_PROPAGATE_INPUTS]     = "propagate_inputs",
    [PERF_PROPAGATE_OUTPUTS]    = "propagate_outputs",
    [PERF_SETTLE_COMBINATIONAL] = "settle_combinational",
    [PERF_SETTLE_HIERARCHY_ONCE]= "settle_hierarchy_once",
    [PERF_RESOLVE_INOUT_Z]      = "resolve_inout_z",
    [PERF_FIRE_DOMAINS]         = "fire_domains",
    [PERF_EXEC_SYNC_DOMAIN]     = "exec_sync_domain",
    [PERF_APPLY_NBA]            = "apply_nba",
    [PERF_EXEC_STMT]            = "exec_stmt",
    [PERF_EVAL_EXPR]            = "eval_expr",
    [PERF_WAVEFORM_DUMP]        = "waveform_dump",
    [PERF_FULL_SETTLE]          = "full_settle",
    [PERF_CLOCK_TOGGLE]         = "clock_toggle",
};

void perf_reset(void) {
    memset(&g_perf, 0, sizeof(g_perf));
}

static void print_time(uint64_t ns) {
    if (ns >= 1000000000ULL) {
        fprintf(stderr, "%8.3f s ", (double)ns / 1e9);
    } else if (ns >= 1000000ULL) {
        fprintf(stderr, "%8.3f ms", (double)ns / 1e6);
    } else if (ns >= 1000ULL) {
        fprintf(stderr, "%8.3f us", (double)ns / 1e3);
    } else {
        fprintf(stderr, "%8llu ns", (unsigned long long)ns);
    }
}

void perf_print_summary(void) {
    /* Compute total tracked time */
    uint64_t total_ns = 0;
    for (int i = 0; i < PERF__COUNT; i++) {
        total_ns += g_perf.timers[i].total_ns;
    }

    fprintf(stderr, "\n");
    fprintf(stderr, "=== SIMULATION PERFORMANCE SUMMARY ===\n");
    fprintf(stderr, "\n");

    /* Timer table */
    fprintf(stderr, "%-24s %12s %14s %14s %14s\n",
            "Phase", "Calls", "Total", "Avg", "Max");
    fprintf(stderr, "%-24s %12s %14s %14s %14s\n",
            "------------------------", "------------",
            "--------------", "--------------", "--------------");

    for (int i = 0; i < PERF__COUNT; i++) {
        PerfTimer *t = &g_perf.timers[i];
        if (t->calls == 0) continue;

        uint64_t avg_ns = t->total_ns / t->calls;

        fprintf(stderr, "%-24s %12llu  ",
                timer_names[i], (unsigned long long)t->calls);
        print_time(t->total_ns);
        fprintf(stderr, "  ");
        print_time(avg_ns);
        fprintf(stderr, "  ");
        print_time(t->max_ns);

        /* Percentage of total */
        if (total_ns > 0) {
            double pct = 100.0 * (double)t->total_ns / (double)total_ns;
            fprintf(stderr, "  %5.1f%%", pct);
        }
        fprintf(stderr, "\n");
    }

    fprintf(stderr, "%-24s %12s  ", "TOTAL", "");
    print_time(total_ns);
    fprintf(stderr, "\n");

    /* State counters */
    fprintf(stderr, "\n");
    fprintf(stderr, "=== STATE TRACKING ===\n");
    fprintf(stderr, "\n");

    fprintf(stderr, "Simulation ticks:        %llu\n",
            (unsigned long long)g_perf.state.tick_count);
    fprintf(stderr, "\n");

    fprintf(stderr, "NBA applies:             %llu\n",
            (unsigned long long)g_perf.state.nba_applies);
    if (g_perf.state.nba_applies > 0) {
        fprintf(stderr, "  Pending total:         %llu\n",
                (unsigned long long)g_perf.state.nba_pending_total);
        fprintf(stderr, "  Pending avg:           %.1f\n",
                (double)g_perf.state.nba_pending_total /
                (double)g_perf.state.nba_applies);
        fprintf(stderr, "  Pending max:           %llu\n",
                (unsigned long long)g_perf.state.nba_pending_max);
    }

    fprintf(stderr, "\n");
    fprintf(stderr, "Settle iterations:       %llu\n",
            (unsigned long long)g_perf.state.settle_iterations);
    if (g_perf.state.settle_iterations > 0) {
        fprintf(stderr, "  Max iters (one call):  %llu\n",
                (unsigned long long)g_perf.state.settle_max_iters);
        fprintf(stderr, "  Signals changed:       %llu\n",
                (unsigned long long)g_perf.state.signals_changed);
    }

    fprintf(stderr, "\n");
}

#else /* !TRACK_PERF */

/* ISO C requires at least one declaration per translation unit */
typedef int sim_perf_unused_t;

#endif /* TRACK_PERF */
