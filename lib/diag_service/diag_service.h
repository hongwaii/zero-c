/**
 * @file diag_service.h
 * @brief 诊断服务：订阅 device_manager，对 READY 设备拉取诊断值。
 */
#ifndef LIB_DIAG_SERVICE_H
#define LIB_DIAG_SERVICE_H

#include "device_manager.h"  /* 顺带把 agent_types.h 里 diag_service_t 前向声明带进来 */
#include "diag_state.h"

/* diag_service_t 由 agent_types.h 前向声明；本头不再重复定义 */

diag_service_t       *diag_service_create(uv_loop_t *loop, device_manager_t *dm);
void                  diag_service_destroy(diag_service_t *s);
void                  diag_service_refresh_now(diag_service_t *s);
const diag_state_t   *diag_service_get_state(diag_service_t *s, int dev_idx);

#endif
