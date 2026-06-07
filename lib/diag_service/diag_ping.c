/**
 * @file diag_ping.c
 * @brief TCP/UDP ping 实现（uv_tcp_t / uv_udp_t + uv_timer_t 超时）
 */
#include "diag_ping.h"
#include "agent_types.h"

#include <uv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 平台相关：getaddrinfo / sockaddr / IPPROTO 头 */
#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <sys/socket.h>
#endif

/* TCP ping 上下文 */
typedef struct {
    uv_loop_t      *loop;
    uv_tcp_t        tcp;
    uv_timer_t      timer;
    diag_ping_cb    cb;
    void           *userdata;
    int64_t         start_ms;
    char            host[64];
    int             port;
    bool            finished;   /* 防止 cb 调多次（connect + timer 同时到）*/
} tcp_ping_ctx_t;

static void tcp_on_connect(uv_connect_t *req, int status)
{
    tcp_ping_ctx_t *ctx = (tcp_ping_ctx_t *)req->data;
    free(req);
    if (ctx->finished) return;
    ctx->finished = true;
    uv_timer_stop(&ctx->timer);
    int64_t ms = uv_now(ctx->loop) - ctx->start_ms;
    bool ok = (status == 0);
    if (!ok) fprintf(stderr, "diag_ping_tcp: connect %s:%d failed: %s\n",
                     ctx->host, ctx->port, uv_strerror(status));
    uv_close((uv_handle_t *)&ctx->tcp, NULL);
    uv_close((uv_handle_t *)&ctx->timer, NULL);
    diag_ping_cb cb = ctx->cb;
    void *ud = ctx->userdata;
    free(ctx);
    if (cb) cb(ok, (int)ms, ud);
}

static void tcp_on_timeout(uv_timer_t *t)
{
    tcp_ping_ctx_t *ctx = (tcp_ping_ctx_t *)t->data;
    if (ctx->finished) return;
    ctx->finished = true;
    fprintf(stderr, "diag_ping_tcp: %s:%d timeout\n", ctx->host, ctx->port);
    /* uv_cancel 在 Windows 不一定有效；用 uv_close 让 callback 走 ERROR 路径 */
    uv_close((uv_handle_t *)&ctx->tcp, NULL);
    uv_close((uv_handle_t *)&ctx->timer, NULL);
    diag_ping_cb cb = ctx->cb;
    void *ud = ctx->userdata;
    int64_t ms = uv_now(ctx->loop) - ctx->start_ms;
    free(ctx);
    if (cb) cb(false, (int)ms, ud);
}

int diag_ping_tcp(uv_loop_t *loop, const char *host, int port,
                  int timeout_ms, diag_ping_cb cb, void *userdata)
{
    if (!loop || !host || port <= 0 || port > 65535) return AGENT_ERR_BAD_ARG;
    if (timeout_ms <= 0) timeout_ms = 5000;

    tcp_ping_ctx_t *ctx = (tcp_ping_ctx_t *)calloc(1, sizeof(*ctx));
    if (!ctx) return AGENT_ERR_OOM;
    ctx->loop = loop;
    ctx->cb = cb;
    ctx->userdata = userdata;
    ctx->port = port;
    strncpy(ctx->host, host, sizeof(ctx->host) - 1);
    ctx->start_ms = uv_now(loop);

    int r = uv_tcp_init(loop, &ctx->tcp);
    if (r != 0) { free(ctx); return AGENT_ERR_IO; }
    ctx->tcp.data = ctx;

    r = uv_timer_init(loop, &ctx->timer);
    if (r != 0) { uv_close((uv_handle_t *)&ctx->tcp, NULL); free(ctx); return AGENT_ERR_IO; }
    ctx->timer.data = ctx;
    uv_timer_start(&ctx->timer, tcp_on_timeout, (uint64_t)timeout_ms, 0);

    /* 异步解析 + connect */
    struct addrinfo hints = {0};
    hints.ai_family = AF_INET;       /* v1.0 只 IPv4 简化 */
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *res = NULL;
    char port_str[16]; snprintf(port_str, sizeof(port_str), "%d", port);
    int rc = getaddrinfo(host, port_str, &hints, &res);
    if (rc != 0 || !res) {
        fprintf(stderr, "diag_ping_tcp: getaddrinfo(%s) failed: %d\n", host, rc);
        uv_close((uv_handle_t *)&ctx->tcp, NULL);
        uv_close((uv_handle_t *)&ctx->timer, NULL);
        free(ctx);
        return AGENT_ERR_NOT_FOUND;
    }

    uv_connect_t *creq = (uv_connect_t *)malloc(sizeof(uv_connect_t));
    if (!creq) {
        freeaddrinfo(res);
        uv_close((uv_handle_t *)&ctx->tcp, NULL);
        uv_close((uv_handle_t *)&ctx->timer, NULL);
        free(ctx);
        return AGENT_ERR_OOM;
    }
    creq->data = ctx;
    r = uv_tcp_connect(creq, &ctx->tcp, (const struct sockaddr *)res->ai_addr, tcp_on_connect);
    freeaddrinfo(res);
    if (r != 0) {
        fprintf(stderr, "diag_ping_tcp: uv_tcp_connect: %s\n", uv_strerror(r));
        free(creq);
        uv_close((uv_handle_t *)&ctx->tcp, NULL);
        uv_close((uv_handle_t *)&ctx->timer, NULL);
        free(ctx);
        return AGENT_ERR_IO;
    }
    return AGENT_OK;
}

/* UDP ping 上下文 */
typedef struct {
    uv_loop_t      *loop;
    uv_udp_t        udp;
    uv_timer_t      timer;
    diag_ping_cb    cb;
    void           *userdata;
    int64_t         start_ms;
    char            host[64];
    int             port;
    bool            finished;
} udp_ping_ctx_t;

/* 收到对端回包：成功 */
static void udp_on_recv(uv_udp_t *h, ssize_t nread, const uv_buf_t *buf,
                        const struct sockaddr *addr, unsigned flags)
{
    udp_ping_ctx_t *ctx = (udp_ping_ctx_t *)h->data;
    (void)addr; (void)flags;
    if (buf && buf->base) free(buf->base);  /* 释放 alloc 出来的缓冲 */
    if (ctx->finished) return;
    if (nread > 0) {
        ctx->finished = true;
        uv_timer_stop(&ctx->timer);
        uv_udp_recv_stop(&ctx->udp);
        int64_t ms = uv_now(ctx->loop) - ctx->start_ms;
        uv_close((uv_handle_t *)&ctx->udp, NULL);
        uv_close((uv_handle_t *)&ctx->timer, NULL);
        diag_ping_cb cb = ctx->cb; void *ud = ctx->userdata;
        free(ctx);
        if (cb) cb(true, (int)ms, ud);
    }
    /* nread < 0 是错误或 EAGAIN，忽略（等 timer 超时统一 fail） */
}

/* 超时：失败 */
static void udp_on_timeout(uv_timer_t *t)
{
    udp_ping_ctx_t *ctx = (udp_ping_ctx_t *)t->data;
    if (ctx->finished) return;
    ctx->finished = true;
    fprintf(stderr, "diag_ping_udp: %s:%d timeout\n", ctx->host, ctx->port);
    uv_udp_recv_stop(&ctx->udp);
    uv_close((uv_handle_t *)&ctx->udp, NULL);
    uv_close((uv_handle_t *)&ctx->timer, NULL);
    diag_ping_cb cb = ctx->cb; void *ud = ctx->userdata;
    int64_t ms = uv_now(ctx->loop) - ctx->start_ms;
    free(ctx);
    if (cb) cb(false, (int)ms, ud);
}

/* libuv alloc 回调：分配接收缓冲 */
static void udp_on_alloc(uv_handle_t *h, size_t suggested, uv_buf_t *buf)
{
    (void)h;
    size_t sz = suggested > 0 ? suggested : 64;
    buf->base = (char *)malloc(sz);
    buf->len  = buf->base ? sz : 0;
}

/* send 完成回调：若失败立即 fail */
static void udp_on_send(uv_udp_send_t *req, int status)
{
    udp_ping_ctx_t *ctx = (udp_ping_ctx_t *)req->data;
    free(req);
    if (ctx->finished) return;
    if (status != 0) {
        /* send 失败 → 立即 fail */
        ctx->finished = true;
        fprintf(stderr, "diag_ping_udp: send failed: %s\n", uv_strerror(status));
        uv_timer_stop(&ctx->timer);
        uv_udp_recv_stop(&ctx->udp);
        uv_close((uv_handle_t *)&ctx->udp, NULL);
        uv_close((uv_handle_t *)&ctx->timer, NULL);
        diag_ping_cb cb = ctx->cb; void *ud = ctx->userdata;
        int64_t ms = uv_now(ctx->loop) - ctx->start_ms;
        free(ctx);
        if (cb) cb(false, (int)ms, ud);
    }
    /* else：等 udp_on_recv 收对端回包 */
}

int diag_ping_udp(uv_loop_t *loop, const char *host, int port,
                  int timeout_ms, diag_ping_cb cb, void *userdata)
{
    if (!loop || !host || port <= 0 || port > 65535) return AGENT_ERR_BAD_ARG;
    if (timeout_ms <= 0) timeout_ms = 5000;

    udp_ping_ctx_t *ctx = (udp_ping_ctx_t *)calloc(1, sizeof(*ctx));
    if (!ctx) return AGENT_ERR_OOM;
    ctx->loop = loop; ctx->cb = cb; ctx->userdata = userdata; ctx->port = port;
    strncpy(ctx->host, host, sizeof(ctx->host) - 1);
    ctx->start_ms = uv_now(loop);

    int r = uv_udp_init(loop, &ctx->udp);
    if (r != 0) { free(ctx); return AGENT_ERR_IO; }
    ctx->udp.data = ctx;

    r = uv_timer_init(loop, &ctx->timer);
    if (r != 0) { uv_close((uv_handle_t *)&ctx->udp, NULL); free(ctx); return AGENT_ERR_IO; }
    ctx->timer.data = ctx;
    uv_timer_start(&ctx->timer, udp_on_timeout, (uint64_t)timeout_ms, 0);

    /* 先启 recv（对端回包才能进 udp_on_recv）*/
    r = uv_udp_recv_start(&ctx->udp, udp_on_alloc, udp_on_recv);
    if (r != 0) {
        uv_close((uv_handle_t *)&ctx->udp, NULL);
        uv_close((uv_handle_t *)&ctx->timer, NULL);
        free(ctx);
        return AGENT_ERR_IO;
    }

    /* 解析远端地址 */
    struct addrinfo hints = {0};
    hints.ai_family = AF_INET; hints.ai_socktype = SOCK_DGRAM;
    struct addrinfo *res = NULL;
    char port_str[16]; snprintf(port_str, sizeof(port_str), "%d", port);
    if (getaddrinfo(host, port_str, &hints, &res) != 0 || !res) {
        fprintf(stderr, "diag_ping_udp: getaddrinfo(%s) failed\n", host);
        uv_udp_recv_stop(&ctx->udp);
        uv_close((uv_handle_t *)&ctx->udp, NULL);
        uv_close((uv_handle_t *)&ctx->timer, NULL);
        free(ctx);
        return AGENT_ERR_NOT_FOUND;
    }

    uv_udp_send_t *sreq = (uv_udp_send_t *)malloc(sizeof(uv_udp_send_t));
    if (!sreq) {
        freeaddrinfo(res);
        uv_udp_recv_stop(&ctx->udp);
        uv_close((uv_handle_t *)&ctx->udp, NULL);
        uv_close((uv_handle_t *)&ctx->timer, NULL);
        free(ctx);
        return AGENT_ERR_OOM;
    }
    sreq->data = ctx;
    char payload = 'P';
    uv_buf_t buf = uv_buf_init(&payload, 1);
    r = uv_udp_send(sreq, &ctx->udp, &buf, 1, (const struct sockaddr *)res->ai_addr, udp_on_send);
    freeaddrinfo(res);
    if (r != 0) {
        fprintf(stderr, "diag_ping_udp: uv_udp_send: %s\n", uv_strerror(r));
        free(sreq);
        uv_udp_recv_stop(&ctx->udp);
        uv_close((uv_handle_t *)&ctx->udp, NULL);
        uv_close((uv_handle_t *)&ctx->timer, NULL);
        free(ctx);
        return AGENT_ERR_IO;
    }
    return AGENT_OK;
}
