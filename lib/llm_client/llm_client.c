/**
 * @file llm_client.c
 * @brief OpenAI 兼容 chat completion 客户端实现：libcurl 同步 POST + SSE 流式回包解析。
 *
 * 流程：
 *   1. llm_chat_stream 分配 chat_ctx_t 并启动 worker 线程（_beginthreadex）
 *   2. worker 线程内：libcurl 同步 POST 到 base_url + /chat/completions
 *   3. CURLOPT_WRITEFUNCTION 回调 → sse_parser_feed → sse_on_event
 *   4. sse_on_event 解析 JSON 抽 choices[0].delta.content 调 on_token_cb
 *   5. 流结束 / 出错时调 on_done_cb，worker 退出前 free 资源
 */
#include "llm_client.h"
#include "llm_sse.h"
#include "agent_types.h"

#include <cJSON.h>
#include <curl/curl.h>
#include <uv.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>

#define REQUEST_BODY_MAX  (32 * 1024)
#define RESPONSE_BODY_MAX (256 * 1024)

typedef struct {
    const agent_llm_provider_t *provider;
    const char *model;
    const char *messages_json;
    llm_token_cb on_token;
    llm_done_cb  on_done;
    void        *userdata;

    /* 累积完整响应（tool_call 解析用） */
    char        *full_buf;
    size_t       full_len;
    size_t       full_cap;

    /* 当前 SSE 解析器 */
    sse_parser_t *sse;

    /* 错误缓冲 */
    char  err[256];
} chat_ctx_t;

static bool sse_on_event(const sse_event_t *ev, void *ud)
{
    chat_ctx_t *ctx = (chat_ctx_t *)ud;
    if (ev->type == SSE_EV_DONE) return true;
    if (ev->type != SSE_EV_DATA) return true;
    /* payload 是 "choices":[{"delta":{"content":"..."}}] 的 JSON 字符串 */
    if (ev->payload_len == 0) return true;
    /* v1.0 简化：用 cJSON 解析 delta.content */
    cJSON *root = cJSON_ParseWithLength(ev->payload, ev->payload_len);
    if (!root) return true;
    cJSON *choices = cJSON_GetObjectItemCaseSensitive(root, "choices");
    if (cJSON_IsArray(choices) && cJSON_GetArraySize(choices) > 0) {
        cJSON *delta = cJSON_GetObjectItemCaseSensitive(
            cJSON_GetArrayItem(choices, 0), "delta");
        cJSON *content = cJSON_GetObjectItemCaseSensitive(delta, "content");
        if (cJSON_IsString(content) && content->valuestring) {
            const char *s = content->valuestring;
            size_t n = strlen(s);
            if (ctx->on_token) ctx->on_token(s, n, ctx->userdata);
            /* 累加到 full_buf */
            if (ctx->full_len + n < ctx->full_cap) {
                memcpy(ctx->full_buf + ctx->full_len, s, n);
                ctx->full_len += n;
                ctx->full_buf[ctx->full_len] = '\0';
            }
        }
    }
    cJSON_Delete(root);
    return true;
}

static size_t curl_write_cb(char *ptr, size_t size, size_t nmemb, void *ud)
{
    chat_ctx_t *ctx = (chat_ctx_t *)ud;
    size_t total = size * nmemb;
    if (ctx->sse) sse_parser_feed(ctx->sse, ptr, total);
    return total;
}

static unsigned __stdcall chat_worker(void *arg)
{
    chat_ctx_t *ctx = (chat_ctx_t *)arg;
    ctx->sse = sse_parser_create(sse_on_event, ctx);
    if (!ctx->sse) { snprintf(ctx->err, sizeof(ctx->err), "sse_parser_create OOM"); goto done; }

    /* 拼 URL：base_url 末尾通常带 /v1，OpenAI 风格 chat completions = /chat/completions */
    char url[512];
    if (ctx->provider->base_url[strlen(ctx->provider->base_url) - 1] == '/')
        snprintf(url, sizeof(url), "%schat/completions", ctx->provider->base_url);
    else
        snprintf(url, sizeof(url), "%s/chat/completions", ctx->provider->base_url);

    /* 拼 request body */
    char body[REQUEST_BODY_MAX];
    int blen = snprintf(body, sizeof(body),
        "{\"model\":\"%s\",\"messages\":%s,\"stream\":true}",
        ctx->model ? ctx->model : ctx->provider->default_model,
        ctx->messages_json);
    if (blen < 0 || blen >= (int)sizeof(body)) {
        snprintf(ctx->err, sizeof(ctx->err), "request body too big");
        goto done;
    }

    /* Authorization header */
    char auth[1024];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", ctx->provider->api_key);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, auth);
    headers = curl_slist_append(headers, "Accept: text/event-stream");

    CURL *c = curl_easy_init();
    if (!c) { snprintf(ctx->err, sizeof(ctx->err), "curl_easy_init failed"); goto done; }

    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_POST, 1L);
    curl_easy_setopt(c, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, ctx);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 2L);

    CURLcode rc = curl_easy_perform(c);
    long status = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(headers);
    curl_easy_cleanup(c);

    if (rc != CURLE_OK) {
        snprintf(ctx->err, sizeof(ctx->err), "curl: %s (status=%ld)", curl_easy_strerror(rc), status);
        goto done;
    }
    if (status >= 400) {
        snprintf(ctx->err, sizeof(ctx->err), "HTTP %ld", status);
        goto done;
    }
done:
    if (ctx->sse) { sse_parser_finalize(ctx->sse); sse_parser_destroy(ctx->sse); ctx->sse = NULL; }
    if (ctx->on_done) ctx->on_done(ctx->err[0] == '\0', ctx->err, ctx->userdata);
    free(ctx->full_buf);
    free(ctx);
    return 0;
}

int llm_chat_stream(const agent_llm_provider_t *provider,
                    const char *model,
                    const char *messages_json,
                    llm_token_cb on_token,
                    llm_done_cb  on_done,
                    void        *userdata)
{
    if (!provider || !messages_json) return AGENT_ERR_BAD_ARG;
    chat_ctx_t *ctx = (chat_ctx_t *)calloc(1, sizeof(*ctx));
    if (!ctx) return AGENT_ERR_OOM;
    ctx->provider = provider;
    ctx->model = model;
    ctx->messages_json = messages_json;
    ctx->on_token = on_token;
    ctx->on_done = on_done;
    ctx->userdata = userdata;
    ctx->full_cap = RESPONSE_BODY_MAX;
    ctx->full_buf = (char *)malloc(ctx->full_cap);
    if (!ctx->full_buf) { free(ctx); return AGENT_ERR_OOM; }
    ctx->full_buf[0] = '\0';

    uintptr_t h = _beginthreadex(NULL, 0, chat_worker, ctx, 0, NULL);
    if (h == 0) { free(ctx->full_buf); free(ctx); return AGENT_ERR_OOM; }
    CloseHandle((HANDLE)h);  /* 脱离——worker 退出时自己清理 */
    return AGENT_OK;
}
