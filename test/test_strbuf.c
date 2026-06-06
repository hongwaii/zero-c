#include "strbuf.h"
#include "agent_types.h"
#include <stdio.h>
#include <string.h>

static int g_failed = 0;
static int g_total  = 0;

#define CHECK(cond) do { \
    g_total++; \
    if (!(cond)) { \
        printf("  FAIL: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        g_failed++; \
    } \
} while (0)

static int test_init_and_append(void)
{
    strbuf_t sb;
    CHECK(strbuf_init(&sb, 16) == AGENT_OK);
    CHECK(strbuf_append(&sb, "hello") == AGENT_OK);
    CHECK(sb.data != NULL);
    CHECK(strcmp(sb.data, "hello") == 0);
    CHECK(sb.len == 5);
    strbuf_free(&sb);
    return 0;
}

static int test_grow(void)
{
    strbuf_t sb;
    CHECK(strbuf_init(&sb, 4) == AGENT_OK);
    CHECK(strbuf_append(&sb, "0123456789") == AGENT_OK);
    CHECK(sb.len == 10);
    CHECK(sb.cap >= 11);
    CHECK(strcmp(sb.data, "0123456789") == 0);
    strbuf_free(&sb);
    return 0;
}

static int test_appendf(void)
{
    strbuf_t sb;
    CHECK(strbuf_init(&sb, 16) == AGENT_OK);
    CHECK(strbuf_appendf(&sb, "n=%d s=%s", 42, "x") == AGENT_OK);
    CHECK(strcmp(sb.data, "n=42 s=x") == 0);
    strbuf_free(&sb);
    return 0;
}

static int test_reset(void)
{
    strbuf_t sb;
    CHECK(strbuf_init(&sb, 16) == AGENT_OK);
    CHECK(strbuf_append(&sb, "abc") == AGENT_OK);
    strbuf_reset(&sb);
    CHECK(sb.len == 0);
    CHECK(sb.data != NULL);
    CHECK(strcmp(sb.data, "") == 0);
    strbuf_free(&sb);
    return 0;
}

int main(void)
{
    test_init_and_append();
    test_grow();
    test_appendf();
    test_reset();
    if (g_failed == 0) {
        printf("test_strbuf: all pass (%d checks)\n", g_total);
        return 0;
    }
    printf("test_strbuf: %d/%d checks failed\n", g_failed, g_total);
    return 1;
}
