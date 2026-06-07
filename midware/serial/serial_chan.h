/**
 * @file serial_chan.h
 * @brief COM 串口 modem_chan_t 实现（纯 Win32 + 工作线程 + uv_async）。
 *
 * **libuv 1.49 没有 uv_tty 之外的串口 API**——`uv_tty_t` 是给 Windows console 用的，
 * 在真 COM 端口 HANDLE 上调 GetNumberOfConsoleInputEvents 永远失败。
 *
 * P3 方案：
 *   - CreateFileA 同步开 COM 端口
 *   - 起一个工作线程，循环 ReadFile 阻塞读
 *   - 收到字节后写进 ringbuf + uv_async_send 唤醒主循环
 *   - 主循环的 async 回调从 ringbuf 读 + 调 chan->on_rx
 *
 * URI 格式：com://COM7?baud=9600&parity=N&stop=1
 */
#ifndef MIDWARE_SERIAL_CHAN_H
#define MIDWARE_SERIAL_CHAN_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>  /* HANDLE / DWORD / BOOL / BYTE / TRUE 都在 windef.h+winnt.h */

#include "agent_chan.h"
#include "agent_types.h"
#include "ringbuf.h"

struct uv_async_s;
struct uv_loop_s;
typedef struct uv_loop_s uv_loop_t;
typedef struct uv_async_s uv_async_t;

/**
 * @brief 串口通道实现结构体。
 *
 * 字段分区：
 *   - chan：基类（必须第一个字段，方便做向上/向下强转）
 *   - rx_ring：reader 线程写、main loop 读
 *   - name / baud：诊断输出 + 状态查询用
 *   - handle：Windows COM 端口 HANDLE
 *   - async / loop：libuv 异步通知与所属 loop 弱引用
 *   - thread / thread_id / stop_flag：reader 线程三件套
 */
typedef struct serial_chan {
    modem_chan_t     chan;         /* 基类，impl 强转时第一个字段 */
    ringbuf_t        rx_ring;      /* 接收 ringbuf：reader 线程写，main loop 读 */
    char             name[16];     /* "COM7" */
    int              baud;
    HANDLE           handle;       /* Windows COM 端口句柄 */
    uv_async_t      *async;        /* 字节到达通知 main loop */
    uv_loop_t       *loop;         /* 弱引用 */
    /* reader 线程 */
    HANDLE           thread;       /* Win32 thread handle */
    DWORD            thread_id;
    volatile bool    stop_flag;    /* 原子标志，close 时设为 true */
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
