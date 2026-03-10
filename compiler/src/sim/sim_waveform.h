/**
 * @file sim_waveform.h
 * @brief Generic waveform writer interface wrapping VCD, FST, and JZW formats.
 */

#ifndef JZ_SIM_WAVEFORM_H
#define JZ_SIM_WAVEFORM_H

#include <stdint.h>

typedef enum {
    SIM_WAVE_VCD = 0,
    SIM_WAVE_FST = 1,
    SIM_WAVE_JZW = 2
} SimWaveFormat;

typedef struct SimWaveWriter SimWaveWriter;

/**
 * @brief Open a waveform writer for the given format.
 */
SimWaveWriter *sim_wave_open(const char *filename, uint64_t timescale_ps,
                              SimWaveFormat format);

/**
 * @brief Set metadata (only used by JZW format; no-op for VCD/FST).
 */
void sim_wave_set_meta(SimWaveWriter *w, const char *key, const char *value);

/**
 * @brief Add a signal to the waveform.
 *
 * @param type Signal type: "clock", "wire", or "tap" (used by JZW only).
 * @return Signal ID (0-based), or -1 on failure.
 */
int sim_wave_add_signal(SimWaveWriter *w, const char *scope, const char *name,
                         int width, const char *type);

/**
 * @brief Finalize definitions.
 */
void sim_wave_end_definitions(SimWaveWriter *w);

/**
 * @brief Set the current simulation time.
 */
void sim_wave_set_time(SimWaveWriter *w, uint64_t time_ps);

/**
 * @brief Dump a value change.
 */
void sim_wave_dump_value(SimWaveWriter *w, int sig_id, uint64_t value,
                          int width);

/**
 * @brief Close the writer and free resources.
 */
void sim_wave_close(SimWaveWriter *w);

#endif /* JZ_SIM_WAVEFORM_H */
