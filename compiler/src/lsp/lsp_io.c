/**
 * @file lsp_io.c
 * @brief JSON-RPC message I/O over stdio for the LSP server.
 */

#include "lsp/lsp_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

void lsp_log(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[jz-hdl-lsp] ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    fflush(stderr);
    va_end(ap);
}

char *lsp_io_read_message(size_t *out_len) {
    /* Read headers until we find Content-Length and a blank line. */
    int content_length = -1;

    for (;;) {
        char header[256];
        if (!fgets(header, sizeof(header), stdin)) {
            return NULL; /* EOF */
        }

        /* Blank line (just \r\n or \n) marks end of headers. */
        if (strcmp(header, "\r\n") == 0 || strcmp(header, "\n") == 0) {
            break;
        }

        if (strncmp(header, "Content-Length:", 15) == 0) {
            content_length = atoi(header + 15);
        }
        /* Ignore other headers (e.g., Content-Type). */
    }

    if (content_length < 0) {
        return NULL;
    }

    char *body = malloc((size_t)content_length + 1);
    if (!body) return NULL;

    size_t total = 0;
    while (total < (size_t)content_length) {
        size_t n = fread(body + total, 1, (size_t)content_length - total, stdin);
        if (n == 0) {
            free(body);
            return NULL; /* EOF or error */
        }
        total += n;
    }
    body[content_length] = '\0';

    if (out_len) *out_len = (size_t)content_length;
    return body;
}

void lsp_io_write_message(const char *json, size_t len) {
    fprintf(stdout, "Content-Length: %zu\r\n\r\n", len);
    fwrite(json, 1, len, stdout);
    fflush(stdout);
}

/* ------------------------------------------------------------------ */
/*  URI helpers                                                       */
/* ------------------------------------------------------------------ */

static int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int lsp_uri_to_path(const char *uri, char *out, size_t out_cap) {
    /* Expect file:///path or file://localhost/path. */
    if (strncmp(uri, "file://", 7) != 0) return -1;
    const char *p = uri + 7;
    /* Skip optional localhost. */
    if (strncmp(p, "localhost", 9) == 0) p += 9;
    /* On unix, path starts with /. On windows, skip the leading /
     * before the drive letter (e.g., /C:/...). We handle unix only. */

    size_t i = 0;
    while (*p && i + 1 < out_cap) {
        if (*p == '%' && p[1] && p[2]) {
            int h = hex_digit(p[1]);
            int l = hex_digit(p[2]);
            if (h >= 0 && l >= 0) {
                out[i++] = (char)(h * 16 + l);
                p += 3;
                continue;
            }
        }
        out[i++] = *p++;
    }
    out[i] = '\0';
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Document store                                                    */
/* ------------------------------------------------------------------ */

void lsp_docstore_init(LspDocStore *store) {
    memset(store, 0, sizeof(*store));
}

void lsp_docstore_free(LspDocStore *store) {
    for (size_t i = 0; i < store->count; ++i) {
        free(store->docs[i].uri);
        free(store->docs[i].content);
    }
    store->count = 0;
}

LspDocument *lsp_docstore_open(LspDocStore *store, const char *uri,
                               const char *content, int version) {
    if (store->count >= LSP_MAX_DOCUMENTS) return NULL;
    LspDocument *doc = &store->docs[store->count++];
    doc->uri = strdup(uri);
    doc->content = strdup(content);
    doc->version = version;
    return doc;
}

LspDocument *lsp_docstore_find(LspDocStore *store, const char *uri) {
    for (size_t i = 0; i < store->count; ++i) {
        if (strcmp(store->docs[i].uri, uri) == 0) {
            return &store->docs[i];
        }
    }
    return NULL;
}

void lsp_docstore_update(LspDocument *doc, const char *content, int version) {
    free(doc->content);
    doc->content = strdup(content);
    doc->version = version;
}

void lsp_docstore_close(LspDocStore *store, const char *uri) {
    for (size_t i = 0; i < store->count; ++i) {
        if (strcmp(store->docs[i].uri, uri) == 0) {
            free(store->docs[i].uri);
            free(store->docs[i].content);
            /* Shift remaining entries down. */
            for (size_t k = i; k + 1 < store->count; ++k) {
                store->docs[k] = store->docs[k + 1];
            }
            --store->count;
            return;
        }
    }
}
