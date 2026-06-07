/**
 * @file diag_sms.h
 * @brief SMS 模板发送：通过现有 at_session 发 AT+CMGS。
 *
 * 流程（文本模式）：
 *   1) AT+CMGF=1          ← 切文本模式
 *   2) AT+CMGS="<number>" ← 等待 "> " 提示（URC 走 _CMGS_PROMPT_）
 *   3) 发送 text + Ctrl-Z (0x1A) — 走 at_session_send_raw（不加 \r）
 *   4) 等待 OK / +CMS ERROR
 *
 * 状态机：
 *   IDLE -> WAIT_CMGF -> WAIT_PROMPT -> WAIT_DONE -> 完成
 */
#ifndef LIB_DIAG_SERVICE_DIAG_SMS_H
#define LIB_DIAG_SERVICE_DIAG_SMS_H

#include <stdbool.h>
#include <stddef.h>

struct at_session;
typedef struct at_session at_session_t;

/* SMS 发送完成回调：ok=true 表示模组回 OK；status 字段填 "OK" 或 "ERROR: ..." */
typedef void (*diag_sms_cb)(bool ok, const char *status, void *userdata);

/**
 * @brief 发一条文本短信。
 *
 * @param s           at_session 句柄（不能 NULL）
 * @param number      目标手机号（不能含双引号；ASCII；长度 < 32）
 * @param text        短信文本（建议 < 160 字符；本函数不主动 UCS2 转换）
 * @param timeout_ms  单步超时（毫秒）；<=0 走默认 3000
 * @param cb          完成回调
 * @param userdata    透传给 cb
 * @return AGENT_OK 或负错误码（AGENT_ERR_BAD_ARG / AGENT_ERR_OOM）。
 *
 * 异步流程：返回值 = 0 表示"已开始"，实际结果由 cb 给出。
 * 真机行为：
 *   - 第一步发 AT+CMGF=1，等 OK
 *   - 第二步发 AT+CMGS="<num>"，等 "> " URC
 *   - 第三步 send_raw(text + 0x1A)，等 +CMGS: <mr> + OK / +CMS ERROR
 *   - 任一失败 → cb(false, "ERROR: <reason>", ud)
 */
int  diag_send_sms(at_session_t *s, const char *number, const char *text,
                   int timeout_ms, diag_sms_cb cb, void *userdata);

#endif /* LIB_DIAG_SERVICE_DIAG_SMS_H */
