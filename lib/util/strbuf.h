/**
 * @file strbuf.h
 * @brief Dynamic string buffer (no libc++).
 */
#ifndef UTIL_STRBUF_H
#define UTIL_STRBUF_H

#include <stdarg.h>
#include <stddef.h>

typedef struct {
    char  *data;
    size_t len;
    size_t cap;       /* always >= 1; data[cap-1] reserved for '\0' */
} strbuf_t;

int  strbuf_init(strbuf_t *sb, size_t initial_cap);
void strbuf_free(strbuf_t *sb);
int  strbuf_reserve(strbuf_t *sb, size_t needed);
int  strbuf_append(strbuf_t *sb, const char *s);
int  strbuf_append_n(strbuf_t *sb, const char *s, size_t n);
int  strbuf_appendf(strbuf_t *sb, const char *fmt, ...)
#ifdef __GNUC__
    __attribute__((format(printf, 2, 3)))
#endif
    ;
void strbuf_reset(strbuf_t *sb);

#endif /* UTIL_STRBUF_H */
