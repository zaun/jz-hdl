/**
 * @file ir_internal.h
 * @brief Internal interfaces and helpers for IR construction.
 *
 * This header exposes shared internal data structures and functions used
 * across the IR builder implementation files. These APIs are not part of
 * the public IR interface and are intended to be consumed only by IR
 * construction units.
 */

#ifndef JZ_HDL_IR_INTERNAL_H
#define JZ_HDL_IR_INTERNAL_H

#include "../../include/ir_builder.h"
#include "../../include/sem.h"
#include "../sem/driver_internal.h"

/**
 * @struct IR_BusSignalMapping
 * @brief Maps an expanded BUS signal to its IR signal ID.
 *
 * When a BUS port is declared (e.g., "BUS PARALLEL_BUS SOURCE pbus;"),
 * the IR builder expands it into individual signals (e.g., "pbus_SEL",
 * "pbus_ADDR", etc.). This structure tracks the mapping from the original
 * BUS access form (pbus.SEL) to the expanded IR signal ID.
 */
typedef struct IR_BusSignalMapping {
    const char *bus_port_name;  /**< BUS port name, e.g., "pbus" */
    const char *signal_name;    /**< Signal name within the BUS, e.g., "SEL" */
    int         array_index;    /**< Array index for arrayed ports, -1 otherwise */
    int         ir_signal_id;   /**< Expanded IR signal's ID */
    int         width;          /**< Signal width in bits */
} IR_BusSignalMapping;

/**
 * @struct IR_ModuleSpecOverride
 * @brief Represents a single CONST override applied to a specialized module.
 *
 * Each override binds a CONST name to a concrete value (integer or string)
 * evaluated in the context of the child module.
 */
typedef struct IR_ModuleSpecOverride {
    const char*             name;           /**< CONST name in child module */
    long long               value;          /**< Evaluated OVERRIDE value (integer) */
    const char*             string_value;   /**< String OVERRIDE value, or NULL */
} IR_ModuleSpecOverride;

/**
 * @struct IR_ModuleSpec
 * @brief Describes a specialized module derived from a base module.
 *
 * A specialization is uniquely identified by its base module and a sorted
 * set of CONST overrides. Specializations are materialized as separate
 * IR_Module entries.
 */
typedef struct IR_ModuleSpec {
    const JZASTNode*        base_mod_node;      /**< Child module AST node */
    int                     base_scope_index;   /**< Index into JZModuleScope array */
    IR_ModuleSpecOverride*  overrides;          /**< Sorted override set */
    int                     num_overrides;      /**< Number of overrides */
    char*                   spec_name;          /**< Specialized module name (arena-owned) */
    int                     spec_module_index;  /**< Assigned IR_Module index */
} IR_ModuleSpec;

/**
 * @brief Duplicate a string into the IR arena.
 *
 * @param arena IR arena used for allocation.
 * @param s     Null-terminated source string.
 * @return Newly allocated string in the arena, or NULL on failure.
 */
char *ir_strdup_arena(JZArena *arena, const char *s);

/* ============================================================================
 * Top-level IR construction
 * ============================================================================
 */

/**
 * @brief Look up a module or blackbox symbol in the project symbol table.
 *
 * Mirrors project-level lookup logic from the semantic layer, but is limited
 * to module and blackbox symbols for IR construction.
 *
 * @param project_symbols Project symbol buffer.
 * @param name            Module or blackbox name.
 * @return Matching symbol, or NULL if not found.
 */
const JZSymbol *ir_project_lookup_module_or_blackbox(const JZBuffer *project_symbols,
                                                     const char *name);

/**
 * @brief Build a complete IR_Design from the project AST.
 *
 * This is the main entry point for IR construction. It rebuilds symbol tables,
 * constructs modules, signals, memories, statements, clock domains, CDCs,
 * instances, specializations, and project-level metadata.
 *
 * @param root        Root AST node.
 * @param out_design  Output pointer to the constructed IR_Design.
 * @param arena       Arena used for all IR allocations.
 * @param diagnostics Diagnostic sink for reporting internal failures.
 * @return 0 on success, non-zero on failure.
 */
int jz_ir_build_design(JZASTNode *root,
                       IR_Design **out_design,
                       JZArena *arena,
                       JZDiagnosticList *diagnostics);

/**
 * @brief Find the IR module index corresponding to an AST module node.
 *
 * Relies on the invariant that IR modules are built in the same order as
 * JZModuleScope entries.
 *
 * @param scopes      Module scope array.
 * @param scope_count Number of scopes.
 * @param node        AST node to match.
 * @return IR module index, or -1 if not found.
 */
int ir_find_module_index_for_node(const JZModuleScope *scopes,
                                  size_t scope_count,
                                  const JZASTNode *node);

/* ============================================================================
 * Expression lowering
 * ============================================================================
 */

/**
 * @brief Lower an AST expression into an IR expression tree.
 *
 * Supports literals, identifiers, unary/binary operators, ternary,
 * concatenation, slices, intrinsics, memory reads, and BUS member access.
 *
 * @param arena           Arena for IR allocation.
 * @param expr            AST expression node.
 * @param mod_scope       Module scope for symbol resolution.
 * @param project_symbols Project-level symbols.
 * @param bus_map         BUS signal mapping array (may be NULL).
 * @param bus_map_count   Number of bus mapping entries.
 * @param diagnostics     Diagnostic sink.
 * @return Pointer to IR_Expr, or NULL if lowering fails.
 */
IR_Expr *ir_build_expr(JZArena *arena,
                       JZASTNode *expr,
                       const JZModuleScope *mod_scope,
                       const JZBuffer *project_symbols,
                       const IR_BusSignalMapping *bus_map,
                       int bus_map_count,
                       JZDiagnosticList *diagnostics);

/**
 * @brief Decode a sized literal into an IR_Literal.
 *
 * Accepts literals of the form "<width>'<base><value>" and evaluates
 * width expressions when necessary.
 *
 * @param lex             Literal lexeme.
 * @param mod_scope       Module scope for CONST evaluation.
 * @param project_symbols Project-level symbols.
 * @param out             Output literal.
 * @return 0 on success, non-zero on failure.
 */
int ir_decode_sized_literal(const char *lex,
                            const JZModuleScope *mod_scope,
                            const JZBuffer *project_symbols,
                            IR_Literal *out);

/**
 * @brief Resolve a qualified global constant reference (GLOBAL.CONST) to a literal.
 *
 * @param qname            Qualified identifier string (e.g., "PROTOCOL.SYNC_WORD").
 * @param mod_scope        Module scope (for width evaluation).
 * @param project_symbols  Project-level symbol table.
 * @param out              Output literal.
 * @return 0 on success, non-zero on failure.
 */
int ir_eval_global_const_qualified(const char *qname,
                                   const JZModuleScope *mod_scope,
                                   const JZBuffer *project_symbols,
                                   IR_Literal *out);

/**
 * @brief Try to evaluate a slice bound (MSB or LSB) from an AST node.
 *
 * Fast path: simple literal.  Slow path: const expression evaluation.
 *
 * @param node             AST expression for the slice bound.
 * @param mod_scope        Module scope for CONST evaluation.
 * @param project_symbols  Project-level symbols.
 * @param out              Output value.
 * @return 1 on success with *out set, 0 on failure.
 */
int ir_eval_slice_bound(const JZASTNode *node,
                        const JZModuleScope *mod_scope,
                        const JZBuffer *project_symbols,
                        unsigned *out);

/**
 * @brief Serialize a restricted AST expression into a constant-expression string.
 *
 * Used for feature-guard conditions. Only a limited subset of expressions
 * is supported.
 *
 * @param expr     AST expression node.
 * @param out_str  Output string (heap-allocated).
 * @return 0 on success, non-zero on failure.
 */
int ir_expr_to_const_expr_string(const JZASTNode *expr, char **out_str);

/* ============================================================================
 * Statement and block lowering
 * ============================================================================
 */

/**
 * @brief Lower an AST block node into an IR statement block.
 *
 * Handles assignments, feature guards, IF/ELIF/ELSE chains, SELECT/CASE,
 * and nested blocks.
 *
 * @param arena           Arena for IR allocation.
 * @param block_node      AST block node.
 * @param mod_scope       Module scope.
 * @param project_symbols Project-level symbols.
 * @param bus_map         BUS signal mapping array (may be NULL).
 * @param bus_map_count   Number of bus mapping entries.
 * @param diagnostics     Diagnostic sink.
 * @param next_assign_id  Assignment ID counter.
 * @return IR_Stmt block, or NULL if empty or on failure.
 */
IR_Stmt *ir_build_block_from_node(JZArena *arena,
                                  JZASTNode *block_node,
                                  const JZModuleScope *mod_scope,
                                  const JZBuffer *project_symbols,
                                  const IR_BusSignalMapping *bus_map,
                                  int bus_map_count,
                                  JZDiagnosticList *diagnostics,
                                  int *next_assign_id);

/* ============================================================================
 * Signal construction and width evaluation
 * ============================================================================
 */

/**
 * @brief Build IR_Signal entries for a module.
 *
 * Converts ports, wires, registers, and latches into IR_Signal structures.
 * BUS ports are expanded into individual IR_Signal entries for each member
 * signal in the BUS definition.
 *
 * @param scope             Module scope.
 * @param owner_module_id   Owning IR module ID.
 * @param arena             Arena for allocation.
 * @param project_symbols   Project-level symbols (for BUS definitions).
 * @param out_signals       Output signal array.
 * @param out_count         Output signal count.
 * @param out_bus_map       Output BUS signal mapping array (may be NULL).
 * @param out_bus_map_count Output BUS signal mapping count (may be NULL).
 * @return 0 on success, non-zero on failure.
 */
int ir_build_signals_for_module(const JZModuleScope *scope,
                                int owner_module_id,
                                JZArena *arena,
                                const JZBuffer *project_symbols,
                                IR_Signal **out_signals,
                                int *out_count,
                                IR_BusSignalMapping **out_bus_map,
                                int *out_bus_map_count);

/**
 * @brief Look up an expanded BUS signal's IR ID from a BUS access.
 *
 * @param map           BUS signal mapping array.
 * @param map_count     Number of entries in the mapping array.
 * @param port_name     BUS port name (e.g., "pbus").
 * @param signal_name   Signal name within BUS (e.g., "DATA").
 * @param array_index   Array index for arrayed ports, -1 for non-arrayed.
 * @return IR signal ID, or -1 if not found.
 */
int ir_lookup_bus_signal_id(const IR_BusSignalMapping *map,
                            int map_count,
                            const char *port_name,
                            const char *signal_name,
                            int array_index);

/**
 * @brief Find an IR signal by its semantic symbol ID.
 *
 * @param mod       IR module.
 * @param signal_id Semantic signal ID.
 * @return Pointer to IR_Signal, or NULL if not found.
 */
IR_Signal *ir_find_signal_by_id(IR_Module *mod, int signal_id);

/**
 * @brief Find the semantic symbol corresponding to a signal ID.
 *
 * @param scope     Module scope.
 * @param signal_id Semantic signal ID.
 * @return Symbol pointer, or NULL if not found.
 */
const JZSymbol *ir_find_symbol_by_signal_id(const JZModuleScope *scope, int signal_id);

/**
 * @brief Evaluate a width expression with specialization overrides applied.
 *
 * @param expr             Width expression text.
 * @param scope            Module scope.
 * @param project_symbols  Project symbols.
 * @param overrides        Override set.
 * @param num_overrides    Number of overrides.
 * @param out_width        Output width.
 * @return 0 on success, non-zero on failure.
 */
int ir_eval_width_with_overrides(const char *expr,
                                 const JZModuleScope *scope,
                                 const JZBuffer *project_symbols,
                                 const IR_ModuleSpecOverride *overrides,
                                 int num_overrides,
                                 unsigned *out_width);

/**
 * @brief Evaluate a constant expression with specialization overrides applied.
 *
 * @param expr             Constant expression text.
 * @param scope            Module scope.
 * @param project_symbols  Project symbols.
 * @param overrides        Override set.
 * @param num_overrides    Number of overrides.
 * @param out_value        Output value.
 * @return 0 on success, non-zero on failure.
 */
int ir_eval_const_with_overrides(const char *expr,
                                 const JZModuleScope *scope,
                                 const JZBuffer *project_symbols,
                                 const IR_ModuleSpecOverride *overrides,
                                 int num_overrides,
                                 long long *out_value);

/* ============================================================================
 * Memory construction
 * ============================================================================
 */

/**
 * @brief Build IR_Memory entries for a module.
 *
 * @param scope           Module scope.
 * @param arena           Arena for allocation.
 * @param project_symbols Project-level symbols.
 * @param out_mems        Output memory array.
 * @param out_count       Output memory count.
 * @return 0 on success, non-zero on failure.
 */
int ir_build_memories_for_module(const JZModuleScope *scope,
                                 JZArena *arena,
                                 const JZBuffer *project_symbols,
                                 IR_Memory **out_mems,
                                 int *out_count);

/**
 * @brief Create synthetic address register signals for SYNC read ports.
 *
 * @param arena           Arena for allocation.
 * @param mod             IR module (signals array may be reallocated).
 * @param bus_map         Pointer to bus_map array (may be reallocated).
 * @param bus_map_count   Pointer to bus_map entry count.
 * @return 0 on success, non-zero on failure.
 */
int ir_create_mem_addr_signals(JZArena *arena,
                               IR_Module *mod,
                               IR_BusSignalMapping **bus_map,
                               int *bus_map_count);

/**
 * @brief Bind memory ports after IR construction.
 *
 * @param design           IR design.
 * @param module_scopes    Module scopes.
 * @param project_symbols  Project symbols.
 * @param diagnostics      Diagnostic sink.
 * @return 0 on success, non-zero on failure.
 */
int jz_ir_bind_memory_ports(IR_Design *design,
                            JZBuffer  *module_scopes,
                            const JZBuffer *project_symbols,
                            JZDiagnosticList *diagnostics);

/* ============================================================================
 * Clock domains and CDC
 * ============================================================================
 */

/**
 * @brief Build IR clock domains for a module.
 *
 * Walks SYNCHRONOUS blocks in the module AST, lowers each to an
 * IR_ClockDomain, builds its statement tree, and collects all registers
 * written under that clock.
 *
 * @param scope            Module scope.
 * @param mod              IR module.
 * @param arena            Arena for allocation.
 * @param project_symbols  Project-level symbols.
 * @param bus_map          BUS signal mapping array (may be NULL).
 * @param bus_map_count    Number of bus mapping entries.
 * @param diagnostics      Diagnostic sink.
 * @return 0 on success, non-zero on failure.
 */
int ir_build_clock_domains_for_module(const JZModuleScope *scope,
                                      IR_Module *mod,
                                      JZArena *arena,
                                      const JZBuffer *project_symbols,
                                      const IR_BusSignalMapping *bus_map,
                                      int bus_map_count,
                                      JZDiagnosticList *diagnostics);

/**
 * @brief Build CDC crossings for a module.
 *
 * @param scope Module scope.
 * @param mod   IR module.
 * @param arena Arena for allocation.
 * @return 0 on success, non-zero on failure.
 */
int ir_build_cdc_for_module(const JZModuleScope *scope,
                            IR_Module *mod,
                            JZArena *arena);

/**
 * @brief Parse clock attributes from a CLOCKS declaration.
 *
 * @param attrs         Attribute string.
 * @param out_period_ns Output clock period in nanoseconds.
 * @param out_edge      Output clock edge.
 * @return 1 on success, 0 on failure.
 */
int ir_clock_parse_attrs(const char *attrs,
                         double *out_period_ns,
                         IR_ClockEdge *out_edge);

/* ============================================================================
 * Instances and specialization
 * ============================================================================
 */

/**
 * @brief Collect all module specializations in the design.
 *
 * @param scopes         Module scopes.
 * @param scope_count    Number of scopes.
 * @param project_symbols Project symbols.
 * @param arena          Arena for allocation.
 * @param out_specs      Output specialization array.
 * @param out_spec_count Output specialization count.
 * @return 0 on success, non-zero on failure.
 */
int ir_collect_module_specializations(const JZModuleScope *scopes,
                                      size_t scope_count,
                                      const JZBuffer *project_symbols,
                                      JZArena *arena,
                                      IR_ModuleSpec **out_specs,
                                      int *out_spec_count);

/**
 * @brief Build IR instances for a module.
 *
 * @param scope           Parent module scope.
 * @param mod             IR module.
 * @param all_scopes      All module scopes.
 * @param scope_count     Number of scopes.
 * @param project_symbols Project symbols.
 * @param specs           Module specializations.
 * @param spec_count      Number of specializations.
 * @param all_modules     All IR modules (for BUS signal lookup).
 * @param parent_bus_map  Parent module's BUS signal mapping.
 * @param parent_bus_map_count Number of entries in parent_bus_map.
 * @param arena           Arena for allocation.
 * @return 0 on success, non-zero on failure.
 */
/**
 * @brief Build bus_map entries for instance output port bindings.
 *
 * Scans @new instance declarations in the module AST and, for each OUT/INOUT
 * port binding that maps to a parent signal, appends an IR_BusSignalMapping
 * entry so that ir_build_expr can resolve "inst.port" qualified identifiers.
 *
 * Must be called during the first IR pass (after signals, before statement
 * lowering).
 *
 * @param scope           Parent module scope.
 * @param project_symbols Project-level symbol table.
 * @param arena           Arena for allocation.
 * @param bus_map         Pointer to bus_map array (may be reallocated).
 * @param bus_map_count   Pointer to bus_map entry count.
 * @return 0 on success, non-zero on failure.
 */
int ir_build_instance_port_mappings(const JZModuleScope *scope,
                                     const JZBuffer *project_symbols,
                                     JZArena *arena,
                                     IR_BusSignalMapping **bus_map,
                                     int *bus_map_count);

int ir_build_instances_for_module(const JZModuleScope *scope,
                                  IR_Module *mod,
                                  const JZModuleScope *all_scopes,
                                  size_t scope_count,
                                  const JZBuffer *project_symbols,
                                  const IR_ModuleSpec *specs,
                                  int spec_count,
                                  IR_Module *all_modules,
                                  const IR_BusSignalMapping *parent_bus_map,
                                  int parent_bus_map_count,
                                  JZArena *arena);

#endif
