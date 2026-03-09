#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "../../include/ir_serialize.h"
#include "../../include/diagnostic.h"

/* Simple JSON string escaper for names and paths. */
static void json_write_string(FILE *out, const char *s)
{
    fputc('"', out);
    if (s) {
        for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
            unsigned char c = *p;
            switch (c) {
            case '"': fputs("\\\"", out); break;
            case '\\': fputs("\\\\", out); break;
            case '\n': fputs("\\n", out); break;
            case '\r': fputs("\\r", out); break;
            case '\t': fputs("\\t", out); break;
            default:
                if (c < 0x20) {
                    fprintf(out, "\\u%04x", (unsigned)c);
                } else {
                    fputc(c, out);
                }
                break;
            }
        }
    }
    fputc('"', out);
}

static const char *ir_signal_kind_string(IR_SignalKind kind)
{
    switch (kind) {
    case SIG_PORT:     return "port";
    case SIG_NET:      return "net";
    case SIG_REGISTER: return "register";
    case SIG_LATCH:    return "latch";
    default:           return "unknown";
    }
}

static const char *ir_port_direction_string(IR_PortDirection dir)
{
    switch (dir) {
    case PORT_IN:    return "IN";
    case PORT_OUT:   return "OUT";
    case PORT_INOUT: return "INOUT";
    default:         return "";
    }
}

static const char *ir_assignment_kind_string(IR_AssignmentKind kind)
{
    switch (kind) {
    case ASSIGN_ALIAS:         return "ASSIGN_ALIAS";
    case ASSIGN_ALIAS_ZEXT:    return "ASSIGN_ALIAS_ZEXT";
    case ASSIGN_ALIAS_SEXT:    return "ASSIGN_ALIAS_SEXT";
    case ASSIGN_DRIVE:         return "ASSIGN_DRIVE";
    case ASSIGN_DRIVE_ZEXT:    return "ASSIGN_DRIVE_ZEXT";
    case ASSIGN_DRIVE_SEXT:    return "ASSIGN_DRIVE_SEXT";
    case ASSIGN_RECEIVE:       return "ASSIGN_RECEIVE";
    case ASSIGN_RECEIVE_ZEXT:  return "ASSIGN_RECEIVE_ZEXT";
    case ASSIGN_RECEIVE_SEXT:  return "ASSIGN_RECEIVE_SEXT";
    default:                   return "ASSIGN_UNKNOWN";
    }
}

static const char *ir_expr_kind_string(IR_ExprKind kind)
{
    switch (kind) {
    case EXPR_LITERAL:      return "literal";
    case EXPR_SIGNAL_REF:   return "signal_ref";
    
    case EXPR_UNARY_NOT:    return "unary_not";
    case EXPR_UNARY_NEG:    return "unary_neg";
    case EXPR_LOGICAL_NOT:  return "logical_not";
    
    case EXPR_BINARY_ADD:   return "binary_add";
    case EXPR_BINARY_SUB:   return "binary_sub";
    case EXPR_BINARY_MUL:   return "binary_mul";
    case EXPR_BINARY_DIV:   return "binary_div";
    case EXPR_BINARY_MOD:   return "binary_mod";
    
    case EXPR_BINARY_AND:   return "binary_and";
    case EXPR_BINARY_OR:    return "binary_or";
    case EXPR_BINARY_XOR:   return "binary_xor";
    
    case EXPR_BINARY_SHL:   return "binary_shl";
    case EXPR_BINARY_SHR:   return "binary_shr";
    case EXPR_BINARY_ASHR:  return "binary_ashr";
    
    case EXPR_BINARY_EQ:    return "binary_eq";
    case EXPR_BINARY_NEQ:   return "binary_neq";
    case EXPR_BINARY_LT:    return "binary_lt";
    case EXPR_BINARY_GT:    return "binary_gt";
    case EXPR_BINARY_LTE:   return "binary_lte";
    case EXPR_BINARY_GTE:   return "binary_gte";
    
    case EXPR_LOGICAL_AND:  return "logical_and";
    case EXPR_LOGICAL_OR:   return "logical_or";
    
    case EXPR_TERNARY:      return "ternary";
    case EXPR_CONCAT:       return "concat";
    case EXPR_SLICE:        return "slice";

    case EXPR_INTRINSIC_UADD:   return "intrinsic_uadd";
    case EXPR_INTRINSIC_SADD:   return "intrinsic_sadd";
    case EXPR_INTRINSIC_UMUL:   return "intrinsic_umul";
    case EXPR_INTRINSIC_SMUL:   return "intrinsic_smul";
    case EXPR_INTRINSIC_GBIT:   return "intrinsic_gbit";
    case EXPR_INTRINSIC_SBIT:   return "intrinsic_sbit";
    case EXPR_INTRINSIC_GSLICE: return "intrinsic_gslice";
    case EXPR_INTRINSIC_SSLICE: return "intrinsic_sslice";
    case EXPR_INTRINSIC_OH2B:   return "intrinsic_oh2b";
    case EXPR_INTRINSIC_B2OH:   return "intrinsic_b2oh";
    case EXPR_INTRINSIC_PRIENC: return "intrinsic_prienc";
    case EXPR_INTRINSIC_LZC:    return "intrinsic_lzc";
    case EXPR_INTRINSIC_USUB:   return "intrinsic_usub";
    case EXPR_INTRINSIC_SSUB:   return "intrinsic_ssub";
    case EXPR_INTRINSIC_ABS:    return "intrinsic_abs";
    case EXPR_INTRINSIC_UMIN:   return "intrinsic_umin";
    case EXPR_INTRINSIC_UMAX:   return "intrinsic_umax";
    case EXPR_INTRINSIC_SMIN:   return "intrinsic_smin";
    case EXPR_INTRINSIC_SMAX:   return "intrinsic_smax";
    case EXPR_INTRINSIC_POPCOUNT:   return "intrinsic_popcount";
    case EXPR_INTRINSIC_REVERSE:    return "intrinsic_reverse";
    case EXPR_INTRINSIC_BSWAP:      return "intrinsic_bswap";
    case EXPR_INTRINSIC_REDUCE_AND: return "intrinsic_reduce_and";
    case EXPR_INTRINSIC_REDUCE_OR:  return "intrinsic_reduce_or";
    case EXPR_INTRINSIC_REDUCE_XOR: return "intrinsic_reduce_xor";
    case EXPR_MEM_READ:         return "mem_read";

    default:                return "expr_other";
    }
}

static void json_write_expr(FILE *out, const IR_Expr *expr)
{
    if (!expr) {
        fputs("null", out);
        return;
    }

    fputs("{ ", out);
    fputs("\"kind\": ", out);
    json_write_string(out, ir_expr_kind_string(expr->kind));
    fputs(", \"width\": ", out);
    fprintf(out, "%d", expr->width);
    if (expr->source_line > 0) {
        fputs(", \"source_line\": ", out);
        fprintf(out, "%d", expr->source_line);
    }

    switch (expr->kind) {
    case EXPR_LITERAL:
        fputs(", \"literal\": { \"value\": ", out);
        fprintf(out, "%" PRIu64, (unsigned long long)expr->u.literal.literal.words[0]);
        fputs(", \"width\": ", out);
        fprintf(out, "%d", expr->u.literal.literal.width);
        if (expr->u.literal.literal.is_z) {
            fputs(", \"is_z\": true", out);
        }
        fputs(" }", out);
        break;

    case EXPR_SIGNAL_REF:
        fputs(", \"signal_id\": ", out);
        fprintf(out, "%d", expr->u.signal_ref.signal_id);
        break;

    case EXPR_UNARY_NOT:
    case EXPR_UNARY_NEG:
    case EXPR_LOGICAL_NOT:
        fputs(", \"operand\": ", out);
        json_write_expr(out, expr->u.unary.operand);
        break;

    case EXPR_BINARY_ADD: case EXPR_BINARY_SUB: case EXPR_BINARY_MUL:
    case EXPR_BINARY_DIV: case EXPR_BINARY_MOD:
    case EXPR_BINARY_AND: case EXPR_BINARY_OR:  case EXPR_BINARY_XOR:
    case EXPR_BINARY_SHL: case EXPR_BINARY_SHR: case EXPR_BINARY_ASHR:
    case EXPR_BINARY_EQ:  case EXPR_BINARY_NEQ:
    case EXPR_BINARY_LT:  case EXPR_BINARY_GT:
    case EXPR_BINARY_LTE: case EXPR_BINARY_GTE:
    case EXPR_LOGICAL_AND: case EXPR_LOGICAL_OR:
    case EXPR_INTRINSIC_UADD: case EXPR_INTRINSIC_SADD:
    case EXPR_INTRINSIC_UMUL: case EXPR_INTRINSIC_SMUL:
    case EXPR_INTRINSIC_USUB: case EXPR_INTRINSIC_SSUB:
    case EXPR_INTRINSIC_UMIN: case EXPR_INTRINSIC_UMAX:
    case EXPR_INTRINSIC_SMIN: case EXPR_INTRINSIC_SMAX:
        fputs(", \"left\": ", out);
        json_write_expr(out, expr->u.binary.left);
        fputs(", \"right\": ", out);
        json_write_expr(out, expr->u.binary.right);
        break;

    case EXPR_TERNARY:
        fputs(", \"condition\": ", out);
        json_write_expr(out, expr->u.ternary.condition);
        fputs(", \"true\": ", out);
        json_write_expr(out, expr->u.ternary.true_val);
        fputs(", \"false\": ", out);
        json_write_expr(out, expr->u.ternary.false_val);
        break;

    case EXPR_CONCAT:
        fputs(", \"operands\": [", out);
        for (int i = 0; i < expr->u.concat.num_operands; ++i) {
            if (i > 0) fputs(", ", out);
            json_write_expr(out, expr->u.concat.operands[i]);
        }
        fputs("]", out);
        break;

    case EXPR_SLICE:
        fputs(", \"signal_id\": ", out);
        fprintf(out, "%d", expr->u.slice.signal_id);
        fputs(", \"msb\": ", out);
        fprintf(out, "%d", expr->u.slice.msb);
        fputs(", \"lsb\": ", out);
        fprintf(out, "%d", expr->u.slice.lsb);
        break;

    case EXPR_MEM_READ:
        fputs(", \"memory_name\": ", out);
        json_write_string(out, expr->u.mem_read.memory_name ? expr->u.mem_read.memory_name : "");
        fputs(", \"port_name\": ", out);
        json_write_string(out, expr->u.mem_read.port_name ? expr->u.mem_read.port_name : "");
        fputs(", \"address\": ", out);
        json_write_expr(out, expr->u.mem_read.address);
        break;

    case EXPR_INTRINSIC_OH2B:
    case EXPR_INTRINSIC_B2OH:
    case EXPR_INTRINSIC_PRIENC:
    case EXPR_INTRINSIC_LZC:
    case EXPR_INTRINSIC_ABS:
    case EXPR_INTRINSIC_POPCOUNT:
    case EXPR_INTRINSIC_REVERSE:
    case EXPR_INTRINSIC_BSWAP:
    case EXPR_INTRINSIC_REDUCE_AND:
    case EXPR_INTRINSIC_REDUCE_OR:
    case EXPR_INTRINSIC_REDUCE_XOR:
        fputs(", \"source\": ", out);
        json_write_expr(out, expr->u.intrinsic.source);
        break;

    case EXPR_INTRINSIC_GBIT:
    case EXPR_INTRINSIC_SBIT:
    case EXPR_INTRINSIC_GSLICE:
    case EXPR_INTRINSIC_SSLICE:
        fputs(", \"source\": ", out);
        json_write_expr(out, expr->u.intrinsic.source);
        fputs(", \"index\": ", out);
        json_write_expr(out, expr->u.intrinsic.index);
        if (expr->kind == EXPR_INTRINSIC_SBIT || expr->kind == EXPR_INTRINSIC_SSLICE) {
            fputs(", \"value\": ", out);
            json_write_expr(out, expr->u.intrinsic.value);
        }
        if (expr->u.intrinsic.element_width > 0) {
            fputs(", \"element_width\": ", out);
            fprintf(out, "%d", expr->u.intrinsic.element_width);
        }
        break;
    
    default:
        break;
    }

    fputs(" }", out);
}

static void json_write_stmt(FILE *out, const IR_Stmt *stmt)
{
    if (!stmt) {
        fputs("null", out);
        return;
    }

    fputs("{ ", out);
    switch (stmt->kind) {
    case STMT_ASSIGNMENT: {
        const IR_Assignment *a = &stmt->u.assign;
        fputs("\"kind\": \"assignment\"", out);
        fputs(", \"id\": ", out);
        fprintf(out, "%d", a->id);
        if (stmt->source_line > 0) {
            fputs(", \"source_line\": ", out);
            fprintf(out, "%d", stmt->source_line);
        }
        fputs(", \"assignment_kind\": ", out);
        json_write_string(out, ir_assignment_kind_string(a->kind));
        fputs(", \"lhs_signal_id\": ", out);
        fprintf(out, "%d", a->lhs_signal_id);
        fputs(", \"is_sliced\": ", out);
        fputs(a->is_sliced ? "true" : "false", out);
        if (a->is_sliced) {
            fputs(", \"lhs_msb\": ", out);
            fprintf(out, "%d", a->lhs_msb);
            fputs(", \"lhs_lsb\": ", out);
            fprintf(out, "%d", a->lhs_lsb);
        }
        fputs(", \"rhs\": ", out);
        json_write_expr(out, a->rhs);
        fputs(" }", out);
        break;
    }

    case STMT_IF: {
        const IR_IfStmt *ifs = &stmt->u.if_stmt;
        fputs("\"kind\": \"if\"", out);
        if (stmt->source_line > 0) {
            fputs(", \"source_line\": ", out);
            fprintf(out, "%d", stmt->source_line);
        }
        fputs(", \"condition\": ", out);
        json_write_expr(out, ifs->condition);
        fputs(", \"then\": ", out);
        json_write_stmt(out, ifs->then_block);
        if (ifs->elif_chain) {
            fputs(", \"elif_chain\": [", out);
            const IR_Stmt *cur = ifs->elif_chain;
            int first = 1;
            while (cur) {
                if (!first) fputs(", ", out);
                /* Write each elif node inline (condition + then) to avoid
                   exponential recursion: json_write_stmt would re-walk the
                   remaining elif_chain for each node, giving O(2^N) work. */
                fputs("{ ", out);
                if (cur->kind == STMT_IF) {
                    const IR_IfStmt *elif_ifs = &cur->u.if_stmt;
                    fputs("\"kind\": \"if\"", out);
                    if (cur->source_line > 0) {
                        fputs(", \"source_line\": ", out);
                        fprintf(out, "%d", cur->source_line);
                    }
                    fputs(", \"condition\": ", out);
                    json_write_expr(out, elif_ifs->condition);
                    fputs(", \"then\": ", out);
                    json_write_stmt(out, elif_ifs->then_block);
                    if (elif_ifs->else_block) {
                        fputs(", \"else\": ", out);
                        json_write_stmt(out, elif_ifs->else_block);
                    }
                    fputs(" }", out);
                    cur = elif_ifs->elif_chain;
                } else {
                    /* Non-IF tail (e.g. bare ELSE block) */
                    json_write_stmt(out, cur);
                    fputs(" }", out);
                    break;
                }
                first = 0;
            }
            fputs("]", out);
        }
        if (ifs->else_block) {
            fputs(", \"else\": ", out);
            json_write_stmt(out, ifs->else_block);
        }
        fputs(" }", out);
        break;
    }

    case STMT_SELECT: {
        const IR_SelectStmt *sel = &stmt->u.select_stmt;
        fputs("\"kind\": \"select\"", out);
        if (stmt->source_line > 0) {
            fputs(", \"source_line\": ", out);
            fprintf(out, "%d", stmt->source_line);
        }
        fputs(", \"selector\": ", out);
        json_write_expr(out, sel->selector);
        fputs(", \"cases\": [", out);
        for (int i = 0; i < sel->num_cases; ++i) {
            const IR_SelectCase *c = &sel->cases[i];
            if (i > 0) fputs(", ", out);
            fputs("{ ", out);
            fputs("\"case_value\": ", out);
            if (c->case_value) {
                json_write_expr(out, c->case_value);
            } else {
                fputs("null", out);
            }
            fputs(", \"body\": ", out);
            json_write_stmt(out, c->body);
            fputs(" }", out);
        }
        fputs("] }", out);
        break;
    }

    case STMT_BLOCK:
        fputs("\"kind\": \"block\"", out);
        if (stmt->source_line > 0) {
            fputs(", \"source_line\": ", out);
            fprintf(out, "%d", stmt->source_line);
        }
        fputs(", \"statements\": [", out);
        for (int i = 0; i < stmt->u.block.count; ++i) {
            if (i > 0) fputs(", ", out);
            json_write_stmt(out, &stmt->u.block.stmts[i]);
        }
        fputs("] }", out);
        break;

    case STMT_MEM_WRITE:
        fputs("\"kind\": \"mem_write\"", out);
        if (stmt->source_line > 0) {
            fputs(", \"source_line\": ", out);
            fprintf(out, "%d", stmt->source_line);
        }
        fputs(", \"memory_name\": ", out);
        json_write_string(out, stmt->u.mem_write.memory_name ? stmt->u.mem_write.memory_name : "");
        fputs(", \"port_name\": ", out);
        json_write_string(out, stmt->u.mem_write.port_name ? stmt->u.mem_write.port_name : "");
        fputs(", \"address\": ", out);
        json_write_expr(out, stmt->u.mem_write.address);
        fputs(", \"data\": ", out);
        json_write_expr(out, stmt->u.mem_write.data);
        fputs(" }", out);
        break;

    default:
        /* Other statement kinds will be added later. */
        fputs("\"kind\": \"unknown\" }", out);
        break;
    }
}

static void json_write_signals(FILE *out, const IR_Module *mod)
{
    fprintf(out, "      \"signals\": [\n");
    for (int i = 0; i < mod->num_signals; ++i) {
        const IR_Signal *sig = &mod->signals[i];
        fprintf(out, "        {\n");
        fprintf(out, "          \"id\": %d,\n", sig->id);
        fprintf(out, "          \"name\": ");
        json_write_string(out, sig->name ? sig->name : "");
        fprintf(out, ",\n");
        fprintf(out, "          \"kind\": ");
        json_write_string(out, ir_signal_kind_string(sig->kind));
        fprintf(out, ",\n");
        fprintf(out, "          \"width\": %d,\n", sig->width);
        fprintf(out, "          \"owner_module_id\": %d,\n", sig->owner_module_id);
        fprintf(out, "          \"source_line\": %d,\n", sig->source_line);
        fprintf(out, "          \"can_be_z\": %s,\n",
                sig->can_be_z ? "true" : "false");
        fprintf(out, "          \"iob\": %s",
                sig->iob ? "true" : "false");

        if (sig->kind == SIG_PORT) {
            fprintf(out, ",\n          \"port\": { \"direction\": ");
            json_write_string(out, ir_port_direction_string(sig->u.port.direction));
            fprintf(out, " }\n");
        } else if (sig->kind == SIG_REGISTER) {
            fprintf(out,
                    ",\n          \"reg\": { \"reset_value\": { \"value\": %" PRIu64 ", \"width\": %d }, \"home_clock_domain_id\": %d }\n",
                    (unsigned long long)sig->u.reg.reset_value.words[0],
                    sig->u.reg.reset_value.width,
                    sig->u.reg.home_clock_domain_id);
        } else {
            fprintf(out, "\n");
        }

        fprintf(out, "        }");
        if (i + 1 < mod->num_signals) {
            fprintf(out, ",\n");
        } else {
            fprintf(out, "\n");
        }
    }
    fprintf(out, "      ]");
}

static const char *ir_clock_edge_string(IR_ClockEdge edge)
{
    switch (edge) {
    case EDGE_RISING:  return "RISING";
    case EDGE_FALLING: return "FALLING";
    case EDGE_BOTH:    return "BOTH";
    default:           return "UNKNOWN";
    }
}

static const char *ir_reset_polarity_string(IR_ResetPolarity pol)
{
    switch (pol) {
    case RESET_ACTIVE_HIGH: return "ACTIVE_HIGH";
    case RESET_ACTIVE_LOW:  return "ACTIVE_LOW";
    default:                return "UNKNOWN";
    }
}

static const char *ir_reset_type_string(IR_ResetType type)
{
    switch (type) {
    case RESET_IMMEDIATE: return "IMMEDIATE";
    case RESET_CLOCKED:   return "CLOCKED";
    default:              return "UNKNOWN";
    }
}

static void json_write_clock_domains(FILE *out, const IR_Module *mod)
{
    fprintf(out, "        \"clock_domains\": [\n");
    for (int i = 0; i < mod->num_clock_domains; ++i) {
        const IR_ClockDomain *cd = &mod->clock_domains[i];
        fprintf(out, "          {\n");
        fprintf(out, "            \"id\": %d,\n", cd->id);
        fprintf(out, "            \"clock_signal_id\": %d,\n", cd->clock_signal_id);
        fprintf(out, "            \"edge\": ");
        json_write_string(out, ir_clock_edge_string(cd->edge));
        fprintf(out, ",\n");

        fprintf(out, "            \"register_ids\": [");
        for (int j = 0; j < cd->num_registers; ++j) {
            if (j > 0) fprintf(out, ", ");
            fprintf(out, "%d", cd->register_ids[j]);
        }
        fprintf(out, "],\n");

        fprintf(out, "            \"reset_signal_id\": %d,\n", cd->reset_signal_id);
        fprintf(out, "            \"reset_sync_signal_id\": %d,\n", cd->reset_sync_signal_id);
        fprintf(out, "            \"reset_active\": ");
        json_write_string(out, ir_reset_polarity_string(cd->reset_active));
        fprintf(out, ",\n");
        fprintf(out, "            \"reset_type\": ");
        json_write_string(out, ir_reset_type_string(cd->reset_type));
        fprintf(out, ",\n");

        /* Sensitivity list */
        fprintf(out, "            \"sensitivity_list\": [");
        if (cd->sensitivity_list && cd->num_sensitivity > 0) {
            for (int si = 0; si < cd->num_sensitivity; ++si) {
                if (si > 0) fprintf(out, ", ");
                fprintf(out, "{ \"signal_id\": %d, \"edge\": ",
                        cd->sensitivity_list[si].signal_id);
                json_write_string(out, ir_clock_edge_string(cd->sensitivity_list[si].edge));
                fprintf(out, " }");
            }
        }
        fprintf(out, "],\n");

        fprintf(out, "            \"statements\": ");
        if (cd->statements) {
            json_write_stmt(out, cd->statements);
            fprintf(out, "\n");
        } else {
            fprintf(out, "null\n");
        }

        fprintf(out, "          }");
        if (i + 1 < mod->num_clock_domains) {
            fprintf(out, ",\n");
        } else {
            fprintf(out, "\n");
        }
    }
    fprintf(out, "        ]");
}

static const char *ir_mem_port_kind_string(IR_MemPortKind kind)
{
    switch (kind) {
    case MEM_PORT_READ_ASYNC: return "READ_ASYNC";
    case MEM_PORT_READ_SYNC:  return "READ_SYNC";
    case MEM_PORT_WRITE:      return "WRITE";
    default:                  return "UNKNOWN";
    }
}

static const char *ir_mem_write_mode_string(IR_MemWriteMode mode)
{
    switch (mode) {
    case WRITE_MODE_FIRST:      return "WRITE_FIRST";
    case WRITE_MODE_READ_FIRST: return "READ_FIRST";
    case WRITE_MODE_NO_CHANGE:  return "NO_CHANGE";
    default:                    return "UNKNOWN";
    }
}

static const char *ir_memory_kind_string(IR_MemoryKind kind)
{
    switch (kind) {
    case MEM_KIND_BLOCK:       return "BLOCK";
    case MEM_KIND_DISTRIBUTED: return "DISTRIBUTED";
    default:                   return "UNKNOWN";
    }
}

static void json_write_memories(FILE *out, const IR_Module *mod)
{
    fprintf(out, "        \"memories\": [\n");
    for (int i = 0; i < mod->num_memories; ++i) {
        const IR_Memory *m = &mod->memories[i];
        fprintf(out, "          {\n");
        fprintf(out, "            \"id\": %d,\n", m->id);
        fprintf(out, "            \"name\": ");
        json_write_string(out, m->name ? m->name : "");
        fprintf(out, ",\n");
        fprintf(out, "            \"type\": ");
        json_write_string(out, ir_memory_kind_string(m->kind));
        fprintf(out, ",\n");
        fprintf(out, "            \"word_width\": %d,\n", m->word_width);
        fprintf(out, "            \"depth\": %d,\n", m->depth);
        fprintf(out, "            \"address_width\": %d", m->address_width);

        if (m->init_is_file && m->init.file_path) {
            fprintf(out, ",\n            \"init_file_path\": ");
            json_write_string(out, m->init.file_path);
        } else if (!m->init_is_file && m->init.literal.width > 0) {
            fprintf(out,
                    ",\n            \"init_literal\": { \"value\": %" PRIu64 ", \"width\": %d }",
                    (unsigned long long)m->init.literal.words[0],
                    m->init.literal.width);
        }

        fprintf(out, ",\n            \"ports\": [\n");
        for (int j = 0; j < m->num_ports; ++j) {
            const IR_MemoryPort *p = &m->ports[j];
            fprintf(out, "              { ");
            fprintf(out, "\"name\": ");
            json_write_string(out, p->name ? p->name : "");
            fprintf(out, ", \"kind\": ");
            json_write_string(out, ir_mem_port_kind_string(p->kind));
            fprintf(out, ", \"address_width\": %d", p->address_width);
            if (p->kind == MEM_PORT_WRITE) {
                fprintf(out, ", \"write_mode\": ");
                json_write_string(out, ir_mem_write_mode_string(p->write_mode));
            }
            /* Optional binding metadata (may be -1 when unbound). */
            if (p->addr_signal_id >= 0) {
                fprintf(out, ", \"addr_signal_id\": %d", p->addr_signal_id);
            }
            if (p->data_in_signal_id >= 0) {
                fprintf(out, ", \"data_in_signal_id\": %d", p->data_in_signal_id);
            }
            if (p->data_out_signal_id >= 0) {
                fprintf(out, ", \"data_out_signal_id\": %d", p->data_out_signal_id);
            }
            if (p->enable_signal_id >= 0) {
                fprintf(out, ", \"enable_signal_id\": %d", p->enable_signal_id);
            }
            if (p->output_signal_id >= 0) {
                fprintf(out, ", \"output_signal_id\": %d", p->output_signal_id);
            }
            if (p->sync_clock_domain_id >= 0) {
                fprintf(out, ", \"sync_clock_domain_id\": %d", p->sync_clock_domain_id);
            }
            fprintf(out, " }");
            if (j + 1 < m->num_ports) {
                fprintf(out, ",\n");
            } else {
                fprintf(out, "\n");
            }
        }
        fprintf(out, "            ]\n");

        fprintf(out, "          }");
        if (i + 1 < mod->num_memories) {
            fprintf(out, ",\n");
        } else {
            fprintf(out, "\n");
        }
    }
    fprintf(out, "        ]");
}

static void json_write_instances(FILE *out, const IR_Module *mod)
{
    fprintf(out, "        \"instances\": [\n");
    for (int i = 0; i < mod->num_instances; ++i) {
        const IR_Instance *inst = &mod->instances[i];
        fprintf(out, "          {\n");
        fprintf(out, "            \"id\": %d,\n", inst->id);
        fprintf(out, "            \"name\": ");
        json_write_string(out, inst->name ? inst->name : "");
        fprintf(out, ",\n");
        fprintf(out, "            \"child_module_id\": %d,\n", inst->child_module_id);
        fprintf(out, "            \"connections\": [");
        for (int j = 0; j < inst->num_connections; ++j) {
            const IR_InstanceConnection *c = &inst->connections[j];
            if (j > 0) {
                fprintf(out, ", ");
            }
            fprintf(out, "{ \"parent_signal_id\": %d, \"child_port_id\": %d",
                    c->parent_signal_id,
                    c->child_port_id);
            if (c->const_expr && c->const_expr[0] != '\0') {
                fprintf(out, ", \"const_expr\": ");
                json_write_string(out, c->const_expr);
            }
            if (c->parent_msb >= 0 && c->parent_lsb >= 0) {
                fprintf(out,
                        ", \"parent_msb\": %d, \"parent_lsb\": %d",
                        c->parent_msb,
                        c->parent_lsb);
            }
            fputs(" }", out);
        }
        fprintf(out, "]\n");
        fprintf(out, "          }");
        if (i + 1 < mod->num_instances) {
            fprintf(out, ",\n");
        } else {
            fprintf(out, "\n");
        }
    }
    fprintf(out, "        ]");
}

static const char *ir_cdc_type_string(IR_CDCType type)
{
    switch (type) {
    case CDC_BIT:       return "BIT";
    case CDC_BUS:       return "BUS";
    case CDC_FIFO:      return "FIFO";
    case CDC_HANDSHAKE: return "HANDSHAKE";
    case CDC_PULSE:     return "PULSE";
    case CDC_MCP:       return "MCP";
    case CDC_RAW:       return "RAW";
    default:            return "UNKNOWN";
    }
}

static void json_write_cdc_crossings(FILE *out, const IR_Module *mod)
{
    fprintf(out, "        \"cdc_crossings\": [\n");
    for (int i = 0; i < mod->num_cdc_crossings; ++i) {
        const IR_CDC *c = &mod->cdc_crossings[i];
        fprintf(out, "          {");
        fprintf(out, "\"id\": %d, ", c->id);
        fprintf(out, "\"source_reg_id\": %d, ", c->source_reg_id);
        if (c->source_msb >= 0) {
            fprintf(out, "\"source_msb\": %d, ", c->source_msb);
            fprintf(out, "\"source_lsb\": %d, ", c->source_lsb);
        }
        fprintf(out, "\"source_clock_id\": %d, ", c->source_clock_id);
        fprintf(out, "\"dest_alias_name\": ");
        json_write_string(out, c->dest_alias_name ? c->dest_alias_name : "");
        fprintf(out, ", ");
        fprintf(out, "\"dest_clock_id\": %d, ", c->dest_clock_id);
        fprintf(out, "\"type\": ");
        json_write_string(out, ir_cdc_type_string(c->type));
        fprintf(out, "}");
        if (i + 1 < mod->num_cdc_crossings) {
            fprintf(out, ",\n");
        } else {
            fprintf(out, "\n");
        }
    }
    fprintf(out, "        ]");
}

static const char *ir_pin_kind_string(IR_PinKind kind)
{
    switch (kind) {
    case PIN_IN:    return "IN";
    case PIN_OUT:   return "OUT";
    case PIN_INOUT: return "INOUT";
    default:        return "UNKNOWN";
    }
}

static void json_write_project(FILE *out, const IR_Design *design)
{
    const IR_Project *proj = design->project;
    if (!proj) {
        fputs("null", out);
        return;
    }

    fputs("{\n", out);
    /* Project name */
    fputs("      \"name\": ", out);
    json_write_string(out, proj->name ? proj->name : "");
    fputs(",\n", out);

    /* Clocks */
    fputs("      \"clocks\": [\n", out);
    for (int i = 0; i < proj->num_clocks; ++i) {
        const IR_Clock *c = &proj->clocks[i];
        fputs("        { ", out);
        fputs("\"name\": ", out);
        json_write_string(out, c->name ? c->name : "");
        fputs(", ", out);
        fputs("\"period_ns\": ", out);
        /* Use a compact floating-point format to preserve fractional periods
         * like 37.04 without introducing excessive precision noise.
         */
        fprintf(out, "%.9g", c->period_ns);
        fputs(", ", out);
        fputs("\"edge\": ", out);
        json_write_string(out, ir_clock_edge_string(c->edge));
        fputs(" }", out);
        if (i + 1 < proj->num_clocks) {
            fputs(",\n", out);
        } else {
            fputs("\n", out);
        }
    }
    fputs("      ],\n", out);

    /* Clock generators */
    fputs("      \"clock_gens\": [\n", out);
    for (int i = 0; i < proj->num_clock_gens; ++i) {
        const IR_ClockGen *cg = &proj->clock_gens[i];
        fputs("        {\n", out);
        fputs("          \"units\": [\n", out);
        for (int u = 0; u < cg->num_units; ++u) {
            const IR_ClockGenUnit *unit = &cg->units[u];
            fputs("            {\n", out);
            fputs("              \"type\": ", out);
            {
                /* Write type as uppercase */
                char upper_buf[32];
                const char *src_type = unit->type ? unit->type : "pll";
                size_t tl = strlen(src_type);
                if (tl >= sizeof(upper_buf)) tl = sizeof(upper_buf) - 1;
                for (size_t ti = 0; ti < tl; ++ti)
                    upper_buf[ti] = (char)toupper((unsigned char)src_type[ti]);
                upper_buf[tl] = '\0';
                json_write_string(out, upper_buf);
            }
            fputs(",\n", out);
            fputs("              \"inputs\": [\n", out);
            for (int inp_i = 0; inp_i < unit->num_inputs; ++inp_i) {
                const IR_ClockGenInput *inp = &unit->inputs[inp_i];
                fputs("                { \"selector\": ", out);
                if (inp->selector) {
                    json_write_string(out, inp->selector);
                } else {
                    fputs("null", out);
                }
                fputs(", \"signal_name\": ", out);
                if (inp->signal_name) {
                    json_write_string(out, inp->signal_name);
                } else {
                    fputs("null", out);
                }
                fputs(" }", out);
                if (inp_i + 1 < unit->num_inputs) {
                    fputs(",\n", out);
                } else {
                    fputs("\n", out);
                }
            }
            fputs("              ],\n", out);
            fputs("              \"outputs\": [\n", out);
            for (int o = 0; o < unit->num_outputs; ++o) {
                const IR_ClockGenOutput *out_clk = &unit->outputs[o];
                fputs("                { \"selector\": ", out);
                if (out_clk->selector) {
                    json_write_string(out, out_clk->selector);
                } else {
                    fputs("null", out);
                }
                fputs(", \"clock_name\": ", out);
                if (out_clk->clock_name) {
                    json_write_string(out, out_clk->clock_name);
                } else {
                    fputs("null", out);
                }
                fputs(" }", out);
                if (o + 1 < unit->num_outputs) {
                    fputs(",\n", out);
                } else {
                    fputs("\n", out);
                }
            }
            fputs("              ],\n", out);
            fputs("              \"configs\": [\n", out);
            for (int c = 0; c < unit->num_configs; ++c) {
                const IR_ClockGenConfig *cfg = &unit->configs[c];
                fputs("                { \"param_name\": ", out);
                if (cfg->param_name) {
                    json_write_string(out, cfg->param_name);
                } else {
                    fputs("null", out);
                }
                fputs(", \"param_value\": ", out);
                if (cfg->param_value) {
                    json_write_string(out, cfg->param_value);
                } else {
                    fputs("null", out);
                }
                fputs(" }", out);
                if (c + 1 < unit->num_configs) {
                    fputs(",\n", out);
                } else {
                    fputs("\n", out);
                }
            }
            fputs("              ]\n", out);
            fputs("            }", out);
            if (u + 1 < cg->num_units) {
                fputs(",\n", out);
            } else {
                fputs("\n", out);
            }
        }
        fputs("          ]\n", out);
        fputs("        }", out);
        if (i + 1 < proj->num_clock_gens) {
            fputs(",\n", out);
        } else {
            fputs("\n", out);
        }
    }
    fputs("      ],\n", out);

    /* Pins */
    fputs("      \"pins\": [\n", out);
    for (int i = 0; i < proj->num_pins; ++i) {
        const IR_Pin *p = &proj->pins[i];
        fputs("        { ", out);
        fputs("\"id\": ", out);
        fprintf(out, "%d", p->id);
        fputs(", ", out);
        fputs("\"name\": ", out);
        json_write_string(out, p->name ? p->name : "");
        fputs(", ", out);
        fputs("\"kind\": ", out);
        json_write_string(out, ir_pin_kind_string(p->kind));
        fputs(", ", out);
        fputs("\"width\": ", out);
        fprintf(out, "%d", p->width);
        fputs(", ", out);
        fputs("\"standard\": ", out);
        json_write_string(out, p->standard ? p->standard : "");
        fputs(", ", out);
        fputs("\"drive_ma\": ", out);
        fprintf(out, "%d", p->drive_ma);
        fputs(", ", out);
        fputs("\"drive\": ", out);
        fprintf(out, "%g", p->drive);
        fputs(", ", out);
        fputs("\"mode\": ", out);
        json_write_string(out, p->mode == PIN_MODE_DIFFERENTIAL ? "DIFFERENTIAL" : "SINGLE");
        fputs(", ", out);
        fputs("\"term\": ", out);
        fprintf(out, "%s", p->term ? "true" : "false");
        fputs(", ", out);
        fputs("\"pull\": ", out);
        json_write_string(out, p->pull == PULL_UP ? "UP" : p->pull == PULL_DOWN ? "DOWN" : "NONE");
        fputs(" }", out);
        if (i + 1 < proj->num_pins) {
            fputs(",\n", out);
        } else {
            fputs("\n", out);
        }
    }
    fputs("      ],\n", out);

    /* Pin mappings */
    fputs("      \"mappings\": [\n", out);
    for (int i = 0; i < proj->num_mappings; ++i) {
        const IR_PinMapping *m = &proj->mappings[i];
        fputs("        { ", out);
        fputs("\"logical_pin_name\": ", out);
        json_write_string(out, m->logical_pin_name ? m->logical_pin_name : "");
        fputs(", ", out);
        fputs("\"bit_index\": ", out);
        fprintf(out, "%d", m->bit_index);
        fputs(", ", out);
        fputs("\"board_pin_id\": ", out);
        json_write_string(out, m->board_pin_id ? m->board_pin_id : "");
        if (m->board_pin_n_id && m->board_pin_n_id[0] != '\0') {
            fputs(", ", out);
            fputs("\"board_pin_n_id\": ", out);
            json_write_string(out, m->board_pin_n_id);
        }
        fputs(" }", out);
        if (i + 1 < proj->num_mappings) {
            fputs(",\n", out);
        } else {
            fputs("\n", out);
        }
    }
    fputs("      ],\n", out);

    /* Top-level bindings */
    fputs("      \"top_module_id\": ", out);
    fprintf(out, "%d", proj->top_module_id);
    fputs(",\n", out);

    fputs("      \"top_bindings\": [\n", out);
    for (int i = 0; i < proj->num_top_bindings; ++i) {
        const IR_TopBinding *tb = &proj->top_bindings[i];
        fputs("        { ", out);
        fputs("\"top_port_signal_id\": ", out);
        fprintf(out, "%d", tb->top_port_signal_id);
        fputs(", ", out);
        fputs("\"top_bit_index\": ", out);
        fprintf(out, "%d", tb->top_bit_index);
        fputs(", ", out);
        fputs("\"pin_id\": ", out);
        fprintf(out, "%d", tb->pin_id);
        fputs(", ", out);
        fputs("\"pin_bit_index\": ", out);
        fprintf(out, "%d", tb->pin_bit_index);
        if (tb->pin_id < 0) {
            fputs(", ", out);
            fputs("\"const_value\": ", out);
            fprintf(out, "%d", tb->const_value);
        }
        if (tb->inverted != 0) {
            fputs(", ", out);
            fputs("\"inverted\": ", out);
            fprintf(out, "%d", tb->inverted);
        }
        fputs(" }", out);
        if (i + 1 < proj->num_top_bindings) {
            fputs(",\n", out);
        } else {
            fputs("\n", out);
        }
    }
    fputs("      ]\n", out);

    fputs("    }", out);
}

int jz_ir_write_json(const IR_Design *design,
                     const char *filename,
                     JZDiagnosticList *diagnostics,
                     const char *input_filename)
{
    if (!design || !filename) {
        return -1;
    }

    FILE *out = NULL;
    int close_out = 0;
    char tmp_path[1024];
    tmp_path[0] = '\0';

    if (strcmp(filename, "-") == 0) {
        /* Stream directly to stdout; atomic semantics do not apply. */
        out = stdout;
        close_out = 0;
    } else {
        /* Write to a temporary file in the same directory, then rename on
         * success so callers never see a partially written IR file.
         */
        int n = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", filename);
        if (n <= 0 || (size_t)n >= sizeof(tmp_path)) {
            if (diagnostics && input_filename) {
                JZLocation loc = { input_filename, 1, 1 };
                jz_diagnostic_report(diagnostics,
                                     loc,
                                     JZ_SEVERITY_ERROR,
                                     "IR_IO",
                                     "failed to construct temporary IR output filename");
            }
            return -1;
        }

        out = fopen(tmp_path, "w");
        if (!out) {
            if (diagnostics && input_filename) {
                JZLocation loc = { input_filename, 1, 1 };
                jz_diagnostic_report(diagnostics,
                                     loc,
                                     JZ_SEVERITY_ERROR,
                                     "IR_IO",
                                     "failed to open temporary IR output file for writing");
            }
            return -1;
        }
        close_out = 1;
    }

    fprintf(out, "{\n");
    fprintf(out, "  \"ir_version\": \"0.1.0\",\n");
    fprintf(out, "  \"design\": {\n");

    /* Design name */
    fprintf(out, "    \"name\": ");
    json_write_string(out, design->name ? design->name : "");
    fprintf(out, ",\n");

    /* Modules */
    fprintf(out, "    \"modules\": [\n");
    for (int mi = 0; mi < design->num_modules; ++mi) {
        const IR_Module *mod = &design->modules[mi];
        fprintf(out, "      {\n");
        fprintf(out, "        \"id\": %d,\n", mod->id);
        fprintf(out, "        \"name\": ");
        json_write_string(out, mod->name ? mod->name : "");
        fprintf(out, ",\n");
        fprintf(out, "        \"base_module_id\": %d,\n", mod->base_module_id);
        fprintf(out, "        \"source_line\": %d,\n", mod->source_line);
        fprintf(out, "        \"source_file\": ");
        if (mod->source_file_id >= 0 &&
            mod->source_file_id < design->num_source_files) {
            const IR_SourceFile *sf = &design->source_files[mod->source_file_id];
            json_write_string(out, sf->path ? sf->path : "");
        } else {
            json_write_string(out, "");
        }
        fprintf(out, ",\n");

        json_write_signals(out, mod);
        fprintf(out, ",\n");

        /* Other IR sections. */
        json_write_clock_domains(out, mod);
        fprintf(out, ",\n");
        json_write_instances(out, mod);
        fprintf(out, ",\n");
        json_write_memories(out, mod);
        fprintf(out, ",\n");
        json_write_cdc_crossings(out, mod);
        fprintf(out, ",\n");
        fprintf(out, "        \"async_block\": ");
        if (mod->async_block) {
            json_write_stmt(out, mod->async_block);
            fprintf(out, "\n");
        } else {
            fprintf(out, "null\n");
        }

        fprintf(out, "      }");
        if (mi + 1 < design->num_modules) {
            fprintf(out, ",\n");
        } else {
            fprintf(out, "\n");
        }
    }
    fprintf(out, "    ],\n");

    /* Project-level IR: serialize IR_Project when present. */
    fprintf(out, "    \"project\": ");
    if (design->project) {
        json_write_project(out, design);
        fprintf(out, ",\n");
    } else {
        fprintf(out, "null,\n");
    }

    /* Source files metadata. */
    fprintf(out, "    \"source_files\": [\n");
    for (int i = 0; i < design->num_source_files; ++i) {
        const IR_SourceFile *sf = &design->source_files[i];
        fprintf(out, "      { \"path\": ");
        json_write_string(out, sf->path ? sf->path : "");
        fprintf(out, ", \"line_count\": %d }", sf->line_count);
        if (i + 1 < design->num_source_files) {
            fprintf(out, ",\n");
        } else {
            fprintf(out, "\n");
        }
    }
    fprintf(out, "    ]\n");

    fprintf(out, "  }\n");
    fprintf(out, "}\n");

    if (close_out) {
        /* For file-backed output, ensure we detect I/O errors before renaming
         * the temporary file into place.
         */
        if (fflush(out) != 0 || ferror(out)) {
            fclose(out);
            if (tmp_path[0] != '\0') {
                /* Best-effort cleanup; ignore errors. */
                (void)remove(tmp_path);
            }
            if (diagnostics && input_filename) {
                JZLocation loc = { input_filename, 1, 1 };
                jz_diagnostic_report(diagnostics,
                                     loc,
                                     JZ_SEVERITY_ERROR,
                                     "IR_IO",
                                     "failed to write complete IR output");
            }
            return -1;
        }
        if (fclose(out) != 0) {
            if (tmp_path[0] != '\0') {
                (void)remove(tmp_path);
            }
            if (diagnostics && input_filename) {
                JZLocation loc = { input_filename, 1, 1 };
                jz_diagnostic_report(diagnostics,
                                     loc,
                                     JZ_SEVERITY_ERROR,
                                     "IR_IO",
                                     "failed to close IR output stream");
            }
            return -1;
        }
        if (tmp_path[0] != '\0') {
            if (rename(tmp_path, filename) != 0) {
                (void)remove(tmp_path);
                if (diagnostics && input_filename) {
                    JZLocation loc = { input_filename, 1, 1 };
                    jz_diagnostic_report(diagnostics,
                                         loc,
                                         JZ_SEVERITY_ERROR,
                                         "IR_IO",
                                         "failed to move temporary IR file into place");
                }
                return -1;
            }
        }
    }
    return 0;
}
