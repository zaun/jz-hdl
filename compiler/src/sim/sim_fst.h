/**
 * @file sim_fst.h
 * @brief FST (Fast Signal Trace) waveform writer for simulation.
 *
 * Produces binary FST files compatible with GTKWave.
 * Same API as sim_vcd.h for easy interchangeability.
 */

#ifndef JZ_SIM_FST_H
#define JZ_SIM_FST_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

typedef struct FSTWriter FSTWriter;

/**
 * @brief Open a new FST file for writing.
 *
 * @param filename     Output file path.
 * @param timescale_ps Timescale in picoseconds (e.g., 1000 for 1ns).
 * @return Writer handle, or NULL on failure.
 */
FSTWriter *fst_open(const char *filename, uint64_t timescale_ps);

/**
 * @brief Add a signal to be tracked in the FST.
 *
 * Must be called before fst_end_definitions().
 *
 * @param w     FST writer.
 * @param scope Hierarchical scope (e.g., "dut", "tb").
 * @param name  Signal name.
 * @param width Signal width in bits.
 * @return Signal ID (0-based index), or -1 on failure.
 */
int fst_add_signal(FSTWriter *w, const char *scope, const char *name, int width);

/**
 * @brief Finalize the FST header and variable definitions.
 *
 * Must be called after all fst_add_signal() calls and before any value dumps.
 *
 * @param w FST writer.
 */
void fst_end_definitions(FSTWriter *w);

/**
 * @brief Set the current simulation time.
 *
 * @param w       FST writer.
 * @param time_ps Current time in picoseconds.
 */
void fst_set_time(FSTWriter *w, uint64_t time_ps);

/**
 * @brief Dump a signal value change.
 *
 * @param w      FST writer.
 * @param sig_id Signal ID returned by fst_add_signal().
 * @param value  Signal value (up to 64 bits).
 * @param width  Signal width.
 */
void fst_dump_value(FSTWriter *w, int sig_id, uint64_t value, int width);

/**
 * @brief Close the FST file and free resources.
 *
 * This is where the FST binary data is finalized and written.
 *
 * @param w FST writer.
 */
void fst_close(FSTWriter *w);

#endif /* JZ_SIM_FST_H */
