/**
 * @file lexer.h
 * @brief Lexical analysis for JZ-HDL source files.
 *
 * Tokenizes JZ-HDL source text into a stream of tokens with full spec
 * compliance, including sized literals, operators, keywords, and
 * punctuation. The resulting JZTokenStream is consumed by the parser.
 */

#ifndef JZ_HDL_LEXER_H
#define JZ_HDL_LEXER_H

#include <stddef.h>
#include "ast.h"
#include "diagnostic.h"

/**
 * @enum JZNumericBase
 * @brief Numeric base for sized literals.
 */
typedef enum JZNumericBase {
    JZ_NUM_BASE_NONE = 0, /**< No base prefix (plain integer). */
    JZ_NUM_BASE_BIN,      /**< Binary (e.g., 8'b10101010). */
    JZ_NUM_BASE_DEC,      /**< Decimal (e.g., 8'd255). */
    JZ_NUM_BASE_HEX       /**< Hexadecimal (e.g., 8'hFF). */
} JZNumericBase;

/**
 * @struct JZNumericInfo
 * @brief Parsed metadata for numeric literal tokens.
 */
typedef struct JZNumericInfo {
    int           is_sized;    /**< Non-zero if token is <width>'<base><value>. */
    JZNumericBase base;        /**< Base for sized literals; NONE for plain integers. */
    size_t        width_chars; /**< Number of characters in <width> prefix before '\''. */
} JZNumericInfo;

/**
 * @enum JZTokenType
 * @brief Token types produced by the lexer.
 */
typedef enum JZTokenType {
    JZ_TOK_EOF = 0,       /**< End of file. */
    JZ_TOK_IDENTIFIER,    /**< Identifier (variable, module, etc.). */
    JZ_TOK_NUMBER,        /**< Plain integer (no base prefix). */
    JZ_TOK_SIZED_NUMBER,  /**< Sized literal: <width>'<base><value>. */
    JZ_TOK_STRING,        /**< String literal. */

    /* Special identifier-like tokens */
    JZ_TOK_NO_CONNECT,    /**< Single '_' placeholder; not a normal identifier. */

    /* Punctuation */
    JZ_TOK_LBRACE,        /**< { */
    JZ_TOK_RBRACE,        /**< } */
    JZ_TOK_LPAREN,        /**< ( */
    JZ_TOK_RPAREN,        /**< ) */
    JZ_TOK_LBRACKET,      /**< [ */
    JZ_TOK_RBRACKET,      /**< ] */
    JZ_TOK_SEMICOLON,     /**< ; */
    JZ_TOK_COMMA,         /**< , */
    JZ_TOK_DOT,           /**< . */
    JZ_TOK_AT,            /**< @ */

    /* Assignment and expression operators */
    JZ_TOK_OP_ASSIGN,     /**< = */
    JZ_TOK_OP_ASSIGN_Z,   /**< =z (zero-extend assign) */
    JZ_TOK_OP_ASSIGN_S,   /**< =s (sign-extend assign) */
    JZ_TOK_OP_DRIVE,      /**< => (drive) */
    JZ_TOK_OP_DRIVE_Z,    /**< =>z (zero-extend drive) */
    JZ_TOK_OP_DRIVE_S,    /**< =>s (sign-extend drive) */
    JZ_TOK_OP_LE_Z,       /**< <=z (zero-extend receive) */
    JZ_TOK_OP_LE_S,       /**< <=s (sign-extend receive) */

    JZ_TOK_OP_PLUS,       /**< + */
    JZ_TOK_OP_MINUS,      /**< - */
    JZ_TOK_OP_STAR,       /**< * */
    JZ_TOK_OP_SLASH,      /**< / */
    JZ_TOK_OP_PERCENT,    /**< % */
    JZ_TOK_OP_AMP,        /**< & (bitwise AND) */
    JZ_TOK_OP_PIPE,       /**< | (bitwise OR) */
    JZ_TOK_OP_CARET,      /**< ^ (bitwise XOR) */
    JZ_TOK_OP_TILDE,      /**< ~ (bitwise NOT) */
    JZ_TOK_OP_BANG,        /**< ! (logical NOT) */
    JZ_TOK_OP_AND_AND,    /**< && (logical AND) */
    JZ_TOK_OP_OR_OR,      /**< || (logical OR) */
    JZ_TOK_OP_EQ,         /**< == */
    JZ_TOK_OP_NEQ,        /**< != */
    JZ_TOK_OP_LT,         /**< < */
    JZ_TOK_OP_LE,         /**< <= (less-than-or-equal / receive) */
    JZ_TOK_OP_GT,         /**< > */
    JZ_TOK_OP_GE,         /**< >= */
    JZ_TOK_OP_SHL,        /**< << (shift left) */
    JZ_TOK_OP_SHR,        /**< >> (logical shift right) */
    JZ_TOK_OP_ASHR,       /**< >>> (arithmetic shift right) */
    JZ_TOK_OP_QUESTION,   /**< ? (ternary condition) */
    JZ_TOK_OP_COLON,      /**< : (ternary separator / slice) */

    /* Keywords / directives */
    JZ_TOK_KW_MODULE,     /**< \@module */
    JZ_TOK_KW_ENDMOD,     /**< \@endmod */
    JZ_TOK_KW_PROJECT,    /**< \@project */
    JZ_TOK_KW_ENDPROJ,    /**< \@endproj */
    JZ_TOK_KW_BLACKBOX,   /**< \@blackbox */
    JZ_TOK_KW_NEW,        /**< \@new */
    JZ_TOK_KW_TOP,        /**< \@top */
    JZ_TOK_KW_IMPORT,     /**< \@import */
    JZ_TOK_KW_GLOBAL,     /**< \@global */
    JZ_TOK_KW_ENDGLOB,    /**< \@endglob */
    JZ_TOK_KW_FEATURE,    /**< \@feature */
    JZ_TOK_KW_ENDFEAT,    /**< \@endfeat */
    JZ_TOK_KW_FEATURE_ELSE, /**< \@else */
    JZ_TOK_KW_CHECK,      /**< \@check */

    JZ_TOK_KW_CONST,      /**< CONST block keyword. */
    JZ_TOK_KW_PORT,       /**< PORT block keyword. */
    JZ_TOK_KW_WIRE,       /**< WIRE block keyword. */
    JZ_TOK_KW_REGISTER,   /**< REGISTER block keyword. */
    JZ_TOK_KW_LATCH,      /**< LATCH block keyword. */
    JZ_TOK_KW_MEM,        /**< MEM block keyword. */
    JZ_TOK_KW_ASYNC,      /**< ASYNCHRONOUS block keyword. */
    JZ_TOK_KW_SYNC,       /**< SYNCHRONOUS block keyword. */
    JZ_TOK_KW_MUX,        /**< MUX block keyword. */
    JZ_TOK_KW_CDC,        /**< CDC block keyword. */
    JZ_TOK_KW_CONFIG,     /**< CONFIG block keyword. */
    JZ_TOK_KW_CLOCKS,     /**< CLOCKS block keyword. */
    JZ_TOK_KW_IN_PINS,    /**< IN_PINS block keyword. */
    JZ_TOK_KW_OUT_PINS,   /**< OUT_PINS block keyword. */
    JZ_TOK_KW_INOUT_PINS, /**< INOUT_PINS block keyword. */
    JZ_TOK_KW_MAP,        /**< MAP block keyword. */
    JZ_TOK_KW_CLOCK_GEN,  /**< CLOCK_GEN block keyword. */

    /* Statement-level and direction/type keywords */
    JZ_TOK_KW_IF,         /**< IF keyword. */
    JZ_TOK_KW_ELIF,       /**< ELIF keyword. */
    JZ_TOK_KW_ELSE,       /**< ELSE keyword. */
    JZ_TOK_KW_SELECT,     /**< SELECT keyword. */
    JZ_TOK_KW_CASE,       /**< CASE keyword. */
    JZ_TOK_KW_DEFAULT,    /**< DEFAULT keyword. */
    JZ_TOK_KW_IN,         /**< IN direction keyword. */
    JZ_TOK_KW_OUT,        /**< OUT direction keyword. */
    JZ_TOK_KW_INOUT,      /**< INOUT direction keyword. */
    JZ_TOK_KW_OVERRIDE,   /**< OVERRIDE keyword. */
    JZ_TOK_KW_GND,        /**< GND special driver. */
    JZ_TOK_KW_VCC,        /**< VCC special driver. */
    JZ_TOK_KW_DISCONNECT, /**< DISCONNECT keyword. */
    JZ_TOK_KW_TEMPLATE,   /**< \@template */
    JZ_TOK_KW_ENDTEMPLATE, /**< \@endtemplate */
    JZ_TOK_KW_APPLY,      /**< \@apply */
    JZ_TOK_KW_SCRATCH,    /**< \@scratch */

    /* Testbench directives */
    JZ_TOK_KW_TESTBENCH,  /**< \@testbench */
    JZ_TOK_KW_ENDTB,      /**< \@endtb */
    JZ_TOK_KW_TEST,       /**< TEST keyword. */
    JZ_TOK_KW_SETUP,      /**< \@setup */
    JZ_TOK_KW_UPDATE,     /**< \@update */
    JZ_TOK_KW_TB_CLOCK,   /**< \@clock (testbench clock directive) */
    JZ_TOK_KW_EXPECT_EQ,  /**< \@expect_equal */
    JZ_TOK_KW_EXPECT_NEQ, /**< \@expect_not_equal */
    JZ_TOK_KW_EXPECT_TRI, /**< \@expect_tristate */

    /* Simulation directives */
    JZ_TOK_KW_SIMULATION, /**< \@simulation */
    JZ_TOK_KW_ENDSIM,     /**< \@endsim */
    JZ_TOK_KW_RUN,        /**< \@run */
    JZ_TOK_KW_RUN_UNTIL, /**< \@run_until */
    JZ_TOK_KW_RUN_WHILE, /**< \@run_while */
    JZ_TOK_KW_PRINT,     /**< \@print */
    JZ_TOK_KW_PRINT_IF,  /**< \@print_if */
    JZ_TOK_KW_TRACE,     /**< \@trace */
    JZ_TOK_KW_MARK,      /**< \@mark */
    JZ_TOK_KW_MARK_IF,   /**< \@mark_if */
    JZ_TOK_KW_ALERT,     /**< \@alert */
    JZ_TOK_KW_ALERT_IF,  /**< \@alert_if */
    JZ_TOK_KW_TAP,        /**< TAP keyword. */

    JZ_TOK_OTHER           /**< Fallback single-character token. */
} JZTokenType;

/**
 * @struct JZToken
 * @brief A single lexical token with location and metadata.
 */
typedef struct JZToken {
    JZTokenType   type;    /**< Token type. */
    JZLocation    loc;     /**< Source location of the token. */
    char         *lexeme;  /**< Null-terminated token text (slice of source). */
    JZNumericInfo num;     /**< Numeric metadata (zeroed for non-numeric tokens). */
} JZToken;

/**
 * @struct JZTokenStream
 * @brief Dynamic array of tokens produced by the lexer.
 */
typedef struct JZTokenStream {
    JZToken *tokens;    /**< Array of tokens. */
    size_t   count;     /**< Number of tokens in the array. */
    size_t   capacity;  /**< Allocated capacity of the tokens array. */
} JZTokenStream;

/**
 * @brief Lex a source buffer into a token stream.
 *
 * Tokenizes the given source string, producing a flat array of tokens.
 * Lexer diagnostics (e.g., invalid characters, malformed literals) are
 * reported through the diagnostics list when non-NULL.
 *
 * @param filename   Source filename (used for JZLocation in each token).
 * @param source     Null-terminated source text to tokenize.
 * @param out_stream Pointer to a token stream to populate. Must not be NULL.
 * @param diagnostics Optional diagnostic list for error reporting (may be NULL).
 * @return 0 on success, non-zero on failure.
 */
int jz_lex_source(const char *filename,
                  const char *source,
                  JZTokenStream *out_stream,
                  JZDiagnosticList *diagnostics);

/**
 * @brief Free all resources owned by a token stream.
 * @param stream Pointer to the token stream to free. Must not be NULL.
 */
void jz_token_stream_free(JZTokenStream *stream);

#endif /* JZ_HDL_LEXER_H */
