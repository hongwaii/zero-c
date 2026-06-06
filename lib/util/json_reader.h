/**
 * @file json_reader.h
 * @brief cJSON 读取封装：加载文件 + 按 key 取值。
 */
#ifndef UTIL_JSON_READER_H
#define UTIL_JSON_READER_H

#include "agent_types.h"  /* 引入 cJSON 前向声明 */
#include <stdbool.h>
#include <stddef.h>
struct cJSON;

/**
 * @brief 从文件加载 JSON 文本并解析为 cJSON 树。
 * @param path 文件路径。
 * @param out  成功时输出 cJSON 根节点，调用方负责 cJSON_Delete。
 * @return AGENT_OK 成功，AGENT_ERR_NOT_FOUND 文件不存在，
 *         AGENT_ERR_IO 解析失败或文件过大，AGENT_ERR_OOM 内存不足。
 */
int json_load_file(const char *path, struct cJSON **out);

/**
 * @brief 按 key 取字符串，找不到或类型不匹配时写入 def。
 * @param root    JSON 根节点。
 * @param key     键名（大小写敏感）。
 * @param def     缺省值（可为 NULL）。
 * @param out     输出缓冲区。
 * @param out_len 输出缓冲区长度（必须 > 0）。
 * @return AGENT_OK 成功，AGENT_ERR_NOT_FOUND 使用了缺省值。
 */
int json_get_string(struct cJSON *root, const char *key, const char *def, char *out, size_t out_len);

/**
 * @brief 按 key 取整数，找不到或类型不匹配时写入 def。
 * @return AGENT_OK 成功，AGENT_ERR_NOT_FOUND 使用了缺省值。
 */
int json_get_int(struct cJSON *root, const char *key, int def, int *out);

/**
 * @brief 按 key 取布尔值，找不到或类型不匹配时写入 def。
 * @return AGENT_OK 成功，AGENT_ERR_NOT_FOUND 使用了缺省值。
 */
int json_get_bool(struct cJSON *root, const char *key, bool def, bool *out);

/**
 * @brief 按 key 取数组节点，非数组返回 NULL。
 */
struct cJSON *json_get_array(struct cJSON *root, const char *key);

#endif /* UTIL_JSON_READER_H */
