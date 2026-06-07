/**
 * @file diag_state.h
 * @brief 单台模组的诊断状态快照。
 */
#ifndef LIB_DIAG_SERVICE_DIAG_STATE_H
#define LIB_DIAG_SERVICE_DIAG_STATE_H

#include <stdbool.h>

typedef struct {
    char  csq[16];
    char  cereg[32];
    char  cop_operator[32];
    char  rat[16];
    char  imei[16];
    char  imsi[16];
    char  iccid[24];
    char  last_update[32];
    bool  valid;
} diag_state_t;

void diag_state_reset(diag_state_t *s);

#endif
