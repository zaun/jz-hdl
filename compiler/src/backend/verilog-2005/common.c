/*
 * common.c - Shared utilities for the Verilog-2005 backend.
 *
 * This file contains helper functions and globals used across the backend.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "verilog_internal.h"
#include "ir.h"
#include "diagnostic.h"

/* -------------------------------------------------------------------------
 * Global state
 * -------------------------------------------------------------------------
 */

/* When non-zero, emit_assignment_stmt will use non-blocking '<=' rather than
 * blocking '=' for assignments (used by synchronous lowering).
 */
int g_in_sequential = 0;

/* -------------------------------------------------------------------------
 * Utility functions
 * -------------------------------------------------------------------------
 */

/* Report a backend I/O error via the diagnostic system. */
void backend_io_error(JZDiagnosticList *diagnostics,
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
    jz_diagnostic_report(diagnostics,
                         loc,
                         JZ_SEVERITY_ERROR,
                         "BACKEND_IO",
                         message);
}

/* -------------------------------------------------------------------------
 * Verilog keyword escaping
 * -------------------------------------------------------------------------
 */

/* Verilog-2005 reserved keywords that might collide with JZ-HDL identifiers.
 * Sorted for binary search.
 */
static const char *verilog_keywords[] = {
    "always", "and", "assign", "automatic", "begin", "buf", "bufif0",
    "bufif1", "case", "casex", "casez", "cell", "cmos", "config",
    "deassign", "default", "defparam", "design", "disable", "edge",
    "else", "end", "endcase", "endconfig", "endfunction", "endgenerate",
    "endmodule", "endprimitive", "endspecify", "endtable", "endtask",
    "event", "for", "force", "forever", "fork", "function", "generate",
    "genvar", "highz0", "highz1", "if", "ifnone", "incdir", "include",
    "initial", "inout", "input", "instance", "integer", "join",
    "large", "liblist", "library", "localparam", "macromodule",
    "medium", "module", "nand", "negedge", "nmos", "nor",
    "noshowcancelled", "not", "notif0", "notif1", "or", "output",
    "parameter", "pmos", "posedge", "primitive", "pull0", "pull1",
    "pulldown", "pullup", "pulsestyle_ondetect", "pulsestyle_onevent",
    "rcmos", "real", "realtime", "reg", "release", "repeat", "rnmos",
    "rpmos", "rtran", "rtranif0", "rtranif1", "scalared",
    "showcancelled", "signed", "small", "specify", "specparam",
    "strong0", "strong1", "supply0", "supply1", "table", "task",
    "time", "tran", "tranif0", "tranif1", "tri", "tri0", "tri1",
    "triand", "trior", "trireg", "unsigned", "use", "uwire",
    "vectored", "wait", "wand", "weak0", "weak1", "while", "wire",
    "wor", "xnor", "xor",
};

static int kw_compare(const void *a, const void *b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}

int verilog_is_keyword(const char *name)
{
    if (!name) return 0;
    size_t count = sizeof(verilog_keywords) / sizeof(verilog_keywords[0]);
    return bsearch(&name, verilog_keywords, count, sizeof(const char *),
                   kw_compare) != NULL;
}

const char *verilog_safe_name(const char *name, char *buf, int buf_size)
{
    if (!name) return name;
    if (!verilog_is_keyword(name)) return name;
    snprintf(buf, buf_size, "\\%s ", name);
    return buf;
}

/* Return a safe Verilog memory array name.  If the raw memory name collides
 * with the enclosing module name (which triggers a yosys crash), append
 * "_mem" to disambiguate.  The result is written into `buf` only when
 * renaming is needed; otherwise the original pointer is returned.
 */
const char *verilog_memory_name(const char *mem_name,
                                const char *module_name,
                                char *buf, int buf_size)
{
    if (!mem_name || !module_name) return mem_name;
    /* Check for Verilog keyword collision (e.g. "buf" is a primitive). */
    if (verilog_is_keyword(mem_name)) {
        snprintf(buf, buf_size, "%s_mem", mem_name);
        return buf;
    }
    /* Check for module name collision (triggers yosys crash). */
    if (strcmp(mem_name, module_name) == 0) {
        snprintf(buf, buf_size, "%s_mem", mem_name);
        return buf;
    }
    return mem_name;
}

/* Emit a Verilog range for a vector width, e.g., [WIDTH-1:0]. */
void emit_width_range(FILE *out, int width)
{
    if (width > 1) {
        fprintf(out, " [%d:0]", width - 1);
    }
}

/* Simple indentation helper for statement emission. */
void emit_indent(FILE *out, int level)
{
    for (int i = 0; i < level; ++i) {
        fputs("    ", out);
    }
}

/* -------------------------------------------------------------------------
 * Signal/clock lookup helpers
 * -------------------------------------------------------------------------
 */

/* Find a clock domain on a module by its IR id. Returns NULL if not found. */
const IR_ClockDomain *find_clock_domain_by_id(const IR_Module *mod, int clock_id)
{
    if (!mod) {
        return NULL;
    }
    for (int i = 0; i < mod->num_clock_domains; ++i) {
        const IR_ClockDomain *cd = &mod->clock_domains[i];
        if (cd->id == clock_id) {
            return cd;
        }
    }
    return NULL;
}

/* Find the index of a signal in a module by its IR id (without aliasing). */
int find_signal_index_by_id_raw(const IR_Module *mod, int signal_id)
{
    if (!mod) {
        return -1;
    }
    for (int i = 0; i < mod->num_signals; ++i) {
        if (mod->signals[i].id == signal_id) {
            return i;
        }
    }
    return -1;
}

/* Lookup a signal in a module by its IR id, remapping through the current
 * alias context so that all references to aliased signals resolve to a
 * canonical Verilog identifier.
 *
 * Returns NULL if no matching signal can be found.
 */
const IR_Signal *find_signal_by_id(const IR_Module *mod, int signal_id)
{
    if (!mod) {
        return NULL;
    }

    int found_index = -1;
    for (int i = 0; i < mod->num_signals; ++i) {
        if (mod->signals[i].id == signal_id) {
            found_index = i;
            break;
        }
    }
    if (found_index < 0) {
        return NULL;
    }

    int canon_index = alias_ctx_get_canonical_index(mod, found_index);
    if (canon_index < 0 || canon_index >= mod->num_signals) {
        canon_index = found_index;
    }
    return &mod->signals[canon_index];
}

/* Lookup a signal without applying alias canonicalization. This is used for
 * backend decisions that depend on the original IR_SignalKind (e.g., latches)
 * while still allowing aliased names to be emitted canonically elsewhere.
 */
const IR_Signal *find_signal_by_id_raw(const IR_Module *mod, int signal_id)
{
    if (!mod) {
        return NULL;
    }
    for (int i = 0; i < mod->num_signals; ++i) {
        if (mod->signals[i].id == signal_id) {
            return &mod->signals[i];
        }
    }
    return NULL;
}

/* Find a signal in a module by its name. This is used for CDC dest_alias_name
 * lookups, which are stored as names rather than signal ids in IR_CDC.
 */
const IR_Signal *find_signal_by_name(const IR_Module *mod, const char *name)
{
    if (!mod || !name || name[0] == '\0') {
        return NULL;
    }
    for (int i = 0; i < mod->num_signals; ++i) {
        const IR_Signal *sig = &mod->signals[i];
        if (sig->name && strcmp(sig->name, name) == 0) {
            return sig;
        }
    }
    return NULL;
}
