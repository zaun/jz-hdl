/**
 * @file lsp_project_discovery.c
 * @brief Project file discovery for the JZ-HDL LSP server.
 *
 * When the LSP opens a standalone module file (one without @project), it
 * needs to find the associated project file so that cross-file definitions
 * (BUS types, globals, etc.) are available during semantic analysis.
 *
 * The .jzhdl-lsp.rc file lists all discovered project files in the format:
 *   <absolute-path> <CHIP> <PROJECT_NAME>
 * One entry per line.  CHIP or PROJECT_NAME may be "-" if not specified.
 *
 * Discovery algorithm:
 *   1. If the open file itself contains @project, write/update
 *      .jzhdl-lsp.rc in its directory with this file's entry.
 *   2. Walk upward from the file's directory toward the workspace root
 *      looking for an existing .jzhdl-lsp.rc.  If found and valid, use it.
 *   3. Starting from the file's directory, scan peer files for @project.
 *      If any found, create .jzhdl-lsp.rc at the current search level.
 *   4. Scan immediate subdirectories for @project.  If any found,
 *      create .jzhdl-lsp.rc at the current search level.
 *   5. Move up one directory (if not at workspace root) and repeat 3-4.
 *   6. The .jzhdl-lsp.rc is always placed in the highest directory
 *      reached during the search.
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
 * @brief Extract CHIP and PROJECT_NAME from a @project directive line.
 *
 * Handles both forms:
 *   @project(CHIP="...") NAME
 *   @project NAME
 */
static void extract_project_metadata(const char *content,
                                     char *chip, size_t chip_cap,
                                     char *name, size_t name_cap)
{
    chip[0] = '\0';
    name[0] = '\0';

    const char *p = content;
    while ((p = strstr(p, "@project")) != NULL) {
        /* Verify it's at start or preceded by whitespace. */
        if (p != content && p[-1] != '\n' && p[-1] != '\r' &&
            p[-1] != ' ' && p[-1] != '\t') {
            p += 8;
            continue;
        }
        char after = p[8];
        if (after != '\0' && after != '(' && after != ' ' &&
            after != '\t' && after != '\n' && after != '\r') {
            p += 8;
            continue;
        }

        /* Found @project — advance past it. */
        p += 8;

        /* Skip whitespace. */
        while (*p == ' ' || *p == '\t') p++;

        /* Check for (CHIP="...") */
        if (*p == '(') {
            p++;
            const char *chip_start = strstr(p, "CHIP=\"");
            if (chip_start && chip_start < strchr(p, ')')) {
                chip_start += 6; /* skip CHIP=" */
                const char *chip_end = strchr(chip_start, '"');
                if (chip_end) {
                    size_t len = (size_t)(chip_end - chip_start);
                    if (len >= chip_cap) len = chip_cap - 1;
                    memcpy(chip, chip_start, len);
                    chip[len] = '\0';
                }
            }
            /* Skip past closing paren. */
            const char *close = strchr(p, ')');
            if (close) p = close + 1;
        }

        /* Skip whitespace to find project name. */
        while (*p == ' ' || *p == '\t') p++;

        /* Read the project name (until whitespace or newline). */
        if (*p && *p != '\n' && *p != '\r') {
            const char *name_start = p;
            while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r')
                p++;
            size_t len = (size_t)(p - name_start);
            if (len >= name_cap) len = name_cap - 1;
            memcpy(name, name_start, len);
            name[len] = '\0';
        }

        return; /* Use first @project found. */
    }
}

/**
 * @brief Check whether a .jz file contains @project and extract metadata.
 * @return 1 if @project found, 0 otherwise.
 */
static int file_get_project_info(const char *path,
                                 char *chip, size_t chip_cap,
                                 char *name, size_t name_cap)
{
    size_t size = 0;
    char *content = jz_read_entire_file(path, &size);
    if (!content) return 0;

    chip[0] = '\0';
    name[0] = '\0';

    /* Quick check: does it contain "@project" at all? */
    if (!strstr(content, "@project")) {
        free(content);
        return 0;
    }

    extract_project_metadata(content, chip, chip_cap, name, name_cap);

    free(content);
    /* If we got a name, we found it.  If chip is empty that's okay
     * (@project without CHIP= is valid). */
    return (name[0] != '\0' || chip[0] != '\0') ? 1 : 0;
}

/**
 * @brief Extract the directory portion of a file path.
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
 * @brief Canonicalize a path via realpath, falling back to the input.
 */
static void canonicalize_path(const char *path, char *out, size_t out_cap)
{
    char *real = realpath(path, NULL);
    if (real) {
        strncpy(out, real, out_cap - 1);
        out[out_cap - 1] = '\0';
        free(real);
    } else {
        strncpy(out, path, out_cap - 1);
        out[out_cap - 1] = '\0';
    }
}

/**
 * @brief Add a project entry to the list if not already present.
 */
static void add_project_entry(LspProjectList *list,
                              const char *file, const char *chip,
                              const char *name)
{
    if (list->count >= LSP_MAX_PROJECTS) return;

    /* Deduplicate by file path. */
    for (size_t i = 0; i < list->count; i++) {
        if (strcmp(list->entries[i].file, file) == 0) return;
    }

    LspProjectEntry *e = &list->entries[list->count++];
    strncpy(e->file, file, sizeof(e->file) - 1);
    e->file[sizeof(e->file) - 1] = '\0';
    strncpy(e->chip, (chip && chip[0]) ? chip : "-", sizeof(e->chip) - 1);
    e->chip[sizeof(e->chip) - 1] = '\0';
    strncpy(e->name, (name && name[0]) ? name : "-", sizeof(e->name) - 1);
    e->name[sizeof(e->name) - 1] = '\0';
}

/* ------------------------------------------------------------------ */
/*  RC file I/O                                                       */
/* ------------------------------------------------------------------ */

/**
 * @brief Read all project entries from a .jzhdl-lsp.rc file.
 * @return 0 on success, -1 if file doesn't exist or is empty.
 */
static int read_rc_file(const char *rc_path, LspProjectList *out)
{
    size_t size = 0;
    char *content = jz_read_entire_file(rc_path, &size);
    if (!content || size == 0) {
        free(content);
        return -1;
    }

    out->count = 0;

    /* Parse line by line: <file> <chip> <name> */
    char *line = content;
    while (line && *line) {
        char *eol = strchr(line, '\n');
        if (eol) *eol = '\0';

        /* Skip empty lines. */
        if (line[0] == '\0' || line[0] == '#') {
            line = eol ? eol + 1 : NULL;
            continue;
        }

        /* Parse three space-separated fields. */
        char file_field[2048] = {0};
        char chip_field[256] = {0};
        char name_field[256] = {0};

        const char *p = line;

        /* Field 1: file path (may not contain spaces in practice). */
        const char *f_start = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        {
            size_t len = (size_t)(p - f_start);
            if (len >= sizeof(file_field)) len = sizeof(file_field) - 1;
            memcpy(file_field, f_start, len);
            file_field[len] = '\0';
        }

        /* Skip whitespace. */
        while (*p == ' ' || *p == '\t') p++;

        /* Field 2: chip. */
        const char *c_start = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        {
            size_t len = (size_t)(p - c_start);
            if (len >= sizeof(chip_field)) len = sizeof(chip_field) - 1;
            memcpy(chip_field, c_start, len);
            chip_field[len] = '\0';
        }

        /* Skip whitespace. */
        while (*p == ' ' || *p == '\t') p++;

        /* Field 3: project name. */
        const char *n_start = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') p++;
        {
            size_t len = (size_t)(p - n_start);
            if (len >= sizeof(name_field)) len = sizeof(name_field) - 1;
            memcpy(name_field, n_start, len);
            name_field[len] = '\0';
        }

        if (file_field[0]) {
            add_project_entry(out, file_field, chip_field, name_field);
        }

        line = eol ? eol + 1 : NULL;
    }

    free(content);
    return (out->count > 0) ? 0 : -1;
}

/**
 * @brief Write a .jzhdl-lsp.rc file containing all project entries.
 */
static void write_rc_file(const char *dir, const LspProjectList *projects)
{
    char rc_path[2048];
    snprintf(rc_path, sizeof(rc_path), "%s/%s", dir, RC_FILENAME);

    FILE *f = fopen(rc_path, "w");
    if (!f) {
        lsp_log("failed to write %s", rc_path);
        return;
    }

    for (size_t i = 0; i < projects->count; i++) {
        const LspProjectEntry *e = &projects->entries[i];
        fprintf(f, "%s %s %s\n", e->file, e->chip, e->name);
    }

    fclose(f);
    lsp_log("wrote %s with %zu project(s)", rc_path, projects->count);
}

/* ------------------------------------------------------------------ */
/*  Directory scanning                                                */
/* ------------------------------------------------------------------ */

/**
 * @brief Check if a path is at or above the workspace root.
 */
static int is_at_or_above_workspace(const char *dir, const char *workspace_root)
{
    if (!workspace_root || !workspace_root[0]) return 0;

    size_t dir_len = strlen(dir);
    size_t ws_len = strlen(workspace_root);

    if (dir_len < ws_len) {
        if (strncmp(dir, workspace_root, dir_len) == 0 &&
            (workspace_root[dir_len] == '/' || workspace_root[dir_len] == '\0')) {
            return 1;
        }
    }

    return 0;
}

/**
 * @brief Scan a directory for .jz files that contain @project.
 *
 * All found projects are added to the list.
 *
 * @return Number of project files found in this directory.
 */
static int scan_dir_for_projects(const char *dir, LspProjectList *list,
                                 const char *exclude_file)
{
    DIR *d = opendir(dir);
    if (!d) return 0;

    int count = 0;
    struct dirent *entry;

    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        size_t nlen = strlen(entry->d_name);
        if (nlen < 3 || strcmp(entry->d_name + nlen - 3, ".jz") != 0) continue;

        char fullpath[2048];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, entry->d_name);

        struct stat st;
        if (stat(fullpath, &st) != 0 || !S_ISREG(st.st_mode)) continue;

        /* Canonicalize for comparison. */
        char canonical[2048];
        canonicalize_path(fullpath, canonical, sizeof(canonical));

        if (exclude_file && strcmp(canonical, exclude_file) == 0) continue;

        char chip[256], name[256];
        if (file_get_project_info(fullpath, chip, sizeof(chip),
                                  name, sizeof(name))) {
            add_project_entry(list, canonical, chip, name);
            count++;
        }
    }

    closedir(d);
    return count;
}

/**
 * @brief Scan immediate subdirectories for .jz project files.
 * @return Number of project files found across all subdirectories.
 */
static int scan_subdirs_for_projects(const char *dir, LspProjectList *list,
                                     const char *exclude_file)
{
    DIR *d = opendir(dir);
    if (!d) return 0;

    int total = 0;
    struct dirent *entry;

    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char subdir[2048];
        snprintf(subdir, sizeof(subdir), "%s/%s", dir, entry->d_name);

        struct stat st;
        if (stat(subdir, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        total += scan_dir_for_projects(subdir, list, exclude_file);
    }

    closedir(d);
    return total;
}

/* ------------------------------------------------------------------ */
/*  Validate RC entries                                               */
/* ------------------------------------------------------------------ */

/**
 * @brief Validate that all entries in an rc-loaded project list still
 *        point to existing files.  Returns 1 if all valid, 0 if any stale.
 */
static int validate_rc_entries(const LspProjectList *list)
{
    for (size_t i = 0; i < list->count; i++) {
        struct stat st;
        if (stat(list->entries[i].file, &st) != 0 || !S_ISREG(st.st_mode)) {
            return 0;
        }
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/*  Import checking                                                   */
/* ------------------------------------------------------------------ */

/**
 * @brief Check if a project file's @import directives reference a given file.
 *
 * Does a text-level scan for @import "..." lines and resolves relative
 * paths against the project file's directory.
 */
static int project_imports_file(const char *project_path,
                                const char *target_file)
{
    size_t size = 0;
    char *content = jz_read_entire_file(project_path, &size);
    if (!content) return 0;

    /* Get the project file's directory for resolving relative imports. */
    char proj_dir[2048];
    extract_directory(project_path, proj_dir, sizeof(proj_dir));

    /* Get the basename of the target file for quick comparison. */
    const char *target_base = strrchr(target_file, '/');
    target_base = target_base ? target_base + 1 : target_file;

    const char *p = content;
    int found = 0;

    while ((p = strstr(p, "@import")) != NULL) {
        p += 7; /* skip "@import" */

        /* Skip whitespace. */
        while (*p == ' ' || *p == '\t') p++;

        if (*p != '"') continue;
        p++; /* skip opening quote */

        const char *path_start = p;
        while (*p && *p != '"' && *p != '\n') p++;
        if (*p != '"') continue;

        size_t path_len = (size_t)(p - path_start);
        p++; /* skip closing quote */

        char import_path[2048];
        if (path_len >= sizeof(import_path)) continue;
        memcpy(import_path, path_start, path_len);
        import_path[path_len] = '\0';

        /* Quick check: does the basename match? */
        const char *import_base = strrchr(import_path, '/');
        import_base = import_base ? import_base + 1 : import_path;

        if (strcmp(import_base, target_base) != 0) continue;

        /* Resolve the full path and compare. */
        char resolved[2048];
        if (import_path[0] == '/') {
            canonicalize_path(import_path, resolved, sizeof(resolved));
        } else {
            char joined[2048];
            snprintf(joined, sizeof(joined), "%s/%s", proj_dir, import_path);
            canonicalize_path(joined, resolved, sizeof(resolved));
        }

        if (strcmp(resolved, target_file) == 0) {
            found = 1;
            break;
        }
    }

    free(content);
    return found;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */

int lsp_discover_projects(const char *filepath,
                          const char *workspace_root,
                          int is_project_file,
                          const char *source_content,
                          LspProjectList *out)
{
    if (!filepath || !out) return -1;
    out->count = 0;

    char file_dir[2048];
    extract_directory(filepath, file_dir, sizeof(file_dir));

    char canonical_file[2048];
    canonicalize_path(filepath, canonical_file, sizeof(canonical_file));

    /* Step 1: If this file IS a project file, extract its info and
     * write/update the rc file. */
    if (is_project_file) {
        char chip[256], name[256];
        chip[0] = '\0';
        name[0] = '\0';

        if (source_content) {
            /* Use the in-memory content (most up-to-date). */
            extract_project_metadata(source_content, chip, sizeof(chip),
                                     name, sizeof(name));
        } else {
            /* Fall back to reading from disk. */
            size_t fsize = 0;
            char *content = jz_read_entire_file(filepath, &fsize);
            if (content) {
                extract_project_metadata(content, chip, sizeof(chip),
                                         name, sizeof(name));
                free(content);
            }
        }
        add_project_entry(out, canonical_file, chip, name);

        /* Also scan peers and subdirs to find sibling projects. */
        scan_dir_for_projects(file_dir, out, canonical_file);

        write_rc_file(file_dir, out);
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

            LspProjectList rc_list;
            rc_list.count = 0;
            if (read_rc_file(rc_path, &rc_list) == 0 && rc_list.count > 0) {
                if (validate_rc_entries(&rc_list)) {
                    lsp_log("found valid rc at %s with %zu project(s)",
                            rc_path, rc_list.count);
                    *out = rc_list;
                    return 0;
                }
                /* Stale rc — remove and continue searching. */
                lsp_log("stale rc at %s, removing", rc_path);
                remove(rc_path);
            }

            if (workspace_root && workspace_root[0] &&
                strcmp(search_dir, workspace_root) == 0) {
                break;
            }

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
            LspProjectList found;
            found.count = 0;

            /* Step 3: Scan peer .jz files in this directory. */
            scan_dir_for_projects(search_dir, &found, canonical_file);

            /* Step 4: Scan immediate subdirectories. */
            scan_subdirs_for_projects(search_dir, &found, canonical_file);

            if (found.count > 0) {
                lsp_log("discovered %zu project(s) from %s",
                        found.count, search_dir);
                write_rc_file(search_dir, &found);
                *out = found;
                return 0;
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

    lsp_log("no project files found for %s", filepath);
    return -1;
}

int lsp_find_project_for_file(const LspProjectList *projects,
                              const char *filepath)
{
    if (!projects || !filepath || projects->count == 0) return -1;

    char canonical[2048];
    canonicalize_path(filepath, canonical, sizeof(canonical));

    /* If there's only one project, use it without checking imports. */
    if (projects->count == 1) return 0;

    /* Check which project(s) import this file. */
    for (size_t i = 0; i < projects->count; i++) {
        if (project_imports_file(projects->entries[i].file, canonical)) {
            return (int)i;
        }
    }

    /* No project imports this file — could be a standalone file. */
    return -1;
}
