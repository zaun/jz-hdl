/*
 * verilog_main.c - Main entry point for the Verilog-2005 backend.
 *
 * This file contains the main jz_emit_verilog function and module ordering
 * logic.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "verilog_backend.h"
#include "verilog_internal.h"
#include "version.h"
#include "ir.h"

/* -------------------------------------------------------------------------
 * Module emission ordering
 * -------------------------------------------------------------------------
 */

void emit_module_order(FILE *out, const IR_Design *design)
{
    if (!design || !out) {
        return;
    }

    const int num_modules = design->num_modules;
    if (num_modules <= 0) {
        return;
    }

    int top_index = -1;
    if (design->project) {
        top_index = design->project->top_module_id;
        if (top_index < 0 || top_index >= num_modules) {
            top_index = -1;
        }
    }

    /* Emit each non-top reachable module in deterministic id order. */
    for (int i = 0; i < num_modules; ++i) {
        if (i == top_index) {
            continue;
        }
        const IR_Module *mod = &design->modules[i];
        if (mod->eliminated) {
            continue;
        }

        int *canon = NULL;
        int *is_repr = NULL;
        prepare_alias_context_for_module(mod, &canon, &is_repr);
        alias_ctx_set(mod, canon, is_repr, mod->num_signals);

        emit_module_header(out, mod);
        emit_port_declarations(out, mod);
        emit_internal_signal_declarations(out, mod);
        emit_memory_declarations(out, mod);
        if (emit_memory_initialization(out, mod) < 0) {
            alias_ctx_clear();
            free(canon);
            free(is_repr);
            return;
        }
        emit_continuous_alias_assignments(out, mod);
        emit_wire_tieoff_assignments(out, mod);
        if (mod->num_instances > 0) {
            fputc('\n', out);
            emit_instances(out, design, mod);
        }
        if (mod->async_block) {
            fputc('\n', out);
            emit_async_block(out, mod);
        }
        if (mod->num_clock_domains > 0) {
            fputc('\n', out);
            emit_clock_domains(out, mod);
        }
        fprintf(out, "endmodule\n\n");

        alias_ctx_clear();
        free(canon);
        free(is_repr);
    }

    /* Emit the top module last, if one was identified. */
    if (top_index >= 0) {
        const IR_Module *top = &design->modules[top_index];

        int *canon = NULL;
        int *is_repr = NULL;
        prepare_alias_context_for_module(top, &canon, &is_repr);
        alias_ctx_set(top, canon, is_repr, top->num_signals);

        emit_module_header(out, top);
        emit_port_declarations(out, top);
        emit_internal_signal_declarations(out, top);
        emit_memory_declarations(out, top);
        if (emit_memory_initialization(out, top) < 0) {
            alias_ctx_clear();
            free(canon);
            free(is_repr);
            return;
        }
        emit_continuous_alias_assignments(out, top);
        if (top->num_instances > 0) {
            fputc('\n', out);
            emit_instances(out, design, top);
        }
        if (top->async_block) {
            fputc('\n', out);
            emit_async_block(out, top);
        }
        if (top->num_clock_domains > 0) {
            fputc('\n', out);
            emit_clock_domains(out, top);
        }
        fprintf(out, "endmodule\n");

        alias_ctx_clear();
        free(canon);
        free(is_repr);
    }
}

/* -------------------------------------------------------------------------
 * Main Verilog emission entry point
 * -------------------------------------------------------------------------
 */

int jz_emit_verilog(const IR_Design *design,
                    const char *filename,
                    JZDiagnosticList *diagnostics,
                    const char *input_filename)
{
    if (!design) {
        backend_io_error(diagnostics, input_filename,
                         "invalid arguments to Verilog backend");
        return -1;
    }

    const char *target = (filename && filename[0] != '\0') ? filename : "-";

    FILE *out = NULL;
    int close_out = 0;
    char tmp_path[1024];
    tmp_path[0] = '\0';

    if (strcmp(target, "-") == 0) {
        out = stdout;
        close_out = 0;
    } else {
        int n = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", target);
        if (n <= 0 || (size_t)n >= sizeof(tmp_path)) {
            backend_io_error(diagnostics, input_filename,
                             "failed to construct temporary Verilog output filename");
            return -1;
        }

        out = fopen(tmp_path, "w");
        if (!out) {
            backend_io_error(diagnostics, input_filename,
                             "failed to open temporary Verilog output file for writing");
            return -1;
        }
        close_out = 1;
    }

    /* File header comment. */
    fprintf(out, "// This Verilog was transpiled from JZ-HDL.\n");
    fprintf(out, "// jz-hdl version: %s\n", JZ_HDL_VERSION_STRING);
    fprintf(out, "// Intended for use with yosys.\n\n");
    fprintf(out, "`default_nettype none\n\n");

    /* Emit modules in order. CDC library modules are now regular IR_Module
     * entries and will be emitted by emit_module_order like any other module.
     */
    emit_module_order(out, design);

    /* Emit project-level wrapper module. */
    if (design->project) {
        emit_project_wrapper(out, design, diagnostics);
    }

    if (close_out) {
        if (fflush(out) != 0 || ferror(out)) {
            fclose(out);
            if (tmp_path[0] != '\0') {
                (void)remove(tmp_path);
            }
            backend_io_error(diagnostics, input_filename,
                             "failed to write complete Verilog output");
            return -1;
        }
        if (fclose(out) != 0) {
            if (tmp_path[0] != '\0') {
                (void)remove(tmp_path);
            }
            backend_io_error(diagnostics, input_filename,
                             "failed to close Verilog output stream");
            return -1;
        }
        if (tmp_path[0] != '\0') {
            if (rename(tmp_path, target) != 0) {
                (void)remove(tmp_path);
                backend_io_error(diagnostics, input_filename,
                                 "failed to move temporary Verilog file into place");
                return -1;
            }
        }
    }

    return 0;
}
