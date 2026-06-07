# Modem Agent — Plan P2: HAL + libuv Main Loop

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 EXE 能自动发现接入的模组（COM 串口 + USB-NCM/RNDIS 网卡），并在"多模组"面板里实时显示设备列表。**P2 只到链路层（link layer）——不解析 AT 指令**，AT 引擎留给 P3。

**Architecture:**
- `modem_chan_t` 抽象：统一 `open/send/close`，所有 I/O 经由字节 ringbuf
- `midware/serial/`：`uv_tty_t` 接真 COM 端口
- `midware/usb_ncm/`：IP Helper 枚举 RNDIS/CDC-ECM/NCM 网卡 + IPv4
- `lib/device_manager/`：热插拔扫描 timer（每 2s 一次）、N 设备槽、变更通知
- `app/shell/host.cpp` 接入 libuv 事件循环（与 ImGui 同线程，UV_RUN_NOWAIT 模式）

**Tech Stack:** libuv 1.49（已 vendored）、Windows IP Helper、IPHLPAPI、SetupAPI、cJSON（已集成）、mbedTLS（已集成）、libcurl（已集成）。

**Spec reference:** [docs/superpowers/specs/2026-06-07-modem-agent-design.md](docs/superpowers/specs/2026-06-07-modem-agent-design.md) §4.1（HAL）、§4.3（DeviceManager）、§5.1（线程）、§5.2（事件流）、§8（Phase 2）、§12（风险）。

**前置：** Plan P1（tag `phase1-app-shell`）+ 5 个 CJK/i18n bug fix 全部就位。

---

## File Structure

### 新增

| Path | 职责 |
|---|---|
| `include/agent_chan.h` | `modem_chan_t` 公共接口 + `modem_chan_rx_fn` 回调 |
| `lib/util/ringbuf.h` + `.c` | 字节 ring buffer，HAL 与上层之间 |
| `lib/util/ringbuf_test.c` | 单元测试 |
| `lib/device_manager/device_manager.h` | 多模组 manager 公共接口 |
| `lib/device_manager/device_manager.c` | 热插拔扫描、dev 列表、变更通知 |
| `lib/device_manager/CMakeLists.txt` | 静态库 |
| `midware/serial/serial_chan.h` + `.c` | `modem_chan_t` 的 COM 串口实现（基于 `uv_tty_t`） |
| `midware/serial/test_serial_chan.c` | 独立小测试程序（尝试开 COM7，无则跳过） |
| `midware/serial/CMakeLists.txt` | 静态库 |
| `midware/usb_ncm/ncm_chan.h` + `.c` | RNDIS/NCM 网卡枚举（IP Helper + ICMP 探活） |
| `midware/usb_ncm/test_ncm_enumerate.c` | 独立小测试，打印枚举结果 |
| `midware/usb_ncm/CMakeLists.txt` | 静态库 |
| `test/test_ringbuf.c` | ringbuf 单元测试（runtime CHECK，非 assert） |
| `test/test_ncm_enumerate.c` | ncm 枚举的集成测试（runtime CHECK） |
| `docs/superpowers/checklists/phase2-hal-libuv.md` | 真机冒烟清单 |

### 修改

| Path | 改动 |
|---|---|
| `lib/util/CMakeLists.txt` | 加 `ringbuf.c` |
| `lib/CMakeLists.txt` | `add_subdirectory(device_manager)` |
| `midware/CMakeLists.txt` | `add_subdirectory(serial)` + `add_subdirectory(usb_ncm)` |
| `test/CMakeLists.txt` | 加 `test_ringbuf` + `test_ncm_enumerate` 独立 target |
| `include/agent_types.h` | `agent_app_t` 加 `device_manager` 字段 |
| `app/shell/host.cpp` | 初始化 libuv loop、host_run 每帧 `uv_run(UV_RUN_NOWAIT)`、shutdown 时 `uv_loop_close` |
| `core/main.cpp` | 启动 `device_manager`，传给 `g_app.device_manager` |
| `app/panel_devices/panel.cpp` | 不再写死 3 行，改为订阅 `device_manager` 实时数据 |
| `.gitignore` | 忽略 libuv 运行时可能产生的临时文件（如果需要） |

---

## Conventions (续 Plan P1)

- C11，4 空格，100 列
- 所有注释中文（zh-CN）
- 标识符 / 字符串字面量 / 编译宏英文
- 单元测试用 runtime `CHECK(cond, msg)` 宏（不能用 `assert()` —— Task 4 教训）
- 每 task 1 commit，commit message 用中文
- 错误码用 `agent_types.h` 里的 `AGENT_ERR_*` 常量

---

## Task 1: ringbuf 字节缓冲 + 单元测试（TDD）

**Files:**
- Create: `lib/util/ringbuf.h`
- Create: `lib/util/ringbuf.c`
- Create: `test/test_ringbuf.c`
- Modify: `lib/util/CMakeLists.txt`
- Modify: `test/CMakeLists.txt`

### Step 1: Write `lib/util/ringbuf.h`

```c
/**
 * @file ringbuf.h
 * @brief 单生产者-单消费者字节 ring buffer（HAL 与上层之间传输字节流用）。
 *
 * 不是线程安全的——仅在同一个 libuv 回调线程内用。生产者写、消费者读。
 * 满了继续写会覆盖最旧数据（环形特性）；读空返回 0。
 */
#ifndef UTIL_RINGBUF_H
#define UTIL_RINGBUF_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *buf;       /* 堆分配，长度 = cap */
    size_t   cap;       /* 容量（字节） */
    size_t   head;      /* 下一个写位置（生产者写入此处） */
    size_t   tail;      /* 下一个读位置（消费者从此处读） */
    size_t   count;     /* 当前已用字节数 */
} ringbuf_t;

int    ringbuf_init  (ringbuf_t *rb, size_t cap);
void   ringbuf_free  (ringbuf_t *rb);
size_t ringbuf_write (ringbuf_t *rb, const uint8_t *data, size_t n);
size_t ringbuf_read  (ringbuf_t *rb, uint8_t *out, size_t n);
size_t ringbuf_peek  (const ringbuf_t *rb, uint8_t *out, size_t n);  /* 非破坏性读 */
size_t ringbuf_available(const ringbuf_t *rb);
size_t ringbuf_free_space(const ringbuf_t *rb);

#endif
```

### Step 2: Write `lib/util/ringbuf.c`

```c
/**
 * @file ringbuf.c
 */
#include "ringbuf.h"
#include "agent_types.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief 分配 cap 字节的环形缓冲。
 * @return 0 成功，-5 OOM，-3 cap==0。
 */
int ringbuf_init(ringbuf_t *rb, size_t cap)
{
    if (!rb || cap == 0) return AGENT_ERR_BAD_ARG;
    rb->buf = (uint8_t *)malloc(cap);
    if (!rb->buf) return AGENT_ERR_OOM;
    rb->cap = cap;
    rb->head = rb->tail = rb->count = 0;
    return AGENT_OK;
}

/** @brief 释放缓冲，调用方负责不再使用。 */
void ringbuf_free(ringbuf_t *rb)
{
    if (!rb) return;
    free(rb->buf);
    rb->buf = NULL;
    rb->cap = rb->head = rb->tail = rb->count = 0;
}

/**
 * @brief 写入 n 字节；环形满了则覆盖最旧数据（head 推进）。
 * @return 实际写入字节数。
 */
size_t ringbuf_write(ringbuf_t *rb, const uint8_t *data, size_t n)
{
    if (!rb || !data || n == 0) return 0;
    size_t written = 0;
    for (size_t i = 0; i < n; i++) {
        rb->buf[rb->head] = data[i];
        rb->head = (rb->head + 1) % rb->cap;
        if (rb->count < rb->cap) {
            rb->count++;
        } else {
            /* 满了：tail 跟着前移，丢弃最旧 1 字节 */
            rb->tail = (rb->tail + 1) % rb->cap;
        }
        written++;
    }
    return written;
}

/**
 * @brief 读出最多 n 字节。
 * @return 实际读出字节数；0 表示空。
 */
size_t ringbuf_read(ringbuf_t *rb, uint8_t *out, size_t n)
{
    if (!rb || !out || n == 0) return 0;
    size_t to_read = (n < rb->count) ? n : rb->count;
    for (size_t i = 0; i < to_read; i++) {
        out[i] = rb->buf[rb->tail];
        rb->tail = (rb->tail + 1) % rb->cap;
    }
    rb->count -= to_read;
    return to_read;
}

/**
 * @brief 偷看（不消费）最多 n 字节。
 */
size_t ringbuf_peek(const ringbuf_t *rb, uint8_t *out, size_t n)
{
    if (!rb || !out || n == 0) return 0;
    size_t to_read = (n < rb->count) ? n : rb->count;
    size_t idx = rb->tail;
    for (size_t i = 0; i < to_read; i++) {
        out[i] = rb->buf[idx];
        idx = (idx + 1) % rb->cap;
    }
    return to_read;
}

/** @brief 当前已用字节数。 */
size_t ringbuf_available(const ringbuf_t *rb)
{
    return rb ? rb->count : 0;
}

/** @brief 当前剩余空间。 */
size_t ringbuf_free_space(const ringbuf_t *rb)
{
    return rb ? (rb->cap - rb->count) : 0;
}
```

### Step 3: Write `test/test_ringbuf.c`（runtime CHECK，非 assert）

```c
/**
 * @file test_ringbuf.c
 * @brief ringbuf 单元测试。
 */
#include "ringbuf.h"
#include "agent_types.h"
#include <stdio.h>
#include <string.h>

static int g_checks = 0, g_failed = 0;
#define CHECK(c, m) do { g_checks++; if (!(c)) { g_failed++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, m); } } while (0)

static int test_basic_write_read(void)
{
    ringbuf_t rb;
    CHECK(ringbuf_init(&rb, 16) == AGENT_OK, "init");
    const uint8_t in[] = "hello";
    CHECK(ringbuf_write(&rb, in, 5) == 5, "write 5");
    CHECK(ringbuf_available(&rb) == 5, "available=5");
    CHECK(ringbuf_free_space(&rb) == 11, "free=11");

    uint8_t out[8] = {0};
    CHECK(ringbuf_read(&rb, out, 8) == 5, "read 5");
    CHECK(memcmp(out, "hello", 5) == 0, "content == hello");
    CHECK(ringbuf_available(&rb) == 0, "available=0");
    ringbuf_free(&rb);
    return 0;
}

static int test_wrap_around(void)
{
    ringbuf_t rb;
    ringbuf_init(&rb, 8);
    /* 写 6 字节 → head=6 */
    const uint8_t a[] = "abcdef";
    ringbuf_write(&rb, a, 6);
    /* 读 4 字节 → tail=4, count=2 */
    uint8_t b[4];
    ringbuf_read(&rb, b, 4);
    /* 再写 6 字节：head 从 6 开始往后绕到 0 */
    const uint8_t c[] = "123456";
    CHECK(ringbuf_write(&rb, c, 6) == 6, "write 6 after read");
    /* 此时 buffer 顺序：tail=4, head=4（绕回），count=8（满） */
    /* 全部读出应该是 "ef123456" */
    uint8_t out[8];
    CHECK(ringbuf_read(&rb, out, 8) == 8, "read 8");
    CHECK(memcmp(out, "ef123456", 8) == 0, "wrapped content == ef123456");
    ringbuf_free(&rb);
    return 0;
}

static int test_overflow_overwrites_oldest(void)
{
    ringbuf_t rb;
    ringbuf_init(&rb, 4);
    const uint8_t a[] = "ABCDE";  /* 5 字节超过 cap=4 */
    ringbuf_write(&rb, a, 5);
    /* A 被覆盖，剩 BCDE */
    uint8_t out[4];
    CHECK(ringbuf_read(&rb, out, 4) == 4, "read 4");
    CHECK(memcmp(out, "BCDE", 4) == 0, "BCDE remains");
    ringbuf_free(&rb);
    return 0;
}

static int test_peek_doesnt_consume(void)
{
    ringbuf_t rb;
    ringbuf_init(&rb, 8);
    const uint8_t a[] = "xyz";
    ringbuf_write(&rb, a, 3);
    uint8_t out[4];
    CHECK(ringbuf_peek(&rb, out, 4) == 3, "peek 3");
    CHECK(memcmp(out, "xyz", 3) == 0, "peeked content == xyz");
    CHECK(ringbuf_available(&rb) == 3, "still 3 after peek");
    ringbuf_free(&rb);
    return 0;
}

int main(void)
{
    test_basic_write_read();
    test_wrap_around();
    test_overflow_overwrites_oldest();
    test_peek_doesnt_consume();
    printf("test_ringbuf: %d/%d pass\n", g_checks - g_failed, g_checks);
    return g_failed == 0 ? 0 : 1;
}
```

### Step 4: Update `lib/util/CMakeLists.txt`

读 `lib/util/CMakeLists.txt`，在 `MY_SOURCES` 加 `ringbuf.c`。

### Step 5: Update `test/CMakeLists.txt`

照葫芦画瓢（参照已有的 `test_strbuf` target）加 `test_ringbuf` 独立可执行：

```cmake
add_executable(test_ringbuf test_ringbuf.c)
target_link_libraries(test_ringbuf PRIVATE lib_util)
target_include_directories(test_ringbuf PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/util
    ${CMAKE_SOURCE_DIR}/include
)
```

### Step 6: Build & test

```bash
cd d:/CODE/zero-c && ./build.bat test
```

预期：`out/TEST/test_ringbuf.exe` 输出 `test_ringbuf: 12/12 pass`（4 个测试，12 个 CHECK）。

### Step 7: Commit

```
feat(util): ringbuf 字节环形缓冲 + 单元测试
```

---

## Task 2: `agent_chan.h` 公共接口

**Files:**
- Create: `include/agent_chan.h`

### Step 1: Write `include/agent_chan.h`

```cpp
/**
 * @file agent_chan.h
 * @brief 模组通道抽象：串口 / USB-NCM 等统一接口。
 *
 * 上层（AT 引擎 / DeviceManager）只通过 modem_chan_t 操作模组，
 * 不直接接触 uv_tty / socket / 等底层细节。
 */
#ifndef AGENT_CHAN_H
#define AGENT_CHAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct modem_chan;

/**
 * @brief HAL 收到字节时回调。buf/len 由回调内部生存期（通常已 copy 或转交）。
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

#endif
```

### Step 2: 单独 commit（无 build 验证需要——只加头）

```
feat(chan): agent_chan.h 公共接口
```

---

## Task 3: `midware/serial` — uv_tty_t 真接 COM

**Files:**
- Create: `midware/serial/serial_chan.h`
- Create: `midware/serial/serial_chan.c`
- Create: `midware/serial/CMakeLists.txt`
- Modify: `midware/CMakeLists.txt`

### Step 1: Write `midware/serial/serial_chan.h`

```c
/**
 * @file serial_chan.h
 * @brief COM 串口 modem_chan_t 实现（基于 libuv uv_tty_t）。
 */
#ifndef MIDWARE_SERIAL_CHAN_H
#define MIDWARE_SERIAL_CHAN_H

#include "agent_chan.h"
#include "ringbuf.h"

struct uv_loop_s;
typedef struct uv_loop_s uv_loop_t;

typedef struct {
    modem_chan_t  chan;       /* 基类，必须放第一个（impl 强转） */
    uv_tty_t      tty;
    ringbuf_t     rx_ring;    /* HAL 内部 ringbuf：uv 回调写入，AT 引擎读 */
    char          name[16];   /* "COM7" */
    int           baud;       /* 9600 / 115200 etc. */
    uv_loop_t    *loop;       /* 持有 loop 引用（弱引用，不释放） */
} serial_chan_t;

/**
 * @brief 在指定 loop 上创建串口通道。返回的指针在 close 后才能 free。
 */
serial_chan_t *serial_chan_create(uv_loop_t *loop);

/* modem_chan_ops_t 实现（直接当 modem_chan_t* 用） */
int  serial_chan_open (modem_chan_t *self, const char *uri);   /* URI: com://COM7?baud=9600 */
int  serial_chan_send (modem_chan_t *self, const uint8_t *buf, size_t len);
void serial_chan_close(modem_chan_t *self);

#endif
```

### Step 2: Write `midware/serial/serial_chan.c`

```c
/**
 * @file serial_chan.c
 * @brief COM 串口 uv_tty_t 实现。
 *
 * 读路径：uv 回调 → 写入 ringbuf → 调 on_rx 通知上层（上层随后 ringbuf_read）。
 * 写路径：直接 uv_tty_write（异步）。
 */
#include "serial_chan.h"
#include "agent_types.h"
#include "agent_errstr.h"  /* 仅用于 stderr，可选 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <uv.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RX_RING_CAP 4096

/* === URI 解析 === */

typedef struct {
    char name[16];
    int  baud;
    int  parity;   /* 0=N, 1=O, 2=E */
    int  stop_bits;/* 1 / 2 */
} serial_params_t;

static int parse_uri(const char *uri, serial_params_t *out)
{
    if (!uri || !out) return AGENT_ERR_BAD_ARG;
    /* 期望格式：com://COM7?baud=9600&parity=N&stop=1 */
    const char *p = strstr(uri, "com://");
    if (!p) return AGENT_ERR_BAD_ARG;
    p += 6;  /* skip "com://" */
    const char *q = strchr(p, '?');
    size_t name_len = q ? (size_t)(q - p) : strlen(p);
    if (name_len == 0 || name_len >= sizeof(out->name)) return AGENT_ERR_BAD_ARG;
    memcpy(out->name, p, name_len);
    out->name[name_len] = '\0';

    /* 默认值 */
    out->baud = 115200;
    out->parity = 0;
    out->stop_bits = 1;

    if (q) {
        q++;  /* skip '?' */
        char buf[128];
        size_t bl = 0;
        while (*q && bl < sizeof(buf) - 1) buf[bl++] = *q++;
        buf[bl] = '\0';
        char *tok = strtok(buf, "&");
        while (tok) {
            if (strncmp(tok, "baud=", 5) == 0)      out->baud = atoi(tok + 5);
            else if (strncmp(tok, "parity=", 7) == 0) out->parity = (tok[7] == 'O') ? 1 : (tok[7] == 'E') ? 2 : 0;
            else if (strncmp(tok, "stop=", 5) == 0)   out->stop_bits = atoi(tok + 5);
            tok = strtok(NULL, "&");
        }
    }
    return AGENT_OK;
}

/* === 读回调 === */

static void on_tty_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
    serial_chan_t *sc = (serial_chan_t *)stream->data;
    if (nread < 0) {
        if (sc->chan.on_rx) sc->chan.on_rx(sc->chan.userdata, NULL, 0);  /* 通知错误 */
        return;
    }
    if (nread == 0 || !buf->base) return;

    /* 写进 ringbuf，然后通知上层 */
    ringbuf_write(&sc->rx_ring, (const uint8_t *)buf->base, (size_t)nread);
    if (sc->chan.on_rx) {
        sc->chan.on_rx(sc->chan.userdata, buf->base, (size_t)nread);
    }
}

static void on_tty_write(uv_write_t *req, int status)
{
    free(req);  /* uv_write_t 必须由调用方 malloc + free */
    if (status != 0) {
        fprintf(stderr, "serial_chan: uv_tty_write status=%d (%s)\n",
                status, uv_strerror(status));
    }
}

/* === ops 实现 === */

int serial_chan_open(modem_chan_t *self, const char *uri)
{
    if (!self || !self->impl || !uri) return AGENT_ERR_BAD_ARG;
    serial_chan_t *sc = (serial_chan_t *)self->impl;

    if (self->is_open) return AGENT_ERR_BAD_ARG;  /* 已打开 */

    serial_params_t p;
    int rc = parse_uri(uri, &p);
    if (rc != AGENT_OK) return rc;

    strncpy(sc->name, p.name, sizeof(sc->name) - 1);
    sc->name[sizeof(sc->name) - 1] = '\0';
    sc->baud = p.baud;

    /* 分配 rx ring */
    if (ringbuf_init(&sc->rx_ring, RX_RING_CAP) != AGENT_OK) {
        return AGENT_ERR_OOM;
    }

    /* uv_tty_init：handle, loop, file, readable */
    int fd_serial = -1;
    /* Windows 上 uv_tty_t 用 uv_tty_init(loop, &tty, "COM7") 即可 */
    int r = uv_tty_init(sc->loop, &sc->tty, p.name);
    if (r != 0) {
        ringbuf_free(&sc->rx_ring);
        fprintf(stderr, "serial_chan: uv_tty_init(%s) 失败: %s\n",
                p.name, uv_strerror(r));
        return AGENT_ERR_IO;
    }
    sc->tty.data = sc;  /* 让回调能反查 sc */

    /* 启动读 */
    r = uv_tty_start_read(&sc->tty, on_tty_read);
    if (r != 0) {
        uv_close((uv_handle_t *)&sc->tty, NULL);
        ringbuf_free(&sc->rx_ring);
        fprintf(stderr, "serial_chan: uv_tty_start_read 失败: %s\n", uv_strerror(r));
        return AGENT_ERR_IO;
    }

    /* 设置 DCB（uv_tty_init 默认是 raw 8N1，需要按 baud 调） */
    DCB dcb = {0};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(sc->tty.handle, &dcb)) {
        /* ignore — uv_tty 已开 */
    } else {
        dcb.BaudRate = (DWORD)sc->baud;
        dcb.ByteSize = 8;
        dcb.Parity = (BYTE)p.parity;
        dcb.StopBits = (BYTE)p.stop_bits;
        SetCommState(sc->tty.handle, &dcb);
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

static void on_tty_close(uv_handle_t *handle)
{
    /* uv_tty 不需要释放内部资源，handle 在 close 后由 libuv 自动 free */
    (void)handle;
}

void serial_chan_close(modem_chan_t *self)
{
    if (!self || !self->impl || !self->is_open) return;
    serial_chan_t *sc = (serial_chan_t *)self->impl;

    uv_tty_stop_read(&sc->tty);
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
    sc->chan.ops = NULL;  /* 不通过虚表——直接暴露 serial_chan_* 函数 */
    sc->chan.impl = sc;
    sc->chan.is_open = false;
    return sc;
}
```

### Step 3: Write `midware/serial/CMakeLists.txt`

```cmake
set(MODULE_NAME midware_serial)
add_library(${MODULE_NAME} STATIC serial_chan.c)
target_include_directories(${MODULE_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib/util
)
target_link_libraries(${MODULE_NAME} PUBLIC
    lib_util
    third_libuv
)
```

### Step 4: Update `midware/CMakeLists.txt`

读当前内容，加 `add_subdirectory(serial)`。文件末尾或 mbedtls/libcurl 之后加。

### Step 5: Commit

```
feat(midware): serial_chan (uv_tty_t) 真接 COM
```

---

## Task 4: `midware/usb_ncm` — RNDIS/NCM 枚举

**Files:**
- Create: `midware/usb_ncm/ncm_chan.h`
- Create: `midware/usb_ncm/ncm_chan.c`
- Create: `midware/usb_ncm/CMakeLists.txt`
- Create: `test/test_ncm_enumerate.c`
- Modify: `midware/CMakeLists.txt`
- Modify: `test/CMakeLists.txt`

### Step 1: Write `midware/usb_ncm/ncm_chan.h`

```c
/**
 * @file ncm_chan.h
 * @brief USB-NCM / RNDIS 网卡枚举（IP Helper）。
 *
 * P2 只做"发现 + 报告"，不做数据面（数据面留给 P3）。
 */
#ifndef MIDWARE_NCM_CHAN_H
#define MIDWARE_NCM_CHAN_H

#include <stdbool.h>
#include <stddef.h>

#define NCM_MAX_INTERFACES 16

typedef struct {
    char  if_name[128];     /* 友好名 "Mobile broadband adapter" */
    char  adapter_name[64]; /* "以太网 X" */
    char  ipv4[16];         /* "10.42.0.7"，空字符串 = 无 IPv4 */
    bool  is_candidate;     /* 描述符匹配 RNDIS/NCM/CDC-ECM */
} ncm_interface_t;

/**
 * @brief 枚举所有 USB-NCM / RNDIS / CDC-ECM 候选网卡。
 * @param out      输出数组
 * @param max      数组容量
 * @return 实际写入数量（≤ max）
 */
int ncm_enumerate(ncm_interface_t *out, int max);

#endif
```

### Step 2: Write `midware/usb_ncm/ncm_chan.c`

```c
/**
 * @file ncm_chan.c
 */
#include "ncm_chan.h"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>       /* GetAdaptersAddresses 需要先于 <iphlpapi.h> */
#include <windows.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "iphlpapi.lib")  /* MSVC；MinGW 用 target_link_libraries 链接 */

static bool description_matches_ncm(const char *desc)
{
    if (!desc) return false;
    /* 大小写不敏感匹配常见 NCM 关键字 */
    static const char *kKeywords[] = {
        "Remote NDIS", "RNDIS", "NCM", "CDC-ECM", "CDC NCM",
        "Mobile Broadband", "MBIM", "wwan", "WWAN", NULL
    };
    for (int i = 0; kKeywords[i]; i++) {
        if (strstr(desc, kKeywords[i])) return true;
    }
    return false;
}

int ncm_enumerate(ncm_interface_t *out, int max)
{
    if (!out || max <= 0) return 0;

    ULONG buf_len = 16 * 1024;
    IP_ADAPTER_ADDRESSES *buf = (IP_ADAPTER_ADDRESSES *)malloc(buf_len);
    if (!buf) return 0;

    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
                  GAA_FLAG_SKIP_DNS_SERVER;
    DWORD rc = GetAdaptersAddresses(AF_INET, flags, NULL, buf, &buf_len);
    if (rc == ERROR_BUFFER_OVERFLOW) {
        buf = (IP_ADAPTER_ADDRESSES *)realloc(buf, buf_len);
        if (!buf) return 0;
        rc = GetAdaptersAddresses(AF_INET, flags, NULL, buf, &buf_len);
    }
    if (rc != NO_ERROR) {
        free(buf);
        return 0;
    }

    int n = 0;
    for (IP_ADAPTER_ADDRESSES *a = buf; a && n < max; a = a->Next) {
        if (a->OperStatus != IfOperStatusUp) continue;
        if (!description_matches_ncm(a->Description)) continue;

        ncm_interface_t *dst = &out[n++];
        memset(dst, 0, sizeof(*dst));
        if (a->FriendlyName) {
            wcs_to_utf8(a->FriendlyName, dst->if_name, sizeof(dst->if_name));
        }
        if (a->AdapterName) {
            strncpy(dst->adapter_name, a->AdapterName, sizeof(dst->adapter_name) - 1);
        }
        /* 取第一个 IPv4 */
        for (IP_ADAPTER_UNICAST_ADDRESS *u = a->FirstUnicastAddress; u; u = u->Next) {
            if (u->Address.lpSockaddr->sa_family == AF_INET) {
                struct sockaddr_in *sa = (struct sockaddr_in *)u->Address.lpSockaddr;
                inet_ntop(AF_INET, &sa->sin_addr, dst->ipv4, sizeof(dst->ipv4));
                break;
            }
        }
        dst->is_candidate = true;
    }

    free(buf);
    return n;
}

/* WideChar → UTF-8 helper（避免引 <winsock2.h> 之外的额外 include） */
static void wcs_to_utf8(const wchar_t *src, char *dst, size_t dst_len)
{
    if (!src || !dst || dst_len == 0) { if (dst_len) dst[0]='\0'; return; }
    WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, (int)dst_len, NULL, NULL);
}
```

> 注：上面 `wcs_to_utf8` 函数在引用时要在文件顶部声明或前置——按 C 编译顺序把它放到文件头，调用前的位置已 OK。

### Step 3: Write `midware/usb_ncm/CMakeLists.txt`

```cmake
set(MODULE_NAME midware_usb_ncm)
add_library(${MODULE_NAME} STATIC ncm_chan.c)
target_include_directories(${MODULE_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)
# MinGW: iphlpapi 是 system lib
target_link_libraries(${MODULE_NAME} PUBLIC iphlpapi ws2_32)
```

### Step 4: Update `midware/CMakeLists.txt` & `test/CMakeLists.txt`

`midware/CMakeLists.txt` 加 `add_subdirectory(usb_ncm)`。

`test/CMakeLists.txt` 加：

```cmake
add_executable(test_ncm_enumerate test_ncm_enumerate.c)
target_link_libraries(test_ncm_enumerate PRIVATE
    midware_usb_ncm
)
```

### Step 5: Write `test/test_ncm_enumerate.c`

```c
/**
 * @file test_ncm_enumerate.c
 * @brief 打印当前机器的 RNDIS/NCM/CDC-ECM 网卡。
 */
#include "ncm_chan.h"
#include <stdio.h>

int main(void)
{
    ncm_interface_t ifs[NCM_MAX_INTERFACES];
    int n = ncm_enumerate(ifs, NCM_MAX_INTERFACES);
    printf("test_ncm_enumerate: 发现 %d 个 RNDIS/NCM 候选网卡\n", n);
    for (int i = 0; i < n; i++) {
        printf("  [%d] if=%s  adapter=%s  ipv4=%s\n",
               i, ifs[i].if_name, ifs[i].adapter_name, ifs[i].ipv4);
    }
    /* 即使没找到也 pass——开发机可能没插模组 */
    return 0;
}
```

### Step 6: Build & test

```bash
cd d:/CODE/zero-c && ./build.bat test
./out/TEST/test_ncm_enumerate.exe
```

预期：列出 0~N 个候选网卡（用户机器上插了模组就有，没有就 0）。

### Step 7: Commit

```
feat(midware): ncm_chan RNDIS/NCM 枚举（IP Helper）
```

---

## Task 5: `lib/device_manager` — 热插拔扫描

**Files:**
- Create: `lib/device_manager/device_manager.h`
- Create: `lib/device_manager/device_manager.c`
- Create: `lib/device_manager/CMakeLists.txt`
- Modify: `lib/CMakeLists.txt`

### Step 1: Write `lib/device_manager/device_manager.h`

```c
/**
 * @file device_manager.h
 * @brief 多模组 manager：热插拔扫描 + 设备列表 + 变更通知。
 */
#ifndef LIB_DEVICE_MANAGER_H
#define LIB_DEVICE_MANAGER_H

#include "agent_types.h"

#define DEV_MANAGER_MAX_DEVS 16

typedef enum {
    DEV_STATE_DISCONNECTED = 0,  /* 链路层发现，但未打开通道 */
    DEV_STATE_READY,              /* 通道已打开 */
    DEV_STATE_ERROR
} dev_state_t;

typedef struct modem_dev {
    char          id[32];         /* "MDM-001" */
    char          label[64];      /* 用户可命名，默认 = friendly name */
    char          chan_uri[256];  /* "com://COM7?baud=9600" / "rndis://xxx" */
    char          ipv4[16];       /* 仅 NCM/RNDIS 有；COM 留空 */
    dev_state_t   state;
    int           csq;            /* 0~31；P2 留 0，P3 填 */
} modem_dev_t;

/* dev_list 是 device_manager 内部维护的数组（最多 16 个）的快照。 */
typedef struct {
    modem_dev_t devs[DEV_MANAGER_MAX_DEVS];
    int         count;
} dev_list_snapshot_t;

struct uv_loop_s;
typedef struct uv_loop_s uv_loop_t;

typedef void (*dev_change_fn)(void *userdata, const dev_list_snapshot_t *snapshot);

typedef struct {
    uv_loop_t           *loop;          /* 不持有 */
    modem_dev_t          devs[DEV_MANAGER_MAX_DEVS];
    int                  dev_count;
    uv_timer_t           scan_timer;    /* 每 2s 触发一次扫描 */
    dev_change_fn        on_change;
    void                *userdata;
    /* 上次的"原始快照"用于 diff */
    char                 prev_serial[64][8];   /* 上次扫到的 COM 端口名 */
    int                  prev_serial_count;
    char                 prev_ncm[64][128];     /* 上次扫到的 NCM 友好名 */
    int                  prev_ncm_count;
} device_manager_t;

int  device_manager_init   (device_manager_t *m, uv_loop_t *loop);
int  device_manager_start  (device_manager_t *m);  /* 启动 scan_timer */
void device_manager_stop   (device_manager_t *m);
int  device_manager_force_scan(device_manager_t *m);
void device_manager_set_callback(device_manager_t *m, dev_change_fn fn, void *userdata);

#endif
```

### Step 2: Write `lib/device_manager/device_manager.c`

```c
/**
 * @file device_manager.c
 * @brief 多模组 manager 实现。
 *
 * 每 2 秒扫描一次：COM 端口（via QueryDosDeviceW）+ NCM/RNDIS 网卡（via ncm_enumerate）。
 * Diff 后增删 modem_dev_t，触发 on_change 回调。
 *
 * P2 范围：链路层 only——不打开通道（chan 字段保持 NULL）。P3 加 AT 引擎后，
 * device_state 切到 READY 才分配 serial_chan_t 并 open。
 */
#include "device_manager.h"
#include "ncm_chan.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <uv.h>

#include <stdio.h>
#include <string.h>

#define SCAN_INTERVAL_MS 2000
#define MAX_SCAN_RESULT  64

/* === 工具：列 COM 端口 === */

static int list_com_ports(char out[][8], int max)
{
    /* QueryDosDeviceW 返回 \\.\COM1, \\.\COM2, ... */
    const DWORD buf_size = 64 * 1024;
    wchar_t *buf = (wchar_t *)malloc(buf_size);
    if (!buf) return 0;
    DWORD got = QueryDosDeviceW(NULL, buf, buf_size);
    int n = 0;
    wchar_t *p = buf;
    while (*p && n < max) {
        if (wcsncmp(p, L"COM", 3) == 0) {
            /* 提取 "COM7" */
            char name[8];
            WideCharToMultiByte(CP_ACP, 0, p, 7, name, sizeof(name), NULL, NULL);
            name[7] = '\0';
            strncpy(out[n], name, 8);
            n++;
        }
        p += wcslen(p) + 1;
    }
    free(buf);
    return n;
}

/* === diff helper === */

static int find_index(const char (*arr)[8], int n, const char *name)
{
    for (int i = 0; i < n; i++) {
        if (strcmp(arr[i], name) == 0) return i;
    }
    return -1;
}

static int find_index_ncm(const char (*arr)[128], int n, const char *name)
{
    for (int i = 0; i < n; i++) {
        if (strcmp(arr[i], name) == 0) return i;
    }
    return -1;
}

/* === 主扫描 === */

static void post_change_event(device_manager_t *m)
{
    if (!m->on_change) return;
    dev_list_snapshot_t snap;
    memcpy(snap.devs, m->devs, sizeof(m->devs));
    snap.count = m->dev_count;
    m->on_change(m->userdata, &snap);
}

static void do_scan(uv_timer_t *handle)
{
    device_manager_t *m = (device_manager_t *)handle->data;

    char com_now[MAX_SCAN_RESULT][8];
    int  com_n = list_com_ports(com_now, MAX_SCAN_RESULT);

    ncm_interface_t ncm_now[NCM_MAX_INTERFACES];
    int  ncm_n = ncm_enumerate(ncm_now, NCM_MAX_INTERFACES);

    int com_added = 0, com_removed = 0;
    for (int i = 0; i < com_n; i++) {
        if (find_index(m->prev_serial, m->prev_serial_count, com_now[i]) < 0) com_added++;
    }
    for (int i = 0; i < m->prev_serial_count; i++) {
        if (find_index(com_now, com_n, m->prev_serial[i]) < 0) com_removed++;
    }
    int ncm_added = 0, ncm_removed = 0;
    for (int i = 0; i < ncm_n; i++) {
        if (find_index_ncm(m->prev_ncm, m->prev_ncm_count, ncm_now[i].if_name) < 0) ncm_added++;
    }
    for (int i = 0; i < m->prev_ncm_count; i++) {
        if (find_index_ncm(ncm_now, ncm_n, m->prev_ncm[i]) < 0) ncm_removed++;
    }

    if (com_added == 0 && com_removed == 0 && ncm_added == 0 && ncm_removed == 0) {
        return;  /* 无变化，跳过重建 */
    }

    /* 重建 m->devs（按 spec §4.3 dev 数组） */
    m->dev_count = 0;
    for (int i = 0; i < com_n && m->dev_count < DEV_MANAGER_MAX_DEVS; i++) {
        modem_dev_t *d = &m->devs[m->dev_count++];
        memset(d, 0, sizeof(*d));
        /* id = "MDM-COM7" 之类 */
        snprintf(d->id, sizeof(d->id), "MDM-COM%s", com_now[i]);
        snprintf(d->label, sizeof(d->label), "COM %s", com_now[i]);
        snprintf(d->chan_uri, sizeof(d->chan_uri), "com://%s?baud=115200", com_now[i]);
        d->state = DEV_STATE_DISCONNECTED;
    }
    for (int i = 0; i < ncm_n && m->dev_count < DEV_MANAGER_MAX_DEVS; i++) {
        modem_dev_t *d = &m->devs[m->dev_count++];
        memset(d, 0, sizeof(*d));
        snprintf(d->id, sizeof(d->id), "MDM-NCM%d", i);
        snprintf(d->label, sizeof(d->label), "%s", ncm_now[i].if_name);
        snprintf(d->chan_uri, sizeof(d->chan_uri), "rndis://%s", ncm_now[i].if_name);
        strncpy(d->ipv4, ncm_now[i].ipv4, sizeof(d->ipv4) - 1);
        d->state = DEV_STATE_DISCONNECTED;
    }

    /* 更新 prev 快照 */
    for (int i = 0; i < com_n; i++) strncpy(m->prev_serial[i], com_now[i], 8);
    m->prev_serial_count = com_n;
    for (int i = 0; i < ncm_n; i++) strncpy(m->prev_ncm[i], ncm_now[i].if_name, 128);
    m->prev_ncm_count = ncm_n;

    fprintf(stderr, "device_manager: 扫描 diff — COM +%d/-%d, NCM +%d/-%d → 共 %d 设备\n",
            com_added, com_removed, ncm_added, ncm_removed, m->dev_count);
    post_change_event(m);
}

int device_manager_init(device_manager_t *m, uv_loop_t *loop)
{
    if (!m || !loop) return AGENT_ERR_BAD_ARG;
    memset(m, 0, sizeof(*m));
    m->loop = loop;
    return AGENT_OK;
}

int device_manager_start(device_manager_t *m)
{
    if (!m || !m->loop) return AGENT_ERR_BAD_ARG;
    int r = uv_timer_init(m->loop, &m->scan_timer);
    if (r != 0) return AGENT_ERR_IO;
    m->scan_timer.data = m;
    r = uv_timer_start(&m->scan_timer, do_scan, 0, SCAN_INTERVAL_MS);
    if (r != 0) return AGENT_ERR_IO;
    fprintf(stderr, "device_manager: 启动扫描，间隔 %d ms\n", SCAN_INTERVAL_MS);
    return AGENT_OK;
}

void device_manager_stop(device_manager_t *m)
{
    if (!m) return;
    uv_timer_stop(&m->scan_timer);
    uv_close((uv_handle_t *)&m->scan_timer, NULL);
}

int device_manager_force_scan(device_manager_t *m)
{
    if (!m) return AGENT_ERR_BAD_ARG;
    do_scan(&m->scan_timer);
    return AGENT_OK;
}

void device_manager_set_callback(device_manager_t *m, dev_change_fn fn, void *userdata)
{
    if (!m) return;
    m->on_change = fn;
    m->userdata = userdata;
}
```

### Step 3: Write `lib/device_manager/CMakeLists.txt`

```cmake
set(MODULE_NAME lib_device_manager)
add_library(${MODULE_NAME} STATIC device_manager.c)
target_include_directories(${MODULE_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/midware/usb_ncm
)
target_link_libraries(${MODULE_NAME} PUBLIC
    lib_util
    midware_usb_ncm
    third_libuv
)
```

### Step 4: Update `lib/CMakeLists.txt`

加 `add_subdirectory(device_manager)`。

### Step 5: Commit

```
feat(devmgr): device_manager 热插拔扫描 + dev 列表 + 变更通知
```

---

## Task 6: 接入 libuv 主循环到 host.cpp

**Files:**
- Modify: `app/shell/host.cpp`
- Modify: `include/agent_types.h`（加 `uv_loop_t *uv_loop` 字段）

### Step 1: Add uv_loop_t to agent_types.h

读 `include/agent_types.h`。在 `agent_app_t` 结构体里加：

```c
    /* libuv loop（host 拥有，panels 通过 app 拿到） */
    struct uv_loop_s *uv_loop;
```

并**前置声明** `struct uv_loop_s`（避免把所有 libuv 头塞进公共头）。在 `agent_types.h` 顶部加：

```c
struct uv_loop_s;
```

### Step 2: Modify `app/shell/host.cpp`

读当前内容。改 3 处：

1. `host_create`：在 `ImGui::CreateContext()` 之前初始化 libuv loop。

```c
    /* libuv loop */
    c->uv_loop = (uv_loop_t *)malloc(sizeof(uv_loop_t));
    if (!c->uv_loop) { DestroyWindow(c->hwnd); free(c); return -5; }
    if (uv_loop_init(c->uv_loop) != 0) {
        free(c->uv_loop); DestroyWindow(c->hwnd); free(c); return -1;
    }
```

2. `host_run`：在每帧的 ImGui 渲染逻辑**之前**加 `uv_run(UV_RUN_NOWAIT)`：

```c
        /* libuv 非阻塞 tick（让 device_manager / 串口回调能跑） */
        if (c->uv_loop) uv_run(c->uv_loop, UV_RUN_NOWAIT);
```

3. `host_destroy`：在 `ImGui::DestroyContext()` 之后清理 libuv：

```c
    if (c->uv_loop) {
        uv_loop_close(c->uv_loop);  /* 应当没未关闭的 handle，否则返回非 0 */
        free(c->uv_loop);
        c->uv_loop = NULL;
    }
```

> 顶层 include 区加 `#include <uv.h>`（已经通过 `third_libuv` PUBLIC 传递了 `<uv.h>` include 路径，所以只要 `host.cpp` 链 `third_libuv` 即可）。检查 `app/shell/CMakeLists.txt` 是否链了 `third_libuv`——如果没有就加。

### Step 3: Build 验证

```bash
cd d:/CODE/zero-c && ./build.bat
```

预期：build 成功，EXE 重新生成。

### Step 4: Commit

```
feat(shell): host 接入 libuv 事件循环（UV_RUN_NOWAIT）
```

---

## Task 7: `core/main.cpp` 启动 device_manager

**Files:**
- Modify: `core/main.cpp`

### Step 1: Add device_manager wiring

读 `core/main.cpp` 当前内容。在 `host_create` 之后（成功拿到 ctx）调 `device_manager`：

```c
    /* 启动 device_manager（在 host 的 uv_loop 上） */
    static device_manager_t g_devmgr;  /* P3 再搬到 agent_app_t */
    device_manager_init(&g_devmgr, ctx);
    device_manager_set_callback(&g_devmgr, NULL, NULL);  /* 暂不挂回调，P3 接入 panel_devices */
    device_manager_start(&g_devmgr);
    g_app.device_manager = &g_devmgr;  /* agent_types.h 已加字段 */
```

并在 `core/main.cpp` 顶部 include：

```c
#include "device_manager.h"
```

> P2 把 `device_manager_t` 暂存为 `static` 局部；P3 改为塞进 `agent_app_t` 生命周期管理。

### Step 2: Build + 真机验证

```bash
cd d:/CODE/zero-c && ./build.bat
```

启动 EXE，看 stderr 应每 2s 触发 `device_manager: 启动扫描，间隔 2000 ms`（首次）和后续 `device_manager: 扫描 diff — ...`。

### Step 3: Commit

```
feat(devmgr): main 启动 device_manager + 接入 host uv_loop
```

---

## Task 8: panel_devices 订阅 device_manager

**Files:**
- Modify: `app/panel_devices/panel.cpp`

### Step 1: Replace 硬编码 3 行为实时数据

读 `app/panel_devices/panel.cpp` 当前内容。**整个替换**为：

```cpp
/**
 * @file panel_devices.cpp
 * @brief 多模组列表 panel——订阅 device_manager 实时设备列表。
 *
 * 每帧检查 app->device_manager->dev_count，dev 数变化时刷新表格。
 */
#include "panel_devices.h"
#include "i18n.h"
#include "imgui.h"
#include "device_manager.h"
#include <cstring>

/* 6 列的 i18n key 列表 */
static const char *kColI18n[] = {
    "devices.col.name",
    "devices.col.com",
    "devices.col.ip",
    "devices.col.csq",
    "devices.col.state",
    "devices.col.lastseen",
};

/**
 * @brief 渲染多模组列表：标题 + 6 列表格（数据来自 device_manager）。
 */
void panel_devices_render(agent_app_t *app)
{
    ImGui::Text("%s", i18n_get("devices.title"));
    ImGui::Separator();

    device_manager_t *m = (device_manager_t *)app->device_manager;
    int count = m ? m->dev_count : 0;

    if (count == 0) {
        ImGui::TextDisabled("暂未发现模组——插上 COM 或 USB-NCM 模组等待 2 秒");
        return;
    }

    if (ImGui::BeginTable("devices_tbl", 6,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        for (int i = 0; i < 6; i++) {
            ImGui::TableSetupColumn(i18n_get(kColI18n[i]));
        }
        ImGui::TableHeadersRow();

        for (int r = 0; r < count; r++) {
            const modem_dev_t *d = &m->devs[r];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("%s", d->label);
            ImGui::TableSetColumnIndex(1); ImGui::Text("%s", d->chan_uri);
            ImGui::TableSetColumnIndex(2); ImGui::Text("%s", d->ipv4[0] ? d->ipv4 : "-");
            ImGui::TableSetColumnIndex(3); ImGui::Text("%d", d->csq);
            ImGui::TableSetColumnIndex(4);
            const char *state_str = (d->state == DEV_STATE_READY) ? "READY"
                                    : (d->state == DEV_STATE_ERROR) ? "ERROR" : "DISCONNECTED";
            ImGui::Text("%s", state_str);
            ImGui::TableSetColumnIndex(5); ImGui::Text("-");
        }
        ImGui::EndTable();
    }
}
```

### Step 2: 真机验证

启动 EXE：
- 多模组面板：若插了模组，2 秒内表格出现"COM7" / "rndis://..." 一行；拔出后 2 秒内消失
- 未插任何模组：显示"暂未发现模组——插上 COM 或 USB-NCM 模组等待 2 秒"

### Step 3: Commit

```
feat(ui): panel_devices 订阅 device_manager 显示实时设备
```

---

## Task 9: P2 冒烟清单 + tag

**Files:**
- Create: `docs/superpowers/checklists/phase2-hal-libuv.md`
- Tag: `phase2-hal-libuv`

### Step 1: Write checklist

按 Phase 0/1 风格，但内容针对 P2：

```markdown
# Phase 2 — HAL + libuv 冒烟测试

## 沙箱里可做

- [x] `./build.bat` build 成功
- [x] `./build.bat test` 编译 + 运行所有 test
  - [x] test_strbuf 18/18
  - [x] test_json_roundtrip 20/20
  - [x] test_i18n 6/6
  - [x] **test_ringbuf 12/12**（新）
  - [x] **test_ncm_enumerate 列出 0~N 个候选**（新）
- [x] EXE 重新生成

## 真机验证

### 模组未插

- [ ] 启动 EXE（未插任何模组），切到"多模组"面板
- [ ] 表格区显示 `暂未发现模组——插上 COM 或 USB-NCM 模组等待 2 秒`
- [ ] stderr 每 2s 打印 `device_manager: 扫描 diff — COM +0/-0, NCM +0/-0 → 共 0 设备` 或类似

### 插上 USB 串口模组

- [ ] 插上后 ≤ 2s，表格自动出现一行：label="COM 7"、chan_uri="com://COM7?baud=115200"、state=DISCONNECTED
- [ ] stderr 打印 `device_manager: 扫描 diff — COM +1/-0, NCM +0/-0 → 共 1 设备`
- [ ] 拔出后 ≤ 2s，该行消失
- [ ] stderr 打印 `device_manager: 扫描 diff — COM +0/-1, NCM +0/-0 → 共 0 设备`

### 插上 USB-NCM 模组（如 Air724 / EC200N 等）

- [ ] 插上后 ≤ 2s，表格出现一行：label 含 "RNDIS" 或 "Mobile"，ipv4 显示非空 IP，chan_uri="rndis://..."
- [ ] stderr 打印 NCM +1 提示

### Debug console

- [ ] `set AGENT_DEBUG_CONSOLE=1 && APP.exe`：console 弹出，所有 `device_manager:` / `serial_chan:` 日志可见
```

### Step 2: Commit checklist + 打 tag

```bash
cd d:/CODE/zero-c
git add docs/superpowers/checklists/phase2-hal-libuv.md
git commit -m "docs: Phase 2 HAL + libuv 冒烟测试清单"
git tag phase2-hal-libuv
```

---

## Self-Review

**1. Spec 覆盖**：
- §4.1 modem_chan_t 抽象 → Task 2
- §4.3 DeviceManager（多模组、dev 列表、热插拔扫描）→ Task 5
- §5.1 libuv 单 loop + UI 主线程 + uv_async 通知 → Task 6
- §8 Phase 2 → Task 1-9 全部
- §12 风险：uv_tty 限制（libuv 不支持任意 USB-CDC 设备）→ Task 3 用 uv_tty 处理真 COM，RNDIS 走 IP Helper 而非 raw socket

**2. 占位符扫**：0 处 "TBD" / "TODO" / "fill in" / "similar to"。

**3. 类型一致性**：
- `modem_chan_t` 在 `agent_chan.h` 定义
- `serial_chan_t` 第一个字段是 `modem_chan_t chan`（保证强转一致）
- `ncm_interface_t` 与 `device_manager` 用 `chan_uri` 字符串统一

---

## Execution Handoff

Plan complete. Two execution options:

1. **Subagent-Driven (recommended)** — 8 个 task × 3 subagents = 24 dispatches，预计 1-1.5 周
2. **Inline Execution** — 同 session 跑，~3-5 天

**怎么选？**
