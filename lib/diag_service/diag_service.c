/**
 * @file diag_service.c
 * @brief 诊断服务实现：一次性拉取 7 项诊断值。
 *
 * P3 简化：不做定期刷新；上层在需要时调 refresh_now()。
 * P3 简化：每条 AT 命令**不等回复**（连续发，串行 chan 会按 FIFO 处理，at_session 状态机在后台跑）。
 * 真实值要在下个 tick 通过 on_response_cb 写回——这里给个最小占位。
 */
#include "diag_service.h"
#include "at_session.h"
#include "agent_types.h"
#include "strbuf.h"

#include <uv.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define MAX_DEVS 16

typedef struct diag_service {
    uv_loop_t        *loop;
    device_manager_t *dm;
    diag_state_t      states[MAX_DEVS];
} diag_service_t;

static void on_at_response(void *ud, const char *res, size_t len, bool ok)
{
    strbuf_t *dst = (strbuf_t *)ud;
    if (!ok || !res) {
        strbuf_reset(dst);
        strbuf_append(dst, "(error)");
        return;
    }
    strbuf_reset(dst);
    strbuf_append_n(dst, res, len);
}

static void fetch(at_session_t *at, const char *cmd, strbuf_t *buf)
{
    strbuf_reset(buf);
    at_session_send(at, cmd, 3000, on_at_response, buf);
}

diag_service_t *diag_service_create(uv_loop_t *loop, device_manager_t *dm)
{
    if (!loop || !dm) return NULL;
    diag_service_t *s = (diag_service_t *)calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->loop = loop;
    s->dm = dm;
    for (int i = 0; i < MAX_DEVS; i++) diag_state_reset(&s->states[i]);
    return s;
}

void diag_service_destroy(diag_service_t *s)
{
    if (!s) return;
    free(s);
}

const diag_state_t *diag_service_get_state(diag_service_t *s, int dev_idx)
{
    if (!s || dev_idx < 0 || dev_idx >= MAX_DEVS) return NULL;
    return &s->states[dev_idx];
}

void diag_service_refresh_now(diag_service_t *s)
{
    if (!s || !s->dm) return;
    /* 用 6 个独立 buf 给 6 条 AT 命令各自的回调写——
     * 原 strbuf_t buf 单 buf 复用导致每条 AT 的响应互相覆盖。
     * static 让 buf 生命周期跨 refresh_now 调用，回调里写时不悬空。 */
    static strbuf_t bufs[6];

    for (int i = 0; i < s->dm->dev_count && i < MAX_DEVS; i++) {
        modem_dev_t *d = &s->dm->devs[i];
        if (d->state != DEV_STATE_READY || !d->at) continue;

        for (int j = 0; j < 6; j++) {
            strbuf_init(&bufs[j], 256);
            strbuf_reset(&bufs[j]);
        }

        fetch(d->at, "AT+CGSN",   &bufs[0]);
        strncpy(s->states[i].imei,         bufs[0].data, sizeof(s->states[i].imei) - 1);
        fetch(d->at, "AT+CIMI",   &bufs[1]);
        strncpy(s->states[i].imsi,         bufs[1].data, sizeof(s->states[i].imsi) - 1);
        fetch(d->at, "AT+CCID",   &bufs[2]);
        strncpy(s->states[i].iccid,        bufs[2].data, sizeof(s->states[i].iccid) - 1);
        fetch(d->at, "AT+CSQ",    &bufs[3]);
        strncpy(s->states[i].csq,          bufs[3].data, sizeof(s->states[i].csq) - 1);
        fetch(d->at, "AT+CEREG?", &bufs[4]);
        strncpy(s->states[i].cereg,        bufs[4].data, sizeof(s->states[i].cereg) - 1);
        fetch(d->at, "AT+COPS?",  &bufs[5]);
        strncpy(s->states[i].cop_operator, bufs[5].data, sizeof(s->states[i].cop_operator) - 1);

        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        strftime(s->states[i].last_update, sizeof(s->states[i].last_update),
                 "%H:%M:%S", t);
        s->states[i].valid = true;
    }
}
