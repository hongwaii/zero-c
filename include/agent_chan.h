/**
 * @file agent_chan.h
 * @brief 模组通道抽象：串口 / USB-NCM 等统一接口。
 *
 * 上层（AT 引擎 / DeviceManager）只通过 modem_chan_t 操作模组，
 * 不直接接触 uv_tty / socket 等底层细节。
 *
 * 头文件带 extern "C" 守卫——同时被 C TU (serial_chan.c) 和 C++ TU (host.cpp) 包含。
 */
#ifndef AGENT_CHAN_H
#define AGENT_CHAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "agent_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明：HAL 内部用 libuv，公共头不引 uv.h 以避免传递依赖膨胀 */
struct uv_loop_s;

/* 前向 typedef：vtable 字段需要 modem_chan_t *，但完整 struct 还没出来 */
typedef struct modem_chan modem_chan_t;

/**
 * @brief HAL 收到字节时回调。buf/len 由回调内部生存期。
 * @param userdata 注册时传入。
 * @param buf      收到的字节（只读，引用 HAL 内部 ringbuf）。
 * @param len      字节数。
 */
typedef void (*modem_chan_rx_fn)(void *userdata, const uint8_t *buf, size_t len);

/**
 * @brief HAL 通道操作虚表（open / send / close）。
 */
typedef struct {
    int  (*open) (modem_chan_t *self, const char *uri);
    int  (*send) (modem_chan_t *self, const uint8_t *buf, size_t len);
    void (*close)(modem_chan_t *self);
} modem_chan_ops_t;

/**
 * @brief 通道基类。impl 是具体实现（serial_chan / ncm_chan）的私有数据。
 */
typedef struct modem_chan {
    const modem_chan_ops_t *ops;
    void                   *impl;
    modem_chan_rx_fn        on_rx;
    void                   *userdata;
    bool                    is_open;
    char                    uri[256];  /* "com://COM7?baud=9600" / "rndis://name" */
} modem_chan_t;

/**
 * @brief 便利函数：通过虚表 send 发数据。
 * @return 0 成功；负错误码。
 *
 * 当前实现：直接走 ops->send，ops 不可用则返回 AGENT_ERR_IO。
 * 备注：serial_chan.c 显式把 ops 置 NULL（走 serial_chan_send 直接调），
 * 后续若要把所有 chan 都收回虚表，需要在 serial_chan_create 里赋
 * ops = &serial_chan_ops，并把 at_session 改为统一走虚表。
 */
static inline int modem_chan_send(modem_chan_t *c, const uint8_t *buf, size_t len)
{
    if (!c || !c->ops || !c->ops->send) return AGENT_ERR_IO;
    return c->ops->send(c, buf, len);
}

#ifdef __cplusplus
}
#endif

#endif
