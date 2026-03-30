/*
 * emit_wrapper.c - Project wrapper emission for the Verilog-2005 backend.
 *
 * This file handles emitting the project-level wrapper module that exposes
 * board pins and instantiates the top module.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "verilog_internal.h"
#include "chip_data.h"
#include "ir.h"

/* Forward declaration for differential pin check (defined later in this file). */
static int pin_has_diff_mapping(const IR_Project *proj, const IR_Pin *pin);

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

/* Find the period (in ns) of a named clock from the project. Returns 0 if not found. */
/* Find an input signal name by selector. Returns NULL if not found. */
static const char *find_clock_gen_input(const IR_ClockGenUnit *unit,
                                         const char *selector)
{
    if (!unit || !selector) return NULL;
    for (int i = 0; i < unit->num_inputs; ++i) {
        const IR_ClockGenInput *inp = &unit->inputs[i];
        if (inp->selector && strcmp(inp->selector, selector) == 0) {
            return inp->signal_name;
        }
    }
    return NULL;
}

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
 * precedence (no parentheses).  Returns 1 on success, 0 on failure.
 * When chip_data and clock_gen_type are provided, unspecified parameters
 * fall back to chip data defaults (e.g., PHASESEL defaults to 0).
 */
static int eval_arith_expr(const IR_ClockGenUnit *unit,
                           const char *expr,
                           long *result,
                           const JZChipData *chip_data,
                           const char *clock_gen_type)
{
    if (!unit || !expr || !result) return 0;

    /* Tokenize into numbers/identifiers and operators */
    long values[16];
    char ops[16];
    int nvals = 0;
    int nops = 0;
    const char *p = expr;

    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;

        if (nvals > nops) {
            /* Expect an operator */
            if (*p == '+' || *p == '-' || *p == '*' || *p == '/') {
                if (nops >= 15) return 0;
                ops[nops++] = *p++;
            } else {
                return 0;
            }
        } else {
            /* Expect a value (number or identifier) */
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

    /* Apply * and / first (left to right) */
    for (int i = 0; i < nops; ) {
        if (ops[i] == '*' || ops[i] == '/') {
            if (ops[i] == '*') {
                values[i] = values[i] * values[i + 1];
            } else {
                if (values[i + 1] == 0) return 0;
                values[i] = values[i] / values[i + 1];
            }
            /* Shift remaining values and ops down */
            for (int j = i + 1; j < nvals - 1; j++) values[j] = values[j + 1];
            for (int j = i; j < nops - 1; j++) ops[j] = ops[j + 1];
            nvals--;
            nops--;
        } else {
            i++;
        }
    }

    /* Apply + and - (left to right) */
    long res = values[0];
    for (int i = 0; i < nops; i++) {
        if (ops[i] == '+') res += values[i + 1];
        else                res -= values[i + 1];
    }

    *result = res;
    return 1;
}

/* Evaluate a derived expression against config values.
 * Supports:
 *   toString(EXPR, BIN)        - binary, minimum width
 *   toString(EXPR, BIN, N)     - binary, zero-padded to N digits
 *   toString(EXPR, HEX)        - hex, minimum width
 *   toString(EXPR, HEX, N)     - hex, zero-padded to N digits
 * Where EXPR can be an arithmetic expression with identifiers (e.g., PHASESEL * 2).
 * Returns 1 on success, 0 on failure.
 */
static int eval_derived_expr(const IR_ClockGenUnit *unit,
                             const char *expr,
                             char *out_buf, size_t buf_size,
                             const JZChipData *chip_data,
                             const char *clock_gen_type)
{
    if (!unit || !expr || !out_buf || buf_size == 0) return 0;

    /* Skip leading whitespace */
    while (*expr == ' ') expr++;

    /* Check for toString( ... ) */
    if (strncmp(expr, "toString(", 9) != 0) return 0;
    const char *args_start = expr + 9;
    const char *paren_end = strchr(args_start, ')');
    if (!paren_end) return 0;

    /* Parse arguments: EXPR, FORMAT [, WIDTH] */
    size_t args_len = (size_t)(paren_end - args_start);
    if (args_len >= 128) return 0;
    char args_buf[128];
    memcpy(args_buf, args_start, args_len);
    args_buf[args_len] = '\0';

    /* Split by commas - but we need to find the comma that separates
     * the expression from the format.  The expression can contain
     * identifiers and operators but not commas, so first comma works. */
    char *arg1 = args_buf;
    char *arg2 = strchr(arg1, ',');
    if (!arg2) return 0;
    *arg2++ = '\0';
    char *arg3 = strchr(arg2, ',');
    if (arg3) *arg3++ = '\0';

    /* Trim whitespace from each arg */
    while (*arg1 == ' ') arg1++;
    { char *e = arg1 + strlen(arg1) - 1; while (e > arg1 && *e == ' ') *e-- = '\0'; }
    while (*arg2 == ' ') arg2++;
    { char *e = arg2 + strlen(arg2) - 1; while (e > arg2 && *e == ' ') *e-- = '\0'; }
    if (arg3) {
        while (*arg3 == ' ') arg3++;
        char *e = arg3 + strlen(arg3) - 1;
        while (e > arg3 && *e == ' ') *e-- = '\0';
    }

    /* Evaluate the arithmetic expression */
    long lvalue = 0;
    if (!eval_arith_expr(unit, arg1, &lvalue, chip_data, clock_gen_type)) return 0;
    unsigned long value = (unsigned long)lvalue;

    /* Parse optional width */
    int width = 0;
    if (arg3) {
        char *wend = NULL;
        long w = strtol(arg3, &wend, 10);
        if (!wend || *wend != '\0' || w <= 0 || w > 64) return 0;
        width = (int)w;
    }

    if (strcmp(arg2, "BIN") == 0) {
        /* Binary format */
        if (width == 0) {
            /* Minimum width: at least 1 digit */
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
        /* Hex format */
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

/* Emit a clock gen instantiation using the chip data template map.
 * Substitutes placeholders like %%refclk%%, %%BASE%%, %%IDIV%%, etc.
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
            /* Find the end of the placeholder */
            const char *end = strstr(p + 2, "%%");
            if (end) {
                size_t name_len = (size_t)(end - (p + 2));
                char placeholder[64];
                if (name_len < sizeof(placeholder)) {
                    memcpy(placeholder, p + 2, name_len);
                    placeholder[name_len] = '\0';

                    /* Substitute based on placeholder name */
                    if (strcmp(placeholder, "instance_idx") == 0) {
                        /* Instance index for unique naming */
                        fprintf(out, "%d_%d", cg_idx, unit_idx);
                    } else {
                        /* Try as named input (e.g., REF_CLK, CE) */
                        const char *input_sig = find_clock_gen_input(unit, placeholder);
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
                            /* Try _mhz suffix */
                            size_t plen = strlen(placeholder);
                            if (plen > 4 && strcmp(placeholder + plen - 4, "_mhz") == 0) {
                                char base_name[64];
                                size_t blen = plen - 4;
                                if (blen < sizeof(base_name)) {
                                    memcpy(base_name, placeholder, blen);
                                    base_name[blen] = '\0';
                                    const char *sig = find_clock_gen_input(unit, base_name);
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
                            /* Try _period_ns suffix */
                            if (!is_input_suffix && plen > 10 && strcmp(placeholder + plen - 10, "_period_ns") == 0) {
                                char base_name[64];
                                size_t blen = plen - 10;
                                if (blen < sizeof(base_name)) {
                                    memcpy(base_name, placeholder, blen);
                                    base_name[blen] = '\0';
                                    const char *sig = find_clock_gen_input(unit, base_name);
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
                        /* Try as config parameter (IDIV, FBDIV, ODIV, etc.) */
                        const char *cfg_val = find_clock_gen_config(unit, placeholder);
                        if (cfg_val) {
                            fputs(cfg_val, out);
                        } else {
                            /* Try as output selector (BASE, PHASE, DIV, DIV3, LOCK) */
                            const char *out_name = find_clock_gen_output(unit, placeholder);
                            if (out_name) {
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
                                    /* Try chip data parameter default */
                                    const char *param_default = NULL;
                                    if (chip_data && clock_gen_type) {
                                        param_default = jz_chip_clock_gen_param_default(
                                            chip_data, clock_gen_type, placeholder);
                                    }
                                    if (param_default) {
                                        fputs(param_default, out);
                                    } else {
                                        /* Unknown placeholder - emit placeholder name */
                                        fprintf(out, "/* %s */", placeholder);
                                    }
                                }
                            }
                        }
                        } /* is_chip_input_default */
                        } /* is_input_suffix */
                        } /* input_sig */
                    } /* instance_idx */

                    p = end + 2; /* skip past closing %% */
                    continue;
                }
            }
        }

        /* Regular character - just emit it */
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
    /* For scalar ports (bit_index == -1), also try bit 0 as a fallback.
     * Literal const bindings store top_bit_index = 0 for single-bit ports.
     */
    if (top_bit_index == -1) {
        for (int i = 0; i < proj->num_top_bindings; ++i) {
            const IR_TopBinding *tb = &proj->top_bindings[i];
            if (tb->top_port_signal_id == top_signal_id &&
                tb->top_bit_index == 0) {
                return tb;
            }
        }
    }
    return NULL;
}

static int count_unbound_bits_for_top_port(const IR_Project *proj,
                                           const IR_Signal *port)
{
    if (!proj || !port) {
        return 0;
    }

    int width = port->width;
    if (width <= 1) {
        const IR_TopBinding *tb = find_top_binding_for_bit(proj, port->id, -1);
        if (!tb || tb->pin_id < 0 || tb->pin_id >= proj->num_pins) {
            return 1;
        }
        return 0;
    }

    int count = 0;
    for (int bit = width - 1; bit >= 0; --bit) {
        const IR_TopBinding *tb = find_top_binding_for_bit(proj, port->id, bit);
        if (!tb || tb->pin_id < 0 || tb->pin_id >= proj->num_pins) {
            count++;
        }
    }
    return count;
}

static int port_has_inverted_output_binding(const IR_Project *proj,
                                            const IR_Signal *port)
{
    if (!proj || !port || !proj->top_bindings) return 0;
    int width = port->width;
    if (width <= 1) {
        const IR_TopBinding *tb = find_top_binding_for_bit(proj, port->id, -1);
        return (tb && tb->inverted != 0) ? 1 : 0;
    }
    for (int bit = 0; bit < width; ++bit) {
        const IR_TopBinding *tb = find_top_binding_for_bit(proj, port->id, bit);
        if (tb && tb->inverted != 0) return 1;
    }
    return 0;
}

/* -------------------------------------------------------------------------
 * Pin expression helpers
 * -------------------------------------------------------------------------
 */

static void emit_pin_lvalue_for_binding(FILE *out,
                                        const IR_Project *proj,
                                        const IR_TopBinding *binding)
{
    if (!out || !proj || !binding) {
        return;
    }
    if (binding->pin_id < 0 || binding->pin_id >= proj->num_pins) {
        return;
    }
    const IR_Pin *pin = &proj->pins[binding->pin_id];
    char esc_pin[256];
    const char *name = verilog_safe_name(pin->name ? pin->name : "jz_pin",
                                         esc_pin, (int)sizeof(esc_pin));
    if (binding->pin_bit_index < 0) {
        fprintf(out, "%s", name);
    } else {
        fprintf(out, "%s[%d]", name, binding->pin_bit_index);
    }
}

static void emit_top_output_port_connection_expr(FILE *out,
                                                 const IR_Project *proj,
                                                 const IR_Signal *port,
                                                 const char *dummy_name,
                                                 int dummy_width)
{
    if (!out || !proj || !port || !dummy_name) {
        fprintf(out, "1'b0");
        return;
    }

    const char *pname = port->name ? port->name : "jz_port";
    int has_inv = port_has_inverted_output_binding(proj, port);

    int width = port->width;
    if (width <= 1) {
        const IR_TopBinding *tb = find_top_binding_for_bit(proj, port->id, -1);
        if (tb && tb->pin_id >= 0 && tb->pin_id < proj->num_pins) {
            if (has_inv && tb->inverted != 0) {
                fprintf(out, "jz_inv_%s", pname);
            } else {
                emit_pin_lvalue_for_binding(out, proj, tb);
            }
        } else {
            if (dummy_width > 0) {
                fprintf(out, "%s", dummy_name);
            } else {
                fputs("1'b0", out);
            }
        }
        return;
    }

    int remaining_nc = dummy_width;
    fputc('{', out);
    for (int bit = width - 1; bit >= 0; --bit) {
        if (bit != width - 1) {
            fputs(", ", out);
        }
        const IR_TopBinding *tb = find_top_binding_for_bit(proj, port->id, bit);
        if (tb && tb->pin_id >= 0 && tb->pin_id < proj->num_pins) {
            if (has_inv && tb->inverted != 0) {
                fprintf(out, "jz_inv_%s[%d]", pname, bit);
            } else {
                emit_pin_lvalue_for_binding(out, proj, tb);
            }
        } else {
            if (remaining_nc > 0) {
                remaining_nc--;
                fprintf(out, "%s[%d]", dummy_name, remaining_nc);
            } else {
                fputs("1'b0", out);
            }
        }
    }
    fputc('}', out);
}

static void emit_pin_expr_for_binding(FILE *out,
                                      const IR_Project *proj,
                                      const IR_TopBinding *binding)
{
    if (!binding || !proj) {
        fprintf(out, "1'b0");
        return;
    }
    if (binding->pin_id < 0 || binding->pin_id >= proj->num_pins) {
        if (binding->clock_name && binding->clock_name[0] != '\0') {
            fprintf(out, "%s", binding->clock_name);
        } else if (binding->const_value != 0) {
            fprintf(out, "1'b1");
        } else {
            fprintf(out, "1'b0");
        }
        return;
    }
    const IR_Pin *pin = &proj->pins[binding->pin_id];
    char esc_pin[256];
    const char *name = verilog_safe_name(pin->name ? pin->name : "jz_pin",
                                         esc_pin, (int)sizeof(esc_pin));
    int inverted = binding->inverted != 0;
    if (binding->pin_bit_index < 0) {
        if (inverted) {
            fprintf(out, "~%s", name);
        } else {
            fprintf(out, "%s", name);
        }
    } else {
        if (inverted) {
            fprintf(out, "~%s[%d]", name, binding->pin_bit_index);
        } else {
            fprintf(out, "%s[%d]", name, binding->pin_bit_index);
        }
    }
}

static void emit_top_port_connection_expr(FILE *out,
                                          const IR_Project *proj,
                                          const IR_Signal *port)
{
    if (!proj || !port) {
        fprintf(out, "1'b0");
        return;
    }

    int width = port->width;
    if (width <= 1) {
        const IR_TopBinding *tb = find_top_binding_for_bit(proj, port->id, -1);
        emit_pin_expr_for_binding(out, proj, tb);
        return;
    }

    fputc('{', out);
    for (int bit = width - 1; bit >= 0; --bit) {
        if (bit != width - 1) {
            fputs(", ", out);
        }
        const IR_TopBinding *tb = find_top_binding_for_bit(proj, port->id, bit);
        emit_pin_expr_for_binding(out, proj, tb);
    }
    fputc('}', out);
}

/* -------------------------------------------------------------------------
 * Pin mapping helpers
 * -------------------------------------------------------------------------
 */

static const char *find_board_pin_id_for_pin_bit(const IR_Project *proj,
                                                 int pin_id,
                                                 int pin_bit_index)
{
    if (!proj || !proj->mappings || proj->num_mappings <= 0) {
        return NULL;
    }
    if (pin_id < 0 || pin_id >= proj->num_pins) {
        return NULL;
    }

    const IR_Pin *pin = &proj->pins[pin_id];
    const char *logical_name = pin->name;
    if (!logical_name || logical_name[0] == '\0') {
        return NULL;
    }

    for (int i = 0; i < proj->num_mappings; ++i) {
        const IR_PinMapping *m = &proj->mappings[i];
        if (!m->logical_pin_name || !m->board_pin_id) {
            continue;
        }
        if (strcmp(m->logical_pin_name, logical_name) != 0) {
            continue;
        }
        if ((m->bit_index < 0 && pin_bit_index < 0) ||
            (m->bit_index == pin_bit_index)) {
            return m->board_pin_id;
        }
    }
    return NULL;
}

static void emit_top_pin_mapping_comments(FILE *out,
                                          const IR_Project *proj,
                                          const IR_Module *top_mod,
                                          const char *top_mod_name)
{
    if (!out || !proj || !top_mod || !top_mod_name) {
        return;
    }
    if (!proj->top_bindings || proj->num_top_bindings <= 0) {
        return;
    }

    fputs("    // Top-level logical→physical pin mapping\n", out);

    for (int i = 0; i < top_mod->num_signals; ++i) {
        const IR_Signal *sig = &top_mod->signals[i];
        if (sig->kind != SIG_PORT) {
            continue;
        }
        const char *pname = sig->name ? sig->name : "jz_port";
        int width = sig->width;

        if (width <= 1) {
            const IR_TopBinding *tb = find_top_binding_for_bit(proj, sig->id, -1);
            if (!tb) {
                continue;
            }

            if (tb->pin_id < 0) {
                if (tb->clock_name && tb->clock_name[0] != '\0') {
                    fprintf(out, "    //   %s.%s -> %s (clock gen)\n",
                            top_mod_name, pname, tb->clock_name);
                } else {
                    fprintf(out, "    //   %s.%s -> (no connect)\n",
                            top_mod_name, pname);
                }
                continue;
            }

            const IR_Pin *pin = &proj->pins[tb->pin_id];
            const char *pin_name = pin->name ? pin->name : "jz_pin";
            const char *board_id = find_board_pin_id_for_pin_bit(proj,
                                                                 tb->pin_id,
                                                                 tb->pin_bit_index);
            if (tb->pin_bit_index < 0) {
                if (board_id) {
                    fprintf(out,
                            "    //   %s.%s -> %s (board %s)\n",
                            top_mod_name, pname, pin_name, board_id);
                } else {
                    fprintf(out,
                            "    //   %s.%s -> %s\n",
                            top_mod_name, pname, pin_name);
                }
            } else {
                if (board_id) {
                    fprintf(out,
                            "    //   %s.%s -> %s[%d] (board %s)\n",
                            top_mod_name, pname, pin_name,
                            tb->pin_bit_index, board_id);
                } else {
                    fprintf(out,
                            "    //   %s.%s -> %s[%d]\n",
                            top_mod_name, pname, pin_name,
                            tb->pin_bit_index);
                }
            }
        } else {
            for (int bit = width - 1; bit >= 0; --bit) {
                const IR_TopBinding *tb = find_top_binding_for_bit(proj, sig->id, bit);
                if (!tb) {
                    continue;
                }

                if (tb->pin_id < 0) {
                    if (tb->clock_name && tb->clock_name[0] != '\0') {
                        fprintf(out,
                                "    //   %s.%s[%d] -> %s (clock gen)\n",
                                top_mod_name, pname, bit, tb->clock_name);
                    } else {
                        fprintf(out,
                                "    //   %s.%s[%d] -> (no connect)\n",
                                top_mod_name, pname, bit);
                    }
                    continue;
                }

                const IR_Pin *pin = &proj->pins[tb->pin_id];
                const char *pin_name = pin->name ? pin->name : "jz_pin";
                const char *board_id = find_board_pin_id_for_pin_bit(proj,
                                                                     tb->pin_id,
                                                                     tb->pin_bit_index);

                if (tb->pin_bit_index < 0) {
                    if (board_id) {
                        fprintf(out,
                                "    //   %s.%s[%d] -> %s (board %s)\n",
                                top_mod_name, pname, bit, pin_name, board_id);
                    } else {
                        fprintf(out,
                                "    //   %s.%s[%d] -> %s\n",
                                top_mod_name, pname, bit, pin_name);
                    }
                } else {
                    if (board_id) {
                        fprintf(out,
                                "    //   %s.%s[%d] -> %s[%d] (board %s)\n",
                                top_mod_name, pname, bit,
                                pin_name, tb->pin_bit_index, board_id);
                    } else {
                        fprintf(out,
                                "    //   %s.%s[%d] -> %s[%d]\n",
                                top_mod_name, pname, bit,
                                pin_name, tb->pin_bit_index);
                    }
                }
            }
        }
    }

    fputc('\n', out);
}

/* -------------------------------------------------------------------------
 * Project wrapper emission
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

/**
 * Expand a differential I/O template from chip data.
 * Placeholders: %%instance%%, %%input%%, %%output%%, %%pin_p%%, %%pin_n%%,
 *               %%D0%%..%%D9%%, %%Q0%%..%%Q9%%, %%fclk%%, %%pclk%%, %%reset%%
 */
typedef struct DiffTemplateCtx {
    const char *instance;  /* Instance name suffix (e.g., "obuf_TMDS_CLK") */
    const char *input;     /* Input signal name */
    const char *output;    /* Output signal name */
    const char *pin_p;     /* P pin port name */
    const char *pin_n;     /* N pin port name */
    const char *diff_wire; /* Diff wire base name (for D0..D9 indexing) */
    int         ser_ratio; /* Serializer data width (for D0..D13 bounds check) */
    const char *fclk;      /* Fast clock name */
    const char *pclk;      /* Parallel clock name */
    const char *reset;     /* Reset signal */
    const char *shiftin1;  /* Cascade: shift wire 1 from slave */
    const char *shiftin2;  /* Cascade: shift wire 2 from slave */
    const char *shiftout1; /* Cascade: shift wire 1 to master */
    const char *shiftout2; /* Cascade: shift wire 2 to master */
    int         data_width; /* Cascade: actual serialization width (e.g., 10) */
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
                    } else if (strcmp(ph, "shiftin1") == 0 && ctx->shiftin1) {
                        fputs(ctx->shiftin1, out);
                    } else if (strcmp(ph, "shiftin2") == 0 && ctx->shiftin2) {
                        fputs(ctx->shiftin2, out);
                    } else if (strcmp(ph, "shiftout1") == 0 && ctx->shiftout1) {
                        fputs(ctx->shiftout1, out);
                    } else if (strcmp(ph, "shiftout2") == 0 && ctx->shiftout2) {
                        fputs(ctx->shiftout2, out);
                    } else if (strcmp(ph, "data_width") == 0 && ctx->data_width > 0) {
                        fprintf(out, "%d", ctx->data_width);
                    } else if ((ph[0] == 'D' || ph[0] == 'Q') && ph[1] >= '0' && ph[1] <= '9' &&
                               (ph[2] == '\0' || (ph[2] >= '0' && ph[2] <= '9' && ph[3] == '\0'))) {
                        /* D0..D9 serializer data inputs / Q0..Q9 deserializer data outputs */
                        int idx = atoi(ph + 1);
                        if (ctx->diff_wire && idx < ctx->ser_ratio) {
                            fprintf(out, "%s[%d]", ctx->diff_wire, idx);
                        } else {
                            fputs("1'b0", out);
                        }
                    } else {
                        /* Unknown placeholder - emit as-is */
                        fprintf(out, "%%%%%s%%%%", ph);
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

/* Find the data width of the top module signal bound to a pin/bit.
 * Returns 0 if no binding found. */
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
            /* Found the binding - look up the signal width */
            for (int s = 0; s < top_mod->num_signals; ++s) {
                if (top_mod->signals[s].id == tb->top_port_signal_id) {
                    return top_mod->signals[s].width;
                }
            }
        }
    }
    /* Try scalar binding (pin_bit_index == -1) */
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

/* Check if a differential pin needs a serializer.
 * A serializer is needed when at least one serialization clock is present.
 * Which clocks are required is chip-specific (validated by the semantic pass). */
static int pin_needs_serializer(const IR_Pin *pin)
{
    if (!pin) return 0;
    int has_fclk = pin->fclk_name && pin->fclk_name[0] != '\0';
    int has_pclk = pin->pclk_name && pin->pclk_name[0] != '\0';
    return has_fclk || has_pclk;
}


void emit_project_wrapper(FILE *out, const IR_Design *design,
                          JZDiagnosticList *diagnostics)
{
    if (!out || !design || !design->project) {
        return;
    }

    const IR_Project *proj = design->project;
    if (proj->top_module_id < 0 || proj->top_module_id >= design->num_modules) {
        return;
    }

    const IR_Module *top_mod = &design->modules[proj->top_module_id];
    const char *top_mod_name = (top_mod->name && top_mod->name[0] != '\0')
                             ? top_mod->name
                             : "jz_top";

    const char *wrapper_name = design_has_module_named(design, "top") ? "project_top" : "top";

    int num_ports = proj->num_pins;
    if (num_ports <= 0) {
        return;
    }

    /* Load project-level chip data (used for differential and clock gen fallback) */
    const char *proj_filename = (design->num_source_files > 0 && design->source_files[0].path)
                              ? design->source_files[0].path : NULL;
    JZChipData proj_chip_data;
    int have_proj_chip = 0;
    if (proj->chip_id && proj->chip_id[0]) {
        JZChipLoadStatus st = jz_chip_data_load(proj->chip_id, proj_filename, &proj_chip_data);
        if (st == JZ_CHIP_LOAD_OK) {
            have_proj_chip = 1;
        }
    }

    /* Emit module port list - differential pins get _p and _n suffixes */
    fprintf(out, "\nmodule %s (\n", wrapper_name);
    int first_port = 1;
    for (int i = 0; i < num_ports; ++i) {
        const IR_Pin *pin = &proj->pins[i];
        char esc_pname[256];
        const char *name = verilog_safe_name(pin->name ? pin->name : "jz_pin",
                                              esc_pname, (int)sizeof(esc_pname));
        int is_diff = pin_has_diff_mapping(proj, pin);

        if (is_diff) {
            /* Differential array pins are flattened: TMDS_DATA0_p, TMDS_DATA0_n, ...
             * so that _p/_n is always at end of the name (required by nextpnr CST parser). */
            for (int bit = 0; bit < pin->width; ++bit) {
                if (!first_port) {
                    fputs(",\n", out);
                }
                first_port = 0;
                if (pin->width > 1) {
                    fprintf(out, "    %s%d_p,\n    %s%d_n", name, bit, name, bit);
                } else {
                    fprintf(out, "    %s_p,\n    %s_n", name, name);
                }
            }
        } else {
            if (!first_port) {
                fputs(",\n", out);
            }
            first_port = 0;
            fprintf(out, "    %s", name);
        }
    }
    fputs("\n);\n", out);

    /* Emit port declarations */
    for (int i = 0; i < num_ports; ++i) {
        const IR_Pin *pin = &proj->pins[i];
        char esc_dname[256];
        const char *name = verilog_safe_name(pin->name ? pin->name : "jz_pin",
                                              esc_dname, (int)sizeof(esc_dname));
        const char *dir = "input";
        switch (pin->kind) {
            case PIN_IN:    dir = "input";  break;
            case PIN_OUT:   dir = "output"; break;
            case PIN_INOUT: dir = "inout";  break;
            default:        dir = "input";  break;
        }
        int is_diff = pin_has_diff_mapping(proj, pin);

        if (is_diff) {
            /* Differential array pins are individual scalar ports */
            for (int bit = 0; bit < pin->width; ++bit) {
                if (pin->width > 1) {
                    fprintf(out, "    %s %s%d_p;\n", dir, name, bit);
                    fprintf(out, "    %s %s%d_n;\n", dir, name, bit);
                } else {
                    fprintf(out, "    %s %s_p;\n", dir, name);
                    fprintf(out, "    %s %s_n;\n", dir, name);
                }
            }
        } else {
            fprintf(out, "    %s", dir);
            emit_width_range(out, pin->width);
            fprintf(out, " %s;\n", name);
        }
    }

    fputc('\n', out);

    emit_top_pin_mapping_comments(out, proj, top_mod, top_mod_name);

    for (int i = 0; i < top_mod->num_signals; ++i) {
        const IR_Signal *sig = &top_mod->signals[i];
        if (sig->kind != SIG_PORT) {
            continue;
        }
        if (sig->u.port.direction != PORT_OUT) {
            continue;
        }
        int nc_count = count_unbound_bits_for_top_port(proj, sig);
        if (nc_count <= 0) {
            continue;
        }
        const char *pname = sig->name ? sig->name : "jz_port";
        fputs("    wire", out);
        emit_width_range(out, nc_count);
        fprintf(out, " jz_top_%s_nc;\n", pname);
    }

    /* Emit intermediate wires for inverted output port bindings */
    for (int i = 0; i < top_mod->num_signals; ++i) {
        const IR_Signal *sig = &top_mod->signals[i];
        if (sig->kind != SIG_PORT) continue;
        if (sig->u.port.direction != PORT_OUT) continue;
        if (!port_has_inverted_output_binding(proj, sig)) continue;
        const char *pname = sig->name ? sig->name : "jz_port";
        fputs("    wire", out);
        emit_width_range(out, sig->width);
        fprintf(out, " jz_inv_%s;\n", pname);
    }

    /* Emit assign statements for inverted output port bindings */
    for (int i = 0; i < top_mod->num_signals; ++i) {
        const IR_Signal *sig = &top_mod->signals[i];
        if (sig->kind != SIG_PORT) continue;
        if (sig->u.port.direction != PORT_OUT) continue;
        if (!port_has_inverted_output_binding(proj, sig)) continue;
        const char *pname = sig->name ? sig->name : "jz_port";
        int width = sig->width;
        if (width <= 1) {
            const IR_TopBinding *tb = find_top_binding_for_bit(proj, sig->id, -1);
            if (tb && tb->pin_id >= 0 && tb->pin_id < proj->num_pins) {
                fputs("    assign ", out);
                emit_pin_lvalue_for_binding(out, proj, tb);
                fprintf(out, " = ~jz_inv_%s;\n", pname);
            }
        } else {
            for (int bit = width - 1; bit >= 0; --bit) {
                const IR_TopBinding *tb = find_top_binding_for_bit(proj, sig->id, bit);
                if (!tb || tb->pin_id < 0 || tb->pin_id >= proj->num_pins) continue;
                if (tb->inverted == 0) {
                    /* Non-inverted bit in a port that has some inverted bits:
                     * still need assign from intermediate wire to pin */
                    fputs("    assign ", out);
                    emit_pin_lvalue_for_binding(out, proj, tb);
                    fprintf(out, " = jz_inv_%s[%d];\n", pname, bit);
                } else {
                    fputs("    assign ", out);
                    emit_pin_lvalue_for_binding(out, proj, tb);
                    fprintf(out, " = ~jz_inv_%s[%d];\n", pname, bit);
                }
            }
        }
    }

    fputc('\n', out);

    for (int cg = 0; cg < proj->num_clock_gens; ++cg) {
        const IR_ClockGen *clock_gen = &proj->clock_gens[cg];
        for (int u = 0; u < clock_gen->num_units; ++u) {
            const IR_ClockGenUnit *unit = &clock_gen->units[u];
            for (int o = 0; o < unit->num_outputs; ++o) {
                const IR_ClockGenOutput *out_clk = &unit->outputs[o];
                if (out_clk->clock_name && out_clk->clock_name[0] != '\0') {
                    fprintf(out, "    wire %s;\n", out_clk->clock_name);
                }
            }
        }
    }

    for (int cg = 0; cg < proj->num_clock_gens; ++cg) {
        const IR_ClockGen *clock_gen = &proj->clock_gens[cg];

        const JZChipData *effective_chip = have_proj_chip ? &proj_chip_data : NULL;

        for (int u = 0; u < clock_gen->num_units; ++u) {
            const IR_ClockGenUnit *unit = &clock_gen->units[u];
            const char *unit_type_str = unit->type ? unit->type : "pll";
            /* Build uppercase version for comments */
            char unit_type_upper_buf[32];
            {
                size_t tl = strlen(unit_type_str);
                if (tl >= sizeof(unit_type_upper_buf)) tl = sizeof(unit_type_upper_buf) - 1;
                for (size_t ti = 0; ti < tl; ++ti)
                    unit_type_upper_buf[ti] = (char)toupper((unsigned char)unit_type_str[ti]);
                unit_type_upper_buf[tl] = '\0';
            }
            const char *unit_type_upper = unit_type_upper_buf;

            /* Try to get the template map from chip data */
            const char *template_text = NULL;
            if (effective_chip) {
                template_text = jz_chip_clock_gen_map(effective_chip, unit_type_str, "verilog-2005");
            }

            if (template_text) {
                /* Emit dummy wires for unused PLL/MMCM outputs before the instantiation */
                if (unit_type_str && strncmp(unit_type_str, "pll", 3) == 0) {
                    static const char *pll_selectors[] = {"LOCK", "BASE", "PHASE", "DIV", "DIV3", NULL};
                    for (int si = 0; pll_selectors[si]; ++si) {
                        const char *oname = find_clock_gen_output(unit, pll_selectors[si]);
                        if (!oname || oname[0] == '\0') {
                            fprintf(out, "    wire jz_unused_pll_%s_cg%d_u%d;\n",
                                    pll_selectors[si], cg, u);
                        }
                    }
                    /* Feedback path wire (from chip data feedback_wire field) */
                    const char *fb_name = effective_chip
                        ? jz_chip_clock_gen_feedback_wire(effective_chip, unit_type_str)
                        : NULL;
                    if (fb_name) {
                        fprintf(out, "    wire %s_%d_%d;\n", fb_name, cg, u);
                    }
                }
                /* Emit using the chip data template */
                fprintf(out, "\n    // CLOCK_GEN %s instantiation (from chip data)\n", unit_type_upper);
                fputs("    ", out);
                emit_clock_gen_from_template(out, proj, unit, template_text,
                                             effective_chip,
                                             unit_type_str, cg, u);
            } else {
                /* Fallback: emit as comments */
                fprintf(out, "\n    // CLOCK_GEN %s instantiation\n", unit_type_upper);
                {
                    const char *ref_name = "(none)";
                    for (int ii2 = 0; ii2 < unit->num_inputs; ++ii2) {
                        if (unit->inputs[ii2].selector && strcmp(unit->inputs[ii2].selector, "REF_CLK") == 0) {
                            ref_name = unit->inputs[ii2].signal_name ? unit->inputs[ii2].signal_name : "(none)";
                            break;
                        }
                    }
                    fprintf(out, "    // Input: %s\n", ref_name);
                }

                for (int c = 0; c < unit->num_configs; ++c) {
                    const IR_ClockGenConfig *cfg = &unit->configs[c];
                    if (cfg->param_name && cfg->param_value) {
                        fprintf(out, "    // CONFIG: %s = %s\n", cfg->param_name, cfg->param_value);
                    }
                }

                for (int o = 0; o < unit->num_outputs; ++o) {
                    const IR_ClockGenOutput *out_clk = &unit->outputs[o];
                    fprintf(out, "    // Output: %s -> %s\n",
                            out_clk->selector ? out_clk->selector : "(default)",
                            out_clk->clock_name ? out_clk->clock_name : "(unnamed)");
                }

                fprintf(out, "    // jz_%s_%d_u%d u_%s_%d (\n", unit_type_upper, cg, u, unit_type_upper, cg);
                for (int ii2 = 0; ii2 < unit->num_inputs; ++ii2) {
                    if (unit->inputs[ii2].selector && strcmp(unit->inputs[ii2].selector, "REF_CLK") == 0 &&
                        unit->inputs[ii2].signal_name) {
                        fprintf(out, "    //     .CLKIN(%s),\n", unit->inputs[ii2].signal_name);
                        break;
                    }
                }
                for (int o = 0; o < unit->num_outputs; ++o) {
                    const IR_ClockGenOutput *out_clk = &unit->outputs[o];
                    if (out_clk->clock_name && out_clk->selector) {
                        fprintf(out, "    //     .CLK%s(%s)%s\n",
                                out_clk->selector, out_clk->clock_name,
                                (o + 1 < unit->num_outputs) ? "," : "");
                    }
                }
                fprintf(out, "    // );\n");
            }
        }

    }

    fputc('\n', out);

    /* ---- Differential I/O primitives ---- */
    /* Get templates from chip data; fall back to hardcoded defaults. */
    const char *obuf_template = have_proj_chip
        ? jz_chip_diff_output_buffer_map(&proj_chip_data, "verilog-2005") : NULL;
    const char *ibuf_template = have_proj_chip
        ? jz_chip_diff_input_buffer_map(&proj_chip_data, "verilog-2005") : NULL;
    const char *clkbuf_template = have_proj_chip
        ? jz_chip_diff_clock_buffer_map(&proj_chip_data, "verilog-2005") : NULL;
    /* Serializer template is now selected per-pin based on needed data width */

    /* Hardcoded fallbacks when no chip data is available */
    static const char fallback_obuf[] =
        "TLVDS_OBUF u_%%instance%% (\n"
        "    .I(%%input%%),\n"
        "    .O(%%pin_p%%),\n"
        "    .OB(%%pin_n%%)\n"
        ");\n";
    static const char fallback_ibuf[] =
        "TLVDS_IBUF u_%%instance%% (\n"
        "    .I(%%pin_p%%),\n"
        "    .IB(%%pin_n%%),\n"
        "    .O(%%output%%)\n"
        ");\n";
    static const char fallback_oser[] =
        "OSER10 #(\n"
        "    .GSREN(\"FALSE\"),\n"
        "    .LSREN(\"TRUE\")\n"
        ") u_%%instance%% (\n"
        "    .D0(%%D0%%),\n"
        "    .D1(%%D1%%),\n"
        "    .D2(%%D2%%),\n"
        "    .D3(%%D3%%),\n"
        "    .D4(%%D4%%),\n"
        "    .D5(%%D5%%),\n"
        "    .D6(%%D6%%),\n"
        "    .D7(%%D7%%),\n"
        "    .D8(%%D8%%),\n"
        "    .D9(%%D9%%),\n"
        "    .FCLK(%%fclk%%),\n"
        "    .PCLK(%%pclk%%),\n"
        "    .RESET(%%reset%%),\n"
        "    .Q(%%output%%)\n"
        ");\n";

    /* Use fallback only when no chip data exists at all.  If chip data
     * has a differential section but no output buffer, the serializer
     * template is expected to drive the pins directly (e.g., iCE40 SB_IO
     * DDR handles both serialization and pin output). */
    int chip_has_diff_out = have_proj_chip &&
        (proj_chip_data.differential.has_output_buffer ||
         proj_chip_data.differential.has_output_serializer);
    int chip_has_diff_in = have_proj_chip &&
        (proj_chip_data.differential.has_input_buffer ||
         proj_chip_data.differential.has_input_deserializer);
    if (!obuf_template && !chip_has_diff_out) obuf_template = fallback_obuf;
    if (!ibuf_template && !chip_has_diff_in) ibuf_template = fallback_ibuf;

    for (int i = 0; i < num_ports; ++i) {
        const IR_Pin *pin = &proj->pins[i];
        if (!pin_has_diff_mapping(proj, pin)) continue;
        const char *name = pin->name ? pin->name : "jz_pin";

        if (pin->kind == PIN_OUT) {
            int has_any_ser = have_proj_chip && jz_chip_diff_serializer_ratio(&proj_chip_data) > 0;
            int needs_ser = pin_needs_serializer(pin) && has_any_ser;

            for (int bit = 0; bit < pin->width; ++bit) {
                char suffix[32];
                if (pin->width > 1) {
                    snprintf(suffix, sizeof(suffix), "%d", bit);
                } else {
                    suffix[0] = '\0';
                }

                /* Build P/N port names */
                char pin_p[128], pin_n[128];
                if (pin->width > 1) {
                    snprintf(pin_p, sizeof(pin_p), "%s%d_p", name, bit);
                    snprintf(pin_n, sizeof(pin_n), "%s%d_n", name, bit);
                } else {
                    snprintf(pin_p, sizeof(pin_p), "%s_p", name);
                    snprintf(pin_n, sizeof(pin_n), "%s_n", name);
                }

                if (needs_ser) {
                    /* Determine actual data width: prefer pin's width= attribute,
                     * then top module signal width, then chip default. */
                    int data_width = pin->ser_width > 0 ? pin->ser_width
                                   : find_signal_width_for_pin(design, proj, i, bit);
                    if (data_width <= 0) data_width = jz_chip_diff_serializer_ratio(&proj_chip_data);

                    /* Find best serializer with ratio >= data_width */
                    int sel_ratio = jz_chip_diff_best_serializer_ratio(&proj_chip_data, data_width);
                    const char *sel_template = sel_ratio > 0
                        ? jz_chip_diff_best_serializer_map(&proj_chip_data, data_width, "verilog-2005")
                        : NULL;

                    if (sel_ratio <= 0 || !sel_template) {
                        /* No serializer supports this width - ERROR */
                        if (!sel_template) sel_template = fallback_oser;
                        if (diagnostics) {
                            JZLocation loc = {0};
                            char msg[256];
                            int max_ratio = jz_chip_diff_max_serializer_ratio(&proj_chip_data);
                            snprintf(msg, sizeof(msg),
                                     "Port width %d exceeds maximum serializer ratio %d for pin %s%s",
                                     data_width, max_ratio, name, suffix);
                            jz_diagnostic_report(diagnostics, loc, JZ_SEVERITY_ERROR,
                                                 "SERIALIZER_WIDTH_EXCEEDS_RATIO", msg);
                        }
                        /* Fall back to largest available */
                        sel_ratio = jz_chip_diff_max_serializer_ratio(&proj_chip_data);
                        if (sel_ratio <= 0) sel_ratio = 10; /* ultimate fallback */
                        sel_template = jz_chip_diff_best_serializer_map(&proj_chip_data, 1, "verilog-2005");
                        if (!sel_template) sel_template = fallback_oser;
                    }

                    int wire_width = sel_ratio;

                    /* Emit INFO when selected ratio differs from data width */
                    if (diagnostics && sel_ratio != data_width) {
                        JZLocation loc = {0};
                        char msg[256];
                        snprintf(msg, sizeof(msg),
                                 "Pin %s%s: using %d:1 serializer for %d-bit data",
                                 name, suffix, sel_ratio, data_width);
                        jz_diagnostic_report(diagnostics, loc, JZ_SEVERITY_NOTE,
                                             "INFO_SERIALIZER_CASCADE", msg);
                    }

                    /* Intermediate wire from top module → serializer */
                    char diff_wire[128], ser_wire[128];
                    snprintf(diff_wire, sizeof(diff_wire), "jz_diff_%s%s", name, suffix);
                    snprintf(ser_wire, sizeof(ser_wire), "jz_ser_%s%s", name, suffix);

                    fprintf(out, "    wire [%d:0] %s;\n", wire_width - 1, diff_wire);
                    fprintf(out, "    wire %s;\n", ser_wire);

                    /* Serializer instance (template handles everything) */
                    char oser_inst[128];
                    snprintf(oser_inst, sizeof(oser_inst), "oser_%s%s", name, suffix);

                    /* Reset signal: pass through as-is (template handles polarity) */
                    const char *reset_expr = "1'b0";
                    if (pin->reset_name && pin->reset_name[0]) {
                        reset_expr = pin->reset_name;
                    }

                    DiffTemplateCtx ser_ctx = {
                        .instance   = oser_inst,
                        .input      = NULL,
                        .output     = ser_wire,
                        .pin_p      = pin_p,
                        .pin_n      = pin_n,
                        .diff_wire  = diff_wire,
                        .ser_ratio  = wire_width,
                        .fclk       = pin->fclk_name,
                        .pclk       = pin->pclk_name,
                        .reset      = reset_expr,
                        .shiftin1   = NULL,
                        .shiftin2   = NULL,
                        .shiftout1  = NULL,
                        .shiftout2  = NULL,
                        .data_width = wire_width,
                    };
                    fputs("    ", out);
                    emit_diff_from_template(out, sel_template, &ser_ctx);

                    /* Output buffer instance (skip when chip data has no obuf,
                     * meaning the serializer template drives pins directly). */
                    if (obuf_template) {
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
                        fputs("    ", out);
                        emit_diff_from_template(out, obuf_template, &buf_ctx);
                    }
                } else {
                    /* Direct diff buffer (no serializer) */
                    char diff_wire[128];
                    snprintf(diff_wire, sizeof(diff_wire), "jz_diff_%s%s", name, suffix);
                    fprintf(out, "    wire %s;\n", diff_wire);

                    if (obuf_template) {
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
                        fputs("    ", out);
                        emit_diff_from_template(out, obuf_template, &buf_ctx);
                    }
                }
            }
            fputc('\n', out);
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
                fprintf(out, "    wire %s;\n", diff_wire);

                char pin_p[128], pin_n[128];
                if (pin->width > 1) {
                    snprintf(pin_p, sizeof(pin_p), "%s%d_p", name, bit);
                    snprintf(pin_n, sizeof(pin_n), "%s%d_n", name, bit);
                } else {
                    snprintf(pin_p, sizeof(pin_p), "%s_p", name);
                    snprintf(pin_n, sizeof(pin_n), "%s_n", name);
                }

                /* Check if this differential input is a clock */
                int is_clock_pin = 0;
                for (int ci = 0; ci < proj->num_clocks; ++ci) {
                    if (proj->clocks[ci].name && pin->name &&
                        strcmp(proj->clocks[ci].name, pin->name) == 0) {
                        is_clock_pin = 1;
                        break;
                    }
                }
                const char *selected_ibuf = (is_clock_pin && clkbuf_template)
                    ? clkbuf_template : ibuf_template;

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
                fputs("    ", out);
                emit_diff_from_template(out, selected_ibuf, &buf_ctx);
            }
            fputc('\n', out);
        }
    }

    /* ---- Top module instantiation ---- */
    const char *inst_name = "u_top";
    fprintf(out, "    %s %s (\n", top_mod_name, inst_name);

    int first = 1;
    for (int i = 0; i < top_mod->num_signals; ++i) {
        const IR_Signal *sig = &top_mod->signals[i];
        if (sig->kind != SIG_PORT) {
            continue;
        }
        char pname_esc[256];
        const char *pname = verilog_safe_name(sig->name ? sig->name : "jz_port",
                                               pname_esc, (int)sizeof(pname_esc));
        if (!first) {
            fputs(",\n", out);
        }
        first = 0;

        /* Check if this port connects to a differential pin bit.
         * For a scalar port bound to a diff pin bit (e.g., tmds_d0 = TMDS_DATA[0]),
         * connect to the corresponding jz_diff wire.
         */
        int is_diff_port = 0;
        const IR_Pin *diff_pin = NULL;
        int diff_pin_bit = -1;
        {
            /* Look at all top bindings for this signal. The binding may
             * have top_bit_index=-1 even for wide ports (whole-port binding
             * to a single pin bit). Try -1 first, then 0. */
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
                /* Port connects to a specific bit of a multi-bit diff pin */
                fprintf(out, "        .%s(jz_diff_%s%d)", pname, dname, diff_pin_bit);
            } else {
                /* Scalar diff pin */
                fprintf(out, "        .%s(jz_diff_%s)", pname, dname);
            }
        } else {
            fprintf(out, "        .%s(", pname);
            if (sig->u.port.direction == PORT_OUT) {
                char dummy_name[128];
                int nc_count = count_unbound_bits_for_top_port(proj, sig);
                snprintf(dummy_name, sizeof(dummy_name), "jz_top_%s_nc", pname);
                emit_top_output_port_connection_expr(out, proj, sig, dummy_name, nc_count);
            } else {
                emit_top_port_connection_expr(out, proj, sig);
            }
            fputs(")", out);
        }
    }
    fputs("\n    );\n", out);

    fputs("endmodule\n", out);

    if (have_proj_chip) {
        jz_chip_data_free(&proj_chip_data);
    }
}
