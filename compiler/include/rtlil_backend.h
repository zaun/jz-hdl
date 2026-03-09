/**
 * @file rtlil_backend.h
 * @brief RTLIL code generation from IR.
 *
 * Provides an entry point for emitting RTLIL (yosys intermediate language)
 * output from a verified IR_Design. This bypasses the Verilog round-trip
 * and gives yosys a cleaner starting point for synthesis.
 */

#ifndef JZ_HDL_RTLIL_BACKEND_H
#define JZ_HDL_RTLIL_BACKEND_H

#include "ir.h"
#include "diagnostic.h"

/**
 * @brief Emit RTLIL output from an IR design.
 *
 * @param design         Verified IR design produced by jz_ir_build_design().
 * @param filename       Output path for generated RTLIL. NULL or "-" for stdout.
 * @param diagnostics    Optional diagnostic list for backend errors (may be NULL).
 * @param input_filename Original JZ-HDL source path for diagnostics (may be NULL).
 * @return 0 on success, non-zero on failure (with BACKEND* diagnostic recorded).
 */
int jz_emit_rtlil(const IR_Design *design,
                   const char *filename,
                   JZDiagnosticList *diagnostics,
                   const char *input_filename);

#endif /* JZ_HDL_RTLIL_BACKEND_H */
