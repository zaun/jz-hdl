/**
 * @file rules.h
 * @brief Static metadata and lookup for validation rules.
 *
 * Provides access to rule definitions loaded from rules.ini, including
 * rule grouping, severity mode, and descriptions. Rules are organized
 * by group (e.g., "LITERALS_AND_TYPES") with individual IDs, modes
 * (error/warning/info), and human-readable descriptions.
 *
 * Usage:
 *   const JZRuleInfo *info = jz_rule_lookup("LIT_OVERFLOW");
 *   jz_rules_print_all(stdout);  // Print all rules grouped by category
 */

#ifndef JZ_HDL_RULES_H
#define JZ_HDL_RULES_H

#include <stddef.h>
#include <stdio.h>

/**
 * @enum JZRuleMode
 * @brief Severity mode for a validation rule.
 */
typedef enum JZRuleMode {
    JZ_RULE_MODE_ERR = 0,  /* Error severity. */
    JZ_RULE_MODE_WRN = 1,  /* Warning severity. */
    JZ_RULE_MODE_INF = 2   /* Info severity. */
} JZRuleMode;

/**
 * @struct JZRuleInfo
 * @brief Metadata for a single validation rule.
 */
typedef struct JZRuleInfo {
    const char *group;        /* Section header from rules.ini (e.g., "LITERALS_AND_TYPES"). */
    const char *id;           /* Rule identifier (e.g., "LIT_OVERFLOW"). */
    int         priority;     /* Rule priority value. */
    JZRuleMode  mode;         /* Severity mode (ERR, WRN, or INF). */
    const char *description;  /* Human-readable description from rules.ini. */
} JZRuleInfo;

/**
 * @brief Table of all defined validation rules.
 */
extern const JZRuleInfo jz_rule_table[];

/**
 * @brief Number of rules in the rule table.
 */
extern const size_t jz_rule_table_count;

/**
 * @brief Look up a rule by identifier.
 * @param id Rule identifier (e.g., "LIT_OVERFLOW").
 * @return Pointer to rule metadata, or NULL if not found.
 */
const JZRuleInfo *jz_rule_lookup(const char *id);

/**
 * @brief Print all linter rules grouped by category.
 * @param out Output stream (e.g., stdout, stderr).
 */
void jz_rules_print_all(FILE *out);

#endif /* JZ_HDL_RULES_H */
