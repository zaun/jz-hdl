#include <string.h>
#include <stdlib.h>

#include "compiler.h"
#include "util.h"
#include "ast.h"

void jz_compiler_init(JZCompiler *comp, JZCompilerMode mode)
{
    if (!comp) {
        return;
    }
    memset(comp, 0, sizeof(*comp));
    comp->mode = mode;
    comp->ast_root = NULL;
    comp->ir_root = NULL;
    jz_arena_init(&comp->ast_arena, 0);
    jz_arena_init(&comp->ir_arena, 0);
    jz_diagnostic_list_init(&comp->diagnostics);
}

void jz_compiler_dispose(JZCompiler *comp)
{
    if (!comp) {
        return;
    }
    /* Free AST if still owned by the compiler. */
    if (comp->ast_root) {
        jz_ast_free(comp->ast_root);
        comp->ast_root = NULL;
    }
    comp->ir_root = NULL;
    jz_arena_free(&comp->ast_arena);
    jz_arena_free(&comp->ir_arena);
    jz_diagnostic_list_free(&comp->diagnostics);
}
