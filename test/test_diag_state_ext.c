/**
 * @file test_diag_state_ext.c
 * @brief diag_state P4 扩展字段单元测试。
 *
 * 验证 P4 新增的 ping/ssl/sms/log/health 字段：
 *   - reset 之后默认值符合约定（-1 / 0 / 空串 / false）
 *   - reset 之后再写一次能再次清空（idempotent）
 *
 * 注意：本文件不允许用 assert()，统一用 CHECK(cond, msg)（P2 Task 4 教训）。
 */
#include "diag_state.h"
#include <stdio.h>
#include <string.h>

static int g_checks = 0, g_failed = 0;
#define CHECK(c, m) do { g_checks++; if (!(c)) { g_failed++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, m); } } while (0)

int main(void)
{
    diag_state_t s;
    diag_state_reset(&s);

    /* 老字段：默认是空 + valid=false */
    CHECK(s.valid == false, "初始 valid == false");
    CHECK(s.csq[0] == '\0', "csq 空");

    /* 新字段：ping */
    CHECK(s.last_ping_host[0] == '\0', "last_ping_host 空");
    CHECK(s.last_ping_port == 0, "last_ping_port == 0");
    CHECK(s.last_ping_ms == -1, "last_ping_ms == -1 (未测过)");
    CHECK(s.last_ping_ok == false, "last_ping_ok == false");

    /* 新字段：ssl */
    CHECK(s.last_ssl_url[0] == '\0', "last_ssl_url 空");
    CHECK(s.last_ssl_status == 0, "last_ssl_status == 0");
    CHECK(s.last_ssl_issuer[0] == '\0', "last_ssl_issuer 空");
    CHECK(s.last_ssl_expiry[0] == '\0', "last_ssl_expiry 空");

    /* 新字段：sms */
    CHECK(s.last_sms_number[0] == '\0', "last_sms_number 空");
    CHECK(s.last_sms_status[0] == '\0', "last_sms_status 空");

    /* 新字段：log */
    CHECK(s.last_log_path[0] == '\0', "last_log_path 空");
    CHECK(s.last_log_size == 0, "last_log_size == 0");

    /* 新字段：health */
    CHECK(s.last_health_report[0] == '\0', "last_health_report 空");

    /* reset 之后能再写 */
    strcpy(s.last_ping_host, "10.0.0.1");
    s.last_ping_ms = 23;
    s.last_ping_ok = true;
    diag_state_reset(&s);
    CHECK(s.last_ping_host[0] == '\0', "二次 reset 后 last_ping_host 清空");
    CHECK(s.last_ping_ms == -1, "二次 reset 后 last_ping_ms == -1");
    CHECK(s.last_ping_ok == false, "二次 reset 后 last_ping_ok == false");

    printf("test_diag_state_ext: %d/%d pass\n", g_checks - g_failed, g_checks);
    return g_failed == 0 ? 0 : 1;
}
