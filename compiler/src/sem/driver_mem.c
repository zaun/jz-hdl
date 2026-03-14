#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

#include "sem_driver.h"
#include "sem.h"
#include "driver_internal.h"
#include "chip_data.h"
#include "path_security.h"

/* Provided by driver.c; used here to enforce x-free MEM initialization
 * literals consistent with the Observability Rule and literal semantics.
 */
int sem_literal_has_x_bits(const char *lex);
#include "rules.h"
#include "driver_internal.h"

/* -------------------------------------------------------------------------
 *  MEM helpers (declaration/introspection) used by MEM_* rules
 * -------------------------------------------------------------------------
 */

/* Forward declarations for local MUX helper functions defined later. */
static void sem_check_mux_aggregate_decl(JZASTNode *mux_decl,
                                         const JZModuleScope *scope,
                                         JZDiagnosticList *diagnostics);
static void sem_check_mux_slice_decl(JZASTNode *mux_decl,
                                     const JZModuleScope *scope,
                                     JZDiagnosticList *diagnostics);

/* sem_extract_identifier_like: now shared from driver.c via driver_internal.h */

/* Heuristic to decide whether a MEM initializer literal node is actually
 * the payload of an @file("...") initializer. The parser represents
 * @file paths as Literal nodes whose text is the decoded string contents,
 * while numeric literals contain only digits/underscores and optional
 * sized-literal syntax.
 */
static int sem_mem_init_looks_like_file_path(const char *s)
{
    if (!s || !*s) return 0;
    for (const char *p = s; *p; ++p) {
        /* Directory separators or dots strongly indicate a path/filename. */
        if (*p == '/' || *p == '\\' || *p == '.') return 1;
        /* Whitespace inside the literal is also not valid for numeric forms. */
        if (isspace((unsigned char)*p)) return 1;
    }
    return 0;
}

/* Build a filesystem path for a MEM @file initializer. If the initializer
 * path already contains a directory component, use it as-is. Otherwise,
 * treat it as relative to the directory of the source file that declared
 * the MEM.
 */
/* Extract a case-insensitive file extension from a path, ignoring any
 * directory components. Returns a pointer into the original string, or
 * NULL if no extension is present.
 */
static const char *sem_mem_get_file_ext(const char *path)
{
    if (!path) return NULL;

    const char *last_slash = strrchr(path, '/');
#ifdef _WIN32
    const char *last_bslash = strrchr(path, '\\');
    if (!last_slash || (last_bslash && last_bslash > last_slash)) {
        last_slash = last_bslash;
    }
#endif
    const char *name = last_slash ? last_slash + 1 : path;
    const char *dot = strrchr(name, '.');
    if (!dot || !*(dot + 1)) {
        return NULL;
    }
    return dot + 1;
}

/* Case-insensitive equality for short ASCII extensions. */
static int sem_mem_ext_equals(const char *ext, const char *want)
{
    if (!ext || !want) return 0;
    while (*ext && *want) {
        char c1 = (char)tolower((unsigned char)*ext);
        char c2 = (char)tolower((unsigned char)*want);
        if (c1 != c2) return 0;
        ++ext;
        ++want;
    }
    return *ext == '\0' && *want == '\0';
}

/* Count logical bits in a hex text file: 0-9, A-F, a-f are treated as
 * hexadecimal digits contributing 4 bits each. Underscores and whitespace
 * (including newlines) are ignored; any other characters are skipped.
 */
static unsigned long long sem_mem_count_bits_hex_file(FILE *fp)
{
    unsigned long long bits = 0ull;
    int ch;
    while ((ch = fgetc(fp)) != EOF) {
        if (ch == '_' || isspace((unsigned char)ch)) {
            continue;
        }
        if ((ch >= '0' && ch <= '9') ||
            (ch >= 'a' && ch <= 'f') ||
            (ch >= 'A' && ch <= 'F')) {
            bits += 4ull;
        }
    }
    return bits;
}

/* Count logical bits in a binary-text MEM file: '0' and '1' are treated as
 * individual bits, underscores and whitespace (including newlines) are
 * ignored; any other characters are skipped.
 */
static unsigned long long sem_mem_count_bits_mem_file(FILE *fp)
{
    unsigned long long bits = 0ull;
    int ch;
    while ((ch = fgetc(fp)) != EOF) {
        if (ch == '_' || isspace((unsigned char)ch)) {
            continue;
        }
        if (ch == '0' || ch == '1') {
            bits += 1ull;
        }
    }
    return bits;
}

/* Validate a file-based MEM initializer against the declared word width and
 * depth, emitting MEM_INIT_FILE_NOT_FOUND, MEM_INIT_FILE_TOO_LARGE, or
 * MEM_WARN_PARTIAL_INIT as appropriate.
 *
 * File semantics are selected based on the (case-insensitive) file
 * extension of the @file payload:
 *   - .hex : Base-16 text (0-9, A-F, _ and whitespace ignored);
 *            each hex digit contributes 4 bits.
 *   - .mem : Base-2 text (0/1, _ and whitespace ignored);
 *            each binary digit contributes 1 bit.
 *   - .bin or anything else : raw binary; file size in bytes is used.
 */
static void sem_check_mem_file_init(JZASTNode *mem,
                                    JZASTNode *init_expr,
                                    unsigned word_w,
                                    unsigned depth,
                                    int have_word_w,
                                    int have_depth,
                                    JZDiagnosticList *diagnostics)
{
    if (!mem || !init_expr || !init_expr->text || !diagnostics) return;

    /* Validate the @file() path against security policy. */
    char base_dir[512];
    base_dir[0] = '\0';
    if (mem->loc.filename) {
        const char *slash = strrchr(mem->loc.filename, '/');
        if (slash) {
            size_t dir_len = (size_t)(slash - mem->loc.filename);
            if (dir_len >= sizeof(base_dir)) dir_len = sizeof(base_dir) - 1;
            memcpy(base_dir, mem->loc.filename, dir_len);
            base_dir[dir_len] = '\0';
        }
    }

    char *validated = jz_path_validate(init_expr->text,
                                        base_dir[0] ? base_dir : NULL,
                                        init_expr->loc,
                                        diagnostics);
    char fullpath[512];
    if (validated) {
        snprintf(fullpath, sizeof(fullpath), "%s", validated);
        free(validated);
    } else {
        /* Path validation failed; diagnostic already emitted. */
        return;
    }

    FILE *fp = fopen(fullpath, "rb");
    if (!fp) {
        char msg[600];
        snprintf(msg, sizeof(msg),
                 "MEM init file not found or not readable: %s", fullpath);
        sem_report_rule(diagnostics,
                        init_expr->loc,
                        "MEM_INIT_FILE_NOT_FOUND",
                        msg);
        return;
    }

    if (!have_word_w || !have_depth || word_w == 0 || depth == 0) {
        /* We validated existence but cannot reason about size without
         * simple literal width/depth information.
         */
        fclose(fp);
        return;
    }

    const char *ext = sem_mem_get_file_ext(init_expr->text);
    unsigned long long file_bits = 0ull;

    if (ext && sem_mem_ext_equals(ext, "hex")) {
        /* Textual hex: count hex digits as 4 bits each. */
        file_bits = sem_mem_count_bits_hex_file(fp);
        fclose(fp);
    } else if (ext && sem_mem_ext_equals(ext, "mem")) {
        /* Textual binary: count '0'/'1' digits as 1 bit each. */
        file_bits = sem_mem_count_bits_mem_file(fp);
        fclose(fp);
    } else {
        /* Default/binary: capacity is based on raw byte size. */
        if (fseek(fp, 0, SEEK_END) != 0) {
            fclose(fp);
            return;
        }
        long sz = ftell(fp);
        fclose(fp);
        if (sz < 0) {
            return;
        }
        file_bits = (unsigned long long)sz * 8ull;
    }

    unsigned long long capacity_bits =
        (unsigned long long)word_w * (unsigned long long)depth;
    if (capacity_bits == 0) {
        return;
    }

    if (file_bits > capacity_bits) {
        sem_report_rule(diagnostics,
                        init_expr->loc,
                        "MEM_INIT_FILE_TOO_LARGE",
                        "MEM init file larger than MEM depth * word_width");
    } else if (file_bits < capacity_bits) {
        char msg[512];
        unsigned long long file_words = word_w > 0 ? file_bits / word_w : 0;
        snprintf(msg, sizeof(msg),
                 "MEM init file has %llu words but MEM depth is %u; "
                 "remaining words zero-filled",
                 file_words, depth);
        sem_report_rule(diagnostics,
                        init_expr->loc,
                        "MEM_WARN_PARTIAL_INIT",
                        msg);
    }
}

/* Given a qualified name "mem.port" or "mem.port.<field>", locate the
 * corresponding MEM_DECL and MEM_PORT nodes in the module scope. Returns 1 on
 * success, 0 otherwise. When present, <field> must be "addr" or "data".
 */
static int sem_lookup_mem_port_qualified(const char *qualified,
                                         const JZModuleScope *mod_scope,
                                         JZMemPortRef *out)
{
    if (!qualified || !mod_scope) return 0;
    const char *dot = strchr(qualified, '.');
    if (!dot || !*(dot + 1)) {
        return 0;
    }

    char mem_name[256];
    size_t mem_len = (size_t)(dot - qualified);
    if (mem_len == 0 || mem_len >= sizeof(mem_name)) {
        return 0;
    }
    memcpy(mem_name, qualified, mem_len);
    mem_name[mem_len] = '\0';

    const char *port_str = dot + 1;
    const char *field_str = NULL;
    const char *second_dot = strchr(port_str, '.');
    if (second_dot) {
        if (!*(second_dot + 1)) {
            return 0;
        }
        field_str = second_dot + 1;
    }

    const JZSymbol *mem_sym = module_scope_lookup_kind(mod_scope, mem_name, JZ_SYM_MEM);
    if (!mem_sym || !mem_sym->node) {
        return 0;
    }

    JZASTNode *mem_decl = mem_sym->node;
    JZASTNode *found_port = NULL;
    for (size_t i = 0; i < mem_decl->child_count; ++i) {
        JZASTNode *child = mem_decl->children[i];
        if (!child || child->type != JZ_AST_MEM_PORT || !child->name) {
            continue;
        }
        size_t port_len = second_dot ? (size_t)(second_dot - port_str) : strlen(port_str);
        if (port_len > 0 && strlen(child->name) == port_len &&
            strncmp(child->name, port_str, port_len) == 0) {
            found_port = child;
            break;
        }
    }

    if (!found_port) {
        return 0;
    }

    if (out) {
        out->mem_decl = mem_decl;
        out->port = found_port;
        out->field = MEM_PORT_FIELD_NONE;
        if (field_str) {
            if (strcmp(field_str, "addr") == 0) {
                out->field = MEM_PORT_FIELD_ADDR;
            } else if (strcmp(field_str, "data") == 0) {
                out->field = MEM_PORT_FIELD_DATA;
            } else if (strcmp(field_str, "wdata") == 0) {
                out->field = MEM_PORT_FIELD_WDATA;
            } else {
                return 0;
            }
        }
    }
    return 1;
}

/* Match expressions of the form `mem.port` or `mem.port.<field>` represented as
 * an identifier or qualified-identifier node. On success, fills out and
 * returns 1.
 */
int sem_match_mem_port_qualified_ident(JZASTNode *expr,
                                       const JZModuleScope *mod_scope,
                                       JZDiagnosticList *diagnostics,
                                       JZMemPortRef *out)
{
    (void)diagnostics;
    if (!expr || !expr->name ||
        (expr->type != JZ_AST_EXPR_IDENTIFIER &&
         expr->type != JZ_AST_EXPR_QUALIFIED_IDENTIFIER)) {
        return 0;
    }

    const char *qualified = expr->name;
    const char *dot = strchr(qualified, '.');
    if (!dot || dot == qualified) {
        return 0;
    }

    char mem_name[256];
    size_t mem_len = (size_t)(dot - qualified);
    if (mem_len == 0 || mem_len >= sizeof(mem_name)) {
        return 0;
    }
    memcpy(mem_name, qualified, mem_len);
    mem_name[mem_len] = '\0';

    const JZSymbol *mem_sym = module_scope_lookup_kind(mod_scope, mem_name, JZ_SYM_MEM);
    if (!mem_sym) {
        return 0;
    }

    return sem_lookup_mem_port_qualified(qualified, mod_scope, out);
}

/* Match expressions of the form `mem.port[addr]` represented as a SLICE node.
 * On success, fills out->mem_decl/out->port and returns 1.
 */
int sem_match_mem_port_slice(JZASTNode *slice,
                             const JZModuleScope *mod_scope,
                             JZDiagnosticList *diagnostics,
                             JZMemPortRef *out)
{
    if (!slice || slice->type != JZ_AST_EXPR_SLICE || slice->child_count < 3) {
        return 0;
    }
    JZASTNode *base = slice->children[0];
    if (!base || !base->name ||
        (base->type != JZ_AST_EXPR_IDENTIFIER &&
         base->type != JZ_AST_EXPR_QUALIFIED_IDENTIFIER)) {
        return 0;
    }

    /* Base name is of the form "mem_name.port_name" for MEM accesses. If the
     * MEM name itself is not declared in the module, classify this via
     * MEM_UNDEFINED_NAME before attempting finer-grained MEM_ACCESS checks.
     */
    const char *qualified = base->name;
    const char *dot = strchr(qualified, '.');
    if (!dot || dot == qualified) {
        return 0;
    }

    char mem_name[256];
    size_t mem_len = (size_t)(dot - qualified);
    if (mem_len == 0 || mem_len >= sizeof(mem_name)) {
        return 0;
    }
    memcpy(mem_name, qualified, mem_len);
    mem_name[mem_len] = '\0';

    const JZSymbol *mem_sym = module_scope_lookup_kind(mod_scope, mem_name, JZ_SYM_MEM);
    if (!mem_sym) {
        /* If the name refers to a BUS port rather than a MEM, this is not a
         * MEM access at all — return 0 silently so that the caller can handle
         * it via the normal BUS field resolution path.
         */
        const JZSymbol *port_sym = module_scope_lookup_kind(mod_scope, mem_name, JZ_SYM_PORT);
        if (port_sym && port_sym->node && port_sym->node->block_kind &&
            strcmp(port_sym->node->block_kind, "BUS") == 0) {
            return 0;
        }
        if (diagnostics) {
            sem_report_rule(diagnostics,
                            slice->loc,
                            "MEM_UNDEFINED_NAME",
                            "access to MEM name not declared in module");
        }
        return 0;
    }

    if (!sem_lookup_mem_port_qualified(qualified, mod_scope, out)) {
        return 0;
    }
    if (out && out->field != MEM_PORT_FIELD_NONE) {
        return 0;
    }
    return 1;
}

/* Compute depth and address width (ceil(log2(depth))) for a MEM_DECL whose
 * depth expression is a simple positive integer literal. Returns 1 when both
 * depth and addr_width are known, 0 otherwise.
 */
static int sem_mem_compute_depth_and_addr_width(JZASTNode *mem_decl,
                                                unsigned *out_depth,
                                                unsigned *out_addr_width)
{
    if (!mem_decl) return 0;
    const char *depth_text = mem_decl->text;
    unsigned depth = 0;
    int rc = eval_simple_positive_decl_int(depth_text, &depth);
    if (rc != 1) {
        /* rc == 0: complex/unknown; rc == -1: invalid already reported by
         * declaration-phase checks.
         */
        return 0;
    }

    unsigned addr_width = 0;
    if (depth > 1) {
        unsigned v = depth - 1u;
        while (v) {
            addr_width++;
            v >>= 1;
        }
    }
    /* Minimum address width is 1 bit — 0-width vectors are not permitted. */
    if (addr_width == 0) addr_width = 1;

    if (out_depth) *out_depth = depth;
    if (out_addr_width) *out_addr_width = addr_width;
    return 1;
}

/* Expression-level MEM access checks: address width and constant range. */
void sem_check_mem_access_expr(JZASTNode *expr,
                               const JZModuleScope *mod_scope,
                               const JZBuffer *project_symbols,
                               JZDiagnosticList *diagnostics)
{
    if (!expr || !mod_scope) return;
    if (expr->type != JZ_AST_EXPR_SLICE || expr->child_count < 3) return;

    JZMemPortRef ref;
    if (!sem_match_mem_port_slice(expr, mod_scope, diagnostics, &ref)) {
        return;
    }
    if (ref.port && ref.port->block_kind &&
        strcmp(ref.port->block_kind, "OUT") == 0 &&
        ref.port->text && strcmp(ref.port->text, "SYNC") == 0) {
        sem_report_rule(diagnostics,
                        expr->loc,
                        "MEM_SYNC_PORT_INDEXED",
                        "SYNC MEM read ports may not be indexed; use .addr/.data");
        return;
    }

    unsigned depth = 0, addr_width = 0;
    if (!sem_mem_compute_depth_and_addr_width(ref.mem_decl, &depth, &addr_width)) {
        return; /* unknown depth; cannot enforce width/range rules yet */
    }

    /* Index expression is the first index node (msb). For [idx] the parser
     * creates msb/lsb duplicates, so inspecting msb is sufficient.
     */
    JZASTNode *msb_node = expr->children[1];
    if (!msb_node) return;

    /* Constant-address out-of-range check when index is a plain integer
     * literal. This is independent of the address *width* rule below.
     */
    if (msb_node->type == JZ_AST_EXPR_LITERAL && msb_node->text && depth > 0) {
        unsigned idx = 0;
        if (parse_simple_nonnegative_int(msb_node->text, &idx) && idx >= depth) {
            sem_report_rule(diagnostics,
                            expr->loc,
                            "MEM_CONST_ADDR_OUT_OF_RANGE",
                            "constant memory address index is out of range for declared depth");
        }
    }

    /* Bit-width based address rule: index width must not exceed
     * ceil(log2(depth)). We reuse infer_expr_type on the index expression.
     *
     * For purely constant indices we already enforce MEM_CONST_ADDR_OUT_OF_RANGE
     * and skip MEM_ADDR_WIDTH_TOO_WIDE to avoid redundant diagnostics.
     */
    if (addr_width > 0 &&
        !(msb_node->type == JZ_AST_EXPR_LITERAL && msb_node->text)) {
        JZBitvecType idx_type;
        idx_type.width = 0;
        idx_type.is_signed = 0;
        infer_expr_type(msb_node, mod_scope, project_symbols, diagnostics, &idx_type);
        if (idx_type.width > addr_width) {
            sem_report_rule(diagnostics,
                            expr->loc,
                            "MEM_ADDR_WIDTH_TOO_WIDE",
                            "memory address expression width exceeds ceil(log2(depth))");
        }
    }
}

void sem_check_mem_addr_assign(const JZMemPortRef *ref,
                               JZASTNode *addr_expr,
                               const JZModuleScope *mod_scope,
                               const JZBuffer *project_symbols,
                               JZDiagnosticList *diagnostics)
{
    if (!ref || !ref->mem_decl || !addr_expr || !mod_scope) return;

    unsigned depth = 0, addr_width = 0;
    if (!sem_mem_compute_depth_and_addr_width(ref->mem_decl, &depth, &addr_width)) {
        return;
    }

    if (addr_expr->type == JZ_AST_EXPR_LITERAL && addr_expr->text && depth > 0) {
        unsigned idx = 0;
        if (parse_simple_nonnegative_int(addr_expr->text, &idx) && idx >= depth) {
            sem_report_rule(diagnostics,
                            addr_expr->loc,
                            "MEM_CONST_ADDR_OUT_OF_RANGE",
                            "constant memory address index is out of range for declared depth");
        }
    }

    if (addr_width > 0 &&
        !(addr_expr->type == JZ_AST_EXPR_LITERAL && addr_expr->text)) {
        JZBitvecType idx_type;
        idx_type.width = 0;
        idx_type.is_signed = 0;
        infer_expr_type(addr_expr, mod_scope, project_symbols, diagnostics, &idx_type);
        if (idx_type.width > addr_width) {
            sem_report_rule(diagnostics,
                            addr_expr->loc,
                            "MEM_ADDR_WIDTH_TOO_WIDE",
                            "memory address expression width exceeds ceil(log2(depth))");
        }
    }
}

/* Track writes to MEM OUT ports within a single SYNCHRONOUS block so that we
 * can enforce MEM_MULTIPLE_WRITES_SAME_IN.
 */
void sem_track_mem_out_write(JZBuffer *writes,
                             const JZMemPortRef *ref,
                             JZDiagnosticList *diagnostics,
                             JZLocation loc)
{
    if (!writes || !ref || !ref->mem_decl || !ref->port) return;

    size_t count = writes->len / sizeof(JZMemWriteKey);
    JZMemWriteKey *arr = (JZMemWriteKey *)writes->data;
    for (size_t i = 0; i < count; ++i) {
        if (arr[i].mem_decl == ref->mem_decl && arr[i].port == ref->port) {
            sem_report_rule(diagnostics,
                            loc,
                            "MEM_MULTIPLE_WRITES_SAME_IN",
                            "multiple writes to same MEM OUT port within single SYNCHRONOUS block");
            return;
        }
    }

    JZMemWriteKey key;
    key.mem_decl = ref->mem_decl;
    key.port = ref->port;
    (void)jz_buf_append(writes, &key, sizeof(key));
}

/* Helper to recognize MEM(TYPE=...) in the MEM block header attributes,
 * allowing arbitrary whitespace around '=' and case variations for TYPE/values.
 */
/* Return 1 if the attribute string contains a TYPE= key, 0 otherwise.
 * This lets callers distinguish "no TYPE specified" from "TYPE specified but
 * value unrecognized".
 */
static int sem_mem_header_has_type_key(const char *attrs)
{
    if (!attrs) return 0;
    const char *p = strstr(attrs, "TYPE");
    if (!p) p = strstr(attrs, "type");
    if (!p) return 0;
    p += 4;
    while (*p && isspace((unsigned char)*p)) p++;
    return (*p == '=');
}

static JZChipMemType sem_mem_header_parse_type(const char *attrs)
{
    if (!attrs) return JZ_CHIP_MEM_UNKNOWN;
    const char *p = strstr(attrs, "TYPE");
    if (!p) p = strstr(attrs, "type");
    if (!p) return JZ_CHIP_MEM_UNKNOWN;
    p += 4; /* skip TYPE */
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '=') return JZ_CHIP_MEM_UNKNOWN;
    p++; /* skip '=' */
    while (*p && isspace((unsigned char)*p)) p++;
    if (strncmp(p, "BLOCK", 5) == 0 || strncmp(p, "block", 5) == 0) {
        return JZ_CHIP_MEM_BLOCK;
    }
    if (strncmp(p, "DISTRIBUTED", 11) == 0 || strncmp(p, "distributed", 11) == 0) {
        return JZ_CHIP_MEM_DISTRIBUTED;
    }
    return JZ_CHIP_MEM_UNKNOWN;
}

void sem_check_module_mem_and_mux_decls(const JZModuleScope *scope,
                                        const JZBuffer *project_symbols,
                                        JZDiagnosticList *diagnostics)
{
    if (!scope || !scope->node) return;
    JZASTNode *mod = scope->node;

    for (size_t i = 0; i < mod->child_count; ++i) {
        JZASTNode *child = mod->children[i];
        if (!child) continue;

        if (child->type == JZ_AST_MEM_BLOCK) {
            /* Per-MEM validation: widths/depths, port lists, and TYPE=BLOCK
             * restrictions.
             */
            const char *attrs = child->text;
            JZChipMemType parsed_type = sem_mem_header_parse_type(attrs);
            int type_is_block = (parsed_type == JZ_CHIP_MEM_BLOCK);

            /* MEM_TYPE_INVALID: TYPE= key is present but value is not
             * one of BLOCK or DISTRIBUTED.
             */
            if (sem_mem_header_has_type_key(attrs) &&
                parsed_type == JZ_CHIP_MEM_UNKNOWN) {
                sem_report_rule(diagnostics,
                                child->loc,
                                "MEM_TYPE_INVALID",
                                "MEM TYPE value is not recognized; expected BLOCK or DISTRIBUTED");
            }

            for (size_t j = 0; j < child->child_count; ++j) {
                JZASTNode *mem = child->children[j];
                if (!mem || mem->type != JZ_AST_MEM_DECL) continue;

                /* MEM_INVALID_WORD_WIDTH / MEM_INVALID_DEPTH (simple decimal
                 * forms only; CONST/CONFIG-based expressions are deferred to
                 * future constant-eval integration).
                 */
                unsigned word_w = 0;
                int width_has_lit = (mem->width && sem_expr_has_lit_call(mem->width));
                if (width_has_lit) {
                    sem_report_rule(diagnostics,
                                    mem->loc,
                                    "LIT_INVALID_CONTEXT",
                                    "lit() may not be used in MEM width/depth declarations");
                }
                int w_rc = width_has_lit ? 0 : eval_simple_positive_decl_int(mem->width, &word_w);
                if (w_rc == -1) {
                    sem_report_rule(diagnostics,
                                    mem->loc,
                                    "MEM_INVALID_WORD_WIDTH",
                                    "MEM word width must be a positive integer");
                }

                if (w_rc == 0 && mem->width && !width_has_lit) {
                    /* Try to evaluate as a CONST expression (S7.10 allows
                     * CONST expressions in MEM widths).
                     */
                    long long wval = 0;
                    if (sem_eval_const_expr_in_module(mem->width, scope,
                                                      project_symbols, &wval) != 0) {
                        sem_report_rule(diagnostics,
                                        mem->loc,
                                        "MEM_UNDEFINED_CONST_IN_WIDTH",
                                        "MEM word width/depth uses undefined CONST name");
                    } else if (wval <= 0) {
                        sem_report_rule(diagnostics,
                                        mem->loc,
                                        "MEM_INVALID_WORD_WIDTH",
                                        "MEM word width must be a positive integer");
                    }
                }

                unsigned depth = 0;
                int depth_has_lit = (mem->text && sem_expr_has_lit_call(mem->text));
                if (depth_has_lit) {
                    sem_report_rule(diagnostics,
                                    mem->loc,
                                    "LIT_INVALID_CONTEXT",
                                    "lit() may not be used in MEM width/depth declarations");
                }
                int d_rc = depth_has_lit ? 0 : eval_simple_positive_decl_int(mem->text, &depth);
                if (d_rc == -1) {
                    sem_report_rule(diagnostics,
                                    mem->loc,
                                    "MEM_INVALID_DEPTH",
                                    "MEM depth must be a positive integer");
                }

                if (d_rc == 0 && mem->text && !depth_has_lit) {
                    /* Try to evaluate as a CONST expression (S7.10 allows
                     * CONST expressions in MEM depths).
                     */
                    long long dval = 0;
                    if (sem_eval_const_expr_in_module(mem->text, scope,
                                                      project_symbols, &dval) != 0) {
                        sem_report_rule(diagnostics,
                                        mem->loc,
                                        "MEM_UNDEFINED_CONST_IN_WIDTH",
                                        "MEM word width/depth uses undefined CONST name");
                    } else if (dval <= 0) {
                        sem_report_rule(diagnostics,
                                        mem->loc,
                                        "MEM_INVALID_DEPTH",
                                        "MEM depth must be a positive integer");
                    }
                }

                /* Validate initialization form: literal/constant expression vs
                 * @file("...") payload, plus file-size checks when widths and
                 * depths are simple positive integers.
                 */
                JZASTNode *init_expr = NULL;
                if (mem->child_count > 0) {
                    JZASTNode *first = mem->children[0];
                    if (first && first->type != JZ_AST_MEM_PORT) {
                        init_expr = first;
                    }
                }

                if (!init_expr) {
                    sem_report_rule(diagnostics,
                                    mem->loc,
                                    "MEM_MISSING_INIT",
                                    "MEM declared without required initialization literal or @file(...) payload");
                } else {
                    int have_word_w = (w_rc == 1 && word_w > 0);
                    int have_depth  = (d_rc == 1 && depth > 0);

                    if (init_expr->block_kind &&
                        strcmp(init_expr->block_kind, "FILE_REF") == 0 &&
                        init_expr->name) {
                        /* @file(CONST_REF) or @file(CONFIG.NAME) form:
                         * resolve the string reference and validate. */
                        const char *resolved = NULL;
                        if (sem_resolve_string_const(init_expr->name,
                                                     scope,
                                                     project_symbols,
                                                     &resolved,
                                                     diagnostics,
                                                     init_expr->loc)) {
                            jz_ast_set_text(init_expr, resolved);
                            sem_check_mem_file_init(mem,
                                                    init_expr,
                                                    word_w,
                                                    depth,
                                                    have_word_w,
                                                    have_depth,
                                                    diagnostics);
                        }
                        /* If resolution failed, diagnostic was already emitted. */
                    } else if (init_expr->type == JZ_AST_EXPR_LITERAL &&
                        init_expr->text &&
                        sem_mem_init_looks_like_file_path(init_expr->text)) {
                        /* @file("...") form: validate existence and size. */
                        sem_check_mem_file_init(mem,
                                                init_expr,
                                                word_w,
                                                depth,
                                                have_word_w,
                                                have_depth,
                                                diagnostics);
                    } else if (have_word_w) {
                        /* Literal or general expression initializer: infer
                         * bit-width and ensure it fits within the word width.
                         */
                        JZBitvecType init_ty;
                        init_ty.width = 0;
                        init_ty.is_signed = 0;
                        infer_expr_type(init_expr,
                                        scope,
                                        project_symbols,
                                        diagnostics,
                                        &init_ty);
                        if (init_ty.width > word_w) {
                            sem_report_rule(diagnostics,
                                            init_expr->loc,
                                            "MEM_INIT_LITERAL_OVERFLOW",
                                            "MEM initializer expression width exceeds declared word width");
                        }

                        /* MEM_INIT_CONTAINS_X: forbid x bits in literal-based
                         * initialization of MEM words. File-based initialization
                         * (handled above) is checked separately.
                         */
                        if (init_expr->type == JZ_AST_EXPR_LITERAL &&
                            init_expr->text &&
                            sem_literal_has_x_bits(init_expr->text)) {
                            sem_report_rule(diagnostics,
                                            init_expr->loc,
                                            "MEM_INIT_CONTAINS_X",
                                            "memory initialization literal must not contain x bits");
                        }
                    }
                }

                /* Port list checks: at least one port, unique names within the
                 * MEM, and no conflicts with module-level identifiers.
                 */
                const size_t MAX_PORTS = 64;
                const char *names[MAX_PORTS];
                size_t name_count = 0;
                unsigned port_count = 0;

                for (size_t k = 0; k < mem->child_count; ++k) {
                    JZASTNode *port = mem->children[k];
                    if (!port || port->type != JZ_AST_MEM_PORT || !port->name) {
                        continue;
                    }
                    port_count++;

                    /* MEM_DUP_PORT_NAME within this MEM. */
                    int dup = 0;
                    for (size_t n = 0; n < name_count; ++n) {
                        if (names[n] && strcmp(names[n], port->name) == 0) {
                            sem_report_rule(diagnostics,
                                            port->loc,
                                            "MEM_DUP_PORT_NAME",
                                            "duplicate MEM port name inside MEM block");
                            dup = 1;
                            break;
                        }
                    }
                    if (!dup && name_count < MAX_PORTS) {
                        names[name_count++] = port->name;
                    }

                    /* MEM_PORT_NAME_CONFLICT_MODULE_ID: port name clashes with
                     * any module-level identifier (port/wire/register/CONST/etc.).
                     */
                    const JZSymbol *existing = module_scope_lookup(scope, port->name);
                    if (existing) {
                        sem_report_rule(diagnostics,
                                        port->loc,
                                        "MEM_PORT_NAME_CONFLICT_MODULE_ID",
                                        "MEM port name conflicts with module-level identifier");
                    }

                    /* MEM_TYPE_BLOCK_WITH_ASYNC_OUT: in TYPE=BLOCK memories,
                     * read ports (OUT) must be synchronous. Forbid ASYNC OUT.
                     */
                    if (type_is_block &&
                        port->block_kind && strcmp(port->block_kind, "OUT") == 0 &&
                        port->text && strcmp(port->text, "ASYNC") == 0) {
                        sem_report_rule(diagnostics,
                                        port->loc,
                                        "MEM_TYPE_BLOCK_WITH_ASYNC_OUT",
                                        "MEM(TYPE=BLOCK) cannot have OUT port declared ASYNC; OUT ports must be SYNC");
                    }

                    /* MEM_INVALID_WRITE_MODE: WRITE_MODE must be one of the
                     * supported values when present on a write (IN or INOUT) port.
                     */
                    if (port->block_kind &&
                        (strcmp(port->block_kind, "IN") == 0 ||
                         strcmp(port->block_kind, "INOUT") == 0) &&
                        port->text) {
                        const char *wm = port->text;
                        if (strcmp(wm, "WRITE_FIRST") != 0 &&
                            strcmp(wm, "READ_FIRST") != 0 &&
                            strcmp(wm, "NO_CHANGE") != 0) {
                            sem_report_rule(diagnostics,
                                            port->loc,
                                            "MEM_INVALID_WRITE_MODE",
                                            "MEM IN/INOUT port WRITE_MODE must be WRITE_FIRST, READ_FIRST, or NO_CHANGE");
                        }
                    }

                    /* MEM_INVALID_PORT_TYPE: reject invalid combinations of
                     * direction and qualifier on MEM ports.
                     */
                    const char *dir = port->block_kind;
                    const char *qual = port->text ? port->text : "";
                    if (!dir || (strcmp(dir, "IN") != 0 && strcmp(dir, "OUT") != 0 &&
                                 strcmp(dir, "INOUT") != 0)) {
                        sem_report_rule(diagnostics,
                                        port->loc,
                                        "MEM_INVALID_PORT_TYPE",
                                        "invalid MEM port direction; expected IN, OUT, or INOUT");
                    } else if (strcmp(dir, "IN") == 0) {
                        /* IN ports carry optional WRITE_MODE; qualifier is
                         * validated separately by MEM_INVALID_WRITE_MODE.
                         */
                    } else if (strcmp(dir, "OUT") == 0) {
                        /* OUT ports may be ASYNC or SYNC, or omit a qualifier
                         * (tool may default). Any other qualifier is invalid.
                         */
                        if (qual[0] &&
                            strcmp(qual, "ASYNC") != 0 &&
                            strcmp(qual, "SYNC") != 0) {
                            sem_report_rule(diagnostics,
                                            port->loc,
                                            "MEM_INVALID_PORT_TYPE",
                                            "invalid MEM OUT port qualifier; expected SYNC or ASYNC");
                        }
                    } else if (strcmp(dir, "INOUT") == 0) {
                        /* INOUT ports carry optional WRITE_MODE; qualifier is
                         * validated separately by MEM_INVALID_WRITE_MODE.
                         * ASYNC/SYNC keywords are already caught by parser.
                         */
                    }
                }

                /* MEM_INOUT_MIXED_WITH_IN_OUT: check for illegal mixing */
                int has_in_out = 0, has_inout = 0;
                for (size_t k2 = 0; k2 < mem->child_count; ++k2) {
                    JZASTNode *p2 = mem->children[k2];
                    if (!p2 || p2->type != JZ_AST_MEM_PORT || !p2->block_kind) continue;
                    if (strcmp(p2->block_kind, "INOUT") == 0) has_inout = 1;
                    else if (strcmp(p2->block_kind, "IN") == 0 ||
                             strcmp(p2->block_kind, "OUT") == 0) has_in_out = 1;
                }
                if (has_inout && has_in_out) {
                    sem_report_rule(diagnostics,
                                    mem->loc,
                                    "MEM_INOUT_MIXED_WITH_IN_OUT",
                                    "INOUT ports cannot be mixed with IN/OUT ports in the same MEM");
                }

                if (port_count == 0) {
                    sem_report_rule(diagnostics,
                                    mem->loc,
                                    "MEM_EMPTY_PORT_LIST",
                                    "MEM declared with no IN, OUT, or INOUT ports");
                }
            }
        } else if (child->type == JZ_AST_MUX_BLOCK) {
            /* Per-MUX validation for each declaration inside the MUX block. */
            for (size_t j = 0; j < child->child_count; ++j) {
                JZASTNode *mux = child->children[j];
                if (!mux || mux->type != JZ_AST_MUX_DECL) continue;

                if (mux->block_kind && strcmp(mux->block_kind, "AGGREGATE") == 0) {
                    sem_check_mux_aggregate_decl(mux, scope, diagnostics);
                } else if (mux->block_kind && strcmp(mux->block_kind, "SLICE") == 0) {
                    sem_check_mux_slice_decl(mux, scope, diagnostics);
                }
            }
        }
    }
}

/* -------------------------------------------------------------------------
 *  MUX declaration helpers used alongside MEM declarations
 * -------------------------------------------------------------------------
 */

/* Collapse whitespace from a substring and return a newly allocated identifier-
 * like string (used for MUX source lists). Returns NULL for empty results.
 */
static char *sem_normalize_name_segment(const char *start, size_t len)
{
    if (!start || len == 0) return NULL;
    size_t out_len = 0;
    for (size_t i = 0; i < len; ++i) {
        if (!isspace((unsigned char)start[i])) {
            out_len++;
        }
    }
    if (out_len == 0) return NULL;

    char *buf = (char *)malloc(out_len + 1);
    if (!buf) return NULL;
    size_t pos = 0;
    for (size_t i = 0; i < len; ++i) {
        if (!isspace((unsigned char)start[i])) {
            buf[pos++] = start[i];
        }
    }
    buf[pos] = '\0';
    return buf;
}

/* Resolve a simple MUX source name (bare identifier) to a module-level symbol
 * and, when possible, a concrete bit-width from the declaration's width
 * expression (only simple decimal widths are handled here).
 */
static int sem_mux_resolve_simple_source(const char *name,
                                         const JZModuleScope *scope,
                                         unsigned *out_width)
{
    if (!name || !scope) return 0;
    const JZSymbol *sym = module_scope_lookup(scope, name);
    if (!sym || !sym->node) return 0;

    /* Only ports, wires, and registers are considered valid aggregation
     * sources for now.
     */
    if (sym->kind != JZ_SYM_PORT &&
        sym->kind != JZ_SYM_WIRE &&
        sym->kind != JZ_SYM_REGISTER) {
        return 0;
    }

    if (out_width) {
        const char *wtext = sym->node->width;
        unsigned w = 0;
        int rc = eval_simple_positive_decl_int(wtext, &w);
        if (rc == 1) {
            *out_width = w;
        } else {
            *out_width = 0; /* unknown/complex width */
        }
    }
    return 1;
}

static void sem_check_mux_aggregate_decl(JZASTNode *mux_decl,
                                         const JZModuleScope *scope,
                                         JZDiagnosticList *diagnostics)
{
    if (!mux_decl || mux_decl->child_count == 0) return;
    JZASTNode *rhs = mux_decl->children[0];
    if (!rhs || rhs->type != JZ_AST_RAW_TEXT || !rhs->text) return;

    const char *text = rhs->text;
    const char *p = text;
    int reported_invalid = 0;
    int reported_width_mismatch = 0;
    unsigned ref_width = 0;
    int have_ref_width = 0;

    while (*p) {
        /* Find next comma-separated segment. */
        const char *seg_start = p;
        const char *comma = strchr(p, ',');
        size_t seg_len = 0;
        if (comma) {
            seg_len = (size_t)(comma - seg_start);
            p = comma + 1;
        } else {
            seg_len = strlen(seg_start);
            p = seg_start + seg_len;
        }

        char *name = sem_normalize_name_segment(seg_start, seg_len);
        if (!name) {
            continue; /* empty or all-whitespace segment */
        }

        unsigned w = 0;
        if (!sem_mux_resolve_simple_source(name, scope, &w)) {
            if (!reported_invalid) {
                sem_report_rule(diagnostics,
                                mux_decl->loc,
                                "MUX_AGG_SOURCE_INVALID",
                                "MUX aggregation source is not a valid readable signal in module scope");
                reported_invalid = 1;
            }
            free(name);
            continue;
        }

        if (w > 0) {
            if (!have_ref_width) {
                ref_width = w;
                have_ref_width = 1;
            } else if (w != ref_width && !reported_width_mismatch) {
                sem_report_rule(diagnostics,
                                mux_decl->loc,
                                "MUX_AGG_SOURCE_WIDTH_MISMATCH",
                                "MUX aggregation sources must all have identical bit-width");
                reported_width_mismatch = 1;
            }
        }

        free(name);
    }
}

static void sem_check_mux_slice_decl(JZASTNode *mux_decl,
                                     const JZModuleScope *scope,
                                     JZDiagnosticList *diagnostics)
{
    if (!mux_decl || mux_decl->child_count == 0) return;
    JZASTNode *rhs = mux_decl->children[0];
    if (!rhs || rhs->type != JZ_AST_RAW_TEXT || !rhs->text) return;

    /* Element width from mux_decl->width. */
    unsigned elem_w = 0;
    int rc = eval_simple_positive_decl_int(mux_decl->width, &elem_w);
    if (rc != 1 || elem_w == 0u) {
        return; /* unknown or invalid handled elsewhere */
    }

    /* Wide source is a single identifier in the RHS text. */
    char *wide_name = sem_normalize_name_segment(rhs->text, strlen(rhs->text));
    if (!wide_name) return;

    unsigned wide_w = 0;
    if (!sem_mux_resolve_simple_source(wide_name, scope, &wide_w) || wide_w == 0u) {
        free(wide_name);
        return;
    }
    free(wide_name);

    if (wide_w % elem_w != 0u) {
        sem_report_rule(diagnostics,
                        mux_decl->loc,
                        "MUX_SLICE_WIDTH_NOT_DIVISOR",
                        "MUX slice element_width must divide wide source width exactly");
    }
}

/* -------------------------------------------------------------------------
 *  MEM_WARN_PORT_NEVER_ACCESSED: warn when MEM IN/OUT ports are declared but
 *  never used in any mem.port[addr] access within the module.
 * -------------------------------------------------------------------------
 */
static int sem_mem_port_vec_contains(const JZBuffer *buf, JZASTNode *port)
{
    if (!buf || !port) return 0;
    size_t count = buf->len / sizeof(JZASTNode *);
    JZASTNode **arr = (JZASTNode **)buf->data;
    for (size_t i = 0; i < count; ++i) {
        if (arr[i] == port) return 1;
    }
    return 0;
}

static void sem_collect_mem_port_uses_recursive(JZASTNode *node,
                                                const JZModuleScope *scope,
                                                JZBuffer *used_ports)
{
    if (!node || !scope || !used_ports) return;

    if (node->type == JZ_AST_EXPR_SLICE && node->child_count >= 3) {
        JZMemPortRef ref;
        memset(&ref, 0, sizeof(ref));
        if (sem_match_mem_port_slice(node, scope, NULL, &ref) && ref.port) {
            if (!sem_mem_port_vec_contains(used_ports, ref.port)) {
                (void)jz_buf_append(used_ports, &ref.port, sizeof(ref.port));
            }
        }
    }
    if (node->type == JZ_AST_EXPR_QUALIFIED_IDENTIFIER) {
        JZMemPortRef ref;
        memset(&ref, 0, sizeof(ref));
        if (sem_match_mem_port_qualified_ident(node, scope, NULL, &ref) &&
            ref.port && (ref.field == MEM_PORT_FIELD_ADDR ||
                         ref.field == MEM_PORT_FIELD_DATA ||
                         ref.field == MEM_PORT_FIELD_WDATA)) {
            if (!sem_mem_port_vec_contains(used_ports, ref.port)) {
                (void)jz_buf_append(used_ports, &ref.port, sizeof(ref.port));
            }
        }
    }

    for (size_t i = 0; i < node->child_count; ++i) {
        sem_collect_mem_port_uses_recursive(node->children[i], scope, used_ports);
    }
}

static int sem_chip_mem_config_supported(const JZChipData *chip,
                                         JZChipMemType type,
                                         unsigned r_ports,
                                         unsigned w_ports,
                                         unsigned width,
                                         unsigned depth)
{
    if (!chip || chip->mem_configs.len == 0) return 1;
    const JZChipMemConfig *cfgs = (const JZChipMemConfig *)chip->mem_configs.data;
    size_t count = chip->mem_configs.len / sizeof(JZChipMemConfig);
    for (size_t i = 0; i < count; ++i) {
        if (cfgs[i].type != type) continue;
        if (cfgs[i].r_ports < r_ports) continue;
        if (cfgs[i].w_ports < w_ports) continue;
        if (type == JZ_CHIP_MEM_BLOCK) {
            /* BLOCK memories: synthesizer auto-splits oversized memories
             * into multiple BSRAM blocks.  Only port compatibility matters. */
            return 1;
        }
        /* DISTRIBUTED: must fit in a single config. */
        if (cfgs[i].width < width) continue;
        if (cfgs[i].depth < depth) continue;
        return 1;
    }
    return 0;
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
                /* INOUT ports are always synchronous, so no change to all_sync */
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

/**
 * Compute how many BSRAM blocks a single BLOCK memory requires.
 *
 * blocks = ceil(width / cfg_w) * ceil(depth / cfg_d)
 *
 * Returns the minimum across all compatible configs, or 1 if no chip data.
 * Also returns the best config width/depth through out parameters (if non-NULL).
 */
static unsigned sem_compute_block_count(const JZChipData *chip,
                                        unsigned r_ports, unsigned w_ports,
                                        unsigned width, unsigned depth,
                                        unsigned *out_cfg_w, unsigned *out_cfg_d)
{
    if (!chip || chip->mem_configs.len == 0) {
        if (out_cfg_w) *out_cfg_w = 0;
        if (out_cfg_d) *out_cfg_d = 0;
        return 1;
    }
    const JZChipMemConfig *cfgs = (const JZChipMemConfig *)chip->mem_configs.data;
    size_t count = chip->mem_configs.len / sizeof(JZChipMemConfig);
    unsigned best = 0;
    unsigned best_w = 0, best_d = 0;
    for (size_t i = 0; i < count; ++i) {
        if (cfgs[i].type != JZ_CHIP_MEM_BLOCK) continue;
        if (cfgs[i].r_ports < r_ports) continue;
        if (cfgs[i].w_ports < w_ports) continue;
        unsigned cw = cfgs[i].width;
        unsigned cd = cfgs[i].depth;
        if (cw == 0 || cd == 0) continue;
        unsigned w_tiles = (width + cw - 1) / cw;
        unsigned d_tiles = (depth + cd - 1) / cd;
        unsigned total = w_tiles * d_tiles;
        if (best == 0 || total < best) {
            best = total;
            best_w = cw;
            best_d = cd;
        }
    }
    if (best == 0) {
        if (out_cfg_w) *out_cfg_w = 0;
        if (out_cfg_d) *out_cfg_d = 0;
        return 1;
    }
    if (out_cfg_w) *out_cfg_w = best_w;
    if (out_cfg_d) *out_cfg_d = best_d;
    return best;
}

void sem_check_module_mem_chip_configs(const JZModuleScope *scope,
                                       const JZBuffer *project_symbols,
                                       const JZChipData *chip,
                                       JZDiagnosticList *diagnostics)
{
    if (!scope || !scope->node || !chip || chip->mem_configs.len == 0) return;
    JZASTNode *mod = scope->node;

    for (size_t i = 0; i < mod->child_count; ++i) {
        JZASTNode *child = mod->children[i];
        if (!child || child->type != JZ_AST_MEM_BLOCK) continue;

        JZChipMemType header_type = sem_mem_header_parse_type(child->text);

        for (size_t j = 0; j < child->child_count; ++j) {
            JZASTNode *mem = child->children[j];
            if (!mem || mem->type != JZ_AST_MEM_DECL) continue;

            unsigned r_ports = 0;
            unsigned w_ports = 0;
            int all_sync = 1;
            sem_mem_decl_port_counts(mem, &r_ports, &w_ports, &all_sync);
            if (r_ports == 0 && w_ports == 0) {
                continue;
            }

            unsigned width = 0;
            unsigned depth = 0;
            long long depth_val = 0;
            int have_width = (sem_eval_width_expr(mem->width, scope, project_symbols, &width) == 0 && width > 0);
            int have_depth = (sem_eval_const_expr_in_module(mem->text, scope, project_symbols, &depth_val) == 0 && depth_val > 0);
            if (have_depth) {
                depth = (unsigned)depth_val;
            }

            if (!have_width || !have_depth) {
                continue;
            }

            JZChipMemType mem_type = header_type;
            if (mem_type == JZ_CHIP_MEM_UNKNOWN) {
                mem_type = sem_mem_infer_type(depth, have_depth, all_sync);
            }
            if (mem_type == JZ_CHIP_MEM_UNKNOWN) {
                continue;
            }

            if (!sem_chip_mem_config_supported(chip, mem_type, r_ports, w_ports, width, depth)) {
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "MEM [%u] [%u] with %u read, %u write port(s) does not match "
                         "any configuration for %s.\nSee --chip-info and --memory-report "
                         "for more information.",
                         width, depth, r_ports, w_ports,
                         chip->chip_id ? chip->chip_id : "selected chip");
                sem_report_rule(diagnostics,
                                mem->loc,
                                "MEM_CHIP_CONFIG_UNSUPPORTED",
                                msg);
            } else if (mem_type == JZ_CHIP_MEM_BLOCK) {
                unsigned cfg_w = 0, cfg_d = 0;
                unsigned blocks = sem_compute_block_count(chip, r_ports, w_ports,
                                                         width, depth, &cfg_w, &cfg_d);
                if (blocks > 1 && cfg_w > 0 && cfg_d > 0) {
                    unsigned w_tiles = (width + cfg_w - 1) / cfg_w;
                    unsigned d_tiles = (depth + cfg_d - 1) / cfg_d;
                    char msg[512];
                    snprintf(msg, sizeof(msg),
                             "MEM '%s' [%u] [%u] requires %u BSRAM blocks (%ux%u of [%u]x[%u])",
                             mem->name ? mem->name : "?",
                             width, depth, blocks, w_tiles, d_tiles, cfg_w, cfg_d);
                    sem_report_rule(diagnostics, mem->loc,
                                    "MEM_BLOCK_MULTI", msg);
                }
            }
        }
    }
}

/**
 * Check whether a latch directly drives a top-level output pin.
 *
 * A latch is "on a pin" when:
 *   1. The current module is the @top module.
 *   2. An ASYNC block contains `<port> <= <latch_name>` where the RHS is a
 *      bare identifier matching the latch name and the LHS is a PORT OUT.
 *   3. That port is bound in @top to a real OUT_PIN or INOUT_PIN (not `_`).
 */
static int latch_drives_output_pin(const JZModuleScope *scope,
                                   const char *latch_name,
                                   JZASTNode *project,
                                   const JZBuffer *project_symbols)
{
    if (!scope || !scope->node || !latch_name || !project || !project_symbols)
        return 0;

    JZASTNode *top_new = sem_find_project_top_new(project);
    if (!top_new || !top_new->name) return 0;

    /* The current module must be the @top module. */
    JZASTNode *mod = scope->node;
    if (!mod->name || strcmp(mod->name, top_new->name) != 0) return 0;

    /* Walk ASYNC blocks looking for `<port> <= <latch_name>`. */
    for (size_t i = 0; i < mod->child_count; ++i) {
        JZASTNode *blk = mod->children[i];
        if (!blk || blk->type != JZ_AST_BLOCK || !blk->block_kind) continue;
        if (strcmp(blk->block_kind, "ASYNCHRONOUS") != 0) continue;

        for (size_t j = 0; j < blk->child_count; ++j) {
            JZASTNode *stmt = blk->children[j];
            if (!stmt) continue;
            if (stmt->type != JZ_AST_STMT_ASSIGN) continue;
            if (stmt->child_count < 2) continue;
            /* Must be a <= (RECEIVE) assignment, not alias. */
            if (!stmt->block_kind || strcmp(stmt->block_kind, "RECEIVE") != 0)
                continue;

            /* RHS must be a bare identifier matching latch_name. */
            JZASTNode *rhs = stmt->children[1];
            if (!rhs || rhs->type != JZ_AST_EXPR_IDENTIFIER) continue;
            if (!rhs->name || strcmp(rhs->name, latch_name) != 0) continue;

            /* LHS must be a port name. */
            JZASTNode *lhs = stmt->children[0];
            if (!lhs || !lhs->name) continue;
            const char *port_name = lhs->name;

            /* Verify LHS is declared as PORT OUT in this module. */
            int is_port_out = 0;
            for (size_t pi = 0; pi < mod->child_count; ++pi) {
                JZASTNode *pb = mod->children[pi];
                if (!pb || pb->type != JZ_AST_PORT_BLOCK) continue;
                for (size_t pk = 0; pk < pb->child_count; ++pk) {
                    JZASTNode *pd = pb->children[pk];
                    if (!pd || pd->type != JZ_AST_PORT_DECL || !pd->name) continue;
                    if (strcmp(pd->name, port_name) != 0) continue;
                    if (pd->block_kind &&
                        (strcmp(pd->block_kind, "OUT") == 0 ||
                         strcmp(pd->block_kind, "INOUT") == 0)) {
                        is_port_out = 1;
                    }
                    break;
                }
                if (is_port_out) break;
            }
            if (!is_port_out) continue;

            /* Check that the port is bound in @top to a real pin. */
            for (size_t k = 0; k < top_new->child_count; ++k) {
                JZASTNode *b = top_new->children[k];
                if (!b || b->type != JZ_AST_PORT_DECL || !b->name) continue;
                if (strcmp(b->name, port_name) != 0) continue;

                /* Must be an OUT or INOUT binding. */
                if (!b->block_kind) break;
                if (strcmp(b->block_kind, "OUT") != 0 &&
                    strcmp(b->block_kind, "INOUT") != 0)
                    break;

                /* Must not be no-connect. */
                const char *target = b->text;
                if (!target || target[0] == '\0') break;
                if (target[0] == '_' && (target[1] == '\0' || target[1] == ' '))
                    break;

                /* Look up in project symbols as a real pin. */
                const JZSymbol *pin_sym = project_lookup(project_symbols,
                                                         target, JZ_SYM_PIN);
                if (pin_sym && pin_sym->node && pin_sym->node->block_kind &&
                    (strcmp(pin_sym->node->block_kind, "OUT_PINS") == 0 ||
                     strcmp(pin_sym->node->block_kind, "INOUT_PINS") == 0)) {
                    return 1;
                }
                break;
            }
        }
    }
    return 0;
}

void sem_check_module_latch_chip_support(const JZModuleScope *scope,
                                         const JZChipData *chip,
                                         JZASTNode *project,
                                         const JZBuffer *project_symbols,
                                         JZDiagnosticList *diagnostics)
{
    if (!scope || !scope->node || !chip || !chip->has_latches) return;
    JZASTNode *mod = scope->node;

    for (size_t i = 0; i < mod->child_count; ++i) {
        JZASTNode *child = mod->children[i];
        if (!child || child->type != JZ_AST_LATCH_BLOCK) continue;

        for (size_t j = 0; j < child->child_count; ++j) {
            JZASTNode *decl = child->children[j];
            if (!decl || decl->type != JZ_AST_LATCH_DECL) continue;

            const char *ltype = decl->block_kind;
            if (!ltype) continue;

            int on_pin = latch_drives_output_pin(scope, decl->name,
                                                  project, project_symbols);

            int supported = 0;
            if (on_pin) {
                /* Try IOB first, then fall back to fabric. */
                if (strcmp(ltype, "D") == 0) {
                    supported = chip->latches.iob_d || chip->latches.fab_d;
                } else if (strcmp(ltype, "SR") == 0) {
                    supported = chip->latches.iob_sr || chip->latches.fab_sr;
                }
            } else {
                /* Internal latch — fabric only. */
                if (strcmp(ltype, "D") == 0) {
                    supported = chip->latches.fab_d;
                } else if (strcmp(ltype, "SR") == 0) {
                    supported = chip->latches.fab_sr;
                }
            }

            if (!supported) {
                char msg[512];
                if (on_pin) {
                    snprintf(msg, sizeof(msg),
                             "%s latch not supported on this chip; "
                             "neither IOB nor CFU supports this latch type",
                             ltype);
                } else {
                    snprintf(msg, sizeof(msg),
                             "%s latch not supported in CFU on this chip; "
                             "CFU supports edge-triggered registers only",
                             ltype);
                }
                sem_report_rule(diagnostics, decl->loc,
                                "LATCH_CHIP_UNSUPPORTED", msg);
            }
        }
    }
}

void sem_check_module_mem_port_usage(const JZModuleScope *scope,
                                     JZDiagnosticList *diagnostics)
{
    if (!scope || !scope->node) return;
    JZASTNode *mod = scope->node;

    /* Collect all MEM ports declared in this module. */
    JZBuffer all_ports = (JZBuffer){0};
    for (size_t i = 0; i < mod->child_count; ++i) {
        JZASTNode *child = mod->children[i];
        if (!child || child->type != JZ_AST_MEM_BLOCK) continue;
        for (size_t j = 0; j < child->child_count; ++j) {
            JZASTNode *mem = child->children[j];
            if (!mem || mem->type != JZ_AST_MEM_DECL) continue;
            for (size_t k = 0; k < mem->child_count; ++k) {
                JZASTNode *port = mem->children[k];
                if (!port || port->type != JZ_AST_MEM_PORT || !port->name) continue;
                (void)jz_buf_append(&all_ports, &port, sizeof(port));
            }
        }
    }

    if (all_ports.len == 0) {
        jz_buf_free(&all_ports);
        return;
    }

    /* Traverse the module AST to find all mem.port[addr] usages. */
    JZBuffer used_ports = (JZBuffer){0};
    sem_collect_mem_port_uses_recursive(mod, scope, &used_ports);

    /* Emit MEM_WARN_PORT_NEVER_ACCESSED for ports that are never used. */
    JZASTNode **ports = (JZASTNode **)all_ports.data;
    size_t port_count = all_ports.len / sizeof(JZASTNode *);
    for (size_t i = 0; i < port_count; ++i) {
        JZASTNode *port = ports[i];
        if (!port) continue;
        if (sem_mem_port_vec_contains(&used_ports, port)) {
            continue;
        }
        /* Only warn for IN/OUT/INOUT style ports; block_kind may be NULL for
         * malformed ASTs, which we skip conservatively.
         */
        if (!port->block_kind) continue;
        if (strcmp(port->block_kind, "IN") != 0 &&
            strcmp(port->block_kind, "OUT") != 0 &&
            strcmp(port->block_kind, "INOUT") != 0) {
            continue;
        }
        sem_report_rule(diagnostics,
                        port->loc,
                        "MEM_WARN_PORT_NEVER_ACCESSED",
                        "MEM IN/OUT/INOUT port declared but never used");
    }

    jz_buf_free(&all_ports);
    jz_buf_free(&used_ports);
}

/* -------------------------------------------------------------------------
 *  Project-wide memory resource usage check
 * -------------------------------------------------------------------------
 */

typedef struct JZModuleInstanceCount {
    const char *module_name;
    unsigned    count;
} JZModuleInstanceCount;

/* Recursively accumulate instance counts starting from a given module.
 * Each entry in `counts` stores the total number of times a module is
 * instantiated across the design hierarchy.  `multiplier` is the number
 * of times the current module itself is instantiated.
 */
static void mem_res_count_instances(JZASTNode *project,
                                     const char *module_name,
                                     unsigned multiplier,
                                     const JZBuffer *module_scopes,
                                     const JZBuffer *project_symbols,
                                     JZBuffer *counts)
{
    if (!module_name || !project || !counts) return;

    /* Find the module AST node. */
    JZASTNode *mod = NULL;
    for (size_t i = 0; i < project->child_count; ++i) {
        JZASTNode *child = project->children[i];
        if (child && child->type == JZ_AST_MODULE && child->name &&
            strcmp(child->name, module_name) == 0) {
            mod = child;
            break;
        }
    }
    if (!mod) return;

    /* Add/update the count for this module. */
    size_t n = counts->len / sizeof(JZModuleInstanceCount);
    JZModuleInstanceCount *arr = (JZModuleInstanceCount *)counts->data;
    int found = 0;
    for (size_t i = 0; i < n; ++i) {
        if (strcmp(arr[i].module_name, module_name) == 0) {
            arr[i].count += multiplier;
            found = 1;
            break;
        }
    }
    if (!found) {
        JZModuleInstanceCount mc;
        mc.module_name = module_name;
        mc.count = multiplier;
        jz_buf_append(counts, &mc, sizeof(mc));
    }

    /* Find the module scope for CONST evaluation. */
    const JZModuleScope *scope = NULL;
    if (module_scopes) {
        size_t sc = module_scopes->len / sizeof(JZModuleScope);
        const JZModuleScope *scopes = (const JZModuleScope *)module_scopes->data;
        for (size_t i = 0; i < sc; ++i) {
            if (scopes[i].node == mod) {
                scope = &scopes[i];
                break;
            }
        }
    }

    /* Walk child instances (including inside FEATURE_GUARD). */
    for (size_t i = 0; i < mod->child_count; ++i) {
        JZASTNode *child = mod->children[i];
        if (!child) continue;

        if (child->type == JZ_AST_MODULE_INSTANCE && child->text) {
            unsigned array_count = 1;
            if (child->width && *child->width) {
                long long val = 0;
                if (scope && sem_eval_const_expr_in_module(child->width, scope,
                        project_symbols, &val) == 0 && val > 0) {
                    array_count = (unsigned)val;
                } else {
                    unsigned v = 0;
                    if (eval_simple_positive_decl_int(child->width, &v) == 1 && v > 0)
                        array_count = v;
                }
            }
            mem_res_count_instances(project, child->text,
                                     multiplier * array_count,
                                     module_scopes, project_symbols, counts);
        } else if (child->type == JZ_AST_FEATURE_GUARD) {
            /* Walk branches of FEATURE_GUARD for instances. */
            for (size_t bi = 1; bi < child->child_count; ++bi) {
                JZASTNode *branch = child->children[bi];
                if (!branch) continue;
                for (size_t bj = 0; bj < branch->child_count; ++bj) {
                    JZASTNode *bchild = branch->children[bj];
                    if (bchild && bchild->type == JZ_AST_MODULE_INSTANCE && bchild->text) {
                        unsigned array_count = 1;
                        if (bchild->width && *bchild->width) {
                            long long val = 0;
                            if (scope && sem_eval_const_expr_in_module(bchild->width, scope,
                                    project_symbols, &val) == 0 && val > 0) {
                                array_count = (unsigned)val;
                            } else {
                                unsigned v = 0;
                                if (eval_simple_positive_decl_int(bchild->width, &v) == 1 && v > 0)
                                    array_count = v;
                            }
                        }
                        mem_res_count_instances(project, bchild->text,
                                                 multiplier * array_count,
                                                 module_scopes, project_symbols, counts);
                    }
                }
            }
        }
    }
}

void sem_check_project_mem_resources(JZASTNode *project,
                                     const JZBuffer *module_scopes,
                                     const JZBuffer *project_symbols,
                                     const JZChipData *chip,
                                     JZDiagnosticList *diagnostics)
{
    if (!project || project->type != JZ_AST_PROJECT || !chip || !diagnostics) return;

    unsigned block_limit = jz_chip_mem_quantity(chip, JZ_CHIP_MEM_BLOCK);
    unsigned dist_limit_bits = jz_chip_mem_total_bits(chip, JZ_CHIP_MEM_DISTRIBUTED);
    if (block_limit == 0 && dist_limit_bits == 0) return;

    JZASTNode *top_new = sem_find_project_top_new(project);
    if (!top_new || !top_new->name) return;

    /* Build instance count map from @top downward. */
    JZBuffer counts = {0};
    mem_res_count_instances(project, top_new->name, 1,
                             module_scopes, project_symbols, &counts);

    /* Accumulate total BLOCK count and DISTRIBUTED bits. */
    unsigned total_block_count = 0;
    unsigned long long total_dist_bits = 0;

    size_t mc_count = counts.len / sizeof(JZModuleInstanceCount);
    const JZModuleInstanceCount *mc_arr = (const JZModuleInstanceCount *)counts.data;

    for (size_t mi = 0; mi < mc_count; ++mi) {
        const char *mod_name = mc_arr[mi].module_name;
        unsigned inst_count = mc_arr[mi].count;

        /* Find the module AST node. */
        JZASTNode *mod = NULL;
        for (size_t i = 0; i < project->child_count; ++i) {
            JZASTNode *child = project->children[i];
            if (child && child->type == JZ_AST_MODULE && child->name &&
                strcmp(child->name, mod_name) == 0) {
                mod = child;
                break;
            }
        }
        if (!mod) continue;

        /* Find the module scope. */
        const JZModuleScope *scope = NULL;
        if (module_scopes) {
            size_t sc = module_scopes->len / sizeof(JZModuleScope);
            const JZModuleScope *scopes = (const JZModuleScope *)module_scopes->data;
            for (size_t i = 0; i < sc; ++i) {
                if (scopes[i].node == mod) {
                    scope = &scopes[i];
                    break;
                }
            }
        }

        /* Walk MEM blocks in this module. */
        for (size_t i = 0; i < mod->child_count; ++i) {
            JZASTNode *child = mod->children[i];
            if (!child || child->type != JZ_AST_MEM_BLOCK) continue;

            JZChipMemType header_type = sem_mem_header_parse_type(child->text);

            for (size_t j = 0; j < child->child_count; ++j) {
                JZASTNode *mem = child->children[j];
                if (!mem || mem->type != JZ_AST_MEM_DECL) continue;

                unsigned width = 0;
                unsigned depth = 0;
                long long depth_val = 0;
                int have_width = (scope && sem_eval_width_expr(mem->width, scope,
                                   project_symbols, &width) == 0 && width > 0);
                int have_depth = (scope && sem_eval_const_expr_in_module(mem->text, scope,
                                   project_symbols, &depth_val) == 0 && depth_val > 0);
                if (have_depth) depth = (unsigned)depth_val;

                JZChipMemType mem_type = header_type;
                if (mem_type == JZ_CHIP_MEM_UNKNOWN && have_depth) {
                    unsigned r_ports = 0, w_ports = 0;
                    int all_sync = 1;
                    sem_mem_decl_port_counts(mem, &r_ports, &w_ports, &all_sync);
                    mem_type = sem_mem_infer_type(depth, have_depth, all_sync);
                }

                if (mem_type == JZ_CHIP_MEM_BLOCK) {
                    if (have_width && have_depth) {
                        unsigned r_p = 0, w_p = 0;
                        int as = 1;
                        sem_mem_decl_port_counts(mem, &r_p, &w_p, &as);
                        unsigned blocks = sem_compute_block_count(chip, r_p, w_p,
                                                                 width, depth,
                                                                 NULL, NULL);
                        total_block_count += blocks * inst_count;
                    } else {
                        total_block_count += inst_count;
                    }
                } else if (mem_type == JZ_CHIP_MEM_DISTRIBUTED && have_width && have_depth) {
                    total_dist_bits += (unsigned long long)width * depth * inst_count;
                }
            }
        }
    }

    /* Report if limits exceeded. */
    if (block_limit > 0 && total_block_count > block_limit) {
        char msg[512];
        snprintf(msg, sizeof(msg),
                 "design uses %u BLOCK memory instances but chip '%s' has only %u BSRAM blocks",
                 total_block_count,
                 chip->chip_id ? chip->chip_id : "selected chip",
                 block_limit);
        sem_report_rule(diagnostics, top_new->loc,
                        "MEM_BLOCK_RESOURCE_EXCEEDED", msg);
    }

    if (dist_limit_bits > 0 && total_dist_bits > dist_limit_bits) {
        char msg[512];
        snprintf(msg, sizeof(msg),
                 "design uses %llu bits of DISTRIBUTED memory but chip '%s' has only %u bits",
                 (unsigned long long)total_dist_bits,
                 chip->chip_id ? chip->chip_id : "selected chip",
                 dist_limit_bits);
        sem_report_rule(diagnostics, top_new->loc,
                        "MEM_DISTRIBUTED_RESOURCE_EXCEEDED", msg);
    }

    jz_buf_free(&counts);
}
