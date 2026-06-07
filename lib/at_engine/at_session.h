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

/* ---------- P4 扩展 API（diag_service 等需要绕过正常 AT 命令路径） ---------- */

/**
 * @brief 取得会话绑定的 libuv loop（diag_service 创建 timer 用）。
 * @return 内部 loop 指针；s 为 NULL 时返回 NULL。
 */
uv_loop_t *at_session_loop(at_session_t *s);

/**
 * @brief 取得会话绑定的 modem_chan（diag_service 调试/旁路用）。
 * @return 内部 chan 指针；s 或内部 chan 为 NULL 时返回 NULL。
 */
struct modem_chan *at_session_chan(at_session_t *s);

/**
 * @brief 直接发原始字节（不自动加 \r）——SMS Ctrl-Z 0x1A / 二进制 PDU 用。
 *
 * 实现要点：
 *   - 走 modem_chan_send 直接发，不进 cmd 队列（"占位 AT" 仍然需要，但纯 raw
 *     发 ctrl-z 不该被 snprintf+"\r" 加尾巴）
 *   - 用 uv_timer 做超时；到时调 complete_current(s, false) 走正常错误结束路径
 *   - 期间不允许并发其他 AT 命令（at_session 单 in_flight 已有约束）；
 *     若 s->in_flight 已为 true，返回 AGENT_ERR_BAD_ARG
 *   - 收包时 FINAL_OK/FINAL_ERROR 走同样的 complete_current 路径；
 *     data 行仍累计到 result buffer
 *
 * @param buf        原始字节（不会被改写）
 * @param len        字节数
 * @param timeout_ms 超时（毫秒）；<=0 走默认 3000ms
 * @param cb         完成回调
 * @param userdata   透传给 cb
 * @return AGENT_OK 或负错误码（AGENT_ERR_BAD_ARG / AGENT_ERR_IO）。
 */
int at_session_send_raw(at_session_t *s, const uint8_t *buf, size_t len,
                        int timeout_ms, at_response_cb cb, void *userdata);

/**
 * @brief 在 chan 上挂一个外部 rx hook（覆盖默认的 at_session on_rx）。
 *
 * 用途：diag_log 需要拦截模组原始字节流到 log 文件，挂完 hook 后
 * at_session 的 cmd 路径不再收到 rx，cmd 收发完全停摆。
 *
 * 警告：当前实现是"单 hook 覆盖"——挂第二个 hook 会丢第一个。生产代码
 * 要用 hook 链表（v1.1 再说）。
 *
 * @param cb        rx 回调（与 modem_chan_rx_fn 同形）
 * @param userdata  透传给 cb
 */
void at_session_install_rx_hook(at_session_t *s,
                                void (*cb)(void *, const uint8_t *, size_t),
                                void *userdata);

#endif /* LIB_AT_ENGINE_AT_SESSION_H */
