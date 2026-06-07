/**
 * @file test_at_session.c
 * @brief at_session 单元测试：队列、final/URC 派发、超时。
 *
 * 用一个 mock modem_chan_t（不挂真实串口），通过 fake_rx() 手动调
 * chan->on_rx 喂字节流，覆盖：
 *   - 单条 AT + OK（无 data）
 *   - AT +CSQ → +CSQ 数据行 + OK
 *   - AT +FOO → ERROR
 *   - URC 派发（订阅 +CEREG，混在 AT+COPS? 响应中）
 *   - 队列两条依次完成
 */
#include "at_session.h"
#include "at_parser.h"
#include "agent_chan.h"

#include <uv.h>
#include <stdio.h>
#include <string.h>

/* 简单的 CHECK 宏：累计检查数与失败数 */
static int g_checks = 0, g_failed = 0;
#define CHECK(c, m) do { \
    g_checks++; \
    if (!(c)) { \
        g_failed++; \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, m); \
    } \
} while (0)

/* 共享一个 mock chan + loop（全局方便各测试函数复用） */
static modem_chan_t  g_chan;
static uv_loop_t    *g_loop;

/* mock 用的 send 桩：返回 0（成功），不真发任何字节 */
static int mock_send(modem_chan_t *self, const uint8_t *buf, size_t len)
{
    (void)self; (void)buf; (void)len;
    return 0;
}

/* mock 用的虚表——只 stub send，因为测试不发真字节 */
static const modem_chan_ops_t s_mock_ops = {
    .open  = NULL,
    .send  = mock_send,
    .close = NULL,
};

/* 命令完成回调记录 */
typedef struct {
    char   result[256];
    size_t len;
    bool   ok;
    int    count;
} cb_record_t;
static cb_record_t g_cb;

/* URC 回调记录 */
typedef struct {
    char line[256];
    size_t len;
    int  count;
} urc_record_t;
static urc_record_t g_urc;

/**
 * @brief 命令完成回调：把 (result, len, ok) 拷到全局记录。
 */
static void on_complete(void *ud, const char *res, size_t len, bool ok)
{
    (void)ud;
    g_cb.count++;
    g_cb.ok = ok;
    g_cb.len = (len < sizeof(g_cb.result) - 1) ? len : sizeof(g_cb.result) - 1;
    memcpy(g_cb.result, res, g_cb.len);
    g_cb.result[g_cb.len] = '\0';
}

/**
 * @brief URC 回调：把行内容拷到全局记录。
 */
static void on_urc(void *ud, const char *line, size_t len)
{
    (void)ud;
    g_urc.count++;
    g_urc.len = (len < sizeof(g_urc.line) - 1) ? len : sizeof(g_urc.line) - 1;
    memcpy(g_urc.line, line, g_urc.len);
    g_urc.line[g_urc.len] = '\0';
}

/**
 * @brief 重置两条全局记录。
 */
static void reset_records(void)
{
    memset(&g_cb, 0, sizeof(g_cb));
    memset(&g_urc, 0, sizeof(g_urc));
}

/**
 * @brief 模拟"模组回包"：把字符串作为一整批字节喂给 on_rx。
 */
static void fake_rx(const char *s)
{
    if (g_chan.on_rx) {
        g_chan.on_rx(g_chan.userdata, (const uint8_t *)s, strlen(s));
    }
}

/* 单条 AT → 立即 OK：验证 result 为空、ok==true、回调触发 1 次。 */
static int test_send_and_ok(void)
{
    reset_records();
    at_session_t *s = at_session_create(g_loop, &g_chan);
    CHECK(s != NULL, "create");
    at_session_open(s);

    at_session_send(s, "AT", 1000, on_complete, NULL);
    fake_rx("OK\r\n");
    uv_run(g_loop, UV_RUN_NOWAIT);

    CHECK(g_cb.count == 1, "1 completion");
    CHECK(g_cb.ok == true, "ok == true");
    CHECK(g_cb.len == 0, "result empty");

    at_session_close(s);
    free(s);
    return 0;
}

/* AT+CSQ → +CSQ: 23,99 后跟 OK：验证 result 累积为 "+CSQ: 23,99"。 */
static int test_send_and_data_then_ok(void)
{
    reset_records();
    at_session_t *s = at_session_create(g_loop, &g_chan);
    at_session_open(s);

    at_session_send(s, "AT+CSQ", 1000, on_complete, NULL);
    fake_rx("+CSQ: 23,99\r\nOK\r\n");
    uv_run(g_loop, UV_RUN_NOWAIT);

    CHECK(g_cb.count == 1, "1 completion");
    CHECK(g_cb.ok == true, "ok");
    CHECK(strcmp(g_cb.result, "+CSQ: 23,99") == 0, "result == +CSQ: 23,99");

    at_session_close(s);
    free(s);
    return 0;
}

/* AT+FOO → ERROR：验证 ok==false、回调触发 1 次。 */
static int test_send_and_error(void)
{
    reset_records();
    at_session_t *s = at_session_create(g_loop, &g_chan);
    at_session_open(s);

    at_session_send(s, "AT+FOO", 1000, on_complete, NULL);
    fake_rx("ERROR\r\n");
    uv_run(g_loop, UV_RUN_NOWAIT);

    CHECK(g_cb.count == 1, "1 completion");
    CHECK(g_cb.ok == false, "ok == false");

    at_session_close(s);
    free(s);
    return 0;
}

/* URC 派发：订阅 +CEREG，串里夹 +CEREG 与 +COPS 数据行。 */
static int test_urc_dispatch(void)
{
    reset_records();
    at_session_t *s = at_session_create(g_loop, &g_chan);
    at_session_open(s);

    at_session_register_urc(s, "+CEREG", on_urc, NULL);

    at_session_send(s, "AT+COPS?", 1000, on_complete, NULL);
    fake_rx("+CEREG: 5\r\n+COPS: 0,0,\"China Mobile\",7\r\nOK\r\n");
    uv_run(g_loop, UV_RUN_NOWAIT);

    CHECK(g_urc.count == 1, "URC fired 1x");
    CHECK(strcmp(g_urc.line, "+CEREG: 5") == 0, "URC line");
    CHECK(g_cb.count == 1, "cmd done 1x");
    CHECK(g_cb.ok == true, "cmd ok");
    /* +COPS 一行被 at_session 归到 DATA（不匹配任何 URC 前缀），进 cmd result */
    CHECK(strcmp(g_cb.result, "+COPS: 0,0,\"China Mobile\",7") == 0, "cmd result");

    at_session_close(s);
    free(s);
    return 0;
}

/* 队列两条：发 AT, AT+CSQ；第一条 OK 完后第二条自动出。 */
static int test_queue_two_cmds(void)
{
    reset_records();
    at_session_t *s = at_session_create(g_loop, &g_chan);
    at_session_open(s);

    at_session_send(s, "AT", 1000, on_complete, NULL);
    at_session_send(s, "AT+CSQ", 1000, on_complete, NULL);
    fake_rx("OK\r\n");
    uv_run(g_loop, UV_RUN_NOWAIT);
    CHECK(g_cb.count == 1, "first done");

    fake_rx("+CSQ: 19,99\r\nOK\r\n");
    uv_run(g_loop, UV_RUN_NOWAIT);
    CHECK(g_cb.count == 2, "second done");
    CHECK(g_cb.ok == true, "ok");
    CHECK(strcmp(g_cb.result, "+CSQ: 19,99") == 0, "csq result");

    at_session_close(s);
    free(s);
    return 0;
}

int main(void)
{
    g_loop = uv_default_loop();
    memset(&g_chan, 0, sizeof(g_chan));
    g_chan.ops = &s_mock_ops;

    test_send_and_ok();
    test_send_and_data_then_ok();
    test_send_and_error();
    test_urc_dispatch();
    test_queue_two_cmds();

    printf("test_at_session: %d/%d pass\n", g_checks - g_failed, g_checks);
    return g_failed == 0 ? 0 : 1;
}
