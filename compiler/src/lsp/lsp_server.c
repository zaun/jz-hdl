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

#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Forward declarations                                              */
/* ------------------------------------------------------------------ */

/* Workspace root path extracted from initialize request. */
static char s_workspace_root[2048] = {0};

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
static void publish_diagnostics_via_project(const char *uri,
                                            const char *filepath,
                                            const char *project_path) {
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
     * diagnostics should be suppressed for them. */
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

    /* For standalone modules, try to discover the parent project file
     * and compile from there to get full cross-file resolution. */
    if (is_standalone_module) {
        char project_path[2048] = {0};
        if (lsp_discover_project_file(filepath, s_workspace_root,
                                      0, project_path,
                                      sizeof(project_path)) == 0) {
            /* Clean up the standalone compilation and re-compile via project. */
            jz_token_stream_free(&tokens);
            if (free_source) free(source);
            jz_compiler_dispose(&compiler);
            jz_path_security_cleanup();
            jz_parser_free_imported_filenames();

            publish_diagnostics_via_project(uri, filepath, project_path);
            return;
        }
        /* No project file found — fall through to standalone analysis. */
        lsp_log("no project file found, using standalone analysis for %s",
                filepath);
    }

    /* If this file IS a project file, register it for future discovery. */
    if (has_project) {
        char unused[2048];
        lsp_discover_project_file(filepath, s_workspace_root, 1,
                                  unused, sizeof(unused));
    }

    /* Template expansion + semantic analysis. */
    if (ast) {
        jz_template_expand(ast, &compiler.diagnostics, filepath);
        jz_sem_run(ast, &compiler.diagnostics, filepath, 0);
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
                 "**%s** (%s)\\n\\n%s\\n\\nGroup: %s",
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
    "@feature", "@endfeat", "@feature_else", "@check",
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
