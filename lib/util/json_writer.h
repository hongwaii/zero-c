/**
 * @file json_writer.h
 * @brief cJSON 写入封装：原子写到文件。
 */
#ifndef UTIL_JSON_WRITER_H
#define UTIL_JSON_WRITER_H

#include "agent_types.h"
struct cJSON;

/**
 * @brief 把 cJSON 树原子写入文件（先写 .tmp，再 rename）。
 *
 * 原子写的意义：写入过程中断电/崩溃不会留下半截 JSON 文件。
 *
 * @param path 目标文件路径。
 * @param root 要写入的 cJSON 根节点。
 * @return AGENT_OK 成功，AGENT_ERR_IO 写入或 rename 失败，AGENT_ERR_OOM 内存不足。
 */
int json_save_file_atomic(const char *path, struct cJSON *root);

#endif /* UTIL_JSON_WRITER_H */
