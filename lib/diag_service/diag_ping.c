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

/* UDP ping — 留作 Task 4 实现 */
int diag_ping_udp(uv_loop_t *loop, const char *host, int port,
                  int timeout_ms, diag_ping_cb cb, void *userdata)
{
    (void)loop; (void)host; (void)port; (void)timeout_ms; (void)cb; (void)userdata;
    return AGENT_ERR_NOT_FOUND;  /* TODO Task 4 */
}
