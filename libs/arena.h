/*
* Single header (STB style) library
* 
* This file has an basic arena library. 
*
* by Andre Herman F. Bezerra (amelco), 2026
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

void arena_init(Arena *arena);
void *arena_alloc(Arena *arena, size_t size);
char *arena_strdup(Arena *arena, const char *content);
char *arena_strndup(Arena *arena, const char *content, size_t size);
void arena_reset(Arena *arena);
void arena_free(Arena *arena);

#endif // ARENA_H


#ifdef ARENA_IMPLEMENTATION

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define ARENA_ALIGNMENT ((size_t)_Alignof(max_align_t))
#else
typedef union ArenaAlignment {
    long double ld;
    double d;
    void *ptr;
    long long ll;
} ArenaAlignment;
#define ARENA_ALIGNMENT ((size_t)sizeof(ArenaAlignment))
#endif

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

    const size_t align = ARENA_ALIGNMENT;
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
