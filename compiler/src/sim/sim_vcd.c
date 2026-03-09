/**
 * @file sim_vcd.c
 * @brief VCD (Value Change Dump) waveform writer for simulation.
 *
 * Implements the IEEE 1364 VCD format for waveform output.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "sim_vcd.h"

#define VCD_MAX_SIGNALS 4096

typedef struct VCDSignal {
    char *scope;
    char *name;
    int   width;
    char  ident[8]; /* VCD identifier character(s) */
} VCDSignal;

struct VCDWriter {
    FILE       *fp;
    uint64_t    timescale_ps;
    uint64_t    current_time;
    int         time_written;  /* has the current time marker been emitted? */
    VCDSignal   signals[VCD_MAX_SIGNALS];
    int         num_signals;
    int         defs_ended;
};

/**
 * @brief Generate a VCD identifier from a signal index.
 *
 * VCD identifiers are printable ASCII characters starting at '!'.
 * For indices > 93, we use multi-character identifiers.
 */
static void make_vcd_ident(int idx, char *out, size_t out_sz)
{
    if (idx < 94 && out_sz >= 2) {
        out[0] = (char)('!' + idx);
        out[1] = '\0';
    } else if (out_sz >= 3) {
        out[0] = (char)('!' + (idx / 94));
        out[1] = (char)('!' + (idx % 94));
        out[2] = '\0';
    } else {
        out[0] = '!';
        out[1] = '\0';
    }
}

VCDWriter *vcd_open(const char *filename, uint64_t timescale_ps)
{
    FILE *fp = fopen(filename, "w");
    if (!fp) return NULL;

    VCDWriter *w = (VCDWriter *)calloc(1, sizeof(VCDWriter));
    if (!w) {
        fclose(fp);
        return NULL;
    }

    w->fp = fp;
    w->timescale_ps = timescale_ps;
    w->current_time = 0;
    w->time_written = 0;
    w->num_signals = 0;
    w->defs_ended = 0;

    /* Write VCD header */
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char date_buf[64];
    strftime(date_buf, sizeof(date_buf), "%Y-%m-%d %H:%M:%S", tm_info);

    fprintf(fp, "$date\n  %s\n$end\n", date_buf);
    fprintf(fp, "$version\n  JZ-HDL Simulator 1.0\n$end\n");

    /* Timescale */
    if (timescale_ps >= 1000000000ULL) {
        fprintf(fp, "$timescale %llums $end\n", (unsigned long long)(timescale_ps / 1000000000ULL));
    } else if (timescale_ps >= 1000000ULL) {
        fprintf(fp, "$timescale %lluus $end\n", (unsigned long long)(timescale_ps / 1000000ULL));
    } else if (timescale_ps >= 1000ULL) {
        fprintf(fp, "$timescale %lluns $end\n", (unsigned long long)(timescale_ps / 1000ULL));
    } else {
        fprintf(fp, "$timescale %llups $end\n", (unsigned long long)timescale_ps);
    }

    return w;
}

int vcd_add_signal(VCDWriter *w, const char *scope, const char *name, int width)
{
    if (!w || w->defs_ended || w->num_signals >= VCD_MAX_SIGNALS) return -1;

    int idx = w->num_signals;
    VCDSignal *sig = &w->signals[idx];
    sig->scope = scope ? strdup(scope) : strdup("top");
    sig->name = strdup(name);
    sig->width = width;
    make_vcd_ident(idx, sig->ident, sizeof(sig->ident));

    w->num_signals++;
    return idx;
}

void vcd_end_definitions(VCDWriter *w)
{
    if (!w || w->defs_ended) return;

    /* Group signals by scope */
    const char *current_scope = NULL;
    for (int i = 0; i < w->num_signals; i++) {
        VCDSignal *sig = &w->signals[i];
        if (!current_scope || strcmp(current_scope, sig->scope) != 0) {
            if (current_scope) {
                fprintf(w->fp, "$upscope $end\n");
            }
            fprintf(w->fp, "$scope module %s $end\n", sig->scope);
            current_scope = sig->scope;
        }
        fprintf(w->fp, "$var wire %d %s %s $end\n",
                sig->width, sig->ident, sig->name);
    }
    if (current_scope) {
        fprintf(w->fp, "$upscope $end\n");
    }

    fprintf(w->fp, "$enddefinitions $end\n");
    w->defs_ended = 1;
}

void vcd_set_time(VCDWriter *w, uint64_t time_ps)
{
    if (!w || !w->defs_ended) return;

    /* Convert from ps to timescale units */
    uint64_t time_units = time_ps / w->timescale_ps;

    if (!w->time_written || time_units != w->current_time) {
        fprintf(w->fp, "#%llu\n", (unsigned long long)time_units);
        w->current_time = time_units;
        w->time_written = 1;
    }
}

void vcd_dump_value(VCDWriter *w, int sig_id, uint64_t value, int width)
{
    if (!w || !w->defs_ended || sig_id < 0 || sig_id >= w->num_signals) return;

    VCDSignal *sig = &w->signals[sig_id];
    (void)width; /* use signal's declared width */

    if (sig->width == 1) {
        fprintf(w->fp, "%c%s\n", (value & 1) ? '1' : '0', sig->ident);
    } else {
        fprintf(w->fp, "b");
        for (int b = sig->width - 1; b >= 0; b--) {
            fprintf(w->fp, "%c", (value >> b) & 1 ? '1' : '0');
        }
        fprintf(w->fp, " %s\n", sig->ident);
    }
}

void vcd_close(VCDWriter *w)
{
    if (!w) return;

    if (w->fp) {
        fclose(w->fp);
    }

    for (int i = 0; i < w->num_signals; i++) {
        free(w->signals[i].scope);
        free(w->signals[i].name);
    }

    free(w);
}
