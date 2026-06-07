#include "diag_ping.h"
#include "agent_types.h"
#include <uv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <process.h>

#define _WINSOCKAPI_
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

/* 简单同步 mock server：accept 一个连接立刻 close，模拟"网络通" */
static unsigned __stdcall mock_server(void *arg)
{
    SOCKET listen = (SOCKET)(intptr_t)arg;
    SOCKET c = accept(listen, NULL, NULL);
    if (c != INVALID_SOCKET) closesocket(c);
    return 0;
}

static int find_free_port(void)
{
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK); a.sin_port = 0;
    bind(s, (struct sockaddr *)&a, sizeof(a));
    int len = sizeof(a);
    getsockname(s, (struct sockaddr *)&a, &len);
    int port = ntohs(a.sin_port);
    closesocket(s);  /* 释放；uv_tcp_bind/connect 会重 bind */
    return port;
}

typedef struct {
    uv_loop_t *loop;
    bool done;
    bool ok;
    int  ms;
} ping_result_t;

static void on_ping_done(bool ok, int ms, void *ud)
{
    ping_result_t *r = (ping_result_t *)ud;
    r->done = true; r->ok = ok; r->ms = ms;
    uv_stop(r->loop);
}

#if 0  /* 实现完 UDP 后取消注释 */
/* UDP echo server：本机 127.0.0.1 收 1 字节立刻回 1 字节 */
static unsigned __stdcall mock_udp_server(void *arg)
{
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    int port = (int)(intptr_t)arg;
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons(port);
    bind(s, (struct sockaddr *)&a, sizeof(a));
    char buf[16]; struct sockaddr_in from; int fromlen = sizeof(from);
    int n = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr *)&from, &fromlen);
    if (n > 0) sendto(s, buf, n, 0, (struct sockaddr *)&from, fromlen);
    closesocket(s);
    return 0;
}

static int find_free_udp_port(void)
{
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK); a.sin_port = 0;
    bind(s, (struct sockaddr *)&a, sizeof(a));
    int len = sizeof(a); getsockname(s, (struct sockaddr *)&a, &len);
    int port = ntohs(a.sin_port);
    closesocket(s);
    return port;
}
#endif

int main(void)
{
    WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa);
    uv_loop_t *loop = uv_default_loop();

    int port = find_free_port();

    /* 起 mock server，accept 一个连接 */
    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1; setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(yes));
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons(port);
    bind(listen_sock, (struct sockaddr *)&a, sizeof(a));
    listen(listen_sock, 1);
    _beginthreadex(NULL, 0, mock_server, (void *)(intptr_t)listen_sock, 0, NULL);

    /* 测 TCP ping */
    ping_result_t r = {0};
    r.loop = loop;
    int rc = diag_ping_tcp(loop, "127.0.0.1", port, 2000, on_ping_done, &r);
    assert(rc == AGENT_OK);
    uv_run(loop, UV_RUN_DEFAULT);
    assert(r.done);
    assert(r.ok == true);
    assert(r.ms >= 0 && r.ms < 2000);

    printf("test_diag_ping: all pass (TCP ok, %d ms)\n", r.ms);

#if 0  /* 实现完 UDP 后取消注释 */
    int udp_port = find_free_udp_port();
    _beginthreadex(NULL, 0, mock_udp_server, (void *)(intptr_t)udp_port, 0, NULL);
    Sleep(50);  /* 等 server 起来 */
    ping_result_t r2 = {0}; r2.loop = loop;
    rc = diag_ping_udp(loop, "127.0.0.1", udp_port, 2000, on_ping_done, &r2);
    assert(rc == AGENT_OK);
    uv_run(loop, UV_RUN_DEFAULT);
    assert(r2.done);
    assert(r2.ok == true);
    printf(" + UDP ok, %d ms\n", r2.ms);
#endif

    closesocket(listen_sock);
    WSACleanup();
    return 0;
}
