#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

#include "sem_driver.h"
#include "sem.h"
#include "util.h"
#include "rules.h"
#include "driver_internal.h"
#include "path_security.h"

/* -------------------------------------------------------------------------
 *  Module scopes and project-level symbol tables
 * -------------------------------------------------------------------------
 */

JZModuleScope *find_module_scope_for_node(JZBuffer *scopes,
                                          JZASTNode *node)
{
    if (!scopes || !node) return NULL;
    size_t count = scopes->len / sizeof(JZModuleScope);
    JZModuleScope *arr = (JZModuleScope *)scopes->data;
    for (size_t i = 0; i < count; ++i) {
        if (arr[i].node == node) {
            return &arr[i];
        }
    }
    return NULL;
}

/**
 * @brief Collect declarations from inside a FEATURE_GUARD AST node into the
 *        module symbol table. Both THEN and ELSE branches are walked so that
 *        both paths are validated (per spec).
 */
static int collect_decls_from_feature_block(JZASTNode *block,
                                             JZModuleScope *scope,
                                             JZDiagnosticList *diagnostics);

static int collect_decls_from_feature(JZASTNode *guard,
                                       JZModuleScope *scope,
                                       JZDiagnosticList *diagnostics)
{
    if (!guard || guard->type != JZ_AST_FEATURE_GUARD) return 0;
    /* children[0] = condition, children[1] = THEN block, children[2] = ELSE block (optional) */
    for (size_t bi = 1; bi < guard->child_count; ++bi) {
        JZASTNode *branch = guard->children[bi];
        if (!branch) continue;
        if (collect_decls_from_feature_block(branch, scope, diagnostics) != 0) {
            return -1;
        }
    }
    return 0;
}

static int collect_decls_from_feature_block(JZASTNode *block,
                                             JZModuleScope *scope,
                                             JZDiagnosticList *diagnostics)
{
    if (!block) return 0;
    for (size_t j = 0; j < block->child_count; ++j) {
        JZASTNode *child = block->children[j];
        if (!child) continue;

        /* Handle nested FEATURE_GUARD within a branch (module-scope feature) */
        if (child->type == JZ_AST_FEATURE_GUARD) {
            if (collect_decls_from_feature(child, scope, diagnostics) != 0)
                return -1;
            continue;
        }

        /* Handle raw declarations directly inside feature branches
         * (e.g., WIRE_DECL inside a WIRE block's feature guard) */
        if (child->type == JZ_AST_WIRE_DECL && child->name) {
            if (module_scope_add_symbol(scope, JZ_SYM_WIRE, child->name, child, diagnostics) != 0)
                return -1;
            continue;
        }
        if (child->type == JZ_AST_REGISTER_DECL && child->name) {
            if (module_scope_add_symbol(scope, JZ_SYM_REGISTER, child->name, child, diagnostics) != 0)
                return -1;
            continue;
        }
        if (child->type == JZ_AST_LATCH_DECL && child->name) {
            if (module_scope_add_symbol(scope, JZ_SYM_LATCH, child->name, child, diagnostics) != 0)
                return -1;
            continue;
        }
        if (child->type == JZ_AST_PORT_DECL && child->name) {
            if (module_scope_add_symbol(scope, JZ_SYM_PORT, child->name, child, diagnostics) != 0)
                return -1;
            continue;
        }
        if (child->type == JZ_AST_CONST_DECL && child->name) {
            if (module_scope_add_symbol(scope, JZ_SYM_CONST, child->name, child, diagnostics) != 0)
                return -1;
            continue;
        }
        if (child->type == JZ_AST_MEM_DECL && child->name) {
            if (module_scope_add_symbol(scope, JZ_SYM_MEM, child->name, child, diagnostics) != 0)
                return -1;
            continue;
        }
        if (child->type == JZ_AST_MUX_DECL && child->name) {
            if (module_scope_add_symbol(scope, JZ_SYM_MUX, child->name, child, diagnostics) != 0)
                return -1;
            continue;
        }
        if (child->type == JZ_AST_MODULE_INSTANCE && child->name) {
            if (module_scope_add_symbol(scope, JZ_SYM_INSTANCE, child->name, child, diagnostics) != 0)
                return -1;
            continue;
        }

        /* Handle declaration blocks inside feature branches (module-scope feature) */
        switch (child->type) {
        case JZ_AST_CONST_BLOCK:
            for (size_t k = 0; k < child->child_count; ++k) {
                JZASTNode *decl = child->children[k];
                if (decl && decl->type == JZ_AST_CONST_DECL && decl->name) {
                    if (module_scope_add_symbol(scope, JZ_SYM_CONST, decl->name, decl, diagnostics) != 0)
                        return -1;
                } else if (decl && decl->type == JZ_AST_FEATURE_GUARD) {
                    if (collect_decls_from_feature(decl, scope, diagnostics) != 0)
                        return -1;
                }
            }
            break;
        case JZ_AST_PORT_BLOCK:
            for (size_t k = 0; k < child->child_count; ++k) {
                JZASTNode *decl = child->children[k];
                if (decl && decl->type == JZ_AST_PORT_DECL && decl->name) {
                    if (module_scope_add_symbol(scope, JZ_SYM_PORT, decl->name, decl, diagnostics) != 0)
                        return -1;
                } else if (decl && decl->type == JZ_AST_FEATURE_GUARD) {
                    if (collect_decls_from_feature(decl, scope, diagnostics) != 0)
                        return -1;
                }
            }
            break;
        case JZ_AST_WIRE_BLOCK:
            for (size_t k = 0; k < child->child_count; ++k) {
                JZASTNode *decl = child->children[k];
                if (decl && decl->type == JZ_AST_WIRE_DECL && decl->name) {
                    if (module_scope_add_symbol(scope, JZ_SYM_WIRE, decl->name, decl, diagnostics) != 0)
                        return -1;
                } else if (decl && decl->type == JZ_AST_FEATURE_GUARD) {
                    if (collect_decls_from_feature(decl, scope, diagnostics) != 0)
                        return -1;
                }
            }
            break;
        case JZ_AST_REGISTER_BLOCK:
            for (size_t k = 0; k < child->child_count; ++k) {
                JZASTNode *decl = child->children[k];
                if (decl && decl->type == JZ_AST_REGISTER_DECL && decl->name) {
                    if (module_scope_add_symbol(scope, JZ_SYM_REGISTER, decl->name, decl, diagnostics) != 0)
                        return -1;
                } else if (decl && decl->type == JZ_AST_FEATURE_GUARD) {
                    if (collect_decls_from_feature(decl, scope, diagnostics) != 0)
                        return -1;
                }
            }
            break;
        case JZ_AST_LATCH_BLOCK:
            for (size_t k = 0; k < child->child_count; ++k) {
                JZASTNode *decl = child->children[k];
                if (decl && decl->type == JZ_AST_LATCH_DECL && decl->name) {
                    if (module_scope_add_symbol(scope, JZ_SYM_LATCH, decl->name, decl, diagnostics) != 0)
                        return -1;
                } else if (decl && decl->type == JZ_AST_FEATURE_GUARD) {
                    if (collect_decls_from_feature(decl, scope, diagnostics) != 0)
                        return -1;
                }
            }
            break;
        case JZ_AST_MEM_BLOCK:
            for (size_t k = 0; k < child->child_count; ++k) {
                JZASTNode *decl = child->children[k];
                if (decl && decl->type == JZ_AST_MEM_DECL && decl->name) {
                    if (module_scope_add_symbol(scope, JZ_SYM_MEM, decl->name, decl, diagnostics) != 0)
                        return -1;
                } else if (decl && decl->type == JZ_AST_FEATURE_GUARD) {
                    if (collect_decls_from_feature(decl, scope, diagnostics) != 0)
                        return -1;
                }
            }
            break;
        case JZ_AST_MUX_BLOCK:
            for (size_t k = 0; k < child->child_count; ++k) {
                JZASTNode *decl = child->children[k];
                if (decl && decl->type == JZ_AST_MUX_DECL && decl->name) {
                    if (module_scope_add_symbol(scope, JZ_SYM_MUX, decl->name, decl, diagnostics) != 0)
                        return -1;
                } else if (decl && decl->type == JZ_AST_FEATURE_GUARD) {
                    if (collect_decls_from_feature(decl, scope, diagnostics) != 0)
                        return -1;
                }
            }
            break;
        case JZ_AST_MODULE_INSTANCE:
            if (child->name) {
                if (module_scope_add_symbol(scope, JZ_SYM_INSTANCE, child->name, child, diagnostics) != 0)
                    return -1;
            }
            break;
        default:
            break;
        }
    }
    return 0;
}

static int populate_module_scope(JZModuleScope *scope,
                                 JZDiagnosticList *diagnostics)
{
    if (!scope || !scope->node) return 0;
    JZASTNode *mod = scope->node;

    /* Initialize signal-ID allocator for this module scope. */
    scope->next_signal_id = 0;

    for (size_t i = 0; i < mod->child_count; ++i) {
        JZASTNode *child = mod->children[i];
        if (!child) continue;

        switch (child->type) {
        case JZ_AST_CONST_BLOCK:
            if (child->block_kind && strcmp(child->block_kind, "CONST") == 0) {
                for (size_t j = 0; j < child->child_count; ++j) {
                    JZASTNode *decl = child->children[j];
                    if (decl && decl->type == JZ_AST_CONST_DECL && decl->name) {
                        if (module_scope_add_symbol(scope,
                                                    JZ_SYM_CONST,
                                                    decl->name,
                                                    decl,
                                                    diagnostics) != 0) {
                            return -1;
                        }
                    } else if (decl && decl->type == JZ_AST_FEATURE_GUARD) {
                        if (collect_decls_from_feature(decl, scope, diagnostics) != 0)
                            return -1;
                    }
                }
            }
            break;

        case JZ_AST_PORT_BLOCK:
            for (size_t j = 0; j < child->child_count; ++j) {
                JZASTNode *decl = child->children[j];
                if (decl && decl->type == JZ_AST_PORT_DECL && decl->name) {
                    if (module_scope_add_symbol(scope,
                                                JZ_SYM_PORT,
                                                decl->name,
                                                decl,
                                                diagnostics) != 0) {
                        return -1;
                    }
                } else if (decl && decl->type == JZ_AST_FEATURE_GUARD) {
                    if (collect_decls_from_feature(decl, scope, diagnostics) != 0)
                        return -1;
                }
            }
            break;

        case JZ_AST_WIRE_BLOCK:
            for (size_t j = 0; j < child->child_count; ++j) {
                JZASTNode *decl = child->children[j];
                if (decl && decl->type == JZ_AST_WIRE_DECL && decl->name) {
                    if (module_scope_add_symbol(scope,
                                                JZ_SYM_WIRE,
                                                decl->name,
                                                decl,
                                                diagnostics) != 0) {
                        return -1;
                    }
                } else if (decl && decl->type == JZ_AST_FEATURE_GUARD) {
                    if (collect_decls_from_feature(decl, scope, diagnostics) != 0)
                        return -1;
                }
            }
            break;

        case JZ_AST_REGISTER_BLOCK:
            for (size_t j = 0; j < child->child_count; ++j) {
                JZASTNode *decl = child->children[j];
                if (decl && decl->type == JZ_AST_REGISTER_DECL && decl->name) {
                    if (module_scope_add_symbol(scope,
                                                JZ_SYM_REGISTER,
                                                decl->name,
                                                decl,
                                                diagnostics) != 0) {
                        return -1;
                    }
                } else if (decl && decl->type == JZ_AST_FEATURE_GUARD) {
                    if (collect_decls_from_feature(decl, scope, diagnostics) != 0)
                        return -1;
                }
            }
            break;

        case JZ_AST_LATCH_BLOCK:
            for (size_t j = 0; j < child->child_count; ++j) {
                JZASTNode *decl = child->children[j];
                if (decl && decl->type == JZ_AST_LATCH_DECL && decl->name) {
                    if (module_scope_add_symbol(scope,
                                                JZ_SYM_LATCH,
                                                decl->name,
                                                decl,
                                                diagnostics) != 0) {
                        return -1;
                    }
                } else if (decl && decl->type == JZ_AST_FEATURE_GUARD) {
                    if (collect_decls_from_feature(decl, scope, diagnostics) != 0)
                        return -1;
                }
            }
            break;

        case JZ_AST_MEM_BLOCK:
            for (size_t j = 0; j < child->child_count; ++j) {
                JZASTNode *decl = child->children[j];
                if (decl && decl->type == JZ_AST_MEM_DECL && decl->name) {
                    if (module_scope_add_symbol(scope,
                                                JZ_SYM_MEM,
                                                decl->name,
                                                decl,
                                                diagnostics) != 0) {
                        return -1;
                    }
                } else if (decl && decl->type == JZ_AST_FEATURE_GUARD) {
                    if (collect_decls_from_feature(decl, scope, diagnostics) != 0)
                        return -1;
                }
            }
            break;

        case JZ_AST_MUX_BLOCK:
            for (size_t j = 0; j < child->child_count; ++j) {
                JZASTNode *decl = child->children[j];
                if (decl && decl->type == JZ_AST_MUX_DECL && decl->name) {
                    if (module_scope_add_symbol(scope,
                                                JZ_SYM_MUX,
                                                decl->name,
                                                decl,
                                                diagnostics) != 0) {
                        return -1;
                    }
                } else if (decl && decl->type == JZ_AST_FEATURE_GUARD) {
                    if (collect_decls_from_feature(decl, scope, diagnostics) != 0)
                        return -1;
                }
            }
            break;

        case JZ_AST_BLOCK:
            /* CDC BLOCK: add destination aliases as symbols so they are not
             * reported as UNDECLARED_IDENTIFIER. CDC semantics (home domains)
             * are enforced in separate passes.
             */
            if (child->block_kind && strcmp(child->block_kind, "CDC") == 0) {
                for (size_t j = 0; j < child->child_count; ++j) {
                    JZASTNode *cdc = child->children[j];
                    if (!cdc || cdc->type != JZ_AST_CDC_DECL) continue;
                    if (cdc->child_count < 4) continue;
                    JZASTNode *dst_id = cdc->children[2];
                    if (!dst_id || dst_id->type != JZ_AST_EXPR_IDENTIFIER || !dst_id->name) continue;
                    if (module_scope_add_symbol(scope,
                                                JZ_SYM_WIRE,
                                                dst_id->name,
                                                dst_id,
                                                diagnostics) != 0) {
                        return -1;
                    }
                }
            }
            break;

        case JZ_AST_MODULE_INSTANCE:
            if (child->name) {
                if (module_scope_add_symbol(scope,
                                            JZ_SYM_INSTANCE,
                                            child->name,
                                            child,
                                            diagnostics) != 0) {
                    return -1;
                }
            }
            break;

        case JZ_AST_FEATURE_GUARD:
            /* Module-scope @feature guard: walk both branches for declarations. */
            if (collect_decls_from_feature(child, scope, diagnostics) != 0)
                return -1;
            break;

        default:
            break;
        }
    }

    return 0;
}

const JZSymbol *project_lookup(const JZBuffer *symbols,
                               const char *name,
                               JZSymbolKind kind)
{
    if (!symbols || !name) return NULL;
    size_t count = symbols->len / sizeof(JZSymbol);
    const JZSymbol *syms = (const JZSymbol *)symbols->data;
    for (size_t i = 0; i < count; ++i) {
        if (syms[i].name && syms[i].kind == kind && strcmp(syms[i].name, name) == 0) {
            return &syms[i];
        }
    }
    return NULL;
}

const JZSymbol *project_lookup_module_or_blackbox(const JZBuffer *symbols,
                                                  const char *name)
{
    if (!symbols || !name) return NULL;
    size_t count = symbols->len / sizeof(JZSymbol);
    const JZSymbol *syms = (const JZSymbol *)symbols->data;
    for (size_t i = 0; i < count; ++i) {
        if (!syms[i].name || strcmp(syms[i].name, name) != 0) continue;
        if (syms[i].kind == JZ_SYM_MODULE || syms[i].kind == JZ_SYM_BLACKBOX) {
            return &syms[i];
        }
    }
    return NULL;
}

static int project_add_module_like(JZBuffer *project_symbols,
                                   JZASTNode *decl,
                                   JZSymbolKind kind,
                                   const char *project_name,
                                   JZDiagnosticList *diagnostics)
{
    if (!project_symbols || !decl || !decl->name) return 0;
    const char *name = decl->name;

    /* Check for duplicate module/blackbox names. */
    size_t count = project_symbols->len / sizeof(JZSymbol);
    JZSymbol *syms = (JZSymbol *)project_symbols->data;
    for (size_t i = 0; i < count; ++i) {
        if (!syms[i].name || strcmp(syms[i].name, name) != 0) continue;
        if (syms[i].kind == JZ_SYM_MODULE && kind == JZ_SYM_MODULE) {
            sem_report_rule(diagnostics,
                            decl->loc,
                            "MODULE_NAME_DUP_IN_PROJECT",
                            "duplicate module name in project");
            return 0;
        }
        if (syms[i].kind == JZ_SYM_BLACKBOX && kind == JZ_SYM_BLACKBOX) {
            sem_report_rule(diagnostics,
                            decl->loc,
                            "BLACKBOX_NAME_DUP_IN_PROJECT",
                            "duplicate blackbox name in project");
            return 0;
        }
        /* Conflict between module and blackbox of same name. */
        sem_report_rule(diagnostics,
                        decl->loc,
                        "BLACKBOX_NAME_DUP_IN_PROJECT",
                        "module/blackbox name conflict in project");
        return 0;
    }

    (void)project_name; /* PROJECT_NAME_NOT_UNIQUE handled separately. */

    JZSymbol sym;
    sym.name = name;
    sym.kind = kind;
    sym.node = decl;
    sym.id = -1;
    sym.can_be_z = 0;
    if (jz_buf_append(project_symbols, &sym, sizeof(sym)) != 0) {
        return -1;
    }
    return 0;
}

static int project_add_config(JZBuffer *project_symbols,
                              JZASTNode *decl,
                              JZDiagnosticList *diagnostics)
{
    if (!project_symbols || !decl || !decl->name) return 0;
    const char *name = decl->name;

    const JZSymbol *existing = project_lookup(project_symbols, name, JZ_SYM_CONFIG);
    if (existing) {
        sem_report_rule(diagnostics,
                        decl->loc,
                        "CONFIG_NAME_DUPLICATE",
                        "duplicate CONFIG entry name in project");
        return 0;
    }

    JZSymbol sym;
    sym.name = name;
    sym.kind = JZ_SYM_CONFIG;
    sym.node = decl;
    sym.id = -1;
    sym.can_be_z = 0;
    if (jz_buf_append(project_symbols, &sym, sizeof(sym)) != 0) {
        return -1;
    }
    return 0;
}

static int project_add_clock(JZBuffer *project_symbols,
                             JZASTNode *decl,
                             JZDiagnosticList *diagnostics)
{
    if (!project_symbols || !decl || !decl->name) return 0;
    const char *name = decl->name;

    const JZSymbol *existing = project_lookup(project_symbols, name, JZ_SYM_CLOCK);
    if (existing) {
        sem_report_rule(diagnostics,
                        decl->loc,
                        "CLOCK_DUPLICATE_NAME",
                        "duplicate clock name in CLOCKS block");
        return 0;
    }

    JZSymbol sym;
    sym.name = name;
    sym.kind = JZ_SYM_CLOCK;
    sym.node = decl;
    sym.id = -1;
    sym.can_be_z = 0;
    if (jz_buf_append(project_symbols, &sym, sizeof(sym)) != 0) {
        return -1;
    }
    return 0;
}

static int project_add_global(JZBuffer *project_symbols,
                               JZASTNode *glob,
                               JZDiagnosticList *diagnostics)
{
    if (!project_symbols || !glob || !glob->name) return 0;
    const char *name = glob->name;

    const JZSymbol *existing = project_lookup(project_symbols, name, JZ_SYM_GLOBAL);
    if (existing) {
        sem_report_rule(diagnostics,
                        glob->loc,
                        "GLOBAL_NAMESPACE_DUPLICATE",
                        "duplicate @global namespace name in compilation root");
        return 0;
    }

    JZSymbol sym;
    sym.name = name;
    sym.kind = JZ_SYM_GLOBAL;
    sym.node = glob;
    sym.id = -1;
    sym.can_be_z = 0;
    if (jz_buf_append(project_symbols, &sym, sizeof(sym)) != 0) {
        return -1;
    }
    return 0;
}

static int project_add_bus(JZBuffer *project_symbols,
                           JZASTNode *bus_block,
                           JZDiagnosticList *diagnostics)
{
    if (!project_symbols || !bus_block || !bus_block->name) return 0;
    const char *name = bus_block->name;

    const JZSymbol *existing = project_lookup(project_symbols, name, JZ_SYM_BUS);
    if (existing) {
        sem_report_rule(diagnostics,
                        bus_block->loc,
                        "BUS_DEF_DUP_NAME",
                        "duplicate BUS definition name in project");
        return 0;
    }

    JZSymbol sym;
    sym.name = name;
    sym.kind = JZ_SYM_BUS;
    sym.node = bus_block;
    sym.id = -1;
    sym.can_be_z = 0;
    if (jz_buf_append(project_symbols, &sym, sizeof(sym)) != 0) {
        return -1;
    }
    return 0;
}

static int project_add_pin(JZBuffer *project_symbols,
                           JZASTNode *decl,
                           JZDiagnosticList *diagnostics)
{
    if (!project_symbols || !decl || !decl->name) return 0;
    const char *name = decl->name;
    const char *kind = decl->block_kind; /* IN_PINS / OUT_PINS / INOUT_PINS */

    size_t count = project_symbols->len / sizeof(JZSymbol);
    JZSymbol *syms = (JZSymbol *)project_symbols->data;
    for (size_t i = 0; i < count; ++i) {
        JZSymbol *s = &syms[i];
        if (!s->name || s->kind != JZ_SYM_PIN) continue;
        if (strcmp(s->name, name) != 0) continue;

        const char *existing_kind = s->node && s->node->block_kind ? s->node->block_kind : "";
        if (kind && existing_kind && strcmp(existing_kind, kind) == 0) {
            sem_report_rule(diagnostics,
                            decl->loc,
                            "PIN_DUP_NAME_WITHIN_BLOCK",
                            "duplicate pin name within same PIN block");
        } else {
            sem_report_rule(diagnostics,
                            decl->loc,
                            "PIN_DECLARED_MULTIPLE_BLOCKS",
                            "pin name declared in multiple PIN blocks");
        }
        return 0;
    }

    JZSymbol sym;
    sym.name = name;
    sym.kind = JZ_SYM_PIN;
    sym.node = decl;
    sym.id = -1;
    sym.can_be_z = 0;
    if (jz_buf_append(project_symbols, &sym, sizeof(sym)) != 0) {
        return -1;
    }
    return 0;
}

/**
 * Check if a file path is readable, emitting MEM_INIT_FILE_NOT_FOUND if not.
 */
static void sem_check_file_readable(const char *file_path,
                                    const char *base_dir,
                                    const char *const_name,
                                    JZLocation loc,
                                    JZDiagnosticList *diagnostics)
{
    char *validated = jz_path_validate(
        file_path, base_dir, loc, diagnostics);
    if (!validated) return;

    FILE *fp = fopen(validated, "rb");
    if (!fp) {
        char msg[600];
        snprintf(msg, sizeof(msg),
                 "MEM init file not found or not readable: %s\n"
                 "(overridden from CONST %s)",
                 validated, const_name);
        sem_report_rule(diagnostics, loc,
                        "MEM_INIT_FILE_NOT_FOUND", msg);
    } else {
        fclose(fp);
    }
    free(validated);
}

/**
 * Extract the base directory from a source location filename.
 */
static void sem_base_dir_from_loc(const JZLocation *loc, char *buf, size_t bufsz)
{
    buf[0] = '\0';
    if (!loc || !loc->filename) return;
    const char *slash = strrchr(loc->filename, '/');
    if (slash) {
        size_t dlen = (size_t)(slash - loc->filename);
        if (dlen >= bufsz) dlen = bufsz - 1;
        memcpy(buf, loc->filename, dlen);
        buf[dlen] = '\0';
    }
}

/**
 * Recursively validate overridden @file() paths for MEM initializers.
 *
 * Walks the target module looking for:
 *  1. MEM blocks with @file(const_name) — validate directly.
 *  2. Sub-instances that pass const_name through an OVERRIDE to a child
 *     module — recurse into that child.
 *
 * The project AST is needed to find child module nodes for recursion.
 */
static void sem_validate_override_file_paths(
    const JZBuffer *project_symbols,
    JZASTNode *target_mod,
    JZASTNode *ov_block,
    JZDiagnosticList *diagnostics)
{
    if (!target_mod || !ov_block || !diagnostics) return;

    for (size_t k = 0; k < ov_block->child_count; ++k) {
        JZASTNode *ov_decl = ov_block->children[k];
        if (!ov_decl || ov_decl->type != JZ_AST_CONST_DECL ||
            !ov_decl->name || !ov_decl->text) continue;
        if (!ov_decl->block_kind ||
            strcmp(ov_decl->block_kind, "STRING") != 0) continue;

        const char *const_name = ov_decl->name;
        const char *file_path  = ov_decl->text;

        /* 1. Check MEM blocks in target module for @file(const_name). */
        for (size_t i = 0; i < target_mod->child_count; ++i) {
            JZASTNode *blk = target_mod->children[i];
            if (!blk || blk->type != JZ_AST_MEM_BLOCK) continue;

            for (size_t j = 0; j < blk->child_count; ++j) {
                JZASTNode *mem = blk->children[j];
                if (!mem || mem->type != JZ_AST_MEM_DECL) continue;
                if (mem->child_count == 0) continue;

                JZASTNode *init = mem->children[0];
                if (!init || init->type == JZ_AST_MEM_PORT) continue;
                if (!init->block_kind ||
                    strcmp(init->block_kind, "FILE_REF") != 0) continue;
                if (!init->name ||
                    strcmp(init->name, const_name) != 0) continue;

                char base_dir[512];
                sem_base_dir_from_loc(&mem->loc, base_dir, sizeof(base_dir));
                sem_check_file_readable(file_path,
                                        base_dir[0] ? base_dir : NULL,
                                        const_name, ov_decl->loc,
                                        diagnostics);
            }
        }

        /* 2. Check sub-instances that forward const_name via OVERRIDE. */
        if (!project_symbols) continue;
        for (size_t i = 0; i < target_mod->child_count; ++i) {
            JZASTNode *sub_inst = target_mod->children[i];
            if (!sub_inst || sub_inst->type != JZ_AST_MODULE_INSTANCE) continue;
            if (!sub_inst->text) continue;

            /* Find OVERRIDE block in this sub-instance. */
            JZASTNode *sub_ov = NULL;
            for (size_t oi = 0; oi < sub_inst->child_count; ++oi) {
                JZASTNode *ch = sub_inst->children[oi];
                if (ch && ch->type == JZ_AST_CONST_BLOCK &&
                    ch->block_kind &&
                    strcmp(ch->block_kind, "OVERRIDE") == 0) {
                    sub_ov = ch;
                    break;
                }
            }
            if (!sub_ov) continue;

            /* Check if any OVERRIDE entry forwards const_name.
             * Pattern: OVERRIDE { CHILD_CONST = const_name; }
             * where the value is the parent's CONST identifier. */
            for (size_t oi = 0; oi < sub_ov->child_count; ++oi) {
                JZASTNode *sub_decl = sub_ov->children[oi];
                if (!sub_decl || sub_decl->type != JZ_AST_CONST_DECL) continue;
                if (!sub_decl->text) continue;

                /* The sub-instance OVERRIDE entry forwards a CONST to the
                 * child module.  By this point the OVERRIDE propagation pass
                 * may have already resolved the RHS identifier to a string.
                 * We match on the *destination* const name (sub_decl->name)
                 * being the same as the const we're tracking (const_name),
                 * meaning this sub-instance passes through the same CONST. */
                if (!sub_decl->name ||
                    strcmp(sub_decl->name, const_name) != 0) continue;

                /* Find the child module via project symbol table. */
                const JZSymbol *child_sym =
                    project_lookup_module_or_blackbox(project_symbols,
                                                     sub_inst->text);
                if (!child_sym || !child_sym->node) continue;

                /* Build a synthetic OVERRIDE block with the resolved value
                 * and the child's CONST name, then recurse. */
                JZLocation no_loc = {0};
                JZASTNode *synth_ov = jz_ast_new(JZ_AST_CONST_BLOCK, no_loc);
                jz_ast_set_block_kind(synth_ov, "OVERRIDE");
                JZASTNode *synth_decl = jz_ast_new(JZ_AST_CONST_DECL, no_loc);
                jz_ast_set_name(synth_decl, sub_decl->name);
                jz_ast_set_text(synth_decl, file_path);
                jz_ast_set_block_kind(synth_decl, "STRING");
                synth_decl->loc = ov_decl->loc;
                jz_ast_add_child(synth_ov, synth_decl);

                sem_validate_override_file_paths(project_symbols,
                                                 child_sym->node,
                                                 synth_ov, diagnostics);
                jz_ast_free(synth_ov);
            }
        }
    }
}

int build_symbol_tables(JZASTNode *project,
                        JZBuffer *module_scopes,
                        JZBuffer *project_symbols,
                        JZDiagnosticList *diagnostics)
{
    if (!project || project->type != JZ_AST_PROJECT) return 0;

    const char *project_name = project->name;

    /* First pass: register modules/blackboxes and create empty module scopes. */
    for (size_t i = 0; i < project->child_count; ++i) {
        JZASTNode *child = project->children[i];
        if (!child) continue;

        if (child->type == JZ_AST_MODULE || child->type == JZ_AST_BLACKBOX) {
            JZSymbolKind kind = (child->type == JZ_AST_MODULE)
                              ? JZ_SYM_MODULE
                              : JZ_SYM_BLACKBOX;

            if (project_add_module_like(project_symbols,
                                        child,
                                        kind,
                                        project_name,
                                        diagnostics) != 0) {
                return -1;
            }

            JZModuleScope scope;
            memset(&scope, 0, sizeof(scope));
            scope.name = child->name;
            scope.node = child;
            if (jz_buf_append(module_scopes, &scope, sizeof(scope)) != 0) {
                return -1;
            }
        }
    }

    /* Second pass: populate module scopes with module-local symbols. */
    size_t scope_count = module_scopes->len / sizeof(JZModuleScope);
    JZModuleScope *scopes = (JZModuleScope *)module_scopes->data;
    for (size_t i = 0; i < scope_count; ++i) {
        if (populate_module_scope(&scopes[i], diagnostics) != 0) {
            return -1;
        }
    }

    /* Pass 2b: apply OVERRIDE values from instances to target module scopes.
     *
     * The template expander runs before semantic analysis and collects
     * CONST values globally (including OVERRIDE blocks). When an OVERRIDE
     * sets TARGET_COUNT=4, the template expands to 4 iterations, producing
     * bus accesses like tgt[2] and tgt[3]. The semantic BUS index check
     * needs to see the overridden value to avoid false
     * BUS_PORT_INDEX_OUT_OF_RANGE errors.
     *
     * If multiple instances override the same CONST, the last one wins.
     *
     * NOTE: This modifies the base module scope to reflect the overridden
     * values. The base module's IR may show the overridden widths/counts,
     * but since instances with OVERRIDE always use a specialized IR module,
     * the base module is never instantiated with incorrect parameters.
     */
    for (size_t i = 0; i < project->child_count; ++i) {
        JZASTNode *mod = project->children[i];
        if (!mod || mod->type != JZ_AST_MODULE) continue;

        for (size_t j = 0; j < mod->child_count; ++j) {
            JZASTNode *inst = mod->children[j];
            if (!inst || inst->type != JZ_AST_MODULE_INSTANCE) continue;
            if (!inst->text) continue; /* inst->text = target module name */

            /* Find the OVERRIDE block inside this instance. */
            JZASTNode *ov_block = NULL;
            for (size_t k = 0; k < inst->child_count; ++k) {
                JZASTNode *child = inst->children[k];
                if (child && child->type == JZ_AST_CONST_BLOCK &&
                    child->block_kind &&
                    strcmp(child->block_kind, "OVERRIDE") == 0) {
                    ov_block = child;
                    break;
                }
            }
            if (!ov_block) continue;

            /* Find the target module's scope. */
            const JZSymbol *target_sym =
                project_lookup_module_or_blackbox(project_symbols, inst->text);
            if (!target_sym || !target_sym->node) continue;

            JZModuleScope *target_scope =
                find_module_scope_for_node(module_scopes, target_sym->node);
            if (!target_scope) continue;

            /* Apply each OVERRIDE entry to the target scope.
             * If the override value is an identifier that resolves to
             * a STRING CONST in the enclosing module's scope, propagate
             * the string type so @file() and similar string contexts
             * see the correct block_kind.
             */
            JZModuleScope *enc_scope =
                find_module_scope_for_node(module_scopes, mod);
            for (size_t k = 0; k < ov_block->child_count; ++k) {
                JZASTNode *ov_decl = ov_block->children[k];
                if (!ov_decl || ov_decl->type != JZ_AST_CONST_DECL ||
                    !ov_decl->name) continue;

                /* Resolve identifier references to string CONSTs. */
                if ((!ov_decl->block_kind ||
                     strcmp(ov_decl->block_kind, "STRING") != 0) &&
                    ov_decl->text && enc_scope) {
                    /* Trim trailing whitespace from text to get bare name. */
                    size_t tlen = strlen(ov_decl->text);
                    while (tlen > 0 && ov_decl->text[tlen - 1] == ' ')
                        tlen--;
                    size_t enc_sym_count =
                        enc_scope->symbols.len / sizeof(JZSymbol);
                    JZSymbol *enc_syms =
                        (JZSymbol *)enc_scope->symbols.data;
                    for (size_t es = 0; es < enc_sym_count; ++es) {
                        if (enc_syms[es].kind == JZ_SYM_CONST &&
                            enc_syms[es].name &&
                            strlen(enc_syms[es].name) == tlen &&
                            strncmp(enc_syms[es].name, ov_decl->text,
                                    tlen) == 0 &&
                            enc_syms[es].node &&
                            enc_syms[es].node->block_kind &&
                            strcmp(enc_syms[es].node->block_kind,
                                   "STRING") == 0) {
                            jz_ast_set_block_kind(ov_decl, "STRING");
                            jz_ast_set_text(ov_decl,
                                            enc_syms[es].node->text);
                            break;
                        }
                    }
                }

                size_t sym_count =
                    target_scope->symbols.len / sizeof(JZSymbol);
                JZSymbol *syms =
                    (JZSymbol *)target_scope->symbols.data;
                for (size_t s = 0; s < sym_count; ++s) {
                    if (syms[s].kind == JZ_SYM_CONST &&
                        syms[s].name &&
                        strcmp(syms[s].name, ov_decl->name) == 0) {
                        syms[s].node = ov_decl;
                        break;
                    }
                }
            }

            /* Validate overridden @file() paths for this instance. */
            sem_validate_override_file_paths(project_symbols, target_sym->node,
                                             ov_block, diagnostics);
        }
    }

    /* Also check the project-level @top instance for OVERRIDE blocks. */
    for (size_t i = 0; i < project->child_count; ++i) {
        JZASTNode *top_inst = project->children[i];
        if (!top_inst || top_inst->type != JZ_AST_PROJECT_TOP_INSTANCE) continue;
        if (!top_inst->name) continue;

        JZASTNode *ov_block = NULL;
        for (size_t k = 0; k < top_inst->child_count; ++k) {
            JZASTNode *child = top_inst->children[k];
            if (child && child->type == JZ_AST_CONST_BLOCK &&
                child->block_kind &&
                strcmp(child->block_kind, "OVERRIDE") == 0) {
                ov_block = child;
                break;
            }
        }
        if (!ov_block) continue;

        const JZSymbol *target_sym =
            project_lookup_module_or_blackbox(project_symbols, top_inst->name);
        if (!target_sym || !target_sym->node) continue;

        JZModuleScope *target_scope =
            find_module_scope_for_node(module_scopes, target_sym->node);
        if (!target_scope) continue;

        for (size_t k = 0; k < ov_block->child_count; ++k) {
            JZASTNode *ov_decl = ov_block->children[k];
            if (!ov_decl || ov_decl->type != JZ_AST_CONST_DECL ||
                !ov_decl->name) continue;

            size_t sym_count =
                target_scope->symbols.len / sizeof(JZSymbol);
            JZSymbol *syms =
                (JZSymbol *)target_scope->symbols.data;
            for (size_t s = 0; s < sym_count; ++s) {
                if (syms[s].kind == JZ_SYM_CONST &&
                    syms[s].name &&
                    strcmp(syms[s].name, ov_decl->name) == 0) {
                    syms[s].node = ov_decl;
                    break;
                }
            }
        }

        /* Validate overridden @file() paths for @top instance. */
        sem_validate_override_file_paths(project_symbols, target_sym->node,
                                         ov_block, diagnostics);
    }

    /* Third pass: project-level CONFIG/CLOCKS/GLOBAL/PIN/MAP symbols. */
    for (size_t i = 0; i < project->child_count; ++i) {
        JZASTNode *child = project->children[i];
        if (!child) continue;

        if (child->type == JZ_AST_CONFIG_BLOCK) {
            for (size_t j = 0; j < child->child_count; ++j) {
                JZASTNode *decl = child->children[j];
                if (decl && decl->type == JZ_AST_CONST_DECL && decl->name) {
                    if (project_add_config(project_symbols, decl, diagnostics) != 0) {
                        return -1;
                    }
                }
            }
        } else if (child->type == JZ_AST_CLOCKS_BLOCK) {
            for (size_t j = 0; j < child->child_count; ++j) {
                JZASTNode *decl = child->children[j];
                if (decl && decl->type == JZ_AST_CONST_DECL && decl->name) {
                    if (project_add_clock(project_symbols, decl, diagnostics) != 0) {
                        return -1;
                    }
                }
            }
        } else if (child->type == JZ_AST_GLOBAL_BLOCK) {
            if (project_add_global(project_symbols, child, diagnostics) != 0) {
                return -1;
            }
        } else if (child->type == JZ_AST_BUS_BLOCK) {
            if (project_add_bus(project_symbols, child, diagnostics) != 0) {
                return -1;
            }
        } else if (child->type == JZ_AST_IN_PINS_BLOCK ||
                   child->type == JZ_AST_OUT_PINS_BLOCK ||
                   child->type == JZ_AST_INOUT_PINS_BLOCK) {
            for (size_t j = 0; j < child->child_count; ++j) {
                JZASTNode *decl = child->children[j];
                if (decl && decl->type == JZ_AST_PORT_DECL && decl->name) {
                    if (project_add_pin(project_symbols, decl, diagnostics) != 0) {
                        return -1;
                    }
                }
            }
        } else if (child->type == JZ_AST_MAP_BLOCK) {
            for (size_t j = 0; j < child->child_count; ++j) {
                JZASTNode *entry = child->children[j];
                if (entry && entry->type == JZ_AST_CONST_DECL && entry->name) {
                    JZSymbol sym;
                    sym.name = entry->name;
                    sym.kind = JZ_SYM_MAP_ENTRY;
                    sym.node = entry;
                    sym.id = -1;
                    sym.can_be_z = 0;
                    if (jz_buf_append(project_symbols, &sym, sizeof(sym)) != 0) {
                        return -1;
                    }
                }
            }
        }
    }

    return 0;
}

/* -------------------------------------------------------------------------
 *  Project-level CONFIG / CLOCKS / PIN / MAP / top-level semantics
 * -------------------------------------------------------------------------
 */

int sem_expr_has_global_ref(const char *expr,
                            const JZBuffer *project_symbols)
{
    if (!expr || !project_symbols || !project_symbols->data) return 0;

    const JZSymbol *syms = (const JZSymbol *)project_symbols->data;
    size_t count = project_symbols->len / sizeof(JZSymbol);

    for (size_t i = 0; i < count; ++i) {
        const JZSymbol *s = &syms[i];
        if (!s->name || s->kind != JZ_SYM_GLOBAL) continue;

        const char *gname = s->name;
        size_t glen = strlen(gname);
        if (glen == 0) continue;

        /* Look for occurrences of "<global> . <id>" allowing arbitrary
         * whitespace between the pieces, but requiring that the GLOBAL
         * name itself be bounded by non-identifier characters.
         */
        const char *p = expr;
        for (;;) {
            const char *hit = strstr(p, gname);
            if (!hit) break;

            char before = (hit == expr) ? '\0' : hit[-1];
            int before_ok = !(isalpha((unsigned char)before) ||
                              isdigit((unsigned char)before) ||
                              before == '_');
            if (!before_ok) {
                p = hit + glen;
                continue;
            }

            /* Skip any whitespace after the global name to find the dot. */
            const char *after = hit + glen;
            while (*after && isspace((unsigned char)*after)) {
                ++after;
            }
            if (*after != '.') {
                p = hit + glen;
                continue;
            }

            /* Require that the character after the dot (after optional
             * whitespace) starts an identifier.
             */
            const char *tail = after + 1;
            while (*tail && isspace((unsigned char)*tail)) {
                ++tail;
            }
            if (!(*tail == '_' || isalpha((unsigned char)*tail))) {
                p = hit + glen;
                continue;
            }

            return 1;
        }
    }

    return 0;
}

void sem_check_project_name_unique(JZASTNode *project,
                                   const JZBuffer *project_symbols,
                                   JZDiagnosticList *diagnostics)
{
    if (!project || project->type != JZ_AST_PROJECT || !project_symbols) return;
    const char *proj_name = project->name;
    if (!proj_name || !*proj_name) return;

    size_t count = project_symbols->len / sizeof(JZSymbol);
    const JZSymbol *syms = (const JZSymbol *)project_symbols->data;
    for (size_t i = 0; i < count; ++i) {
        const JZSymbol *s = &syms[i];
        if (!s->name) continue;
        if ((s->kind == JZ_SYM_MODULE || s->kind == JZ_SYM_BLACKBOX) &&
            strcmp(s->name, proj_name) == 0) {
            sem_report_rule(diagnostics,
                            project->loc,
                            "PROJECT_NAME_NOT_UNIQUE",
                            "project name conflicts with module/blackbox name in project");
            break;
        }
    }
}

void sem_check_project_buses(JZASTNode *project,
                             const JZBuffer *project_symbols,
                             JZDiagnosticList *diagnostics)
{
    (void)project_symbols; /* BUS ids already validated by project_add_bus. */
    if (!project || project->type != JZ_AST_PROJECT) return;

    for (size_t i = 0; i < project->child_count; ++i) {
        JZASTNode *bus = project->children[i];
        if (!bus || bus->type != JZ_AST_BUS_BLOCK) continue;

        /* Check for duplicate signal names and invalid directions within BUS. */
        for (size_t j = 0; j < bus->child_count; ++j) {
            JZASTNode *decl = bus->children[j];
            if (!decl || decl->type != JZ_AST_BUS_DECL || !decl->name) continue;

            /* Duplicate signal name within same BUS block. */
            for (size_t k = 0; k < j; ++k) {
                JZASTNode *prev = bus->children[k];
                if (!prev || prev->type != JZ_AST_BUS_DECL || !prev->name) continue;
                if (strcmp(prev->name, decl->name) == 0) {
                    sem_report_rule(diagnostics,
                                    decl->loc,
                                    "BUS_DEF_SIGNAL_DUP_NAME",
                                    "duplicate signal name inside BUS definition");
                    break;
                }
            }

            const char *dir = decl->block_kind ? decl->block_kind : "";
            if (strcmp(dir, "IN") != 0 &&
                strcmp(dir, "OUT") != 0 &&
                strcmp(dir, "INOUT") != 0) {
                sem_report_rule(diagnostics,
                                decl->loc,
                                "BUS_DEF_INVALID_DIR",
                                "BUS signal direction must be IN, OUT, or INOUT");
            }
        }
    }
}

static JZASTNode *sem_find_project_config_block(JZASTNode *project,
                                                JZDiagnosticList *diagnostics)
{
    if (!project || project->type != JZ_AST_PROJECT) return NULL;

    JZASTNode *first = NULL;
    for (size_t i = 0; i < project->child_count; ++i) {
        JZASTNode *child = project->children[i];
        if (!child || child->type != JZ_AST_CONFIG_BLOCK) continue;
        if (!first) {
            first = child;
        } else {
            sem_report_rule(diagnostics,
                            child->loc,
                            "CONFIG_MULTIPLE_BLOCKS",
                            "more than one CONFIG block defined in project");
        }
    }
    return first;
}

static int sem_config_index_of(JZASTNode *config_block,
                               const char *name)
{
    if (!config_block || !name || !*name) return -1;
    for (size_t i = 0; i < config_block->child_count; ++i) {
        JZASTNode *decl = config_block->children[i];
        if (!decl || decl->type != JZ_AST_CONST_DECL || !decl->name) continue;
        if (strcmp(decl->name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

void sem_check_project_config(JZASTNode *project,
                              JZDiagnosticList *diagnostics)
{
    if (!project || project->type != JZ_AST_PROJECT) return;

    JZASTNode *cfg = sem_find_project_config_block(project, diagnostics);
    if (!cfg) {
        return;
    }

    size_t count = cfg->child_count;
    if (count == 0) {
        return;
    }

    unsigned char *edges = (unsigned char *)calloc(count * count, sizeof(unsigned char));
    if (!edges) {
        return;
    }

    int *has_forward_ref = (int *)calloc(count, sizeof(int));
    int *has_cycle      = (int *)calloc(count, sizeof(int));
    int any_static_error = 0;
    if (!has_forward_ref || !has_cycle) {
        free(edges);
        free(has_forward_ref);
        free(has_cycle);
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        JZASTNode *decl = cfg->children[i];
        if (!decl || decl->type != JZ_AST_CONST_DECL) continue;
        const char *expr_text = decl->text;
        if (!expr_text) continue;

        if (sem_expr_has_lit_call(expr_text)) {
            sem_report_rule(diagnostics,
                            decl->loc,
                            "LIT_INVALID_CONTEXT",
                            "lit() may not be used in CONFIG expressions");
            any_static_error = 1;
        }

        const char *p = expr_text;
        int expecting_dot = 0;
        int expecting_name = 0;

        char token[128];
        while (*p) {
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
                int dep_index = sem_config_index_of(cfg, token);
                if (dep_index < 0) {
                    sem_report_rule(diagnostics,
                                    decl->loc,
                                    "CONFIG_USE_UNDECLARED",
                                    "use of CONFIG.<name> not declared in project CONFIG block");
                    any_static_error = 1;
                } else {
                    size_t idx = i * count + (size_t)dep_index;
                    edges[idx] = 1u;
                    if ((size_t)dep_index > i) {
                        if (!has_forward_ref[i]) {
                            sem_report_rule(diagnostics,
                                            decl->loc,
                                            "CONFIG_FORWARD_REF",
                                            "CONFIG entry references later CONFIG.<name> (forward reference)");
                        }
                        has_forward_ref[i] = 1;
                        any_static_error = 1;
                    }
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

    int *visit = (int *)calloc(count, sizeof(int));
    if (visit) {
        for (size_t i = 0; i < count; ++i) {
            if (visit[i] != 0) continue;

            size_t *stack = (size_t *)malloc(count * sizeof(size_t));
            size_t *iter  = (size_t *)malloc(count * sizeof(size_t));
            if (!stack || !iter) {
                free(stack);
                free(iter);
                break;
            }
            size_t sp = 0;
            stack[sp] = i;
            iter[sp] = 0;
            visit[i] = 1;

            while (sp < count) {
                size_t v = stack[sp];
                size_t j = iter[sp];
                if (j >= count) {
                    visit[v] = 2;
                    if (sp == 0) {
                        break;
                    }
                    --sp;
                    continue;
                }
                iter[sp] = j + 1;
                if (!edges[v * count + j]) {
                    continue;
                }
                if (visit[j] == 1) {
                    if (!has_cycle[v]) {
                        JZASTNode *decl = cfg->children[v];
                        if (decl) {
                            sem_report_rule(diagnostics,
                                            decl->loc,
                                            "CONFIG_CIRCULAR_DEP",
                                            "circular dependency between CONFIG entries");
                        }
                        has_cycle[v] = 1;
                        any_static_error = 1;
                    }
                    continue;
                }
                if (visit[j] == 0) {
                    ++sp;
                    stack[sp] = j;
                    iter[sp] = 0;
                    visit[j] = 1;
                }
            }

            free(stack);
            free(iter);
        }
        free(visit);
    }

    if (!any_static_error) {
        /* Evaluate CONFIG entries with the general constant-eval engine,
         * but treat each entry independently so that a failure in one
         * definition does not automatically poison all later ones.
         *
         * We maintain an environment of successfully evaluated CONFIG
         * entries (env_defs/env_values). For each new entry:
         *   - Build a temporary defs[] = env_defs + current.
         *   - Call jz_const_eval_all on that small set.
         *   - On success, extend the environment; on failure, emit
         *     CONFIG_INVALID_EXPR_TYPE only for this entry.
         */
        JZConstDef *env_defs = (JZConstDef *)calloc(count, sizeof(JZConstDef));
        long long  *env_vals = (long long *)calloc(count, sizeof(long long));
        if (env_defs && env_vals) {
            size_t env_len = 0;

            for (size_t i = 0; i < count; ++i) {
                JZASTNode *decl = cfg->children[i];
                if (!decl || decl->type != JZ_AST_CONST_DECL || !decl->name) {
                    continue;
                }

                /* String CONFIG: skip numeric evaluation. The string value
                 * is already stored in decl->text and will be resolved by
                 * sem_resolve_string_const() when referenced in @file().
                 */
                if (decl->block_kind && strcmp(decl->block_kind, "STRING") == 0) {
                    continue;
                }

                JZConstDef cur_def;
                cur_def.name = decl->name;
                cur_def.expr = decl->text ? decl->text : "0";

                /* Build temporary definition list: existing env plus current. */
                size_t tmp_count = env_len + 1;
                JZConstDef *tmp_defs = (JZConstDef *)calloc(tmp_count, sizeof(JZConstDef));
                long long  *tmp_vals = (long long *)calloc(tmp_count, sizeof(long long));
                if (!tmp_defs || !tmp_vals) {
                    free(tmp_defs);
                    free(tmp_vals);
                    break;
                }

                if (env_len > 0) {
                    memcpy(tmp_defs, env_defs, env_len * sizeof(JZConstDef));
                }
                tmp_defs[env_len] = cur_def;

                JZConstEvalOptions opts;
                memset(&opts, 0, sizeof(opts));

                int rc = jz_const_eval_all(tmp_defs, tmp_count, &opts, tmp_vals);
                if (rc != 0) {
                    sem_report_rule(diagnostics,
                                    decl->loc,
                                    "CONFIG_INVALID_EXPR_TYPE",
                                    "CONFIG value is not a nonnegative integer expression");
                    /* Do not extend environment with a failing entry. */
                } else {
                    /* Extend environment with this successfully evaluated entry. */
                    env_defs[env_len] = cur_def;
                    env_vals[env_len] = tmp_vals[tmp_count - 1];
                    ++env_len;

                    /* Persist the evaluated numeric value on the AST node's width
                     * field so that downstream passes (e.g. module CONST
                     * evaluation) can retrieve it without re-evaluating.
                     */
                    char val_buf[32];
                    snprintf(val_buf, sizeof(val_buf), "%lld", tmp_vals[tmp_count - 1]);
                    jz_ast_set_width(decl, val_buf);
                }

                free(tmp_defs);
                free(tmp_vals);
            }
        }
        free(env_defs);
        free(env_vals);
    }

    free(edges);
    free(has_forward_ref);
    free(has_cycle);
}

JZASTNode *sem_find_project_top_new(JZASTNode *project)
{
    if (!project || project->type != JZ_AST_PROJECT) return NULL;
    for (size_t i = 0; i < project->child_count; ++i) {
        JZASTNode *child = project->children[i];
        if (child && child->type == JZ_AST_PROJECT_TOP_INSTANCE) {
            return child;
        }
    }
    return NULL;
}

typedef struct JZModuleUsage {
    JZASTNode *node; /* JZ_AST_MODULE */
    int        used; /* non-zero once referenced by @new */
} JZModuleUsage;

static void sem_mark_module_used(JZBuffer *modules,
                                 const char *name)
{
    if (!modules || !modules->data || !name || !*name) return;
    size_t count = modules->len / sizeof(JZModuleUsage);
    JZModuleUsage *arr = (JZModuleUsage *)modules->data;
    for (size_t i = 0; i < count; ++i) {
        if (!arr[i].node || !arr[i].node->name) continue;
        if (strcmp(arr[i].node->name, name) == 0) {
            arr[i].used = 1;
            break;
        }
    }
}

void sem_check_unused_modules(JZASTNode *project,
                              JZDiagnosticList *diagnostics)
{
    if (!project || project->type != JZ_AST_PROJECT) return;

    JZBuffer modules = {0}; /* array of JZModuleUsage */

    /* Collect all module declarations in the project. */
    for (size_t i = 0; i < project->child_count; ++i) {
        JZASTNode *child = project->children[i];
        if (!child || child->type != JZ_AST_MODULE || !child->name) continue;
        JZModuleUsage mu;
        mu.node = child;
        mu.used = 0;
        if (jz_buf_append(&modules, &mu, sizeof(mu)) != 0) {
            jz_buf_free(&modules);
            return;
        }
    }

    size_t module_count = modules.len / sizeof(JZModuleUsage);
    if (module_count == 0) {
        jz_buf_free(&modules);
        return;
    }

    /* Only issue WARN_UNUSED_MODULE when the project declares an explicit
     * top-level @new binding. This avoids flagging library-style files that
     * define modules but do not select a top.
     */
    JZASTNode *top_new = sem_find_project_top_new(project);
    if (!top_new || !top_new->name) {
        jz_buf_free(&modules);
        return;
    }

    /* Mark the top-level module referenced by the project @new. */
    sem_mark_module_used(&modules, top_new->name);

    /* Mark modules that are targets of module-level @new instantiations. */
    for (size_t i = 0; i < project->child_count; ++i) {
        JZASTNode *mod = project->children[i];
        if (!mod || mod->type != JZ_AST_MODULE) continue;
        for (size_t j = 0; j < mod->child_count; ++j) {
            JZASTNode *inst = mod->children[j];
            if (!inst || inst->type != JZ_AST_MODULE_INSTANCE || !inst->text) continue;
            sem_mark_module_used(&modules, inst->text);
        }
    }

    /* Emit WARN_UNUSED_MODULE for any module that was never referenced. */
    JZModuleUsage *arr = (JZModuleUsage *)modules.data;
    for (size_t i = 0; i < module_count; ++i) {
        if (!arr[i].node || arr[i].used) continue;
        sem_report_rule(diagnostics,
                        arr[i].node->loc,
                        "WARN_UNUSED_MODULE",
                        "module is declared but never instantiated or used as project top");
    }

    jz_buf_free(&modules);
}

void sem_check_project_blackboxes(JZASTNode *project,
                                  JZDiagnosticList *diagnostics)
{
    if (!project || project->type != JZ_AST_PROJECT) return;

    for (size_t i = 0; i < project->child_count; ++i) {
        JZASTNode *bb = project->children[i];
        if (!bb || bb->type != JZ_AST_BLACKBOX) continue;

        for (size_t j = 0; j < bb->child_count; ++j) {
            JZASTNode *child = bb->children[j];
            if (!child) continue;
            if (child->type == JZ_AST_PORT_BLOCK) {
                continue;
            }
            sem_report_rule(diagnostics,
                            child->loc,
                            "BLACKBOX_BODY_DISALLOWED",
                            "blackbox contains forbidden internal blocks");
        }
    }
}

/* -------------------------------------------------------------------------
 *  Name resolution
 * -------------------------------------------------------------------------
 */

static void report_undeclared(JZDiagnosticList *diagnostics,
                              JZASTNode *node)
{
    if (!diagnostics || !node) return;
    sem_report_rule(diagnostics,
                    node->loc,
                    "UNDECLARED_IDENTIFIER",
                    "use of undeclared identifier");
}

static void resolve_identifier_node(JZASTNode *node,
                                    const JZModuleScope *mod_scope,
                                    const JZBuffer *project_symbols,
                                    JZDiagnosticList *diagnostics)
{
    (void)project_symbols;
    if (!node || !node->name) return;

    if (node->name[0] == '_' && node->name[1] == '\0') {
        return;
    }

    /* IDX is a built-in identifier valid in array instance bindings.
     * Context validation is handled by driver_instance.c
     * (INSTANCE_ARRAY_IDX_INVALID_CONTEXT), so skip undeclared check here. */
    if (strcmp(node->name, "IDX") == 0) {
        return;
    }

    if (!mod_scope) {
        return;
    }

    const JZSymbol *sym = module_scope_lookup(mod_scope, node->name);
    if (!sym) {
        report_undeclared(diagnostics, node);
    }
}

static void resolve_qualified_identifier_node(JZASTNode *node,
                                              const JZModuleScope *mod_scope,
                                              const JZBuffer *project_symbols,
                                              JZDiagnosticList *diagnostics)
{
    if (!node || !node->name) return;

    if (mod_scope && project_symbols) {
        JZBusAccessInfo info;
        if (sem_resolve_bus_access(node, mod_scope, project_symbols, &info, diagnostics)) {
            return;
        }
    }

    const char *full = node->name;
    const char *dot = strchr(full, '.');
    if (!dot || !*(dot + 1)) {
        resolve_identifier_node(node, mod_scope, project_symbols, diagnostics);
        return;
    }

    char head[256];
    size_t head_len = (size_t)(dot - full);
    if (head_len >= sizeof(head)) head_len = sizeof(head) - 1;
    memcpy(head, full, head_len);
    head[head_len] = '\0';

    const char *rest = dot + 1;
    const char *second_dot = strchr(rest, '.');

    if (strcmp(head, "CONFIG") == 0) {
        if (!rest || !*rest || second_dot) {
            sem_report_rule(diagnostics,
                            node->loc,
                            "CONFIG_USE_UNDECLARED",
                            "invalid CONFIG.<name> reference");
            return;
        }

        char cfg_name[256];
        size_t cfg_len = strlen(rest);
        if (cfg_len >= sizeof(cfg_name)) cfg_len = sizeof(cfg_name) - 1;
        memcpy(cfg_name, rest, cfg_len);
        cfg_name[cfg_len] = '\0';

        const JZSymbol *cfg = project_lookup(project_symbols, cfg_name, JZ_SYM_CONFIG);
        if (!cfg) {
            sem_report_rule(diagnostics,
                            node->loc,
                            "CONFIG_USE_UNDECLARED",
                            "reference to undeclared CONFIG.<name>");
        }
        return;
    }

    /* Global namespace: <global>.<CONST>. */
    if (project_symbols) {
        const JZSymbol *glob = project_lookup(project_symbols, head, JZ_SYM_GLOBAL);
        if (glob) {
            if (!rest || !*rest || second_dot) {
                sem_report_rule(diagnostics,
                                node->loc,
                                "GLOBAL_CONST_USE_UNDECLARED",
                                "invalid GLOBAL.<name> reference");
                return;
            }

            JZASTNode *glob_block = glob->node;
            int found_const = 0;
            if (glob_block) {
                for (size_t gi = 0; gi < glob_block->child_count; ++gi) {
                    JZASTNode *decl = glob_block->children[gi];
                    if (!decl || decl->type != JZ_AST_CONST_DECL || !decl->name) continue;
                    if (strcmp(decl->name, rest) == 0) {
                        found_const = 1;
                        break;
                    }
                }
            }

            if (!found_const) {
                sem_report_rule(diagnostics,
                                node->loc,
                                "GLOBAL_CONST_USE_UNDECLARED",
                                "reference to undeclared GLOBAL.<name> constant");
            }
            return;
        }
    }

    if (!mod_scope) {
        report_undeclared(diagnostics, node);
        return;
    }

    const JZSymbol *head_sym = module_scope_lookup(mod_scope, head);
    if (!head_sym) {
        report_undeclared(diagnostics, node);
        return;
    }

    char tail[256] = {0};
    if (rest && *rest) {
        size_t tail_len = second_dot ? (size_t)(second_dot - rest) : strlen(rest);
        if (tail_len >= sizeof(tail)) tail_len = sizeof(tail) - 1;
        memcpy(tail, rest, tail_len);
        tail[tail_len] = '\0';
    }

    if (head_sym->kind == JZ_SYM_MEM) {
        if (!tail[0]) {
            sem_report_rule(diagnostics,
                            node->loc,
                            "MEM_PORT_UNDEFINED",
                            "memory port name missing after '.'");
            return;
        }

        JZASTNode *mem_decl = head_sym->node;
        int found = 0;
        if (mem_decl) {
            for (size_t i = 0; i < mem_decl->child_count; ++i) {
                JZASTNode *mp = mem_decl->children[i];
                if (mp && mp->type == JZ_AST_MEM_PORT && mp->name && strcmp(mp->name, tail) == 0) {
                    found = 1;
                    break;
                }
            }
        }

        if (!found) {
            sem_report_rule(diagnostics,
                            node->loc,
                            "MEM_PORT_UNDEFINED",
                            "access to undefined MEM port");
        } else if (second_dot) {
            const char *field = second_dot + 1;
            if (!field || !*field ||
                (strcmp(field, "addr") != 0 && strcmp(field, "data") != 0 &&
                 strcmp(field, "wdata") != 0)) {
                sem_report_rule(diagnostics,
                                node->loc,
                                "MEM_PORT_FIELD_UNDEFINED",
                                "invalid MEM port field; expected '.addr', '.data', or '.wdata'");
            }
        }
        return;
    }

    if (head_sym->kind == JZ_SYM_INSTANCE) {
        if (!tail[0]) {
            report_undeclared(diagnostics, node);
            return;
        }

        JZASTNode *inst_node = head_sym->node;
        const char *child_mod_name = inst_node ? inst_node->text : NULL;
        if (!child_mod_name) {
            sem_report_rule(diagnostics,
                            node->loc,
                            "INSTANCE_UNDEFINED_MODULE",
                            "instance references undefined module/blackbox");
            return;
        }

        const JZSymbol *child_sym = project_lookup_module_or_blackbox(project_symbols,
                                                                       child_mod_name);
        if (!child_sym || !child_sym->node) {
            sem_report_rule(diagnostics,
                            node->loc,
                            "INSTANCE_UNDEFINED_MODULE",
                            "instance references undefined module/blackbox");
            return;
        }

        JZASTNode *child_mod = child_sym->node;
        int found = 0;
        for (size_t i = 0; i < child_mod->child_count; ++i) {
            JZASTNode *blk = child_mod->children[i];
            if (!blk || blk->type != JZ_AST_PORT_BLOCK) continue;
            for (size_t j = 0; j < blk->child_count; ++j) {
                JZASTNode *pd = blk->children[j];
                if (pd && pd->type == JZ_AST_PORT_DECL && pd->name && strcmp(pd->name, tail) == 0) {
                    found = 1;

                    /* Per spec, instance-qualified access is only meaningful for
                     * child OUT/INOUT ports. IN ports are considered internal
                     * to the child module's interface: they may be driven from
                     * the parent via the @new binding, but are not readable as
                     * `inst.port` expressions. Treat `inst.<in_port>` as an
                     * undeclared/invalid reference.
                     */
                    const char *dir = pd->block_kind ? pd->block_kind : "";
                    if (strcmp(dir, "IN") == 0) {
                        sem_report_rule(diagnostics,
                                        node->loc,
                                        "UNDECLARED_IDENTIFIER",
                                        "instance input port is not readable from parent module");
                        return;
                    }
                    break;
                }
            }
            if (found) break;
        }

        if (!found) {
            sem_report_rule(diagnostics,
                            node->loc,
                            "UNDECLARED_IDENTIFIER",
                            "reference to undefined instance port");
        }
        return;
    }

    report_undeclared(diagnostics, node);
}

void resolve_names_recursive(JZASTNode *node,
                             JZBuffer *module_scopes,
                             const JZBuffer *project_symbols,
                             const JZModuleScope *current_scope,
                             JZDiagnosticList *diagnostics)
{
    if (!node) return;

    if (node->type == JZ_AST_MODULE || node->type == JZ_AST_BLACKBOX) {
        JZModuleScope *scope = find_module_scope_for_node(module_scopes, node);
        const JZModuleScope *next_scope = scope ? scope : current_scope;
        for (size_t i = 0; i < node->child_count; ++i) {
            resolve_names_recursive(node->children[i], module_scopes, project_symbols,
                                    next_scope, diagnostics);
        }
        return;
    }

    /* SYNCHRONOUS header parameters carry small enum-like values for EDGE,
     * RESET_ACTIVE, and RESET_TYPE. These should not participate in normal
     * identifier resolution (and must not trigger UNDECLARED_IDENTIFIER),
     * while CLK and RESET still refer to real signals in module scope.
     */
    if (node->type == JZ_AST_SYNC_PARAM && node->name) {
        const char *param = node->name;
        if (strcmp(param, "CLK") == 0 || strcmp(param, "RESET") == 0) {
            /* Resolve signal expressions for CLK/RESET normally. */
            for (size_t i = 0; i < node->child_count; ++i) {
                resolve_names_recursive(node->children[i], module_scopes,
                                        project_symbols, current_scope,
                                        diagnostics);
            }
        } else {
            /* EDGE/RESET_ACTIVE/RESET_TYPE and any future enum-like params:
             * skip name resolution so bare identifiers like Rising/Low do not
             * require declarations in module scope.
             */
        }
        return;
    }

    if (node->type == JZ_AST_EXPR_IDENTIFIER) {
        resolve_identifier_node(node, current_scope, project_symbols, diagnostics);
    } else if (node->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER) {
        resolve_qualified_identifier_node(node, current_scope, project_symbols, diagnostics);
    } else if (node->type == JZ_AST_EXPR_BUS_ACCESS) {
        if (current_scope && project_symbols) {
            JZBusAccessInfo info;
            (void)sem_resolve_bus_access(node, current_scope, project_symbols, &info, diagnostics);
        }
    }

    for (size_t i = 0; i < node->child_count; ++i) {
        resolve_names_recursive(node->children[i], module_scopes, project_symbols,
                                current_scope, diagnostics);
    }
}
