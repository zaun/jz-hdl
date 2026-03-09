/*
 * common.c - Shared utilities for the RTLIL backend.
 *
 * This file contains helper functions, auto-ID counter, and signal lookup
 * functions used across the RTLIL backend.
 */
#include <stdio.h>
#include <string.h>

#include "rtlil_internal.h"
#include "ir.h"
#include "diagnostic.h"

/* We reuse the Verilog backend's alias context for signal resolution. */
#include "backend/verilog-2005/verilog_internal.h"

/* -------------------------------------------------------------------------
 * Auto-ID counter
 * -------------------------------------------------------------------------
 */

static int s_auto_id = 0;

int rtlil_next_id(void)
{
    return ++s_auto_id;
}

void rtlil_reset_id(void)
{
    s_auto_id = 0;
}

int rtlil_current_id(void)
{
    return s_auto_id;
}

/* -------------------------------------------------------------------------
 * Utility functions
 * -------------------------------------------------------------------------
 */

void rtlil_backend_io_error(JZDiagnosticList *diagnostics,
                            const char *input_filename,
                            const char *message)
{
    if (!diagnostics || !input_filename) {
        return;
    }
    JZLocation loc;
    loc.filename = input_filename;
    loc.line = 1;
    loc.column = 1;
    jz_diagnostic_report(diagnostics, loc, JZ_SEVERITY_ERROR,
                         "BACKEND_IO", message);
}

void rtlil_indent(FILE *out, int level)
{
    for (int i = 0; i < level; ++i) {
        fputs("  ", out);
    }
}

void rtlil_emit_const(FILE *out, const IR_Literal *lit)
{
    if (!lit) {
        fputs("1'0", out);
        return;
    }
    if (lit->width <= 0) {
        fprintf(out, "32'%032llu",
                (unsigned long long)(lit->words[0] & 0xFFFFFFFFULL));
        return;
    }

    int width = lit->width;
    fprintf(out, "%d'", width);

    if (lit->is_z) {
        for (int i = 0; i < width; ++i) {
            fputc('z', out);
        }
        return;
    }

    for (int i = width - 1; i >= 0; --i) {
        int wi = i / 64;
        int bi = i % 64;
        unsigned bit = (wi < IR_LIT_WORDS) ? (unsigned)((lit->words[wi] >> bi) & 1u) : 0;
        fputc(bit ? '1' : '0', out);
    }
}

void rtlil_emit_const_val(FILE *out, int width, uint64_t value)
{
    if (width <= 0) width = 1;
    fprintf(out, "%d'", width);
    for (int i = width - 1; i >= 0; --i) {
        unsigned bit = (unsigned)((value >> i) & 1u);
        fputc(bit ? '1' : '0', out);
    }
}

void rtlil_emit_zero(FILE *out, int width)
{
    rtlil_emit_const_val(out, width, 0);
}

void rtlil_emit_sigspec_signal(FILE *out, const IR_Module *mod, int signal_id)
{
    const IR_Signal *sig = rtlil_find_signal_by_id(mod, signal_id);
    if (sig && sig->name) {
        fprintf(out, "\\%s", sig->name);
    } else {
        fputs("1'0", out);
    }
}

void rtlil_emit_sigspec_bit(FILE *out, const IR_Module *mod, int signal_id, int bit)
{
    const IR_Signal *sig = rtlil_find_signal_by_id(mod, signal_id);
    if (sig && sig->name) {
        fprintf(out, "\\%s [%d]", sig->name, bit);
    } else {
        fputs("1'0", out);
    }
}

void rtlil_emit_sigspec_range(FILE *out, const IR_Module *mod, int signal_id,
                              int offset, int width)
{
    const IR_Signal *sig = rtlil_find_signal_by_id(mod, signal_id);
    if (sig && sig->name) {
        if (width == 1) {
            fprintf(out, "\\%s [%d]", sig->name, offset);
        } else {
            fprintf(out, "\\%s [%d:%d]", sig->name, offset + width - 1, offset);
        }
    } else {
        rtlil_emit_zero(out, width);
    }
}

/* -------------------------------------------------------------------------
 * Signal/clock lookup helpers
 *
 * These delegate to the Verilog backend's common.c functions which handle
 * alias context resolution.
 * -------------------------------------------------------------------------
 */

const IR_Signal *rtlil_find_signal_by_id(const IR_Module *mod, int signal_id)
{
    return find_signal_by_id(mod, signal_id);
}

const IR_Signal *rtlil_find_signal_by_id_raw(const IR_Module *mod, int signal_id)
{
    return find_signal_by_id_raw(mod, signal_id);
}

const IR_ClockDomain *rtlil_find_clock_domain_by_id(const IR_Module *mod, int clock_id)
{
    return find_clock_domain_by_id(mod, clock_id);
}
