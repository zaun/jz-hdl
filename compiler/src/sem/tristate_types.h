#ifndef JZ_HDL_TRISTATE_TYPES_H
#define JZ_HDL_TRISTATE_TYPES_H

#include "ast.h"
#include "util.h"

/* -------------------------------------------------------------------------
 *  Tri-State Analysis Data Structures
 *
 *  Shared between the proof engine (driver_tristate.c) and the report
 *  formatter (tristate_report.c).
 * -------------------------------------------------------------------------
 */

/* Result classification for tri-state resolution analysis. */
typedef enum JZTristateResult {
    JZ_TRISTATE_PROVEN,     /* Mutual exclusion proven - safe */
    JZ_TRISTATE_DISPROVEN,  /* Conflict detected - multiple drivers possible */
    JZ_TRISTATE_UNKNOWN     /* Cannot determine - analysis incomplete */
} JZTristateResult;

/* Reason codes for UNKNOWN classification. */
typedef enum JZTristateUnknownReason {
    JZ_TRISTATE_UNKNOWN_BLACKBOX,           /* Blackbox module output */
    JZ_TRISTATE_UNKNOWN_UNCONSTRAINED_INPUT, /* Primary input not constrained */
    JZ_TRISTATE_UNKNOWN_COMPLEX_EXPR,        /* Expression too complex to analyze */
    JZ_TRISTATE_UNKNOWN_MULTI_CLOCK,         /* Cross clock domain */
    JZ_TRISTATE_UNKNOWN_NO_GUARD             /* No guard condition found */
} JZTristateUnknownReason;

/* Method used to prove mutual exclusion. */
typedef enum JZTristateProofMethod {
    JZ_TRISTATE_PROOF_SINGLE_DRIVER,        /* Only one driver exists */
    JZ_TRISTATE_PROOF_DISTINCT_CONSTANTS,   /* Different constant inputs */
    JZ_TRISTATE_PROOF_COMPLEMENTARY_GUARDS, /* Guards are mutually exclusive */
    JZ_TRISTATE_PROOF_SINGLE_NON_Z,         /* Only one driver produces non-z */
    JZ_TRISTATE_PROOF_IF_ELSE_BRANCHES,     /* Drivers in complementary IF/ELSE branches */
    JZ_TRISTATE_PROOF_PAIRWISE              /* All driver pairs proven mutually exclusive independently */
} JZTristateProofMethod;

/* Maximum number of IF/ELSE guard terms stored per driver. */
#define JZ_MAX_GUARD_TERMS 8

/* Maximum number of comparison terms extracted from an AND chain. */
#define JZ_MAX_COMPARE_TERMS 8

/* Maximum number of alias entries stored per driver. */
#define JZ_MAX_DRIVER_ALIASES 8

/* A single comparison term extracted from a guard condition (e.g., pbus.CMD == CMD.READ). */
typedef struct JZCompareTerm {
    const char *input_name;      /* Signal name (e.g., "pbus.CMD") */
    const char *compare_value;   /* Compared value (e.g., "CMD.READ") */
    int         is_inverted;     /* Non-zero if NE comparison */
} JZCompareTerm;

/* Enable condition extracted from a driver. */
typedef struct JZDriverEnable {
    const char *input_name;       /* Input identifier being compared */
    const char *compare_value;    /* Literal value being compared to */
    int         is_inverted;      /* Non-zero if condition is inverted */
    int         is_complex;       /* Non-zero if condition involves complex expressions */
    char        normalized_lhs[64]; /* Formatted LHS of comparison for display */
    char        normalized_rhs[64]; /* Formatted RHS of comparison for display */
    char        condition_text[256]; /* Full human-readable condition string */
    JZLocation  loc;              /* Source location */
    size_t      n_guard_terms;    /* Number of IF/ELSE guard terms */
    struct {
        const void *expr;         /* JZASTNode pointer for the condition expression */
        int         neg;          /* Non-zero if this term is negated (ELSE branch) */
    } guard_terms[JZ_MAX_GUARD_TERMS];
    size_t         n_compare_terms;                    /* Number of comparison terms */
    JZCompareTerm  compare_terms[JZ_MAX_COMPARE_TERMS]; /* Multi-term comparisons from AND chains */
} JZDriverEnable;

/* Information about a single driver of a tri-state net. */
typedef struct JZTristateDriver {
    JZASTNode      *stmt;             /* Driver statement or instance node */
    JZLocation      loc;              /* Source location */
    int             can_produce_z;    /* Non-zero if can output high-impedance */
    int             can_produce_non_z; /* Non-zero if can output non-z values */
    JZDriverEnable  enable;           /* Enable condition info */
    const char     *source_snippet;   /* Optional source code snippet */
    const char     *instance_name;    /* Instance name if module instance */
    const char     *module_name;      /* Module name if module instance */
    const char     *port_name;        /* Port name if bound through port */
    size_t          n_aliases;        /* Number of alias entries */
    struct {
        const char *from;             /* Register name (e.g., "bus_cmd") */
        const char *to;               /* Qualified port field (e.g., "pbus.CMD") */
    } aliases[JZ_MAX_DRIVER_ALIASES];
} JZTristateDriver;

/* Information about a sink (reader) of a tri-state net. */
typedef struct JZTristateSink {
    JZASTNode  *stmt;      /* Sink statement node */
    JZLocation  loc;       /* Source location */
    const char *snippet;   /* Optional source code snippet */
} JZTristateSink;

/* Witness for a potential conflict (for DISPROVEN case). */
typedef struct JZConflictWitness {
    size_t     driver_a;   /* Index of first conflicting driver */
    size_t     driver_b;   /* Index of second conflicting driver */
    const char *reason;    /* Human-readable reason for conflict */
} JZConflictWitness;

/* Complete information about a multi-driver net for the report. */
typedef struct JZTristateNetInfo {
    const char        *name;           /* Net name */
    unsigned           width;          /* Bit width */
    JZLocation         decl_loc;       /* Declaration location */
    JZBuffer           drivers;        /* Array of JZTristateDriver */
    JZBuffer           sinks;          /* Array of JZTristateSink */
    JZTristateResult   result;         /* Analysis result */
    JZTristateProofMethod proof_method; /* How it was proven (if PROVEN) */
    JZTristateUnknownReason unknown_reason; /* Why unknown (if UNKNOWN) */
    JZConflictWitness  conflict;       /* Conflict info (if DISPROVEN) */
} JZTristateNetInfo;

/* Guard condition info used during analysis. */
typedef struct JZTristateGuardInfo {
    const char *input_name;
    const char *compare_lit;
    int         is_inverted;
    char        normalized_lhs[64];
    char        normalized_rhs[64];
    char        condition_text[256];
    size_t      n_guard_terms;
    struct {
        const JZASTNode *expr;
        int              neg;
    } guard_terms[JZ_MAX_GUARD_TERMS];
    size_t         n_compare_terms;
    JZCompareTerm  compare_terms[JZ_MAX_COMPARE_TERMS];
    size_t         n_aliases;
    struct {
        const char *from;
        const char *to;
    } aliases[JZ_MAX_DRIVER_ALIASES];
} JZTristateGuardInfo;

#endif /* JZ_HDL_TRISTATE_TYPES_H */
