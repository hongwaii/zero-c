/**
 * @file mock_at_session.c
 * @brief at_session 关键函数的 weak symbol mock 桩。
 *
 * 提供 weak 版本的 at_session_send / at_session_send_raw / at_session_register_urc。
 * 当测试 exe 不链 lib_at_engine（只链 lib_diag_service）时，这些 weak 实现
 * 生效；链 lib_at_engine 时则被 .c 里的强符号覆盖。
 *
 * 默认 mock 行为（mock_at_set_mode 切换）：
 *   - MOCK_MODE_SIMPLE：
 *       1) at_session_send("AT+CMGF=1") → cb(true, "OK", 2)
 *       2) at_session_register_urc(">") → 立即同步调 cb("> ", 2)
 *       3) at_session_send("AT+CMGS=...") → 不调 cb（等 URC）
 *       4) at_session_send_raw(text+0x1A) → cb(true, "+CMGS: 1\r\nOK", 12)
 *   - MOCK_MODE_ERROR：
 *       1) at_session_send("AT+CMGF=1") → cb(false, "ERROR", 5)
 *   - MOCK_MODE_URC_NEVER：
 *       URC 不触发（测超时——v1.1 扩展）
 */
#include "mock_at_session.h"

#include <stdio.h>
#include <string.h>

/* ---------- 全局状态 ---------- */
static mock_mode_t g_mode = MOCK_MODE_SIMPLE;

static int  g_send_count         = 0;
static int  g_send_raw_count     = 0;
static int  g_register_urc_count = 0;
static char g_last_cmd[128]      = {0};
static uint8_t g_last_raw[256]   = {0};
static size_t  g_last_raw_len    = 0;

void mock_at_set_mode(mock_mode_t m) { g_mode = m; }
mock_mode_t mock_at_get_mode(void)   { return g_mode; }
int mock_at_send_count(void)         { return g_send_count; }
int mock_at_send_raw_count(void)     { return g_send_raw_count; }
int mock_at_register_urc_count(void) { return g_register_urc_count; }

void mock_at_last_cmd(char *out, size_t out_size)
{
    if (!out || out_size == 0) return;
    strncpy(out, g_last_cmd, out_size - 1);
    out[out_size - 1] = '\0';
}

void mock_at_last_raw(uint8_t *out, size_t *out_len)
{
    if (out && g_last_raw_len > 0) {
        memcpy(out, g_last_raw, g_last_raw_len);
    }
    if (out_len) *out_len = g_last_raw_len;
}

/* ---------- weak 桩实现 ---------- */

/* GCC/Clang/MinGW 通用：__attribute__((weak))。
 * 链接 lib_at_engine 时，对应强符号会覆盖这些 weak。 */
#if defined(__GNUC__) || defined(__clang__)
#define MOCK_WEAK __attribute__((weak))
#else
#define MOCK_WEAK
#endif

/**
 * @brief mock at_session_send：按 call seq 模拟响应。
 *
 * 重要：state 推进在 diag_sms.c 内部，mock 不知道"这是第几条 send"——
 * 只能按 cmd 字符串 + 模式来判定行为。
 */
MOCK_WEAK int at_session_send(at_session_t *s, const char *cmd, int timeout_ms,
                              at_response_cb cb, void *userdata)
{
    (void)s; (void)timeout_ms;
    g_send_count++;
    if (cmd) {
        strncpy(g_last_cmd, cmd, sizeof(g_last_cmd) - 1);
        g_last_cmd[sizeof(g_last_cmd) - 1] = '\0';
    }
    if (g_mode == MOCK_MODE_ERROR) {
        if (cb) cb(userdata, "ERROR", 5, false);
        return 0;
    }
    /* MOCK_MODE_SIMPLE：CMGF 立即成功；CMGS 不 cb（等 URC） */
    if (cmd && strncmp(cmd, "AT+CMGF", 7) == 0) {
        if (cb) cb(userdata, "OK", 2, true);
    }
    /* CMGS / 其他：不调 cb，让 diag_sms 走 URC 路径 */
    return 0;
}

/**
 * @brief mock at_session_send_raw：模拟发 text+0x1A 后收 +CMGS: 1 + OK。
 */
MOCK_WEAK int at_session_send_raw(at_session_t *s, const uint8_t *buf, size_t len,
                                  int timeout_ms, at_response_cb cb, void *userdata)
{
    (void)s; (void)timeout_ms;
    g_send_raw_count++;
    if (buf && len > 0 && len <= sizeof(g_last_raw)) {
        memcpy(g_last_raw, buf, len);
        g_last_raw_len = len;
    } else {
        g_last_raw_len = 0;
    }
    if (cb) cb(userdata, "+CMGS: 1\r\nOK", 12, true);
    return 0;
}

/**
 * @brief mock at_session_register_urc：按 prefix 模拟 URC 触发。
 *
 * 关键：diag_sms 注册的是 ">" 前缀。我们立即同步调 urc_cb("> ", 2)——
 * 因为测试时 diag_sms 在调 at_session_register_urc 之后才调
 * at_session_send(CMGS)，CMGS 是 in_flight 中；URC 同步触发后
 * diag_sms 走 send_raw，send_raw 走我们另一个 mock。
 *
 * 注意：at_session_register_urc 本身要求 s 非 NULL，但 diag_sms 已经
 * 传了一个 dummy at_session_t——我们的 mock 永不真正解引用 s，安全。
 */
MOCK_WEAK int at_session_register_urc(at_session_t *s, const char *prefix,
                                      at_urc_cb cb, void *userdata)
{
    (void)s;
    g_register_urc_count++;
    if (g_mode == MOCK_MODE_URC_NEVER) {
        return 0;  /* 不触发 */
    }
    if (cb && prefix && prefix[0] == '>') {
        cb(userdata, "> ", 2);
    }
    return 0;
}
