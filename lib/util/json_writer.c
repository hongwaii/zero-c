/**
 * @file json_writer.c
 * @brief cJSON 原子写实现：先写 .tmp 再 rename，避免半截文件。
 *
 * Windows 注意事项：MSVC/MinGW 的 rename() 在目标文件已存在时返回 -1
 * （errno=EEXIST），与 POSIX 的覆盖语义不同。因此在 rename 之前必须先
 * remove 目标文件。代价是一段极短的非原子窗口（P1 阶段可接受，P6 写真实
 * 配置文件时建议改用 ReplaceFileW / MoveFileEx 等更严格方案）。
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
 * 流程：cJSON_Print -> 写 "path.tmp" -> remove 目标（Windows 需要） ->
 * rename 成 "path"。任一步失败都返回错误码，临时文件可能残留，调用方需
 * 自行清理。
 *
 * Windows 行为差异：MSVC/MinGW 的 rename() 在目标已存在时返回 -1，
 * 因此本函数先 remove 再 rename——存在极短的非原子窗口（P1 可接受）。
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

    /* Windows rename() 在目标已存在时失败；先 remove 目标保证 rename 成功。
     * 这牺牲了严格的原子性——调用方应避免并发访问同一路径。 */
    remove(path);
    if (rename(tmp, path) != 0) {
        /* rename 失败时清理临时文件，避免残留。 */
        remove(tmp);
        free(tmp);
        return AGENT_ERR_IO;
    }
    free(tmp);
    return AGENT_OK;
}
