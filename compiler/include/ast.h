/**
 * @file ast.h
 * @brief Abstract Syntax Tree (AST) node types and manipulation functions.
 *
 * Defines the AST node structure used throughout the compiler pipeline.
 * Each node has a type, source location, optional metadata (name, block
 * kind, text, width), and a dynamically-sized child array. Nodes are
 * heap-allocated and must be freed with jz_ast_free().
 */

#ifndef JZ_HDL_AST_H
#define JZ_HDL_AST_H

#include <stdio.h>
#include <stddef.h>

/**
 * @struct JZLocation
 * @brief Source location tracking for AST nodes and diagnostics.
 */
typedef struct JZLocation {
    const char *filename; /**< Source filename (not owned; must outlive the location). */
    int line;             /**< 1-based line number. */
    int column;           /**< 1-based column number. */
} JZLocation;

/**
 * @enum JZASTNodeType
 * @brief Discriminator for all AST node kinds.
 */
typedef enum JZASTNodeType {
    /* Top-level constructs */
    JZ_AST_MODULE,                  /**< Module definition (\@module ... \@endmod). */
    JZ_AST_PROJECT,                 /**< Project definition (\@project ... \@endproj). */
    JZ_AST_BLOCK,                   /**< Generic declaration block. */
    JZ_AST_BLACKBOX,                /**< Blackbox module declaration. */
    JZ_AST_INSTANTIATION,           /**< Module instantiation (\@new). */
    JZ_AST_RAW_TEXT,                /**< Unparsed text (imports, legacy). */

    /* Feature guards */
    JZ_AST_FEATURE_GUARD,           /**< Feature guard (\@feature ... \@endfeat). */

    /* Compile-time assertions */
    JZ_AST_CHECK,                   /**< Compile-time assertion (\@check). */

    /* Templates */
    JZ_AST_TEMPLATE_DEF,            /**< Template definition (\@template ... \@endtemplate). */
    JZ_AST_TEMPLATE_PARAM,          /**< Template parameter declaration. */
    JZ_AST_TEMPLATE_APPLY,          /**< Template application (\@apply). */
    JZ_AST_SCRATCH_DECL,            /**< Scratch wire declaration (\@scratch). */

    /* Declaration blocks */
    JZ_AST_CONST_BLOCK,             /**< CONST declaration block. */
    JZ_AST_PORT_BLOCK,              /**< PORT declaration block. */
    JZ_AST_WIRE_BLOCK,              /**< WIRE declaration block. */
    JZ_AST_REGISTER_BLOCK,          /**< REGISTER declaration block. */
    JZ_AST_LATCH_BLOCK,             /**< LATCH declaration block. */
    JZ_AST_MEM_BLOCK,               /**< MEM declaration block. */
    JZ_AST_MUX_BLOCK,               /**< MUX declaration block. */
    JZ_AST_BUS_BLOCK,               /**< BUS declaration block. */
    JZ_AST_CONFIG_BLOCK,            /**< CONFIG declaration block. */
    JZ_AST_GLOBAL_BLOCK,            /**< GLOBAL declaration block. */
    JZ_AST_CLOCKS_BLOCK,            /**< CLOCKS declaration block. */
    JZ_AST_IN_PINS_BLOCK,           /**< IN_PINS declaration block. */
    JZ_AST_OUT_PINS_BLOCK,          /**< OUT_PINS declaration block. */
    JZ_AST_INOUT_PINS_BLOCK,        /**< INOUT_PINS declaration block. */
    JZ_AST_MAP_BLOCK,               /**< MAP declaration block. */
    JZ_AST_CLOCK_GEN_BLOCK,         /**< CLOCK_GEN declaration block. */
    JZ_AST_CLOCK_GEN_UNIT,          /**< Clock generator unit (PLL/DLL). */
    JZ_AST_CLOCK_GEN_IN,            /**< Clock generator input. */
    JZ_AST_CLOCK_GEN_OUT,           /**< Clock generator output. */
    JZ_AST_CLOCK_GEN_CONFIG,        /**< Clock generator configuration. */
    JZ_AST_MODULE_INSTANCE,         /**< Module instance in parent module. */
    JZ_AST_PROJECT_TOP_INSTANCE,    /**< Top-level instance in project. */

    /* Individual declarations */
    JZ_AST_CONST_DECL,              /**< Single constant declaration. */
    JZ_AST_PORT_DECL,               /**< Single port declaration. */
    JZ_AST_WIRE_DECL,               /**< Single wire declaration. */
    JZ_AST_REGISTER_DECL,           /**< Single register declaration. */
    JZ_AST_LATCH_DECL,              /**< Single latch declaration. */
    JZ_AST_MEM_DECL,                /**< Single memory declaration. */
    JZ_AST_MEM_PORT,                /**< Memory port declaration. */
    JZ_AST_MUX_DECL,                /**< Single mux declaration. */
    JZ_AST_BUS_DECL,                /**< Single bus declaration. */
    JZ_AST_CDC_DECL,                /**< CDC crossing declaration. */

    /* Types and widths */
    JZ_AST_WIDTH_EXPR,              /**< Width expression for declarations. */
    JZ_AST_MEM_DEPTH_EXPR,          /**< Memory depth expression. */

    /* Misc */
    JZ_AST_SYNC_PARAM,              /**< SYNCHRONOUS block parameter. */

    /* Statements */
    JZ_AST_STMT_ASSIGN,             /**< Assignment statement. */
    JZ_AST_STMT_IF,                 /**< IF statement. */
    JZ_AST_STMT_ELIF,               /**< ELIF clause. */
    JZ_AST_STMT_ELSE,               /**< ELSE clause. */
    JZ_AST_STMT_SELECT,             /**< SELECT statement. */
    JZ_AST_STMT_CASE,               /**< CASE clause. */
    JZ_AST_STMT_DEFAULT,            /**< DEFAULT clause. */

    /* Expressions */
    JZ_AST_EXPR_LITERAL,            /**< Numeric or string literal. */
    JZ_AST_EXPR_IDENTIFIER,         /**< Simple identifier reference. */
    JZ_AST_EXPR_QUALIFIED_IDENTIFIER, /**< Qualified identifier (a.b). */
    JZ_AST_EXPR_BUS_ACCESS,         /**< Bus access expression. */
    JZ_AST_EXPR_UNARY,              /**< Unary operator expression. */
    JZ_AST_EXPR_BINARY,             /**< Binary operator expression. */
    JZ_AST_EXPR_TERNARY,            /**< Ternary (? :) expression. */
    JZ_AST_EXPR_CONCAT,             /**< Concatenation expression. */
    JZ_AST_EXPR_SLICE,              /**< Bit slice expression [msb:lsb]. */
    JZ_AST_EXPR_BUILTIN_CALL,       /**< Builtin function call. */
    JZ_AST_EXPR_SPECIAL_DRIVER,     /**< GND or VCC polymorphic driver. */

    /* Testbench constructs */
    JZ_AST_TESTBENCH,               /**< Testbench definition (\@testbench ... \@endtb). */
    JZ_AST_TB_TEST,                 /**< Test case (TEST "desc" { ... }). */
    JZ_AST_TB_CLOCK_BLOCK,          /**< Testbench CLOCK block. */
    JZ_AST_TB_CLOCK_DECL,           /**< Individual testbench clock declaration. */
    JZ_AST_TB_WIRE_BLOCK,           /**< Testbench WIRE block. */
    JZ_AST_TB_WIRE_DECL,            /**< Individual testbench wire declaration. */
    JZ_AST_TB_SETUP,                /**< \@setup block. */
    JZ_AST_TB_UPDATE,               /**< \@update block. */
    JZ_AST_TB_CLOCK_ADV,            /**< \@clock(clk, cycle=N) directive. */
    JZ_AST_TB_EXPECT_EQ,            /**< \@expect_equal(signal, value). */
    JZ_AST_TB_EXPECT_NEQ,           /**< \@expect_not_equal(signal, value). */
    JZ_AST_TB_EXPECT_TRI,           /**< \@expect_tristate(signal). */

    /* Simulation constructs */
    JZ_AST_SIMULATION,              /**< Simulation definition (\@simulation ... \@endsim). */
    JZ_AST_SIM_CLOCK_BLOCK,         /**< Simulation CLOCK block (auto-toggling clocks). */
    JZ_AST_SIM_CLOCK_DECL,          /**< Individual simulation clock with period (text=period). */
    JZ_AST_SIM_TAP_BLOCK,           /**< TAP block for internal signal monitoring. */
    JZ_AST_SIM_TAP_DECL,            /**< Individual TAP signal (name=hierarchical path). */
    JZ_AST_SIM_RUN,                 /**< \@run directive (text=unit, name=value). */
    JZ_AST_SIM_RUN_UNTIL,           /**< \@run_until directive. Children: [0]=signal, [1]=value, text=timeout_unit, name=timeout_value, block_kind=op. */
    JZ_AST_SIM_RUN_WHILE,           /**< \@run_while directive. Children: [0]=signal, [1]=value, text=timeout_unit, name=timeout_value, block_kind=op. */
    JZ_AST_PRINT,                   /**< \@print directive. text=format_string, children=args. */
    JZ_AST_PRINT_IF,                /**< \@print_if directive. text=format_string, children[0]=condition, children[1..]=args. */
    JZ_AST_SIM_TRACE,               /**< \@trace(state=on/off) directive. name="on" or "off". */
    JZ_AST_SIM_MARK,                /**< \@mark(color, "message") directive. text=color, name=message. */
    JZ_AST_SIM_ALERT,               /**< \@alert(condition, color, "message") directive. children[0]=condition, text=color, name=message. */
} JZASTNodeType;

typedef struct JZASTNode JZASTNode;

/**
 * @struct JZASTNode
 * @brief A single node in the abstract syntax tree.
 *
 * Nodes form a tree via the children array. Optional string fields
 * (name, block_kind, text, width) are heap-allocated and owned by the
 * node; they are freed when jz_ast_free() is called.
 */
struct JZASTNode {
    JZASTNodeType type;        /**< Node type discriminator. */
    JZLocation loc;            /**< Source location of this node. */
    char *name;                /**< Optional name (module, project, declaration). */
    char *block_kind;          /**< Block kind string (e.g., "CONST", "PORT"). */
    char *text;                /**< Optional text payload (imports, diagnostics). */
    char *width;               /**< Optional width expression string. */

    JZASTNode **children;      /**< Array of child node pointers. */
    size_t child_count;        /**< Number of children. */
    size_t child_capacity;     /**< Allocated capacity of children array. */
};

/**
 * @brief Create a new AST node.
 * @param type Node type.
 * @param loc  Source location for the node.
 * @return Newly allocated node, or NULL on allocation failure.
 */
JZASTNode *jz_ast_new(JZASTNodeType type, JZLocation loc);

/**
 * @brief Set the name field of an AST node.
 * @param node Target node. Must not be NULL.
 * @param name Name string to copy. May be NULL to clear.
 */
void jz_ast_set_name(JZASTNode *node, const char *name);

/**
 * @brief Set the block_kind field of an AST node.
 * @param node Target node. Must not be NULL.
 * @param kind Block kind string to copy (e.g., "CONST"). May be NULL.
 */
void jz_ast_set_block_kind(JZASTNode *node, const char *kind);

/**
 * @brief Set the text field of an AST node.
 * @param node Target node. Must not be NULL.
 * @param text Text string to copy. May be NULL to clear.
 */
void jz_ast_set_text(JZASTNode *node, const char *text);

/**
 * @brief Set the width field of an AST node.
 * @param node  Target node. Must not be NULL.
 * @param width Width expression string to copy. May be NULL to clear.
 */
void jz_ast_set_width(JZASTNode *node, const char *width);

/**
 * @brief Append a child node to a parent.
 * @param parent Parent node. Must not be NULL.
 * @param child  Child node to append. Must not be NULL.
 * @return 0 on success, non-zero on allocation failure.
 */
int  jz_ast_add_child(JZASTNode *parent, JZASTNode *child);

/**
 * @brief Recursively free an AST node and all its children.
 * @param node Node to free. May be NULL (no-op).
 */
void jz_ast_free(JZASTNode *node);

#endif /* JZ_HDL_AST_H */
