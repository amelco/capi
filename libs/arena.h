/*
 * Single header (STB style) bump allocator.
 *
 * An Arena owns a linked list of blocks. arena_alloc() carves aligned memory out of the current
 * block and grows by appending a new block when the current one is full, so the whole arena is
 * released in one shot with arena_free(). arena_reset() rewinds every block and keeps them for
 * reuse (useful when a worker thread serves request after request).
 *
 * Intended lifetime: REQUEST SCOPE. Give each thread its own Arena, use it for short-lived
 * allocations, and free it when the request is done. An Arena is NOT thread-safe on its own --
 * two threads must never share one without external locking, and the usual pattern is one Arena
 * per request/thread, so no locking is needed at all.
 *
 *   Arena arena;
 *   arena_init(&arena);
 *   char *scratch = arena_strdup(&arena, "temporary");
 *   ...
 *   arena_free(&arena);
 *
 * INVARIANT -- do NOT allocate process-lifetime data from a request arena. Anything that must
 * outlive the request (e.g. the shared email->code map and the user-code list, both owned and
 * freed by code that runs outside the request) must keep using malloc/realloc/strdup. If such a
 * pointer were carved from the arena, arena_free() at the end of the request would leave a
 * dangling pointer behind and the next access would be a use-after-free.
 *
 * Alignment: every allocation is aligned to _Alignof(max_align_t). Block size defaults to 4 KiB
 * (ARENA_BLOCK_SIZE) and a single oversized request gets its own dedicated block.
 *
 * Define ARENA_IMPLEMENTATION in exactly ONE translation unit before including this header.
 */

#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef ARENA_BLOCK_SIZE
#define ARENA_BLOCK_SIZE (4 * 1024)
#endif

typedef struct ArenaBlock {
    struct ArenaBlock *next;
    size_t capacity;
    size_t used;
    char data[];
} ArenaBlock;

typedef struct {
    ArenaBlock *head;
    ArenaBlock *current;
} Arena;

/* Sets up an empty arena. No memory is allocated until the first arena_alloc(). */
void arena_init(Arena *arena);
/* Returns `size` bytes aligned to max_align_t, or NULL on allocation failure / size == 0. */
void *arena_alloc(Arena *arena, size_t size);
/* Copies `content` (NUL included). Returns NULL if `content` is NULL or on allocation failure. */
char *arena_strdup(Arena *arena, const char *content);
/* Copies at most `size` bytes from `content` and NUL-terminates. NULL `content` -> NULL. */
char *arena_strndup(Arena *arena, const char *content, size_t size);
/* Rewinds all blocks to empty and keeps them allocated for reuse. */
void arena_reset(Arena *arena);
/* Releases every block. The arena is left initialized and can be reused. */
void arena_free(Arena *arena);

#endif // ARENA_H


#ifdef ARENA_IMPLEMENTATION

static ArenaBlock *_arena_new_block(size_t capacity)
{
    ArenaBlock *block = malloc(sizeof *block + capacity);
    if (!block) return NULL;
    block->next = NULL;
    block->capacity = capacity;
    block->used = 0;
    return block;
}

void arena_init(Arena *arena)
{
    arena->head = NULL;
    arena->current = NULL;
}

void *arena_alloc(Arena *arena, size_t size)
{
    if (size == 0) return NULL;

    const size_t align = _Alignof(max_align_t);
    size_t aligned_size = (size + align - 1) & ~(align - 1);

    ArenaBlock *block = arena->current;
    if (!block || block->used + aligned_size + align > block->capacity) {
        size_t wanted = aligned_size > ARENA_BLOCK_SIZE ? aligned_size : ARENA_BLOCK_SIZE;
        block = _arena_new_block(wanted + align);
        if (!block) return NULL;
        if (arena->current) arena->current->next = block;
        else arena->head = block;
        arena->current = block;
    }

    uintptr_t base = (uintptr_t)block->data + block->used;
    uintptr_t aligned = (base + (align - 1)) & ~(uintptr_t)(align - 1);
    block->used = (size_t)(aligned - (uintptr_t)block->data) + size;
    return (void *)aligned;
}

char *arena_strdup(Arena *arena, const char *content)
{
    if (!content) return NULL;
    size_t size = strlen(content);
    char *copy = arena_alloc(arena, size + 1);
    if (!copy) return NULL;
    memcpy(copy, content, size + 1);
    return copy;
}

char *arena_strndup(Arena *arena, const char *content, size_t size)
{
    if (!content) return NULL;
    char *copy = arena_alloc(arena, size + 1);
    if (!copy) return NULL;
    memcpy(copy, content, size);
    copy[size] = '\0';
    return copy;
}

void arena_reset(Arena *arena)
{
    for (ArenaBlock *block = arena->head; block; block = block->next) {
        block->used = 0;
    }
    arena->current = arena->head;
}

void arena_free(Arena *arena)
{
    ArenaBlock *block = arena->head;
    while (block) {
        ArenaBlock *next = block->next;
        free(block);
        block = next;
    }
    arena->head = NULL;
    arena->current = NULL;
}

#endif // ARENA_IMPLEMENTATION
