/*
 * rtlil_internal.h - Internal declarations for the RTLIL backend.
 *
 * This header is shared among the split backend source files. It declares
 * functions exposed across compilation units within the backend as well as
 * shared state and utility functions.
 */
#ifndef JZ_HDL_RTLIL_INTERNAL_H
#define JZ_HDL_RTLIL_INTERNAL_H

#include <stdio.h>
#include <stdbool.h>
#include "ir.h"
#include "diagnostic.h"

/* -------------------------------------------------------------------------
 * Auto-ID counter for generating unique names
 * -------------------------------------------------------------------------
 */

/* Get the next unique auto-ID. Thread-unsafe (single-threaded backend). */
int rtlil_next_id(void);

/* Reset the auto-ID counter (call before starting a new design). */
void rtlil_reset_id(void);

/* Get the current auto-ID value (for autoidx attribute). */
int rtlil_current_id(void);

/* -------------------------------------------------------------------------
 * Utility functions (common.c)
 * -------------------------------------------------------------------------
 */

/* Report a backend I/O error via the diagnostic system. */
void rtlil_backend_io_error(JZDiagnosticList *diagnostics,
                            const char *input_filename,
                            const char *message);

/* Simple indentation helper. */
void rtlil_indent(FILE *out, int level);

/* Emit an RTLIL constant: width'binary_digits (e.g., 8'00001010).
 * For z literals, emits x characters.
 */
void rtlil_emit_const(FILE *out, const IR_Literal *lit);

/* Emit a constant of given width and value. */
void rtlil_emit_const_val(FILE *out, int width, uint64_t value);

/* Emit a zero constant of given width. */
void rtlil_emit_zero(FILE *out, int width);

/* Emit a sigspec for a signal reference, including alias resolution. */
void rtlil_emit_sigspec_signal(FILE *out, const IR_Module *mod, int signal_id);

/* Emit a sigspec for a signal with bit selection. */
void rtlil_emit_sigspec_bit(FILE *out, const IR_Module *mod, int signal_id, int bit);

/* Emit a sigspec for a signal with range selection. */
void rtlil_emit_sigspec_range(FILE *out, const IR_Module *mod, int signal_id,
                              int offset, int width);

/* -------------------------------------------------------------------------
 * Signal/clock lookup helpers (common.c)
 * -------------------------------------------------------------------------
 */

/* Lookup a signal in a module by its IR id, remapping through alias context. */
const IR_Signal *rtlil_find_signal_by_id(const IR_Module *mod, int signal_id);

/* Lookup a signal without alias canonicalization. */
const IR_Signal *rtlil_find_signal_by_id_raw(const IR_Module *mod, int signal_id);

/* Find a clock domain by its IR id. */
const IR_ClockDomain *rtlil_find_clock_domain_by_id(const IR_Module *mod, int clock_id);

/* -------------------------------------------------------------------------
 * Alias context (common.c)
 *
 * The RTLIL backend reuses the same alias union-find logic as the Verilog
 * backend. These are thin wrappers that forward to the Verilog backend's
 * alias.c functions (which are compiled into jz_hdl_lib).
 * -------------------------------------------------------------------------
 */

/* -------------------------------------------------------------------------
 * Expression emission (emit_cells.c)
 *
 * RTLIL expressions are decomposed into cells with intermediate wires.
 * The emit function returns a sigspec string for the result.
 * -------------------------------------------------------------------------
 */

/* Maximum sigspec string length. */
#define RTLIL_SIGSPEC_MAX 4096

/*
 * Emit cells for an expression tree and write the result sigspec into
 * `out_sigspec`. Returns 0 on success.
 */
int rtlil_emit_expr(FILE *out, const IR_Module *mod, const IR_Expr *expr,
                    char *out_sigspec, int sigspec_size);

/* -------------------------------------------------------------------------
 * Process emission (emit_process.c)
 * -------------------------------------------------------------------------
 */

/* Emit the ASYNCHRONOUS block as an RTLIL process. */
void rtlil_emit_async_block(FILE *out, const IR_Module *mod);

/* Emit all clock domain (SYNCHRONOUS) processes. */
void rtlil_emit_clock_domains(FILE *out, const IR_Module *mod);

/* -------------------------------------------------------------------------
 * Module emission (emit_module.c)
 * -------------------------------------------------------------------------
 */

/* Emit module header with autoidx. */
void rtlil_emit_module_header(FILE *out, const IR_Module *mod, int is_top);

/* Emit wire declarations for all signals. */
void rtlil_emit_wires(FILE *out, const IR_Module *mod);

/* Emit memory declarations. */
void rtlil_emit_memories(FILE *out, const IR_Module *mod);

/* Emit connect statements for alias assignments. */
void rtlil_emit_alias_connects(FILE *out, const IR_Module *mod);

/* -------------------------------------------------------------------------
 * Instance emission (emit_instances.c)
 * -------------------------------------------------------------------------
 */

/* Emit user module instances as RTLIL cells. */
void rtlil_emit_instances(FILE *out, const IR_Design *design, const IR_Module *mod);

/* -------------------------------------------------------------------------
 * Memory emission (emit_memory.c)
 * -------------------------------------------------------------------------
 */

/* Emit $memrd_v2/$memwr_v2 cells for memory accesses. */
void rtlil_emit_memory_cells(FILE *out, const IR_Module *mod);

/* -------------------------------------------------------------------------
 * Project wrapper emission (emit_wrapper.c)
 * -------------------------------------------------------------------------
 */

/* Emit the project-level wrapper module. */
void rtlil_emit_project_wrapper(FILE *out, const IR_Design *design);

/* -------------------------------------------------------------------------
 * Main emission (rtlil_main.c)
 * -------------------------------------------------------------------------
 */

/* Emit all modules in dependency order. */
void rtlil_emit_module_order(FILE *out, const IR_Design *design);

/* Return non-zero if any IR_Module in the design has the given name. */
int rtlil_design_has_module_named(const IR_Design *design, const char *name);

#endif /* JZ_HDL_RTLIL_INTERNAL_H */
