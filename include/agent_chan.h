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

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明：HAL 内部用 libuv，公共头不引 uv.h 以避免传递依赖膨胀 */
struct uv_loop_s;

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
    int  (*open) (struct modem_chan *self, const char *uri);
    int  (*send) (struct modem_chan *self, const uint8_t *buf, size_t len);
    void (*close)(struct modem_chan *self);
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

#ifdef __cplusplus
}
#endif

#endif
