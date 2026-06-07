/**
 * @file llm_chat_writer.h
 * @brief llm_chat 表写入：LLM 对话消息（user / assistant / tool）→ SQLite 持久化。
 *
 * 字段：ts / provider / model / role / content / tool_calls（tool_calls 是 JSON 字符串）。
 * 写入策略：同步小流量（每次 LLM 角色切换一条；UI 主循环中调，量小）。
 * stub 模式：storage_get_db() 返 NULL → 直接返 AGENT_ERR_IO。
 *
 * v1.0 简化：不提供 recent() 读 API——v1.0 仅写库不展示，留给 v1.1 UI 接入。
 */
#ifndef LIB_STORAGE_LLM_CHAT_WRITER_H
#define LIB_STORAGE_LLM_CHAT_WRITER_H

#include "agent_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化（占位：v1.0 同步写，不申请资源）。
 * @return AGENT_OK
 */
int  llm_chat_writer_init(void);

/**
 * @brief 关闭（占位）。
 */
void llm_chat_writer_close(void);

/**
 * @brief 写入一条 LLM 对话消息。
 * @param provider          提供商标识（"DeepSeek" / "OpenAI" / ...）
 * @param model             模型名（"deepseek-chat" / "gpt-4o" / ...）
 * @param role              角色（"user" / "assistant" / "tool"）
 * @param content           消息正文（user 输入 / assistant 回复 / tool 错误）
 * @param tool_calls_json   tool_call JSON 字符串（assistant 角色时填；user/tool 可传 NULL）
 * @return AGENT_OK 成功；AGENT_ERR_IO DB 未初始化或写失败
 *
 * 字段为空时存 ""（不存 NULL），便于 v1.1 UI 渲染时直接读取。
 * 写库失败时不重试（高频重试由调用方决定；本接口是 best-effort 写）。
 */
int  llm_chat_writer_add(const char *provider, const char *model,
                         const char *role,    const char *content,
                         const char *tool_calls_json);

#ifdef __cplusplus
}
#endif

#endif /* LIB_STORAGE_LLM_CHAT_WRITER_H */
