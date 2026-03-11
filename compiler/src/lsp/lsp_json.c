/**
 * @file lsp_json.c
 * @brief Minimal JSON builder and reader for LSP messages.
 */

#include "lsp/lsp_internal.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/*  JSON builder                                                      */
/* ------------------------------------------------------------------ */

void lsp_json_init(LspJson *j) {
    j->data = NULL;
    j->len = 0;
    j->cap = 0;
}

void lsp_json_free(LspJson *j) {
    free(j->data);
    j->data = NULL;
    j->len = 0;
    j->cap = 0;
}

static void lsp_json_grow(LspJson *j, size_t need) {
    if (j->len + need <= j->cap) return;
    size_t new_cap = j->cap ? j->cap * 2 : 256;
    while (new_cap < j->len + need) new_cap *= 2;
    j->data = realloc(j->data, new_cap);
    j->cap = new_cap;
}

void lsp_json_append(LspJson *j, const char *s) {
    size_t n = strlen(s);
    lsp_json_grow(j, n + 1);
    memcpy(j->data + j->len, s, n);
    j->len += n;
    j->data[j->len] = '\0';
}

void lsp_json_append_char(LspJson *j, char c) {
    lsp_json_grow(j, 2);
    j->data[j->len++] = c;
    j->data[j->len] = '\0';
}

void lsp_json_append_int(LspJson *j, int v) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", v);
    lsp_json_append(j, buf);
}

void lsp_json_append_escaped(LspJson *j, const char *s) {
    lsp_json_append_char(j, '"');
    for (const char *p = s; *p; ++p) {
        switch (*p) {
        case '"':  lsp_json_append(j, "\\\""); break;
        case '\\': lsp_json_append(j, "\\\\"); break;
        case '\n': lsp_json_append(j, "\\n"); break;
        case '\r': lsp_json_append(j, "\\r"); break;
        case '\t': lsp_json_append(j, "\\t"); break;
        default:
            if ((unsigned char)*p < 0x20) {
                char esc[8];
                snprintf(esc, sizeof(esc), "\\u%04x", (unsigned char)*p);
                lsp_json_append(j, esc);
            } else {
                lsp_json_append_char(j, *p);
            }
            break;
        }
    }
    lsp_json_append_char(j, '"');
}

/* ------------------------------------------------------------------ */
/*  Minimal JSON reader (hand-rolled, no dependency on jsmn)          */
/* ------------------------------------------------------------------ */

/* Skip whitespace. */
static const char *skip_ws(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
    return p;
}

/* Skip a JSON value (string, number, object, array, true/false/null).
 * Returns pointer past the value, or NULL on error. */
static const char *skip_value(const char *p) {
    p = skip_ws(p);
    if (*p == '"') {
        ++p;
        while (*p && *p != '"') {
            if (*p == '\\') ++p; /* skip escaped char */
            if (*p) ++p;
        }
        if (*p == '"') ++p;
        return p;
    }
    if (*p == '{') {
        int depth = 1;
        ++p;
        while (*p && depth > 0) {
            if (*p == '{') ++depth;
            else if (*p == '}') --depth;
            else if (*p == '"') {
                ++p;
                while (*p && *p != '"') {
                    if (*p == '\\') ++p;
                    if (*p) ++p;
                }
            }
            if (*p) ++p;
        }
        return p;
    }
    if (*p == '[') {
        int depth = 1;
        ++p;
        while (*p && depth > 0) {
            if (*p == '[') ++depth;
            else if (*p == ']') --depth;
            else if (*p == '"') {
                ++p;
                while (*p && *p != '"') {
                    if (*p == '\\') ++p;
                    if (*p) ++p;
                }
            }
            if (*p) ++p;
        }
        return p;
    }
    /* number, true, false, null */
    while (*p && *p != ',' && *p != '}' && *p != ']' &&
           *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
        ++p;
    }
    return p;
}

/* Find a key in the top-level JSON object. Returns pointer to the value
 * start (after the colon), or NULL if not found. */
static const char *find_key(const char *json, const char *key) {
    const char *p = skip_ws(json);
    if (*p != '{') return NULL;
    ++p;

    size_t key_len = strlen(key);

    while (*p) {
        p = skip_ws(p);
        if (*p == '}') return NULL;
        if (*p == ',') { ++p; continue; }
        /* Expect a string key. */
        if (*p != '"') return NULL;
        ++p;
        const char *ks = p;
        while (*p && *p != '"') {
            if (*p == '\\') ++p;
            if (*p) ++p;
        }
        size_t kl = (size_t)(p - ks);
        if (*p == '"') ++p;
        p = skip_ws(p);
        if (*p == ':') ++p;
        p = skip_ws(p);

        if (kl == key_len && memcmp(ks, key, key_len) == 0) {
            return p;
        }

        /* Skip the value. */
        p = skip_value(p);
        if (!p) return NULL;
    }
    return NULL;
}

int lsp_json_get_string(const char *json, const char *key,
                        char *out, size_t out_cap) {
    const char *v = find_key(json, key);
    if (!v) return -1;
    v = skip_ws(v);
    if (*v != '"') return -1;
    ++v;
    size_t i = 0;
    while (*v && *v != '"' && i + 1 < out_cap) {
        if (*v == '\\') {
            ++v;
            if (!*v) break;
            switch (*v) {
            case 'n':  out[i++] = '\n'; break;
            case 'r':  out[i++] = '\r'; break;
            case 't':  out[i++] = '\t'; break;
            case '"':  out[i++] = '"'; break;
            case '\\': out[i++] = '\\'; break;
            case '/':  out[i++] = '/'; break;
            default:   out[i++] = *v; break;
            }
        } else {
            out[i++] = *v;
        }
        ++v;
    }
    out[i] = '\0';
    return 0;
}

int lsp_json_get_int(const char *json, const char *key, int *out) {
    const char *v = find_key(json, key);
    if (!v) return -1;
    v = skip_ws(v);
    char *end;
    long val = strtol(v, &end, 10);
    if (end == v) return -1;
    *out = (int)val;
    return 0;
}

int lsp_json_get_object(const char *json, const char *key,
                        char *out, size_t out_cap) {
    const char *v = find_key(json, key);
    if (!v) return -1;
    v = skip_ws(v);
    const char *start = v;
    const char *end = skip_value(v);
    if (!end) return -1;
    size_t len = (size_t)(end - start);
    if (len + 1 > out_cap) len = out_cap - 1;
    memcpy(out, start, len);
    out[len] = '\0';
    return 0;
}
