#include <string.h>
#include <strings.h>
#include <stdlib.h>

#include "sem_driver.h"
#include "sem.h"
#include "util.h"
#include "rules.h"
#include "driver_internal.h"

/* -------------------------------------------------------------------------
 *  SYNCHRONOUS block clock/domain rules (DOMAIN_CONFLICT, DUPLICATE_BLOCK,
 *  MULTI_CLK_ASSIGN)
 * -------------------------------------------------------------------------
 */

typedef struct JZRegisterDomainInfo {
    JZASTNode *decl;          /* REGISTER declaration node */
    char       home_clk[64];  /* clock identifier for home domain */
    int        has_home;      /* 1 if home_clk is valid */
    int        multi_clk_reported; /* avoid duplicate MULTI_CLK_ASSIGN diags */
} JZRegisterDomainInfo;

typedef struct JZCdcAliasDomainInfo {
    JZASTNode *decl;          /* destination alias identifier node */
    char       home_clk[64];  /* clock identifier for alias home domain */
    int        has_home;
} JZCdcAliasDomainInfo;

static JZRegisterDomainInfo *sem_sync_find_reg_info(JZRegisterDomainInfo *regs,
                                                    size_t reg_count,
                                                    JZASTNode *decl)
{
    if (!regs || !decl) return NULL;
    for (size_t i = 0; i < reg_count; ++i) {
        if (regs[i].decl == decl) {
            return &regs[i];
        }
    }
    return NULL;
}

static void sem_sync_block_clock_id(JZASTNode *block,
                                    char *out,
                                    size_t out_size)
{
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!block) return;

    for (size_t i = 0; i < block->child_count; ++i) {
        JZASTNode *child = block->children[i];
        if (!child || child->type != JZ_AST_SYNC_PARAM || !child->name) continue;
        if (strcmp(child->name, "CLK") != 0) continue;
        if (child->child_count == 0) return;
        JZASTNode *expr = child->children[0];
        if (!expr) return;

        const char *id = NULL;
        if ((expr->type == JZ_AST_EXPR_IDENTIFIER ||
             expr->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER) &&
            expr->name) {
            id = expr->name;
        } else if (expr->type == JZ_AST_EXPR_LITERAL && expr->text) {
            id = expr->text;
        }
        if (!id) return;

        size_t len = strlen(id);
        if (len >= out_size) len = out_size - 1;
        memcpy(out, id, len);
        out[len] = '\0';
        return;
    }
}

static void sem_sync_collect_reg_assigns_stmt(JZASTNode *stmt,
                                              const JZModuleScope *scope,
                                              int nesting_depth,
                                              JZBuffer *out_targets)
{
    if (!stmt || !scope || !out_targets) return;

    switch (stmt->type) {
    case JZ_AST_STMT_ASSIGN:
        if (stmt->child_count >= 1) {
            JZASTNode *lhs = stmt->children[0];
            sem_excl_collect_targets_from_lhs(lhs,
                                             scope,
                                             NULL,
                                             1,
                                             (nesting_depth > 0),
                                             out_targets);
        }
        break;

    case JZ_AST_STMT_IF:
    case JZ_AST_STMT_ELIF:
        for (size_t j = 1; j < stmt->child_count; ++j) {
            sem_sync_collect_reg_assigns_stmt(stmt->children[j],
                                              scope,
                                              nesting_depth + 1,
                                              out_targets);
        }
        break;

    case JZ_AST_STMT_ELSE:
    case JZ_AST_STMT_SELECT:
    case JZ_AST_STMT_CASE:
    case JZ_AST_STMT_DEFAULT:
        for (size_t j = 0; j < stmt->child_count; ++j) {
            sem_sync_collect_reg_assigns_stmt(stmt->children[j],
                                              scope,
                                              nesting_depth + 1,
                                              out_targets);
        }
        break;

    case JZ_AST_BLOCK:
        for (size_t j = 0; j < stmt->child_count; ++j) {
            sem_sync_collect_reg_assigns_stmt(stmt->children[j],
                                              scope,
                                              nesting_depth + 1,
                                              out_targets);
        }
        break;

    default:
        break;
    }
}

static void sem_sync_check_register_uses_expr(JZASTNode *expr,
                                              const JZModuleScope *scope,
                                              const char *block_clk,
                                              JZRegisterDomainInfo *regs,
                                              size_t reg_count,
                                              const JZCdcAliasDomainInfo *aliases,
                                              size_t alias_count,
                                              JZDiagnosticList *diagnostics)
{
    if (!expr || !scope || !block_clk || !*block_clk) return;

    if (expr->type == JZ_AST_EXPR_IDENTIFIER && expr->name) {
        /* Check REGISTER home domain via CDC or assignment. */
        const JZSymbol *sym_reg = module_scope_lookup_kind(scope,
                                                           expr->name,
                                                           JZ_SYM_REGISTER);
        if (sym_reg && sym_reg->node) {
            JZRegisterDomainInfo *info = sem_sync_find_reg_info(regs, reg_count, sym_reg->node);
            if (info && info->has_home && info->home_clk[0] != '\0' &&
                strcmp(info->home_clk, block_clk) != 0) {
                char explain[256];
                snprintf(explain, sizeof(explain),
                         "register '%s' belongs to clock domain '%s' but is read in a\n"
                         "SYNCHRONOUS block clocked by '%s' — this is a CDC violation.\n"
                         "Use a CDC block to safely transfer the value between domains.",
                         expr->name, info->home_clk, block_clk);
                sem_report_rule(diagnostics,
                                expr->loc,
                                "DOMAIN_CONFLICT",
                                explain);
            }
        }

        /* Check CDC destination alias home domain. Aliases are modeled as
         * symbols of kind JZ_SYM_WIRE with JZCdcAliasDomainInfo entries.
         */
        const JZSymbol *sym_wire = module_scope_lookup_kind(scope,
                                                            expr->name,
                                                            JZ_SYM_WIRE);
        if (sym_wire && sym_wire->node && aliases && alias_count > 0) {
            for (size_t i = 0; i < alias_count; ++i) {
                const JZCdcAliasDomainInfo *ai = &aliases[i];
                if (!ai->decl || !ai->has_home || ai->home_clk[0] == '\0') continue;
                if (ai->decl == sym_wire->node && strcmp(ai->home_clk, block_clk) != 0) {
                    char explain[256];
                    snprintf(explain, sizeof(explain),
                             "CDC alias '%s' belongs to clock domain '%s' but is used in a\n"
                             "SYNCHRONOUS block clocked by '%s' — reading a CDC destination\n"
                             "alias outside its target domain defeats the synchronizer.",
                             expr->name, ai->home_clk, block_clk);
                    sem_report_rule(diagnostics,
                                    expr->loc,
                                    "DOMAIN_CONFLICT",
                                    explain);
                    break;
                }
            }
        }
    }

    for (size_t i = 0; i < expr->child_count; ++i) {
        sem_sync_check_register_uses_expr(expr->children[i],
                                          scope,
                                          block_clk,
                                          regs,
                                          reg_count,
                                          aliases,
                                          alias_count,
                                          diagnostics);
    }
}

/* Walk a subtree looking for an identifier matching |name|.  Returns the
 * identifier node if found, NULL otherwise. */
static const JZASTNode *find_ident_in_subtree(const JZASTNode *node,
                                               const char *name)
{
    if (!node || !name) return NULL;
    if (node->type == JZ_AST_EXPR_IDENTIFIER && node->name &&
        strcmp(node->name, name) == 0) {
        return node;
    }
    for (size_t i = 0; i < node->child_count; ++i) {
        const JZASTNode *r = find_ident_in_subtree(node->children[i], name);
        if (r) return r;
    }
    return NULL;
}

/* Scan a block for assignments whose LHS contains |alias_name| and emit
 * CDC_DEST_ALIAS_ASSIGNED for each occurrence. */
static void sem_check_cdc_alias_lhs_in_block(const JZASTNode *node,
                                               const char *alias_name,
                                               JZDiagnosticList *diagnostics)
{
    if (!node || !alias_name) return;
    if (node->type == JZ_AST_STMT_ASSIGN && node->child_count >= 1) {
        const JZASTNode *lhs = node->children[0];
        const JZASTNode *hit = find_ident_in_subtree(lhs, alias_name);
        if (hit) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "CDC destination alias '%s' may not be assigned directly;\n"
                     "it is driven automatically by the CDC synchronizer",
                     alias_name);
            sem_report_rule(diagnostics, hit->loc,
                            "CDC_DEST_ALIAS_ASSIGNED", msg);
            return;
        }
    }
    for (size_t i = 0; i < node->child_count; ++i) {
        sem_check_cdc_alias_lhs_in_block(node->children[i], alias_name,
                                          diagnostics);
    }
}

void sem_check_sync_clock_domains(JZBuffer *module_scopes,
                                  JZDiagnosticList *diagnostics)
{
    if (!module_scopes) return;

    size_t scope_count = module_scopes->len / sizeof(JZModuleScope);
    JZModuleScope *scopes = (JZModuleScope *)module_scopes->data;

    for (size_t si = 0; si < scope_count; ++si) {
        JZModuleScope *scope = &scopes[si];
        if (!scope->node) continue;
        JZASTNode *mod = scope->node;

        size_t sym_count = scope->symbols.len / sizeof(JZSymbol);
        JZSymbol *syms = (JZSymbol *)scope->symbols.data;
        JZRegisterDomainInfo reg_buf[128];
        JZRegisterDomainInfo *regs = reg_buf;
        size_t reg_cap = sizeof(reg_buf) / sizeof(reg_buf[0]);
        size_t reg_count = 0;

        JZCdcAliasDomainInfo alias_buf[32];
        JZCdcAliasDomainInfo *aliases = alias_buf;
        size_t alias_cap = sizeof(alias_buf) / sizeof(alias_buf[0]);
        size_t alias_count = 0;

        if (sym_count > reg_cap) {
            regs = (JZRegisterDomainInfo *)calloc(sym_count, sizeof(JZRegisterDomainInfo));
            if (!regs) continue;
            reg_cap = sym_count;
        }

        for (size_t i = 0; i < sym_count && reg_count < reg_cap; ++i) {
            if (syms[i].kind != JZ_SYM_REGISTER || !syms[i].node) continue;
            regs[reg_count].decl = syms[i].node;
            regs[reg_count].home_clk[0] = '\0';
            regs[reg_count].has_home = 0;
            regs[reg_count].multi_clk_reported = 0;
            reg_count++;
        }

        if (sym_count > alias_cap) {
            aliases = (JZCdcAliasDomainInfo *)calloc(sym_count, sizeof(JZCdcAliasDomainInfo));
            if (!aliases) {
                if (regs != reg_buf) free(regs);
                continue;
            }
            alias_cap = sym_count;
        }

        /* Seed register home domains and CDC alias domains from CDC blocks. */
        for (size_t ci = 0; ci < mod->child_count; ++ci) {
            JZASTNode *blk = mod->children[ci];
            if (!blk || blk->type != JZ_AST_BLOCK || !blk->block_kind) continue;
            if (strcmp(blk->block_kind, "CDC") != 0) continue;

            for (size_t j = 0; j < blk->child_count; ++j) {
                JZASTNode *cdc = blk->children[j];
                if (!cdc || cdc->type != JZ_AST_CDC_DECL) continue;
                if (cdc->child_count < 4) continue;

                JZASTNode *src_node = cdc->children[0];
                JZASTNode *src_clk  = cdc->children[1];
                JZASTNode *dst_id   = cdc->children[2];
                JZASTNode *dst_clk  = cdc->children[3];

                /* Extract base identifier from bare id or slice node */
                JZASTNode *src_id = src_node;
                int has_bit_select = 0;
                if (src_node && src_node->type == JZ_AST_EXPR_SLICE &&
                    src_node->child_count >= 1) {
                    src_id = src_node->children[0]; /* base identifier */
                    has_bit_select = 1;
                }

                const char *src_name = (src_id && src_id->type == JZ_AST_EXPR_IDENTIFIER) ? src_id->name : NULL;
                const char *src_clk_name = (src_clk && src_clk->type == JZ_AST_EXPR_IDENTIFIER) ? src_clk->name : NULL;
                const char *dst_clk_name = (dst_clk && dst_clk->type == JZ_AST_EXPR_IDENTIFIER) ? dst_clk->name : NULL;

                if (!src_name || !src_clk_name || !dst_clk_name) {
                    continue;
                }

                /* LATCH_AS_CLOCK_OR_CDC: CDC clocks must not be latches. */
                {
                    const JZSymbol *src_clk_sym = module_scope_lookup(scope, src_clk_name);
                    if (src_clk_sym && src_clk_sym->kind == JZ_SYM_LATCH) {
                        char explain[256];
                        snprintf(explain, sizeof(explain),
                                 "'%s' is a LATCH, not a clock. LATCHes are level-sensitive\n"
                                 "storage elements and cannot be used as CDC source clock.\n"
                                 "Use a declared clock signal instead.",
                                 src_clk_name);
                        sem_report_rule(diagnostics,
                                        src_clk->loc,
                                        "LATCH_AS_CLOCK_OR_CDC",
                                        explain);
                    }
                    const JZSymbol *dst_clk_sym = module_scope_lookup(scope, dst_clk_name);
                    if (dst_clk_sym && dst_clk_sym->kind == JZ_SYM_LATCH) {
                        char explain[256];
                        snprintf(explain, sizeof(explain),
                                 "'%s' is a LATCH, not a clock. LATCHes are level-sensitive\n"
                                 "storage elements and cannot be used as CDC destination clock.\n"
                                 "Use a declared clock signal instead.",
                                 dst_clk_name);
                        sem_report_rule(diagnostics,
                                        dst_clk->loc,
                                        "LATCH_AS_CLOCK_OR_CDC",
                                        explain);
                    }
                }

                const JZSymbol *src_sym = module_scope_lookup_kind(scope, src_name, JZ_SYM_REGISTER);
                if (!src_sym || !src_sym->node) {
                    char explain[256];
                    snprintf(explain, sizeof(explain),
                             "'%s' is not a REGISTER in this module.\n"
                             "CDC source must be a REGISTER — only registers hold stable\n"
                             "values across clock domain boundaries.",
                             src_name);
                    sem_report_rule(diagnostics,
                                    cdc->loc,
                                    "CDC_SOURCE_NOT_REGISTER",
                                    explain);
                } else {
                    JZRegisterDomainInfo *info = sem_sync_find_reg_info(regs, reg_count, src_sym->node);
                    if (info) {
                        if (!info->has_home) {
                            strncpy(info->home_clk, src_clk_name, sizeof(info->home_clk) - 1);
                            info->home_clk[sizeof(info->home_clk) - 1] = '\0';
                            info->has_home = 1;
                        } else if (info->home_clk[0] != '\0' &&
                                   strcmp(info->home_clk, src_clk_name) != 0) {
                            char explain[256];
                            snprintf(explain, sizeof(explain),
                                     "register '%s' has home domain '%s' but this CDC declares\n"
                                     "source clock '%s' — a register can only belong to one\n"
                                     "clock domain.",
                                     src_name, info->home_clk, src_clk_name);
                            sem_report_rule(diagnostics,
                                            src_sym->node->loc,
                                            "MULTI_CLK_ASSIGN",
                                            explain);
                        }

                        /* BIT CDC must target width-1 register when width is statically known.
                         * When a bit-select is present (e.g. reg[26]), the selected slice
                         * determines the effective width, so skip the full-register check. */
                        if (!has_bit_select &&
                            cdc->block_kind && strcmp(cdc->block_kind, "BIT") == 0 &&
                            src_sym->node && src_sym->node->width) {
                            unsigned w = 0;
                            int rc = eval_simple_positive_decl_int(src_sym->node->width, &w);
                            if (rc == 1 && w != 1u) {
                                char explain[256];
                                snprintf(explain, sizeof(explain),
                                         "register '%s' has width [%u] but BIT CDC requires width [1].\n"
                                         "BIT synchronizers transfer a single-bit flag; use BUS CDC\n"
                                         "for multi-bit values.",
                                         src_name, w);
                                sem_report_rule(diagnostics,
                                                src_sym->node->loc,
                                                "CDC_BIT_WIDTH_NOT_1",
                                                explain);
                            }
                        }

                        /* PULSE CDC must also target width-1 register. */
                        if (!has_bit_select &&
                            cdc->block_kind && strcmp(cdc->block_kind, "PULSE") == 0 &&
                            src_sym->node && src_sym->node->width) {
                            unsigned w = 0;
                            int rc = eval_simple_positive_decl_int(src_sym->node->width, &w);
                            if (rc == 1 && w != 1u) {
                                char explain[256];
                                snprintf(explain, sizeof(explain),
                                         "register '%s' has width [%u] but PULSE CDC requires width [1].\n"
                                         "PULSE synchronizers detect single-bit edges across domains.",
                                         src_name, w);
                                sem_report_rule(diagnostics,
                                                src_sym->node->loc,
                                                "CDC_PULSE_WIDTH_NOT_1",
                                                explain);
                            }
                        }
                    }
                }

                /* Record destination alias home clock. Aliases are modeled as
                 * WIRE-like symbols; we only track those introduced by CDC.
                 */
                if (dst_id && dst_id->type == JZ_AST_EXPR_IDENTIFIER && dst_id->name &&
                    alias_count < alias_cap) {
                    aliases[alias_count].decl = dst_id;
                    strncpy(aliases[alias_count].home_clk, dst_clk_name, sizeof(aliases[alias_count].home_clk) - 1);
                    aliases[alias_count].home_clk[sizeof(aliases[alias_count].home_clk) - 1] = '\0';
                    aliases[alias_count].has_home = 1;
                    alias_count++;
                }
            }
        }

        /* Phase 5: CDC additional checks. */
        for (size_t ci = 0; ci < mod->child_count; ++ci) {
            JZASTNode *blk = mod->children[ci];
            if (!blk || blk->type != JZ_AST_BLOCK || !blk->block_kind) continue;
            if (strcmp(blk->block_kind, "CDC") != 0) continue;

            for (size_t j = 0; j < blk->child_count; ++j) {
                JZASTNode *cdc = blk->children[j];
                if (!cdc || cdc->type != JZ_AST_CDC_DECL) continue;

                /* CDC_TYPE_INVALID: block_kind must be BIT, BUS, FIFO, HANDSHAKE, PULSE, or MCP. */
                if (cdc->block_kind) {
                    if (strcmp(cdc->block_kind, "BIT") != 0 &&
                        strcmp(cdc->block_kind, "BUS") != 0 &&
                        strcmp(cdc->block_kind, "FIFO") != 0 &&
                        strcmp(cdc->block_kind, "HANDSHAKE") != 0 &&
                        strcmp(cdc->block_kind, "PULSE") != 0 &&
                        strcmp(cdc->block_kind, "MCP") != 0 &&
                        strcmp(cdc->block_kind, "RAW") != 0) {
                        char explain[256];
                        snprintf(explain, sizeof(explain),
                                 "CDC type '%s' is not recognized.\n"
                                 "Valid types: BIT, BUS, FIFO, HANDSHAKE, PULSE, MCP, RAW.",
                                 cdc->block_kind);
                        sem_report_rule(diagnostics,
                                        cdc->loc,
                                        "CDC_TYPE_INVALID",
                                        explain);
                    }

                    /* CDC_RAW_STAGES_FORBIDDEN: RAW does not accept stages. */
                    if (strcmp(cdc->block_kind, "RAW") == 0 &&
                        cdc->width && *cdc->width) {
                        char explain[256];
                        snprintf(explain, sizeof(explain),
                                 "RAW CDC was given stages '%s' but RAW bypasses the\n"
                                 "synchronizer — no stage count is allowed. Remove the\n"
                                 "stages parameter or use a different CDC type.",
                                 cdc->width);
                        sem_report_rule(diagnostics,
                                        cdc->loc,
                                        "CDC_RAW_STAGES_FORBIDDEN",
                                        explain);
                    }
                }

                /* CDC_STAGES_INVALID: if width field is present, it encodes stages. */
                if (cdc->width && *cdc->width) {
                    unsigned stages = 0;
                    int rc = eval_simple_positive_decl_int(cdc->width, &stages);
                    if (rc != 1 || stages == 0) {
                        char explain[256];
                        snprintf(explain, sizeof(explain),
                                 "CDC stages value '%s' is not a valid positive integer.\n"
                                 "Stages specifies the synchronizer depth (typically 2 or 3).",
                                 cdc->width);
                        sem_report_rule(diagnostics,
                                        cdc->loc,
                                        "CDC_STAGES_INVALID",
                                        explain);
                    }
                }

                /* CDC_SOURCE_NOT_PLAIN_REG: source must be a plain identifier
                 * (not a slice with range or binary expression). The existing
                 * CDC_SOURCE_NOT_REGISTER handles "not a register" case; this
                 * additional check catches complex source expressions.
                 */
                if (cdc->child_count >= 1) {
                    JZASTNode *src_node = cdc->children[0];
                    if (src_node && src_node->type == JZ_AST_EXPR_BINARY) {
                        sem_report_rule(diagnostics,
                                        src_node->loc,
                                        "CDC_SOURCE_NOT_PLAIN_REG",
                                        "CDC source is a binary expression but must be a plain\n"
                                        "register identifier. The synchronizer needs a stable,\n"
                                        "single-register source.");
                    } else if (src_node && src_node->type == JZ_AST_EXPR_CONCAT) {
                        sem_report_rule(diagnostics,
                                        src_node->loc,
                                        "CDC_SOURCE_NOT_PLAIN_REG",
                                        "CDC source is a concatenation but must be a plain\n"
                                        "register identifier. Use BUS CDC with a single register\n"
                                        "to transfer multi-signal values.");
                    } else if (src_node && src_node->type == JZ_AST_EXPR_SLICE) {
                        sem_report_rule(diagnostics,
                                        src_node->loc,
                                        "CDC_SOURCE_NOT_PLAIN_REG",
                                        "CDC source is a slice but must be a plain register\n"
                                        "identifier. The synchronizer needs the full register,\n"
                                        "not a bit range.");
                    }
                }

                /* CDC_DEST_ALIAS_DUP: check destination alias for scope conflict. */
                if (cdc->child_count >= 3) {
                    JZASTNode *dst_id = cdc->children[2];
                    if (dst_id && dst_id->type == JZ_AST_EXPR_IDENTIFIER && dst_id->name) {
                        const JZSymbol *existing = module_scope_lookup(scope, dst_id->name);
                        if (existing && existing->kind != JZ_SYM_WIRE) {
                            /* A non-wire symbol already exists with that name. */
                            const char *kind_str = "identifier";
                            if (existing->kind == JZ_SYM_REGISTER) kind_str = "REGISTER";
                            else if (existing->kind == JZ_SYM_PORT) kind_str = "PORT";
                            else if (existing->kind == JZ_SYM_CONST) kind_str = "CONST";
                            else if (existing->kind == JZ_SYM_LATCH) kind_str = "LATCH";
                            char explain[256];
                            snprintf(explain, sizeof(explain),
                                     "CDC destination alias '%s' conflicts with an existing %s\n"
                                     "of the same name. Choose a different alias name.",
                                     dst_id->name, kind_str);
                            sem_report_rule(diagnostics,
                                            dst_id->loc,
                                            "CDC_DEST_ALIAS_DUP",
                                            explain);
                        }
                    }
                }
            }
        }

        /* CDC_DEST_ALIAS_ASSIGNED: scan blocks for assignments to CDC alias names. */
        for (size_t ai = 0; ai < alias_count; ++ai) {
            if (!aliases[ai].decl || !aliases[ai].decl->name) continue;
            const char *alias_name = aliases[ai].decl->name;
            for (size_t ci = 0; ci < mod->child_count; ++ci) {
                JZASTNode *blk = mod->children[ci];
                if (!blk || blk->type != JZ_AST_BLOCK || !blk->block_kind) continue;
                if (strcmp(blk->block_kind, "ASYNCHRONOUS") != 0 &&
                    strcmp(blk->block_kind, "SYNCHRONOUS") != 0) continue;
                sem_check_cdc_alias_lhs_in_block(blk, alias_name,
                                                 diagnostics);
            }
        }

        typedef struct JZSyncClockSeen {
            char      clk[64];
            JZLocation first_loc;
        } JZSyncClockSeen;

        JZSyncClockSeen clk_seen[16];
        size_t clk_seen_count = 0;

        for (size_t ci = 0; ci < mod->child_count; ++ci) {
            JZASTNode *blk = mod->children[ci];
            if (!blk || blk->type != JZ_AST_BLOCK || !blk->block_kind) continue;
            if (strcmp(blk->block_kind, "SYNCHRONOUS") != 0) continue;

            /* Phase 4: Validate SYNCHRONOUS block parameters. */
            {
                int has_clk = 0;
                for (size_t pi = 0; pi < blk->child_count; ++pi) {
                    JZASTNode *param = blk->children[pi];
                    if (!param || param->type != JZ_AST_SYNC_PARAM || !param->name) continue;

                    if (strcasecmp(param->name, "CLK") == 0) {
                        has_clk = 1;
                        /* SYNC_CLK_WIDTH_NOT_1: CLK signal must be width-1. */
                        if (param->child_count >= 1 && param->children[0]) {
                            JZBitvecType clk_t;
                            clk_t.width = 0;
                            clk_t.is_signed = 0;
                            infer_expr_type(param->children[0], scope, NULL, NULL, &clk_t);
                            if (clk_t.width > 0 && clk_t.width != 1) {
                                char explain[256];
                                const char *clk_name = (param->children[0]->name) ? param->children[0]->name : "?";
                                snprintf(explain, sizeof(explain),
                                         "CLK signal '%s' has width [%u] but must be width [1].\n"
                                         "A clock is a single-bit signal.",
                                         clk_name, clk_t.width);
                                sem_report_rule(diagnostics,
                                                param->children[0]->loc,
                                                "SYNC_CLK_WIDTH_NOT_1",
                                                explain);
                            }
                            /* LATCH_AS_CLOCK_OR_CDC: CLK must not be a latch. */
                            if (param->children[0]->type == JZ_AST_EXPR_IDENTIFIER &&
                                param->children[0]->name) {
                                const JZSymbol *sym = module_scope_lookup(scope, param->children[0]->name);
                                if (sym && sym->kind == JZ_SYM_LATCH) {
                                    char explain[256];
                                    snprintf(explain, sizeof(explain),
                                             "'%s' is a LATCH, not a clock. LATCHes are level-sensitive\n"
                                             "storage elements and cannot drive SYNCHRONOUS block CLK.\n"
                                             "Use a declared clock signal instead.",
                                             param->children[0]->name);
                                    sem_report_rule(diagnostics,
                                                    param->children[0]->loc,
                                                    "LATCH_AS_CLOCK_OR_CDC",
                                                    explain);
                                }
                            }
                        }
                    } else if (strcasecmp(param->name, "RESET") == 0) {
                        /* SYNC_RESET_WIDTH_NOT_1: RESET signal must be width-1. */
                        if (param->child_count >= 1 && param->children[0]) {
                            JZBitvecType rst_t;
                            rst_t.width = 0;
                            rst_t.is_signed = 0;
                            infer_expr_type(param->children[0], scope, NULL, NULL, &rst_t);
                            if (rst_t.width > 0 && rst_t.width != 1) {
                                char explain[256];
                                const char *rst_name = (param->children[0]->name) ? param->children[0]->name : "?";
                                snprintf(explain, sizeof(explain),
                                         "RESET signal '%s' has width [%u] but must be width [1].\n"
                                         "A reset is a single-bit signal.",
                                         rst_name, rst_t.width);
                                sem_report_rule(diagnostics,
                                                param->children[0]->loc,
                                                "SYNC_RESET_WIDTH_NOT_1",
                                                explain);
                            }
                        }
                    } else if (strcasecmp(param->name, "EDGE") == 0) {
                        /* SYNC_EDGE_INVALID: must be Rising, Falling, or Both
                         * (including legacy all-caps forms).
                         */
                        if (param->child_count >= 1 && param->children[0] &&
                            param->children[0]->type == JZ_AST_EXPR_IDENTIFIER &&
                            param->children[0]->name) {
                            const char *val = param->children[0]->name;
                            if (strcmp(val, "Rising") != 0 &&
                                strcmp(val, "RISING") != 0 &&
                                strcmp(val, "Falling") != 0 &&
                                strcmp(val, "FALLING") != 0 &&
                                strcmp(val, "Both") != 0 &&
                                strcmp(val, "BOTH") != 0) {
                                char explain[256];
                                snprintf(explain, sizeof(explain),
                                         "EDGE value '%s' is not valid.\n"
                                         "Must be Rising, Falling, or Both.",
                                         val);
                                sem_report_rule(diagnostics,
                                                param->children[0]->loc,
                                                "SYNC_EDGE_INVALID",
                                                explain);
                            }
                            /* SYNC_EDGE_BOTH_WARNING: dual-edge may not be supported by all targets. */
                            if (strcmp(val, "Both") == 0 || strcmp(val, "BOTH") == 0) {
                                char clk_name[64];
                                sem_sync_block_clock_id(blk, clk_name, sizeof(clk_name));
                                char explain[256];
                                snprintf(explain, sizeof(explain),
                                         "EDGE=Both on clock '%s' — dual-edge clocking triggers on both\n"
                                         "rising and falling edges. Many FPGA architectures do not\n"
                                         "support this in flip-flop primitives.",
                                         clk_name[0] ? clk_name : "?");
                                sem_report_rule(diagnostics,
                                                param->children[0]->loc,
                                                "SYNC_EDGE_BOTH_WARNING",
                                                explain);
                            }
                        }
                    } else if (strcasecmp(param->name, "RESET_ACTIVE") == 0) {
                        /* SYNC_RESET_ACTIVE_INVALID: must be High or Low
                         * (or legacy ACTIVE_HIGH/ACTIVE_LOW/HIGH/LOW).
                         */
                        if (param->child_count >= 1 && param->children[0] &&
                            param->children[0]->type == JZ_AST_EXPR_IDENTIFIER &&
                            param->children[0]->name) {
                            const char *val = param->children[0]->name;
                            if (strcmp(val, "High") != 0 &&
                                strcmp(val, "Low") != 0 &&
                                strcmp(val, "HIGH") != 0 &&
                                strcmp(val, "LOW") != 0 &&
                                strcmp(val, "ACTIVE_HIGH") != 0 &&
                                strcmp(val, "ACTIVE_LOW") != 0) {
                                char explain[256];
                                snprintf(explain, sizeof(explain),
                                         "RESET_ACTIVE value '%s' is not valid.\n"
                                         "Must be High or Low.",
                                         val);
                                sem_report_rule(diagnostics,
                                                param->children[0]->loc,
                                                "SYNC_RESET_ACTIVE_INVALID",
                                                explain);
                            }
                        }
                    } else if (strcasecmp(param->name, "RESET_TYPE") == 0) {
                        /* SYNC_RESET_TYPE_INVALID: must be Clocked or Immediate
                         * (or legacy aliases Sync/Async).
                         */
                        if (param->child_count >= 1 && param->children[0] &&
                            param->children[0]->type == JZ_AST_EXPR_IDENTIFIER &&
                            param->children[0]->name) {
                            const char *val = param->children[0]->name;
                            if (strcmp(val, "Clocked") != 0 &&
                                strcmp(val, "CLOCKED") != 0 &&
                                strcmp(val, "Immediate") != 0 &&
                                strcmp(val, "IMMEDIATE") != 0) {
                                char explain[256];
                                snprintf(explain, sizeof(explain),
                                         "RESET_TYPE value '%s' is not valid.\n"
                                         "Must be Clocked (synchronous reset) or Immediate (asynchronous reset).",
                                         val);
                                sem_report_rule(diagnostics,
                                                param->children[0]->loc,
                                                "SYNC_RESET_TYPE_INVALID",
                                                explain);
                            }
                        }
                    } else {
                        /* SYNC_UNKNOWN_PARAM: unrecognized parameter name. */
                        char explain[256];
                        snprintf(explain, sizeof(explain),
                                 "parameter '%s' is not recognized for SYNCHRONOUS blocks.\n"
                                 "Valid parameters: CLK, RESET, EDGE, RESET_ACTIVE, RESET_TYPE.",
                                 param->name);
                        sem_report_rule(diagnostics,
                                        param->loc,
                                        "SYNC_UNKNOWN_PARAM",
                                        explain);
                    }
                }

                /* SYNC_MISSING_CLK: CLK parameter is required. */
                if (!has_clk) {
                    const char *mod_name = (mod->name) ? mod->name : "?";
                    char explain[256];
                    snprintf(explain, sizeof(explain),
                             "SYNCHRONOUS block in module '%s' has no CLK parameter.\n"
                             "Every SYNCHRONOUS block must specify which clock drives it,\n"
                             "e.g. SYNCHRONOUS (CLK = my_clock) { ... }",
                             mod_name);
                    sem_report_rule(diagnostics,
                                    blk->loc,
                                    "SYNC_MISSING_CLK",
                                    explain);
                }
            }

            char clk_id[64];
            sem_sync_block_clock_id(blk, clk_id, sizeof(clk_id));

            if (clk_id[0] != '\0') {
                int seen = 0;
                for (size_t k = 0; k < clk_seen_count; ++k) {
                    if (strcmp(clk_seen[k].clk, clk_id) == 0) {
                        seen = 1;
                        break;
                    }
                }
                if (!seen && clk_seen_count < sizeof(clk_seen) / sizeof(clk_seen[0])) {
                    strncpy(clk_seen[clk_seen_count].clk, clk_id, sizeof(clk_seen[clk_seen_count].clk) - 1);
                    clk_seen[clk_seen_count].clk[sizeof(clk_seen[clk_seen_count].clk) - 1] = '\0';
                    clk_seen[clk_seen_count].first_loc = blk->loc;
                    clk_seen_count++;
                } else if (seen && diagnostics) {
                    char explain[256];
                    const char *mod_name = (mod->name) ? mod->name : "?";
                    snprintf(explain, sizeof(explain),
                             "module '%s' already has a SYNCHRONOUS block for clock '%s'.\n"
                             "Each clock domain must have exactly one SYNCHRONOUS block;\n"
                             "merge the logic into the existing block.",
                             mod_name, clk_id);
                    sem_report_rule(diagnostics,
                                    blk->loc,
                                    "DUPLICATE_BLOCK",
                                    explain);
                }
            }

            JZBuffer targets = (JZBuffer){0};
            for (size_t i = 0; i < blk->child_count; ++i) {
                JZASTNode *stmt = blk->children[i];
                if (!stmt) continue;
                sem_sync_collect_reg_assigns_stmt(stmt, scope, 0, &targets);
            }

            JZAssignTargetEntry *entries = (JZAssignTargetEntry *)targets.data;
            size_t entry_count = targets.len / sizeof(JZAssignTargetEntry);

            for (size_t ei = 0; ei < entry_count; ++ei) {
                JZAssignTargetEntry *e = &entries[ei];
                if (!e->decl || !e->is_register) continue;

                JZRegisterDomainInfo *info = sem_sync_find_reg_info(regs, reg_count, e->decl);
                if (!info) continue;

                if (!info->has_home && clk_id[0] != '\0') {
                    strncpy(info->home_clk, clk_id, sizeof(info->home_clk) - 1);
                    info->home_clk[sizeof(info->home_clk) - 1] = '\0';
                    info->has_home = 1;
                } else if (info->has_home && clk_id[0] != '\0' &&
                           info->home_clk[0] != '\0' &&
                           strcmp(info->home_clk, clk_id) != 0 &&
                           !info->multi_clk_reported) {
                    char explain[256];
                    const char *reg_name = (e->decl->name) ? e->decl->name : "?";
                    snprintf(explain, sizeof(explain),
                             "register '%s' is assigned in a block clocked by '%s' but\n"
                             "its home domain is '%s'. A register must be written by\n"
                             "exactly one clock domain.",
                             reg_name, clk_id, info->home_clk);
                    sem_report_rule(diagnostics,
                                    e->decl->loc,
                                    "MULTI_CLK_ASSIGN",
                                    explain);
                    info->multi_clk_reported = 1;
                }
            }

            jz_buf_free(&targets);
        }

        for (size_t ci = 0; ci < mod->child_count; ++ci) {
            JZASTNode *blk = mod->children[ci];
            if (!blk || blk->type != JZ_AST_BLOCK || !blk->block_kind) continue;
            if (strcmp(blk->block_kind, "SYNCHRONOUS") != 0) continue;

            char clk_id[64];
            sem_sync_block_clock_id(blk, clk_id, sizeof(clk_id));
            if (clk_id[0] == '\0') continue;

            for (size_t i = 0; i < blk->child_count; ++i) {
                JZASTNode *stmt = blk->children[i];
                if (!stmt) continue;
                sem_sync_check_register_uses_expr(stmt,
                                                  scope,
                                                  clk_id,
                                                  regs,
                                                  reg_count,
                                                  aliases,
                                                  alias_count,
                                                  diagnostics);
            }
        }

        if (regs != reg_buf) {
            free(regs);
        }
        if (aliases != alias_buf) {
            free(aliases);
        }
    }
}
