#define _POSIX_C_SOURCE 200809L
#include "arena.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void die_oom(void)
{
    static const char msg[] = "mgcc: out of memory\n";
    write(2, msg, sizeof(msg) - 1);
    _exit(1);
}

static struct arena_block *block_new(size_t size)
{
    struct arena_block *b = malloc(sizeof(*b) + size);
    if (!b)
        die_oom();
    b->next = NULL;
    b->size = size;
    b->used = 0;
    return b;
}

void arena_init(struct arena *a, size_t block_size)
{
    if (block_size == 0)
        block_size = ARENA_DEFAULT_BLOCK;
    a->block_size = block_size;
    a->head = block_new(block_size);
}

void *arena_alloc_aligned(struct arena *a, size_t size, size_t align)
{
    if (!a->head)
        arena_init(a, ARENA_DEFAULT_BLOCK);

    if (align == 0)
        align = 1;

    struct arena_block *b = a->head;
    while (b) {
        uintptr_t base = (uintptr_t)b->data + b->used;
        uintptr_t aligned = (base + align - 1) & ~(uintptr_t)(align - 1);
        size_t pad = aligned - base;
        if (b->used + pad + size <= b->size) {
            b->used += pad + size;
            return (void *)aligned;
        }
        if (b->next)
            b = b->next;
        else
            break;
    }

    size_t need = size + align;
    size_t bs = a->block_size > need ? a->block_size : need;
    struct arena_block *nb = block_new(bs);
    b->next = nb;
    uintptr_t base = (uintptr_t)nb->data;
    uintptr_t aligned = (base + align - 1) & ~(uintptr_t)(align - 1);
    nb->used = (aligned - base) + size;
    return (void *)aligned;
}

void *arena_alloc(struct arena *a, size_t size)
{
    return arena_alloc_aligned(a, size, sizeof(void *));
}

char *arena_strdup(struct arena *a, const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = arena_alloc(a, n);
    memcpy(p, s, n);
    return p;
}

char *arena_strndup(struct arena *a, const char *s, size_t n)
{
    size_t len = strnlen(s, n);
    char *p = arena_alloc(a, len + 1);
    memcpy(p, s, len);
    p[len] = '\0';
    return p;
}

void arena_free(struct arena *a)
{
    struct arena_block *b = a->head;
    while (b) {
        struct arena_block *next = b->next;
        free(b);
        b = next;
    }
    a->head = NULL;
    a->block_size = 0;
}

void arena_reset(struct arena *a)
{
    arena_free(a);
    arena_init(a, a->block_size ? a->block_size : ARENA_DEFAULT_BLOCK);
}
