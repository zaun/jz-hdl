/**
 * @file lsp_internal.h
 * @brief Internal declarations shared across LSP implementation files.
 */

#ifndef JZ_HDL_LSP_INTERNAL_H
#define JZ_HDL_LSP_INTERNAL_H

#include <stddef.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/*  JSON builder                                                      */
/* ------------------------------------------------------------------ */

/**
 * @struct LspJson
 * @brief Growable string buffer for building JSON output.
 */
typedef struct LspJson {
    char  *data;
    size_t len;
    size_t cap;
} LspJson;

void lsp_json_init(LspJson *j);
void lsp_json_free(LspJson *j);
void lsp_json_append(LspJson *j, const char *s);
void lsp_json_append_char(LspJson *j, char c);
void lsp_json_append_int(LspJson *j, int v);
void lsp_json_append_escaped(LspJson *j, const char *s);

/* ------------------------------------------------------------------ */
/*  JSON-RPC I/O                                                      */
/* ------------------------------------------------------------------ */

/**
 * @brief Read one JSON-RPC message from stdin.
 * @param out_len If non-NULL, receives the length of the returned string.
 * @return Heap-allocated JSON body (caller frees), or NULL on EOF/error.
 */
char *lsp_io_read_message(size_t *out_len);

/**
 * @brief Write a JSON-RPC message to stdout with Content-Length header.
 * @param json The JSON body to send.
 * @param len  Length of the JSON body in bytes.
 */
void lsp_io_write_message(const char *json, size_t len);

/**
 * @brief Log a message to stderr (for debugging).
 */
void lsp_log(const char *fmt, ...);

/* ------------------------------------------------------------------ */
/*  Minimal JSON token reader (using bundled jsmn)                    */
/* ------------------------------------------------------------------ */

/**
 * @brief Extract a string value for a given key from a JSON object.
 * @param json The raw JSON string.
 * @param key  The key to look up.
 * @param out  Buffer to write the value into.
 * @param out_cap Capacity of the output buffer.
 * @return 0 on success, -1 if not found or not a string.
 */
int lsp_json_get_string(const char *json, const char *key,
                        char *out, size_t out_cap);

/**
 * @brief Extract an integer value for a given key from a JSON object.
 * @param json The raw JSON string.
 * @param key  The key to look up.
 * @param out  Pointer to receive the integer value.
 * @return 0 on success, -1 if not found or not a number.
 */
int lsp_json_get_int(const char *json, const char *key, int *out);

/**
 * @brief Extract a nested JSON object substring for a given key.
 * @param json The raw JSON string.
 * @param key  The key to look up.
 * @param out  Buffer to write the nested JSON into.
 * @param out_cap Capacity of the output buffer.
 * @return 0 on success, -1 if not found.
 */
int lsp_json_get_object(const char *json, const char *key,
                        char *out, size_t out_cap);

/* ------------------------------------------------------------------ */
/*  Document store                                                    */
/* ------------------------------------------------------------------ */

#define LSP_MAX_DOCUMENTS 128

typedef struct LspDocument {
    char *uri;      /* Document URI (heap-allocated). */
    char *content;  /* Full document text (heap-allocated). */
    int   version;  /* Client-reported version number. */
} LspDocument;

typedef struct LspDocStore {
    LspDocument docs[LSP_MAX_DOCUMENTS];
    size_t      count;
} LspDocStore;

void lsp_docstore_init(LspDocStore *store);
void lsp_docstore_free(LspDocStore *store);
LspDocument *lsp_docstore_open(LspDocStore *store, const char *uri,
                               const char *content, int version);
LspDocument *lsp_docstore_find(LspDocStore *store, const char *uri);
void lsp_docstore_update(LspDocument *doc, const char *content, int version);
void lsp_docstore_close(LspDocStore *store, const char *uri);

/* ------------------------------------------------------------------ */
/*  Project discovery                                                 */
/* ------------------------------------------------------------------ */

#define LSP_MAX_PROJECTS 32

/**
 * @struct LspProjectEntry
 * @brief A single discovered project file with its metadata.
 */
typedef struct LspProjectEntry {
    char file[2048];  /* Absolute path to the project .jz file. */
    char chip[256];   /* CHIP parameter (may be empty). */
    char name[256];   /* Project name (may be empty). */
} LspProjectEntry;

/**
 * @struct LspProjectList
 * @brief Collection of discovered project files.
 */
typedef struct LspProjectList {
    LspProjectEntry entries[LSP_MAX_PROJECTS];
    size_t          count;
} LspProjectList;

/**
 * @brief Discover all project files associated with a source file.
 *
 * Searches for @project files using cached .jzhdl-lsp.rc files and
 * directory scanning.  See lsp_project_discovery.c for the full algorithm.
 *
 * @param filepath        Absolute path to the source file being edited.
 * @param workspace_root  Workspace root path from the LSP initialize request.
 * @param is_project_file Non-zero if the file itself contains @project.
 * @param source_content  If non-NULL, use this as the file content instead
 *                        of reading from disk (for in-memory buffers).
 * @param out             Output list to receive discovered projects.
 * @return 0 if at least one project was found, -1 otherwise.
 */
int lsp_discover_projects(const char *filepath,
                          const char *workspace_root,
                          int is_project_file,
                          const char *source_content,
                          LspProjectList *out);

/**
 * @brief Find which project in a list imports a given source file.
 *
 * Scans each project file's @import directives to see which one
 * references the given filepath.
 *
 * @param projects  List of discovered projects.
 * @param filepath  Absolute path to the source file being edited.
 * @return Index into projects->entries, or -1 if none imports the file.
 */
int lsp_find_project_for_file(const LspProjectList *projects,
                              const char *filepath);

/* ------------------------------------------------------------------ */
/*  URI helpers                                                       */
/* ------------------------------------------------------------------ */

/**
 * @brief Convert a file:// URI to a local filesystem path.
 * @param uri The URI string.
 * @param out Buffer to write the path.
 * @param out_cap Capacity of the output buffer.
 * @return 0 on success, -1 on error.
 */
int lsp_uri_to_path(const char *uri, char *out, size_t out_cap);

#endif /* JZ_HDL_LSP_INTERNAL_H */
