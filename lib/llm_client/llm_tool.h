/**
 * @file llm_tool.h
 * @brief 从完整 chat completion 响应里抽 tool_calls。
 *
 * OpenAI tool_calls 格式：
 *   "tool_calls": [
 *     {"id": "...", "type": "function",
 *      "function": {"name": "send_at", "arguments": "{\"cmd\":\"AT+CSQ\"}"}}
 *   ]
 *
 * v1.0：只解析 function 类型。最多 1 个 tool_call（多 tool 用第一个）。
 */
#ifndef LIB_LLM_TOOL_H
#define LIB_LLM_TOOL_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char id[64];          /* tool_call id */
    char name[64];        /* function name，如 "send_at" */
    char arguments[1024]; /* arguments JSON 字符串原文 */
} llm_tool_call_t;

/* 从完整 JSON 响应里抽第一条 tool_call。
 * 成功返回 true，out 填充；无 tool_call 或解析失败返回 false。 */
bool llm_extract_tool_call(const char *response_json, size_t json_len,
                           llm_tool_call_t *out);

#endif
