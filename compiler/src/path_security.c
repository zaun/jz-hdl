/**
 * @file path_security.c
 * @brief File path security and sandboxing implementation.
 *
 * Enforces Section 12 of the JZ-HDL specification: all user-specified
 * file paths are validated against sandbox roots, and absolute paths
 * or directory traversal are forbidden unless explicitly allowed.
 */

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "path_security.h"
#include "rules.h"
#include "util.h"

/* Use realpath on POSIX, _fullpath on Windows. */
#ifdef _WIN32
#include <direct.h>
#define PATH_SEP '\\'
#else
#include <limits.h>
#include <unistd.h>
#define PATH_SEP '/'
#endif

#define MAX_SANDBOX_ROOTS 32

/* -------------------------------------------------------------------------
 * File-level globals (same pattern as tristate_default in driver.c)
 * ------------------------------------------------------------------------- */

static char *g_sandbox_roots[MAX_SANDBOX_ROOTS];
static size_t g_sandbox_root_count = 0;
static char *g_default_sandbox_root = NULL;
static int g_allow_absolute = 0;
static int g_allow_traversal = 0;

/* -------------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------------- */

/**
 * @brief Extract the directory portion of a file path.
 * @return Heap-allocated directory string, or NULL.
 */
static char *path_dirname(const char *filepath)
{
    if (!filepath) return NULL;

    const char *slash = strrchr(filepath, '/');
#ifdef _WIN32
    const char *bslash = strrchr(filepath, '\\');
    if (!slash || (bslash && bslash > slash)) {
        slash = bslash;
    }
#endif

    if (!slash) {
        /* No directory component; use current directory. */
        return jz_strdup(".");
    }

    size_t len = (size_t)(slash - filepath);
    if (len == 0) len = 1; /* root "/" */
    char *dir = (char *)malloc(len + 1);
    if (!dir) return NULL;
    memcpy(dir, filepath, len);
    dir[len] = '\0';
    return dir;
}

/**
 * @brief Canonicalize a path using realpath (POSIX) or _fullpath (Win).
 * @return Heap-allocated canonical path, or NULL on failure.
 */
static char *resolve_path(const char *path)
{
    if (!path) return NULL;
#ifdef _WIN32
    char buf[_MAX_PATH];
    if (_fullpath(buf, path, sizeof(buf))) {
        return jz_strdup(buf);
    }
    return NULL;
#else
    char *resolved = realpath(path, NULL);
    return resolved; /* already heap-allocated */
#endif
}

/**
 * @brief Textual path normalization (collapse ., .., //).
 *
 * Used as fallback when realpath() fails (file doesn't exist yet).
 * Operates purely on the string; no filesystem access.
 *
 * @param path Input path.
 * @return Heap-allocated normalized path, or NULL.
 */
static char *normalize_path_textual(const char *path)
{
    if (!path || !*path) return NULL;

    size_t len = strlen(path);
    char *buf = (char *)malloc(len + 1);
    if (!buf) return NULL;
    memcpy(buf, path, len + 1);

    /* Split into components. */
    char *components[256];
    size_t comp_count = 0;
    int is_absolute = (buf[0] == '/');

    char *p = buf;
    if (is_absolute) p++; /* skip leading / */

    while (*p) {
        /* Skip consecutive slashes. */
        while (*p == '/') p++;
        if (!*p) break;

        char *start = p;
        while (*p && *p != '/') p++;
        if (*p) { *p = '\0'; p++; }

        if (strcmp(start, ".") == 0) {
            continue; /* skip . */
        } else if (strcmp(start, "..") == 0) {
            if (comp_count > 0 && strcmp(components[comp_count - 1], "..") != 0) {
                comp_count--;
            } else if (!is_absolute) {
                if (comp_count < 256) components[comp_count++] = start;
            }
            /* For absolute paths, going above root is a no-op. */
        } else {
            if (comp_count < 256) components[comp_count++] = start;
        }
    }

    /* Reconstruct path. */
    char *result = (char *)malloc(len + 2);
    if (!result) { free(buf); return NULL; }

    size_t pos = 0;
    if (is_absolute) {
        result[pos++] = '/';
    }
    for (size_t i = 0; i < comp_count; i++) {
        if (i > 0) result[pos++] = '/';
        size_t clen = strlen(components[i]);
        memcpy(result + pos, components[i], clen);
        pos += clen;
    }
    if (pos == 0) {
        result[pos++] = '.';
    }
    result[pos] = '\0';

    free(buf);
    return result;
}

/**
 * @brief Check if a path starts with a given root prefix.
 *
 * Ensures the match is at a directory boundary (root ends with /,
 * or the character after the prefix in path is / or NUL).
 */
static int path_is_under_root(const char *path, const char *root)
{
    if (!path || !root) return 0;

    size_t root_len = strlen(root);
    /* Strip trailing slash from root for comparison. */
    while (root_len > 1 && root[root_len - 1] == '/') {
        root_len--;
    }

    if (strncmp(path, root, root_len) != 0) return 0;

    /* path must be exactly root or continue with /. */
    char next = path[root_len];
    return (next == '\0' || next == '/');
}

/**
 * @brief Check if raw path contains '..' components.
 */
static int path_has_traversal(const char *path)
{
    if (!path) return 0;

    const char *p = path;
    while (*p) {
        /* Check for ".." at start of component. */
        int at_start = (p == path || *(p - 1) == '/');
        if (at_start && p[0] == '.' && p[1] == '.') {
            char after = p[2];
            if (after == '\0' || after == '/' || after == '\\') {
                return 1;
            }
        }
        p++;
    }
    return 0;
}

/**
 * @brief Report a path security diagnostic.
 */
static void path_report(JZDiagnosticList *diag, JZLocation loc,
                         const char *rule_id, const char *fallback)
{
    if (!diag || !rule_id) return;

    const JZRuleInfo *rule = jz_rule_lookup(rule_id);
    JZSeverity sev = JZ_SEVERITY_ERROR;
    const char *msg = fallback;

    if (rule) {
        switch (rule->mode) {
        case JZ_RULE_MODE_WRN: sev = JZ_SEVERITY_WARNING; break;
        case JZ_RULE_MODE_INF: sev = JZ_SEVERITY_NOTE; break;
        case JZ_RULE_MODE_ERR:
        default: sev = JZ_SEVERITY_ERROR; break;
        }
        if (rule->description) msg = rule->description;
    }
    if (!msg) msg = rule_id;

    jz_diagnostic_report(diag, loc, sev, rule_id, msg);
}

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

void jz_path_security_init(const char *project_filename)
{
    /* Derive default sandbox root from the project file directory. */
    char *dir = path_dirname(project_filename);
    if (dir) {
        char *resolved = resolve_path(dir);
        if (resolved) {
            free(dir);
            g_default_sandbox_root = resolved;
        } else {
            /* realpath may fail if path doesn't exist; use textual. */
            char *norm = normalize_path_textual(dir);
            free(dir);
            g_default_sandbox_root = norm;
        }
    }
}

void jz_path_security_add_root(const char *dir)
{
    if (!dir || g_sandbox_root_count >= MAX_SANDBOX_ROOTS) return;

    char *resolved = resolve_path(dir);
    if (!resolved) {
        resolved = normalize_path_textual(dir);
    }
    if (resolved) {
        g_sandbox_roots[g_sandbox_root_count++] = resolved;
    }
}

void jz_path_security_set_allow_absolute(int allow)
{
    g_allow_absolute = allow;
}

void jz_path_security_set_allow_traversal(int allow)
{
    g_allow_traversal = allow;
}

char *jz_path_validate(const char *raw_path,
                        const char *base_dir,
                        JZLocation loc,
                        JZDiagnosticList *diag)
{
    if (!raw_path || !*raw_path) return NULL;

    /* Step 1: Check for absolute path. */
    int is_absolute = (raw_path[0] == '/');
#ifdef _WIN32
    is_absolute = is_absolute || (raw_path[0] != '\0' && raw_path[1] == ':');
#endif
    if (is_absolute && !g_allow_absolute) {
        path_report(diag, loc, "PATH_ABSOLUTE_FORBIDDEN",
                    "absolute path used without --allow-absolute-paths");
        return NULL;
    }

    /* Step 2: Check for '..' traversal. */
    if (path_has_traversal(raw_path) && !g_allow_traversal) {
        path_report(diag, loc, "PATH_TRAVERSAL_FORBIDDEN",
                    "path contains '..' traversal without --allow-traversal");
        return NULL;
    }

    /* Step 3: Build full path. */
    char *full_path = NULL;
    if (is_absolute) {
        full_path = jz_strdup(raw_path);
    } else if (base_dir && *base_dir) {
        size_t dir_len = strlen(base_dir);
        size_t path_len = strlen(raw_path);
        /* Ensure a separator between dir and path. */
        int need_sep = (dir_len > 0 && base_dir[dir_len - 1] != '/');
        full_path = (char *)malloc(dir_len + need_sep + path_len + 1);
        if (full_path) {
            memcpy(full_path, base_dir, dir_len);
            if (need_sep) full_path[dir_len] = '/';
            memcpy(full_path + dir_len + need_sep, raw_path, path_len + 1);
        }
    } else {
        full_path = jz_strdup(raw_path);
    }
    if (!full_path) return NULL;

    /* Step 4+5: Canonicalize. Try realpath first, fall back to textual. */
    char *canonical = resolve_path(full_path);
    if (!canonical) {
        canonical = normalize_path_textual(full_path);
    }
    free(full_path);
    if (!canonical) return NULL;

    /* Step 6: Check for symlink escape. */
#ifndef _WIN32
    {
        struct stat st;
        if (lstat(canonical, &st) == 0 && S_ISLNK(st.st_mode)) {
            /* canonical came from realpath which follows symlinks,
             * so if lstat says it's a symlink, the original was a symlink.
             * But realpath already resolved it, so we need to check the
             * original non-resolved path for symlink components.
             * Actually, realpath resolves ALL symlinks and returns the
             * final target. We re-check: resolve raw again, if the
             * resolved target differs and is outside sandbox, report. */
        }
        /* For a more practical check: if the file exists and realpath
         * resolved it, we already have the true target. The sandbox
         * check below catches escapes. Symlink-specific diagnostic is
         * emitted when lstat of the original path shows a symlink AND
         * the resolved path is outside the sandbox. We handle this in
         * the sandbox check below with a more specific message. */
    }
#endif

    /* Step 7: Check resolved path against sandbox roots. */
    int in_sandbox = 0;

    /* Check default root first. */
    if (g_default_sandbox_root && path_is_under_root(canonical, g_default_sandbox_root)) {
        in_sandbox = 1;
    }

    /* Check additional roots. */
    if (!in_sandbox) {
        for (size_t i = 0; i < g_sandbox_root_count; i++) {
            if (path_is_under_root(canonical, g_sandbox_roots[i])) {
                in_sandbox = 1;
                break;
            }
        }
    }

    if (!in_sandbox) {
        /* Determine if this is a symlink escape or general sandbox violation. */
#ifndef _WIN32
        {
            /* Build the full (non-resolved) path again for lstat check. */
            char *unresolv = NULL;
            if (base_dir && *base_dir && !is_absolute) {
                size_t dlen = strlen(base_dir);
                size_t plen = strlen(raw_path);
                int nsep = (dlen > 0 && base_dir[dlen - 1] != '/');
                unresolv = (char *)malloc(dlen + nsep + plen + 1);
                if (unresolv) {
                    memcpy(unresolv, base_dir, dlen);
                    if (nsep) unresolv[dlen] = '/';
                    memcpy(unresolv + dlen + nsep, raw_path, plen + 1);
                }
            }
            if (unresolv) {
                struct stat ust;
                if (lstat(unresolv, &ust) == 0 && S_ISLNK(ust.st_mode)) {
                    free(unresolv);
                    free(canonical);
                    path_report(diag, loc, "PATH_SYMLINK_ESCAPE",
                                "symbolic link resolves to target outside sandbox root");
                    return NULL;
                }
                free(unresolv);
            }
        }
#endif
        free(canonical);
        path_report(diag, loc, "PATH_OUTSIDE_SANDBOX",
                    "resolved path falls outside all permitted sandbox roots");
        return NULL;
    }

    /* Step 8: Return canonical path. */
    return canonical;
}

void jz_path_security_cleanup(void)
{
    for (size_t i = 0; i < g_sandbox_root_count; i++) {
        free(g_sandbox_roots[i]);
        g_sandbox_roots[i] = NULL;
    }
    g_sandbox_root_count = 0;

    free(g_default_sandbox_root);
    g_default_sandbox_root = NULL;

    g_allow_absolute = 0;
    g_allow_traversal = 0;
}
