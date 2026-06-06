/**
 * @file test_i18n.c
 * @brief i18n_get 单元测试（TDD 运行时检查版）。
 */
#include "i18n.h"
#include <cJSON.h>
#include <stdio.h>
#include <string.h>

/* 运行时检查宏：失败计数 +1 并打印。 */
static int g_checks = 0;
static int g_failed = 0;
#define CHECK(cond, msg) do { \
    g_checks++; \
    if (!(cond)) { g_failed++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); } \
} while (0)

int main(void)
{
    /* 加载 zh-CN。测试从仓库根目录跑（build.bat test 在根目录调 cmake）。 */
    CHECK(i18n_init(AGENT_LANG_ZH_CN) == 0, "i18n_init 成功");

    const char *s = i18n_get("nav.diag");
    CHECK(s != NULL, "i18n_get(nav.diag) 非空");
    CHECK(s && strcmp(s, "现场诊断") == 0, "nav.diag == 现场诊断");

    s = i18n_get("llm.drawer.disclaimer");
    CHECK(s && strstr(s, "AT 指令") != NULL, "drawer.disclaimer 含 AT 指令");

    /* 找不到的 key 返回 key 本身（不返回 NULL）。 */
    const char *missing = i18n_get("missing.key");
    CHECK(missing != NULL, "missing key 非空");
    CHECK(strcmp(missing, "missing.key") == 0, "missing key 返回原 key");

    i18n_shutdown();
    printf("test_i18n: %d/%d pass\n", g_checks - g_failed, g_checks);
    return g_failed == 0 ? 0 : 1;
}
