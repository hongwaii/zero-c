/**
 * @file serial_chan.h
 * @brief COM 串口 modem_chan_t 实现（基于 libuv uv_tty_t）。
 *
 * URI 格式：com://COM7?baud=9600&parity=N&stop=1
 */
#ifndef MIDWARE_SERIAL_CHAN_H
#define MIDWARE_SERIAL_CHAN_H

#include "agent_chan.h"
#include "agent_types.h"
#include "ringbuf.h"
#include <uv.h>

typedef struct {
    modem_chan_t  chan;       /* 基类，impl 强转时第一个字段 */
    uv_tty_t      tty;
    ringbuf_t     rx_ring;    /* HAL 内部 ringbuf：uv 回调写入，AT 引擎读 */
    char          name[16];   /* "COM7" */
    int           baud;
    uv_loop_t    *loop;       /* 弱引用 */
} serial_chan_t;

/**
 * @brief 在指定 loop 上创建串口通道。返回的指针在 close 后才能 free。
 */
serial_chan_t *serial_chan_create(uv_loop_t *loop);

/* modem_chan_ops_t 实现（直接当 modem_chan_t* 用） */
int  serial_chan_open (modem_chan_t *self, const char *uri);
int  serial_chan_send (modem_chan_t *self, const uint8_t *buf, size_t len);
void serial_chan_close(modem_chan_t *self);

#endif
