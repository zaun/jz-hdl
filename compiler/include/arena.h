/**
 * @file arena.h
 * @brief Region-based memory allocator for long-lived allocations.
 *
 * An arena (or region) allocator manages memory in contiguous blocks,
 * allowing fast allocation with minimal fragmentation. All allocations
 * within an arena share a single lifetime—typically freed together via
 * reset() or free(). This is ideal for temporary data structures like
 * AST/IR nodes that are allocated during parsing/compilation and freed
 * in bulk once processing completes.
 *
 * Features:
 * - O(1) allocation with minimal overhead
 * - Automatic pointer-size alignment
 * - Linked block management for unbounded allocations
 * - Efficient reset() to reuse the current block without deallocation
 * - Proper cleanup via free()
 *
 * Usage:
 *   JZArena arena;
 *   jz_arena_init(&arena, 0);  // Use default block size
 *   void *ptr = jz_arena_alloc(&arena, 256);
 *   jz_arena_reset(&arena);    // Reuse blocks
 *   jz_arena_free(&arena);     // Clean up all blocks
 */

#ifndef JZ_HDL_ARENA_H
#define JZ_HDL_ARENA_H

#include <stddef.h>

/**
 * @struct JZArenaBlock
 * @brief A single memory block within an arena.
 */
typedef struct JZArenaBlock {
    struct JZArenaBlock *next;      /* Pointer to the previous block in the linked list. */
    size_t               used;      /* Number of bytes currently allocated in this block. */
    size_t               capacity;  /* Total capacity of this block's data region. */
    unsigned char        data[];    /* Flexible array member for allocation data. */
} JZArenaBlock;

/**
 * @struct JZArena
 * @brief An arena allocator managing multiple blocks.
 */
typedef struct JZArena {
    JZArenaBlock *head;         /*  Pointer to the most recently allocated block.*/
    size_t        block_size;   /*  Minimum size for new blocks.*/
} JZArena;

/**
 * @brief Initialize an arena allocator.
 * @param arena Pointer to the arena to initialize.
 * @param block_size Minimum block size; 0 uses default (4096 bytes).
 */
void jz_arena_init(JZArena *arena, size_t block_size);

/**
 * @brief Allocate memory from the arena.
 * @param arena Pointer to the arena.
 * @param size Number of bytes to allocate.
 * @return Pointer to allocated memory, or NULL on failure.
 * @note Memory is automatically aligned to pointer size.
 */
void *jz_arena_alloc(JZArena *arena, size_t size);

/**
 * @brief Reset the arena, freeing all but the most recent block.
 * @param arena Pointer to the arena.
 * @note This preserves the current block to avoid allocation thrashing.
 */
void jz_arena_reset(JZArena *arena);

/**
 * @brief Free all blocks and clean up the arena.
 * @param arena Pointer to the arena.
 */
void jz_arena_free(JZArena *arena);

#endif /* JZ_HDL_ARENA_H */
