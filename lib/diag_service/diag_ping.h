/**
 * @file diag_ping.h
 * @brief TCP/UDP 网络探活（不依赖 NCM 物理通道；用 libuv 走本机 socket）。
 *
 * 设计：
 *   - TCP：uv_tcp_t + uv_connect + 5s timer；CONNECT 成功即"通"
 *   - UDP：uv_udp_t + uv_udp_send + 等待 recv 或 5s 超时
 *   - 不解析响应内容，只看是否到达（TCP = 三次握手成功；UDP = 对端回包）
 */
#ifndef LIB_DIAG_PING_H
#define LIB_DIAG_PING_H

#include <stdbool.h>
#include <stddef.h>

struct uv_loop_s;
typedef struct uv_loop_s uv_loop_t;

/* ping 完成回调：ok=true 表示网络通；ms 是 RTT 毫秒（TCP=连接耗时；UDP=回包耗时） */
typedef void (*diag_ping_cb)(bool ok, int ms, void *userdata);

/* TCP ping。timeout_ms <= 0 用默认 5000。host 是 "1.2.3.4" 或域名（libuv getaddrinfo）。
 * 返回 0 = 已开始；负数 = 错误（loop=NULL/host 空等）。 */
int  diag_ping_tcp (uv_loop_t *loop, const char *host, int port,
                    int timeout_ms, diag_ping_cb cb, void *userdata);

/* UDP ping。发一个 1 字节 payload 等回包；timeout 内没收到就 false。 */
int  diag_ping_udp (uv_loop_t *loop, const char *host, int port,
                    int timeout_ms, diag_ping_cb cb, void *userdata);

#endif
