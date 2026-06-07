/**
 * @file test_diag_sms.c
 * @brief diag_sms 单元测试：mock at_session，验证 4 步状态机。
 *
 * 覆盖：
 *   1) HAPPY PATH：CMGF OK → CMGS → URC ">" → raw(text+0x1A) → OK
 *      验证：状态机走通、调用次数、最后 raw 末尾是 0x1A、status="OK"
 *   2) CMGF 失败：mock at_session_send 返 ERROR，diag_sms 应 cb(false, ...)
 *   3) 参数校验：NULL 参数 / number 含双引号 → AGENT_ERR_BAD_ARG
 *   4) number 超长 → AGENT_ERR_BAD_ARG
 *
 * 关键设计：mock_at_session.c 提供 weak 符号 at_session_send / send_raw /
 * register_urc。本测试 exe 只链 lib_diag_service（不链 lib_at_engine），
 * 弱符号生效。
 */
#include "diag_sms.h"
#include "at_session.h"
#include "agent_types.h"
#include "mock_at_session.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ---------- CHECK 宏（参考 test_at_session.c 风格） ---------- */
static int g_checks = 0, g_failed = 0;
#define CHECK(c, m) do { \
    g_checks++; \
    if (!(c)) { \
        g_failed++; \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, m); \
    } \
} while (0)

/* ---------- 完成回调捕获 ---------- */
typedef struct {
    int  done;
    bool ok;
    char status[64];
} sms_result_t;

static sms_result_t g_res;

static void on_sms_done(bool ok, const char *status, void *ud)
{
    (void)ud;
    g_res.done = 1;
    g_res.ok   = ok;
    if (status) {
        strncpy(g_res.status, status, sizeof(g_res.status) - 1);
        g_res.status[sizeof(g_res.status) - 1] = '\0';
    }
}

static void reset_result(void) { memset(&g_res, 0, sizeof(g_res)); }

/* ---------- 公共 setup ---------- */
/* at_session 在头文件里只是前向声明，struct 大小未公开。
 * mock 不解引用 s，所以准备一个固定大小的字节 buffer，强制 cast 成
 * at_session_t* 传给 diag_send_sms——纯粹占位。 */
static char g_dummy_sess[256];

static at_session_t *dummy_sess(void)
{
    return (at_session_t *)g_dummy_sess;
}

static void setup(void)
{
    memset(g_dummy_sess, 0, sizeof(g_dummy_sess));
    mock_at_set_mode(MOCK_MODE_SIMPLE);
    /* 重置 mock 计数器：本测试 exe 多 case 共用 mock state，每次重置 */
    /* mock_at_session.c 没有 reset 函数；用 mode 切换 + 直接重新开始
     * 不精确，但 case 各自独立调 diag_send_sms 也只走一遍，counter 累加。
     * 下面 case 用"差值"验证（counter 至少增加 N）。 */
}

/* ---------- Test 1: HAPPY PATH ---------- */
static void test_happy_path(void)
{
    setup();
    reset_result();

    int send_before     = mock_at_send_count();
    int send_raw_before = mock_at_send_raw_count();
    int urc_before      = mock_at_register_urc_count();

    int rc = diag_send_sms(dummy_sess(), "10086", "hello", 3000, on_sms_done, NULL);
    CHECK(rc == AGENT_OK, "diag_send_sms returns AGENT_OK");

    /* 验证 call sequence：
     *   1) send("AT+CMGF=1")  → +1
     *   2) register_urc(">")  → +1（URC 同步触发 → 调 on_prompt_urc）
     *   3) send("AT+CMGS=...")→ +1（在 prompt_urc 后由 on_cmgf_done 内部触发）
     *   4) send_raw(text+0x1A)→ +1（prompt_urc 内部触发，URC 同步后立即调）
     */
    CHECK(mock_at_send_count() - send_before >= 2,
          "send called >= 2x (CMGF + CMGS)");
    CHECK(mock_at_register_urc_count() - urc_before == 1,
          "register_urc called 1x");
    CHECK(mock_at_send_raw_count() - send_raw_before == 1,
          "send_raw called 1x");

    /* 验证最后一条 send 是 CMGS */
    char last[128];
    mock_at_last_cmd(last, sizeof(last));
    CHECK(strncmp(last, "AT+CMGS=\"", 9) == 0,
          "last send cmd is AT+CMGS=...");

    /* 验证 raw 末尾是 Ctrl-Z 0x1A */
    uint8_t raw_buf[256];
    size_t  raw_len = 0;
    mock_at_last_raw(raw_buf, &raw_len);
    CHECK(raw_len == 6, "raw_len == 5 (hello) + 1 (Ctrl-Z) == 6");
    if (raw_len >= 1) {
        CHECK(raw_buf[raw_len - 1] == 0x1A, "raw ends with 0x1A (Ctrl-Z)");
    }
    if (raw_len >= 5) {
        CHECK(memcmp(raw_buf, "hello", 5) == 0, "raw starts with 'hello'");
    }

    /* 验证完成回调触发 + status = "OK" */
    CHECK(g_res.done == 1, "sms done callback fired");
    CHECK(g_res.ok == true, "sms ok == true");
    CHECK(strcmp(g_res.status, "OK") == 0, "status == OK");
}

/* ---------- Test 2: CMGF 失败 ---------- */
static void test_cmgf_error(void)
{
    setup();
    reset_result();
    mock_at_set_mode(MOCK_MODE_ERROR);

    int rc = diag_send_sms(dummy_sess(), "10086", "test", 3000, on_sms_done, NULL);
    CHECK(rc == AGENT_OK, "diag_send_sms returns AGENT_OK (async start)");

    /* CMGF 失败 → cb(false, "ERROR: CMGF failed") */
    CHECK(g_res.done == 1, "callback fired");
    CHECK(g_res.ok == false, "ok == false");
    CHECK(strncmp(g_res.status, "ERROR", 5) == 0, "status starts with ERROR");
}

/* ---------- Test 3: 参数校验 ---------- */
static void test_bad_args(void)
{
    setup();
    reset_result();

    /* NULL session */
    int rc = diag_send_sms(NULL, "10086", "x", 3000, on_sms_done, NULL);
    CHECK(rc == AGENT_ERR_BAD_ARG, "NULL session → BAD_ARG");

    /* NULL number */
    rc = diag_send_sms(dummy_sess(), NULL, "x", 3000, on_sms_done, NULL);
    CHECK(rc == AGENT_ERR_BAD_ARG, "NULL number → BAD_ARG");

    /* NULL text */
    rc = diag_send_sms(dummy_sess(), "10086", NULL, 3000, on_sms_done, NULL);
    CHECK(rc == AGENT_ERR_BAD_ARG, "NULL text → BAD_ARG");

    /* NULL cb */
    rc = diag_send_sms(dummy_sess(), "10086", "x", 3000, NULL, NULL);
    CHECK(rc == AGENT_ERR_BAD_ARG, "NULL cb → BAD_ARG");

    /* number 含双引号 */
    rc = diag_send_sms(dummy_sess(), "100\"86", "x", 3000, on_sms_done, NULL);
    CHECK(rc == AGENT_ERR_BAD_ARG, "number with quote → BAD_ARG");

    /* number 含 \r */
    rc = diag_send_sms(dummy_sess(), "100\r86", "x", 3000, on_sms_done, NULL);
    CHECK(rc == AGENT_ERR_BAD_ARG, "number with CR → BAD_ARG");
}

/* ---------- Test 4: number 超长 ---------- */
static void test_long_number(void)
{
    setup();
    reset_result();

    char big[64];
    memset(big, '1', 63);
    big[63] = '\0';
    int rc = diag_send_sms(dummy_sess(), big, "x", 3000, on_sms_done, NULL);
    CHECK(rc == AGENT_ERR_BAD_ARG, "63-char number → BAD_ARG");
}

/* ---------- Test 5: 空字符串 text 也能发（仅 0x1A） ---------- */
static void test_empty_text(void)
{
    setup();
    reset_result();

    int send_raw_before = mock_at_send_raw_count();
    int rc = diag_send_sms(dummy_sess(), "10086", "", 3000, on_sms_done, NULL);
    CHECK(rc == AGENT_OK, "empty text starts OK");

    CHECK(mock_at_send_raw_count() - send_raw_before == 1, "send_raw called 1x");
    uint8_t raw_buf[8];
    size_t  raw_len = 0;
    mock_at_last_raw(raw_buf, &raw_len);
    CHECK(raw_len == 1, "raw is just Ctrl-Z (1 byte)");
    if (raw_len == 1) {
        CHECK(raw_buf[0] == 0x1A, "raw[0] == 0x1A");
    }
    CHECK(g_res.ok == true, "empty text → ok");
}

int main(void)
{
    test_happy_path();
    test_cmgf_error();
    test_bad_args();
    test_long_number();
    test_empty_text();

    printf("test_diag_sms: %d/%d pass\n", g_checks - g_failed, g_checks);
    return g_failed == 0 ? 0 : 1;
}
