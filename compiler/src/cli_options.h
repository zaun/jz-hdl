#ifndef JZ_HDL_CLI_OPTIONS_H
#define JZ_HDL_CLI_OPTIONS_H

#include <stddef.h>
#include <stdint.h>

#include "diagnostic.h"
#include "sim/sim_engine.h"

/**
 * @file cli_options.h
 * @brief Command-line option parsing for jz-hdl.
 */

/** Parsed command-line options. */
typedef struct JZCLIOptions {
    const char *input_filename;
    const char *output_filename;
    const char *mode;

    /* Constraint file paths. */
    const char *sdc_filename;
    const char *xdc_filename;
    const char *pcf_filename;
    const char *cst_filename;

    /* Boolean flags. */
    int lint_rules;
    int warn_as_error;
    int show_info;
    int use_color;
    int alias_report;
    int memory_report;
    int tristate_report;
    int test_mode;
    int simulate_mode;
    int verbose;
    int show_explain;
    int allow_absolute_paths;
    int allow_traversal;

    /* Chip info. */
    int chip_info;
    const char *chip_info_id;

    /* Tristate default: 0=none, 1=GND, 2=VCC */
    int tristate_default;

    /* Simulation format. */
    int sim_format_fst;
    int sim_format_jzw;

    /* Test seed. */
    uint32_t test_seed;
    int test_seed_set;

    /* Sandbox roots. */
    const char *sandbox_roots[16];
    size_t sandbox_root_count;

    /* Warning group overrides. */
    JZWarningGroupOverride group_overrides[16];
    size_t group_override_count;

    /* Simulation jitter/drift configs. */
    SimJitterConfig jitter_configs[16];
    int num_jitter;
    SimDriftConfig drift_configs[16];
    int num_drift;
} JZCLIOptions;

/**
 * Parse command-line arguments into a JZCLIOptions struct.
 * Returns 0 on success, 1 on error, or -1 if the program should exit
 * successfully (e.g. --help, --version).
 */
int jz_cli_parse_options(JZCLIOptions *opts, int argc, char **argv);

/** Print usage to stderr. */
void jz_cli_print_usage(const char *prog);

/** Print version to stdout. */
void jz_cli_print_version(void);

#endif /* JZ_HDL_CLI_OPTIONS_H */
