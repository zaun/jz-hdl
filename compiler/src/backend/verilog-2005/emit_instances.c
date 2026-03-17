/*
 * emit_instances.c - Module instance emission for the Verilog-2005 backend.
 *
 * This file handles emitting structural module instantiations.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "verilog_internal.h"
#include "ir.h"

/* -------------------------------------------------------------------------
 * Helper: canonicalize signal names inside a const_expr string through the
 * alias union-find. Signal names that are aliased (e.g., sel_a → en) get
 * replaced by their canonical representative so that eliminated wires are
 * never referenced.
 *
 * The buffer is written into `buf` (size `buf_size`). Returns buf.
 * -------------------------------------------------------------------------
 */
static const char *canonicalize_const_expr(const IR_Module *mod,
                                           const char *expr,
                                           char *buf, int buf_size)
{
    if (!expr || !buf || buf_size <= 0) return expr;

    int out = 0;
    const char *p = expr;
    while (*p && out < buf_size - 1) {
        /* Copy non-identifier characters directly. */
        if (!(isalpha((unsigned char)*p) || *p == '_')) {
            buf[out++] = *p++;
            continue;
        }

        /* Extract an identifier token. */
        const char *start = p;
        while (*p && (isalnum((unsigned char)*p) || *p == '_')) p++;
        int len = (int)(p - start);

        /* Check if this looks like a sized literal prefix (digits followed
         * by 'b', 'h', 'd', 'o') — these are not signal names. Sized
         * literals start with digits (e.g., 2'b10), so an identifier
         * starting with a letter is always a signal name candidate. */
        char name[256];
        if (len >= (int)sizeof(name)) len = (int)sizeof(name) - 1;
        memcpy(name, start, (size_t)len);
        name[len] = '\0';

        /* Look up as a signal in the module. */
        const IR_Signal *raw = find_signal_by_name(mod, name);
        const char *emit_name = name;
        if (raw) {
            const IR_Signal *canon = find_signal_by_id(mod, raw->id);
            if (canon && canon->name)
                emit_name = canon->name;
        }

        int nlen = (int)strlen(emit_name);
        if (out + nlen < buf_size) {
            memcpy(buf + out, emit_name, (size_t)nlen);
            out += nlen;
        }
    }
    buf[out] = '\0';
    return buf;
}

/* -------------------------------------------------------------------------
 * Design query helpers
 * -------------------------------------------------------------------------
 */

int design_has_module_named(const IR_Design *design, const char *name)
{
    if (!design || !name) return 0;
    for (int i = 0; i < design->num_modules; ++i) {
        const IR_Module *mod = &design->modules[i];
        if (mod->name && strcmp(mod->name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

/* -------------------------------------------------------------------------
 * Instance emission
 * -------------------------------------------------------------------------
 */

void emit_instances(FILE *out, const IR_Design *design, const IR_Module *mod)
{
    if (!out || !design || !mod || mod->num_instances <= 0) {
        return;
    }

    for (int i = 0; i < mod->num_instances; ++i) {
        const IR_Instance *inst = &mod->instances[i];
        if (!inst) {
            continue;
        }
        if (inst->child_module_id < 0 || inst->child_module_id >= design->num_modules) {
            continue;
        }
        const IR_Module *child = &design->modules[inst->child_module_id];
        const char *child_name = (child && child->name && child->name[0] != '\0')
                               ? child->name
                               : "jz_unnamed_child";
        const char *inst_name = (inst->name && inst->name[0] != '\0')
                              ? inst->name
                              : "jz_inst";

        if (inst->num_params > 0 && inst->params) {
            fprintf(out, "    %s #(\n", child_name);
            for (int p = 0; p < inst->num_params; ++p) {
                const IR_InstanceParam *param = &inst->params[p];
                if (param->string_value) {
                    fprintf(out, "        .%s(\"%s\")", param->name, param->string_value);
                } else {
                    fprintf(out, "        .%s(%lld)", param->name, param->value);
                }
                fprintf(out, "%s\n", (p + 1 < inst->num_params) ? "," : "");
            }
            fprintf(out, "    ) %s (\n", inst_name);
        } else {
            fprintf(out, "    %s %s (\n", child_name, inst_name);
        }

        for (int c = 0; c < inst->num_connections; ++c) {
            const IR_InstanceConnection *conn = &inst->connections[c];
            const IR_Signal *child_port = find_signal_by_id(child, conn->child_port_id);
            const IR_Signal *parent_sig = find_signal_by_id(mod, conn->parent_signal_id);
            if (!child_port || !child_port->name) {
                continue;
            }

            {
                char esc_cp[256];
                fprintf(out, "        .%s(", verilog_safe_name(child_port->name, esc_cp, (int)sizeof(esc_cp)));
            }
            if (parent_sig && parent_sig->name) {
                char esc_ps[256];
                fprintf(out, "%s", verilog_safe_name(parent_sig->name, esc_ps, (int)sizeof(esc_ps)));
                if (conn->parent_msb >= 0 && conn->parent_lsb >= 0) {
                    if (conn->parent_msb == conn->parent_lsb) {
                        fprintf(out, "[%d]", conn->parent_msb);
                    } else {
                        fprintf(out, "[%d:%d]", conn->parent_msb, conn->parent_lsb);
                    }
                } else if (child_port && parent_sig->width > 0 &&
                           child_port->width > 0 &&
                           parent_sig->width > child_port->width) {
                    /* Array instance: parent signal wider than child port.
                     * Parse array index from instance name and slice. */
                    int arr_idx = 0;
                    const char *bracket = strchr(inst_name, '[');
                    if (bracket) {
                        arr_idx = (int)strtol(bracket + 1, NULL, 10);
                    }
                    int cw = child_port->width;
                    int lsb = arr_idx * cw;
                    int msb = lsb + cw - 1;
                    if (msb == lsb) {
                        fprintf(out, "[%d]", lsb);
                    } else {
                        fprintf(out, "[%d:%d]", msb, lsb);
                    }
                }
            } else if (conn->const_expr && conn->const_expr[0] != '\0') {
                char canon_buf[1024];
                const char *ce = canonicalize_const_expr(mod, conn->const_expr,
                                                          canon_buf, (int)sizeof(canon_buf));
                fprintf(out, "%s", ce);
            } else {
                fprintf(out, "1'b0");
            }
            fprintf(out, ")");

            if (c + 1 < inst->num_connections) {
                fprintf(out, ",\n");
            } else {
                fprintf(out, "\n");
            }
        }

        fprintf(out, "    );\n");
    }
}
