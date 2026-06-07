/**
 * @file test_llm_client_mock.c
 * @brief P5 LLM 客户端集成测试：mock HTTP server（127.0.0.1）返回 3 段 SSE token + [DONE]，
 *        验证 llm_chat_stream 能正确拼 URL / 拼 body / 解 SSE / 累 token / 调 on_token。
 */
#include "llm_client.h"
#include "llm_sse.h"
#include "agent_types.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <process.h>

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

/* 找空闲端口 */
static int find_port(void)
{
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK); a.sin_port = 0;
    bind(s, (struct sockaddr *)&a, sizeof(a));
    int len = sizeof(a); getsockname(s, (struct sockaddr *)&a, &len);
    int port = ntohs(a.sin_port);
    closesocket(s);
    return port;
}

/* mock server：接收一个 POST，返 200 + 3 个 SSE data + [DONE] */
static unsigned __stdcall mock_llm(void *arg)
{
    int port = (int)(intptr_t)arg;
    SOCKET listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons(port);
    bind(listen_fd, (struct sockaddr *)&a, sizeof(a));
    listen(listen_fd, 1);
    SOCKET c = accept(listen_fd, NULL, NULL);
    if (c == INVALID_SOCKET) { closesocket(listen_fd); return 0; }

    char buf[2048]; int got = recv(c, buf, sizeof(buf) - 1, 0);
    buf[got] = '\0';
    /* 解析到 \r\n\r\n 为止，丢弃 body */
    char *body = strstr(buf, "\r\n\r\n");
    if (body) {
        body += 4;
        /* 模拟流式响应：3 个 token + done */
        const char *resp =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/event-stream\r\n\r\n"
            "data: {\"choices\":[{\"delta\":{\"content\":\"He\"}}]}\n\n"
            "data: {\"choices\":[{\"delta\":{\"content\":\"llo\"}}]}\n\n"
            "data: {\"choices\":[{\"delta\":{\"content\":\" world\"}}]}\n\n"
            "data: [DONE]\n\n";
        send(c, resp, strlen(resp), 0);
    }
    closesocket(c);
    closesocket(listen_fd);
    return 0;
}

static char g_tokens[256]; static int g_tok_count = 0;
static void on_token(const char *t, size_t n, void *ud)
{
    (void)ud;
    if (g_tok_count + n < (int)sizeof(g_tokens)) {
        memcpy(g_tokens + g_tok_count, t, n);
        g_tok_count += n;
        g_tokens[g_tok_count] = '\0';
    }
}
static int g_done = 0; static int g_done_ok = 0; static char g_done_err[128];
static void on_done(bool ok, const char *err, void *ud)
{
    (void)ud;
    g_done = 1; g_done_ok = ok;
    if (err) { strncpy(g_done_err, err, sizeof(g_done_err) - 1); g_done_err[sizeof(g_done_err)-1] = '\0'; }
}

int main(void)
{
    WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa);
    int port = find_port();
    _beginthreadex(NULL, 0, mock_llm, (void *)(intptr_t)port, 0, NULL);
    Sleep(100);

    agent_llm_provider_t p = {0};
    snprintf(p.base_url, sizeof(p.base_url), "http://127.0.0.1:%d/v1/", port);
    snprintf(p.api_key, sizeof(p.api_key), "sk-test");
    strcpy(p.default_model, "mock-model");
    strcpy(p.name, "Mock");

    const char *messages = "[{\"role\":\"user\",\"content\":\"hi\"}]";
    int rc = llm_chat_stream(&p, NULL, messages, on_token, on_done, NULL);
    assert(rc == AGENT_OK);

    /* 等 worker 完成（最多 5 秒） */
    for (int i = 0; i < 50 && !g_done; i++) Sleep(100);
    assert(g_done);
    assert(g_done_ok);
    assert(strcmp(g_tokens, "Hello world") == 0);

    printf("test_llm_client_mock: tokens='%s' ok=%d\n", g_tokens, g_done_ok);
    WSACleanup();
    return 0;
}
