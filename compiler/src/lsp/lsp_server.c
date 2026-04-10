/**
 * @file lsp_server.c
 * @brief LSP server main loop and request handlers for JZ-HDL.
 *
 * Implements the Language Server Protocol over stdio, providing:
 *   - textDocument/publishDiagnostics (on open/change/save)
 *   - textDocument/hover (rule description lookup)
 *   - textDocument/completion (keyword completion)
 *   - textDocument/definition (go-to-definition for identifiers)
 */

#include "lsp/lsp_internal.h"
#include "lsp.h"

#include "compiler.h"
#include "lexer.h"
#include "parser.h"
#include "sem_driver.h"
#include "template_expand.h"
#include "repeat_expand.h"
#include "diagnostic.h"
#include "rules.h"
#include "ast.h"
#include "path_security.h"
#include "ir_builder.h"
#include "ir.h"

#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Forward declarations                                              */
/* ------------------------------------------------------------------ */

/* Workspace root path extracted from initialize request. */
static char s_workspace_root[2048] = {0};

/* Hover feature toggles (from initializationOptions). */
static int s_hover_clocks = 1;
static int s_hover_declarations = 1;

/* ------------------------------------------------------------------ */
/*  Project override map                                              */
/*                                                                    */
/*  When the user explicitly selects a project via the status bar     */
/*  picker, we store the mapping: source file → project file.        */
/*  This takes priority over automatic discovery.                     */
/* ------------------------------------------------------------------ */

#define LSP_MAX_OVERRIDES 128

typedef struct {
    char file_path[2048];     /* Canonical path of the source file. */
    char project_path[2048];  /* Canonical path of the selected project. */
} LspProjectOverride;

static LspProjectOverride s_project_overrides[LSP_MAX_OVERRIDES];
static size_t s_project_override_count = 0;

static const char *lookup_project_override(const char *filepath) {
    for (size_t i = 0; i < s_project_override_count; i++) {
        if (strcmp(s_project_overrides[i].file_path, filepath) == 0) {
            return s_project_overrides[i].project_path;
        }
    }
    return NULL;
}

static void set_project_override(const char *filepath, const char *project_path) {
    /* Update existing entry. */
    for (size_t i = 0; i < s_project_override_count; i++) {
        if (strcmp(s_project_overrides[i].file_path, filepath) == 0) {
            strncpy(s_project_overrides[i].project_path, project_path,
                    sizeof(s_project_overrides[i].project_path) - 1);
            s_project_overrides[i].project_path[
                sizeof(s_project_overrides[i].project_path) - 1] = '\0';
            return;
        }
    }
    /* Add new entry. */
    if (s_project_override_count < LSP_MAX_OVERRIDES) {
        LspProjectOverride *o = &s_project_overrides[s_project_override_count++];
        strncpy(o->file_path, filepath, sizeof(o->file_path) - 1);
        o->file_path[sizeof(o->file_path) - 1] = '\0';
        strncpy(o->project_path, project_path, sizeof(o->project_path) - 1);
        o->project_path[sizeof(o->project_path) - 1] = '\0';
    }
}

static void handle_select_project(const char *msg, LspDocStore *store);

/* ------------------------------------------------------------------ */
/*  Clock info cache                                                  */
/*                                                                    */
/*  Populated from the IR during project compilation so that hover    */
/*  can show clock properties.  Entries are indexed by both project   */
/*  clock names AND module port names that are wired to clocks.       */
/* ------------------------------------------------------------------ */

#define LSP_MAX_CLOCKS 128

typedef struct {
    char name[128];           /* Lookup key (project clock name or port alias). */
    char project_clock[128];  /* Canonical project-level clock name. */
    double period_ns;         /* 0 if generated (no explicit period). */
    char edge[16];            /* "Rising", "Falling", or empty. */
    int is_external;          /* Has period → external pin clock. */
    int is_generated;         /* Output of a CLOCK_GEN unit. */
    char gen_type[16];        /* "PLL", "CLKDIV", "DLL", "OSC", "BUF", or empty. */
    char gen_output[16];      /* "BASE", "PHASE", "DIV", "DIV3", or empty. */
    char gen_input_clock[128]; /* Name of the reference clock feeding the generator. */
} LspClockInfo;

static LspClockInfo s_clock_cache[LSP_MAX_CLOCKS];
static size_t s_clock_cache_count = 0;

static const LspClockInfo *lookup_clock_info(const char *name);

static LspClockInfo *add_clock_entry(const char *name) {
    /* Deduplicate by name. */
    for (size_t i = 0; i < s_clock_cache_count; ++i) {
        if (strcmp(s_clock_cache[i].name, name) == 0) {
            return &s_clock_cache[i];
        }
    }
    if (s_clock_cache_count >= LSP_MAX_CLOCKS) return NULL;
    LspClockInfo *info = &s_clock_cache[s_clock_cache_count++];
    memset(info, 0, sizeof(*info));
    strncpy(info->name, name, sizeof(info->name) - 1);
    return info;
}

/**
 * @brief Add a port alias that maps to an existing project clock entry.
 */
static void add_clock_alias(const char *alias_name, const LspClockInfo *src) {
    if (!alias_name || !src) return;
    if (strcmp(alias_name, src->name) == 0) return; /* already exists */
    LspClockInfo *entry = add_clock_entry(alias_name);
    if (!entry) return;
    /* Copy all fields from source, then override the name. */
    *entry = *src;
    strncpy(entry->name, alias_name, sizeof(entry->name) - 1);
}

/**
 * @brief Populate the clock cache from the IR.
 *
 * Uses IR_Clock, IR_ClockGen, IR_TopBinding, and IR_ClockDomain
 * to build a comprehensive clock lookup table indexed by both
 * project clock names and module port names.
 */
static void cache_clock_info_from_ir(const IR_Design *design) {
    s_clock_cache_count = 0;
    if (!design || !design->project) return;

    const IR_Project *proj = design->project;

    /* Step 1: Add project-level clocks. */
    for (int i = 0; i < proj->num_clocks; ++i) {
        const IR_Clock *clk = &proj->clocks[i];
        if (!clk->name) continue;

        LspClockInfo *info = add_clock_entry(clk->name);
        if (!info) continue;

        strncpy(info->project_clock, clk->name, sizeof(info->project_clock) - 1);
        info->period_ns = clk->period_ns;
        info->is_external = (clk->period_ns > 0.0 && !clk->is_generated);
        info->is_generated = clk->is_generated;

        switch (clk->edge) {
        case EDGE_RISING:  strncpy(info->edge, "Rising", sizeof(info->edge) - 1); break;
        case EDGE_FALLING: strncpy(info->edge, "Falling", sizeof(info->edge) - 1); break;
        case EDGE_BOTH:    strncpy(info->edge, "Both", sizeof(info->edge) - 1); break;
        default: break;
        }
    }

    /* Step 2: Enrich generated clocks with CLOCK_GEN details. */
    for (int i = 0; i < proj->num_clock_gens; ++i) {
        const IR_ClockGen *gen = &proj->clock_gens[i];
        for (int u = 0; u < gen->num_units; ++u) {
            const IR_ClockGenUnit *unit = &gen->units[u];

            /* Find the reference clock name. */
            const char *ref_clock = NULL;
            for (int inp = 0; inp < unit->num_inputs; ++inp) {
                if (unit->inputs[inp].signal_name) {
                    ref_clock = unit->inputs[inp].signal_name;
                    break;
                }
            }

            for (int out = 0; out < unit->num_outputs; ++out) {
                const char *clock_name = unit->outputs[out].clock_name;
                if (!clock_name) continue;

                /* Find the existing entry. */
                for (size_t ci = 0; ci < s_clock_cache_count; ++ci) {
                    if (strcmp(s_clock_cache[ci].name, clock_name) == 0) {
                        if (unit->type) {
                            strncpy(s_clock_cache[ci].gen_type, unit->type,
                                    sizeof(s_clock_cache[ci].gen_type) - 1);
                        }
                        if (unit->outputs[out].selector) {
                            strncpy(s_clock_cache[ci].gen_output,
                                    unit->outputs[out].selector,
                                    sizeof(s_clock_cache[ci].gen_output) - 1);
                        }
                        if (ref_clock) {
                            strncpy(s_clock_cache[ci].gen_input_clock, ref_clock,
                                    sizeof(s_clock_cache[ci].gen_input_clock) - 1);
                        }
                        break;
                    }
                }
            }
        }
    }

    /* Step 3: Add aliases from @top bindings (port signal → clock).
     * A top binding can reference a clock in two ways:
     *   a) clock_name is set (for clock-gen outputs like PLL clocks)
     *   b) pin_id is set and the pin name matches a project clock
     *      (for external pin clocks like SCLK) */
    {
        const IR_Module *top_mod = NULL;
        if (proj->top_module_id >= 0 && proj->top_module_id < design->num_modules) {
            top_mod = &design->modules[proj->top_module_id];
        }

        for (int i = 0; i < proj->num_top_bindings; ++i) {
            const IR_TopBinding *tb = &proj->top_bindings[i];

            /* Determine the clock name from this binding. */
            const char *bound_clock = tb->clock_name;

            /* If no explicit clock_name, check if the pin is also a clock. */
            if (!bound_clock && tb->pin_id >= 0 && tb->pin_id < proj->num_pins) {
                const char *pin_name = proj->pins[tb->pin_id].name;
                if (pin_name) {
                    for (size_t ci = 0; ci < s_clock_cache_count; ++ci) {
                        if (strcmp(s_clock_cache[ci].name, pin_name) == 0) {
                            bound_clock = pin_name;
                            break;
                        }
                    }
                }
            }

            if (!bound_clock) continue;

            /* Resolve top port signal ID to a name. */
            const char *port_name = NULL;
            if (top_mod && tb->top_port_signal_id >= 0) {
                for (int si = 0; si < top_mod->num_signals; ++si) {
                    if (top_mod->signals[si].id == tb->top_port_signal_id) {
                        port_name = top_mod->signals[si].name;
                        break;
                    }
                }
            }

            const LspClockInfo *src = NULL;
            for (size_t ci = 0; ci < s_clock_cache_count; ++ci) {
                if (strcmp(s_clock_cache[ci].name, bound_clock) == 0) {
                    src = &s_clock_cache[ci];
                    break;
                }
            }
            if (src && port_name) {
                add_clock_alias(port_name, src);
            }
        }
    }

    /* Step 4: Walk instance connections to propagate clock aliases down
     * the hierarchy.  For each module that instantiates another, check
     * if any connection wires a known clock signal to a child port.
     * This lets us resolve e.g. module port "clk" → parent "sclk" →
     * project "sys_clk". We iterate until no new aliases are added. */
    {
        int changed = 1;
        int max_iters = 8; /* prevent infinite loops on pathological cases */
        while (changed && max_iters-- > 0) {
            changed = 0;
            for (int mi = 0; mi < design->num_modules; ++mi) {
                const IR_Module *parent = &design->modules[mi];
                for (int ii = 0; ii < parent->num_instances; ++ii) {
                    const IR_Instance *inst = &parent->instances[ii];
                    if (inst->child_module_id < 0 ||
                        inst->child_module_id >= design->num_modules) continue;
                    const IR_Module *child = &design->modules[inst->child_module_id];

                    for (int ci = 0; ci < inst->num_connections; ++ci) {
                        const IR_InstanceConnection *conn = &inst->connections[ci];

                        /* Find the parent signal name. */
                        const char *parent_sig = NULL;
                        for (int si = 0; si < parent->num_signals; ++si) {
                            if (parent->signals[si].id == conn->parent_signal_id) {
                                parent_sig = parent->signals[si].name;
                                break;
                            }
                        }
                        if (!parent_sig) continue;

                        /* Is the parent signal a known clock? */
                        const LspClockInfo *parent_clk = lookup_clock_info(parent_sig);
                        if (!parent_clk) continue;

                        /* Find the child port name. */
                        const char *child_port = NULL;
                        for (int si = 0; si < child->num_signals; ++si) {
                            if (child->signals[si].id == conn->child_port_id) {
                                child_port = child->signals[si].name;
                                break;
                            }
                        }
                        if (!child_port) continue;

                        /* Add alias if not already known. */
                        if (!lookup_clock_info(child_port)) {
                            add_clock_alias(child_port, parent_clk);
                            changed = 1;
                        }
                    }
                }
            }
        }
    }

    lsp_log("clock cache: %zu entries", s_clock_cache_count);
}

/**
 * @brief Populate the clock cache from the AST (fallback when IR is not available).
 */
static void cache_clock_info_from_ast(JZASTNode *ast) {
    s_clock_cache_count = 0;
    if (!ast) return;

    JZASTNode *project = NULL;
    for (size_t i = 0; i < ast->child_count; ++i) {
        if (ast->children[i] && ast->children[i]->type == JZ_AST_PROJECT) {
            project = ast->children[i];
            break;
        }
    }
    if (!project) return;

    for (size_t i = 0; i < project->child_count; ++i) {
        JZASTNode *child = project->children[i];
        if (!child || child->type != JZ_AST_CLOCKS_BLOCK) continue;

        for (size_t ci = 0; ci < child->child_count; ++ci) {
            JZASTNode *decl = child->children[ci];
            if (!decl || decl->type != JZ_AST_CONST_DECL || !decl->name) continue;

            LspClockInfo *info = add_clock_entry(decl->name);
            if (!info) continue;

            strncpy(info->project_clock, decl->name, sizeof(info->project_clock) - 1);

            if (decl->text) {
                const char *p = strstr(decl->text, "period");
                if (p) {
                    p = strchr(p, '=');
                    if (p) {
                        p++;
                        while (*p == ' ' || *p == '\t') p++;
                        info->period_ns = strtod(p, NULL);
                    }
                }
                const char *e = strstr(decl->text, "edge");
                if (e) {
                    e = strchr(e, '=');
                    if (e) {
                        e++;
                        while (*e == ' ' || *e == '\t') e++;
                        size_t len = 0;
                        while (e[len] && e[len] != ',' && e[len] != ';' &&
                               e[len] != '}' && e[len] != ' ' && e[len] != '\t')
                            len++;
                        if (len >= sizeof(info->edge)) len = sizeof(info->edge) - 1;
                        memcpy(info->edge, e, len);
                        info->edge[len] = '\0';
                    }
                }
            }

            info->is_external = (info->period_ns > 0.0);
        }
    }
}

/**
 * @brief Look up a clock by name in the cache.
 */
static const LspClockInfo *lookup_clock_info(const char *name) {
    for (size_t i = 0; i < s_clock_cache_count; ++i) {
        if (strcmp(s_clock_cache[i].name, name) == 0) {
            return &s_clock_cache[i];
        }
    }
    return NULL;
}

static JZASTNode *find_declaration(JZASTNode *node, const char *name);
static void handle_initialize(const char *msg, int id, LspDocStore *store);
static void handle_initialized(void);
static void handle_shutdown(int id);
static void handle_text_document_did_open(const char *msg, LspDocStore *store);
static void handle_text_document_did_change(const char *msg, LspDocStore *store);
static void handle_text_document_did_close(const char *msg, LspDocStore *store);
static void handle_text_document_did_save(const char *msg, LspDocStore *store);
static void handle_text_document_hover(const char *msg, int id, LspDocStore *store);
static void handle_text_document_completion(const char *msg, int id, LspDocStore *store);
static void handle_text_document_definition(const char *msg, int id, LspDocStore *store);
static void publish_diagnostics(const char *uri, LspDocStore *store);
static void send_response(int id, const char *result_json);
static void send_error(int id, int code, const char *message);
static void send_notification(const char *method, const char *params_json);

/* ------------------------------------------------------------------ */
/*  Main loop                                                         */
/* ------------------------------------------------------------------ */

int jz_lsp_run(void) {
    lsp_log("starting LSP server");

    LspDocStore store;
    lsp_docstore_init(&store);

    int shutdown_requested = 0;
    int exit_code = 0;

    for (;;) {
        size_t msg_len = 0;
        char *msg = lsp_io_read_message(&msg_len);
        if (!msg) {
            /* EOF — client disconnected. */
            exit_code = shutdown_requested ? 0 : 1;
            break;
        }

        /* Extract method and id. */
        char method[128] = {0};
        int id = -1;
        lsp_json_get_string(msg, "method", method, sizeof(method));
        lsp_json_get_int(msg, "id", &id);

        if (strcmp(method, "initialize") == 0) {
            handle_initialize(msg, id, &store);
        } else if (strcmp(method, "initialized") == 0) {
            handle_initialized();
        } else if (strcmp(method, "shutdown") == 0) {
            handle_shutdown(id);
            shutdown_requested = 1;
        } else if (strcmp(method, "exit") == 0) {
            free(msg);
            exit_code = shutdown_requested ? 0 : 1;
            break;
        } else if (strcmp(method, "textDocument/didOpen") == 0) {
            handle_text_document_did_open(msg, &store);
        } else if (strcmp(method, "textDocument/didChange") == 0) {
            handle_text_document_did_change(msg, &store);
        } else if (strcmp(method, "textDocument/didClose") == 0) {
            handle_text_document_did_close(msg, &store);
        } else if (strcmp(method, "textDocument/didSave") == 0) {
            handle_text_document_did_save(msg, &store);
        } else if (strcmp(method, "textDocument/hover") == 0) {
            handle_text_document_hover(msg, id, &store);
        } else if (strcmp(method, "textDocument/completion") == 0) {
            handle_text_document_completion(msg, id, &store);
        } else if (strcmp(method, "textDocument/definition") == 0) {
            handle_text_document_definition(msg, id, &store);
        } else if (strcmp(method, "jz-hdl/selectProject") == 0) {
            handle_select_project(msg, &store);
        } else if (id >= 0) {
            /* Unknown request — respond with MethodNotFound. */
            send_error(id, -32601, "Method not found");
        }
        /* Unknown notifications are silently ignored per spec. */

        free(msg);
    }

    lsp_docstore_free(&store);
    lsp_log("LSP server exiting with code %d", exit_code);
    return exit_code;
}

/* ------------------------------------------------------------------ */
/*  Response helpers                                                  */
/* ------------------------------------------------------------------ */

static void send_response(int id, const char *result_json) {
    LspJson j;
    lsp_json_init(&j);
    lsp_json_append(&j, "{\"jsonrpc\":\"2.0\",\"id\":");
    lsp_json_append_int(&j, id);
    lsp_json_append(&j, ",\"result\":");
    lsp_json_append(&j, result_json);
    lsp_json_append_char(&j, '}');
    lsp_io_write_message(j.data, j.len);
    lsp_json_free(&j);
}

static void send_error(int id, int code, const char *message) {
    LspJson j;
    lsp_json_init(&j);
    lsp_json_append(&j, "{\"jsonrpc\":\"2.0\",\"id\":");
    lsp_json_append_int(&j, id);
    lsp_json_append(&j, ",\"error\":{\"code\":");
    lsp_json_append_int(&j, code);
    lsp_json_append(&j, ",\"message\":");
    lsp_json_append_escaped(&j, message);
    lsp_json_append(&j, "}}");
    lsp_io_write_message(j.data, j.len);
    lsp_json_free(&j);
}

static void send_notification(const char *method, const char *params_json) {
    LspJson j;
    lsp_json_init(&j);
    lsp_json_append(&j, "{\"jsonrpc\":\"2.0\",\"method\":");
    lsp_json_append_escaped(&j, method);
    lsp_json_append(&j, ",\"params\":");
    lsp_json_append(&j, params_json);
    lsp_json_append_char(&j, '}');
    lsp_io_write_message(j.data, j.len);
    lsp_json_free(&j);
}

/* ------------------------------------------------------------------ */
/*  initialize                                                        */
/* ------------------------------------------------------------------ */

static void handle_initialize(const char *msg, int id, LspDocStore *store) {
    (void)store;
    lsp_log("received initialize request");

    /* Extract workspace root from params.rootUri (preferred) or
     * params.rootPath (deprecated fallback). */
    char params[4096];
    if (lsp_json_get_object(msg, "params", params, sizeof(params)) == 0) {
        char root_uri[2048] = {0};
        if (lsp_json_get_string(params, "rootUri", root_uri, sizeof(root_uri)) == 0 &&
            root_uri[0] != '\0') {
            lsp_uri_to_path(root_uri, s_workspace_root, sizeof(s_workspace_root));
        } else {
            lsp_json_get_string(params, "rootPath", s_workspace_root,
                                sizeof(s_workspace_root));
        }
    }
    if (s_workspace_root[0]) {
        lsp_log("workspace root: %s", s_workspace_root);
    }

    /* Extract hover feature toggles from initializationOptions. */
    char init_opts[4096] = {0};
    if (lsp_json_get_object(params, "initializationOptions", init_opts,
                            sizeof(init_opts)) == 0) {
        char hover_opts[1024] = {0};
        if (lsp_json_get_object(init_opts, "hover", hover_opts,
                                sizeof(hover_opts)) == 0) {
            /* JSON booleans: strstr for "false" after the key. */
            if (strstr(hover_opts, "\"clocks\":false") ||
                strstr(hover_opts, "\"clocks\": false")) {
                s_hover_clocks = 0;
            }
            if (strstr(hover_opts, "\"declarations\":false") ||
                strstr(hover_opts, "\"declarations\": false")) {
                s_hover_declarations = 0;
            }
            lsp_log("hover config: clocks=%d declarations=%d",
                    s_hover_clocks, s_hover_declarations);
        }
    }

    const char *result =
        "{"
            "\"capabilities\":{"
                "\"textDocumentSync\":{"
                    "\"openClose\":true,"
                    "\"change\":1,"  /* Full sync */
                    "\"save\":{\"includeText\":true}"
                "},"
                "\"hoverProvider\":true,"
                "\"completionProvider\":{"
                    "\"triggerCharacters\":[\"@\"]"
                "},"
                "\"definitionProvider\":true"
            "},"
            "\"serverInfo\":{"
                "\"name\":\"jz-hdl-lsp\","
                "\"version\":\"0.1.0\""
            "}"
        "}";

    send_response(id, result);
}

static void handle_initialized(void) {
    lsp_log("client initialized");
}

/* ------------------------------------------------------------------ */
/*  shutdown                                                          */
/* ------------------------------------------------------------------ */

static void handle_shutdown(int id) {
    lsp_log("received shutdown request");
    send_response(id, "null");
}

/* ------------------------------------------------------------------ */
/*  Compile a document and publish diagnostics                        */
/* ------------------------------------------------------------------ */

/**
 * @brief Compile from a project file and emit diagnostics filtered to the
 *        current file.
 *
 * This is the project-aware compilation path used when the LSP discovers
 * that a standalone module belongs to a project.  The project file is read
 * from disk and compiled normally (its @import directives pull in all
 * modules), then diagnostics are filtered to only those originating from
 * the file the user is editing.
 */
/**
 * @brief Send a jz-hdl/projectInfo notification to the client.
 *
 * This custom notification tells the editor which project files are
 * available and which one is currently active for the file being edited.
 */
static void send_project_info(const char *uri,
                              const LspProjectList *projects,
                              int active_index) {
    LspJson j;
    lsp_json_init(&j);
    lsp_json_append(&j, "{\"uri\":");
    lsp_json_append_escaped(&j, uri);
    lsp_json_append(&j, ",\"projects\":[");

    for (size_t i = 0; i < projects->count; i++) {
        if (i > 0) lsp_json_append_char(&j, ',');
        lsp_json_append(&j, "{\"file\":");
        lsp_json_append_escaped(&j, projects->entries[i].file);
        lsp_json_append(&j, ",\"chip\":");
        lsp_json_append_escaped(&j, projects->entries[i].chip);
        lsp_json_append(&j, ",\"name\":");
        lsp_json_append_escaped(&j, projects->entries[i].name);
        lsp_json_append_char(&j, '}');
    }

    lsp_json_append(&j, "],\"activeIndex\":");
    lsp_json_append_int(&j, active_index);
    lsp_json_append_char(&j, '}');

    send_notification("jz-hdl/projectInfo", j.data);
    lsp_json_free(&j);
}

static void publish_diagnostics_via_project(const char *uri,
                                            const char *filepath,
                                            const char *project_path,
                                            const LspProjectList *projects,
                                            int active_index) {
    lsp_log("compiling via project: %s for file: %s", project_path, filepath);

    /* Initialize path security from the project file's perspective. */
    if (s_workspace_root[0]) {
        jz_path_security_init(project_path);
        jz_path_security_add_root(s_workspace_root);
    } else {
        jz_path_security_init(project_path);
    }

    JZCompiler compiler;
    jz_compiler_init(&compiler, JZ_COMPILER_MODE_LINT);

    /* Read the project file from disk. */
    size_t proj_size = 0;
    char *proj_source = jz_read_entire_file(project_path, &proj_size);
    if (!proj_source) {
        lsp_log("failed to read project file: %s", project_path);
        jz_compiler_dispose(&compiler);
        jz_path_security_cleanup();
        /* Fall through: publish empty diagnostics. */
        LspJson j;
        lsp_json_init(&j);
        lsp_json_append(&j, "{\"uri\":");
        lsp_json_append_escaped(&j, uri);
        lsp_json_append(&j, ",\"diagnostics\":[]}");
        send_notification("textDocument/publishDiagnostics", j.data);
        lsp_json_free(&j);
        return;
    }

    /* Expand @repeat blocks in the project source. */
    char *expanded = jz_repeat_expand(proj_source, project_path,
                                      &compiler.diagnostics);
    char *source = expanded ? expanded : proj_source;
    int free_source = (expanded != NULL);

    /* Lex the project file. */
    JZTokenStream tokens;
    memset(&tokens, 0, sizeof(tokens));
    int lex_ok = (jz_lex_source(project_path, source, &tokens,
                                &compiler.diagnostics) == 0);

    /* Parse — this processes @import directives, pulling in all modules. */
    JZASTNode *ast = NULL;
    if (lex_ok) {
        ast = jz_parse_file(project_path, &tokens, &compiler.diagnostics);
        compiler.ast_root = ast;
    }

    /* Template expansion + semantic analysis on the full project AST. */
    if (ast) {
        jz_template_expand(ast, &compiler.diagnostics, project_path);
        jz_sem_run(ast, &compiler.diagnostics, project_path, 0);

        /* Build IR for clock info extraction.  The IR gives us resolved
         * clock properties and port-to-clock mappings that the AST alone
         * cannot provide.  We always attempt this even if there are
         * diagnostics (e.g. PATH_TRAVERSAL_FORBIDDEN from sandbox
         * restrictions are not real semantic errors). */
        {
            IR_Design *ir = NULL;
            JZArena ir_arena;
            jz_arena_init(&ir_arena, 64 * 1024);
            if (jz_ir_build_design(ast, &ir, &ir_arena, &compiler.diagnostics) == 0
                && ir) {
                lsp_log("IR build succeeded for project, using IR clock cache");
                cache_clock_info_from_ir(ir);
            } else {
                lsp_log("IR build failed for project, falling back to AST clock cache");
                cache_clock_info_from_ast(ast);
            }
            for (size_t dbg = 0; dbg < s_clock_cache_count; ++dbg) {
                lsp_log("  clock[%zu]: name='%s' proj='%s' ext=%d gen=%d type='%s'",
                        dbg, s_clock_cache[dbg].name,
                        s_clock_cache[dbg].project_clock,
                        s_clock_cache[dbg].is_external,
                        s_clock_cache[dbg].is_generated,
                        s_clock_cache[dbg].gen_type);
            }
            jz_arena_free(&ir_arena);
        }
    }

    /* Build the JSON diagnostics array, filtering to only the current file. */
    LspJson j;
    lsp_json_init(&j);
    lsp_json_append(&j, "{\"uri\":");
    lsp_json_append_escaped(&j, uri);
    lsp_json_append(&j, ",\"diagnostics\":[");

    JZDiagnostic *diags = (JZDiagnostic *)compiler.diagnostics.buffer.data;
    size_t diag_count = compiler.diagnostics.buffer.len / sizeof(JZDiagnostic);

    /* Resolve the canonical base name of the current file for filtering. */
    const char *base_name = strrchr(filepath, '/');
    base_name = base_name ? base_name + 1 : filepath;

    int first = 1;
    for (size_t i = 0; i < diag_count; ++i) {
        JZDiagnostic *d = &diags[i];

        /* Only publish diagnostics that belong to the edited file. */
        if (d->loc.filename) {
            const char *diag_base = strrchr(d->loc.filename, '/');
            diag_base = diag_base ? diag_base + 1 : d->loc.filename;
            if (strcmp(diag_base, base_name) != 0) continue;
        } else {
            /* Diagnostics without a filename come from the project file
             * itself, not the module being edited. */
            continue;
        }

        /* Map severity. LSP: 1=Error, 2=Warning, 3=Information, 4=Hint. */
        int lsp_severity;
        switch (d->severity) {
        case JZ_SEVERITY_ERROR:   lsp_severity = 1; break;
        case JZ_SEVERITY_WARNING: lsp_severity = 2; break;
        case JZ_SEVERITY_NOTE:    lsp_severity = 3; break;
        default:                  lsp_severity = 3; break;
        }

        int line = d->loc.line > 0 ? d->loc.line - 1 : 0;
        int col = d->loc.column > 0 ? d->loc.column - 1 : 0;

        if (!first) lsp_json_append_char(&j, ',');
        first = 0;

        lsp_json_append(&j, "{\"range\":{\"start\":{\"line\":");
        lsp_json_append_int(&j, line);
        lsp_json_append(&j, ",\"character\":");
        lsp_json_append_int(&j, col);
        lsp_json_append(&j, "},\"end\":{\"line\":");
        lsp_json_append_int(&j, line);
        lsp_json_append(&j, ",\"character\":");
        lsp_json_append_int(&j, col);
        lsp_json_append(&j, "}},\"severity\":");
        lsp_json_append_int(&j, lsp_severity);
        lsp_json_append(&j, ",\"code\":");
        lsp_json_append_escaped(&j, d->code ? d->code : "");
        lsp_json_append(&j, ",\"source\":\"jz-hdl\"");
        lsp_json_append(&j, ",\"message\":");
        lsp_json_append_escaped(&j, d->message ? d->message : "");
        lsp_json_append_char(&j, '}');
    }

    lsp_json_append(&j, "]}");

    send_notification("textDocument/publishDiagnostics", j.data);
    lsp_json_free(&j);

    /* Notify the client about project context. */
    send_project_info(uri, projects, active_index);

    /* Cleanup. */
    jz_token_stream_free(&tokens);
    if (free_source) free(expanded);
    free(proj_source);
    jz_compiler_dispose(&compiler);
    jz_path_security_cleanup();
    jz_parser_free_imported_filenames();
}

static void publish_diagnostics(const char *uri, LspDocStore *store) {
    LspDocument *doc = lsp_docstore_find(store, uri);
    if (!doc) return;

    char filepath[1024];
    if (lsp_uri_to_path(uri, filepath, sizeof(filepath)) != 0) {
        strncpy(filepath, "untitled.jz", sizeof(filepath) - 1);
        filepath[sizeof(filepath) - 1] = '\0';
    }

    /* Initialize path security sandbox. Use workspace root if available,
     * otherwise fall back to the file's own directory. */
    if (s_workspace_root[0]) {
        jz_path_security_init(filepath);
        jz_path_security_add_root(s_workspace_root);
    } else {
        jz_path_security_init(filepath);
    }

    /* Run the compiler frontend on the in-memory document content. */
    JZCompiler compiler;
    jz_compiler_init(&compiler, JZ_COMPILER_MODE_LINT);

    /* Expand @repeat blocks. */
    char *expanded = jz_repeat_expand(doc->content, filepath,
                                      &compiler.diagnostics);
    char *source = expanded ? expanded : doc->content;
    int free_source = (expanded != NULL);

    /* Lex. */
    JZTokenStream tokens;
    memset(&tokens, 0, sizeof(tokens));
    int lex_ok = (jz_lex_source(filepath, source, &tokens,
                                &compiler.diagnostics) == 0);

    /* Parse. */
    JZASTNode *ast = NULL;
    if (lex_ok) {
        ast = jz_parse_file(filepath, &tokens, &compiler.diagnostics);
        compiler.ast_root = ast;
    }

    /* Detect whether the source file is a standalone module (no @project).
     * Standalone modules are valid importable files; project-level
     * diagnostics should be suppressed for them.
     *
     * We check both the AST (when parsing succeeds) and the raw source
     * text (as a fallback for mid-edit states where parsing may fail).
     * This ensures project discovery and rc updates happen even when
     * the file has transient syntax errors. */
    int is_standalone_module = 0;
    int has_project = 0;
    if (ast) {
        for (size_t i = 0; i < ast->child_count; ++i) {
            if (ast->children[i] &&
                ast->children[i]->type == JZ_AST_PROJECT) {
                has_project = 1;
                break;
            }
        }
        is_standalone_module = !has_project;
    }
    /* Fallback: text-level check when the AST is unavailable or
     * didn't detect @project (e.g. parse error mid-edit). */
    if (!has_project && doc->content && strstr(doc->content, "@project")) {
        has_project = 1;
        is_standalone_module = 0;
    }

    /* For standalone modules, try to discover the parent project file
     * and compile from there to get full cross-file resolution. */
    if (is_standalone_module) {
        LspProjectList projects;
        projects.count = 0;
        int idx = -1;

        if (lsp_discover_projects(filepath, s_workspace_root,
                                  0, NULL, &projects) == 0) {
            /* Check for a user override first. */
            const char *override = lookup_project_override(filepath);
            if (override) {
                for (size_t oi = 0; oi < projects.count; oi++) {
                    if (strcmp(projects.entries[oi].file, override) == 0) {
                        idx = (int)oi;
                        break;
                    }
                }
            }

            /* Fall back to automatic detection via @import. */
            if (idx < 0) {
                idx = lsp_find_project_for_file(&projects, filepath);
            }

            if (idx >= 0) {
                /* Clean up the standalone compilation and re-compile
                 * via the project that imports this file. */
                jz_token_stream_free(&tokens);
                if (free_source) free(source);
                jz_compiler_dispose(&compiler);
                jz_path_security_cleanup();
                jz_parser_free_imported_filenames();

                publish_diagnostics_via_project(uri, filepath,
                                                projects.entries[idx].file,
                                                &projects, idx);
                return;
            }
        }
        /* No project file found — fall through to standalone analysis. */
        lsp_log("no project file found, using standalone analysis for %s",
                filepath);

        /* Notify client: no active project for this file. */
        send_project_info(uri, &projects, -1);
    }

    /* If this file IS a project file, register it for future discovery. */
    if (has_project) {
        LspProjectList proj_list;
        proj_list.count = 0;
        lsp_discover_projects(filepath, s_workspace_root, 1,
                              doc->content, &proj_list);

        /* Find this file's index in the discovered list. */
        int self_idx = -1;
        char canonical[2048] = {0};
        {
            char *real = realpath(filepath, NULL);
            if (real) {
                strncpy(canonical, real, sizeof(canonical) - 1);
                free(real);
            } else {
                strncpy(canonical, filepath, sizeof(canonical) - 1);
            }
        }
        for (size_t pi = 0; pi < proj_list.count; pi++) {
            if (strcmp(proj_list.entries[pi].file, canonical) == 0) {
                self_idx = (int)pi;
                break;
            }
        }
        send_project_info(uri, &proj_list, self_idx);
    }

    /* Template expansion + semantic analysis. */
    if (ast) {
        jz_template_expand(ast, &compiler.diagnostics, filepath);
        jz_sem_run(ast, &compiler.diagnostics, filepath, 0);
        if (has_project) {
            {
                IR_Design *ir = NULL;
                JZArena ir_arena;
                jz_arena_init(&ir_arena, 64 * 1024);
                if (jz_ir_build_design(ast, &ir, &ir_arena,
                                       &compiler.diagnostics) == 0 && ir) {
                    cache_clock_info_from_ir(ir);
                } else {
                    cache_clock_info_from_ast(ast);
                }
                jz_arena_free(&ir_arena);
            }
        }
    }

    /* Build the JSON diagnostics array. */
    LspJson j;
    lsp_json_init(&j);
    lsp_json_append(&j, "{\"uri\":");
    lsp_json_append_escaped(&j, uri);
    lsp_json_append(&j, ",\"diagnostics\":[");

    JZDiagnostic *diags = (JZDiagnostic *)compiler.diagnostics.buffer.data;
    size_t diag_count = compiler.diagnostics.buffer.len / sizeof(JZDiagnostic);

    /* Resolve the canonical base name of the current file for filtering. */
    const char *base_name = strrchr(filepath, '/');
    base_name = base_name ? base_name + 1 : filepath;

    int first = 1;
    for (size_t i = 0; i < diag_count; ++i) {
        JZDiagnostic *d = &diags[i];

        /* Only publish diagnostics that belong to this file.  Diagnostics
         * from @import-ed files have a different loc.filename. */
        if (d->loc.filename) {
            const char *diag_base = strrchr(d->loc.filename, '/');
            diag_base = diag_base ? diag_base + 1 : d->loc.filename;
            if (strcmp(diag_base, base_name) != 0) continue;
        }

        /* Suppress project-level diagnostics for standalone module files. */
        if (is_standalone_module && d->code &&
            strncmp(d->code, "PROJECT_", 8) == 0) {
            continue;
        }

        /* Map severity. LSP: 1=Error, 2=Warning, 3=Information, 4=Hint. */
        int lsp_severity;
        switch (d->severity) {
        case JZ_SEVERITY_ERROR:   lsp_severity = 1; break;
        case JZ_SEVERITY_WARNING: lsp_severity = 2; break;
        case JZ_SEVERITY_NOTE:    lsp_severity = 3; break;
        default:                  lsp_severity = 3; break;
        }

        /* Lines and columns in LSP are 0-based; JZ-HDL uses 1-based. */
        int line = d->loc.line > 0 ? d->loc.line - 1 : 0;
        int col = d->loc.column > 0 ? d->loc.column - 1 : 0;

        if (!first) lsp_json_append_char(&j, ',');
        first = 0;

        lsp_json_append(&j, "{\"range\":{\"start\":{\"line\":");
        lsp_json_append_int(&j, line);
        lsp_json_append(&j, ",\"character\":");
        lsp_json_append_int(&j, col);
        lsp_json_append(&j, "},\"end\":{\"line\":");
        lsp_json_append_int(&j, line);
        lsp_json_append(&j, ",\"character\":");
        lsp_json_append_int(&j, col);
        lsp_json_append(&j, "}},\"severity\":");
        lsp_json_append_int(&j, lsp_severity);
        lsp_json_append(&j, ",\"code\":");
        lsp_json_append_escaped(&j, d->code ? d->code : "");
        lsp_json_append(&j, ",\"source\":\"jz-hdl\"");
        lsp_json_append(&j, ",\"message\":");
        lsp_json_append_escaped(&j, d->message ? d->message : "");
        lsp_json_append_char(&j, '}');
    }

    lsp_json_append(&j, "]}");

    send_notification("textDocument/publishDiagnostics", j.data);
    lsp_json_free(&j);

    /* Cleanup. */
    jz_token_stream_free(&tokens);
    if (free_source) free(source);
    jz_compiler_dispose(&compiler);
    jz_path_security_cleanup();
    jz_parser_free_imported_filenames();
}

/* ------------------------------------------------------------------ */
/*  textDocument/didOpen                                               */
/* ------------------------------------------------------------------ */

static void handle_text_document_did_open(const char *msg, LspDocStore *store) {
    /* Extract params.textDocument.{uri, text, version}. */
    char params[65536];
    if (lsp_json_get_object(msg, "params", params, sizeof(params)) != 0) return;

    char td[65536];
    if (lsp_json_get_object(params, "textDocument", td, sizeof(td)) != 0) return;

    char uri[2048];
    if (lsp_json_get_string(td, "uri", uri, sizeof(uri)) != 0) return;

    /* Extract text — it may be large, so we allocate dynamically. */
    char text[65536];
    if (lsp_json_get_string(td, "text", text, sizeof(text)) != 0) return;

    int version = 0;
    lsp_json_get_int(td, "version", &version);

    lsp_log("didOpen: %s (version %d)", uri, version);

    lsp_docstore_open(store, uri, text, version);
    publish_diagnostics(uri, store);
}

/* ------------------------------------------------------------------ */
/*  textDocument/didChange                                             */
/* ------------------------------------------------------------------ */

static void handle_text_document_did_change(const char *msg, LspDocStore *store) {
    char params[65536];
    if (lsp_json_get_object(msg, "params", params, sizeof(params)) != 0) return;

    char td[2048];
    if (lsp_json_get_object(params, "textDocument", td, sizeof(td)) != 0) return;

    char uri[2048];
    if (lsp_json_get_string(td, "uri", uri, sizeof(uri)) != 0) return;

    int version = 0;
    lsp_json_get_int(td, "version", &version);

    /* For full sync (change mode 1), contentChanges[0].text has the
     * entire file. We do a rough extraction. */
    char changes[65536];
    if (lsp_json_get_object(params, "contentChanges", changes, sizeof(changes)) != 0) return;

    /* contentChanges is an array. Find the first element's "text". */
    const char *p = changes;
    while (*p && *p != '{') ++p;
    if (*p == '{') {
        char element[65536];
        const char *end = p;
        int depth = 1;
        ++end;
        while (*end && depth > 0) {
            if (*end == '{') ++depth;
            else if (*end == '}') --depth;
            else if (*end == '"') {
                ++end;
                while (*end && *end != '"') {
                    if (*end == '\\') ++end;
                    if (*end) ++end;
                }
            }
            if (*end) ++end;
        }
        size_t elen = (size_t)(end - p);
        if (elen + 1 > sizeof(element)) elen = sizeof(element) - 1;
        memcpy(element, p, elen);
        element[elen] = '\0';

        char text[65536];
        if (lsp_json_get_string(element, "text", text, sizeof(text)) == 0) {
            LspDocument *doc = lsp_docstore_find(store, uri);
            if (doc) {
                lsp_docstore_update(doc, text, version);
            }
        }
    }

    publish_diagnostics(uri, store);
}

/* ------------------------------------------------------------------ */
/*  textDocument/didClose                                              */
/* ------------------------------------------------------------------ */

static void handle_text_document_did_close(const char *msg, LspDocStore *store) {
    char params[4096];
    if (lsp_json_get_object(msg, "params", params, sizeof(params)) != 0) return;

    char td[2048];
    if (lsp_json_get_object(params, "textDocument", td, sizeof(td)) != 0) return;

    char uri[2048];
    if (lsp_json_get_string(td, "uri", uri, sizeof(uri)) != 0) return;

    lsp_log("didClose: %s", uri);

    /* Clear diagnostics for the closed file. */
    LspJson j;
    lsp_json_init(&j);
    lsp_json_append(&j, "{\"uri\":");
    lsp_json_append_escaped(&j, uri);
    lsp_json_append(&j, ",\"diagnostics\":[]}");
    send_notification("textDocument/publishDiagnostics", j.data);
    lsp_json_free(&j);

    lsp_docstore_close(store, uri);
}

/* ------------------------------------------------------------------ */
/*  textDocument/didSave                                               */
/* ------------------------------------------------------------------ */

static void handle_text_document_did_save(const char *msg, LspDocStore *store) {
    char params[65536];
    if (lsp_json_get_object(msg, "params", params, sizeof(params)) != 0) return;

    char td[2048];
    if (lsp_json_get_object(params, "textDocument", td, sizeof(td)) != 0) return;

    char uri[2048];
    if (lsp_json_get_string(td, "uri", uri, sizeof(uri)) != 0) return;

    /* If the save includes text, update the document. */
    char text[65536];
    if (lsp_json_get_string(params, "text", text, sizeof(text)) == 0) {
        LspDocument *doc = lsp_docstore_find(store, uri);
        if (doc) {
            lsp_docstore_update(doc, text, doc->version);
        }
    }

    publish_diagnostics(uri, store);

    /* If this was a project file, other open documents that depend on it
     * need to be refreshed too (e.g. updated CHIP, renamed project,
     * added/removed @import lines). */
    {
        LspDocument *saved_doc = lsp_docstore_find(store, uri);
        int saved_is_project = 0;
        if (saved_doc && saved_doc->content &&
            strstr(saved_doc->content, "@project")) {
            saved_is_project = 1;
        }
        if (saved_is_project) {
            lsp_log("project file saved, refreshing all open documents");
            for (size_t i = 0; i < store->count; i++) {
                if (store->docs[i].uri &&
                    strcmp(store->docs[i].uri, uri) != 0) {
                    publish_diagnostics(store->docs[i].uri, store);
                }
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/*  jz-hdl/selectProject                                               */
/* ------------------------------------------------------------------ */

static void handle_select_project(const char *msg, LspDocStore *store) {
    char params[4096];
    if (lsp_json_get_object(msg, "params", params, sizeof(params)) != 0) return;

    char uri[2048] = {0};
    char project_file[2048] = {0};
    lsp_json_get_string(params, "uri", uri, sizeof(uri));
    lsp_json_get_string(params, "projectFile", project_file, sizeof(project_file));

    if (!uri[0] || !project_file[0]) return;

    char filepath[1024];
    if (lsp_uri_to_path(uri, filepath, sizeof(filepath)) != 0) return;

    lsp_log("selectProject: %s -> %s", filepath, project_file);

    set_project_override(filepath, project_file);

    /* Re-run diagnostics with the new project selection. */
    publish_diagnostics(uri, store);
}

/* ------------------------------------------------------------------ */
/*  textDocument/hover                                                 */
/* ------------------------------------------------------------------ */

static void handle_text_document_hover(const char *msg, int id,
                                       LspDocStore *store) {
    char params[4096];
    if (lsp_json_get_object(msg, "params", params, sizeof(params)) != 0) {
        send_response(id, "null");
        return;
    }

    char td[2048];
    if (lsp_json_get_object(params, "textDocument", td, sizeof(td)) != 0) {
        send_response(id, "null");
        return;
    }

    char uri[2048];
    if (lsp_json_get_string(td, "uri", uri, sizeof(uri)) != 0) {
        send_response(id, "null");
        return;
    }

    char pos_json[256];
    if (lsp_json_get_object(params, "position", pos_json, sizeof(pos_json)) != 0) {
        send_response(id, "null");
        return;
    }

    int line = 0, character = 0;
    lsp_json_get_int(pos_json, "line", &line);
    lsp_json_get_int(pos_json, "character", &character);

    LspDocument *doc = lsp_docstore_find(store, uri);
    if (!doc) {
        send_response(id, "null");
        return;
    }

    /* Find the word at the given position. */
    const char *src = doc->content;
    int cur_line = 0;
    const char *line_start = src;
    while (*line_start && cur_line < line) {
        if (*line_start == '\n') ++cur_line;
        ++line_start;
    }

    /* Find word boundaries at the character offset. */
    const char *p = line_start + character;
    if (p >= src + strlen(src)) {
        send_response(id, "null");
        return;
    }

    /* Check if cursor is on a @keyword. Scan backwards for @. */
    const char *word_start = p;
    while (word_start > line_start &&
           (word_start[-1] == '_' ||
            (word_start[-1] >= 'a' && word_start[-1] <= 'z') ||
            (word_start[-1] >= 'A' && word_start[-1] <= 'Z') ||
            (word_start[-1] >= '0' && word_start[-1] <= '9') ||
            word_start[-1] == '@')) {
        --word_start;
    }
    const char *word_end = p;
    while (*word_end == '_' ||
           (*word_end >= 'a' && *word_end <= 'z') ||
           (*word_end >= 'A' && *word_end <= 'Z') ||
           (*word_end >= '0' && *word_end <= '9')) {
        ++word_end;
    }

    size_t wlen = (size_t)(word_end - word_start);
    if (wlen == 0 || wlen > 127) {
        send_response(id, "null");
        return;
    }

    char word[128];
    memcpy(word, word_start, wlen);
    word[wlen] = '\0';

    /* Try to look up as a rule ID (for hover on diagnostic codes). */
    const JZRuleInfo *rule = jz_rule_lookup(word);

    /* Also provide hover for common JZ-HDL keywords. */
    const char *hover_text = NULL;
    char hover_buf[512];

    if (rule) {
        snprintf(hover_buf, sizeof(hover_buf),
                 "**%s** (%s)\n\n%s\n\nGroup: %s",
                 rule->id,
                 rule->mode == JZ_RULE_MODE_ERR ? "error" :
                 rule->mode == JZ_RULE_MODE_WRN ? "warning" : "info",
                 rule->description ? rule->description : "",
                 rule->group ? rule->group : "");
        hover_text = hover_buf;
    } else if (strcmp(word, "@module") == 0) {
        hover_text = "**@module** — Defines a hardware module with ports, wires, registers, and logic.";
    } else if (strcmp(word, "@project") == 0) {
        hover_text = "**@project** — Top-level container that instantiates and wires modules together.";
    } else if (strcmp(word, "@import") == 0) {
        hover_text = "**@import** — Imports a module definition from another JZ-HDL source file.";
    } else if (strcmp(word, "@new") == 0) {
        hover_text = "**@new** — Instantiates a module within a project or module.";
    } else if (strcmp(word, "@top") == 0) {
        hover_text = "**@top** — Marks the top-level module for synthesis.";
    } else if (strcmp(word, "@template") == 0) {
        hover_text = "**@template** — Defines a reusable block of statements that can be applied with @apply.";
    } else if (strcmp(word, "@apply") == 0) {
        hover_text = "**@apply** — Expands a previously defined @template with argument substitution.";
    } else if (strcmp(word, "@if") == 0 || strcmp(word, "IF") == 0) {
        hover_text = "**IF** — Conditional statement for multiplexed logic selection.";
    } else if (strcmp(word, "@select") == 0 || strcmp(word, "SELECT") == 0) {
        hover_text = "**SELECT** — Multi-way selection (like case/switch) on a key expression.";
    } else if (strcmp(word, "CONST") == 0) {
        hover_text = "**CONST** — Block declaring compile-time constant values.";
    } else if (strcmp(word, "PORT") == 0) {
        hover_text = "**PORT** — Block declaring module input/output ports.";
    } else if (strcmp(word, "WIRE") == 0) {
        hover_text = "**WIRE** — Block declaring combinational wires (no storage).";
    } else if (strcmp(word, "REGISTER") == 0) {
        hover_text = "**REGISTER** — Block declaring clocked flip-flop registers.";
    } else if (strcmp(word, "LATCH") == 0) {
        hover_text = "**LATCH** — Block declaring level-sensitive latches.";
    } else if (strcmp(word, "MEM") == 0) {
        hover_text = "**MEM** — Block declaring memory arrays (RAM/ROM).";
    } else if (strcmp(word, "SYNCHRONOUS") == 0) {
        hover_text = "**SYNCHRONOUS** — Block for clocked (synchronous) logic assignments.";
    } else if (strcmp(word, "ASYNCHRONOUS") == 0) {
        hover_text = "**ASYNCHRONOUS** — Block for combinational (asynchronous) logic.";
    } else if (strcmp(word, "MUX") == 0) {
        hover_text = "**MUX** — Block for multiplexer-based signal selection.";
    } else if (strcmp(word, "CDC") == 0) {
        hover_text = "**CDC** — Clock Domain Crossing block for safe cross-domain signal transfer.";
    }

    /* Check if the word is a known clock name. */
    if (!hover_text && s_hover_clocks) {
        const LspClockInfo *clk = lookup_clock_info(word);
        if (clk) {
            char *p = hover_buf;
            size_t rem = sizeof(hover_buf);
            int n;

            n = snprintf(p, rem, "**Clock: %s**\n\n", clk->name);
            if (n > 0 && (size_t)n < rem) { p += n; rem -= (size_t)n; }

            if (clk->is_external) {
                n = snprintf(p, rem, "Source: External (input pin)\n");
                if (n > 0 && (size_t)n < rem) { p += n; rem -= (size_t)n; }
            } else if (clk->is_generated) {
                n = snprintf(p, rem, "Source: %s", clk->gen_type);
                if (n > 0 && (size_t)n < rem) { p += n; rem -= (size_t)n; }

                if (clk->gen_output[0]) {
                    n = snprintf(p, rem, " %s output", clk->gen_output);
                    if (n > 0 && (size_t)n < rem) { p += n; rem -= (size_t)n; }
                }
                n = snprintf(p, rem, "\n");
                if (n > 0 && (size_t)n < rem) { p += n; rem -= (size_t)n; }

                if (clk->gen_input_clock[0]) {
                    n = snprintf(p, rem, "Reference: %s\n", clk->gen_input_clock);
                    if (n > 0 && (size_t)n < rem) { p += n; rem -= (size_t)n; }
                }
            } else {
                n = snprintf(p, rem, "Source: Unknown\n");
                if (n > 0 && (size_t)n < rem) { p += n; rem -= (size_t)n; }
            }

            if (clk->period_ns > 0.0) {
                double freq_mhz = 1000.0 / clk->period_ns;
                n = snprintf(p, rem, "Period: %.3f ns (%.3f MHz)\n",
                             clk->period_ns, freq_mhz);
                if (n > 0 && (size_t)n < rem) { p += n; rem -= (size_t)n; }
            }

            if (clk->edge[0]) {
                n = snprintf(p, rem, "Edge: %s", clk->edge);
                if (n > 0 && (size_t)n < rem) { p += n; rem -= (size_t)n; }
            } else {
                n = snprintf(p, rem, "Edge: Rising (default)");
                if (n > 0 && (size_t)n < rem) { p += n; rem -= (size_t)n; }
            }

            hover_text = hover_buf;
        }
    }

    /* Check if the word matches a declaration (register, wire, port, etc.)
     * by parsing the current file's AST and searching for the name. */
    if (!hover_text && s_hover_declarations) {
        char hover_filepath[1024];
        if (lsp_uri_to_path(uri, hover_filepath, sizeof(hover_filepath)) == 0) {
            JZCompiler hover_compiler;
            jz_compiler_init(&hover_compiler, JZ_COMPILER_MODE_LINT);

            char *hover_expanded = jz_repeat_expand(doc->content, hover_filepath,
                                                     &hover_compiler.diagnostics);
            char *hover_src = hover_expanded ? hover_expanded : doc->content;

            JZTokenStream hover_tokens;
            memset(&hover_tokens, 0, sizeof(hover_tokens));
            if (jz_lex_source(hover_filepath, hover_src, &hover_tokens,
                              &hover_compiler.diagnostics) == 0) {
                JZASTNode *hover_ast = jz_parse_file(hover_filepath, &hover_tokens,
                                                      &hover_compiler.diagnostics);
                if (hover_ast) {
                    jz_template_expand(hover_ast, &hover_compiler.diagnostics,
                                       hover_filepath);
                    JZASTNode *decl = find_declaration(hover_ast, word);
                    if (decl) {
                        char *bp = hover_buf;
                        size_t rem = sizeof(hover_buf);
                        int n;

                        const char *kind = "";
                        switch (decl->type) {
                        case JZ_AST_REGISTER_DECL: kind = "Register"; break;
                        case JZ_AST_WIRE_DECL:     kind = "Wire"; break;
                        case JZ_AST_PORT_DECL:     kind = "Port"; break;
                        case JZ_AST_LATCH_DECL:    kind = "Latch"; break;
                        case JZ_AST_MEM_DECL:      kind = "Memory"; break;
                        case JZ_AST_CONST_DECL:    kind = "Constant"; break;
                        default:                   kind = "Declaration"; break;
                        }

                        n = snprintf(bp, rem, "**%s: %s**\n\n",
                                     kind, decl->name ? decl->name : word);
                        if (n > 0 && (size_t)n < rem) { bp += n; rem -= (size_t)n; }

                        /* Direction for ports. */
                        if (decl->type == JZ_AST_PORT_DECL && decl->block_kind) {
                            n = snprintf(bp, rem, "Direction: %s\n", decl->block_kind);
                            if (n > 0 && (size_t)n < rem) { bp += n; rem -= (size_t)n; }
                        }

                        /* Width. */
                        if (decl->width) {
                            /* Trim trailing whitespace from width string. */
                            char width_clean[128];
                            strncpy(width_clean, decl->width, sizeof(width_clean) - 1);
                            width_clean[sizeof(width_clean) - 1] = '\0';
                            size_t wl = strlen(width_clean);
                            while (wl > 0 && (width_clean[wl-1] == ' ' ||
                                              width_clean[wl-1] == '\t')) {
                                width_clean[--wl] = '\0';
                            }
                            n = snprintf(bp, rem, "Width: [%s]\n", width_clean);
                            if (n > 0 && (size_t)n < rem) { bp += n; rem -= (size_t)n; }
                        }

                        /* Latch type. */
                        if (decl->type == JZ_AST_LATCH_DECL && decl->block_kind) {
                            n = snprintf(bp, rem, "Type: %s\n", decl->block_kind);
                            if (n > 0 && (size_t)n < rem) { bp += n; rem -= (size_t)n; }
                        }

                        /* Reset value for registers (first child is init expr). */
                        if (decl->type == JZ_AST_REGISTER_DECL &&
                            decl->child_count > 0 && decl->children[0]) {
                            JZASTNode *init = decl->children[0];
                            const char *init_text = NULL;
                            if (init->text) {
                                init_text = init->text;
                            } else if (init->name) {
                                init_text = init->name;
                            }
                            if (init_text) {
                                n = snprintf(bp, rem, "Reset: %s", init_text);
                                if (n > 0 && (size_t)n < rem) { bp += n; rem -= (size_t)n; }
                            }
                        }

                        /* Constant value (first child is value expr). */
                        if (decl->type == JZ_AST_CONST_DECL &&
                            decl->child_count > 0 && decl->children[0]) {
                            JZASTNode *val = decl->children[0];
                            const char *val_text = val->text ? val->text : val->name;
                            if (val_text) {
                                n = snprintf(bp, rem, "Value: %s", val_text);
                                if (n > 0 && (size_t)n < rem) { bp += n; rem -= (size_t)n; }
                            }
                        }

                        hover_text = hover_buf;
                    }
                }
                jz_token_stream_free(&hover_tokens);
            }
            if (hover_expanded) free(hover_expanded);
            jz_compiler_dispose(&hover_compiler);
            jz_parser_free_imported_filenames();
        }
    }

    if (!hover_text) {
        send_response(id, "null");
        return;
    }

    LspJson j;
    lsp_json_init(&j);
    lsp_json_append(&j, "{\"contents\":{\"kind\":\"markdown\",\"value\":");
    lsp_json_append_escaped(&j, hover_text);
    lsp_json_append(&j, "}}");
    send_response(id, j.data);
    lsp_json_free(&j);
}

/* ------------------------------------------------------------------ */
/*  textDocument/completion                                            */
/* ------------------------------------------------------------------ */

static const char *jz_keywords[] = {
    "@module", "@endmod", "@project", "@endproj", "@blackbox",
    "@new", "@top", "@import", "@global", "@endglob",
    "@feature", "@endfeat", "@else", "@check",
    "@template", "@endtemplate", "@apply", "@scratch",
    "@testbench", "@endtb", "@simulation", "@endsim",
    "@setup", "@update", "@clock", "@run", "@run_until", "@run_while",
    "@print", "@print_if", "@trace", "@mark", "@mark_if",
    "@alert", "@alert_if",
    "@expect_equal", "@expect_not_equal", "@expect_tristate",
    NULL
};

static const char *jz_block_keywords[] = {
    "CONST", "PORT", "WIRE", "REGISTER", "LATCH", "MEM",
    "ASYNCHRONOUS", "SYNCHRONOUS", "MUX", "CDC", "CONFIG",
    "CLOCKS", "IN_PINS", "OUT_PINS", "INOUT_PINS", "MAP",
    "CLOCK_GEN", "IF", "ELIF", "ELSE", "SELECT", "CASE",
    "DEFAULT", "IN", "OUT", "INOUT", "OVERRIDE", "GND", "VCC",
    "DISCONNECT", "TEST", "TAP",
    NULL
};

static void handle_text_document_completion(const char *msg, int id,
                                            LspDocStore *store) {
    (void)msg;
    (void)store;

    LspJson j;
    lsp_json_init(&j);
    lsp_json_append(&j, "[");

    int first = 1;

    /* @ directives (CompletionItemKind.Keyword = 14). */
    for (int i = 0; jz_keywords[i]; ++i) {
        if (!first) lsp_json_append_char(&j, ',');
        first = 0;
        lsp_json_append(&j, "{\"label\":");
        lsp_json_append_escaped(&j, jz_keywords[i]);
        lsp_json_append(&j, ",\"kind\":14}");
    }

    /* Block keywords (CompletionItemKind.Keyword = 14). */
    for (int i = 0; jz_block_keywords[i]; ++i) {
        if (!first) lsp_json_append_char(&j, ',');
        first = 0;
        lsp_json_append(&j, "{\"label\":");
        lsp_json_append_escaped(&j, jz_block_keywords[i]);
        lsp_json_append(&j, ",\"kind\":14}");
    }

    lsp_json_append_char(&j, ']');
    send_response(id, j.data);
    lsp_json_free(&j);
}

/* ------------------------------------------------------------------ */
/*  textDocument/definition                                            */
/* ------------------------------------------------------------------ */

/* Walk AST to find a declaration node matching a given name. */
static JZASTNode *find_declaration(JZASTNode *node, const char *name) {
    if (!node || !name) return NULL;

    /* Check if this node is a declaration with the matching name. */
    if (node->name && strcmp(node->name, name) == 0) {
        switch (node->type) {
        case JZ_AST_MODULE:
        case JZ_AST_PROJECT:
        case JZ_AST_PORT_DECL:
        case JZ_AST_WIRE_DECL:
        case JZ_AST_REGISTER_DECL:
        case JZ_AST_LATCH_DECL:
        case JZ_AST_MEM_DECL:
        case JZ_AST_CONST_DECL:
        case JZ_AST_INSTANTIATION:
            return node;
        default:
            break;
        }
    }

    /* Recurse into children. */
    for (size_t i = 0; i < node->child_count; ++i) {
        JZASTNode *result = find_declaration(node->children[i], name);
        if (result) return result;
    }
    return NULL;
}

static void handle_text_document_definition(const char *msg, int id,
                                            LspDocStore *store) {
    char params[4096];
    if (lsp_json_get_object(msg, "params", params, sizeof(params)) != 0) {
        send_response(id, "null");
        return;
    }

    char td[2048];
    if (lsp_json_get_object(params, "textDocument", td, sizeof(td)) != 0) {
        send_response(id, "null");
        return;
    }

    char uri[2048];
    if (lsp_json_get_string(td, "uri", uri, sizeof(uri)) != 0) {
        send_response(id, "null");
        return;
    }

    char pos_json[256];
    if (lsp_json_get_object(params, "position", pos_json, sizeof(pos_json)) != 0) {
        send_response(id, "null");
        return;
    }

    int line = 0, character = 0;
    lsp_json_get_int(pos_json, "line", &line);
    lsp_json_get_int(pos_json, "character", &character);

    LspDocument *doc = lsp_docstore_find(store, uri);
    if (!doc) {
        send_response(id, "null");
        return;
    }

    /* Find word under cursor. */
    const char *src = doc->content;
    int cur_line = 0;
    const char *line_start = src;
    while (*line_start && cur_line < line) {
        if (*line_start == '\n') ++cur_line;
        ++line_start;
    }

    const char *p = line_start + character;
    if (p >= src + strlen(src)) {
        send_response(id, "null");
        return;
    }

    const char *word_start = p;
    while (word_start > src &&
           (word_start[-1] == '_' ||
            (word_start[-1] >= 'a' && word_start[-1] <= 'z') ||
            (word_start[-1] >= 'A' && word_start[-1] <= 'Z') ||
            (word_start[-1] >= '0' && word_start[-1] <= '9'))) {
        --word_start;
    }
    const char *word_end = p;
    while (*word_end == '_' ||
           (*word_end >= 'a' && *word_end <= 'z') ||
           (*word_end >= 'A' && *word_end <= 'Z') ||
           (*word_end >= '0' && *word_end <= '9')) {
        ++word_end;
    }

    size_t wlen = (size_t)(word_end - word_start);
    if (wlen == 0 || wlen > 127) {
        send_response(id, "null");
        return;
    }

    char word[128];
    memcpy(word, word_start, wlen);
    word[wlen] = '\0';

    /* Parse the document to get the AST, then search for declarations. */
    char filepath[1024];
    if (lsp_uri_to_path(uri, filepath, sizeof(filepath)) != 0) {
        send_response(id, "null");
        return;
    }

    if (s_workspace_root[0]) {
        jz_path_security_init(filepath);
        jz_path_security_add_root(s_workspace_root);
    } else {
        jz_path_security_init(filepath);
    }

    JZCompiler compiler;
    jz_compiler_init(&compiler, JZ_COMPILER_MODE_LINT);

    char *expanded = jz_repeat_expand(doc->content, filepath,
                                      &compiler.diagnostics);
    char *compile_src = expanded ? expanded : doc->content;
    int free_src = (expanded != NULL);

    JZTokenStream tokens;
    memset(&tokens, 0, sizeof(tokens));
    int lex_ok = (jz_lex_source(filepath, compile_src, &tokens,
                                &compiler.diagnostics) == 0);
    JZASTNode *ast = NULL;
    if (lex_ok) {
        ast = jz_parse_file(filepath, &tokens, &compiler.diagnostics);
        compiler.ast_root = ast;
    }

    if (ast) {
        jz_template_expand(ast, &compiler.diagnostics, filepath);
    }

    JZASTNode *target = ast ? find_declaration(ast, word) : NULL;

    if (target && target->loc.line > 0) {
        LspJson j;
        lsp_json_init(&j);
        lsp_json_append(&j, "{\"uri\":");
        lsp_json_append_escaped(&j, uri);
        lsp_json_append(&j, ",\"range\":{\"start\":{\"line\":");
        lsp_json_append_int(&j, target->loc.line - 1);
        lsp_json_append(&j, ",\"character\":");
        lsp_json_append_int(&j, target->loc.column > 0 ? target->loc.column - 1 : 0);
        lsp_json_append(&j, "},\"end\":{\"line\":");
        lsp_json_append_int(&j, target->loc.line - 1);
        lsp_json_append(&j, ",\"character\":");
        lsp_json_append_int(&j, target->loc.column > 0 ? target->loc.column - 1 : 0);
        lsp_json_append(&j, "}}}");
        send_response(id, j.data);
        lsp_json_free(&j);
    } else {
        send_response(id, "null");
    }

    jz_token_stream_free(&tokens);
    if (free_src) free(compile_src);
    jz_compiler_dispose(&compiler);
    jz_path_security_cleanup();
    jz_parser_free_imported_filenames();
}
