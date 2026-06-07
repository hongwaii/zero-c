/**
 * @file test_diag_state.c
 * @brief diag_state 字段读写测试。
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
    CHECK(s.valid == false, "初始 valid == false");
    CHECK(strcmp(s.csq, "") == 0, "csq 空");
    CHECK(strcmp(s.last_update, "-") == 0, "last_update == '-'");
    /* 写几个字段再 reset 确认 idempotent */
    strncpy(s.csq, "23", sizeof(s.csq) - 1);
    s.valid = true;
    diag_state_reset(&s);
    CHECK(s.valid == false, "二次 reset 仍 valid == false");
    CHECK(strcmp(s.csq, "") == 0, "csq 清空");
    printf("test_diag_state: %d/%d pass\n", g_checks - g_failed, g_checks);
    return g_failed == 0 ? 0 : 1;
}
