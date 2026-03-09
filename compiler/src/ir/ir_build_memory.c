/**
 * @file ir_mem_bind.c
 * @brief Memory port binding pass for IR construction.
 *
 * This file implements a post-IR-build binding pass that infers concrete
 * signal connections for IR_MemoryPort entries by walking the AST while
 * semantic context (JZModuleScope, JZMemPortRef) is still available.
 *
 * The pass runs after jz_ir_build_design() has constructed IR_Module and
 * IR_Memory objects, but before semantic scopes are discarded.
 */

#include <string.h>
#include <ctype.h>

#include "../../include/ast.h"
#include "../../include/ir_mem_bind.h"
#include "../sem/driver_internal.h"
#include "ir_internal.h"

/**
 * @brief Find an IR_Memory by name within a module.
 *
 * @param mod  IR module.
 * @param name Memory name.
 * @return Pointer to IR_Memory, or NULL if not found.
 */
static IR_Memory *ir_find_memory_by_name(IR_Module *mod, const char *name)
{
    if (!mod || !name || !mod->memories || mod->num_memories <= 0) {
        return NULL;
    }
    for (int i = 0; i < mod->num_memories; ++i) {
        IR_Memory *m = &mod->memories[i];
        if (m->name && strcmp(m->name, name) == 0) {
            return m;
        }
    }
    return NULL;
}

/**
 * @brief Find an IR_MemoryPort by name on a given memory.
 *
 * @param mem       IR memory.
 * @param port_name Port name.
 * @return Pointer to IR_MemoryPort, or NULL if not found.
 */
static IR_MemoryPort *ir_find_memory_port_by_name(IR_Memory *mem, const char *port_name)
{
    if (!mem || !port_name || !mem->ports || mem->num_ports <= 0) {
        return NULL;
    }
    for (int i = 0; i < mem->num_ports; ++i) {
        IR_MemoryPort *mp = &mem->ports[i];
        if (mp->name && strcmp(mp->name, port_name) == 0) {
            return mp;
        }
    }
    return NULL;
}

/**
 * @brief Match a bare qualified identifier of the form mem.port.
 *
 * Mirrors sem_match_mem_port_qualified_ident() from the semantic layer,
 * but is restricted to identifier-only forms without indexing.
 *
 * @param expr  AST expression node.
 * @param scope Module scope.
 * @param out   Output memory/port reference.
 * @return 1 if matched, 0 otherwise.
 */
static int ir_match_mem_port_qualified_ident(JZASTNode *expr,
                                             const JZModuleScope *scope,
                                             JZMemPortRef *out)
{
    if (!expr || !scope || !expr->name ||
        (expr->type != JZ_AST_EXPR_IDENTIFIER &&
         expr->type != JZ_AST_EXPR_QUALIFIED_IDENTIFIER)) {
        return 0;
    }

    const char *qualified = expr->name;
    const char *dot = strchr(qualified, '.');
    if (!dot || dot == qualified) {
        return 0;
    }

    char mem_name[256];
    size_t mem_len = (size_t)(dot - qualified);
    if (mem_len == 0 || mem_len >= sizeof(mem_name)) {
        return 0;
    }
    memcpy(mem_name, qualified, mem_len);
    mem_name[mem_len] = '\0';

    const JZSymbol *mem_sym = module_scope_lookup_kind(scope, mem_name, JZ_SYM_MEM);
    if (!mem_sym || !mem_sym->node) {
        return 0;
    }

    const char *port_str = dot + 1;
    const char *field_str = NULL;
    const char *second_dot = strchr(port_str, '.');
    if (second_dot) {
        if (!*(second_dot + 1)) {
            return 0;
        }
        field_str = second_dot + 1;
    }

    JZASTNode *mem_decl = mem_sym->node;
    JZASTNode *found_port = NULL;
    for (size_t i = 0; i < mem_decl->child_count; ++i) {
        JZASTNode *child = mem_decl->children[i];
        if (!child || child->type != JZ_AST_MEM_PORT || !child->name) {
            continue;
        }
        size_t port_len = second_dot ? (size_t)(second_dot - port_str) : strlen(port_str);
        if (port_len > 0 && strlen(child->name) == port_len &&
            strncmp(child->name, port_str, port_len) == 0) {
            found_port = child;
            break;
        }
    }

    if (!found_port) {
        return 0;
    }

    if (out) {
        out->mem_decl = mem_decl;
        out->port = found_port;
        out->field = MEM_PORT_FIELD_NONE;
        if (field_str) {
            if (strcmp(field_str, "addr") == 0) {
                out->field = MEM_PORT_FIELD_ADDR;
            } else if (strcmp(field_str, "data") == 0) {
                out->field = MEM_PORT_FIELD_DATA;
            } else {
                return 0;
            }
        }
    }
    return 1;
}

/**
 * @brief Extract a semantic signal ID from an expression.
 *
 * Accepts plain identifiers referring to PORT, WIRE, or REGISTER symbols,
 * or qualified identifiers referring to expanded BUS signals (e.g., pbus.ADDR
 * resolves to the IR signal pbus_ADDR).
 *
 * @param scope Module scope.
 * @param mod   IR module (for looking up expanded BUS signals).
 * @param expr  AST expression.
 * @return Signal ID, or -1 if unsupported.
 */
static int ir_extract_signal_id_from_simple_expr(const JZModuleScope *scope,
                                                 const IR_Module *mod,
                                                 JZASTNode *expr)
{
    if (!scope || !expr) {
        return -1;
    }

    /* Handle plain identifiers via module scope lookup. */
    if (expr->type == JZ_AST_EXPR_IDENTIFIER && expr->name) {
        const JZSymbol *sym = module_scope_lookup(scope, expr->name);
        if (!sym) {
            return -1;
        }
        if (sym->kind != JZ_SYM_PORT &&
            sym->kind != JZ_SYM_WIRE &&
            sym->kind != JZ_SYM_REGISTER) {
            return -1;
        }
        return sym->id;
    }

    /* Handle qualified identifiers that may refer to expanded BUS signals.
     * For "pbus.ADDR", we look for an IR signal named "pbus_ADDR".
     */
    if (expr->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER && expr->name && mod) {
        const char *full = expr->name;
        const char *dot = strchr(full, '.');
        if (dot && dot != full && dot[1] != '\0') {
            size_t prefix_len = (size_t)(dot - full);
            const char *suffix = dot + 1;

            /* Build expected signal name: prefix_suffix */
            char expected_name[512];
            if (prefix_len + 1 + strlen(suffix) < sizeof(expected_name)) {
                memcpy(expected_name, full, prefix_len);
                expected_name[prefix_len] = '_';
                strcpy(expected_name + prefix_len + 1, suffix);

                /* Search for signal by name in IR module */
                for (int i = 0; i < mod->num_signals; ++i) {
                    const IR_Signal *sig = &mod->signals[i];
                    if (sig->name && strcmp(sig->name, expected_name) == 0) {
                        return sig->id;
                    }
                }
            }
        }
    }

    return -1;
}

/* -------------------------------------------------------------------------
 *  Write-port binding (MEM_PORT_WRITE)
 * -------------------------------------------------------------------------
 */

/**
 * @brief Accumulate a candidate write-port binding.
 *
 * Tracks address, data-in, and enable signals and detects conflicts.
 *
 * @param mp            Memory port.
 * @param addr_id       Address signal ID.
 * @param data_in_id    Data input signal ID.
 * @param enable_id     Enable signal ID.
 */
static void ir_bind_write_port_candidate(IR_MemoryPort *mp,
                                         int addr_id,
                                         int data_in_id,
                                         int enable_id)
{
    if (!mp) {
        return;
    }

    /* Address */
    if (addr_id >= 0) {
        if (mp->addr_signal_id == -1) {
            mp->addr_signal_id = addr_id;
        } else if (mp->addr_signal_id >= 0 && mp->addr_signal_id != addr_id) {
            mp->addr_signal_id = -2; /* conflict */
        }
    }

    /* Data-in */
    if (data_in_id >= 0) {
        if (mp->data_in_signal_id == -1) {
            mp->data_in_signal_id = data_in_id;
        } else if (mp->data_in_signal_id >= 0 && mp->data_in_signal_id != data_in_id) {
            mp->data_in_signal_id = -2; /* conflict */
        }
    }

    /* Enable */
    if (enable_id >= 0) {
        if (mp->enable_signal_id == -1) {
            mp->enable_signal_id = enable_id;
        } else if (mp->enable_signal_id >= 0 && mp->enable_signal_id != enable_id) {
            mp->enable_signal_id = -2; /* conflict */
        }
    }
}

/**
 * @brief Walk a statement subtree and bind memory write ports.
 *
 * Matches patterns of the form:
 *   mem.port[addr] <= data;
 *
 * inside SYNCHRONOUS blocks.
 *
 * @param scope            Module scope.
 * @param mod              IR module.
 * @param stmt             AST statement node.
 * @param enable_signal_id Current enable/gating signal.
 */
static void ir_bind_mem_writes_in_stmt(const JZModuleScope *scope,
                                       IR_Module *mod,
                                       JZASTNode *stmt,
                                       int enable_signal_id)
{
    if (!scope || !mod || !stmt) {
        return;
    }

    switch (stmt->type) {
    case JZ_AST_STMT_ASSIGN:
        if (stmt->child_count >= 2) {
            JZASTNode *lhs = stmt->children[0];
            JZASTNode *rhs = stmt->children[1];
            if (!lhs || lhs->type != JZ_AST_EXPR_SLICE) {
                return;
            }

            JZMemPortRef ref;
            memset(&ref, 0, sizeof(ref));
            if (!sem_match_mem_port_slice(lhs, scope, NULL, &ref)) {
                return; /* not a mem.port[addr] access */
            }
            if (!ref.mem_decl || !ref.mem_decl->name ||
                !ref.port || !ref.port->name || !ref.port->block_kind) {
                return;
            }
            if (strcmp(ref.port->block_kind, "IN") != 0) {
                return; /* only MEM IN write ports participate */
            }

            IR_Memory *mem = ir_find_memory_by_name(mod, ref.mem_decl->name);
            if (!mem) {
                return;
            }
            IR_MemoryPort *mp = ir_find_memory_port_by_name(mem, ref.port->name);
            if (!mp || mp->kind != MEM_PORT_WRITE) {
                return;
            }

            int addr_id = -1;
            if (lhs->child_count >= 2) {
                JZASTNode *addr_expr = lhs->children[1];
                addr_id = ir_extract_signal_id_from_simple_expr(scope, mod, addr_expr);
            }

            int data_id = ir_extract_signal_id_from_simple_expr(scope, mod, rhs);

            int en_id = (enable_signal_id >= 0) ? enable_signal_id : -1;
            ir_bind_write_port_candidate(mp, addr_id, data_id, en_id);
        }
        break;

    case JZ_AST_STMT_IF:
    case JZ_AST_STMT_ELIF:
        if (stmt->child_count > 0) {
            JZASTNode *cond = stmt->children[0];
            int cond_id = ir_extract_signal_id_from_simple_expr(scope, mod, cond);

            int new_enable = enable_signal_id;
            if (cond_id >= 0) {
                if (new_enable < 0) {
                    new_enable = cond_id;
                } else if (new_enable >= 0 && new_enable != cond_id) {
                    new_enable = -2; /* conflicting/unsupported gating */
                }
            }

            for (size_t i = 1; i < stmt->child_count; ++i) {
                JZASTNode *child = stmt->children[i];
                if (!child) continue;
                ir_bind_mem_writes_in_stmt(scope, mod, child, new_enable);
            }
        }
        break;

    case JZ_AST_STMT_ELSE:
    case JZ_AST_STMT_SELECT:
    case JZ_AST_STMT_CASE:
    case JZ_AST_STMT_DEFAULT:
    case JZ_AST_BLOCK:
        for (size_t i = 0; i < stmt->child_count; ++i) {
            JZASTNode *child = stmt->children[i];
            if (!child) continue;
            ir_bind_mem_writes_in_stmt(scope, mod, child, enable_signal_id);
        }
        break;

    default:
        for (size_t i = 0; i < stmt->child_count; ++i) {
            JZASTNode *child = stmt->children[i];
            if (!child) continue;
            ir_bind_mem_writes_in_stmt(scope, mod, child, enable_signal_id);
        }
        break;
    }
}

/**
 * @brief Bind all memory write ports for a module.
 *
 * Walks all SYNCHRONOUS blocks and normalizes conflicts.
 *
 * @param scope Module scope.
 * @param mod   IR module.
 */
static void ir_bind_mem_write_ports_for_module(const JZModuleScope *scope,
                                               IR_Module *mod)
{
    if (!scope || !scope->node || !mod || mod->num_memories <= 0) {
        return;
    }

    JZASTNode *ast_mod = scope->node;

    /* Walk SYNCHRONOUS blocks only; MEM IN writes are only legal there. */
    for (size_t i = 0; i < ast_mod->child_count; ++i) {
        JZASTNode *child = ast_mod->children[i];
        if (!child || child->type != JZ_AST_BLOCK || !child->block_kind) {
            continue;
        }
        if (strcmp(child->block_kind, "SYNCHRONOUS") != 0) {
            continue;
        }

        ir_bind_mem_writes_in_stmt(scope, mod, child, -1);
    }

    /* Normalize conflicts (-2) back to -1 for write ports. */
    for (int mi = 0; mi < mod->num_memories; ++mi) {
        IR_Memory *m = &mod->memories[mi];
        if (!m->ports || m->num_ports <= 0) {
            continue;
        }
        for (int pi = 0; pi < m->num_ports; ++pi) {
            IR_MemoryPort *mp = &m->ports[pi];
            if (mp->kind != MEM_PORT_WRITE) {
                continue;
            }
            if (mp->addr_signal_id < 0) {
                mp->addr_signal_id = -1;
            }
            if (mp->data_in_signal_id < 0) {
                mp->data_in_signal_id = -1;
            }
            if (mp->enable_signal_id < 0) {
                mp->enable_signal_id = -1;
            }
        }
    }
}

/* -------------------------------------------------------------------------
 *  Async-read binding (MEM_PORT_READ_ASYNC)
 * -------------------------------------------------------------------------
 */

/**
 * @brief Accumulate a candidate async-read binding.
 *
 * @param mp           Memory port.
 * @param addr_id      Address signal ID.
 * @param data_out_id  Data output signal ID.
 */
static void ir_bind_async_read_port_candidate(IR_MemoryPort *mp,
                                              int addr_id,
                                              int data_out_id)
{
    if (!mp) {
        return;
    }

    /* Address */
    if (addr_id >= 0) {
        if (mp->addr_signal_id == -1) {
            mp->addr_signal_id = addr_id;
        } else if (mp->addr_signal_id >= 0 && mp->addr_signal_id != addr_id) {
            mp->addr_signal_id = -2; /* conflict */
        }
    }

    /* Data-out */
    if (data_out_id >= 0) {
        if (mp->data_out_signal_id == -1) {
            mp->data_out_signal_id = data_out_id;
        } else if (mp->data_out_signal_id >= 0 && mp->data_out_signal_id != data_out_id) {
            mp->data_out_signal_id = -2; /* conflict */
        }
    }
}

/**
 * @brief Walk a statement subtree and bind async-read memory ports.
 *
 * Matches patterns of the form:
 *   dest <= mem.port[addr];
 *
 * inside ASYNCHRONOUS blocks.
 *
 * @param scope Module scope.
 * @param mod   IR module.
 * @param stmt  AST statement node.
 */
static void ir_bind_mem_async_reads_in_stmt(const JZModuleScope *scope,
                                            IR_Module *mod,
                                            JZASTNode *stmt)
{
    if (!scope || !mod || !stmt) {
        return;
    }

    switch (stmt->type) {
    case JZ_AST_STMT_ASSIGN:
        if (stmt->child_count >= 2) {
            JZASTNode *lhs = stmt->children[0];
            JZASTNode *rhs = stmt->children[1];
            if (!rhs || rhs->type != JZ_AST_EXPR_SLICE) {
                break;
            }

            JZMemPortRef ref;
            memset(&ref, 0, sizeof(ref));
            if (!sem_match_mem_port_slice(rhs, scope, NULL, &ref)) {
                break; /* not a mem.port[addr] access */
            }
            if (!ref.mem_decl || !ref.mem_decl->name ||
                !ref.port || !ref.port->name || !ref.port->block_kind) {
                break;
            }
            if (strcmp(ref.port->block_kind, "OUT") != 0) {
                break; /* async reads are from OUT ports */
            }

            IR_Memory *mem = ir_find_memory_by_name(mod, ref.mem_decl->name);
            if (!mem) {
                break;
            }
            IR_MemoryPort *mp = ir_find_memory_port_by_name(mem, ref.port->name);
            if (!mp || mp->kind != MEM_PORT_READ_ASYNC) {
                break;
            }

            int dest_id = ir_extract_signal_id_from_simple_expr(scope, mod, lhs);

            int addr_id = -1;
            if (rhs->child_count >= 2) {
                /* For mem.port[addr], parser duplicates [addr] into msb/lsb; treat
                 * msb (child[1]) as the address expression.
                 */
                JZASTNode *addr_expr = rhs->children[1];
                addr_id = ir_extract_signal_id_from_simple_expr(scope, mod, addr_expr);
            }

            ir_bind_async_read_port_candidate(mp, addr_id, dest_id);
        }
        break;

    case JZ_AST_STMT_IF:
    case JZ_AST_STMT_ELIF:
    case JZ_AST_STMT_ELSE:
    case JZ_AST_STMT_SELECT:
    case JZ_AST_STMT_CASE:
    case JZ_AST_STMT_DEFAULT:
    case JZ_AST_BLOCK:
        for (size_t i = 0; i < stmt->child_count; ++i) {
            JZASTNode *child = stmt->children[i];
            if (!child) continue;
            ir_bind_mem_async_reads_in_stmt(scope, mod, child);
        }
        break;

    default:
        for (size_t i = 0; i < stmt->child_count; ++i) {
            JZASTNode *child = stmt->children[i];
            if (!child) continue;
            ir_bind_mem_async_reads_in_stmt(scope, mod, child);
        }
        break;
    }
}

/**
 * @brief Bind all async-read memory ports for a module.
 *
 * @param scope Module scope.
 * @param mod   IR module.
 */
static void ir_bind_mem_async_read_ports_for_module(const JZModuleScope *scope,
                                                    IR_Module *mod)
{
    if (!scope || !scope->node || !mod || mod->num_memories <= 0) {
        return;
    }

    JZASTNode *ast_mod = scope->node;

    /* Walk ASYNCHRONOUS blocks only; async reads are only valid there. */
    for (size_t i = 0; i < ast_mod->child_count; ++i) {
        JZASTNode *child = ast_mod->children[i];
        if (!child || child->type != JZ_AST_BLOCK || !child->block_kind) {
            continue;
        }
        if (strcmp(child->block_kind, "ASYNCHRONOUS") != 0) {
            continue;
        }

        ir_bind_mem_async_reads_in_stmt(scope, mod, child);
    }

    /* Normalize conflicts (-2) back to -1 for async read ports. */
    for (int mi = 0; mi < mod->num_memories; ++mi) {
        IR_Memory *m = &mod->memories[mi];
        if (!m->ports || m->num_ports <= 0) {
            continue;
        }
        for (int pi = 0; pi < m->num_ports; ++pi) {
            IR_MemoryPort *mp = &m->ports[pi];
            if (mp->kind != MEM_PORT_READ_ASYNC) {
                continue;
            }
            if (mp->addr_signal_id < 0) {
                mp->addr_signal_id = -1;
            }
            if (mp->data_out_signal_id < 0) {
                mp->data_out_signal_id = -1;
            }
        }
    }
}

/**
 * @brief Detect MEM(TYPE=BLOCK) in a MEM block header.
 *
 * @param attrs Attribute string.
 * @return 1 if TYPE=BLOCK, 0 otherwise.
 */
static int ir_mem_block_is_type_block(const char *attrs)
{
    if (!attrs) return 0;
    const char *p = strstr(attrs, "TYPE");
    if (!p) p = strstr(attrs, "type");
    if (!p) return 0;
    p += 4; /* skip TYPE */
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '=') return 0;
    p++; /* skip '=' */
    while (*p && isspace((unsigned char)*p)) p++;
    if (strncmp(p, "BLOCK", 5) == 0 || strncmp(p, "block", 5) == 0) {
        return 1;
    }
    return 0;
}

/* -------------------------------------------------------------------------
 *  Sync-read binding (MEM_PORT_READ_SYNC)
 * -------------------------------------------------------------------------
 */

/**
 * @brief Walk a statement subtree and bind synchronous memory read ports.
 *
 * Handles:
 *   mem.port <= addr;
 *   dest <= mem.port;
 *   dest <= mem.port[addr];
 *
 * inside SYNCHRONOUS blocks.
 *
 * @param scope Module scope.
 * @param mod   IR module.
 * @param stmt  AST statement node.
 */
static void ir_bind_mem_sync_reads_in_stmt(const JZModuleScope *scope,
                                           IR_Module *mod,
                                           JZASTNode *stmt,
                                           int clock_domain_idx)
{
    if (!scope || !mod || !stmt) {
        return;
    }

    switch (stmt->type) {
    case JZ_AST_STMT_ASSIGN:
        if (stmt->child_count >= 2) {
            JZASTNode *lhs = stmt->children[0];
            JZASTNode *rhs = stmt->children[1];

            /* Address-sample: mem.port.addr <= addr_expr; */
            JZMemPortRef ref;
            memset(&ref, 0, sizeof(ref));
            if (ir_match_mem_port_qualified_ident(lhs, scope, &ref) &&
                ref.port && ref.port->block_kind &&
                strcmp(ref.port->block_kind, "OUT") == 0 &&
                ref.port->text && strcmp(ref.port->text, "SYNC") == 0 &&
                ref.field == MEM_PORT_FIELD_ADDR) {
                IR_Memory *mem = ir_find_memory_by_name(mod, ref.mem_decl->name);
                if (mem) {
                    IR_MemoryPort *mp = ir_find_memory_port_by_name(mem, ref.port->name);
                    if (mp && mp->kind == MEM_PORT_READ_SYNC) {
                        int addr_id = ir_extract_signal_id_from_simple_expr(scope, mod, rhs);
                        ir_bind_async_read_port_candidate(mp, addr_id, -1);
                        if (addr_id >= 0 && clock_domain_idx >= 0) {
                            mp->sync_clock_domain_id = clock_domain_idx;
                        }
                    }
                }
            }

            /* Data-consume: dst <= mem.port.data; */
            memset(&ref, 0, sizeof(ref));
            if (ir_match_mem_port_qualified_ident(rhs, scope, &ref) &&
                ref.port && ref.port->block_kind &&
                strcmp(ref.port->block_kind, "OUT") == 0 &&
                ref.port->text && strcmp(ref.port->text, "SYNC") == 0 &&
                ref.field == MEM_PORT_FIELD_DATA) {
                IR_Memory *mem = ir_find_memory_by_name(mod, ref.mem_decl->name);
                if (mem) {
                    IR_MemoryPort *mp = ir_find_memory_port_by_name(mem, ref.port->name);
                    if (mp && mp->kind == MEM_PORT_READ_SYNC) {
                        int dest_id = ir_extract_signal_id_from_simple_expr(scope, mod, lhs);
                        ir_bind_async_read_port_candidate(mp, -1, dest_id);
                    }
                }
            }
        }
        break;

    case JZ_AST_STMT_IF:
    case JZ_AST_STMT_ELIF:
    case JZ_AST_STMT_ELSE:
    case JZ_AST_STMT_SELECT:
    case JZ_AST_STMT_CASE:
    case JZ_AST_STMT_DEFAULT:
    case JZ_AST_BLOCK:
        for (size_t i = 0; i < stmt->child_count; ++i) {
            JZASTNode *child = stmt->children[i];
            if (!child) continue;
            ir_bind_mem_sync_reads_in_stmt(scope, mod, child, clock_domain_idx);
        }
        break;

    default:
        for (size_t i = 0; i < stmt->child_count; ++i) {
            JZASTNode *child = stmt->children[i];
            if (!child) continue;
            ir_bind_mem_sync_reads_in_stmt(scope, mod, child, clock_domain_idx);
        }
        break;
    }
}

/**
 * @brief Bind all synchronous read memory ports for a module.
 *
 * @param scope Module scope.
 * @param mod   IR module.
 */
static void ir_bind_mem_sync_read_ports_for_module(const JZModuleScope *scope,
                                                   IR_Module *mod)
{
    if (!scope || !scope->node || !mod || mod->num_memories <= 0) {
        return;
    }

    JZASTNode *ast_mod = scope->node;

    /* Walk SYNCHRONOUS blocks only; sync reads are only valid there.
     * Track the clock domain index so we can record which domain owns
     * each SYNC read port's address capture.
     */
    int sync_block_idx = 0;
    for (size_t i = 0; i < ast_mod->child_count; ++i) {
        JZASTNode *child = ast_mod->children[i];
        if (!child || child->type != JZ_AST_BLOCK || !child->block_kind) {
            continue;
        }
        if (strcmp(child->block_kind, "SYNCHRONOUS") != 0) {
            continue;
        }

        int clock_domain_idx = (sync_block_idx < mod->num_clock_domains)
                              ? sync_block_idx : -1;
        ir_bind_mem_sync_reads_in_stmt(scope, mod, child, clock_domain_idx);
        sync_block_idx++;
    }

    /* Normalize conflicts (-2) back to -1 for sync read ports. */
    for (int mi = 0; mi < mod->num_memories; ++mi) {
        IR_Memory *m = &mod->memories[mi];
        if (!m->ports || m->num_ports <= 0) {
            continue;
        }
        for (int pi = 0; pi < m->num_ports; ++pi) {
            IR_MemoryPort *mp = &m->ports[pi];
            if (mp->kind != MEM_PORT_READ_SYNC) {
                continue;
            }
            if (mp->addr_signal_id < 0) {
                mp->addr_signal_id = -1;
            }
            if (mp->data_out_signal_id < 0) {
                mp->data_out_signal_id = -1;
            }
        }
    }
}

/* -------------------------------------------------------------------------
 *  Public pass entry point
 * -------------------------------------------------------------------------
 */

/**
 * @brief Bind memory ports for all base modules in an IR design.
 *
 * This pass infers concrete signal connections for:
 * - MEM_PORT_WRITE
 * - MEM_PORT_READ_ASYNC
 * - MEM_PORT_READ_SYNC
 *
 * Specializations inherit bindings from their base modules.
 *
 * @param design         IR design.
 * @param module_scopes  Module scopes buffer.
 * @param project_symbols Project symbols (reserved for future use).
 * @param diagnostics    Diagnostic sink (currently unused).
 * @return 0 on success, non-zero on failure.
 */
int jz_ir_bind_memory_ports(IR_Design *design,
                            JZBuffer  *module_scopes,
                            const JZBuffer *project_symbols,
                            JZDiagnosticList *diagnostics)
{
    (void)project_symbols; /* currently unused but reserved for future use */
    (void)diagnostics;     /* binding is best-effort and does not emit errors */

    if (!design || !module_scopes || !module_scopes->data) {
        return -1;
    }

    size_t scope_count = module_scopes->len / sizeof(JZModuleScope);
    JZModuleScope *scopes = (JZModuleScope *)module_scopes->data;

    if (design->num_modules <= 0 || !design->modules) {
        return 0;
    }

    /* Base modules occupy [0, scope_count); specializations copy their
     * memories from these bases and therefore inherit the bindings.
     */
    for (size_t i = 0; i < scope_count && (int)i < design->num_modules; ++i) {
        IR_Module *mod = &design->modules[i];
        const JZModuleScope *scope = &scopes[i];
        if (!scope->node || mod->num_memories <= 0) {
            continue;
        }

        /* WRITE, READ_ASYNC, READ_SYNC port binding. */
        ir_bind_mem_write_ports_for_module(scope, mod);
        ir_bind_mem_async_read_ports_for_module(scope, mod);
        ir_bind_mem_sync_read_ports_for_module(scope, mod);
    }

    return 0;
}


/**
 * @brief Build IR_Memory entries for a module.
 *
 * Walks MEM declarations in the module AST and constructs IR_Memory and
 * IR_MemoryPort objects with resolved widths, depths, and initialization.
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
                                 int *out_count)
{
    if (!scope || !scope->node || !arena || !out_mems || !out_count) {
        return -1;
    }

    JZASTNode *mod = scope->node;

    /* First pass: count MEM declarations. */
    int mem_count = 0;
    for (size_t i = 0; i < mod->child_count; ++i) {
        JZASTNode *child = mod->children[i];
        if (!child || child->type != JZ_AST_MEM_BLOCK) {
            continue;
        }
        for (size_t j = 0; j < child->child_count; ++j) {
            JZASTNode *mem = child->children[j];
            if (!mem || mem->type != JZ_AST_MEM_DECL) {
                continue;
            }
            mem_count++;
        }
    }

    if (mem_count == 0) {
        *out_mems = NULL;
        *out_count = 0;
        return 0;
    }

    IR_Memory *mems = (IR_Memory *)jz_arena_alloc(arena, sizeof(IR_Memory) * (size_t)mem_count);
    if (!mems) {
        return -1;
    }
    memset(mems, 0, sizeof(IR_Memory) * (size_t)mem_count);

    int mem_idx = 0;
    for (size_t i = 0; i < mod->child_count && mem_idx < mem_count; ++i) {
        JZASTNode *child = mod->children[i];
        if (!child || child->type != JZ_AST_MEM_BLOCK) {
            continue;
        }

        int type_is_block = ir_mem_block_is_type_block(child->text);

        for (size_t j = 0; j < child->child_count && mem_idx < mem_count; ++j) {
            JZASTNode *mem_decl = child->children[j];
            if (!mem_decl || mem_decl->type != JZ_AST_MEM_DECL) {
                continue;
            }

            IR_Memory *ir_mem = &mems[mem_idx++];
            ir_mem->id = mem_idx - 1;
            ir_mem->name = mem_decl->name ? ir_strdup_arena(arena, mem_decl->name)
                                          : ir_strdup_arena(arena, "");
            ir_mem->kind = type_is_block ? MEM_KIND_BLOCK : MEM_KIND_DISTRIBUTED;

            /* Resolve word width and depth using semantic helpers. */
            unsigned word_w = 0;
            if (mem_decl->width &&
                sem_eval_width_expr(mem_decl->width,
                                     scope,
                                     project_symbols,
                                     &word_w) == 0 &&
                word_w > 0u) {
                ir_mem->word_width = (int)word_w;
            } else {
                ir_mem->word_width = 0;
            }

            long long depth_val = 0;
            if (mem_decl->text &&
                sem_eval_const_expr_in_module(mem_decl->text,
                                              scope,
                                              project_symbols,
                                              &depth_val) == 0 &&
                depth_val > 0) {
                ir_mem->depth = (int)depth_val;
            } else {
                ir_mem->depth = 0;
            }

            /* Compute address_width as ceil(log2(depth)), with 0 meaning
             * unknown/unset when depth is not a positive integer.
             */
            ir_mem->address_width = 0;
            if (ir_mem->depth > 0) {
                int aw = 0;
                int d = ir_mem->depth - 1;
                while (d > 0) {
                    aw++;
                    d >>= 1;
                }
                if (aw == 0) aw = 1;
                ir_mem->address_width = aw;
            }

            /* Initialization: detect simple sized-literal vs. file-based init.
             * sem_check_module_mem_and_mux_decls has already validated the
             * forms, so here we only distinguish literal vs. @file payload.
             */
            ir_mem->init_is_file = false;
            memset(ir_mem->init.literal.words, 0, sizeof(ir_mem->init.literal.words));
            ir_mem->init.literal.width = 0;

            JZASTNode *init_expr = NULL;
            if (mem_decl->child_count > 0) {
                JZASTNode *first = mem_decl->children[0];
                if (first && first->type != JZ_AST_MEM_PORT) {
                    init_expr = first;
                }
            }

            if (init_expr) {
                if (init_expr->block_kind &&
                    strcmp(init_expr->block_kind, "FILE_REF") == 0 &&
                    init_expr->name) {
                    /* @file(CONST/CONFIG reference) — the semantic pass has
                     * already resolved the string and stored it in text. */
                    if (init_expr->text) {
                        ir_mem->init_is_file = true;
                        ir_mem->init.file_path = ir_strdup_arena(arena, init_expr->text);
                    }
                } else if (init_expr->type == JZ_AST_EXPR_LITERAL &&
                           init_expr->text) {
                    IR_Literal lit;
                    if (ir_decode_sized_literal(init_expr->text,
                                                scope,
                                                project_symbols,
                                                &lit) == 0) {
                        ir_mem->init.literal = lit;
                        ir_mem->init_is_file = false;
                    } else {
                        ir_mem->init_is_file = true;
                        ir_mem->init.file_path = ir_strdup_arena(arena, init_expr->text);
                    }
                }
            }

            /* MEM ports: build IR_MemoryPort[] with direction, kind, and
             * address_width. In the current IR, synchronous read ports do not
             * allocate an implicit output signal; output_signal_id is left as
             * -1 and synchronous behavior is represented via IR_Stmt
             * assignments that read mem.port[addr] and drive user registers.
             */
            int port_count = 0;
            for (size_t k = 0; k < mem_decl->child_count; ++k) {
                JZASTNode *port = mem_decl->children[k];
                if (!port || port->type != JZ_AST_MEM_PORT || !port->name) {
                    continue;
                }
                port_count++;
            }

            if (port_count > 0) {
                IR_MemoryPort *ports =
                    (IR_MemoryPort *)jz_arena_alloc(arena, sizeof(IR_MemoryPort) * (size_t)port_count);
                if (!ports) {
                    return -1;
                }
                memset(ports, 0, sizeof(IR_MemoryPort) * (size_t)port_count);

                int pi = 0;
                for (size_t k = 0; k < mem_decl->child_count && pi < port_count; ++k) {
                    JZASTNode *port = mem_decl->children[k];
                    if (!port || port->type != JZ_AST_MEM_PORT || !port->name) {
                        continue;
                    }

                    IR_MemoryPort *mp = &ports[pi++];
                    mp->name = ir_strdup_arena(arena, port->name);
                    mp->address_width = ir_mem->address_width;

                    /* Initialize port binding fields; jz_ir_bind_memory_ports
                     * will populate these where it can infer concrete
                     * connections from the AST.
                     */
                    mp->write_mode = WRITE_MODE_FIRST; /* default; may be overridden */
                    mp->addr_signal_id = -1;
                    mp->data_in_signal_id = -1;
                    mp->data_out_signal_id = -1;
                    mp->enable_signal_id = -1;
                    mp->wdata_signal_id = -1;
                    mp->output_signal_id = -1;
                    mp->addr_reg_signal_id = -1;
                    mp->sync_clock_domain_id = -1;

                    const char *dir = port->block_kind;
                    const char *qual = port->text ? port->text : "";

                    if (dir && strcmp(dir, "IN") == 0) {
                        mp->kind = MEM_PORT_WRITE;
                        if (strcmp(qual, "WRITE_FIRST") == 0) {
                            mp->write_mode = WRITE_MODE_FIRST;
                        } else if (strcmp(qual, "READ_FIRST") == 0) {
                            mp->write_mode = WRITE_MODE_READ_FIRST;
                        } else if (strcmp(qual, "NO_CHANGE") == 0) {
                            mp->write_mode = WRITE_MODE_NO_CHANGE;
                        } else {
                            /* Default when qualifier omitted or unrecognized. */
                            mp->write_mode = WRITE_MODE_FIRST;
                        }
                    } else if (dir && strcmp(dir, "OUT") == 0) {
                        if (qual && strcmp(qual, "ASYNC") == 0) {
                            mp->kind = MEM_PORT_READ_ASYNC;
                        } else {
                            mp->kind = MEM_PORT_READ_SYNC;
                        }
                        mp->write_mode = WRITE_MODE_FIRST; /* unused for read */
                    } else if (dir && strcmp(dir, "INOUT") == 0) {
                        /* INOUT = shared read/write single port */
                        mp->kind = MEM_PORT_INOUT;
                        if (strcmp(qual, "READ_FIRST") == 0) {
                            mp->write_mode = WRITE_MODE_READ_FIRST;
                        } else if (strcmp(qual, "NO_CHANGE") == 0) {
                            mp->write_mode = WRITE_MODE_NO_CHANGE;
                        } else {
                            /* Default WRITE_FIRST */
                            mp->write_mode = WRITE_MODE_FIRST;
                        }
                    } else {
                        /* Fallback: treat as read-async if direction is
                         * malformed; semantic layer should have caught this.
                         */
                        mp->kind = MEM_PORT_READ_ASYNC;
                        mp->write_mode = WRITE_MODE_FIRST;
                    }
                }

                ir_mem->ports = ports;
                ir_mem->num_ports = port_count;
            } else {
                ir_mem->ports = NULL;
                ir_mem->num_ports = 0;
            }
        }
    }

    *out_mems = mems;
    *out_count = mem_idx;
    return 0;
}

/**
 * @brief Create synthetic address register signals for SYNC read ports.
 *
 * For each memory with a SYNC read port, creates a SIG_REGISTER signal
 * named "{mem}_{port}_addr" and registers it in the bus_map so that
 * qualified identifier resolution (e.g., "rom.read.addr") finds it.
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
                               int *bus_map_count)
{
    if (!arena || !mod || !bus_map || !bus_map_count) {
        return -1;
    }
    if (mod->num_memories <= 0 || !mod->memories) {
        return 0;
    }

    /* Count how many SYNC read ports need synthetic addr signals. */
    int needed = 0;
    for (int mi = 0; mi < mod->num_memories; ++mi) {
        const IR_Memory *m = &mod->memories[mi];
        for (int pi = 0; pi < m->num_ports; ++pi) {
            const IR_MemoryPort *mp = &m->ports[pi];
            if (mp->kind == MEM_PORT_READ_SYNC) {
                needed++;
            }
        }
    }
    if (needed == 0) {
        return 0;
    }

    /* Synthetic IDs for mem addr registers start at 200000 to avoid
     * collisions with semantic IDs (<100000) and BUS synthetic IDs (100000+).
     */
    int next_id = 200000;

    /* Grow the module's signals array. */
    int new_sig_count = mod->num_signals + needed;
    IR_Signal *new_signals = (IR_Signal *)jz_arena_alloc(
        arena, sizeof(IR_Signal) * (size_t)new_sig_count);
    if (!new_signals) {
        return -1;
    }
    if (mod->signals && mod->num_signals > 0) {
        memcpy(new_signals, mod->signals,
               sizeof(IR_Signal) * (size_t)mod->num_signals);
    }

    /* Grow the bus_map array. */
    int new_map_count = *bus_map_count + needed;
    IR_BusSignalMapping *new_map = (IR_BusSignalMapping *)jz_arena_alloc(
        arena, sizeof(IR_BusSignalMapping) * (size_t)new_map_count);
    if (!new_map) {
        return -1;
    }
    if (*bus_map && *bus_map_count > 0) {
        memcpy(new_map, *bus_map,
               sizeof(IR_BusSignalMapping) * (size_t)*bus_map_count);
    }

    int sig_idx = mod->num_signals;
    int map_idx = *bus_map_count;

    for (int mi = 0; mi < mod->num_memories; ++mi) {
        IR_Memory *m = &mod->memories[mi];
        const char *mem_name = (m->name && m->name[0] != '\0') ? m->name : "jz_mem";

        for (int pi = 0; pi < m->num_ports; ++pi) {
            IR_MemoryPort *mp = &m->ports[pi];
            if (mp->kind != MEM_PORT_READ_SYNC) {
                continue;
            }

            const char *port_name = (mp->name && mp->name[0] != '\0') ? mp->name : "rd";
            int addr_w = mp->address_width > 0 ? mp->address_width : m->address_width;

            /* Build signal name: "{mem}_{port}_addr" */
            char name_buf[256];
            snprintf(name_buf, sizeof(name_buf), "%s_%s_addr", mem_name, port_name);

            int new_id = next_id++;

            /* Create the synthetic SIG_REGISTER signal. */
            IR_Signal *sig = &new_signals[sig_idx++];
            memset(sig, 0, sizeof(*sig));
            sig->id = new_id;
            sig->name = ir_strdup_arena(arena, name_buf);
            sig->kind = SIG_REGISTER;
            sig->width = addr_w > 0 ? addr_w : 1;
            sig->owner_module_id = mod->id;
            sig->source_line = 0;
            sig->can_be_z = false;
            memset(sig->u.reg.reset_value.words, 0, sizeof(sig->u.reg.reset_value.words));
            sig->u.reg.reset_value.width = sig->width;
            sig->u.reg.reset_value.is_z = 0;
            sig->u.reg.reset_value_gnd_vcc = NULL;
            sig->u.reg.home_clock_domain_id = -1;

            /* Record in bus_map so "mem.port.addr" resolves.
             * bus_port_name = mem_name, signal_name = "{port}.addr"
             */
            char sig_name_buf[256];
            snprintf(sig_name_buf, sizeof(sig_name_buf), "%s.addr", port_name);

            IR_BusSignalMapping *bm = &new_map[map_idx++];
            bm->bus_port_name = ir_strdup_arena(arena, mem_name);
            bm->signal_name = ir_strdup_arena(arena, sig_name_buf);
            bm->array_index = -1;
            bm->ir_signal_id = new_id;
            bm->width = sig->width;

            /* Store the synthetic signal ID in the port. */
            mp->addr_reg_signal_id = new_id;
        }
    }

    mod->signals = new_signals;
    mod->num_signals = new_sig_count;
    *bus_map = new_map;
    *bus_map_count = new_map_count;

    return 0;
}
