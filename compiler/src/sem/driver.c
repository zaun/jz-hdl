#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <stdio.h>
#include <time.h>

#include "sem_driver.h"
#include "sem.h"
#include "util.h"
#include "rules.h"
#include "driver_internal.h"



/* -------------------------------------------------------------------------
 *  Helper for rule-based diagnostics (uses JZRuleInfo for severity/description)
 * -------------------------------------------------------------------------
 */

void sem_report_rule(JZDiagnosticList *diagnostics,
                     JZLocation loc,
                     const char *rule_id,
                     const char *explanation)
{
    if (!diagnostics || !rule_id) return;

    const JZRuleInfo *rule = jz_rule_lookup(rule_id);
    JZSeverity sev = JZ_SEVERITY_ERROR;

    if (rule) {
        switch (rule->mode) {
        case JZ_RULE_MODE_WRN:
            sev = JZ_SEVERITY_WARNING;
            break;
        case JZ_RULE_MODE_INF:
            sev = JZ_SEVERITY_NOTE;
            break;
        case JZ_RULE_MODE_ERR:
        default:
            sev = JZ_SEVERITY_ERROR;
            break;
        }
    }

    /* Store the explanation as the diagnostic message. At print time,
     * print_line() uses the rule table description for the main output line.
     * d->message is shown as the --explain detail underneath.
     * For rules without a table description, d->message doubles as display text.
     */
    const char *msg = explanation ? explanation : rule_id;
    jz_diagnostic_report(diagnostics, loc, sev, rule_id, msg);
}

/* -------------------------------------------------------------------------
 *  Identifier lexical rules (length, single underscore)
 * -------------------------------------------------------------------------
 */

/* Forward declarations for simple integer parsing helpers defined later in
 * this file (used by project-level semantic checks).
 */
int parse_simple_nonnegative_int(const char *s, unsigned *out);
int eval_simple_positive_decl_int(const char *s, unsigned *out);

/* Forward declaration for widthof()-expansion helper used by CONST/width
 * evaluation routines later in this file.
 */
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

/* Return non-zero if the given identifier name is a reserved keyword from
 * the language specification (statement-level, block, or direction/type).
 */
static int sem_is_reserved_keyword(const char *name)
{
    if (!name || !*name) return 0;

    /* Statement-level keywords */
    if (!strcmp(name, "IF") || !strcmp(name, "ELIF") || !strcmp(name, "ELSE") ||
        !strcmp(name, "SELECT") || !strcmp(name, "CASE") || !strcmp(name, "DEFAULT")) {
        return 1;
    }

    /* Direction/type and block keywords */
    if (!strcmp(name, "CONST") || !strcmp(name, "PORT") || !strcmp(name, "REGISTER") ||
        !strcmp(name, "LATCH") ||
        !strcmp(name, "WIRE") || !strcmp(name, "MEM") || !strcmp(name, "MUX") ||
        !strcmp(name, "CDC") ||
        !strcmp(name, "IN") || !strcmp(name, "OUT") || !strcmp(name, "INOUT") ||
        !strcmp(name, "ASYNCHRONOUS") || !strcmp(name, "SYNCHRONOUS") ||
        !strcmp(name, "OVERRIDE") || !strcmp(name, "CONFIG") ||
        !strcmp(name, "CLOCKS") || !strcmp(name, "IN_PINS") ||
        !strcmp(name, "OUT_PINS") || !strcmp(name, "INOUT_PINS") ||
        !strcmp(name, "MAP") ||
        !strcmp(name, "IDX") ||
        !strcmp(name, "VCC") || !strcmp(name, "GND")) {
        return 1;
    }

    return 0;
}

static int sem_identifier_is_decl_context(const JZASTNode *node)
{
    if (!node) return 0;
    switch (node->type) {
    case JZ_AST_MODULE:
    case JZ_AST_PROJECT:
    case JZ_AST_CONST_DECL:
    case JZ_AST_PORT_DECL:
    case JZ_AST_WIRE_DECL:
    case JZ_AST_REGISTER_DECL:
    case JZ_AST_MEM_DECL:
    case JZ_AST_MEM_PORT:
    case JZ_AST_LATCH_DECL:
    case JZ_AST_MUX_DECL:
    case JZ_AST_BUS_DECL:
    case JZ_AST_MODULE_INSTANCE:
        return 1;
    default:
        return 0;
    }
}

static void sem_check_identifier_lexical(JZASTNode *node,
                                         JZASTNode *parent,
                                         JZDiagnosticList *diagnostics)
{
    if (!node) return;

    if (node->name) {
        size_t len = strlen(node->name);

        /* ID_SYNTAX_INVALID: identifier exceeds 255 characters. */
        if (len > 255 && diagnostics) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "'%.*s...' is %zu characters long\n"
                     "identifiers must be 255 characters or fewer",
                     32, node->name, len);
            sem_report_rule(diagnostics,
                            node->loc,
                            "ID_SYNTAX_INVALID",
                            msg);
        }

        /* ID_SINGLE_UNDERSCORE: single '_' used as a regular identifier outside
         * no-connect contexts.
         *
         * Allowed contexts:
         *   - project-level top @top bindings (ProjectTopInstance -> PortDecl)
         *   - module-level @new bindings where '_' appears as the RHS expression
         *     under a PortDecl (instance body binding).
         */
        if (len == 1 && node->name[0] == '_' && diagnostics) {
            int allowed = 0;
            if (parent) {
                if (parent->type == JZ_AST_PROJECT_TOP_INSTANCE &&
                    node->type == JZ_AST_PORT_DECL) {
                    allowed = 1;
                } else if (parent->type == JZ_AST_PORT_DECL &&
                           node->type == JZ_AST_EXPR_IDENTIFIER) {
                    /* RHS '_' in module-level @new instance binding. */
                    allowed = 1;
                }
            }
            if (!allowed) {
                sem_report_rule(diagnostics,
                                node->loc,
                                "ID_SINGLE_UNDERSCORE",
                                "'_' is reserved for no-connect bindings in @new and @top blocks\n"
                                "use a descriptive name instead");
            }
        }

        /* KEYWORD_AS_IDENTIFIER: reserved keyword used in a declaration name. */
        if (diagnostics && sem_identifier_is_decl_context(node) &&
            sem_is_reserved_keyword(node->name)) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "'%s' is a reserved keyword and cannot be used as a declaration name",
                     node->name);
            sem_report_rule(diagnostics,
                            node->loc,
                            "KEYWORD_AS_IDENTIFIER",
                            msg);
        }
    }

    for (size_t i = 0; i < node->child_count; ++i) {
        sem_check_identifier_lexical(node->children[i], node, diagnostics);
    }
}

/* -------------------------------------------------------------------------
 *  Symbol tables
 * -------------------------------------------------------------------------
 */

int module_scope_add_symbol(JZModuleScope *scope,
                                   JZSymbolKind kind,
                                   const char *name,
                                   JZASTNode *decl,
                                   JZDiagnosticList *diagnostics)
{
    return module_scope_add_symbol_featured(scope, kind, name, decl,
                                            NULL, NULL, diagnostics);
}

int module_scope_add_symbol_featured(JZModuleScope *scope,
                                     JZSymbolKind kind,
                                     const char *name,
                                     JZASTNode *decl,
                                     JZASTNode *feature_guard,
                                     JZASTNode *feature_branch,
                                     JZDiagnosticList *diagnostics)
{
    if (!scope || !name || !*name) return 0;

    size_t count = scope->symbols.len / sizeof(JZSymbol);
    JZSymbol *syms = (JZSymbol *)scope->symbols.data;

    for (size_t i = 0; i < count; ++i) {
        JZSymbol *existing = &syms[i];
        if (!existing->name) continue;
        if (strcmp(existing->name, name) != 0) continue;

        /* Allow duplicate names in mutually exclusive @feature branches:
         * same guard, different branch nodes. */
        if (feature_guard && existing->feature_guard == feature_guard &&
            feature_branch && existing->feature_branch &&
            existing->feature_branch != feature_branch) {
            continue;
        }

        /* Handle instance-specific conflicts first. */
        if (existing->kind == JZ_SYM_INSTANCE && kind == JZ_SYM_INSTANCE) {
            if (diagnostics) {
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "instance '%s' is already declared in this module\n"
                         "each @new instance must have a unique name",
                         name);
                sem_report_rule(diagnostics,
                                decl->loc,
                                "INSTANCE_NAME_DUP_IN_MODULE",
                                msg);
            }
            return 0;
        }
        if (existing->kind == JZ_SYM_INSTANCE || kind == JZ_SYM_INSTANCE) {
            if (diagnostics) {
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "'%s' is used as both an instance name and a signal/CONST name\n"
                         "instance names must not collide with other identifiers",
                         name);
                sem_report_rule(diagnostics,
                                decl->loc,
                                "INSTANCE_NAME_CONFLICT",
                                msg);
            }
            return 0;
        }

        /* MUX_NAME_DUPLICATE: any duplicate involving a MUX identifier gets a
         * dedicated rule so that MUX namespace conflicts are reported with
         * MUX-specific context instead of the generic ID_DUP_IN_MODULE.
         */
        if ((existing->kind == JZ_SYM_MUX && kind != JZ_SYM_INSTANCE) ||
            (kind == JZ_SYM_MUX && existing->kind != JZ_SYM_INSTANCE)) {
            if (diagnostics) {
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "MUX '%s' conflicts with an existing declaration of the same name",
                         name);
                sem_report_rule(diagnostics,
                                decl->loc,
                                "MUX_NAME_DUPLICATE",
                                msg);
            }
            return 0;
        }

        /* MEM_DUP_NAME: two MEM declarations with the same name in a module. */
        if (existing->kind == JZ_SYM_MEM && kind == JZ_SYM_MEM) {
            if (diagnostics) {
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "MEM '%s' is already declared in this module\n"
                         "each MEM block must have a unique name",
                         name);
                sem_report_rule(diagnostics,
                                decl->loc,
                                "MEM_DUP_NAME",
                                msg);
            }
            return 0;
        }

        /* Generic duplicate identifier within module. */
        if (diagnostics) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "'%s' is already declared in this module\n"
                     "all PORT, WIRE, REGISTER, CONST, and LATCH names must be unique",
                     name);
            sem_report_rule(diagnostics,
                            decl->loc,
                            "ID_DUP_IN_MODULE",
                            msg);
        }
        return 0;
    }

    JZSymbol sym;
    sym.name = name;
    sym.kind = kind;
    sym.node = decl;
    sym.can_be_z = 0;
    sym.feature_guard = feature_guard;
    sym.feature_branch = feature_branch;
    /* Assign stable IDs only for symbols that will become IR_Signal entries.
     * Other symbol kinds keep id = -1.
     */
    if (kind == JZ_SYM_PORT || kind == JZ_SYM_WIRE ||
        kind == JZ_SYM_REGISTER || kind == JZ_SYM_LATCH) {
        if (scope->next_signal_id < 0) {
            scope->next_signal_id = 0;
        }
        sym.id = scope->next_signal_id++;
    } else {
        sym.id = -1;
    }

    if (jz_buf_append(&scope->symbols, &sym, sizeof(sym)) != 0) {
        return -1;
    }
    return 0;
}

const JZSymbol *module_scope_lookup(const JZModuleScope *scope,
                                      const char *name)
{
    if (!scope || !name) return NULL;
    size_t count = scope->symbols.len / sizeof(JZSymbol);
    const JZSymbol *syms = (const JZSymbol *)scope->symbols.data;
    for (size_t i = 0; i < count; ++i) {
        if (syms[i].name && strcmp(syms[i].name, name) == 0) {
            return &syms[i];
        }
    }
    return NULL;
}

const JZSymbol *module_scope_lookup_kind(const JZModuleScope *scope,
                                           const char *name,
                                           JZSymbolKind kind)
{
    if (!scope || !name) return NULL;
    size_t count = scope->symbols.len / sizeof(JZSymbol);
    const JZSymbol *syms = (const JZSymbol *)scope->symbols.data;
    for (size_t i = 0; i < count; ++i) {
        if (syms[i].name && syms[i].kind == kind && strcmp(syms[i].name, name) == 0) {
            return &syms[i];
        }
    }
    return NULL;
}

static int sem_parse_bus_port_meta(const JZASTNode *decl,
                                   char *bus_id,
                                   size_t bus_sz,
                                   char *role,
                                   size_t role_sz)
{
    if (bus_id && bus_sz > 0) bus_id[0] = '\0';
    if (role && role_sz > 0) role[0] = '\0';
    if (!decl || !decl->text || !bus_id || !role) {
        return 0;
    }
    if (sscanf(decl->text, "%127s %127s", bus_id, role) != 2) {
        return 0;
    }
    return 1;
}

static void sem_bus_signal_access_dirs(const char *bus_dir,
                                       const char *role,
                                       int *out_readable,
                                       int *out_writable)
{
    if (out_readable) *out_readable = 0;
    if (out_writable) *out_writable = 0;
    if (!bus_dir || !role) return;

    if (strcmp(bus_dir, "INOUT") == 0) {
        if (out_readable) *out_readable = 1;
        if (out_writable) *out_writable = 1;
        return;
    }

    int is_source = (strcmp(role, "SOURCE") == 0);
    int is_target = (strcmp(role, "TARGET") == 0);
    if (!is_source && !is_target) return;

    if (strcmp(bus_dir, "OUT") == 0) {
        if (is_source) {
            if (out_writable) *out_writable = 1;
        } else {
            if (out_readable) *out_readable = 1;
        }
    } else if (strcmp(bus_dir, "IN") == 0) {
        if (is_source) {
            if (out_readable) *out_readable = 1;
        } else {
            if (out_writable) *out_writable = 1;
        }
    }
}

int sem_bus_port_has_writable_signal(const JZASTNode *port_decl,
                                            const JZBuffer *project_symbols)
{
    if (!port_decl || !project_symbols || !project_symbols->data) return 0;
    if (!port_decl->block_kind || strcmp(port_decl->block_kind, "BUS") != 0) return 0;

    char bus_id[128];
    char role[128];
    if (!sem_parse_bus_port_meta(port_decl, bus_id, sizeof(bus_id), role, sizeof(role))) {
        return 0;
    }

    const JZSymbol *syms = (const JZSymbol *)project_symbols->data;
    size_t count = project_symbols->len / sizeof(JZSymbol);
    const JZSymbol *bus_sym = NULL;
    for (size_t i = 0; i < count; ++i) {
        if (syms[i].kind == JZ_SYM_BUS && syms[i].name &&
            strcmp(syms[i].name, bus_id) == 0) {
            bus_sym = &syms[i];
            break;
        }
    }
    if (!bus_sym || !bus_sym->node) {
        return 0;
    }

    for (size_t i = 0; i < bus_sym->node->child_count; ++i) {
        JZASTNode *decl = bus_sym->node->children[i];
        if (!decl || decl->type != JZ_AST_BUS_DECL || !decl->block_kind) continue;
        int readable = 0;
        int writable = 0;
        sem_bus_signal_access_dirs(decl->block_kind, role, &readable, &writable);
        if (writable) {
            return 1;
        }
    }

    return 0;
}

int sem_resolve_bus_access(const JZASTNode *expr,
                           const JZModuleScope *mod_scope,
                           const JZBuffer *project_symbols,
                           JZBusAccessInfo *out,
                           JZDiagnosticList *diagnostics)
{
    if (!expr || !mod_scope || !out) return 0;
    memset(out, 0, sizeof(*out));

    const char *bus_port_name = NULL;
    const char *signal_name = NULL;
    const JZASTNode *index_expr = NULL;
    int is_wildcard = 0;

    if (expr->type == JZ_AST_EXPR_BUS_ACCESS) {
        bus_port_name = expr->name;
        signal_name = expr->text;
        if (expr->child_count > 0) {
            index_expr = expr->children[0];
        }
        if (expr->block_kind && strcmp(expr->block_kind, "WILDCARD") == 0) {
            is_wildcard = 1;
        }
    } else if (expr->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER && expr->name) {
        const char *full = expr->name;
        const char *dot = strchr(full, '.');
        if (!dot || dot == full || !*(dot + 1)) {
            return 0;
        }
        if (strchr(dot + 1, '.') != NULL) {
            return 0;
        }
        static char head[256];
        size_t head_len = (size_t)(dot - full);
        if (head_len >= sizeof(head)) head_len = sizeof(head) - 1;
        memcpy(head, full, head_len);
        head[head_len] = '\0';
        bus_port_name = head;
        signal_name = dot + 1;
    } else {
        return 0;
    }

    if (!bus_port_name || !signal_name) {
        return 0;
    }

    const JZSymbol *port_sym = module_scope_lookup_kind(mod_scope, bus_port_name, JZ_SYM_PORT);
    if (!port_sym || !port_sym->node) {
        if (diagnostics && expr->type == JZ_AST_EXPR_BUS_ACCESS) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "'%s' is not declared in this module",
                     bus_port_name);
            sem_report_rule(diagnostics,
                            expr->loc,
                            "UNDECLARED_IDENTIFIER",
                            msg);
            return 1;
        }
        return 0;
    }
    if (!port_sym->node->block_kind || strcmp(port_sym->node->block_kind, "BUS") != 0) {
        if (diagnostics && (expr->type == JZ_AST_EXPR_BUS_ACCESS ||
                            expr->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER)) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "'%s' is not a BUS port; dot-access syntax requires BUS PORT declaration",
                     bus_port_name);
            sem_report_rule(diagnostics,
                            expr->loc,
                            "BUS_PORT_NOT_BUS",
                            msg);
            return 1;
        }
        return 0;
    }

    out->port_decl = port_sym->node;

    if (!sem_parse_bus_port_meta(port_sym->node, out->bus_id, sizeof(out->bus_id),
                                 out->role, sizeof(out->role))) {
        return 1;
    }

    if (!project_symbols || !project_symbols->data) {
        return 1;
    }

    const JZSymbol *syms = (const JZSymbol *)project_symbols->data;
    size_t count = project_symbols->len / sizeof(JZSymbol);
    const JZSymbol *bus_sym = NULL;
    for (size_t i = 0; i < count; ++i) {
        if (syms[i].kind == JZ_SYM_BUS && syms[i].name &&
            strcmp(syms[i].name, out->bus_id) == 0) {
            bus_sym = &syms[i];
            break;
        }
    }
    if (!bus_sym || !bus_sym->node) {
        if (diagnostics) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "BUS type '%s' referenced by port '%s' is not declared in the project\n"
                     "add a BUS %s { ... } definition at the project level",
                     out->bus_id, bus_port_name, out->bus_id);
            sem_report_rule(diagnostics,
                            expr->loc,
                            "BUS_PORT_UNKNOWN_BUS",
                            msg);
        }
        return 1;
    }
    out->bus_def = bus_sym->node;

    strncpy(out->signal_name, signal_name, sizeof(out->signal_name) - 1u);
    out->signal_name[sizeof(out->signal_name) - 1u] = '\0';

    const JZASTNode *signal_decl = NULL;
    if (bus_sym->node) {
        for (size_t bi = 0; bi < bus_sym->node->child_count; ++bi) {
            JZASTNode *decl = bus_sym->node->children[bi];
            if (!decl || decl->type != JZ_AST_BUS_DECL || !decl->name) continue;
            if (strcmp(decl->name, out->signal_name) == 0) {
                signal_decl = decl;
                break;
            }
        }
    }
    if (!signal_decl) {
        if (diagnostics) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "'%s' is not a signal in BUS '%s'\n"
                     "check the BUS definition for valid signal names",
                     out->signal_name, out->bus_id);
            sem_report_rule(diagnostics,
                            expr->loc,
                            "BUS_SIGNAL_UNDEFINED",
                            msg);
        }
        return 1;
    }
    out->signal_decl = signal_decl;

    unsigned bus_count = 1;
    int is_array = 0;
    if (port_sym->node->width) {
        unsigned tmp = 0;
        if (sem_eval_width_expr(port_sym->node->width, mod_scope, project_symbols, &tmp) != 0) {
            if (diagnostics) {
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "BUS port '%s' array count '%s' could not be evaluated\n"
                         "array count must be a positive integer constant expression",
                         bus_port_name, port_sym->node->width);
                sem_report_rule(diagnostics,
                                port_sym->node->loc,
                                "BUS_PORT_ARRAY_COUNT_INVALID",
                                msg);
            }
            bus_count = 1;
        } else {
            bus_count = tmp;
            is_array = 1;
        }
    }

    out->count = bus_count;
    out->is_array = is_array;
    out->has_index = (index_expr != NULL) || is_wildcard;
    out->is_wildcard = is_wildcard;

    if (!out->has_index && is_array) {
        if (diagnostics) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "'%s' is a BUS array of %u elements; use %s[index].%s or %s[*].%s",
                     bus_port_name, bus_count,
                     bus_port_name, signal_name,
                     bus_port_name, signal_name);
            sem_report_rule(diagnostics,
                            expr->loc,
                            "BUS_PORT_INDEX_REQUIRED",
                            msg);
        }
    }

    if (out->has_index && !is_array) {
        if (diagnostics) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "'%s' is not a BUS array; remove the index and use %s.%s",
                     bus_port_name, bus_port_name, signal_name);
            sem_report_rule(diagnostics,
                            expr->loc,
                            "BUS_PORT_INDEX_NOT_ARRAY",
                            msg);
        }
    }

    if (index_expr && !is_wildcard) {
        unsigned idx_val = 0;
        if (sem_eval_simple_index_literal((JZASTNode *)index_expr, &idx_val)) {
            out->index_known = 1;
            out->index_value = idx_val;
            if (is_array && idx_val >= bus_count) {
                if (diagnostics) {
                    char msg[512];
                    snprintf(msg, sizeof(msg),
                             "%s[%u] is out of range; '%s' has %u element%s (valid indices: 0..%u)",
                             bus_port_name, idx_val,
                             bus_port_name, bus_count,
                             bus_count == 1 ? "" : "s",
                             bus_count - 1);
                    sem_report_rule(diagnostics,
                                    expr->loc,
                                    "BUS_PORT_INDEX_OUT_OF_RANGE",
                                    msg);
                }
            }
        }
    }

    sem_bus_signal_access_dirs(signal_decl->block_kind ? signal_decl->block_kind : "",
                               out->role,
                               &out->readable,
                               &out->writable);
    return 1;
}

JZASTNode *sem_bus_get_or_create_signal_decl(JZModuleScope *scope,
                                             const char *bus_port_name,
                                             int has_index,
                                             unsigned index,
                                             const char *signal_name,
                                             const JZASTNode *signal_decl)
{
    if (!scope || !bus_port_name || !signal_name) return NULL;

    char name_buf[256];
    if (has_index) {
        snprintf(name_buf, sizeof(name_buf), "%s[%u].%s", bus_port_name, index, signal_name);
    } else {
        snprintf(name_buf, sizeof(name_buf), "%s.%s", bus_port_name, signal_name);
    }

    size_t count = scope->bus_signal_decls.len / sizeof(JZASTNode *);
    JZASTNode **arr = (JZASTNode **)scope->bus_signal_decls.data;
    for (size_t i = 0; i < count; ++i) {
        if (arr[i] && arr[i]->name && strcmp(arr[i]->name, name_buf) == 0) {
            return arr[i];
        }
    }

    JZLocation loc = signal_decl ? signal_decl->loc : (scope->node ? scope->node->loc : (JZLocation){0});
    JZASTNode *node = jz_ast_new(JZ_AST_WIRE_DECL, loc);
    if (!node) return NULL;
    jz_ast_set_name(node, name_buf);
    if (signal_decl && signal_decl->width) {
        jz_ast_set_width(node, signal_decl->width);
    }

    (void)jz_buf_append(&scope->bus_signal_decls, &node, sizeof(node));
    return node;
}




/* Parse a width string that is a simple positive integer literal. This helper is
 * intentionally conservative: any non-digit (after trimming whitespace) causes
 * it to fail so that widths depending on CONST/CONFIG are left to future
 * semantic passes instead of producing false errors here.
 */
int parse_simple_positive_int(const char *s, unsigned *out)
{
    if (!s || !out) return 0;
    unsigned value = 0;
    int saw_digit = 0;
    for (const char *p = s; *p; ++p) {
        if (isspace((unsigned char)*p) || *p == '_') {
            continue;
        }
        if (*p < '0' || *p > '9') {
            return 0;
        }
        unsigned d = (unsigned)(*p - '0');
        if (value > (unsigned)(~0u) / 10u ||
            (value == (unsigned)(~0u) / 10u && d > (unsigned)(~0u) % 10u)) {
            return 0; /* overflow */
        }
        value = value * 10u + d;
        saw_digit = 1;
    }
    if (!saw_digit || value == 0u) {
        return 0;
    }
    *out = value;
    return 1;
}

/* Parse a simple non-negative integer (including zero). Used for indices. */
int parse_simple_nonnegative_int(const char *s, unsigned *out)
{
    if (!s || !out) return 0;
    unsigned value = 0;
    int saw_digit = 0;
    for (const char *p = s; *p; ++p) {
        if (isspace((unsigned char)*p)) {
            continue;
        }
        if (*p < '0' || *p > '9') {
            return 0;
        }
        unsigned d = (unsigned)(*p - '0');
        if (value > (unsigned)(~0u) / 10u ||
            (value == (unsigned)(~0u) / 10u && d > (unsigned)(~0u) % 10u)) {
            return 0; /* overflow */
        }
        value = value * 10u + d;
        saw_digit = 1;
    }
    if (!saw_digit) {
        return 0;
    }
    *out = value;
    return 1;
}

/*
 * Parse an unsigned integer value from either a plain decimal or a sized
 * literal (e.g. 8'd5, 16'hFF, 4'b0011).  Returns 1 on success with *out
 * set, 0 on failure or if the value overflows unsigned.
 */
int parse_literal_unsigned_value(const char *s, unsigned *out)
{
    if (!s || !out) return 0;

    /* Fast path: try plain decimal first. */
    if (parse_simple_nonnegative_int(s, out)) return 1;

    /* Look for tick indicating sized literal: <width>'<base><digits> */
    const char *tick = strchr(s, '\'');
    if (!tick || tick == s) return 0;

    char base_ch = tick[1];
    const char *digits = tick + 2;
    if (!digits || !*digits) return 0;

    unsigned long long acc = 0;
    int saw_digit = 0;

    if (base_ch == 'd' || base_ch == 'D') {
        for (const char *p = digits; *p; ++p) {
            if (*p == '_') continue;
            if (*p < '0' || *p > '9') return 0;
            saw_digit = 1;
            if (acc > (UINT_MAX - (unsigned)(*p - '0')) / 10ULL) return 0;
            acc = acc * 10ULL + (unsigned long long)(*p - '0');
        }
    } else if (base_ch == 'h' || base_ch == 'H') {
        for (const char *p = digits; *p; ++p) {
            if (*p == '_') continue;
            saw_digit = 1;
            unsigned d;
            if (*p >= '0' && *p <= '9')      d = (unsigned)(*p - '0');
            else if (*p >= 'a' && *p <= 'f') d = 10u + (unsigned)(*p - 'a');
            else if (*p >= 'A' && *p <= 'F') d = 10u + (unsigned)(*p - 'A');
            else return 0; /* x/z digits */
            if (acc > (UINT_MAX - d) / 16ULL) return 0;
            acc = acc * 16ULL + d;
        }
    } else if (base_ch == 'b' || base_ch == 'B') {
        for (const char *p = digits; *p; ++p) {
            if (*p == '_') continue;
            saw_digit = 1;
            if (*p != '0' && *p != '1') return 0; /* x/z digits */
            unsigned d = (unsigned)(*p - '0');
            if (acc > (UINT_MAX - d) / 2ULL) return 0;
            acc = acc * 2ULL + d;
        }
    } else {
        return 0;
    }

    if (!saw_digit) return 0;
    if (acc > UINT_MAX) return 0;
    *out = (unsigned)acc;
    return 1;
}

/*
 * Parse a very simple signed integer (optional leading '+'/'-' followed by
 * decimal digits and arbitrary whitespace). This is used in places where we
 * want to recognize obviously non-positive widths like "-1" or "0" while
 * still treating more complex expressions (CONST/CONFIG-based, arithmetic,
 * etc.) as unknown so they can be handled by later constant-eval passes.
 *
 * Returns 1 on success with *out set, 0 if the text is not a simple signed
 * integer.
 */
int parse_simple_signed_int(const char *s, long long *out)
{
    if (!s || !out) return 0;

    const char *p = s;
    /* Skip leading whitespace. */
    while (*p && isspace((unsigned char)*p)) {
        ++p;
    }
    int sign = 1;
    if (*p == '+') {
        ++p;
    } else if (*p == '-') {
        sign = -1;
        ++p;
    }

    long long value = 0;
    int saw_digit = 0;
    for (; *p; ++p) {
        if (isspace((unsigned char)*p)) {
            continue;
        }
        if (*p < '0' || *p > '9') {
            return 0; /* not a pure signed integer token */
        }
        int d = (int)(*p - '0');
        if (value > (LLONG_MAX - d) / 10) {
            return 0; /* overflow */
        }
        value = value * 10 + d;
        saw_digit = 1;
    }
    if (!saw_digit) {
        return 0;
    }

    *out = sign * value;
    return 1;
}

/*
 * Evaluate a very simple positive integer expression used for widths/depths
 * in declarations. This helper deliberately only accepts decimal digits and
 * whitespace so that CONST/CONFIG-based expressions are left for future
 * constant-eval integration.
 *
 * Return values:
 *   1  -> successfully parsed a positive integer > 0 into *out.
 *   0  -> expression is non-simple (contains non-digits) or empty; caller
 *         should treat the value as unknown and avoid emitting errors.
 *  -1  -> expression consists only of digits but is invalid (e.g. 0 or
 *         overflow); callers should report MEM_*_INVALID style errors.
 */
int eval_simple_positive_decl_int(const char *s, unsigned *out)
{
    if (!s) return 0;

    int saw_digit = 0;
    int saw_nondigit = 0;
    for (const char *p = s; *p; ++p) {
        if (isspace((unsigned char)*p) || *p == '_') continue;
        if (*p < '0' || *p > '9') {
            saw_nondigit = 1;
            break;
        }
        saw_digit = 1;
    }
    if (!saw_digit) {
        return 0; /* nothing meaningful */
    }
    if (saw_nondigit) {
        return 0; /* complex expression (CONST/CONFIG/etc.), defer */
    }

    unsigned tmp = 0;
    if (!parse_simple_positive_int(s, &tmp)) {
        return -1; /* digits-only but <= 0 or overflow */
    }
    if (out) *out = tmp;
    return 1;
}

/*
 * Parse a width/depth string that is a single identifier-like token
 * (e.g. CONST name). Returns 1 and copies the identifier into out when
 * successful, 0 otherwise.
 */
int sem_extract_identifier_like(const char *s,
                                       char *out,
                                       size_t out_size)
{
    if (!s || !out || out_size == 0) return 0;

    /* Trim leading/trailing whitespace. */
    const char *start = s;
    while (*start && isspace((unsigned char)*start)) start++;
    const char *end = s + strlen(s);
    while (end > start && isspace((unsigned char)end[-1])) {
        --end;
    }
    if (start >= end) return 0;

    size_t len = (size_t)(end - start);
    if (len >= out_size) return 0;

    /* Must be identifier characters only, and first char not a digit. */
    if (!((*start >= 'A' && *start <= 'Z') ||
          (*start >= 'a' && *start <= 'z') ||
          *start == '_')) {
        return 0;
    }
    for (const char *p = start + 1; p < end; ++p) {
        if (!((*p >= 'A' && *p <= 'Z') ||
              (*p >= 'a' && *p <= 'z') ||
              (*p >= '0' && *p <= '9') ||
              *p == '_')) {
            return 0;
        }
    }

    memcpy(out, start, len);
    out[len] = '\0';
    return 1;
}

/*
 * Lightweight validation of width expressions used in module instantiations.
 *
 * This helper classifies an instantiation width expression as "invalid" when
 * we can prove one of the following from the raw text:
 *   - it is digits-only but non-positive or overflows (eval_simple_positive_decl_int
 *     returns -1), or
 *   - it is a single identifier that does not resolve to a module CONST or
 *     project CONFIG entry, or
 *   - it contains CONFIG.<name> references where <name> is not declared in the
 *     project CONFIG block.
 *
 * More complex expressions that we cannot fully analyse yet are treated as
 * "unknown" but not invalid; they will be covered by future constant-eval
 * integration.
 */

/*
 * Module-level CONST evaluation for simple integer CONSTs.
 *
 * For each CONST block inside a module, we evaluate CONST initializers in
 * declared order using the shared constant-eval library. Earlier CONSTs are
 * substituted as plain decimal integers when referenced by name in later
 * expressions. Any failure (syntax error, undefined reference within the
 * block, negative result, etc.) is reported as CONST_NEGATIVE_OR_NONINT on
 * the corresponding CONST declaration.
 *
 * CONFIG-based expressions (those containing CONFIG.<name>) are evaluated
 * by rewriting CONFIG.<name> to bare <name> and injecting project CONFIG
 * entries (using their pre-evaluated numeric values) into the evaluation
 * environment.
 */
void sem_check_module_const_blocks(const JZModuleScope *scope,
                                          const JZBuffer *project_symbols,
                                          JZDiagnosticList *diagnostics)
{
    if (!scope || !scope->node) return;
    JZASTNode *mod = scope->node;

    for (size_t i = 0; i < mod->child_count; ++i) {
        JZASTNode *blk = mod->children[i];
        if (!blk || blk->type != JZ_AST_CONST_BLOCK) continue;
        if (!blk->block_kind || strcmp(blk->block_kind, "CONST") != 0) continue;

        /* Collect CONST decls in this block. */
        size_t decl_count = 0;
        for (size_t j = 0; j < blk->child_count; ++j) {
            JZASTNode *decl = blk->children[j];
            if (!decl || decl->type != JZ_AST_CONST_DECL || !decl->name) continue;
            ++decl_count;
        }
        if (decl_count == 0) {
            continue;
        }

        JZASTNode **decls = (JZASTNode **)calloc(decl_count, sizeof(JZASTNode *));
        long long *values   = (long long *)calloc(decl_count, sizeof(long long));
        int       *ok       = (int *)calloc(decl_count, sizeof(int));
        char      (*value_strs)[64] = (char (*)[64])calloc(decl_count, sizeof(*value_strs));
        if (!decls || !values || !ok || !value_strs) {
            free(decls);
            free(values);
            free(ok);
            free(value_strs);
            return; /* Out of memory: skip further CONST checks. */
        }

        size_t idx = 0;
        for (size_t j = 0; j < blk->child_count && idx < decl_count; ++j) {
            JZASTNode *decl = blk->children[j];
            if (!decl || decl->type != JZ_AST_CONST_DECL || !decl->name) continue;
            decls[idx] = decl;
            ok[idx] = 0;
            values[idx] = 0;
            ++idx;
        }
        decl_count = idx;
        if (decl_count == 0) {
            free(decls);
            free(values);
            free(ok);
            free(value_strs);
            continue;
        }

        /* --- Pre-pass: batch-evaluate all CONSTs to detect circular deps ---
         * The incremental per-CONST evaluation below can't detect cycles for
         * the first CONST in a chain (its dependency hasn't been added yet).
         * This batch pass puts ALL CONSTs into one environment so that
         * jz_const_eval_all can detect EVAL_VISITING cycles.
         */
        {
            size_t config_count_batch = 0;
            if (project_symbols && project_symbols->data) {
                const JZSymbol *psyms = (const JZSymbol *)project_symbols->data;
                size_t pcount = project_symbols->len / sizeof(JZSymbol);
                for (size_t ci = 0; ci < pcount; ++ci) {
                    if (psyms[ci].kind == JZ_SYM_CONFIG && psyms[ci].node &&
                        psyms[ci].node->name && psyms[ci].node->width) {
                        ++config_count_batch;
                    }
                }
            }
            size_t batch_cap = decl_count + config_count_batch;
            JZConstDef *batch_defs = (JZConstDef *)calloc(batch_cap, sizeof(JZConstDef));
            long long  *batch_vals = (long long *)calloc(batch_cap, sizeof(long long));
            if (batch_defs && batch_vals) {
                size_t bc = 0;
                /* Add all non-string CONSTs with their raw expressions. */
                for (size_t di2 = 0; di2 < decl_count; ++di2) {
                    if (!decls[di2]) continue;
                    if (decls[di2]->block_kind && strcmp(decls[di2]->block_kind, "STRING") == 0) continue;
                    batch_defs[bc].name = decls[di2]->name;
                    batch_defs[bc].expr = decls[di2]->text ? decls[di2]->text : "0";
                    ++bc;
                }
                /* Add CONFIG entries. */
                if (project_symbols && project_symbols->data) {
                    const JZSymbol *psyms = (const JZSymbol *)project_symbols->data;
                    size_t pcount = project_symbols->len / sizeof(JZSymbol);
                    for (size_t ci = 0; ci < pcount; ++ci) {
                        if (psyms[ci].kind != JZ_SYM_CONFIG || !psyms[ci].node ||
                            !psyms[ci].node->name || !psyms[ci].node->width) continue;
                        batch_defs[bc].name = psyms[ci].node->name;
                        batch_defs[bc].expr = psyms[ci].node->width;
                        ++bc;
                    }
                }
                JZConstEvalOptions batch_opts;
                memset(&batch_opts, 0, sizeof(batch_opts));
                int batch_rc = jz_const_eval_all(batch_defs, bc, &batch_opts, batch_vals);
                if (batch_rc == -2) {
                    /* Circular dependency detected.  Mark all CONSTs involved
                     * (those whose batch eval did NOT complete) and report
                     * CONST_CIRCULAR_DEP with the correct source location.
                     * We rely on the fact that jz_const_eval_all stops at the
                     * first cycle, so we check which CONSTs didn't get a value.
                     * Re-run to identify each one individually.
                     */
                    /* Identify which CONSTs are part of cycles by checking
                     * if they can be evaluated without the others. Simple
                     * approach: any CONST whose expression references another
                     * CONST in the same block that also failed is circular.
                     * For simplicity, mark any CONST that failed batch eval
                     * and whose name appears in another failed CONST's expr.
                     */
                    for (size_t di2 = 0; di2 < decl_count; ++di2) {
                        if (!decls[di2]) continue;
                        if (decls[di2]->block_kind && strcmp(decls[di2]->block_kind, "STRING") == 0) continue;
                        const char *dname = decls[di2]->name;
                        const char *dexpr = decls[di2]->text ? decls[di2]->text : "0";
                        /* Check if this CONST's expression references any other
                         * CONST that also references it back (simple cycle check). */
                        for (size_t di3 = 0; di3 < decl_count; ++di3) {
                            if (di3 == di2 || !decls[di3]) continue;
                            if (decls[di3]->block_kind && strcmp(decls[di3]->block_kind, "STRING") == 0) continue;
                            const char *oname = decls[di3]->name;
                            const char *oexpr = decls[di3]->text ? decls[di3]->text : "0";
                            if (strstr(dexpr, oname) && strstr(oexpr, dname)) {
                                /* Mutual reference — mark as circular. */
                                ok[di2] = -1; /* -1 = circular dep */
                                break;
                            }
                        }
                    }
                }
            }
            free(batch_defs);
            free(batch_vals);
        }

        for (size_t di = 0; di < decl_count; ++di) {
            JZASTNode *decl = decls[di];
            if (!decl) continue;

            /* String CONSTs are valid but don't go through numeric eval. */
            if (decl->block_kind && strcmp(decl->block_kind, "STRING") == 0) {
                continue;
            }

            /* If the batch pre-pass identified this CONST as part of a
             * circular dependency, report CONST_CIRCULAR_DEP and skip
             * incremental evaluation.  Keep ok[di] == -1 so the
             * incremental env loop below can exclude circular CONSTs.
             */
            if (ok[di] == -1) {
                if (diagnostics) {
                    sem_report_rule(diagnostics,
                                    decl->loc,
                                    "CONST_CIRCULAR_DEP",
                                    "circular dependency in CONST definitions");
                }
                continue;
            }

            const char *expr_text = decl->text ? decl->text : "0";

            /* First, flag any CONFIG.<name> references that use undeclared
             * CONFIG entries at the project level.
             */
            if (project_symbols) {
                sem_check_undeclared_config_in_width(expr_text,
                                                     decl->loc,
                                                     project_symbols,
                                                     diagnostics);
            }

            /* Forbid GLOBAL.<name> usage in module-level CONST initializers. */
            if (project_symbols && sem_expr_has_global_ref(expr_text, project_symbols)) {
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "CONST '%s' references GLOBAL.<name> which is not allowed\n"
                         "module-level CONSTs may only reference other CONSTs or CONFIG values",
                         decl->name);
                sem_report_rule(diagnostics,
                                decl->loc,
                                "GLOBAL_USED_WHERE_FORBIDDEN",
                                msg);
                continue;
            }

            if (sem_expr_has_lit_call(expr_text)) {
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "CONST '%s' uses lit() which is not allowed\n"
                         "lit() is only valid in signal assignment expressions, not CONST initializers",
                         decl->name);
                sem_report_rule(diagnostics,
                                decl->loc,
                                "LIT_INVALID_CONTEXT",
                                msg);
                continue;
            }

            /* Detect CONFIG.<name> references in the CONST initializer. */
            int has_config_ref = 0;
            {
                const char *p = expr_text;
                int expecting_dot = 0;
                int expecting_name = 0;
                char token[128];

                while (*p && !has_config_ref) {
                    while (*p && isspace((unsigned char)*p)) {
                        ++p;
                    }
                    if (!*p) break;

                    const char *start = p;
                    while (*p && !isspace((unsigned char)*p)) {
                        ++p;
                    }
                    size_t len = (size_t)(p - start);
                    if (len >= sizeof(token)) len = sizeof(token) - 1;
                    memcpy(token, start, len);
                    token[len] = '\0';

                    if (expecting_name) {
                        expecting_name = 0;
                        if (token[0] != '\0') {
                            has_config_ref = 1;
                            break;
                        }
                        continue;
                    }

                    if (expecting_dot) {
                        expecting_dot = 0;
                        if (strcmp(token, ".") == 0) {
                            expecting_name = 1;
                        }
                        continue;
                    }

                    if (strcmp(token, "CONFIG") == 0) {
                        expecting_dot = 1;
                        continue;
                    }
                }
            }

            /* If the expression references CONFIG.<name> but any referenced
             * name has no evaluated value (e.g. undeclared CONFIG), skip
             * evaluation. The CONFIG_USE_UNDECLARED diagnostic is already
             * emitted above.
             */
            if (has_config_ref) {
                int all_resolved = 1;
                const char *p2 = expr_text;
                while (*p2 && all_resolved) {
                    if (p2[0] == 'C' && strncmp(p2, "CONFIG", 6) == 0) {
                        if (p2 != expr_text) {
                            char prev = p2[-1];
                            if ((prev >= 'A' && prev <= 'Z') ||
                                (prev >= 'a' && prev <= 'z') ||
                                (prev >= '0' && prev <= '9') ||
                                prev == '_') {
                                ++p2;
                                continue;
                            }
                        }
                        const char *q2 = p2 + 6;
                        while (*q2 && isspace((unsigned char)*q2)) ++q2;
                        if (*q2 != '.') { ++p2; continue; }
                        ++q2;
                        while (*q2 && isspace((unsigned char)*q2)) ++q2;
                        const char *ns = q2;
                        while ((*q2 >= 'A' && *q2 <= 'Z') ||
                               (*q2 >= 'a' && *q2 <= 'z') ||
                               (*q2 >= '0' && *q2 <= '9') ||
                               *q2 == '_') {
                            ++q2;
                        }
                        size_t nlen = (size_t)(q2 - ns);
                        if (nlen == 0) { ++p2; continue; }
                        /* Check that this CONFIG name has an evaluated value. */
                        int found = 0;
                        if (project_symbols && project_symbols->data) {
                            const JZSymbol *psyms = (const JZSymbol *)project_symbols->data;
                            size_t pcount = project_symbols->len / sizeof(JZSymbol);
                            for (size_t ci = 0; ci < pcount; ++ci) {
                                if (psyms[ci].kind == JZ_SYM_CONFIG && psyms[ci].node &&
                                    psyms[ci].node->name && psyms[ci].node->width &&
                                    strlen(psyms[ci].node->name) == nlen &&
                                    strncmp(psyms[ci].node->name, ns, nlen) == 0) {
                                    found = 1;
                                    break;
                                }
                            }
                        }
                        if (!found) { all_resolved = 0; }
                        p2 = q2;
                    } else {
                        ++p2;
                    }
                }
                if (!all_resolved) {
                    continue;
                }
            }

            /* CLOG2_*: detect obviously invalid uses with literal arguments
             * before invoking the general constant evaluator so that callers
             * see rule-specific diagnostics.
             */
            if (sem_check_clog2_expr_simple(expr_text, decl->loc, diagnostics)) {
                ok[di] = 0;
                continue;
            }

            /* If this CONST initializer contains a sized literal whose width is
             * a simple non-positive decimal (e.g. 0'h0, -1'h0), report
             * LIT_WIDTH_NOT_POSITIVE on the CONST declaration and skip generic
             * CONST_NEGATIVE_OR_NONINT so the more specific literal-width
             * diagnostic is visible.
             */
            if (sem_expr_has_nonpositive_simple_width_literal(expr_text)) {
                if (diagnostics) {
                    char msg[512];
                    snprintf(msg, sizeof(msg),
                             "CONST '%s' contains a literal with non-positive width\n"
                             "in expression: %.*s",
                             decl->name, 200, expr_text);
                    sem_report_rule(diagnostics,
                                    decl->loc,
                                    "LIT_WIDTH_NOT_POSITIVE",
                                    msg);
                }
                ok[di] = 0;
                continue;
            }

            /* If this CONST initializer contains an identifier-based literal
             * width NAME'... where NAME is neither a module-level CONST nor a
             * project-level CONFIG, we intentionally skip constant-evaluating
             * it here. The literal-width pass will report
             * LIT_UNDEFINED_CONST_WIDTH on the literal itself; emitting
             * CONST_NEGATIVE_OR_NONINT on the CONST in addition would be
             * redundant and less specific.
             */
            if (sem_expr_has_undefined_width_ident(expr_text, scope, project_symbols)) {
                if (diagnostics) {
                    char msg[512];
                    snprintf(msg, sizeof(msg),
                             "CONST '%s' contains a sized literal whose width name is undefined\n"
                             "in expression: %.*s",
                             decl->name, 200, expr_text);
                    sem_report_rule(diagnostics,
                                    decl->loc,
                                    "LIT_UNDEFINED_CONST_WIDTH",
                                    msg);
                }
                ok[di] = 0;
                continue;
            }

            /* Build a small evaluation environment consisting of previously
             * successful CONSTs (as numeric literals), project CONFIG entries
             * (when the expression references CONFIG.<name>), and the current
             * definition.
             */

            /* Count CONFIG entries to include in the environment. */
            size_t config_count = 0;
            if (has_config_ref && project_symbols && project_symbols->data) {
                const JZSymbol *psyms = (const JZSymbol *)project_symbols->data;
                size_t pcount = project_symbols->len / sizeof(JZSymbol);
                for (size_t ci = 0; ci < pcount; ++ci) {
                    if (psyms[ci].kind == JZ_SYM_CONFIG && psyms[ci].node &&
                        psyms[ci].node->name && psyms[ci].node->width) {
                        ++config_count;
                    }
                }
            }

            size_t env_cap = di + 1 + config_count;
            JZConstDef *defs = (JZConstDef *)calloc(env_cap, sizeof(JZConstDef));
            long long  *env_values = (long long *)calloc(env_cap, sizeof(long long));
            if (!defs || !env_values) {
                free(defs);
                free(env_values);
                break;
            }

            size_t env_count = 0;
            for (size_t pj = 0; pj < di; ++pj) {
                if (ok[pj] == -1) continue; /* Skip circular CONSTs. */
                if (!ok[pj]) continue;      /* Skip other failed CONSTs. */
                snprintf(value_strs[pj], sizeof(value_strs[pj]), "%lld", values[pj]);
                defs[env_count].name = decls[pj]->name;
                defs[env_count].expr = value_strs[pj];
                ++env_count;
            }

            /* Inject evaluated CONFIG entries as bare names so that
             * rewritten expressions can reference them.
             */
            if (has_config_ref && project_symbols && project_symbols->data) {
                const JZSymbol *psyms = (const JZSymbol *)project_symbols->data;
                size_t pcount = project_symbols->len / sizeof(JZSymbol);
                for (size_t ci = 0; ci < pcount; ++ci) {
                    if (psyms[ci].kind != JZ_SYM_CONFIG || !psyms[ci].node ||
                        !psyms[ci].node->name || !psyms[ci].node->width) {
                        continue;
                    }
                    defs[env_count].name = psyms[ci].node->name;
                    defs[env_count].expr = psyms[ci].node->width; /* evaluated numeric string */
                    ++env_count;
                }
            }

            /* If the expression contains CONFIG.<name>, rewrite it to strip
             * the CONFIG. prefix so that the const-eval engine can resolve
             * bare names against the CONFIG entries added above.
             */
            char *rewritten_expr = NULL;
            const char *eval_text = expr_text;
            if (has_config_ref) {
                size_t len = strlen(expr_text);
                rewritten_expr = (char *)malloc(len + 1);
                if (!rewritten_expr) {
                    free(defs);
                    free(env_values);
                    break;
                }
                size_t out_len = 0;
                const char *p = expr_text;
                while (*p) {
                    if (p[0] == 'C' && strncmp(p, "CONFIG", 6) == 0) {
                        /* Require identifier boundary before CONFIG. */
                        if (p != expr_text) {
                            char prev = p[-1];
                            if ((prev >= 'A' && prev <= 'Z') ||
                                (prev >= 'a' && prev <= 'z') ||
                                (prev >= '0' && prev <= '9') ||
                                prev == '_') {
                                rewritten_expr[out_len++] = *p++;
                                continue;
                            }
                        }
                        const char *q = p + 6; /* past CONFIG */
                        while (*q && isspace((unsigned char)*q)) {
                            ++q;
                        }
                        if (*q != '.') {
                            rewritten_expr[out_len++] = *p++;
                            continue;
                        }
                        ++q; /* skip '.' */
                        while (*q && isspace((unsigned char)*q)) {
                            ++q;
                        }
                        if (!((*q >= 'A' && *q <= 'Z') ||
                              (*q >= 'a' && *q <= 'z') ||
                              *q == '_')) {
                            rewritten_expr[out_len++] = *p++;
                            continue;
                        }
                        const char *name_start = q;
                        while ((*q >= 'A' && *q <= 'Z') ||
                               (*q >= 'a' && *q <= 'z') ||
                               (*q >= '0' && *q <= '9') ||
                               *q == '_') {
                            ++q;
                        }
                        size_t name_len = (size_t)(q - name_start);
                        memcpy(rewritten_expr + out_len, name_start, name_len);
                        out_len += name_len;
                        p = q;
                        continue;
                    }
                    rewritten_expr[out_len++] = *p++;
                }
                rewritten_expr[out_len] = '\0';
                eval_text = rewritten_expr;
            }

            /* Anonymous expression for this CONST; expand any widthof()
             * occurrences before feeding to the generic constant evaluator so
             * that widthof() is visible here too.
             */
            char *expanded_expr = NULL;
            if (sem_expand_widthof_in_width_expr_diag(eval_text,
                                                      scope,
                                                      project_symbols,
                                                      &expanded_expr,
                                                      0,
                                                      diagnostics,
                                                      decl->loc) != 0) {
                free(rewritten_expr);
                free(defs);
                free(env_values);
                break;
            }

            size_t current_index = env_count;
            defs[env_count].name = decl->name;
            defs[env_count].expr = expanded_expr ? expanded_expr : eval_text;
            ++env_count;

            JZConstEvalOptions opts;
            memset(&opts, 0, sizeof(opts));
            /* We intentionally suppress low-level CONST00x diagnostics here and
             * instead map any failure to the rule-based
             * CONST_NEGATIVE_OR_NONINT diagnostic.  However, we do NOT suppress
             * CONST_CIRCULAR_DEP — jz_const_eval_all returns -2 for that case
             * so we can emit it with the correct source location.
             */
            int rc = jz_const_eval_all(defs, env_count, &opts, env_values);
            if (expanded_expr) {
                free(expanded_expr);
            }
            if (rc != 0) {
                if (rc == -2) {
                    /* Circular dependency detected — emit with correct location. */
                    if (diagnostics) {
                        sem_report_rule(diagnostics,
                                        decl->loc,
                                        "CONST_CIRCULAR_DEP",
                                        "circular dependency in CONST definitions");
                    }
                } else if (diagnostics && !sem_expr_has_nonpositive_simple_width_literal(expr_text)) {
                    /* If we already flagged this initializer for non-positive literal
                     * width, do not also emit a generic CONST_NEGATIVE_OR_NONINT
                     * diagnostic on the same CONST.
                     */
                    char msg[512];
                    snprintf(msg, sizeof(msg),
                             "CONST '%s' = %.*s does not evaluate to a nonnegative integer\n"
                             "CONST values must be compile-time constant nonnegative integers",
                             decl->name, 200, expr_text);
                    sem_report_rule(diagnostics,
                                    decl->loc,
                                    "CONST_NEGATIVE_OR_NONINT",
                                    msg);
                }
                ok[di] = 0;
            } else {
                ok[di] = 1;
                values[di] = env_values[current_index];
            }

            free(rewritten_expr);
            free(defs);
            free(env_values);
        }

        free(decls);
        free(values);
        free(ok);
        free(value_strs);
    }
}


/* -------------------------------------------------------------------------
 *  String CONST/CONFIG resolution
 * -------------------------------------------------------------------------
 */

int sem_resolve_string_const(const char *name,
                             const JZModuleScope *scope,
                             const JZBuffer *project_symbols,
                             const char **out_str,
                             JZDiagnosticList *diagnostics,
                             JZLocation loc)
{
    if (!name || !out_str) return 0;

    /* Check for CONFIG.NAME references. */
    if (strncmp(name, "CONFIG.", 7) == 0) {
        const char *cfg_name = name + 7;
        if (project_symbols && project_symbols->data) {
            const JZSymbol *psyms = (const JZSymbol *)project_symbols->data;
            size_t pcount = project_symbols->len / sizeof(JZSymbol);
            for (size_t i = 0; i < pcount; ++i) {
                if (psyms[i].kind == JZ_SYM_CONFIG &&
                    psyms[i].node && psyms[i].node->name &&
                    strcmp(psyms[i].node->name, cfg_name) == 0) {
                    if (psyms[i].node->block_kind &&
                        strcmp(psyms[i].node->block_kind, "STRING") == 0) {
                        *out_str = psyms[i].node->text;
                        return 1;
                    }
                    /* Found but not a string. */
                    if (diagnostics) {
                        char msg[512];
                        snprintf(msg, sizeof(msg),
                                 "CONFIG.%s is a numeric value, but a string is required here\n"
                                 "declare it as STRING CONFIG in the project CONFIG block",
                                 cfg_name);
                        sem_report_rule(diagnostics, loc,
                                        "CONST_NUMERIC_IN_STRING_CONTEXT",
                                        msg);
                    }
                    return 0;
                }
            }
        }
        return 0;
    }

    /* Module-scope CONST lookup. */
    if (scope) {
        size_t sym_count = scope->symbols.len / sizeof(JZSymbol);
        const JZSymbol *syms = (const JZSymbol *)scope->symbols.data;
        for (size_t i = 0; i < sym_count; ++i) {
            if (syms[i].kind == JZ_SYM_CONST &&
                syms[i].node && syms[i].node->name &&
                strcmp(syms[i].node->name, name) == 0) {
                if (syms[i].node->block_kind &&
                    strcmp(syms[i].node->block_kind, "STRING") == 0) {
                    *out_str = syms[i].node->text;
                    return 1;
                }
                /* Found but not a string. */
                if (diagnostics) {
                    char msg[512];
                    snprintf(msg, sizeof(msg),
                             "CONST '%s' is a numeric value, but a string is required here\n"
                             "declare it with STRING CONST to use as a string",
                             name);
                    sem_report_rule(diagnostics, loc,
                                    "CONST_NUMERIC_IN_STRING_CONTEXT",
                                    msg);
                }
                return 0;
            }
        }
    }

    return 0;
}


/* -------------------------------------------------------------------------
 *  General CONST/CONFIG integer and width evaluation helpers
 * -------------------------------------------------------------------------
 */

int sem_eval_const_expr_in_module(const char *expr,
                                  const JZModuleScope *scope,
                                  const JZBuffer *project_symbols,
                                  long long *out_value)
{
    if (!expr || !scope || !scope->node || !out_value) {
        return -1;
    }

    /* Build an environment of named CONST (module) and CONFIG (project) entries
     * that may be referenced by the expression.
     */
    JZASTNode *mod = scope->node;

    size_t named_count = 0;
    {
        size_t sym_count = scope->symbols.len / sizeof(JZSymbol);
        JZSymbol *syms = (JZSymbol *)scope->symbols.data;
        for (size_t i = 0; i < sym_count; ++i) {
            if (syms[i].kind == JZ_SYM_CONST && syms[i].node && syms[i].node->text) {
                /* Skip string CONSTs — they cannot participate in numeric eval. */
                if (syms[i].node->block_kind && strcmp(syms[i].node->block_kind, "STRING") == 0)
                    continue;
                ++named_count;
            }
        }
        if (project_symbols && project_symbols->data) {
            const JZSymbol *psyms = (const JZSymbol *)project_symbols->data;
            size_t pcount = project_symbols->len / sizeof(JZSymbol);
            for (size_t i = 0; i < pcount; ++i) {
                if (psyms[i].kind == JZ_SYM_CONFIG && psyms[i].node &&
                    psyms[i].node->name &&
                    (psyms[i].node->width || psyms[i].node->text)) {
                    /* Skip string CONFIGs. */
                    if (psyms[i].node->block_kind && strcmp(psyms[i].node->block_kind, "STRING") == 0)
                        continue;
                    ++named_count;
                }
            }
        }
    }

    size_t total = named_count + 1; /* +1 for the anonymous expression */
    if (total == 0) {
        /* Expand widthof() in the anonymous expression before calling the raw
         * constant-expression evaluator so that intrinsic width queries are
         * honored here as well.
         */
        char *expanded = NULL;
        if (sem_expand_widthof_in_width_expr(expr,
                                             scope,
                                             project_symbols,
                                             &expanded,
                                             0) != 0) {
            if (expanded) free(expanded);
            return -1;
        }
        const char *to_eval = expanded ? expanded : expr;
        int rc = jz_const_eval_expr(to_eval, NULL, out_value);
        if (expanded) free(expanded);
        return rc;
    }

    JZConstDef *defs = (JZConstDef *)calloc(total, sizeof(JZConstDef));
    long long  *vals = (long long *)calloc(total, sizeof(long long));
    if (!defs || !vals) {
        free(defs);
        free(vals);
        return -1;
    }

    /* Track dynamically allocated expression strings that we need to free
     * after the const evaluator finishes.
     */
    char **owned_exprs = (char **)calloc(total, sizeof(char *));
    if (!owned_exprs) {
        free(defs);
        free(vals);
        return -1;
    }

    size_t idx = 0;
    /* Module-level CONST definitions. */
    if (named_count > 0) {
        size_t sym_count = scope->symbols.len / sizeof(JZSymbol);
        JZSymbol *syms = (JZSymbol *)scope->symbols.data;
        for (size_t i = 0; i < sym_count; ++i) {
            if (syms[i].kind != JZ_SYM_CONST || !syms[i].node || !syms[i].node->name || !syms[i].node->text) {
                continue;
            }
            /* Skip string CONSTs — they cannot participate in numeric eval. */
            if (syms[i].node->block_kind && strcmp(syms[i].node->block_kind, "STRING") == 0) {
                continue;
            }
            defs[idx].name = syms[i].node->name;
            defs[idx].expr = syms[i].node->text;

            /* Skip self-referential CONST defs where the expression is
             * just the CONST's own name.  This happens when OVERRIDE
             * propagation (Pass 2b) replaces the CONST node with the
             * OVERRIDE AST node (e.g. OVERRIDE { XLEN = XLEN; }).
             * The actual value will come from a CONFIG or parent-scope def.
             */
            {
                const char *se = syms[i].node->text;
                const char *sp = se;
                while (*sp && isspace((unsigned char)*sp)) ++sp;
                const char *ep = sp;
                while (*ep && ((*ep >= 'A' && *ep <= 'Z') || (*ep >= 'a' && *ep <= 'z') ||
                               (*ep >= '0' && *ep <= '9') || *ep == '_')) ++ep;
                const char *tp = ep;
                while (*tp && isspace((unsigned char)*tp)) ++tp;
                if (*tp == '\0' && (size_t)(ep - sp) > 0) {
                    size_t ilen = (size_t)(ep - sp);
                    if (syms[i].node->name &&
                        strlen(syms[i].node->name) == ilen &&
                        strncmp(sp, syms[i].node->name, ilen) == 0) {
                        continue;
                    }
                }
            }

            /* Strip "CONFIG." / "CONFIG . " prefixes from CONST def
             * expressions.  This can happen when OVERRIDE propagation
             * (Pass 2b) replaces the CONST node with the OVERRIDE AST
             * node whose text contains parent-scope CONFIG references.
             */
            {
                const char *ce = syms[i].node->text;
                if (ce && strstr(ce, "CONFIG")) {
                    size_t celen = strlen(ce);
                    char *cebuf = (char *)malloc(celen + 1);
                    if (cebuf) {
                        const char *cp = ce;
                        size_t co = 0;
                        while (*cp) {
                            if (cp[0] == 'C' && strncmp(cp, "CONFIG", 6) == 0) {
                                int ws = (cp == ce) ||
                                    !((cp[-1] >= 'A' && cp[-1] <= 'Z') ||
                                      (cp[-1] >= 'a' && cp[-1] <= 'z') ||
                                      (cp[-1] >= '0' && cp[-1] <= '9') ||
                                      cp[-1] == '_');
                                if (ws) {
                                    const char *rr = cp + 6;
                                    while (*rr && isspace((unsigned char)*rr)) ++rr;
                                    if (*rr == '.') {
                                        ++rr;
                                        while (*rr && isspace((unsigned char)*rr)) ++rr;
                                        cp = rr;
                                        continue;
                                    }
                                }
                            }
                            cebuf[co++] = *cp++;
                        }
                        cebuf[co] = '\0';

                        /* If after stripping CONFIG., the expression is just
                         * the CONST's own name, it's self-referential (e.g.
                         * OVERRIDE { XLEN = CONFIG.XLEN; } made the module
                         * CONST "XLEN" have expr "CONFIG.XLEN" → "XLEN").
                         * Skip this def; the CONFIG entry provides the value.
                         */
                        if (defs[idx].name && strcmp(cebuf, defs[idx].name) == 0) {
                            free(cebuf);
                            continue;
                        }

                        owned_exprs[idx] = cebuf;
                        defs[idx].expr = cebuf;
                    }
                }
            }

            /* If the expression contains widthof(), resolve it directly
             * without going through the full const-eval chain (which would
             * cause infinite recursion).  We look up the target signal's
             * width and resolve it using only CONFIG defs.
             */
            const char *raw = syms[i].node->text;
            const char *wp = raw;
            while (*wp && isspace((unsigned char)*wp)) ++wp;
            if (strncmp(wp, "widthof", 7) == 0) {
                /* Parse widthof(<ident>) */
                const char *q = wp + 7;
                while (*q && isspace((unsigned char)*q)) ++q;
                if (*q == '(') {
                    ++q;
                    while (*q && isspace((unsigned char)*q)) ++q;
                    const char *id_start = q;
                    while ((*q >= 'A' && *q <= 'Z') || (*q >= 'a' && *q <= 'z') ||
                           (*q >= '0' && *q <= '9') || *q == '_') {
                        ++q;
                    }
                    size_t id_len = (size_t)(q - id_start);
                    if (id_len > 0 && id_len < 256) {
                        char ident[256];
                        memcpy(ident, id_start, id_len);
                        ident[id_len] = '\0';

                        /* Look up the signal to get its width text. */
                        const JZSymbol *target = NULL;
                        JZSymbol *msyms = (JZSymbol *)scope->symbols.data;
                        size_t msym_count = scope->symbols.len / sizeof(JZSymbol);
                        for (size_t si = 0; si < msym_count; ++si) {
                            if ((msyms[si].kind == JZ_SYM_WIRE ||
                                 msyms[si].kind == JZ_SYM_REGISTER ||
                                 msyms[si].kind == JZ_SYM_PORT) &&
                                msyms[si].name && strcmp(msyms[si].name, ident) == 0) {
                                target = &msyms[si];
                                break;
                            }
                        }
                        if (target && target->node && target->node->width) {
                            /* Try to evaluate the width using only CONFIG defs
                             * (no module CONST defs to avoid recursion). */
                            const char *wtext = target->node->width;
                            /* Build a small CONFIG-only def table. */
                            size_t cfg_count = 0;
                            if (project_symbols && project_symbols->data) {
                                const JZSymbol *psyms = (const JZSymbol *)project_symbols->data;
                                size_t pcount = project_symbols->len / sizeof(JZSymbol);
                                for (size_t pi = 0; pi < pcount; ++pi) {
                                    if (psyms[pi].kind == JZ_SYM_CONFIG && psyms[pi].node && psyms[pi].node->name)
                                        ++cfg_count;
                                }
                            }
                            size_t cfg_total = cfg_count + 1;
                            JZConstDef *cfg_defs = (JZConstDef *)calloc(cfg_total, sizeof(JZConstDef));
                            long long *cfg_vals = (long long *)calloc(cfg_total, sizeof(long long));
                            if (cfg_defs && cfg_vals) {
                                size_t ci = 0;
                                if (project_symbols && project_symbols->data) {
                                    const JZSymbol *psyms = (const JZSymbol *)project_symbols->data;
                                    size_t pcount = project_symbols->len / sizeof(JZSymbol);
                                    for (size_t pi = 0; pi < pcount; ++pi) {
                                        if (psyms[pi].kind == JZ_SYM_CONFIG && psyms[pi].node && psyms[pi].node->name) {
                                            cfg_defs[ci].name = psyms[pi].node->name;
                                            cfg_defs[ci].expr = psyms[pi].node->width
                                                                ? psyms[pi].node->width
                                                                : psyms[pi].node->text;
                                            ++ci;
                                        }
                                    }
                                }
                                /* Strip CONFIG. prefix from width text. */
                                char wbuf[256];
                                {
                                    const char *sp = wtext;
                                    size_t wo = 0;
                                    while (*sp && wo < sizeof(wbuf) - 1) {
                                        if (sp[0] == 'C' && strncmp(sp, "CONFIG", 6) == 0) {
                                            int ws = (sp == wtext) ||
                                                !((sp[-1] >= 'A' && sp[-1] <= 'Z') ||
                                                  (sp[-1] >= 'a' && sp[-1] <= 'z') ||
                                                  (sp[-1] >= '0' && sp[-1] <= '9') ||
                                                  sp[-1] == '_');
                                            if (ws) {
                                                const char *rr = sp + 6;
                                                while (*rr && isspace((unsigned char)*rr)) ++rr;
                                                if (*rr == '.') {
                                                    ++rr;
                                                    while (*rr && isspace((unsigned char)*rr)) ++rr;
                                                    sp = rr;
                                                    continue;
                                                }
                                            }
                                        }
                                        wbuf[wo++] = *sp++;
                                    }
                                    wbuf[wo] = '\0';
                                }
                                cfg_defs[ci].name = NULL;
                                cfg_defs[ci].expr = wbuf;
                                if (jz_const_eval_all(cfg_defs, cfg_total, NULL, cfg_vals) == 0 &&
                                    cfg_vals[cfg_total - 1] > 0) {
                                    char num_buf[32];
                                    snprintf(num_buf, sizeof(num_buf), "%lld", cfg_vals[cfg_total - 1]);
                                    owned_exprs[idx] = strdup(num_buf);
                                    if (owned_exprs[idx]) {
                                        defs[idx].expr = owned_exprs[idx];
                                    }
                                }
                            }
                            free(cfg_defs);
                            free(cfg_vals);
                        }
                    }
                }
            }
            ++idx;
        }
        /* Project-level CONFIG entries.  Prefer the pre-evaluated numeric
         * value (stored in node->width by sem_check_project_config) over the
         * original expression (node->text), since the original may contain
         * CONFIG. self-references that the flat const-eval cannot resolve.
         */
        if (project_symbols && project_symbols->data) {
            const JZSymbol *psyms = (const JZSymbol *)project_symbols->data;
            size_t pcount = project_symbols->len / sizeof(JZSymbol);
            for (size_t i = 0; i < pcount; ++i) {
                if (psyms[i].kind != JZ_SYM_CONFIG || !psyms[i].node ||
                    !psyms[i].node->name) {
                    continue;
                }
                /* Skip string CONFIGs. */
                if (psyms[i].node->block_kind && strcmp(psyms[i].node->block_kind, "STRING") == 0) {
                    continue;
                }
                const char *val = psyms[i].node->width
                                    ? psyms[i].node->width
                                    : psyms[i].node->text;
                if (!val) continue;
                defs[idx].name = psyms[i].node->name;
                defs[idx].expr = val;
                ++idx;
            }
        }
    }

    /* Adjust total in case self-referential CONST defs were skipped above.
     * idx now holds the number of named defs actually added; +1 for the
     * anonymous expression.
     */
    total = idx + 1;

    /* Anonymous expression to evaluate: expand widthof() before handing it to
     * the constant evaluator.
     */
    char *expanded = NULL;
    if (sem_expand_widthof_in_width_expr(expr,
                                         scope,
                                         project_symbols,
                                         &expanded,
                                         0) != 0) {
        for (size_t fi = 0; fi < total; ++fi) { if (owned_exprs[fi]) free(owned_exprs[fi]); }
        free(owned_exprs);
        free(defs);
        free(vals);
        if (expanded) free(expanded);
        return -1;
    }
    const char *anon_expr = expanded ? expanded : expr;

    /* Strip "CONFIG ." / "CONFIG." prefixes so the flat constant evaluator
     * can resolve CONFIG references by their bare name (DATA_WIDTH rather
     * than CONFIG.DATA_WIDTH).  The CONFIG entries are already registered
     * as defs with their bare names above.
     */
    char *stripped = NULL;
    {
        const char *p = anon_expr;
        size_t slen = strlen(p);
        stripped = (char *)malloc(slen + 1);
        if (!stripped) {
            for (size_t fi = 0; fi < total; ++fi) { if (owned_exprs[fi]) free(owned_exprs[fi]); }
            free(owned_exprs);
            free(defs);
            free(vals);
            if (expanded) free(expanded);
            return -1;
        }
        size_t out = 0;
        while (*p) {
            /* Match "CONFIG" followed by optional whitespace and "." */
            if (p[0] == 'C' && strncmp(p, "CONFIG", 6) == 0) {
                /* Ensure it's a word boundary (not part of a larger identifier). */
                int word_start = (p == anon_expr) ||
                    !((p[-1] >= 'A' && p[-1] <= 'Z') ||
                      (p[-1] >= 'a' && p[-1] <= 'z') ||
                      (p[-1] >= '0' && p[-1] <= '9') ||
                      p[-1] == '_');
                if (word_start) {
                    const char *q = p + 6;
                    while (*q && isspace((unsigned char)*q)) ++q;
                    if (*q == '.') {
                        ++q;
                        while (*q && isspace((unsigned char)*q)) ++q;
                        /* Skip "CONFIG . " prefix; copy remainder. */
                        p = q;
                        continue;
                    }
                }
            }
            stripped[out++] = *p++;
        }
        stripped[out] = '\0';
        anon_expr = stripped;
    }

    defs[idx].name = NULL;
    defs[idx].expr = anon_expr;

    JZConstEvalOptions opts;
    memset(&opts, 0, sizeof(opts));
    if (mod && mod->loc.filename) {
        opts.filename = mod->loc.filename;
    }

    int rc = jz_const_eval_all(defs, total, &opts, vals);
    if (rc == 0) {
        *out_value = vals[total - 1];
        if (expanded) free(expanded);
        if (stripped) free(stripped);
        for (size_t fi = 0; fi < total; ++fi) { if (owned_exprs[fi]) free(owned_exprs[fi]); }
        free(owned_exprs);
        free(defs);
        free(vals);
        return 0;
    }

    /* Batch evaluation failed (e.g. a CONST has an invalid expression like
     * lit()).  Retry evaluating just the anonymous expression alone — it may
     * not reference any of the poisoned CONSTs.
     */
    long long fallback_val = 0;
    int fb_rc = jz_const_eval_expr(anon_expr, NULL, &fallback_val);
    if (expanded) free(expanded);
    if (stripped) free(stripped);
    for (size_t fi = 0; fi < total; ++fi) { if (owned_exprs[fi]) free(owned_exprs[fi]); }
    free(owned_exprs);
    free(defs);
    free(vals);
    if (fb_rc == 0) {
        *out_value = fallback_val;
        return 0;
    }
    return -1;
}

/*
 * Evaluate a constant expression in project scope using the CONFIG table as
 * the environment. This is used for project-level @check conditions.
 */
int sem_eval_const_expr_in_project(const char *expr,
                                   const JZBuffer *project_symbols,
                                   long long *out_value)
{
    if (!expr || !out_value) {
        return -1;
    }

    /* Rewrite CONFIG.<name> tokens to bare <name> so that the constant-eval
     * engine can resolve them against CONFIG definitions.
     */
    const char *expr_to_use = expr;
    char *owned_expr = NULL;
    if (project_symbols && project_symbols->data) {
        size_t len = strlen(expr);
        owned_expr = (char *)malloc(len + 1);
        if (!owned_expr) {
            return -1;
        }
        size_t out_len = 0;
        const char *p = expr;
        while (*p) {
            if (p[0] == 'C' && strncmp(p, "CONFIG", 6) == 0) {
                /* Require identifier boundary before CONFIG. */
                if (p != expr) {
                    char prev = p[-1];
                    if ((prev >= 'A' && prev <= 'Z') ||
                        (prev >= 'a' && prev <= 'z') ||
                        (prev >= '0' && prev <= '9') ||
                        prev == '_') {
                        /* Part of a larger identifier; treat as normal text. */
                        owned_expr[out_len++] = *p++;
                        continue;
                    }
                }
                const char *q = p + 6; /* past CONFIG */
                while (*q && isspace((unsigned char)*q)) {
                    ++q;
                }
                if (*q != '.') {
                    /* Not actually CONFIG.<name>; copy 'C' and continue. */
                    owned_expr[out_len++] = *p++;
                    continue;
                }
                ++q; /* skip '.' */
                while (*q && isspace((unsigned char)*q)) {
                    ++q;
                }
                if (!((*q >= 'A' && *q <= 'Z') ||
                      (*q >= 'a' && *q <= 'z') ||
                      *q == '_')) {
                    /* Malformed; fall back to literal copy of 'C'. */
                    owned_expr[out_len++] = *p++;
                    continue;
                }
                const char *name_start = q;
                while ((*q >= 'A' && *q <= 'Z') ||
                       (*q >= 'a' && *q <= 'z') ||
                       (*q >= '0' && *q <= '9') ||
                       *q == '_') {
                    ++q;
                }
                size_t name_len = (size_t)(name_start - q);
                name_len = (size_t)(q - name_start);
                memcpy(owned_expr + out_len, name_start, name_len);
                out_len += name_len;
                p = q;
                continue;
            }
            owned_expr[out_len++] = *p++;
        }
        owned_expr[out_len] = '\0';
        expr_to_use = owned_expr;
    }

    size_t named_count = 0;
    if (project_symbols && project_symbols->data) {
        const JZSymbol *syms = (const JZSymbol *)project_symbols->data;
        size_t count = project_symbols->len / sizeof(JZSymbol);
        for (size_t i = 0; i < count; ++i) {
            if (syms[i].kind == JZ_SYM_CONFIG && syms[i].node &&
                syms[i].node->name && syms[i].node->text) {
                ++named_count;
            }
        }
    }

    size_t total = named_count + 1u; /* +1 for anonymous expression */
    if (total == 1u) {
        /* No CONFIG entries; fall back to plain expression evaluation. */
        int rc = jz_const_eval_expr(expr_to_use, NULL, out_value);
        if (owned_expr) free(owned_expr);
        return rc;
    }

    JZConstDef *defs = (JZConstDef *)calloc(total, sizeof(JZConstDef));
    long long  *vals = (long long *)calloc(total, sizeof(long long));
    if (!defs || !vals) {
        free(defs);
        free(vals);
        if (owned_expr) free(owned_expr);
        return -1;
    }

    size_t idx = 0;
    if (project_symbols && project_symbols->data) {
        const JZSymbol *syms = (const JZSymbol *)project_symbols->data;
        size_t count = project_symbols->len / sizeof(JZSymbol);
        for (size_t i = 0; i < count; ++i) {
            const JZSymbol *s = &syms[i];
            if (s->kind != JZ_SYM_CONFIG || !s->node ||
                !s->node->name || !s->node->text) {
                continue;
            }
            defs[idx].name = s->node->name;
            defs[idx].expr = s->node->text;
            ++idx;
        }
    }

    defs[idx].name = NULL;
    defs[idx].expr = expr_to_use;

    JZConstEvalOptions opts;
    memset(&opts, 0, sizeof(opts));

    int rc = jz_const_eval_all(defs, total, &opts, vals);
    if (owned_expr) free(owned_expr);
    if (rc != 0) {
        free(defs);
        free(vals);
        return -1;
    }

    *out_value = vals[total - 1u];
    free(defs);
    free(vals);
    return 0;
}

/* Internal helper used by width-expression evaluation. Depth is used to
 * prevent unbounded recursion when widthof() appears (directly or
 * indirectly) inside signal width declarations.
 */
int sem_expr_has_lit_call(const char *expr_text)
{
    if (!expr_text) return 0;
    const char *p = expr_text;
    while ((p = strstr(p, "lit")) != NULL) {
        if (p != expr_text) {
            char prev = p[-1];
            if ((prev >= 'A' && prev <= 'Z') ||
                (prev >= 'a' && prev <= 'z') ||
                (prev >= '0' && prev <= '9') ||
                prev == '_') {
                p += 3;
                continue;
            }
        }
        const char *q = p + 3;
        while (*q && isspace((unsigned char)*q)) {
            ++q;
        }
        if (*q == '(') {
            return 1;
        }
        p += 3;
    }
    return 0;
}



/* Helper for UNARY_ARITH_MISSING_PARENS: best-effort check that a unary
 * arithmetic operator appears immediately inside parentheses, e.g. "(-flag)".
 *
 * We approximate this by loading the source line for the node's location and
 * verifying that the first non-whitespace character before the operator is
 * an opening parenthesis. If the source cannot be read, we conservatively
 * return 1 ("parens present") to avoid false positives.
 */



/* Forward declaration so expression checking can call into MUX selector
 * analysis before the full definition appears later in this file.
 */
void sem_check_mux_selectors_recursive(JZASTNode *node,
                                       const JZModuleScope *mod_scope,
                                       JZDiagnosticList *diagnostics);


/* -------------------------------------------------------------------------
 *  Assignment semantics: width checks and basic target validation
 * -------------------------------------------------------------------------
 */

/* Compute width for a literal-index slice [msb:lsb]. Returns 1 on success. */
int sem_slice_literal_width(JZASTNode *slice, unsigned *out_width)
{
    if (!slice || !out_width) return 0;
    if (slice->type != JZ_AST_EXPR_SLICE || slice->child_count < 3) return 0;

    JZASTNode *msb_node = slice->children[1];
    JZASTNode *lsb_node = slice->children[2];
    if (!msb_node || !lsb_node ||
        msb_node->type != JZ_AST_EXPR_LITERAL ||
        lsb_node->type != JZ_AST_EXPR_LITERAL ||
        !msb_node->text || !lsb_node->text) {
        return 0;
    }

    unsigned msb = 0, lsb = 0;
    if (!parse_simple_nonnegative_int(msb_node->text, &msb) ||
        !parse_simple_nonnegative_int(lsb_node->text, &lsb)) {
        return 0;
    }
    if (msb < lsb) return 0;

    *out_width = msb - lsb + 1;
    return 1;
}

/* Return 1 if expr is a literal or concatenation that is entirely 'z' bits.
 * This is a local helper for PORT_TRISTATE_MISMATCH.
 */
/* -------------------------------------------------------------------------
 *  MUX declaration helpers (aggregation/slice forms)
 * -------------------------------------------------------------------------
 */

/* Collapse whitespace from a substring and return a newly allocated identifier-
 * like string (used for MUX source lists). Returns NULL for empty results.
 */
static char *sem_normalize_name_segment(const char *start, size_t len)
{
    if (!start || len == 0) return NULL;
    size_t out_len = 0;
    for (size_t i = 0; i < len; ++i) {
        if (!isspace((unsigned char)start[i])) {
            out_len++;
        }
    }
    if (out_len == 0) return NULL;

    char *buf = (char *)malloc(out_len + 1);
    if (!buf) return NULL;
    size_t pos = 0;
    for (size_t i = 0; i < len; ++i) {
        if (!isspace((unsigned char)start[i])) {
            buf[pos++] = start[i];
        }
    }
    buf[pos] = '\0';
    return buf;
}

/* Resolve a simple MUX source name (bare identifier) to a module-level symbol
 * and, when possible, a concrete bit-width from the declaration's width
 * expression (only simple decimal widths are handled here).
 */
static int sem_mux_resolve_simple_source(const char *name,
                                         const JZModuleScope *scope,
                                         unsigned *out_width)
{
    if (!name || !scope) return 0;
    const JZSymbol *sym = module_scope_lookup(scope, name);
    if (!sym || !sym->node) return 0;

    /* Only ports, wires, and registers are considered valid aggregation
     * sources for now.
     */
    if (sym->kind != JZ_SYM_PORT &&
            sym->kind != JZ_SYM_WIRE &&
            sym->kind != JZ_SYM_REGISTER &&
            sym->kind != JZ_SYM_LATCH) {
        return 0;
    }

    if (out_width) {
        const char *wtext = sym->node->width;
        unsigned w = 0;
        int rc = eval_simple_positive_decl_int(wtext, &w);
        if (rc == 1) {
            *out_width = w;
        } else {
            *out_width = 0; /* unknown/complex width */
        }
    }
    return 1;
}


/* Compute the element count for a MUX identifier used in selector range
 * checks. Returns 1 on success with *out_count set, 0 if the count could not
 * be determined from the declaration.
 */
static int sem_mux_compute_element_count(const JZModuleScope *scope,
                                         JZASTNode *mux_decl,
                                         unsigned *out_count)
{
    if (!scope || !mux_decl || !mux_decl->name) return 0;

    if (mux_decl->block_kind && strcmp(mux_decl->block_kind, "AGGREGATE") == 0) {
        if (mux_decl->child_count == 0) return 0;
        JZASTNode *rhs = mux_decl->children[0];
        if (!rhs || rhs->type != JZ_AST_RAW_TEXT || !rhs->text) return 0;

        /* Count non-empty comma-separated segments. */
        const char *p = rhs->text;
        unsigned count = 0;
        while (*p) {
            const char *seg_start = p;
            const char *comma = strchr(p, ',');
            size_t seg_len = 0;
            if (comma) {
                seg_len = (size_t)(comma - seg_start);
                p = comma + 1;
            } else {
                seg_len = strlen(seg_start);
                p = seg_start + seg_len;
            }
            char *name = sem_normalize_name_segment(seg_start, seg_len);
            if (name) {
                count++;
                free(name);
            }
        }
        if (count == 0) return 0;
        if (out_count) *out_count = count;
        return 1;
    }

    if (mux_decl->block_kind && strcmp(mux_decl->block_kind, "SLICE") == 0) {
        /* For slicing form, derive K = wide_width / elem_width when possible. */
        if (mux_decl->child_count == 0) return 0;
        JZASTNode *rhs = mux_decl->children[0];
        if (!rhs || rhs->type != JZ_AST_RAW_TEXT || !rhs->text) return 0;

        unsigned elem_w = 0;
        int rc = eval_simple_positive_decl_int(mux_decl->width, &elem_w);
        if (rc != 1 || elem_w == 0u) return 0;

        char *wide_name = sem_normalize_name_segment(rhs->text, strlen(rhs->text));
        if (!wide_name) return 0;
        unsigned wide_w = 0;
        int ok = sem_mux_resolve_simple_source(wide_name, scope, &wide_w) && wide_w > 0u;
        free(wide_name);
        if (!ok || wide_w % elem_w != 0u) return 0;

        if (out_count) *out_count = wide_w / elem_w;
        return 1;
    }

    return 0;
}

/* Check constant MUX selector indices against the number of elements inferred
 * from the MUX declaration. Only literal indices are handled here.
 */
static void sem_check_mux_selector_expr(JZASTNode *expr,
                                        const JZModuleScope *mod_scope,
                                        JZDiagnosticList *diagnostics)
{
    if (!expr || !mod_scope) return;
    if (expr->type != JZ_AST_EXPR_SLICE || expr->child_count < 3) return;

    JZASTNode *base = expr->children[0];
    if (!base || base->type != JZ_AST_EXPR_IDENTIFIER || !base->name) return;

    const JZSymbol *mux_sym = module_scope_lookup_kind(mod_scope, base->name, JZ_SYM_MUX);
    if (!mux_sym || !mux_sym->node) return;
    JZASTNode *mux_decl = mux_sym->node;

    /* Only handle literal selector indices. */
    JZASTNode *idx_node = expr->children[1];
    if (!idx_node || idx_node->type != JZ_AST_EXPR_LITERAL || !idx_node->text) return;

    unsigned idx_val = 0;
    if (!parse_literal_unsigned_value(idx_node->text, &idx_val)) return;

    unsigned elem_count = 0;
    if (!sem_mux_compute_element_count(mod_scope, mux_decl, &elem_count) || elem_count == 0u) {
        return; /* unable to determine element count statically */
    }

    if (idx_val >= elem_count) {
        char msg[512];
        snprintf(msg, sizeof(msg),
                 "%s[%u] is out of range; MUX '%s' has %u element%s (valid indices: 0..%u)",
                 base->name, idx_val,
                 base->name, elem_count,
                 elem_count == 1 ? "" : "s",
                 elem_count - 1);
        sem_report_rule(diagnostics,
                        expr->loc,
                        "MUX_SELECTOR_OUT_OF_RANGE_CONST",
                        msg);
    }
}

void sem_check_mux_selectors_recursive(JZASTNode *node,
                                       const JZModuleScope *mod_scope,
                                       JZDiagnosticList *diagnostics)
{
    if (!node) return;
    sem_check_mux_selector_expr(node, mod_scope, diagnostics);
    for (size_t i = 0; i < node->child_count; ++i) {
        sem_check_mux_selectors_recursive(node->children[i], mod_scope, diagnostics);
    }
}

/* Check that a concatenation LHS in a SYNCHRONOUS block does not include the
 * same register more than once.
 */
/* Forward declaration for testbench semantic validation. */
extern int jz_sem_run_testbench(JZASTNode *root, JZDiagnosticList *diagnostics);

/**
 * @brief Check if the root AST contains testbench nodes.
 */
static int is_testbench_file(JZASTNode *root)
{
    if (!root) return 0;
    for (size_t i = 0; i < root->child_count; ++i) {
        JZASTNode *child = root->children[i];
        if (child && (child->type == JZ_AST_TESTBENCH ||
                      child->type == JZ_AST_SIMULATION)) {
            return 1;
        }
    }
    return 0;
}

int jz_sem_run(JZASTNode *root,
               JZDiagnosticList *diagnostics,
               const char *filename,
               int verbose)
{
    if (!root) {
        return 0;
    }

    /* Testbench/simulation files get their own validation path. */
    if (is_testbench_file(root)) {
        return jz_sem_run_testbench(root, diagnostics);
    }

    clock_t st0;

    /* 1. Lexical identifier rules (length, '_' usage). */
    st0 = clock();
    sem_check_identifier_lexical(root, NULL, diagnostics);
    if (verbose) fprintf(stderr, "[verbose]   sem: identifier_lexical: %.1f ms\n",
                         (double)(clock() - st0) / CLOCKS_PER_SEC * 1000.0);

    /* 2. Build symbol tables from the AST (project + modules). */
    JZBuffer module_scopes = (JZBuffer){0};
    JZBuffer project_symbols = (JZBuffer){0};

    st0 = clock();
    if (build_symbol_tables(root, &module_scopes, &project_symbols, diagnostics) != 0) {
        /* Cleanup and return failure on allocation problems. */
        size_t scope_count = module_scopes.len / sizeof(JZModuleScope);
        JZModuleScope *scopes = (JZModuleScope *)module_scopes.data;
        for (size_t i = 0; i < scope_count; ++i) {
            jz_buf_free(&scopes[i].symbols);
            if (scopes[i].bus_signal_decls.len > 0) {
                size_t bcount = scopes[i].bus_signal_decls.len / sizeof(JZASTNode *);
                JZASTNode **barr = (JZASTNode **)scopes[i].bus_signal_decls.data;
                for (size_t bi = 0; bi < bcount; ++bi) {
                    jz_ast_free(barr[bi]);
                }
                jz_buf_free(&scopes[i].bus_signal_decls);
            }
        }
        jz_buf_free(&module_scopes);
        jz_buf_free(&project_symbols);
        return -1;
    }
    if (verbose) fprintf(stderr, "[verbose]   sem: build_symbol_tables: %.1f ms\n",
                         (double)(clock() - st0) / CLOCKS_PER_SEC * 1000.0);

    JZChipData chip = (JZChipData){0};
    const JZChipData *chip_ptr = NULL;
    JZChipLoadStatus chip_status = JZ_CHIP_LOAD_GENERIC;
    if (root->text && root->text[0] != '\0') {
        chip_status = jz_chip_data_load(root->text, filename, &chip);
        if (chip_status == JZ_CHIP_LOAD_OK) {
            chip_ptr = &chip;
        } else if (chip_status == JZ_CHIP_LOAD_NOT_FOUND) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "no chip data found for '%s'\n"
                     "provide a local .json file or use a supported built-in chip name",
                     root->text);
            sem_report_rule(diagnostics,
                            root->loc,
                            "PROJECT_CHIP_DATA_NOT_FOUND",
                            msg);
        } else if (chip_status == JZ_CHIP_LOAD_JSON_ERROR) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "chip data JSON for '%s' could not be parsed\n"
                     "check the JSON file for syntax errors",
                     root->text);
            sem_report_rule(diagnostics,
                            root->loc,
                            "PROJECT_CHIP_DATA_INVALID",
                            msg);
        }
    }

    /* 3. Project-level semantics: project name uniqueness, GLOBAL blocks,
     *    CONFIG, CLOCKS, PIN blocks, MAP, blackboxes, and top-level @new.
     */
    st0 = clock();
    sem_check_project_name_unique(root, &project_symbols, diagnostics);
    sem_check_globals(root, &project_symbols, diagnostics);
    sem_check_project_config(root, diagnostics);
    sem_check_project_clocks(root, &module_scopes, &project_symbols, diagnostics);
    sem_check_project_clock_gen(root, &project_symbols, chip_ptr, diagnostics);
    sem_check_project_pins(root, &project_symbols, diagnostics);
    sem_check_project_map(root, &project_symbols, diagnostics);
    sem_check_project_buses(root, &project_symbols, diagnostics);
    sem_check_project_blackboxes(root, diagnostics);
    sem_check_project_top_new(root, &module_scopes, &project_symbols, diagnostics);
    sem_check_project_checks(root, &project_symbols, diagnostics);
    if (verbose) fprintf(stderr, "[verbose]   sem: project_checks: %.1f ms\n",
                         (double)(clock() - st0) / CLOCKS_PER_SEC * 1000.0);

    /* 3b. Lint-style project-level warnings (unused modules, etc.). */
    sem_check_unused_modules(root, diagnostics);

    /* 4. Name resolution for identifiers and qualified identifiers. */
    st0 = clock();
    resolve_names_recursive(root, &module_scopes, &project_symbols, NULL, diagnostics);
    if (verbose) fprintf(stderr, "[verbose]   sem: resolve_names: %.1f ms\n",
                         (double)(clock() - st0) / CLOCKS_PER_SEC * 1000.0);

    /* 5. Constant evaluation & expression typing on executable blocks. */
    st0 = clock();
    sem_check_expressions(root, &module_scopes, &project_symbols, chip_ptr, diagnostics);
    if (verbose) fprintf(stderr, "[verbose]   sem: check_expressions: %.1f ms\n",
                         (double)(clock() - st0) / CLOCKS_PER_SEC * 1000.0);

    /* 5a. Project-wide memory resource usage check. */
    sem_check_project_mem_resources(root, &module_scopes, &project_symbols, chip_ptr, diagnostics);

    /* 5b. Memory report (optional). */
    sem_emit_memory_report(root, &module_scopes, &project_symbols, chip_ptr, filename);

    /* 6. Net graph construction and simple net usage checks.
     *
     * Pass project_symbols so that reporting helpers (e.g. alias reports)
     * can attach project-level context such as CLOCKS/IN_PINS/MAP details.
     */
    st0 = clock();
    sem_build_net_graphs(root, &module_scopes, &project_symbols, diagnostics);
    if (verbose) fprintf(stderr, "[verbose]   sem: net_graphs: %.1f ms\n",
                         (double)(clock() - st0) / CLOCKS_PER_SEC * 1000.0);

    /* 7. Exclusive Assignment Rule (PED): per-path assignment checks. */
    st0 = clock();
    sem_check_exclusive_assignments(root, &module_scopes, &project_symbols, diagnostics);
    if (verbose) fprintf(stderr, "[verbose]   sem: exclusive_assignments: %.1f ms\n",
                         (double)(clock() - st0) / CLOCKS_PER_SEC * 1000.0);

    /* 8. Dead-code analysis (WARN_DEAD_CODE_UNREACHABLE, MEM_WARN_DEAD_CODE_ACCESS). */
    st0 = clock();
    sem_check_dead_code(root, &module_scopes, diagnostics);
    if (verbose) fprintf(stderr, "[verbose]   sem: dead_code: %.1f ms\n",
                         (double)(clock() - st0) / CLOCKS_PER_SEC * 1000.0);

    /* 9. Clock-domain rules for SYNCHRONOUS blocks. */
    st0 = clock();
    sem_check_sync_clock_domains(&module_scopes, diagnostics);
    if (verbose) fprintf(stderr, "[verbose]   sem: clock_domains: %.1f ms\n",
                         (double)(clock() - st0) / CLOCKS_PER_SEC * 1000.0);

    /* Cleanup temporary symbol tables. */
    size_t scope_count = module_scopes.len / sizeof(JZModuleScope);
    JZModuleScope *scopes = (JZModuleScope *)module_scopes.data;
    for (size_t i = 0; i < scope_count; ++i) {
        jz_buf_free(&scopes[i].symbols);
        if (scopes[i].bus_signal_decls.len > 0) {
            size_t bcount = scopes[i].bus_signal_decls.len / sizeof(JZASTNode *);
            JZASTNode **barr = (JZASTNode **)scopes[i].bus_signal_decls.data;
            for (size_t bi = 0; bi < bcount; ++bi) {
                jz_ast_free(barr[bi]);
            }
            jz_buf_free(&scopes[i].bus_signal_decls);
        }
    }
    jz_buf_free(&module_scopes);
    jz_buf_free(&project_symbols);
    jz_chip_data_free(&chip);

    return 0;
}
