#ifndef EMC_CODEGEN_POOL_H
#define EMC_CODEGEN_POOL_H

#include <stddef.h>

typedef struct {
    char *text;
    char label[32];
} StringConst;

typedef struct {
    StringConst *items;
    size_t count;
    size_t capacity;
} StringPool;

void pool_init(StringPool *pool);
const char *pool_get_string(StringPool *pool, const char *str);
void pool_free(StringPool *pool);

#endif
