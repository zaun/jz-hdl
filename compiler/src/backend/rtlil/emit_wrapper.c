/*
 * emit_wrapper.c - Project wrapper emission for the RTLIL backend.
 *
 * The project wrapper is a top-level module that:
 *   - Exposes board pins as ports
 *   - Instantiates clock generators (PLL/DLL/CLKDIV)
 *   - Instantiates differential I/O primitives (OBUF/IBUF/OSER10)
 *   - Instantiates the user's top module with pin-to-port bindings
 *
 * Clock generator and differential I/O instantiation uses chip-specific
 * templates from chip data JSON files (the "rtlil" map entries).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "rtlil_internal.h"
#include "chip_data.h"
#include "ir.h"

/* Forward declaration for differential pin check (defined later in this file). */
static int pin_has_diff_mapping(const IR_Project *proj, const IR_Pin *pin);

/* Reuse Verilog backend helpers. */
#include "backend/verilog-2005/verilog_internal.h"

/* -------------------------------------------------------------------------
 * Clock gen template substitution helpers
 * -------------------------------------------------------------------------
 */

/* Find a config value by parameter name. Returns NULL if not found. */
static const char *find_clock_gen_config(const IR_ClockGenUnit *unit,
                                         const char *param_name)
{
    if (!unit || !param_name) return NULL;
    for (int i = 0; i < unit->num_configs; ++i) {
        const IR_ClockGenConfig *cfg = &unit->configs[i];
        if (cfg->param_name && strcmp(cfg->param_name, param_name) == 0) {
            return cfg->param_value;
        }
    }
    return NULL;
}

/* Find an output clock name by selector. Returns NULL if not found. */
static const char *find_clock_gen_output(const IR_ClockGenUnit *unit,
                                         const char *selector)
{
    if (!unit || !selector) return NULL;
    for (int i = 0; i < unit->num_outputs; ++i) {
        const IR_ClockGenOutput *out = &unit->outputs[i];
        if (out->selector && strcmp(out->selector, selector) == 0) {
            return out->clock_name;
        }
    }
    return NULL;
}

/* Find the period (in ns) of a named clock from the project. */
static double find_clock_period_ns(const IR_Project *proj, const char *clock_name)
{
    if (!proj || !clock_name) return 0.0;
    for (int i = 0; i < proj->num_clocks; ++i) {
        const IR_Clock *clk = &proj->clocks[i];
        if (clk->name && strcmp(clk->name, clock_name) == 0) {
            return clk->period_ns;
        }
    }
    return 0.0;
}

/* Evaluate a simple arithmetic expression with identifiers resolved
 * against clock gen config values.  Supports +, -, *, / with standard
 * precedence.  Returns 1 on success, 0 on failure.
 */
static int eval_arith_expr(const IR_ClockGenUnit *unit,
                           const char *expr,
                           long *result,
                           const JZChipData *chip_data,
                           const char *clock_gen_type)
{
    if (!unit || !expr || !result) return 0;

    long values[16];
    char ops[16];
    int nvals = 0;
    int nops = 0;
    const char *p = expr;

    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;

        if (nvals > nops) {
            if (*p == '+' || *p == '-' || *p == '*' || *p == '/') {
                if (nops >= 15) return 0;
                ops[nops++] = *p++;
            } else {
                return 0;
            }
        } else {
            if (nvals >= 16) return 0;
            if (isdigit((unsigned char)*p)) {
                char *end = NULL;
                long v = strtol(p, &end, 10);
                if (end == p) return 0;
                values[nvals++] = v;
                p = end;
            } else if (isalpha((unsigned char)*p) || *p == '_') {
                const char *start = p;
                while (isalnum((unsigned char)*p) || *p == '_') p++;
                size_t len = (size_t)(p - start);
                char ident[64];
                if (len >= sizeof(ident)) return 0;
                memcpy(ident, start, len);
                ident[len] = '\0';
                const char *val_str = find_clock_gen_config(unit, ident);
                if (!val_str && chip_data && clock_gen_type) {
                    val_str = jz_chip_clock_gen_param_default(
                        chip_data, clock_gen_type, ident);
                }
                if (!val_str) return 0;
                char *end = NULL;
                long v = strtol(val_str, &end, 10);
                if (!end || *end != '\0') return 0;
                values[nvals++] = v;
            } else {
                return 0;
            }
        }
    }

    if (nvals == 0 || nvals != nops + 1) return 0;

    for (int i = 0; i < nops; ) {
        if (ops[i] == '*' || ops[i] == '/') {
            if (ops[i] == '*') {
                values[i] = values[i] * values[i + 1];
            } else {
                if (values[i + 1] == 0) return 0;
                values[i] = values[i] / values[i + 1];
            }
            for (int j = i + 1; j < nvals - 1; j++) values[j] = values[j + 1];
            for (int j = i; j < nops - 1; j++) ops[j] = ops[j + 1];
            nvals--;
            nops--;
        } else {
            i++;
        }
    }

    long res = values[0];
    for (int i = 0; i < nops; i++) {
        if (ops[i] == '+') res += values[i + 1];
        else                res -= values[i + 1];
    }

    *result = res;
    return 1;
}

/* Evaluate a derived expression (e.g., toString(PHASESEL * 2, BIN, 4)). */
static int eval_derived_expr(const IR_ClockGenUnit *unit,
                             const char *expr,
                             char *out_buf, size_t buf_size,
                             const JZChipData *chip_data,
                             const char *clock_gen_type)
{
    if (!unit || !expr || !out_buf || buf_size == 0) return 0;

    while (*expr == ' ') expr++;

    if (strncmp(expr, "toString(", 9) != 0) return 0;
    const char *args_start = expr + 9;
    const char *paren_end = strchr(args_start, ')');
    if (!paren_end) return 0;

    size_t args_len = (size_t)(paren_end - args_start);
    if (args_len >= 128) return 0;
    char args_buf[128];
    memcpy(args_buf, args_start, args_len);
    args_buf[args_len] = '\0';

    char *arg1 = args_buf;
    char *arg2 = strchr(arg1, ',');
    if (!arg2) return 0;
    *arg2++ = '\0';
    char *arg3 = strchr(arg2, ',');
    if (arg3) *arg3++ = '\0';

    while (*arg1 == ' ') arg1++;
    { char *e = arg1 + strlen(arg1) - 1; while (e > arg1 && *e == ' ') *e-- = '\0'; }
    while (*arg2 == ' ') arg2++;
    { char *e = arg2 + strlen(arg2) - 1; while (e > arg2 && *e == ' ') *e-- = '\0'; }
    if (arg3) {
        while (*arg3 == ' ') arg3++;
        char *e = arg3 + strlen(arg3) - 1;
        while (e > arg3 && *e == ' ') *e-- = '\0';
    }

    long lvalue = 0;
    if (!eval_arith_expr(unit, arg1, &lvalue, chip_data, clock_gen_type)) return 0;
    unsigned long value = (unsigned long)lvalue;

    int width = 0;
    if (arg3) {
        char *wend = NULL;
        long w = strtol(arg3, &wend, 10);
        if (!wend || *wend != '\0' || w <= 0 || w > 64) return 0;
        width = (int)w;
    }

    if (strcmp(arg2, "BIN") == 0) {
        if (width == 0) {
            unsigned long tmp = value;
            width = 1;
            while (tmp > 1) { width++; tmp >>= 1; }
        }
        if ((size_t)width >= buf_size) return 0;
        for (int i = width - 1; i >= 0; --i) {
            out_buf[width - 1 - i] = (value & (1UL << (unsigned)i)) ? '1' : '0';
        }
        out_buf[width] = '\0';
        return 1;
    } else if (strcmp(arg2, "HEX") == 0) {
        if (width == 0) {
            unsigned long tmp = value;
            width = 1;
            while (tmp > 15) { width++; tmp >>= 4; }
        }
        if ((size_t)width >= buf_size) return 0;
        snprintf(out_buf, buf_size, "%0*lX", width, value);
        return 1;
    }

    return 0;
}

/* Emit a clock gen instantiation using the chip data RTLIL template.
 * Substitutes placeholders like %%refclk%%, %%BASE%%, %%IDIV%%, etc.
 * For the RTLIL backend, output selectors that have no matching clock name
 * get a dummy wire name using the pattern jz_unused_pll_SELECTOR_cgN_uM.
 */
static void emit_clock_gen_from_template(FILE *out,
                                         const IR_Project *proj,
                                         const IR_ClockGenUnit *unit,
                                         const char *template_text,
                                         const JZChipData *chip_data,
                                         const char *clock_gen_type,
                                         int cg_idx, int unit_idx)
{
    if (!out || !unit || !template_text) return;

    /* Look up the feedback wire base name from chip data (e.g., "clkfb"). */
    const char *fb_wire_base = NULL;
    if (chip_data && clock_gen_type) {
        fb_wire_base = jz_chip_clock_gen_feedback_wire(chip_data, clock_gen_type);
    }

    const char *p = template_text;
    while (*p) {
        /* Handle escape sequences */
        if (p[0] == '\\' && p[1]) {
            switch (p[1]) {
                case 'n':  fputc('\n', out); break;
                case 't':  fputc('\t', out); break;
                case 'r':  fputc('\r', out); break;
                case '\\': fputc('\\', out); break;
                case '"':  fputc('"', out);  break;
                default:   fputc(p[1], out); break;
            }
            p += 2;
            continue;
        }

        /* Look for %% placeholder start */
        if (p[0] == '%' && p[1] == '%') {
            const char *end = strstr(p + 2, "%%");
            if (end) {
                size_t name_len = (size_t)(end - (p + 2));
                char placeholder[64];
                if (name_len < sizeof(placeholder)) {
                    memcpy(placeholder, p + 2, name_len);
                    placeholder[name_len] = '\0';

                    if (strcmp(placeholder, "instance_idx") == 0) {
                        fprintf(out, "%d_%d", cg_idx, unit_idx);
                    } else {
                        /* Try as named input (e.g., REF_CLK, CE) */
                        const char *input_sig = NULL;
                        for (int ii = 0; ii < unit->num_inputs; ++ii) {
                            if (unit->inputs[ii].selector && strcmp(unit->inputs[ii].selector, placeholder) == 0) {
                                input_sig = unit->inputs[ii].signal_name;
                                break;
                            }
                        }
                        if (input_sig) {
                            /* Check if input signal is a differential pin;
                             * if so, use the jz_diff_ buffered wire instead. */
                            int is_diff_input = 0;
                            if (proj) {
                                for (int pi = 0; pi < proj->num_pins; ++pi) {
                                    const IR_Pin *pin = &proj->pins[pi];
                                    if (pin->name && strcmp(pin->name, input_sig) == 0 &&
                                        pin->kind == PIN_IN &&
                                        pin_has_diff_mapping(proj, pin)) {
                                        is_diff_input = 1;
                                        break;
                                    }
                                }
                            }
                            if (is_diff_input) {
                                fprintf(out, "jz_diff_%s", input_sig);
                            } else {
                                fputs(input_sig, out);
                            }
                        } else {
                        /* Check for _mhz or _period_ns suffix on input names */
                        int is_input_suffix = 0;
                        {
                            size_t plen = strlen(placeholder);
                            if (plen > 4 && strcmp(placeholder + plen - 4, "_mhz") == 0) {
                                char base_name[64];
                                size_t blen = plen - 4;
                                if (blen < sizeof(base_name)) {
                                    memcpy(base_name, placeholder, blen);
                                    base_name[blen] = '\0';
                                    const char *sig = NULL;
                                    for (int ii = 0; ii < unit->num_inputs; ++ii) {
                                        if (unit->inputs[ii].selector && strcmp(unit->inputs[ii].selector, base_name) == 0) {
                                            sig = unit->inputs[ii].signal_name;
                                            break;
                                        }
                                    }
                                    if (sig) {
                                        double period_ns = find_clock_period_ns(proj, sig);
                                        if (period_ns > 0) {
                                            double freq_mhz = 1000.0 / period_ns;
                                            fprintf(out, "\"%.3f\"", freq_mhz);
                                        } else {
                                            fputs("\"27\"", out);
                                        }
                                        is_input_suffix = 1;
                                    }
                                }
                            }
                            if (!is_input_suffix && plen > 10 && strcmp(placeholder + plen - 10, "_period_ns") == 0) {
                                char base_name[64];
                                size_t blen = plen - 10;
                                if (blen < sizeof(base_name)) {
                                    memcpy(base_name, placeholder, blen);
                                    base_name[blen] = '\0';
                                    const char *sig = NULL;
                                    for (int ii = 0; ii < unit->num_inputs; ++ii) {
                                        if (unit->inputs[ii].selector && strcmp(unit->inputs[ii].selector, base_name) == 0) {
                                            sig = unit->inputs[ii].signal_name;
                                            break;
                                        }
                                    }
                                    if (sig) {
                                        double period_ns = find_clock_period_ns(proj, sig);
                                        if (period_ns > 0) {
                                            fprintf(out, "%.3f", period_ns);
                                        } else {
                                            fputs("37.037", out);
                                        }
                                        is_input_suffix = 1;
                                    }
                                }
                            }
                        }
                        if (!is_input_suffix) {
                        /* Check chip data for input default */
                        int is_chip_input_default = 0;
                        if (!input_sig && chip_data && clock_gen_type) {
                            const char *dflt = jz_chip_clock_gen_input_default(
                                chip_data, clock_gen_type, placeholder);
                            if (dflt) {
                                fputs(dflt, out);
                                is_chip_input_default = 1;
                            }
                        }
                        if (!is_chip_input_default) {
                        /* Try as config parameter */
                        const char *cfg_val = find_clock_gen_config(unit, placeholder);
                        if (cfg_val) {
                            fputs(cfg_val, out);
                        } else {
                            /* Try as output selector (BASE, PHASE, DIV, DIV3, LOCK) */
                            const char *out_name = find_clock_gen_output(unit, placeholder);
                            if (out_name && out_name[0] != '\0') {
                                fputs(out_name, out);
                            } else if (fb_wire_base &&
                                       strcmp(placeholder, fb_wire_base) == 0) {
                                /* Feedback path wire from chip data */
                                fprintf(out, "%s_%d_%d",
                                        fb_wire_base, cg_idx, unit_idx);
                            } else if (strcmp(placeholder, "LOCK") == 0 ||
                                       strcmp(placeholder, "BASE") == 0 ||
                                       strcmp(placeholder, "PHASE") == 0 ||
                                       strcmp(placeholder, "DIV") == 0 ||
                                       strcmp(placeholder, "DIV3") == 0) {
                                /* Unused output — use a deterministic dummy wire name */
                                fprintf(out, "jz_unused_pll_%s_cg%d_u%d",
                                        placeholder, cg_idx, unit_idx);
                            } else {
                                /* Try as derived expression from chip data */
                                int derived_ok = 0;
                                if (chip_data && clock_gen_type) {
                                    const char *dexpr = jz_chip_clock_gen_derived_expr(
                                        chip_data, clock_gen_type, placeholder);
                                    if (dexpr) {
                                        char dbuf[128];
                                        if (eval_derived_expr(unit, dexpr, dbuf, sizeof(dbuf),
                                                             chip_data, clock_gen_type)) {
                                            fputs(dbuf, out);
                                            derived_ok = 1;
                                        }
                                    }
                                }
                                if (!derived_ok) {
                                    const char *param_default = NULL;
                                    if (chip_data && clock_gen_type) {
                                        param_default = jz_chip_clock_gen_param_default(
                                            chip_data, clock_gen_type, placeholder);
                                    }
                                    if (param_default) {
                                        fputs(param_default, out);
                                    } else {
                                        /* Unknown placeholder — use dummy wire */
                                        fprintf(out, "jz_unused_pll_%s_cg%d_u%d",
                                                placeholder, cg_idx, unit_idx);
                                    }
                                }
                            }
                        }
                        } /* is_chip_input_default */
                        } /* is_input_suffix */
                        } /* input_sig */
                    } /* instance_idx */

                    p = end + 2;
                    continue;
                }
            }
        }

        fputc(*p, out);
        ++p;
    }
}

/* -------------------------------------------------------------------------
 * Differential I/O template expansion
 * -------------------------------------------------------------------------
 */

typedef struct DiffTemplateCtx {
    const char *instance;
    const char *input;
    const char *output;
    const char *pin_p;
    const char *pin_n;
    const char *diff_wire;
    int         ser_ratio;
    const char *fclk;
    const char *pclk;
    const char *reset;
} DiffTemplateCtx;

static void emit_diff_from_template(FILE *out,
                                     const char *template_text,
                                     const DiffTemplateCtx *ctx)
{
    if (!out || !template_text || !ctx) return;

    const char *p = template_text;
    while (*p) {
        if (p[0] == '\\' && p[1]) {
            switch (p[1]) {
                case 'n':  fputc('\n', out); break;
                case 't':  fputc('\t', out); break;
                case '\\': fputc('\\', out); break;
                case '"':  fputc('"', out);  break;
                default:   fputc(p[1], out); break;
            }
            p += 2;
            continue;
        }

        if (p[0] == '%' && p[1] == '%') {
            const char *end = strstr(p + 2, "%%");
            if (end) {
                size_t name_len = (size_t)(end - (p + 2));
                char ph[64];
                if (name_len < sizeof(ph)) {
                    memcpy(ph, p + 2, name_len);
                    ph[name_len] = '\0';

                    if (strcmp(ph, "instance") == 0 && ctx->instance) {
                        fputs(ctx->instance, out);
                    } else if (strcmp(ph, "input") == 0 && ctx->input) {
                        fputs(ctx->input, out);
                    } else if (strcmp(ph, "output") == 0 && ctx->output) {
                        fputs(ctx->output, out);
                    } else if (strcmp(ph, "pin_p") == 0 && ctx->pin_p) {
                        fputs(ctx->pin_p, out);
                    } else if (strcmp(ph, "pin_n") == 0 && ctx->pin_n) {
                        fputs(ctx->pin_n, out);
                    } else if (strcmp(ph, "fclk") == 0 && ctx->fclk) {
                        fputs(ctx->fclk, out);
                    } else if (strcmp(ph, "pclk") == 0 && ctx->pclk) {
                        fputs(ctx->pclk, out);
                    } else if (strcmp(ph, "reset") == 0 && ctx->reset) {
                        fputs(ctx->reset, out);
                    } else if ((ph[0] == 'D' || ph[0] == 'Q') && ph[1] >= '0' && ph[1] <= '9' &&
                               (ph[2] == '\0' || (ph[2] >= '0' && ph[2] <= '9' && ph[3] == '\0'))) {
                        /* D0..D9 serializer data inputs / Q0..Q9 deserializer data outputs */
                        int idx = atoi(ph + 1);
                        if (ctx->diff_wire && idx < ctx->ser_ratio) {
                            fprintf(out, "%s [%d]", ctx->diff_wire, idx);
                        } else {
                            fputs("1'0", out);
                        }
                    } else {
                        /* Unknown placeholder */
                        fprintf(out, "$unknown_%s", ph);
                    }
                }
                p = end + 2;
                continue;
            }
        }

        fputc(*p, out);
        ++p;
    }
}

/* -------------------------------------------------------------------------
 * Top binding helpers
 * -------------------------------------------------------------------------
 */

static const IR_TopBinding *find_top_binding_for_bit(const IR_Project *proj,
                                                     int top_signal_id,
                                                     int top_bit_index)
{
    if (!proj || !proj->top_bindings) return NULL;
    for (int i = 0; i < proj->num_top_bindings; ++i) {
        const IR_TopBinding *tb = &proj->top_bindings[i];
        if (tb->top_port_signal_id == top_signal_id &&
            tb->top_bit_index == top_bit_index) {
            return tb;
        }
    }
    return NULL;
}

/* -------------------------------------------------------------------------
 * Differential I/O helpers
 * -------------------------------------------------------------------------
 */

/* Check if a pin has differential P/N mappings in the project. */
static int pin_has_diff_mapping(const IR_Project *proj, const IR_Pin *pin)
{
    if (!proj || !pin || pin->mode != PIN_MODE_DIFFERENTIAL) return 0;
    for (int i = 0; i < proj->num_mappings; ++i) {
        const IR_PinMapping *m = &proj->mappings[i];
        if (m->logical_pin_name && pin->name &&
            strcmp(m->logical_pin_name, pin->name) == 0 &&
            m->board_pin_n_id && m->board_pin_n_id[0] != '\0') {
            return 1;
        }
    }
    return 0;
}

/* Check if a differential pin has fclk/pclk (needs serializer). */
static int pin_needs_serializer(const IR_Pin *pin)
{
    return pin && pin->fclk_name && pin->fclk_name[0] != '\0' &&
           pin->pclk_name && pin->pclk_name[0] != '\0';
}

/* Find the data width of the top module signal bound to a pin/bit. */
static int find_signal_width_for_pin(const IR_Design *design,
                                      const IR_Project *proj,
                                      int pin_idx, int pin_bit)
{
    if (!design || !proj) return 0;
    const IR_Module *top_mod = NULL;
    if (proj->top_module_id >= 0 && proj->top_module_id < design->num_modules) {
        top_mod = &design->modules[proj->top_module_id];
    }
    if (!top_mod) return 0;

    for (int t = 0; t < proj->num_top_bindings; ++t) {
        const IR_TopBinding *tb = &proj->top_bindings[t];
        if (tb->pin_id == pin_idx && tb->pin_bit_index == pin_bit) {
            for (int s = 0; s < top_mod->num_signals; ++s) {
                if (top_mod->signals[s].id == tb->top_port_signal_id) {
                    return top_mod->signals[s].width;
                }
            }
        }
    }
    for (int t = 0; t < proj->num_top_bindings; ++t) {
        const IR_TopBinding *tb = &proj->top_bindings[t];
        if (tb->pin_id == pin_idx && tb->pin_bit_index == -1) {
            for (int s = 0; s < top_mod->num_signals; ++s) {
                if (top_mod->signals[s].id == tb->top_port_signal_id) {
                    return top_mod->signals[s].width;
                }
            }
        }
    }
    return 0;
}

/* -------------------------------------------------------------------------
 * Project wrapper emission
 * -------------------------------------------------------------------------
 */

void rtlil_emit_project_wrapper(FILE *out, const IR_Design *design)
{
    if (!out || !design || !design->project) return;

    const IR_Project *proj = design->project;
    if (proj->top_module_id < 0 || proj->top_module_id >= design->num_modules) {
        return;
    }

    const IR_Module *top_mod = &design->modules[proj->top_module_id];
    const char *top_mod_name = (top_mod->name && top_mod->name[0] != '\0')
                             ? top_mod->name : "jz_top";

    const char *wrapper_name = rtlil_design_has_module_named(design, "top")
                             ? "project_top" : "top";

    int num_pins = proj->num_pins;
    if (num_pins <= 0) return;

    /* Load chip data for templates */
    const char *proj_filename = (design->num_source_files > 0 && design->source_files[0].path)
                              ? design->source_files[0].path : NULL;
    JZChipData chip_data;
    int have_chip_data = 0;
    if (proj->chip_id && proj->chip_id[0]) {
        JZChipLoadStatus st = jz_chip_data_load(proj->chip_id, proj_filename, &chip_data);
        if (st == JZ_CHIP_LOAD_OK) {
            have_chip_data = 1;
        }
    }

    /* Emit wrapper module header. */
    fprintf(out, "attribute \\top 1\n");
    fprintf(out, "module \\%s\n", wrapper_name);

    /* Emit pin wires as ports.
     * Differential pins are split into _p and _n scalar ports. */
    int port_idx = 1;
    for (int i = 0; i < num_pins; ++i) {
        const IR_Pin *pin = &proj->pins[i];
        const char *name = pin->name ? pin->name : "jz_pin";
        int width = pin->width > 0 ? pin->width : 1;
        int is_diff = pin_has_diff_mapping(proj, pin);

        const char *dir_str;
        switch (pin->kind) {
            case PIN_IN:    dir_str = "input"; break;
            case PIN_OUT:   dir_str = "output"; break;
            case PIN_INOUT: dir_str = "inout"; break;
            default:        dir_str = "input"; break;
        }

        if (is_diff) {
            for (int bit = 0; bit < width; ++bit) {
                rtlil_indent(out, 1);
                if (width > 1) {
                    fprintf(out, "wire width 1 %s %d \\%s%d_p\n",
                            dir_str, port_idx++, name, bit);
                    rtlil_indent(out, 1);
                    fprintf(out, "wire width 1 %s %d \\%s%d_n\n",
                            dir_str, port_idx++, name, bit);
                } else {
                    fprintf(out, "wire width 1 %s %d \\%s_p\n",
                            dir_str, port_idx++, name);
                    rtlil_indent(out, 1);
                    fprintf(out, "wire width 1 %s %d \\%s_n\n",
                            dir_str, port_idx++, name);
                }
            }
        } else {
            rtlil_indent(out, 1);
            fprintf(out, "wire width %d", width);
            fprintf(out, " %s %d", dir_str, port_idx++);
            fprintf(out, " \\%s\n", name);
        }
    }

    /* Emit internal wires for clock gen outputs. */
    for (int cg = 0; cg < proj->num_clock_gens; ++cg) {
        const IR_ClockGen *clock_gen = &proj->clock_gens[cg];
        for (int u = 0; u < clock_gen->num_units; ++u) {
            const IR_ClockGenUnit *unit = &clock_gen->units[u];
            for (int o = 0; o < unit->num_outputs; ++o) {
                const IR_ClockGenOutput *out_clk = &unit->outputs[o];
                if (out_clk->clock_name && out_clk->clock_name[0] != '\0') {
                    rtlil_indent(out, 1);
                    fprintf(out, "wire width 1 \\%s\n", out_clk->clock_name);
                }
            }
        }
    }

    /* Pre-declare jz_diff_ wires for differential input pins used by clock generators.
     * In RTLIL, wires must be declared before they are referenced.
     * Deduplicate: multiple clock gen units may share the same input. */
    {
        const char *declared_diff[16];
        int num_declared_diff = 0;
        for (int cg = 0; cg < proj->num_clock_gens; ++cg) {
            const IR_ClockGen *clock_gen = &proj->clock_gens[cg];
            for (int u = 0; u < clock_gen->num_units; ++u) {
                const IR_ClockGenUnit *unit = &clock_gen->units[u];
                for (int ii = 0; ii < unit->num_inputs; ++ii) {
                    const char *sig = unit->inputs[ii].signal_name;
                    if (!sig) continue;
                    /* Check if already declared */
                    int already = 0;
                    for (int d = 0; d < num_declared_diff; ++d) {
                        if (strcmp(declared_diff[d], sig) == 0) { already = 1; break; }
                    }
                    if (already) continue;
                    for (int pi = 0; pi < proj->num_pins; ++pi) {
                        const IR_Pin *pin = &proj->pins[pi];
                        if (pin->name && strcmp(pin->name, sig) == 0 &&
                            pin->kind == PIN_IN && pin_has_diff_mapping(proj, pin)) {
                            rtlil_indent(out, 1);
                            fprintf(out, "wire width 1 \\jz_diff_%s\n", sig);
                            if (num_declared_diff < 16)
                                declared_diff[num_declared_diff++] = sig;
                            break;
                        }
                    }
                }
            }
        }
    }

    /* Emit clock gen instantiations using chip data templates. */
    for (int cg = 0; cg < proj->num_clock_gens; ++cg) {
        const IR_ClockGen *clock_gen = &proj->clock_gens[cg];

        const JZChipData *effective_chip = have_chip_data ? &chip_data : NULL;

        for (int u = 0; u < clock_gen->num_units; ++u) {
            const IR_ClockGenUnit *unit = &clock_gen->units[u];
            const char *unit_type_str = unit->type ? unit->type : "pll";

            /* Try to get the RTLIL template map from chip data */
            const char *template_text = NULL;
            if (effective_chip) {
                template_text = jz_chip_clock_gen_map(effective_chip, unit_type_str, "rtlil");
            }

            if (template_text) {
                /* Emit dummy wires for unused PLL/MMCM outputs before the cell */
                if (unit_type_str && strncmp(unit_type_str, "pll", 3) == 0) {
                    static const char *pll_selectors[] = {
                        "LOCK", "PHASE", "DIV", "DIV3",
                        "CLKOUT1", "CLKOUT2", "CLKOUT3", "CLKOUT4",
                        "CLKOUT5", "CLKOUT6", NULL
                    };
                    for (int si = 0; pll_selectors[si]; ++si) {
                        const char *out_name = find_clock_gen_output(unit, pll_selectors[si]);
                        if (!out_name || out_name[0] == '\0') {
                            rtlil_indent(out, 1);
                            fprintf(out, "wire width 1 \\jz_unused_pll_%s_cg%d_u%d\n",
                                    pll_selectors[si], cg, u);
                        }
                    }
                    /* Feedback path wire (from chip data feedback_wire field) */
                    const char *fb_name = effective_chip
                        ? jz_chip_clock_gen_feedback_wire(effective_chip, unit_type_str)
                        : NULL;
                    if (fb_name) {
                        rtlil_indent(out, 1);
                        fprintf(out, "wire width 1 \\%s_%d_%d\n", fb_name, cg, u);
                    }
                }

                /* Expand template */
                rtlil_indent(out, 1);
                emit_clock_gen_from_template(out, proj, unit, template_text,
                                             effective_chip, unit_type_str,
                                             cg, u);
            } else {
                /* Fallback: hardcoded emission (unchanged from original) */
                /* Build uppercase type for fallback cell name */
                char unit_type_buf[32];
                {
                    size_t tl = strlen(unit_type_str);
                    if (tl >= sizeof(unit_type_buf)) tl = sizeof(unit_type_buf) - 1;
                    for (size_t ti = 0; ti < tl; ++ti)
                        unit_type_buf[ti] = (char)toupper((unsigned char)unit_type_str[ti]);
                    unit_type_buf[tl] = '\0';
                }
                const char *unit_type = unit_type_buf;

                int cell_id = rtlil_next_id();
                rtlil_indent(out, 1);
                fprintf(out, "cell \\%s $auto$%d\n", unit_type, cell_id);

                for (int c = 0; c < unit->num_configs; ++c) {
                    const IR_ClockGenConfig *cfg = &unit->configs[c];
                    if (cfg->param_name && cfg->param_value) {
                        rtlil_indent(out, 2);
                        char *endp = NULL;
                        long val = strtol(cfg->param_value, &endp, 10);
                        if (endp && *endp == '\0') {
                            fprintf(out, "parameter \\%s_SEL %ld\n",
                                    cfg->param_name, val);
                        } else {
                            fprintf(out, "parameter \\%s \"%s\"\n",
                                    cfg->param_name, cfg->param_value);
                        }
                    }
                }

                /* Emit REF_CLK input connection for fallback path */
                for (int ii = 0; ii < unit->num_inputs; ++ii) {
                    if (unit->inputs[ii].selector &&
                        strcmp(unit->inputs[ii].selector, "REF_CLK") == 0 &&
                        unit->inputs[ii].signal_name &&
                        unit->inputs[ii].signal_name[0] != '\0') {
                        rtlil_indent(out, 2);
                        fprintf(out, "connect \\CLKIN \\%s\n", unit->inputs[ii].signal_name);
                        break;
                    }
                }

                for (int o = 0; o < unit->num_outputs; ++o) {
                    const IR_ClockGenOutput *out_clk = &unit->outputs[o];
                    if (out_clk->clock_name && out_clk->clock_name[0] != '\0' &&
                        out_clk->selector && out_clk->selector[0] != '\0') {
                        rtlil_indent(out, 2);
                        fprintf(out, "connect \\CLK%s \\%s\n",
                                out_clk->selector, out_clk->clock_name);
                    }
                }

                rtlil_indent(out, 1);
                fprintf(out, "end\n");
            }
        }

    }

    /* ---- Differential I/O primitives ---- */
    const char *obuf_template = have_chip_data
        ? jz_chip_diff_output_buffer_map(&chip_data, "rtlil") : NULL;
    const char *ibuf_template = have_chip_data
        ? jz_chip_diff_input_buffer_map(&chip_data, "rtlil") : NULL;
    /* Serializer template is now selected per-pin based on needed data width */

    /* Hardcoded RTLIL fallbacks (Gowin TLVDS primitives).
     * The template engine interprets \n as newline and \\ as literal backslash.
     * RTLIL identifiers need \\ to produce a literal \ in output. */
    static const char fallback_obuf[] =
        "cell \\\\TLVDS_OBUF \\\\u_%%instance%%\\n"
        "    connect \\\\I \\\\%%input%%\\n"
        "    connect \\\\O \\\\%%pin_p%%\\n"
        "    connect \\\\OB \\\\%%pin_n%%\\n"
        "  end\\n";
    static const char fallback_ibuf[] =
        "cell \\\\TLVDS_IBUF \\\\u_%%instance%%\\n"
        "    connect \\\\I \\\\%%pin_p%%\\n"
        "    connect \\\\IB \\\\%%pin_n%%\\n"
        "    connect \\\\O \\\\%%output%%\\n"
        "  end\\n";
    static const char fallback_oser[] =
        "cell \\\\OSER10 \\\\u_%%instance%%\\n"
        "    parameter \\\\GSREN \\\"FALSE\\\"\\n"
        "    parameter \\\\LSREN \\\"TRUE\\\"\\n"
        "    connect \\\\D0 \\\\%%D0%%\\n"
        "    connect \\\\D1 \\\\%%D1%%\\n"
        "    connect \\\\D2 \\\\%%D2%%\\n"
        "    connect \\\\D3 \\\\%%D3%%\\n"
        "    connect \\\\D4 \\\\%%D4%%\\n"
        "    connect \\\\D5 \\\\%%D5%%\\n"
        "    connect \\\\D6 \\\\%%D6%%\\n"
        "    connect \\\\D7 \\\\%%D7%%\\n"
        "    connect \\\\D8 \\\\%%D8%%\\n"
        "    connect \\\\D9 \\\\%%D9%%\\n"
        "    connect \\\\FCLK \\\\%%fclk%%\\n"
        "    connect \\\\PCLK \\\\%%pclk%%\\n"
        "    connect \\\\RESET %%reset%%\\n"
        "    connect \\\\Q \\\\%%output%%\\n"
        "  end\\n";

    if (!obuf_template) obuf_template = fallback_obuf;
    if (!ibuf_template) ibuf_template = fallback_ibuf;

    for (int i = 0; i < num_pins; ++i) {
        const IR_Pin *pin = &proj->pins[i];
        if (!pin_has_diff_mapping(proj, pin)) continue;
        const char *name = pin->name ? pin->name : "jz_pin";

        if (pin->kind == PIN_OUT) {
            int has_any_ser = have_chip_data && jz_chip_diff_serializer_ratio(&chip_data) > 0;
            int needs_ser = pin_needs_serializer(pin) && has_any_ser;

            for (int bit = 0; bit < pin->width; ++bit) {
                char suffix[32];
                if (pin->width > 1) {
                    snprintf(suffix, sizeof(suffix), "%d", bit);
                } else {
                    suffix[0] = '\0';
                }

                char pin_p[128], pin_n[128];
                if (pin->width > 1) {
                    snprintf(pin_p, sizeof(pin_p), "%s%d_p", name, bit);
                    snprintf(pin_n, sizeof(pin_n), "%s%d_n", name, bit);
                } else {
                    snprintf(pin_p, sizeof(pin_p), "%s_p", name);
                    snprintf(pin_n, sizeof(pin_n), "%s_n", name);
                }

                if (needs_ser) {
                    /* Determine actual data width from the top module signal */
                    int data_width = find_signal_width_for_pin(design, proj, i, bit);
                    if (data_width <= 0) data_width = jz_chip_diff_serializer_ratio(&chip_data);

                    /* Find best serializer with ratio >= data_width */
                    int sel_ratio = jz_chip_diff_best_serializer_ratio(&chip_data, data_width);
                    const char *sel_template = sel_ratio > 0
                        ? jz_chip_diff_best_serializer_map(&chip_data, data_width, "rtlil")
                        : NULL;

                    if (sel_ratio <= 0 || !sel_template) {
                        sel_template = fallback_oser;
                        sel_ratio = jz_chip_diff_max_serializer_ratio(&chip_data);
                        if (sel_ratio <= 0) sel_ratio = 10;
                        sel_template = jz_chip_diff_best_serializer_map(&chip_data, 1, "rtlil");
                        if (!sel_template) sel_template = fallback_oser;
                    }

                    int wire_width = sel_ratio;

                    /* Intermediate wires */
                    char diff_wire[128], ser_wire[128];
                    snprintf(diff_wire, sizeof(diff_wire), "jz_diff_%s%s", name, suffix);
                    snprintf(ser_wire, sizeof(ser_wire), "jz_ser_%s%s", name, suffix);

                    rtlil_indent(out, 1);
                    fprintf(out, "wire width %d \\%s\n", wire_width, diff_wire);
                    rtlil_indent(out, 1);
                    fprintf(out, "wire width 1 \\%s\n", ser_wire);

                    /* Serializer cell */
                    char oser_inst[128];
                    snprintf(oser_inst, sizeof(oser_inst), "oser_%s%s", name, suffix);

                    /* Build reset: RTLIL can't express inline ~, so we emit a $not
                     * cell to match the ~%%reset%% in the Verilog templates. */
                    char reset_wire_buf[128];
                    const char *reset_val = "1'0";
                    if (pin->reset_name && pin->reset_name[0]) {
                        snprintf(reset_wire_buf, sizeof(reset_wire_buf),
                                 "jz_rst_inv_%s%s", name, suffix);
                        reset_val = reset_wire_buf;

                        /* Emit inverted reset wire and NOT cell */
                        rtlil_indent(out, 1);
                        fprintf(out, "wire width 1 \\%s\n", reset_wire_buf);
                        rtlil_indent(out, 1);
                        fprintf(out, "cell $not $auto$rst_inv_%s%s\n", name, suffix);
                        rtlil_indent(out, 2);
                        fprintf(out, "parameter \\A_SIGNED 0\n");
                        rtlil_indent(out, 2);
                        fprintf(out, "parameter \\A_WIDTH 1\n");
                        rtlil_indent(out, 2);
                        fprintf(out, "parameter \\Y_WIDTH 1\n");
                        rtlil_indent(out, 2);
                        fprintf(out, "connect \\A \\%s\n", pin->reset_name);
                        rtlil_indent(out, 2);
                        fprintf(out, "connect \\Y \\%s\n", reset_wire_buf);
                        rtlil_indent(out, 1);
                        fprintf(out, "end\n");
                    }

                    DiffTemplateCtx ser_ctx = {
                        .instance  = oser_inst,
                        .input     = NULL,
                        .output    = ser_wire,
                        .pin_p     = NULL,
                        .pin_n     = NULL,
                        .diff_wire = diff_wire,
                        .ser_ratio = wire_width,
                        .fclk      = pin->fclk_name,
                        .pclk      = pin->pclk_name,
                        .reset     = reset_val,
                    };
                    rtlil_indent(out, 1);
                    emit_diff_from_template(out, sel_template, &ser_ctx);

                    /* Output buffer cell */
                    char obuf_inst[128];
                    snprintf(obuf_inst, sizeof(obuf_inst), "obuf_%s%s", name, suffix);
                    DiffTemplateCtx buf_ctx = {
                        .instance  = obuf_inst,
                        .input     = ser_wire,
                        .output    = NULL,
                        .pin_p     = pin_p,
                        .pin_n     = pin_n,
                        .diff_wire = NULL,
                        .ser_ratio = 0,
                        .fclk      = NULL,
                        .pclk      = NULL,
                        .reset     = NULL,
                    };
                    if (obuf_template) {
                        rtlil_indent(out, 1);
                        emit_diff_from_template(out, obuf_template, &buf_ctx);
                    }
                } else {
                    /* Direct diff buffer (no serializer) */
                    char diff_wire[128];
                    snprintf(diff_wire, sizeof(diff_wire), "jz_diff_%s%s", name, suffix);
                    rtlil_indent(out, 1);
                    fprintf(out, "wire width 1 \\%s\n", diff_wire);

                    char obuf_inst[128];
                    snprintf(obuf_inst, sizeof(obuf_inst), "obuf_%s%s", name, suffix);
                    DiffTemplateCtx buf_ctx = {
                        .instance  = obuf_inst,
                        .input     = diff_wire,
                        .output    = NULL,
                        .pin_p     = pin_p,
                        .pin_n     = pin_n,
                        .diff_wire = NULL,
                        .ser_ratio = 0,
                        .fclk      = NULL,
                        .pclk      = NULL,
                        .reset     = NULL,
                    };
                    if (obuf_template) {
                        rtlil_indent(out, 1);
                        emit_diff_from_template(out, obuf_template, &buf_ctx);
                    }
                }
            }
        } else if (pin->kind == PIN_IN) {
            for (int bit = 0; bit < pin->width; ++bit) {
                char suffix[32];
                if (pin->width > 1) {
                    snprintf(suffix, sizeof(suffix), "%d", bit);
                } else {
                    suffix[0] = '\0';
                }

                char diff_wire[128];
                snprintf(diff_wire, sizeof(diff_wire), "jz_diff_%s%s", name, suffix);

                /* Skip wire declaration if already pre-declared for clock gen input */
                int already_declared = 0;
                for (int cg2 = 0; cg2 < proj->num_clock_gens && !already_declared; ++cg2) {
                    const IR_ClockGen *cgen = &proj->clock_gens[cg2];
                    for (int u2 = 0; u2 < cgen->num_units && !already_declared; ++u2) {
                        for (int ii2 = 0; ii2 < cgen->units[u2].num_inputs; ++ii2) {
                            const char *sig = cgen->units[u2].inputs[ii2].signal_name;
                            if (sig && strcmp(sig, name) == 0) {
                                already_declared = 1;
                                break;
                            }
                        }
                    }
                }
                if (!already_declared) {
                    rtlil_indent(out, 1);
                    fprintf(out, "wire width 1 \\%s\n", diff_wire);
                }

                char pin_p[128], pin_n[128];
                if (pin->width > 1) {
                    snprintf(pin_p, sizeof(pin_p), "%s%d_p", name, bit);
                    snprintf(pin_n, sizeof(pin_n), "%s%d_n", name, bit);
                } else {
                    snprintf(pin_p, sizeof(pin_p), "%s_p", name);
                    snprintf(pin_n, sizeof(pin_n), "%s_n", name);
                }

                char ibuf_inst[128];
                snprintf(ibuf_inst, sizeof(ibuf_inst), "ibuf_%s%s", name, suffix);
                DiffTemplateCtx buf_ctx = {
                    .instance  = ibuf_inst,
                    .input     = NULL,
                    .output    = diff_wire,
                    .pin_p     = pin_p,
                    .pin_n     = pin_n,
                    .diff_wire = NULL,
                    .ser_ratio = 0,
                    .fclk      = NULL,
                    .pclk      = NULL,
                    .reset     = NULL,
                };
                if (ibuf_template) {
                    rtlil_indent(out, 1);
                    emit_diff_from_template(out, ibuf_template, &buf_ctx);
                }
            }
        }
    }

    /* ---- Pre-scan: declare dummy wires for unbound output ports ---- */
    for (int i = 0; i < top_mod->num_signals; ++i) {
        const IR_Signal *sig = &top_mod->signals[i];
        if (sig->kind != SIG_PORT) continue;
        if (sig->u.port.direction != PORT_OUT) continue;

        int w = sig->width > 0 ? sig->width : 1;
        /* Check if all bits are unbound. */
        int all_unbound = 1;
        if (w <= 1) {
            const IR_TopBinding *tb = find_top_binding_for_bit(proj, sig->id, -1);
            if (!tb) tb = find_top_binding_for_bit(proj, sig->id, 0);
            if (tb) all_unbound = 0;
        } else {
            for (int bit = 0; bit < w && all_unbound; ++bit) {
                if (find_top_binding_for_bit(proj, sig->id, bit))
                    all_unbound = 0;
            }
        }
        if (all_unbound) {
            rtlil_indent(out, 1);
            if (w > 1) {
                fprintf(out, "wire width %d \\jz_nc_%s\n", w, sig->name);
            } else {
                fprintf(out, "wire \\jz_nc_%s\n", sig->name);
            }
        }
    }

    /* ---- Pre-scan for inverted bindings and emit $not cells ---- */
    for (int i = 0; i < top_mod->num_signals; ++i) {
        const IR_Signal *sig = &top_mod->signals[i];
        if (sig->kind != SIG_PORT) continue;

        int width = sig->width;
        if (width <= 1) {
            const IR_TopBinding *tb = find_top_binding_for_bit(proj, sig->id, -1);
            if (!tb) tb = find_top_binding_for_bit(proj, sig->id, 0);
            if (tb && tb->inverted) {
                /* Declare intermediate wire and $not cell. */
                rtlil_indent(out, 1);
                fprintf(out, "wire \\jz_inv_%s\n", sig->name);

                /* Determine the pin sigspec. */
                char pin_ss[256];
                if (tb->pin_id >= 0 && tb->pin_id < proj->num_pins) {
                    const IR_Pin *pin = &proj->pins[tb->pin_id];
                    const char *pn = pin->name ? pin->name : "jz_pin";
                    if (tb->pin_bit_index >= 0) {
                        snprintf(pin_ss, sizeof(pin_ss), "\\%s [%d]", pn, tb->pin_bit_index);
                    } else {
                        snprintf(pin_ss, sizeof(pin_ss), "\\%s", pn);
                    }
                } else {
                    snprintf(pin_ss, sizeof(pin_ss), "1'0");
                }

                int not_id = rtlil_next_id();
                if (sig->u.port.direction == PORT_IN) {
                    /* Input: invert pin → intermediate. */
                    rtlil_indent(out, 1);
                    fprintf(out, "cell $not $auto$%d\n", not_id);
                    rtlil_indent(out, 2);
                    fprintf(out, "parameter \\A_SIGNED 0\n");
                    rtlil_indent(out, 2);
                    fprintf(out, "parameter \\A_WIDTH 1\n");
                    rtlil_indent(out, 2);
                    fprintf(out, "parameter \\Y_WIDTH 1\n");
                    rtlil_indent(out, 2);
                    fprintf(out, "connect \\A %s\n", pin_ss);
                    rtlil_indent(out, 2);
                    fprintf(out, "connect \\Y \\jz_inv_%s\n", sig->name);
                    rtlil_indent(out, 1);
                    fprintf(out, "end\n");
                } else {
                    /* Output: invert intermediate → pin. */
                    rtlil_indent(out, 1);
                    fprintf(out, "cell $not $auto$%d\n", not_id);
                    rtlil_indent(out, 2);
                    fprintf(out, "parameter \\A_SIGNED 0\n");
                    rtlil_indent(out, 2);
                    fprintf(out, "parameter \\A_WIDTH 1\n");
                    rtlil_indent(out, 2);
                    fprintf(out, "parameter \\Y_WIDTH 1\n");
                    rtlil_indent(out, 2);
                    fprintf(out, "connect \\A \\jz_inv_%s\n", sig->name);
                    rtlil_indent(out, 2);
                    fprintf(out, "connect \\Y %s\n", pin_ss);
                    rtlil_indent(out, 1);
                    fprintf(out, "end\n");
                }
            }
        }
        /* TODO: multi-bit inverted ports if needed. */
    }

    /* ---- Top module instantiation ---- */
    int top_cell_id = rtlil_next_id();
    rtlil_indent(out, 1);
    fprintf(out, "cell \\%s \\u_top\n", top_mod_name);

    for (int i = 0; i < top_mod->num_signals; ++i) {
        const IR_Signal *sig = &top_mod->signals[i];
        if (sig->kind != SIG_PORT) continue;
        const char *pname = sig->name ? sig->name : "jz_port";

        rtlil_indent(out, 2);
        fprintf(out, "connect \\%s ", pname);

        /* Check if this port connects to a differential pin. */
        int is_diff_port = 0;
        const IR_Pin *diff_pin = NULL;
        int diff_pin_bit = -1;
        {
            const IR_TopBinding *tb0 = find_top_binding_for_bit(proj, sig->id, -1);
            if (!tb0 && sig->width > 1) {
                tb0 = find_top_binding_for_bit(proj, sig->id, 0);
            }
            if (tb0 && tb0->pin_id >= 0 && tb0->pin_id < proj->num_pins) {
                const IR_Pin *pin = &proj->pins[tb0->pin_id];
                if (pin_has_diff_mapping(proj, pin)) {
                    is_diff_port = 1;
                    diff_pin = pin;
                    diff_pin_bit = tb0->pin_bit_index;
                }
            }
        }

        if (is_diff_port && diff_pin) {
            const char *dname = diff_pin->name ? diff_pin->name : "jz_pin";
            if (diff_pin_bit >= 0) {
                fprintf(out, "\\jz_diff_%s%d", dname, diff_pin_bit);
            } else {
                fprintf(out, "\\jz_diff_%s", dname);
            }
        } else {
            int width = sig->width;
            if (width <= 1) {
                const IR_TopBinding *tb = find_top_binding_for_bit(
                    proj, sig->id, -1);
                if (!tb) {
                    tb = find_top_binding_for_bit(proj, sig->id, 0);
                }
                if (tb && tb->inverted) {
                    /* Use the intermediate inverted wire. */
                    fprintf(out, "\\jz_inv_%s", pname);
                } else if (tb) {
                    if (tb->pin_id >= 0 && tb->pin_id < proj->num_pins) {
                        const IR_Pin *pin = &proj->pins[tb->pin_id];
                        const char *pin_name = pin->name ? pin->name : "jz_pin";
                        if (tb->pin_bit_index >= 0) {
                            fprintf(out, "\\%s [%d]", pin_name, tb->pin_bit_index);
                        } else {
                            fprintf(out, "\\%s", pin_name);
                        }
                    } else if (tb->clock_name && tb->clock_name[0] != '\0') {
                        fprintf(out, "\\%s", tb->clock_name);
                    } else {
                        fprintf(out, "1'%c", tb->const_value ? '1' : '0');
                    }
                } else if (sig->u.port.direction == PORT_OUT) {
                    fprintf(out, "\\jz_nc_%s", pname);
                } else {
                    fprintf(out, "1'0");
                }
            } else {
                /* Multi-bit port: check if it's an unbound output. */
                int all_unbound = 1;
                for (int bit = 0; bit < width && all_unbound; ++bit) {
                    if (find_top_binding_for_bit(proj, sig->id, bit))
                        all_unbound = 0;
                }
                if (all_unbound && sig->u.port.direction == PORT_OUT) {
                    /* Unbound output: use a dummy wire. */
                    fprintf(out, "\\jz_nc_%s", pname);
                } else {
                    /* Build a concat sigspec. */
                    fprintf(out, "{ ");
                    for (int bit = width - 1; bit >= 0; --bit) {
                        if (bit != width - 1) fputs(" ", out);
                        const IR_TopBinding *tb = find_top_binding_for_bit(
                            proj, sig->id, bit);
                        if (tb) {
                            if (tb->pin_id >= 0 && tb->pin_id < proj->num_pins) {
                                const IR_Pin *pin = &proj->pins[tb->pin_id];
                                const char *pin_name = pin->name ? pin->name : "jz_pin";
                                int pin_w = pin->width > 0 ? pin->width : 1;
                                if (tb->pin_bit_index >= 0 && tb->pin_bit_index < pin_w) {
                                    if (pin_w > 1) {
                                        fprintf(out, "\\%s [%d]", pin_name, tb->pin_bit_index);
                                    } else {
                                        fprintf(out, "\\%s", pin_name);
                                    }
                                } else if (tb->pin_bit_index < 0) {
                                    fprintf(out, "\\%s", pin_name);
                                } else {
                                    fprintf(out, "1'0");
                                }
                            } else if (tb->clock_name && tb->clock_name[0] != '\0') {
                                fprintf(out, "\\%s", tb->clock_name);
                            } else {
                                fprintf(out, "1'%c", tb->const_value ? '1' : '0');
                            }
                        } else {
                            fprintf(out, "1'0");
                        }
                    }
                    fprintf(out, " }");
                }
            }
        }
        fputc('\n', out);
    }

    rtlil_indent(out, 1);
    fprintf(out, "end\n");

    (void)top_cell_id;

    fprintf(out, "end\n\n");

    if (have_chip_data) {
        jz_chip_data_free(&chip_data);
    }
}
