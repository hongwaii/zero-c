/**
 * @file json_reader.c
 * @brief cJSON 读取实现。
 */
#include "json_reader.h"
#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 文件大小上限：4 MiB，防止恶意/损坏文件把进程内存吃光。 */
#define JSON_MAX_FILE_SIZE (4 * 1024 * 1024)

/**
 * @brief 从文件加载 JSON 文本并解析为 cJSON 树。
 * @param path 文件路径。
 * @param out  成功时输出 cJSON 根节点，调用方负责 cJSON_Delete。
 * @return AGENT_OK 成功，AGENT_ERR_NOT_FOUND 文件不存在，
 *         AGENT_ERR_IO 文件大小异常或解析失败，AGENT_ERR_OOM 内存不足。
 */
int json_load_file(const char *path, struct cJSON **out)
{
    if (!path || !out) return AGENT_ERR_BAD_ARG;

    FILE *f = fopen(path, "rb");
    if (!f) return AGENT_ERR_NOT_FOUND;

    /* 探测文件大小。 */
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return AGENT_ERR_IO; }
    long n = ftell(f);
    if (n < 0) { fclose(f); return AGENT_ERR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return AGENT_ERR_IO; }

    /* 拒绝空文件或超大文件。 */
    if (n == 0 || n > JSON_MAX_FILE_SIZE) { fclose(f); return AGENT_ERR_IO; }

    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return AGENT_ERR_OOM; }

    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    if (got != (size_t)n) { free(buf); return AGENT_ERR_IO; }
    buf[n] = '\0';

    struct cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) return AGENT_ERR_IO;

    *out = root;
    return AGENT_OK;
}

/**
 * @brief 按 key 取字符串，找不到或类型不匹配时写入 def。
 */
int json_get_string(struct cJSON *root, const char *key, const char *def,
                    char *out, size_t out_len)
{
    if (!root || !key || !out || out_len == 0) return AGENT_ERR_BAD_ARG;

    struct cJSON *v = cJSON_GetObjectItemCaseSensitive(root, key);
    if (cJSON_IsString(v) && v->valuestring) {
        strncpy(out, v->valuestring, out_len - 1);
        out[out_len - 1] = '\0';
        return AGENT_OK;
    }
    /* 缺省值写入：可能为 NULL 调用方表示不关心缺省内容。 */
    if (def) {
        strncpy(out, def, out_len - 1);
        out[out_len - 1] = '\0';
    }
    return AGENT_ERR_NOT_FOUND;
}

/** @brief 按 key 取整数。 */
int json_get_int(struct cJSON *root, const char *key, int def, int *out)
{
    if (!root || !key || !out) return AGENT_ERR_BAD_ARG;

    struct cJSON *v = cJSON_GetObjectItemCaseSensitive(root, key);
    if (cJSON_IsNumber(v)) {
        *out = v->valueint;
        return AGENT_OK;
    }
    *out = def;
    return AGENT_ERR_NOT_FOUND;
}

/** @brief 按 key 取布尔值。 */
int json_get_bool(struct cJSON *root, const char *key, bool def, bool *out)
{
    if (!root || !key || !out) return AGENT_ERR_BAD_ARG;

    struct cJSON *v = cJSON_GetObjectItemCaseSensitive(root, key);
    if (cJSON_IsBool(v)) {
        *out = cJSON_IsTrue(v);
        return AGENT_OK;
    }
    *out = def;
    return AGENT_ERR_NOT_FOUND;
}

/** @brief 按 key 取数组节点，非数组返回 NULL。 */
struct cJSON *json_get_array(struct cJSON *root, const char *key)
{
    if (!root || !key) return NULL;
    struct cJSON *v = cJSON_GetObjectItemCaseSensitive(root, key);
    return cJSON_IsArray(v) ? v : NULL;
}
