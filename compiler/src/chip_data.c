#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chip_data.h"
#include "util.h"

#define JSMN_IMPLEMENTATION
#include "third_party/jsmn.h"

/* Last chip-data load error, for detailed diagnostics via
 * jz_chip_data_last_error(). Reset at the start of each load attempt. */
static char g_chip_last_error[512];

const char *jz_chip_data_last_error(void)
{
    return g_chip_last_error[0] ? g_chip_last_error : NULL;
}

static void jz_chip_clear_error(void)
{
    g_chip_last_error[0] = '\0';
}

static void jz_chip_set_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_chip_last_error, sizeof(g_chip_last_error), fmt, ap);
    va_end(ap);
}

typedef struct JZChipBuiltin {
    const char *chip_id;
    const char *json;
} JZChipBuiltin;

/* Built-in chip data generated at build time. */
#include "data/gw1nr-9-qn88-c6-i5.h"
#include "data/gw1nz-1-qn48-c6-i5.h"
#include "data/gw2ar-18-qn88-c7-i6.h"
#include "data/gw2ar-18-qn88-c8-i7.h"
#include "data/ice40up-5k-sg.h"
#include "data/ice40up-5k-uwg.h"
#include "data/lfe5u-45f-6bg381.h"
#include "data/xc7a35t-2fgg484.h"

static const JZChipBuiltin k_builtin_chips[] = {
    { "GW1NR-9-QN88-C6-I5",  (const char *)gw1nr_9_qn88_c6_i5_json },
    { "GW1NZ-1-QN48-C6-I5",  (const char *)gw1nz_1_qn48_c6_i5_json },
    { "GW2AR-18-QN88-C7-I6", (const char *)gw2ar_18_qn88_c7_i6_json },
    { "GW2AR-18-QN88-C8-I7", (const char *)gw2ar_18_qn88_c8_i7_json },
    { "ICE40UP-5K-SG48",     (const char *)ice40up_5k_sg_json },
    { "ICE40UP-5K-UWG30",    (const char *)ice40up_5k_uwg_json },
    { "LFE5U-45F-6BG381",    (const char *)lfe5u_45f_6bg381_json },
    { "XC7A35T-2FGG484",     (const char *)xc7a35t_2fgg484_json }
};

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

static char *jz_strdup_lower(const char *s)
{
    if (!s) return NULL;
    size_t len = strlen(s);
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < len; ++i) {
        out[i] = (char)tolower((unsigned char)s[i]);
    }
    out[len] = '\0';
    return out;
}

static char *jz_strdup_upper(const char *s)
{
    if (!s) return NULL;
    size_t len = strlen(s);
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < len; ++i) {
        out[i] = (char)toupper((unsigned char)s[i]);
    }
    out[len] = '\0';
    return out;
}

static char *jz_build_chip_json_path(const char *project_filename,
                                     const char *chip_id)
{
    if (!chip_id || !chip_id[0]) return NULL;
    const char *slash = NULL;
    if (project_filename) {
        slash = strrchr(project_filename, '/');
        const char *bslash = strrchr(project_filename, '\\');
        if (!slash || (bslash && bslash > slash)) {
            slash = bslash;
        }
    }

    const char *dir = ".";
    size_t dir_len = 1;
    if (slash) {
        dir = project_filename;
        dir_len = (size_t)(slash - project_filename);
    }

    size_t chip_len = strlen(chip_id);
    size_t total = dir_len + 1 + chip_len + 5 + 1;
    char *out = (char *)malloc(total);
    if (!out) return NULL;

    memcpy(out, dir, dir_len);
    out[dir_len] = '\0';
    strcat(out, "/");
    strcat(out, chip_id);
    strcat(out, ".json");
    return out;
}

/* Check if chip_id is a prefix of target (case-insensitive).
 * E.g., "GW2AR-18" is a prefix of "GW2AR-18-QN88-C8-I7".
 */
static int jz_chip_id_is_prefix(const char *chip_id, const char *target)
{
    if (!chip_id || !target) return 0;
    size_t prefix_len = strlen(chip_id);
    size_t target_len = strlen(target);
    if (prefix_len > target_len) return 0;
    for (size_t i = 0; i < prefix_len; ++i) {
        if (tolower((unsigned char)chip_id[i]) != tolower((unsigned char)target[i])) {
            return 0;
        }
    }
    /* Prefix must end at a word boundary (end of string or hyphen) */
    if (prefix_len == target_len) return 1;
    return target[prefix_len] == '-';
}

const char *jz_chip_builtin_json(const char *chip_id)
{
    if (!chip_id) return NULL;
    size_t count = sizeof(k_builtin_chips) / sizeof(k_builtin_chips[0]);

    /* First try exact match */
    for (size_t i = 0; i < count; ++i) {
        if (jz_strcasecmp(chip_id, k_builtin_chips[i].chip_id) == 0) {
            return k_builtin_chips[i].json;
        }
    }

    /* Then try prefix match (e.g., "GW2AR-18" matches "GW2AR-18-QN88-C8-I7") */
    for (size_t i = 0; i < count; ++i) {
        if (jz_chip_id_is_prefix(chip_id, k_builtin_chips[i].chip_id)) {
            return k_builtin_chips[i].json;
        }
    }

    return NULL;
}

int jz_json_token_to_bool(const char *json, const jsmntok_t *tok, int *out)
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

int jz_json_token_eq(const char *json, const jsmntok_t *tok, const char *s)
{
    if (!json || !tok || tok->type != JSMN_STRING || !s) return 0;
    size_t len = (size_t)(tok->end - tok->start);
    return strlen(s) == len && strncmp(json + tok->start, s, len) == 0;
}

int jz_json_token_eq_ci(const char *json, const jsmntok_t *tok, const char *s)
{
    if (!json || !tok || tok->type != JSMN_STRING || !s) return 0;
    size_t len = (size_t)(tok->end - tok->start);
    if (strlen(s) != len) return 0;
    const char *p = json + tok->start;
    for (size_t i = 0; i < len; ++i) {
        if (tolower((unsigned char)p[i]) != tolower((unsigned char)s[i])) {
            return 0;
        }
    }
    return 1;
}

int jz_json_token_to_uint(const char *json, const jsmntok_t *tok, unsigned *out)
{
    if (!json || !tok || !out) return 0;
    if (tok->type != JSMN_PRIMITIVE && tok->type != JSMN_STRING) return 0;
    size_t len = (size_t)(tok->end - tok->start);
    if (len == 0 || len > 31) return 0;
    char buf[32];
    memcpy(buf, json + tok->start, len);
    buf[len] = '\0';
    char *end = NULL;
    unsigned long v = strtoul(buf, &end, 10);
    if (!end || *end != '\0') return 0;
    if (v > 0xFFFFFFFFu) return 0;
    *out = (unsigned)v;
    return 1;
}

int jz_json_skip(const jsmntok_t *toks, int count, int index)
{
    if (!toks || index >= count) return index;
    int next = index + 1;
    if (toks[index].type == JSMN_OBJECT || toks[index].type == JSMN_ARRAY) {
        int end = toks[index].end;
        while (next < count && toks[next].start < end) {
            next = jz_json_skip(toks, count, next);
        }
    }
    return next;
}

const char *jz_chip_mem_type_name(JZChipMemType type)
{
    switch (type) {
    case JZ_CHIP_MEM_DISTRIBUTED: return "DISTRIBUTED";
    case JZ_CHIP_MEM_BLOCK:       return "BLOCK";
    case JZ_CHIP_MEM_SDRAM:       return "SDRAM";
    case JZ_CHIP_MEM_FLASH:       return "FLASH";
    case JZ_CHIP_MEM_SPRAM:       return "SPRAM";
    default:                      return "UNKNOWN";
    }
}

size_t jz_chip_builtin_count(void)
{
    return sizeof(k_builtin_chips) / sizeof(k_builtin_chips[0]);
}

const char *jz_chip_builtin_id(size_t index)
{
    size_t count = jz_chip_builtin_count();
    if (index >= count) return NULL;
    return k_builtin_chips[index].chip_id;
}

static JZChipMemType jz_chip_mem_type_from_token(const char *json,
                                                 const jsmntok_t *tok)
{
    if (!tok || tok->type != JSMN_STRING) return JZ_CHIP_MEM_UNKNOWN;
    if (jz_json_token_eq_ci(json, tok, "DISTRIBUTED")) return JZ_CHIP_MEM_DISTRIBUTED;
    if (jz_json_token_eq_ci(json, tok, "BLOCK")) return JZ_CHIP_MEM_BLOCK;
    if (jz_json_token_eq_ci(json, tok, "SDRAM")) return JZ_CHIP_MEM_SDRAM;
    if (jz_json_token_eq_ci(json, tok, "FLASH")) return JZ_CHIP_MEM_FLASH;
    if (jz_json_token_eq_ci(json, tok, "SPRAM")) return JZ_CHIP_MEM_SPRAM;
    return JZ_CHIP_MEM_UNKNOWN;
}

static int jz_chip_add_mem_config(JZChipData *out,
                                  JZChipMemType type,
                                  unsigned r_ports,
                                  unsigned w_ports,
                                  unsigned width,
                                  unsigned depth)
{
    if (!out || type == JZ_CHIP_MEM_UNKNOWN || width == 0 || depth == 0) {
        return 0;
    }
    JZChipMemConfig cfg;
    cfg.type = type;
    cfg.r_ports = r_ports;
    cfg.w_ports = w_ports;
    cfg.width = width;
    cfg.depth = depth;
    return jz_buf_append(&out->mem_configs, &cfg, sizeof(cfg)) == 0;
}

static void jz_chip_parse_config_array(const char *json,
                                       const jsmntok_t *toks,
                                       int count,
                                       int array_index,
                                       JZChipMemType type,
                                       unsigned r_ports,
                                       unsigned w_ports,
                                       JZChipData *out)
{
    if (!json || !toks || array_index < 0 || array_index >= count || !out) return;
    const jsmntok_t *arr = &toks[array_index];
    if (arr->type != JSMN_ARRAY) return;
    int idx = array_index + 1;
    for (int i = 0; i < arr->size; ++i) {
        if (idx >= count) return;
        const jsmntok_t *obj = &toks[idx];
        if (obj->type != JSMN_OBJECT) {
            idx = jz_json_skip(toks, count, idx);
            continue;
        }
        unsigned width = 0;
        unsigned depth = 0;
        int cur = idx + 1;
        while (cur < count && toks[cur].start < obj->end) {
            const jsmntok_t *key = &toks[cur++];
            const jsmntok_t *val = &toks[cur];
            if (jz_json_token_eq(json, key, "width")) {
                (void)jz_json_token_to_uint(json, val, &width);
            } else if (jz_json_token_eq(json, key, "depth")) {
                (void)jz_json_token_to_uint(json, val, &depth);
            }
            cur = jz_json_skip(toks, count, cur);
        }
        if (width > 0 && depth > 0) {
            jz_chip_add_mem_config(out, type, r_ports, w_ports, width, depth);
        }
        idx = jz_json_skip(toks, count, idx);
    }
}

/* Parse a "ports" array like: [{"id":"A","read":true,"write":true}, ...]
 * Returns 1 if successfully parsed, 0 otherwise.
 */
static int jz_chip_parse_ports_array(const char *json,
                                     const jsmntok_t *toks,
                                     int count,
                                     int array_index,
                                     unsigned *out_r_ports,
                                     unsigned *out_w_ports)
{
    if (!json || !toks || array_index < 0 || array_index >= count) return 0;
    const jsmntok_t *arr = &toks[array_index];
    if (arr->type != JSMN_ARRAY) return 0;

    unsigned r_ports = 0;
    unsigned w_ports = 0;
    int idx = array_index + 1;

    for (int i = 0; i < arr->size; ++i) {
        if (idx >= count) break;
        const jsmntok_t *port_obj = &toks[idx];
        if (port_obj->type != JSMN_OBJECT) {
            idx = jz_json_skip(toks, count, idx);
            continue;
        }

        int has_read = 0, has_write = 0;
        int cur = idx + 1;
        while (cur < count && toks[cur].start < port_obj->end) {
            const jsmntok_t *key = &toks[cur++];
            const jsmntok_t *val = &toks[cur];
            if (jz_json_token_eq(json, key, "read")) {
                if (val->type == JSMN_PRIMITIVE && json[val->start] == 't') {
                    has_read = 1;
                }
            } else if (jz_json_token_eq(json, key, "write")) {
                if (val->type == JSMN_PRIMITIVE && json[val->start] == 't') {
                    has_write = 1;
                }
            }
            cur = jz_json_skip(toks, count, cur);
        }

        if (has_read) r_ports++;
        if (has_write) w_ports++;
        idx = jz_json_skip(toks, count, idx);
    }

    if (out_r_ports) *out_r_ports = r_ports;
    if (out_w_ports) *out_w_ports = w_ports;
    return 1;
}

static void jz_chip_parse_modes_array(const char *json,
                                      const jsmntok_t *toks,
                                      int count,
                                      int array_index,
                                      JZChipMemType type,
                                      JZChipData *out)
{
    if (!json || !toks || array_index < 0 || array_index >= count || !out) return;
    const jsmntok_t *arr = &toks[array_index];
    if (arr->type != JSMN_ARRAY) return;
    int idx = array_index + 1;
    for (int i = 0; i < arr->size; ++i) {
        if (idx >= count) return;
        const jsmntok_t *obj = &toks[idx];
        if (obj->type != JSMN_OBJECT) {
            idx = jz_json_skip(toks, count, idx);
            continue;
        }
        unsigned r_ports = 0;
        unsigned w_ports = 0;
        int have_ports = 0;
        int config_idx = -1;
        int ports_idx = -1;

        int cur = idx + 1;
        while (cur < count && toks[cur].start < obj->end) {
            const jsmntok_t *key = &toks[cur++];
            const jsmntok_t *val = &toks[cur];
            if (jz_json_token_eq(json, key, "r_ports")) {
                /* Legacy format: r_ports as integer */
                if (jz_json_token_to_uint(json, val, &r_ports)) {
                    have_ports = 1;
                }
            } else if (jz_json_token_eq(json, key, "w_ports")) {
                /* Legacy format: w_ports as integer */
                unsigned tmp = 0;
                if (jz_json_token_to_uint(json, val, &tmp)) {
                    w_ports = tmp;
                    have_ports = 1;
                }
            } else if (jz_json_token_eq(json, key, "ports")) {
                /* New format: ports array */
                ports_idx = cur;
            } else if (jz_json_token_eq(json, key, "configurations")) {
                config_idx = cur;
            }
            cur = jz_json_skip(toks, count, cur);
        }

        /* Parse new-style ports array if present */
        if (ports_idx >= 0) {
            if (jz_chip_parse_ports_array(json, toks, count, ports_idx,
                                          &r_ports, &w_ports)) {
                have_ports = 1;
            }
        }

        if (have_ports && config_idx >= 0) {
            jz_chip_parse_config_array(json, toks, count, config_idx,
                                       type, r_ports, w_ports, out);
        }
        idx = jz_json_skip(toks, count, idx);
    }
}

static int jz_chip_parse_memory_object(const char *json,
                                       const jsmntok_t *toks,
                                       int count,
                                       int obj_index,
                                       JZChipData *out)
{
    const jsmntok_t *obj = &toks[obj_index];
    if (obj->type != JSMN_OBJECT) return jz_json_skip(toks, count, obj_index);

    JZChipMemType type = JZ_CHIP_MEM_UNKNOWN;
    unsigned r_ports = 0;
    unsigned w_ports = 0;
    int have_r = 0;
    int have_w = 0;
    int configs_idx = -1;
    int modes_idx = -1;
    unsigned quantity = 0;
    unsigned bits_per_block = 0;
    unsigned total_bits = 0;
    int have_quantity = 0;
    int have_bits_per_block = 0;
    int have_total_bits = 0;

    int cur = obj_index + 1;
    while (cur < count && toks[cur].start < obj->end) {
        const jsmntok_t *key = &toks[cur++];
        const jsmntok_t *val = &toks[cur];
        if (jz_json_token_eq(json, key, "type")) {
            type = jz_chip_mem_type_from_token(json, val);
        } else if (jz_json_token_eq(json, key, "r_ports")) {
            have_r = jz_json_token_to_uint(json, val, &r_ports);
        } else if (jz_json_token_eq(json, key, "w_ports")) {
            have_w = jz_json_token_to_uint(json, val, &w_ports);
        } else if (jz_json_token_eq(json, key, "configurations")) {
            configs_idx = cur;
        } else if (jz_json_token_eq(json, key, "modes")) {
            modes_idx = cur;
        } else if (jz_json_token_eq(json, key, "quantity")) {
            have_quantity = jz_json_token_to_uint(json, val, &quantity);
        } else if (jz_json_token_eq(json, key, "bits_per_block")) {
            have_bits_per_block = jz_json_token_to_uint(json, val, &bits_per_block);
        } else if (jz_json_token_eq(json, key, "total_bits")) {
            have_total_bits = jz_json_token_to_uint(json, val, &total_bits);
        }
        cur = jz_json_skip(toks, count, cur);
    }

    if (type != JZ_CHIP_MEM_UNKNOWN && have_r && have_w && configs_idx >= 0) {
        jz_chip_parse_config_array(json, toks, count, configs_idx,
                                   type, r_ports, w_ports, out);
    }
    if (type != JZ_CHIP_MEM_UNKNOWN && modes_idx >= 0) {
        jz_chip_parse_modes_array(json, toks, count, modes_idx, type, out);
    }

    /* Store resource limits for this memory type. */
    if (type != JZ_CHIP_MEM_UNKNOWN && have_total_bits) {
        JZChipMemResource res;
        res.type = type;
        res.quantity = have_quantity ? quantity : 0;
        res.bits_per_block = have_bits_per_block ? bits_per_block : 0;
        res.total_bits = total_bits;
        jz_buf_append(&out->mem_resources, &res, sizeof(res));
    }

    return jz_json_skip(toks, count, obj_index);
}

static int jz_chip_parse_memory(const char *json,
                                const jsmntok_t *toks,
                                int count,
                                JZChipData *out)
{
    if (!json || !toks || count < 1 || !out) return -1;
    if (toks[0].type != JSMN_OBJECT) return -1;

    int idx = 1;
    int memory_idx = -1;
    while (idx < count && toks[idx].start < toks[0].end) {
        const jsmntok_t *key = &toks[idx++];
        if (jz_json_token_eq(json, key, "memory")) {
            memory_idx = idx;
        }
        idx = jz_json_skip(toks, count, idx);
    }

    if (memory_idx < 0) return -1;
    if (toks[memory_idx].type != JSMN_ARRAY) return -1;

    int cur = memory_idx + 1;
    for (int i = 0; i < toks[memory_idx].size; ++i) {
        if (cur >= count) break;
        cur = jz_chip_parse_memory_object(json, toks, count, cur, out);
    }

    return (out->mem_configs.len > 0) ? 0 : -1;
}

/* Helper to extract a string token as a newly allocated string. */
char *jz_json_token_strdup(const char *json, const jsmntok_t *tok)
{
    if (!json || !tok || tok->type != JSMN_STRING) return NULL;
    size_t len = (size_t)(tok->end - tok->start);
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, json + tok->start, len);
    out[len] = '\0';
    return out;
}

/* Parse a clock_gen map object: { "verilog-2005": ["line1", "line2", ...] } */
/* Parse a clock_gen `map` object into the given JZBuffer of JZChipClockGenMap.
 * Used for both the legacy top-level `map` and each per-variant `map`. */
static void jz_chip_parse_clock_gen_map_into(const char *json,
                                              const jsmntok_t *toks,
                                              int count,
                                              int map_obj_idx,
                                              JZBuffer *out_maps)
{
    if (!json || !toks || map_obj_idx < 0 || map_obj_idx >= count || !out_maps) return;
    const jsmntok_t *map_obj = &toks[map_obj_idx];
    if (map_obj->type != JSMN_OBJECT) return;

    int cur = map_obj_idx + 1;
    while (cur < count && toks[cur].start < map_obj->end) {
        const jsmntok_t *key = &toks[cur++];
        if (key->type != JSMN_STRING) {
            cur = jz_json_skip(toks, count, cur);
            continue;
        }
        const jsmntok_t *val = &toks[cur];
        if (val->type != JSMN_ARRAY) {
            cur = jz_json_skip(toks, count, cur);
            continue;
        }

        /* Extract backend name (the key) */
        char *backend = jz_json_token_strdup(json, key);
        if (!backend) {
            cur = jz_json_skip(toks, count, cur);
            continue;
        }

        /* Concatenate all array elements into one template string */
        size_t total_len = 0;
        int arr_idx = cur + 1;
        for (int i = 0; i < val->size && arr_idx < count; ++i) {
            const jsmntok_t *elem = &toks[arr_idx];
            if (elem->type == JSMN_STRING) {
                total_len += (size_t)(elem->end - elem->start);
            }
            arr_idx = jz_json_skip(toks, count, arr_idx);
        }

        char *template_text = (char *)malloc(total_len + 1);
        if (template_text) {
            size_t offset = 0;
            arr_idx = cur + 1;
            for (int i = 0; i < val->size && arr_idx < count; ++i) {
                const jsmntok_t *elem = &toks[arr_idx];
                if (elem->type == JSMN_STRING) {
                    size_t len = (size_t)(elem->end - elem->start);
                    memcpy(template_text + offset, json + elem->start, len);
                    offset += len;
                }
                arr_idx = jz_json_skip(toks, count, arr_idx);
            }
            template_text[offset] = '\0';

            JZChipClockGenMap map_entry;
            map_entry.backend = backend;
            map_entry.template_text = template_text;
            jz_buf_append(out_maps, &map_entry, sizeof(map_entry));
        } else {
            free(backend);
        }

        cur = jz_json_skip(toks, count, cur);
    }
}

/* Backward-compat wrapper: parse into cg->maps (legacy top-level `map`). */
static void jz_chip_parse_clock_gen_map(const char *json,
                                        const jsmntok_t *toks,
                                        int count,
                                        int map_obj_idx,
                                        JZChipClockGen *cg)
{
    if (!cg) return;
    jz_chip_parse_clock_gen_map_into(json, toks, count, map_obj_idx, &cg->maps);
}

/* Parse the derived section of a clock_gen object.
 * The derived section is an object like:
 *   "derived": { "PSDA_SEL": { "expr": "toString(PHASESEL, BIN, 4)" }, ... }
 */
static void jz_chip_parse_clock_gen_derived(const char *json,
                                            const jsmntok_t *toks,
                                            int count,
                                            int derived_obj_idx,
                                            JZChipClockGen *cg)
{
    if (!json || !toks || derived_obj_idx < 0 || derived_obj_idx >= count || !cg) return;
    const jsmntok_t *obj = &toks[derived_obj_idx];
    if (obj->type != JSMN_OBJECT) return;

    int cur = derived_obj_idx + 1;
    while (cur < count && toks[cur].start < obj->end) {
        const jsmntok_t *name_tok = &toks[cur++];
        if (name_tok->type != JSMN_STRING || cur >= count) {
            cur = jz_json_skip(toks, count, cur);
            continue;
        }
        const jsmntok_t *val = &toks[cur];
        if (val->type != JSMN_OBJECT) {
            cur = jz_json_skip(toks, count, cur);
            continue;
        }

        char *name = jz_json_token_strdup(json, name_tok);
        char *expr = NULL;

        /* Scan inner object for "expr", "min", "max" keys */
        int d_has_min = 0, d_has_max = 0;
        double d_min = 0.0, d_max = 0.0;
        int inner = cur + 1;
        while (inner < count && toks[inner].start < val->end) {
            const jsmntok_t *ikey = &toks[inner++];
            if (inner >= count) break;
            const jsmntok_t *ival = &toks[inner];
            if (jz_json_token_eq(json, ikey, "expr")) {
                expr = jz_json_token_strdup(json, ival);
            } else if (jz_json_token_eq(json, ikey, "min")) {
                if (ival->type == JSMN_PRIMITIVE) {
                    size_t len = (size_t)(ival->end - ival->start);
                    char buf[64];
                    if (len < sizeof(buf)) {
                        memcpy(buf, json + ival->start, len);
                        buf[len] = '\0';
                        char *endptr = NULL;
                        d_min = strtod(buf, &endptr);
                        if (endptr != buf) d_has_min = 1;
                    }
                }
            } else if (jz_json_token_eq(json, ikey, "max")) {
                if (ival->type == JSMN_PRIMITIVE) {
                    size_t len = (size_t)(ival->end - ival->start);
                    char buf[64];
                    if (len < sizeof(buf)) {
                        memcpy(buf, json + ival->start, len);
                        buf[len] = '\0';
                        char *endptr = NULL;
                        d_max = strtod(buf, &endptr);
                        if (endptr != buf) d_has_max = 1;
                    }
                }
            }
            inner = jz_json_skip(toks, count, inner);
        }

        if (name && expr) {
            JZChipClockGenDerived d;
            d.name = name;
            d.expr = expr;
            d.has_min = d_has_min;
            d.min = d_min;
            d.has_max = d_has_max;
            d.max = d_max;
            jz_buf_append(&cg->deriveds, &d, sizeof(d));
        } else {
            free(name);
            free(expr);
        }

        cur = jz_json_skip(toks, count, cur);
    }
}

/* Parse the parameters section of a clock_gen object.
 * The parameters section is an object like:
 *   "parameters": { "IDIV": { "default": 1, ... }, "FBDIV": { "default": 1, ... } }
 */
static void jz_chip_parse_clock_gen_params(const char *json,
                                           const jsmntok_t *toks,
                                           int count,
                                           int params_obj_idx,
                                           JZChipClockGen *cg)
{
    if (!json || !toks || params_obj_idx < 0 || params_obj_idx >= count || !cg) return;
    const jsmntok_t *obj = &toks[params_obj_idx];
    if (obj->type != JSMN_OBJECT) return;

    int cur = params_obj_idx + 1;
    while (cur < count && toks[cur].start < obj->end) {
        const jsmntok_t *name_tok = &toks[cur++];
        if (name_tok->type != JSMN_STRING || cur >= count) {
            cur = jz_json_skip(toks, count, cur);
            continue;
        }
        const jsmntok_t *val = &toks[cur];
        if (val->type != JSMN_OBJECT) {
            cur = jz_json_skip(toks, count, cur);
            continue;
        }

        char *name = jz_json_token_strdup(json, name_tok);
        char *default_val = NULL;
        int p_is_double = 0;
        int p_has_min = 0, p_has_max = 0;
        double p_min = 0, p_max = 0;
        char **valid_values = NULL;
        size_t valid_count = 0;

        /* Scan inner object for "type", "default", "min", "max", "valid" keys */
        int inner = cur + 1;
        while (inner < count && toks[inner].start < val->end) {
            const jsmntok_t *ikey = &toks[inner++];
            if (inner >= count) break;
            const jsmntok_t *ival = &toks[inner];
            if (jz_json_token_eq(json, ikey, "type")) {
                if (jz_json_token_eq(json, ival, "double")) {
                    p_is_double = 1;
                }
            } else if (jz_json_token_eq(json, ikey, "default")) {
                /* Default can be a number (primitive) or string */
                if (ival->type == JSMN_PRIMITIVE || ival->type == JSMN_STRING) {
                    size_t len = (size_t)(ival->end - ival->start);
                    default_val = (char *)malloc(len + 1);
                    if (default_val) {
                        memcpy(default_val, json + ival->start, len);
                        default_val[len] = '\0';
                    }
                }
            } else if (jz_json_token_eq(json, ikey, "min")) {
                if (ival->type == JSMN_PRIMITIVE) {
                    size_t len = (size_t)(ival->end - ival->start);
                    char buf[64];
                    if (len < sizeof(buf)) {
                        memcpy(buf, json + ival->start, len);
                        buf[len] = '\0';
                        char *endptr = NULL;
                        p_min = strtod(buf, &endptr);
                        if (endptr != buf) p_has_min = 1;
                    }
                }
            } else if (jz_json_token_eq(json, ikey, "max")) {
                if (ival->type == JSMN_PRIMITIVE) {
                    size_t len = (size_t)(ival->end - ival->start);
                    char buf[64];
                    if (len < sizeof(buf)) {
                        memcpy(buf, json + ival->start, len);
                        buf[len] = '\0';
                        char *endptr = NULL;
                        p_max = strtod(buf, &endptr);
                        if (endptr != buf) p_has_max = 1;
                    }
                }
            } else if (jz_json_token_eq(json, ikey, "valid")) {
                if (ival->type == JSMN_ARRAY && ival->size > 0) {
                    valid_count = (size_t)ival->size;
                    valid_values = (char **)calloc(valid_count, sizeof(char *));
                    if (valid_values) {
                        int vidx = inner + 1;
                        for (size_t vi = 0; vi < valid_count && vidx < count; ++vi) {
                            /* valid array may contain numbers (JSMN_PRIMITIVE) or strings */
                            const jsmntok_t *vtok = &toks[vidx];
                            if (vtok->type == JSMN_STRING || vtok->type == JSMN_PRIMITIVE) {
                                size_t vlen = (size_t)(vtok->end - vtok->start);
                                char *vs = (char *)malloc(vlen + 1);
                                if (vs) {
                                    memcpy(vs, json + vtok->start, vlen);
                                    vs[vlen] = '\0';
                                }
                                valid_values[vi] = vs;
                            }
                            vidx = jz_json_skip(toks, count, vidx);
                        }
                    }
                }
            }
            inner = jz_json_skip(toks, count, inner);
        }

        if (name && default_val) {
            JZChipClockGenParam p;
            p.name = name;
            p.default_value = default_val;
            p.is_double = p_is_double;
            p.has_min = p_has_min;
            p.min = p_min;
            p.has_max = p_has_max;
            p.max = p_max;
            p.valid_values = valid_values;
            p.valid_count = valid_count;
            jz_buf_append(&cg->params, &p, sizeof(p));
        } else {
            free(name);
            free(default_val);
            if (valid_values) {
                for (size_t vi = 0; vi < valid_count; ++vi) free(valid_values[vi]);
                free(valid_values);
            }
        }

        cur = jz_json_skip(toks, count, cur);
    }
}

/* Parse the outputs section of a clock_gen object.
 * The outputs section is an object like:
 *   "outputs": { "BASE": { "frequency_mhz": { "expr": "FVCO / ODIV" } }, ... }
 */
static void jz_chip_parse_clock_gen_outputs(const char *json,
                                            const jsmntok_t *toks,
                                            int count,
                                            int outputs_obj_idx,
                                            JZChipClockGen *cg)
{
    if (!json || !toks || outputs_obj_idx < 0 || outputs_obj_idx >= count || !cg) return;
    const jsmntok_t *obj = &toks[outputs_obj_idx];
    if (obj->type != JSMN_OBJECT) return;

    int cur = outputs_obj_idx + 1;
    while (cur < count && toks[cur].start < obj->end) {
        const jsmntok_t *sel_tok = &toks[cur++];
        if (sel_tok->type != JSMN_STRING || cur >= count) {
            cur = jz_json_skip(toks, count, cur);
            continue;
        }
        const jsmntok_t *val = &toks[cur];
        if (val->type != JSMN_OBJECT) {
            cur = jz_json_skip(toks, count, cur);
            continue;
        }

        char *selector = jz_json_token_strdup(json, sel_tok);
        char *freq_expr = NULL;
        char *phase_expr = NULL;
        int is_clock = -1; /* -1 = not specified */

        /* Scan inner object for "frequency_mhz" -> "expr", "phase_deg" -> "expr", and "is_clock" */
        int inner = cur + 1;
        while (inner < count && toks[inner].start < val->end) {
            const jsmntok_t *ikey = &toks[inner++];
            if (inner >= count) break;
            const jsmntok_t *ival = &toks[inner];
            if (jz_json_token_eq(json, ikey, "is_clock") &&
                ival->type == JSMN_PRIMITIVE) {
                int bval = 0;
                if (jz_json_token_to_bool(json, ival, &bval)) {
                    is_clock = bval;
                }
            } else if (ival->type == JSMN_OBJECT &&
                (jz_json_token_eq(json, ikey, "frequency_mhz") ||
                 jz_json_token_eq(json, ikey, "phase_deg"))) {
                int is_phase = jz_json_token_eq(json, ikey, "phase_deg");
                /* Look for "expr" inside the sub-object */
                int finner = inner + 1;
                while (finner < count && toks[finner].start < ival->end) {
                    const jsmntok_t *fkey = &toks[finner++];
                    if (finner >= count) break;
                    const jsmntok_t *fval = &toks[finner];
                    if (jz_json_token_eq(json, fkey, "expr")) {
                        if (is_phase) {
                            phase_expr = jz_json_token_strdup(json, fval);
                        } else {
                            freq_expr = jz_json_token_strdup(json, fval);
                        }
                    }
                    finner = jz_json_skip(toks, count, finner);
                }
            }
            inner = jz_json_skip(toks, count, inner);
        }

        if (selector) {
            JZChipClockGenOutput out_entry;
            out_entry.selector = selector;
            out_entry.frequency_expr = freq_expr;
            out_entry.phase_deg_expr = phase_expr;
            /* If is_clock not explicitly set, infer from presence of frequency_mhz */
            out_entry.is_clock = (is_clock >= 0) ? is_clock : (freq_expr != NULL);
            jz_buf_append(&cg->outputs, &out_entry, sizeof(out_entry));
        } else {
            free(selector);
            free(freq_expr);
            free(phase_expr);
        }

        cur = jz_json_skip(toks, count, cur);
    }
}

/* Parse one variant fact key of the form "input.<NAME>.source" or
 * "output.count". Returns 1 on success with *out_fact populated, 0 on
 * malformed/unknown key. On error sets g_chip_last_error. */
static int jz_chip_parse_variant_fact(const char *json,
                                       const jsmntok_t *toks,
                                       int count,
                                       int key_idx,
                                       int val_idx,
                                       const char *cg_type,
                                       JZChipClockGenVariantFact *out_fact)
{
    (void)count;
    if (!out_fact) return 0;
    memset(out_fact, 0, sizeof(*out_fact));

    const jsmntok_t *key = &toks[key_idx];
    const jsmntok_t *val = &toks[val_idx];
    if (key->type != JSMN_STRING) return 0;

    size_t klen = (size_t)(key->end - key->start);
    const char *kstr = json + key->start;

    /* Match "input.<NAME>.source" */
    if (klen > 7 && strncmp(kstr, "input.", 6) == 0) {
        /* find ".source" suffix */
        const char *dot = (const char *)memchr(kstr + 6, '.', klen - 6);
        if (!dot) {
            jz_chip_set_error("clock_gen '%s': malformed variant fact key '%.*s'",
                              cg_type ? cg_type : "?", (int)klen, kstr);
            return 0;
        }
        size_t name_len = (size_t)(dot - (kstr + 6));
        size_t tail_len = klen - 6 - name_len;
        if (tail_len != 7 || strncmp(dot, ".source", 7) != 0) {
            jz_chip_set_error("clock_gen '%s': unknown variant fact key '%.*s' "
                              "(expected 'input.<NAME>.source')",
                              cg_type ? cg_type : "?", (int)klen, kstr);
            return 0;
        }
        if (name_len == 0) {
            jz_chip_set_error("clock_gen '%s': empty input name in variant fact key",
                              cg_type ? cg_type : "?");
            return 0;
        }

        /* Value must be a string "pad" or "fabric" (or "" for unwired). */
        if (val->type != JSMN_STRING) {
            jz_chip_set_error("clock_gen '%s': variant fact '%.*s' value must be a string",
                              cg_type ? cg_type : "?", (int)klen, kstr);
            return 0;
        }
        size_t vlen = (size_t)(val->end - val->start);
        const char *vstr = json + val->start;
        if (!(vlen == 3 && strncmp(vstr, "pad", 3) == 0) &&
            !(vlen == 6 && strncmp(vstr, "fabric", 6) == 0) &&
            !(vlen == 0)) {
            jz_chip_set_error("clock_gen '%s': variant fact '%.*s' value must be "
                              "\"pad\", \"fabric\", or \"\" (got \"%.*s\")",
                              cg_type ? cg_type : "?", (int)klen, kstr,
                              (int)vlen, vstr);
            return 0;
        }

        out_fact->kind = JZ_CG_FACT_INPUT_SOURCE;
        out_fact->input_name = (char *)malloc(name_len + 1);
        if (!out_fact->input_name) return 0;
        /* canonicalise input name to uppercase to match parsed inputs */
        for (size_t i = 0; i < name_len; ++i) {
            out_fact->input_name[i] = (char)toupper((unsigned char)(kstr + 6)[i]);
        }
        out_fact->input_name[name_len] = '\0';
        out_fact->source_value = (char *)malloc(vlen + 1);
        if (!out_fact->source_value) {
            free(out_fact->input_name);
            out_fact->input_name = NULL;
            return 0;
        }
        memcpy(out_fact->source_value, vstr, vlen);
        out_fact->source_value[vlen] = '\0';
        return 1;
    }

    /* Match "output.count" */
    if (klen == 12 && strncmp(kstr, "output.count", 12) == 0) {
        if (val->type != JSMN_PRIMITIVE) {
            jz_chip_set_error("clock_gen '%s': variant fact 'output.count' must be an integer",
                              cg_type ? cg_type : "?");
            return 0;
        }
        unsigned u = 0;
        if (!jz_json_token_to_uint(json, val, &u)) {
            jz_chip_set_error("clock_gen '%s': variant fact 'output.count' is not a valid integer",
                              cg_type ? cg_type : "?");
            return 0;
        }
        out_fact->kind = JZ_CG_FACT_OUTPUT_COUNT;
        out_fact->int_value = (int)u;
        return 1;
    }

    jz_chip_set_error("clock_gen '%s': unknown variant fact key '%.*s' "
                      "(expected 'input.<NAME>.source' or 'output.count')",
                      cg_type ? cg_type : "?", (int)klen, kstr);
    return 0;
}

/* Free a single variant's owned resources. */
static void jz_chip_clock_gen_variant_free(JZChipClockGenVariant *v)
{
    if (!v) return;
    size_t fc = v->facts.len / sizeof(JZChipClockGenVariantFact);
    JZChipClockGenVariantFact *fs = (JZChipClockGenVariantFact *)v->facts.data;
    for (size_t i = 0; i < fc; ++i) {
        free(fs[i].input_name);
        free(fs[i].source_value);
    }
    jz_buf_free(&v->facts);
    size_t mc = v->maps.len / sizeof(JZChipClockGenMap);
    JZChipClockGenMap *ms = (JZChipClockGenMap *)v->maps.data;
    for (size_t i = 0; i < mc; ++i) {
        free(ms[i].backend);
        free(ms[i].template_text);
    }
    jz_buf_free(&v->maps);
}

/* Parse a single variant object { "when": {...}, "map": {...} }. */
static int jz_chip_parse_clock_gen_variant(const char *json,
                                            const jsmntok_t *toks,
                                            int count,
                                            int obj_idx,
                                            const char *cg_type,
                                            JZChipClockGenVariant *out_v)
{
    memset(out_v, 0, sizeof(*out_v));
    const jsmntok_t *obj = &toks[obj_idx];
    if (obj->type != JSMN_OBJECT) {
        jz_chip_set_error("clock_gen '%s': variants[] entry must be an object",
                          cg_type ? cg_type : "?");
        return 0;
    }

    int when_idx = -1;
    int map_idx = -1;
    int cur = obj_idx + 1;
    while (cur < count && toks[cur].start < obj->end) {
        const jsmntok_t *key = &toks[cur++];
        if (jz_json_token_eq(json, key, "when")) {
            when_idx = cur;
        } else if (jz_json_token_eq(json, key, "map")) {
            map_idx = cur;
        }
        cur = jz_json_skip(toks, count, cur);
    }

    if (when_idx < 0 || toks[when_idx].type != JSMN_OBJECT) {
        jz_chip_set_error("clock_gen '%s': variants[] entry missing or non-object 'when'",
                          cg_type ? cg_type : "?");
        return 0;
    }
    if (map_idx < 0 || toks[map_idx].type != JSMN_OBJECT) {
        jz_chip_set_error("clock_gen '%s': variants[] entry missing or non-object 'map'",
                          cg_type ? cg_type : "?");
        return 0;
    }

    /* Parse `when` facts. */
    const jsmntok_t *when_obj = &toks[when_idx];
    int wi = when_idx + 1;
    while (wi < count && toks[wi].start < when_obj->end) {
        int key_i = wi++;
        int val_i = wi;
        JZChipClockGenVariantFact fact;
        if (!jz_chip_parse_variant_fact(json, toks, count, key_i, val_i,
                                        cg_type, &fact)) {
            jz_chip_clock_gen_variant_free(out_v);
            return 0;
        }
        jz_buf_append(&out_v->facts, &fact, sizeof(fact));
        wi = jz_json_skip(toks, count, wi);
    }

    /* Parse `map` templates. */
    jz_chip_parse_clock_gen_map_into(json, toks, count, map_idx, &out_v->maps);
    if (out_v->maps.len == 0) {
        jz_chip_set_error("clock_gen '%s': variants[] entry has empty 'map'",
                          cg_type ? cg_type : "?");
        jz_chip_clock_gen_variant_free(out_v);
        return 0;
    }
    return 1;
}

/* Parse the entire `variants` array. Returns 1 on success, 0 on failure. */
static int jz_chip_parse_clock_gen_variants(const char *json,
                                              const jsmntok_t *toks,
                                              int count,
                                              int arr_idx,
                                              const char *cg_type,
                                              JZChipClockGen *cg)
{
    if (arr_idx < 0 || arr_idx >= count) return 0;
    const jsmntok_t *arr = &toks[arr_idx];
    if (arr->type != JSMN_ARRAY) {
        jz_chip_set_error("clock_gen '%s': 'variants' must be an array",
                          cg_type ? cg_type : "?");
        return 0;
    }

    int cur = arr_idx + 1;
    for (int i = 0; i < arr->size && cur < count; ++i) {
        JZChipClockGenVariant v;
        if (!jz_chip_parse_clock_gen_variant(json, toks, count, cur, cg_type, &v)) {
            return 0;
        }
        jz_buf_append(&cg->variants, &v, sizeof(v));
        cur = jz_json_skip(toks, count, cur);
    }

    if (cg->variants.len == 0) {
        jz_chip_set_error("clock_gen '%s': 'variants' array is empty",
                          cg_type ? cg_type : "?");
        return 0;
    }
    return 1;
}

/* ---- Variant fact matching and exhaustive/disjoint validation ---- */

/* Axis used to enumerate the cartesian product at load time.
 * kind == JZ_CG_FACT_INPUT_SOURCE: name[] of fact; values are strings.
 * kind == JZ_CG_FACT_OUTPUT_COUNT: values are ints. */
typedef struct VariantAxis {
    JZChipVariantFactKind kind;
    char *name;          /* input name for INPUT_SOURCE, NULL for OUTPUT_COUNT */
    char **svals;        /* distinct string values for INPUT_SOURCE */
    int   *ivals;        /* distinct int values for OUTPUT_COUNT */
    size_t val_count;
} VariantAxis;

static void variant_axes_free(VariantAxis *axes, size_t count)
{
    if (!axes) return;
    for (size_t i = 0; i < count; ++i) {
        free(axes[i].name);
        if (axes[i].svals) {
            for (size_t j = 0; j < axes[i].val_count; ++j) free(axes[i].svals[j]);
            free(axes[i].svals);
        }
        free(axes[i].ivals);
    }
    free(axes);
}

static int variant_axes_add_sval(VariantAxis *ax, const char *v)
{
    for (size_t i = 0; i < ax->val_count; ++i) {
        if (strcmp(ax->svals[i], v) == 0) return 1;
    }
    char **nv = (char **)realloc(ax->svals, sizeof(char *) * (ax->val_count + 1));
    if (!nv) return 0;
    ax->svals = nv;
    ax->svals[ax->val_count] = strdup(v);
    if (!ax->svals[ax->val_count]) return 0;
    ax->val_count++;
    return 1;
}

static int variant_axes_add_ival(VariantAxis *ax, int v)
{
    for (size_t i = 0; i < ax->val_count; ++i) {
        if (ax->ivals[i] == v) return 1;
    }
    int *nv = (int *)realloc(ax->ivals, sizeof(int) * (ax->val_count + 1));
    if (!nv) return 0;
    ax->ivals = nv;
    ax->ivals[ax->val_count++] = v;
    return 1;
}

/* Build the set of axes referenced across all variants in cg.
 * On success returns allocated array in *out_axes with *out_count entries.
 * Caller must free via variant_axes_free(). Returns 1 on success. */
static int variant_build_axes(const JZChipClockGen *cg,
                               VariantAxis **out_axes,
                               size_t *out_count)
{
    *out_axes = NULL;
    *out_count = 0;
    size_t cap = 0;
    VariantAxis *axes = NULL;

    size_t vc = cg->variants.len / sizeof(JZChipClockGenVariant);
    const JZChipClockGenVariant *vs =
        (const JZChipClockGenVariant *)cg->variants.data;
    for (size_t vi = 0; vi < vc; ++vi) {
        size_t fc = vs[vi].facts.len / sizeof(JZChipClockGenVariantFact);
        const JZChipClockGenVariantFact *fs =
            (const JZChipClockGenVariantFact *)vs[vi].facts.data;
        for (size_t fi = 0; fi < fc; ++fi) {
            /* Find or create matching axis */
            size_t ai;
            for (ai = 0; ai < *out_count; ++ai) {
                if (axes[ai].kind != fs[fi].kind) continue;
                if (fs[fi].kind == JZ_CG_FACT_INPUT_SOURCE) {
                    if (axes[ai].name && fs[fi].input_name &&
                        strcmp(axes[ai].name, fs[fi].input_name) == 0) break;
                } else {
                    break;
                }
            }
            if (ai == *out_count) {
                if (*out_count == cap) {
                    cap = cap ? cap * 2 : 4;
                    VariantAxis *na = (VariantAxis *)realloc(axes, sizeof(VariantAxis) * cap);
                    if (!na) { variant_axes_free(axes, *out_count); return 0; }
                    axes = na;
                }
                memset(&axes[*out_count], 0, sizeof(VariantAxis));
                axes[*out_count].kind = fs[fi].kind;
                if (fs[fi].kind == JZ_CG_FACT_INPUT_SOURCE && fs[fi].input_name) {
                    axes[*out_count].name = strdup(fs[fi].input_name);
                    if (!axes[*out_count].name) {
                        variant_axes_free(axes, *out_count); return 0;
                    }
                }
                (*out_count)++;
            }
            /* Record the value. */
            if (fs[fi].kind == JZ_CG_FACT_INPUT_SOURCE) {
                if (!variant_axes_add_sval(&axes[ai],
                                            fs[fi].source_value ? fs[fi].source_value : "")) {
                    variant_axes_free(axes, *out_count); return 0;
                }
            } else {
                if (!variant_axes_add_ival(&axes[ai], fs[fi].int_value)) {
                    variant_axes_free(axes, *out_count); return 0;
                }
            }
        }
    }
    *out_axes = axes;
    return 1;
}

/* Does the tuple (axis value indices in `idx[]`) satisfy a variant's when clause? */
static int variant_matches_tuple(const JZChipClockGenVariant *v,
                                  const VariantAxis *axes,
                                  const size_t *idx,
                                  size_t axis_count)
{
    size_t fc = v->facts.len / sizeof(JZChipClockGenVariantFact);
    const JZChipClockGenVariantFact *fs =
        (const JZChipClockGenVariantFact *)v->facts.data;
    for (size_t fi = 0; fi < fc; ++fi) {
        /* Find the axis for this fact. */
        size_t ai;
        for (ai = 0; ai < axis_count; ++ai) {
            if (axes[ai].kind != fs[fi].kind) continue;
            if (fs[fi].kind == JZ_CG_FACT_INPUT_SOURCE) {
                if (axes[ai].name && fs[fi].input_name &&
                    strcmp(axes[ai].name, fs[fi].input_name) == 0) break;
            } else {
                break;
            }
        }
        if (ai == axis_count) return 0;  /* axis not built; shouldn't happen */

        if (fs[fi].kind == JZ_CG_FACT_INPUT_SOURCE) {
            const char *tuple_val = axes[ai].svals[idx[ai]];
            const char *want = fs[fi].source_value ? fs[fi].source_value : "";
            if (strcmp(tuple_val, want) != 0) return 0;
        } else {
            int tuple_val = axes[ai].ivals[idx[ai]];
            if (tuple_val != fs[fi].int_value) return 0;
        }
    }
    return 1;
}

/* Validate that the variants in `cg` are exhaustive and disjoint over the
 * cartesian product of all referenced fact axes. Returns 1 on success.
 * On failure, sets g_chip_last_error. */
static int variant_validate_coverage(const JZChipClockGen *cg)
{
    VariantAxis *axes = NULL;
    size_t axis_count = 0;
    if (!variant_build_axes(cg, &axes, &axis_count)) {
        jz_chip_set_error("clock_gen '%s': out of memory validating variants",
                          cg->type ? cg->type : "?");
        return 0;
    }
    if (axis_count == 0) {
        /* No axes and there are variants: at most one variant allowed. */
        size_t vc = cg->variants.len / sizeof(JZChipClockGenVariant);
        if (vc > 1) {
            jz_chip_set_error("clock_gen '%s': multiple variants with no 'when' facts",
                              cg->type ? cg->type : "?");
            variant_axes_free(axes, axis_count);
            return 0;
        }
        variant_axes_free(axes, axis_count);
        return 1;
    }

    size_t *idx = (size_t *)calloc(axis_count, sizeof(size_t));
    if (!idx) {
        jz_chip_set_error("clock_gen '%s': out of memory validating variants",
                          cg->type ? cg->type : "?");
        variant_axes_free(axes, axis_count);
        return 0;
    }

    const JZChipClockGenVariant *vs =
        (const JZChipClockGenVariant *)cg->variants.data;
    size_t vc = cg->variants.len / sizeof(JZChipClockGenVariant);

    int ok = 1;
    while (ok) {
        /* Count matching variants for this tuple. */
        int matches = 0;
        for (size_t vi = 0; vi < vc; ++vi) {
            if (variant_matches_tuple(&vs[vi], axes, idx, axis_count)) {
                matches++;
            }
        }
        if (matches != 1) {
            /* Describe the offending tuple. */
            char buf[384];
            size_t off = 0;
            for (size_t ai = 0; ai < axis_count && off + 1 < sizeof(buf); ++ai) {
                const char *sep = (ai == 0) ? "" : ", ";
                if (axes[ai].kind == JZ_CG_FACT_INPUT_SOURCE) {
                    int n = snprintf(buf + off, sizeof(buf) - off,
                                     "%sinput.%s.source=\"%s\"", sep,
                                     axes[ai].name ? axes[ai].name : "?",
                                     axes[ai].svals[idx[ai]]);
                    if (n > 0) off += (size_t)n;
                } else {
                    int n = snprintf(buf + off, sizeof(buf) - off,
                                     "%soutput.count=%d", sep, axes[ai].ivals[idx[ai]]);
                    if (n > 0) off += (size_t)n;
                }
            }
            if (matches == 0) {
                jz_chip_set_error(
                    "clock_gen '%s': variants not exhaustive; no variant matches { %s }",
                    cg->type ? cg->type : "?", buf);
            } else {
                jz_chip_set_error(
                    "clock_gen '%s': variants not disjoint; %d variants match { %s }",
                    cg->type ? cg->type : "?", matches, buf);
            }
            ok = 0;
            break;
        }

        /* Advance tuple indices (like an odometer). */
        size_t k = 0;
        while (k < axis_count) {
            idx[k]++;
            if (idx[k] < axes[k].val_count) break;
            idx[k] = 0;
            k++;
        }
        if (k == axis_count) break;  /* enumerated all tuples */
    }

    free(idx);
    variant_axes_free(axes, axis_count);
    return ok;
}

/* Parse a single clock_gen entry from the array. */
static int jz_chip_parse_clock_gen_object(const char *json,
                                          const jsmntok_t *toks,
                                          int count,
                                          int obj_index,
                                          JZChipData *out,
                                          int *err)
{
    const jsmntok_t *obj = &toks[obj_index];
    if (obj->type != JSMN_OBJECT) return jz_json_skip(toks, count, obj_index);

    char *type = NULL;
    char *mode = NULL;
    char *feedback_wire = NULL;
    int map_idx = -1;
    int variants_idx = -1;
    int derived_idx = -1;
    int params_idx = -1;
    int outputs_idx = -1;
    int inputs_idx = -1;
    int constraints_idx = -1;
    int gen_count = 0;
    int has_chaining = 0;
    int chaining = 0;
    int has_pad_exclusive = 0;
    int pad_exclusive = 0;

    int cur = obj_index + 1;
    while (cur < count && toks[cur].start < obj->end) {
        const jsmntok_t *key = &toks[cur++];
        const jsmntok_t *val = &toks[cur];
        if (jz_json_token_eq(json, key, "type")) {
            type = jz_json_token_strdup(json, val);
        } else if (jz_json_token_eq(json, key, "mode")) {
            mode = jz_json_token_strdup(json, val);
        } else if (jz_json_token_eq(json, key, "map")) {
            map_idx = cur;
        } else if (jz_json_token_eq(json, key, "variants")) {
            variants_idx = cur;
        } else if (jz_json_token_eq(json, key, "derived")) {
            derived_idx = cur;
        } else if (jz_json_token_eq(json, key, "parameters")) {
            params_idx = cur;
        } else if (jz_json_token_eq(json, key, "outputs")) {
            outputs_idx = cur;
        } else if (jz_json_token_eq(json, key, "inputs")) {
            inputs_idx = cur;
        } else if (jz_json_token_eq(json, key, "constraints")) {
            constraints_idx = cur;
        } else if (jz_json_token_eq(json, key, "count")) {
            unsigned v = 0;
            if (jz_json_token_to_uint(json, val, &v)) {
                gen_count = (int)v;
            }
        } else if (jz_json_token_eq(json, key, "chaining")) {
            int bval = 0;
            if (val->type == JSMN_PRIMITIVE) {
                if (json[val->start] == 't') bval = 1;
                has_chaining = 1;
                chaining = bval;
            }
        } else if (jz_json_token_eq(json, key, "pad_exclusive")) {
            int bval = 0;
            if (val->type == JSMN_PRIMITIVE) {
                if (json[val->start] == 't') bval = 1;
                has_pad_exclusive = 1;
                pad_exclusive = bval;
            }
        } else if (jz_json_token_eq(json, key, "feedback_wire")) {
            if (feedback_wire) free(feedback_wire);
            feedback_wire = jz_json_token_strdup(json, val);
        }
        cur = jz_json_skip(toks, count, cur);
    }

    /* Enforce mutual exclusion of `map` and `variants`. Exactly one required. */
    if (type && map_idx >= 0 && variants_idx >= 0) {
        jz_chip_set_error("clock_gen '%s': both 'map' and 'variants' are present; "
                          "use exactly one (S9.3)", type);
        if (err) *err = 1;
        free(type); free(mode); free(feedback_wire);
        return jz_json_skip(toks, count, obj_index);
    }
    if (type && map_idx < 0 && variants_idx < 0) {
        jz_chip_set_error("clock_gen '%s': neither 'map' nor 'variants' is present; "
                          "one is required (S9.3)", type);
        if (err) *err = 1;
        free(type); free(mode); free(feedback_wire);
        return jz_json_skip(toks, count, obj_index);
    }

    if (type && (map_idx >= 0 || variants_idx >= 0)) {
        JZChipClockGen cg;
        memset(&cg, 0, sizeof(cg));
        cg.type = type;
        cg.mode = mode;
        cg.feedback_wire = feedback_wire;
        int variants_parse_failed = 0;
        if (map_idx >= 0) {
            jz_chip_parse_clock_gen_map(json, toks, count, map_idx, &cg);
        } else {
            if (!jz_chip_parse_clock_gen_variants(json, toks, count,
                                                   variants_idx, type, &cg)) {
                if (err) *err = 1;
                variants_parse_failed = 1;
                /* Fall through to cleanup path below. */
            }
        }
        if (derived_idx >= 0) {
            jz_chip_parse_clock_gen_derived(json, toks, count, derived_idx, &cg);
        }
        if (params_idx >= 0) {
            jz_chip_parse_clock_gen_params(json, toks, count, params_idx, &cg);
        }
        if (outputs_idx >= 0) {
            jz_chip_parse_clock_gen_outputs(json, toks, count, outputs_idx, &cg);
        }
        /* Parse inputs as generic named inputs */
        if (inputs_idx >= 0 && toks[inputs_idx].type == JSMN_OBJECT) {
            int ii = inputs_idx + 1;
            while (ii < count && toks[ii].start < toks[inputs_idx].end) {
                const jsmntok_t *ik = &toks[ii++];
                int val_idx = ii;
                char *input_name = jz_json_token_strdup(json, ik);
                if (!input_name) {
                    ii = jz_json_skip(toks, count, ii);
                    continue;
                }
                /* Convert to uppercase for canonical name */
                for (char *cp = input_name; *cp; ++cp)
                    *cp = (char)toupper((unsigned char)*cp);

                JZChipClockGenInput inp;
                memset(&inp, 0, sizeof(inp));
                inp.name = input_name;
                inp.required = 1; /* default: required */

                if (toks[val_idx].type == JSMN_OBJECT) {
                    int ri = val_idx + 1;
                    while (ri < count && toks[ri].start < toks[val_idx].end) {
                        const jsmntok_t *rk = &toks[ri++];
                        const jsmntok_t *rv = &toks[ri];
                        if (jz_json_token_eq(json, rk, "min_mhz")) {
                            unsigned v = 0;
                            if (jz_json_token_to_uint(json, rv, &v)) {
                                inp.min_mhz = (double)v;
                                inp.has_min_mhz = 1;
                            }
                        } else if (jz_json_token_eq(json, rk, "max_mhz")) {
                            unsigned v = 0;
                            if (jz_json_token_to_uint(json, rv, &v)) {
                                inp.max_mhz = (double)v;
                                inp.has_max_mhz = 1;
                            }
                        } else if (jz_json_token_eq(json, rk, "requires_period")) {
                            if (rv->type == JSMN_PRIMITIVE && json[rv->start] == 't') {
                                inp.requires_period = 1;
                            }
                        } else if (jz_json_token_eq(json, rk, "required")) {
                            if (rv->type == JSMN_PRIMITIVE && json[rv->start] == 'f') {
                                inp.required = 0;
                            }
                        } else if (jz_json_token_eq(json, rk, "default")) {
                            inp.default_value = jz_json_token_strdup(json, rv);
                            if (inp.default_value) inp.required = 0;
                        }
                        ri = jz_json_skip(toks, count, ri);
                    }
                }
                /* Maintain backward compat: populate has_refclk_range from REF_CLK */
                if (strcmp(inp.name, "REF_CLK") == 0 && inp.has_min_mhz && inp.has_max_mhz) {
                    cg.has_refclk_range = 1;
                    cg.refclk_min_mhz = inp.min_mhz;
                    cg.refclk_max_mhz = inp.max_mhz;
                }
                jz_buf_append(&cg.inputs, &inp, sizeof(inp));
                ii = jz_json_skip(toks, count, val_idx);
            }
        }
        /* Store count, chaining, and constraints */
        cg.count = gen_count;
        cg.has_chaining = has_chaining;
        cg.chaining = chaining;
        cg.has_pad_exclusive = has_pad_exclusive;
        cg.pad_exclusive = pad_exclusive;
        if (constraints_idx >= 0 && toks[constraints_idx].type == JSMN_ARRAY) {
            const jsmntok_t *carr = &toks[constraints_idx];
            int ci = constraints_idx + 1;
            for (int cj = 0; cj < carr->size; ++cj) {
                if (ci >= count) break;
                const jsmntok_t *cobj = &toks[ci];
                if (cobj->type == JSMN_OBJECT) {
                    /* Look for "rule" key */
                    int ck = ci + 1;
                    while (ck < count && toks[ck].start < cobj->end) {
                        const jsmntok_t *ckey = &toks[ck++];
                        const jsmntok_t *cval = &toks[ck];
                        if (jz_json_token_eq(json, ckey, "rule")) {
                            char *rule = jz_json_token_strdup(json, cval);
                            if (rule) {
                                jz_buf_append(&cg.constraints, &rule, sizeof(rule));
                            }
                        }
                        ck = jz_json_skip(toks, count, ck);
                    }
                }
                ci = jz_json_skip(toks, count, ci);
            }
        }
        /* Validate variants coverage if present. */
        int cg_ok = 1;
        if (variants_parse_failed) {
            cg_ok = 0;
        } else if (cg.variants.len > 0) {
            if (!variant_validate_coverage(&cg)) {
                if (err) *err = 1;
                cg_ok = 0;
            }
        } else if (cg.maps.len == 0) {
            /* Neither path produced data (map parse failed, or empty). */
            cg_ok = 0;
        }

        if (cg_ok) {
            jz_buf_append(&out->clock_gens, &cg, sizeof(cg));
        } else {
            /* Free maps if any */
            size_t m_count = cg.maps.len / sizeof(JZChipClockGenMap);
            JZChipClockGenMap *ms2 = (JZChipClockGenMap *)cg.maps.data;
            for (size_t mi = 0; mi < m_count; ++mi) {
                free(ms2[mi].backend);
                free(ms2[mi].template_text);
            }
            jz_buf_free(&cg.maps);
            /* Free variants */
            size_t v_count = cg.variants.len / sizeof(JZChipClockGenVariant);
            JZChipClockGenVariant *vs2 = (JZChipClockGenVariant *)cg.variants.data;
            for (size_t vi = 0; vi < v_count; ++vi) {
                jz_chip_clock_gen_variant_free(&vs2[vi]);
            }
            jz_buf_free(&cg.variants);
            /* Free deriveds if maps failed */
            size_t d_count = cg.deriveds.len / sizeof(JZChipClockGenDerived);
            JZChipClockGenDerived *ds = (JZChipClockGenDerived *)cg.deriveds.data;
            for (size_t di = 0; di < d_count; ++di) {
                free(ds[di].name);
                free(ds[di].expr);
            }
            jz_buf_free(&cg.deriveds);
            /* Free params */
            size_t p_count = cg.params.len / sizeof(JZChipClockGenParam);
            JZChipClockGenParam *ps = (JZChipClockGenParam *)cg.params.data;
            for (size_t pi = 0; pi < p_count; ++pi) {
                free(ps[pi].name);
                free(ps[pi].default_value);
            }
            jz_buf_free(&cg.params);
            /* Free outputs */
            size_t o_count = cg.outputs.len / sizeof(JZChipClockGenOutput);
            JZChipClockGenOutput *os = (JZChipClockGenOutput *)cg.outputs.data;
            for (size_t oi = 0; oi < o_count; ++oi) {
                free(os[oi].selector);
                free(os[oi].frequency_expr);
                free(os[oi].phase_deg_expr);
            }
            jz_buf_free(&cg.outputs);
            /* Free inputs */
            {
                size_t in_count = cg.inputs.len / sizeof(JZChipClockGenInput);
                JZChipClockGenInput *ins = (JZChipClockGenInput *)cg.inputs.data;
                for (size_t ini = 0; ini < in_count; ++ini) {
                    free(ins[ini].name);
                    free(ins[ini].default_value);
                }
            }
            jz_buf_free(&cg.inputs);
            /* Free constraints */
            size_t ct_count = cg.constraints.len / sizeof(char *);
            char **cts = (char **)cg.constraints.data;
            for (size_t cti = 0; cti < ct_count; ++cti) {
                free(cts[cti]);
            }
            jz_buf_free(&cg.constraints);
            free(type);
            free(mode);
            free(feedback_wire);
        }
    } else {
        free(type);
        free(mode);
        free(feedback_wire);
    }

    return jz_json_skip(toks, count, obj_index);
}

/* Parse a differential primitive's map section (same format as clock_gen map). */
static void jz_chip_parse_diff_map_into(const char *json,
                                         const jsmntok_t *toks,
                                         int count,
                                         int map_obj_idx,
                                         JZBuffer *maps)
{
    if (!json || !toks || map_obj_idx < 0 || map_obj_idx >= count || !maps) return;
    const jsmntok_t *map_obj = &toks[map_obj_idx];
    if (map_obj->type != JSMN_OBJECT) return;

    int cur = map_obj_idx + 1;
    while (cur < count && toks[cur].start < map_obj->end) {
        const jsmntok_t *key = &toks[cur++];
        if (key->type != JSMN_STRING) {
            cur = jz_json_skip(toks, count, cur);
            continue;
        }
        const jsmntok_t *val = &toks[cur];
        if (val->type != JSMN_ARRAY) {
            cur = jz_json_skip(toks, count, cur);
            continue;
        }

        char *backend = jz_json_token_strdup(json, key);
        if (!backend) {
            cur = jz_json_skip(toks, count, cur);
            continue;
        }

        size_t total_len = 0;
        int arr_idx = cur + 1;
        for (int i = 0; i < val->size && arr_idx < count; ++i) {
            const jsmntok_t *elem = &toks[arr_idx];
            if (elem->type == JSMN_STRING) {
                total_len += (size_t)(elem->end - elem->start);
            }
            arr_idx = jz_json_skip(toks, count, arr_idx);
        }

        char *template_text = (char *)malloc(total_len + 1);
        if (template_text) {
            size_t offset = 0;
            arr_idx = cur + 1;
            for (int i = 0; i < val->size && arr_idx < count; ++i) {
                const jsmntok_t *elem = &toks[arr_idx];
                if (elem->type == JSMN_STRING) {
                    size_t len = (size_t)(elem->end - elem->start);
                    memcpy(template_text + offset, json + elem->start, len);
                    offset += len;
                }
                arr_idx = jz_json_skip(toks, count, arr_idx);
            }
            template_text[offset] = '\0';

            JZChipDiffMap map_entry;
            map_entry.backend = backend;
            map_entry.template_text = template_text;
            jz_buf_append(maps, &map_entry, sizeof(map_entry));
        } else {
            free(backend);
        }

        cur = jz_json_skip(toks, count, cur);
    }
}

static void jz_chip_parse_diff_map(const char *json,
                                    const jsmntok_t *toks,
                                    int count,
                                    int map_obj_idx,
                                    JZChipDiffPrimitive *prim)
{
    if (!prim) return;
    jz_chip_parse_diff_map_into(json, toks, count, map_obj_idx, &prim->maps);
}

/* Parse a differential primitive object (buffer or serializer). */
static void jz_chip_parse_diff_primitive(const char *json,
                                          const jsmntok_t *toks,
                                          int count,
                                          int obj_idx,
                                          JZChipDiffPrimitive *prim)
{
    if (!json || !toks || obj_idx < 0 || obj_idx >= count || !prim) return;
    const jsmntok_t *obj = &toks[obj_idx];
    if (obj->type != JSMN_OBJECT) return;

    prim->ratio = 0;
    memset(&prim->maps, 0, sizeof(prim->maps));
    prim->requires_fclk = 0;
    prim->requires_pclk = 0;
    prim->requires_reset = 0;

    int cur = obj_idx + 1;
    while (cur < count && toks[cur].start < obj->end) {
        const jsmntok_t *key = &toks[cur++];
        const jsmntok_t *val = &toks[cur];
        if (jz_json_token_eq(json, key, "ratio")) {
            unsigned r = 0;
            if (jz_json_token_to_uint(json, val, &r)) {
                prim->ratio = (int)r;
            }
        } else if (jz_json_token_eq(json, key, "required_clocks") &&
                   val->type == JSMN_ARRAY) {
            int arr_cur = cur + 1;
            for (int i = 0; i < val->size && arr_cur < count; ++i) {
                if (jz_json_token_eq(json, &toks[arr_cur], "fclk"))
                    prim->requires_fclk = 1;
                else if (jz_json_token_eq(json, &toks[arr_cur], "pclk"))
                    prim->requires_pclk = 1;
                else if (jz_json_token_eq(json, &toks[arr_cur], "reset"))
                    prim->requires_reset = 1;
                arr_cur = jz_json_skip(toks, count, arr_cur);
            }
        } else if (jz_json_token_eq(json, key, "map")) {
            jz_chip_parse_diff_map(json, toks, count, cur, prim);
        }
        cur = jz_json_skip(toks, count, cur);
    }
}

/* Parse the "differential" section from the chip JSON. */
static void jz_chip_parse_differential(const char *json,
                                        const jsmntok_t *toks,
                                        int count,
                                        JZChipData *out)
{
    if (!json || !toks || count < 1 || !out) return;
    if (toks[0].type != JSMN_OBJECT) return;

    int idx = 1;
    int diff_idx = -1;
    while (idx < count && toks[idx].start < toks[0].end) {
        const jsmntok_t *key = &toks[idx++];
        if (jz_json_token_eq(json, key, "differential")) {
            diff_idx = idx;
        }
        idx = jz_json_skip(toks, count, idx);
    }

    if (diff_idx < 0) return;
    const jsmntok_t *diff_obj = &toks[diff_idx];
    if (diff_obj->type != JSMN_OBJECT) return;

    int cur = diff_idx + 1;
    while (cur < count && toks[cur].start < diff_obj->end) {
        const jsmntok_t *dir_key = &toks[cur++];
        const jsmntok_t *dir_val = &toks[cur];

        /* Parse string fields (type, io_type) */
        if (dir_val->type == JSMN_STRING) {
            if (jz_json_token_eq(json, dir_key, "io_type")) {
                out->differential.io_type = jz_json_token_strdup(json, dir_val);
            } else if (jz_json_token_eq(json, dir_key, "type")) {
                out->differential.diff_type = jz_json_token_strdup(json, dir_val);
            }
            cur = jz_json_skip(toks, count, cur);
            continue;
        }

        if (dir_val->type != JSMN_OBJECT) {
            cur = jz_json_skip(toks, count, cur);
            continue;
        }

        int is_output = jz_json_token_eq(json, dir_key, "output");
        int is_input = jz_json_token_eq(json, dir_key, "input");
        int is_clock = jz_json_token_eq(json, dir_key, "clock");

        if (is_clock) {
            int inner = cur + 1;
            while (inner < count && toks[inner].start < dir_val->end) {
                const jsmntok_t *prim_key = &toks[inner++];
                const jsmntok_t *prim_val = &toks[inner];

                if (jz_json_token_eq(json, prim_key, "buffer") &&
                    prim_val->type == JSMN_OBJECT) {
                    jz_chip_parse_diff_primitive(json, toks, count, inner,
                                                  &out->differential.clock_buffer);
                    out->differential.has_clock_buffer = 1;
                }

                inner = jz_json_skip(toks, count, inner);
            }
        }

        if (is_output || is_input) {
            int inner = cur + 1;
            while (inner < count && toks[inner].start < dir_val->end) {
                const jsmntok_t *prim_key = &toks[inner++];
                const jsmntok_t *prim_val = &toks[inner];

                if (is_output && jz_json_token_eq(json, prim_key, "buffer") &&
                    prim_val->type == JSMN_OBJECT) {
                    jz_chip_parse_diff_primitive(json, toks, count, inner,
                                                  &out->differential.output_buffer);
                    out->differential.has_output_buffer = 1;
                } else if (is_output && jz_json_token_eq(json, prim_key, "serializer")) {
                    if (prim_val->type == JSMN_ARRAY) {
                        /* New format: array of serializer options */
                        int arr_idx = inner + 1;
                        for (int ai = 0; ai < prim_val->size && arr_idx < count; ++ai) {
                            if (toks[arr_idx].type == JSMN_OBJECT) {
                                JZChipDiffPrimitive ser;
                                jz_chip_parse_diff_primitive(json, toks, count, arr_idx, &ser);
                                jz_buf_append(&out->differential.output_serializers,
                                              &ser, sizeof(ser));
                            }
                            arr_idx = jz_json_skip(toks, count, arr_idx);
                        }
                        out->differential.has_output_serializer = 1;
                    } else if (prim_val->type == JSMN_OBJECT) {
                        /* Legacy format: single serializer object */
                        JZChipDiffPrimitive ser;
                        jz_chip_parse_diff_primitive(json, toks, count, inner, &ser);
                        jz_buf_append(&out->differential.output_serializers,
                                      &ser, sizeof(ser));
                        out->differential.has_output_serializer = 1;
                    }
                } else if (is_input && jz_json_token_eq(json, prim_key, "buffer") &&
                           prim_val->type == JSMN_OBJECT) {
                    jz_chip_parse_diff_primitive(json, toks, count, inner,
                                                  &out->differential.input_buffer);
                    out->differential.has_input_buffer = 1;
                } else if (is_input && jz_json_token_eq(json, prim_key, "deserializer")) {
                    if (prim_val->type == JSMN_ARRAY) {
                        /* New format: array of deserializer options */
                        int arr_idx = inner + 1;
                        for (int ai = 0; ai < prim_val->size && arr_idx < count; ++ai) {
                            if (toks[arr_idx].type == JSMN_OBJECT) {
                                JZChipDiffPrimitive deser;
                                jz_chip_parse_diff_primitive(json, toks, count, arr_idx, &deser);
                                jz_buf_append(&out->differential.input_deserializers,
                                              &deser, sizeof(deser));
                            }
                            arr_idx = jz_json_skip(toks, count, arr_idx);
                        }
                        out->differential.has_input_deserializer = 1;
                    } else if (prim_val->type == JSMN_OBJECT) {
                        /* Legacy format: single deserializer object */
                        JZChipDiffPrimitive deser;
                        jz_chip_parse_diff_primitive(json, toks, count, inner, &deser);
                        jz_buf_append(&out->differential.input_deserializers,
                                      &deser, sizeof(deser));
                        out->differential.has_input_deserializer = 1;
                    }
                }

                inner = jz_json_skip(toks, count, inner);
            }
        }

        cur = jz_json_skip(toks, count, cur);
    }
}

/* Parse the clock_gen array from the chip JSON. */
static void jz_chip_parse_clock_gens(const char *json,
                                     const jsmntok_t *toks,
                                     int count,
                                     JZChipData *out,
                                     int *err)
{
    if (!json || !toks || count < 1 || !out) return;
    if (toks[0].type != JSMN_OBJECT) return;

    int idx = 1;
    int clock_gen_idx = -1;
    while (idx < count && toks[idx].start < toks[0].end) {
        const jsmntok_t *key = &toks[idx++];
        if (jz_json_token_eq(json, key, "clock_gen")) {
            clock_gen_idx = idx;
        }
        idx = jz_json_skip(toks, count, idx);
    }

    if (clock_gen_idx < 0) return;
    if (toks[clock_gen_idx].type != JSMN_ARRAY) return;

    int cur = clock_gen_idx + 1;
    for (int i = 0; i < toks[clock_gen_idx].size; ++i) {
        if (cur >= count) break;
        cur = jz_chip_parse_clock_gen_object(json, toks, count, cur, out, err);
    }
}

/* Parse the latches section from the chip JSON. */
static void jz_chip_parse_latches(const char *json,
                                   const jsmntok_t *toks,
                                   int count,
                                   JZChipData *out)
{
    if (!json || !toks || count < 1 || !out) return;
    if (toks[0].type != JSMN_OBJECT) return;

    int idx = 1;
    int latches_idx = -1;
    while (idx < count && toks[idx].start < toks[0].end) {
        const jsmntok_t *key = &toks[idx++];
        if (jz_json_token_eq(json, key, "latches")) {
            latches_idx = idx;
        }
        idx = jz_json_skip(toks, count, idx);
    }

    if (latches_idx < 0) return;
    const jsmntok_t *lobj = &toks[latches_idx];
    if (lobj->type != JSMN_OBJECT) return;

    out->has_latches = 1;

    int cur = latches_idx + 1;
    while (cur < count && toks[cur].start < lobj->end) {
        const jsmntok_t *block_key = &toks[cur++];
        const jsmntok_t *block_val = &toks[cur];
        if (block_val->type != JSMN_OBJECT) {
            cur = jz_json_skip(toks, count, cur);
            continue;
        }

        int is_fab = jz_json_token_eq(json, block_key, "FAB");
        int is_iob = jz_json_token_eq(json, block_key, "IOB");

        if (is_fab || is_iob) {
            int inner = cur + 1;
            while (inner < count && toks[inner].start < block_val->end) {
                const jsmntok_t *fkey = &toks[inner++];
                const jsmntok_t *fval = &toks[inner];
                int bval = 0;
                if (jz_json_token_eq(json, fkey, "D") &&
                    fval->type == JSMN_PRIMITIVE) {
                    size_t len = (size_t)(fval->end - fval->start);
                    if (len == 4 && strncmp(json + fval->start, "true", 4) == 0) bval = 1;
                    if (is_fab) out->latches.fab_d = bval;
                    else        out->latches.iob_d = bval;
                } else if (jz_json_token_eq(json, fkey, "SR") &&
                           fval->type == JSMN_PRIMITIVE) {
                    size_t len = (size_t)(fval->end - fval->start);
                    if (len == 4 && strncmp(json + fval->start, "true", 4) == 0) bval = 1;
                    if (is_fab) out->latches.fab_sr = bval;
                    else        out->latches.iob_sr = bval;
                }
                inner = jz_json_skip(toks, count, inner);
            }
        }

        cur = jz_json_skip(toks, count, cur);
    }
}

JZChipLoadStatus jz_chip_data_load(const char *chip_id,
                                   const char *project_filename,
                                   JZChipData *out)
{
    if (!out) return JZ_CHIP_LOAD_NOT_FOUND;
    memset(out, 0, sizeof(*out));
    jz_chip_clear_error();

    if (!chip_id || chip_id[0] == '\0' || jz_strcasecmp(chip_id, "GENERIC") == 0) {
        return JZ_CHIP_LOAD_GENERIC;
    }

    char *chip_id_upper = jz_strdup_upper(chip_id);
    if (!chip_id_upper) return JZ_CHIP_LOAD_NOT_FOUND;
    out->chip_id = chip_id_upper;

    char *path = jz_build_chip_json_path(project_filename, chip_id);
    char *path_lower = NULL;
    char *json = NULL;
    if (path) {
        json = jz_read_entire_file(path, NULL);
    }
    if (!json) {
        char *lower = jz_strdup_lower(chip_id);
        if (lower) {
            path_lower = jz_build_chip_json_path(project_filename, lower);
            if (path_lower) {
                json = jz_read_entire_file(path_lower, NULL);
            }
        }
        free(lower);
    }
    free(path);
    free(path_lower);

    const char *builtin_json = NULL;
    if (!json) {
        builtin_json = jz_chip_builtin_json(chip_id);
        if (!builtin_json) {
            builtin_json = jz_chip_builtin_json(out->chip_id);
        }
    }

    const char *json_source = json ? json : builtin_json;
    if (!json_source) {
        jz_chip_data_free(out);
        return JZ_CHIP_LOAD_NOT_FOUND;
    }

    jsmn_parser parser;
    jsmn_init(&parser);
    int tok_count = jsmn_parse(&parser, json_source, strlen(json_source), NULL, 0);
    if (tok_count <= 0) {
        free(json);
        jz_chip_data_free(out);
        return JZ_CHIP_LOAD_JSON_ERROR;
    }

    jsmntok_t *toks = (jsmntok_t *)calloc((size_t)tok_count, sizeof(jsmntok_t));
    if (!toks) {
        free(json);
        jz_chip_data_free(out);
        return JZ_CHIP_LOAD_JSON_ERROR;
    }

    jsmn_init(&parser);
    tok_count = jsmn_parse(&parser, json_source, strlen(json_source), toks, (unsigned int)tok_count);
    if (tok_count <= 0) {
        free(toks);
        free(json);
        jz_chip_data_free(out);
        return JZ_CHIP_LOAD_JSON_ERROR;
    }

    int rc = jz_chip_parse_memory(json_source, toks, tok_count, out);

    /* Parse clock_gen data (optional, don't fail if not present) */
    int cg_err = 0;
    jz_chip_parse_clock_gens(json_source, toks, tok_count, out, &cg_err);

    /* Parse differential I/O data (optional) */
    jz_chip_parse_differential(json_source, toks, tok_count, out);

    /* Parse latch support data (optional) */
    jz_chip_parse_latches(json_source, toks, tok_count, out);

    free(toks);
    free(json);
    if (rc != 0 || cg_err) {
        jz_chip_data_free(out);
        return JZ_CHIP_LOAD_JSON_ERROR;
    }

    return JZ_CHIP_LOAD_OK;
}

void jz_chip_data_free(JZChipData *data)
{
    if (!data) return;
    if (data->chip_id) {
        free(data->chip_id);
        data->chip_id = NULL;
    }
    jz_buf_free(&data->mem_configs);
    jz_buf_free(&data->mem_resources);

    /* Free clock_gens */
    size_t cg_count = data->clock_gens.len / sizeof(JZChipClockGen);
    JZChipClockGen *cgs = (JZChipClockGen *)data->clock_gens.data;
    for (size_t i = 0; i < cg_count; ++i) {
        free(cgs[i].type);
        free(cgs[i].mode);
        size_t map_count = cgs[i].maps.len / sizeof(JZChipClockGenMap);
        JZChipClockGenMap *maps = (JZChipClockGenMap *)cgs[i].maps.data;
        for (size_t j = 0; j < map_count; ++j) {
            free(maps[j].backend);
            free(maps[j].template_text);
        }
        jz_buf_free(&cgs[i].maps);

        /* Free variants */
        {
            size_t v_count = cgs[i].variants.len / sizeof(JZChipClockGenVariant);
            JZChipClockGenVariant *vs = (JZChipClockGenVariant *)cgs[i].variants.data;
            for (size_t vi = 0; vi < v_count; ++vi) {
                jz_chip_clock_gen_variant_free(&vs[vi]);
            }
            jz_buf_free(&cgs[i].variants);
        }

        size_t d_count = cgs[i].deriveds.len / sizeof(JZChipClockGenDerived);
        JZChipClockGenDerived *ds = (JZChipClockGenDerived *)cgs[i].deriveds.data;
        for (size_t j = 0; j < d_count; ++j) {
            free(ds[j].name);
            free(ds[j].expr);
        }
        jz_buf_free(&cgs[i].deriveds);

        size_t p_count = cgs[i].params.len / sizeof(JZChipClockGenParam);
        JZChipClockGenParam *ps = (JZChipClockGenParam *)cgs[i].params.data;
        for (size_t j = 0; j < p_count; ++j) {
            free(ps[j].name);
            free(ps[j].default_value);
            if (ps[j].valid_values) {
                for (size_t vi = 0; vi < ps[j].valid_count; ++vi) {
                    free(ps[j].valid_values[vi]);
                }
                free(ps[j].valid_values);
            }
        }
        jz_buf_free(&cgs[i].params);

        size_t o_count = cgs[i].outputs.len / sizeof(JZChipClockGenOutput);
        JZChipClockGenOutput *os = (JZChipClockGenOutput *)cgs[i].outputs.data;
        for (size_t j = 0; j < o_count; ++j) {
            free(os[j].selector);
            free(os[j].frequency_expr);
            free(os[j].phase_deg_expr);
        }
        jz_buf_free(&cgs[i].outputs);
        /* Free inputs */
        {
            size_t in_count = cgs[i].inputs.len / sizeof(JZChipClockGenInput);
            JZChipClockGenInput *ins = (JZChipClockGenInput *)cgs[i].inputs.data;
            for (size_t j = 0; j < in_count; ++j) {
                free(ins[j].name);
                free(ins[j].default_value);
            }
        }
        jz_buf_free(&cgs[i].inputs);
        /* Free constraints */
        size_t ct_count = cgs[i].constraints.len / sizeof(char *);
        char **cts = (char **)cgs[i].constraints.data;
        for (size_t j2 = 0; j2 < ct_count; ++j2) {
            free(cts[j2]);
        }
        jz_buf_free(&cgs[i].constraints);
        free(cgs[i].feedback_wire);
    }
    jz_buf_free(&data->clock_gens);

    /* Free differential data */
    {
        /* Free non-array primitives */
        JZChipDiffPrimitive *prims[2] = {
            &data->differential.output_buffer,
            &data->differential.input_buffer
        };
        for (int pi2 = 0; pi2 < 2; ++pi2) {
            size_t dm_count = prims[pi2]->maps.len / sizeof(JZChipDiffMap);
            JZChipDiffMap *dms = (JZChipDiffMap *)prims[pi2]->maps.data;
            for (size_t di = 0; di < dm_count; ++di) {
                free(dms[di].backend);
                free(dms[di].template_text);
            }
            jz_buf_free(&prims[pi2]->maps);
        }
        /* Free serializer array */
        {
            size_t ns = data->differential.output_serializers.len / sizeof(JZChipDiffPrimitive);
            JZChipDiffPrimitive *sers =
                (JZChipDiffPrimitive *)data->differential.output_serializers.data;
            for (size_t si = 0; si < ns; ++si) {
                size_t dm_count = sers[si].maps.len / sizeof(JZChipDiffMap);
                JZChipDiffMap *dms = (JZChipDiffMap *)sers[si].maps.data;
                for (size_t di = 0; di < dm_count; ++di) {
                    free(dms[di].backend);
                    free(dms[di].template_text);
                }
                jz_buf_free(&sers[si].maps);
            }
            jz_buf_free(&data->differential.output_serializers);
        }
        /* Free deserializer array */
        {
            size_t nd = data->differential.input_deserializers.len / sizeof(JZChipDiffPrimitive);
            JZChipDiffPrimitive *desers =
                (JZChipDiffPrimitive *)data->differential.input_deserializers.data;
            for (size_t di2 = 0; di2 < nd; ++di2) {
                size_t dm_count = desers[di2].maps.len / sizeof(JZChipDiffMap);
                JZChipDiffMap *dms = (JZChipDiffMap *)desers[di2].maps.data;
                for (size_t di = 0; di < dm_count; ++di) {
                    free(dms[di].backend);
                    free(dms[di].template_text);
                }
                jz_buf_free(&desers[di2].maps);
            }
            jz_buf_free(&data->differential.input_deserializers);
        }
        free(data->differential.io_type);
        free(data->differential.diff_type);
    }
}

/* Forward declarations for internal helpers */
static const JZChipClockGen *jz_chip_find_clock_gen_with_mode(
    const JZChipData *data, const char *type, const char *mode);
static const JZChipClockGen *jz_chip_find_clock_gen(
    const JZChipData *data, const char *type);

const char *jz_chip_clock_gen_map(const JZChipData *data,
                                   const char *type,
                                   const char *backend)
{
    if (!data || !type || !backend) return NULL;
    const JZChipClockGen *cg = jz_chip_find_clock_gen(data, type);
    if (!cg) return NULL;
    /* Legacy API: only defined for entries using the legacy `map` form.
     * For variants entries, callers must use jz_chip_clock_gen_map_for_facts(). */
    if (cg->variants.len > 0) return NULL;
    size_t map_count = cg->maps.len / sizeof(JZChipClockGenMap);
    const JZChipClockGenMap *maps = (const JZChipClockGenMap *)cg->maps.data;
    for (size_t j = 0; j < map_count; ++j) {
        if (maps[j].backend && jz_strcasecmp(maps[j].backend, backend) == 0) {
            return maps[j].template_text;
        }
    }
    return NULL;
}

/* Facts-based dispatch: select the variant whose `when` facts match `facts`,
 * then return that variant's template text for `backend`. Works for both
 * legacy `map` entries (which are treated as a single always-matching variant)
 * and for new `variants` entries.
 *
 * Sets *out_match_count (if non-NULL) to the number of matching variants —
 * callers can use this to distinguish 0 (no-match) from >1 (ambiguous). */
const char *jz_chip_clock_gen_map_for_facts(const JZChipData *data,
                                             const char *type,
                                             const char *backend,
                                             const JZChipClockGenFacts *facts,
                                             int *out_match_count)
{
    if (out_match_count) *out_match_count = 0;
    if (!data || !type || !backend) return NULL;
    const JZChipClockGen *cg = jz_chip_find_clock_gen(data, type);
    if (!cg) return NULL;

    /* Legacy path: no variants, fall back to flat map lookup. */
    if (cg->variants.len == 0) {
        size_t map_count = cg->maps.len / sizeof(JZChipClockGenMap);
        const JZChipClockGenMap *maps = (const JZChipClockGenMap *)cg->maps.data;
        for (size_t j = 0; j < map_count; ++j) {
            if (maps[j].backend && jz_strcasecmp(maps[j].backend, backend) == 0) {
                if (out_match_count) *out_match_count = 1;
                return maps[j].template_text;
            }
        }
        return NULL;
    }

    /* Variants path: enumerate and match. */
    size_t vc = cg->variants.len / sizeof(JZChipClockGenVariant);
    const JZChipClockGenVariant *vs =
        (const JZChipClockGenVariant *)cg->variants.data;
    const JZChipClockGenVariant *chosen = NULL;
    int matches = 0;
    for (size_t vi = 0; vi < vc; ++vi) {
        int ok = 1;
        size_t fc = vs[vi].facts.len / sizeof(JZChipClockGenVariantFact);
        const JZChipClockGenVariantFact *fs =
            (const JZChipClockGenVariantFact *)vs[vi].facts.data;
        for (size_t fi = 0; fi < fc && ok; ++fi) {
            if (fs[fi].kind == JZ_CG_FACT_INPUT_SOURCE) {
                /* Look up the fact's input in `facts`. */
                const char *got = "";
                if (facts && facts->inputs) {
                    for (size_t ii = 0; ii < facts->input_count; ++ii) {
                        if (facts->inputs[ii].input_name &&
                            fs[fi].input_name &&
                            strcmp(facts->inputs[ii].input_name,
                                   fs[fi].input_name) == 0) {
                            got = facts->inputs[ii].source
                                    ? facts->inputs[ii].source : "";
                            break;
                        }
                    }
                }
                const char *want = fs[fi].source_value ? fs[fi].source_value : "";
                if (strcmp(got, want) != 0) ok = 0;
            } else { /* JZ_CG_FACT_OUTPUT_COUNT */
                int got = facts ? facts->output_count : 0;
                if (got != fs[fi].int_value) ok = 0;
            }
        }
        if (ok) {
            matches++;
            if (!chosen) chosen = &vs[vi];
        }
    }

    if (out_match_count) *out_match_count = matches;
    if (matches != 1 || !chosen) return NULL;

    size_t map_count = chosen->maps.len / sizeof(JZChipClockGenMap);
    const JZChipClockGenMap *maps = (const JZChipClockGenMap *)chosen->maps.data;
    for (size_t j = 0; j < map_count; ++j) {
        if (maps[j].backend && jz_strcasecmp(maps[j].backend, backend) == 0) {
            return maps[j].template_text;
        }
    }
    return NULL;
}

const char *jz_chip_clock_gen_derived_expr(const JZChipData *data,
                                            const char *type,
                                            const char *derived_name)
{
    if (!data || !type || !derived_name) return NULL;
    const JZChipClockGen *cg = jz_chip_find_clock_gen(data, type);
    if (!cg) return NULL;
    size_t d_count = cg->deriveds.len / sizeof(JZChipClockGenDerived);
    const JZChipClockGenDerived *ds = (const JZChipClockGenDerived *)cg->deriveds.data;
    for (size_t j = 0; j < d_count; ++j) {
        if (ds[j].name && strcmp(ds[j].name, derived_name) == 0) {
            return ds[j].expr;
        }
    }
    return NULL;
}

const char *jz_chip_clock_gen_param_default(const JZChipData *data,
                                             const char *type,
                                             const char *param_name)
{
    if (!data || !type || !param_name) return NULL;
    const JZChipClockGen *cg = jz_chip_find_clock_gen(data, type);
    if (!cg) return NULL;
    size_t p_count = cg->params.len / sizeof(JZChipClockGenParam);
    const JZChipClockGenParam *ps = (const JZChipClockGenParam *)cg->params.data;
    for (size_t j = 0; j < p_count; ++j) {
        if (ps[j].name && strcmp(ps[j].name, param_name) == 0) {
            return ps[j].default_value;
        }
    }
    return NULL;
}

/* Find a clock gen entry matching type and optional mode.
 * If mode is NULL, matches the first entry with the given type.
 * If mode is non-NULL, matches type AND mode.
 */
static const JZChipClockGen *jz_chip_find_clock_gen_with_mode(
    const JZChipData *data, const char *type, const char *mode)
{
    if (!data || !type) return NULL;
    size_t cg_count = data->clock_gens.len / sizeof(JZChipClockGen);
    const JZChipClockGen *cgs = (const JZChipClockGen *)data->clock_gens.data;
    for (size_t i = 0; i < cg_count; ++i) {
        if (!cgs[i].type || jz_strcasecmp(cgs[i].type, type) != 0) continue;
        if (mode) {
            if (cgs[i].mode && jz_strcasecmp(cgs[i].mode, mode) == 0) {
                return &cgs[i];
            }
        } else {
            return &cgs[i];
        }
    }
    return NULL;
}

/* Convenience: find clock gen by type only (mode=NULL). */
static const JZChipClockGen *jz_chip_find_clock_gen(const JZChipData *data,
                                                     const char *type)
{
    return jz_chip_find_clock_gen_with_mode(data, type, NULL);
}

int jz_chip_clock_gen_param_range(const JZChipData *data,
                                   const char *type,
                                   const char *param_name,
                                   double *out_min, double *out_max)
{
    if (!data || !type || !param_name || !out_min || !out_max) return 0;
    const JZChipClockGen *cg = jz_chip_find_clock_gen(data, type);
    if (!cg) return 0;
    size_t p_count = cg->params.len / sizeof(JZChipClockGenParam);
    const JZChipClockGenParam *ps = (const JZChipClockGenParam *)cg->params.data;
    for (size_t j = 0; j < p_count; ++j) {
        if (ps[j].name && strcmp(ps[j].name, param_name) == 0) {
            if (ps[j].has_min && ps[j].has_max) {
                *out_min = ps[j].min;
                *out_max = ps[j].max;
                return 1;
            }
            return 0;
        }
    }
    return 0;
}

int jz_chip_clock_gen_derived_range(const JZChipData *data,
                                     const char *type,
                                     const char *derived_name,
                                     double *out_min, double *out_max)
{
    if (!data || !type || !derived_name || !out_min || !out_max) return 0;
    const JZChipClockGen *cg = jz_chip_find_clock_gen(data, type);
    if (!cg) return 0;
    size_t d_count = cg->deriveds.len / sizeof(JZChipClockGenDerived);
    const JZChipClockGenDerived *ds = (const JZChipClockGenDerived *)cg->deriveds.data;
    for (size_t j = 0; j < d_count; ++j) {
        if (ds[j].name && strcmp(ds[j].name, derived_name) == 0) {
            if (ds[j].has_min && ds[j].has_max) {
                *out_min = ds[j].min;
                *out_max = ds[j].max;
                return 1;
            }
            return 0;
        }
    }
    return 0;
}

size_t jz_chip_clock_gen_derived_count(const JZChipData *data, const char *type)
{
    const JZChipClockGen *cg = jz_chip_find_clock_gen(data, type);
    if (!cg) return 0;
    return cg->deriveds.len / sizeof(JZChipClockGenDerived);
}

const JZChipClockGenDerived *jz_chip_clock_gen_derived_at(
    const JZChipData *data, const char *type, size_t index)
{
    const JZChipClockGen *cg = jz_chip_find_clock_gen(data, type);
    if (!cg) return NULL;
    size_t d_count = cg->deriveds.len / sizeof(JZChipClockGenDerived);
    if (index >= d_count) return NULL;
    const JZChipClockGenDerived *ds = (const JZChipClockGenDerived *)cg->deriveds.data;
    return &ds[index];
}

size_t jz_chip_clock_gen_param_count(const JZChipData *data, const char *type)
{
    const JZChipClockGen *cg = jz_chip_find_clock_gen(data, type);
    if (!cg) return 0;
    return cg->params.len / sizeof(JZChipClockGenParam);
}

const JZChipClockGenParam *jz_chip_clock_gen_param_at(
    const JZChipData *data, const char *type, size_t index)
{
    const JZChipClockGen *cg = jz_chip_find_clock_gen(data, type);
    if (!cg) return NULL;
    size_t p_count = cg->params.len / sizeof(JZChipClockGenParam);
    if (index >= p_count) return NULL;
    const JZChipClockGenParam *ps = (const JZChipClockGenParam *)cg->params.data;
    return &ps[index];
}

int jz_chip_clock_gen_output_is_clock(const JZChipData *data,
                                      const char *type,
                                      const char *selector)
{
    if (!data || !type || !selector) return -1;
    const JZChipClockGen *cg = jz_chip_find_clock_gen(data, type);
    if (!cg) return -1;
    size_t o_count = cg->outputs.len / sizeof(JZChipClockGenOutput);
    const JZChipClockGenOutput *os = (const JZChipClockGenOutput *)cg->outputs.data;
    for (size_t j = 0; j < o_count; ++j) {
        if (os[j].selector && strcmp(os[j].selector, selector) == 0) {
            return os[j].is_clock;
        }
    }
    return -1;
}

const char *jz_chip_clock_gen_output_freq_expr(const JZChipData *data,
                                                const char *type,
                                                const char *selector)
{
    if (!data || !type || !selector) return NULL;
    const JZChipClockGen *cg = jz_chip_find_clock_gen(data, type);
    if (!cg) return NULL;
    size_t o_count = cg->outputs.len / sizeof(JZChipClockGenOutput);
    const JZChipClockGenOutput *os = (const JZChipClockGenOutput *)cg->outputs.data;
    for (size_t j = 0; j < o_count; ++j) {
        if (os[j].selector && strcmp(os[j].selector, selector) == 0) {
            return os[j].frequency_expr;
        }
    }
    return NULL;
}

int jz_chip_clock_gen_output_valid(const JZChipData *data,
                                   const char *type,
                                   const char *selector)
{
    if (!data || !type || !selector) return 0;
    const JZChipClockGen *cg = jz_chip_find_clock_gen(data, type);
    if (!cg) return 0;
    size_t o_count = cg->outputs.len / sizeof(JZChipClockGenOutput);
    const JZChipClockGenOutput *os = (const JZChipClockGenOutput *)cg->outputs.data;
    for (size_t j = 0; j < o_count; ++j) {
        if (os[j].selector && strcmp(os[j].selector, selector) == 0) {
            return 1;
        }
    }
    return 0;
}

const char *jz_chip_clock_gen_output_phase_expr(const JZChipData *data,
                                                 const char *type,
                                                 const char *selector)
{
    if (!data || !type || !selector) return NULL;
    const JZChipClockGen *cg = jz_chip_find_clock_gen(data, type);
    if (!cg) return NULL;
    size_t o_count = cg->outputs.len / sizeof(JZChipClockGenOutput);
    const JZChipClockGenOutput *os = (const JZChipClockGenOutput *)cg->outputs.data;
    for (size_t j = 0; j < o_count; ++j) {
        if (os[j].selector && strcmp(os[j].selector, selector) == 0) {
            return os[j].phase_deg_expr;
        }
    }
    return NULL;
}

int jz_chip_clock_gen_refclk_range(const JZChipData *data,
                                    const char *type,
                                    double *out_min, double *out_max)
{
    if (!data || !type || !out_min || !out_max) return 0;
    const JZChipClockGen *cg = jz_chip_find_clock_gen(data, type);
    if (!cg || !cg->has_refclk_range) return 0;
    *out_min = cg->refclk_min_mhz;
    *out_max = cg->refclk_max_mhz;
    return 1;
}

const char *jz_chip_clock_gen_input_default(const JZChipData *data,
                                             const char *type,
                                             const char *input_name)
{
    const JZChipClockGenInput *inp = jz_chip_clock_gen_input(data, type, input_name);
    return inp ? inp->default_value : NULL;
}

const JZChipClockGenInput *jz_chip_clock_gen_input(const JZChipData *data,
                                                     const char *type,
                                                     const char *input_name)
{
    if (!data || !type || !input_name) return NULL;
    const JZChipClockGen *cg = jz_chip_find_clock_gen(data, type);
    if (!cg) return NULL;
    size_t in_count = cg->inputs.len / sizeof(JZChipClockGenInput);
    const JZChipClockGenInput *ins = (const JZChipClockGenInput *)cg->inputs.data;
    for (size_t i = 0; i < in_count; ++i) {
        if (ins[i].name && jz_strcasecmp(ins[i].name, input_name) == 0) {
            return &ins[i];
        }
    }
    return NULL;
}

size_t jz_chip_clock_gen_input_count(const JZChipData *data, const char *type)
{
    if (!data || !type) return 0;
    const JZChipClockGen *cg = jz_chip_find_clock_gen(data, type);
    if (!cg) return 0;
    return cg->inputs.len / sizeof(JZChipClockGenInput);
}

const JZChipClockGenInput *jz_chip_clock_gen_input_at(
    const JZChipData *data, const char *type, size_t index)
{
    if (!data || !type) return NULL;
    const JZChipClockGen *cg = jz_chip_find_clock_gen(data, type);
    if (!cg) return NULL;
    size_t in_count = cg->inputs.len / sizeof(JZChipClockGenInput);
    if (index >= in_count) return NULL;
    const JZChipClockGenInput *ins = (const JZChipClockGenInput *)cg->inputs.data;
    return &ins[index];
}

/* Helper to find a template in a JZChipDiffPrimitive by backend name. */
static const char *jz_chip_diff_find_map_in(const JZBuffer *maps_buf,
                                              const char *backend)
{
    if (!maps_buf || !backend) return NULL;
    size_t map_count = maps_buf->len / sizeof(JZChipDiffMap);
    const JZChipDiffMap *maps = (const JZChipDiffMap *)maps_buf->data;
    for (size_t i = 0; i < map_count; ++i) {
        if (maps[i].backend && jz_strcasecmp(maps[i].backend, backend) == 0) {
            return maps[i].template_text;
        }
    }
    return NULL;
}

static const char *jz_chip_diff_find_map(const JZChipDiffPrimitive *prim,
                                          const char *backend)
{
    if (!prim || !backend) return NULL;
    return jz_chip_diff_find_map_in(&prim->maps, backend);
}

const char *jz_chip_diff_output_buffer_map(const JZChipData *data,
                                            const char *backend)
{
    if (!data || !backend || !data->differential.has_output_buffer) return NULL;
    return jz_chip_diff_find_map(&data->differential.output_buffer, backend);
}

const char *jz_chip_diff_output_serializer_map(const JZChipData *data,
                                                const char *backend)
{
    if (!data || !backend || !data->differential.has_output_serializer) return NULL;
    /* Return the template for the smallest available serializer */
    int ratio = jz_chip_diff_serializer_ratio(data);
    if (ratio <= 0) return NULL;
    return jz_chip_diff_best_serializer_map(data, 1, backend);
}

const char *jz_chip_diff_input_buffer_map(const JZChipData *data,
                                           const char *backend)
{
    if (!data || !backend || !data->differential.has_input_buffer) return NULL;
    return jz_chip_diff_find_map(&data->differential.input_buffer, backend);
}

const char *jz_chip_diff_clock_buffer_map(const JZChipData *data,
                                           const char *backend)
{
    if (!data || !backend || !data->differential.has_clock_buffer) return NULL;
    return jz_chip_diff_find_map(&data->differential.clock_buffer, backend);
}

int jz_chip_diff_serializer_ratio(const JZChipData *data)
{
    if (!data || !data->differential.has_output_serializer) return 0;
    /* Return the smallest available ratio */
    size_t n = data->differential.output_serializers.len / sizeof(JZChipDiffPrimitive);
    const JZChipDiffPrimitive *sers =
        (const JZChipDiffPrimitive *)data->differential.output_serializers.data;
    int best = 0;
    for (size_t i = 0; i < n; ++i) {
        if (best == 0 || sers[i].ratio < best) best = sers[i].ratio;
    }
    return best;
}

int jz_chip_diff_best_serializer_ratio(const JZChipData *data, int needed_width)
{
    if (!data || !data->differential.has_output_serializer || needed_width <= 0) return 0;
    size_t n = data->differential.output_serializers.len / sizeof(JZChipDiffPrimitive);
    const JZChipDiffPrimitive *sers =
        (const JZChipDiffPrimitive *)data->differential.output_serializers.data;
    int best = 0;
    for (size_t i = 0; i < n; ++i) {
        if (sers[i].ratio >= needed_width) {
            if (best == 0 || sers[i].ratio < best) best = sers[i].ratio;
        }
    }
    return best;
}

const char *jz_chip_diff_best_serializer_map(const JZChipData *data,
                                              int needed_width,
                                              const char *backend)
{
    if (!data || !backend || !data->differential.has_output_serializer ||
        needed_width <= 0) return NULL;
    size_t n = data->differential.output_serializers.len / sizeof(JZChipDiffPrimitive);
    const JZChipDiffPrimitive *sers =
        (const JZChipDiffPrimitive *)data->differential.output_serializers.data;
    const JZChipDiffPrimitive *best = NULL;
    for (size_t i = 0; i < n; ++i) {
        if (sers[i].ratio >= needed_width) {
            if (!best || sers[i].ratio < best->ratio) best = &sers[i];
        }
    }
    if (!best) return NULL;
    return jz_chip_diff_find_map_in(&best->maps, backend);
}

int jz_chip_diff_max_serializer_ratio(const JZChipData *data)
{
    if (!data || !data->differential.has_output_serializer) return 0;
    size_t n = data->differential.output_serializers.len / sizeof(JZChipDiffPrimitive);
    const JZChipDiffPrimitive *sers =
        (const JZChipDiffPrimitive *)data->differential.output_serializers.data;
    int max_r = 0;
    for (size_t i = 0; i < n; ++i) {
        if (sers[i].ratio > max_r) max_r = sers[i].ratio;
    }
    return max_r;
}

const char *jz_chip_diff_input_deserializer_map(const JZChipData *data,
                                                  const char *backend)
{
    if (!data || !backend || !data->differential.has_input_deserializer) return NULL;
    /* Return the template for the smallest available deserializer */
    int ratio = jz_chip_diff_deserializer_ratio(data);
    if (ratio <= 0) return NULL;
    return jz_chip_diff_best_deserializer_map(data, 1, backend);
}

int jz_chip_diff_deserializer_ratio(const JZChipData *data)
{
    if (!data || !data->differential.has_input_deserializer) return 0;
    /* Return the smallest available ratio */
    size_t n = data->differential.input_deserializers.len / sizeof(JZChipDiffPrimitive);
    const JZChipDiffPrimitive *desers =
        (const JZChipDiffPrimitive *)data->differential.input_deserializers.data;
    int best = 0;
    for (size_t i = 0; i < n; ++i) {
        if (best == 0 || desers[i].ratio < best) best = desers[i].ratio;
    }
    return best;
}

int jz_chip_diff_best_deserializer_ratio(const JZChipData *data, int needed_width)
{
    if (!data || !data->differential.has_input_deserializer || needed_width <= 0) return 0;
    size_t n = data->differential.input_deserializers.len / sizeof(JZChipDiffPrimitive);
    const JZChipDiffPrimitive *desers =
        (const JZChipDiffPrimitive *)data->differential.input_deserializers.data;
    int best = 0;
    for (size_t i = 0; i < n; ++i) {
        if (desers[i].ratio >= needed_width) {
            if (best == 0 || desers[i].ratio < best) best = desers[i].ratio;
        }
    }
    return best;
}

const char *jz_chip_diff_best_deserializer_map(const JZChipData *data,
                                                int needed_width,
                                                const char *backend)
{
    if (!data || !backend || !data->differential.has_input_deserializer ||
        needed_width <= 0) return NULL;
    size_t n = data->differential.input_deserializers.len / sizeof(JZChipDiffPrimitive);
    const JZChipDiffPrimitive *desers =
        (const JZChipDiffPrimitive *)data->differential.input_deserializers.data;
    const JZChipDiffPrimitive *best = NULL;
    for (size_t i = 0; i < n; ++i) {
        if (desers[i].ratio >= needed_width) {
            if (!best || desers[i].ratio < best->ratio) best = &desers[i];
        }
    }
    if (!best) return NULL;
    return jz_chip_diff_find_map_in(&best->maps, backend);
}

int jz_chip_diff_max_deserializer_ratio(const JZChipData *data)
{
    if (!data || !data->differential.has_input_deserializer) return 0;
    size_t n = data->differential.input_deserializers.len / sizeof(JZChipDiffPrimitive);
    const JZChipDiffPrimitive *desers =
        (const JZChipDiffPrimitive *)data->differential.input_deserializers.data;
    int max_r = 0;
    for (size_t i = 0; i < n; ++i) {
        if (desers[i].ratio > max_r) max_r = desers[i].ratio;
    }
    return max_r;
}

int jz_chip_diff_serializer_required_clocks(const JZChipData *data,
                                             int needed_width,
                                             int *out_fclk,
                                             int *out_pclk,
                                             int *out_reset)
{
    if (!data || !data->differential.has_output_serializer || needed_width <= 0)
        return 0;
    size_t n = data->differential.output_serializers.len / sizeof(JZChipDiffPrimitive);
    const JZChipDiffPrimitive *sers =
        (const JZChipDiffPrimitive *)data->differential.output_serializers.data;
    const JZChipDiffPrimitive *best = NULL;
    for (size_t i = 0; i < n; ++i) {
        if (sers[i].ratio >= needed_width) {
            if (!best || sers[i].ratio < best->ratio) best = &sers[i];
        }
    }
    if (!best) return 0;
    if (out_fclk)  *out_fclk  = best->requires_fclk;
    if (out_pclk)  *out_pclk  = best->requires_pclk;
    if (out_reset) *out_reset = best->requires_reset;
    return 1;
}

int jz_chip_diff_deserializer_required_clocks(const JZChipData *data,
                                               int needed_width,
                                               int *out_fclk,
                                               int *out_pclk,
                                               int *out_reset)
{
    if (!data || !data->differential.has_input_deserializer || needed_width <= 0)
        return 0;
    size_t n = data->differential.input_deserializers.len / sizeof(JZChipDiffPrimitive);
    const JZChipDiffPrimitive *desers =
        (const JZChipDiffPrimitive *)data->differential.input_deserializers.data;
    const JZChipDiffPrimitive *best = NULL;
    for (size_t i = 0; i < n; ++i) {
        if (desers[i].ratio >= needed_width) {
            if (!best || desers[i].ratio < best->ratio) best = &desers[i];
        }
    }
    if (!best) return 0;
    if (out_fclk)  *out_fclk  = best->requires_fclk;
    if (out_pclk)  *out_pclk  = best->requires_pclk;
    if (out_reset) *out_reset = best->requires_reset;
    return 1;
}

const char *jz_chip_diff_io_type(const JZChipData *data)
{
    if (!data) return NULL;
    return data->differential.io_type;
}

const char *jz_chip_diff_type(const JZChipData *data)
{
    if (!data) return NULL;
    return data->differential.diff_type;
}

int jz_chip_clock_gen_count(const JZChipData *data, const char *type)
{
    if (!data || !type) return 0;
    const JZChipClockGen *cg = jz_chip_find_clock_gen(data, type);
    if (!cg) return 0;
    return cg->count;
}

int jz_chip_clock_gen_chaining(const JZChipData *data, const char *type,
                                int *out_chaining)
{
    if (!data || !type) return 0;
    const JZChipClockGen *cg = jz_chip_find_clock_gen(data, type);
    if (!cg || !cg->has_chaining) return 0;
    if (out_chaining) *out_chaining = cg->chaining;
    return 1;
}

int jz_chip_clock_gen_pad_exclusive(const JZChipData *data, const char *type,
                                     int *out_pad_exclusive)
{
    if (!data || !type) return 0;
    const JZChipClockGen *cg = jz_chip_find_clock_gen(data, type);
    if (!cg || !cg->has_pad_exclusive) return 0;
    if (out_pad_exclusive) *out_pad_exclusive = cg->pad_exclusive;
    return 1;
}

size_t jz_chip_clock_gen_constraint_count(const JZChipData *data, const char *type)
{
    if (!data || !type) return 0;
    const JZChipClockGen *cg = jz_chip_find_clock_gen(data, type);
    if (!cg) return 0;
    return cg->constraints.len / sizeof(char *);
}

const char *jz_chip_clock_gen_constraint_at(const JZChipData *data,
                                             const char *type, size_t index)
{
    if (!data || !type) return NULL;
    const JZChipClockGen *cg = jz_chip_find_clock_gen(data, type);
    if (!cg) return NULL;
    size_t ct_count = cg->constraints.len / sizeof(char *);
    if (index >= ct_count) return NULL;
    const char **cts = (const char **)cg->constraints.data;
    return cts[index];
}

const char *jz_chip_clock_gen_feedback_wire(const JZChipData *data,
                                             const char *type)
{
    if (!data || !type) return NULL;
    const JZChipClockGen *cg = jz_chip_find_clock_gen(data, type);
    if (!cg) return NULL;
    return cg->feedback_wire;
}

unsigned jz_chip_mem_quantity(const JZChipData *data, JZChipMemType type)
{
    if (!data || data->mem_resources.len == 0) return 0;
    size_t count = data->mem_resources.len / sizeof(JZChipMemResource);
    const JZChipMemResource *res = (const JZChipMemResource *)data->mem_resources.data;
    for (size_t i = 0; i < count; ++i) {
        if (res[i].type == type) return res[i].quantity;
    }
    return 0;
}

unsigned jz_chip_mem_total_bits(const JZChipData *data, JZChipMemType type)
{
    if (!data || data->mem_resources.len == 0) return 0;
    size_t count = data->mem_resources.len / sizeof(JZChipMemResource);
    const JZChipMemResource *res = (const JZChipMemResource *)data->mem_resources.data;
    for (size_t i = 0; i < count; ++i) {
        if (res[i].type == type) return res[i].total_bits;
    }
    return 0;
}
