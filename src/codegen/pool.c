#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include "pool.h"
#include "../common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void pool_init(StringPool *pool) {
    assert(pool);
    pool->items = NULL;
    pool->count = 0;
    pool->capacity = 0;
}

/* TODO: Replace the linear scan with a hash map once we hit 100 strings.  */
const char *pool_get_string(StringPool *pool, const char *str) {
    assert(pool && str);

    for (size_t i = 0; i < pool->count; i++) {
        if (strcmp(pool->items[i].text, str) == 0) {
            return pool->items[i].label;
        }
    }

    da_grow(pool->items, pool->count, pool->capacity);

    size_t idx = pool->count++;
    pool->items[idx].text = xstrdup(str);
    snprintf(pool->items[idx].label, sizeof(pool->items[idx].label), "str_%zu", idx);
    return pool->items[idx].label;
}

void pool_free(StringPool *pool) {
    assert(pool);
    for (size_t i = 0; i < pool->count; i++) {
        free(pool->items[i].text);
    }
    free(pool->items);
    pool->items = NULL;
    pool->count = pool->capacity = 0;
}
