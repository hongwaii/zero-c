/**
 * @file diag_service.c
 * @brief 诊断服务实现。
 *
 * refresh_now 异步发 6 条 AT 命令，回调直接写 s->states[i].* 字段——
 * 之前用 strbuf 中转导致字段永远空（strncpy 发生在响应到达前）。
 *
 * 简化：每条 AT 配一个静态 diag_field_ctx_t 存"目标字段指针 + 大小"。
 * 一次只刷一个设备（UI 一次只能点一个按钮）。多设备并发刷新是 P3.5 TODO。
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

/* === 字段 ctx（生命周期跟进程一样长；一次只刷一个设备） === */

typedef struct {
    char  *dest;
    size_t dest_size;
} diag_field_ctx_t;

/* 6 条 AT 命令各自的目标字段 ctx */
static diag_field_ctx_t s_ctx_imei;
static diag_field_ctx_t s_ctx_imsi;
static diag_field_ctx_t s_ctx_iccid;
static diag_field_ctx_t s_ctx_csq;
static diag_field_ctx_t s_ctx_cereg;
static diag_field_ctx_t s_ctx_cops;

/* last_update 跟最后一条命令的回调一起更新 */
static diag_state_t *s_last_state = NULL;
static char         *s_last_state_last_update = NULL;
static size_t        s_last_state_last_update_size = 0;

/**
 * @brief 5 条中间命令（IMEI/IMSI/ICCID/CSQ/CEREG）的回调：直接写 dest。
 */
static void on_diag_field(void *ud, const char *res, size_t len, bool ok)
{
    diag_field_ctx_t *ctx = (diag_field_ctx_t *)ud;
    if (!ctx || !ctx->dest || ctx->dest_size == 0) return;
    if (ok && res) {
        size_t copy_len = (len < ctx->dest_size - 1) ? len : ctx->dest_size - 1;
        memcpy(ctx->dest, res, copy_len);
        ctx->dest[copy_len] = '\0';
    } else {
        ctx->dest[0] = '\0';
    }
}

/**
 * @brief 最后一条命令（COPS）的回调：写完字段后顺手更新 last_update + valid。
 */
static void on_diag_last_field(void *ud, const char *res, size_t len, bool ok)
{
    on_diag_field(ud, res, len, ok);
    /* 全部 6 条 AT 命令响应都到——更新 last_update */
    if (s_last_state && s_last_state_last_update && s_last_state_last_update_size > 0) {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        strftime(s_last_state_last_update, s_last_state_last_update_size,
                 "%H:%M:%S", t);
        s_last_state->valid = true;
    }
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
    for (int i = 0; i < s->dm->dev_count && i < MAX_DEVS; i++) {
        modem_dev_t *d = &s->dm->devs[i];
        if (d->state != DEV_STATE_READY || !d->at) continue;

        diag_state_t *st = &s->states[i];

        /* 设 6 个 ctx 指向 st 的字段（指针常驻；回调时直接写 dest） */
        s_ctx_imei  = (diag_field_ctx_t){ st->imei,         sizeof(st->imei)         };
        s_ctx_imsi  = (diag_field_ctx_t){ st->imsi,         sizeof(st->imsi)         };
        s_ctx_iccid = (diag_field_ctx_t){ st->iccid,        sizeof(st->iccid)        };
        s_ctx_csq   = (diag_field_ctx_t){ st->csq,          sizeof(st->csq)          };
        s_ctx_cereg = (diag_field_ctx_t){ st->cereg,        sizeof(st->cereg)        };
        s_ctx_cops  = (diag_field_ctx_t){ st->cop_operator, sizeof(st->cop_operator) };

        /* last_update 在最后一条响应来时更新 */
        s_last_state = st;
        s_last_state_last_update = st->last_update;
        s_last_state_last_update_size = sizeof(st->last_update);

        /* 6 条 AT 命令各自发；前 5 用 on_diag_field，最后用 on_diag_last_field */
        at_session_send(d->at, "AT+CGSN",   3000, on_diag_field,      &s_ctx_imei);
        at_session_send(d->at, "AT+CIMI",   3000, on_diag_field,      &s_ctx_imsi);
        at_session_send(d->at, "AT+CCID",   3000, on_diag_field,      &s_ctx_iccid);
        at_session_send(d->at, "AT+CSQ",    3000, on_diag_field,      &s_ctx_csq);
        at_session_send(d->at, "AT+CEREG?", 3000, on_diag_field,      &s_ctx_cereg);
        at_session_send(d->at, "AT+COPS?",  3000, on_diag_last_field, &s_ctx_cops);

        fprintf(stderr, "diag_service: refresh started for dev %d\n", i);
    }
}
