#include <stdlib.h>
#include <string.h>

#include "../include/arena.h"

#define JZ_ARENA_DEFAULT_BLOCK_SIZE 4096

void jz_arena_init(JZArena *arena, size_t block_size)
{
    if (!arena) {
        return;
    }
    arena->head = NULL;
    arena->block_size = block_size ? block_size : JZ_ARENA_DEFAULT_BLOCK_SIZE;
}

void *jz_arena_alloc(JZArena *arena, size_t size)
{
    if (!arena || size == 0) {
        return NULL;
    }

    /* Ensure alignment to pointer size. */
    size = (size + sizeof(void *) - 1) & ~(sizeof(void *) - 1);

    JZArenaBlock *block = arena->head;
    if (!block || block->used + size > block->capacity) {
        size_t min_bytes = size + sizeof(JZArenaBlock);
        size_t req = arena->block_size > min_bytes ? arena->block_size : min_bytes;
        JZArenaBlock *new_block = (JZArenaBlock *)malloc(req);
        if (!new_block) {
            return NULL;
        }
        new_block->next = block;
        new_block->used = 0;
        new_block->capacity = req - sizeof(JZArenaBlock);
        arena->head = new_block;
        block = new_block;
    }

    void *ptr = block->data + block->used;
    block->used += size;
    return ptr;
}

void jz_arena_reset(JZArena *arena)
{
    if (!arena) {
        return;
    }
    /* Keep only the most recent block (if any) to avoid thrashing. */
    JZArenaBlock *block = arena->head;
    if (!block) {
        return;
    }
    JZArenaBlock *next = block->next;
    while (next) {
        JZArenaBlock *tmp = next->next;
        free(next);
        next = tmp;
    }
    block->next = NULL;
    block->used = 0;
}

void jz_arena_free(JZArena *arena)
{
    if (!arena) {
        return;
    }
    JZArenaBlock *block = arena->head;
    while (block) {
        JZArenaBlock *next = block->next;
        free(block);
        block = next;
    }
    arena->head = NULL;
}
