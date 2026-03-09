#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <time.h>

#include "sem_driver.h"
#include "sem.h"
#include "../../sem/driver_internal.h"
#include "util.h"
#include "rules.h"

/* -------------------------------------------------------------------------
 *  Tri-state report global state
 * -------------------------------------------------------------------------
 */

static int g_tristate_report_enabled = 0;
static int g_tristate_report_header_printed = 0;
static FILE *g_tristate_report_out = NULL;
static char g_tristate_report_generated[64];
static const char *g_tristate_report_version = NULL;
static const char *g_tristate_report_input = NULL;

/* Overall summary accumulator: one entry per net name across all modules. */
typedef struct {
    char   name[128];       /* Net name (e.g., "pbus.DATA") */
    size_t total_drivers;   /* Sum of drivers across all modules */
    int    worst_result;    /* Worst result seen (PROVEN < UNKNOWN < DISPROVEN) */
} JZTristateSummaryEntry;

static JZBuffer g_tristate_summary = {0};  /* Array of JZTristateSummaryEntry */

void jz_sem_enable_tristate_report(FILE *out,
                                   const char *tool_version,
                                   const char *input_filename)
{
    g_tristate_report_enabled = (out != NULL);
    g_tristate_report_header_printed = 0;
    g_tristate_report_out = out;
    g_tristate_report_version = tool_version;
    g_tristate_report_input = input_filename;
    jz_buf_free(&g_tristate_summary);
    memset(&g_tristate_summary, 0, sizeof(g_tristate_summary));

    time_t now = time(NULL);
    struct tm tm_info;
    if (localtime_r(&now, &tm_info) != NULL) {
        if (strftime(g_tristate_report_generated,
                     sizeof(g_tristate_report_generated),
                     "%Y-%m-%d %H:%M %Z",
                     &tm_info) == 0) {
            snprintf(g_tristate_report_generated,
                     sizeof(g_tristate_report_generated),
                     "<unknown>");
        }
    } else {
        snprintf(g_tristate_report_generated,
                 sizeof(g_tristate_report_generated),
                 "<unknown>");
    }
}

/* -------------------------------------------------------------------------
 *  Report helper functions
 * -------------------------------------------------------------------------
 */

/* Print source snippet for a given location. */
static void tristate_print_source_at_loc(FILE *out, JZLocation loc)
{
    if (!out || !loc.filename || loc.line <= 0) return;

    size_t size = 0;
    char *contents = jz_read_entire_file(loc.filename, &size);
    if (!contents || size == 0) {
        if (contents) free(contents);
        return;
    }

    const char *p = contents;
    const char *line_start = contents;
    int current_line = 1;

    while (*p && current_line < loc.line) {
        if (*p == '\n') {
            ++current_line;
            line_start = p + 1;
        }
        ++p;
    }

    if (current_line == loc.line) {
        const char *line_end = line_start;
        while (*line_end && *line_end != '\n' && *line_end != '\r') {
            ++line_end;
        }

        while (line_start < line_end && isspace((unsigned char)*line_start)) {
            ++line_start;
        }
        while (line_end > line_start && isspace((unsigned char)line_end[-1])) {
            --line_end;
        }

        size_t len = (size_t)(line_end - line_start);
        if (len > 0 && len < 200) {
            fprintf(out, "        Code: ");
            fwrite(line_start, 1, len, out);
            fputc('\n', out);
        }
    }

    free(contents);
}

/* -------------------------------------------------------------------------
 *  Report generation
 * -------------------------------------------------------------------------
 */

/* Print the report for a module. */
void sem_emit_tristate_report_for_module(const JZModuleScope *scope,
                                          JZBuffer *nets,
                                          const JZBuffer *module_scopes,
                                          const JZBuffer *project_symbols,
                                          JZASTNode *project_root)
{
    (void)project_symbols; /* reserved for future use */
    (void)project_root;    /* reserved for future use */

    if (!g_tristate_report_enabled || !g_tristate_report_out || !scope || !scope->node) {
        return;
    }

    FILE *out = g_tristate_report_out;

    /* Collect multi-driver nets. */
    size_t raw_net_count = nets->len / sizeof(JZNet);
    JZBuffer multi_driver_nets = {0};

    for (size_t ni = 0; ni < raw_net_count; ++ni) {
        JZNet *net = &((JZNet *)nets->data)[ni];
        if (net->atoms.len == 0) continue;

        /* Count unique drivers. */
        JZASTNode **drv = (JZASTNode **)net->driver_stmts.data;
        size_t drv_count = net->driver_stmts.len / sizeof(JZASTNode *);

        size_t unique_count = 0;
        for (size_t di = 0; di < drv_count; ++di) {
            if (!drv[di]) continue;
            int seen = 0;
            for (size_t dj = 0; dj < di; ++dj) {
                if (drv[dj] == drv[di]) {
                    seen = 1;
                    break;
                }
            }
            if (!seen) unique_count++;
        }

        if (unique_count >= 2) {
            (void)jz_buf_append(&multi_driver_nets, &ni, sizeof(ni));
        }
    }

    size_t multi_count = multi_driver_nets.len / sizeof(size_t);
    if (multi_count == 0) {
        jz_buf_free(&multi_driver_nets);
        return;
    }

    /* Analyze all multi-driver nets and filter to tristate-relevant ones. */
    JZBuffer net_infos = {0};
    JZBuffer bus_field_name_ptrs = {0}; /* Array of char* for allocated bus field names */
    size_t *indices = (size_t *)multi_driver_nets.data;

    for (size_t i = 0; i < multi_count; ++i) {
        size_t ni = indices[i];
        JZNet *net = &((JZNet *)nets->data)[ni];

        /* Get net name from first atom. */
        const char *net_name = NULL;
        JZASTNode **atoms = (JZASTNode **)net->atoms.data;
        size_t atom_count = net->atoms.len / sizeof(JZASTNode *);
        for (size_t ai = 0; ai < atom_count; ++ai) {
            if (atoms[ai] && atoms[ai]->name) {
                net_name = atoms[ai]->name;
                break;
            }
        }
        if (!net_name) net_name = "<unnamed>";

        /* For BUS ports, split into per-field entries. */
        if (jz_tristate_net_is_bus_port(net)) {
            /* Collect unique field names from driver_stmts. */
            JZASTNode **drv = (JZASTNode **)net->driver_stmts.data;
            size_t drv_count = net->driver_stmts.len / sizeof(JZASTNode *);

            const char *fields[64];
            size_t field_count = 0;

            for (size_t di = 0; di < drv_count && field_count < 64; ++di) {
                if (!drv[di]) continue;
                const char *field = jz_tristate_extract_bus_field(drv[di]);
                if (!field) continue;
                /* Check for duplicates. */
                int dup = 0;
                for (size_t fi = 0; fi < field_count; ++fi) {
                    if (strcmp(fields[fi], field) == 0) {
                        dup = 1;
                        break;
                    }
                }
                if (!dup) {
                    fields[field_count++] = field;
                }
            }

            /* For each unique field, analyze as a separate net entry. */
            for (size_t fi = 0; fi < field_count; ++fi) {
                /* Build display name: "netname.FIELD" and persist via allocation. */
                char field_name[256];
                snprintf(field_name, sizeof(field_name), "%s.%s", net_name, fields[fi]);
                char *persistent_name = strdup(field_name);
                if (!persistent_name) continue;
                (void)jz_buf_append(&bus_field_name_ptrs, &persistent_name, sizeof(char *));

                JZTristateNetInfo info;
                jz_tristate_analyze_net(&info, net, persistent_name, fields[fi], scope, module_scopes);

                /* Skip if no driver can produce z. */
                size_t drv_cnt = info.drivers.len / sizeof(JZTristateDriver);
                JZTristateDriver *drv_arr = (JZTristateDriver *)info.drivers.data;
                int any_can_z = 0;
                for (size_t di = 0; di < drv_cnt; ++di) {
                    if (drv_arr[di].can_produce_z) {
                        any_can_z = 1;
                        break;
                    }
                }
                if (!any_can_z) {
                    jz_buf_free(&info.drivers);
                    jz_buf_free(&info.sinks);
                    continue;
                }

                (void)jz_buf_append(&net_infos, &info, sizeof(info));
            }
            continue;
        }

        JZTristateNetInfo info;
        jz_tristate_analyze_net(&info, net, net_name, NULL, scope, module_scopes);

        /* Skip nets where no driver can produce z - these are not tristate
         * concerns (e.g., registers assigned in multiple CASE branches).
         */
        size_t drv_cnt = info.drivers.len / sizeof(JZTristateDriver);
        JZTristateDriver *drv_arr = (JZTristateDriver *)info.drivers.data;
        int any_can_z = 0;
        for (size_t di = 0; di < drv_cnt; ++di) {
            if (drv_arr[di].can_produce_z) {
                any_can_z = 1;
                break;
            }
        }
        if (!any_can_z) {
            jz_buf_free(&info.drivers);
            jz_buf_free(&info.sinks);
            continue;
        }

        (void)jz_buf_append(&net_infos, &info, sizeof(info));
    }

    JZTristateNetInfo *infos = (JZTristateNetInfo *)net_infos.data;
    size_t info_count = net_infos.len / sizeof(JZTristateNetInfo);

    if (info_count == 0) {
        jz_buf_free(&net_infos);
        jz_buf_free(&multi_driver_nets);
        size_t nc = bus_field_name_ptrs.len / sizeof(char *);
        char **np = (char **)bus_field_name_ptrs.data;
        for (size_t ii = 0; ii < nc; ++ii) free(np[ii]);
        jz_buf_free(&bus_field_name_ptrs);
        return;
    }

    /* Print top-level header once. */
    if (!g_tristate_report_header_printed) {
        fprintf(out, "JZ-HDL Tri-State Resolution Report\n");
        fprintf(out, "Version: %s\n",
                g_tristate_report_version ? g_tristate_report_version : "(unknown)");
        fprintf(out, "Generated: %s\n", g_tristate_report_generated);
        fprintf(out, "\n");
        fprintf(out, "Legend\n");
        fprintf(out, "------\n");
        fprintf(out, "Can produce Z: Driver can output high-impedance (tri-state)\n");
        fprintf(out, "Can produce non-Z: Driver can output logic 0 or 1\n");
        fprintf(out, "E0, E1, ...: Driver conditions (guards)\n");
        fprintf(out, "G: Globally operator from temporal logic, globally true (at all times)\n");
        fprintf(out, "AT_MOST_ONE: At most one driver active per execution path\n");
        fprintf(out, "EXACTLY_ONE: Exactly one driver active per execution path (no floating nets)\n");
        g_tristate_report_header_printed = 1;
    }

    /* Print per-module header. */
    fprintf(out, "\n");
    if (g_tristate_report_input) {
        fprintf(out, "Input file: %s\n", g_tristate_report_input);
    }
    fprintf(out, "Module: %s\n", scope->name ? scope->name : "<anonymous>");
    fprintf(out, "\n");

    /* Print summary. */
    fprintf(out, "Summary\n");
    fprintf(out, "-------\n");

    for (size_t i = 0; i < info_count; ++i) {
        JZTristateNetInfo *info = &infos[i];
        size_t driver_count = info->drivers.len / sizeof(JZTristateDriver);

        const char *result_str = "UNKNOWN";
        if (info->result == JZ_TRISTATE_PROVEN) result_str = "PROVEN";
        else if (info->result == JZ_TRISTATE_DISPROVEN) result_str = "DISPROVEN";

        fprintf(out, "%s@%d: %s, Possible Drivers: %zu, Result: %s\n",
                info->decl_loc.filename ? info->decl_loc.filename : "<unknown>",
                info->decl_loc.line,
                info->name,
                driver_count,
                result_str);
    }
    fprintf(out, "\n");

    /* Print detailed per-net information. */
    for (size_t i = 0; i < info_count; ++i) {
        JZTristateNetInfo *info = &infos[i];

        fprintf(out, "%s Details\n", info->name);
        fprintf(out, "------------------\n");
        fprintf(out, "Width: %u\n", info->width);
        fprintf(out, "Declared at: %s@%d\n",
                info->decl_loc.filename ? info->decl_loc.filename : "<unknown>",
                info->decl_loc.line);

        /* Drivers. */
        fprintf(out, "Drivers:\n");
        JZTristateDriver *drivers = (JZTristateDriver *)info->drivers.data;
        size_t driver_count = info->drivers.len / sizeof(JZTristateDriver);

        for (size_t di = 0; di < driver_count; ++di) {
            JZTristateDriver *drv = &drivers[di];

            fprintf(out, "    %s@%d: E%zu := (",
                    drv->loc.filename ? drv->loc.filename : "<unknown>",
                    drv->loc.line,
                    di);

            if (drv->enable.condition_text[0]) {
                fprintf(out, "%s", drv->enable.condition_text);
            } else if (drv->enable.input_name && drv->enable.compare_value) {
                if (drv->enable.is_inverted) {
                    fprintf(out, "%s != %s", drv->enable.input_name, drv->enable.compare_value);
                } else {
                    fprintf(out, "%s == %s", drv->enable.input_name, drv->enable.compare_value);
                }
            } else if (drv->enable.normalized_lhs[0] && drv->enable.normalized_rhs[0]) {
                if (drv->enable.is_inverted) {
                    fprintf(out, "%s != %s", drv->enable.normalized_lhs, drv->enable.normalized_rhs);
                } else {
                    fprintf(out, "%s == %s", drv->enable.normalized_lhs, drv->enable.normalized_rhs);
                }
            } else {
                fprintf(out, "unconditional");
            }
            fprintf(out, ")\n");

            if (drv->instance_name) {
                fprintf(out, "        Instance: %s (module: %s)\n",
                        drv->instance_name,
                        drv->module_name ? drv->module_name : "<unknown>");
            }
            for (size_t ai = 0; ai < drv->n_aliases; ++ai) {
                /* Only show aliases whose register name appears in the condition text. */
                if (drv->enable.condition_text[0] &&
                    strstr(drv->enable.condition_text, drv->aliases[ai].from)) {
                    fprintf(out, "        Alias: %s == %s\n",
                            drv->aliases[ai].from,
                            drv->aliases[ai].to);
                }
            }
            fprintf(out, "        Can produce Z: %s, Can produce non-Z: %s\n",
                    drv->can_produce_z ? "yes" : "no",
                    drv->can_produce_non_z ? "yes" : "no");

            tristate_print_source_at_loc(out, drv->loc);
        }

        /* Proof obligations. */
        fprintf(out, "\nProof Obligations:\n");
        if (driver_count >= 2) {
            fprintf(out, "    AT_MOST_ONE: ");
            int first = 1;
            for (size_t di = 0; di < driver_count; ++di) {
                for (size_t dj = di + 1; dj < driver_count; ++dj) {
                    if (!first) fprintf(out, " AND ");
                    fprintf(out, "G !(E%zu && E%zu)", di, dj);
                    first = 0;
                }
            }
            fprintf(out, "\n");

            fprintf(out, "    EXACTLY_ONE: G (");
            for (size_t di = 0; di < driver_count; ++di) {
                if (di > 0) fprintf(out, " || ");
                fprintf(out, "E%zu", di);
            }
            fprintf(out, ") AND AT_MOST_ONE\n");
        } else {
            fprintf(out, "    (trivially satisfied - single driver)\n");
        }

        /* Result. */
        fprintf(out, "\nResult: ");
        if (info->result == JZ_TRISTATE_PROVEN) {
            fprintf(out, "PROVEN\n");
            switch (info->proof_method) {
            case JZ_TRISTATE_PROOF_SINGLE_DRIVER:
                fprintf(out, "    Reason: Single driver - trivially safe\n");
                break;
            case JZ_TRISTATE_PROOF_SINGLE_NON_Z:
                fprintf(out, "    Reason: Only one driver can produce non-Z values\n");
                break;
            case JZ_TRISTATE_PROOF_DISTINCT_CONSTANTS:
                fprintf(out, "    Reason: Drivers have distinct constant enable conditions\n");
                break;
            case JZ_TRISTATE_PROOF_COMPLEMENTARY_GUARDS:
                fprintf(out, "    Reason: Guard conditions are complementary\n");
                break;
            case JZ_TRISTATE_PROOF_IF_ELSE_BRANCHES:
                fprintf(out, "    Reason: Drivers are in complementary IF/ELSE branches\n");
                break;
            case JZ_TRISTATE_PROOF_PAIRWISE:
                fprintf(out, "    Reason: All driver pairs proven mutually exclusive independently\n");
                break;
            }
        } else if (info->result == JZ_TRISTATE_DISPROVEN) {
            fprintf(out, "DISPROVEN\n");
            fprintf(out, "    Conflict: Drivers E%zu and E%zu may be active simultaneously\n",
                    info->conflict.driver_a, info->conflict.driver_b);
            if (info->conflict.reason) {
                fprintf(out, "    Reason: %s\n", info->conflict.reason);
            }
        } else {
            fprintf(out, "UNKNOWN\n");
            switch (info->unknown_reason) {
            case JZ_TRISTATE_UNKNOWN_BLACKBOX:
                fprintf(out, "    Reason: Blackbox module output - cannot analyze internal enable logic\n");
                break;
            case JZ_TRISTATE_UNKNOWN_UNCONSTRAINED_INPUT:
                fprintf(out, "    Reason: Guard depends on unconstrained primary input\n");
                break;
            case JZ_TRISTATE_UNKNOWN_COMPLEX_EXPR:
                fprintf(out, "    Reason: Enable expression too complex to analyze\n");
                break;
            case JZ_TRISTATE_UNKNOWN_MULTI_CLOCK:
                fprintf(out, "    Reason: Multi-clock domain crossing\n");
                break;
            case JZ_TRISTATE_UNKNOWN_NO_GUARD:
                fprintf(out, "    Reason: Could not extract guard condition from one or more drivers\n");
                break;
            }
        }

        fprintf(out, "\n");
    }

    /* Accumulate into overall summary. */
    for (size_t i = 0; i < info_count; ++i) {
        JZTristateNetInfo *info = &infos[i];
        size_t driver_count = info->drivers.len / sizeof(JZTristateDriver);
        const char *net_name = info->name ? info->name : "<unnamed>";

        /* Search for existing entry with the same name. */
        size_t entry_count = g_tristate_summary.len / sizeof(JZTristateSummaryEntry);
        JZTristateSummaryEntry *entries = (JZTristateSummaryEntry *)g_tristate_summary.data;
        int found = 0;
        for (size_t ei = 0; ei < entry_count; ++ei) {
            if (strcmp(entries[ei].name, net_name) == 0) {
                entries[ei].total_drivers += driver_count;
                if ((int)info->result > entries[ei].worst_result) {
                    entries[ei].worst_result = (int)info->result;
                }
                found = 1;
                break;
            }
        }
        if (!found) {
            JZTristateSummaryEntry entry;
            memset(&entry, 0, sizeof(entry));
            strncpy(entry.name, net_name, sizeof(entry.name) - 1);
            entry.name[sizeof(entry.name) - 1] = '\0';
            entry.total_drivers = driver_count;
            entry.worst_result = (int)info->result;
            (void)jz_buf_append(&g_tristate_summary, &entry, sizeof(entry));
        }
    }

    /* Clean up. */
    for (size_t i = 0; i < info_count; ++i) {
        jz_buf_free(&infos[i].drivers);
        jz_buf_free(&infos[i].sinks);
    }
    jz_buf_free(&net_infos);
    jz_buf_free(&multi_driver_nets);

    /* Free allocated bus field display names. */
    size_t name_count = bus_field_name_ptrs.len / sizeof(char *);
    char **name_ptrs = (char **)bus_field_name_ptrs.data;
    for (size_t i = 0; i < name_count; ++i) {
        free(name_ptrs[i]);
    }
    jz_buf_free(&bus_field_name_ptrs);
}

void sem_emit_tristate_report_finalize(void)
{
    if (!g_tristate_report_enabled || !g_tristate_report_out) {
        jz_buf_free(&g_tristate_summary);
        return;
    }

    size_t entry_count = g_tristate_summary.len / sizeof(JZTristateSummaryEntry);
    if (entry_count == 0) {
        jz_buf_free(&g_tristate_summary);
        return;
    }

    FILE *out = g_tristate_report_out;
    JZTristateSummaryEntry *entries = (JZTristateSummaryEntry *)g_tristate_summary.data;

    fprintf(out, "\nOverall Summary\n");
    fprintf(out, "---------------\n");

    for (size_t i = 0; i < entry_count; ++i) {
        const char *result_str = "UNKNOWN";
        if (entries[i].worst_result == (int)JZ_TRISTATE_PROVEN) result_str = "PROVEN";
        else if (entries[i].worst_result == (int)JZ_TRISTATE_DISPROVEN) result_str = "DISPROVEN";

        fprintf(out, "%s, Possible Drivers: %zu, Result: %s\n",
                entries[i].name,
                entries[i].total_drivers,
                result_str);
    }

    jz_buf_free(&g_tristate_summary);
}
