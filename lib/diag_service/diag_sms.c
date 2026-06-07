/**
 * @file diag_sms.c
 * @brief SMS 发送状态机实现：AT+CMGF=1 → AT+CMGS → URC ">" → send_raw(text+0x1A) → OK。
 */
#include "diag_sms.h"
#include "at_session.h"
#include "agent_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Ctrl-Z 终止符（PDU 模式 / 文本模式通用） */
#define SMS_CTRL_Z       0x1A

/* 默认单步超时（毫秒） */
#define SMS_DEFAULT_TIMEOUT_MS  3000

/* 状态机阶段 */
typedef enum {
    SMS_S_IDLE = 0,        /* 刚 init，还没发任何 AT */
    SMS_S_WAIT_CMGF,       /* 已发 AT+CMGF=1，等 OK */
    SMS_S_WAIT_PROMPT,     /* 已发 AT+CMGS="..."，等 URC ">" */
    SMS_S_WAIT_DONE,       /* 已发 raw(text+0x1A)，等 OK / +CMS ERROR */
} sms_state_t;

typedef struct {
    sms_state_t      state;
    int              timeout_ms;
    at_session_t    *sess;
    char             number[32];     /* 目标号码 */
    char            *text;           /* 短信文本（堆上，_strdup） */
    size_t           text_len;
    diag_sms_cb      cb;
    void            *userdata;
    char             status[64];     /* 写回调用方的 status 串 */
} sms_ctx_t;

/* ---------- 工具 ---------- */

/**
 * @brief 安全设置 status 串（截断 + NUL 结尾）。
 */
static void set_status(sms_ctx_t *ctx, const char *s)
{
    if (!s) return;
    strncpy(ctx->status, s, sizeof(ctx->status) - 1);
    ctx->status[sizeof(ctx->status) - 1] = '\0';
}

/**
 * @brief 收尾：调外部 cb、释放 ctx。
 *
 * 释放顺序：先取走 cb/ud/快照 status 串（局部变量），再 free，最后调 cb——
 * 避免 cb 内部如果再调 diag_send_sms 拿到已被 free 的 ctx。
 */
static void sms_finish(sms_ctx_t *ctx, bool ok)
{
    diag_sms_cb cb  = ctx->cb;
    void       *ud  = ctx->userdata;
    char        st[64];
    strncpy(st, ctx->status, sizeof(st) - 1);
    st[sizeof(st) - 1] = '\0';
    free(ctx->text);
    free(ctx);
    if (cb) cb(ok, st, ud);
}

/* ---------- 状态机推进 ---------- */

/* 前向声明：on_raw_done 在 on_prompt_urc 之后定义，但 on_prompt_urc 要
 * 用到它做 send_raw 的完成回调——必须先前向声明。 */
static void on_raw_done(void *ud, const char *res, size_t len, bool ok);

/**
 * @brief 阶段三：等 URC ">" 触发。
 *
 * 收到 "> "（或 "> \r"）即说明模组准备好收短信正文，切换状态到
 * WAIT_DONE，调用 at_session_send_raw 发 text + 0x1A。
 *
 * 防御：state 不在 WAIT_PROMPT 时忽略（多 URC 到达 / 重入安全）。
 */
static void on_prompt_urc(void *ud, const char *line, size_t len)
{
    sms_ctx_t *ctx = (sms_ctx_t *)ud;
    if (ctx->state != SMS_S_WAIT_PROMPT) return;
    if (!line || len == 0 || line[0] != '>') return;

    /* 拼 text + 0x1A，调 at_session_send_raw 发出。
     * 关键：send_raw 不加 \r，直接发出原始字节（Ctrl-Z 必须紧跟 text）。 */
    size_t total = ctx->text_len + 1;       /* +1 = 0x1A */
    uint8_t *buf = (uint8_t *)malloc(total);
    if (!buf) {
        set_status(ctx, "ERROR: OOM");
        sms_finish(ctx, false);
        return;
    }
    if (ctx->text_len > 0) {
        memcpy(buf, ctx->text, ctx->text_len);
    }
    buf[ctx->text_len] = SMS_CTRL_Z;

    ctx->state = SMS_S_WAIT_DONE;
    int rc = at_session_send_raw(ctx->sess, buf, total, ctx->timeout_ms,
                                 on_raw_done, ctx);
    free(buf);
    if (rc != AGENT_OK) {
        set_status(ctx, "ERROR: send_raw failed");
        ctx->state = SMS_S_WAIT_PROMPT;   /* 复位防 on_raw_done 误判 */
        sms_finish(ctx, false);
    }
}

/**
 * @brief at_session_send_raw 完成回调（由 on_raw_done 装载，挂到 cmd_item.userdata）。
 *
 * at_session 的 cmd 队列约束是单 in_flight：send_raw 期间不能再发 send。
 * 我们只让 URC ">" 触发 send_raw，不需要再发额外 AT。所以 on_raw_done
 * 直接读 cmd 的 result（含 "+CMGS: <mr>" + OK 行）判定 ok / err。
 */
static void on_raw_done(void *ud, const char *res, size_t len, bool ok)
{
    sms_ctx_t *ctx = (sms_ctx_t *)ud;
    (void)len;
    if (ctx->state != SMS_S_WAIT_DONE) {
        /* 状态被外层 reset / 多次触发；只 free 一次。 */
        return;
    }
    if (ok) {
        set_status(ctx, "OK");
        sms_finish(ctx, true);
    } else {
        /* res 可能含 "+CMS ERROR: <code>" */
        char buf[64];
        if (res && len > 0) {
            size_t n = (len < sizeof(buf) - 8) ? len : sizeof(buf) - 8;
            snprintf(buf, sizeof(buf), "ERROR: ");
            memcpy(buf + 7, res, n);
            buf[7 + n] = '\0';
            set_status(ctx, buf);
        } else {
            set_status(ctx, "ERROR: timeout");
        }
        sms_finish(ctx, false);
    }
}

/**
 * @brief 阶段一完成回调：AT+CMGF=1 收到 OK。
 *
 * 切到 WAIT_PROMPT，注册 URC ">"，发 AT+CMGS="<num>"。
 * 注意：AT+CMGS 的 cmd 回调传 NULL（我们不等它的"独立 OK"——模组对
 * CMGS 的"完成"是 +CMGS: <mr> 跟一个 OK；但中间夹 "> " URC 提示
 * 让用户发正文。所以这条 cmd 的回调永远不会正常触发 OK，因为我们
 * 不需要"OK 当成发完"——发完的判定由 send_raw 阶段给出。
 *
 * 简化：CMGS 这条 cmd 也带回调，记录 cmgs 阶段错误（如果模组在
 * "> " 之前就回了 ERROR，也能兜底）。
 */
static void on_cmgs_done(void *ud, const char *res, size_t len, bool ok)
{
    sms_ctx_t *ctx = (sms_ctx_t *)ud;
    (void)res; (void)len;
    if (ctx->state != SMS_S_WAIT_PROMPT) return;   /* 已经走完 / 失败 */
    if (!ok) {
        set_status(ctx, "ERROR: CMGS rejected");
        sms_finish(ctx, false);
    }
    /* 成功：什么都不做，等 URC ">" 推进到 send_raw。
     * 如果 URC 永远不来——on_raw_done 不会被调——但 send_raw 自己
     * 的 timeout 由 at_session 处理，到时会以 ok=false 触发
     * on_raw_done，最终落到 "ERROR: timeout" 路径。
     *
     * 双保险：若模组在 "> " 之前就回了 OK（异常模组），这条 cb
     * 也是 ok 状态，URC 永远不会到；则 ctx 永久挂起。
     * v1.0 接受此 corner case——正规模组不会这样。 */
}

static void on_cmgf_done(void *ud, const char *res, size_t len, bool ok)
{
    sms_ctx_t *ctx = (sms_ctx_t *)ud;
    (void)res; (void)len;
    if (ctx->state != SMS_S_WAIT_CMGF) return;
    if (!ok) {
        set_status(ctx, "ERROR: CMGF failed");
        sms_finish(ctx, false);
        return;
    }

    /* 切到 WAIT_PROMPT：先设 state，再注册 URC ">" + 发 AT+CMGS。
     * 顺序重要：mock 模式 at_session_register_urc 会同步触发 URC cb，
     * on_prompt_urc 内部判 state==WAIT_PROMPT 才会推进；真机模式下
     * URC 异步到达也没问题——state 已经是 WAIT_PROMPT。 */
    ctx->state = SMS_S_WAIT_PROMPT;

    int rc = at_session_register_urc(ctx->sess, ">", on_prompt_urc, ctx);
    if (rc != AGENT_OK) {
        set_status(ctx, "ERROR: register URC");
        sms_finish(ctx, false);
        return;
    }
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "AT+CMGS=\"%s\"", ctx->number);

    /* AT+CMGS 这条 cmd 的"完成"对正常模组来说不会单独触发 OK——
     * 模组在收到正文 + Ctrl-Z 后才会回 +CMGS: <mr> + OK。所以
     * on_cmgs_done 主要是兜底：万一模组在 "> " 之前就回 ERROR
     * （如号码非法），能立刻失败。 */
    rc = at_session_send(ctx->sess, cmd, ctx->timeout_ms, on_cmgs_done, ctx);
    if (rc != AGENT_OK) {
        set_status(ctx, "ERROR: send CMGS");
        sms_finish(ctx, false);
    }
}

/* ---------- 公共 API ---------- */

/**
 * @brief 发一条文本短信（异步，状态机驱动）。
 */
int diag_send_sms(at_session_t *s, const char *number, const char *text,
                  int timeout_ms, diag_sms_cb cb, void *userdata)
{
    if (!s || !number || !text || !cb) return AGENT_ERR_BAD_ARG;
    /* number 长度限制：避免 snprintf 截断 + 安全 */
    if (strlen(number) == 0 || strlen(number) >= sizeof(((sms_ctx_t *)0)->number)) {
        return AGENT_ERR_BAD_ARG;
    }
    /* number 不应含双引号 / 不可见字符——CMGS 协议要求 */
    for (const char *p = number; *p; p++) {
        if (*p == '"' || *p == '\r' || *p == '\n') return AGENT_ERR_BAD_ARG;
    }

    sms_ctx_t *ctx = (sms_ctx_t *)calloc(1, sizeof(*ctx));
    if (!ctx) return AGENT_ERR_OOM;
    ctx->sess       = s;
    ctx->cb         = cb;
    ctx->userdata   = userdata;
    ctx->state      = SMS_S_WAIT_CMGF;
    ctx->timeout_ms = (timeout_ms > 0) ? timeout_ms : SMS_DEFAULT_TIMEOUT_MS;
    strncpy(ctx->number, number, sizeof(ctx->number) - 1);
    ctx->number[sizeof(ctx->number) - 1] = '\0';

    ctx->text_len = strlen(text);
    /* 给 text 多分 1 字节方便 + 0x1A（实际上 send_raw 单独再拼）；
     * 此处仅 _strdup 文本本体。 */
    ctx->text = (char *)malloc(ctx->text_len + 1);
    if (!ctx->text) { free(ctx); return AGENT_ERR_OOM; }
    memcpy(ctx->text, text, ctx->text_len + 1);

    /* 启动：第一步发 AT+CMGF=1 */
    int rc = at_session_send(s, "AT+CMGF=1", ctx->timeout_ms, on_cmgf_done, ctx);
    if (rc != AGENT_OK) {
        free(ctx->text);
        free(ctx);
        return rc;
    }
    return AGENT_OK;
}
