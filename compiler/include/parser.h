/**
 * @file parser.h
 * @brief HDL file parsing and AST generation.
 *
 * Parses HDL source files into abstract syntax trees (ASTs) using a token
 * stream from the lexer. Maintains imported module filenames for diagnostic
 * and AST location validity throughout compilation.
 */

#ifndef JZ_HDL_PARSER_H
#define JZ_HDL_PARSER_H

#include "ast.h"
#include "lexer.h"
#include "diagnostic.h"

/**
 * @brief Parse a file into an abstract syntax tree.
 * @param filename Path to the source file.
 * @param stream Token stream from the lexer.
 * @param diagnostics Pointer to diagnostic list for error collection.
 * @return Root AST node, or NULL on parse error.
 */
JZASTNode *jz_parse_file(const char *filename,
                         const JZTokenStream *stream,
                         JZDiagnosticList *diagnostics);

/**
 * @brief Free filename strings retained for imported modules.
 *
 * The parser preserves full path strings for imported files to keep
 * JZLocation.filename pointers in the AST and diagnostics valid for the
 * compilation lifetime. Call this once the AST and all diagnostics are
 * no longer needed, typically before program exit.
 */
void jz_parser_free_imported_filenames(void);

#endif /* JZ_HDL_PARSER_H */
