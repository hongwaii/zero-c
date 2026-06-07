/**
 * @file serial_chan.c
 * @brief COM 串口 uv_tty_t 实现。
 *
 * 读路径：uv 回调 → 写入 ringbuf → 调 on_rx 通知上层（上层随后 ringbuf_read）。
 * 写路径：直接 uv_write（异步）。
 *
 * 适配 libuv 1.49：uv_tty_init 需要文件描述符，因此先用 CreateFileA
 * 打开 COM 口，转成 C runtime fd 后再交给 libuv；读用通用 uv_read_start。
 */
#include "serial_chan.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <uv.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RX_RING_CAP 4096

/* === URI 解析 === */

typedef struct {
    char name[16];
    int  baud;
    int  parity;    /* 0=N, 1=O, 2=E */
    int  stop_bits; /* 1 / 2 */
} serial_params_t;

/**
 * @brief 解析 com://COM7?baud=9600&parity=N&stop=1
 * @return AGENT_OK 成功，AGENT_ERR_BAD_ARG 格式错。
 */
static int parse_uri(const char *uri, serial_params_t *out)
{
    if (!uri || !out) return AGENT_ERR_BAD_ARG;
    const char *p = strstr(uri, "com://");
    if (!p) return AGENT_ERR_BAD_ARG;
    p += 6;  /* skip "com://" */
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
            if (strncmp(tok, "baud=", 5) == 0)          out->baud = atoi(tok + 5);
            else if (strncmp(tok, "parity=", 7) == 0)  out->parity = (tok[7] == 'O') ? 1 : (tok[7] == 'E') ? 2 : 0;
            else if (strncmp(tok, "stop=", 5) == 0)    out->stop_bits = atoi(tok + 5);
            tok = strtok(NULL, "&");
        }
    }
    return AGENT_OK;
}

/* === 读回调 === */

/**
 * @brief libuv 分配读缓冲回调。
 */
static void on_alloc(uv_handle_t *handle, size_t suggested, uv_buf_t *buf)
{
    (void)handle;
    /* 一次性分配大块，避免短包反复 alloc/free */
    buf->base = (char *)malloc(suggested > 0 ? suggested : 4096);
    buf->len  = (buf->base) ? (suggested > 0 ? suggested : 4096) : 0;
}

/**
 * @brief libuv 读回调：拿到字节后写进 ringbuf，然后通知上层 on_rx。
 */
static void on_tty_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
    serial_chan_t *sc = (serial_chan_t *)stream->data;
    if (nread < 0) {
        if (sc && sc->chan.on_rx) sc->chan.on_rx(sc->chan.userdata, NULL, 0);  /* 通知错误 */
        if (buf && buf->base) free(buf->base);
        return;
    }
    if (nread == 0 || !buf->base || !sc) {
        if (buf && buf->base) free(buf->base);
        return;
    }

    ringbuf_write(&sc->rx_ring, (const uint8_t *)buf->base, (size_t)nread);
    if (sc->chan.on_rx) {
        sc->chan.on_rx(sc->chan.userdata, (const uint8_t *)buf->base, (size_t)nread);
    }
    free(buf->base);
}

/**
 * @brief libuv 写完成回调：释放 uv_write_t 自身，错误打印到 stderr。
 */
static void on_tty_write(uv_write_t *req, int status)
{
    free(req);
    if (status != 0 && status != UV_ECANCELED) {
        fprintf(stderr, "serial_chan: uv_write status=%d (%s)\n",
                status, uv_strerror(status));
    }
}

static void on_tty_close(uv_handle_t *handle)
{
    (void)handle;
}

/* === 串口底层打开 === */

/**
 * @brief 用 Windows API 打开 COM 端口并把 DCB 配成指定参数。
 * @return 成功返回 C runtime fd（>= 0），失败返回 -1。
 */
static int open_com_port(const serial_params_t *p)
{
    char full_name[32];
    /* Windows 串口命名需带 "\\\\.\\" 前缀以避免 COM10+ 被截断 */
    snprintf(full_name, sizeof(full_name), "\\\\.\\%s", p->name);

    HANDLE h = CreateFileA(
        full_name,
        GENERIC_READ | GENERIC_WRITE,
        0, NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
        NULL);
    if (h == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "serial_chan: CreateFile(%s) 失败: %lu\n",
                full_name, GetLastError());
        return -1;
    }

    DCB dcb = {0};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(h, &dcb)) {
        CloseHandle(h);
        return -1;
    }
    dcb.BaudRate = (DWORD)p->baud;
    dcb.ByteSize = 8;
    dcb.Parity   = (BYTE)p->parity;
    dcb.StopBits = (BYTE)p->stop_bits;
    dcb.fBinary  = TRUE;
    dcb.fParity  = (p->parity != 0);
    if (!SetCommState(h, &dcb)) {
        CloseHandle(h);
        return -1;
    }

    /* 让 HANDLE 关联到 C runtime fd，再交给 libuv 接管。 */
    int fd = _open_osfhandle((intptr_t)h, _O_RDWR | _O_BINARY);
    if (fd < 0) {
        CloseHandle(h);
        return -1;
    }
    return fd;
}

/* === ops 实现 === */

int serial_chan_open(modem_chan_t *self, const char *uri)
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

    int fd = open_com_port(&p);
    if (fd < 0) {
        ringbuf_free(&sc->rx_ring);
        return AGENT_ERR_IO;
    }

    int r = uv_tty_init(sc->loop, &sc->tty, fd, 1);
    if (r != 0) {
        _close(fd);
        ringbuf_free(&sc->rx_ring);
        fprintf(stderr, "serial_chan: uv_tty_init(%s) 失败: %s\n",
                p.name, uv_strerror(r));
        return AGENT_ERR_IO;
    }
    sc->tty.data = sc;

    r = uv_read_start((uv_stream_t *)&sc->tty, on_alloc, on_tty_read);
    if (r != 0) {
        uv_close((uv_handle_t *)&sc->tty, on_tty_close);
        ringbuf_free(&sc->rx_ring);
        fprintf(stderr, "serial_chan: uv_read_start 失败: %s\n", uv_strerror(r));
        return AGENT_ERR_IO;
    }

    strncpy(self->uri, uri, sizeof(self->uri) - 1);
    self->uri[sizeof(self->uri) - 1] = '\0';
    self->is_open = true;

    fprintf(stderr, "serial_chan: 打开 %s @ %d 成功\n", p.name, sc->baud);
    return AGENT_OK;
}

int serial_chan_send(modem_chan_t *self, const uint8_t *buf, size_t len)
{
    if (!self || !self->impl || !buf || len == 0) return AGENT_ERR_BAD_ARG;
    if (!self->is_open) return AGENT_ERR_IO;
    serial_chan_t *sc = (serial_chan_t *)self->impl;

    uv_write_t *req = (uv_write_t *)malloc(sizeof(uv_write_t) + len);
    if (!req) return AGENT_ERR_OOM;
    uv_buf_t b = uv_buf_init((char *)(req + 1), (unsigned int)len);
    memcpy(req + 1, buf, len);
    int r = uv_write(req, (uv_stream_t *)&sc->tty, &b, 1, on_tty_write);
    if (r != 0) {
        free(req);
        return AGENT_ERR_IO;
    }
    return AGENT_OK;
}

void serial_chan_close(modem_chan_t *self)
{
    if (!self || !self->impl || !self->is_open) return;
    serial_chan_t *sc = (serial_chan_t *)self->impl;

    uv_read_stop((uv_stream_t *)&sc->tty);
    uv_close((uv_handle_t *)&sc->tty, on_tty_close);
    ringbuf_free(&sc->rx_ring);
    self->is_open = false;
    fprintf(stderr, "serial_chan: 关闭 %s\n", sc->name);
}

serial_chan_t *serial_chan_create(uv_loop_t *loop)
{
    if (!loop) return NULL;
    serial_chan_t *sc = (serial_chan_t *)calloc(1, sizeof(*sc));
    if (!sc) return NULL;
    sc->loop = loop;
    sc->chan.impl = sc;
    sc->chan.is_open = false;
    return sc;
}
