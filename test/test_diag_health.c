/**
 * @file test_diag_health.c
 * @brief 一键健康检查单元测试（TEST_MODE）。
 *
 * TEST_MODE 下 diag_health_check 走假数据路径，5s 内同步返回：
 *   - loop / dm 传 NULL 也不崩
 *   - 报告写 out_json_path，路径有效
 *   - JSON 包含 ts / dev_idx / ping / ssl / log / errors 字段
 *
 * 注意：本文件不允许用 assert()，统一用 CHECK(cond, msg)（P2 Task 4 教训）。
 */
#include "diag_health.h"
#include "agent_types.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int g_checks = 0, g_failed = 0;
#define CHECK(c, m) do { g_checks++; if (!(c)) { g_failed++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, m); } } while (0)

static int  g_done = 0;
static int  g_ok   = 0;
static char g_path[512];

static void on_done(bool ok, const char *path, void *ud)
{
    (void)ud;
    g_done = 1;
    g_ok   = ok;
    if (path) {
        strncpy(g_path, path, sizeof(g_path) - 1);
        g_path[sizeof(g_path) - 1] = '\0';
    }
}

/* 读整个文件到 buf（简单读，限长 8KB） */
static int read_file(const char *path, char *buf, size_t bufsz)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    size_t n = fread(buf, 1, bufsz - 1, f);
    buf[n] = '\0';
    fclose(f);
    return (int)n;
}

int main(void)
{
    /* 1) 同步路径：dm=NULL, loop=NULL（TEST_MODE 允许） */
    int rc = diag_health_check(NULL, NULL, 0,
                               "out/test_health.json",
                               on_done, NULL);
    CHECK(rc == AGENT_OK, "diag_health_check 返回 AGENT_OK");
    CHECK(g_done == 1,   "TEST_MODE 同步触发 cb");
    CHECK(g_ok == 1,     "TEST_MODE 报告写成功 → cb(true)");

    /* 2) 验证 JSON 路径 */
    CHECK(g_path[0] != '\0', "json path 非空");
    CHECK(strcmp(g_path, "out/test_health.json") == 0, "json path 一致");

    /* 3) 读 JSON 验证 schema 字段 */
    char buf[8192] = {0};
    int n = read_file(g_path, buf, sizeof(buf));
    CHECK(n > 0, "JSON 报告可读且非空");

    /* 必备字段：ts, dev_idx, dev_label, ping, ssl, log, errors */
    CHECK(strstr(buf, "\"ts\"")           != NULL, "包含 ts 字段");
    CHECK(strstr(buf, "\"dev_idx\"")      != NULL, "包含 dev_idx 字段");
    CHECK(strstr(buf, "\"dev_label\"")    != NULL, "包含 dev_label 字段");
    CHECK(strstr(buf, "\"ping\"")         != NULL, "包含 ping 子对象");
    CHECK(strstr(buf, "\"ssl\"")          != NULL, "包含 ssl 子对象");
    CHECK(strstr(buf, "\"log\"")          != NULL, "包含 log 子对象");
    CHECK(strstr(buf, "\"errors\"")       != NULL, "包含 errors 字段");

    /* AT 字段（v1.0 简化版：st==NULL 给空串，仍要写 key） */
    CHECK(strstr(buf, "\"csq\"")          != NULL, "包含 csq 字段");
    CHECK(strstr(buf, "\"cereg\"")        != NULL, "包含 cereg 字段");
    CHECK(strstr(buf, "\"cop_operator\"") != NULL, "包含 cop_operator 字段");
    CHECK(strstr(buf, "\"imei\"")         != NULL, "包含 imei 字段");
    CHECK(strstr(buf, "\"imsi\"")         != NULL, "包含 imsi 字段");
    CHECK(strstr(buf, "\"iccid\"")        != NULL, "包含 iccid 字段");
    CHECK(strstr(buf, "\"rat\"")          != NULL, "包含 rat 字段");

    /* ping 子对象内部字段 */
    CHECK(strstr(buf, "\"host\"") != NULL, "ping.host 存在");
    CHECK(strstr(buf, "\"port\"") != NULL, "ping.port 存在");
    CHECK(strstr(buf, "\"ok\"")   != NULL, "ping.ok 存在");
    CHECK(strstr(buf, "\"ms\"")   != NULL, "ping.ms 存在");

    /* ssl 子对象内部字段 */
    CHECK(strstr(buf, "\"status\"") != NULL, "ssl.status 存在");
    CHECK(strstr(buf, "\"issuer\"") != NULL, "ssl.issuer 存在");
    CHECK(strstr(buf, "\"expiry\"") != NULL, "ssl.expiry 存在");

    /* log 子对象内部字段 */
    CHECK(strstr(buf, "\"path\"") != NULL, "log.path 存在");
    CHECK(strstr(buf, "\"size\"") != NULL, "log.size 存在");

    /* 4) 坏参数：out_json_path=NULL → BAD_ARG */
    rc = diag_health_check(NULL, NULL, 0, NULL, on_done, NULL);
    CHECK(rc == AGENT_ERR_BAD_ARG, "out_json_path=NULL 返回 BAD_ARG");

    /* 5) 坏参数：cb=NULL → BAD_ARG */
    rc = diag_health_check(NULL, NULL, 0, "out/x.json", NULL, NULL);
    CHECK(rc == AGENT_ERR_BAD_ARG, "cb=NULL 返回 BAD_ARG");

    /* 清理 */
    remove(g_path);

    printf("test_diag_health: %d/%d pass\n", g_checks - g_failed, g_checks);
    return g_failed == 0 ? 0 : 1;
}
