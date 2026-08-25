#ifndef HCPL_ARENA_H
#define HCPL_ARENA_H

/*
 * Bump allocator for compile-time memory (token text, AST nodes, diagnostic
 * strings). docs/v0_0_1/language.md section 7 asks for exactly this: allocate
 * during compilation, then release the whole arena in one call instead of
 * chasing thousands of small frees.
 *
 * There is deliberately no arena_free() for individual allocations. Growing a
 * NodeList abandons its old array inside the arena rather than reusing it;
 * the waste is bounded at under 2x and buys a much simpler ownership story.
 */

#include <stdarg.h>
#include <stddef.h>

typedef struct ArenaBlock ArenaBlock;

typedef struct {
    ArenaBlock* head;
    size_t      block_size;
} Arena;

Arena* arena_create(size_t block_size);
void   arena_destroy(Arena* arena);

void*  arena_alloc(Arena* arena, size_t size);
void*  arena_zalloc(Arena* arena, size_t size);

char*  arena_strndup(Arena* arena, const char* text, size_t length);
char*  arena_strdup(Arena* arena, const char* text);
char*  arena_vsprintf(Arena* arena, const char* format, va_list args);
char*  arena_sprintf(Arena* arena, const char* format, ...);

size_t arena_bytes_used(const Arena* arena);

#endif /* HCPL_ARENA_H */
