/**
 * @file compiler.h
 * @brief Global compiler context and lifecycle management.
 *
 * Provides the top-level JZCompiler structure that owns the AST, IR,
 * allocation arenas, and buffered diagnostics for a single compilation
 * unit. Initialize with jz_compiler_init() and release with
 * jz_compiler_dispose().
 */

#ifndef JZ_HDL_COMPILER_H
#define JZ_HDL_COMPILER_H

#include "ast.h"
#include "arena.h"
#include "diagnostic.h"
#include "ir.h"
#include "ir_builder.h"
#include "ir_serialize.h"

/**
 * @enum JZCompilerMode
 * @brief Output mode for the compiler invocation.
 */
typedef enum JZCompilerMode {
    JZ_COMPILER_MODE_AST = 0, /**< Emit the AST as JSON. */
    JZ_COMPILER_MODE_LINT,    /**< Lint-only mode (diagnostics, no codegen). */
    JZ_COMPILER_MODE_VERILOG  /**< Generate Verilog output. */
} JZCompilerMode;

/**
 * @struct JZError
 * @brief Simple error representation with source location.
 */
typedef struct JZError {
    JZLocation loc;      /**< Source location where the error occurred. */
    char      *message;  /**< Heap-allocated error message. */
} JZError;

/**
 * @struct JZCompiler
 * @brief Global compiler context owning all compilation state.
 *
 * Holds the compilation mode, root AST and IR pointers, allocation
 * arenas, and buffered diagnostics. All fields are valid after
 * jz_compiler_init() and remain so until jz_compiler_dispose().
 */
typedef struct JZCompiler {
    JZCompilerMode mode;           /**< Active compilation mode. */

    JZASTNode     *ast_root;       /**< Root AST node (NULL on parse failure). */
    IR_Design     *ir_root;        /**< Root IR design (NULL until IR build). */

    JZArena ast_arena;             /**< Arena for AST allocations. */
    JZArena ir_arena;              /**< Arena for IR allocations. */

    JZDiagnosticList diagnostics;  /**< Buffered diagnostics for this compilation. */
} JZCompiler;

/**
 * @brief Initialize a compiler context.
 * @param comp Pointer to the compiler context to initialize. Must not be NULL.
 * @param mode Compilation mode (AST, lint, or Verilog).
 */
void jz_compiler_init(JZCompiler *comp, JZCompilerMode mode);

/**
 * @brief Dispose of a compiler context, releasing all owned resources.
 * @param comp Pointer to the compiler context to dispose. Must not be NULL.
 *
 * Frees the AST, IR, arenas, and diagnostic list. The compiler context
 * must not be used after this call.
 */
void jz_compiler_dispose(JZCompiler *comp);

/**
 * @brief Global verbose flag for timing diagnostics.
 *
 * When non-zero, compiler phases print timing information to stderr.
 * Set by main() when --verbose is passed on the command line.
 */
extern int jz_verbose;

#endif /* JZ_HDL_COMPILER_H */
