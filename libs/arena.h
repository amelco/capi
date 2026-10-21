#ifndef AHB_ARENA_H
#define AHB_ARENA_H

#include <stddef.h>

/* C99 alignment shenanigans */
#define ARENA_ALIGNMENT ((size_t)sizeof(union { long double ld; double d; void *ptr; long long ll; }))
// compile-time assert unused on purpose. Errors out on build if alignment is not a power of two
typedef char _arena_alignment_is_power_of_two[
    (ARENA_ALIGNMENT & (ARENA_ALIGNMENT - 1)) == 0 ? 1 : -1];
/* end alignment shenanigans */ 

typedef struct {
    void *data;
    size_t capacity;
    size_t index;
} Arena;

void arena_init(Arena *arena);
char *arena_alloc(Arena *arena, size_t size);
void arena_reset(Arena *arena);
void arena_free(Arena *arena);

// aren't those should be on ahb_lib??
char *arena_strdup(Arena *arena, const char *content);
char *arena_strndup(Arena *arena, const char *content, size_t size);

#endif // AHB_ARENA_H

#ifdef ARENA_IMPLEMENTATION
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#ifndef INIT_CAPACITY
#define INIT_CAPACITY (4 * 1024 * 1024)  // 4 MB
#endif


// 'private' functions
// TODO: do not use realloc, so we don't loose out pointers
//       use malloc instead to create a new arena "block" and link the older with the newer somehow 
static bool _realloc_if_needed(Arena *arena, size_t size)
{
    assert(arena->index <= arena->capacity);
    if (size == 0) return true;

    size_t remaining_space = arena->capacity - arena->index;
    bool needs_realloc = size > remaining_space;

    if (needs_realloc) {
        size_t needed = arena->index + size;
        size_t new_size = arena->capacity ? arena->capacity : INIT_CAPACITY;
        // make sure that we allocate enough space
        while (new_size < needed) {
            if (new_size > SIZE_MAX / 2) { 
                // double new_size here would overflow size_t
                new_size = needed; 
                break; 
            }
            new_size *= 2;
        }

        void *new_data = realloc(arena->data, new_size);
        if (new_data == NULL) return false;

        arena->data = new_data;
        arena->capacity = new_size;
    }
    return true;
}

// function implementations

// this is not really needed. If you have arena zero initilized you can call arena_alloc() directly.
// keeping this for simetry (arena_init, arena_free)
void arena_init(Arena *arena) {
    (void)_realloc_if_needed(arena, INIT_CAPACITY);
}

// arena must be zero initialized (e.g. `Arena arena = {0};`)
// it's the only way we can guarantee correct behaviour
// returns memory aligned to ARENA_ALIGNMENT, or NULL on failure
char *arena_alloc(Arena *arena, size_t size)
{
    if (size == 0) return NULL;

    assert(arena->index % ARENA_ALIGNMENT == 0);

    // simple alignment implementation
    size_t padding = size % ARENA_ALIGNMENT;
    if (padding != 0) padding = ARENA_ALIGNMENT - padding;
    if (size > SIZE_MAX - padding) return NULL;  // size + padding would wrap size_t

    size_t reserved = size + padding;
    if (reserved > SIZE_MAX - arena->index) return NULL;  // index + reserved would wrap size_t
    if (!_realloc_if_needed(arena, reserved)) return NULL;

    char *ini = (char*)arena->data + arena->index;
    arena->index += reserved;
    return ini;
}

// returns NULL of failure
char *arena_strdup(Arena *arena, const char *content)
{
    assert(content != NULL && "content cannot be NULL on arena_strdup()");
    return arena_strndup(arena, content, 0);
}

// returns NULL of failure
char *arena_strndup(Arena *arena, const char *content, size_t size)
{
    assert(content != NULL && "content cannot be NULL on arena_strndup()");
    size_t content_size = size == 0 ? strlen(content) : size;
    char *pointer = arena_alloc(arena, content_size + 1);
    if (pointer == NULL) return NULL;
    memcpy(pointer, content, content_size);
    pointer[content_size] = '\0';
    return pointer;
}

void arena_free(Arena *arena)
{
    if (arena->data) {
        free(arena->data);
        arena->data = NULL;
        arena->index = 0;
        arena->capacity = 0;
    }
}

void arena_reset(Arena *arena)
{
    arena->index = 0;
}

#endif // ARENA_IMPLEMENTATION
