/**
 * @file test_json_roundtrip.c
 * @brief json_reader + json_writer 的单元测试（TDD 运行时检查版）。
 *
 * 注意：本测试不使用 assert()——release build（-DNDEBUG）下 assert
 * 会被优化为空，容易掩盖 heap-corruption 等严重问题。
 * 改用运行时 CHECK 宏，统计失败/通过数并以退出码反映。
 */
#include "json_reader.h"
#include "json_writer.h"
#include "agent_types.h"
#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 运行时检查宏：失败计数 +1 并打印，最后退出码反映是否有失败。 */
static int g_checks = 0;
static int g_failed = 0;
#define CHECK(cond, msg) do { \
    g_checks++; \
    if (!(cond)) { g_failed++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); } \
} while (0)

/* 测试夹具路径：放在 out/ 下，避免污染源码树。 */
#define ROUNDTRIP_PATH "out/test_roundtrip.json"

/**
 * @brief 测试 json_get_string 命中/缺省/未找到三条路径。
 */
static void test_get_string(void)
{
    cJSON *r = cJSON_CreateObject();
    cJSON_AddStringToObject(r, "name", "alice");

    char buf[64];
    CHECK(json_get_string(r, "name", "x", buf, sizeof(buf)) == AGENT_OK, "get name 返回 OK");
    CHECK(strcmp(buf, "alice") == 0, "name == alice");

    char def[64];
    CHECK(json_get_string(r, "missing", "fallback", def, sizeof(def)) == AGENT_ERR_NOT_FOUND, "missing 返回 NOT_FOUND");
    CHECK(strcmp(def, "fallback") == 0, "default == fallback");

    cJSON_Delete(r);
}

/**
 * @brief 测试写文件 -> 读回 -> 数据完整性的 roundtrip。
 */
static void test_atomic_write(void)
{
    const char *path = ROUNDTRIP_PATH;

    cJSON *r = cJSON_CreateObject();
    cJSON_AddNumberToObject(r, "n", 7);
    cJSON_AddStringToObject(r, "s", "ok");
    CHECK(json_save_file_atomic(path, r) == AGENT_OK, "save 返回 OK");
    cJSON_Delete(r);

    cJSON *r2 = NULL;
    CHECK(json_load_file(path, &r2) == AGENT_OK, "load 返回 OK");
    if (r2) {
        int n = 0;
        json_get_int(r2, "n", -1, &n);
        CHECK(n == 7, "n == 7");

        char sb[16];
        json_get_string(r2, "s", "", sb, sizeof(sb));
        CHECK(strcmp(sb, "ok") == 0, "s == ok");

        cJSON_Delete(r2);
    }
}

/**
 * @brief 测试 json_get_int / json_get_bool / json_get_array 边界。
 */
static void test_get_helpers(void)
{
    cJSON *r = cJSON_CreateObject();
    cJSON_AddNumberToObject(r, "port", 8080);
    cJSON_AddBoolToObject(r, "verbose", 1);
    cJSON *arr = cJSON_AddArrayToObject(r, "items");
    cJSON_AddItemToArray(arr, cJSON_CreateString("a"));
    cJSON_AddItemToArray(arr, cJSON_CreateString("b"));

    int port = 0;
    CHECK(json_get_int(r, "port", -1, &port) == AGENT_OK, "port 返回 OK");
    CHECK(port == 8080, "port == 8080");

    int missing = 999;
    CHECK(json_get_int(r, "absent", 999, &missing) == AGENT_ERR_NOT_FOUND, "缺省 port 返回 NOT_FOUND");
    CHECK(missing == 999, "缺省 port 值为 999");

    bool verbose = false;
    CHECK(json_get_bool(r, "verbose", false, &verbose) == AGENT_OK, "verbose 返回 OK");
    CHECK(verbose == true, "verbose == true");

    struct cJSON *items = json_get_array(r, "items");
    CHECK(items != NULL, "items 非 NULL");
    CHECK(cJSON_GetArraySize(items) == 2, "items 长度 == 2");

    struct cJSON *notarr = json_get_array(r, "port");
    CHECK(notarr == NULL, "非数组返回 NULL");

    cJSON_Delete(r);
}

int main(void)
{
    test_get_string();
    test_atomic_write();
    test_get_helpers();

    /* 幂等性回归测试：连续 3 次写到同一路径都必须成功。
     * 保护 Windows 上 rename() 因目标已存在而失败的 bug 不再回归。 */
    {
        const char *path = ROUNDTRIP_PATH;
        cJSON *r = cJSON_CreateObject();
        cJSON_AddNumberToObject(r, "iter", 1);
        CHECK(json_save_file_atomic(path, r) == AGENT_OK, "幂等：第 1 次 save 返回 OK");
        CHECK(json_save_file_atomic(path, r) == AGENT_OK, "幂等：第 2 次 save 返回 OK");
        CHECK(json_save_file_atomic(path, r) == AGENT_OK, "幂等：第 3 次 save 返回 OK");
        cJSON_Delete(r);
    }

    if (g_failed == 0) {
        printf("test_json_roundtrip: %d/%d pass\n", g_checks, g_checks);
        return 0;
    }
    printf("test_json_roundtrip: %d/%d pass\n", g_checks - g_failed, g_checks);
    return 1;
}
