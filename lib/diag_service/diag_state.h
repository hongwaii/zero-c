/**
 * @file diag_state.h
 * @brief 单台模组的诊断状态快照。
 */
#ifndef LIB_DIAG_SERVICE_DIAG_STATE_H
#define LIB_DIAG_SERVICE_DIAG_STATE_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    /* P3 已有：基础 AT 状态 */
    char  csq[16];
    char  cereg[32];
    char  cop_operator[32];
    char  rat[16];
    char  imei[16];
    char  imsi[16];
    char  iccid[24];
    char  last_update[32];
    bool  valid;

    /* === P4 新增：诊断操作结果 ===
     * 约定：last_xxx 是"最近一次操作"的结果；UI 直接读。
     * 数值字段默认值 = -1 / 0 表示"未测过"。 */

    /* TCP/UDP ping */
    char  last_ping_host[64];
    int   last_ping_port;
    int   last_ping_ms;       /* -1 = 未测 */
    bool  last_ping_ok;

    /* SSL probe */
    char  last_ssl_url[256];
    int   last_ssl_status;    /* HTTP status, 0 = 未测 */
    char  last_ssl_issuer[128];
    char  last_ssl_expiry[32];

    /* SMS */
    char  last_sms_number[32];
    char  last_sms_status[32]; /* "OK" / "ERROR: xxx" / 空 */

    /* Log capture */
    char  last_log_path[512];
    size_t last_log_size;

    /* Health check report path */
    char  last_health_report[512];
} diag_state_t;

void diag_state_reset(diag_state_t *s);

#endif
