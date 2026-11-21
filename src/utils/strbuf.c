#include "strbuf.h"
#include "../common.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void sb_init(StrBuf *sb) {
    assert(sb);
    sb->cap = 1024;
    sb->len = 0;
    sb->data = xmalloc(sb->cap);
    sb->data[0] = '\0';
}

void sb_free(StrBuf *sb) {
    assert(sb);
    free(sb->data);
    sb->data = NULL;
    sb->len = sb->cap = 0;
}

void sb_ensure(StrBuf *sb, size_t needed) {
    assert(sb);
    if (sb->len + needed + 1 > sb->cap) {
        while (sb->len + needed + 1 > sb->cap) {
            sb->cap *= 2;
        }
        sb->data = xrealloc(sb->data, sb->cap);
    }
}

void sb_printf(StrBuf *sb, const char *fmt, ...) {
    assert(sb && fmt);
    va_list args;
    va_start(args, fmt);
    va_list args_copy;
    va_copy(args_copy, args);

    int needed = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    if (needed > 0) {
        sb_ensure(sb, (size_t)needed);
        vsnprintf(sb->data + sb->len, (size_t)needed + 1, fmt, args_copy);
        sb->len += (size_t)needed;
    }
    va_end(args_copy);
}
