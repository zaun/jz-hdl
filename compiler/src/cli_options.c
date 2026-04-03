#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli_options.h"
#include "version.h"

void jz_cli_print_usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s JZ_FILE --lint [--warn-as-error] [--color] [--info] [--explain] [--Wno-group=NAME] [--tristate-default=GND|VCC] [-o OUT_FILE]\n"
            "       %s JZ_FILE --verilog [-o OUT_FILE] [--sdc SDC_FILE] [--xdc XDC_FILE] [--pcf PCF_FILE] [--cst CST_FILE] [--tristate-default=GND|VCC]\n"
            "       %s JZ_FILE --rtlil [-o OUT_FILE] [--sdc SDC_FILE] [--xdc XDC_FILE] [--pcf PCF_FILE] [--cst CST_FILE] [--tristate-default=GND|VCC]\n"
            "       %s JZ_FILE --alias-report [-o OUT_FILE]\n"
            "       %s JZ_FILE --memory-report [-o OUT_FILE]\n"
            "       %s JZ_FILE --tristate-report [-o OUT_FILE]\n"
            "       %s JZ_FILE --ast [-o OUT_FILE]\n"
            "       %s JZ_FILE --ir [-o OUT_FILE] [--tristate-default=GND|VCC]\n"
            "       %s JZ_FILE --test [--verbose] [--seed=0xHEX] [--tristate-default=GND|VCC]\n"
            "       %s JZ_FILE --simulate [-o WAVEFORM_FILE] [--vcd] [--fst] [--jzw] [--verbose] [--seed=0xHEX] [--tristate-default=GND|VCC]\n"
            "       %s --chip-info [CHIP_ID] [-o OUT_FILE]\n"
            "       %s --lint-rules\n"
            "       %s --lsp\n"
            "       %s --help\n"
            "       %s --version\n"
            "\n"
            "Path security options:\n"
            "  --sandbox-root=<dir>     Add permitted root directory for file access\n"
            "  --allow-absolute-paths   Allow absolute paths in @import / @file()\n"
            "  --allow-traversal        Allow '..' directory traversal in paths\n",
            prog, prog, prog, prog, prog, prog, prog, prog, prog, prog, prog, prog, prog, prog, prog);
}

void jz_cli_print_version(void) {
    fprintf(stdout, "%s\n", JZ_HDL_VERSION_STRING);
}

int jz_cli_parse_options(JZCLIOptions *opts, int argc, char **argv) {
    memset(opts, 0, sizeof(*opts));

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (strcmp(arg, "--help") == 0) {
            jz_cli_print_usage(argv[0]);
            return -1;
        } else if (strcmp(arg, "--version") == 0) {
            jz_cli_print_version();
            return -1;
        } else if (strcmp(arg, "--warn-as-error") == 0) {
            opts->warn_as_error = 1;
        } else if (strcmp(arg, "--info") == 0) {
            opts->show_info = 1;
        } else if (strcmp(arg, "--color") == 0) {
            opts->use_color = 1;
        } else if (strcmp(arg, "--explain") == 0) {
            opts->show_explain = 1;
        } else if (strcmp(arg, "--alias-report") == 0) {
            opts->alias_report = 1;
        } else if (strcmp(arg, "--memory-report") == 0) {
            opts->memory_report = 1;
        } else if (strcmp(arg, "--tristate-report") == 0) {
            opts->tristate_report = 1;
        } else if (strcmp(arg, "--chip-info") == 0) {
            opts->chip_info = 1;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                opts->chip_info_id = argv[++i];
            }
        } else if (strcmp(arg, "--test") == 0) {
            opts->test_mode = 1;
        } else if (strcmp(arg, "--simulate") == 0) {
            opts->simulate_mode = 1;
        } else if (strcmp(arg, "--vcd") == 0) {
            opts->sim_format_fst = 0;
            opts->sim_format_jzw = 0;
        } else if (strcmp(arg, "--fst") == 0) {
            opts->sim_format_fst = 1;
            opts->sim_format_jzw = 0;
        } else if (strcmp(arg, "--jzw") == 0) {
            opts->sim_format_jzw = 1;
            opts->sim_format_fst = 0;
        } else if (strcmp(arg, "--verbose") == 0) {
            opts->verbose = 1;
        } else if (strncmp(arg, "--seed=", 7) == 0) {
            opts->test_seed = (uint32_t)strtoul(arg + 7, NULL, 16);
            opts->test_seed_set = 1;
        } else if (strncmp(arg, "--jitter=", 9) == 0) {
            if (opts->num_jitter >= (int)(sizeof(opts->jitter_configs) / sizeof(opts->jitter_configs[0]))) {
                fprintf(stderr, "Too many --jitter flags (max %d)\n",
                        (int)(sizeof(opts->jitter_configs) / sizeof(opts->jitter_configs[0])));
                return 1;
            }
            const char *val = arg + 9;
            const char *semi = strchr(val, ':');
            if (!semi || semi == val || *(semi + 1) == '\0') {
                fprintf(stderr, "Invalid --jitter format: '%s' (expected --jitter=<clock>:<ps>)\n", arg);
                return 1;
            }
            size_t name_len = (size_t)(semi - val);
            char *name_copy = malloc(name_len + 1);
            memcpy(name_copy, val, name_len);
            name_copy[name_len] = '\0';
            uint64_t pp = strtoull(semi + 1, NULL, 10);
            if (pp == 0) {
                fprintf(stderr, "Invalid --jitter value: '%s' (peak-to-peak must be > 0)\n", semi + 1);
                free(name_copy);
                return 1;
            }
            opts->jitter_configs[opts->num_jitter].clock_name = name_copy;
            opts->jitter_configs[opts->num_jitter].pp_ps = pp;
            opts->num_jitter++;
        } else if (strncmp(arg, "--drift=", 8) == 0) {
            if (opts->num_drift >= (int)(sizeof(opts->drift_configs) / sizeof(opts->drift_configs[0]))) {
                fprintf(stderr, "Too many --drift flags (max %d)\n",
                        (int)(sizeof(opts->drift_configs) / sizeof(opts->drift_configs[0])));
                return 1;
            }
            const char *val = arg + 8;
            const char *colon = strchr(val, ':');
            if (!colon || colon == val || *(colon + 1) == '\0') {
                fprintf(stderr, "Invalid --drift format: '%s' (expected --drift=<clock>:<ppm>)\n", arg);
                return 1;
            }
            size_t name_len = (size_t)(colon - val);
            char *name_copy = malloc(name_len + 1);
            memcpy(name_copy, val, name_len);
            name_copy[name_len] = '\0';
            double ppm = strtod(colon + 1, NULL);
            if (ppm <= 0.0) {
                fprintf(stderr, "Invalid --drift value: '%s' (ppm must be > 0)\n", colon + 1);
                free(name_copy);
                return 1;
            }
            opts->drift_configs[opts->num_drift].clock_name = name_copy;
            opts->drift_configs[opts->num_drift].max_ppm = ppm;
            opts->num_drift++;
        } else if (strcmp(arg, "--lint-rules") == 0) {
            opts->lint_rules = 1;
        } else if (strcmp(arg, "--sdc") == 0 ||
                   strcmp(arg, "--xdc") == 0 ||
                   strcmp(arg, "--pcf") == 0 ||
                   strcmp(arg, "--cst") == 0) {
            const char **target = NULL;
            if (strcmp(arg, "--sdc") == 0) {
                target = &opts->sdc_filename;
            } else if (strcmp(arg, "--xdc") == 0) {
                target = &opts->xdc_filename;
            } else if (strcmp(arg, "--pcf") == 0) {
                target = &opts->pcf_filename;
            } else {
                target = &opts->cst_filename;
            }
            if (i + 1 >= argc) {
                fprintf(stderr, "Missing filename after %s\n", arg);
                return 1;
            }
            if (*target) {
                fprintf(stderr, "%s specified more than once\n", arg);
                return 1;
            }
            *target = argv[++i];
        } else if (strcmp(arg, "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Missing output filename after -o\n");
                return 1;
            }
            if (opts->output_filename) {
                fprintf(stderr, "-o specified more than once\n");
                return 1;
            }
            opts->output_filename = argv[++i];
        } else if (strncmp(arg, "--tristate-default=", 19) == 0) {
            const char *val = arg + 19;
            if (strcmp(val, "GND") == 0) {
                opts->tristate_default = 1;
            } else if (strcmp(val, "VCC") == 0) {
                opts->tristate_default = 2;
            } else {
                fprintf(stderr, "Invalid value for --tristate-default: '%s' (expected GND or VCC)\n", val);
                return 1;
            }
        } else if (strncmp(arg, "--Wno-group=", 12) == 0) {
            const char *group = arg + 12;
            if (*group && opts->group_override_count < sizeof(opts->group_overrides) / sizeof(opts->group_overrides[0])) {
                opts->group_overrides[opts->group_override_count].group = group;
                opts->group_overrides[opts->group_override_count].enabled = 0;
                opts->group_override_count++;
            }
        } else if (strncmp(arg, "--Wgroup=", 9) == 0) {
            const char *group = arg + 9;
            if (*group && opts->group_override_count < sizeof(opts->group_overrides) / sizeof(opts->group_overrides[0])) {
                opts->group_overrides[opts->group_override_count].group = group;
                opts->group_overrides[opts->group_override_count].enabled = 1;
                opts->group_override_count++;
            }
        } else if (strcmp(arg, "--allow-absolute-paths") == 0) {
            opts->allow_absolute_paths = 1;
        } else if (strcmp(arg, "--allow-traversal") == 0) {
            opts->allow_traversal = 1;
        } else if (strncmp(arg, "--sandbox-root=", 15) == 0) {
            const char *val = arg + 15;
            if (*val && opts->sandbox_root_count < sizeof(opts->sandbox_roots) / sizeof(opts->sandbox_roots[0])) {
                opts->sandbox_roots[opts->sandbox_root_count++] = val;
            }
        } else if (strcmp(arg, "--ast") == 0 || strcmp(arg, "--lint") == 0 ||
                   strcmp(arg, "--verilog") == 0 || strcmp(arg, "--emit-verilog") == 0 ||
                   strcmp(arg, "--rtlil") == 0 || strcmp(arg, "--emit-rtlil") == 0 ||
                   strcmp(arg, "--ir") == 0) {
            if (opts->mode && strcmp(opts->mode, arg) != 0) {
                fprintf(stderr, "Multiple modes specified (%s and %s)\n", opts->mode, arg);
                return 1;
            }
            opts->mode = arg;
        } else if (arg[0] == '-' && arg[1] != '\0') {
            fprintf(stderr, "Unknown option: %s\n", arg);
            return 1;
        } else {
            if (opts->input_filename) {
                fprintf(stderr, "Multiple input files specified (%s and %s)\n", opts->input_filename, arg);
                return 1;
            }
            opts->input_filename = arg;
        }
    }

    return 0;
}
