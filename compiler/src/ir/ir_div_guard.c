/**
 * @file ir_div_guard.c
 * @brief IR-level division guard proof pass.
 *
 * Walks all statement trees in the IR and checks that every DIV/MOD
 * expression whose divisor is not a compile-time nonzero literal is
 * guarded by an enclosing IF (divisor != 0) condition. Unguarded
 * divisions emit DIV_UNGUARDED_RUNTIME_ZERO as a warning.
 */

#include <stdbool.h>
#include <string.h>

#include "ir.h"
#include "ir_builder.h"
#include "diagnostic.h"
#include "rules.h"

/* -----------------------------------------------------------------------
 * Guard stack: tracks known-nonzero signal IDs from enclosing IF branches.
 * ----------------------------------------------------------------------- */

typedef struct {
    int  signal_id;   /* Signal proven nonzero in this scope. */
    bool is_nonzero;  /* true = signal != 0 (THEN), false = signal == 0 (ELSE). */
} GuardEntry;

#define MAX_GUARD_DEPTH 64

typedef struct {
    GuardEntry entries[MAX_GUARD_DEPTH];
    int        count;
} GuardStack;

static void guard_push(GuardStack *gs, int signal_id, bool is_nonzero)
{
    if (gs->count < MAX_GUARD_DEPTH) {
        gs->entries[gs->count].signal_id = signal_id;
        gs->entries[gs->count].is_nonzero = is_nonzero;
        gs->count++;
    }
}

static void guard_pop(GuardStack *gs)
{
    if (gs->count > 0) {
        gs->count--;
    }
}

static bool guard_is_nonzero(const GuardStack *gs, int signal_id)
{
    for (int i = gs->count - 1; i >= 0; i--) {
        if (gs->entries[i].signal_id == signal_id && gs->entries[i].is_nonzero) {
            return true;
        }
    }
    return false;
}

/* -----------------------------------------------------------------------
 * Expression helpers.
 * ----------------------------------------------------------------------- */

/** Return true if expr is a literal with value 0. */
static bool expr_is_zero_literal(const IR_Expr *expr)
{
    if (!expr) return false;
    if (expr->kind != EXPR_LITERAL) return false;
    if (expr->u.literal.literal.is_z) return false;
    for (int i = 0; i < IR_LIT_WORDS; i++) {
        if (expr->u.literal.literal.words[i] != 0) return false;
    }
    return true;
}

/** Return true if expr is a literal with a nonzero value. */
static bool expr_is_nonzero_literal(const IR_Expr *expr)
{
    if (!expr) return false;
    if (expr->kind != EXPR_LITERAL) return false;
    if (expr->u.literal.literal.is_z) return false;
    for (int i = 0; i < IR_LIT_WORDS; i++) {
        if (expr->u.literal.literal.words[i] != 0) return true;
    }
    return false;
}

/** Return the signal_id if expr is a plain signal ref, -1 otherwise. */
static int expr_signal_id(const IR_Expr *expr)
{
    if (!expr) return -1;
    if (expr->kind != EXPR_SIGNAL_REF) return -1;
    return expr->u.signal_ref.signal_id;
}

/* -----------------------------------------------------------------------
 * Guard condition extraction.
 *
 * Given an IF condition that compares a signal to a literal, determines
 * whether the THEN branch and/or ELSE branch prove the signal is nonzero.
 * All JZ-HDL bit-vectors are unsigned, so the analysis uses unsigned
 * arithmetic throughout.
 *
 * Supported patterns (and their commuted forms):
 *   sig != N      THEN: nonzero if N==0.  ELSE: nonzero if N!=0.
 *   sig == N      THEN: nonzero if N!=0.  ELSE: nonzero if N==0.
 *   sig >  N      THEN: always nonzero.   ELSE (sig <= N): nonzero if N==0? no, == 0 possible.
 *   sig >= N      THEN: nonzero if N>=1.  ELSE (sig < N):  nonzero? only if N<=0, impossible for N>=1.
 *   sig <  N      THEN: maybe zero.       ELSE (sig >= N): nonzero if N>=1.
 *   sig <= N      THEN: maybe zero.       ELSE (sig > N):  always nonzero (unsigned > any N).
 *
 * Returns true if a guard pattern was matched.
 * ----------------------------------------------------------------------- */

/** Return the literal value if expr is a non-z literal, or -1. */
static int64_t expr_literal_value(const IR_Expr *expr, bool *ok)
{
    *ok = false;
    if (!expr) return -1;
    if (expr->kind != EXPR_LITERAL) return -1;
    if (expr->u.literal.literal.is_z) return -1;
    *ok = true;
    return (int64_t)expr->u.literal.literal.words[0];
}

/**
 * Normalized comparison: signal <op> literal.
 * We canonicalize so the signal is always on the left.
 */
typedef enum {
    CMP_EQ,   /* == */
    CMP_NEQ,  /* != */
    CMP_GT,   /* >  */
    CMP_GTE,  /* >= */
    CMP_LT,   /* <  */
    CMP_LTE   /* <= */
} NormCmpOp;

static NormCmpOp flip_cmp(NormCmpOp op)
{
    switch (op) {
    case CMP_EQ:  return CMP_EQ;
    case CMP_NEQ: return CMP_NEQ;
    case CMP_GT:  return CMP_LT;
    case CMP_GTE: return CMP_LTE;
    case CMP_LT:  return CMP_GT;
    case CMP_LTE: return CMP_GTE;
    }
    return op;
}

static bool normalize_cmp(const IR_Expr *cond,
                           int *out_signal_id,
                           NormCmpOp *out_op,
                           uint64_t *out_value)
{
    if (!cond) return false;

    NormCmpOp op;
    switch (cond->kind) {
    case EXPR_BINARY_EQ:  op = CMP_EQ;  break;
    case EXPR_BINARY_NEQ: op = CMP_NEQ; break;
    case EXPR_BINARY_GT:  op = CMP_GT;  break;
    case EXPR_BINARY_GTE: op = CMP_GTE; break;
    case EXPR_BINARY_LT:  op = CMP_LT;  break;
    case EXPR_BINARY_LTE: op = CMP_LTE; break;
    default: return false;
    }

    /* Try signal on left, literal on right. */
    int sig = expr_signal_id(cond->u.binary.left);
    bool lit_ok = false;
    int64_t val = expr_literal_value(cond->u.binary.right, &lit_ok);
    if (sig >= 0 && lit_ok) {
        *out_signal_id = sig;
        *out_op = op;
        *out_value = (uint64_t)val;
        return true;
    }

    /* Try literal on left, signal on right — flip the operator. */
    sig = expr_signal_id(cond->u.binary.right);
    val = expr_literal_value(cond->u.binary.left, &lit_ok);
    if (sig >= 0 && lit_ok) {
        *out_signal_id = sig;
        *out_op = flip_cmp(op);
        *out_value = (uint64_t)val;
        return true;
    }

    return false;
}

static bool extract_nonzero_guard(const IR_Expr *cond,
                                  int *out_signal_id,
                                  bool *out_then_nonzero,
                                  bool *out_else_nonzero)
{
    int sig = -1;
    NormCmpOp op;
    uint64_t val = 0;

    if (!normalize_cmp(cond, &sig, &op, &val))
        return false;

    *out_signal_id = sig;
    *out_then_nonzero = false;
    *out_else_nonzero = false;

    /* Determine what each branch proves about sig being nonzero.
     * All values are unsigned. */
    switch (op) {
    case CMP_NEQ:
        /* sig != N:  THEN → sig can be anything except N.
         *            ELSE → sig == N. */
        if (val == 0)
            *out_then_nonzero = true;   /* sig != 0 → nonzero */
        else
            *out_else_nonzero = true;   /* sig == N (N!=0) → nonzero */
        break;

    case CMP_EQ:
        /* sig == N:  THEN → sig == N.
         *            ELSE → sig != N. */
        if (val != 0)
            *out_then_nonzero = true;   /* sig == N (N!=0) → nonzero */
        else
            *out_else_nonzero = true;   /* sig != 0 → nonzero */
        break;

    case CMP_GT:
        /* sig > N (unsigned):  THEN → sig >= N+1 >= 1 → always nonzero.
         *                      ELSE → sig <= N → could be 0 unless N < 0 (impossible unsigned). */
        *out_then_nonzero = true;
        /* ELSE: sig <= N.  Only nonzero if max possible is nonzero AND min is nonzero.
         * sig <= 0 means sig == 0, so not nonzero. sig <= N (N>=1) includes 0. */
        break;

    case CMP_GTE:
        /* sig >= N:  THEN → nonzero if N >= 1.
         *            ELSE → sig < N → could include 0 unless N <= 0 (impossible for useful guard). */
        if (val >= 1)
            *out_then_nonzero = true;
        /* ELSE: sig < N.  If N == 1, sig < 1 means sig == 0, not nonzero.
         *                 If N == 0, sig < 0 is impossible (unsigned), so ELSE is dead. */
        break;

    case CMP_LT:
        /* sig < N:  THEN → could be 0 (e.g. sig < 5 includes 0).
         *           ELSE → sig >= N → nonzero if N >= 1. */
        if (val >= 1)
            *out_else_nonzero = true;
        break;

    case CMP_LTE:
        /* sig <= N:  THEN → could be 0.
         *            ELSE → sig > N → sig >= N+1 >= 1 → always nonzero. */
        *out_else_nonzero = true;
        break;
    }

    return *out_then_nonzero || *out_else_nonzero;
}

/* -----------------------------------------------------------------------
 * Diagnostic reporting helper.
 * ----------------------------------------------------------------------- */

static void report_unguarded_div(JZDiagnosticList *diagnostics,
                                 const IR_Design *design,
                                 const IR_Module *mod,
                                 int source_line)
{
    const char *filename = "";
    if (mod->source_file_id >= 0 &&
        mod->source_file_id < design->num_source_files) {
        filename = design->source_files[mod->source_file_id].path;
    }
    JZLocation loc = { filename, source_line, 0 };

    const JZRuleInfo *rule = jz_rule_lookup("DIV_UNGUARDED_RUNTIME_ZERO");
    JZSeverity sev = JZ_SEVERITY_WARNING;
    const char *msg = "divisor may be zero at runtime; guard with IF (divisor != 0) or use a compile-time constant";

    if (rule) {
        switch (rule->mode) {
        case JZ_RULE_MODE_WRN: sev = JZ_SEVERITY_WARNING; break;
        case JZ_RULE_MODE_INF: sev = JZ_SEVERITY_NOTE; break;
        case JZ_RULE_MODE_ERR: sev = JZ_SEVERITY_ERROR; break;
        default: sev = JZ_SEVERITY_WARNING; break;
        }
        if (rule->description) {
            msg = rule->description;
        }
    }

    jz_diagnostic_report(diagnostics, loc, sev,
                         "DIV_UNGUARDED_RUNTIME_ZERO", msg);
}

/* -----------------------------------------------------------------------
 * Expression walker: find DIV/MOD and check divisor.
 * ----------------------------------------------------------------------- */

static void check_expr_for_div(const IR_Expr *expr,
                               const IR_Design *design,
                               const IR_Module *mod,
                               const GuardStack *guards,
                               JZDiagnosticList *diagnostics)
{
    if (!expr) return;

    switch (expr->kind) {
    case EXPR_BINARY_DIV:
    case EXPR_BINARY_MOD: {
        const IR_Expr *divisor = expr->u.binary.right;
        if (divisor) {
            /* If divisor is a nonzero literal, it's safe. */
            if (expr_is_nonzero_literal(divisor)) {
                /* OK — compile-time nonzero. */
            }
            /* If divisor is zero literal, DIV_CONST_ZERO is handled by
             * semantic analysis; skip here. */
            else if (expr_is_zero_literal(divisor)) {
                /* Already reported by sem. */
            }
            /* If divisor is a signal ref, check guard stack. */
            else {
                int sig = expr_signal_id(divisor);
                if (sig < 0 || !guard_is_nonzero(guards, sig)) {
                    report_unguarded_div(diagnostics, design, mod,
                                         expr->source_line);
                }
            }
        }
        /* Recurse into both operands (left may contain nested divs). */
        check_expr_for_div(expr->u.binary.left, design, mod, guards, diagnostics);
        check_expr_for_div(expr->u.binary.right, design, mod, guards, diagnostics);
        return;
    }

    /* All other binary operators. */
    case EXPR_BINARY_ADD:
    case EXPR_BINARY_SUB:
    case EXPR_BINARY_MUL:
    case EXPR_BINARY_AND:
    case EXPR_BINARY_OR:
    case EXPR_BINARY_XOR:
    case EXPR_BINARY_SHL:
    case EXPR_BINARY_SHR:
    case EXPR_BINARY_ASHR:
    case EXPR_BINARY_EQ:
    case EXPR_BINARY_NEQ:
    case EXPR_BINARY_LT:
    case EXPR_BINARY_GT:
    case EXPR_BINARY_LTE:
    case EXPR_BINARY_GTE:
    case EXPR_LOGICAL_AND:
    case EXPR_LOGICAL_OR:
        check_expr_for_div(expr->u.binary.left, design, mod, guards, diagnostics);
        check_expr_for_div(expr->u.binary.right, design, mod, guards, diagnostics);
        return;

    /* Unary operators. */
    case EXPR_UNARY_NOT:
    case EXPR_UNARY_NEG:
    case EXPR_LOGICAL_NOT:
        check_expr_for_div(expr->u.unary.operand, design, mod, guards, diagnostics);
        return;

    /* Ternary. */
    case EXPR_TERNARY:
        check_expr_for_div(expr->u.ternary.condition, design, mod, guards, diagnostics);
        check_expr_for_div(expr->u.ternary.true_val, design, mod, guards, diagnostics);
        check_expr_for_div(expr->u.ternary.false_val, design, mod, guards, diagnostics);
        return;

    /* Concatenation. */
    case EXPR_CONCAT:
        for (int i = 0; i < expr->u.concat.num_operands; i++) {
            check_expr_for_div(expr->u.concat.operands[i], design, mod, guards, diagnostics);
        }
        return;

    /* Slice. */
    case EXPR_SLICE:
        check_expr_for_div(expr->u.slice.base_expr, design, mod, guards, diagnostics);
        return;

    /* Intrinsics. */
    case EXPR_INTRINSIC_UADD:
    case EXPR_INTRINSIC_SADD:
    case EXPR_INTRINSIC_UMUL:
    case EXPR_INTRINSIC_SMUL:
    case EXPR_INTRINSIC_GBIT:
    case EXPR_INTRINSIC_SBIT:
    case EXPR_INTRINSIC_GSLICE:
    case EXPR_INTRINSIC_SSLICE:
    case EXPR_INTRINSIC_OH2B:
    case EXPR_INTRINSIC_B2OH:
    case EXPR_INTRINSIC_PRIENC:
    case EXPR_INTRINSIC_LZC:
    case EXPR_INTRINSIC_USUB:
    case EXPR_INTRINSIC_SSUB:
    case EXPR_INTRINSIC_ABS:
    case EXPR_INTRINSIC_UMIN:
    case EXPR_INTRINSIC_UMAX:
    case EXPR_INTRINSIC_SMIN:
    case EXPR_INTRINSIC_SMAX:
    case EXPR_INTRINSIC_POPCOUNT:
    case EXPR_INTRINSIC_REVERSE:
    case EXPR_INTRINSIC_BSWAP:
    case EXPR_INTRINSIC_REDUCE_AND:
    case EXPR_INTRINSIC_REDUCE_OR:
    case EXPR_INTRINSIC_REDUCE_XOR:
        check_expr_for_div(expr->u.intrinsic.source, design, mod, guards, diagnostics);
        check_expr_for_div(expr->u.intrinsic.index, design, mod, guards, diagnostics);
        check_expr_for_div(expr->u.intrinsic.value, design, mod, guards, diagnostics);
        return;

    /* Memory read. */
    case EXPR_MEM_READ:
        check_expr_for_div(expr->u.mem_read.address, design, mod, guards, diagnostics);
        return;

    /* Leaves. */
    case EXPR_LITERAL:
    case EXPR_SIGNAL_REF:
        return;
    }
}

/* -----------------------------------------------------------------------
 * Statement walker: recurse through control flow, maintaining guards.
 * ----------------------------------------------------------------------- */

static void check_stmt_for_div(const IR_Stmt *stmt,
                               const IR_Design *design,
                               const IR_Module *mod,
                               GuardStack *guards,
                               JZDiagnosticList *diagnostics);

static void check_stmts_for_div(const IR_Stmt *stmts, int count,
                                const IR_Design *design,
                                const IR_Module *mod,
                                GuardStack *guards,
                                JZDiagnosticList *diagnostics)
{
    for (int i = 0; i < count; i++) {
        check_stmt_for_div(&stmts[i], design, mod, guards, diagnostics);
    }
}

static void check_stmt_for_div(const IR_Stmt *stmt,
                               const IR_Design *design,
                               const IR_Module *mod,
                               GuardStack *guards,
                               JZDiagnosticList *diagnostics)
{
    if (!stmt) return;

    switch (stmt->kind) {
    case STMT_ASSIGNMENT:
        check_expr_for_div(stmt->u.assign.rhs, design, mod, guards, diagnostics);
        break;

    case STMT_MEM_WRITE:
        check_expr_for_div(stmt->u.mem_write.address, design, mod, guards, diagnostics);
        check_expr_for_div(stmt->u.mem_write.data, design, mod, guards, diagnostics);
        break;

    case STMT_BLOCK:
        check_stmts_for_div(stmt->u.block.stmts, stmt->u.block.count,
                            design, mod, guards, diagnostics);
        break;

    case STMT_IF: {
        const IR_IfStmt *ifs = &stmt->u.if_stmt;

        /* Check the condition expression itself for divisions. */
        check_expr_for_div(ifs->condition, design, mod, guards, diagnostics);

        /* Try to extract a guard from the condition. */
        int guard_sig = -1;
        bool then_nonzero = false;
        bool else_nonzero = false;
        bool has_guard = extract_nonzero_guard(ifs->condition,
                                               &guard_sig,
                                               &then_nonzero,
                                               &else_nonzero);

        /* THEN branch. */
        if (has_guard && then_nonzero) {
            guard_push(guards, guard_sig, true);
        }
        if (ifs->then_block) {
            check_stmt_for_div(ifs->then_block, design, mod, guards, diagnostics);
        }
        if (has_guard && then_nonzero) {
            guard_pop(guards);
        }

        /* ELIF chain: each elif is another STMT_IF, recurse normally. */
        if (ifs->elif_chain) {
            check_stmt_for_div(ifs->elif_chain, design, mod, guards, diagnostics);
        }

        /* ELSE branch. */
        if (has_guard && else_nonzero) {
            guard_push(guards, guard_sig, true);
        }
        if (ifs->else_block) {
            check_stmt_for_div(ifs->else_block, design, mod, guards, diagnostics);
        }
        if (has_guard && else_nonzero) {
            guard_pop(guards);
        }
        break;
    }

    case STMT_SELECT: {
        const IR_SelectStmt *sel = &stmt->u.select_stmt;
        /* Check selector for divisions. */
        check_expr_for_div(sel->selector, design, mod, guards, diagnostics);
        /* Recurse into each case body. */
        for (int i = 0; i < sel->num_cases; i++) {
            check_expr_for_div(sel->cases[i].case_value, design, mod, guards, diagnostics);
            if (sel->cases[i].body) {
                check_stmt_for_div(sel->cases[i].body, design, mod, guards, diagnostics);
            }
        }
        break;
    }
    }
}

/* -----------------------------------------------------------------------
 * Public entry point.
 * ----------------------------------------------------------------------- */

int jz_ir_div_guard_check(IR_Design *design, JZDiagnosticList *diagnostics)
{
    if (!design || !diagnostics) return 0;

    for (int m = 0; m < design->num_modules; m++) {
        IR_Module *mod = &design->modules[m];
        if (mod->eliminated) continue;

        GuardStack guards;
        guards.count = 0;

        /* Walk async block. */
        if (mod->async_block) {
            check_stmt_for_div(mod->async_block, design, mod,
                               &guards, diagnostics);
        }

        /* Walk each clock domain's statements. */
        for (int d = 0; d < mod->num_clock_domains; d++) {
            guards.count = 0;
            IR_Stmt *stmts = mod->clock_domains[d].statements;
            if (stmts) {
                check_stmt_for_div(stmts, design, mod,
                                   &guards, diagnostics);
            }
        }
    }

    return 0;
}
