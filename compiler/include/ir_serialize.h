/**
 * @file ir_serialize.h
 * @brief JSON serialization of the IR for the --ir CLI mode.
 *
 * Writes an IR_Design as JSON to a file or stdout. File output uses
 * atomic write-and-rename to avoid partial writes on failure.
 */

#ifndef JZ_HDL_IR_SERIALIZE_H
#define JZ_HDL_IR_SERIALIZE_H

#include "ir.h"
#include "diagnostic.h"

/**
 * @brief Serialize an IR_Design to JSON.
 *
 * When filename is "-", JSON is written to stdout. Otherwise, the output
 * is written atomically via a temporary file and rename.
 *
 * @param design         IR design to serialize. Must not be NULL.
 * @param filename       Output path, or "-" for stdout.
 * @param diagnostics    Optional diagnostic list for I/O error reporting (may be NULL).
 * @param input_filename Original JZ-HDL source path for metadata (may be NULL).
 * @return 0 on success, non-zero on I/O or rename failure.
 */
int jz_ir_write_json(const IR_Design *design,
                     const char *filename,
                     JZDiagnosticList *diagnostics,
                     const char *input_filename);

#endif /* JZ_HDL_IR_SERIALIZE_H */
