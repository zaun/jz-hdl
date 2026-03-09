/**
 * @file sem.h
 * @brief Semantic analysis types and expression type inference.
 *
 * Provides the bit-vector type system used by semantic analysis, width
 * inference rules for unary/binary/ternary operators, concatenation,
 * and slices, as well as literal analysis and constant expression
 * evaluation.
 */

#ifndef JZ_HDL_SEM_H
#define JZ_HDL_SEM_H

#include <stddef.h>

#include "diagnostic.h"
#include "lexer.h"

/**
 * @struct JZBitvecType
 * @brief Bit-vector type representation for semantic width inference.
 */
typedef struct JZBitvecType {
    unsigned width;     /**< Bit-width; must be > 0 for valid types. */
    int      is_signed; /**< 0 = unsigned, 1 = signed. */
} JZBitvecType;

/**
 * @enum JZUnaryOp
 * @brief Unary operator categories for width inference.
 */
typedef enum JZUnaryOp {
    JZ_UNARY_PLUS,        /**< Unary plus (+). */
    JZ_UNARY_MINUS,       /**< Unary minus (-). */
    JZ_UNARY_BIT_NOT,     /**< Bitwise NOT (~). */
    JZ_UNARY_LOGICAL_NOT  /**< Logical NOT (!). */
} JZUnaryOp;

/**
 * @enum JZBinaryOp
 * @brief Binary operator categories for width inference.
 */
typedef enum JZBinaryOp {
    /* Arithmetic */
    JZ_BIN_ADD,     /**< Addition (+). */
    JZ_BIN_SUB,     /**< Subtraction (-). */
    JZ_BIN_MUL,     /**< Multiplication (*). */
    JZ_BIN_DIV,     /**< Division (/). */
    JZ_BIN_MOD,     /**< Modulo (%). */

    /* Bitwise */
    JZ_BIN_BIT_AND, /**< Bitwise AND (&). */
    JZ_BIN_BIT_OR,  /**< Bitwise OR (|). */
    JZ_BIN_BIT_XOR, /**< Bitwise XOR (^). */

    /* Logical */
    JZ_BIN_LOG_AND, /**< Logical AND (&&). */
    JZ_BIN_LOG_OR,  /**< Logical OR (||). */

    /* Comparison */
    JZ_BIN_EQ,      /**< Equality (==). */
    JZ_BIN_NE,      /**< Inequality (!=). */
    JZ_BIN_LT,      /**< Less than (<). */
    JZ_BIN_LE,      /**< Less than or equal (<=). */
    JZ_BIN_GT,      /**< Greater than (>). */
    JZ_BIN_GE,      /**< Greater than or equal (>=). */

    /* Shifts */
    JZ_BIN_SHL,     /**< Shift left (<<). */
    JZ_BIN_SHR,     /**< Logical shift right (>>). */
    JZ_BIN_ASHR     /**< Arithmetic shift right (>>>). */
} JZBinaryOp;

/**
 * @brief Construct a scalar bit-vector type.
 * @param width     Bit-width (must be > 0).
 * @param is_signed Non-zero for signed type.
 * @param out       Pointer to receive the constructed type. Must not be NULL.
 */
void jz_type_scalar(unsigned width, int is_signed, JZBitvecType *out);

/**
 * @brief Compute the result type of a unary operation.
 *
 * Applies the width inference rules from spec Section 3.1/3.2.
 *
 * @param op      Unary operator.
 * @param operand Operand type.
 * @param out     Pointer to receive the result type. Must not be NULL.
 * @return 0 on success, non-zero on error.
 */
int jz_type_unary(JZUnaryOp op, const JZBitvecType *operand, JZBitvecType *out);

/**
 * @brief Compute the result type of a binary operation.
 *
 * Handles arithmetic, bitwise, logical, comparison, and shift operators.
 *
 * @param op  Binary operator.
 * @param lhs Left-hand operand type.
 * @param rhs Right-hand operand type.
 * @param out Pointer to receive the result type. Must not be NULL.
 * @return 0 on success, non-zero on error.
 */
int jz_type_binary(JZBinaryOp op,
                   const JZBitvecType *lhs,
                   const JZBitvecType *rhs,
                   JZBitvecType *out);

/**
 * @brief Compute the result type of a ternary (? :) expression.
 * @param cond     Condition type (must be width-1).
 * @param if_true  True-branch type.
 * @param if_false False-branch type.
 * @param out      Pointer to receive the result type. Must not be NULL.
 * @return 0 on success, non-zero on error.
 */
int jz_type_ternary(const JZBitvecType *cond,
                    const JZBitvecType *if_true,
                    const JZBitvecType *if_false,
                    JZBitvecType *out);

/**
 * @brief Compute the result type of a concatenation.
 *
 * Result width is the sum of all element widths.
 *
 * @param elems Array of element types.
 * @param count Number of elements.
 * @param out   Pointer to receive the result type. Must not be NULL.
 * @return 0 on success, non-zero on error.
 */
int jz_type_concat(const JZBitvecType *elems,
                   size_t count,
                   JZBitvecType *out);

/**
 * @brief Compute the result type of a bit slice [msb:lsb].
 * @param base Base signal type.
 * @param msb  Most significant bit index (inclusive).
 * @param lsb  Least significant bit index (inclusive).
 * @param out  Pointer to receive the result type. Must not be NULL.
 * @return 0 on success, non-zero on error.
 */
int jz_type_slice(const JZBitvecType *base,
                  unsigned msb,
                  unsigned lsb,
                  JZBitvecType *out);

/**
 * @enum JZLiteralExtKind
 * @brief Extension behavior when a sized literal's intrinsic width is
 *        less than its declared width.
 */
typedef enum JZLiteralExtKind {
    JZ_LITERAL_EXT_NONE = 0, /**< No extension needed. */
    JZ_LITERAL_EXT_ZERO,     /**< Zero-extend. */
    JZ_LITERAL_EXT_X,        /**< X-extend (don't-care). */
    JZ_LITERAL_EXT_Z         /**< Z-extend (high-impedance). */
} JZLiteralExtKind;

/**
 * @brief Analyze a sized literal's value and declared width.
 *
 * Computes the intrinsic bit-width of the literal value and determines
 * how the value would be extended to fill the declared width.
 *
 * @param base               Numeric base (binary, decimal, hex).
 * @param value_lexeme       Value substring (no width prefix or base character),
 *                           as it appears in source (may contain underscores).
 * @param declared_width     Width from the <width> prefix.
 * @param out_intrinsic_width Receives the number of meaningful bits in the literal.
 * @param out_ext_kind       Receives the extension kind.
 * @return 0 on success, non-zero on overflow (intrinsic > declared).
 */
int jz_literal_analyze(JZNumericBase base,
                       const char   *value_lexeme,
                       unsigned      declared_width,
                       unsigned     *out_intrinsic_width,
                       JZLiteralExtKind *out_ext_kind);

/**
 * @struct JZConstDef
 * @brief A named or anonymous constant definition for evaluation.
 */
typedef struct JZConstDef {
    const char *name; /**< Constant name (NULL or empty for anonymous expressions). */
    const char *expr; /**< Null-terminated expression string. */
} JZConstDef;

/**
 * @struct JZConstEvalOptions
 * @brief Options for constant expression evaluation.
 */
typedef struct JZConstEvalOptions {
    JZDiagnosticList *diagnostics; /**< Optional diagnostic list (may be NULL). */
    const char       *filename;    /**< Source filename for diagnostics (may be NULL). */
} JZConstEvalOptions;

/**
 * @brief Evaluate a single anonymous integer constant expression.
 *
 * Supports decimal integers, arithmetic operators (+, -, *, /, %),
 * comparison operators, parentheses, and the builtin clog2().
 *
 * @param expr      Null-terminated expression string.
 * @param options   Evaluation options (diagnostics, filename). May be NULL.
 * @param out_value Receives the non-negative result on success.
 * @return 0 on success, non-zero on evaluation failure.
 */
int jz_const_eval_expr(const char *expr,
                       const JZConstEvalOptions *options,
                       long long *out_value);

/**
 * @brief Evaluate a set of named CONST/CONFIG definitions.
 *
 * Definitions may reference each other by name. Circular dependencies
 * are detected and reported via diagnostics when available.
 *
 * @param defs       Array of constant definitions.
 * @param count      Number of definitions.
 * @param options    Evaluation options (diagnostics, filename). May be NULL.
 * @param out_values Array to receive evaluated values (one per definition).
 * @return 0 on success (all values evaluated), non-zero on error.
 */
int jz_const_eval_all(const JZConstDef *defs,
                      size_t count,
                      const JZConstEvalOptions *options,
                      long long *out_values);

#endif /* JZ_HDL_SEM_H */
