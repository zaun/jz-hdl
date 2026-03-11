#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chip_data.h"
#include "../chip_data_internal.h"
#include "util.h"

typedef struct JZChipDim {
    unsigned width;
    unsigned depth;
} JZChipDim;

typedef struct JZChipModeInfo {
    char    *name;
    unsigned r_ports;
    unsigned w_ports;
    JZBuffer configs; /* JZChipDim[] */
} JZChipModeInfo;

static int jz_strcasecmp(const char *a, const char *b)
{
    if (!a || !b) return (a == b) ? 0 : (a ? 1 : -1);
    while (*a && *b) {
        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        ++a;
        ++b;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

static int jz_json_token_to_bool(const char *json, const jsmntok_t *tok, int *out)
{
    if (!json || !tok || !out) return 0;
    if (tok->type != JSMN_PRIMITIVE && tok->type != JSMN_STRING) return 0;
    size_t len = (size_t)(tok->end - tok->start);
    if (len == 4 && strncmp(json + tok->start, "true", 4) == 0) {
        *out = 1;
        return 1;
    }
    if (len == 5 && strncmp(json + tok->start, "false", 5) == 0) {
        *out = 0;
        return 1;
    }
    return 0;
}

static int jz_json_token_to_string(const char *json,
                                   const jsmntok_t *tok,
                                   char *out,
                                   size_t out_size)
{
    if (!json || !tok || !out || out_size == 0) return 0;
    if (tok->type != JSMN_STRING) return 0;
    size_t len = (size_t)(tok->end - tok->start);
    if (len + 1 > out_size) return 0;
    memcpy(out, json + tok->start, len);
    out[len] = '\0';
    return 1;
}

static int jz_json_object_get(const char *json,
                              const jsmntok_t *toks,
                              int count,
                              int obj_index,
                              const char *key)
{
    if (!json || !toks || obj_index < 0 || obj_index >= count || !key) return -1;
    const jsmntok_t *obj = &toks[obj_index];
    if (obj->type != JSMN_OBJECT) return -1;
    int idx = obj_index + 1;
    for (int k = 0; k < obj->size; ++k) {
        const jsmntok_t *tok_key = &toks[idx++];
        if (jz_json_token_eq(json, tok_key, key)) {
            return idx;
        }
        idx = jz_json_skip(toks, count, idx);
    }
    return -1;
}

static int jz_json_object_get_string(const char *json,
                                     const jsmntok_t *toks,
                                     int count,
                                     int obj_index,
                                     const char *key,
                                     char *out,
                                     size_t out_size)
{
    if (!out || out_size == 0) return 0;
    int val_idx = jz_json_object_get(json, toks, count, obj_index, key);
    if (val_idx < 0) return 0;
    return jz_json_token_to_string(json, &toks[val_idx], out, out_size);
}

static int jz_json_param_minmax(const char *json,
                                const jsmntok_t *toks,
                                int count,
                                int params_idx,
                                const char *name,
                                double *min_out,
                                double *max_out)
{
    if (!json || !toks || !name || !min_out || !max_out) return 0;
    int param_idx = jz_json_object_get(json, toks, count, params_idx, name);
    if (param_idx < 0) return 0;
    int min_idx = jz_json_object_get(json, toks, count, param_idx, "min");
    int max_idx = jz_json_object_get(json, toks, count, param_idx, "max");
    if (min_idx < 0 || max_idx < 0) return 0;
    const jsmntok_t *min_tok = &toks[min_idx];
    const jsmntok_t *max_tok = &toks[max_idx];
    if (min_tok->type != JSMN_PRIMITIVE || max_tok->type != JSMN_PRIMITIVE) return 0;
    char buf[64];
    size_t len;
    char *endptr;
    len = (size_t)(min_tok->end - min_tok->start);
    if (len >= sizeof(buf)) return 0;
    memcpy(buf, json + min_tok->start, len);
    buf[len] = '\0';
    *min_out = strtod(buf, &endptr);
    if (endptr == buf) return 0;
    len = (size_t)(max_tok->end - max_tok->start);
    if (len >= sizeof(buf)) return 0;
    memcpy(buf, json + max_tok->start, len);
    buf[len] = '\0';
    *max_out = strtod(buf, &endptr);
    if (endptr == buf) return 0;
    return 1;
}

static void jz_print_section_heading(FILE *out, const char *title)
{
    if (!out || !title) return;
    fprintf(out, "--- %s ", title);
    size_t used = 4 + strlen(title) + 1;
    for (size_t i = used; i < 80; ++i) fputc('-', out);
    fputc('\n', out);
    fputc('\n', out);
}

static void jz_print_kbits(FILE *out, double kbits)
{
    if (!out) return;
    double rounded = (double)((long long)(kbits + 0.5));
    double diff = kbits - rounded;
    if (diff < 0) diff = -diff;
    if (diff < 0.05) {
        fprintf(out, "%.0f Kbits", rounded);
    } else {
        fprintf(out, "%.1f Kbits", kbits);
    }
}

static void jz_print_width_depth_table(const char *json,
                                       const jsmntok_t *toks,
                                       int count,
                                       int array_index,
                                       FILE *out,
                                       const char *indent)
{
    if (!json || !toks || !out || array_index < 0 || array_index >= count) return;
    const jsmntok_t *arr = &toks[array_index];
    if (arr->type != JSMN_ARRAY) return;

    fprintf(out, "%sWidth | Depth\n", indent);
    fprintf(out, "%s------|------\n", indent);

    int cur = array_index + 1;
    while (cur < count && toks[cur].start < arr->end) {
        const jsmntok_t *obj = &toks[cur];
        if (obj->type != JSMN_OBJECT) {
            cur = jz_json_skip(toks, count, cur);
            continue;
        }
        unsigned width = 0;
        unsigned depth = 0;
        int have_width = 0;
        int have_depth = 0;
        int idx = cur + 1;
        for (int k = 0; k < obj->size; ++k) {
            const jsmntok_t *key = &toks[idx++];
            const jsmntok_t *val = &toks[idx];
            if (jz_json_token_eq(json, key, "width")) {
                have_width = jz_json_token_to_uint(json, val, &width);
            } else if (jz_json_token_eq(json, key, "depth")) {
                have_depth = jz_json_token_to_uint(json, val, &depth);
            }
            idx = jz_json_skip(toks, count, idx);
        }
        if (have_width && have_depth) {
            fprintf(out, "%s%5u | %u\n", indent, width, depth);
        }
        cur = jz_json_skip(toks, count, cur);
    }
}

static void jz_print_padded(FILE *out, const char *s, size_t width)
{
    if (!out) return;
    size_t len = s ? strlen(s) : 0;
    if (s && len > 0) {
        fputs(s, out);
    }
    if (width > len) {
        for (size_t i = 0; i < width - len; ++i) {
            fputc(' ', out);
        }
    }
}

static void jz_print_table_header(FILE *out,
                                  const char **headers,
                                  const size_t *widths,
                                  size_t cols)
{
    if (!out || !headers || !widths || cols == 0) return;
    for (size_t i = 0; i < cols; ++i) {
        jz_print_padded(out, headers[i], widths[i]);
        if (i + 1 < cols) {
            fputs(" | ", out);
        }
    }
    fputc('\n', out);
}

static void jz_print_table_separator(FILE *out,
                                     const size_t *widths,
                                     size_t cols)
{
    if (!out || !widths || cols == 0) return;
    for (size_t i = 0; i < cols; ++i) {
        for (size_t k = 0; k < widths[i]; ++k) {
            fputc('-', out);
        }
        if (i + 1 < cols) {
            fputs("-|-", out);
        }
    }
    fputc('\n', out);
}

#define JZ_DESC_WRAP_LIMIT 100

/* Print a table row, wrapping the last column if it exceeds JZ_DESC_WRAP_LIMIT.
 * Break on space if possible, anywhere if not. Continuation lines are
 * indented to align with the last column. */
static void jz_print_table_row(FILE *out,
                                const char **cols,
                                const size_t *widths,
                                size_t ncols)
{
    if (!out || !cols || !widths || ncols == 0) return;

    /* Print all columns except the last normally. */
    for (size_t i = 0; i + 1 < ncols; ++i) {
        jz_print_padded(out, cols[i], widths[i]);
        fputs(" | ", out);
    }

    /* Last column: wrap if longer than JZ_DESC_WRAP_LIMIT. */
    const char *text = cols[ncols - 1] ? cols[ncols - 1] : "";
    size_t tlen = strlen(text);

    if (tlen <= JZ_DESC_WRAP_LIMIT) {
        jz_print_padded(out, text, widths[ncols - 1]);
        fputc('\n', out);
        return;
    }

    /* Compute indent width for continuation lines (sum of prior columns + separators). */
    size_t indent = 0;
    for (size_t i = 0; i + 1 < ncols; ++i) {
        indent += widths[i] + 3; /* " | " */
    }

    size_t pos = 0;
    int first_line = 1;
    while (pos < tlen) {
        if (!first_line) {
            for (size_t s = 0; s < indent; ++s) fputc(' ', out);
        }
        size_t remaining = tlen - pos;
        if (remaining <= JZ_DESC_WRAP_LIMIT) {
            /* Last chunk — print and pad to column width. */
            fputs(text + pos, out);
            if (widths[ncols - 1] > remaining) {
                for (size_t s = 0; s < widths[ncols - 1] - remaining; ++s)
                    fputc(' ', out);
            }
            fputc('\n', out);
            break;
        }
        /* Find a break point at or before the limit. */
        size_t brk = JZ_DESC_WRAP_LIMIT;
        /* Scan backwards for a space. */
        size_t sp = brk;
        while (sp > 0 && text[pos + sp] != ' ') --sp;
        if (sp > 0) {
            brk = sp; /* break before the space */
        }
        /* Print this chunk. */
        fwrite(text + pos, 1, brk, out);
        /* Pad to column width. */
        if (widths[ncols - 1] > brk) {
            for (size_t s = 0; s < widths[ncols - 1] - brk; ++s)
                fputc(' ', out);
        }
        fputc('\n', out);
        pos += brk;
        /* Skip the space at the break point. */
        if (pos < tlen && text[pos] == ' ') ++pos;
        first_line = 0;
    }
}

static void jz_sanitize_table_text(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0) return;
    dst[0] = '\0';
    if (!src) return;

    size_t di = 0;
    for (size_t si = 0; src[si] != '\0' && di + 1 < dst_size; ) {
        unsigned char c = (unsigned char)src[si];
        if (c < 0x80) {
            dst[di++] = (char)c;
            si++;
            continue;
        }
        if (c == 0xC2 && (unsigned char)src[si + 1] == 0xB0) {
            const char *rep = "deg";
            for (size_t r = 0; rep[r] && di + 1 < dst_size; ++r) {
                dst[di++] = rep[r];
            }
            si += 2;
            continue;
        }
        if (c == 0xC3 && (unsigned char)src[si + 1] == 0x97) {
            dst[di++] = 'x';
            si += 2;
            continue;
        }
        if (c == 0xE2 && (unsigned char)src[si + 1] == 0x80 &&
            ((unsigned char)src[si + 2] == 0x93 || (unsigned char)src[si + 2] == 0x94)) {
            dst[di++] = '-';
            si += 3;
            continue;
        }
        if (c == 0xE2 && (unsigned char)src[si + 1] == 0x80 &&
            (unsigned char)src[si + 2] == 0xA2) {
            dst[di++] = '*';
            si += 3;
            continue;
        }
        dst[di++] = '?';
        si++;
    }
    dst[di] = '\0';
}

static void jz_mode_info_free(JZChipModeInfo *mode)
{
    if (!mode) return;
    free(mode->name);
    mode->name = NULL;
    jz_buf_free(&mode->configs);
}

static size_t jz_mode_config_count(const JZChipModeInfo *mode)
{
    if (!mode) return 0;
    return mode->configs.len / sizeof(JZChipDim);
}

static size_t jz_mode_display_name(const JZChipModeInfo *mode,
                                   int index,
                                   char *out,
                                   size_t out_size)
{
    if (!out || out_size == 0) return 0;
    const char *name = (mode && mode->name && mode->name[0]) ? mode->name : NULL;
    int written = 0;
    if (name) {
        if (mode && mode->r_ports > 0 && mode->w_ports > 0) {
            written = snprintf(out, out_size, "%s (%uR/%uW)",
                               name, mode->r_ports, mode->w_ports);
        } else {
            written = snprintf(out, out_size, "%s", name);
        }
    } else {
        if (mode && mode->r_ports > 0 && mode->w_ports > 0) {
            written = snprintf(out, out_size, "Mode %d (%uR/%uW)",
                               index, mode->r_ports, mode->w_ports);
        } else {
            written = snprintf(out, out_size, "Mode %d", index);
        }
    }
    if (written < 0) {
        out[0] = '\0';
        return 0;
    }
    return (size_t)written;
}

static void jz_collect_configs(const char *json,
                               const jsmntok_t *toks,
                               int count,
                               int array_index,
                               JZBuffer *out)
{
    if (!json || !toks || !out || array_index < 0 || array_index >= count) return;
    const jsmntok_t *arr = &toks[array_index];
    if (arr->type != JSMN_ARRAY) return;
    int cur = array_index + 1;
    while (cur < count && toks[cur].start < arr->end) {
        const jsmntok_t *obj = &toks[cur];
        if (obj->type != JSMN_OBJECT) {
            cur = jz_json_skip(toks, count, cur);
            continue;
        }
        unsigned width = 0;
        unsigned depth = 0;
        int have_width = 0;
        int have_depth = 0;
        int idx = cur + 1;
        while (idx < count && toks[idx].start < obj->end) {
            const jsmntok_t *key = &toks[idx++];
            const jsmntok_t *val = &toks[idx];
            if (jz_json_token_eq(json, key, "width")) {
                have_width = jz_json_token_to_uint(json, val, &width);
            } else if (jz_json_token_eq(json, key, "depth")) {
                have_depth = jz_json_token_to_uint(json, val, &depth);
            }
            idx = jz_json_skip(toks, count, idx);
        }
        if (have_width && have_depth) {
            JZChipDim dim;
            dim.width = width;
            dim.depth = depth;
            (void)jz_buf_append(out, &dim, sizeof(dim));
        }
        cur = jz_json_skip(toks, count, cur);
    }
}

static void jz_collect_modes(const char *json,
                             const jsmntok_t *toks,
                             int count,
                             int array_index,
                             JZBuffer *out)
{
    if (!json || !toks || !out || array_index < 0 || array_index >= count) return;
    const jsmntok_t *arr = &toks[array_index];
    if (arr->type != JSMN_ARRAY) return;
    int cur = array_index + 1;
    while (cur < count && toks[cur].start < arr->end) {
        const jsmntok_t *mode = &toks[cur];
        if (mode->type != JSMN_OBJECT) {
            cur = jz_json_skip(toks, count, cur);
            continue;
        }

        int name_idx = -1;
        int mode_r_idx = -1;
        int mode_w_idx = -1;
        int mode_cfg_idx = -1;
        int ports_idx = -1;
        int idx = cur + 1;
        while (idx < count && toks[idx].start < mode->end) {
            const jsmntok_t *key = &toks[idx++];
            if (jz_json_token_eq(json, key, "name")) {
                name_idx = idx;
            } else if (jz_json_token_eq(json, key, "r_ports")) {
                mode_r_idx = idx;
            } else if (jz_json_token_eq(json, key, "w_ports")) {
                mode_w_idx = idx;
            } else if (jz_json_token_eq(json, key, "ports")) {
                ports_idx = idx;
            } else if (jz_json_token_eq(json, key, "configurations")) {
                mode_cfg_idx = idx;
            }
            idx = jz_json_skip(toks, count, idx);
        }

        JZChipModeInfo info;
        memset(&info, 0, sizeof(info));
        if (name_idx >= 0) {
            char name_buf[128] = {0};
            if (jz_json_token_to_string(json, &toks[name_idx], name_buf, sizeof(name_buf))) {
                info.name = jz_strdup(name_buf);
            }
        }
        if (mode_r_idx >= 0) {
            (void)jz_json_token_to_uint(json, &toks[mode_r_idx], &info.r_ports);
        }
        if (mode_w_idx >= 0) {
            (void)jz_json_token_to_uint(json, &toks[mode_w_idx], &info.w_ports);
        }
        /* Parse new-style "ports" array: [{"read":true,"write":true}, ...] */
        if (ports_idx >= 0 && toks[ports_idx].type == JSMN_ARRAY) {
            unsigned r = 0, w = 0;
            int pidx = ports_idx + 1;
            for (int pi = 0; pi < toks[ports_idx].size && pidx < count; ++pi) {
                const jsmntok_t *pobj = &toks[pidx];
                if (pobj->type == JSMN_OBJECT) {
                    int pk = pidx + 1;
                    while (pk < count && toks[pk].start < pobj->end) {
                        const jsmntok_t *pkey = &toks[pk++];
                        const jsmntok_t *pval = &toks[pk];
                        if (jz_json_token_eq(json, pkey, "read") &&
                            pval->type == JSMN_PRIMITIVE && json[pval->start] == 't') {
                            r++;
                        } else if (jz_json_token_eq(json, pkey, "write") &&
                                   pval->type == JSMN_PRIMITIVE && json[pval->start] == 't') {
                            w++;
                        }
                        pk = jz_json_skip(toks, count, pk);
                    }
                }
                pidx = jz_json_skip(toks, count, pidx);
            }
            info.r_ports = r;
            info.w_ports = w;
        }
        if (mode_cfg_idx >= 0) {
            jz_collect_configs(json, toks, count, mode_cfg_idx, &info.configs);
        }
        if (jz_buf_append(out, &info, sizeof(info)) != 0) {
            jz_mode_info_free(&info);
            return;
        }

        cur = jz_json_skip(toks, count, cur);
    }
}

static int jz_desc_has_bram(const char *desc)
{
    if (!desc) return 0;
    if (strstr(desc, "BSRAM")) return 1;
    if (strstr(desc, "Block Static Random Access Memory")) return 1;
    return 0;
}

static void jz_print_clock_gen_info(const char *json,
                                    const jsmntok_t *toks,
                                    int count,
                                    int clock_gen_idx,
                                    FILE *out)
{
    if (!json || !toks || !out || clock_gen_idx < 0 || clock_gen_idx >= count) return;
    if (toks[clock_gen_idx].type != JSMN_ARRAY) return;

    const jsmntok_t *arr = &toks[clock_gen_idx];
    int cur = clock_gen_idx + 1;
    while (cur < count && toks[cur].start < arr->end) {
        const jsmntok_t *obj = &toks[cur];
        if (obj->type != JSMN_OBJECT) {
            cur = jz_json_skip(toks, count, cur);
            continue;
        }

        int type_idx = jz_json_object_get(json, toks, count, cur, "type");
        if (type_idx < 0 || toks[type_idx].type != JSMN_STRING) {
            cur = jz_json_skip(toks, count, cur);
            continue;
        }

        char type_buf[32] = {0};
        (void)jz_json_token_to_string(json, &toks[type_idx], type_buf, sizeof(type_buf));

        /* Determine display name based on type (generic: supports numbered variants) */
        char type_display_buf[128];
        char type_short_buf[32];
        {
            /* Uppercase the type for display */
            size_t tl = strlen(type_buf);
            if (tl >= sizeof(type_short_buf)) tl = sizeof(type_short_buf) - 1;
            for (size_t ti = 0; ti < tl; ++ti)
                type_short_buf[ti] = (char)toupper((unsigned char)type_buf[ti]);
            type_short_buf[tl] = '\0';

            /* Map base type to descriptive name (type_buf is lowercase from JSON) */
            const char *desc = NULL;
            if (strncmp(type_buf, "pll", 3) == 0) desc = "PHASE-LOCKED LOOP";
            else if (strncmp(type_buf, "dll", 3) == 0) desc = "DELAY-LOCKED LOOP";
            else if (strncmp(type_buf, "clkdiv", 6) == 0) desc = "CLOCK DIVIDER";
            else if (strncmp(type_buf, "osc", 3) == 0) desc = "INTERNAL OSCILLATOR";
            else if (strncmp(type_buf, "buf", 3) == 0) desc = "CLOCK BUFFER";

            if (desc) {
                snprintf(type_display_buf, sizeof(type_display_buf),
                         "%s (%s)", desc, type_short_buf);
            } else {
                snprintf(type_display_buf, sizeof(type_display_buf), "%s", type_short_buf);
            }
        }
        const char *type_display = type_display_buf;
        const char *type_short = type_short_buf;

        int count_idx = jz_json_object_get(json, toks, count, cur, "count");
        int chaining_idx = jz_json_object_get(json, toks, count, cur, "chaining");
        int mode_idx = jz_json_object_get(json, toks, count, cur, "mode");
        int params_idx = jz_json_object_get(json, toks, count, cur, "parameters");
        int derived_idx = jz_json_object_get(json, toks, count, cur, "derived");
        int constraints_idx = jz_json_object_get(json, toks, count, cur, "constraints");

        unsigned gen_count = 0;
        int have_count = (count_idx >= 0 && jz_json_token_to_uint(json, &toks[count_idx], &gen_count));
        int chaining = 0;
        int have_chaining = (chaining_idx >= 0 && jz_json_token_to_bool(json, &toks[chaining_idx], &chaining));
        char mode_buf[32] = {0};
        if (mode_idx >= 0) {
            (void)jz_json_token_to_string(json, &toks[mode_idx], mode_buf, sizeof(mode_buf));
        }

        struct {
            char name[64];
            char min[16];
            char max[16];
            char formula[256];
            char desc[256];
        } derived_rows[16];
        size_t derived_count = 0;
        unsigned vco_min = 0;
        unsigned vco_max = 0;
        int have_vco = 0;
        if (derived_idx >= 0 && toks[derived_idx].type == JSMN_OBJECT) {
            int didx = derived_idx + 1;
            while (didx < count && toks[didx].start < toks[derived_idx].end) {
                const jsmntok_t *key = &toks[didx++];
                const jsmntok_t *val = &toks[didx];
                char name_buf[64] = {0};
                char desc_buf[256] = {0};
                char expr_buf[256] = {0};
                double min_val = 0;
                double max_val = 0;
                int have_min = 0;
                int have_max = 0;
                if (key->type == JSMN_STRING) {
                    (void)jz_json_token_to_string(json, key, name_buf, sizeof(name_buf));
                }
                if (val->type == JSMN_OBJECT) {
                    int min_idx2 = jz_json_object_get(json, toks, count, didx, "min");
                    int max_idx2 = jz_json_object_get(json, toks, count, didx, "max");
                    int expr_idx = jz_json_object_get(json, toks, count, didx, "expr");
                    int desc_idx = jz_json_object_get(json, toks, count, didx, "description");
                    if (min_idx2 >= 0 && toks[min_idx2].type == JSMN_PRIMITIVE) {
                        size_t len = (size_t)(toks[min_idx2].end - toks[min_idx2].start);
                        char buf[64];
                        if (len < sizeof(buf)) {
                            memcpy(buf, json + toks[min_idx2].start, len);
                            buf[len] = '\0';
                            char *ep;
                            min_val = strtod(buf, &ep);
                            if (ep != buf) have_min = 1;
                        }
                    }
                    if (max_idx2 >= 0 && toks[max_idx2].type == JSMN_PRIMITIVE) {
                        size_t len = (size_t)(toks[max_idx2].end - toks[max_idx2].start);
                        char buf[64];
                        if (len < sizeof(buf)) {
                            memcpy(buf, json + toks[max_idx2].start, len);
                            buf[len] = '\0';
                            char *ep;
                            max_val = strtod(buf, &ep);
                            if (ep != buf) have_max = 1;
                        }
                    }
                    if (expr_idx >= 0) {
                        (void)jz_json_token_to_string(json, &toks[expr_idx], expr_buf, sizeof(expr_buf));
                    }
                    if (desc_idx >= 0) {
                        (void)jz_json_token_to_string(json, &toks[desc_idx], desc_buf, sizeof(desc_buf));
                    }
                }
                if (name_buf[0] &&
                    (have_min || have_max || expr_buf[0] || desc_buf[0]) &&
                    derived_count < (sizeof(derived_rows) / sizeof(derived_rows[0]))) {
                    char safe_expr[256];
                    char safe_desc[256];
                    jz_sanitize_table_text(safe_expr, sizeof(safe_expr), expr_buf);
                    jz_sanitize_table_text(safe_desc, sizeof(safe_desc), desc_buf);
                    memset(&derived_rows[derived_count], 0, sizeof(derived_rows[derived_count]));
                    snprintf(derived_rows[derived_count].name, sizeof(derived_rows[derived_count].name), "%s", name_buf);
                    if (have_min) {
                        if (min_val == (long)min_val)
                            snprintf(derived_rows[derived_count].min, sizeof(derived_rows[derived_count].min), "%ld", (long)min_val);
                        else
                            snprintf(derived_rows[derived_count].min, sizeof(derived_rows[derived_count].min), "%g", min_val);
                    } else {
                        snprintf(derived_rows[derived_count].min, sizeof(derived_rows[derived_count].min), "N/A");
                    }
                    if (have_max) {
                        if (max_val == (long)max_val)
                            snprintf(derived_rows[derived_count].max, sizeof(derived_rows[derived_count].max), "%ld", (long)max_val);
                        else
                            snprintf(derived_rows[derived_count].max, sizeof(derived_rows[derived_count].max), "%g", max_val);
                    } else {
                        snprintf(derived_rows[derived_count].max, sizeof(derived_rows[derived_count].max), "N/A");
                    }
                    snprintf(derived_rows[derived_count].formula, sizeof(derived_rows[derived_count].formula), "%s",
                             safe_expr);
                    snprintf(derived_rows[derived_count].desc, sizeof(derived_rows[derived_count].desc), "%s",
                             safe_desc);
                    if (jz_strcasecmp(name_buf, "FVCO") == 0 ||
                        jz_strcasecmp(name_buf, "VCO") == 0) {
                        have_vco = 1;
                        vco_min = min_val;
                        vco_max = max_val;
                    }
                    derived_count++;
                }
                didx = jz_json_skip(toks, count, didx);
            }
        }

        /* Print sub-heading */
        fprintf(out, "  %s\n", type_display);
        if (have_vco) {
            fprintf(out, "  %u %s%s available | Max VCO: %u-%u MHz | Chaining: %s\n\n",
                    have_count ? gen_count : 0,
                    type_short,
                    (have_count && gen_count == 1) ? "" : "s",
                    vco_min, vco_max,
                    have_chaining ? (chaining ? "Supported" : "Not supported") : "Unknown");
        } else {
            fprintf(out, "  %u %s%s available",
                    have_count ? gen_count : 0,
                    type_short,
                    (have_count && gen_count == 1) ? "" : "s");
            if (mode_buf[0]) {
                fprintf(out, " | Mode: %s", mode_buf);
            }
            if (have_chaining) {
                fprintf(out, " | Chaining: %s", chaining ? "Supported" : "Not supported");
            }
            fprintf(out, "\n\n");
        }

        /* Inputs (if present) */
        int inputs_idx = jz_json_object_get(json, toks, count, cur, "inputs");
        if (inputs_idx >= 0 && toks[inputs_idx].type == JSMN_OBJECT) {
            fprintf(out, "  %s Inputs\n", type_short);
            const char *input_headers[] = { "Input", "Width", "Requires Period", "Description" };
            struct {
                char name[32];
                char width[16];
                char requires_period[16];
                char desc[256];
            } input_rows[8];
            size_t input_count = 0;
            int iidx = inputs_idx + 1;
            while (iidx < count && toks[iidx].start < toks[inputs_idx].end) {
                const jsmntok_t *key = &toks[iidx++];
                const jsmntok_t *val = &toks[iidx];
                if (key->type != JSMN_STRING || val->type != JSMN_OBJECT) {
                    iidx = jz_json_skip(toks, count, iidx);
                    continue;
                }
                char name_buf[32] = {0};
                char desc_buf[256] = {0};
                unsigned width_val = 0;
                int has_width = 0;
                int requires_period = 0;
                int has_requires = 0;
                (void)jz_json_token_to_string(json, key, name_buf, sizeof(name_buf));
                int width_idx = jz_json_object_get(json, toks, count, iidx, "width");
                if (width_idx >= 0) {
                    has_width = jz_json_token_to_uint(json, &toks[width_idx], &width_val);
                }
                int req_idx = jz_json_object_get(json, toks, count, iidx, "requires_period");
                if (req_idx >= 0) {
                    has_requires = jz_json_token_to_bool(json, &toks[req_idx], &requires_period);
                }
                (void)jz_json_object_get_string(json, toks, count,
                                                iidx, "description",
                                                desc_buf, sizeof(desc_buf));
                if (name_buf[0] && input_count < (sizeof(input_rows) / sizeof(input_rows[0]))) {
                    memset(&input_rows[input_count], 0, sizeof(input_rows[input_count]));
                    snprintf(input_rows[input_count].name, sizeof(input_rows[input_count].name), "%s", name_buf);
                    if (has_width) {
                        snprintf(input_rows[input_count].width, sizeof(input_rows[input_count].width), "%u", width_val);
                    } else {
                        input_rows[input_count].width[0] = '\0';
                    }
                    if (has_requires) {
                        snprintf(input_rows[input_count].requires_period,
                                 sizeof(input_rows[input_count].requires_period),
                                 "%s", requires_period ? "yes" : "no");
                    }
                    jz_sanitize_table_text(input_rows[input_count].desc,
                                           sizeof(input_rows[input_count].desc),
                                           desc_buf);
                    input_count++;
                }
                iidx = jz_json_skip(toks, count, iidx);
            }
            size_t input_widths[4] = {
                strlen(input_headers[0]),
                strlen(input_headers[1]),
                strlen(input_headers[2]),
                strlen(input_headers[3])
            };
            for (size_t i = 0; i < input_count; ++i) {
                size_t len0 = strlen(input_rows[i].name);
                size_t len1 = strlen(input_rows[i].width);
                size_t len2 = strlen(input_rows[i].requires_period);
                size_t len3 = strlen(input_rows[i].desc);
                if (len0 > input_widths[0]) input_widths[0] = len0;
                if (len1 > input_widths[1]) input_widths[1] = len1;
                if (len2 > input_widths[2]) input_widths[2] = len2;
                if (len3 > input_widths[3]) input_widths[3] = len3;
            }
            if (input_widths[3] > JZ_DESC_WRAP_LIMIT)
                input_widths[3] = JZ_DESC_WRAP_LIMIT;
            jz_print_table_header(out, input_headers, input_widths, 4);
            jz_print_table_separator(out, input_widths, 4);
            for (size_t i = 0; i < input_count; ++i) {
                const char *cols[] = {
                    input_rows[i].name,
                    input_rows[i].width,
                    input_rows[i].requires_period,
                    input_rows[i].desc
                };
                jz_print_table_row(out, cols, input_widths, 4);
            }
            fprintf(out, "\n");
        }

        /* Configurable parameters - read dynamically from JSON */
        if (params_idx >= 0 && toks[params_idx].type == JSMN_OBJECT) {
            const char *param_headers[] = { "Parameter", "Range/Valid", "Description" };
            struct {
                char name[32];
                char range[128];
                char desc[256];
            } param_rows[16];
            size_t param_count = 0;

            int pidx = params_idx + 1;
            while (pidx < count && toks[pidx].start < toks[params_idx].end) {
                const jsmntok_t *pkey = &toks[pidx++];
                const jsmntok_t *pval = &toks[pidx];
                if (pkey->type != JSMN_STRING || pval->type != JSMN_OBJECT) {
                    pidx = jz_json_skip(toks, count, pidx);
                    continue;
                }
                if (param_count >= sizeof(param_rows) / sizeof(param_rows[0])) {
                    pidx = jz_json_skip(toks, count, pidx);
                    continue;
                }

                char pname[32] = {0};
                char pdesc[256] = {0};
                (void)jz_json_token_to_string(json, pkey, pname, sizeof(pname));

                double min_val = 0, max_val = 0;
                int have_minmax = jz_json_param_minmax(json, toks, count, params_idx,
                                                       pname, &min_val, &max_val);
                (void)jz_json_object_get_string(json, toks, count,
                                                pidx, "description",
                                                pdesc, sizeof(pdesc));

                /* Check for "valid" array */
                int valid_idx = jz_json_object_get(json, toks, count, pidx, "valid");
                char range_buf[128] = {0};
                if (valid_idx >= 0 && toks[valid_idx].type == JSMN_ARRAY) {
                    size_t off = 0;
                    int vidx = valid_idx + 1;
                    int vcount = toks[valid_idx].size;
                    /* For large valid arrays, show range shorthand */
                    if (vcount > 8) {
                        char first[32] = {0}, last[32] = {0};
                        /* Read first element */
                        {
                            const jsmntok_t *ft = &toks[vidx];
                            size_t flen = (size_t)(ft->end - ft->start);
                            if (flen < sizeof(first)) {
                                memcpy(first, json + ft->start, flen);
                                first[flen] = '\0';
                            }
                        }
                        /* Walk to last element */
                        int last_idx = vidx;
                        for (int vi = 0; vi < vcount && last_idx < count; ++vi) {
                            if (vi == vcount - 1) break;
                            last_idx = jz_json_skip(toks, count, last_idx);
                        }
                        {
                            const jsmntok_t *lt = &toks[last_idx];
                            size_t llen = (size_t)(lt->end - lt->start);
                            if (llen < sizeof(last)) {
                                memcpy(last, json + lt->start, llen);
                                last[llen] = '\0';
                            }
                        }
                        snprintf(range_buf, sizeof(range_buf),
                                 "%s - %s (%d values)", first, last, vcount);
                        /* Skip past all elements */
                        for (int vi = 0; vi < vcount && vidx < count; ++vi) {
                            vidx = jz_json_skip(toks, count, vidx);
                        }
                    } else {
                        for (int vi = 0; vi < vcount && vidx < count; ++vi) {
                            char vbuf[32] = {0};
                            const jsmntok_t *vtok = &toks[vidx];
                            size_t vtlen = (size_t)(vtok->end - vtok->start);
                            if (vtlen < sizeof(vbuf)) {
                                memcpy(vbuf, json + vtok->start, vtlen);
                                vbuf[vtlen] = '\0';
                            }
                            if (off > 0 && off < sizeof(range_buf) - 2) {
                                range_buf[off++] = ',';
                                range_buf[off++] = ' ';
                            }
                            size_t vlen = strlen(vbuf);
                            if (off + vlen < sizeof(range_buf) - 1) {
                                memcpy(range_buf + off, vbuf, vlen);
                                off += vlen;
                            }
                            vidx = jz_json_skip(toks, count, vidx);
                        }
                        range_buf[off] = '\0';
                    }
                } else if (have_minmax) {
                    /* Use integer format if values are whole numbers */
                    if (min_val == (long)min_val && max_val == (long)max_val) {
                        snprintf(range_buf, sizeof(range_buf), "%ld - %ld", (long)min_val, (long)max_val);
                    } else {
                        snprintf(range_buf, sizeof(range_buf), "%g - %g", min_val, max_val);
                    }
                }

                memset(&param_rows[param_count], 0, sizeof(param_rows[param_count]));
                snprintf(param_rows[param_count].name, sizeof(param_rows[param_count].name), "%s", pname);
                snprintf(param_rows[param_count].range, sizeof(param_rows[param_count].range), "%s", range_buf);
                jz_sanitize_table_text(param_rows[param_count].desc,
                                       sizeof(param_rows[param_count].desc),
                                       pdesc);
                param_count++;
                pidx = jz_json_skip(toks, count, pidx);
            }
            if (param_count > 0) {
                fprintf(out, "  Configurable Parameters\n");
                size_t param_widths[3] = {
                    strlen(param_headers[0]),
                    strlen(param_headers[1]),
                    strlen(param_headers[2])
                };
                for (size_t i = 0; i < param_count; ++i) {
                    size_t len0 = strlen(param_rows[i].name);
                    size_t len1 = strlen(param_rows[i].range);
                    size_t len2 = strlen(param_rows[i].desc);
                    if (len0 > param_widths[0]) param_widths[0] = len0;
                    if (len1 > param_widths[1]) param_widths[1] = len1;
                    if (len2 > param_widths[2]) param_widths[2] = len2;
                }
                if (param_widths[2] > JZ_DESC_WRAP_LIMIT)
                    param_widths[2] = JZ_DESC_WRAP_LIMIT;
                jz_print_table_header(out, param_headers, param_widths, 3);
                jz_print_table_separator(out, param_widths, 3);
                for (size_t i = 0; i < param_count; ++i) {
                    const char *cols[] = {
                        param_rows[i].name,
                        param_rows[i].range,
                        param_rows[i].desc
                    };
                    jz_print_table_row(out, cols, param_widths, 3);
                }
                fprintf(out, "\n");
            }
        }

        /* Outputs - read dynamically from JSON */
        int outputs_idx = jz_json_object_get(json, toks, count, cur, "outputs");
        if (outputs_idx >= 0 && toks[outputs_idx].type == JSMN_OBJECT) {
            fprintf(out, "  %s Outputs\n", type_short);
            const char *output_headers[] = {
                "Output", "Frequency Formula", "Description"
            };
            struct {
                char name[16];
                char formula[256];
                char desc[256];
            } output_rows[8];
            size_t output_count = 0;

            int oidx = outputs_idx + 1;
            while (oidx < count && toks[oidx].start < toks[outputs_idx].end) {
                const jsmntok_t *okey = &toks[oidx++];
                const jsmntok_t *oval = &toks[oidx];
                if (okey->type != JSMN_STRING || oval->type != JSMN_OBJECT) {
                    oidx = jz_json_skip(toks, count, oidx);
                    continue;
                }
                if (output_count >= sizeof(output_rows) / sizeof(output_rows[0])) {
                    oidx = jz_json_skip(toks, count, oidx);
                    continue;
                }

                char oname[16] = {0};
                char odesc[256] = {0};
                char oformula[256] = {0};
                (void)jz_json_token_to_string(json, okey, oname, sizeof(oname));
                (void)jz_json_object_get_string(json, toks, count,
                                                oidx, "description",
                                                odesc, sizeof(odesc));
                int freq_idx = jz_json_object_get(json, toks, count, oidx, "frequency_mhz");
                if (freq_idx >= 0) {
                    (void)jz_json_object_get_string(json, toks, count,
                                                    freq_idx, "expr",
                                                    oformula, sizeof(oformula));
                }

                memset(&output_rows[output_count], 0, sizeof(output_rows[output_count]));
                snprintf(output_rows[output_count].name, sizeof(output_rows[output_count].name), "%s", oname);
                jz_sanitize_table_text(output_rows[output_count].formula,
                                       sizeof(output_rows[output_count].formula),
                                       oformula);
                jz_sanitize_table_text(output_rows[output_count].desc,
                                       sizeof(output_rows[output_count].desc),
                                       odesc);
                output_count++;
                oidx = jz_json_skip(toks, count, oidx);
            }

            size_t output_widths[3] = {
                strlen(output_headers[0]),
                strlen(output_headers[1]),
                strlen(output_headers[2])
            };
            for (size_t i = 0; i < output_count; ++i) {
                size_t len0 = strlen(output_rows[i].name);
                size_t len1 = strlen(output_rows[i].formula);
                size_t len2 = strlen(output_rows[i].desc);
                if (len0 > output_widths[0]) output_widths[0] = len0;
                if (len1 > output_widths[1]) output_widths[1] = len1;
                if (len2 > output_widths[2]) output_widths[2] = len2;
            }
            if (output_widths[2] > JZ_DESC_WRAP_LIMIT)
                output_widths[2] = JZ_DESC_WRAP_LIMIT;
            jz_print_table_header(out, output_headers, output_widths, 3);
            jz_print_table_separator(out, output_widths, 3);
            for (size_t i = 0; i < output_count; ++i) {
                const char *cols[] = {
                    output_rows[i].name,
                    output_rows[i].formula,
                    output_rows[i].desc
                };
                jz_print_table_row(out, cols, output_widths, 3);
            }
            fprintf(out, "\n");
        }

        /* Derived values (if present) */
        if (derived_count > 0) {
            fprintf(out, "  Derived Values\n");
            const char *derived_headers[] = { "Name", "Min", "Max", "Formula", "Description" };
            size_t derived_widths[5] = {
                strlen(derived_headers[0]),
                strlen(derived_headers[1]),
                strlen(derived_headers[2]),
                strlen(derived_headers[3]),
                strlen(derived_headers[4])
            };
            for (size_t i = 0; i < derived_count; ++i) {
                size_t len0 = strlen(derived_rows[i].name);
                size_t len1 = strlen(derived_rows[i].min);
                size_t len2 = strlen(derived_rows[i].max);
                size_t len3 = strlen(derived_rows[i].formula);
                size_t len4 = strlen(derived_rows[i].desc);
                if (len0 > derived_widths[0]) derived_widths[0] = len0;
                if (len1 > derived_widths[1]) derived_widths[1] = len1;
                if (len2 > derived_widths[2]) derived_widths[2] = len2;
                if (len3 > derived_widths[3]) derived_widths[3] = len3;
                if (len4 > derived_widths[4]) derived_widths[4] = len4;
            }
            if (derived_widths[4] > JZ_DESC_WRAP_LIMIT)
                derived_widths[4] = JZ_DESC_WRAP_LIMIT;
            jz_print_table_header(out, derived_headers, derived_widths, 5);
            jz_print_table_separator(out, derived_widths, 5);
            for (size_t i = 0; i < derived_count; ++i) {
                const char *cols[] = {
                    derived_rows[i].name,
                    derived_rows[i].min,
                    derived_rows[i].max,
                    derived_rows[i].formula,
                    derived_rows[i].desc
                };
                jz_print_table_row(out, cols, derived_widths, 5);
            }
            fprintf(out, "\n");
        }

        /* Constraints (if present) */
        if (constraints_idx >= 0 && toks[constraints_idx].type == JSMN_ARRAY) {
            fprintf(out, "  Constraints\n");
            const jsmntok_t *carr = &toks[constraints_idx];
            int ccur = constraints_idx + 1;
            while (ccur < count && toks[ccur].start < carr->end) {
                const jsmntok_t *cobj = &toks[ccur];
                if (cobj->type != JSMN_OBJECT) {
                    ccur = jz_json_skip(toks, count, ccur);
                    continue;
                }
                int rule_idx = jz_json_object_get(json, toks, count, ccur, "rule");
                if (rule_idx >= 0) {
                    char rule_buf[256];
                    if (jz_json_token_to_string(json, &toks[rule_idx], rule_buf, sizeof(rule_buf))) {
                        fprintf(out, "  %s\n", rule_buf);
                    }
                }
                ccur = jz_json_skip(toks, count, ccur);
            }
            fprintf(out, "\n");
        }

        fprintf(out, "\n");
        cur = jz_json_skip(toks, count, cur);
    }
}

int jz_chip_print_info(const char *chip_id, FILE *out)
{
    if (!chip_id || !out) return -1;
    const char *json = jz_chip_builtin_json(chip_id);
    if (!json) return -1;

    jsmn_parser parser;
    jsmn_init(&parser);
    int tok_count = jsmn_parse(&parser, json, strlen(json), NULL, 0);
    if (tok_count <= 0) return -1;

    jsmntok_t *toks = (jsmntok_t *)calloc((size_t)tok_count, sizeof(jsmntok_t));
    if (!toks) return -1;
    jsmn_init(&parser);
    tok_count = jsmn_parse(&parser, json, strlen(json), toks, (unsigned int)tok_count);
    if (tok_count <= 0) {
        free(toks);
        return -1;
    }

    if (toks[0].type != JSMN_OBJECT) {
        free(toks);
        return -1;
    }

    int memory_idx = -1;
    int chipid_idx = -1;
    int desc_idx = -1;
    int clock_gen_idx = -1;
    int fixed_pins_idx = -1;
    int resources_idx = -1;
    int dsp_idx = -1;
    int latches_idx = -1;
    int differential_idx = -1;
    int boards_idx = -1;

    int idx = 1;
    while (idx < tok_count && toks[idx].start < toks[0].end) {
        const jsmntok_t *key = &toks[idx++];
        if (jz_json_token_eq(json, key, "memory")) {
            memory_idx = idx;
        } else if (jz_json_token_eq(json, key, "chipid")) {
            chipid_idx = idx;
        } else if (jz_json_token_eq(json, key, "description")) {
            desc_idx = idx;
        } else if (jz_json_token_eq(json, key, "clock_gen")) {
            clock_gen_idx = idx;
        } else if (jz_json_token_eq(json, key, "fixed_pins")) {
            fixed_pins_idx = idx;
        } else if (jz_json_token_eq(json, key, "resources")) {
            resources_idx = idx;
        } else if (jz_json_token_eq(json, key, "dsp")) {
            dsp_idx = idx;
        } else if (jz_json_token_eq(json, key, "latches")) {
            latches_idx = idx;
        } else if (jz_json_token_eq(json, key, "differential")) {
            differential_idx = idx;
        } else if (jz_json_token_eq(json, key, "boards")) {
            boards_idx = idx;
        }
        idx = jz_json_skip(toks, tok_count, idx);
    }

    char chip_name[128];
    if (chipid_idx >= 0) {
        if (!jz_json_token_to_string(json, &toks[chipid_idx], chip_name, sizeof(chip_name))) {
            snprintf(chip_name, sizeof(chip_name), "%s", chip_id);
        }
    } else {
        snprintf(chip_name, sizeof(chip_name), "%s", chip_id);
    }
    char chip_desc[256] = {0};
    if (desc_idx >= 0) {
        (void)jz_json_token_to_string(json, &toks[desc_idx], chip_desc, sizeof(chip_desc));
    }

    for (int i = 0; i < 80; ++i) fputc('=', out);
    fputc('\n', out);
    fprintf(out, "  CHIP: %s\n", chip_name);
    if (chip_desc[0]) {
        fprintf(out, "  %s\n", chip_desc);
    }
    for (int i = 0; i < 80; ++i) fputc('=', out);
    fputc('\n', out);
    fputc('\n', out);

    /* Resources section */
    if (resources_idx >= 0 && toks[resources_idx].type == JSMN_OBJECT) {
        jz_print_section_heading(out, "Resources");
        const jsmntok_t *robj = &toks[resources_idx];
        int ridx = resources_idx + 1;
        size_t max_name_len = 0;
        /* First pass: find max name length */
        int ridx_tmp = ridx;
        while (ridx_tmp < tok_count && toks[ridx_tmp].start < robj->end) {
            const jsmntok_t *rkey = &toks[ridx_tmp++];
            size_t klen = (size_t)(rkey->end - rkey->start);
            if (klen > max_name_len) max_name_len = klen;
            ridx_tmp = jz_json_skip(toks, tok_count, ridx_tmp);
        }
        /* Second pass: print */
        while (ridx < tok_count && toks[ridx].start < robj->end) {
            const jsmntok_t *rkey = &toks[ridx++];
            const jsmntok_t *rval = &toks[ridx];
            char rname[64] = {0};
            (void)jz_json_token_to_string(json, rkey, rname, sizeof(rname));
            unsigned rcount = 0;
            if (jz_json_token_to_uint(json, rval, &rcount)) {
                fprintf(out, "  %-*s  %u\n", (int)max_name_len, rname, rcount);
            }
            ridx = jz_json_skip(toks, tok_count, ridx);
        }
        fprintf(out, "\n");
    }

    /* Latches section */
    if (latches_idx >= 0 && toks[latches_idx].type == JSMN_OBJECT) {
        jz_print_section_heading(out, "Latches");
        const jsmntok_t *lobj = &toks[latches_idx];
        int lidx = latches_idx + 1;
        while (lidx < tok_count && toks[lidx].start < lobj->end) {
            const jsmntok_t *lkey = &toks[lidx++];
            const jsmntok_t *lval = &toks[lidx];
            if (lval->type != JSMN_OBJECT) {
                lidx = jz_json_skip(toks, tok_count, lidx);
                continue;
            }
            char lname[64] = {0};
            (void)jz_json_token_to_string(json, lkey, lname, sizeof(lname));

            int d_idx = jz_json_object_get(json, toks, tok_count, lidx, "D");
            int sr_idx = jz_json_object_get(json, toks, tok_count, lidx, "SR");
            int note_idx = jz_json_object_get(json, toks, tok_count, lidx, "note");

            const char *d_str = "No";
            const char *sr_str = "No";
            if (d_idx >= 0) {
                size_t dlen = (size_t)(toks[d_idx].end - toks[d_idx].start);
                if (dlen == 4 && strncmp(json + toks[d_idx].start, "true", 4) == 0) d_str = "Yes";
            }
            if (sr_idx >= 0) {
                size_t slen = (size_t)(toks[sr_idx].end - toks[sr_idx].start);
                if (slen == 4 && strncmp(json + toks[sr_idx].start, "true", 4) == 0) sr_str = "Yes";
            }

            fprintf(out, "  %-4s D: %-3s  SR: %-3s", lname, d_str, sr_str);
            if (note_idx >= 0) {
                char note_buf[256] = {0};
                if (jz_json_token_to_string(json, &toks[note_idx], note_buf, sizeof(note_buf))) {
                    fprintf(out, "  (%s)", note_buf);
                }
            }
            fprintf(out, "\n");

            lidx = jz_json_skip(toks, tok_count, lidx);
        }
        fprintf(out, "\n");
    }

    /* DSP section */
    if (dsp_idx >= 0 && toks[dsp_idx].type == JSMN_OBJECT) {
        jz_print_section_heading(out, "DSP");
        const jsmntok_t *dobj = &toks[dsp_idx];
        int didx = dsp_idx + 1;
        while (didx < tok_count && toks[didx].start < dobj->end) {
            const jsmntok_t *dkey = &toks[didx++];
            const jsmntok_t *dval = &toks[didx];
            char dname[64] = {0};
            (void)jz_json_token_to_string(json, dkey, dname, sizeof(dname));
            if (dval->type == JSMN_OBJECT) {
                int qty_idx = jz_json_object_get(json, toks, tok_count, didx, "quantity");
                int ddesc_idx = jz_json_object_get(json, toks, tok_count, didx, "description");
                unsigned qty = 0;
                char ddesc[256] = {0};
                int have_qty = (qty_idx >= 0 && jz_json_token_to_uint(json, &toks[qty_idx], &qty));
                if (ddesc_idx >= 0) {
                    (void)jz_json_token_to_string(json, &toks[ddesc_idx], ddesc, sizeof(ddesc));
                }
                if (have_qty) {
                    fprintf(out, "  %-12s  %u", dname, qty);
                    if (ddesc[0]) {
                        fprintf(out, "  (%s)", ddesc);
                    }
                    fprintf(out, "\n");
                }
            }
            didx = jz_json_skip(toks, tok_count, didx);
        }
        fprintf(out, "\n");
    }

    jz_print_section_heading(out, "Memory");
    if (memory_idx < 0 || toks[memory_idx].type != JSMN_ARRAY) {
        fprintf(out, "  No memory data available.\n\n");
    } else {
        const jsmntok_t *mem_arr = &toks[memory_idx];
        int cur = memory_idx + 1;
        while (cur < tok_count && toks[cur].start < mem_arr->end) {
            const jsmntok_t *obj = &toks[cur];
            if (obj->type != JSMN_OBJECT) {
                cur = jz_json_skip(toks, tok_count, cur);
                continue;
            }

            int type_idx = -1;
            int desc_mem_idx = -1;
            int total_bits_idx = -1;
            int r_ports_idx = -1;
            int w_ports_idx = -1;
            int configs_idx = -1;
            int modes_idx = -1;
            int quantity_idx = -1;
            int bits_per_block_idx = -1;
            int max_freq_idx = -1;
            int capacity_mbits_idx = -1;
            int bus_width_idx = -1;

            int obj_cur = cur + 1;
            while (obj_cur < tok_count && toks[obj_cur].start < obj->end) {
                const jsmntok_t *key = &toks[obj_cur++];
                if (jz_json_token_eq(json, key, "type")) {
                    type_idx = obj_cur;
                } else if (jz_json_token_eq(json, key, "description")) {
                    desc_mem_idx = obj_cur;
                } else if (jz_json_token_eq(json, key, "total_bits")) {
                    total_bits_idx = obj_cur;
                } else if (jz_json_token_eq(json, key, "r_ports")) {
                    r_ports_idx = obj_cur;
                } else if (jz_json_token_eq(json, key, "w_ports")) {
                    w_ports_idx = obj_cur;
                } else if (jz_json_token_eq(json, key, "configurations")) {
                    configs_idx = obj_cur;
                } else if (jz_json_token_eq(json, key, "modes")) {
                    modes_idx = obj_cur;
                } else if (jz_json_token_eq(json, key, "quantity")) {
                    quantity_idx = obj_cur;
                } else if (jz_json_token_eq(json, key, "bits_per_block")) {
                    bits_per_block_idx = obj_cur;
                } else if (jz_json_token_eq(json, key, "max_freq_mhz")) {
                    max_freq_idx = obj_cur;
                } else if (jz_json_token_eq(json, key, "capacity_mbits")) {
                    capacity_mbits_idx = obj_cur;
                } else if (jz_json_token_eq(json, key, "bus_width")) {
                    bus_width_idx = obj_cur;
                }
                obj_cur = jz_json_skip(toks, tok_count, obj_cur);
            }

            char type_buf[64] = {0};
            if (type_idx >= 0) {
                (void)jz_json_token_to_string(json, &toks[type_idx], type_buf, sizeof(type_buf));
            }
            if (type_buf[0] == '\0') {
                strncpy(type_buf, "UNKNOWN", sizeof(type_buf) - 1);
            }

            int is_block = (jz_strcasecmp(type_buf, "BLOCK") == 0);
            int is_dist = (jz_strcasecmp(type_buf, "DISTRIBUTED") == 0);
            int is_sdram = (jz_strcasecmp(type_buf, "SDRAM") == 0);
            int is_flash = (jz_strcasecmp(type_buf, "FLASH") == 0);

            char mem_desc[256] = {0};
            if (desc_mem_idx >= 0) {
                (void)jz_json_token_to_string(json, &toks[desc_mem_idx], mem_desc, sizeof(mem_desc));
            }

            if (is_block && jz_desc_has_bram(mem_desc)) {
                fprintf(out, "  BLOCK (BSRAM)\n");
            } else {
                fprintf(out, "  %s\n", type_buf);
            }

            unsigned total_bits = 0;
            unsigned r_ports = 0;
            unsigned w_ports = 0;
            unsigned quantity = 0;
            unsigned bits_per_block = 0;
            unsigned max_freq = 0;
            unsigned capacity_mbits = 0;
            unsigned bus_width = 0;
            int have_total = (total_bits_idx >= 0 && jz_json_token_to_uint(json, &toks[total_bits_idx], &total_bits));
            int have_r = (r_ports_idx >= 0 && jz_json_token_to_uint(json, &toks[r_ports_idx], &r_ports));
            int have_w = (w_ports_idx >= 0 && jz_json_token_to_uint(json, &toks[w_ports_idx], &w_ports));
            int have_qty = (quantity_idx >= 0 && jz_json_token_to_uint(json, &toks[quantity_idx], &quantity));
            int have_bpb = (bits_per_block_idx >= 0 && jz_json_token_to_uint(json, &toks[bits_per_block_idx], &bits_per_block));
            int have_mhz = (max_freq_idx >= 0 && jz_json_token_to_uint(json, &toks[max_freq_idx], &max_freq));
            int have_cap = (capacity_mbits_idx >= 0 && jz_json_token_to_uint(json, &toks[capacity_mbits_idx], &capacity_mbits));
            int have_bw = (bus_width_idx >= 0 && jz_json_token_to_uint(json, &toks[bus_width_idx], &bus_width));

            if (is_sdram && have_cap) {
                fprintf(out, "  %u Mbits", capacity_mbits);
                if (have_bw) {
                    fprintf(out, " | %u-bit bus", bus_width);
                }
                if (have_mhz) {
                    fprintf(out, " | Max %u MHz", max_freq);
                }
                fprintf(out, "\n\n");
            } else if (is_dist && have_total && have_r && have_w) {
                fprintf(out, "  ");
                jz_print_kbits(out, ((double)total_bits) / 1000.0);
                fprintf(out, " total | %u read port%s, %u write port%s\n\n",
                        r_ports, (r_ports == 1 ? "" : "s"),
                        w_ports, (w_ports == 1 ? "" : "s"));
            } else if (is_flash && have_total) {
                fprintf(out, "  ");
                jz_print_kbits(out, ((double)total_bits) / 1000.0);
                if (have_qty) {
                    fprintf(out, " total | %u block%s", quantity,
                            (quantity == 1 ? "" : "s"));
                }
                fprintf(out, "\n\n");
            } else if (is_block && have_total) {
                fprintf(out, "  ");
                jz_print_kbits(out, ((double)total_bits) / 1000.0);
                if (have_qty && have_bpb) {
                    double bpb_kbits = ((double)bits_per_block) / 1024.0;
                    fprintf(out, " total | %u blocks x ", quantity);
                    jz_print_kbits(out, bpb_kbits);
                }
                if (have_mhz) {
                    fprintf(out, " | Max %u MHz", max_freq);
                }
                fprintf(out, "\n\n");
            } else {
                if (have_total) {
                    fprintf(out, "  ");
                    jz_print_kbits(out, ((double)total_bits) / 1000.0);
                    fprintf(out, " total\n\n");
                } else {
                    fprintf(out, "\n");
                }
            }

            if (modes_idx >= 0 && toks[modes_idx].type == JSMN_ARRAY) {
                JZBuffer modes = (JZBuffer){0};
                jz_collect_modes(json, toks, tok_count, modes_idx, &modes);

                size_t mode_count = modes.len / sizeof(JZChipModeInfo);
                JZChipModeInfo *mode_list = (JZChipModeInfo *)modes.data;
                for (size_t group = 0; group < mode_count; group += 3) {
                    size_t group_count = mode_count - group;
                    if (group_count > 3) group_count = 3;

                    char name_bufs[3][256];
                    size_t col_widths[3] = {0, 0, 0};
                    size_t rows = 0;

                    for (size_t i = 0; i < group_count; ++i) {
                        memset(name_bufs[i], 0, sizeof(name_bufs[i]));
                        size_t len = jz_mode_display_name(&mode_list[group + i],
                                                          (int)(group + i + 1),
                                                          name_bufs[i],
                                                          sizeof(name_bufs[i]));
                        size_t width = (len > 13) ? len : 13;
                        col_widths[i] = width;
                        size_t cfgs = jz_mode_config_count(&mode_list[group + i]);
                        if (cfgs > rows) rows = cfgs;
                    }

                    fputs("  ", out);
                    for (size_t i = 0; i < group_count; ++i) {
                        jz_print_padded(out, name_bufs[i], col_widths[i]);
                        if (i + 1 < group_count) {
                            fputs("  ", out);
                        }
                    }
                    fputc('\n', out);

                    fputs("  ", out);
                    for (size_t i = 0; i < group_count; ++i) {
                        jz_print_padded(out, "Width | Depth", col_widths[i]);
                        if (i + 1 < group_count) {
                            fputs("  ", out);
                        }
                    }
                    fputc('\n', out);

                    fputs("  ", out);
                    for (size_t i = 0; i < group_count; ++i) {
                        jz_print_padded(out, "------|------", col_widths[i]);
                        if (i + 1 < group_count) {
                            fputs("  ", out);
                        }
                    }
                    fputc('\n', out);

                    for (size_t row = 0; row < rows; ++row) {
                        fputs("  ", out);
                        for (size_t col = 0; col < group_count; ++col) {
                            size_t cfg_count = jz_mode_config_count(&mode_list[group + col]);
                            const JZChipDim *dims = (const JZChipDim *)mode_list[group + col].configs.data;
                            if (row < cfg_count && dims) {
                                char cell[32];
                                snprintf(cell, sizeof(cell), "%5u | %5u",
                                         dims[row].width, dims[row].depth);
                                jz_print_padded(out, cell, col_widths[col]);
                            } else {
                                jz_print_padded(out, "", col_widths[col]);
                            }
                            if (col + 1 < group_count) {
                                fputs("  ", out);
                            }
                        }
                        fputc('\n', out);
                    }
                    fputc('\n', out);
                }

                for (size_t i = 0; i < mode_count; ++i) {
                    jz_mode_info_free(&mode_list[i]);
                }
                jz_buf_free(&modes);
            } else if (configs_idx >= 0) {
                jz_print_width_depth_table(json, toks, tok_count, configs_idx, out, "  ");
                fprintf(out, "\n");
            }

            cur = jz_json_skip(toks, tok_count, cur);
        }
    }

    if (clock_gen_idx >= 0) {
        jz_print_section_heading(out, "Clock Generation");
        jz_print_clock_gen_info(json, toks, tok_count, clock_gen_idx, out);
    }

    /* Differential I/O section */
    if (differential_idx >= 0 && toks[differential_idx].type == JSMN_OBJECT) {
        jz_print_section_heading(out, "Differential I/O");
        char diff_type[64] = {0};
        char io_type[64] = {0};
        jz_json_object_get_string(json, toks, tok_count, differential_idx,
                                  "type", diff_type, sizeof(diff_type));
        jz_json_object_get_string(json, toks, tok_count, differential_idx,
                                  "io_type", io_type, sizeof(io_type));
        if (diff_type[0]) {
            fprintf(out, "  Type:     %s%s\n",
                    (jz_strcasecmp(diff_type, "true") == 0) ? "True LVDS" :
                    (jz_strcasecmp(diff_type, "emulated") == 0) ? "Emulated LVDS" :
                    diff_type,
                    io_type[0] ? "" : "");
        }
        if (io_type[0]) {
            fprintf(out, "  IO Type:  %s\n", io_type);
        }

        int output_idx = jz_json_object_get(json, toks, tok_count,
                                             differential_idx, "output");
        if (output_idx >= 0 && toks[output_idx].type == JSMN_OBJECT) {
            int buf_idx = jz_json_object_get(json, toks, tok_count,
                                              output_idx, "buffer");
            int ser_idx = jz_json_object_get(json, toks, tok_count,
                                              output_idx, "serializer");
            if (buf_idx >= 0) {
                char buf_desc[256] = {0};
                jz_json_object_get_string(json, toks, tok_count,
                                          buf_idx, "description",
                                          buf_desc, sizeof(buf_desc));
                if (buf_desc[0]) {
                    fprintf(out, "  Output:   %s\n", buf_desc);
                }
            }
            if (ser_idx >= 0) {
                char ser_desc[256] = {0};
                jz_json_object_get_string(json, toks, tok_count,
                                          ser_idx, "description",
                                          ser_desc, sizeof(ser_desc));
                unsigned ratio = 0;
                int ratio_idx = jz_json_object_get(json, toks, tok_count,
                                                    ser_idx, "ratio");
                int have_ratio = (ratio_idx >= 0 &&
                                  jz_json_token_to_uint(json, &toks[ratio_idx], &ratio));
                fprintf(out, "  Serializer:");
                if (have_ratio) fprintf(out, " %u:1", ratio);
                if (ser_desc[0]) fprintf(out, " (%s)", ser_desc);
                fprintf(out, "\n");
            }
        }

        int input_idx = jz_json_object_get(json, toks, tok_count,
                                            differential_idx, "input");
        if (input_idx >= 0 && toks[input_idx].type == JSMN_OBJECT) {
            int ibuf_idx = jz_json_object_get(json, toks, tok_count,
                                               input_idx, "buffer");
            int deser_idx = jz_json_object_get(json, toks, tok_count,
                                                input_idx, "deserializer");
            if (ibuf_idx >= 0) {
                char ibuf_desc[256] = {0};
                jz_json_object_get_string(json, toks, tok_count,
                                          ibuf_idx, "description",
                                          ibuf_desc, sizeof(ibuf_desc));
                if (ibuf_desc[0]) {
                    fprintf(out, "  Input:    %s\n", ibuf_desc);
                }
            }
            if (deser_idx >= 0) {
                char deser_desc[256] = {0};
                jz_json_object_get_string(json, toks, tok_count,
                                          deser_idx, "description",
                                          deser_desc, sizeof(deser_desc));
                unsigned ratio = 0;
                int ratio_idx = jz_json_object_get(json, toks, tok_count,
                                                    deser_idx, "ratio");
                int have_ratio = (ratio_idx >= 0 &&
                                  jz_json_token_to_uint(json, &toks[ratio_idx], &ratio));
                fprintf(out, "  Deserializer:");
                if (have_ratio) fprintf(out, " 1:%u", ratio);
                if (deser_desc[0]) fprintf(out, " (%s)", deser_desc);
                fprintf(out, "\n");
            }
        }

        fprintf(out, "\n");
    }

    if (fixed_pins_idx >= 0 && toks[fixed_pins_idx].type == JSMN_ARRAY) {
        typedef struct {
            char pad[32];
            char name[64];
            char pin[16];
            char ball[16];
            char note[64];
        } PinEntry;

        PinEntry pins[256];
        size_t pin_count = 0;
        size_t max_pad = 3;   /* min width for "Pad" header */
        size_t max_name = 4;  /* min width for "Name" header */
        size_t max_pin = 3;   /* min width for "Pin" header */
        size_t max_ball = 4;  /* min width for "Ball" header */
        size_t max_note = 4;  /* min width for "Note" header */
        int has_pin = 0;
        int has_ball = 0;
        int has_note = 0;

        const jsmntok_t *arr = &toks[fixed_pins_idx];
        int cur = fixed_pins_idx + 1;
        while (cur < tok_count && toks[cur].start < arr->end) {
            const jsmntok_t *obj = &toks[cur];
            if (obj->type != JSMN_OBJECT || pin_count >= 256) {
                cur = jz_json_skip(toks, tok_count, cur);
                continue;
            }
            PinEntry *p = &pins[pin_count];
            p->pad[0] = '\0';
            p->name[0] = '\0';
            p->pin[0] = '\0';
            p->ball[0] = '\0';
            p->note[0] = '\0';
            int oidx = cur + 1;
            while (oidx < tok_count && toks[oidx].start < obj->end) {
                const jsmntok_t *key = &toks[oidx++];
                const jsmntok_t *val = &toks[oidx];
                if (jz_json_token_eq(json, key, "pad")) {
                    (void)jz_json_token_to_string(json, val,
                                                   p->pad, sizeof(p->pad));
                } else if (jz_json_token_eq(json, key, "name")) {
                    (void)jz_json_token_to_string(json, val,
                                                   p->name, sizeof(p->name));
                } else if (jz_json_token_eq(json, key, "pin")) {
                    (void)jz_json_token_to_string(json, val,
                                                   p->pin, sizeof(p->pin));
                    if (!p->pin[0] && val->type == JSMN_PRIMITIVE) {
                        /* Handle numeric pin values */
                        size_t len = (size_t)(val->end - val->start);
                        if (len < sizeof(p->pin)) {
                            memcpy(p->pin, json + val->start, len);
                            p->pin[len] = '\0';
                        }
                    }
                } else if (jz_json_token_eq(json, key, "ball")) {
                    (void)jz_json_token_to_string(json, val,
                                                   p->ball, sizeof(p->ball));
                } else if (jz_json_token_eq(json, key, "note")) {
                    (void)jz_json_token_to_string(json, val,
                                                   p->note, sizeof(p->note));
                }
                oidx = jz_json_skip(toks, tok_count, oidx);
            }
            if (p->pad[0] || p->name[0]) {
                size_t pl = strlen(p->pad);
                size_t nl = strlen(p->name);
                if (pl > max_pad) max_pad = pl;
                if (nl > max_name) max_name = nl;
                if (p->pin[0]) {
                    has_pin = 1;
                    size_t l = strlen(p->pin);
                    if (l > max_pin) max_pin = l;
                }
                if (p->ball[0]) {
                    has_ball = 1;
                    size_t l = strlen(p->ball);
                    if (l > max_ball) max_ball = l;
                }
                if (p->note[0]) {
                    has_note = 1;
                    size_t l = strlen(p->note);
                    if (l > max_note) max_note = l;
                }
                pin_count++;
            }
            cur = jz_json_skip(toks, tok_count, cur);
        }

        if (pin_count > 0) {
            {
                char heading_buf[64];
                snprintf(heading_buf, sizeof(heading_buf), "Fixed Pins (%zu)", pin_count);
                jz_print_section_heading(out, heading_buf);
            }

            /* Single-column table with all fields */
            /* Header */
            fprintf(out, "%-*s | %-*s",
                    (int)max_pad, "Pad", (int)max_name, "Name");
            if (has_pin)  fprintf(out, " | %-*s", (int)max_pin, "Pin");
            if (has_ball) fprintf(out, " | %-*s", (int)max_ball, "Ball");
            if (has_note) fprintf(out, " | %s", "Note");
            fputc('\n', out);

            /* Separator */
            for (size_t k = 0; k < max_pad; ++k) fputc('-', out);
            fputs("-+-", out);
            for (size_t k = 0; k < max_name; ++k) fputc('-', out);
            if (has_pin) {
                fputs("-+-", out);
                for (size_t k = 0; k < max_pin; ++k) fputc('-', out);
            }
            if (has_ball) {
                fputs("-+-", out);
                for (size_t k = 0; k < max_ball; ++k) fputc('-', out);
            }
            if (has_note) {
                fputs("-+-", out);
                for (size_t k = 0; k < max_note; ++k) fputc('-', out);
            }
            fputc('\n', out);

            /* Data rows */
            for (size_t i = 0; i < pin_count; ++i) {
                fprintf(out, "%-*s | %-*s",
                        (int)max_pad, pins[i].pad,
                        (int)max_name, pins[i].name);
                if (has_pin) {
                    fprintf(out, " | %-*s", (int)max_pin,
                            pins[i].pin[0] ? pins[i].pin : "   ");
                }
                if (has_ball) {
                    fprintf(out, " | %-*s", (int)max_ball,
                            pins[i].ball[0] ? pins[i].ball : "   ");
                }
                if (has_note) {
                    fprintf(out, " | %s",
                            pins[i].note[0] ? pins[i].note : "");
                }
                fputc('\n', out);
            }
            fputc('\n', out);
        }
    }

    /* Boards section (optional) */
    if (boards_idx >= 0 && toks[boards_idx].type == JSMN_ARRAY) {
        const jsmntok_t *arr = &toks[boards_idx];
        /* Count boards first */
        size_t board_count = 0;
        int cur = boards_idx + 1;
        while (cur < tok_count && toks[cur].start < arr->end) {
            if (toks[cur].type == JSMN_OBJECT) board_count++;
            cur = jz_json_skip(toks, tok_count, cur);
        }
        if (board_count > 0) {
            char heading_buf[64];
            snprintf(heading_buf, sizeof(heading_buf), "Example Boards (%zu)", board_count);
            jz_print_section_heading(out, heading_buf);

            cur = boards_idx + 1;
            while (cur < tok_count && toks[cur].start < arr->end) {
                if (toks[cur].type != JSMN_OBJECT) {
                    cur = jz_json_skip(toks, tok_count, cur);
                    continue;
                }
                char bname[128] = {0};
                char burl[256] = {0};
                (void)jz_json_object_get_string(json, toks, tok_count, cur,
                                                "name", bname, sizeof(bname));
                (void)jz_json_object_get_string(json, toks, tok_count, cur,
                                                "url", burl, sizeof(burl));
                if (bname[0]) {
                    fprintf(out, "  %s", bname);
                    if (burl[0]) {
                        fprintf(out, "\n    %s", burl);
                    }
                    fprintf(out, "\n");
                }
                cur = jz_json_skip(toks, tok_count, cur);
            }
            fprintf(out, "\n");
        }
    }

    free(toks);
    return 0;
}
