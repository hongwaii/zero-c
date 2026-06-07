/**
 * @file diag_state.c
 */
#include "diag_state.h"
#include <string.h>

void diag_state_reset(diag_state_t *s)
{
    if (!s) return;
    memset(s, 0, sizeof(*s));
    strncpy(s->last_update, "-", sizeof(s->last_update) - 1);

    /* P4 新增字段重置：
     * 数值类用 -1 表示"未测过"；UI 据此判断是否要隐藏结果区。 */
    s->last_ping_ms = -1;
    s->last_ping_ok = false;

    /* 字符串类 memset 已清空，OK；显式列出来便于阅读。 */
    s->last_ping_host[0]   = '\0';
    s->last_ping_port      = 0;
    s->last_ssl_url[0]     = '\0';
    s->last_ssl_status     = 0;
    s->last_ssl_issuer[0]  = '\0';
    s->last_ssl_expiry[0]  = '\0';
    s->last_sms_number[0]  = '\0';
    s->last_sms_status[0]  = '\0';
    s->last_log_path[0]    = '\0';
    s->last_log_size       = 0;
    s->last_health_report[0] = '\0';
}
