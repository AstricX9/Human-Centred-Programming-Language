#include "arena.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARENA_ALIGNMENT     16u
#define ARENA_DEFAULT_BLOCK (64u * 1024u)

struct ArenaBlock {
    ArenaBlock*    next;
    size_t         used;
    size_t         capacity;
    unsigned char* data;
};

static size_t align_up(size_t value) {
    return (value + (ARENA_ALIGNMENT - 1u)) & ~(size_t)(ARENA_ALIGNMENT - 1u);
}

static ArenaBlock* block_create(size_t capacity) {
    ArenaBlock* block = (ArenaBlock*)malloc(sizeof(ArenaBlock));
    if (!block) return NULL;

    block->data = (unsigned char*)malloc(capacity);
    if (!block->data) {
        free(block);
        return NULL;
    }

    block->next     = NULL;
    block->used     = 0;
    block->capacity = capacity;
    return block;
}

Arena* arena_create(size_t block_size) {
    Arena* arena = (Arena*)malloc(sizeof(Arena));
    if (!arena) return NULL;

    arena->block_size = block_size ? block_size : ARENA_DEFAULT_BLOCK;
    arena->head       = block_create(arena->block_size);

    if (!arena->head) {
        free(arena);
        return NULL;
    }
    return arena;
}

void arena_destroy(Arena* arena) {
    ArenaBlock* block;
    if (!arena) return;

    block = arena->head;
    while (block) {
        ArenaBlock* next = block->next;
        free(block->data);
        free(block);
        block = next;
    }
    free(arena);
}

void* arena_alloc(Arena* arena, size_t size) {
    size_t      needed;
    ArenaBlock* block;
    void*       result;

    if (!arena) return NULL;

    needed = align_up(size ? size : 1u);

    if (arena->head->used + needed > arena->head->capacity) {
        /* An oversized request gets a block of its own so one large string
           cannot strand the rest of a fresh block. */
        size_t capacity = needed > arena->block_size ? needed : arena->block_size;

        block = block_create(capacity);
        if (!block) return NULL;

        block->next = arena->head;
        arena->head = block;
    }

    result = arena->head->data + arena->head->used;
    arena->head->used += needed;
    return result;
}

void* arena_zalloc(Arena* arena, size_t size) {
    void* memory = arena_alloc(arena, size);
    if (memory) memset(memory, 0, size);
    return memory;
}

char* arena_strndup(Arena* arena, const char* text, size_t length) {
    char* copy = (char*)arena_alloc(arena, length + 1u);
    if (!copy) return NULL;

    if (length && text) memcpy(copy, text, length);
    copy[length] = '\0';
    return copy;
}

char* arena_strdup(Arena* arena, const char* text) {
    if (!text) return NULL;
    return arena_strndup(arena, text, strlen(text));
}

char* arena_vsprintf(Arena* arena, const char* format, va_list args) {
    va_list measure;
    int     length;
    char*   buffer;

    va_copy(measure, args);
    length = vsnprintf(NULL, 0, format, measure);
    va_end(measure);

    if (length < 0) return NULL;

    buffer = (char*)arena_alloc(arena, (size_t)length + 1u);
    if (!buffer) return NULL;

    vsnprintf(buffer, (size_t)length + 1u, format, args);
    return buffer;
}

char* arena_sprintf(Arena* arena, const char* format, ...) {
    va_list args;
    char*   result;

    va_start(args, format);
    result = arena_vsprintf(arena, format, args);
    va_end(args);
    return result;
}

size_t arena_bytes_used(const Arena* arena) {
    const ArenaBlock* block;
    size_t            total = 0;

    if (!arena) return 0;

    for (block = arena->head; block; block = block->next) {
        total += block->used;
    }
    return total;
}
