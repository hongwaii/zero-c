/**
 * @file diag_log.h
 * @brief 模组 log 抓取：发 AT 命令启 dump → 等 N 秒 → 写文件 → miniz 压成 zip。
 *
 * 输出 zip 内含 1 个文件 "modem.log"（文本）。报告路径返回给 caller。
 *
 * 设计：
 *   - 在 at_session 绑定的 loop 上挂 uv_timer（sec 秒后触发 finish）
 *   - 挂 at_session 的 rx hook，把模组原始字节流落到 logs/modem_<ts>.log
 *   - 收到 timer 到点 → 关 fp → 调 miniz 把 tmp log 压成 zip
 *   - cb 在 main loop 上触发，caller 读 zip_path / zip_size 即可
 *
 * 约束（at_session 当前限制）：
 *   - at_session_install_rx_hook 是单 hook 覆盖模式——挂 hook 期间
 *     at_session 自身的 cmd 收发停摆（这是设计意图：抓 log 时不该有
 *     AT 干扰）
 *   - sec 上限由 caller 控制（建议 <= 120s；过长 zip 可能巨大）
 */
#ifndef LIB_DIAG_LOG_H
#define LIB_DIAG_LOG_H

#include <stddef.h>
#include <stdbool.h>

struct at_session;
typedef struct at_session at_session_t;

/**
 * @brief 抓 log 完成回调（main loop 上触发）。
 * @param ok         true = zip 文件已写且非空
 * @param zip_path   最终 zip 绝对路径（caller 提供的 out_zip_path）
 * @param zip_size   zip 文件字节数；ok=false 时为 0
 * @param userdata   注册时传入
 */
typedef void (*diag_log_cb)(bool ok, const char *zip_path, size_t zip_size,
                            void *userdata);

/**
 * @brief 启动一次 log 抓取。
 *
 * @param s             at_session（需是已 open 状态）
 * @param sec           抓取时长（秒），必须 > 0
 * @param out_zip_path  输出 zip 路径（caller 保证父目录存在）
 * @param cb            完成回调（必须非 NULL）
 * @param userdata      透传给 cb
 * @return AGENT_OK / AGENT_ERR_BAD_ARG / AGENT_ERR_OOM / AGENT_ERR_IO
 *
 * 流程：
 *   1) 打开 <out_zip_path 换 .log 后缀> 当 tmp 落盘
 *   2) 挂 at_session rx hook，把每个 rx 字节写 tmp
 *   3) 启 uv_timer(sec * 1000 ms)
 *   4) timer 到点 → 关 fp → 删 hook → miniz 压 zip → 删 tmp → 调 cb
 */
int  diag_capture_log(at_session_t *s, int sec, const char *out_zip_path,
                      diag_log_cb cb, void *userdata);

#endif /* LIB_DIAG_LOG_H */
