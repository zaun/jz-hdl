/*
 * verilog_internal.h - Internal declarations for the Verilog-2005 backend.
 *
 * This header is shared among the split backend source files. It declares
 * static-linkage functions exposed across compilation units within the backend
 * as well as shared state and utility functions.
 */
#ifndef JZ_HDL_VERILOG_INTERNAL_H
#define JZ_HDL_VERILOG_INTERNAL_H

#include <stdio.h>
#include <stdbool.h>
#include "ir.h"
#include "diagnostic.h"

/* -------------------------------------------------------------------------
 * Global state
 * -------------------------------------------------------------------------
 */

/* When non-zero, assignment emission uses non-blocking '<=' rather than
 * blocking '=' (used for synchronous lowering in clock domains).
 */
extern int g_in_sequential;

/* When non-zero, memory accesses for BLOCK memories are skipped in the main
 * clock domain block (they are emitted in separate always blocks for BSRAM
 * inference).
 */
extern int g_skip_block_mem_accesses;

/* When non-zero, assignment emission skips INOUT port assignments in the
 * async block (they are emitted as continuous assign statements instead).
 */
extern int g_skip_inout_port_assigns;

/* -------------------------------------------------------------------------
 * Utility functions (common.c)
 * -------------------------------------------------------------------------
 */

/* Report a backend I/O error via the diagnostic system. */
void backend_io_error(JZDiagnosticList *diagnostics,
                      const char *input_filename,
                      const char *message);

/* Return non-zero if `name` is a Verilog-2005 reserved keyword. */
int verilog_is_keyword(const char *name);

/* Return `name` unchanged if it is not a Verilog keyword, otherwise write
 * the escaped form (\name ) into `buf` and return `buf`.
 */
const char *verilog_safe_name(const char *name, char *buf, int buf_size);

/* Return a safe Verilog memory array name, disambiguating when it collides
 * with the enclosing module name (which triggers a yosys crash).
 */
const char *verilog_memory_name(const char *mem_name,
                                const char *module_name,
                                char *buf, int buf_size);

/* Emit a Verilog range for a vector width, e.g., [WIDTH-1:0]. */
void emit_width_range(FILE *out, int width);

/* Simple indentation helper for statement emission. */
void emit_indent(FILE *out, int level);

/* -------------------------------------------------------------------------
 * Signal/clock lookup helpers (common.c)
 * -------------------------------------------------------------------------
 */

/* Lookup a signal in a module by its IR id, remapping through the current
 * alias context so that all references to aliased signals resolve to a
 * canonical Verilog identifier. Returns NULL if not found.
 */
const IR_Signal *find_signal_by_id(const IR_Module *mod, int signal_id);

/* Lookup a signal without applying alias canonicalization. Used for backend
 * decisions that depend on the original IR_SignalKind.
 */
const IR_Signal *find_signal_by_id_raw(const IR_Module *mod, int signal_id);

/* Find a signal in a module by its name. */
const IR_Signal *find_signal_by_name(const IR_Module *mod, const char *name);

/* Find a clock domain on a module by its IR id. Returns NULL if not found. */
const IR_ClockDomain *find_clock_domain_by_id(const IR_Module *mod, int clock_id);

/* Find the index of a signal in a module by its IR id (without aliasing). */
int find_signal_index_by_id_raw(const IR_Module *mod, int signal_id);

/* -------------------------------------------------------------------------
 * Alias context (alias.c)
 * -------------------------------------------------------------------------
 */

/* Return non-zero if the given assignment kind is an alias assignment. */
int assignment_kind_is_alias(IR_AssignmentKind kind);

/* Prepare the alias union-find context for a module. This must be called
 * before emitting any signal references within the module.
 */
void prepare_alias_context_for_module(const IR_Module *mod,
                                      int **out_canonical_index,
                                      int **out_is_repr);

/* Set the alias context arrays (called after prepare). */
void alias_ctx_set(const IR_Module *mod,
                   int *canonical_index,
                   int *is_repr,
                   int size);

/* Clear the alias context when done with a module. */
void alias_ctx_clear(void);

/* Get the canonical signal index for a signal, applying alias resolution. */
int alias_ctx_get_canonical_index(const IR_Module *mod, int signal_index);

/* Return non-zero if the signal at this index is the representative of its
 * alias group (and thus should be declared in Verilog).
 */
int alias_ctx_is_representative(const IR_Module *mod, int signal_index);

/* Emit continuous "assign" statements for alias assignments in a module. */
void emit_continuous_alias_assignments(FILE *out, const IR_Module *mod);

/* Emit tieoff assignments for wire bits not covered by sliced aliases. */
void emit_wire_tieoff_assignments(FILE *out, const IR_Module *mod);

/* -------------------------------------------------------------------------
 * Expression emission (emit_expr.c)
 * -------------------------------------------------------------------------
 */

/* Return the precedence level for an expression kind. */
int expr_precedence(IR_ExprKind kind);

/* Emit a sized literal as <width>'b<value>. */
void emit_literal(FILE *out, const IR_Literal *lit);

/* Emit an expression with full precedence handling. */
void emit_expr(FILE *out, const IR_Module *mod, const IR_Expr *expr);

/* Internal expression emitter with parent precedence for parenthesization. */
void emit_expr_internal(FILE *out,
                        const IR_Module *mod,
                        const IR_Expr *expr,
                        int parent_prec);

/* Return true if an expression kind is an intrinsic operator. */
bool is_intrinsic_expr(IR_ExprKind kind);

/* Emit a padded (zero- or sign-extended) expression. */
void emit_padded_expr(FILE *out,
                      const IR_Module *mod,
                      const IR_Expr *arg,
                      int target_width,
                      bool sign_extend);

/* -------------------------------------------------------------------------
 * Statement emission (emit_stmt.c)
 * -------------------------------------------------------------------------
 */

/* Emit a single statement with indentation. */
void emit_stmt(FILE *out, const IR_Module *mod, const IR_Stmt *stmt, int indent_level);

/* Emit a block statement (STMT_BLOCK). */
void emit_block_stmt(FILE *out, const IR_Module *mod, const IR_Stmt *block_stmt,
                     int indent_level, int is_root_block);

/* Emit an assignment statement. */
void emit_assignment_stmt(FILE *out,
                          const IR_Module *mod,
                          const IR_Assignment *a,
                          int indent_level);

/* Emit an IF/ELIF/ELSE chain. */
void emit_if_chain(FILE *out,
                   const IR_Module *mod,
                   const IR_Stmt *if_stmt_node,
                   int indent_level);

/* Emit a SELECT/CASE statement. */
void emit_select_stmt(FILE *out,
                      const IR_Module *mod,
                      const IR_Stmt *select_stmt_node,
                      int indent_level);

/* Helpers for width-extension assignment kinds. */
int assignment_kind_is_zext(IR_AssignmentKind kind);
int assignment_kind_is_sext(IR_AssignmentKind kind);

/* Emit memory read expression for null RHS assignments. Returns non-zero
 * if a memory read was emitted.
 */
int emit_mem_read_for_null(FILE *out,
                           const IR_Module *mod,
                           const IR_Signal *lhs_sig,
                           IR_MemPortKind required_port_kind);

/* -------------------------------------------------------------------------
 * Declaration emission (emit_decl.c)
 * -------------------------------------------------------------------------
 */

/* Emit the module header with port list. */
void emit_module_header(FILE *out, const IR_Module *mod);

/* Emit port direction/width declarations inside the module body. */
void emit_port_declarations(FILE *out, const IR_Module *mod);

/* Emit internal signal declarations (wires and regs). */
void emit_internal_signal_declarations(FILE *out, const IR_Module *mod);

/* Emit memory declarations as reg arrays. */
void emit_memory_declarations(FILE *out, const IR_Module *mod);

/* Emit memory initialization blocks.  Returns 0 on success, -1 on error. */
int emit_memory_initialization(FILE *out, const IR_Module *mod);

/* Helper: return non-zero if a given signal id is written in any
 * ASYNCHRONOUS or SYNCHRONOUS block of the module.
 */
int module_signal_is_written(const IR_Module *mod, int signal_id);

/* Helper: return non-zero if a given OUT port needs to be declared as reg. */
int module_port_needs_reg(const IR_Module *mod, int signal_id);

/* -------------------------------------------------------------------------
 * Always block emission (emit_blocks.c)
 * -------------------------------------------------------------------------
 */

/* Emit the ASYNCHRONOUS block as an always @* block. */
void emit_async_block(FILE *out, const IR_Module *mod);

/* Emit all clock domain (SYNCHRONOUS) blocks. */
void emit_clock_domains(FILE *out, const IR_Module *mod);

/* Emit a single clock domain block. */
void emit_clock_domain(FILE *out,
                       const IR_Module *mod,
                       const IR_ClockDomain *cd);

/* Check if a memory is BLOCK type. */
int memory_is_block_type(const IR_Module *mod, const char *mem_name);

/* Check if a statement is a BLOCK memory write that should be skipped. */
int stmt_is_skipped_block_mem_write(const IR_Module *mod, const IR_Stmt *stmt);

/* Check if an assignment has a BLOCK memory read on the RHS that should be skipped. */
int assignment_has_skipped_block_mem_read(const IR_Module *mod, const IR_Assignment *a);

/* Check if a BLOCK memory has a BSRAM read intermediate signal registered.
 * Returns non-zero if the named memory has been emitted with a separate
 * BSRAM read always block and an intermediate output signal.
 */
int has_bsram_read_intermediate(const char *mem_name);

/* Clear all BSRAM read mappings (call before each clock domain). */
void bsram_mappings_clear(void);

/* -------------------------------------------------------------------------
 * Module instance emission (emit_instances.c)
 * -------------------------------------------------------------------------
 */

/* Emit structural instances for a module based on IR_Instance entries. */
void emit_instances(FILE *out, const IR_Design *design, const IR_Module *mod);

/* Return non-zero if any IR_Module in the design has the given name. */
int design_has_module_named(const IR_Design *design, const char *name);

/* -------------------------------------------------------------------------
 * Project wrapper emission (emit_wrapper.c)
 * -------------------------------------------------------------------------
 */

/* Emit the project-level wrapper module that exposes board pins. */
void emit_project_wrapper(FILE *out, const IR_Design *design,
                          JZDiagnosticList *diagnostics);

/* -------------------------------------------------------------------------
 * Constraint file emission (constraints.c)
 * -------------------------------------------------------------------------
 */

/* Helper to open a backend output file with atomic write semantics. */
int open_backend_output(const char *target,
                        FILE **out,
                        int *close_out,
                        char *tmp_path,
                        size_t tmp_path_size,
                        JZDiagnosticList *diagnostics,
                        const char *input_filename);

/* Helper to close and finalize a backend output file. */
int close_backend_output(FILE *out,
                         int close_out,
                         const char *tmp_path,
                         const char *final_path,
                         JZDiagnosticList *diagnostics,
                         const char *input_filename);

/* -------------------------------------------------------------------------
 * Main emission (verilog.c)
 * -------------------------------------------------------------------------
 */

/* Emit all modules in dependency order. */
void emit_module_order(FILE *out, const IR_Design *design);

#endif /* JZ_HDL_VERILOG_INTERNAL_H */
