/**
 * @file verilog_backend.h
 * @brief Verilog and constraint-file code generation from IR.
 *
 * Provides entry points for emitting Verilog-2005 output and optional
 * constraint files (SDC, XDC, PCF, CST) from a verified IR_Design.
 */

#ifndef JZ_HDL_VERILOG_BACKEND_H
#define JZ_HDL_VERILOG_BACKEND_H

#include "ir.h"
#include "diagnostic.h"

/**
 * @brief Emit Verilog output from an IR design.
 *
 * @param design         Verified IR design produced by jz_ir_build_design().
 * @param filename       Output path for generated Verilog. NULL or "-" for stdout.
 * @param diagnostics    Optional diagnostic list for backend errors (may be NULL).
 * @param input_filename Original JZ-HDL source path for diagnostics (may be NULL).
 * @return 0 on success, non-zero on failure (with BACKEND* diagnostic recorded).
 */
int jz_emit_verilog(const IR_Design *design,
                    const char *filename,
                    JZDiagnosticList *diagnostics,
                    const char *input_filename);

/**
 * @brief Emit SDC timing constraints from an IR design.
 *
 * No-op returning 0 when the design has no project or clock data.
 *
 * @param design         IR design to process.
 * @param filename       Output path for the .sdc file.
 * @param diagnostics    Optional diagnostic list for I/O errors.
 * @param input_filename Original source path for diagnostics (may be NULL).
 * @return 0 on success, non-zero on I/O failure.
 */
int jz_emit_sdc_constraints(const IR_Design *design,
                            const char *filename,
                            JZDiagnosticList *diagnostics,
                            const char *input_filename);

/**
 * @brief Emit Xilinx XDC constraints from an IR design.
 *
 * No-op returning 0 when the design has no project or pin data.
 *
 * @param design         IR design to process.
 * @param filename       Output path for the .xdc file.
 * @param diagnostics    Optional diagnostic list for I/O errors.
 * @param input_filename Original source path for diagnostics (may be NULL).
 * @return 0 on success, non-zero on I/O failure.
 */
int jz_emit_xdc_constraints(const IR_Design *design,
                            const char *filename,
                            JZDiagnosticList *diagnostics,
                            const char *input_filename);

/**
 * @brief Emit PCF pin constraints from an IR design.
 *
 * No-op returning 0 when the design has no project or pin data.
 *
 * @param design         IR design to process.
 * @param filename       Output path for the .pcf file.
 * @param diagnostics    Optional diagnostic list for I/O errors.
 * @param input_filename Original source path for diagnostics (may be NULL).
 * @return 0 on success, non-zero on I/O failure.
 */
int jz_emit_pcf_constraints(const IR_Design *design,
                            const char *filename,
                            JZDiagnosticList *diagnostics,
                            const char *input_filename);

/**
 * @brief Emit Gowin-style CST constraints from an IR design.
 *
 * No-op returning 0 when the design has no project or pin data.
 *
 * @param design         IR design to process.
 * @param filename       Output path for the .cst file.
 * @param diagnostics    Optional diagnostic list for I/O errors.
 * @param input_filename Original source path for diagnostics (may be NULL).
 * @return 0 on success, non-zero on I/O failure.
 */
int jz_emit_cst_constraints(const IR_Design *design,
                            const char *filename,
                            JZDiagnosticList *diagnostics,
                            const char *input_filename);

#endif /* JZ_HDL_VERILOG_BACKEND_H */
