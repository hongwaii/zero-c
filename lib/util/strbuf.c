/**
 * @file strbuf.c
 */
#include "strbuf.h"
#include "agent_types.h"   /* for AGENT_ERR_* */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int strbuf_init(strbuf_t *sb, size_t initial_cap)
{
    if (!sb || initial_cap == 0) return AGENT_ERR_BAD_ARG;
    sb->data = (char *)malloc(initial_cap);
    if (!sb->data) return AGENT_ERR_OOM;
    sb->data[0] = '\0';
    sb->len = 0;
    sb->cap = initial_cap;
    return AGENT_OK;
}

void strbuf_free(strbuf_t *sb)
{
    if (!sb) return;
    free(sb->data);
    sb->data = NULL;
    sb->len = sb->cap = 0;
}

int strbuf_reserve(strbuf_t *sb, size_t needed)
{
    if (!sb) return AGENT_ERR_BAD_ARG;
    /* need space for: current len + needed bytes + 1 NUL */
    size_t total = sb->len + needed + 1;
    if (total <= sb->cap) return AGENT_OK;
    size_t new_cap = sb->cap;
    while (new_cap < total) new_cap *= 2;
    char *p = (char *)realloc(sb->data, new_cap);
    if (!p) return AGENT_ERR_OOM;
    sb->data = p;
    sb->cap = new_cap;
    return AGENT_OK;
}

int strbuf_append_n(strbuf_t *sb, const char *s, size_t n)
{
    if (!sb || !s) return AGENT_ERR_BAD_ARG;
    int rc = strbuf_reserve(sb, n);
    if (rc != AGENT_OK) return rc;
    memcpy(sb->data + sb->len, s, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
    return AGENT_OK;
}

int strbuf_append(strbuf_t *sb, const char *s)
{
    return s ? strbuf_append_n(sb, s, strlen(s)) : AGENT_ERR_BAD_ARG;
}

int strbuf_appendf(strbuf_t *sb, const char *fmt, ...)
{
    if (!sb || !fmt) return AGENT_ERR_BAD_ARG;
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    int needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (needed < 0) { va_end(ap2); return AGENT_ERR_IO; }
    int rc = strbuf_reserve(sb, (size_t)needed);
    if (rc != AGENT_OK) { va_end(ap2); return rc; }
    vsnprintf(sb->data + sb->len, sb->cap - sb->len, fmt, ap2);
    va_end(ap2);
    sb->len += (size_t)needed;
    return AGENT_OK;
}

void strbuf_reset(strbuf_t *sb)
{
    if (!sb || !sb->data) return;
    sb->len = 0;
    sb->data[0] = '\0';
}
