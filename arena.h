#ifndef MGCC_ARENA_H
#define MGCC_ARENA_H

#include <stddef.h>

struct arena_block {
    struct arena_block *next;
    size_t size;
    size_t used;
    char data[];
};

struct arena {
    struct arena_block *head;
    size_t block_size;
};

void arena_init(struct arena *a, size_t block_size);
void *arena_alloc(struct arena *a, size_t size);
void *arena_alloc_aligned(struct arena *a, size_t size, size_t align);
char *arena_strdup(struct arena *a, const char *s);
char *arena_strndup(struct arena *a, const char *s, size_t n);
void arena_free(struct arena *a);
void arena_reset(struct arena *a);

#define ARENA_DEFAULT_BLOCK (64 * 1024)

#endif
