/**
 * @file lsp_project_discovery.c
 * @brief Project file discovery for the JZ-HDL LSP server.
 *
 * When the LSP opens a standalone module file (one without @project), it
 * needs to find the associated project file so that cross-file definitions
 * (BUS types, globals, etc.) are available during semantic analysis.
 *
 * Discovery algorithm:
 *   1. If the open file itself contains @project, write a .jzhdl-lsp.rc
 *      in its directory pointing to itself.
 *   2. Walk upward from the file's directory toward the workspace root
 *      looking for an existing .jzhdl-lsp.rc.  If found and valid, use it.
 *   3. Starting from the file's directory, scan peer files for @project.
 *      If found, create .jzhdl-lsp.rc at the current search level.
 *   4. Scan immediate subdirectories for @project.  If exactly one is
 *      found, create .jzhdl-lsp.rc at the current search level.
 *   5. Move up one directory (if not at workspace root) and repeat 3-4.
 *   6. The .jzhdl-lsp.rc is always placed in the highest directory
 *      reached during the search (i.e. the directory level where we
 *      started that iteration of steps 3-4).
 */

#include "lsp/lsp_internal.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#ifndef _WIN32
#include <limits.h>
#include <unistd.h>
#endif

#define RC_FILENAME ".jzhdl-lsp.rc"

/* ------------------------------------------------------------------ */
/*  Helpers                                                           */
/* ------------------------------------------------------------------ */

/**
 * @brief Check whether a .jz file contains an @project directive.
 *
 * This does a quick text scan rather than a full parse — it looks for
 * the token "@project" at the start of a line (ignoring leading
 * whitespace).  This is intentionally conservative; a commented-out
 * @project or one inside a string won't match unless it sits at line
 * start, but that's acceptable for discovery purposes.
 */
static int file_contains_project(const char *path)
{
    size_t size = 0;
    char *content = jz_read_entire_file(path, &size);
    if (!content) return 0;

    /* Look for "@project" in the content. We check that it appears
     * either at the very start or after a newline, so we don't match
     * random substrings in comments/strings. */
    const char *p = content;
    int found = 0;
    while ((p = strstr(p, "@project")) != NULL) {
        /* Verify it's at start of content or preceded by whitespace/newline. */
        if (p == content || p[-1] == '\n' || p[-1] == '\r' ||
            p[-1] == ' ' || p[-1] == '\t') {
            /* Verify the character after "@project" is not alphanumeric
             * (to avoid matching e.g. "@project_foo"). */
            char after = p[8];
            if (after == '\0' || after == '(' || after == ' ' ||
                after == '\t' || after == '\n' || after == '\r') {
                found = 1;
                break;
            }
        }
        p += 8;
    }

    free(content);
    return found;
}

/**
 * @brief Extract the directory portion of a file path.
 * @param filepath Full file path.
 * @param dir      Output buffer for the directory.
 * @param dir_cap  Capacity of the output buffer.
 */
static void extract_directory(const char *filepath, char *dir, size_t dir_cap)
{
    const char *slash = strrchr(filepath, '/');
    if (slash) {
        size_t len = (size_t)(slash - filepath);
        if (len >= dir_cap) len = dir_cap - 1;
        memcpy(dir, filepath, len);
        dir[len] = '\0';
    } else {
        strncpy(dir, ".", dir_cap - 1);
        dir[dir_cap - 1] = '\0';
    }
}

/**
 * @brief Read the project file path from a .jzhdl-lsp.rc file.
 * @param rc_path  Path to the .jzhdl-lsp.rc file.
 * @param out      Output buffer for the project file path.
 * @param out_cap  Capacity of the output buffer.
 * @return 0 on success, -1 if the file doesn't exist or is empty.
 */
static int read_rc_file(const char *rc_path, char *out, size_t out_cap)
{
    size_t size = 0;
    char *content = jz_read_entire_file(rc_path, &size);
    if (!content || size == 0) {
        free(content);
        return -1;
    }

    /* Strip trailing whitespace/newlines. */
    while (size > 0 && (content[size - 1] == '\n' || content[size - 1] == '\r' ||
                        content[size - 1] == ' '  || content[size - 1] == '\t')) {
        content[--size] = '\0';
    }

    if (size == 0 || size >= out_cap) {
        free(content);
        return -1;
    }

    memcpy(out, content, size + 1);
    free(content);
    return 0;
}

/**
 * @brief Write a .jzhdl-lsp.rc file containing the project file path.
 * @param dir          Directory in which to create the rc file.
 * @param project_path Absolute path to the project file.
 */
static void write_rc_file(const char *dir, const char *project_path)
{
    char rc_path[2048];
    snprintf(rc_path, sizeof(rc_path), "%s/%s", dir, RC_FILENAME);

    FILE *f = fopen(rc_path, "w");
    if (!f) {
        lsp_log("failed to write %s", rc_path);
        return;
    }
    fprintf(f, "%s\n", project_path);
    fclose(f);
    lsp_log("wrote %s -> %s", rc_path, project_path);
}

/**
 * @brief Check if a path is at or above the workspace root.
 *
 * Returns 1 if dir is a prefix of (or equal to) workspace_root,
 * meaning we've gone above the workspace.
 */
static int is_at_or_above_workspace(const char *dir, const char *workspace_root)
{
    if (!workspace_root || !workspace_root[0]) return 0;

    size_t dir_len = strlen(dir);
    size_t ws_len = strlen(workspace_root);

    /* If dir is shorter than workspace root, we've gone above it. */
    if (dir_len < ws_len) {
        /* Check if dir is a prefix of workspace_root. */
        if (strncmp(dir, workspace_root, dir_len) == 0 &&
            (workspace_root[dir_len] == '/' || workspace_root[dir_len] == '\0')) {
            return 1;
        }
    }

    return 0;
}

/**
 * @brief Scan a directory for .jz files that contain @project.
 * @param dir          Directory to scan.
 * @param result       Output buffer for the found project file path.
 * @param result_cap   Capacity of the output buffer.
 * @param exclude_file File to exclude from scanning (the file being edited).
 * @return Number of project files found (0, 1, or >1).
 *         If 1, result contains the absolute path.
 */
static int scan_dir_for_project(const char *dir, char *result,
                                size_t result_cap, const char *exclude_file)
{
    DIR *d = opendir(dir);
    if (!d) return 0;

    int count = 0;
    struct dirent *entry;

    while ((entry = readdir(d)) != NULL) {
        /* Skip hidden files and directories. */
        if (entry->d_name[0] == '.') continue;

        /* Only look at .jz files. */
        size_t nlen = strlen(entry->d_name);
        if (nlen < 3 || strcmp(entry->d_name + nlen - 3, ".jz") != 0) continue;

        char fullpath[2048];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, entry->d_name);

        /* Make sure it's a regular file. */
        struct stat st;
        if (stat(fullpath, &st) != 0 || !S_ISREG(st.st_mode)) continue;

        /* Skip the file being edited. */
        if (exclude_file && strcmp(fullpath, exclude_file) == 0) continue;

        if (file_contains_project(fullpath)) {
            if (count == 0 && result_cap > 0) {
                /* Canonicalize the path. */
                char *real = realpath(fullpath, NULL);
                if (real) {
                    strncpy(result, real, result_cap - 1);
                    result[result_cap - 1] = '\0';
                    free(real);
                } else {
                    strncpy(result, fullpath, result_cap - 1);
                    result[result_cap - 1] = '\0';
                }
            }
            count++;
            /* Keep scanning to detect multiple project files. */
        }
    }

    closedir(d);
    return count;
}

/**
 * @brief Scan immediate subdirectories of a directory for .jz project files.
 * @param dir          Parent directory to scan.
 * @param result       Output buffer for the found project file path.
 * @param result_cap   Capacity of the output buffer.
 * @param exclude_file File to exclude from scanning.
 * @return Number of project files found across all subdirectories.
 *         If 1, result contains the absolute path.
 */
static int scan_subdirs_for_project(const char *dir, char *result,
                                    size_t result_cap, const char *exclude_file)
{
    DIR *d = opendir(dir);
    if (!d) return 0;

    int total_count = 0;
    char first_result[2048] = {0};
    struct dirent *entry;

    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char subdir[2048];
        snprintf(subdir, sizeof(subdir), "%s/%s", dir, entry->d_name);

        struct stat st;
        if (stat(subdir, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        char found[2048] = {0};
        int n = scan_dir_for_project(subdir, found, sizeof(found), exclude_file);
        if (n > 0) {
            if (total_count == 0 && found[0]) {
                strncpy(first_result, found, sizeof(first_result) - 1);
            }
            total_count += n;
        }
    }

    closedir(d);

    if (total_count == 1 && first_result[0] && result_cap > 0) {
        strncpy(result, first_result, result_cap - 1);
        result[result_cap - 1] = '\0';
    }

    return total_count;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */

/**
 * @brief Discover the project file for a given source file.
 *
 * Implements the full discovery algorithm: check self, check rc files
 * walking upward, then scan peers and subdirectories walking upward.
 *
 * @param filepath        Absolute path to the source file being edited.
 * @param workspace_root  Workspace root path (may be empty).
 * @param is_project_file Non-zero if the file itself contains @project.
 * @param out             Output buffer for the project file path.
 * @param out_cap         Capacity of the output buffer.
 * @return 0 if a project file was found, -1 otherwise.
 */
int lsp_discover_project_file(const char *filepath,
                              const char *workspace_root,
                              int is_project_file,
                              char *out, size_t out_cap)
{
    if (!filepath || !out || out_cap == 0) return -1;

    char file_dir[2048];
    extract_directory(filepath, file_dir, sizeof(file_dir));

    /* Canonicalize the file path for consistent comparisons. */
    char canonical_file[2048] = {0};
    {
        char *real = realpath(filepath, NULL);
        if (real) {
            strncpy(canonical_file, real, sizeof(canonical_file) - 1);
            free(real);
        } else {
            strncpy(canonical_file, filepath, sizeof(canonical_file) - 1);
        }
    }

    /* Step 1: If this file IS a project file, write rc and return self. */
    if (is_project_file) {
        write_rc_file(file_dir, canonical_file);
        strncpy(out, canonical_file, out_cap - 1);
        out[out_cap - 1] = '\0';
        return 0;
    }

    /* Step 2: Walk upward looking for .jzhdl-lsp.rc. */
    {
        char search_dir[2048];
        strncpy(search_dir, file_dir, sizeof(search_dir) - 1);
        search_dir[sizeof(search_dir) - 1] = '\0';

        for (;;) {
            char rc_path[2048];
            snprintf(rc_path, sizeof(rc_path), "%s/%s", search_dir, RC_FILENAME);

            char project_path[2048] = {0};
            if (read_rc_file(rc_path, project_path, sizeof(project_path)) == 0 &&
                project_path[0]) {
                /* Validate the project file still exists. */
                struct stat st;
                if (stat(project_path, &st) == 0 && S_ISREG(st.st_mode)) {
                    lsp_log("found rc at %s -> %s", rc_path, project_path);
                    strncpy(out, project_path, out_cap - 1);
                    out[out_cap - 1] = '\0';
                    return 0;
                }
                /* RC points to a missing file — stale, remove it. */
                lsp_log("stale rc at %s (target %s missing), removing",
                        rc_path, project_path);
                remove(rc_path);
            }

            /* Stop if at workspace root or filesystem root. */
            if (workspace_root && workspace_root[0] &&
                strcmp(search_dir, workspace_root) == 0) {
                break;
            }

            /* Move up one directory. */
            char *slash = strrchr(search_dir, '/');
            if (!slash || slash == search_dir) break;
            *slash = '\0';
        }
    }

    /* Steps 3-5: Scan peers, then subdirs, walking upward. */
    {
        char search_dir[2048];
        strncpy(search_dir, file_dir, sizeof(search_dir) - 1);
        search_dir[sizeof(search_dir) - 1] = '\0';

        for (;;) {
            /* Step 3: Scan peer .jz files in this directory. */
            char found[2048] = {0};
            int n = scan_dir_for_project(search_dir, found, sizeof(found),
                                         canonical_file);
            if (n == 1 && found[0]) {
                lsp_log("discovered project file: %s (peers in %s)",
                        found, search_dir);
                write_rc_file(search_dir, found);
                strncpy(out, found, out_cap - 1);
                out[out_cap - 1] = '\0';
                return 0;
            }
            if (n > 1) {
                lsp_log("multiple project files in %s, skipping", search_dir);
                /* Ambiguous — don't create rc, keep searching upward. */
            }

            /* Step 4: Scan immediate subdirectories. */
            if (n == 0) {
                found[0] = '\0';
                n = scan_subdirs_for_project(search_dir, found, sizeof(found),
                                             canonical_file);
                if (n == 1 && found[0]) {
                    lsp_log("discovered project file: %s (subdir of %s)",
                            found, search_dir);
                    write_rc_file(search_dir, found);
                    strncpy(out, found, out_cap - 1);
                    out[out_cap - 1] = '\0';
                    return 0;
                }
                if (n > 1) {
                    lsp_log("multiple project files in subdirs of %s, skipping",
                            search_dir);
                }
            }

            /* Step 5: Move up one directory if not at workspace root. */
            if (workspace_root && workspace_root[0] &&
                strcmp(search_dir, workspace_root) == 0) {
                break;
            }
            if (is_at_or_above_workspace(search_dir, workspace_root)) {
                break;
            }

            char *slash = strrchr(search_dir, '/');
            if (!slash || slash == search_dir) break;
            *slash = '\0';
        }
    }

    lsp_log("no project file found for %s", filepath);
    return -1;
}
