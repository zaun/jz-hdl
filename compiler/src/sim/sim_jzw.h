/**
 * @file sim_jzw.h
 * @brief JZW (JZ-HDL Waveform) SQLite-based waveform writer for simulation.
 *
 * Produces .jzw files per the JZW specification.
 * Same API pattern as sim_vcd.h / sim_fst.h.
 */

#ifndef JZ_SIM_JZW_H
#define JZ_SIM_JZW_H

#include <stdint.h>

typedef struct JZWWriter JZWWriter;

/**
 * @brief Open a new JZW file for writing.
 *
 * @param filename      Output file path.
 * @param timescale_ps  Timescale in picoseconds (informational; all times stored as ps).
 * @return Writer handle, or NULL on failure.
 */
JZWWriter *jzw_open(const char *filename, uint64_t timescale_ps);

/**
 * @brief Set simulation metadata.
 *
 * Must be called before jzw_end_definitions().
 */
void jzw_set_meta(JZWWriter *w, const char *key, const char *value);

/**
 * @brief Add a clock definition to the clocks table.
 */
void jzw_add_clock(JZWWriter *w, const char *name, uint64_t period_ps,
                   uint64_t phase_ps, uint64_t jitter_pp_ps,
                   double jitter_sigma_ps, double drift_max_ppm,
                   double drift_actual_ppm, double drifted_period_ps);

/**
 * @brief Add a signal to be tracked.
 *
 * Must be called before jzw_end_definitions().
 *
 * @param w      JZW writer.
 * @param scope  Hierarchical scope (e.g., "clocks", "wires", "dut").
 * @param name   Signal name.
 * @param width  Signal width in bits.
 * @param type   Signal type: "clock", "wire", or "tap".
 * @return Signal ID (0-based index), or -1 on failure.
 */
int jzw_add_signal(JZWWriter *w, const char *scope, const char *name,
                   int width, const char *type);

/**
 * @brief Finalize definitions and begin accepting value changes.
 */
void jzw_end_definitions(JZWWriter *w);

/**
 * @brief Set the current simulation time.
 *
 * @param w       JZW writer.
 * @param time_ps Current time in picoseconds.
 */
void jzw_set_time(JZWWriter *w, uint64_t time_ps);

/**
 * @brief Dump a signal value change.
 *
 * Only writes if the value differs from the signal's last recorded value.
 *
 * @param w      JZW writer.
 * @param sig_id Signal ID returned by jzw_add_signal().
 * @param value  Signal value (up to 64 bits).
 * @param width  Signal width.
 */
void jzw_dump_value(JZWWriter *w, int sig_id, uint64_t value, int width);

/**
 * @brief Add an annotation to the JZW database.
 *
 * @param w         JZW writer.
 * @param time_ps   Simulation time in picoseconds.
 * @param type      Annotation type string (e.g., "trace", "alert", "mark").
 * @param signal_id Signal ID, or -1 for global annotations.
 * @param message   Optional message text (may be NULL).
 * @param color     Optional color name (may be NULL).
 * @param end_time  Optional end time for range annotations, or 0 for point.
 */
void jzw_add_annotation(JZWWriter *w, uint64_t time_ps, const char *type,
                         int signal_id, const char *message,
                         const char *color, uint64_t end_time);

/**
 * @brief Set the simulation end time and close the database.
 *
 * @param w JZW writer.
 */
void jzw_close(JZWWriter *w);

#endif /* JZ_SIM_JZW_H */
