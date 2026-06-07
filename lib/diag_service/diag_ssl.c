/**
 * @file diag_ssl.c
 * @brief diag_ssl.h 实现 —— 同步 GET 简化版（不阻塞 main loop 几毫秒）。
 *
 * 后续 v1.1 可换异步 libcurl-multi。
 */
#include "diag_ssl.h"
#include "agent_types.h"

#include <curl/curl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* 引用计数：curl_global_init/cleanup 必须成对，多次调用安全 */
static int g_init_count = 0;

int diag_ssl_global_init(void)
{
    if (g_init_count++ > 0) return AGENT_OK;
    curl_global_init(CURL_GLOBAL_DEFAULT);
    return AGENT_OK;
}

void diag_ssl_global_cleanup(void)
{
    if (--g_init_count > 0) return;
    curl_global_cleanup();
}

/* 内部上下文：单次 probe 状态 */
typedef struct {
    diag_ssl_cb        cb;
    void              *userdata;
    diag_ssl_result_t  res;
} ssl_ctx_t;

/* libcurl write callback：丢弃 body（我们只关心握手 + HTTP 状态 + 证书） */
static size_t write_discard(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    (void)ptr; (void)userdata;
    return size * nmemb;
}

/* 解析 libcurl certinfo 文本，提取 Issuer 和 Expire date
 * 格式：每条一行 "Subject: ...\nIssuer: ...\nStart date: ...\nExpire date: ...\n" */
static void parse_cert_info(const char *info, diag_ssl_result_t *r)
{
    if (!info || !r) return;

    const char *issuer_p = strstr(info, "Issuer:");
    if (issuer_p) {
        const char *v = issuer_p + strlen("Issuer:");
        while (*v == ' ' || *v == '\t') v++;
        const char *eol = strchr(v, '\n');
        size_t len = eol ? (size_t)(eol - v) : sizeof(r->issuer) - 1;
        if (len >= sizeof(r->issuer)) len = sizeof(r->issuer) - 1;
        memcpy(r->issuer, v, len);
        r->issuer[len] = '\0';
    }

    const char *expire_p = strstr(info, "Expire date:");
    if (expire_p) {
        const char *v = expire_p + strlen("Expire date:");
        while (*v == ' ' || *v == '\t') v++;
        const char *eol = strchr(v, '\n');
        size_t len = eol ? (size_t)(eol - v) : sizeof(r->expiry) - 1;
        if (len >= sizeof(r->expiry)) len = sizeof(r->expiry) - 1;
        memcpy(r->expiry, v, len);
        r->expiry[len] = '\0';
    }
}

int diag_ssl_probe(const char *url, int timeout_ms, diag_ssl_cb cb, void *userdata)
{
    if (!url || !cb) return AGENT_ERR_BAD_ARG;
    if (timeout_ms <= 0) timeout_ms = 5000;

    diag_ssl_global_init();

    ssl_ctx_t *ctx = (ssl_ctx_t *)calloc(1, sizeof(*ctx));
    if (!ctx) { diag_ssl_global_cleanup(); return AGENT_ERR_OOM; }
    ctx->cb = cb;
    ctx->userdata = userdata;

    CURL *c = curl_easy_init();
    if (!c) { free(ctx); diag_ssl_global_cleanup(); return AGENT_ERR_OOM; }

    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_TIMEOUT_MS, (long)timeout_ms);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_discard);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(c, CURLOPT_CERTINFO, 1L);
    /* 不跟重定向：探活一次，redirect 跳走反而看不到原始证书 */
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 0L);

    clock_t t0 = clock();
    CURLcode rc = curl_easy_perform(c);
    clock_t t1 = clock();
    ctx->res.total_ms = (int)(((t1 - t0) * 1000) / CLOCKS_PER_SEC);

    if (rc == CURLE_OK) {
        long status = 0;
        curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
        ctx->res.http_status = (int)status;
        /* 拿 certinfo：取第一张证书（server cert）的 issuer/expiry */
        struct curl_certinfo *ci = NULL;
        curl_easy_getinfo(c, CURLINFO_CERTINFO, &ci);
        if (ci && ci->num_of_certs > 0 && ci->certinfo[0]) {
            parse_cert_info(ci->certinfo[0]->data, &ctx->res);
        }
    } else {
        snprintf(ctx->res.err, sizeof(ctx->res.err), "%s", curl_easy_strerror(rc));
        ctx->res.err[sizeof(ctx->res.err) - 1] = '\0';
        ctx->res.http_status = 0;
    }
    curl_easy_cleanup(c);
    diag_ssl_global_cleanup();

    /* 同步调 cb（v1.0 简化：不另起线程——几毫秒握手可接受） */
    ctx->cb(&ctx->res, ctx->userdata);
    free(ctx);
    return AGENT_OK;
}
