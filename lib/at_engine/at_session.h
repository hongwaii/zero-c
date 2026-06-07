/**
 * @file at_session.h
 * @brief AT 命令会话：命令队列 + 状态机 + URC 路由。
 *
 * 驱动模型：用户 at_session_send() 排队，serial_chan 收字节 → 调 on_rx →
 * 解析 → 推进状态机 → 触发命令完成回调 或 派发 URC。
 */
#ifndef LIB_AT_ENGINE_AT_SESSION_H
#define LIB_AT_ENGINE_AT_SESSION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct at_session;
typedef struct at_session at_session_t;

struct modem_chan;
struct uv_loop_s;
typedef struct uv_loop_s uv_loop_t;

/**
 * @brief 命令完成回调。
 * @param userdata 注册时传入
 * @param result    完整响应（final 之前累积的所有 data 行拼成多行字符串）
 *                 失败时为 NULL
 * @param result_len 字节数
 * @param ok        true=OK / false=ERROR
 */
typedef void (*at_response_cb)(void *userdata, const char *result, size_t result_len, bool ok);

/**
 * @brief URC 回调（按前缀匹配）。
 */
typedef void (*at_urc_cb)(void *userdata, const char *line, size_t len);

/**
 * @brief 在指定 loop 上创建 AT 会话，绑到某个 modem_chan。
 * @return 新会话句柄；失败返回 NULL。
 */
at_session_t *at_session_create(uv_loop_t *loop, struct modem_chan *chan);

/**
 * @brief 打开会话（启动收包，注册 chan 的 on_rx 回调）。
 * @return AGENT_OK 或负错误码。
 */
int at_session_open(at_session_t *s);

/**
 * @brief 关闭会话：取消未完成命令、注销 rx 回调。
 */
void at_session_close(at_session_t *s);

/**
 * @brief 排队一条 AT 命令。cb 在命令完成（OK / ERROR / 超时）时被调用。
 * @param timeout_ms 超时（毫秒）；<=0 表示不超时
 */
int at_session_send(at_session_t *s, const char *cmd, int timeout_ms,
                    at_response_cb cb, void *userdata);

/**
 * @brief 注册 URC 处理器（按 prefix 匹配：行首 = prefix 即派发）。
 * @return AGENT_OK 或负错误码。
 */
int at_session_register_urc(at_session_t *s, const char *prefix,
                            at_urc_cb cb, void *userdata);

#endif /* LIB_AT_ENGINE_AT_SESSION_H */
