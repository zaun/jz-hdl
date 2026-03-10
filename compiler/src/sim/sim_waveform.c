/**
 * @file sim_waveform.c
 * @brief Generic waveform writer wrapping VCD and FST backends.
 */

#include <stdlib.h>
#include "sim_waveform.h"
#include "sim_vcd.h"
#include "sim_fst.h"

struct SimWaveWriter {
    SimWaveFormat format;
    union {
        VCDWriter *vcd;
        FSTWriter *fst;
    } backend;
};

SimWaveWriter *sim_wave_open(const char *filename, uint64_t timescale_ps,
                              SimWaveFormat format)
{
    SimWaveWriter *w = (SimWaveWriter *)calloc(1, sizeof(SimWaveWriter));
    if (!w) return NULL;

    w->format = format;
    if (format == SIM_WAVE_FST) {
        w->backend.fst = fst_open(filename, timescale_ps);
        if (!w->backend.fst) { free(w); return NULL; }
    } else {
        w->backend.vcd = vcd_open(filename, timescale_ps);
        if (!w->backend.vcd) { free(w); return NULL; }
    }
    return w;
}

int sim_wave_add_signal(SimWaveWriter *w, const char *scope, const char *name,
                         int width)
{
    if (!w) return -1;
    if (w->format == SIM_WAVE_FST)
        return fst_add_signal(w->backend.fst, scope, name, width);
    return vcd_add_signal(w->backend.vcd, scope, name, width);
}

void sim_wave_end_definitions(SimWaveWriter *w)
{
    if (!w) return;
    if (w->format == SIM_WAVE_FST)
        fst_end_definitions(w->backend.fst);
    else
        vcd_end_definitions(w->backend.vcd);
}

void sim_wave_set_time(SimWaveWriter *w, uint64_t time_ps)
{
    if (!w) return;
    if (w->format == SIM_WAVE_FST)
        fst_set_time(w->backend.fst, time_ps);
    else
        vcd_set_time(w->backend.vcd, time_ps);
}

void sim_wave_dump_value(SimWaveWriter *w, int sig_id, uint64_t value,
                          int width)
{
    if (!w) return;
    if (w->format == SIM_WAVE_FST)
        fst_dump_value(w->backend.fst, sig_id, value, width);
    else
        vcd_dump_value(w->backend.vcd, sig_id, value, width);
}

void sim_wave_close(SimWaveWriter *w)
{
    if (!w) return;
    if (w->format == SIM_WAVE_FST)
        fst_close(w->backend.fst);
    else
        vcd_close(w->backend.vcd);
    free(w);
}
