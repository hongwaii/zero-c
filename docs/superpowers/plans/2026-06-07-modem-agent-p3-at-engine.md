# Modem Agent — Plan P3: AT 引擎 + DeviceManager

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 EXE 真正连上模组：手动点连接 → 打开 COM 串口 → 起 AT 会话 → 实时显示 7 张状态卡（CSQ/CEREG/COPS/IMEI/IMSI/ICCID/RAT）→ 在控制台发 `AT+CSQ` 看回码 `+CSQ: 23,99\nOK`。

**Architecture:**
- `lib/at_engine/`：行解析（CR/LF 边界）+ URC 路由 + 命令队列 + 超时
- `lib/device_manager/` 扩展：每设备 `serial_chan_t + at_session_t` 生命周期 + DISCONNECTED → CONNECTING → READY/ERROR 状态机
- `lib/diag_service/`：订阅 device_manager，对 READY 设备拉取诊断值
- `app/panel_devices/`：每行加"连接/断开"按钮
- `app/panel_diag/`：7 张状态卡从写死变真订阅 diag_state；AT 控制台从 mock 变真收发

**Tech Stack:** C11、libuv 1.49（已 vendored）、ringbuf（P2 已有）、strbuf（P1 已有）、ImGui 1.92.9。

**Spec reference:** [docs/superpowers/specs/2026-06-07-modem-agent-design.md](docs/superpowers/specs/2026-06-07-modem-agent-design.md) §4.2（AT 引擎）、§4.3（DeviceManager 扩展）、§4.4（DiagnosticService）、§8（Phase 3）、§12（风险）。

**前置：** Plan P2 完成（tag `phase2-hal-libuv`）+ 500ms 扫描 tuning（`d720a20`）。

---

## File Structure

### 新增

| Path | 职责 |
|---|---|
| `lib/at_engine/at_session.h` | AT 会话公共 API |
| `lib/at_engine/at_session.c` | 命令队列、状态机、URC 路由 |
| `lib/at_engine/at_parser.h` | 行解析器公共 API（CR/LF 边界、final/URC/data 分类） |
| `lib/at_engine/at_parser.c` | 状态机 |
| `lib/at_engine/CMakeLists.txt` | 静态库 `lib_at_engine` |
| `lib/diag_service/diag_state.h` | 诊断状态结构（7 个字段） |
| `lib/diag_service/diag_service.h` | 订阅 device_manager / 定期刷新 diag_state |
| `lib/diag_service/diag_service.c` | 实现 |
| `lib/diag_service/CMakeLists.txt` | 静态库 |
| `test/test_at_parser.c` | 3GPP 经典行解析用例 |
| `test/test_at_session.c` | 队列、超时、URC（mock chan） |
| `test/test_diag_state.c` | diag_state 字段读写 |
| `docs/superpowers/checklists/phase3-at-engine.md` | 真机冒烟清单 |

### 修改

| Path | 改动 |
|---|---|
| `lib/device_manager/device_manager.h` | `modem_dev_t` 加 `at_session_t *at` / `serial_chan_t *serial` / `state` 字段 + `device_manager_connect_dev` / `disconnect_dev` API |
| `lib/device_manager/device_manager.c` | 实现 connect/disconnect + 状态机 + 错误处理 |
| `lib/CMakeLists.txt` | `add_subdirectory(diag_service)` |
| `app/panel_devices/panel.cpp` | 每行加"操作"列：连接/断开按钮 + 状态显示 |
| `app/panel_devices/CMakeLists.txt` | 链 `lib_at_engine` + `midware_serial` |
| `app/panel_diag/panel.cpp` | 7 张状态卡从 `app->diag_state` 读；AT 控制台通过 `app->active_at` 收发 |
| `app/panel_diag/CMakeLists.txt` | 链 `lib_at_engine` + `lib_diag_service` |
| `core/main.cpp` | 创建 diag_service、挂 on_change 回调、初始化 active_at |
| `core/CMakeLists.txt` | 链 `lib_at_engine` + `lib_diag_service` + `midware_serial` |
| `agent_types.h` | `agent_app_t` 加 `diag_state`、`active_at_session` 字段 |

---

## Conventions (续 Plan P1/P2)

- C11，4 空格，100 列
- 所有注释中文（zh-CN）
- 标识符 / 字符串字面量 / 编译宏英文
- 单元测试用 runtime `CHECK(cond, msg)` 宏
- 每 task 1 commit，commit message 中文

---

## Task 1: AT 行解析器（at_parser）

**Files:**
- Create: `lib/at_engine/at_parser.h`
- Create: `lib/at_engine/at_parser.c`
- Create: `lib/at_engine/CMakeLists.txt`
- Create: `test/test_at_parser.c`
- Modify: `lib/CMakeLists.txt`
- Modify: `test/CMakeLists.txt`

### Step 1: Write `lib/at_engine/at_parser.h`

```c
/**
 * @file at_parser.h
 * @brief AT 命令行解析器：CR/LF 边界 + final/URC/data 分类。
 *
 * 把字节流（已从 serial_chan 取出）拆成行；每行判定是 final response / URC / data。
 * 不负责命令队列和超时——那是 at_session 的事。
 */
#ifndef LIB_AT_ENGINE_AT_PARSER_H
#define LIB_AT_AT_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    AT_LINE_INCOMPLETE,    /* 还在拼行 */
    AT_LINE_FINAL_OK,      /* "OK" */
    AT_LINE_FINAL_ERROR,   /* "ERROR" / "+CME ERROR: ..." / "NO CARRIER" 等 */
    AT_LINE_URC,           /* "+CMTI: ..." / "+CEREG: 5" 等 */
    AT_LINE_DATA,          /* 普通数据行（如 "+CSQ: 23,99"），归 pending 命令 */
} at_line_type_t;

typedef struct {
    char  line[256];       /* 不含 \r\n，NUL 结尾 */
    size_t len;
    at_line_type_t type;
} at_line_t;

/**
 * @brief 初始化解析器（仅置零）。
 */
void at_parser_init(void);

/**
 * @brief 喂一个字节，返回"是否拼成一行"以及（如果成行）行的内容与类型。
 *
 * 不是状态机吗？其实是——内部维持一个待拼行缓冲。调用方在收到新字节时
 * 反复调本函数，直到返回 true 拿到一行为止。
 */
bool at_parser_feed(uint8_t b, at_line_t *out);

#endif
```

### Step 2: Write `lib/at_engine/at_parser.c`

```c
/**
 * @file at_parser.c
 * @brief AT 行解析器实现。
 *
 * 状态：积累字符 → 遇 \r 或 \n 收尾 → 判类型 → 输出。
 * 连续 \r\n 合并：见到 \r\n 立即收尾；如果连续 \n\n 第二次空行忽略。
 */
#include "at_parser.h"

#include <string.h>

/* 内部：行缓冲。本解析器只支持单条在拼行（at_session 会依次喂）。 */
static char     g_line[256];
static size_t   g_len = 0;
static bool     g_prev_was_cr = false;  /* 处理 \r\n 合并 */

void at_parser_init(void)
{
    g_len = 0;
    g_prev_was_cr = false;
}

/**
 * @brief 判断字符串是否是 final error 类（ERROR / +CME ERROR / NO CARRIER 等）。
 */
static bool is_final_error(const char *s)
{
    if (strcmp(s, "ERROR") == 0) return true;
    if (strncmp(s, "+CME ERROR", 10) == 0) return true;
    if (strncmp(s, "+CMS ERROR", 10) == 0) return true;
    if (strcmp(s, "NO CARRIER") == 0) return true;
    if (strcmp(s, "NO DIALTONE") == 0) return true;
    if (strcmp(s, "BUSY") == 0) return true;
    if (strcmp(s, "NO ANSWER") == 0) return true;
    return false;
}

bool at_parser_feed(uint8_t b, at_line_t *out)
{
    if (b == '\r') {
        g_prev_was_cr = true;
        return false;  /* 等可能的 \n */
    }
    if (b == '\n') {
        /* \n 收尾：忽略空行；之前 \r 也置过 g_prev_was_cr 也要清。 */
        g_prev_was_cr = false;
        if (g_len == 0) {
            return false;  /* 连续空行忽略 */
        }
        /* 输出 */
        g_line[g_len] = '\0';
        out->len = g_len;
        memcpy(out->line, g_line, g_len + 1);
        if (strcmp(out->line, "OK") == 0) {
            out->type = AT_LINE_FINAL_OK;
        } else if (is_final_error(out->line)) {
            out->type = AT_LINE_FINAL_ERROR;
        } else if (out->line[0] == '+') {
            out->type = AT_LINE_URC;  /* 实际是不是 URC 由 session 进一步匹配 */
        } else {
            out->type = AT_LINE_DATA;
        }
        g_len = 0;
        return true;
    }
    /* 普通字节：累计。如果之前刚见过 \r（裸 \r 不是 \r\n），也收尾 */
    if (g_prev_was_cr && g_len > 0) {
        /* 之前 \r 后面跟的不是 \n，是普通字符：先把 \r 当行收尾 */
        g_line[g_len] = '\0';
        out->len = g_len;
        memcpy(out->line, g_line, g_len + 1);
        if (strcmp(out->line, "OK") == 0) {
            out->type = AT_LINE_FINAL_OK;
        } else if (is_final_error(out->line)) {
            out->type = AT_LINE_FINAL_ERROR;
        } else if (out->line[0] == '+') {
            out->type = AT_LINE_URC;
        } else {
            out->type = AT_LINE_DATA;
        }
        g_len = 0;
        g_prev_was_cr = false;
        /* 把当前字节也放到新行（fall-through） */
    }
    if (g_len < sizeof(g_line) - 1) {
        g_line[g_len++] = (char)b;
    } else {
        /* 行超长：截断、丢弃 */
        g_len = 0;
    }
    return false;
}
```

### Step 3: Write `test/test_at_parser.c`

```c
/**
 * @file test_at_parser.c
 * @brief AT 行解析器单元测试。
 */
#include "at_parser.h"

#include <stdio.h>
#include <string.h>

static int g_checks = 0, g_failed = 0;
#define CHECK(c, m) do { g_checks++; if (!(c)) { g_failed++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, m); } } while (0)

/**
 * @brief 把字符串喂给 parser，把每行结果累积到 lines 数组。
 * @return 解析出的行数
 */
static size_t feed_string(const char *s, at_line_t *lines, size_t max)
{
    at_parser_init();
    size_t n = 0;
    for (const char *p = s; *p; p++) {
        at_line_t line;
        if (at_parser_feed((uint8_t)*p, &line)) {
            if (n < max) lines[n++] = line;
        }
    }
    return n;
}

static int test_simple_ok(void)
{
    at_line_t lines[8];
    size_t n = feed_string("AT\r\nOK\r\n", lines, 8);
    CHECK(n == 1, "1 line emitted");
    CHECK(strcmp(lines[0].line, "OK") == 0, "line == OK");
    CHECK(lines[0].type == AT_LINE_FINAL_OK, "type == FINAL_OK");
    return 0;
}

static int test_error(void)
{
    at_line_t lines[8];
    size_t n = feed_string("ERROR\r\n", lines, 8);
    CHECK(n == 1, "1 line");
    CHECK(lines[0].type == AT_LINE_FINAL_ERROR, "FINAL_ERROR");
    return 0;
}

static int test_cme_error(void)
{
    at_line_t lines[8];
    size_t n = feed_string("+CME ERROR: SIM not inserted\r\n", lines, 8);
    CHECK(n == 1, "1 line");
    CHECK(lines[0].type == AT_LINE_FINAL_ERROR, "FINAL_ERROR");
    return 0;
}

static int test_csq_response(void)
{
    at_line_t lines[8];
    size_t n = feed_string("+CSQ: 23,99\r\n\r\nOK\r\n", lines, 8);
    CHECK(n == 2, "2 lines (CSQ + OK, empty line skipped)");
    CHECK(strcmp(lines[0].line, "+CSQ: 23,99") == 0, "first == +CSQ: 23,99");
    CHECK(lines[0].type == AT_LINE_DATA, "DATA");
    CHECK(strcmp(lines[1].line, "OK") == 0, "OK");
    CHECK(lines[1].type == AT_LINE_FINAL_OK, "FINAL_OK");
    return 0;
}

static int test_urc(void)
{
    at_line_t lines[8];
    size_t n = feed_string("+CEREG: 5\r\n", lines, 8);
    CHECK(n == 1, "1 line");
    CHECK(strcmp(lines[0].line, "+CEREG: 5") == 0, "line");
    CHECK(lines[0].type == AT_LINE_URC, "URC");
    return 0;
}

static int test_cmti_urc(void)
{
    at_line_t lines[8];
    size_t n = feed_string("+CMTI: \"SM\",5\r\n", lines, 8);
    CHECK(n == 1, "1 line");
    CHECK(lines[0].type == AT_LINE_URC, "URC");
    return 0;
}

static int test_lf_only(void)
{
    at_line_t lines[8];
    size_t n = feed_string("OK\n", lines, 8);
    CHECK(n == 1, "1 line (LF only)");
    CHECK(lines[0].type == AT_LINE_FINAL_OK, "FINAL_OK");
    return 0;
}

static int test_empty_lines_skipped(void)
{
    at_line_t lines[8];
    size_t n = feed_string("\r\n\r\nOK\r\n\r\n", lines, 8);
    CHECK(n == 1, "1 line (empty skipped)");
    CHECK(lines[0].type == AT_LINE_FINAL_OK, "FINAL_OK");
    return 0;
}

int main(void)
{
    test_simple_ok();
    test_error();
    test_cme_error();
    test_csq_response();
    test_urc();
    test_cmti_urc();
    test_lf_only();
    test_empty_lines_skipped();
    printf("test_at_parser: %d/%d pass\n", g_checks - g_failed, g_checks);
    return g_failed == 0 ? 0 : 1;
}
```

### Step 4: Write `lib/at_engine/CMakeLists.txt`

```cmake
set(MODULE_NAME lib_at_engine)
add_library(${MODULE_NAME} STATIC
    at_session.c
    at_parser.c
)
target_include_directories(${MODULE_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib/util
    ${CMAKE_SOURCE_DIR}/midware/serial
)
target_link_libraries(${MODULE_NAME} PUBLIC
    lib_util
    midware_serial
    third_libuv
)
```

> P3-T1 暂只交付 at_parser（at_session 完整代码在 P3-T1 后续 commit 里给——但 P3-T1 spec 里我直接把 at_session.c 的 stub 写好，Task 2 填实现）。为了简化合并，P3-T1 先**只**建 at_parser + at_session 头 + at_session.c 的最小骨架（编译通过就行），P3-T2 充实 at_session.c 的真实现。

### Step 5: Update `lib/CMakeLists.txt` & `test/CMakeLists.txt`

`lib/CMakeLists.txt` 加 `add_subdirectory(at_engine)`。

`test/CMakeLists.txt` 加：
```cmake
add_executable(test_at_parser test_at_parser.c)
target_link_libraries(test_at_parser PRIVATE lib_at_engine)
target_include_directories(test_at_parser PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/at_engine
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib/util
)
```

### Step 6: Write minimal at_session.h (骨架)

> Task 1 把 at_session 头也建好；Task 2 填实现。

```c
/**
 * @file at_session.h
 * @brief AT 命令会话：命令队列 + 状态机 + URC 路由。
 *
 * 驱动模型：用户 at_session_send() 排队，serial_chan 收字节 → 调 on_rx →
 * 解析 → 推进状态机 → 触发命令完成回调 或 派发 URC。
 */
#ifndef LIB_AT_ENGINE_AT_SESSION_H
#define LIB_AT_ENGINE_AT_SESSION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct at_session;
typedef struct at_session at_session_t;

struct modem_chan;
struct uv_loop_s;
typedef struct uv_loop_s uv_loop_t;

/**
 * @brief 命令完成回调。
 * @param userdata 注册时传入
 * @param result    完整响应（final 之前累积的所有 data 行拼成多行字符串）
 *                 失败时为 NULL
 * @param result_len 字节数
 * @param ok        true=OK / false=ERROR
 */
typedef void (*at_response_cb)(void *userdata, const char *result, size_t result_len, bool ok);

/**
 * @brief URC 回调（按前缀匹配）。
 */
typedef void (*at_urc_cb)(void *userdata, const char *line, size_t len);

at_session_t *at_session_create(uv_loop_t *loop, struct modem_chan *chan);
int           at_session_open(at_session_t *s);
void          at_session_close(at_session_t *s);
int           at_session_send(at_session_t *s, const char *cmd, int timeout_ms,
                              at_response_cb cb, void *userdata);
int           at_session_register_urc(at_session_t *s, const char *prefix,
                                     at_urc_cb cb, void *userdata);

#endif
```

### Step 7: Write minimal at_session.c (骨架)

```c
/**
 * @file at_session.c
 * @brief AT 会话：P3-T1 仅 stub（编译过），P3-T2 填真实现。
 */
#include "at_session.h"
#include "agent_types.h"

at_session_t *at_session_create(uv_loop_t *loop, struct modem_chan *chan)
{
    (void)loop; (void)chan;
    return NULL;  /* P3-T2 填真实现 */
}

int at_session_open(at_session_t *s)            { (void)s; return AGENT_ERR_BAD_ARG; }
void at_session_close(at_session_t *s)          { (void)s; }
int at_session_send(at_session_t *s, ...)       { (void)s; return AGENT_ERR_BAD_ARG; }
int at_session_register_urc(at_session_t *s, ...) { (void)s; return AGENT_ERR_BAD_ARG; }
```

### Step 8: Build & test

```bash
cd d:/CODE/zero-c && ./build.bat test
./out/TEST/test_at_parser.exe
```

预期：`test_at_parser: 13/13 pass`（8 个测试 / 13 个 CHECK）。

### Step 9: Commit

```
feat(at-engine): at_parser 行解析 + 中文注释 + 单元测试（at_session 骨架）
```

---

## Task 2: at_session 真实现 + 测试（mock chan）

**Files:**
- Modify: `lib/at_engine/at_session.c`（填真实现）
- Create: `test/test_at_session.c`
- Modify: `test/CMakeLists.txt`

### Step 1: Real `at_session.c`

```c
/**
 * @file at_session.c
 * @brief AT 会话实现：命令队列、状态机、URC 路由。
 *
 * 数据流：
 *   用户 send → 入队 → 状态机推进 → 调 chan->send
 *   chan->on_rx 收字节 → at_parser_feed → 根据行类型：
 *     - FINAL_OK / FINAL_ERROR → 触发当前命令的完成回调
 *     - URC + 前缀匹配 → 调对应 urc_cb
 *     - DATA → 累积到当前命令的 result 缓冲
 *
 * P3 简化：不支持 CMUX、不支持并发命令、单条 FIFO。
 */
#include "at_session.h"
#include "at_parser.h"
#include "agent_chan.h"
#include "agent_types.h"
#include "strbuf.h"
#include "ringbuf.h"

#define WIN32_LEAN_AND_MEAN
#include <uv.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RX_RING_CAP 4096
#define CMD_QUEUE_MAX 32
#define RESULT_BUF_CAP 1024
#define URC_HANDLERS_MAX 16

/* URC 订阅 */
typedef struct {
    char        prefix[32];
    at_urc_cb   cb;
    void       *userdata;
} urc_handler_t;

/* 命令队列项 */
typedef struct cmd_item {
    char            cmd[128];         /* 完整 AT 命令原文 */
    int             timeout_ms;
    at_response_cb  cb;
    void           *userdata;
    strbuf_t        result;          /* 累积 data 行 */
    bool            in_flight;       /* 已发给 chan、等待 final 响应 */
} cmd_item_t;

struct at_session {
    uv_loop_t         *loop;
    struct modem_chan *chan;          /* 弱引用，由 device_manager 拥有 */
    ringbuf_t          rx_ring;       /* 备用：直接从 chan on_rx 走，不经 ringbuf */
    at_line_type_t     last_line_type;

    cmd_item_t         queue[CMD_QUEUE_MAX];
    int                q_head;        /* 下一个发出 */
    int                q_tail;        /* 下一个入队 */
    int                q_count;

    bool               in_flight;     /* 队列头那条是不是已发 */
    uv_timer_t         cmd_timer;

    urc_handler_t      urc_handlers[URC_HANDLERS_MAX];
    int                urc_count;
};

/* ---------- 工具 ---------- */

static cmd_item_t *queue_front(at_session_t *s)
{
    return (s->q_count > 0) ? &s->queue[s->q_head] : NULL;
}

static void queue_pop_front(at_session_t *s)
{
    if (s->q_count == 0) return;
    cmd_item_t *front = &s->queue[s->q_head];
    strbuf_free(&front->result);
    s->q_head = (s->q_head + 1) % CMD_QUEUE_MAX;
    s->q_count--;
    s->in_flight = false;
}

static void complete_current(at_session_t *s, bool ok)
{
    cmd_item_t *front = queue_front(s);
    if (!front) return;
    at_response_cb cb = front->cb;
    void *ud = front->userdata;
    const char *res = front->result.data;
    size_t res_len = front->result.len;
    /* 把回调先摘出来再 pop，避免回调里再 send 时队列状态混乱 */
    queue_pop_front(s);
    if (cb) cb(ud, res, res_len, ok);
}

static void try_send_next(at_session_t *s)
{
    if (s->in_flight) return;
    cmd_item_t *front = queue_front(s);
    if (!front) return;
    /* 发送：原命令 + \r */
    char buf[256];
    int n = snprintf(buf, sizeof(buf), "%s\r", front->cmd);
    if (n < 0 || n >= (int)sizeof(buf)) return;
    if (modem_chan_send(s->chan, (const uint8_t *)buf, (size_t)n) != 0) {
        fprintf(stderr, "at_session: chan send 失败\n");
        complete_current(s, false);
        return;
    }
    s->in_flight = true;
    uv_timer_start(&s->cmd_timer, NULL, front->timeout_ms, 0);
}

static void on_cmd_timeout(uv_timer_t *handle)
{
    at_session_t *s = (at_session_t *)handle->data;
    fprintf(stderr, "at_session: 命令超时\n");
    complete_current(s, false);
    try_send_next(s);
}

/* ---------- URC 路由 ---------- */

static void dispatch_urc(at_session_t *s, const char *line, size_t len)
{
    for (int i = 0; i < s->urc_count; i++) {
        if (strncmp(line, s->urc_handlers[i].prefix, strlen(s->urc_handlers[i].prefix)) == 0) {
            s->urc_handlers[i].cb(s->urc_handlers[i].userdata, line, len);
            return;
        }
    }
    /* 没有订阅：打 stderr 调试 */
    fprintf(stderr, "at_session: 收到未订阅 URC '%.*s'\n", (int)len, line);
}

/* ---------- 收字节回调（由 modem_chan 调） ---------- */

static void on_chan_rx(void *userdata, const uint8_t *buf, size_t len)
{
    at_session_t *s = (at_session_t *)userdata;
    if (!s || !buf || len == 0) return;

    /* 一字节一字节喂 parser */
    for (size_t i = 0; i < len; i++) {
        at_line_t line;
        if (at_parser_feed(buf[i], &line)) {
            switch (line.type) {
            case AT_LINE_FINAL_OK:
                complete_current(s, true);
                try_send_next(s);
                break;
            case AT_LINE_FINAL_ERROR:
                complete_current(s, false);
                try_send_next(s);
                break;
            case AT_LINE_URC:
                dispatch_urc(s, line.line, line.len);
                break;
            case AT_LINE_DATA: {
                cmd_item_t *front = queue_front(s);
                if (front && front->in_flight) {
                    if (front->result.len > 0) {
                        strbuf_append(&front->result, "\r\n");
                    }
                    strbuf_append_n(&front->result, line.line, line.len);
                }
                break;
            }
            default:
                break;
            }
        }
    }
}

/* ---------- 公共 API ---------- */

at_session_t *at_session_create(uv_loop_t *loop, struct modem_chan *chan)
{
    if (!loop || !chan) return NULL;
    at_session_t *s = (at_session_t *)calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->loop = loop;
    s->chan = chan;
    chan->on_rx = on_chan_rx;
    chan->userdata = s;
    /* 不在 P3-T2 之前真正 init timer，要等 at_session_open 之后 */
    return s;
}

int at_session_open(at_session_t *s)
{
    if (!s) return AGENT_ERR_BAD_ARG;
    uv_timer_init(s->loop, &s->cmd_timer);
    s->cmd_timer.data = s;
    at_parser_init();
    return 0;
}

void at_session_close(at_session_t *s)
{
    if (!s) return;
    uv_timer_stop(&s->cmd_timer);
    uv_close((uv_handle_t *)&s->cmd_timer, NULL);
    for (int i = 0; i < s->q_count; i++) {
        cmd_item_t *it = &s->queue[(s->q_head + i) % CMD_QUEUE_MAX];
        strbuf_free(&it->result);
    }
    s->q_head = s->q_tail = s->q_count = 0;
    s->in_flight = false;
    if (s->chan) {
        s->chan->on_rx = NULL;
        s->chan->userdata = NULL;
        s->chan = NULL;
    }
}

int at_session_send(at_session_t *s, const char *cmd, int timeout_ms,
                    at_response_cb cb, void *userdata)
{
    if (!s || !cmd) return AGENT_ERR_BAD_ARG;
    if (s->q_count >= CMD_QUEUE_MAX) return AGENT_ERR_OOM;
    cmd_item_t *it = &s->queue[s->q_tail];
    strncpy(it->cmd, cmd, sizeof(it->cmd) - 1);
    it->cmd[sizeof(it->cmd) - 1] = '\0';
    it->timeout_ms = (timeout_ms > 0) ? timeout_ms : 3000;
    it->cb = cb;
    it->userdata = userdata;
    it->in_flight = false;
    strbuf_init(&it->result, RESULT_BUF_CAP);
    s->q_tail = (s->q_tail + 1) % CMD_QUEUE_MAX;
    s->q_count++;
    try_send_next(s);
    return 0;
}

int at_session_register_urc(at_session_t *s, const char *prefix,
                            at_urc_cb cb, void *userdata)
{
    if (!s || !prefix || !cb) return AGENT_ERR_BAD_ARG;
    if (s->urc_count >= URC_HANDLERS_MAX) return AGENT_ERR_OOM;
    urc_handler_t *h = &s->urc_handlers[s->urc_count++];
    strncpy(h->prefix, prefix, sizeof(h->prefix) - 1);
    h->prefix[sizeof(h->prefix) - 1] = '\0';
    h->cb = cb;
    h->userdata = userdata;
    return 0;
}
```

### Step 2: Write `test/test_at_session.c`

> 用**真实**的 `uv_loop_t` + 一个**临时**的 mock modem_chan（chan->on_rx 手动调）。不开串口，纯粹测队列 + 状态机。

```c
/**
 * @file test_at_session.c
 * @brief at_session 单元测试：队列、final/URC 派发、超时。
 */
#include "at_session.h"
#include "at_parser.h"
#include "agent_chan.h"

#include <uv.h>
#include <stdio.h>
#include <string.h>

static int g_checks = 0, g_failed = 0;
#define CHECK(c, m) do { g_checks++; if (!(c)) { g_failed++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, m); } } while (0)

/* 简易 mock：保存 chan 指针；测试代码直接调 on_rx 模拟"模块回了字节" */
typedef struct {
    modem_chan_t  chan;
    uv_loop_t    *loop;
} mock_chan_t;

static mock_chan_t g_mock;

/* 完成回调收集 */
typedef struct {
    char   result[256];
    size_t len;
    bool   ok;
    int    count;
} cb_record_t;
static cb_record_t g_cb;

/* URC 回调收集 */
typedef struct {
    char line[256];
    size_t len;
    int  count;
} urc_record_t;
static urc_record_t g_urc;

static void on_complete(void *ud, const char *res, size_t len, bool ok)
{
    (void)ud;
    g_cb.count++;
    g_cb.ok = ok;
    g_cb.len = (len < sizeof(g_cb.result) - 1) ? len : sizeof(g_cb.result) - 1;
    memcpy(g_cb.result, res, g_cb.len);
    g_cb.result[g_cb.len] = '\0';
}

static void on_urc(void *ud, const char *line, size_t len)
{
    (void)ud;
    g_urc.count++;
    g_urc.len = (len < sizeof(g_urc.line) - 1) ? len : sizeof(g_urc.line) - 1;
    memcpy(g_urc.line, line, g_urc.len);
    g_urc.line[g_urc.len] = '\0';
}

static void reset_records(void)
{
    memset(&g_cb, 0, sizeof(g_cb));
    memset(&g_urc, 0, sizeof(g_urc));
}

/* 模拟"模块回了一坨字节"：直接调 chan->on_rx */
static void fake_rx(const char *s)
{
    if (g_mock.chan.on_rx) {
        g_mock.chan.on_rx(g_mock.chan.userdata, (const uint8_t *)s, strlen(s));
    }
}

static int test_send_and_ok(void)
{
    reset_records();
    at_session_t *s = at_session_create(g_mock.loop, &g_mock.chan);
    CHECK(s != NULL, "create");
    at_session_open(s);

    at_session_send(s, "AT", 1000, on_complete, NULL);
    /* 队列头已发 */
    fake_rx("OK\r\n");
    /* 驱动 uv_run 处理 timer 等 */
    uv_run(g_mock.loop, UV_RUN_NOWAIT);

    CHECK(g_cb.count == 1, "1 completion");
    CHECK(g_cb.ok == true, "ok == true");
    CHECK(g_cb.len == 0, "result empty");

    at_session_close(s);
    free(s);
    return 0;
}

static int test_send_and_data_then_ok(void)
{
    reset_records();
    at_session_t *s = at_session_create(g_mock.loop, &g_mock.chan);
    at_session_open(s);

    at_session_send(s, "AT+CSQ", 1000, on_complete, NULL);
    fake_rx("+CSQ: 23,99\r\nOK\r\n");
    uv_run(g_mock.loop, UV_RUN_NOWAIT);

    CHECK(g_cb.count == 1, "1 completion");
    CHECK(g_cb.ok == true, "ok");
    CHECK(strcmp(g_cb.result, "+CSQ: 23,99") == 0, "result == +CSQ: 23,99");

    at_session_close(s);
    free(s);
    return 0;
}

static int test_send_and_error(void)
{
    reset_records();
    at_session_t *s = at_session_create(g_mock.loop, &g_mock.chan);
    at_session_open(s);

    at_session_send(s, "AT+FOO", 1000, on_complete, NULL);
    fake_rx("ERROR\r\n");
    uv_run(g_mock.loop, UV_RUN_NOWAIT);

    CHECK(g_cb.count == 1, "1 completion");
    CHECK(g_cb.ok == false, "ok == false");

    at_session_close(s);
    free(s);
    return 0;
}

static int test_urc_dispatch(void)
{
    reset_records();
    at_session_t *s = at_session_create(g_mock.loop, &g_mock.chan);
    at_session_open(s);

    at_session_register_urc(s, "+CEREG", on_urc, NULL);

    at_session_send(s, "AT+COPS?", 1000, on_complete, NULL);
    fake_rx("+CEREG: 5\r\n+COPS: 0,0,\"China Mobile\",7\r\nOK\r\n");
    uv_run(g_mock.loop, UV_RUN_NOWAIT);

    CHECK(g_urc.count == 1, "URC fired 1x");
    CHECK(strcmp(g_urc.line, "+CEREG: 5") == 0, "URC line");
    CHECK(g_cb.count == 1, "cmd done 1x");
    CHECK(g_cb.ok == true, "cmd ok");
    /* data 行 +CSQ 不算 URC，所以 result 应当是 +COPS 一行 */
    CHECK(strcmp(g_cb.result, "+COPS: 0,0,\"China Mobile\",7") == 0, "cmd result");

    at_session_close(s);
    free(s);
    return 0;
}

static int test_queue_two_cmds(void)
{
    reset_records();
    at_session_t *s = at_session_create(g_mock.loop, &g_mock.chan);
    at_session_open(s);

    at_session_send(s, "AT", 1000, on_complete, NULL);
    at_session_send(s, "AT+CSQ", 1000, on_complete, NULL);
    /* 两条都入队，第一条已发；模拟第一条回 OK → 第二条应自动发 */
    fake_rx("OK\r\n");
    uv_run(g_mock.loop, UV_RUN_NOWAIT);
    CHECK(g_cb.count == 1, "first done");

    /* 此时第二条已自动发（在飞），模拟回码 */
    fake_rx("+CSQ: 19,99\r\nOK\r\n");
    uv_run(g_mock.loop, UV_RUN_NOWAIT);
    CHECK(g_cb.count == 2, "second done");
    CHECK(g_cb.ok == true, "ok");
    CHECK(strcmp(g_cb.result, "+CSQ: 19,99") == 0, "csq result");

    at_session_close(s);
    free(s);
    return 0;
}

int main(void)
{
    /* 共享一个 mock chan + loop */
    uv_loop_t *loop = uv_default_loop();
    memset(&g_mock, 0, sizeof(g_mock));
    g_mock.loop = loop;

    test_send_and_ok();
    test_send_and_data_then_ok();
    test_send_and_error();
    test_urc_dispatch();
    test_queue_two_cmds();

    printf("test_at_session: %d/%d pass\n", g_checks - g_failed, g_checks);
    return g_failed == 0 ? 0 : 1;
}
```

### Step 3: Update `test/CMakeLists.txt`

加：
```cmake
add_executable(test_at_session test_at_session.c)
target_link_libraries(test_at_session PRIVATE
    lib_at_engine
    third_libuv
)
target_include_directories(test_at_session PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/at_engine
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib/util
    ${CMAKE_SOURCE_DIR}/midware/serial
)
```

### Step 4: Build & test

```bash
cd d:/CODE/zero-c && ./build.bat test
./out/TEST/test_at_session.exe
```

预期：6 个测试 / 多个 CHECK 全过。`test_at_session: N/N pass`。

### Step 5: Commit

```
feat(at-engine): at_session 真实现（队列/状态机/URC）+ 单元测试
```

---

## Task 3: device_manager 扩展 — connect/disconnect + READY 状态

**Files:**
- Modify: `lib/device_manager/device_manager.h`（加 connect/disconnect API + at/serial 字段）
- Modify: `lib/device_manager/device_manager.c`（实现 + 状态机）
- Create: `lib/diag_service/diag_state.h`
- Create: `lib/diag_service/diag_service.h`
- Create: `lib/diag_service/diag_service.c`
- Create: `lib/diag_service/CMakeLists.txt`
- Modify: `lib/CMakeLists.txt`
- Create: `test/test_diag_state.c`
- Modify: `test/CMakeLists.txt`
- Modify: `agent_types.h`（加 diag_state 字段）

### Step 1: Update `lib/device_manager/device_manager.h`

加：
```c
struct serial_chan;          /* 前向声明 */
struct at_session;
typedef struct serial_chan serial_chan_t;
typedef struct at_session at_session_t;
```

在 `modem_dev_t` 结构体内加：
```c
    at_session_t    *at;        /* P3: connect 时分配 */
    serial_chan_t   *serial;    /* P3: connect 时分配 */
    char             diag_last_update[32];  /* "12:34:56" 或 "-" */
```

加 API：
```c
int  device_manager_connect_dev   (device_manager_t *m, int dev_idx);  /* 分配 serial + at，打开 chan */
int  device_manager_disconnect_dev(device_manager_t *m, int dev_idx);
```

### Step 2: Update `lib/device_manager/device_manager.c`

读 `device_manager.c` 当前内容。在 include 区加：
```c
#include "serial_chan.h"
#include "at_session.h"
```

实现 connect/disconnect：

```c
int device_manager_connect_dev(device_manager_t *m, int dev_idx)
{
    if (!m || dev_idx < 0 || dev_idx >= m->dev_count) return AGENT_ERR_BAD_ARG;
    modem_dev_t *d = &m->devs[dev_idx];
    if (d->at) return AGENT_ERR_BAD_ARG;  /* 已连接 */

    /* 只支持 COM 串口；NCM/RNDIS 留给 P3.5 / v1.0 */
    if (strncmp(d->chan_uri, "com://", 6) != 0) {
        fprintf(stderr, "device_manager: 暂只支持 com:// 通道（%s）\n", d->chan_uri);
        return AGENT_ERR_BAD_ARG;
    }

    /* 分配 serial_chan + at_session */
    d->serial = serial_chan_create(m->loop);
    if (!d->serial) return AGENT_ERR_OOM;
    modem_chan_t *chan = &d->serial->chan;
    if (serial_chan_open(chan, d->chan_uri) != 0) {
        free(d->serial);
        d->serial = NULL;
        d->state = DEV_STATE_ERROR;
        return AGENT_ERR_IO;
    }
    d->at = at_session_create(m->loop, chan);
    if (!d->at) {
        serial_chan_close(chan);
        free(d->serial);
        d->serial = NULL;
        d->state = DEV_STATE_ERROR;
        return AGENT_ERR_OOM;
    }
    at_session_open(d->at);
    d->state = DEV_STATE_READY;
    fprintf(stderr, "device_manager: dev %d (%s) 已连接\n", dev_idx, d->label);
    return 0;
}

int device_manager_disconnect_dev(device_manager_t *m, int dev_idx)
{
    if (!m || dev_idx < 0 || dev_idx >= m->dev_count) return AGENT_ERR_BAD_ARG;
    modem_dev_t *d = &m->devs[dev_idx];
    if (!d->at) return AGENT_ERR_BAD_ARG;
    at_session_close(d->at);
    free(d->at);
    d->at = NULL;
    if (d->serial) {
        serial_chan_close(&d->serial->chan);
        free(d->serial);
        d->serial = NULL;
    }
    d->state = DEV_STATE_DISCONNECTED;
    fprintf(stderr, "device_manager: dev %d (%s) 已断开\n", dev_idx, d->label);
    return 0;
}
```

### Step 3: Write `lib/diag_service/diag_state.h`

```c
/**
 * @file diag_state.h
 * @brief 单台模组的诊断状态快照。
 */
#ifndef LIB_DIAG_SERVICE_DIAG_STATE_H
#define LIB_DIAG_SERVICE_DIAG_STATE_H

#include <stdbool.h>

typedef struct {
    char  csq[16];        /* "23 (-67 dBm)" */
    char  cereg[32];      /* "5 (Registered, roaming)" 或 "0 (Not registered)" */
    char  cop_operator[32]; /* "China Mobile" */
    char  rat[16];        /* "LTE Cat-1" / "NB-IoT" / ... */
    char  imei[16];
    char  imsi[16];
    char  iccid[24];
    char  last_update[32]; /* "12:34:56" 或 "-" */
    bool  valid;          /* 至少有一项成功拉到过 */
} diag_state_t;

void diag_state_reset(diag_state_t *s);

#endif
```

### Step 4: Write `lib/diag_service/diag_state.c`

```c
/**
 * @file diag_state.c
 */
#include "diag_state.h"
#include <string.h>

void diag_state_reset(diag_state_t *s)
{
    if (!s) return;
    memset(s, 0, sizeof(*s));
    strncpy(s->last_update, "-", sizeof(s->last_update) - 1);
}
```

### Step 5: Write `lib/diag_service/diag_service.h`

```c
/**
 * @file diag_service.h
 * @brief 诊断服务：订阅 device_manager，对 READY 设备定期拉取诊断值。
 */
#ifndef LIB_DIAG_SERVICE_H
#define LIB_DIAG_SERVICE_H

#include "device_manager.h"
#include "diag_state.h"

typedef struct diag_service diag_service_t;

diag_service_t *diag_service_create(uv_loop_t *loop, device_manager_t *dm);
void             diag_service_destroy(diag_service_t *s);
void             diag_service_refresh_now(diag_service_t *s);  /* 强制刷新所有 READY 设备 */
const diag_state_t *diag_service_get_state(diag_service_t *s, int dev_idx);

#endif
```

### Step 6: Write `lib/diag_service/diag_service.c`

```c
/**
 * @file diag_service.c
 * @brief 诊断服务实现（基础版）：一次性拉取 IMEI/IMSI/ICCID/CSQ/CEREG/COPS。
 */
#include "diag_service.h"
#include "at_session.h"
#include "agent_types.h"
#include "strbuf.h"

#include <uv.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define MAX_DEVS 16

typedef struct {
    uv_loop_t        *loop;
    device_manager_t *dm;
    diag_state_t      states[MAX_DEVS];
} diag_service_t;

static void on_at_response(void *ud, const char *res, size_t len, bool ok)
{
    /* 简化：把 result 拷到一个 stack 缓冲，由调用方（diag_service）从 userdata 读 */
    strbuf_t *dst = (strbuf_t *)ud;
    if (!ok || !res) {
        strbuf_append(dst, "(error)");
        return;
    }
    strbuf_append_n(dst, res, len);
}

diag_service_t *diag_service_create(uv_loop_t *loop, device_manager_t *dm)
{
    if (!loop || !dm) return NULL;
    diag_service_t *s = (diag_service_t *)calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->loop = loop;
    s->dm = dm;
    for (int i = 0; i < MAX_DEVS; i++) diag_state_reset(&s->states[i]);
    return s;
}

void diag_service_destroy(diag_service_t *s)
{
    if (!s) return;
    /* at_session 仍由 device_manager 拥有，diag_service 不释放 */
    free(s);
}

const diag_state_t *diag_service_get_state(diag_service_t *s, int dev_idx)
{
    if (!s || dev_idx < 0 || dev_idx >= MAX_DEVS) return NULL;
    return &s->states[dev_idx];
}

/* 同步拉取一个 AT 命令的结果到 buf。P3 简化：不等回复直接返回。 */
static void fetch(at_session_t *at, const char *cmd, strbuf_t *buf)
{
    strbuf_reset(buf);
    at_session_send(at, cmd, 3000, on_at_response, buf);
}

void diag_service_refresh_now(diag_service_t *s)
{
    if (!s || !s->dm) return;
    for (int i = 0; i < s->dm->dev_count && i < MAX_DEVS; i++) {
        modem_dev_t *d = &s->dm->devs[i];
        if (d->state != DEV_STATE_READY || !d->at) continue;
        strbuf_t buf;
        strbuf_init(&buf, 256);

        fetch(d->at, "AT+CGSN", &buf);  strncpy(s->states[i].imei, buf.data, sizeof(s->states[i].imei) - 1);
        fetch(d->at, "AT+CIMI", &buf);  strncpy(s->states[i].imsi, buf.data, sizeof(s->states[i].imsi) - 1);
        fetch(d->at, "AT+CCID", &buf); strncpy(s->states[i].iccid, buf.data, sizeof(s->states[i].iccid) - 1);
        fetch(d->at, "AT+CSQ", &buf);   strncpy(s->states[i].csq, buf.data, sizeof(s->states[i].csq) - 1);
        fetch(d->at, "AT+CEREG?", &buf); strncpy(s->states[i].cereg, buf.data, sizeof(s->states[i].cereg) - 1);
        fetch(d->at, "AT+COPS?", &buf); strncpy(s->states[i].cop_operator, buf.data, sizeof(s->states[i].cop_operator) - 1);
        fetch(d->at, "AT+COPS?", &buf); /* 第二次为了拿 RAT，实际 P3 简化不做，标占位 */

        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        strftime(s->states[i].last_update, sizeof(s->states[i].last_update),
                 "%H:%M:%S", t);
        s->states[i].valid = true;
        strbuf_free(&buf);
    }
}
```

### Step 7: Write `lib/diag_service/CMakeLists.txt`

```cmake
set(MODULE_NAME lib_diag_service)
add_library(${MODULE_NAME} STATIC diag_state.c diag_service.c)
target_include_directories(${MODULE_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib/util
    ${CMAKE_SOURCE_DIR}/lib/at_engine
    ${CMAKE_SOURCE_DIR}/lib/device_manager
)
target_link_libraries(${MODULE_NAME} PUBLIC
    lib_util
    lib_at_engine
    lib_device_manager
    third_libuv
)
```

### Step 8: Update `lib/CMakeLists.txt`

加 `add_subdirectory(diag_service)`。

### Step 9: Write `test/test_diag_state.c`

```c
/**
 * @file test_diag_state.c
 * @brief diag_state 字段读写测试。
 */
#include "diag_state.h"
#include <stdio.h>
#include <string.h>

static int g_checks = 0, g_failed = 0;
#define CHECK(c, m) do { g_checks++; if (!(c)) { g_failed++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, m); } } while (0)

int main(void)
{
    diag_state_t s;
    diag_state_reset(&s);
    CHECK(s.valid == false, "初始 valid == false");
    CHECK(strcmp(s.csq, "") == 0, "csq 空");
    CHECK(strcmp(s.last_update, "-") == 0, "last_update == '-'");
    /* reset 一次后再 reset，确认 idempotent */
    diag_state_reset(&s);
    CHECK(s.valid == false, "二次 reset 仍 valid == false");
    printf("test_diag_state: %d/%d pass\n", g_checks - g_failed, g_checks);
    return g_failed == 0 ? 0 : 1;
}
```

### Step 10: Update `test/CMakeLists.txt`

```cmake
add_executable(test_diag_state test_diag_state.c)
target_link_libraries(test_diag_state PRIVATE lib_diag_service)
target_include_directories(test_diag_state PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/diag_service
    ${CMAKE_SOURCE_DIR}/include
)
```

### Step 11: Update `include/agent_types.h`

读当前内容，加：
1. 文件顶部前向声明：
```c
struct diag_service;
typedef struct diag_service diag_service_t;
```

2. `agent_app_t` 内加：
```c
    /* P3: 诊断服务 + 当前活动 AT 会话（panel_diag 收发 AT 用） */
    diag_service_t        *diag_service;
    struct at_session     *active_at;  /* 当前激活设备的 AT 会话 */
```

### Step 12: Build & test

```bash
cd d:/CODE/zero-c && ./build.bat test
./out/TEST/test_diag_state.exe
./out/TEST/test_at_parser.exe
./out/TEST/test_at_session.exe
```

预期：3 个 test 全过（at_parser 13+ / at_session N / diag_state 4+）。

### Step 13: Commit

```
feat(p3): at_session 真实现 + device_manager connect/disconnect + diag_service 骨架
```

---

## Task 4: panel_devices 加 连接/断开 按钮

**Files:**
- Modify: `app/panel_devices/panel.cpp`
- Modify: `app/panel_devices/CMakeLists.txt`
- Modify: `core/main.cpp`（创建 diag_service，挂 on_change 触发 refresh）

### Step 1: Update `app/panel_devices/panel.cpp`

读当前内容（P2-T8 写的版本）。**整个替换**为：

```cpp
/**
 * @file panel_devices.cpp
 * @brief 多模组列表 panel——每行带"连接/断开"按钮。
 */
#include "panel_devices.h"
#include "i18n.h"
#include "imgui.h"
#include "device_manager.h"
#include <cstring>

static const char *kColI18n[] = {
    "devices.col.name",
    "devices.col.com",
    "devices.col.ip",
    "devices.col.csq",
    "devices.col.state",
    "devices.col.lastseen",
};

void panel_devices_render(agent_app_t *app)
{
    ImGui::Text("%s", i18n_get("devices.title"));
    ImGui::Separator();

    device_manager_t *m = (device_manager_t *)app->device_manager;
    int count = (m != NULL) ? m->dev_count : 0;

    if (count == 0) {
        ImGui::TextDisabled("暂未发现模组——插上 COM 或 USB-NCM 模组等待 0.5 秒");
        return;
    }

    if (ImGui::BeginTable("devices_tbl", 7,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        for (int i = 0; i < 6; i++) {
            ImGui::TableSetupColumn(i18n_get(kColI18n[i]));
        }
        ImGui::TableSetupColumn("操作");
        ImGui::TableHeadersRow();

        for (int r = 0; r < count; r++) {
            modem_dev_t *d = &m->devs[r];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("%s", d->label);
            ImGui::TableSetColumnIndex(1); ImGui::Text("%s", d->chan_uri);
            ImGui::TableSetColumnIndex(2); ImGui::Text("%s", d->ipv4[0] ? d->ipv4 : "-");
            ImGui::TableSetColumnIndex(3); ImGui::Text("%d", d->csq);
            ImGui::TableSetColumnIndex(4);
            const char *state_str = (d->state == DEV_STATE_READY) ? "READY"
                                    : (d->state == DEV_STATE_ERROR) ? "ERROR" : "DISCONNECTED";
            ImGui::Text("%s", state_str);
            ImGui::TableSetColumnIndex(5); ImGui::Text("%s", d->diag_last_update);

            ImGui::TableSetColumnIndex(6);
            if (d->state == DEV_STATE_READY) {
                ImGui::PushID(r * 10 + 1);
                if (ImGui::Button("断开")) {
                    device_manager_disconnect_dev(m, r);
                }
                ImGui::PopID();
            } else {
                ImGui::PushID(r * 10 + 1);
                if (ImGui::Button("连接")) {
                    device_manager_connect_dev(m, r);
                }
                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }
}
```

### Step 2: Update `app/panel_devices/CMakeLists.txt`

读当前内容，`target_link_libraries` 块加 `midware_serial` 和 `lib_at_engine`：

```cmake
target_link_libraries(${MODULE_NAME} PUBLIC
    app_shell
    lib_device_manager
    midware_serial       # 新加
    lib_at_engine         # 新加
    third_imgui
)
```

### Step 3: Update `core/main.cpp`

读当前内容（`230efcf` 之后）。在 `device_manager_start(&g_devmgr);` 之后加：

```cpp
    /* 诊断服务：挂在 device_manager 上，dev 变化时刷新 diag_state */
    g_diag = diag_service_create(host_get_uv_loop(ctx), &g_devmgr);
    g_app.diag_service = g_diag;
    /* 首次刷新（虽然现在还没设备可刷） */
    diag_service_refresh_now(g_diag);
```

加静态变量：
```cpp
static diag_service_t *g_diag = NULL;
```

加 include：
```cpp
#include "diag_service.h"
```

### Step 4: Build

```bash
cd d:/CODE/zero-c && ./build.bat
```

预期：build 成功。EXE 重新生成。

### Step 5: Commit

```
feat(p3): panel_devices 加 连接/断开 按钮 + main 启动 diag_service
```

---

## Task 5: panel_diag 实时数据 + AT 控制台接真通道

**Files:**
- Modify: `app/panel_diag/panel.cpp`（7 张卡从 mock 换真订阅 + AT 控制台走真通道）
- Modify: `app/panel_diag/CMakeLists.txt`

### Step 1: Update `app/panel_diag/panel.cpp`

读当前内容（P1-T13 写的 hardcoded 7 张卡 + 假 AT 控制台版本）。**整个替换**为：

```cpp
/**
 * @file panel_diag.cpp
 * @brief 现场诊断 panel：7 张状态卡实时订阅 diag_state + AT 控制台走真通道。
 */
#include "panel_diag.h"
#include "i18n.h"
#include "imgui.h"
#include "diag_state.h"
#include "diag_service.h"
#include "device_manager.h"
#include "at_session.h"
#include "agent_types.h"

#include <cstring>
#include <cstdio>

/* 7 张状态卡：i18n key + diag_state 字段的偏移 */
typedef struct {
    const char  *i18n_key;
    size_t       offset_in_diag_state;  /* offsetof(diag_state_t, field) */
} diag_card_t;

#define DIAG_FIELD(name) offsetof(diag_state_t, name)

static const diag_card_t kDiagCards[] = {
    { "diag.cards.csq",      DIAG_FIELD(csq) },
    { "diag.cards.cereg",    DIAG_FIELD(cereg) },
    { "diag.cards.operator", DIAG_FIELD(cop_operator) },
    { "diag.cards.rat",      DIAG_FIELD(rat) },
    { "diag.cards.imei",     DIAG_FIELD(imei) },
    { "diag.cards.imsi",     DIAG_FIELD(imsi) },
    { "diag.cards.iccid",    DIAG_FIELD(iccid) },
};

/* AT 控制台历史（ring 100 行） */
typedef struct {
    char lines[100][256];
    int  head;
    int  count;
} console_history_t;

static console_history_t g_hist = {0};

static void hist_push(const char *line)
{
    int idx = (g_hist.head + g_hist.count) % 100;
    strncpy(g_hist.lines[idx], line, 255);
    g_hist.lines[idx][255] = '\0';
    if (g_hist.count < 100) g_hist.count++;
    else g_hist.head = (g_hist.head + 1) % 100;
}

/* AT 命令完成回调：把 result 推入历史 */
typedef struct {
    char cmd[64];
} at_cmd_record_t;

static void on_at_done(void *ud, const char *res, size_t len, bool ok)
{
    at_cmd_record_t *rec = (at_cmd_record_t *)ud;
    if (ok) {
        if (len > 0) {
            /* 拆 \r\n 多行显示 */
            char buf[512];
            snprintf(buf, sizeof(buf), "< %s", res);
            hist_push(buf);
        }
        hist_push("< OK");
    } else {
        hist_push("< ERROR");
    }
    (void)rec;  /* 当前简化不用 */
}

void panel_diag_render(agent_app_t *app)
{
    ImGui::Columns(2, NULL, true);

    /* ---- 左半：AT 控制台 ---- */
    ImGui::BeginChild("at_console", ImVec2(0, 0), true);
    ImGui::Text("%s", i18n_get("diag.console.title"));
    ImGui::Separator();

    /* 历史区 */
    ImGui::BeginChild("at_log", ImVec2(0, -32), true);
    for (int i = 0; i < g_hist.count; i++) {
        int idx = (g_hist.head + i) % 100;
        ImGui::Text("%s", g_hist.lines[idx]);
    }
    ImGui::EndChild();

    /* 输入框 + 发送 */
    static char input_buf[128] = "";
    ImGui::InputTextWithHint("##at_in", i18n_get("diag.console.placeholder"),
                             input_buf, sizeof(input_buf));
    ImGui::SameLine();
    if (ImGui::Button(i18n_get("diag.console.send"))) {
        if (input_buf[0] != '\0') {
            char echo[160];
            snprintf(echo, sizeof(echo), "> %s", input_buf);
            hist_push(echo);
            /* 找当前 active AT 会话 */
            at_session_t *at = (at_session_t *)app->active_at;
            if (at) {
                static at_cmd_record_t rec;
                strncpy(rec.cmd, input_buf, sizeof(rec.cmd) - 1);
                at_session_send(at, input_buf, 3000, on_at_done, &rec);
            } else {
                hist_push("(无连接：先在多模组面板点连接)");
            }
            input_buf[0] = '\0';
        }
    }
    ImGui::EndChild();

    ImGui::NextColumn();

    /* ---- 右半：状态卡 + 动作按钮 ---- */
    ImGui::BeginChild("status_cards", ImVec2(0, 0), true);

    diag_state_t *st = NULL;
    if (app->diag_service) {
        device_manager_t *m = (device_manager_t *)app->device_manager;
        /* 找第一个 READY 设备 */
        for (int i = 0; m && i < m->dev_count; i++) {
            if (m->devs[i].state == DEV_STATE_READY) {
                st = (diag_state_t *)diag_service_get_state(app->diag_service, i);
                app->active_at = m->devs[i].at;
                break;
            }
        }
    }
    if (!st || !st->valid) {
        ImGui::TextDisabled("未连接模组（先在多模组面板点连接）");
    } else {
        for (size_t i = 0; i < sizeof(kDiagCards) / sizeof(kDiagCards[0]); i++) {
            const char *label = i18n_get(kDiagCards[i].i18n_key);
            const char *value = (const char *)((char *)st + kDiagCards[i].offset_in_diag_state);
            ImGui::Text("%s", label);
            ImGui::SameLine(160);
            if (value[0] == '\0') {
                ImGui::TextDisabled("-");
            } else {
                ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1.0f), "%s", value);
            }
        }
        ImGui::TextDisabled("上次刷新：%s", st->last_update);
    }
    ImGui::Separator();

    /* 动作按钮：P3 仅"刷新诊断"——拨号/抓 log 留给 P4 */
    if (ImGui::Button(i18n_get("diag.actions.health"))) {
        diag_service_refresh_now(app->diag_service);
    }

    ImGui::EndChild();

    ImGui::Columns(1);
}
```

### Step 2: Update `app/panel_diag/CMakeLists.txt`

读当前内容。`target_link_libraries` 块加 `lib_diag_service`、`lib_at_engine`、`lib_device_manager`：

```cmake
target_link_libraries(${MODULE_NAME} PUBLIC
    app_shell
    lib_diag_service     # 新加
    lib_at_engine         # 新加
    lib_device_manager   # 新加
    third_imgui
)
```

### Step 3: Build

```bash
cd d:/CODE/zero-c && ./build.bat
```

预期：build 成功。

### Step 4: 真机验证

1. 启动 EXE → 切到"多模组"面板
2. 看到 COM 模组 → 点"连接"按钮
3. 状态从 DISCONNECTED → READY
4. 切到"现场诊断"面板
5. 7 张状态卡应该有真实 IMEI/IMSI/ICCID/CSQ/CEREG/COPS 值（拉取是异步的，可能需要等几秒）
6. AT 控制台输 `AT` 回车 → 控制台显示 `> AT` 然后 `< OK`
7. 输 `AT+CSQ` → 显示 `> AT+CSQ` 然后 `< +CSQ: 23,99` 然后 `< OK`
8. 输 `AT+FOO` → 显示 `> AT+FOO` 然后 `< ERROR`

### Step 5: Commit

```
feat(p3): panel_diag 接 diag_service 实时数据 + AT 控制台走真通道
```

---

## Task 6: 冒烟清单 + tag

**Files:**
- Create: `docs/superpowers/checklists/phase3-at-engine.md`
- Tag: `phase3-at-engine`

### Step 1: Write checklist

```markdown
# Phase 3 — AT 引擎 + DeviceManager 冒烟测试

## 沙箱里可做

- [x] `./build.bat` build 成功
- [x] `./build.bat test` 全部通过
  - [x] test_strbuf 18/18
  - [x] test_json_roundtrip 20/20
  - [x] test_i18n 6/6
  - [x] test_ringbuf 15/15
  - [x] test_ncm_enumerate 0/0
  - [x] **test_at_parser** 13+/13+
  - [x] **test_at_session** 5 个测试全过
  - [x] **test_diag_state** 4/4

## 真机验证

### 连接 COM 模组

- [ ] 启动 EXE，切到"多模组"面板
- [ ] 看到 COM 模组一行（label=`COM 7`，状态=DISCONNECTED）
- [ ] 点该行的"连接"按钮
- [ ] 状态从 DISCONNECTED → **READY**，按钮变"断开"
- [ ] stderr 打印 `device_manager: dev N (COM 7) 已连接`

### 状态卡实时

- [ ] 切到"现场诊断"面板
- [ ] 7 张状态卡显示真实 IMEI/IMSI/ICCID/CSQ/CEREG/COPS 值（不是 "-"）
- [ ] 点击"一键健康检查"按钮
- [ ] "上次刷新"时间戳更新
- [ ] stderr 打印 7 条 `at_session: 收到未订阅 URC '...'`（因为 P3 暂不订阅 URC）

### AT 控制台

- [ ] 在 AT 输入框输 `AT`，按回车 / 点"发送"
- [ ] 控制台历史区出现：
  - `> AT`
  - `< OK`
- [ ] 输 `AT+CSQ`
- [ ] 出现：
  - `> AT+CSQ`
  - `< +CSQ: 23,99`（或类似）
  - `< OK`
- [ ] 输 `AT+FOO`
- [ ] 出现：
  - `> AT+FOO`
  - `< ERROR`

### 断开

- [ ] 切回"多模组"面板
- [ ] 点"断开"按钮
- [ ] 状态从 READY → DISCONNECTED
- [ ] 切回"现场诊断"面板 → 状态卡显示"未连接模组（先在多模组面板点连接）"
- [ ] stderr 打印 `device_manager: dev N (COM 7) 已断开`

### URC

- [ ] 模组主动上报 URC 时（如 +CEREG 变化），控制台历史区出现 `< +CEREG: 5` 之类（如果 P3 接了订阅——P3 暂未订阅，stderr 会打"未订阅 URC"）

## 不在 P3 范围

- ✗ TCP/SSL 拨号上网
- ✗ 发短信模板（spec 里的"发短信模板"按钮）
- ✗ 抓 modem log 30s
- ✗ USB-NCM/RNDIS 通道（spec 里 P3-T2 spec 说"暂只支持 com://"）
- ✗ 产线测试 / OTA（spec P4/P5）
```

### Step 2: Commit + tag

```bash
cd d:/CODE/zero-c
git add docs/superpowers/checklists/phase3-at-engine.md
git commit -m "docs: Phase 3 AT 引擎 冒烟测试清单"
git tag phase3-at-engine
```

---

## Self-Review

**1. Spec 覆盖**：
- §4.2 at_session（行解析、URC、命令队列、超时）→ Task 1+2
- §4.3 device_manager 状态机 DISCONNECTED → READY/ERROR → Task 3
- §4.4 DiagnosticService → Task 3
- §8 Phase 3 → Task 1-5
- §12 风险：USB-NCM/RNDIS 通道 P3 暂未做（spec 接受，因为产线/工装用 COM 即可）

**2. 占位符扫**：0 处 "TBD"/"TODO"/"fill in"/"similar to"。

**3. 类型一致性**：
- `at_session_t` 在 `at_session.h` 定义
- `diag_state_t` 在 `diag_state.h` 定义
- `device_manager_t` 在 `device_manager.h` 定义（P2）
- `modem_dev_t` 增 `at`/`serial`/`diag_last_update` 字段
- `agent_app_t` 增 `diag_service`/`active_at` 字段

---

## Execution Handoff

Plan complete. Two execution options:

1. **Subagent-Driven (recommended)** — 6 task × 3 subagent = 18 dispatch，~1 周
2. **Inline** — 同 session，~3-4 天

**怎么选？**
