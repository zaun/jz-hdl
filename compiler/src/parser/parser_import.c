/**
 * @file parser_import.c
 * @brief Support for @import directives and imported source file handling.
 *
 * This file implements the logic required to load, lex, and parse external
 * source files referenced by @import directives inside a @project block.
 * Imported modules and global blocks are merged into the host project AST.
 *
 * To ensure diagnostic stability, filename strings associated with imported
 * tokens are retained for the lifetime of parsing and freed only when parsing
 * is fully complete.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <limits.h>
#endif

#include "parser_internal.h"
#include "path_security.h"

/**
 * @brief Record an imported filename for lifetime management.
 *
 * Imported files allocate their filename strings dynamically. These pointers
 * are copied into token locations and AST nodes, so they must remain valid
 * until parsing and diagnostics are complete.
 *
 * This function appends the filename pointer to a global list that is later
 * released by jz_parser_free_imported_filenames().
 *
 * @param path Allocated filename string to retain
 * @return 0 on success, -1 on allocation failure
 */
static int remember_imported_filename(char *path)
{
    if (!path) return 0;
    if (g_imported_filenames_len == g_imported_filenames_cap) {
        size_t new_cap = g_imported_filenames_cap ? g_imported_filenames_cap * 2 : 8;
        char **new_arr = (char **)realloc(g_imported_filenames, new_cap * sizeof(char *));
        if (!new_arr) {
            /* If we fail here, we must not drop the pointer, otherwise it would
             * leak without being tracked. Just fall back to leaking this one
             * path; callers can still free previously remembered ones. */
            return -1;
        }
        g_imported_filenames = new_arr;
        g_imported_filenames_cap = new_cap;
    }
    g_imported_filenames[g_imported_filenames_len++] = path;
    return 0;
}

/**
 * @brief Import modules and globals from an external source file.
 *
 * This function resolves a relative or absolute path, loads the source file,
 * lexes it, and parses its top-level constructs. Imported modules and global
 * blocks are attached directly to the target project AST.
 *
 * Rules enforced:
 * - Each resolved file path may only be imported once per project
 * - Imported files must not contain their own @project blocks
 * - Imported module/blackbox names must not collide with existing ones
 *
 * @param parent       Parser performing the import
 * @param proj         Target project AST node
 * @param rel_path     Path string from the @import directive
 * @param import_token Token corresponding to the @import keyword
 * @return 0 on success, -1 on error
 */
int import_modules_from_path(const Parser *parent,
                                    JZASTNode *proj,
                                    const char *rel_path,
                                    const JZToken *import_token) {
    if (!proj || !rel_path) return -1;

    /* Validate the import path against security policy. */
    char base_dir[512];
    base_dir[0] = '\0';
    if (parent->filename) {
        const char *slash = strrchr(parent->filename, '/');
        if (slash) {
            size_t dir_len = (size_t)(slash - parent->filename);
            if (dir_len >= sizeof(base_dir)) dir_len = sizeof(base_dir) - 1;
            memcpy(base_dir, parent->filename, dir_len);
            base_dir[dir_len] = '\0';
        }
    }

    JZLocation import_loc = import_token ? import_token->loc :
        (JZLocation){ parent->filename, 1, 1 };

    /* Validate and canonicalize the import path.  When the parent parser
     * carries a diagnostic list the full security policy (sandbox, absolute,
     * traversal) is enforced.  Otherwise we still canonicalize with
     * realpath() so that the dedup comparison in the IMPORT_FILE_MULTIPLE_TIMES
     * check below uses a consistent canonical representation regardless of
     * how the path was spelled (symlinks, extra slashes, case on
     * case-insensitive filesystems, etc.).
     */
    char *full_path = NULL;
    if (parent->diagnostics) {
        full_path = jz_path_validate(rel_path, base_dir[0] ? base_dir : NULL,
                                      import_loc, parent->diagnostics);
        if (!full_path) return -1;
    } else {
        /* Build a joined path, then canonicalize it. */
        char *joined = NULL;
        if (rel_path[0] == '/') {
            joined = jz_strdup(rel_path);
        } else if (base_dir[0]) {
            size_t dir_len = strlen(base_dir);
            size_t path_len = strlen(rel_path);
            joined = (char *)malloc(dir_len + 1 + path_len + 1);
            if (!joined) return -1;
            memcpy(joined, base_dir, dir_len);
            joined[dir_len] = '/';
            memcpy(joined + dir_len + 1, rel_path, path_len + 1);
        } else {
            joined = jz_strdup(rel_path);
        }
        if (!joined) return -1;

        /* Canonicalize via realpath so dedup is symlink- and case-aware. */
        full_path = realpath(joined, NULL);
        if (!full_path) {
            /* File may not exist yet; keep the joined path as-is. */
            full_path = joined;
        } else {
            free(joined);
        }
    }

    /* IMPORT_FILE_MULTIPLE_TIMES: disallow importing the same resolved path more
     * than once into a single project (duplicate @import or nested re-import).
     * We compare against the global list of previously imported filenames that
     * is also used to keep JZLocation.filename pointers alive.
     */
    for (size_t i = 0; i < g_imported_filenames_len; ++i) {
        const char *seen = g_imported_filenames[i];
        if (seen && strcmp(seen, full_path) == 0) {
            if (parent && parent->diagnostics && import_token) {
                parser_report_rule(parent,
                                   import_token,
                                   "IMPORT_FILE_MULTIPLE_TIMES",
                                   "same source file imported more than once into a single project");
            } else if (import_token) {
                fprintf(stderr,
                        "%s:%d:%d: import error: same source file imported more than once into a single project\n",
                        import_token->loc.filename ? import_token->loc.filename : "<input>",
                        import_token->loc.line,
                        import_token->loc.column);
            } else {
                fprintf(stderr,
                        "%s:1:1: import error: same source file imported more than once into a single project\n",
                        full_path);
            }
            free(full_path);
            return -1;
        }
    }

    size_t size = 0;
    char *source = jz_read_entire_file(full_path, &size);
    if (!source) {
        /* Failed to read imported file; surface as a generic parse error on
         * the parent stream rather than through a dedicated rule for now.
         */
        fprintf(stderr, "%s:1:1: import error: failed to read imported file '%s'\n",
                full_path, rel_path);
        free(full_path);
        return -1;
    }

    JZTokenStream tokens;
    if (jz_lex_source(full_path, source, &tokens, NULL) != 0) {
        fprintf(stderr, "%s:1:1: import error: lexing failed for imported file\n", full_path);
        free(source);
        free(full_path);
        return -1;
    }

    Parser ip;
    ip.filename = full_path;
    ip.tokens = tokens.tokens;
    ip.count = tokens.count;
    ip.pos = 0;
    /* Propagate the parent's diagnostic list so that nested imports from
     * this file go through jz_path_validate() with full security policy
     * and produce consistent canonical paths for dedup.
     */
    ip.diagnostics = parent->diagnostics;

    int saw_project = 0;

    while (peek(&ip)->type != JZ_TOK_EOF) {
        const JZToken *t = peek(&ip);
        if (t->type == JZ_TOK_KW_MODULE) {
            advance(&ip);
            JZASTNode *mod = parse_module(&ip);
            if (!mod) {
                jz_token_stream_free(&tokens);
                free(source);
                free(full_path);
                return -1;
            }

            /* IMPORT_DUP_MODULE_OR_BLACKBOX: detect when an imported module
             * name collides with an existing module/blackbox in the project.
             */
            int is_duplicate = 0;
            if (mod->name) {
                for (size_t i = 0; i < proj->child_count; ++i) {
                    JZASTNode *existing = proj->children[i];
                    if (!existing || !existing->name) continue;
                    if (existing->type != JZ_AST_MODULE &&
                        existing->type != JZ_AST_BLACKBOX) {
                        continue;
                    }
                    if (strcmp(existing->name, mod->name) == 0) {
                        is_duplicate = 1;
                        break;
                    }
                }
            }

            if (is_duplicate && parent && parent->diagnostics) {
                JZToken fake;
                memset(&fake, 0, sizeof(fake));
                fake.loc = mod->loc;
                parser_report_rule(parent,
                                   &fake,
                                   "IMPORT_DUP_MODULE_OR_BLACKBOX",
                                   "imported module/blackbox name duplicates existing definition in project");
                jz_ast_free(mod);
                continue;
            }

            if (jz_ast_add_child(proj, mod) != 0) {
                jz_ast_free(mod);
                jz_token_stream_free(&tokens);
                free(source);
                free(full_path);
                return -1;
            }
        } else if (t->type == JZ_TOK_KW_GLOBAL) {
            /* Imported @global blocks contribute GLOBAL namespaces just like
             * top-level globals in the primary compilation unit. They are
             * attached directly to the host project so that build_symbol_tables
             * and sem_check_globals can discover them.
             */
            advance(&ip);
            JZASTNode *glob = parse_global(&ip);
            if (!glob) {
                jz_token_stream_free(&tokens);
                free(source);
                free(full_path);
                return -1;
            }

            if (jz_ast_add_child(proj, glob) != 0) {
                jz_ast_free(glob);
                jz_token_stream_free(&tokens);
                free(source);
                free(full_path);
                return -1;
            }
        } else if (t->type == JZ_TOK_KW_PROJECT) {
            if (!saw_project) {
                saw_project = 1;
                if (parent && parent->diagnostics) {
                    parser_report_rule(parent,
                                       t,
                                       "IMPORT_FILE_HAS_PROJECT",
                                       "imported files must not contain their own @project/@endproj block");
                } else {
                    fprintf(stderr,
                            "%s:%d:%d: import error: imported files may not contain @project\n",
                            t->loc.filename ? t->loc.filename : full_path,
                            t->loc.line, t->loc.column);
                }
            }
            /* Consume the project to keep parsing position consistent, then fail. */
            advance(&ip); /* consume @project */
            JZASTNode *bad_proj = parse_project(&ip);
            if (bad_proj) jz_ast_free(bad_proj);
            jz_token_stream_free(&tokens);
            free(source);
            /* Keep full_path alive so the diagnostic's loc.filename pointer
             * (set by the lexer) remains valid for later printing.
             */
            remember_imported_filename(full_path);
            return -1;
        } else {
            /* Skip other top-level constructs in imported files for now. */
            advance(&ip);
        }
    }

    jz_token_stream_free(&tokens);
    free(source);

    /*
     * Keep the allocated filename string alive so that all JZLocation.filename
     * pointers in the imported AST remain valid. The caller is responsible
     * for eventually calling jz_parser_free_imported_filenames() once the AST
     * and any diagnostics that reference these locations are no longer used.
     */
    remember_imported_filename(full_path);
    return 0;
}

/**
 * @brief Free all retained imported filename strings.
 *
 * This function must be called once parsing and all diagnostics are complete.
 * It releases all filename strings retained for imported source files.
 */
void jz_parser_free_imported_filenames(void)
{
    if (!g_imported_filenames) {
        return;
    }
    for (size_t i = 0; i < g_imported_filenames_len; ++i) {
        free(g_imported_filenames[i]);
    }
    free(g_imported_filenames);
    g_imported_filenames = NULL;
    g_imported_filenames_len = 0;
    g_imported_filenames_cap = 0;
}
