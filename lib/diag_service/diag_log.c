/**
 * @file diag_log.c
 * @brief 模组 log 抓取实现：挂 rx hook 收字节写 tmp log → uv_timer 到点 →
 *        miniz 压成 zip → 回调。
 */
#include "diag_log.h"
#include "at_session.h"
#include "agent_types.h"

#include <uv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* miniz 单文件头（third_party/miniz/miniz.h） */
#include "miniz.h"

/* 默认 log 抓取期间落盘的临时文件最大字节数（防磁盘爆）。
 * 实际模组 30s 抓 log 远低于此；仅作硬上限。 */
#define LOG_MAX_BYTES_DEFAULT  (64u * 1024u * 1024u)   /* 64 MB */

typedef struct {
    uv_loop_t      *loop;        /* 复用 at_session 内部 loop */
    uv_timer_t      timer;       /* 到点触发 finish */
    at_session_t   *sess;
    char            zip_path[512];
    char            tmp_log_path[512];
    FILE           *fp;          /* 当前在写的 tmp log 句柄 */
    size_t          bytes_written;
    size_t          max_bytes;   /* 硬上限 */
    bool            finished;    /* 防 cb 多次触发 */
    diag_log_cb     cb;
    void           *userdata;
} log_ctx_t;

/* ---------- 内部工具 ---------- */

/**
 * @brief 把 zip_path 的扩展名替换成 .log，得到 tmp 落盘路径。
 *
 * 例子： "logs/capture.zip" → "logs/capture.log"
 *       "logs/foo"         → "logs/foo.log"
 *       "/tmp/x.y.zip"     → "/tmp/x.y.log"
 */
static void make_tmp_log_path(const char *zip_path, char *out, size_t out_size)
{
    if (!zip_path || !out || out_size == 0) return;
    strncpy(out, zip_path, out_size - 1);
    out[out_size - 1] = '\0';

    /* 找最后一个 '.zip' / '.ZIP' 替换；否则直接追加 .log */
    char *dot = strrchr(out, '.');
    if (dot && (strcasecmp(dot, ".zip") == 0)) {
        strcpy(dot, ".log");
    } else {
        /* 没 .zip 后缀：直接追加 .log（要预留 4 字节） */
        size_t cur = strlen(out);
        if (cur + 4 < out_size) {
            strcat(out, ".log");
        }
    }
}

/* ---------- 收尾：压 zip + 调 cb ---------- */

/**
 * @brief 抓 log 收尾。流程：
 *   1) 关 fp（如果还开着）
 *   2) 停 timer / 关 handle
 *   3) miniz 把 tmp log 压成 zip；fseek/ftell 算 zip_size
 *   4) 删 tmp log
 *   5) 调 caller cb
 *
 * 注意：plan 原始版本里 `if (!mz_zip_archive zip = {0}; 1) { ok = false; }`
 * 是语法错误（`if` 初始化器只支持 C99 单变量）。本实现拆成多行：
 *   mz_zip_archive zip = {0};
 *   if (mz_zip_writer_init_file(&zip, ...)) { ... } else { ok = false; }
 */
static void log_finish(log_ctx_t *ctx, bool ok)
{
    if (ctx->finished) return;
    ctx->finished = true;

    /* 1) 关 fp */
    if (ctx->fp) {
        fclose(ctx->fp);
        ctx->fp = NULL;
    }

    /* 2) 停 timer；handle 在 uv_close 里释放（log_ctx 在 cb 之后才 free） */
    if (ctx->timer.data) {            /* data 没被清掉就说明 timer 已 init */
        uv_timer_stop(&ctx->timer);
        uv_close((uv_handle_t *)&ctx->timer, NULL);
    }

    size_t zip_size = 0;
    if (ok) {
        /* 3) miniz 压 zip */
        mz_zip_archive zip = {0};
        if (mz_zip_writer_init_file(&zip, ctx->zip_path, 0)) {
            if (mz_zip_writer_add_file(&zip, "modem.log", ctx->tmp_log_path,
                                       NULL, 0, MZ_DEFAULT_LEVEL)) {
                mz_zip_writer_finalize_archive(&zip);
                mz_zip_writer_end(&zip);
                /* 4a) 算 zip_size：fopen rb + fseek/ftell */
                FILE *fz = fopen(ctx->zip_path, "rb");
                if (fz) {
                    if (fseek(fz, 0, SEEK_END) == 0) {
                        long sz = ftell(fz);
                        if (sz > 0) zip_size = (size_t)sz;
                    }
                    fclose(fz);
                }
                if (zip_size == 0) ok = false;   /* 压出来空文件 = 失败 */
            } else {
                ok = false;
                mz_zip_writer_end(&zip);
            }
        } else {
            ok = false;
        }
    }

    /* 4b) 删 tmp log（无论 ok 与否，避免占盘） */
    remove(ctx->tmp_log_path);

    /* 5) 调 caller cb（先把 cb/ud/zip_path 拷到栈上，再 free ctx） */
    diag_log_cb cb = ctx->cb;
    void       *ud = ctx->userdata;
    char        zp[512];
    strncpy(zp, ctx->zip_path, sizeof(zp) - 1);
    zp[sizeof(zp) - 1] = '\0';
    free(ctx);
    if (cb) cb(ok, zp, zip_size, ud);
}

/* ---------- uv 回调 ---------- */

/**
 * @brief uv_timer 到点：触发 finish。
 */
static void log_on_timer(uv_timer_t *t)
{
    log_ctx_t *ctx = (log_ctx_t *)t->data;
    log_finish(ctx, true);
}

/**
 * @brief at_session rx hook：把每个 rx 块直接落到 tmp log。
 */
static void log_on_rx(void *ud, const uint8_t *buf, size_t len)
{
    log_ctx_t *ctx = (log_ctx_t *)ud;
    if (!ctx || !ctx->fp || !buf || len == 0) return;

    /* 达到硬上限就停写（不关 fp，由 timer 统一收尾） */
    if (ctx->bytes_written >= ctx->max_bytes) return;

    size_t writable = len;
    if (ctx->bytes_written + writable > ctx->max_bytes) {
        writable = ctx->max_bytes - ctx->bytes_written;
    }
    size_t n = fwrite(buf, 1, writable, ctx->fp);
    ctx->bytes_written += n;
}

/* ---------- 公共 API ---------- */

/**
 * @brief 启动一次 log 抓取。
 */
int diag_capture_log(at_session_t *s, int sec, const char *out_zip_path,
                     diag_log_cb cb, void *userdata)
{
    if (!s || !out_zip_path || sec <= 0 || !cb) return AGENT_ERR_BAD_ARG;

    uv_loop_t *loop = at_session_loop(s);
    if (!loop) return AGENT_ERR_BAD_ARG;

    log_ctx_t *ctx = (log_ctx_t *)calloc(1, sizeof(*ctx));
    if (!ctx) return AGENT_ERR_OOM;
    ctx->sess        = s;
    ctx->cb          = cb;
    ctx->userdata    = userdata;
    ctx->loop        = loop;
    ctx->max_bytes   = LOG_MAX_BYTES_DEFAULT;

    strncpy(ctx->zip_path, out_zip_path, sizeof(ctx->zip_path) - 1);
    ctx->zip_path[sizeof(ctx->zip_path) - 1] = '\0';
    make_tmp_log_path(out_zip_path, ctx->tmp_log_path, sizeof(ctx->tmp_log_path));

    /* 打开 tmp log（"wb" 覆盖） */
    ctx->fp = fopen(ctx->tmp_log_path, "wb");
    if (!ctx->fp) {
        free(ctx);
        return AGENT_ERR_IO;
    }

    /* 启 uv_timer */
    int r = uv_timer_init(loop, &ctx->timer);
    if (r != 0) {
        fclose(ctx->fp);
        remove(ctx->tmp_log_path);
        free(ctx);
        return AGENT_ERR_IO;
    }
    ctx->timer.data = ctx;
    r = uv_timer_start(&ctx->timer, log_on_timer, (uint64_t)(sec * 1000u), 0);
    if (r != 0) {
        fclose(ctx->fp);
        remove(ctx->tmp_log_path);
        uv_close((uv_handle_t *)&ctx->timer, NULL);
        free(ctx);
        return AGENT_ERR_IO;
    }

    /* 挂 rx hook（覆盖模式——挂上后 at_session 自己的 cmd 路径停摆） */
    at_session_install_rx_hook(s, log_on_rx, ctx);
    return AGENT_OK;
}
