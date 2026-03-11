/**
 * @file lsp.h
 * @brief Language Server Protocol (LSP) server for JZ-HDL.
 *
 * Provides a stdio-based LSP server that exposes JZ-HDL compiler
 * diagnostics, hover information, and document synchronization to
 * any LSP-compatible editor.
 */

#ifndef JZ_HDL_LSP_H
#define JZ_HDL_LSP_H

/**
 * @brief Run the LSP server main loop.
 *
 * Reads JSON-RPC messages from stdin and writes responses to stdout.
 * Blocks until the client sends a shutdown/exit sequence or stdin is
 * closed.
 *
 * @return 0 on clean exit, non-zero on error.
 */
int jz_lsp_run(void);

#endif /* JZ_HDL_LSP_H */
