/**
 * @file llm_client.h
 * @brief OpenAI 兼容 chat completion 客户端：libcurl POST + SSE 流式回包 + 逐 token 回调。
 *
 * 线程模型：llm_chat_stream 在调用方线程内启动 worker 线程（_beginthreadex），
 *          worker 用 libcurl 同步 POST。SSE 解析后通过 on_token_cb 同步调
 *          用户回调（v1.0 简化：不通过 libuv async——worker 线程直接调）。
 *          v1.1 改进：worker 内部写 ringbuf，main loop 拉。
 */
#ifndef LIB_LLM_CLIENT_H
#define LIB_LLM_CLIENT_H

#include <stdbool.h>
#include <stddef.h>

#include "agent_types.h"  /* agent_llm_provider_t */

#ifdef __cplusplus
extern "C" {
#endif

/* 单个 token 流式回调。v1.0：worker 线程直接调。 */
typedef void (*llm_token_cb)(const char *token, size_t len, void *userdata);

/* 完成回调。ok=true 表示正常收尾，err=NULL 或空。 */
typedef void (*llm_done_cb)(bool ok, const char *err, void *userdata);

/* 发起一个 chat completion 请求：
 *   - provider: 已加载的 provider（含 base_url / api_key 明文）
 *   - model: 模型名（NULL = 用 provider->default_model）
 *   - messages_json: OpenAI 格式 messages 数组的 JSON 字符串（已序列化好）
 *   - on_token: 每收到一个 token 调一次（多次）
 *   - on_done: 流结束或错误时调一次
 *   - userdata: 两个回调的 userdata
 * 返回 0=已启动；负=参数错/资源失败。
 *
 * 注意：api_key 是 DPAPI 解密后的明文（由调用方负责）。 */
int  llm_chat_stream(const agent_llm_provider_t *provider,
                     const char *model,
                     const char *messages_json,
                     llm_token_cb on_token,
                     llm_done_cb  on_done,
                     void        *userdata);

#ifdef __cplusplus
}
#endif

#endif
