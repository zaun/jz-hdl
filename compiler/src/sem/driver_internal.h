#ifndef JZ_HDL_DRIVER_INTERNAL_H
#define JZ_HDL_DRIVER_INTERNAL_H

#include "ast.h"
#include "util.h"
#include "diagnostic.h"
#include "rules.h"
#include "chip_data.h"
#include "tristate_types.h"

/* Forward declaration of bit-vector type used in expression inference. */
struct JZBitvecType;

/* Symbol table types shared across driver implementation files. */
typedef enum JZSymbolKind {
    JZ_SYM_MODULE,
    JZ_SYM_BLACKBOX,
    JZ_SYM_CONST,
    JZ_SYM_PORT,
    JZ_SYM_WIRE,
    JZ_SYM_REGISTER,
    JZ_SYM_LATCH,
    JZ_SYM_MEM,
    JZ_SYM_MUX,
    JZ_SYM_INSTANCE,
    JZ_SYM_CONFIG,
    JZ_SYM_CLOCK,
    JZ_SYM_PIN,
    JZ_SYM_MAP_ENTRY,
    JZ_SYM_GLOBAL,
    JZ_SYM_BUS
} JZSymbolKind;

typedef struct JZSymbol {
    const char   *name;  /* pointer into AST node->name */
    JZSymbolKind  kind;
    JZASTNode    *node;  /* declaration node */
    int           id;    /* stable integer ID for IR_Signal binding; -1 if not a signal */
    int           can_be_z; /* non-zero if any driver can assign 'z' to this signal */
    JZASTNode    *feature_guard;  /* owning @feature guard (NULL if unconditional) */
    JZASTNode    *feature_branch; /* THEN/ELSE branch node within the guard */
} JZSymbol;

typedef struct JZModuleScope {
    const char *name;     /* module or blackbox name */
    JZASTNode  *node;     /* JZ_AST_MODULE or JZ_AST_BLACKBOX */
    JZBuffer    symbols;  /* array of JZSymbol */
    JZBuffer    bus_signal_decls; /* array of JZASTNode* for BUS member signals */
    int         next_signal_id; /* monotonically increasing ID for PORT/WIRE/REGISTER symbols */
} JZModuleScope;

/* Net-graph types shared between flow analysis and alias reporting. */
typedef struct JZAliasEdge {
    JZASTNode *lhs_decl;   /* base declaration on LHS of alias */
    JZASTNode *rhs_decl;   /* base declaration on RHS of alias */
    JZASTNode *stmt;       /* originating JZ_AST_STMT_ASSIGN node */
} JZAliasEdge;

typedef struct JZNet {
    JZBuffer atoms;        /* array of JZASTNode* declarations (PORT/WIRE/REGISTER/LATCH) */
    JZBuffer driver_stmts; /* array of JZASTNode* JZ_AST_STMT_ASSIGN that drive this net */
    JZBuffer sink_stmts;   /* array of JZASTNode* JZ_AST_STMT_ASSIGN that read from this net */
    JZBuffer alias_edges;  /* array of JZAliasEdge describing alias relationships */
} JZNet;

typedef struct JZNetBinding {
    JZASTNode *decl;   /* declaration node belonging to a net */
    size_t     net_ix; /* index into JZNet array */
} JZNetBinding;

/* MEM helper types shared with dead-code analysis. */
typedef struct JZMemPortRef {
    JZASTNode *mem_decl; /* JZ_AST_MEM_DECL */
    JZASTNode *port;     /* JZ_AST_MEM_PORT (IN/OUT) */
    int        field;    /* JZMemPortField */
} JZMemPortRef;

typedef struct JZMemWriteKey {
    JZASTNode *mem_decl;
    JZASTNode *port; /* OUT port node */
} JZMemWriteKey;

typedef struct JZMemSyncReadAssignKey {
    JZASTNode *mem_decl;
    JZASTNode *port;      /* OUT SYNC port node */
    JZASTNode *dest_decl; /* destination declaration (PORT/WIRE/REGISTER) */
    JZASTNode *addr_expr; /* address expression for mem.port.addr */
} JZMemSyncReadAssignKey;

typedef enum JZMemPortField {
    MEM_PORT_FIELD_NONE = 0,
    MEM_PORT_FIELD_ADDR,
    MEM_PORT_FIELD_DATA,
    MEM_PORT_FIELD_WDATA   /* Write data for INOUT ports */
} JZMemPortField;

/* Exclusive-assignment helper types shared with clock-domain checks. */
typedef struct JZAssignRange {
    int      has_range;
    unsigned lsb;
    unsigned msb;
} JZAssignRange;

typedef struct JZAssignTargetEntry {
    JZASTNode    *decl;
    int           is_register;
    JZAssignRange range;
    int           is_nested;
} JZAssignTargetEntry;

typedef struct JZBusAccessInfo {
    const JZASTNode *port_decl;
    const JZASTNode *bus_def;
    const JZASTNode *signal_decl;
    int has_index;
    int is_wildcard;
    int index_known;
    unsigned index_value;
    unsigned count;
    int is_array;
    int readable;
    int writable;
    char bus_id[128];
    char role[128];
    char signal_name[128];
} JZBusAccessInfo;

/* Shared helpers implemented across driver*.c. */
void sem_report_rule(JZDiagnosticList *diagnostics,
                     JZLocation loc,
                     const char *rule_id,
                     const char *explanation);

/* Simple integer parsing helpers used across driver translation units. */
int parse_simple_positive_int(const char *s, unsigned *out);
int parse_simple_nonnegative_int(const char *s, unsigned *out);
int parse_literal_unsigned_value(const char *s, unsigned *out);
int eval_simple_positive_decl_int(const char *s, unsigned *out);
int parse_simple_signed_int(const char *s, long long *out);
int sem_resolve_bus_access(const JZASTNode *expr,
                           const JZModuleScope *mod_scope,
                           const JZBuffer *project_symbols,
                           JZBusAccessInfo *out,
                           JZDiagnosticList *diagnostics);
JZASTNode *sem_bus_get_or_create_signal_decl(JZModuleScope *scope,
                                             const char *bus_port_name,
                                             int has_index,
                                             unsigned index,
                                             const char *signal_name,
                                             const JZASTNode *signal_decl);

/* CONFIG usage helpers shared between module and project semantic checks. */
void sem_check_undeclared_config_in_width(const char *expr,
                                          JZLocation loc,
                                          const JZBuffer *project_symbols,
                                          JZDiagnosticList *diagnostics);

/* GLOBAL usage helpers shared between module and project semantic checks. */
int sem_expr_has_global_ref(const char *expr,
                            const JZBuffer *project_symbols);

/* Literal analysis helpers shared with flow/dead-code and operator passes. */
void infer_literal_type(JZASTNode *node,
                        JZDiagnosticList *diagnostics,
                        struct JZBitvecType *out);
int sem_literal_is_const_zero(const char *lex, int *out_known);

/*
 * CONST/CONFIG-based integer and width evaluation helpers.
 *
 * These helpers are used by front-end consumers (including IR construction)
 * to obtain fully-resolved integer values for width/depth expressions given
 * the current module scope and project-level CONFIG table.
 */
int sem_eval_const_expr_in_module(const char *expr,
                                  const JZModuleScope *scope,
                                  const JZBuffer *project_symbols,
                                  long long *out_value);

int sem_resolve_string_const(const char *name,
                             const JZModuleScope *scope,
                             const JZBuffer *project_symbols,
                             const char **out_str,
                             JZDiagnosticList *diagnostics,
                             JZLocation loc);

int sem_eval_const_expr_in_project(const char *expr,
                                   const JZBuffer *project_symbols,
                                   long long *out_value);

int sem_eval_width_expr(const char *expr,
                        const JZModuleScope *scope,
                        const JZBuffer *project_symbols,
                        unsigned *out_width);

int sem_expand_widthof_in_width_expr(const char *expr,
                                     const JZModuleScope *scope,
                                     const JZBuffer *project_symbols,
                                     char **out_expanded,
                                     int depth);

int sem_expand_widthof_in_width_expr_diag(const char *expr,
                                          const JZModuleScope *scope,
                                          const JZBuffer *project_symbols,
                                          char **out_expanded,
                                          int depth,
                                          JZDiagnosticList *diagnostics,
                                          JZLocation loc);

int sem_expr_has_lit_call(const char *expr_text);

/* Core expression type/width inference helper implemented in driver_operators.c
 * and reused by MEM and flow passes.
 */
void infer_expr_type(JZASTNode *expr,
                     const JZModuleScope *mod_scope,
                     const JZBuffer *project_symbols,
                     JZDiagnosticList *diagnostics,
                     struct JZBitvecType *out);

/* Module-level literal width validation implemented in driver_literal.c. */
void sem_check_module_literal_widths(const JZModuleScope *scope,
                                     const JZBuffer *project_symbols,
                                     JZDiagnosticList *diagnostics);

int module_scope_add_symbol(JZModuleScope *scope,
                            JZSymbolKind kind,
                            const char *name,
                            JZASTNode *decl,
                            JZDiagnosticList *diagnostics);

int module_scope_add_symbol_featured(JZModuleScope *scope,
                                     JZSymbolKind kind,
                                     const char *name,
                                     JZASTNode *decl,
                                     JZASTNode *feature_guard,
                                     JZASTNode *feature_branch,
                                     JZDiagnosticList *diagnostics);

const JZSymbol *module_scope_lookup(const JZModuleScope *scope,
                                    const char *name);

const JZSymbol *module_scope_lookup_kind(const JZModuleScope *scope,
                                         const char *name,
                                         JZSymbolKind kind);

/* MEM helper used across modules. */
int sem_match_mem_port_slice(JZASTNode *slice,
                             const JZModuleScope *mod_scope,
                             JZDiagnosticList *diagnostics,
                             JZMemPortRef *out);
int sem_match_mem_port_qualified_ident(JZASTNode *expr,
                                       const JZModuleScope *mod_scope,
                                       JZDiagnosticList *diagnostics,
                                       JZMemPortRef *out);

/* MEM declaration/access/usage helpers implemented in driver_mem.c. */
void sem_check_mem_access_expr(JZASTNode *expr,
                               const JZModuleScope *mod_scope,
                               const JZBuffer *project_symbols,
                               JZDiagnosticList *diagnostics);
void sem_check_mem_addr_assign(const JZMemPortRef *ref,
                               JZASTNode *addr_expr,
                               const JZModuleScope *mod_scope,
                               const JZBuffer *project_symbols,
                               JZDiagnosticList *diagnostics);

void sem_track_mem_out_write(JZBuffer *writes,
                             const JZMemPortRef *ref,
                             JZDiagnosticList *diagnostics,
                             JZLocation loc);

void sem_check_module_mem_and_mux_decls(const JZModuleScope *scope,
                                        const JZBuffer *project_symbols,
                                        JZDiagnosticList *diagnostics);

void sem_check_module_mem_chip_configs(const JZModuleScope *scope,
                                       const JZBuffer *project_symbols,
                                       const JZChipData *chip,
                                       JZDiagnosticList *diagnostics);

void sem_check_module_latch_chip_support(const JZModuleScope *scope,
                                          const JZChipData *chip,
                                          JZASTNode *project,
                                          const JZBuffer *project_symbols,
                                          JZDiagnosticList *diagnostics);

void sem_check_module_mem_port_usage(const JZModuleScope *scope,
                                     JZDiagnosticList *diagnostics);

void sem_check_project_mem_resources(JZASTNode *project,
                                     const JZBuffer *module_scopes,
                                     const JZBuffer *project_symbols,
                                     const JZChipData *chip,
                                     JZDiagnosticList *diagnostics);

/* Project-level symbol table helpers shared between driver_project.c and driver_project_hw.c. */
JZModuleScope *find_module_scope_for_node(JZBuffer *scopes,
                                          JZASTNode *node);

const JZSymbol *project_lookup(const JZBuffer *symbols,
                               const char *name,
                               JZSymbolKind kind);

const JZSymbol *project_lookup_module_or_blackbox(const JZBuffer *symbols,
                                                  const char *name);

JZASTNode *sem_find_project_top_new(JZASTNode *project);

/* Project-level symbol table and semantic helpers implemented in driver_project.c. */
int build_symbol_tables(JZASTNode *project,
                        JZBuffer *module_scopes,
                        JZBuffer *project_symbols,
                        JZDiagnosticList *diagnostics);

void sem_check_project_name_unique(JZASTNode *project,
                                   const JZBuffer *project_symbols,
                                   JZDiagnosticList *diagnostics);

void sem_check_project_config(JZASTNode *project,
                              JZDiagnosticList *diagnostics);

void sem_check_project_clocks(JZASTNode *project,
                              JZBuffer *module_scopes,
                              const JZBuffer *project_symbols,
                              JZDiagnosticList *diagnostics);

void sem_check_project_clock_gen(JZASTNode *project,
                                 const JZBuffer *project_symbols,
                                 const JZChipData *chip,
                                 JZDiagnosticList *diagnostics);

void sem_check_project_pins(JZASTNode *project,
                            const JZBuffer *project_symbols,
                            JZDiagnosticList *diagnostics);

void sem_check_project_map(JZASTNode *project,
                           const JZBuffer *project_symbols,
                           JZDiagnosticList *diagnostics);

void sem_check_project_buses(JZASTNode *project,
                             const JZBuffer *project_symbols,
                             JZDiagnosticList *diagnostics);

void sem_check_project_blackboxes(JZASTNode *project,
                                  JZDiagnosticList *diagnostics);

void sem_check_globals(JZASTNode *project,
                       const JZBuffer *project_symbols,
                       JZDiagnosticList *diagnostics);

void sem_check_project_top_new(JZASTNode *project,
                               JZBuffer *module_scopes,
                               const JZBuffer *project_symbols,
                               JZDiagnosticList *diagnostics);

void sem_check_unused_modules(JZASTNode *project,
                              JZDiagnosticList *diagnostics);

void resolve_names_recursive(JZASTNode *node,
                             JZBuffer *module_scopes,
                             const JZBuffer *project_symbols,
                             const JZModuleScope *current_scope,
                             JZDiagnosticList *diagnostics);

/* Flow-pass entry points implemented in driver_flow.c.
 *
 * The net-graph construction now accepts project_symbols so that reporting
 * helpers (such as the alias-resolution report) can incorporate
 * project-level context like CLOCKS/IN_PINS/MAP when describing nets.
 */
void sem_build_net_graphs(JZASTNode *root,
                          JZBuffer *module_scopes,
                          const JZBuffer *project_symbols,
                          JZDiagnosticList *diagnostics);

/* Alias-report emission (implemented in src/report/alias/alias_report.c). */
void sem_emit_alias_report_for_module(const JZModuleScope *scope,
                                      JZBuffer *nets,
                                      const JZBuffer *module_scopes,
                                      const JZBuffer *project_symbols,
                                      JZASTNode *project_root);
void sem_emit_alias_report_finalize(void);

/* Tri-state report emission (implemented in src/report/tristate/tristate_report.c). */
void sem_emit_tristate_report_for_module(const JZModuleScope *scope,
                                         JZBuffer *nets,
                                         const JZBuffer *module_scopes,
                                         const JZBuffer *project_symbols,
                                         JZASTNode *project_root);
void sem_emit_tristate_report_finalize(void);

/* Memory-report emission (implemented in src/report/memory_report.c). */
void sem_emit_memory_report(JZASTNode *root,
                            const JZBuffer *module_scopes,
                            const JZBuffer *project_symbols,
                            const JZChipData *chip,
                            const char *input_filename);

void sem_check_exclusive_assignments(JZASTNode *root,
                                     JZBuffer *module_scopes,
                                     const JZBuffer *project_symbols,
                                     JZDiagnosticList *diagnostics);

/* Exclusive assignment helpers shared with clock-domain checks (driver_clocks.c). */
void sem_excl_collect_targets_from_lhs(JZASTNode *lhs,
                                       const JZModuleScope *scope,
                                       const JZBuffer *project_symbols,
                                       int is_sync,
                                       int is_nested,
                                       JZBuffer *out);

void sem_check_dead_code(JZASTNode *root,
                         JZBuffer *module_scopes,
                         JZDiagnosticList *diagnostics);

void sem_check_sync_clock_domains(JZBuffer *module_scopes,
                                  JZDiagnosticList *diagnostics);


/* --- Declarations for functions split across driver_*.c files --- */

/* driver.c helpers shared with new files */
void sem_check_mux_selectors_recursive(JZASTNode *node,
                                       const JZModuleScope *mod_scope,
                                       JZDiagnosticList *diagnostics);

int sem_expr_contains_x_literal_anywhere(const JZASTNode *expr);

void sem_lhs_observable_classify(JZASTNode *lhs,
                                 const JZModuleScope *mod_scope,
                                 int *out_has_register,
                                 int *out_has_out_inout);

int sem_expr_has_latch_identifier(const JZASTNode *expr,
                                  const JZModuleScope *mod_scope);

int sem_bus_port_has_writable_signal(const JZASTNode *port_decl,
                                    const JZBuffer *project_symbols);

int sem_slice_literal_width(JZASTNode *slice, unsigned *out_width);

/**
 * @brief Evaluate a pure-literal AST expression tree to an integer.
 *
 * Handles EXPR_LITERAL leaves and EXPR_BINARY nodes with arithmetic
 * operators (ADD, SUB, MUL, DIV, MOD).  Used for template-expanded
 * slice bounds where IDX substitution produces expression trees like
 * 0*11+10 rather than simple literal nodes.
 *
 * @param expr  AST expression node.
 * @param out   Output value.
 * @return 1 on success, 0 on failure (unknown node type, div-by-zero, etc.).
 */
int sem_try_const_eval_ast_expr(const JZASTNode *expr, long *out);

int sem_extract_identifier_like(const char *s,
                                char *ident,
                                size_t ident_cap);

int sem_eval_simple_index_literal(JZASTNode *idx, unsigned *out);

int sem_literal_has_x_bits(const char *lex);
int sem_literal_has_z_bits(const char *lex);

/* driver_control.c */
void sem_check_block_control_flow(JZASTNode *block,
                                  const JZModuleScope *mod_scope,
                                  const JZBuffer *project_symbols,
                                  int is_async,
                                  int is_sync,
                                  JZDiagnosticList *diagnostics);

/* driver_assign.c */
void sem_check_block_assignments(JZASTNode *block,
                                 const JZModuleScope *mod_scope,
                                 const JZBuffer *project_symbols,
                                 JZDiagnosticList *diagnostics,
                                 int is_sync,
                                 JZBuffer *mem_out_writes,
                                 JZBuffer *mem_sync_reads);

/* driver_width.c */
int sem_instance_width_expr_is_invalid(const char *expr,
                                       const JZModuleScope *parent_scope,
                                       const JZBuffer *project_symbols);

int sem_expr_has_undefined_width_ident(const char *expr_text,
                                       const JZModuleScope *scope,
                                       const JZBuffer *project_symbols);

int sem_expr_has_nonpositive_simple_width_literal(const char *expr_text);

void sem_check_module_decl_widths(const JZModuleScope *scope,
                                  const JZBuffer *project_symbols,
                                  JZDiagnosticList *diagnostics);

int sem_check_clog2_expr_simple(const char *expr_text,
                                JZLocation loc,
                                JZDiagnosticList *diagnostics);

/* driver_instance.c */
void sem_check_module_instantiations(const JZModuleScope *scope,
                                     JZBuffer *module_scopes,
                                     const JZBuffer *project_symbols,
                                     JZDiagnosticList *diagnostics);

/* driver_expr.c */
void sem_check_project_checks(JZASTNode *root,
                              const JZBuffer *project_symbols,
                              JZDiagnosticList *diagnostics);

void sem_check_module_checks(const JZModuleScope *scope,
                             const JZBuffer *project_symbols,
                             JZDiagnosticList *diagnostics);

void sem_check_module_expressions(const JZModuleScope *scope,
                                  const JZBuffer *project_symbols,
                                  JZDiagnosticList *diagnostics);

void sem_check_expressions(JZASTNode *root,
                           JZBuffer *module_scopes,
                           const JZBuffer *project_symbols,
                           const JZChipData *chip,
                           JZDiagnosticList *diagnostics);

void sem_check_module_slices(const JZModuleScope *scope,
                             const JZBuffer *project_symbols,
                             JZDiagnosticList *diagnostics);

/* driver.c (was static, now called from driver_expr.c) */
void sem_check_module_const_blocks(const JZModuleScope *scope,
                                   const JZBuffer *project_symbols,
                                   JZDiagnosticList *diagnostics);

int sem_block_reads_name(const JZASTNode *node, const char *name);

/* driver_net.c */
JZNetBinding *sem_net_find_binding(JZBuffer *bindings,
                                   JZASTNode *decl);

/* driver_comb.c */
void sem_check_combinational_loops_for_module(const JZModuleScope *scope,
                                              JZBuffer *nets,
                                              JZBuffer *bindings,
                                              JZDiagnosticList *diagnostics);

void sem_comb_collect_targets_from_lhs(JZASTNode *lhs,
                                       const JZModuleScope *scope,
                                       JZBuffer *out_decls);

void sem_comb_collect_sources_from_expr(JZASTNode *expr,
                                        const JZModuleScope *scope,
                                        JZBuffer *out_decls);

/* driver_tristate.c — tri-state proof engine */

/* Set project symbols context for resolving qualified identifiers (e.g., DEV.ROM)
 * in tristate analysis. Must be called before tristate analysis.
 */
void jz_tristate_set_project_symbols(const JZBuffer *project_symbols);

/* Set parent module scope for resolving module CONST values (e.g., DEV_A)
 * in tristate analysis. Must be called before analyzing each module.
 */
void jz_tristate_set_parent_scope(const JZModuleScope *scope);

void jz_tristate_analyze_net(JZTristateNetInfo *info,
                             const JZNet *net,
                             const char *net_name,
                             const char *bus_field,
                             const JZModuleScope *scope,
                             const JZBuffer *module_scopes);

int sem_tristate_check_net(const JZNet *net,
                           const char *net_name,
                           const JZModuleScope *scope,
                           const JZBuffer *module_scopes);

int jz_tristate_net_is_bus_port(const JZNet *net);
const char *jz_tristate_extract_bus_field(const JZASTNode *stmt);
unsigned jz_tristate_decl_width_simple(JZASTNode *decl);

#endif /* JZ_HDL_DRIVER_INTERNAL_H */
