#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "../../include/ast.h"
#include "../../include/util.h"

#define INITIAL_CHILD_CAPACITY 4

/*
 * Helper to duplicate a string while trimming leading and trailing
 * whitespace. Used for AST identifier names and free-form text so
 * that parser-constructed lexemes like "foo " or "expr ... " do not
 * carry trailing spaces into later phases (JSON, diagnostics, IR).
 */
static char *jz_strdup_trim(const char *s)
{
    if (!s) return NULL;

    const char *start = s;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }

    const char *end = s + strlen(s);
    while (end > start && isspace((unsigned char)*(end - 1))) {
        end--;
    }

    size_t len = (size_t)(end - start);
    char *copy = (char *)malloc(len + 1);
    if (!copy) return NULL;
    if (len > 0) {
        memcpy(copy, start, len);
    }
    copy[len] = '\0';
    return copy;
}

JZASTNode *jz_ast_new(JZASTNodeType type, JZLocation loc) {
    JZASTNode *node = (JZASTNode *)calloc(1, sizeof(JZASTNode));
    if (!node) return NULL;
    node->type = type;
    node->loc = loc;
    return node;
}

void jz_ast_set_name(JZASTNode *node, const char *name) {
    if (!node) return;
    free(node->name);
    node->name = name ? jz_strdup_trim(name) : NULL;
}

void jz_ast_set_block_kind(JZASTNode *node, const char *kind) {
    if (!node) return;
    free(node->block_kind);
    node->block_kind = kind ? jz_strdup_trim(kind) : NULL;
}

void jz_ast_set_text(JZASTNode *node, const char *text) {
    if (!node) return;
    free(node->text);
    node->text = text ? jz_strdup_trim(text) : NULL;
}

void jz_ast_set_width(JZASTNode *node, const char *width) {
    if (!node) return;
    free(node->width);
    node->width = width ? jz_strdup_trim(width) : NULL;
}

int jz_ast_add_child(JZASTNode *parent, JZASTNode *child) {
    if (!parent || !child) return -1;
    if (parent->child_count == parent->child_capacity) {
        size_t new_cap = parent->child_capacity ? parent->child_capacity * 2 : INITIAL_CHILD_CAPACITY;
        JZASTNode **new_children = (JZASTNode **)realloc(parent->children, new_cap * sizeof(JZASTNode *));
        if (!new_children) return -1;
        parent->children = new_children;
        parent->child_capacity = new_cap;
    }
    parent->children[parent->child_count++] = child;
    return 0;
}

void jz_ast_free(JZASTNode *node) {
    if (!node) return;
    for (size_t i = 0; i < node->child_count; ++i) {
        jz_ast_free(node->children[i]);
    }
    free(node->children);
    free(node->name);
    free(node->block_kind);
    free(node->text);
    free(node->width);
    free(node);
}
