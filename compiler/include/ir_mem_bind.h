/**
 * @file ir_mem_bind.h
 * @brief Memory port-binding IR pass.
 *
 * Runs after IR construction (jz_ir_build_design()) to bind
 * IR_MemoryPort fields to concrete signal IDs by analyzing the module
 * scopes and MEM AST nodes.
 *
 * Bound fields per port kind:
 *   - WRITE ports: addr_signal_id, data_in_signal_id, enable_signal_id
 *   - READ_ASYNC ports: addr_signal_id, data_out_signal_id
 *   - READ_SYNC ports: addr_signal_id, data_out_signal_id
 */

#ifndef JZ_HDL_IR_MEM_BIND_H
#define JZ_HDL_IR_MEM_BIND_H

#include "ir.h"
#include "diagnostic.h"

/**
 * @brief Bind memory port signals in an IR design.
 *
 * Uses module scopes to resolve IR_MemoryPort address, data, and enable
 * signal IDs from the AST. Must be called after jz_ir_build_design().
 *
 * @param design          IR design to process. Must not be NULL.
 * @param module_scopes   Module scope buffer from IR construction.
 * @param project_symbols Project-level symbol table (reserved for future use).
 * @param diagnostics     Diagnostic list for error reporting.
 * @return 0 on success, non-zero on internal error.
 */
int jz_ir_bind_memory_ports(IR_Design *design,
                            JZBuffer  *module_scopes,
                            const JZBuffer *project_symbols,
                            JZDiagnosticList *diagnostics);

#endif /* JZ_HDL_IR_MEM_BIND_H */
