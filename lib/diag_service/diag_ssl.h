/**
 * @file diag_ssl.h
 * @brief HTTPS SSL 探活：用 libcurl 做 GET，提取证书 issuer/expiry/HTTP 状态。
 *
 * 不解析 HTML body，只看"能不能 TLS 握手 + 证书对不对 + HTTP 200"。
 */
#ifndef LIB_DIAG_SSL_H
#define LIB_DIAG_SSL_H

#include <stdbool.h>
#include <stddef.h>

/* 探活结果（一次性拷贝到 caller 提供的缓冲里） */
typedef struct {
    int   http_status;       /* 0 = 连接/TLS 失败 */
    char  issuer[128];       /* "CN=Let's Encrypt, O=..." */
    char  expiry[32];        /* "2026-12-31" */
    int   total_ms;          /* 总耗时 */
    char  err[128];          /* 失败时填 uv_strerror/curl_easy_strerror */
} diag_ssl_result_t;

/* 异步探活。cb 由 main loop 触发；result 由 lib/diag_service 内部 free。 */
typedef void (*diag_ssl_cb)(const diag_ssl_result_t *result, void *userdata);

int  diag_ssl_probe(const char *url, int timeout_ms, diag_ssl_cb cb, void *userdata);

/* 全局 init/cleanup：libcurl 需要。多次调用幂等。 */
int  diag_ssl_global_init(void);
void diag_ssl_global_cleanup(void);

#endif
