/**
 * @file json_writer.c
 * @brief cJSON 原子写实现：先写 .tmp 再 rename，避免半截文件。
 */
#include "json_writer.h"
#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 临时文件后缀长度（含结尾的 NUL）。 */
#define TMP_SUFFIX_LEN 5  /* ".tmp" + '\0' */

/**
 * @brief 把 cJSON 树原子写入文件（写 .tmp 后 rename）。
 *
 * 流程：cJSON_Print -> 写 "path.tmp" -> rename 成 "path"。
 * 任一步失败都返回错误码，临时文件可能残留，调用方需自行清理。
 *
 * @return AGENT_OK 成功，AGENT_ERR_IO 写入或 rename 失败，AGENT_ERR_OOM 内存不足。
 */
int json_save_file_atomic(const char *path, struct cJSON *root)
{
    if (!path || !root) return AGENT_ERR_BAD_ARG;

    /* 序列化为字符串（cJSON_Print 返回带格式的多行 JSON）。 */
    char *s = cJSON_Print(root);
    if (!s) return AGENT_ERR_OOM;

    /* 拼接临时文件名："path" + ".tmp"。 */
    size_t path_len = strlen(path);
    char *tmp = (char *)malloc(path_len + TMP_SUFFIX_LEN);
    if (!tmp) { free(s); return AGENT_ERR_OOM; }
    memcpy(tmp, path, path_len);
    memcpy(tmp + path_len, ".tmp", TMP_SUFFIX_LEN);

    /* 写入临时文件。 */
    FILE *f = fopen(tmp, "wb");
    if (!f) { free(s); free(tmp); return AGENT_ERR_IO; }
    fputs(s, f);
    fclose(f);
    free(s);

    /* 原子替换：rename 在同一文件系统上是原子的。 */
    if (rename(tmp, path) != 0) {
        /* rename 失败时清理临时文件，避免残留。 */
        remove(tmp);
        free(tmp);
        return AGENT_ERR_IO;
    }
    free(tmp);
    return AGENT_OK;
}
