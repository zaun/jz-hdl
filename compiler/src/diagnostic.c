#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diagnostic.h"
#include "util.h"
#include "rules.h"

void jz_diagnostic_list_init(JZDiagnosticList *list)
{
    if (!list) return;
    memset(&list->buffer, 0, sizeof(list->buffer));
}

void jz_diagnostic_list_clear(JZDiagnosticList *list)
{
    if (!list) return;
    size_t count = list->buffer.len / sizeof(JZDiagnostic);
    JZDiagnostic *diags = (JZDiagnostic *)list->buffer.data;
    for (size_t i = 0; i < count; ++i) {
        free(diags[i].message);
    }
    list->buffer.len = 0;
}

void jz_diagnostic_list_free(JZDiagnosticList *list)
{
    if (!list) return;
    jz_diagnostic_list_clear(list);
    jz_buf_free(&list->buffer);
}

int jz_diagnostic_report(JZDiagnosticList *list,
                         JZLocation loc,
                         JZSeverity severity,
                         const char *code,
                         const char *message)
{
    if (!list || !message) return -1;

    JZDiagnostic diag;
    memset(&diag, 0, sizeof(diag));
    diag.loc = loc;
    diag.severity = severity;
    diag.code = code;
    diag.message = jz_strdup(message);
    if (!diag.message) return -1;

    return jz_buf_append(&list->buffer, &diag, sizeof(diag));
}

static void normalize_filename(const char *filename,
                               const char *primary_filename,
                               char *out,
                               size_t out_size)
{
    if (!out || out_size == 0) {
        return;
    }

    /* Default for NULL filenames. */
    if (!filename || !*filename) {
        snprintf(out, out_size, "<input>");
        return;
    }

    /* If we do not know the primary filename, just copy as-is. */
    if (!primary_filename || !*primary_filename) {
        snprintf(out, out_size, "%s", filename);
        return;
    }

    /* Derive the root directory from the primary filename (validator input). */
    const char *last_slash = strrchr(primary_filename, '/');
#ifdef _WIN32
    const char *last_backslash = strrchr(primary_filename, '\\');
    if (!last_slash || (last_backslash && last_backslash > last_slash)) {
        last_slash = last_backslash;
    }
#endif

    char root[512];
    if (last_slash) {
        size_t root_len = (size_t)(last_slash - primary_filename);
        if (root_len >= sizeof(root)) {
            root_len = sizeof(root) - 1;
        }
        memcpy(root, primary_filename, root_len);
        root[root_len] = '\0';
    } else {
        /* No directory component; treat current directory as root. */
        snprintf(root, sizeof(root), ".");
    }

    size_t root_len = strlen(root);

    /* If the diagnostic filename is under the root directory, strip that
     * prefix to make it project-relative. Otherwise, keep it as-is.
     *
     * When the primary filename is relative but the diagnostic filename is
     * absolute (e.g. from realpath() in import resolution), the prefix
     * match will fail. In that case, resolve the root directory to an
     * absolute path and retry.
     */
    if (root_len > 0 && strncmp(filename, root, root_len) == 0 &&
        (filename[root_len] == '/' || filename[root_len] == '\\')) {
        const char *rel = filename + root_len + 1; /* skip the separator */
        snprintf(out, out_size, "%s", rel && *rel ? rel : filename);
    } else if (filename[0] == '/' && root[0] != '/') {
        /* Root is relative but filename is absolute — resolve root. */
        char *abs_root = realpath(root, NULL);
        if (abs_root) {
            size_t abs_len = strlen(abs_root);
            if (abs_len > 0 && strncmp(filename, abs_root, abs_len) == 0 &&
                (filename[abs_len] == '/' || filename[abs_len] == '\\')) {
                const char *rel = filename + abs_len + 1;
                snprintf(out, out_size, "%s", rel && *rel ? rel : filename);
            } else {
                snprintf(out, out_size, "%s", filename);
            }
            free(abs_root);
        } else {
            snprintf(out, out_size, "%s", filename);
        }
    } else {
        snprintf(out, out_size, "%s", filename);
    }
}

static int get_rule_priority_for_diag(const JZDiagnostic *d)
{
    if (!d || !d->code) {
        return 0;
    }
    const JZRuleInfo *rule = jz_rule_lookup(d->code);
    if (!rule) {
        return 0;
    }
    return rule->priority;
}

static int compare_diag_location(const JZDiagnostic *a, const JZDiagnostic *b)
{
    const char *fa = a->loc.filename ? a->loc.filename : "<input>";
    const char *fb = b->loc.filename ? b->loc.filename : "<input>";

    int cmp = strcmp(fa, fb);
    if (cmp != 0) return cmp;

    if (a->loc.line != b->loc.line) {
        return (a->loc.line < b->loc.line) ? -1 : 1;
    }
    if (a->loc.column != b->loc.column) {
        return (a->loc.column < b->loc.column) ? -1 : 1;
    }
    return 0;
}

static int compare_diag_ptrs(const void *a, const void *b)
{
    const JZDiagnostic *const *da = (const JZDiagnostic *const *)a;
    const JZDiagnostic *const *db = (const JZDiagnostic *const *)b;

    int loc_cmp = compare_diag_location(*da, *db);
    if (loc_cmp != 0) {
        return loc_cmp;
    }

    /* For diagnostics at the same file/line/column, sort by rule priority so
     * that the highest-priority diagnostics appear first.
     */
    int pa = get_rule_priority_for_diag(*da);
    int pb = get_rule_priority_for_diag(*db);
    if (pa != pb) {
        return (pa > pb) ? -1 : 1; /* higher priority first */
    }

    return 0;
}

static void print_line(const JZDiagnostic *d,
                       const JZRuleInfo *rule,
                       FILE *out,
                       int use_color,
                       int show_explain)
{
    const char *dim        = "";
    const char *red        = "";
    const char *green      = "";
    const char *blue       = "";
    const char *light_blue = "";
    const char *white      = "";
    const char *reset      = "";

    if (use_color) {
        dim        = "\x1b[90m"; /* dark gray */
        red        = "\x1b[31m";
        green      = "\x1b[32m";
        blue       = "\x1b[34m";
        light_blue = "\x1b[94m"; /* bright blue for info */
        white      = "\x1b[37m";
        reset      = "\x1b[0m";
    }

    const char *sev_label = "note ";
    const char *sev_color = blue;

    switch (d->severity) {
    case JZ_SEVERITY_ERROR:
        sev_label = "error";
        sev_color = red;
        break;
    case JZ_SEVERITY_WARNING:
        sev_label = "warn ";
        sev_color = green;
        break;
    case JZ_SEVERITY_NOTE:
    default:
        sev_label = "note ";
        sev_color = blue;
        break;
    }

    const char *code_text = NULL;
    const char *desc_text = NULL;

    if (rule) {
        code_text = rule->id ? rule->id : "";
        /* Prefer the static rule description when available; otherwise fall back
         * to the diagnostic's message. This allows rules like CHECK_FAILED,
         * which intentionally have no table description, to surface their
         * dynamic message text (e.g., "CHECK FAILED: <msg>").
         */
        if (rule->description) {
            desc_text = rule->description;
        } else if (d->message) {
            desc_text = d->message;
        } else {
            desc_text = "";
        }

        /* JZ_RULE_MODE_INF: print as light-blue "info" when color is enabled. */
        if (rule->mode == JZ_RULE_MODE_INF) {
            sev_label = "info ";
            if (use_color) {
                sev_color = light_blue;
            }
        }
    } else {
        code_text = "MISSING_RULE_FIXME";
        desc_text = d->message ? d->message : "";
    }

    if (use_color) {
        /* Location in dark gray. */
        fprintf(out, "    %s%4d:%-3d%s  ",
                dim, d->loc.line, d->loc.column, reset);

        /* Severity word in color. */
        fprintf(out, "%s%s%s ", sev_color, sev_label, reset);

        /* Code in dark gray. */
        fprintf(out, "%s%s%s ", dim, code_text, reset);

        /* Description in white, possibly augmented with original code. */
        fprintf(out, "%s%s", white, desc_text);
        if (!rule && d->code) {
            fprintf(out, " (orig: %s)", d->code);
        }
        fprintf(out, "%s\n", reset);
    } else {
        fprintf(out, "    %4d:%-3d  %s %s %s",
                d->loc.line, d->loc.column,
                sev_label, code_text, desc_text);
        if (!rule && d->code) {
            fprintf(out, " (orig: %s)", d->code);
        }
        fprintf(out, "\n");
    }

    /* --explain: print the explanation underneath when it differs from the
     * rule description already shown on the main line.
     */
    if (show_explain && d->message && d->message[0]) {
        /* Skip if the explanation is the same as what was already printed. */
        if (!desc_text || strcmp(d->message, desc_text) != 0) {
            const char *indent = "                    ";
            if (use_color) fprintf(out, "%s", dim);
            const char *p = d->message;
            while (*p) {
                const char *nl = strchr(p, '\n');
                if (nl) {
                    fprintf(out, "%s%.*s\n", indent, (int)(nl - p), p);
                    p = nl + 1;
                } else {
                    fprintf(out, "%s%s\n", indent, p);
                    break;
                }
            }
            if (use_color) fprintf(out, "%s", reset);
        }
    }
}

static void print_one(const JZDiagnostic *d, FILE *out, int use_color,
                       int show_explain)
{
    const JZRuleInfo *rule = NULL;
    if (d->code) {
        rule = jz_rule_lookup(d->code);
    }
    print_line(d, rule, out, use_color, show_explain);
}

void jz_diagnostic_apply_warning_policy(JZDiagnosticList *list,
                                        const JZWarningPolicy *policy)
{
    if (!list || !policy) return;

    size_t count = list->buffer.len / sizeof(JZDiagnostic);
    if (count == 0) return;

    JZDiagnostic *diags = (JZDiagnostic *)list->buffer.data;

    /* First, filter out diagnostics from disabled warning groups. Errors are
     * never suppressed by this mechanism.
     */
    if (policy->groups && policy->group_count > 0) {
        JZBuffer filtered = {0};
        for (size_t i = 0; i < count; ++i) {
            JZDiagnostic *d = &diags[i];

            int suppress = 0;
            if (d->severity == JZ_SEVERITY_WARNING && d->code) {
                const JZRuleInfo *rule = jz_rule_lookup(d->code);
                if (rule && rule->group) {
                    for (size_t g = 0; g < policy->group_count; ++g) {
                        const JZWarningGroupOverride *ov = &policy->groups[g];
                        if (!ov->group) continue;
                        if (strcmp(ov->group, rule->group) == 0) {
                            if (!ov->enabled) {
                                suppress = 1;
                            }
                            break;
                        }
                    }
                }
            }

            if (suppress) {
                free(d->message);
                continue;
            }

            (void)jz_buf_append(&filtered, d, sizeof(*d));
        }

        jz_buf_free(&list->buffer);
        list->buffer = filtered;

        diags = (JZDiagnostic *)list->buffer.data;
        count = list->buffer.len / sizeof(JZDiagnostic);
    }

    /* Optionally promote remaining warnings to errors for exit-status
     * purposes and for display.
     */
    if (policy->warn_as_error) {
        for (size_t i = 0; i < count; ++i) {
            JZDiagnostic *d = &diags[i];
            if (d->severity == JZ_SEVERITY_WARNING) {
                d->severity = JZ_SEVERITY_ERROR;
            }
        }
    }
}

int jz_diagnostic_has_severity(const JZDiagnosticList *list,
                               JZSeverity severity)
{
    if (!list) return 0;
    size_t count = list->buffer.len / sizeof(JZDiagnostic);
    const JZDiagnostic *diags = (const JZDiagnostic *)list->buffer.data;
    for (size_t i = 0; i < count; ++i) {
        if (diags[i].severity >= severity) {
            return 1;
        }
    }
    return 0;
}

void jz_diagnostic_print_all(const JZDiagnosticList *list,
                             FILE *out,
                             int use_color,
                             const char *primary_filename,
                             int show_info,
                             int show_explain)
{
    if (!list || !out) return;

    size_t count = list->buffer.len / sizeof(JZDiagnostic);
    const JZDiagnostic *diags = (const JZDiagnostic *)list->buffer.data;
    if (count == 0) return;

    const JZDiagnostic **order =
        (const JZDiagnostic **)malloc(count * sizeof(const JZDiagnostic *));
    if (!order) {
        /* Fallback: print in insertion order without grouping. */
        for (size_t i = 0; i < count; ++i) {
            const JZDiagnostic *d = &diags[i];
            if (!show_info && d->code) {
                const JZRuleInfo *rule = jz_rule_lookup(d->code);
                if (rule && rule->mode == JZ_RULE_MODE_INF) {
                    continue;
                }
            }
            print_one(d, out, use_color, show_explain);
        }
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        order[i] = &diags[i];
    }

    qsort(order, count, sizeof(const JZDiagnostic *), compare_diag_ptrs);

    char last_file[512];
    last_file[0] = '\0';
    int first_file = 1;

    size_t i = 0;
    while (i < count) {
        /* Group diagnostics by file and line (column differences are ignored
         * for priority resolution).
         */
        const JZDiagnostic *base = order[i];
        const char *base_file = base->loc.filename ? base->loc.filename : "<input>";
        int base_line = base->loc.line;

        size_t j = i + 1;
        while (j < count) {
            const JZDiagnostic *cur = order[j];
            const char *cur_file = cur->loc.filename ? cur->loc.filename : "<input>";
            if (strcmp(base_file, cur_file) != 0 || cur->loc.line != base_line) {
                break;
            }
            ++j;
        }

        /* Determine the maximum priority among diagnostics on this line. */
        int max_priority = 0;
        for (size_t k = i; k < j; ++k) {
            int p = get_rule_priority_for_diag(order[k]);
            if (p > max_priority) {
                max_priority = p;
            }
        }

        /* Compute and emit the file header if this is the first diagnostic for
         * this (normalized) file.
         */
        char norm[512];
        normalize_filename(base->loc.filename, primary_filename, norm, sizeof(norm));
        if (first_file || strcmp(norm, last_file) != 0) {
            if (!first_file) {
                fputc('\n', out);
            }
            fprintf(out, "File: %s\n", norm);
            strncpy(last_file, norm, sizeof(last_file));
            last_file[sizeof(last_file) - 1] = '\0';
            first_file = 0;
        }

        /* Within this line, print only diagnostics whose rule priority equals
         * max_priority, and remove duplicates (same column, code, and message).
         */
        for (size_t k = i; k < j; ++k) {
            const JZDiagnostic *d = order[k];
            if (get_rule_priority_for_diag(d) != max_priority) {
                continue;
            }

            int is_duplicate = 0;
            for (size_t m = i; m < k; ++m) {
                const JZDiagnostic *prev = order[m];
                if (get_rule_priority_for_diag(prev) != max_priority) {
                    continue;
                }
                if (prev->loc.column != d->loc.column) {
                    continue;
                }
                if (prev->code == d->code ||
                    (prev->code && d->code && strcmp(prev->code, d->code) == 0)) {
                    const char *msg_a = prev->message ? prev->message : "";
                    const char *msg_b = d->message ? d->message : "";
                    if (strcmp(msg_a, msg_b) == 0) {
                        is_duplicate = 1;
                        break;
                    }
                }
            }

            if (!is_duplicate) {
                if (!show_info && d->code) {
                    const JZRuleInfo *rule = jz_rule_lookup(d->code);
                    if (rule && rule->mode == JZ_RULE_MODE_INF) {
                        continue;
                    }
                }
                print_one(d, out, use_color, show_explain);
            }
        }

        i = j;
    }

    free(order);
}
