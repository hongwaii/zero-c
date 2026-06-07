/**
 * @file llm_tool.c
 * @brief tool_call JSON 解析：从 OpenAI 兼容 chat completion 完整响应里
 *        抽第一条 tool_call 的 id / name / arguments。
 */
#include "llm_tool.h"
#include "agent_types.h"
#include <cJSON.h>
#include <string.h>

bool llm_extract_tool_call(const char *json, size_t len, llm_tool_call_t *out)
{
    if (!json || !out) return false;
    cJSON *root = cJSON_ParseWithLength(json, len);
    if (!root) return false;
    cJSON *choices = cJSON_GetObjectItemCaseSensitive(root, "choices");
    if (!cJSON_IsArray(choices) || cJSON_GetArraySize(choices) == 0) {
        cJSON_Delete(root); return false;
    }
    cJSON *message = cJSON_GetObjectItemCaseSensitive(
        cJSON_GetArrayItem(choices, 0), "message");
    cJSON *tools = cJSON_GetObjectItemCaseSensitive(message, "tool_calls");
    if (!cJSON_IsArray(tools) || cJSON_GetArraySize(tools) == 0) {
        cJSON_Delete(root); return false;
    }
    cJSON *first = cJSON_GetArrayItem(tools, 0);
    cJSON *id = cJSON_GetObjectItemCaseSensitive(first, "id");
    cJSON *func = cJSON_GetObjectItemCaseSensitive(first, "function");
    cJSON *name = cJSON_GetObjectItemCaseSensitive(func, "name");
    cJSON *args = cJSON_GetObjectItemCaseSensitive(func, "arguments");
    bool got = false;
    if (cJSON_IsString(id) && cJSON_IsString(name) && cJSON_IsString(args)) {
        strncpy(out->id, id->valuestring, sizeof(out->id) - 1);
        strncpy(out->name, name->valuestring, sizeof(out->name) - 1);
        strncpy(out->arguments, args->valuestring, sizeof(out->arguments) - 1);
        got = true;
    }
    cJSON_Delete(root);
    return got;
}
