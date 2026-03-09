#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "sem_driver.h"
#include "chip_data.h"
#include "util.h"
#include "../sem/driver_internal.h"
#include "../chip_data_internal.h"

typedef struct JZChipMemConfigEntry {
    unsigned width;
    unsigned depth;
} JZChipMemConfigEntry;

typedef struct JZChipMemModeInfo {
    char    *name;
    unsigned r_ports;
    unsigned w_ports;
    unsigned port_count; /* Number of physical ports (1=shared, 2=separate) */
    JZBuffer configs; /* JZChipMemConfigEntry[] */
} JZChipMemModeInfo;

typedef struct JZChipMemInfo {
    JZChipMemType type;
    unsigned total_bits;
    unsigned quantity;
    unsigned bits_per_block;
    unsigned r_ports;
    unsigned w_ports;
    JZBuffer configs; /* JZChipMemConfigEntry[] */
    JZBuffer modes;   /* JZChipMemModeInfo[] */
} JZChipMemInfo;

static int g_mem_report_enabled = 0;
static FILE *g_mem_report_out = NULL;
static char g_mem_report_generated[64];
static const char *g_mem_report_version = NULL;
static const char *g_mem_report_input = NULL;

void jz_sem_enable_memory_report(FILE *out,
                                 const char *tool_version,
                                 const char *input_filename)
{
    g_mem_report_enabled = (out != NULL);
    g_mem_report_out = out;
    g_mem_report_version = tool_version;
    g_mem_report_input = input_filename;

    time_t now = time(NULL);
    struct tm tm_info;
    if (localtime_r(&now, &tm_info) != NULL) {
        if (strftime(g_mem_report_generated,
                     sizeof(g_mem_report_generated),
                     "%Y-%m-%d %H:%M %Z",
                     &tm_info) == 0) {
            snprintf(g_mem_report_generated,
                     sizeof(g_mem_report_generated),
                     "<unknown>");
        }
    } else {
        snprintf(g_mem_report_generated,
                 sizeof(g_mem_report_generated),
                 "<unknown>");
    }
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
    size_t total = dir_len + 1 + chip_len + 5;
    char *out = (char *)malloc(total);
    if (!out) return NULL;

    memcpy(out, dir, dir_len);
    out[dir_len] = '\0';
    strcat(out, "/");
    strcat(out, chip_id);
    strcat(out, ".json");
    return out;
}

static const char *jz_load_chip_json(const char *chip_id,
                                     const char *project_filename,
                                     char **out_owned)
{
    if (out_owned) *out_owned = NULL;
    if (!chip_id || !chip_id[0]) return NULL;

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

    if (json) {
        if (out_owned) *out_owned = json;
        return json;
    }

    const char *builtin = jz_chip_builtin_json(chip_id);
    if (!builtin) {
        char *upper = jz_strdup_upper(chip_id);
        if (upper) {
            builtin = jz_chip_builtin_json(upper);
        }
        free(upper);
    }
    return builtin;
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

static void mem_mode_free(JZChipMemModeInfo *mode)
{
    if (!mode) return;
    free(mode->name);
    mode->name = NULL;
    jz_buf_free(&mode->configs);
}

static void mem_info_free(JZChipMemInfo *info)
{
    if (!info) return;
    if (info->modes.len > 0) {
        size_t count = info->modes.len / sizeof(JZChipMemModeInfo);
        JZChipMemModeInfo *modes = (JZChipMemModeInfo *)info->modes.data;
        for (size_t i = 0; i < count; ++i) {
            mem_mode_free(&modes[i]);
        }
        jz_buf_free(&info->modes);
    }
    jz_buf_free(&info->configs);
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
            JZChipMemConfigEntry cfg;
            cfg.width = width;
            cfg.depth = depth;
            (void)jz_buf_append(out, &cfg, sizeof(cfg));
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
        int r_idx = -1;
        int w_idx = -1;
        int cfg_idx = -1;
        int ports_idx = -1;
        int idx = cur + 1;
        while (idx < count && toks[idx].start < mode->end) {
            const jsmntok_t *key = &toks[idx++];
            if (jz_json_token_eq(json, key, "name")) {
                name_idx = idx;
            } else if (jz_json_token_eq(json, key, "r_ports")) {
                r_idx = idx;
            } else if (jz_json_token_eq(json, key, "w_ports")) {
                w_idx = idx;
            } else if (jz_json_token_eq(json, key, "ports")) {
                ports_idx = idx;
            } else if (jz_json_token_eq(json, key, "configurations")) {
                cfg_idx = idx;
            }
            idx = jz_json_skip(toks, count, idx);
        }

        JZChipMemModeInfo info;
        memset(&info, 0, sizeof(info));
        if (name_idx >= 0) {
            char name_buf[128] = {0};
            if (jz_json_token_to_string(json, &toks[name_idx], name_buf, sizeof(name_buf))) {
                info.name = jz_strdup(name_buf);
            }
        }
        /* Parse legacy r_ports/w_ports if present (assume shared port) */
        if (r_idx >= 0) {
            (void)jz_json_token_to_uint(json, &toks[r_idx], &info.r_ports);
            info.port_count = 1;  /* Legacy format implies shared port */
        }
        if (w_idx >= 0) {
            (void)jz_json_token_to_uint(json, &toks[w_idx], &info.w_ports);
            info.port_count = 1;  /* Legacy format implies shared port */
        }
        /* Parse new-style ports array if present */
        if (ports_idx >= 0 && toks[ports_idx].type == JSMN_ARRAY) {
            const jsmntok_t *parr = &toks[ports_idx];
            info.port_count = (unsigned)parr->size;  /* Number of physical ports */
            int pidx = ports_idx + 1;
            for (int pi = 0; pi < parr->size && pidx < count; ++pi) {
                const jsmntok_t *port_obj = &toks[pidx];
                if (port_obj->type == JSMN_OBJECT) {
                    int pcur = pidx + 1;
                    while (pcur < count && toks[pcur].start < port_obj->end) {
                        const jsmntok_t *pkey = &toks[pcur++];
                        const jsmntok_t *pval = &toks[pcur];
                        if (jz_json_token_eq(json, pkey, "read")) {
                            if (pval->type == JSMN_PRIMITIVE &&
                                json[pval->start] == 't') {
                                info.r_ports++;
                            }
                        } else if (jz_json_token_eq(json, pkey, "write")) {
                            if (pval->type == JSMN_PRIMITIVE &&
                                json[pval->start] == 't') {
                                info.w_ports++;
                            }
                        }
                        pcur = jz_json_skip(toks, count, pcur);
                    }
                }
                pidx = jz_json_skip(toks, count, pidx);
            }
        }
        if (cfg_idx >= 0) {
            jz_collect_configs(json, toks, count, cfg_idx, &info.configs);
        }
        if (jz_buf_append(out, &info, sizeof(info)) != 0) {
            mem_mode_free(&info);
            return;
        }

        cur = jz_json_skip(toks, count, cur);
    }
}

static void jz_collect_chip_memory(const char *json,
                                   const jsmntok_t *toks,
                                   int count,
                                   int memory_idx,
                                   JZBuffer *out)
{
    if (!json || !toks || !out || memory_idx < 0 || memory_idx >= count) return;
    const jsmntok_t *arr = &toks[memory_idx];
    if (arr->type != JSMN_ARRAY) return;
    int cur = memory_idx + 1;
    while (cur < count && toks[cur].start < arr->end) {
        const jsmntok_t *obj = &toks[cur];
        if (obj->type != JSMN_OBJECT) {
            cur = jz_json_skip(toks, count, cur);
            continue;
        }

        int type_idx = -1;
        int total_bits_idx = -1;
        int r_ports_idx = -1;
        int w_ports_idx = -1;
        int configs_idx = -1;
        int modes_idx = -1;
        int quantity_idx = -1;
        int bits_per_block_idx = -1;
        int capacity_mbits_idx = -1;

        int obj_cur = cur + 1;
        while (obj_cur < count && toks[obj_cur].start < obj->end) {
            const jsmntok_t *key = &toks[obj_cur++];
            if (jz_json_token_eq(json, key, "type")) {
                type_idx = obj_cur;
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
            } else if (jz_json_token_eq(json, key, "capacity_mbits")) {
                capacity_mbits_idx = obj_cur;
            }
            obj_cur = jz_json_skip(toks, count, obj_cur);
        }

        JZChipMemInfo info;
        memset(&info, 0, sizeof(info));
        if (type_idx >= 0) {
            info.type = jz_chip_mem_type_from_token(json, &toks[type_idx]);
        }
        if (total_bits_idx >= 0) {
            (void)jz_json_token_to_uint(json, &toks[total_bits_idx], &info.total_bits);
        }
        /* Fall back to capacity_mbits (e.g., SDRAM entries) if total_bits absent */
        if (info.total_bits == 0 && capacity_mbits_idx >= 0) {
            unsigned cap_mbits = 0;
            if (jz_json_token_to_uint(json, &toks[capacity_mbits_idx], &cap_mbits) && cap_mbits > 0) {
                info.total_bits = cap_mbits * (1024u * 1024u);
            }
        }
        if (quantity_idx >= 0) {
            (void)jz_json_token_to_uint(json, &toks[quantity_idx], &info.quantity);
        }
        if (bits_per_block_idx >= 0) {
            (void)jz_json_token_to_uint(json, &toks[bits_per_block_idx], &info.bits_per_block);
        }
        if (r_ports_idx >= 0) {
            (void)jz_json_token_to_uint(json, &toks[r_ports_idx], &info.r_ports);
        }
        if (w_ports_idx >= 0) {
            (void)jz_json_token_to_uint(json, &toks[w_ports_idx], &info.w_ports);
        }
        if (configs_idx >= 0) {
            jz_collect_configs(json, toks, count, configs_idx, &info.configs);
        }
        if (modes_idx >= 0) {
            jz_collect_modes(json, toks, count, modes_idx, &info.modes);
        }

        if (jz_buf_append(out, &info, sizeof(info)) != 0) {
            mem_info_free(&info);
            return;
        }
        cur = jz_json_skip(toks, count, cur);
    }
}

static JZChipMemType sem_mem_header_parse_type(const char *attrs)
{
    if (!attrs) return JZ_CHIP_MEM_UNKNOWN;
    const char *p = strstr(attrs, "TYPE");
    if (!p) p = strstr(attrs, "type");
    if (!p) return JZ_CHIP_MEM_UNKNOWN;
    p += 4;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '=') return JZ_CHIP_MEM_UNKNOWN;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    if (strncmp(p, "BLOCK", 5) == 0 || strncmp(p, "block", 5) == 0) {
        return JZ_CHIP_MEM_BLOCK;
    }
    if (strncmp(p, "DISTRIBUTED", 11) == 0 || strncmp(p, "distributed", 11) == 0) {
        return JZ_CHIP_MEM_DISTRIBUTED;
    }
    return JZ_CHIP_MEM_UNKNOWN;
}

static void sem_mem_decl_port_counts(const JZASTNode *mem_decl,
                                     unsigned *out_r_ports,
                                     unsigned *out_w_ports,
                                     int *out_all_sync)
{
    unsigned r_ports = 0;
    unsigned w_ports = 0;
    int all_sync = 1;

    if (mem_decl) {
        for (size_t k = 0; k < mem_decl->child_count; ++k) {
            JZASTNode *port = mem_decl->children[k];
            if (!port || port->type != JZ_AST_MEM_PORT || !port->block_kind) {
                continue;
            }
            if (strcmp(port->block_kind, "OUT") == 0) {
                r_ports++;
                if (!port->text || strcmp(port->text, "SYNC") != 0) {
                    all_sync = 0;
                }
            } else if (strcmp(port->block_kind, "IN") == 0) {
                w_ports++;
            } else if (strcmp(port->block_kind, "INOUT") == 0) {
                /* INOUT counts as both read and write (always synchronous) */
                r_ports++;
                w_ports++;
                /* INOUT ports are always synchronous */
            }
        }
    }

    if (out_r_ports) *out_r_ports = r_ports;
    if (out_w_ports) *out_w_ports = w_ports;
    if (out_all_sync) *out_all_sync = all_sync;
}

static JZChipMemType sem_mem_infer_type(unsigned depth, int have_depth, int all_sync)
{
    if (!have_depth) return JZ_CHIP_MEM_UNKNOWN;
    if (depth <= 16) {
        return JZ_CHIP_MEM_DISTRIBUTED;
    }
    if (all_sync) {
        return JZ_CHIP_MEM_BLOCK;
    }
    return JZ_CHIP_MEM_DISTRIBUTED;
}

static unsigned sem_addr_width_for_depth(unsigned depth)
{
    unsigned addr_width = 0;
    if (depth > 1) {
        unsigned v = depth - 1u;
        while (v) {
            addr_width++;
            v >>= 1;
        }
    }
    /* Minimum address width is 1 bit. */
    if (addr_width == 0) addr_width = 1;
    return addr_width;
}

static const JZChipMemInfo *find_chip_mem_info(const JZBuffer *mems, JZChipMemType type)
{
    if (!mems || mems->len == 0) return NULL;
    size_t count = mems->len / sizeof(JZChipMemInfo);
    const JZChipMemInfo *arr = (const JZChipMemInfo *)mems->data;
    for (size_t i = 0; i < count; ++i) {
        if (arr[i].type == type) {
            return &arr[i];
        }
    }
    return NULL;
}

static const JZChipMemModeInfo *find_best_mode(const JZChipMemInfo *info,
                                               unsigned r_ports,
                                               unsigned w_ports,
                                               unsigned port_count)
{
    if (!info || info->modes.len == 0) return NULL;
    const JZChipMemModeInfo *best = NULL;
    size_t count = info->modes.len / sizeof(JZChipMemModeInfo);
    const JZChipMemModeInfo *modes = (const JZChipMemModeInfo *)info->modes.data;
    for (size_t i = 0; i < count; ++i) {
        const JZChipMemModeInfo *mode = &modes[i];
        if (mode->r_ports < r_ports || mode->w_ports < w_ports) {
            continue;
        }
        /* Prefer modes with matching physical port count */
        if (port_count > 0 && mode->port_count > 0 &&
            mode->port_count < port_count) {
            continue;  /* Not enough physical ports */
        }
        if (!best) {
            best = mode;
            continue;
        }
        /* Prefer exact port_count match */
        int best_pc_match = (best->port_count == port_count) ? 1 : 0;
        int mode_pc_match = (mode->port_count == port_count) ? 1 : 0;
        if (mode_pc_match > best_pc_match) {
            best = mode;
            continue;
        }
        if (mode_pc_match < best_pc_match) {
            continue;
        }
        /* Among equal port_count matches, prefer fewer total ports */
        unsigned best_ports = best->r_ports + best->w_ports;
        unsigned mode_ports = mode->r_ports + mode->w_ports;
        if (mode_ports < best_ports ||
            (mode_ports == best_ports && mode->r_ports < best->r_ports) ||
            (mode_ports == best_ports && mode->r_ports == best->r_ports &&
             mode->w_ports < best->w_ports)) {
            best = mode;
        }
    }
    return best;
}

static const JZChipMemModeInfo *find_exact_mode(const JZChipMemInfo *info,
                                                unsigned r_ports,
                                                unsigned w_ports,
                                                unsigned port_count)
{
    if (!info || info->modes.len == 0) return NULL;
    size_t count = info->modes.len / sizeof(JZChipMemModeInfo);
    const JZChipMemModeInfo *modes = (const JZChipMemModeInfo *)info->modes.data;
    for (size_t i = 0; i < count; ++i) {
        if (modes[i].r_ports == r_ports && modes[i].w_ports == w_ports &&
            (port_count == 0 || modes[i].port_count == 0 ||
             modes[i].port_count == port_count)) {
            return &modes[i];
        }
    }
    return NULL;
}

static void format_requested_mode(char *out, size_t out_size,
                                  const JZChipMemInfo *info,
                                  unsigned r_ports,
                                  unsigned w_ports,
                                  unsigned port_count)
{
    if (!out || out_size == 0) return;
    out[0] = '\0';
    const JZChipMemModeInfo *exact = find_exact_mode(info, r_ports, w_ports, port_count);
    if (exact && exact->name && exact->name[0]) {
        snprintf(out, out_size, "%s", exact->name);
        return;
    }
    if (w_ports == 0 && r_ports > 0) {
        snprintf(out, out_size, "Read Only Memory");
        return;
    }
    snprintf(out, out_size, "Ports (%uR/%uW)", r_ports, w_ports);
}

static void format_matched_mode(char *out, size_t out_size,
                                const JZChipMemInfo *info,
                                unsigned r_ports,
                                unsigned w_ports,
                                unsigned port_count)
{
    if (!out || out_size == 0) return;
    out[0] = '\0';
    const JZChipMemModeInfo *best = find_best_mode(info, r_ports, w_ports, port_count);
    if (best && best->name && best->name[0]) {
        snprintf(out, out_size, "%s", best->name);
        return;
    }
    if (info && info->r_ports >= r_ports && info->w_ports >= w_ports &&
        (info->r_ports || info->w_ports)) {
        snprintf(out, out_size, "Ports (%uR/%uW)", info->r_ports, info->w_ports);
        return;
    }
    snprintf(out, out_size, "Not supported by chip");
}

static unsigned long long bits_to_bytes_ceil(unsigned long long bits)
{
    return (bits + 7ull) / 8ull;
}

static void print_padded(FILE *out, const char *s, size_t width)
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

void sem_emit_memory_report(JZASTNode *root,
                            const JZBuffer *module_scopes,
                            const JZBuffer *project_symbols,
                            const JZChipData *chip,
                            const char *input_filename)
{
    if (!g_mem_report_enabled || !g_mem_report_out || !root || !module_scopes) {
        return;
    }

    (void)g_mem_report_version;
    (void)g_mem_report_generated;
    (void)chip;
    if (!input_filename && g_mem_report_input) {
        input_filename = g_mem_report_input;
    }

    FILE *out = g_mem_report_out;
    const char *chip_id = (root->text && root->text[0]) ? root->text : NULL;
    char *json_owned = NULL;
    const char *json = chip_id ? jz_load_chip_json(chip_id, input_filename, &json_owned) : NULL;

    char chip_id_buf[128] = {0};
    JZBuffer chip_mems = (JZBuffer){0};

    if (json) {
        jsmn_parser parser;
        jsmn_init(&parser);
        int tok_count = jsmn_parse(&parser, json, strlen(json), NULL, 0);
        if (tok_count > 0) {
            jsmntok_t *toks = (jsmntok_t *)calloc((size_t)tok_count, sizeof(jsmntok_t));
            if (toks) {
                jsmn_init(&parser);
                tok_count = jsmn_parse(&parser, json, strlen(json), toks, (unsigned int)tok_count);
                if (tok_count > 0 && toks[0].type == JSMN_OBJECT) {
                    int memory_idx = -1;
                    int chipid_idx = -1;
                    int idx = 1;
                    while (idx < tok_count && toks[idx].start < toks[0].end) {
                        const jsmntok_t *key = &toks[idx++];
                        if (jz_json_token_eq(json, key, "memory")) {
                            memory_idx = idx;
                        } else if (jz_json_token_eq(json, key, "chipid")) {
                            chipid_idx = idx;
                        }
                        idx = jz_json_skip(toks, tok_count, idx);
                    }
                    if (chipid_idx >= 0) {
                        (void)jz_json_token_to_string(json, &toks[chipid_idx],
                                                      chip_id_buf, sizeof(chip_id_buf));
                    }
                    if (memory_idx >= 0) {
                        jz_collect_chip_memory(json, toks, tok_count, memory_idx, &chip_mems);
                    }
                }
                free(toks);
            }
        }
    }

    fprintf(out, "Memory Report\n");
    fprintf(out, "-------------\n");
    fprintf(out, "Project: %s\n", input_filename ? input_filename : "(unknown)");
    if (chip_id_buf[0]) {
        fprintf(out, "Chip: %s\n\n", chip_id_buf);
    } else if (chip_id) {
        fprintf(out, "Chip: %s\n\n", chip_id);
    } else {
        fprintf(out, "Chip: (unknown)\n\n");
    }

    unsigned long long block_requested_bytes = 0;
    unsigned long long dist_requested_bytes = 0;
    unsigned long long block_used_blocks = 0;
    unsigned long long error_count = 0;
    unsigned long long block_mem_count = 0;

    size_t scope_count = module_scopes->len / sizeof(JZModuleScope);
    const JZModuleScope *scopes = (const JZModuleScope *)module_scopes->data;
    for (size_t si = 0; si < scope_count; ++si) {
        const JZModuleScope *scope = &scopes[si];
        if (!scope->node) continue;
        JZASTNode *mod = scope->node;
        for (size_t i = 0; i < mod->child_count; ++i) {
            JZASTNode *block = mod->children[i];
            if (!block || block->type != JZ_AST_MEM_BLOCK) continue;

            JZChipMemType header_type = sem_mem_header_parse_type(block->text);

            for (size_t j = 0; j < block->child_count; ++j) {
                JZASTNode *mem = block->children[j];
                if (!mem || mem->type != JZ_AST_MEM_DECL) continue;

                unsigned r_ports = 0;
                unsigned w_ports = 0;
                int all_sync = 1;
                sem_mem_decl_port_counts(mem, &r_ports, &w_ports, &all_sync);
                unsigned port_count = r_ports + w_ports;  /* Number of physical ports */

                unsigned width = 0;
                long long depth_val = 0;
                int have_width = (sem_eval_width_expr(mem->width, scope, project_symbols, &width) == 0 && width > 0);
                int have_depth = (sem_eval_const_expr_in_module(mem->text, scope, project_symbols, &depth_val) == 0 && depth_val > 0);
                unsigned depth = have_depth ? (unsigned)depth_val : 0;

                JZChipMemType mem_type = header_type;
                if (mem_type == JZ_CHIP_MEM_UNKNOWN) {
                    mem_type = sem_mem_infer_type(depth, have_depth, all_sync);
                }

                unsigned addr_width = have_depth ? sem_addr_width_for_depth(depth) : 0;
                unsigned long long bits = (have_width && have_depth) ? (unsigned long long)width * (unsigned long long)depth : 0ull;
                unsigned long long bytes = bits_to_bytes_ceil(bits);

                const JZChipMemInfo *chip_info = find_chip_mem_info(&chip_mems, mem_type);
                const JZChipMemModeInfo *best_mode = find_best_mode(chip_info, r_ports, w_ports, port_count);
                const JZBuffer *cfgs = NULL;
                if (best_mode) {
                    cfgs = &best_mode->configs;
                } else if (chip_info && chip_info->configs.len > 0) {
                    cfgs = &chip_info->configs;
                }

                int matched_any = 0;
                if (cfgs && have_width && have_depth) {
                    size_t cfg_count = cfgs->len / sizeof(JZChipMemConfigEntry);
                    const JZChipMemConfigEntry *arr = (const JZChipMemConfigEntry *)cfgs->data;
                    for (size_t ci = 0; ci < cfg_count; ++ci) {
                        if (arr[ci].width >= width && arr[ci].depth >= depth) {
                            matched_any = 1;
                            break;
                        }
                    }
                }

                int has_error = 0;
                if (!chip_info || (!best_mode && (!chip_info->r_ports && !chip_info->w_ports) &&
                                   chip_info->modes.len == 0 && chip_info->configs.len == 0)) {
                    has_error = 1;
                } else if (!matched_any && have_width && have_depth) {
                    has_error = 1;
                }

                if (has_error) {
                    error_count++;
                }

                const char *type_name = jz_chip_mem_type_name(mem_type);
                const char *mem_label = (type_name && type_name[0]) ? type_name : "MEMORY";
                const char *file = mem->loc.filename ? mem->loc.filename : "(unknown)";
                const char *mem_name = mem->name ? mem->name : "";

                fprintf(out, "%s@ %d = %s MEMORY ",
                        file,
                        mem->loc.line,
                        mem_label);
                if (have_width && have_depth) {
                    fprintf(out, "%llu Bytes", bytes);
                } else {
                    fprintf(out, "Unknown size");
                }
                if (mem_name[0]) {
                    fprintf(out, " (%s)", mem_name);
                }
                if (has_error) {
                    fprintf(out, " [ERROR]");
                }
                fprintf(out, "\n");

                char req_mode[128];
                char match_mode[128];
                format_requested_mode(req_mode, sizeof(req_mode), chip_info, r_ports, w_ports, port_count);
                format_matched_mode(match_mode, sizeof(match_mode), chip_info, r_ports, w_ports, port_count);

                fprintf(out, "    Mode:\n");
                fprintf(out, "    Requested | %s\n", req_mode[0] ? req_mode : "Unknown");
                fprintf(out, "    Matched   | %s\n\n", match_mode[0] ? match_mode : "Unknown");

                fprintf(out, "    Configuration:\n");
                struct {
                    char label[16];
                    char col1[32];
                    char col2[64];
                } cfg_rows[64];
                size_t cfg_row_count = 0;
                size_t label_w = strlen("Requested");
                size_t col1_w = 0;
                size_t col2_w = 0;

                if (have_width && have_depth) {
                    snprintf(cfg_rows[cfg_row_count].label, sizeof(cfg_rows[cfg_row_count].label), "Requested");
                    snprintf(cfg_rows[cfg_row_count].col1, sizeof(cfg_rows[cfg_row_count].col1),
                             "%u bit", width);
                    snprintf(cfg_rows[cfg_row_count].col2, sizeof(cfg_rows[cfg_row_count].col2),
                             "%u bit (%u)", addr_width, depth);
                    cfg_row_count++;
                } else {
                    snprintf(cfg_rows[cfg_row_count].label, sizeof(cfg_rows[cfg_row_count].label), "Requested");
                    snprintf(cfg_rows[cfg_row_count].col1, sizeof(cfg_rows[cfg_row_count].col1),
                             "%s", mem->width ? mem->width : "Unknown");
                    snprintf(cfg_rows[cfg_row_count].col2, sizeof(cfg_rows[cfg_row_count].col2),
                             "%s", mem->text ? mem->text : "Unknown");
                    cfg_row_count++;
                }

                if (cfgs && have_width && have_depth) {
                    size_t cfg_count = cfgs->len / sizeof(JZChipMemConfigEntry);
                    const JZChipMemConfigEntry *arr = (const JZChipMemConfigEntry *)cfgs->data;
                    for (size_t ci = 0; ci < cfg_count && cfg_row_count < (sizeof(cfg_rows) / sizeof(cfg_rows[0])); ++ci) {
                        if (arr[ci].width >= width && arr[ci].depth >= depth) {
                            unsigned addr_bits = sem_addr_width_for_depth(arr[ci].depth);
                            snprintf(cfg_rows[cfg_row_count].label, sizeof(cfg_rows[cfg_row_count].label), "Matched");
                            snprintf(cfg_rows[cfg_row_count].col1, sizeof(cfg_rows[cfg_row_count].col1),
                                     "%u bit", arr[ci].width);
                            snprintf(cfg_rows[cfg_row_count].col2, sizeof(cfg_rows[cfg_row_count].col2),
                                     "%u bit", addr_bits);
                            cfg_row_count++;
                        }
                    }
                } else if (has_error && cfg_row_count < (sizeof(cfg_rows) / sizeof(cfg_rows[0]))) {
                    snprintf(cfg_rows[cfg_row_count].label, sizeof(cfg_rows[cfg_row_count].label), "Unmatched");
                    snprintf(cfg_rows[cfg_row_count].col1, sizeof(cfg_rows[cfg_row_count].col1), "Not supported");
                    snprintf(cfg_rows[cfg_row_count].col2, sizeof(cfg_rows[cfg_row_count].col2), "by chip");
                    cfg_row_count++;
                }

                for (size_t ri = 0; ri < cfg_row_count; ++ri) {
                    size_t l0 = strlen(cfg_rows[ri].label);
                    size_t l1 = strlen(cfg_rows[ri].col1);
                    size_t l2 = strlen(cfg_rows[ri].col2);
                    if (l0 > label_w) label_w = l0;
                    if (l1 > col1_w) col1_w = l1;
                    if (l2 > col2_w) col2_w = l2;
                }
                for (size_t ri = 0; ri < cfg_row_count; ++ri) {
                    fputs("    ", out);
                    print_padded(out, cfg_rows[ri].label, label_w);
                    fputs(" | ", out);
                    print_padded(out, cfg_rows[ri].col1, col1_w);
                    fputs(" | ", out);
                    print_padded(out, cfg_rows[ri].col2, col2_w);
                    fputc('\n', out);
                }

                fprintf(out, "\n\n");

                if (have_width && have_depth) {
                    switch (mem_type) {
                    case JZ_CHIP_MEM_BLOCK:
                        block_requested_bytes += bytes;
                        block_mem_count++;
                        if (chip_info && chip_info->bits_per_block > 0) {
                            unsigned long long block_bits = chip_info->bits_per_block;
                            unsigned long long need_blocks = (bits + block_bits - 1ull) / block_bits;
                            block_used_blocks += need_blocks;
                        }
                        break;
                    case JZ_CHIP_MEM_DISTRIBUTED:
                        dist_requested_bytes += bytes;
                        break;
                    default:
                        break;
                    }
                }
            }
        }
    }

    const JZChipMemInfo *block_info = find_chip_mem_info(&chip_mems, JZ_CHIP_MEM_BLOCK);
    const JZChipMemInfo *dist_info = find_chip_mem_info(&chip_mems, JZ_CHIP_MEM_DISTRIBUTED);

    fprintf(out, "Memory Summary\n");
    fprintf(out, "--------------\n");

    fprintf(out, "Block Memory:\n");
    fprintf(out, "    Requested: %llu bytes\n", block_requested_bytes);
    if (block_info && block_info->quantity && block_info->bits_per_block) {
        unsigned long long max_bytes = bits_to_bytes_ceil((unsigned long long)block_info->quantity *
                                                          (unsigned long long)block_info->bits_per_block);
        fprintf(out, "    Max Available: %llu bytes (%u blocks x %u Kbits)\n",
                max_bytes,
                block_info->quantity,
                block_info->bits_per_block / 1024u);
        fprintf(out, "    Used: %llu / %u blocks",
                block_used_blocks,
                block_info->quantity);
        if (block_used_blocks == block_mem_count) {
            fprintf(out, " (one per BLOCK MEMORY)");
        }
        fprintf(out, "\n");
    } else if (block_info && block_info->total_bits) {
        unsigned long long max_bytes = bits_to_bytes_ceil(block_info->total_bits);
        fprintf(out, "    Max Available: %llu bytes\n", max_bytes);
        fprintf(out, "    Used: %llu bytes\n", block_requested_bytes);
    } else {
        fprintf(out, "    Max Available: Unknown\n");
        fprintf(out, "    Used: %llu bytes\n", block_requested_bytes);
    }
    fprintf(out, "\n");

    fprintf(out, "Distributed Memory:\n");
    fprintf(out, "    Requested: %llu bytes\n", dist_requested_bytes);
    if (dist_info && dist_info->total_bits) {
        unsigned long long max_bytes = bits_to_bytes_ceil(dist_info->total_bits);
        fprintf(out, "    Max Available: %llu bytes\n", max_bytes);
        fprintf(out, "    Used: %llu bytes\n", dist_requested_bytes);
    } else {
        fprintf(out, "    Max Available: Unknown\n");
        fprintf(out, "    Used: %llu bytes\n", dist_requested_bytes);
    }
    fprintf(out, "\n");

    /* Show SDRAM/FLASH if present in chip data */
    {
        const JZChipMemInfo *sdram_info = find_chip_mem_info(&chip_mems, JZ_CHIP_MEM_SDRAM);
        const JZChipMemInfo *flash_info = find_chip_mem_info(&chip_mems, JZ_CHIP_MEM_FLASH);
        if (sdram_info) {
            fprintf(out, "SDRAM (on-chip):\n");
            if (sdram_info->total_bits) {
                unsigned long long max_bytes = bits_to_bytes_ceil(sdram_info->total_bits);
                fprintf(out, "    Capacity: %llu bytes\n", max_bytes);
            }
            fprintf(out, "\n");
        }
        if (flash_info) {
            fprintf(out, "Flash (on-chip):\n");
            if (flash_info->total_bits) {
                unsigned long long max_bytes = bits_to_bytes_ceil(flash_info->total_bits);
                fprintf(out, "    Capacity: %llu bytes\n", max_bytes);
            }
            fprintf(out, "\n");
        }
    }

    fprintf(out, "Errors: %llu\n", error_count);

    if (json_owned) {
        free(json_owned);
    }
    if (chip_mems.len > 0) {
        size_t count = chip_mems.len / sizeof(JZChipMemInfo);
        JZChipMemInfo *arr = (JZChipMemInfo *)chip_mems.data;
        for (size_t i = 0; i < count; ++i) {
            mem_info_free(&arr[i]);
        }
        jz_buf_free(&chip_mems);
    }
}
