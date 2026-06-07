/**
 * @file serial_chan.c
 * @brief COM 串口 Win32 + 工作线程实现。
 *
 * 设计：
 *   - 主线程（libuv loop）：跑 on_async_wake 回调，从 ringbuf 读 + 调 chan->on_rx
 *   - reader 线程（每设备一）：循环 ReadFile 阻塞读，收到字节后 ringbuf_write + uv_async_send
 *   - close 流程：set stop_flag → CancelIo 让 ReadFile 立刻返回 → WaitForSingleObject 等线程退出
 *
 * 为什么不用 uv_tty：libuv 1.49 的 uv_tty_t 调的是 console API（GetConsoleScreenBufferInfo
 * 等），在真 COM 端口 HANDLE 上永远失败，会报 UV_EBADF。
 */
#include "serial_chan.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <uv.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>  /* _beginthreadex */

#define RX_RING_CAP   4096
#define READ_BUF_CAP  1024

/* === URI 解析 === */

/**
 * @brief 串口 URI 解析结果。
 */
typedef struct {
    char name[16];
    int  baud;
    int  parity;     /* 0=N, 1=O, 2=E */
    int  stop_bits;  /* 1 / 2 */
} serial_params_t;

/**
 * @brief 解析 com://COM7?baud=9600&parity=N&stop=1
 * @return 0 成功，AGENT_ERR_BAD_ARG 格式错。
 */
static int parse_uri(const char *uri, serial_params_t *out)
{
    if (!uri || !out) return AGENT_ERR_BAD_ARG;
    const char *p = strstr(uri, "com://");
    if (!p) return AGENT_ERR_BAD_ARG;
    p += 6;
    const char *q = strchr(p, '?');
    size_t name_len = q ? (size_t)(q - p) : strlen(p);
    if (name_len == 0 || name_len >= sizeof(out->name)) return AGENT_ERR_BAD_ARG;
    memcpy(out->name, p, name_len);
    out->name[name_len] = '\0';

    /* 默认 115200 8N1 */
    out->baud = 115200;
    out->parity = 0;
    out->stop_bits = 1;

    if (q) {
        q++;
        char buf[128];
        size_t bl = 0;
        while (*q && bl < sizeof(buf) - 1) buf[bl++] = *q++;
        buf[bl] = '\0';
        char *tok = strtok(buf, "&");
        while (tok) {
            if (strncmp(tok, "baud=", 5) == 0) {
                out->baud = atoi(tok + 5);
            } else if (strncmp(tok, "parity=", 7) == 0) {
                out->parity = (tok[7] == 'O') ? 1 : (tok[7] == 'E') ? 2 : 0;
            } else if (strncmp(tok, "stop=", 5) == 0) {
                out->stop_bits = atoi(tok + 5);
            }
            tok = strtok(NULL, "&");
        }
    }
    return AGENT_OK;
}

/* === 工具 === */

/**
 * @brief 打开 COM 端口（HANDLE），配置 DCB。
 * @return 成功返回 HANDLE；失败返回 INVALID_HANDLE_VALUE。
 */
static HANDLE open_com_port_handle(const serial_params_t *p)
{
    char full_name[32];
    /* Windows 串口需带 "\\\\.\\" 前缀以避免 COM10+ 被截断 */
    snprintf(full_name, sizeof(full_name), "\\\\.\\%s", p->name);

    /* CreateFile 带重试：ERROR_BUSY/ERROR_ACCESS_DENIED 多半是 kernel 还在清理前一个 handle
     * （前一会话 Close 之后 kernel 释放需要数百 ms——Sleep(200) 兜底不一定够）。
     * 重试 5 次 × 200ms ≈ 1s 上限，覆盖大多数情况。 */
    HANDLE h = INVALID_HANDLE_VALUE;
    for (int retry = 0; retry < 5; retry++) {
        /* 同步 I/O（无 FILE_FLAG_OVERLAPPED）—— 工作线程用 ReadFile 阻塞读 */
        h = CreateFileA(
            full_name,
            GENERIC_READ | GENERIC_WRITE,
            0, NULL, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
        if (h != INVALID_HANDLE_VALUE) break;

        DWORD err = GetLastError();
        if (err != ERROR_BUSY && err != ERROR_ACCESS_DENIED) {
            /* 非"忙/拒绝访问"类错误——重试无意义，直接退出循环 */
            break;
        }
        if (retry < 4) {
            fprintf(stderr,
                "serial_chan: CreateFile(%s) busy (err=%lu), retry %d/5 in 200ms\n",
                full_name, err, retry + 1);
            Sleep(200);
        }
    }
    if (h == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "serial_chan: CreateFile(%s) failed: %lu (after 5 retries)\n",
                full_name, GetLastError());
        return INVALID_HANDLE_VALUE;
    }

    DCB dcb = {0};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(h, &dcb)) {
        fprintf(stderr, "serial_chan: GetCommState(%s) failed: %lu\n",
                p->name, GetLastError());
        CloseHandle(h);
        return INVALID_HANDLE_VALUE;
    }
    dcb.BaudRate = (DWORD)p->baud;
    dcb.ByteSize = 8;
    dcb.Parity   = (BYTE)p->parity;
    dcb.StopBits = (BYTE)p->stop_bits;
    dcb.fBinary  = TRUE;
    dcb.fParity  = (p->parity != 0);
    if (!SetCommState(h, &dcb)) {
        fprintf(stderr, "serial_chan: SetCommState(%s) failed: %lu\n",
                p->name, GetLastError());
        CloseHandle(h);
        return INVALID_HANDLE_VALUE;
    }

    /* 串口超时配置：
     *   ReadIntervalTimeout: 字节间最大间隔 20ms（AT 命令响应字节都连着来）
     *   ReadTotalTimeoutMultiplier=0: 0 表示不按字节数累计——ReadTotalTimeout
     *                              = 0*N + Constant = Constant，与请求 buffer
     *                              大小无关（避免之前 10*1024=10 秒卡顿）
     *   ReadTotalTimeoutConstant=100: 总超时 100ms——AT 命令毫秒级响应足够
     *
     * 历史 bug：之前 Multi=10、Constant=100、READ_BUF_CAP=1024 时
     *   ReadTotalTimeout = 10*1024 + 100 = 10340ms
     * 即使模组立刻回 "AT\r\nOK\r\n"（6 字节），ReadFile 也要傻等 ~10s 凑够 1024 字节
     * 才返回，导致用户发 AT 后 UI 卡 10+ 秒。
     *
     * Multi=0 是 Windows 串口"快速轮询"的标准配置。 */
    COMMTIMEOUTS ct = {0};
    ct.ReadIntervalTimeout = 20;
    ct.ReadTotalTimeoutMultiplier = 0;
    ct.ReadTotalTimeoutConstant = 100;
    SetCommTimeouts(h, &ct);

    return h;
}

/* === reader 线程 === */

/**
 * @brief 工作线程主函数：循环 ReadFile，收到字节后写 ringbuf + uv_async_send 通知。
 *
 * 退出条件：stop_flag = true（serial_chan_close 设）。
 */
static unsigned __stdcall reader_thread(void *arg)
{
    serial_chan_t *sc = (serial_chan_t *)arg;
    uint8_t buf[READ_BUF_CAP];
    DWORD got = 0;

    while (!sc->stop_flag) {
        BOOL ok = ReadFile(sc->handle, buf, sizeof(buf), &got, NULL);
        if (!ok) {
            DWORD err = GetLastError();
            /* ERROR_OPERATION_ABORTED 在 CancelIo 关闭时常见——非致命 */
            if (err == ERROR_OPERATION_ABORTED || err == ERROR_INVALID_HANDLE) {
                break;
            }
            fprintf(stderr, "serial_chan[%s]: ReadFile failed: %lu\n",
                    sc->name, err);
            break;
        }
        if (got > 0) {
            ringbuf_write(&sc->rx_ring, buf, got);
            if (sc->async) uv_async_send(sc->async);
        }
        /* got == 0 罕见，正常情况没数据会阻塞在 ReadFile 上等超时 */
    }
    fprintf(stderr, "serial_chan[%s]: reader thread exiting\n", sc->name);
    return 0;
}

/* === async 回调（main loop 线程跑） === */

/**
 * @brief uv_async_t 唤醒回调：把 ringbuf 里累积的字节一次性调 on_rx 上抛给 AT 引擎。
 */
static void on_async_wake(uv_async_t *handle)
{
    serial_chan_t *sc = (serial_chan_t *)handle->data;
    if (!sc || !sc->chan.on_rx) return;

    /* 一次性把所有待发字节拷到临时缓冲，调 on_rx */
    uint8_t buf[READ_BUF_CAP];
    size_t n = ringbuf_read(&sc->rx_ring, buf, sizeof(buf));
    if (n > 0) {
        sc->chan.on_rx(sc->chan.userdata, buf, n);
    }
}

/* === ops 实现 === */

/**
 * @brief 打开串口并启动 reader 线程 + uv_async。
 * @return AGENT_OK 成功；负错误码。
 */
int serial_chan_open_impl(modem_chan_t *self, const char *uri)
{
    if (!self || !self->impl || !uri) return AGENT_ERR_BAD_ARG;
    serial_chan_t *sc = (serial_chan_t *)self->impl;
    if (self->is_open) return AGENT_ERR_BAD_ARG;

    serial_params_t p;
    int rc = parse_uri(uri, &p);
    if (rc != AGENT_OK) return rc;

    strncpy(sc->name, p.name, sizeof(sc->name) - 1);
    sc->name[sizeof(sc->name) - 1] = '\0';
    sc->baud = p.baud;

    if (ringbuf_init(&sc->rx_ring, RX_RING_CAP) != AGENT_OK) {
        return AGENT_ERR_OOM;
    }

    /* 分配 + init uv_async（在 worker 线程启动前完成，async 必须是 main loop 线程创建） */
    sc->async = (uv_async_t *)calloc(1, sizeof(uv_async_t));
    if (!sc->async) {
        ringbuf_free(&sc->rx_ring);
        return AGENT_ERR_OOM;
    }
    int r = uv_async_init(sc->loop, sc->async, on_async_wake);
    if (r != 0) {
        free(sc->async);
        sc->async = NULL;
        ringbuf_free(&sc->rx_ring);
        fprintf(stderr, "serial_chan: uv_async_init failed: %s\n", uv_strerror(r));
        return AGENT_ERR_IO;
    }
    sc->async->data = sc;

    /* 打开 COM 端口 */
    sc->handle = open_com_port_handle(&p);
    if (sc->handle == INVALID_HANDLE_VALUE) {
        uv_close((uv_handle_t *)sc->async, NULL);
        free(sc->async);
        sc->async = NULL;
        ringbuf_free(&sc->rx_ring);
        return AGENT_ERR_IO;
    }

    /* 启动 reader 线程 */
    sc->stop_flag = false;
    sc->thread = (HANDLE)_beginthreadex(
        NULL, 0, reader_thread, sc, 0, (unsigned *)&sc->thread_id);
    if (sc->thread == NULL) {
        CloseHandle(sc->handle);
        sc->handle = INVALID_HANDLE_VALUE;
        uv_close((uv_handle_t *)sc->async, NULL);
        free(sc->async);
        sc->async = NULL;
        ringbuf_free(&sc->rx_ring);
        fprintf(stderr, "serial_chan: _beginthreadex failed\n");
        return AGENT_ERR_OOM;
    }

    strncpy(self->uri, uri, sizeof(self->uri) - 1);
    self->uri[sizeof(self->uri) - 1] = '\0';
    self->is_open = true;

    fprintf(stderr, "serial_chan: opened %s @ %d (thread %lu)\n",
            p.name, sc->baud, sc->thread_id);
    return AGENT_OK;
}

/**
 * @brief 同步 WriteFile 发数据。
 * @return AGENT_OK 成功；负错误码。
 */
int serial_chan_send_impl(modem_chan_t *self, const uint8_t *buf, size_t len)
{
    if (!self || !self->impl || !buf || len == 0) return AGENT_ERR_BAD_ARG;
    if (!self->is_open) return AGENT_ERR_IO;
    serial_chan_t *sc = (serial_chan_t *)self->impl;

    /* WriteFile 同步写（带超时） */
    DWORD wrote = 0;
    BOOL ok = WriteFile(sc->handle, buf, (DWORD)len, &wrote, NULL);
    if (!ok) {
        fprintf(stderr, "serial_chan[%s]: WriteFile failed: %lu\n",
                sc->name, GetLastError());
        return AGENT_ERR_IO;
    }
    if ((size_t)wrote != len) {
        fprintf(stderr, "serial_chan[%s]: WriteFile short: %lu/%zu\n",
                sc->name, wrote, len);
        return AGENT_ERR_IO;
    }
    return AGENT_OK;
}

/**
 * @brief 关闭串口、回收 reader 线程、释放 uv_async 与 ringbuf。
 *
 * 7 步顺序：stop_flag → CancelSynchronousIo → 等线程 → 关 HANDLE → 关 async → 释放 ringbuf → Sleep 200ms。
 * 必须先取消挂起 I/O 再 WaitForSingleObject，否则 reader 线程会一直阻塞在 ReadFile。
 *
 * 关键改动：
 *   - 用 CancelSynchronousIo(thread) 替代 CancelIo(handle)：前者专门用来在另一个线程
 *     里取消目标线程的同步 I/O（Win Vista+），比 CancelIo 更彻底。
 *   - Sleep(200) 给 kernel 时间释放 in-flight ReadFile 的句柄引用，避免立即重连时
 *     ERROR_BUSY=170。
 */
void serial_chan_close_impl(modem_chan_t *self)
{
    if (!self || !self->impl || !self->is_open) return;
    serial_chan_t *sc = (serial_chan_t *)self->impl;

    /* 1) 通知 reader 线程停止 + 取消挂起的 ReadFile。
     *    CancelSynchronousIo 专门用来在另一个线程里取消目标线程当前正在做的同步 I/O
     *    （Win Vista+）。比 CancelIo 更彻底——它会等到 ReadFile 返回才让 CancelIoEx
     *    真正完成。 */
    sc->stop_flag = true;
    if (sc->thread) {
        CancelSynchronousIo(sc->thread);   /* 针对 reader 线程句柄 */
    } else if (sc->handle != INVALID_HANDLE_VALUE) {
        CancelIo(sc->handle);
    }

    /* 2) 等 reader 线程退出（最多 2s） */
    if (sc->thread) {
        DWORD wait_rc = WaitForSingleObject(sc->thread, 2000);
        if (wait_rc == WAIT_TIMEOUT) {
            fprintf(stderr,
                    "serial_chan[%s]: reader thread did not exit in 2s, terminating\n",
                    sc->name);
            TerminateThread(sc->thread, 1);
        }
        CloseHandle(sc->thread);
        sc->thread = NULL;
    }

    /* 3) 关串口 HANDLE */
    if (sc->handle != INVALID_HANDLE_VALUE) {
        CloseHandle(sc->handle);
        sc->handle = INVALID_HANDLE_VALUE;
    }

    /* 4) 等 kernel 释放句柄引用（200ms 兜底，避免立即重连时 ERROR_BUSY=170）。
     *    CloseHandle 不保证 in-flight ReadFile 的句柄引用立即归零——kernel 串口驱动
     *    独占模式下需要短暂时间清理。200ms 同步 Sleep 可接受因为 close 路径用户感知不到。 */
    Sleep(200);

    /* 5) 关闭 + 释放 uv_async */
    if (sc->async) {
        uv_close((uv_handle_t *)sc->async, NULL);
        free(sc->async);
        sc->async = NULL;
    }

    /* 6) 释放 ringbuf */
    ringbuf_free(&sc->rx_ring);

    /* 7) 解除 chan 关联 */
    self->is_open = false;
    sc->chan.on_rx = NULL;
    sc->chan.userdata = NULL;
    fprintf(stderr, "serial_chan: closed %s\n", sc->name);
}

/* === ops 虚表（at_session 通过 modem_chan_send -> ops->send 调到我们） === */
int  serial_chan_open_impl(modem_chan_t *self, const char *uri);
int  serial_chan_send_impl(modem_chan_t *self, const uint8_t *buf, size_t len);
void serial_chan_close_impl(modem_chan_t *self);

static const modem_chan_ops_t s_serial_ops = {
    .open  = serial_chan_open_impl,
    .send  = serial_chan_send_impl,
    .close = serial_chan_close_impl,
};

/**
 * @brief 分配 + 零初始化 serial_chan_t。
 * @param loop libuv 主循环（弱引用，串口 close 前 loop 不能销毁）。
 * @return 成功返回指针；失败返回 NULL。
 */
serial_chan_t *serial_chan_create(uv_loop_t *loop)
{
    if (!loop) return NULL;
    serial_chan_t *sc = (serial_chan_t *)calloc(1, sizeof(*sc));
    if (!sc) return NULL;
    sc->loop = loop;
    sc->chan.impl = sc;
    sc->chan.ops = &s_serial_ops;  /* 新加：vtable 让 modem_chan_send 找到我们的 send */
    sc->handle = INVALID_HANDLE_VALUE;
    sc->chan.is_open = false;
    return sc;
}

/* === 公共 API 包装（device_manager.c 等直接调用者用） === */

/* 公共头声明的旧名透传到 _impl，避免破坏外部调用方链接。 */
int  serial_chan_open (modem_chan_t *self, const char *uri)
{ return serial_chan_open_impl(self, uri); }

int  serial_chan_send (modem_chan_t *self, const uint8_t *buf, size_t len)
{ return serial_chan_send_impl(self, buf, len); }

void serial_chan_close(modem_chan_t *self)
{ serial_chan_close_impl(self); }
