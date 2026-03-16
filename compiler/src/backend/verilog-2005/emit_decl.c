/*
 * emit_decl.c - Declaration emission for the Verilog-2005 backend.
 *
 * This file handles emitting module headers, port declarations, signal
 * declarations, and memory declarations.
 */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "verilog_internal.h"
#include "ir.h"

/* -------------------------------------------------------------------------
 * Helper: determine if a statement assigns to a given signal
 * -------------------------------------------------------------------------
 */

static int stmt_assigns_to_signal(const IR_Stmt *stmt, int signal_id)
{
    if (!stmt) return 0;

    switch (stmt->kind) {
    case STMT_ASSIGNMENT:
        if (assignment_kind_is_alias(stmt->u.assign.kind)) return 0;
        return (stmt->u.assign.lhs_signal_id == signal_id);

    case STMT_IF: {
        const IR_IfStmt *ifs = &stmt->u.if_stmt;
        if (stmt_assigns_to_signal(ifs->then_block, signal_id)) return 1;
        const IR_Stmt *elif = ifs->elif_chain;
        while (elif && elif->kind == STMT_IF) {
            const IR_IfStmt *eifs = &elif->u.if_stmt;
            if (stmt_assigns_to_signal(eifs->then_block, signal_id)) return 1;
            elif = eifs->elif_chain;
        }
        if (stmt_assigns_to_signal(ifs->else_block, signal_id)) return 1;
        return 0;
    }

    case STMT_SELECT: {
        const IR_SelectStmt *sel = &stmt->u.select_stmt;
        for (int i = 0; i < sel->num_cases; ++i) {
            if (stmt_assigns_to_signal(sel->cases[i].body, signal_id)) return 1;
        }
        return 0;
    }

    case STMT_BLOCK: {
        const IR_BlockStmt *blk = &stmt->u.block;
        for (int i = 0; i < blk->count; ++i) {
            if (stmt_assigns_to_signal(&blk->stmts[i], signal_id)) return 1;
        }
        return 0;
    }

    default:
        return 0;
    }
}

/* -------------------------------------------------------------------------
 * Helper: determine if a port needs to be declared as reg
 * -------------------------------------------------------------------------
 */

int module_port_needs_reg(const IR_Module *mod, int signal_id)
{
    if (!mod) return 0;

    IR_Stmt *stack[512];
    int      top = 0;
    const int stack_cap = (int)(sizeof(stack) / sizeof(stack[0]));

    if (mod->async_block) {
        stack[top++] = mod->async_block;
    }
    for (int i = 0; i < mod->num_clock_domains; ++i) {
        const IR_ClockDomain *cd = &mod->clock_domains[i];
        if (cd->statements) {
            stack[top++] = cd->statements;
        }
    }

    /* Pre-resolve the target signal through aliasing so we can match
     * assignments that target an aliased signal (e.g., bin_dest -> data_out).
     */
    const IR_Signal *target_sig = find_signal_by_id(mod, signal_id);

    while (top > 0) {
        IR_Stmt *stmt = stack[--top];
        if (!stmt) continue;

        switch (stmt->kind) {
        case STMT_ASSIGNMENT: {
            const IR_Assignment *a = &stmt->u.assign;
            if (!assignment_kind_is_alias(a->kind)) {
                /* Check direct match or alias-resolved match */
                if (a->lhs_signal_id == signal_id) {
                    return 1;
                }
                if (target_sig) {
                    const IR_Signal *lhs_sig = find_signal_by_id(mod, a->lhs_signal_id);
                    if (lhs_sig && lhs_sig == target_sig) {
                        return 1;
                    }
                }
            }
            break;
        }
        case STMT_IF: {
            const IR_IfStmt *ifs = &stmt->u.if_stmt;
            if (ifs->then_block && top < stack_cap) stack[top++] = ifs->then_block;
            IR_Stmt *elif = ifs->elif_chain;
            while (elif && elif->kind == STMT_IF && top < stack_cap) {
                const IR_IfStmt *eifs = &elif->u.if_stmt;
                if (eifs->then_block && top < stack_cap) stack[top++] = eifs->then_block;
                elif = eifs->elif_chain;
            }
            if (ifs->else_block && top < stack_cap) stack[top++] = ifs->else_block;
            break;
        }
        case STMT_SELECT: {
            const IR_SelectStmt *sel = &stmt->u.select_stmt;
            for (int i = 0; i < sel->num_cases && top < stack_cap; ++i) {
                if (sel->cases[i].body) {
                    stack[top++] = sel->cases[i].body;
                }
            }
            break;
        }
        case STMT_BLOCK: {
            const IR_BlockStmt *blk = &stmt->u.block;
            for (int i = 0; i < blk->count && top < stack_cap; ++i) {
                stack[top++] = &blk->stmts[i];
            }
            break;
        }
        default:
            break;
        }

        if (top >= stack_cap) {
            break;
        }
    }

    return 0;
}

/* -------------------------------------------------------------------------
 * Helper: determine if a signal is written in any block
 * -------------------------------------------------------------------------
 */

int module_signal_is_written(const IR_Module *mod, int signal_id)
{
    if (!mod) return 0;

    if (mod->async_block && stmt_assigns_to_signal(mod->async_block, signal_id)) {
        return 1;
    }
    for (int i = 0; i < mod->num_clock_domains; ++i) {
        const IR_ClockDomain *cd = &mod->clock_domains[i];
        if (cd->statements && stmt_assigns_to_signal(cd->statements, signal_id)) {
            return 1;
        }
    }
    return 0;
}

/* -------------------------------------------------------------------------
 * Module header emission
 * -------------------------------------------------------------------------
 */

void emit_module_header(FILE *out, const IR_Module *mod)
{
    const char *name = (mod->name && mod->name[0] != '\0') ? mod->name : "jz_unnamed_module";

    int num_ports = 0;
    for (int i = 0; i < mod->num_signals; ++i) {
        if (mod->signals[i].kind == SIG_PORT) {
            ++num_ports;
        }
    }

    if (num_ports == 0) {
        fprintf(out, "module %s;\n", name);
        return;
    }

    fprintf(out, "module %s (\n", name);

    int seen = 0;
    for (int i = 0; i < mod->num_signals; ++i) {
        IR_Signal *sig = &mod->signals[i];
        if (sig->kind != SIG_PORT) {
            continue;
        }
        char esc[256];
        const char *sn = verilog_safe_name(sig->name ? sig->name : "jz_unnamed_port", esc, (int)sizeof(esc));
        fprintf(out, "    %s%s\n",
                sn,
                (seen + 1 < num_ports) ? "," : "");
        ++seen;
    }

    fprintf(out, ");\n");
}

/* -------------------------------------------------------------------------
 * Port declaration emission
 * -------------------------------------------------------------------------
 */

void emit_port_declarations(FILE *out, const IR_Module *mod)
{
    fprintf(out, "    // Ports\n");

    for (int i = 0; i < mod->num_signals; ++i) {
        const IR_Signal *sig = &mod->signals[i];
        if (sig->kind != SIG_PORT) {
            continue;
        }

        const char *dir = "input";
        const char *extra = "";
        switch (sig->u.port.direction) {
            case PORT_IN:
                dir = "input";
                break;
            case PORT_OUT: {
                dir = "output";
                if (module_port_needs_reg(mod, sig->id)) {
                    extra = " reg";
                }
                break;
            }
            case PORT_INOUT:
                dir = "inout";
                break;
            default:
                dir = "input";
                break;
        }

        fprintf(out, "    %s%s", dir, extra);
        emit_width_range(out, sig->width);
        char esc2[256];
        const char *sn2 = verilog_safe_name(sig->name ? sig->name : "jz_unnamed_port", esc2, (int)sizeof(esc2));
        fprintf(out, " %s;\n", sn2);
    }

    fputc('\n', out);
}

/* -------------------------------------------------------------------------
 * Helper: collect signal IDs used as SELECT selectors in sync blocks
 * -------------------------------------------------------------------------
 */

static void collect_select_signals_from_stmt(const IR_Stmt *stmt, int *ids, int *count, int cap)
{
    if (!stmt) return;

    switch (stmt->kind) {
    case STMT_SELECT: {
        const IR_SelectStmt *sel = &stmt->u.select_stmt;
        if (sel->selector && sel->selector->kind == EXPR_SIGNAL_REF && *count < cap) {
            int sid = sel->selector->u.signal_ref.signal_id;
            /* Avoid duplicates */
            bool found = false;
            for (int i = 0; i < *count; i++) {
                if (ids[i] == sid) { found = true; break; }
            }
            if (!found) ids[(*count)++] = sid;
        }
        /* Recurse into case bodies */
        for (int i = 0; i < sel->num_cases; i++) {
            collect_select_signals_from_stmt(sel->cases[i].body, ids, count, cap);
        }
        break;
    }
    case STMT_IF: {
        const IR_IfStmt *ifs = &stmt->u.if_stmt;
        collect_select_signals_from_stmt(ifs->then_block, ids, count, cap);
        collect_select_signals_from_stmt(ifs->else_block, ids, count, cap);
        break;
    }
    case STMT_BLOCK: {
        const IR_BlockStmt *blk = &stmt->u.block;
        for (int i = 0; i < blk->count; i++) {
            collect_select_signals_from_stmt(&blk->stmts[i], ids, count, cap);
        }
        break;
    }
    default:
        break;
    }
}

static bool signal_is_select_selector(const IR_Module *mod, int signal_id)
{
    int ids[64];
    int count = 0;

    for (int d = 0; d < mod->num_clock_domains; d++) {
        const IR_ClockDomain *cd = &mod->clock_domains[d];
        if (cd->statements) {
            collect_select_signals_from_stmt(cd->statements, ids, &count, 64);
        }
    }

    for (int i = 0; i < count; i++) {
        if (ids[i] == signal_id) return true;
    }
    return false;
}

/* -------------------------------------------------------------------------
 * Internal signal declaration emission
 * -------------------------------------------------------------------------
 */

void emit_internal_signal_declarations(FILE *out, const IR_Module *mod)
{
    fprintf(out, "    // Signals\n");

    for (int i = 0; i < mod->num_signals; ++i) {
        const IR_Signal *sig = &mod->signals[i];
        if (sig->kind == SIG_PORT) {
            continue;
        }

        if (sig->kind == SIG_NET && !alias_ctx_is_representative(mod, i)) {
            continue;
        }

        const char *kw = NULL;
        switch (sig->kind) {
            case SIG_NET:
                kw = module_signal_is_written(mod, sig->id) ? "reg" : "wire";
                break;
            case SIG_REGISTER:
                kw = "reg";
                break;
            case SIG_LATCH:
                kw = "reg";
                break;
            default:
                continue;
        }

        int width_for_decl = sig->width;
        if (sig->kind == SIG_NET && (width_for_decl <= 0) && mod->num_cdc_crossings > 0 && sig->name) {
            for (int c = 0; c < mod->num_cdc_crossings; ++c) {
                const IR_CDC *cdc = &mod->cdc_crossings[c];
                if (!cdc || !cdc->dest_alias_name) {
                    continue;
                }
                if (strcmp(cdc->dest_alias_name, sig->name) != 0) {
                    continue;
                }
                const IR_Signal *src_reg = find_signal_by_id(mod, cdc->source_reg_id);
                if (src_reg && src_reg->width > 0) {
                    width_for_decl = src_reg->width;
                    break;
                }
            }
        }

        /* Prevent yosys FSM extraction from re-encoding registers used
         * as SELECT selectors — the user's encoding is intentional and
         * the FSM optimizer can incorrectly discard reachable states. */
        if (sig->kind == SIG_REGISTER && signal_is_select_selector(mod, sig->id)) {
            fprintf(out, "    (* fsm_encoding = \"none\" *) %s", kw);
        } else if (sig->kind == SIG_LATCH && sig->iob) {
            fprintf(out, "    (* IOB = \"TRUE\" *) %s", kw);
        } else {
            fprintf(out, "    %s", kw);
        }
        emit_width_range(out, width_for_decl);
        char esc3[256];
        const char *sn3 = verilog_safe_name(sig->name ? sig->name : "jz_unnamed_signal", esc3, (int)sizeof(esc3));
        fprintf(out, " %s;\n", sn3);
    }

    fputc('\n', out);
}

/* -------------------------------------------------------------------------
 * Memory declaration emission
 * -------------------------------------------------------------------------
 */

void emit_memory_declarations(FILE *out, const IR_Module *mod)
{
    if (!out || !mod || mod->num_memories <= 0) {
        return;
    }

    fprintf(out, "    // Memories\n");

    for (int i = 0; i < mod->num_memories; ++i) {
        const IR_Memory *m = &mod->memories[i];
        const char *raw_name = (m->name && m->name[0] != '\0') ? m->name : "jz_mem";
        char mem_safe_buf[256];
        const char *name = verilog_memory_name(raw_name, mod->name, mem_safe_buf, sizeof(mem_safe_buf));
        bool needs_literal_init = !m->init_is_file && m->init.literal.width > 0 && m->depth > 0;
        char init_i_name[64];

        /* Declare implicit address registers for SYNC read ports.
         * Skip ports with synthetic addr register signals; those are
         * already declared as normal signals.
         */
        for (int p = 0; p < m->num_ports; ++p) {
            const IR_MemoryPort *mp = &m->ports[p];
            if (mp->kind != MEM_PORT_READ_SYNC || mp->addr_signal_id < 0) {
                continue;
            }
            if (mp->addr_reg_signal_id >= 0) {
                continue;
            }
            const char *port_name = (mp->name && mp->name[0] != '\0') ? mp->name : "rd";
            int addr_w = mp->address_width > 0 ? mp->address_width : m->address_width;
            fprintf(out, "    reg");
            emit_width_range(out, addr_w);
            fprintf(out, " %s_%s_addr;\n", name, port_name);
        }

        if (m->kind == MEM_KIND_BLOCK) {
            fprintf(out, "    (* ram_style = \"block\" *) reg");
        } else if (m->kind == MEM_KIND_DISTRIBUTED) {
            fprintf(out, "    (* ram_style = \"distributed\" *) reg");
        } else {
            fprintf(out, "    reg");
        }
        emit_width_range(out, m->word_width);
        if (m->depth > 0) {
            fprintf(out, " %s[0:%d];\n", name, m->depth - 1);
        } else {
            fprintf(out, " %s;\n", name);
        }

        if (needs_literal_init) {
            snprintf(init_i_name, sizeof(init_i_name), "jz_mem_init_i_%d", i);
            fprintf(out, "    integer %s;\n", init_i_name);
        }
    }

    fputc('\n', out);
}

/* -------------------------------------------------------------------------
 * Memory initialization emission
 * -------------------------------------------------------------------------
 */

/* Extract file extension from a path (pointer into original string). */
static const char *mem_init_get_ext(const char *path)
{
    if (!path) return NULL;
    const char *last_slash = strrchr(path, '/');
#ifdef _WIN32
    const char *last_bslash = strrchr(path, '\\');
    if (!last_slash || (last_bslash && last_bslash > last_slash))
        last_slash = last_bslash;
#endif
    const char *name = last_slash ? last_slash + 1 : path;
    const char *dot = strrchr(name, '.');
    if (!dot || !*(dot + 1)) return NULL;
    return dot + 1;
}

/* Case-insensitive extension comparison. */
static int mem_init_ext_eq(const char *ext, const char *want)
{
    if (!ext || !want) return 0;
    for (; *ext && *want; ++ext, ++want) {
        char c1 = (*ext >= 'A' && *ext <= 'Z') ? (char)(*ext + 32) : *ext;
        char c2 = (*want >= 'A' && *want <= 'Z') ? (char)(*want + 32) : *want;
        if (c1 != c2) return 0;
    }
    return *ext == '\0' && *want == '\0';
}

/* Convert a raw binary file to a $readmemh-compatible hex text file.
 * The output file is written alongside the input with a ".hex" suffix
 * appended (e.g. "out/sample.bin" -> "out/sample.bin.hex").
 * Returns the path to the hex file (static buffer), or NULL on failure.
 */
static const char *mem_init_convert_bin_to_hex(const char *bin_path,
                                               int word_width,
                                               int depth)
{
    static char hex_path[1024];
    int n = snprintf(hex_path, sizeof(hex_path), "%s.hex", bin_path);
    if (n <= 0 || (size_t)n >= sizeof(hex_path)) return NULL;

    FILE *fin = fopen(bin_path, "rb");
    if (!fin) return NULL;

    FILE *fout = fopen(hex_path, "w");
    if (!fout) { fclose(fin); return NULL; }

    int bytes_per_word = (word_width + 7) / 8;
    int hex_chars = (word_width + 3) / 4;
    unsigned char buf[16]; /* max 128-bit words */
    if (bytes_per_word > (int)sizeof(buf)) bytes_per_word = (int)sizeof(buf);

    for (int addr = 0; addr < depth; ++addr) {
        size_t got = fread(buf, 1, (size_t)bytes_per_word, fin);

        /* Zero-pad if file is shorter than memory depth. */
        for (size_t j = got; j < (size_t)bytes_per_word; ++j)
            buf[j] = 0;

        /* Build big-endian hex value from bytes. */
        unsigned long long val = 0;
        for (int b = 0; b < bytes_per_word; ++b)
            val = (val << 8) | buf[b];

        fprintf(fout, "%0*llX\n", hex_chars, val);
    }

    fclose(fin);
    fclose(fout);
    return hex_path;
}

int emit_memory_initialization(FILE *out, const IR_Module *mod)
{
    if (!out || !mod || mod->num_memories <= 0) {
        return 0;
    }
    int errors = 0;

    for (int i = 0; i < mod->num_memories; ++i) {
        const IR_Memory *m = &mod->memories[i];
        const char *raw_name = (m->name && m->name[0] != '\0') ? m->name : "jz_mem";
        char mem_safe_buf[256];
        const char *name = verilog_memory_name(raw_name, mod->name, mem_safe_buf, sizeof(mem_safe_buf));

        if (!m->init_is_file && m->init.literal.width <= 0) {
            continue;
        }

        if (m->init_is_file) {
            if (!m->init.file_path || m->init.file_path[0] == '\0') {
                continue;
            }

            const char *ext = mem_init_get_ext(m->init.file_path);

            if (ext && mem_init_ext_eq(ext, "mem")) {
                /* .mem: Verilog binary text format -> $readmemb */
                fprintf(out, "    initial begin\n");
                fprintf(out, "        $readmemb(\"%s\", %s);\n",
                        m->init.file_path, name);
                fprintf(out, "    end\n");
            } else if (ext && mem_init_ext_eq(ext, "hex")) {
                /* .hex: hex text format -> $readmemh */
                fprintf(out, "    initial begin\n");
                fprintf(out, "        $readmemh(\"%s\", %s);\n",
                        m->init.file_path, name);
                fprintf(out, "    end\n");
            } else {
                /* .bin or unknown: raw binary -> convert to hex text file */
                const char *hex_path = mem_init_convert_bin_to_hex(
                    m->init.file_path, m->word_width, m->depth);
                if (hex_path) {
                    fprintf(out, "    initial begin\n");
                    fprintf(out, "        $readmemh(\"%s\", %s);\n",
                            hex_path, name);
                    fprintf(out, "    end\n");
                } else {
                    fprintf(stderr, "error: failed to load binary file "
                            "\"%s\" for memory %s in module %s\n",
                            m->init.file_path, name, mod->name ? mod->name : "?");
                    errors++;
                }
            }
            continue;
        }

        if (m->depth <= 0) {
            continue;
        }

        fprintf(out, "    initial begin\n");
        fprintf(out, "        for (jz_mem_init_i_%d = 0; jz_mem_init_i_%d < %d; jz_mem_init_i_%d = jz_mem_init_i_%d + 1) begin\n",
            i, i, m->depth, i, i);
        fprintf(out, "            %s[jz_mem_init_i_%d] = ", name, i);
        emit_literal(out, &m->init.literal);
        fprintf(out, ";\n");
        fprintf(out, "        end\n");
        fprintf(out, "    end\n");
    }

    fputc('\n', out);
    return errors ? -1 : 0;
}
