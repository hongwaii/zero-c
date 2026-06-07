#include "llm_sse.h"
#include "agent_types.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* 收集所有事件，存到全局数组 */
#define MAX_EVENTS 32
static sse_event_t g_events[MAX_EVENTS];
static int g_count = 0;

static bool on_event(const sse_event_t *ev, void *ud)
{
    (void)ud;
    if (g_count >= MAX_EVENTS) return false;
    g_events[g_count++] = *ev;
    return true;
}

static void reset(void) { g_count = 0; memset(g_events, 0, sizeof(g_events)); }

static int test_simple_data(void)
{
    reset();
    sse_parser_t *p = sse_parser_create(on_event, NULL);
    /* 单条事件：data: hello\n\n */
    const char *msg = "data: hello\n\n";
    sse_parser_feed(p, msg, strlen(msg));
    sse_parser_finalize(p);
    assert(g_count == 1);
    assert(g_events[0].type == SSE_EV_DATA);
    assert(g_events[0].payload_len == 5);
    assert(memcmp(g_events[0].payload, "hello", 5) == 0);
    sse_parser_destroy(p);
    return 0;
}

static int test_split_feed(void)
{
    reset();
    sse_parser_t *p = sse_parser_create(on_event, NULL);
    /* 跨多次 feed */
    sse_parser_feed(p, "da", 2);
    sse_parser_feed(p, "ta: hello\n", 9);
    sse_parser_feed(p, "\n", 1);
    sse_parser_finalize(p);
    assert(g_count == 1);
    assert(g_events[0].type == SSE_EV_DATA);
    assert(g_events[0].payload_len == 5);
    sse_parser_destroy(p);
    return 0;
}

static int test_done_sentinel(void)
{
    reset();
    sse_parser_t *p = sse_parser_create(on_event, NULL);
    const char *msg = "data: hello\n\ndata: [DONE]\n\n";
    sse_parser_feed(p, msg, strlen(msg));
    sse_parser_finalize(p);
    assert(g_count == 2);
    assert(g_events[0].type == SSE_EV_DATA);
    assert(g_events[1].type == SSE_EV_DONE);
    sse_parser_destroy(p);
    return 0;
}

static int test_multi_data_lines(void)
{
    reset();
    sse_parser_t *p = sse_parser_create(on_event, NULL);
    /* OpenAI 实际格式：单事件可能含多个 data: 行，用 \n 拼 */
    const char *msg = "data: line1\ndata: line2\n\n";
    sse_parser_feed(p, msg, strlen(msg));
    sse_parser_finalize(p);
    assert(g_count == 1);
    /* payload = "line1\nline2" */
    assert(g_events[0].payload_len == 11);
    assert(memcmp(g_events[0].payload, "line1\nline2", 11) == 0);
    sse_parser_destroy(p);
    return 0;
}

static int test_comment_ignored(void)
{
    reset();
    sse_parser_t *p = sse_parser_create(on_event, NULL);
    const char *msg = ": heartbeat\n\ndata: ok\n\n";
    sse_parser_feed(p, msg, strlen(msg));
    sse_parser_finalize(p);
    assert(g_count == 1);
    assert(g_events[0].type == SSE_EV_DATA);
    sse_parser_destroy(p);
    return 0;
}

int main(void)
{
    test_simple_data();
    test_split_feed();
    test_done_sentinel();
    test_multi_data_lines();
    test_comment_ignored();
    printf("test_llm_sse: 5/5 pass\n");
    return 0;
}
