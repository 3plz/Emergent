#ifndef EMC_UTILS_STRBUF_H
#define EMC_UTILS_STRBUF_H

#include <stddef.h>

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} StrBuf;

void sb_init(StrBuf *sb);
void sb_free(StrBuf *sb);
void sb_ensure(StrBuf *sb, size_t needed);
void sb_printf(StrBuf *sb, const char *fmt, ...);

#endif
