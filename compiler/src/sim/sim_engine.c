/**
 * @file sim_engine.c
 * @brief Unified simulation engine for both @testbench and @simulation systems.
 *
 * Shared infrastructure: declaration collection, port binding, combinational
 * settling, clock domain firing, and reset application.
 *
 * System-specific: testbench uses cycle-stepped clocks with assertions;
 * simulation uses time-based auto-toggling clocks with VCD output.
 */

#include "sim_engine.h"
#include "sim_state.h"
#include "sim_eval.h"
#include "sim_exec.h"
#include "sim_value.h"
#include "../../include/ast.h"
#include "../../include/ir.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ---- Forward declarations ---- */

static int  sim_run_test(const JZASTNode *root,
                         const JZASTNode *tb_node,
                         const JZASTNode *test_node,
                         const IR_Module *dut_module,
                         const IR_Design *design,
                         uint32_t seed,
                         int verbose,
                         const char *filename);

static void sim_propagate_inputs(SimTestState *ts);
static void sim_propagate_outputs(SimTestState *ts);
static void sim_full_settle(SimTestState *ts);
static void sim_apply_domain_reset(SimContext *ctx, const IR_ClockDomain *cd);
static void sim_fire_domains_for_clock(SimTestState *ts, int clock_port_id,
                                        uint64_t new_clk_val,
                                        int apply_nba_per_domain);
static void sim_clock_advance(SimTestState *ts, const char *clock_name, int num_cycles);
static void record_failure(SimTestState *ts, const char *msg);

/* ---- Helpers ---- */

/**
 * Check if any SimContext in the DUT hierarchy has flagged a runtime error
 * (e.g., z reached a non-tristate expression).  Propagate to the test state.
 */
static void propagate_ctx_runtime_error(SimTestState *ts) {
    if (ts->runtime_error) return;
    /* Check root DUT context (child contexts share the same root) */
    if (ts->dut && ts->dut->runtime_error) {
        ts->runtime_error = 1;
        ts->num_failed++;
        record_failure(ts,
            "    RUNTIME ERROR: z reached non-tristate expression (SE-008)\n"
            "      Test aborted");
    }
}

/**
 * Settle combinational logic and check for oscillation.
 * Sets ts->runtime_error if settling does not converge (SE-001).
 */
static void sim_settle_checked(SimTestState *ts) {
    int rc = sim_settle_combinational(ts->dut, SIM_SETTLE_MAX_ITERS);
    if (rc != 0 && !ts->runtime_error) {
        ts->runtime_error = 1;
        ts->num_failed++;
        char msg[512];
        snprintf(msg, sizeof(msg),
                 "    RUNTIME ERROR at %s\n"
                 "      Combinational logic did not converge after %d delta cycles (SE-001)\n"
                 "      Possible dynamic combinational loop\n"
                 "      Test aborted",
                 ts->filename ? ts->filename : "?",
                 SIM_SETTLE_MAX_ITERS);
        record_failure(ts, msg);
    }
    /* Also check if the executor flagged z in non-tristate expression */
    propagate_ctx_runtime_error(ts);
}

static const IR_Module *find_module_by_name(const IR_Design *design, const char *name) {
    for (int i = 0; i < design->num_modules; i++) {
        if (strcmp(design->modules[i].name, name) == 0)
            return &design->modules[i];
    }
    return NULL;
}

static int find_tb_wire(SimTestState *ts, const char *name) {
    for (int i = 0; i < ts->num_tb_wires; i++) {
        if (strcmp(ts->tb_wires[i].name, name) == 0)
            return i;
    }
    return -1;
}

static int find_port_signal_id(const IR_Module *mod, const char *name) {
    for (int i = 0; i < mod->num_signals; i++) {
        if (mod->signals[i].kind == SIG_PORT &&
            strcmp(mod->signals[i].name, name) == 0)
            return mod->signals[i].id;
    }
    return -1;
}

static const JZASTNode *find_bus_def(const JZASTNode *root, const char *bus_name) {
    if (!root || !bus_name) return NULL;
    /* Search root-level children first */
    for (size_t i = 0; i < root->child_count; i++) {
        const JZASTNode *c = root->children[i];
        if (c && c->type == JZ_AST_BUS_BLOCK && c->name &&
            strcmp(c->name, bus_name) == 0)
            return c;
    }
    /* Also search inside testbench/simulation nodes */
    for (size_t i = 0; i < root->child_count; i++) {
        const JZASTNode *c = root->children[i];
        if (c && (c->type == JZ_AST_TESTBENCH || c->type == JZ_AST_SIMULATION)) {
            for (size_t j = 0; j < c->child_count; j++) {
                const JZASTNode *gc = c->children[j];
                if (gc && gc->type == JZ_AST_BUS_BLOCK && gc->name &&
                    strcmp(gc->name, bus_name) == 0)
                    return gc;
            }
        }
    }
    return NULL;
}

static int parse_width_text(const char *text) {
    if (!text) return 1;
    /* Parse width from text like "1", "8", "[8]" */
    const char *p = text;
    while (*p && (*p == '[' || *p == ' ')) p++;
    int w = (int)strtol(p, NULL, 10);
    return w > 0 ? w : 1;
}

/* Parse a sized literal string like "8'h00" into a SimValue */
static SimValue parse_literal_to_simval(const char *text) {
    if (!text) return sim_val_zero(1);

    /* Find the apostrophe */
    const char *apos = strchr(text, '\'');
    if (!apos) {
        /* Plain decimal number */
        uint64_t v = (uint64_t)strtoul(text, NULL, 10);
        return sim_val_from_uint(v, 32);
    }

    /* Parse width */
    int width = (int)strtol(text, NULL, 10);
    if (width <= 0) width = 1;
    if (width > SIM_VAL_WORDS * 64) width = SIM_VAL_WORDS * 64;

    /* Parse base */
    char base = apos[1];
    const char *digits = apos + 2;

    uint64_t words[SIM_VAL_WORDS];
    memset(words, 0, sizeof(words));
    int is_z = 0;

    switch (base) {
    case 'b': case 'B': {
        /* Two-pass: count digits, then place MSB-first into word array */
        int num_bits = 0;
        for (const char *p = digits; *p; p++) {
            if (*p == '_') continue;
            num_bits++;
        }
        int bit_idx = num_bits - 1;
        for (const char *p = digits; *p; p++) {
            if (*p == '_') continue;
            if (*p == '1' && bit_idx >= 0 && bit_idx < SIM_VAL_WORDS * 64) {
                words[bit_idx / 64] |= 1ULL << (bit_idx % 64);
            } else if (*p == 'z' || *p == 'Z') {
                is_z = 1;
            }
            bit_idx--;
        }
        break;
    }
    case 'h': case 'H': {
        /* Two-pass: count nibbles, then place MSB-first into word array */
        int num_nibs = 0;
        for (const char *p = digits; *p; p++) {
            if (*p == '_') continue;
            num_nibs++;
        }
        int nib_idx = num_nibs - 1;
        for (const char *p = digits; *p; p++) {
            if (*p == '_') continue;
            unsigned nibble = 0;
            if (*p >= '0' && *p <= '9') nibble = (unsigned)(*p - '0');
            else if (*p >= 'a' && *p <= 'f') nibble = 10u + (unsigned)(*p - 'a');
            else if (*p >= 'A' && *p <= 'F') nibble = 10u + (unsigned)(*p - 'A');
            else if (*p == 'z' || *p == 'Z') { is_z = 1; nib_idx--; continue; }
            else { nib_idx--; continue; }
            if (nib_idx >= 0) {
                int bit_base = nib_idx * 4;
                int wi = bit_base / 64;
                int bi = bit_base % 64;
                if (wi < SIM_VAL_WORDS) {
                    words[wi] |= (uint64_t)nibble << bi;
                    if (bi > 60 && wi + 1 < SIM_VAL_WORDS)
                        words[wi + 1] |= (uint64_t)nibble >> (64 - bi);
                }
            }
            nib_idx--;
        }
        break;
    }
    case 'd': case 'D':
        words[0] = (uint64_t)strtoull(digits, NULL, 10);
        break;
    default:
        break;
    }

    if (is_z) return sim_val_all_z(width);
    return sim_val_from_words(words, SIM_VAL_WORDS, width);
}

/* Evaluate an AST expression node from testbench context into a SimValue.
 * For @expect_equal, the arguments are AST nodes — identifiers or literals. */
static SimValue eval_tb_ast_expr(SimTestState *ts, const JZASTNode *node) {
    if (!node) return sim_val_all_x(1);

    if (node->type == JZ_AST_EXPR_LITERAL) {
        return parse_literal_to_simval(node->text);
    }

    if (node->type == JZ_AST_EXPR_IDENTIFIER) {
        int idx = find_tb_wire(ts, node->name);
        if (idx >= 0) return ts->tb_wires[idx].value;
        return sim_val_all_x(1);
    }

    if (node->type == JZ_AST_EXPR_BINARY) {
        if (node->child_count < 2) return sim_val_all_x(1);
        SimValue lhs = eval_tb_ast_expr(ts, node->children[0]);
        SimValue rhs = eval_tb_ast_expr(ts, node->children[1]);
        const char *op = node->block_kind;
        if (!op) return sim_val_all_x(1);

        if (strcmp(op, "EQ") == 0)        return sim_val_eq(lhs, rhs);
        if (strcmp(op, "NEQ") == 0)       return sim_val_neq(lhs, rhs);
        if (strcmp(op, "LT") == 0)        return sim_val_lt(lhs, rhs);
        if (strcmp(op, "GT") == 0)        return sim_val_gt(lhs, rhs);
        if (strcmp(op, "LTE") == 0)       return sim_val_lte(lhs, rhs);
        if (strcmp(op, "GTE") == 0)       return sim_val_gte(lhs, rhs);
        if (strcmp(op, "AND") == 0)       return sim_val_and(lhs, rhs);
        if (strcmp(op, "OR") == 0)        return sim_val_or(lhs, rhs);
        if (strcmp(op, "XOR") == 0)       return sim_val_xor(lhs, rhs);
        if (strcmp(op, "ADD") == 0)       return sim_val_add(lhs, rhs);
        if (strcmp(op, "SUB") == 0)       return sim_val_sub(lhs, rhs);
        if (strcmp(op, "MUL") == 0)       return sim_val_mul(lhs, rhs);
        if (strcmp(op, "DIV") == 0)       return sim_val_div(lhs, rhs);
        if (strcmp(op, "MOD") == 0)       return sim_val_mod(lhs, rhs);
        if (strcmp(op, "SHL") == 0)       return sim_val_shl(lhs, rhs);
        if (strcmp(op, "SHR") == 0)       return sim_val_shr(lhs, rhs);
        if (strcmp(op, "LOG_AND") == 0)   return sim_val_logical_and(lhs, rhs);
        if (strcmp(op, "LOG_OR") == 0)    return sim_val_logical_or(lhs, rhs);
        return sim_val_all_x(1);
    }

    if (node->type == JZ_AST_EXPR_UNARY) {
        if (node->child_count < 1) return sim_val_all_x(1);
        SimValue operand = eval_tb_ast_expr(ts, node->children[0]);
        const char *op = node->block_kind;
        if (!op) return sim_val_all_x(1);

        if (strcmp(op, "NOT") == 0)       return sim_val_not(operand);
        if (strcmp(op, "LOG_NOT") == 0)   return sim_val_logical_not(operand);
        if (strcmp(op, "NEG") == 0)       return sim_val_neg(operand);
        return sim_val_all_x(1);
    }

    if (node->type == JZ_AST_EXPR_SLICE) {
        if (node->child_count < 1) return sim_val_all_x(1);
        SimValue base = eval_tb_ast_expr(ts, node->children[0]);
        /* name="msb:lsb" or children[1]=msb, children[2]=lsb */
        if (node->name) {
            int msb = 0, lsb = 0;
            sscanf(node->name, "%d:%d", &msb, &lsb);
            return sim_val_slice(base, msb, lsb);
        }
        return sim_val_all_x(1);
    }

    if (node->type == JZ_AST_EXPR_CONCAT) {
        if (node->child_count == 0) return sim_val_all_x(1);
        SimValue parts[64];
        int count = (int)node->child_count;
        if (count > 64) count = 64;
        for (int i = 0; i < count; i++)
            parts[i] = eval_tb_ast_expr(ts, node->children[i]);
        return sim_val_concat(parts, count);
    }

    if (node->type == JZ_AST_EXPR_TERNARY) {
        if (node->child_count < 3) return sim_val_all_x(1);
        SimValue cond = eval_tb_ast_expr(ts, node->children[0]);
        SimValue t = eval_tb_ast_expr(ts, node->children[1]);
        SimValue f = eval_tb_ast_expr(ts, node->children[2]);
        return sim_val_ternary(cond, t, f);
    }

    /* Fallback: try text as a literal */
    if (node->text)
        return parse_literal_to_simval(node->text);

    return sim_val_all_x(1);
}

/* Record a failure message */
static void record_failure(SimTestState *ts, const char *msg) {
    if (ts->num_failure_msgs >= ts->cap_failure_msgs) {
        int new_cap = ts->cap_failure_msgs < 8 ? 8 : ts->cap_failure_msgs * 2;
        char **new_buf = realloc(ts->failure_msgs, (size_t)new_cap * sizeof(char *));
        if (!new_buf) return;
        ts->failure_msgs = new_buf;
        ts->cap_failure_msgs = new_cap;
    }
    ts->failure_msgs[ts->num_failure_msgs++] = strdup(msg);
}

/* ---- @print / @print_if execution ---- */

/**
 * Process a @print or @print_if AST node.
 *
 * Format specifiers:
 *   %h  - hex value of next arg
 *   %d  - decimal value of next arg
 *   %b  - binary value of next arg
 *   %tick - current tick/cycle count
 *   %ms   - current simulation time in milliseconds
 */
static void process_print(SimTestState *ts, const JZASTNode *node) {
    if (!node) return;

    int is_print_if = (node->type == JZ_AST_PRINT_IF);

    /* For @print_if, children[0] is the condition */
    int arg_start = 0;
    if (is_print_if) {
        if (node->child_count < 1) return;
        SimValue cond = eval_tb_ast_expr(ts, node->children[0]);
        int truth = sim_val_is_true(cond);
        if (truth != 1) return; /* skip if false or x/z */
        arg_start = 1;
    }

    const char *fmt = node->text;
    if (!fmt) return;

    int arg_idx = arg_start;
    char valbuf[512];

    for (const char *p = fmt; *p; p++) {
        if (*p == '%') {
            /* Check for %tick */
            if (strncmp(p, "%tick", 5) == 0) {
                /* In testbench mode, tick = cycle_count; in simulation, use time */
                if (ts->tick_ps > 0) {
                    uint64_t ticks = ts->current_time_ps / ts->tick_ps;
                    fprintf(stdout, "%llu", (unsigned long long)ticks);
                } else {
                    fprintf(stdout, "%llu", (unsigned long long)ts->cycle_count);
                }
                p += 4; /* skip "tick" (loop will advance past %) */
                continue;
            }
            /* Check for %ms */
            if (strncmp(p, "%ms", 3) == 0) {
                double ms = (double)ts->current_time_ps / 1e9;
                fprintf(stdout, "%.6f", ms);
                p += 2; /* skip "ms" */
                continue;
            }
            /* %h, %d, %b — consume next argument */
            if (p[1] == 'h' || p[1] == 'd' || p[1] == 'b') {
                char spec = p[1];
                p++; /* skip the specifier char */

                if (arg_idx < (int)node->child_count) {
                    SimValue val = eval_tb_ast_expr(ts, node->children[arg_idx++]);
                    switch (spec) {
                    case 'h':
                        sim_val_to_hex(val, valbuf, sizeof(valbuf));
                        break;
                    case 'd':
                        sim_val_to_dec(val, valbuf, sizeof(valbuf));
                        break;
                    case 'b':
                        sim_val_to_bin(val, valbuf, sizeof(valbuf));
                        break;
                    }
                    fputs(valbuf, stdout);
                } else {
                    /* Not enough arguments */
                    fputc('%', stdout);
                    fputc(spec, stdout);
                }
                continue;
            }
            /* Unknown % sequence, print literally */
            fputc('%', stdout);
            continue;
        }
        fputc(*p, stdout);
    }
    fputc('\n', stdout);
}

/* ---- Collect clocks and wires (shared by testbench and simulation) ---- */

static int count_bus_signals(const JZASTNode *bus_def) {
    if (!bus_def) return 0;
    int n = 0;
    for (size_t i = 0; i < bus_def->child_count; i++) {
        if (bus_def->children[i] &&
            bus_def->children[i]->type == JZ_AST_BUS_DECL)
            n++;
    }
    return n;
}

/**
 * Unified declaration collector for both @testbench and @simulation.
 * The only difference is the AST node types for clock blocks/decls:
 *   - Testbench:  JZ_AST_TB_CLOCK_BLOCK / JZ_AST_TB_CLOCK_DECL
 *   - Simulation: JZ_AST_SIM_CLOCK_BLOCK / JZ_AST_SIM_CLOCK_DECL
 * Wire blocks use JZ_AST_TB_WIRE_BLOCK / JZ_AST_TB_WIRE_DECL in both cases.
 */
static void collect_decls(SimTestState *ts, const JZASTNode *container,
                           const JZASTNode *root,
                           JZASTNodeType clock_block_type,
                           JZASTNodeType clock_decl_type) {
    /* Count clocks and wires */
    int num_clocks = 0, num_wires = 0;

    for (size_t i = 0; i < container->child_count; i++) {
        const JZASTNode *child = container->children[i];
        if (!child) continue;
        if (child->type == clock_block_type) {
            for (size_t j = 0; j < child->child_count; j++)
                if (child->children[j] &&
                    child->children[j]->type == clock_decl_type)
                    num_clocks++;
        } else if (child->type == JZ_AST_TB_WIRE_BLOCK) {
            for (size_t j = 0; j < child->child_count; j++) {
                const JZASTNode *wd = child->children[j];
                if (!wd || wd->type != JZ_AST_TB_WIRE_DECL) continue;
                if (wd->block_kind && strcmp(wd->block_kind, "BUS") == 0) {
                    int arr_count = wd->width ? (int)strtol(wd->width, NULL, 10) : 1;
                    if (arr_count <= 0) arr_count = 1;
                    const JZASTNode *bus_def = find_bus_def(root, wd->text);
                    num_wires += arr_count * count_bus_signals(bus_def);
                } else {
                    num_wires++;
                }
            }
        }
    }

    int total = num_clocks + num_wires;
    ts->num_tb_wires = total;
    ts->tb_wires = calloc((size_t)(total > 0 ? total : 1), sizeof(SimTbWire));
    int idx = 0;

    /* Clocks first */
    for (size_t i = 0; i < container->child_count; i++) {
        const JZASTNode *child = container->children[i];
        if (!child || child->type != clock_block_type) continue;
        for (size_t j = 0; j < child->child_count; j++) {
            const JZASTNode *decl = child->children[j];
            if (!decl || decl->type != clock_decl_type) continue;
            ts->tb_wires[idx].name = decl->name;
            ts->tb_wires[idx].width = 1;
            ts->tb_wires[idx].is_clock = 1;
            ts->tb_wires[idx].value = sim_val_zero(1);
            idx++;
        }
    }

    /* Wires */
    for (size_t i = 0; i < container->child_count; i++) {
        const JZASTNode *child = container->children[i];
        if (!child || child->type != JZ_AST_TB_WIRE_BLOCK) continue;
        for (size_t j = 0; j < child->child_count; j++) {
            const JZASTNode *decl = child->children[j];
            if (!decl || decl->type != JZ_AST_TB_WIRE_DECL) continue;

            if (decl->block_kind && strcmp(decl->block_kind, "BUS") == 0) {
                int arr_count = decl->width ? (int)strtol(decl->width, NULL, 10) : 1;
                if (arr_count <= 0) arr_count = 1;
                const JZASTNode *bus_def = find_bus_def(root, decl->text);
                if (!bus_def) continue;

                for (int ai = 0; ai < arr_count; ai++) {
                    for (size_t bi = 0; bi < bus_def->child_count; bi++) {
                        const JZASTNode *bd = bus_def->children[bi];
                        if (!bd || bd->type != JZ_AST_BUS_DECL) continue;
                        int w = parse_width_text(bd->width);

                        char namebuf[128];
                        if (arr_count > 1)
                            snprintf(namebuf, sizeof(namebuf), "%s%d_%s",
                                     decl->name, ai, bd->name);
                        else
                            snprintf(namebuf, sizeof(namebuf), "%s_%s",
                                     decl->name, bd->name);

                        ts->tb_wires[idx].name = strdup(namebuf);
                        ts->tb_wires[idx].width = w;
                        ts->tb_wires[idx].is_clock = 0;
                        ts->tb_wires[idx].owns_name = 1;
                        ts->tb_wires[idx].value = sim_val_zero(w);
                        idx++;
                    }
                }
            } else {
                int w = parse_width_text(decl->width);
                ts->tb_wires[idx].name = decl->name;
                ts->tb_wires[idx].width = w;
                ts->tb_wires[idx].is_clock = 0;
                ts->tb_wires[idx].value = sim_val_zero(w);
                idx++;
            }
        }
    }
    ts->num_tb_wires = idx; /* actual count after expansion */
}

/* ---- Build port bindings from @new ---- */

static void build_port_bindings(SimTestState *ts,
                                const JZASTNode *instance_node,
                                const IR_Module *dut_module) {
    /* First pass: count how many bindings we'll need (BUS expands to many) */
    int n = 0;
    for (size_t i = 0; i < instance_node->child_count; i++) {
        const JZASTNode *pd = instance_node->children[i];
        if (!pd || pd->type != JZ_AST_PORT_DECL) continue;
        if (pd->block_kind && strcmp(pd->block_kind, "BUS") == 0) {
            /* Count DUT port signals matching this prefix */
            const char *port_prefix = pd->name;
            size_t plen = strlen(port_prefix);
            for (int s = 0; s < dut_module->num_signals; s++) {
                if (dut_module->signals[s].kind != SIG_PORT) continue;
                const char *sn = dut_module->signals[s].name;
                if (strncmp(sn, port_prefix, plen) == 0 &&
                    (sn[plen] == '_' || (sn[plen] >= '0' && sn[plen] <= '9')))
                    n++;
            }
        } else {
            n++;
        }
    }

    ts->bindings = calloc((size_t)(n > 0 ? n : 1), sizeof(SimPortBinding));
    ts->num_bindings = 0;

    for (size_t i = 0; i < instance_node->child_count; i++) {
        const JZASTNode *pd = instance_node->children[i];
        if (!pd || pd->type != JZ_AST_PORT_DECL) continue;

        if (pd->block_kind && strcmp(pd->block_kind, "BUS") == 0) {
            /* BUS port binding: expand by matching DUT port prefix to tb wire prefix */
            const char *port_prefix = pd->name;
            size_t plen = strlen(port_prefix);

            const char *wire_prefix = NULL;
            if (pd->child_count > 0 && pd->children[0] &&
                pd->children[0]->type == JZ_AST_EXPR_IDENTIFIER)
                wire_prefix = pd->children[0]->name;
            if (!wire_prefix) continue;

            for (int s = 0; s < dut_module->num_signals; s++) {
                if (dut_module->signals[s].kind != SIG_PORT) continue;
                const char *sn = dut_module->signals[s].name;
                if (strncmp(sn, port_prefix, plen) != 0) continue;
                /* Must be followed by digit or underscore */
                if (sn[plen] != '_' && !(sn[plen] >= '0' && sn[plen] <= '9'))
                    continue;

                /* Compute wire name by replacing port_prefix with wire_prefix */
                char wire_name[128];
                snprintf(wire_name, sizeof(wire_name), "%s%s", wire_prefix, sn + plen);

                int sig_id = dut_module->signals[s].id;
                int wi = find_tb_wire(ts, wire_name);
                if (wi < 0) continue;

                ts->bindings[ts->num_bindings].port_signal_id = sig_id;
                ts->bindings[ts->num_bindings].tb_wire_index = wi;
                ts->num_bindings++;
            }
            continue;
        }

        const char *port_name = pd->name;
        int sig_id = find_port_signal_id(dut_module, port_name);
        if (sig_id < 0) continue;

        /* RHS is the tb wire name (children[0] is EXPR_IDENTIFIER) */
        const char *wire_name = NULL;
        if (pd->child_count > 0 && pd->children[0] &&
            pd->children[0]->type == JZ_AST_EXPR_IDENTIFIER) {
            wire_name = pd->children[0]->name;
        }
        if (!wire_name) continue;

        int wi = find_tb_wire(ts, wire_name);
        if (wi < 0) continue;

        ts->bindings[ts->num_bindings].port_signal_id = sig_id;
        ts->bindings[ts->num_bindings].tb_wire_index = wi;
        ts->num_bindings++;
    }
}

/* ---- Input / output propagation ---- */

static void sim_propagate_inputs(SimTestState *ts) {
    for (int i = 0; i < ts->num_bindings; i++) {
        SimPortBinding *b = &ts->bindings[i];
        const IR_Signal *sig = NULL;

        /* Find the signal in the DUT module */
        for (int j = 0; j < ts->dut->module->num_signals; j++) {
            if (ts->dut->module->signals[j].id == b->port_signal_id) {
                sig = &ts->dut->module->signals[j];
                break;
            }
        }
        if (!sig) continue;

        if (sig->kind == SIG_PORT &&
            (sig->u.port.direction == PORT_IN || sig->u.port.direction == PORT_INOUT)) {
            /* Copy tb wire value -> DUT input */
            SimSignalEntry *entry = sim_ctx_lookup(ts->dut, b->port_signal_id);
            if (entry) {
                entry->current = ts->tb_wires[b->tb_wire_index].value;
                /* Match widths */
                if (entry->current.width != sig->width) {
                    entry->current.width = sig->width;
                    entry->current = sim_val_mask(entry->current);
                }
            }
        }
    }
}

static void sim_propagate_outputs(SimTestState *ts) {
    for (int i = 0; i < ts->num_bindings; i++) {
        SimPortBinding *b = &ts->bindings[i];
        const IR_Signal *sig = NULL;

        for (int j = 0; j < ts->dut->module->num_signals; j++) {
            if (ts->dut->module->signals[j].id == b->port_signal_id) {
                sig = &ts->dut->module->signals[j];
                break;
            }
        }
        if (!sig) continue;

        if (sig->kind == SIG_PORT &&
            (sig->u.port.direction == PORT_OUT || sig->u.port.direction == PORT_INOUT)) {
            /* Copy DUT output -> tb wire */
            SimSignalEntry *entry = sim_ctx_lookup(ts->dut, b->port_signal_id);
            if (entry) {
                /* For INOUT ports: if DUT drives z, preserve the testbench
                   value (the testbench is the other driver on the bus).
                   This implements simple z-resolution: z loses to any
                   real value from the other side. */
                if (sig->u.port.direction == PORT_INOUT &&
                    sim_val_is_all_z(entry->current)) {
                    continue;
                }
                ts->tb_wires[b->tb_wire_index].value = entry->current;
            }
        }
    }
}

/* ---- INOUT z-resolution ---- */

/**
 * For INOUT ports, if the DUT drives z (high-impedance), restore the
 * testbench wire value.  This implements simple tri-state resolution:
 * z loses to any real value from the other side of the bus.
 */
static void sim_resolve_inout_z(SimTestState *ts) {
    for (int i = 0; i < ts->num_bindings; i++) {
        SimPortBinding *b = &ts->bindings[i];
        const IR_Signal *sig = NULL;

        for (int j = 0; j < ts->dut->module->num_signals; j++) {
            if (ts->dut->module->signals[j].id == b->port_signal_id) {
                sig = &ts->dut->module->signals[j];
                break;
            }
        }
        if (!sig) continue;

        if (sig->kind == SIG_PORT && sig->u.port.direction == PORT_INOUT) {
            SimSignalEntry *entry = sim_ctx_lookup(ts->dut, b->port_signal_id);
            if (entry && sim_val_is_all_z(entry->current)) {
                /* DUT drives z -> restore testbench value */
                entry->current = ts->tb_wires[b->tb_wire_index].value;
                if (entry->current.width != sig->width) {
                    entry->current.width = sig->width;
                    entry->current = sim_val_mask(entry->current);
                }
            }
        }
    }
}

/* ---- Full settle: propagate inputs, settle, resolve inout, propagate outputs ---- */

static void sim_full_settle(SimTestState *ts) {
    sim_propagate_inputs(ts);
    sim_settle_checked(ts);
    sim_resolve_inout_z(ts);
    sim_propagate_outputs(ts);
}

/* ---- Apply reset values for a clock domain ---- */

static void sim_apply_domain_reset(SimContext *ctx, const IR_ClockDomain *cd) {
    for (int r = 0; r < cd->num_registers; r++) {
        int reg_id = cd->register_ids[r];
        SimSignalEntry *re = sim_ctx_lookup(ctx, reg_id);
        if (!re) continue;

        for (int s = 0; s < ctx->module->num_signals; s++) {
            if (ctx->module->signals[s].id == reg_id) {
                const IR_Signal *sig = &ctx->module->signals[s];
                if (sig->kind == SIG_REGISTER) {
                    SimValue rv = sim_val_from_words(
                        sig->u.reg.reset_value.words,
                        IR_LIT_WORDS,
                        sig->u.reg.reset_value.width);
                    if (rv.width != sig->width)
                        rv = sim_val_zext(rv, sig->width);
                    re->current = rv;
                }
                break;
            }
        }
    }
}

/* ---- Fire clock domains for a given clock edge ---- */

/**
 * Check all clock domains against a clock edge and fire matching ones.
 * @param ts                Test state with DUT context.
 * @param clock_port_id     IR signal ID of the clock port (-1 for unknown).
 * @param new_clk_val       Current clock value after toggle (0 or 1).
 * @param apply_nba_per_domain  If true, apply NBA after each domain (testbench semantics).
 *                              If false, caller must apply NBA after all domains.
 */
static void sim_fire_domains_for_clock(SimTestState *ts, int clock_port_id,
                                        uint64_t new_clk_val,
                                        int apply_nba_per_domain) {
    for (int d = 0; d < ts->dut->module->num_clock_domains; d++) {
        const IR_ClockDomain *cd = &ts->dut->module->clock_domains[d];

        /* Check if this domain's clock matches */
        if (clock_port_id >= 0 && cd->clock_signal_id != clock_port_id)
            continue;

        /* Check edge */
        int is_active_edge = 0;
        if (cd->edge == EDGE_RISING && new_clk_val == 1)
            is_active_edge = 1;
        else if (cd->edge == EDGE_FALLING && new_clk_val == 0)
            is_active_edge = 1;
        else if (cd->edge == EDGE_BOTH)
            is_active_edge = 1;

        if (!is_active_edge) continue;

        /* Check reset */
        int reset_active = 0;
        if (cd->reset_signal_id >= 0) {
            SimSignalEntry *rst_entry = sim_ctx_lookup(ts->dut, cd->reset_signal_id);
            if (rst_entry) {
                uint64_t rst_val = rst_entry->current.val[0] & 1;
                if (cd->reset_active == RESET_ACTIVE_HIGH && rst_val == 1)
                    reset_active = 1;
                else if (cd->reset_active == RESET_ACTIVE_LOW && rst_val == 0)
                    reset_active = 1;
            }
        }

        if (reset_active) {
            sim_apply_domain_reset(ts->dut, cd);
        }

        /* Execute synchronous domain logic */
        sim_exec_sync_domain_with_children(ts->dut, d, reset_active);

        if (apply_nba_per_domain) {
            sim_ctx_apply_nba(ts->dut);
        }
    }
}

/* ---- Clock advancement ---- */

static void sim_clock_advance(SimTestState *ts, const char *clock_name, int num_cycles) {
    int clock_idx = find_tb_wire(ts, clock_name);
    if (clock_idx < 0) return;

    /* Find the clock port signal in DUT */
    int clock_port_id = -1;
    for (int i = 0; i < ts->num_bindings; i++) {
        if (ts->bindings[i].tb_wire_index == clock_idx) {
            clock_port_id = ts->bindings[i].port_signal_id;
            break;
        }
    }

    for (int i = 0; i < 2 * num_cycles; i++) {
        /* Toggle clock */
        uint64_t cur = ts->tb_wires[clock_idx].value.val[0] & 1;
        ts->tb_wires[clock_idx].value = sim_val_from_uint(cur ^ 1, 1);

        /* Propagate inputs and settle */
        sim_propagate_inputs(ts);
        sim_settle_checked(ts);
        sim_resolve_inout_z(ts);

        /* Fire matching clock domains (NBA applied per-domain for testbench) */
        uint64_t new_clk = ts->tb_wires[clock_idx].value.val[0] & 1;
        sim_fire_domains_for_clock(ts, clock_port_id, new_clk, /*apply_nba_per_domain=*/1);

        /* Settle after sync updates and propagate outputs */
        sim_settle_checked(ts);
        sim_resolve_inout_z(ts);
        sim_propagate_outputs(ts);
    }

    ts->cycle_count += (uint64_t)num_cycles;
}

/* ---- Process @setup / @update ---- */

static void process_setup_update(SimTestState *ts, const JZASTNode *node,
                                  int is_setup) {
    if (!node) return;

    /* Simultaneous assignment semantics: evaluate ALL RHS expressions using
     * pre-update wire values, then apply all LHS targets atomically.
     * This ensures `a <= a+1; b <= a;` gives b the OLD value of a. */

    /* Pass 1: evaluate all RHS expressions (snapshot pre-update state) */
    int num_assigns = 0;
    int wire_indices[256];
    SimValue rhs_values[256];
    const char *wire_names[256];

    for (size_t i = 0; i < node->child_count && num_assigns < 256; i++) {
        const JZASTNode *stmt = node->children[i];
        if (!stmt || stmt->type != JZ_AST_STMT_ASSIGN) continue;
        if (stmt->child_count < 2) continue;

        const JZASTNode *lhs = stmt->children[0];
        const JZASTNode *rhs = stmt->children[1];
        if (!lhs || lhs->type != JZ_AST_EXPR_IDENTIFIER) continue;

        int wi = find_tb_wire(ts, lhs->name);
        if (wi < 0) continue;

        SimValue val = eval_tb_ast_expr(ts, rhs);
        val.width = ts->tb_wires[wi].width;
        val = sim_val_mask(val);

        wire_indices[num_assigns] = wi;
        rhs_values[num_assigns] = val;
        wire_names[num_assigns] = lhs->name;
        num_assigns++;
    }

    /* Pass 2: apply all evaluated values atomically */
    for (int i = 0; i < num_assigns; i++) {
        ts->tb_wires[wire_indices[i]].value = rhs_values[i];

        if (ts->verbose) {
            char vbuf[80];
            sim_val_format_literal(rhs_values[i], vbuf, sizeof(vbuf));
            fprintf(stdout, "    %s <%s> %s\n",
                    is_setup ? "Setup" : "Update",
                    wire_names[i], vbuf);
        }
    }

    /* After updating tb wires, propagate to DUT and settle */
    sim_propagate_inputs(ts);
    sim_settle_checked(ts);
    sim_resolve_inout_z(ts);
    sim_propagate_outputs(ts);
}

/* ---- Process @expect_equal / @expect_not_equal ---- */

static void process_expect(SimTestState *ts, const JZASTNode *node, int is_equal) {
    ts->num_expects++;

    if (node->child_count < 2) {
        ts->num_failed++;
        record_failure(ts, "  (malformed expect)");
        return;
    }

    const JZASTNode *sig_node = node->children[0];
    const JZASTNode *val_node = node->children[1];

    SimValue actual = eval_tb_ast_expr(ts, sig_node);
    SimValue expected = eval_tb_ast_expr(ts, val_node);

    /* Runtime error: z observed in assertion (spec Section 2.5.4).
     * x is not a runtime value, but the simulator tracks it internally
     * to detect z propagation through expressions.  Treat any x/z as
     * a z-originated runtime error. */
    if (sim_val_has_xz(actual)) {
        ts->num_failed++;
        ts->runtime_error = 1;

        /* Build z bit-position string */
        int cw = actual.width;
        char actual_str[80];
        sim_val_format_literal(actual, actual_str, sizeof(actual_str));

        /* Find z bit range for reporting */
        int lo = -1, hi = -1;
        for (int b = 0; b < cw; b++) {
            int wi = b / 64;
            int bi = b % 64;
            if (((actual.zmask[wi] | actual.xmask[wi]) >> bi) & 1) {
                if (lo < 0) lo = b;
                hi = b;
            }
        }

        char msg[512];
        snprintf(msg, sizeof(msg),
                 "    RUNTIME ERROR at %s:%d @expect_%s(%s, ...)\n"
                 "      z observed in signal\n"
                 "      Value:  %s\n"
                 "      Bits [%d:%d] are z\n"
                 "      Test aborted",
                 ts->filename ? ts->filename : "?",
                 node->loc.line,
                 is_equal ? "equal" : "not_equal",
                 sig_node->name ? sig_node->name : "?",
                 actual_str,
                 hi, lo);
        record_failure(ts, msg);
        return;
    }

    /* Compare using multi-word-aware sim_val_eq/neq */
    int pass;
    if (is_equal)
        pass = sim_val_is_true(sim_val_eq(actual, expected)) == 1;
    else
        pass = sim_val_is_true(sim_val_neq(actual, expected)) == 1;

    if (pass) {
        ts->num_passed++;
        if (ts->verbose) {
            char expected_str[32];
            sim_val_format_literal(expected, expected_str, sizeof(expected_str));
            fprintf(stdout, "    Expect %s <%s> %s PASS\n",
                    is_equal ? "Equal" : "Not Equal",
                    sig_node->name ? sig_node->name : "?",
                    expected_str);
        }
    } else {
        ts->num_failed++;

        char actual_str[80], expected_str[80];
        SimValue afmt = actual;
        afmt.width = expected.width;
        sim_val_format_literal(afmt, actual_str, sizeof(actual_str));
        sim_val_format_literal(expected, expected_str, sizeof(expected_str));

        if (ts->verbose) {
            fprintf(stdout, "    Expect %s <%s> %s FAIL\n",
                    is_equal ? "Equal" : "Not Equal",
                    sig_node->name ? sig_node->name : "?",
                    expected_str);
        }

        char msg[512];
        snprintf(msg, sizeof(msg),
                 "    FAIL at %s:%d @expect_%s(%s, %s)\n"
                 "      Expected: %s\n"
                 "      Actual:   %s",
                 ts->filename ? ts->filename : "?",
                 node->loc.line,
                 is_equal ? "equal" : "not_equal",
                 sig_node->name ? sig_node->name : "?",
                 val_node->text ? val_node->text : "?",
                 expected_str,
                 actual_str);
        record_failure(ts, msg);
    }
}

/* ---- Process @expect_tristate ---- */

static void process_expect_tristate(SimTestState *ts, const JZASTNode *node) {
    ts->num_expects++;

    if (node->child_count < 1) {
        ts->num_failed++;
        record_failure(ts, "  (malformed expect_tristate)");
        return;
    }

    const JZASTNode *sig_node = node->children[0];
    SimValue actual = eval_tb_ast_expr(ts, sig_node);

    int pass = sim_val_is_all_z(actual);

    if (pass) {
        ts->num_passed++;
        if (ts->verbose) {
            fprintf(stdout, "    Expect Tristate <%s> PASS\n",
                    sig_node->name ? sig_node->name : "?");
        }
    } else {
        ts->num_failed++;

        char actual_str[32];
        sim_val_format_literal(actual, actual_str, sizeof(actual_str));

        if (ts->verbose) {
            fprintf(stdout, "    Expect Tristate <%s> FAIL\n",
                    sig_node->name ? sig_node->name : "?");
        }

        char msg[512];
        snprintf(msg, sizeof(msg),
                 "    FAIL at %s:%d @expect_tristate(%s)\n"
                 "      Expected: all z\n"
                 "      Actual:   %s",
                 ts->filename ? ts->filename : "?",
                 node->loc.line,
                 sig_node->name ? sig_node->name : "?",
                 actual_str);
        record_failure(ts, msg);
    }
}

/* ---- Run a single test ---- */

static int sim_run_test(const JZASTNode *root,
                        const JZASTNode *tb_node,
                        const JZASTNode *test_node,
                        const IR_Module *dut_module,
                        const IR_Design *design,
                        uint32_t seed,
                        int verbose,
                        const char *filename) {
    SimTestState ts;
    memset(&ts, 0, sizeof(ts));
    ts.verbose = verbose;
    ts.filename = filename;
    ts.test_passed = 1;

    /* Collect clocks and wires from the testbench level */
    collect_decls(&ts, tb_node, root, JZ_AST_TB_CLOCK_BLOCK, JZ_AST_TB_CLOCK_DECL);

    /* Find @new instance in this test */
    const JZASTNode *instance_node = NULL;
    for (size_t i = 0; i < test_node->child_count; i++) {
        if (test_node->children[i] &&
            test_node->children[i]->type == JZ_AST_MODULE_INSTANCE) {
            instance_node = test_node->children[i];
            break;
        }
    }

    if (!instance_node) {
        fprintf(stderr, "  ERROR: no @new instance in test \"%s\"\n",
                test_node->text ? test_node->text : "?");
        for (int fi = 0; fi < ts.num_tb_wires; fi++)
            if (ts.tb_wires[fi].owns_name) free((char *)ts.tb_wires[fi].name);
        free(ts.tb_wires);
        return 1;
    }

    /* Create DUT context */
    ts.dut = sim_ctx_create(dut_module, design, seed);
    if (!ts.dut) {
        fprintf(stderr, "  ERROR: failed to create simulation context\n");
        for (int fi = 0; fi < ts.num_tb_wires; fi++)
            if (ts.tb_wires[fi].owns_name) free((char *)ts.tb_wires[fi].name);
        free(ts.tb_wires);
        return 1;
    }

    /* Build port bindings */
    build_port_bindings(&ts, instance_node, dut_module);

    /* Initial propagation */
    sim_full_settle(&ts);

    if (verbose) {
        fprintf(stdout, "  --- \"%s\" ---\n",
                test_node->text ? test_node->text : "?");
    }

    /* Walk test children sequentially */
    for (size_t i = 0; i < test_node->child_count; i++) {
        const JZASTNode *child = test_node->children[i];
        if (!child) continue;

        /* Abort test on runtime error (z observed in assertion) */
        if (ts.runtime_error) break;

        switch (child->type) {
        case JZ_AST_TB_SETUP:
            process_setup_update(&ts, child, 1);
            break;
        case JZ_AST_TB_UPDATE:
            process_setup_update(&ts, child, 0);
            break;

        case JZ_AST_TB_CLOCK_ADV: {
            const char *clk_name = child->name;
            int cycles = 1;
            if (child->text) {
                cycles = (int)strtol(child->text, NULL, 10);
                if (cycles <= 0) cycles = 1;
            }
            if (verbose) {
                fprintf(stdout, "    Clock <%s> Cycle %d\n", clk_name, cycles);
            }
            sim_clock_advance(&ts, clk_name, cycles);
            propagate_ctx_runtime_error(&ts);
            break;
        }

        case JZ_AST_TB_EXPECT_EQ:
            process_expect(&ts, child, 1);
            break;

        case JZ_AST_TB_EXPECT_NEQ:
            process_expect(&ts, child, 0);
            break;

        case JZ_AST_TB_EXPECT_TRI:
            process_expect_tristate(&ts, child);
            break;

        case JZ_AST_PRINT:
        case JZ_AST_PRINT_IF:
            process_print(&ts, child);
            break;

        default:
            break;
        }
    }

    /* Determine pass/fail */
    int test_ok = (ts.num_failed == 0);

    /* Print result */
    if (test_ok) {
        fprintf(stdout, "  PASS  \"%s\"%*s(%d/%d assertions)\n",
                test_node->text ? test_node->text : "?",
                1, " ",
                ts.num_passed, ts.num_expects);
    } else if (ts.runtime_error) {
        fprintf(stdout, "  ERROR \"%s\"%*s(runtime error: z observed)\n",
                test_node->text ? test_node->text : "?",
                1, " ");
        for (int i = 0; i < ts.num_failure_msgs; i++) {
            fprintf(stdout, "%s\n", ts.failure_msgs[i]);
        }
    } else {
        fprintf(stdout, "  FAIL  \"%s\"%*s(%d/%d assertions)\n",
                test_node->text ? test_node->text : "?",
                1, " ",
                ts.num_passed, ts.num_expects);
        for (int i = 0; i < ts.num_failure_msgs; i++) {
            fprintf(stdout, "%s\n", ts.failure_msgs[i]);
        }
    }

    /* Cleanup */
    sim_ctx_destroy(ts.dut);
    free(ts.bindings);
    for (int i = 0; i < ts.num_tb_wires; i++) {
        if (ts.tb_wires[i].owns_name)
            free((char *)ts.tb_wires[i].name);
    }
    free(ts.tb_wires);
    for (int i = 0; i < ts.num_failure_msgs; i++)
        free(ts.failure_msgs[i]);
    free(ts.failure_msgs);

    return test_ok ? 0 : 1;
}

/* ---- Public API ---- */

int jz_sim_run_testbenches(const JZASTNode *root,
                           const IR_Design *design,
                           uint32_t seed,
                           int verbose,
                           JZDiagnosticList *diagnostics,
                           const char *filename) {
    (void)diagnostics; /* unused for now */

    if (!root || !design) return 1;

    int total_tests = 0;
    int total_passed = 0;
    int total_failed = 0;
    int any_tb_found = 0;

    for (size_t i = 0; i < root->child_count; i++) {
        const JZASTNode *child = root->children[i];
        if (!child || child->type != JZ_AST_TESTBENCH) continue;

        any_tb_found = 1;
        const char *module_name = child->name;

        /* Find the DUT module in the IR */
        const IR_Module *dut_module = find_module_by_name(design, module_name);
        if (!dut_module) {
            fprintf(stderr, "error: cannot find module '%s' for testbench\n",
                    module_name ? module_name : "?");
            continue;
        }

        fprintf(stdout, "\n=== @testbench %s ===\n", module_name);

        /* Run each TEST in this testbench */
        int test_index = 0;
        for (size_t j = 0; j < child->child_count; j++) {
            const JZASTNode *test = child->children[j];
            if (!test || test->type != JZ_AST_TB_TEST) continue;

            if (test_index > 0)
                fprintf(stdout, "\n");
            test_index++;
            total_tests++;
            int rc = sim_run_test(root, child, test, dut_module, design,
                                   seed, verbose, filename);
            if (rc == 0)
                total_passed++;
            else
                total_failed++;
        }
    }

    if (!any_tb_found) {
        fprintf(stdout, "\nNo testbenches found.\n");
        return 0;
    }

    /* Print summary */
    fprintf(stdout, "\nResults: %d passed, %d failed, %d total\n",
            total_passed, total_failed, total_tests);
    fprintf(stdout, "Seed: 0x%08x\n", seed);

    return total_failed > 0 ? 1 : 0;
}

/* ==================================================================
 * @simulation support — time-based simulation with VCD output
 * ================================================================== */

#include "sim_vcd.h"
#include <math.h>

/* GCD for uint64_t */
static uint64_t gcd64(uint64_t a, uint64_t b) {
    while (b) {
        uint64_t t = b;
        b = a % b;
        a = t;
    }
    return a;
}

/**
 * Convert a time value to exact integer picoseconds.
 * @param value  The numeric value (e.g., 10.0 for 10ns).
 * @param scale  Multiplier to picoseconds (1000.0 for ns, 1000000.0 for ms).
 * @param ps_out Output: the integer picosecond count.
 * @return 0 on success, -1 if the result is not an exact integer.
 */
static int time_to_exact_ps(double value, double scale, uint64_t *ps_out) {
    double ps = value * scale;
    double rounded = floor(ps + 0.5);
    /* Check that the conversion is exact (within fp tolerance) */
    if (fabs(ps - rounded) > 1e-6) {
        *ps_out = 0;
        return -1;
    }
    *ps_out = (uint64_t)rounded;
    return 0;
}

/**
 * @brief Collect simulation clocks from a SIM_CLOCK_BLOCK.
 *
 * Fills parallel arrays of clock name, tb wire index, and toggle interval (in ps).
 */
typedef struct SimClock {
    const char *name;
    int         tb_wire_idx;
    uint64_t    toggle_ps;  /* half period in picoseconds */
} SimClock;

static int collect_sim_clocks(const JZASTNode *sim_node, SimTestState *ts,
                               SimClock *clocks, int max_clocks) {
    int n = 0;
    for (size_t i = 0; i < sim_node->child_count && n < max_clocks; i++) {
        const JZASTNode *child = sim_node->children[i];
        if (!child || child->type != JZ_AST_SIM_CLOCK_BLOCK) continue;

        for (size_t j = 0; j < child->child_count && n < max_clocks; j++) {
            const JZASTNode *cd = child->children[j];
            if (!cd || cd->type != JZ_AST_SIM_CLOCK_DECL) continue;

            /* period is in ns (from text field) */
            double period_ns = cd->text ? atof(cd->text) : 10.0;
            if (period_ns <= 0.0) period_ns = 10.0;

            /* Convert to exact integer ps; reject fractional picoseconds */
            uint64_t period_ps;
            if (time_to_exact_ps(period_ns, 1000.0, &period_ps) != 0) {
                fprintf(stderr, "ERROR: Clock '%s' period %.6g ns does not "
                        "convert to an exact integer number of picoseconds\n",
                        cd->name ? cd->name : "?", period_ns);
                return -1;
            }
            uint64_t toggle_ps = period_ps / 2;
            if (toggle_ps == 0) toggle_ps = 1;

            int wi = find_tb_wire(ts, cd->name);

            clocks[n].name = cd->name;
            clocks[n].tb_wire_idx = wi;
            clocks[n].toggle_ps = toggle_ps;
            n++;
        }
    }
    return n;
}

/**
 * @brief Dump all tracked signals to VCD.
 */
/* ---- TAP signal tracking ---- */

typedef struct SimTap {
    int          vcd_id;       /* VCD signal ID */
    int          signal_id;    /* IR signal ID in DUT */
    const char  *full_path;    /* e.g. "dut.count_a" */
} SimTap;

static void vcd_dump_all(VCDWriter *vcd, SimTestState *ts, int *vcd_ids,
                          SimTap *taps, int num_taps) {
    for (int i = 0; i < ts->num_tb_wires; i++) {
        if (vcd_ids[i] >= 0) {
            vcd_dump_value(vcd, vcd_ids[i],
                           ts->tb_wires[i].value.val[0],
                           ts->tb_wires[i].width);
        }
    }
    /* Dump TAP signals from DUT internals */
    for (int i = 0; i < num_taps; i++) {
        if (taps[i].vcd_id >= 0 && ts->dut) {
            SimSignalEntry *se = sim_ctx_lookup(ts->dut, taps[i].signal_id);
            if (se) {
                vcd_dump_value(vcd, taps[i].vcd_id, se->current.val[0],
                               se->current.width);
            }
        }
    }
}

/**
 * @brief Convert @run duration to picoseconds.
 */
static uint64_t run_to_ps(const JZASTNode *run_node) {
    if (!run_node || !run_node->text || !run_node->name) return 0;

    double val = atof(run_node->name);
    if (val <= 0.0) return 0;

    uint64_t result;
    if (strcmp(run_node->text, "ns") == 0) {
        if (time_to_exact_ps(val, 1000.0, &result) != 0) {
            fprintf(stderr, "ERROR: @run(ns=%.6g) does not convert to an "
                    "exact integer number of picoseconds\n", val);
            return 0;
        }
        return result;
    } else if (strcmp(run_node->text, "ms") == 0) {
        if (time_to_exact_ps(val, 1000000.0, &result) != 0) {
            fprintf(stderr, "ERROR: @run(ms=%.6g) does not convert to an "
                    "exact integer number of picoseconds\n", val);
            return 0;
        }
        return result;
    } else if (strcmp(run_node->text, "ticks") == 0) {
        /* Ticks: will be multiplied by tick_ps later, return raw value */
        return (uint64_t)(val + 0.5);
    }
    return 0;
}

/**
 * @brief Run a single @simulation block.
 */
static int sim_run_simulation(const JZASTNode *root,
                               const JZASTNode *sim_node,
                               const IR_Module *dut_module,
                               const IR_Design *design,
                               uint32_t seed,
                               int verbose,
                               const char *filename,
                               const char *vcd_path) {
    SimTestState ts;
    memset(&ts, 0, sizeof(ts));
    ts.verbose = verbose;
    ts.filename = filename;
    ts.test_passed = 1;

    /* Collect clocks and wires */
    collect_decls(&ts, sim_node, root, JZ_AST_SIM_CLOCK_BLOCK, JZ_AST_SIM_CLOCK_DECL);

    /* Find @new instance */
    const JZASTNode *instance_node = NULL;
    for (size_t i = 0; i < sim_node->child_count; i++) {
        if (sim_node->children[i] &&
            sim_node->children[i]->type == JZ_AST_MODULE_INSTANCE) {
            instance_node = sim_node->children[i];
            break;
        }
    }

    if (!instance_node) {
        fprintf(stderr, "error: no @new instance in @simulation\n");
        goto cleanup_early;
    }

    /* Create DUT context */
    ts.dut = sim_ctx_create(dut_module, design, seed);
    if (!ts.dut) {
        fprintf(stderr, "error: failed to create simulation context\n");
        goto cleanup_early;
    }

    /* Build port bindings */
    build_port_bindings(&ts, instance_node, dut_module);

    /* Collect simulation clocks */
    SimClock sim_clocks[64];
    int num_sim_clocks = collect_sim_clocks(sim_node, &ts, sim_clocks, 64);
    if (num_sim_clocks < 0) return 1; /* conversion error already reported */

    /* Compute GCD tick */
    uint64_t tick_ps = 0;
    for (int i = 0; i < num_sim_clocks; i++) {
        if (tick_ps == 0)
            tick_ps = sim_clocks[i].toggle_ps;
        else
            tick_ps = gcd64(tick_ps, sim_clocks[i].toggle_ps);
    }
    if (tick_ps == 0) tick_ps = 1000; /* default 1ns */
    ts.tick_ps = tick_ps;

    if (verbose) {
        fprintf(stdout, "Simulation tick resolution: %llu ps\n",
                (unsigned long long)tick_ps);
        for (int i = 0; i < num_sim_clocks; i++) {
            fprintf(stdout, "  Clock <%s> period=%llu ps (toggle every %llu ps)\n",
                    sim_clocks[i].name,
                    (unsigned long long)(sim_clocks[i].toggle_ps * 2),
                    (unsigned long long)sim_clocks[i].toggle_ps);
        }
    }

    /* Open VCD — use 1ns timescale so waveform viewers show readable timestamps */
    uint64_t vcd_timescale_ps = 1000; /* 1 ns */
    VCDWriter *vcd = vcd_open(vcd_path, vcd_timescale_ps);
    if (!vcd) {
        fprintf(stderr, "error: failed to open VCD file '%s'\n", vcd_path);
        goto cleanup_dut;
    }

    /* Register testbench wires in VCD */
    int *vcd_ids = calloc((size_t)ts.num_tb_wires, sizeof(int));
    for (int i = 0; i < ts.num_tb_wires; i++) {
        const char *scope = ts.tb_wires[i].is_clock ? "clocks" : "wires";
        vcd_ids[i] = vcd_add_signal(vcd, scope, ts.tb_wires[i].name,
                                     ts.tb_wires[i].width);
    }

    /* Collect and register TAP signals in VCD */
    SimTap *sim_taps = NULL;
    int num_sim_taps = 0;
    {
        /* Count TAP declarations */
        int tap_count = 0;
        for (size_t i = 0; i < sim_node->child_count; i++) {
            const JZASTNode *child = sim_node->children[i];
            if (!child || child->type != JZ_AST_SIM_TAP_BLOCK) continue;
            for (size_t j = 0; j < child->child_count; j++) {
                if (child->children[j] &&
                    child->children[j]->type == JZ_AST_SIM_TAP_DECL)
                    tap_count++;
            }
        }

        if (tap_count > 0) {
            sim_taps = calloc((size_t)tap_count, sizeof(SimTap));
            for (size_t i = 0; i < sim_node->child_count; i++) {
                const JZASTNode *child = sim_node->children[i];
                if (!child || child->type != JZ_AST_SIM_TAP_BLOCK) continue;
                for (size_t j = 0; j < child->child_count; j++) {
                    const JZASTNode *td = child->children[j];
                    if (!td || td->type != JZ_AST_SIM_TAP_DECL || !td->name)
                        continue;

                    /* Parse "inst.signal" — find the signal name after the
                     * last dot. The scope is everything before it. */
                    const char *path = td->name;
                    const char *last_dot = strrchr(path, '.');
                    if (!last_dot) continue;

                    const char *sig_name = last_dot + 1;
                    size_t scope_len = (size_t)(last_dot - path);
                    char *scope = malloc(scope_len + 1);
                    memcpy(scope, path, scope_len);
                    scope[scope_len] = '\0';

                    /* Find the signal in the DUT module */
                    int found_id = -1;
                    int found_width = 0;
                    for (int si = 0; si < dut_module->num_signals; si++) {
                        const IR_Signal *sig = &dut_module->signals[si];
                        if (sig->name && strcmp(sig->name, sig_name) == 0) {
                            found_id = sig->id;
                            found_width = sig->width;
                            break;
                        }
                    }

                    if (found_id >= 0) {
                        sim_taps[num_sim_taps].full_path = path;
                        sim_taps[num_sim_taps].signal_id = found_id;
                        sim_taps[num_sim_taps].vcd_id =
                            vcd_add_signal(vcd, scope, sig_name, found_width);
                        num_sim_taps++;
                    } else if (verbose) {
                        fprintf(stderr, "warning: TAP signal '%s' not found in DUT\n",
                                path);
                    }

                    free(scope);
                }
            }
        }
    }

    vcd_end_definitions(vcd);

    /* ------------------------------------------------------------------ */
    /* Time 0 initialization sequence:                                     */
    /*   1. Registers already randomized by sim_ctx_create (seed-based).   */
    /*   2. Clocks hold at 0 (tb_wires are zero-initialized).              */
    /*   3. Process @setup — apply explicit initial wire values.           */
    /*   4. Propagate inputs, settle combinational logic.                  */
    /*   5. Dump Time 0 to VCD (initial state before first clock edge).    */
    /*   6. Then the first @run begins advancing ticks.                    */
    /* ------------------------------------------------------------------ */
    uint64_t current_time_ps = 0;

    /* Step 3: find and execute @setup before any @run */
    for (size_t ci = 0; ci < sim_node->child_count; ci++) {
        const JZASTNode *child = sim_node->children[ci];
        if (!child) continue;
        if (child->type == JZ_AST_TB_SETUP) {
            process_setup_update(&ts, child, 1);
            break; /* only first @setup is the init block */
        }
        /* Stop at first @run — anything after is sequenced, not init */
        if (child->type == JZ_AST_SIM_RUN) break;
    }

    /* Step 4: propagate and settle */
    sim_full_settle(&ts);

    /* Step 5: dump Time 0 */
    vcd_set_time(vcd, 0);
    vcd_dump_all(vcd, &ts, vcd_ids, sim_taps, num_sim_taps);

    /* Walk simulation body sequentially (skip the @setup we already ran) */
    int setup_done = 0;
    for (size_t ci = 0; ci < sim_node->child_count; ci++) {
        const JZASTNode *child = sim_node->children[ci];
        if (!child) continue;

        /* Abort simulation on runtime error (combinational loop) */
        if (ts.runtime_error) break;

        if (child->type == JZ_AST_TB_SETUP) {
            if (!setup_done) {
                setup_done = 1;
                continue; /* already executed above */
            }
            /* Subsequent @setup blocks (unusual) — treat like @update */
            process_setup_update(&ts, child, 1);
            sim_full_settle(&ts);
            vcd_set_time(vcd, current_time_ps);
            vcd_dump_all(vcd, &ts, vcd_ids, sim_taps, num_sim_taps);

        } else if (child->type == JZ_AST_TB_UPDATE) {
            /* @update: apply wire changes at current time, then settle */
            process_setup_update(&ts, child, 0);
            sim_full_settle(&ts);
            vcd_set_time(vcd, current_time_ps);
            vcd_dump_all(vcd, &ts, vcd_ids, sim_taps, num_sim_taps);

        } else if (child->type == JZ_AST_SIM_RUN) {
            /* @run: advance time */
            uint64_t duration_ps;
            if (child->text && strcmp(child->text, "ticks") == 0) {
                uint64_t raw = run_to_ps(child);
                duration_ps = raw * tick_ps;
            } else {
                duration_ps = run_to_ps(child);
            }

            if (verbose) {
                fprintf(stdout, "@run(%s=%s) -> %llu ps\n",
                        child->text ? child->text : "?",
                        child->name ? child->name : "?",
                        (unsigned long long)duration_ps);
            }

            uint64_t end_time_ps = current_time_ps + duration_ps;

            while (current_time_ps < end_time_ps && !ts.runtime_error) {
                current_time_ps += tick_ps;

                /* Toggle clocks scheduled at this tick */
                int any_edge = 0;
                for (int ci2 = 0; ci2 < num_sim_clocks; ci2++) {
                    if (sim_clocks[ci2].toggle_ps > 0 &&
                        (current_time_ps % sim_clocks[ci2].toggle_ps) == 0) {
                        int wi = sim_clocks[ci2].tb_wire_idx;
                        if (wi >= 0) {
                            uint64_t cur = ts.tb_wires[wi].value.val[0] & 1;
                            ts.tb_wires[wi].value = sim_val_from_uint(cur ^ 1, 1);
                            any_edge = 1;
                        }
                    }
                }

                /* Propagate inputs and settle combinational */
                sim_propagate_inputs(&ts);
                sim_settle_checked(&ts);
                sim_resolve_inout_z(&ts);

                /* Fire sync domains for active clock edges */
                if (any_edge) {
                    for (int ci2 = 0; ci2 < num_sim_clocks; ci2++) {
                        if (sim_clocks[ci2].toggle_ps == 0) continue;
                        if ((current_time_ps % sim_clocks[ci2].toggle_ps) != 0) continue;

                        int wi = sim_clocks[ci2].tb_wire_idx;
                        if (wi < 0) continue;
                        uint64_t new_clk = ts.tb_wires[wi].value.val[0] & 1;

                        /* Find clock port ID */
                        int clock_port_id = -1;
                        for (int b = 0; b < ts.num_bindings; b++) {
                            if (ts.bindings[b].tb_wire_index == wi) {
                                clock_port_id = ts.bindings[b].port_signal_id;
                                break;
                            }
                        }

                        /* Fire matching domains (NBA deferred for simulation) */
                        sim_fire_domains_for_clock(&ts, clock_port_id, new_clk,
                                                    /*apply_nba_per_domain=*/0);
                    }

                    /* Apply all NBA updates at once (simulation semantics) */
                    sim_ctx_apply_nba(ts.dut);

                    /* Settle again after NBA */
                    sim_settle_checked(&ts);
                    sim_resolve_inout_z(&ts);
                }

                /* Propagate outputs */
                sim_propagate_outputs(&ts);

                /* Dump to VCD */
                vcd_set_time(vcd, current_time_ps);
                vcd_dump_all(vcd, &ts, vcd_ids, sim_taps, num_sim_taps);
            }
        } else if (child->type == JZ_AST_SIM_RUN_UNTIL ||
                   child->type == JZ_AST_SIM_RUN_WHILE) {
            /* @run_until / @run_while: advance time with condition check */
            int is_until = (child->type == JZ_AST_SIM_RUN_UNTIL);

            /* Parse timeout duration */
            uint64_t timeout_ps;
            if (child->text && strcmp(child->text, "ticks") == 0) {
                uint64_t raw = run_to_ps(child);
                timeout_ps = raw * tick_ps;
            } else {
                timeout_ps = run_to_ps(child);
            }

            /* Get condition: children[0]=signal, children[1]=value, block_kind=op */
            const JZASTNode *sig_node = (child->child_count > 0) ? child->children[0] : NULL;
            const JZASTNode *val_node = (child->child_count > 1) ? child->children[1] : NULL;
            int cond_is_eq = (!child->block_kind || strcmp(child->block_kind, "==") == 0);

            SimValue expected_val = eval_tb_ast_expr(&ts, val_node);

            if (verbose) {
                fprintf(stdout, "@%s(%s %s ..., timeout=%s=%s) -> %llu ps\n",
                        is_until ? "run_until" : "run_while",
                        sig_node && sig_node->name ? sig_node->name : "?",
                        cond_is_eq ? "==" : "!=",
                        child->text ? child->text : "?",
                        child->name ? child->name : "?",
                        (unsigned long long)timeout_ps);
            }

            uint64_t end_time_ps = current_time_ps + timeout_ps;
            int condition_met = 0;

            while (current_time_ps < end_time_ps && !ts.runtime_error) {
                current_time_ps += tick_ps;

                /* Toggle clocks scheduled at this tick */
                int any_edge = 0;
                for (int ci2 = 0; ci2 < num_sim_clocks; ci2++) {
                    if (sim_clocks[ci2].toggle_ps > 0 &&
                        (current_time_ps % sim_clocks[ci2].toggle_ps) == 0) {
                        int wi = sim_clocks[ci2].tb_wire_idx;
                        if (wi >= 0) {
                            uint64_t cur = ts.tb_wires[wi].value.val[0] & 1;
                            ts.tb_wires[wi].value = sim_val_from_uint(cur ^ 1, 1);
                            any_edge = 1;
                        }
                    }
                }

                /* Propagate inputs and settle combinational */
                sim_propagate_inputs(&ts);
                sim_settle_checked(&ts);
                sim_resolve_inout_z(&ts);

                /* Fire sync domains for active clock edges */
                if (any_edge) {
                    for (int ci2 = 0; ci2 < num_sim_clocks; ci2++) {
                        if (sim_clocks[ci2].toggle_ps == 0) continue;
                        if ((current_time_ps % sim_clocks[ci2].toggle_ps) != 0) continue;

                        int wi = sim_clocks[ci2].tb_wire_idx;
                        if (wi < 0) continue;
                        uint64_t new_clk = ts.tb_wires[wi].value.val[0] & 1;

                        int clock_port_id = -1;
                        for (int b = 0; b < ts.num_bindings; b++) {
                            if (ts.bindings[b].tb_wire_index == wi) {
                                clock_port_id = ts.bindings[b].port_signal_id;
                                break;
                            }
                        }

                        sim_fire_domains_for_clock(&ts, clock_port_id, new_clk,
                                                    /*apply_nba_per_domain=*/0);
                    }

                    sim_ctx_apply_nba(ts.dut);
                    sim_settle_checked(&ts);
                    sim_resolve_inout_z(&ts);
                }

                sim_propagate_outputs(&ts);

                /* Dump to VCD */
                vcd_set_time(vcd, current_time_ps);
                vcd_dump_all(vcd, &ts, vcd_ids, sim_taps, num_sim_taps);

                /* Evaluate condition */
                SimValue actual = eval_tb_ast_expr(&ts, sig_node);
                int match;
                if (cond_is_eq)
                    match = sim_val_is_true(sim_val_eq(actual, expected_val)) == 1;
                else
                    match = sim_val_is_true(sim_val_neq(actual, expected_val)) == 1;

                if (is_until) {
                    /* @run_until: stop when condition becomes true */
                    if (match) {
                        condition_met = 1;
                        break;
                    }
                } else {
                    /* @run_while: stop when condition becomes false */
                    if (!match) {
                        condition_met = 1;
                        break;
                    }
                }
            }

            if (!condition_met && !ts.runtime_error) {
                /* Timeout reached without condition being met */
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "    TIMEOUT: @%s(%s %s ...) at %s:%d\n"
                         "      Condition not met within timeout",
                         is_until ? "run_until" : "run_while",
                         sig_node && sig_node->name ? sig_node->name : "?",
                         cond_is_eq ? "==" : "!=",
                         filename ? filename : "?",
                         child->loc.line);
                record_failure(&ts, msg);
                ts.num_failed++;
                ts.runtime_error = 1;
            }
        } else if (child->type == JZ_AST_PRINT ||
                   child->type == JZ_AST_PRINT_IF) {
            ts.current_time_ps = current_time_ps;
            process_print(&ts, child);
        }
        /* Skip other node types (CLOCK_BLOCK, WIRE_BLOCK, TAP_BLOCK, etc.) */
    }

    if (ts.runtime_error) {
        fprintf(stderr, "RUNTIME ERROR: Simulation aborted\n");
        for (int i = 0; i < ts.num_failure_msgs; i++)
            fprintf(stderr, "%s\n", ts.failure_msgs[i]);
    }

    if (verbose) {
        fprintf(stdout, "Simulation complete. %llu ps simulated.\n",
                (unsigned long long)current_time_ps);
    }
    fprintf(stdout, "VCD written to: %s\n", vcd_path);

    /* Cleanup */
    free(vcd_ids);
    free(sim_taps);
    vcd_close(vcd);

cleanup_dut:
    sim_ctx_destroy(ts.dut);
    free(ts.bindings);

cleanup_early:
    for (int i = 0; i < ts.num_tb_wires; i++) {
        if (ts.tb_wires[i].owns_name)
            free((char *)ts.tb_wires[i].name);
    }
    free(ts.tb_wires);
    for (int i = 0; i < ts.num_failure_msgs; i++)
        free(ts.failure_msgs[i]);
    free(ts.failure_msgs);

    return 0;
}

/* ---- Public API: jz_sim_run_simulations ---- */

int jz_sim_run_simulations(const JZASTNode *root,
                            const IR_Design *design,
                            uint32_t seed,
                            int verbose,
                            JZDiagnosticList *diagnostics,
                            const char *filename,
                            const char *vcd_path) {
    (void)diagnostics;

    if (!root || !design) return 1;

    int any_sim_found = 0;
    int rc = 0;

    for (size_t i = 0; i < root->child_count; i++) {
        const JZASTNode *child = root->children[i];
        if (!child || child->type != JZ_AST_SIMULATION) continue;

        any_sim_found = 1;
        const char *module_name = child->name;

        const IR_Module *dut_module = find_module_by_name(design, module_name);
        if (!dut_module) {
            fprintf(stderr, "error: cannot find module '%s' for simulation\n",
                    module_name ? module_name : "?");
            rc = 1;
            continue;
        }

        fprintf(stdout, "\n=== @simulation %s ===\n", module_name);

        int sim_rc = sim_run_simulation(root, child, dut_module, design,
                                         seed, verbose, filename, vcd_path);
        if (sim_rc != 0) rc = 1;
    }

    if (!any_sim_found) {
        fprintf(stdout, "\nNo simulations found.\n");
    }

    return rc;
}
