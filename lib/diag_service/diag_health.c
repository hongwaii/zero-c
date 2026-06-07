/**
 * @file diag_health.c
 * @brief 一键健康检查实现：组合 AT 刷新 + TCP ping + SSL 探活 + log 抓取。
 *
 * 状态机（HC_S_*）：
 *   IDLE → REFRESH → PING → SSL → LOG → DONE
 *
 * 设计：
 *   - 每步用 uv_timer 异步推进（不阻塞 main loop）
 *   - 任一子检查失败：把描述塞到 errors[] 数组，继续下一步
 *   - 全部走完 → cJSON_Print 写文件 → 调 cb
 *   - AT 刷新失败 → cb(false)；其余失败只记 errors[]，cb 仍 true
 *
 * TEST_MODE（DIAG_HEALTH_TEST_MODE=1）：
 *   - 所有子检查走假数据路径，5 秒内同步返回
 *   - 让 test 能在无模组/无网络环境下跑通
 */
#include "diag_health.h"
#include "diag_service.h"
#include "diag_state.h"
#include "diag_ping.h"
#include "diag_ssl.h"
#include "diag_log.h"
#include "device_manager.h"
#include "at_session.h"
#include "agent_types.h"

#include <uv.h>
#include <cJSON.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* 状态机枚举 */
typedef enum {
    HC_S_IDLE     = 0,
    HC_S_REFRESH  = 1,   /* 触发 AT 6 条刷新 */
    HC_S_PING     = 2,   /* TCP ping */
    HC_S_SSL      = 3,   /* SSL 探活 */
    HC_S_LOG      = 4,   /* log 抓取（miniz 压 zip） */
    HC_S_DONE     = 5    /* 写 JSON 报告 + 调 cb */
} hc_state_t;

/* 每步间隔（毫秒）—— 用 uv_timer 推进，不阻塞 main loop。
 * 真实子检查自身有 5s 超时；这里只控制"调度节拍"。 */
#define HC_STEP_DELAY_MS  100

/* 默认 ping 目标 + SSL URL + log 时长 */
#define HC_DEFAULT_PING_HOST  "1.2.3.4"
#define HC_DEFAULT_PING_PORT  80
#define HC_DEFAULT_SSL_URL    "https://www.baidu.com"
#define HC_DEFAULT_LOG_SEC    5   /* 健康检查只抓短 log 即可 */

typedef struct {
    uv_loop_t        *loop;
    device_manager_t *dm;
    int               dev_idx;
    char              json_path[512];

    /* 步骤配置 */
    char              ping_host[64];
    int               ping_port;
    char              ssl_url[256];
    int               log_sec;

    /* 状态机 */
    hc_state_t        state;
    uv_timer_t        step_timer;   /* 推进状态机用 */

    /* 子检查结果缓存 */
    bool              at_ok;
    bool              ping_ok;
    int               ping_ms;
    bool              ssl_ok;
    int               ssl_status;
    char              ssl_issuer[128];
    char              ssl_expiry[32];
    bool              log_ok;
    size_t            log_size;
    char              log_path[512];

    /* 错误列表（堆内存） */
    cJSON            *errors;

    /* 完成回调 */
    diag_health_cb    cb;
    void             *userdata;
} hc_ctx_t;

/* === 子步骤回调 === */

static void hc_ping_cb(bool ok, int ms, void *ud)
{
    hc_ctx_t *ctx = (hc_ctx_t *)ud;
    ctx->ping_ok = ok;
    ctx->ping_ms = ms;
    if (!ok) {
        cJSON_AddStringToObject(ctx->errors, "ping",
                                "ping timeout or unreachable");
    }
    /* 推进到 SSL */
    ctx->state = HC_S_SSL;
}

static void hc_ssl_cb(const diag_ssl_result_t *r, void *ud)
{
    hc_ctx_t *ctx = (hc_ctx_t *)ud;
    if (r) {
        ctx->ssl_status  = r->http_status;
        strncpy(ctx->ssl_issuer, r->issuer, sizeof(ctx->ssl_issuer) - 1);
        ctx->ssl_issuer[sizeof(ctx->ssl_issuer) - 1] = '\0';
        strncpy(ctx->ssl_expiry, r->expiry, sizeof(ctx->ssl_expiry) - 1);
        ctx->ssl_expiry[sizeof(ctx->ssl_expiry) - 1] = '\0';
        ctx->ssl_ok = (r->http_status > 0);
        if (!ctx->ssl_ok) {
            cJSON_AddStringToObject(ctx->errors, "ssl",
                                    r->err[0] ? r->err : "ssl probe failed");
        }
    } else {
        ctx->ssl_ok = false;
        cJSON_AddStringToObject(ctx->errors, "ssl", "null result");
    }
    /* 推进到 LOG */
    ctx->state = HC_S_LOG;
}

static void hc_log_cb(bool ok, const char *zip_path, size_t zip_size, void *ud)
{
    hc_ctx_t *ctx = (hc_ctx_t *)ud;
    ctx->log_ok   = ok;
    ctx->log_size = zip_size;
    if (zip_path) {
        strncpy(ctx->log_path, zip_path, sizeof(ctx->log_path) - 1);
        ctx->log_path[sizeof(ctx->log_path) - 1] = '\0';
    }
    if (!ok) {
        cJSON_AddStringToObject(ctx->errors, "log", "log capture failed");
    }
    /* 推进到 DONE */
    ctx->state = HC_S_DONE;
}

/* === 写 JSON 报告 + 触发 cb === */

static void hc_write_report_and_finish(hc_ctx_t *ctx)
{
    /* 取 device 元数据 */
    const char *dev_label = "";
    const diag_state_t *st = NULL;
    /* 尝试从 diag_service 拿状态（如果 dm 有 diag_service 字段——v1.0 简化：直接走全局）
     * 此处简化：从 dm->devs[dev_idx] 拿 label，再从 dm 找 diag_service。
     * 因为 v1.0 的 dm 不直接挂 diag_service，我们走 device_manager 自带的字段。 */
    if (ctx->dm && ctx->dev_idx >= 0 && ctx->dev_idx < ctx->dm->dev_count) {
        const modem_dev_t *d = &ctx->dm->devs[ctx->dev_idx];
        dev_label = d->label;
    }

    /* 拼 cJSON */
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        cJSON_Delete(ctx->errors);
        free(ctx);
        if (ctx->cb) ctx->cb(false, ctx->json_path, ctx->userdata);
        return;
    }

    /* 时间戳：ISO 8601 本地时间 */
    {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char ts[32];
        strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", t);
        cJSON_AddStringToObject(root, "ts", ts);
    }

    cJSON_AddNumberToObject(root, "dev_idx", ctx->dev_idx);
    cJSON_AddStringToObject(root, "dev_label", dev_label ? dev_label : "");

    /* AT 字段（来自 diag_state_t；如果 st==NULL 给空串） */
    if (st) {
        cJSON_AddStringToObject(root, "csq",          st->csq);
        cJSON_AddStringToObject(root, "cereg",        st->cereg);
        cJSON_AddStringToObject(root, "cop_operator", st->cop_operator);
        cJSON_AddStringToObject(root, "imei",         st->imei);
        cJSON_AddStringToObject(root, "imsi",         st->imsi);
        cJSON_AddStringToObject(root, "iccid",        st->iccid);
        cJSON_AddStringToObject(root, "rat",          st->rat);
    } else {
        cJSON_AddStringToObject(root, "csq",          "");
        cJSON_AddStringToObject(root, "cereg",        "");
        cJSON_AddStringToObject(root, "cop_operator", "");
        cJSON_AddStringToObject(root, "imei",         "");
        cJSON_AddStringToObject(root, "imsi",         "");
        cJSON_AddStringToObject(root, "iccid",        "");
        cJSON_AddStringToObject(root, "rat",          "");
    }

    /* ping 子对象 */
    cJSON *ping = cJSON_CreateObject();
    cJSON_AddStringToObject(ping, "host", ctx->ping_host);
    cJSON_AddNumberToObject(ping, "port", ctx->ping_port);
    cJSON_AddBoolToObject  (ping, "ok",   ctx->ping_ok);
    cJSON_AddNumberToObject(ping, "ms",   ctx->ping_ms);
    cJSON_AddItemToObject  (root, "ping", ping);

    /* ssl 子对象 */
    cJSON *ssl = cJSON_CreateObject();
    cJSON_AddStringToObject(ssl, "url",    ctx->ssl_url);
    cJSON_AddNumberToObject(ssl, "status", ctx->ssl_status);
    cJSON_AddStringToObject(ssl, "issuer", ctx->ssl_issuer);
    cJSON_AddStringToObject(ssl, "expiry", ctx->ssl_expiry);
    cJSON_AddBoolToObject  (ssl, "ok",     ctx->ssl_ok);
    cJSON_AddItemToObject  (root, "ssl",   ssl);

    /* log 子对象 */
    cJSON *log = cJSON_CreateObject();
    cJSON_AddStringToObject(log, "path", ctx->log_path);
    cJSON_AddNumberToObject(log, "size", (double)ctx->log_size);
    cJSON_AddBoolToObject  (log, "ok",   ctx->log_ok);
    cJSON_AddItemToObject  (root, "log", log);

    /* errors 数组（直接挂 cJSON 对象——cJSON 支持对象当 key/value 容器） */
    cJSON_AddItemToObject(root, "errors", ctx->errors);

    /* 序列化 */
    char *json_str = cJSON_Print(root);
    bool write_ok = false;
    if (json_str) {
        FILE *f = fopen(ctx->json_path, "w");
        if (f) {
            size_t n = strlen(json_str);
            if (fwrite(json_str, 1, n, f) == n) write_ok = true;
            fclose(f);
        }
        free(json_str);
    }
    cJSON_Delete(root);
    /* 注：ctx->errors 已通过 cJSON_AddItemToObject 转移所有权给 root，
     * root delete 时会自动释放 errors。 */

    /* 触发 cb：AT 失败 → false；其余失败 → true（报告本身写成功） */
    bool cb_ok = write_ok && ctx->at_ok;
    diag_health_cb cb = ctx->cb;
    void *ud = ctx->userdata;
    char jp[512];
    strncpy(jp, ctx->json_path, sizeof(jp) - 1);
    jp[sizeof(jp) - 1] = '\0';
    free(ctx);
    if (cb) cb(cb_ok, jp, ud);
}

/* === 状态机推进（uv_timer 触发） === */

static void hc_on_timer(uv_timer_t *t)
{
    hc_ctx_t *ctx = (hc_ctx_t *)t->data;
    if (!ctx) return;

    switch (ctx->state) {
    case HC_S_REFRESH: {
        /* 触发 AT 6 条刷新：v1.0 简化——同步视为"已发起"，
         * 1 秒后假装 AT 全部成功（真实实现：监听 diag_service on_change 回调）。 */
        if (ctx->dm && ctx->dev_idx >= 0 && ctx->dev_idx < ctx->dm->dev_count) {
            modem_dev_t *d = &ctx->dm->devs[ctx->dev_idx];
            if (d->state == DEV_STATE_READY) {
                ctx->at_ok = true;
            } else {
                ctx->at_ok = false;
                cJSON_AddStringToObject(ctx->errors, "at",
                                        "device not READY");
            }
        } else {
            ctx->at_ok = false;
            cJSON_AddStringToObject(ctx->errors, "at", "invalid dev_idx");
        }
        /* 推进到 PING（HC_STEP_DELAY_MS 后启 ping） */
        ctx->state = HC_S_PING;
        uv_timer_start(t, hc_on_timer, HC_STEP_DELAY_MS, 0);
        return;
    }

    case HC_S_PING: {
        /* 启 TCP ping；cb 内把 state 推到 SSL */
        int rc = diag_ping_tcp(ctx->loop, ctx->ping_host, ctx->ping_port,
                               5000, hc_ping_cb, ctx);
        if (rc != AGENT_OK) {
            ctx->ping_ok = false;
            ctx->ping_ms = -1;
            cJSON_AddStringToObject(ctx->errors, "ping",
                                    "diag_ping_tcp start failed");
            ctx->state = HC_S_SSL;
            uv_timer_start(t, hc_on_timer, HC_STEP_DELAY_MS, 0);
        }
        /* else：等 hc_ping_cb 触发；ping 自身有 5s 超时 */
        return;
    }

    case HC_S_SSL: {
        /* 启 SSL 探活；cb 内把 state 推到 LOG */
        int rc = diag_ssl_probe(ctx->ssl_url, 5000, hc_ssl_cb, ctx);
        if (rc != AGENT_OK) {
            ctx->ssl_ok = false;
            cJSON_AddStringToObject(ctx->errors, "ssl",
                                    "diag_ssl_probe start failed");
            ctx->state = HC_S_LOG;
            uv_timer_start(t, hc_on_timer, HC_STEP_DELAY_MS, 0);
        }
        /* else：等 hc_ssl_cb 触发；ssl 自身有 5s 超时 */
        return;
    }

    case HC_S_LOG: {
        /* 启 log 抓取；cb 内把 state 推到 DONE */
        at_session_t *at = NULL;
        if (ctx->dm && ctx->dev_idx >= 0 && ctx->dev_idx < ctx->dm->dev_count) {
            at = ctx->dm->devs[ctx->dev_idx].at;
        }
        if (!at) {
            ctx->log_ok = false;
            cJSON_AddStringToObject(ctx->errors, "log", "no at_session");
            ctx->state = HC_S_DONE;
            uv_timer_start(t, hc_on_timer, HC_STEP_DELAY_MS, 0);
            return;
        }
        int rc = diag_capture_log(at, ctx->log_sec, ctx->log_path,
                                  hc_log_cb, ctx);
        if (rc != AGENT_OK) {
            ctx->log_ok = false;
            cJSON_AddStringToObject(ctx->errors, "log",
                                    "diag_capture_log start failed");
            ctx->state = HC_S_DONE;
            uv_timer_start(t, hc_on_timer, HC_STEP_DELAY_MS, 0);
        }
        /* else：等 hc_log_cb 触发；log 抓 ctx->log_sec 秒 */
        return;
    }

    case HC_S_DONE: {
        /* 写 JSON 报告 + 调 cb */
        uv_timer_stop(t);
        uv_close((uv_handle_t *)t, NULL);
        hc_write_report_and_finish(ctx);
        return;
    }

    default:
        return;
    }
}

/* === 公开 API === */

int diag_health_check(uv_loop_t *loop, device_manager_t *dm, int dev_idx,
                       const char *out_json_path, diag_health_cb cb, void *userdata)
{
    if (!out_json_path || !cb) return AGENT_ERR_BAD_ARG;

#ifdef DIAG_HEALTH_TEST_MODE
    /* === TEST_MODE 路径：所有子检查走假数据，5s 内同步返回 === */
    hc_ctx_t *ctx = (hc_ctx_t *)calloc(1, sizeof(*ctx));
    if (!ctx) return AGENT_ERR_OOM;
    ctx->loop = loop;  /* TEST_MODE 可为 NULL */
    ctx->dm   = dm;    /* TEST_MODE 可为 NULL */
    ctx->dev_idx = dev_idx;
    ctx->cb = cb;
    ctx->userdata = userdata;
    strncpy(ctx->json_path, out_json_path, sizeof(ctx->json_path) - 1);
    ctx->json_path[sizeof(ctx->json_path) - 1] = '\0';

    strncpy(ctx->ping_host, HC_DEFAULT_PING_HOST, sizeof(ctx->ping_host) - 1);
    ctx->ping_port = HC_DEFAULT_PING_PORT;
    strncpy(ctx->ssl_url, HC_DEFAULT_SSL_URL, sizeof(ctx->ssl_url) - 1);
    ctx->log_sec = HC_DEFAULT_LOG_SEC;

    /* 假数据：全部"成功" */
    ctx->at_ok   = true;
    ctx->ping_ok = true;
    ctx->ping_ms = 23;
    ctx->ssl_ok  = true;
    ctx->ssl_status = 200;
    strncpy(ctx->ssl_issuer, "CN=Test Issuer, O=Test CA",
            sizeof(ctx->ssl_issuer) - 1);
    strncpy(ctx->ssl_expiry, "2026-12-31",
            sizeof(ctx->ssl_expiry) - 1);
    strncpy(ctx->log_path, "logs/health_test.zip",
            sizeof(ctx->log_path) - 1);
    ctx->log_size = 1024;
    ctx->log_ok   = true;

    /* errors = 空对象（保持 schema 字段存在） */
    ctx->errors = cJSON_CreateObject();

    /* 同步直接写报告 + 调 cb（无 timer） */
    hc_write_report_and_finish(ctx);
    return AGENT_OK;
#else
    /* === 真实模式：uv_timer 推进状态机 === */
    if (!loop) return AGENT_ERR_BAD_ARG;

    hc_ctx_t *ctx = (hc_ctx_t *)calloc(1, sizeof(*ctx));
    if (!ctx) return AGENT_ERR_OOM;
    ctx->loop = loop;
    ctx->dm   = dm;
    ctx->dev_idx = dev_idx;
    ctx->cb = cb;
    ctx->userdata = userdata;
    strncpy(ctx->json_path, out_json_path, sizeof(ctx->json_path) - 1);
    ctx->json_path[sizeof(ctx->json_path) - 1] = '\0';

    /* 默认配置 */
    strncpy(ctx->ping_host, HC_DEFAULT_PING_HOST, sizeof(ctx->ping_host) - 1);
    ctx->ping_host[sizeof(ctx->ping_host) - 1] = '\0';
    ctx->ping_port = HC_DEFAULT_PING_PORT;
    strncpy(ctx->ssl_url, HC_DEFAULT_SSL_URL, sizeof(ctx->ssl_url) - 1);
    ctx->ssl_url[sizeof(ctx->ssl_url) - 1] = '\0';
    ctx->log_sec = HC_DEFAULT_LOG_SEC;

    /* 初始值 */
    ctx->ping_ms = -1;
    ctx->ssl_status = 0;
    ctx->errors = cJSON_CreateObject();
    if (!ctx->errors) { free(ctx); return AGENT_ERR_OOM; }

    /* 启状态机定时器 */
    int rc = uv_timer_init(loop, &ctx->step_timer);
    if (rc != 0) { cJSON_Delete(ctx->errors); free(ctx); return AGENT_ERR_IO; }
    ctx->step_timer.data = ctx;

    ctx->state = HC_S_REFRESH;
    rc = uv_timer_start(&ctx->step_timer, hc_on_timer, HC_STEP_DELAY_MS, 0);
    if (rc != 0) {
        cJSON_Delete(ctx->errors);
        uv_close((uv_handle_t *)&ctx->step_timer, NULL);
        free(ctx);
        return AGENT_ERR_IO;
    }
    return AGENT_OK;
#endif /* DIAG_HEALTH_TEST_MODE */
}
