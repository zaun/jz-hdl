/*
 * emit_instances.c - Module instance emission for the RTLIL backend.
 *
 * User module instances are emitted as RTLIL cells with the module name
 * as the cell type. Port connections use `connect` statements within
 * the cell body.
 *
 * RTLIL format:
 *   cell \ChildModule \instance_name
 *     connect \port_name \parent_signal
 *   end
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "rtlil_internal.h"
#include "ir.h"

/* Reuse alias helpers from Verilog backend. */
#include "backend/verilog-2005/verilog_internal.h"

/* -------------------------------------------------------------------------
 * Parse a single Verilog literal token (e.g., "8'h0F", "2'b10") and emit
 * as RTLIL binary constant. Returns 1 on success, 0 on failure.
 * -------------------------------------------------------------------------
 */
static int emit_single_literal(FILE *out, const char *tok, int tok_len)
{
    char buf[256];
    if (tok_len <= 0 || tok_len >= (int)sizeof(buf)) return 0;
    memcpy(buf, tok, (size_t)tok_len);
    buf[tok_len] = '\0';

    char *tick = strchr(buf, '\'');
    if (!tick) return 0;

    int width = (int)strtol(buf, NULL, 10);
    if (width <= 0 || width > 64) return 0;

    char fmt = tick[1];
    const char *digits = tick + 2;
    uint64_t value = 0;

    if (fmt == 'h' || fmt == 'H') {
        value = (uint64_t)strtoull(digits, NULL, 16);
    } else if (fmt == 'b' || fmt == 'B') {
        value = (uint64_t)strtoull(digits, NULL, 2);
    } else if (fmt == 'd' || fmt == 'D') {
        value = (uint64_t)strtoull(digits, NULL, 10);
    } else if (fmt >= '0' && fmt <= '9') {
        value = (uint64_t)strtoull(tick + 1, NULL, 10);
    } else {
        return 0;
    }

    rtlil_emit_const_val(out, width, value);
    return 1;
}

/* -------------------------------------------------------------------------
 * Emit a const_expr string as RTLIL sigspec. Handles:
 * - Simple literals: "8'h0F"
 * - Concatenations: "{8'h3F, 8'h2F, 8'h1F, 8'h0F}"
 * - Signal references within concatenations: "{sel_a, 2'b01}"
 * Falls back to zero if unparseable.
 * -------------------------------------------------------------------------
 */
static void rtlil_emit_const_expr(FILE *out, const IR_Module *mod,
                                    const char *ce, int fallback_width)
{
    if (!ce || !ce[0]) {
        rtlil_emit_zero(out, fallback_width);
        return;
    }

    /* Skip leading whitespace. */
    while (*ce == ' ') ce++;

    /* Concatenation: {elem, elem, ...} */
    if (ce[0] == '{') {
        const char *end = strrchr(ce, '}');
        if (!end) {
            rtlil_emit_zero(out, fallback_width);
            return;
        }

        /* Collect elements between { and }. */
        const char *p = ce + 1;
        fprintf(out, "{ ");
        int first = 1;

        while (p < end) {
            /* Skip whitespace and commas. */
            while (p < end && (*p == ' ' || *p == ',')) p++;
            if (p >= end) break;

            /* Extract token until comma or end. */
            const char *tok_start = p;
            while (p < end && *p != ',') p++;
            /* Trim trailing whitespace. */
            const char *tok_end = p;
            while (tok_end > tok_start && tok_end[-1] == ' ') tok_end--;
            int tok_len = (int)(tok_end - tok_start);
            if (tok_len <= 0) continue;

            if (!first) fprintf(out, " ");
            first = 0;

            /* Try as literal first. */
            if (!emit_single_literal(out, tok_start, tok_len)) {
                /* Try as signal reference (resolve through alias union-find). */
                char name[256];
                if (tok_len >= (int)sizeof(name)) tok_len = (int)sizeof(name) - 1;
                memcpy(name, tok_start, (size_t)tok_len);
                name[tok_len] = '\0';

                const IR_Signal *raw = find_signal_by_name(mod, name);
                const char *emit_name = NULL;
                if (raw) {
                    const IR_Signal *canon = find_signal_by_id(mod, raw->id);
                    if (canon && canon->name)
                        emit_name = canon->name;
                }
                if (emit_name) {
                    fprintf(out, "\\%s", emit_name);
                } else {
                    /* Unknown token — emit as zero. */
                    rtlil_emit_zero(out, 1);
                }
            }
        }
        fprintf(out, " }");
        return;
    }

    /* Simple literal. */
    if (emit_single_literal(out, ce, (int)strlen(ce))) {
        return;
    }

    /* Try as a bare signal reference (resolve through alias union-find). */
    {
        const IR_Signal *raw = find_signal_by_name(mod, ce);
        if (raw) {
            const IR_Signal *canon = find_signal_by_id(mod, raw->id);
            if (canon && canon->name) {
                fprintf(out, "\\%s", canon->name);
                return;
            }
        }
    }

    /* Fallback: emit zero. */
    rtlil_emit_zero(out, fallback_width);
}

void rtlil_emit_instances(FILE *out, const IR_Design *design,
                           const IR_Module *mod)
{
    if (!out || !design || !mod || mod->num_instances <= 0) return;

    for (int i = 0; i < mod->num_instances; ++i) {
        const IR_Instance *inst = &mod->instances[i];
        if (!inst) continue;
        if (inst->child_module_id < 0 ||
            inst->child_module_id >= design->num_modules) {
            continue;
        }

        const IR_Module *child = &design->modules[inst->child_module_id];
        const char *child_name = (child && child->name && child->name[0] != '\0')
                               ? child->name : "jz_unnamed_child";
        const char *inst_name = (inst->name && inst->name[0] != '\0')
                              ? inst->name : "jz_inst";

        /* Pre-scan for no-connect output ports and declare dummy wires. */
        for (int c = 0; c < inst->num_connections; ++c) {
            const IR_InstanceConnection *conn = &inst->connections[c];
            const IR_Signal *cp = NULL;
            if (child) {
                for (int s = 0; s < child->num_signals; ++s) {
                    if (child->signals[s].id == conn->child_port_id) {
                        cp = &child->signals[s];
                        break;
                    }
                }
            }
            if (!cp || !cp->name) continue;
            const IR_Signal *ps = rtlil_find_signal_by_id(mod, conn->parent_signal_id);
            if (!ps && (!conn->const_expr || conn->const_expr[0] == '\0') &&
                cp->kind == SIG_PORT &&
                (cp->u.port.direction == PORT_OUT ||
                 cp->u.port.direction == PORT_INOUT)) {
                int w = cp->width > 0 ? cp->width : 1;
                rtlil_indent(out, 1);
                if (w > 1) {
                    fprintf(out, "wire width %d \\jz_nc_%s_%s\n", w,
                            inst_name, cp->name);
                } else {
                    fprintf(out, "wire \\jz_nc_%s_%s\n",
                            inst_name, cp->name);
                }
            }
        }

        rtlil_indent(out, 1);
        fprintf(out, "cell \\%s \\%s\n", child_name, inst_name);

        for (int c = 0; c < inst->num_connections; ++c) {
            const IR_InstanceConnection *conn = &inst->connections[c];

            /* Look up the child port signal. */
            const IR_Signal *child_port = NULL;
            if (child) {
                for (int s = 0; s < child->num_signals; ++s) {
                    if (child->signals[s].id == conn->child_port_id) {
                        child_port = &child->signals[s];
                        break;
                    }
                }
            }
            if (!child_port || !child_port->name) continue;

            rtlil_indent(out, 2);
            fprintf(out, "connect \\%s ", child_port->name);

            const IR_Signal *parent_sig = rtlil_find_signal_by_id(
                mod, conn->parent_signal_id);

            if (parent_sig && parent_sig->name) {
                if (conn->parent_msb >= 0 && conn->parent_lsb >= 0) {
                    int width = conn->parent_msb - conn->parent_lsb + 1;
                    if (width == 1) {
                        fprintf(out, "\\%s [%d]", parent_sig->name,
                                conn->parent_lsb);
                    } else {
                        fprintf(out, "\\%s [%d:%d]", parent_sig->name,
                                conn->parent_msb,
                                conn->parent_lsb);
                    }
                } else if (child_port && parent_sig->width > 0 &&
                           child_port->width > 0 &&
                           parent_sig->width > child_port->width) {
                    /* Array instance: parent signal is wider than child port.
                     * Parse the array index from instance name (e.g., "worker[3]")
                     * and slice the parent signal accordingly. */
                    int arr_idx = 0;
                    const char *bracket = strchr(inst_name, '[');
                    if (bracket) {
                        arr_idx = (int)strtol(bracket + 1, NULL, 10);
                    }
                    int cw = child_port->width;
                    int lsb = arr_idx * cw;
                    int msb = lsb + cw - 1;
                    if (cw == 1) {
                        fprintf(out, "\\%s [%d]", parent_sig->name, lsb);
                    } else {
                        fprintf(out, "\\%s [%d:%d]", parent_sig->name,
                                msb, lsb);
                    }
                } else {
                    fprintf(out, "\\%s", parent_sig->name);
                }
            } else if (conn->const_expr && conn->const_expr[0] != '\0') {
                /* Constant binding: parse and emit as RTLIL constant.
                 * The const_expr is in Verilog format (e.g., "8'h0F").
                 * Handles simple literals and concatenations. */
                rtlil_emit_const_expr(out, mod, conn->const_expr,
                                      child_port->width > 0 ? child_port->width : 1);
            } else {
                /* No binding. For output/inout ports, use the pre-declared
                 * dummy wire (yosys rejects output ports connected to
                 * constants). For input ports, emit zero. */
                int w = child_port->width > 0 ? child_port->width : 1;
                if (child_port->kind == SIG_PORT &&
                    (child_port->u.port.direction == PORT_OUT ||
                     child_port->u.port.direction == PORT_INOUT)) {
                    fprintf(out, "\\jz_nc_%s_%s", inst_name, child_port->name);
                } else {
                    rtlil_emit_zero(out, w);
                }
            }

            fputc('\n', out);
        }

        rtlil_indent(out, 1);
        fprintf(out, "end\n");
    }
}
