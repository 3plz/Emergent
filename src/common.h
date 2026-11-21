#ifndef EMC_COMMON_H
#define EMC_COMMON_H

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int line;
    int col;
} Loc;

static inline Loc loc_make(int line, int col) {
    return (Loc){.line = line, .col = col};
}

static inline void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    if (ptr == NULL && size != 0) {
        fputs("emc: out of memory\n", stderr);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

static inline void *xcalloc(size_t count, size_t size) {
    void *ptr = calloc(count, size);
    if (ptr == NULL && count != 0 && size != 0) {
        fputs("emc: out of memory\n", stderr);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

static inline void *xrealloc(void *old, size_t size) {
    void *ptr = realloc(old, size);
    if (ptr == NULL && size != 0) {
        fputs("emc: out of memory\n", stderr);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

static inline char *xstrdup(const char *str) {
    assert(str);
    size_t len = strlen(str) + 1;
    char *copy = xmalloc(len);
    memcpy(copy, str, len);
    return copy;
}

static inline char *xstrndup(const char *str, size_t len) {
    assert(str);
    char *copy = xmalloc(len + 1);
    memcpy(copy, str, len);
    copy[len] = '\0';
    return copy;
}

#ifdef EMC_DEBUG
#define DBG(fmt, ...) fprintf(stderr, "[DEBUG %s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define DBG(fmt, ...) ((void)0)
#endif

#define da_grow(arr, count, cap)                                                                   \
    do {                                                                                           \
        if ((count) >= (cap)) {                                                                    \
            (cap) = (cap) ? (cap) * 2 : 8;                                                         \
            (arr) = xrealloc((arr), (cap) * sizeof(*(arr)));                                       \
        }                                                                                          \
    } while (0)

#endif
