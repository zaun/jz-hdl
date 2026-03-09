/**
 * @file parser_internal.h
 * @brief Internal interfaces and state for the JZ HDL recursive-descent parser.
 *
 * This header declares internal parser structures, helper functions, and
 * entry points used to construct the abstract syntax tree (AST) for the JZ
 * hardware description language. It is not part of the public API and is
 * intended for use only within the parser implementation.
 *
 * The parser operates on a pre-tokenized input stream and performs
 * syntax-directed construction of AST nodes, deferring most semantic
 * validation to later compilation phases. To enable high-quality diagnostics,
 * the parser intentionally accepts certain syntactically valid but
 * semantically illegal constructs so that rule-specific errors can be
 * reported during semantic analysis rather than as generic parse failures.
 *
 * This file also defines infrastructure for safely managing filename
 * lifetimes associated with imported source files. Imported filenames are
 * retained for the duration of parsing so that all token locations and AST
 * nodes continue to reference valid filename strings when emitting
 * diagnostics or serialized output.
 *
 * @note All declarations in this file are internal and subject to change.
 */

#ifndef JZ_HDL_PARSER_INTERNAL_H
#define JZ_HDL_PARSER_INTERNAL_H

#include "../include/parser.h"
#include "../include/util.h"
#include "../include/diagnostic.h"
#include "../include/rules.h"

/**
 * @struct Parser
 * @brief Internal parser state used during recursive-descent parsing.
 *
 * This structure tracks the token stream, current position, filename for
 * diagnostics, and the active diagnostic list. It is passed throughout
 * parsing routines and is not exposed publicly.
 */
typedef struct Parser {
    const char     *filename;     /**< Source filename for diagnostics */
    const JZToken  *tokens;       /**< Token array produced by the lexer */
    size_t          count;        /**< Total number of tokens */
    size_t          pos;          /**< Current parsing position */
    JZDiagnosticList *diagnostics; /**< Diagnostic sink for rule-based errors */
} Parser;

/**
 * @brief Global list of imported filename strings.
 *
 * Imported files are lexed with a filename string allocated in
 * import_modules_from_path(). The lexer copies that filename pointer into
 * each token's JZLocation, and the parser copies it into AST nodes.
 *
 * To avoid dangling pointers (and corrupted JSON or diagnostics output),
 * these filename strings are kept alive here and freed only when parsing
 * is fully complete.
 */
static char  **g_imported_filenames      = NULL;
static size_t  g_imported_filenames_len  = 0;
static size_t  g_imported_filenames_cap  = 0;

/**
 * @brief Conditionally consume the next token if it matches a given type.
 *
 * @param p     Active parser
 * @param type  Token type to match
 * @return 1 if the token matched and was consumed, 0 otherwise
 */
int match(Parser *p, JZTokenType type);

/**
 * @brief Peek at the current token without consuming it.
 *
 * @param p Active parser
 * @return Pointer to the current token (never NULL)
 */
const JZToken *peek(const Parser *p);

/**
 * @brief Advance the parser to the next token.
 *
 * If already at EOF, this function keeps returning the EOF token.
 *
 * @param p Active parser
 * @return Pointer to the new current token
 */
const JZToken *advance(Parser *p);

/**
 * @brief Emit a generic parse error at the current token location.
 *
 * This reports an unconditional parse error, not tied to a specific
 * semantic rule.
 *
 * @param p   Active parser
 * @param msg Human-readable error message
 */
void parser_error(const Parser *p, const char *msg);

/**
 * @brief Construct a RAW_TEXT AST node from a token range.
 *
 * Tokens in the half-open range [start, end) are concatenated (by lexeme)
 * into a single string stored in the node's text field.
 *
 * @param p     Active parser
 * @param start Starting token index (inclusive)
 * @param end   Ending token index (exclusive)
 * @return Newly allocated RAW_TEXT AST node, or NULL on failure
 */
JZASTNode *make_raw_text_node(const Parser *p, size_t start, size_t end);

/**
 * @brief Determine whether a token may act as an identifier in declarations.
 *
 * This allows reserved keywords (e.g., IF, PORT, MODULE) to parse as
 * identifiers in declaration contexts so that semantic analysis can emit
 * targeted diagnostics like KEYWORD_AS_IDENTIFIER instead of generic
 * parse failures.
 *
 * @param tok Token to test
 * @return 1 if the token is identifier-like, 0 otherwise
 */
int is_decl_identifier_token(const JZToken *tok);

/**
 * @brief Report a rule-based diagnostic tied to a specific token.
 *
 * If the rule exists in the rule database, its severity and description
 * are used; otherwise, the fallback message is emitted.
 *
 * @param p                Active parser
 * @param t                Token associated with the diagnostic
 * @param rule_id          Rule identifier string
 * @param fallback_message Message to use if the rule is not found
 */
void parser_report_rule(const Parser *p,
                        const JZToken *t,
                        const char *rule_id,
                        const char *fallback_message);

/**
 * @brief Parse a full expression with correct operator precedence.
 *
 * This is the entry point for all expression parsing.
 *
 * @param p Active parser
 * @return Expression AST node, or NULL on error
 */
JZASTNode *parse_expression(Parser *p);

/**
 * @brief Parse a structured block following a block keyword.
 *
 * This dispatches to block-specific parsing logic based on node type
 * and block kind.
 *
 * @param p         Active parser
 * @param block_kw  Token corresponding to the block keyword
 * @param kind      String identifier for the block kind
 * @param node_type AST node type to create
 * @return Block AST node, or NULL on error
 */
JZASTNode *parse_block(Parser *p,
                       const JZToken *block_kw,
                       const char *kind,
                       JZASTNodeType node_type);

/**
 * @brief Parse the body of a CONST or CONFIG block.
 *
 * @param p      Active parser
 * @param parent Parent AST node
 * @return 0 on success, -1 on error
 */
int parse_const_block_body(Parser *p, JZASTNode *parent);

/**
 * @brief Parse the body of a PORT block.
 *
 * @param p      Active parser
 * @param parent PORT block AST node
 * @return 0 on success, -1 on error
 */
int parse_port_block_body(Parser *p, JZASTNode *parent);

/**
 * @brief Parse the body of a WIRE block.
 *
 * @param p      Active parser
 * @param parent WIRE block AST node
 * @return 0 on success, -1 on error
 */
int parse_wire_block_body(Parser *p, JZASTNode *parent);

/**
 * @brief Parse the body of a REGISTER block.
 *
 * @param p      Active parser
 * @param parent REGISTER block AST node
 * @return 0 on success, -1 on error
 */
int parse_register_block_body(Parser *p, JZASTNode *parent);

/**
 * @brief Parse the body of a LATCH block.
 *
 * @param p      Active parser
 * @param parent LATCH block AST node
 * @return 0 on success, -1 on error
 */
int parse_latch_block_body(Parser *p, JZASTNode *parent);

/**
 * @brief Parse the body of a MEM block.
 *
 * @param p      Active parser
 * @param parent MEM block AST node
 * @return 0 on success, -1 on error
 */
int parse_mem_block_body(Parser *p, JZASTNode *parent);

/**
 * @brief Parse the body of a MUX block.
 *
 * @param p      Active parser
 * @param parent MUX block AST node
 * @return 0 on success, -1 on error
 */
int parse_mux_block_body(Parser *p, JZASTNode *parent);

/**
 * @brief Parse the body of a CDC block.
 *
 * @param p      Active parser
 * @param parent CDC block AST node
 * @return 0 on success, -1 on error
 */
int parse_cdc_block_body(Parser *p, JZASTNode *parent);

/**
 * @brief Parse the body of a CLOCKS block.
 *
 * @param p      Active parser
 * @param parent CLOCKS block AST node
 * @return 0 on success, -1 on error
 */
int parse_clocks_block_body(Parser *p, JZASTNode *parent);

/**
 * @brief Parse the body of a PIN block (IN_PINS, OUT_PINS, INOUT_PINS).
 *
 * @param p          Active parser
 * @param parent     PIN block AST node
 * @param block_kind Block kind string
 * @return 0 on success, -1 on error
 */
int parse_pins_block_body(Parser *p, JZASTNode *parent, const char *block_kind);

/**
 * @brief Parse the body of a MAP block.
 *
 * @param p      Active parser
 * @param parent MAP block AST node
 * @return 0 on success, -1 on error
 */
int parse_map_block_body(Parser *p, JZASTNode *parent);

/**
 * @brief Parse a list of executable statements until a terminator token.
 *
 * Used for ASYNCHRONOUS, SYNCHRONOUS, IF, SELECT, FEATURE, and similar blocks.
 *
 * @param p          Active parser
 * @param parent     Parent AST node
 * @param terminator Token that ends the statement list
 * @param is_sync    Nonzero if parsing a SYNCHRONOUS context
 * @return 0 on success, -1 on error
 */
int parse_statement_list(Parser *p,
                         JZASTNode *parent,
                         JZTokenType terminator,
                         int is_sync);

/**
 * @brief Parse a primary expression.
 *
 * Primary expressions include literals, identifiers, qualified identifiers,
 * parenthesized expressions, built-in calls, and concatenations.
 *
 * @param p Active parser
 * @return Expression AST node, or NULL on error
 */
JZASTNode *parse_primary_expr(Parser *p);

/**
 * @brief Parse a simple index expression for slices.
 *
 * This is a restricted expression form used in indexing contexts such as
 * [msb:lsb] or [idx].
 *
 * @param p Active parser
 * @return Expression AST node, or NULL on error
 */
JZASTNode *parse_simple_index_expr(Parser *p);

/**
 * @brief Parse a module definition.
 *
 * Expects @module to have already been consumed.
 *
 * @param p Active parser
 * @return Module AST node, or NULL on error
 */
JZASTNode *parse_module(Parser *p);

/**
 * @brief Parse a global block.
 *
 * Expects @global to have already been consumed.
 *
 * @param p Active parser
 * @return Global block AST node, or NULL on error
 */
JZASTNode *parse_global(Parser *p);

/**
 * @brief Parse a compile-time @check directive.
 *
 * @param p Active parser
 * @return Check AST node, or NULL on error
 */
JZASTNode *parse_check(Parser *p);

/**
 * @brief Parse a project definition.
 *
 * Expects @project to have already been consumed.
 *
 * @param p Active parser
 * @return Project AST node, or NULL on error
 */
JZASTNode *parse_project(Parser *p);

/**
 * @brief Import and parse modules from another source file.
 *
 * Imported modules and globals are merged into the target project AST.
 *
 * @param parent       Parser performing the import
 * @param proj         Target project AST node
 * @param rel_path     Path string from the @import directive
 * @param import_token Token corresponding to the @import keyword
 * @return 0 on success, -1 on error
 */
int import_modules_from_path(const Parser *parent,
                             JZASTNode *proj,
                             const char *rel_path,
                             const JZToken *import_token);

/**
 * @brief Parse a BUS definition block: BUS <name> { IN/OUT/INOUT [w] sig; ... }
 *
 * Expects the BUS keyword to have already been consumed.
 *
 * @param p      Active parser
 * @param bus_kw Token corresponding to the BUS keyword
 * @return BUS_BLOCK AST node, or NULL on error
 */
JZASTNode *parse_bus_definition(Parser *p, const JZToken *bus_kw);

/**
 * @brief Parse a module instantiation (@new) inside a module body.
 *
 * @param p Active parser
 * @return Module instance AST node, or NULL on error
 */
JZASTNode *parse_module_instantiation(Parser *p);

/**
 * @brief Parse the body of a @blackbox definition.
 *
 * @param p  Active parser
 * @param bb Blackbox AST node
 * @return 0 on success, -1 on error
 */
int parse_blackbox_body(Parser *p, JZASTNode *bb);

/**
 * @brief Parse a CLOCK_GEN block inside a project.
 *
 * CLOCK_GEN blocks define PLLs/DLLs that generate clocks from other clocks.
 * The chip information comes from the PROJECT-level CHIP declaration.
 * The syntax is:
 *   CLOCK_GEN {
 *     PLL {
 *       IN <clock>;
 *       OUT <pll_output> <clock>;
 *       CONFIG { <param>=<value>; ... };
 *     };
 *   }
 *
 * @param p        Active parser
 * @param block_kw CLOCK_GEN keyword token
 * @return CLOCK_GEN block AST node, or NULL on error
 */
JZASTNode *parse_clock_gen_block(Parser *p, const JZToken *block_kw);

/**
 * @brief Parse a @template definition.
 *
 * Expects the @template keyword to have already been consumed.
 *
 * @param p Active parser
 * @return TEMPLATE_DEF AST node, or NULL on error
 */
JZASTNode *parse_template_def(Parser *p);

/**
 * @brief Parse an @apply statement inside a statement list.
 *
 * Expects the @apply keyword to have already been consumed.
 *
 * @param p Active parser
 * @return TEMPLATE_APPLY AST node, or NULL on error
 */
JZASTNode *parse_apply_stmt(Parser *p);

/**
 * @brief Parse a @scratch declaration inside a template body.
 *
 * Expects the @scratch keyword to have already been consumed.
 *
 * @param p Active parser
 * @return SCRATCH_DECL AST node, or NULL on error
 */
JZASTNode *parse_scratch_decl(Parser *p);

/**
 * @brief Parse a @testbench block.
 *
 * Expects the @testbench keyword to have already been consumed.
 *
 * @param p Active parser
 * @return TESTBENCH AST node, or NULL on error
 */
JZASTNode *parse_testbench(Parser *p);

/**
 * @brief Parse a @simulation block.
 *
 * Expects the @simulation keyword to have already been consumed.
 *
 * @param p Active parser
 * @return SIMULATION AST node, or NULL on error
 */
JZASTNode *parse_simulation(Parser *p);

/**
 * @brief Parse a @feature guard inside a declaration block or module scope.
 *
 * Creates a FEATURE_GUARD AST node with block_kind="FEATURE_DECL" containing
 * the condition expression, a THEN body, and an optional ELSE body. The body_fn
 * callback is used to parse declarations in each branch; it should return 0 when
 * it encounters @else or @endfeat sentinel tokens.
 *
 * @param p       Active parser
 * @param parent  Parent AST node to attach the FEATURE_GUARD to
 * @param body_fn Callback that parses the body declarations
 * @return 0 on success, -1 on error
 */
int parse_feature_guard_in_block(Parser *p, JZASTNode *parent,
                                  int (*body_fn)(Parser *p, JZASTNode *parent));

#endif
