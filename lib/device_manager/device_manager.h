/**
 * @file device_manager.h
 * @brief 多模组 manager：热插拔扫描 + 设备列表 + 变更通知。
 *
 * P2 范围：链路层 only——不打开通道（chan 字段保持 NULL），P3 加 AT 引擎后
 * 设备切到 READY 才分配 serial_chan_t 并 open。
 */
#ifndef LIB_DEVICE_MANAGER_H
#define LIB_DEVICE_MANAGER_H

#include "agent_types.h"

#define DEV_MANAGER_MAX_DEVS 16

typedef enum {
    DEV_STATE_DISCONNECTED = 0,  /* 链路层发现，未打开通道 */
    DEV_STATE_READY,              /* 通道已打开 */
    DEV_STATE_ERROR
} dev_state_t;

typedef struct modem_dev {
    char          id[32];
    char          label[64];
    char          chan_uri[256];
    char          ipv4[16];
    dev_state_t   state;
    int           csq;        /* 0~31；P2 留 0，P3 填 */
} modem_dev_t;

typedef struct {
    modem_dev_t devs[DEV_MANAGER_MAX_DEVS];
    int         count;
} dev_list_snapshot_t;

struct uv_loop_s;
typedef struct uv_loop_s uv_loop_t;
struct uv_timer_s;
typedef struct uv_timer_s uv_timer_t;

typedef void (*dev_change_fn)(void *userdata, const dev_list_snapshot_t *snapshot);

/* 给 struct 命名（tag）以允许 agent_types.h 里前向声明 struct device_manager; */
typedef struct device_manager device_manager_t;
struct device_manager {
    uv_loop_t           *loop;  /* 弱引用 */
    modem_dev_t          devs[DEV_MANAGER_MAX_DEVS];
    int                  dev_count;
    /* scan_timer 实际类型需要完整定义——存指针，避开在公共头里 include <uv.h> */
    uv_timer_t          *scan_timer;
    dev_change_fn        on_change;
    void                *userdata;
    /* 上次"原始快照"用于 diff */
    char                 prev_serial[64][8];
    int                  prev_serial_count;
    char                 prev_ncm[64][128];
    int                  prev_ncm_count;
};

int  device_manager_init       (device_manager_t *m, uv_loop_t *loop);
int  device_manager_start      (device_manager_t *m);
void device_manager_stop       (device_manager_t *m);
int  device_manager_force_scan (device_manager_t *m);
void device_manager_set_callback(device_manager_t *m, dev_change_fn fn, void *userdata);

#endif
