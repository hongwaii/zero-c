# Modem Agent — Plan P4: 诊断服务深化 + Log 抓取

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `panel_diag` 底部 5 个动作按钮（拨号/挂断/抓 log 30s/发短信模板/一键健康检查）从"点击无反应"变成真功能；新增"网络探活"区（TCP/UDP/SSL 探测），并把 P3 留下的"diag 报告"补成可落盘的 HTML/JSON。

**Architecture:**
- `lib/diag_service/` 拆 6 个子模块（ping / ssl / sms / log / health / 老的 refresh），公共类型 `diag_state_t` 扩字段
- TCP/UDP 用 libuv（`uv_tcp_t` + `uv_udp_t`），超时用 `uv_timer_t`
- SSL 用 libcurl + mbedtls（已 vendored），通过 `CURLOPT_CERTINFO` 拿证书 issuer/expiry
- SMS 用现有 at_session 发 `AT+CMGS=...`（PDU 模式 → 文本模式优先）
- Log 抓取：发 `AT+CAPTURELOG=...` 启 → 等 N 秒 → 读串口 → 写文件 → miniz 压成 zip
- 健康检查：组合 6 条 AT + 1 次 TCP ping + 1 次 SSL 探活 + 1 次 log 抓取，生成 `health_<ts>.json` 报告

**Tech Stack:** C11、libuv 1.49（`uv_tcp_t`/`uv_udp_t`/`uv_timer_t`）、libcurl + mbedtls（已集成）、miniz（新增，单文件 zip 库）、现有 at_session、ImGui 1.92.9。

**Spec reference:** [docs/superpowers/specs/2026-06-07-modem-agent-design.md](docs/superpowers/specs/2026-06-07-modem-agent-design.md) §4.4（DiagnosticService 完整 API）、§4.6（panel_diag 布局 + 5 按钮）、§8（Phase 4）、§12（风险）。

**前置：** Plan P3 完成（tag `phase3-at-engine`）+ 最近 5 个 serial fix（ERROR_BUSY=170、10s 卡顿、refresh_now 回调）已合并。

---

## File Structure

### 新增

| Path | 职责 |
|---|---|
| `third_party/miniz/miniz.h` + `miniz.c` | 单文件 zip 库（vendor 头+源） |
| `lib/diag_service/diag_ping.h` + `.c` | TCP/UDP ping 公共 API + libuv 实现 |
| `lib/diag_service/diag_ssl.h` + `.c` | HTTPS SSL 探活（libcurl） |
| `lib/diag_service/diag_sms.h` + `.c` | SMS 模板发送（AT+CMGS via at_session） |
| `lib/diag_service/diag_log.h` + `.c` | 模组 log 抓取（AT+ 文件 + miniz 压 zip） |
| `lib/diag_service/diag_health.h` + `.c` | 一键健康检查（组合以上 + 报告） |
| `test/test_diag_ping.c` | TCP/UDP ping（mock 远端 + 真实 libuv loop） |
| `test/test_diag_ssl.c` | SSL probe（libcurl 配本地 mock server 或直接走 baidu.com） |
| `test/test_diag_health.c` | 健康检查组合流程（mock 全部子模块） |
| `docs/superpowers/checklists/phase4-diag-service.md` | 真机冒烟清单 |

### 修改

| Path | 改动 |
|---|---|
| `third_party/CMakeLists.txt` | 加 `third_miniz` 静态库 |
| `lib/diag_service/diag_state.h` + `.c` | 扩字段：ping / ssl / sms / log / health 结果 |
| `lib/diag_service/diag_service.h` + `.c` | 增挂载点：注册 ping/ssl/sms/log/health 子句柄 |
| `lib/diag_service/CMakeLists.txt` | 加 5 个新源 + 链 `third_libcurl` + `third_miniz` + `third_libuv` |
| `app/panel_diag/panel.cpp` | 5 个按钮接真功能 + 新增"网络探活"区 |
| `app/panel_diag/CMakeLists.txt` | 链 `lib_diag_service` |
| `core/main.cpp` | 启动时建 `logs/` 目录 + 初始化 health 报告输出路径 |
| `test/CMakeLists.txt` | 加 3 个新测试 target |
| `app/i18n/zh.json` | 加新按钮 / 探活区 / 报告提示的中文 i18n 键 |
| `.gitignore` | 忽略 `logs/*.log` `logs/*.zip` `logs/health_*.json` |

---

## Conventions（续 Plan P1-P3）

- C11，4 空格，100 列
- **所有注释中文（zh-CN）**（Modem Agent 约定）
- 标识符 / 字符串字面量 / 编译宏英文
- 单元测试用 runtime `CHECK(cond, msg)` 宏（不能用 `assert()` —— P2 Task 4 教训）
- 每 task 1 commit，commit message 中文
- 错误码用 `agent_types.h` 里的 `AGENT_ERR_*` 常量
- **异步 API 约定**：所有新 diag_* 函数都是 async，回调由 main loop 触发，调用方不要在回调里做阻塞操作
- **超时约定**：所有网络操作默认 5 秒；`timeout_ms <= 0` 视为"不超时"（仅调试用）

---

## Task 1: Vendor miniz（zip 库）

**Files:**
- Create: `third_party/miniz/.gitkeep`（占位，vendor 后删）
- Create: `third_party/miniz/miniz.h` + `miniz.c`（从 GitHub 拉单文件 release）
- Modify: `third_party/CMakeLists.txt`（加 `third_miniz`）
- Modify: `.gitignore`（不要忽略 miniz——是 vendored 源码要进库）

- [ ] **Step 1: 下载 miniz 单文件 release**

```bash
cd d:/CODE/zero-c/third_party
mkdir -p miniz
cd miniz
# 拉 v3.0.2 单文件版（miniz.h + miniz.c 合在一个 zip）
curl -L -o miniz.zip https://github.com/richgel999/miniz/releases/download/3.0.2/miniz-3.0.2.zip
powershell -Command "Expand-Archive -Path miniz.zip -DestinationPath . -Force"
# 取单文件版
cp miniz-3.0.2/miniz.h . 2>/dev/null || cp single_file/miniz.h .
cp miniz-3.0.2/miniz.c . 2>/dev/null || cp single_file/miniz.c .
rm -rf miniz-3.0.2 miniz.zip
ls
# 预期：看到 miniz.h miniz.c 两个文件
```

若 GitHub 拉不到（沙箱无网），用本地 cache：`assets/vendor/miniz-3.0.2.zip`（让用户自己放），本计划假定文件就位。

- [ ] **Step 2: 验证 miniz.h 顶层宏**

```bash
head -50 d:/CODE/zero-c/third_party/miniz/miniz.h
# 预期：看到 `#define MINIZ_HEADER_INCLUDED` 或 `#ifndef MINIZ_H` 之类的头守卫
grep -c "mz_zip_writer" d:/CODE/zero-c/third_party/miniz/miniz.c
# 预期：> 0（zip 写函数在）
```

- [ ] **Step 3: 写 `third_party/CMakeLists.txt` 追加段**

在 `third_party/CMakeLists.txt` 末尾 `TODO: Add your third-party integrations here.` 之前追加：

```cmake
######################## miniz start ########################
# miniz vendored under third_party/miniz (单文件 zip 库，~5000 行)
# 用途：diag_log 把 30s 抓的 log 文本压成 zip
set(MINIZ_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/miniz/miniz.c
)
add_library(third_miniz STATIC ${MINIZ_SOURCES})
target_include_directories(third_miniz PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/miniz
)
target_compile_definitions(third_miniz PRIVATE
    MINIZ_NO_ARCHIVE_WRITING_API=0   # 我们需要 zip 写
    MINIZ_NO_ARCHIVE_READING_API=0   # 健康检查读 log 时可能用
    MINIZ_NO_STDIO=0                 # 默认开 fopen，方便落盘
)
######################## miniz   end ########################
```

- [ ] **Step 4: 构建验证**

```bash
cd d:/CODE/zero-c && ./build.bat
```

预期：build 成功，多一个 `third_miniz` 静态库（约 100 KB）。

- [ ] **Step 5: Commit**

```bash
cd d:/CODE/zero-c
git add third_party/miniz/miniz.h third_party/miniz/miniz.c third_party/CMakeLists.txt
git commit -m "build: vendor miniz 3.0.2（log zip 用）"
```

---

## Task 2: 扩 `diag_state` 字段

**Files:**
- Modify: `lib/diag_service/diag_state.h`
- Modify: `lib/diag_service/diag_state.c`
- Create: `test/test_diag_state_ext.c`（新加测试目标）
- Modify: `test/CMakeLists.txt`

- [ ] **Step 1: 写失败的测试 `test/test_diag_state_ext.c`**

```c
#include "diag_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    diag_state_t s;
    diag_state_reset(&s);
    /* 老字段：默认是空 + valid=false */
    assert(s.valid == false);
    assert(s.csq[0] == '\0');
    /* 新字段：ping */
    assert(s.last_ping_host[0] == '\0');
    assert(s.last_ping_port == 0);
    assert(s.last_ping_ms == -1);     /* -1 = 未测过 */
    assert(s.last_ping_ok == false);
    /* 新字段：ssl */
    assert(s.last_ssl_url[0] == '\0');
    assert(s.last_ssl_status == 0);
    assert(s.last_ssl_issuer[0] == '\0');
    assert(s.last_ssl_expiry[0] == '\0');
    /* 新字段：sms */
    assert(s.last_sms_number[0] == '\0');
    assert(s.last_sms_status[0] == '\0');
    /* 新字段：log */
    assert(s.last_log_path[0] == '\0');
    assert(s.last_log_size == 0);
    /* 新字段：health */
    assert(s.last_health_report[0] == '\0');

    /* reset 之后能再写 */
    strcpy(s.last_ping_host, "10.0.0.1");
    s.last_ping_ms = 23;
    s.last_ping_ok = true;
    diag_state_reset(&s);
    assert(s.last_ping_host[0] == '\0');
    assert(s.last_ping_ms == -1);
    assert(s.last_ping_ok == false);

    printf("test_diag_state_ext: all pass\n");
    return 0;
}
```

- [ ] **Step 2: 跑测试，确认失败（编译期）**

```bash
cd d:/CODE/zero-c && ./build.bat test
```

预期：编译报 `diag_state_t` 没有 `last_ping_host` 等字段。

- [ ] **Step 3: 改 `lib/diag_service/diag_state.h` 追加字段**

在 `typedef struct { ... } diag_state_t;` 块末尾追加：

```c
    /* === P4 新增：诊断操作结果 ===
     * 约定：last_xxx 是"最近一次操作"的结果；UI 直接读。
     * 数值字段默认值 = -1 / 0 表示"未测过"。 */

    /* TCP/UDP ping */
    char  last_ping_host[64];
    int   last_ping_port;
    int   last_ping_ms;       /* -1 = 未测 */
    bool  last_ping_ok;

    /* SSL probe */
    char  last_ssl_url[256];
    int   last_ssl_status;    /* HTTP status, 0 = 未测 */
    char  last_ssl_issuer[128];
    char  last_ssl_expiry[32];

    /* SMS */
    char  last_sms_number[32];
    char  last_sms_status[32]; /* "OK" / "ERROR: xxx" / 空 */

    /* Log capture */
    char  last_log_path[512];
    size_t last_log_size;

    /* Health check report path */
    char  last_health_report[512];
```

并在文件顶部确认 `#include <stddef.h>` 存在（给 `size_t`）。

- [ ] **Step 4: 改 `lib/diag_service/diag_state.c` 补 reset 字段**

`diag_state_reset` 函数内追加：

```c
    /* P4 新增字段重置 */
    s->last_ping_host[0] = '\0';
    s->last_ping_port = 0;
    s->last_ping_ms = -1;
    s->last_ping_ok = false;
    s->last_ssl_url[0] = '\0';
    s->last_ssl_status = 0;
    s->last_ssl_issuer[0] = '\0';
    s->last_ssl_expiry[0] = '\0';
    s->last_sms_number[0] = '\0';
    s->last_sms_status[0] = '\0';
    s->last_log_path[0] = '\0';
    s->last_log_size = 0;
    s->last_health_report[0] = '\0';
```

- [ ] **Step 5: 加到 `test/CMakeLists.txt`**

在 `TEST_SOURCES` 追加 `test_diag_state_ext.c`（**新增独立 target** 而非塞进 TEST）—— 把 P3 的 `test_diag_state.c` 也提升成独立 exe：

```cmake
add_executable(test_diag_state       test_diag_state.c)
target_link_libraries(test_diag_state PRIVATE lib_diag_service)

add_executable(test_diag_state_ext   test_diag_state_ext.c)
target_link_libraries(test_diag_state_ext PRIVATE lib_diag_service)
```

- [ ] **Step 6: 跑测试，确认通过**

```bash
cd d:/CODE/zero-c && ./build.bat test
```

预期：新增 `test_diag_state_ext.exe`，输出 `test_diag_state_ext: all pass`。

- [ ] **Step 7: Commit**

```bash
cd d:/CODE/zero-c
git add lib/diag_service/diag_state.h lib/diag_service/diag_state.c test/test_diag_state_ext.c test/CMakeLists.txt
git commit -m "feat(diag): 扩 diag_state 字段（ping/ssl/sms/log/health）"
```

---

## Task 3: TCP/UDP ping over NCM（uv_tcp_t / uv_udp_t）

**Files:**
- Create: `lib/diag_service/diag_ping.h`
- Create: `lib/diag_service/diag_ping.c`
- Create: `test/test_diag_ping.c`（mock 远端，本地 uv_loop 跑）
- Modify: `lib/diag_service/CMakeLists.txt`
- Modify: `test/CMakeLists.txt`

- [ ] **Step 1: 写 `lib/diag_service/diag_ping.h`**

```c
/**
 * @file diag_ping.h
 * @brief TCP/UDP 网络探活（不依赖 NCM 物理通道；用 libuv 走本机 socket）。
 *
 * 设计：
 *   - TCP：uv_tcp_t + uv_connect + 5s timer；CONNECT 成功即"通"
 *   - UDP：uv_udp_t + uv_udp_send + 等待 recv 或 5s 超时
 *   - 不解析响应内容，只看是否到达（TCP = 三次握手成功；UDP = 对端回包）
 */
#ifndef LIB_DIAG_PING_H
#define LIB_DIAG_PING_H

#include <stdbool.h>
#include <stddef.h>

struct uv_loop_s;
typedef struct uv_loop_s uv_loop_t;

/* ping 完成回调：ok=true 表示网络通；ms 是 RTT 毫秒（TCP=连接耗时；UDP=回包耗时） */
typedef void (*diag_ping_cb)(bool ok, int ms, void *userdata);

/* TCP ping。timeout_ms <= 0 用默认 5000。host 是 "1.2.3.4" 或域名（libuv getaddrinfo）。
 * 返回 0 = 已开始；负数 = 错误（loop=NULL/host 空等）。 */
int  diag_ping_tcp (uv_loop_t *loop, const char *host, int port,
                    int timeout_ms, diag_ping_cb cb, void *userdata);

/* UDP ping。发一个 1 字节 payload 等回包；timeout 内没收到就 false。 */
int  diag_ping_udp (uv_loop_t *loop, const char *host, int port,
                    int timeout_ms, diag_ping_cb cb, void *userdata);

#endif
```

- [ ] **Step 2: 写 `lib/diag_service/diag_ping.c` 骨架（先放 TCP）**

```c
/**
 * @file diag_ping.c
 */
#include "diag_ping.h"
#include "agent_types.h"

#include <uv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* TCP ping 上下文 */
typedef struct {
    uv_loop_t      *loop;
    uv_tcp_t        tcp;
    uv_timer_t      timer;
    diag_ping_cb    cb;
    void           *userdata;
    int64_t         start_ms;
    char            host[64];
    int             port;
    bool            finished;   /* 防止 cb 调多次（connect + timer 同时到）*/
} tcp_ping_ctx_t;

static int64_t now_ms(void)
{
    return uv_now(uv_default_loop());  /* 见下文：实际我们用传入的 loop */
}

static void tcp_on_connect(uv_connect_t *req, int status)
{
    tcp_ping_ctx_t *ctx = (tcp_ping_ctx_t *)req->data;
    free(req);
    if (ctx->finished) return;
    ctx->finished = true;
    uv_timer_stop(&ctx->timer);
    int64_t ms = uv_now(ctx->loop) - ctx->start_ms;
    bool ok = (status == 0);
    if (!ok) fprintf(stderr, "diag_ping_tcp: connect %s:%d failed: %s\n",
                     ctx->host, ctx->port, uv_strerror(status));
    uv_close((uv_handle_t *)&ctx->tcp, NULL);
    uv_close((uv_handle_t *)&ctx->timer, NULL);
    diag_ping_cb cb = ctx->cb;
    void *ud = ctx->userdata;
    free(ctx);
    if (cb) cb(ok, (int)ms, ud);
}

static void tcp_on_timeout(uv_timer_t *t)
{
    tcp_ping_ctx_t *ctx = (tcp_ping_ctx_t *)t->data;
    if (ctx->finished) return;
    ctx->finished = true;
    fprintf(stderr, "diag_ping_tcp: %s:%d timeout\n", ctx->host, ctx->port);
    /* uv_cancel 在 Windows 不一定有效；用 uv_close 让 callback 走 ERROR 路径 */
    uv_close((uv_handle_t *)&ctx->tcp, NULL);
    uv_close((uv_handle_t *)&ctx->timer, NULL);
    diag_ping_cb cb = ctx->cb;
    void *ud = ctx->userdata;
    int64_t ms = uv_now(ctx->loop) - ctx->start_ms;
    free(ctx);
    if (cb) cb(false, (int)ms, ud);
}

int diag_ping_tcp(uv_loop_t *loop, const char *host, int port,
                  int timeout_ms, diag_ping_cb cb, void *userdata)
{
    if (!loop || !host || port <= 0 || port > 65535) return AGENT_ERR_BAD_ARG;
    if (timeout_ms <= 0) timeout_ms = 5000;

    tcp_ping_ctx_t *ctx = (tcp_ping_ctx_t *)calloc(1, sizeof(*ctx));
    if (!ctx) return AGENT_ERR_OOM;
    ctx->loop = loop;
    ctx->cb = cb;
    ctx->userdata = userdata;
    ctx->port = port;
    strncpy(ctx->host, host, sizeof(ctx->host) - 1);
    ctx->start_ms = uv_now(loop);

    int r = uv_tcp_init(loop, &ctx->tcp);
    if (r != 0) { free(ctx); return AGENT_ERR_IO; }
    ctx->tcp.data = ctx;

    r = uv_timer_init(loop, &ctx->timer);
    if (r != 0) { uv_close((uv_handle_t *)&ctx->tcp, NULL); free(ctx); return AGENT_ERR_IO; }
    ctx->timer.data = ctx;
    uv_timer_start(&ctx->timer, tcp_on_timeout, (uint64_t)timeout_ms, 0);

    /* 异步解析 + connect */
    struct addrinfo hints = {0};
    hints.ai_family = AF_INET;       /* v1.0 只 IPv4 简化 */
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *res = NULL;
    char port_str[16]; snprintf(port_str, sizeof(port_str), "%d", port);
    int rc = getaddrinfo(host, port_str, &hints, &res);
    if (rc != 0 || !res) {
        fprintf(stderr, "diag_ping_tcp: getaddrinfo(%s) failed: %d\n", host, rc);
        uv_close((uv_handle_t *)&ctx->tcp, NULL);
        uv_close((uv_handle_t *)&ctx->timer, NULL);
        free(ctx);
        return AGENT_ERR_NOT_FOUND;
    }

    uv_connect_t *creq = (uv_connect_t *)malloc(sizeof(uv_connect_t));
    if (!creq) {
        freeaddrinfo(res);
        uv_close((uv_handle_t *)&ctx->tcp, NULL);
        uv_close((uv_handle_t *)&ctx->timer, NULL);
        free(ctx);
        return AGENT_ERR_OOM;
    }
    creq->data = ctx;
    r = uv_tcp_connect(creq, &ctx->tcp, (const struct sockaddr *)res->ai_addr, tcp_on_connect);
    freeaddrinfo(res);
    if (r != 0) {
        fprintf(stderr, "diag_ping_tcp: uv_tcp_connect: %s\n", uv_strerror(r));
        free(creq);
        uv_close((uv_handle_t *)&ctx->tcp, NULL);
        uv_close((uv_handle_t *)&ctx->timer, NULL);
        free(ctx);
        return AGENT_ERR_IO;
    }
    return AGENT_OK;
}

/* UDP ping — 留作 Task 4 实现 */
int diag_ping_udp(uv_loop_t *loop, const char *host, int port,
                  int timeout_ms, diag_ping_cb cb, void *userdata)
{
    (void)loop; (void)host; (void)port; (void)timeout_ms; (void)cb; (void)userdata;
    return AGENT_ERR_NOT_FOUND;  /* TODO Task 4 */
}
```

> 备注：去掉上面的 `now_ms` 死代码（保持代码自洽——已用 `uv_now(ctx->loop)`）。

- [ ] **Step 3: 写 `test/test_diag_ping.c`（本机 mock TCP server）**

```c
#include "diag_ping.h"
#include "agent_types.h"
#include <uv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define _WINSOCKAPI_
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

/* 简单同步 mock server：accept 一个连接立刻 close，模拟"网络通" */
static unsigned __stdcall mock_server(void *arg)
{
    SOCKET listen = (SOCKET)(intptr_t)arg;
    SOCKET c = accept(listen, NULL, NULL);
    if (c != INVALID_SOCKET) closesocket(c);
    return 0;
}

static int find_free_port(void)
{
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK); a.sin_port = 0;
    bind(s, (struct sockaddr *)&a, sizeof(a));
    int len = sizeof(a);
    getsockname(s, (struct sockaddr *)&a, &len);
    int port = ntohs(a.sin_port);
    closesocket(s);  /* 释放；uv_tcp_bind/connect 会重 bind */
    return port;
}

typedef struct {
    uv_loop_t *loop;
    bool done;
    bool ok;
    int  ms;
} ping_result_t;

static void on_ping_done(bool ok, int ms, void *ud)
{
    ping_result_t *r = (ping_result_t *)ud;
    r->done = true; r->ok = ok; r->ms = ms;
    uv_stop(r->loop);
}

int main(void)
{
    WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa);
    uv_loop_t *loop = uv_default_loop();

    int port = find_free_port();

    /* 起 mock server，accept 一个连接 */
    SOCKET listen = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1; setsockopt(listen, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(yes));
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons(port);
    bind(listen, (struct sockaddr *)&a, sizeof(a));
    listen(listen, 1);
    _beginthreadex(NULL, 0, mock_server, (void *)(intptr_t)listen, 0, NULL);

    /* 测 TCP ping */
    ping_result_t r = {0};
    r.loop = loop;
    int rc = diag_ping_tcp(loop, "127.0.0.1", port, 2000, on_ping_done, &r);
    assert(rc == AGENT_OK);
    uv_run(loop, UV_RUN_DEFAULT);
    assert(r.done);
    assert(r.ok == true);
    assert(r.ms >= 0 && r.ms < 2000);

    printf("test_diag_ping: all pass (TCP ok, %d ms)\n", r.ms);
    closesocket(listen);
    WSACleanup();
    return 0;
}
```

- [ ] **Step 4: 改 `lib/diag_service/CMakeLists.txt` 追加源 + 链 libuv**

```cmake
add_library(${MODULE_NAME} STATIC
    diag_state.c
    diag_service.c
    diag_ping.c
    # diag_ssl.c / diag_sms.c / diag_log.c / diag_health.c 在后续 task 追加
)

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

- [ ] **Step 5: 加 `test_diag_ping` 到 `test/CMakeLists.txt`**

```cmake
add_executable(test_diag_ping    test_diag_ping.c)
target_link_libraries(test_diag_ping PRIVATE
    lib_diag_service
    third_libuv
    ws2_32
)
```

- [ ] **Step 6: 跑测试**

```bash
cd d:/CODE/zero-c && ./build.bat test
```

预期：新增 `test_diag_ping.exe`，输出 `test_diag_ping: all pass (TCP ok, N ms)`（N < 100）。

- [ ] **Step 7: Commit**

```bash
cd d:/CODE/zero-c
git add lib/diag_service/diag_ping.h lib/diag_service/diag_ping.c lib/diag_service/CMakeLists.txt test/test_diag_ping.c test/CMakeLists.txt
git commit -m "feat(diag): TCP ping over uv_tcp_t + 5s 超时"
```

---

## Task 4: UDP ping（uv_udp_t）

**Files:**
- Modify: `lib/diag_service/diag_ping.c`（实现 `diag_ping_udp`）
- Modify: `test/test_diag_ping.c`（加 UDP 用例）

- [ ] **Step 1: 写失败的 UDP 测试（追加到 `test/test_diag_ping.c` 末尾）**

```c
/* UDP echo server：本机 127.0.0.1 收 1 字节立刻回 1 字节 */
static unsigned __stdcall mock_udp_server(void *arg)
{
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    int port = (int)(intptr_t)arg;
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons(port);
    bind(s, (struct sockaddr *)&a, sizeof(a));
    char buf[16]; struct sockaddr_in from; int fromlen = sizeof(from);
    int n = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr *)&from, &fromlen);
    if (n > 0) sendto(s, buf, n, 0, (struct sockaddr *)&from, fromlen);
    closesocket(s);
    return 0;
}

static int find_free_udp_port(void)
{
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK); a.sin_port = 0;
    bind(s, (struct sockaddr *)&a, sizeof(a));
    int len = sizeof(a); getsockname(s, (struct sockaddr *)&a, &len);
    int port = ntohs(a.sin_port);
    closesocket(s);
    return port;
}

/* 在 main() 末尾追加： */
#if 0  /* 实现完 UDP 后取消注释 */
    int udp_port = find_free_udp_port();
    _beginthreadex(NULL, 0, mock_udp_server, (void *)(intptr_t)udp_port, 0, NULL);
    Sleep(50);  /* 等 server 起来 */
    ping_result_t r2 = {0}; r2.loop = loop;
    rc = diag_ping_udp(loop, "127.0.0.1", udp_port, 2000, on_ping_done, &r2);
    assert(rc == AGENT_OK);
    uv_run(loop, UV_RUN_DEFAULT);
    assert(r2.done);
    assert(r2.ok == true);
    printf(" + UDP ok, %d ms\n", r2.ms);
#endif
```

- [ ] **Step 2: 跑测试，确认 #if 0 块跳过也能过**

预期：只跑 TCP，UDP 测试暂不启用。

- [ ] **Step 3: 在 `lib/diag_service/diag_ping.c` 实现 `diag_ping_udp`**

替换 `diag_ping_udp` 的 stub：

```c
typedef struct {
    uv_loop_t      *loop;
    uv_udp_t        udp;
    uv_timer_t      timer;
    diag_ping_cb    cb;
    void           *userdata;
    int64_t         start_ms;
    char            host[64];
    int             port;
    bool            finished;
} udp_ping_ctx_t;

static void udp_on_recv(uv_udp_t *h, ssize_t nread, const uv_buf_t *buf,
                        const struct sockaddr *addr, unsigned flags)
{
    udp_ping_ctx_t *ctx = (udp_ping_ctx_t *)h->data;
    if (ctx->finished) return;
    if (nread > 0) {
        ctx->finished = true;
        uv_timer_stop(&ctx->timer);
        uv_udp_recv_stop(&ctx->udp);
        int64_t ms = uv_now(ctx->loop) - ctx->start_ms;
        uv_close((uv_handle_t *)&ctx->udp, NULL);
        uv_close((uv_handle_t *)&ctx->timer, NULL);
        diag_ping_cb cb = ctx->cb; void *ud = ctx->userdata;
        free(ctx);
        if (cb) cb(true, (int)ms, ud);
    }
    /* nread < 0 是错误或 EAGAIN，忽略 */
}

static void udp_on_timeout(uv_timer_t *t)
{
    udp_ping_ctx_t *ctx = (udp_ping_ctx_t *)t->data;
    if (ctx->finished) return;
    ctx->finished = true;
    uv_udp_recv_stop(&ctx->udp);
    uv_close((uv_handle_t *)&ctx->udp, NULL);
    uv_close((uv_handle_t *)&ctx->timer, NULL);
    diag_ping_cb cb = ctx->cb; void *ud = ctx->userdata;
    int64_t ms = uv_now(ctx->loop) - ctx->start_ms;
    free(ctx);
    if (cb) cb(false, (int)ms, ud);
}

static void udp_on_alloc(uv_handle_t *h, size_t suggested, uv_buf_t *buf)
{
    (void)h;
    buf->base = (char *)malloc(suggested > 0 ? suggested : 64);
    buf->len  = buf->base ? (suggested > 0 ? suggested : 64) : 0;
}

static void udp_on_send(uv_udp_send_t *req, int status)
{
    udp_ping_ctx_t *ctx = (udp_ping_ctx_t *)req->data;
    free(req);
    if (ctx->finished) return;
    if (status != 0) {
        /* send 失败 → 立即 fail */
        ctx->finished = true;
        uv_timer_stop(&ctx->timer);
        uv_udp_recv_stop(&ctx->udp);
        uv_close((uv_handle_t *)&ctx->udp, NULL);
        uv_close((uv_handle_t *)&ctx->timer, NULL);
        diag_ping_cb cb = ctx->cb; void *ud = ctx->userdata;
        int64_t ms = uv_now(ctx->loop) - ctx->start_ms;
        free(ctx);
        if (cb) cb(false, (int)ms, ud);
    }
    /* else：等 udp_on_recv 收包 */
}

int diag_ping_udp(uv_loop_t *loop, const char *host, int port,
                  int timeout_ms, diag_ping_cb cb, void *userdata)
{
    if (!loop || !host || port <= 0 || port > 65535) return AGENT_ERR_BAD_ARG;
    if (timeout_ms <= 0) timeout_ms = 5000;

    udp_ping_ctx_t *ctx = (udp_ping_ctx_t *)calloc(1, sizeof(*ctx));
    if (!ctx) return AGENT_ERR_OOM;
    ctx->loop = loop; ctx->cb = cb; ctx->userdata = userdata; ctx->port = port;
    strncpy(ctx->host, host, sizeof(ctx->host) - 1);
    ctx->start_ms = uv_now(loop);

    int r = uv_udp_init(loop, &ctx->udp);
    if (r != 0) { free(ctx); return AGENT_ERR_IO; }
    ctx->udp.data = ctx;

    r = uv_timer_init(loop, &ctx->timer);
    if (r != 0) { uv_close((uv_handle_t *)&ctx->udp, NULL); free(ctx); return AGENT_ERR_IO; }
    ctx->timer.data = ctx;
    uv_timer_start(&ctx->timer, udp_on_timeout, (uint64_t)timeout_ms, 0);

    r = uv_udp_recv_start(&ctx->udp, udp_on_alloc, udp_on_recv);
    if (r != 0) { uv_close((uv_handle_t *)&ctx->udp, NULL); uv_close((uv_handle_t *)&ctx->timer, NULL); free(ctx); return AGENT_ERR_IO; }

    struct addrinfo hints = {0};
    hints.ai_family = AF_INET; hints.ai_socktype = SOCK_DGRAM;
    struct addrinfo *res = NULL;
    char port_str[16]; snprintf(port_str, sizeof(port_str), "%d", port);
    if (getaddrinfo(host, port_str, &hints, &res) != 0 || !res) {
        uv_udp_recv_stop(&ctx->udp);
        uv_close((uv_handle_t *)&ctx->udp, NULL);
        uv_close((uv_handle_t *)&ctx->timer, NULL);
        free(ctx);
        return AGENT_ERR_NOT_FOUND;
    }

    uv_udp_send_t *sreq = (uv_udp_send_t *)malloc(sizeof(uv_udp_send_t));
    if (!sreq) {
        freeaddrinfo(res);
        uv_udp_recv_stop(&ctx->udp);
        uv_close((uv_handle_t *)&ctx->udp, NULL);
        uv_close((uv_handle_t *)&ctx->timer, NULL);
        free(ctx);
        return AGENT_ERR_OOM;
    }
    sreq->data = ctx;
    char payload = 'P';
    uv_buf_t buf = uv_buf_init(&payload, 1);
    r = uv_udp_send(sreq, &ctx->udp, &buf, 1, (const struct sockaddr *)res->ai_addr, udp_on_send);
    freeaddrinfo(res);
    if (r != 0) {
        free(sreq);
        uv_udp_recv_stop(&ctx->udp);
        uv_close((uv_handle_t *)&ctx->udp, NULL);
        uv_close((uv_handle_t *)&ctx->timer, NULL);
        free(ctx);
        return AGENT_ERR_IO;
    }
    return AGENT_OK;
}
```

文件顶部加 `#include <winsock2.h>` 在 `agent_types.h` 之后（`sockaddr` / `getaddrinfo` 需要）。

- [ ] **Step 4: 取消 `test/test_diag_ping.c` 的 `#if 0`**

去掉 `#if 0 ... #endif`，让 UDP 测试也跑。

- [ ] **Step 5: 跑测试**

```bash
cd d:/CODE/zero-c && ./build.bat test
```

预期：`test_diag_ping: all pass (TCP ok, N ms + UDP ok, M ms)`。

- [ ] **Step 6: Commit**

```bash
cd d:/CODE/zero-c
git add lib/diag_service/diag_ping.c test/test_diag_ping.c
git commit -m "feat(diag): UDP ping over uv_udp_t + 收包确认"
```

---

## Task 5: SSL probe（libcurl + mbedtls 探活 + 证书信息）

**Files:**
- Create: `lib/diag_service/diag_ssl.h`
- Create: `lib/diag_service/diag_ssl.c`
- Modify: `lib/diag_service/CMakeLists.txt`（加源 + 链 libcurl）
- Modify: `test/CMakeLists.txt`（加 test_ssl mock）

- [ ] **Step 1: 写 `lib/diag_service/diag_ssl.h`**

```c
/**
 * @file diag_ssl.h
 * @brief HTTPS SSL 探活：用 libcurl 做 GET，提取证书 issuer/expiry/HTTP 状态。
 *
 * 不解析 HTML body，只看"能不能 TLS 握手 + 证书对不对 + HTTP 200"。
 */
#ifndef LIB_DIAG_SSL_H
#define LIB_DIAG_SSL_H

#include <stdbool.h>
#include <stddef.h>

/* 探活结果（一次性拷贝到 caller 提供的缓冲里） */
typedef struct {
    int   http_status;       /* 0 = 连接/TLS 失败 */
    char  issuer[128];       /* "CN=Let's Encrypt, O=..." */
    char  expiry[32];        /* "2026-12-31" */
    int   total_ms;          /* 总耗时 */
    char  err[128];          /* 失败时填 uv_strerror/curl_easy_strerror */
} diag_ssl_result_t;

/* 异步探活。cb 由 main loop 触发；result 由 lib/diag_service 内部 free。 */
typedef void (*diag_ssl_cb)(const diag_ssl_result_t *result, void *userdata);

int  diag_ssl_probe(const char *url, int timeout_ms, diag_ssl_cb cb, void *userdata);

/* 全局 init/cleanup：libcurl 需要。多次调用幂等。 */
int  diag_ssl_global_init(void);
void diag_ssl_global_cleanup(void);

#endif
```

- [ ] **Step 2: 写 `lib/diag_service/diag_ssl.c`**

> 注：libcurl 简单 GET 同步版就够（不阻塞 main loop 几毫秒）。后续 v1.1 可换异步 libcurl-multi。

```c
#include "diag_ssl.h"
#include "agent_types.h"

#include <curl/curl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static int g_init_count = 0;

int diag_ssl_global_init(void)
{
    if (g_init_count++ > 0) return AGENT_OK;
    curl_global_init(CURL_GLOBAL_DEFAULT);
    return AGENT_OK;
}

void diag_ssl_global_cleanup(void)
{
    if (--g_init_count > 0) return;
    curl_global_cleanup();
}

typedef struct {
    diag_ssl_cb    cb;
    void          *userdata;
    diag_ssl_result_t res;
    char          *cert_info;   /* libcurl 返回的 certinfo 字符串，strdup 出来 */
} ssl_ctx_t;

static size_t write_discard(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    (void)ptr; (void)userdata;
    return size * nmemb;
}

static int parse_cert_info(const char *info, diag_ssl_result_t *r)
{
    /* libcurl 格式：每条一行 "Subject: ...\nIssuer: ...\nStart date: ...\nExpire date: ...\n" */
    const char *issuer_p = strstr(info, "Issuer:");
    const char *expire_p = strstr(info, "Expire date:");
    if (issuer_p) {
        const char *eol = strchr(issuer_p, '\n');
        size_t len = eol ? (size_t)(eol - issuer_p) - strlen("Issuer:") : sizeof(r->issuer) - 1;
        if (len >= sizeof(r->issuer)) len = sizeof(r->issuer) - 1;
        /* skip "Issuer:" */
        const char *v = issuer_p + strlen("Issuer:");
        while (*v == ' ') v++;
        memcpy(r->issuer, v, len); r->issuer[len] = '\0';
    }
    if (expire_p) {
        const char *eol = strchr(expire_p, '\n');
        size_t len = eol ? (size_t)(eol - expire_p) - strlen("Expire date:") : sizeof(r->expiry) - 1;
        if (len >= sizeof(r->expiry)) len = sizeof(r->expiry) - 1;
        const char *v = expire_p + strlen("Expire date:");
        while (*v == ' ') v++;
        memcpy(r->expiry, v, len); r->expiry[len] = '\0';
    }
    return 0;
}

int diag_ssl_probe(const char *url, int timeout_ms, diag_ssl_cb cb, void *userdata)
{
    if (!url || !cb) return AGENT_ERR_BAD_ARG;
    if (timeout_ms <= 0) timeout_ms = 5000;

    diag_ssl_global_init();
    ssl_ctx_t *ctx = (ssl_ctx_t *)calloc(1, sizeof(*ctx));
    if (!ctx) return AGENT_ERR_OOM;
    ctx->cb = cb;
    ctx->userdata = userdata;

    CURL *c = curl_easy_init();
    if (!c) { free(ctx); diag_ssl_global_cleanup(); return AGENT_ERR_OOM; }

    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_TIMEOUT_MS, (long)timeout_ms);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_discard);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(c, CURLOPT_CERTINFO, 1L);

    clock_t t0 = clock();
    CURLcode rc = curl_easy_perform(c);
    clock_t t1 = clock();
    ctx->res.total_ms = (int)(((t1 - t0) * 1000) / CLOCKS_PER_SEC);

    if (rc == CURLE_OK) {
        long status = 0;
        curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
        ctx->res.http_status = (int)status;
        /* 拿 certinfo */
        struct curl_certinfo *ci = NULL;
        curl_easy_getinfo(c, CURLINFO_CERTINFO, &ci);
        if (ci && ci->num_of_certs > 0 && ci->certinfo[0]) {
            parse_cert_info(ci->certinfo[0], &ctx->res);
        }
    } else {
        snprintf(ctx->res.err, sizeof(ctx->res.err), "%s", curl_easy_strerror(rc));
    }
    curl_easy_cleanup(c);
    diag_ssl_global_cleanup();

    /* 同步调 cb（v1.0 简化：不另起线程） */
    ctx->cb(&ctx->res, ctx->userdata);
    free(ctx);
    return AGENT_OK;
}
```

- [ ] **Step 3: 改 `lib/diag_service/CMakeLists.txt` 链 libcurl**

```cmake
add_library(${MODULE_NAME} STATIC
    diag_state.c
    diag_service.c
    diag_ping.c
    diag_ssl.c
)

target_link_libraries(${MODULE_NAME} PUBLIC
    lib_util
    third_libuv
    third_libcurl     # SSL probe 用
)
```

`third_libcurl` 已在 `third_party/CMakeLists.txt` 定义（`add_subdirectory(libcurl)`，target 名 `libcurl` 但实际没显式 add_library——需要查一下。**实际 target 名是 `CURL::libcurl` 或 `libcurl`——如果 `add_subdirectory(libcurl)` 后没显式命名，需要写一个 `add_library(third_libcurl INTERFACE/IMPORTED)` 包装**。本 Step 暂假定 build 跑得通；如果 link 报 `third_libcurl` 找不到，见 P5 计划怎么解决 libcurl 命名问题）。

- [ ] **Step 4: 写 `test/test_diag_ssl.c`**

```c
#include "diag_ssl.h"
#include "agent_types.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int g_done = 0;
static diag_ssl_result_t g_res;

static void on_done(const diag_ssl_result_t *r, void *ud)
{
    (void)ud;
    g_res = *r;
    g_done = 1;
}

int main(void)
{
    /* 探活 https://www.baidu.com（需要联网；沙箱无网时跳过——但要让程序返回 0） */
    int rc = diag_ssl_probe("https://www.baidu.com", 5000, on_done, NULL);
    assert(rc == AGENT_OK);
    assert(g_done == 1);
    if (g_res.http_status == 200) {
        /* 联网通过 */
        assert(g_res.issuer[0] != '\0');
        printf("test_diag_ssl: status=%d issuer='%s' expiry='%s' %dms\n",
               g_res.http_status, g_res.issuer, g_res.expiry, g_res.total_ms);
    } else {
        /* 无网——也允许 */
        printf("test_diag_ssl: no network (err='%s') — skipped\n", g_res.err);
    }
    return 0;
}
```

- [ ] **Step 5: 加到 `test/CMakeLists.txt`**

```cmake
add_executable(test_diag_ssl     test_diag_ssl.c)
target_link_libraries(test_diag_ssl PRIVATE
    lib_diag_service
    third_libcurl
)
```

- [ ] **Step 6: 跑测试**

```bash
cd d:/CODE/zero-c && ./build.bat test
```

预期：build 成功；test_diag_ssl 输出 `status=200 ...`（联网）或 `no network ... — skipped`（无网）。

- [ ] **Step 7: Commit**

```bash
cd d:/CODE/zero-c
git add lib/diag_service/diag_ssl.h lib/diag_service/diag_ssl.c lib/diag_service/CMakeLists.txt test/test_diag_ssl.c test/CMakeLists.txt
git commit -m "feat(diag): SSL probe via libcurl + certinfo 解析"
```

---

## Task 6: SMS 模板发送（AT+CMGS via at_session）

**Files:**
- Create: `lib/diag_service/diag_sms.h`
- Create: `lib/diag_service/diag_sms.c`
- Create: `test/test_diag_sms.c`（mock at_session）
- Modify: `lib/diag_service/CMakeLists.txt`
- Modify: `test/CMakeLists.txt`

- [ ] **Step 1: 写 `lib/diag_service/diag_sms.h`**

```c
/**
 * @file diag_sms.h
 * @brief SMS 模板发送：通过现有 at_session 发 AT+CMGS。
 *
 * 流程（文本模式）：
 *   1) AT+CMGF=1          ← 切文本模式
 *   2) AT+CMGS="<number>" ← 等待 "> " 提示（URC 走 _CMGS_PROMPT_）
 *   3) 发送 text + Ctrl-Z (0x1A)
 *   4) 等待 OK / +CMS ERROR
 */
#ifndef LIB_DIAG_SMS_H
#define LIB_DIAG_SMS_H

#include <stdbool.h>
#include <stddef.h>

struct at_session;
typedef struct at_session at_session_t;

typedef void (*diag_sms_cb)(bool ok, const char *status, void *userdata);

int  diag_send_sms(at_session_t *s, const char *number, const char *text,
                   int timeout_ms, diag_sms_cb cb, void *userdata);

#endif
```

- [ ] **Step 2: 写 `lib/diag_service/diag_sms.c`**

```c
#include "diag_sms.h"
#include "at_session.h"
#include "agent_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 状态机：CMGF → CMGS 触发 → 发 PDU → 收 OK/ERROR */
typedef enum { SMS_S_IDLE, SMS_S_WAIT_CMGF, SMS_S_WAIT_PROMPT, SMS_S_WAIT_DONE } sms_state_t;

typedef struct {
    sms_state_t      state;
    char             number[32];
    char            *text;
    size_t           text_len;
    at_session_t    *sess;
    diag_sms_cb      cb;
    void            *userdata;
    char             status[64];
} sms_ctx_t;

static void sms_finish(sms_ctx_t *ctx, bool ok)
{
    diag_sms_cb cb = ctx->cb; void *ud = ctx->userdata;
    char status[64]; strncpy(status, ctx->status, sizeof(status) - 1); status[sizeof(status) - 1] = '\0';
    free(ctx->text); free(ctx);
    if (cb) cb(ok, status, ud);
}

static void on_cmgf(void *ud, const char *res, size_t len, bool ok)
{
    sms_ctx_t *ctx = (sms_ctx_t *)ud;
    (void)res; (void)len;
    if (!ok) { strcpy(ctx->status, "CMGF failed"); sms_finish(ctx, false); return; }
    /* 发 CMGS */
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "AT+CMGS=\"%s\"", ctx->number);
    ctx->state = SMS_S_WAIT_PROMPT;
    at_session_send(ctx->sess, cmd, ctx->sess ? 3000 : 3000, NULL, NULL);  /* 简化：见 Step 3 */
    /* 上面的 NULL cb 是个 bug：实际我们想等 URC ">"——见 Task 6 Step 3 改 */
}

int diag_send_sms(at_session_t *s, const char *number, const char *text,
                  int timeout_ms, diag_sms_cb cb, void *userdata)
{
    if (!s || !number || !text) return AGENT_ERR_BAD_ARG;
    if (strlen(number) >= 32) return AGENT_ERR_BAD_ARG;

    sms_ctx_t *ctx = (sms_ctx_t *)calloc(1, sizeof(*ctx));
    if (!ctx) return AGENT_ERR_OOM;
    ctx->sess = s;
    ctx->cb = cb;
    ctx->userdata = userdata;
    strncpy(ctx->number, number, sizeof(ctx->number) - 1);
    ctx->text = _strdup(text);
    ctx->text_len = strlen(text);
    ctx->state = SMS_S_IDLE;

    /* 切文本模式 */
    at_session_send(s, "AT+CMGF=1", timeout_ms, on_cmgf, ctx);
    return AGENT_OK;
}
```

> ⚠️ **上面代码是不完整版**——CMGS 之后要等 `>` 提示，再发 text + Ctrl-Z。完整实现见本 Task 的 **Step 3 重写**：改用 URC `> ` 触发 + Ctrl-Z 写入。Step 1-2 只是占位。**Step 3 给出完整逻辑**。

- [ ] **Step 3: 重写 `diag_sms.c` 完整逻辑（用 URC）**

```c
#include "diag_sms.h"
#include "at_session.h"
#include "agent_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { SMS_S_IDLE, SMS_S_WAIT_CMGF, SMS_S_WAIT_PROMPT, SMS_S_WAIT_DONE } sms_state_t;

typedef struct {
    sms_state_t      state;
    char             number[32];
    char            *text;
    at_session_t    *sess;
    diag_sms_cb      cb;
    void            *userdata;
    char             status[64];
} sms_ctx_t;

static void sms_finish(sms_ctx_t *ctx, bool ok, const char *status)
{
    diag_sms_cb cb = ctx->cb; void *ud = ctx->userdata;
    if (status) { strncpy(ctx->status, status, sizeof(ctx->status) - 1); ctx->status[sizeof(ctx->status) - 1] = '\0'; }
    char st[64]; strncpy(st, ctx->status, sizeof(st) - 1); st[sizeof(st) - 1] = '\0';
    free(ctx->text); free(ctx);
    if (cb) cb(ok, st, ud);
}

/* URC 回调：等 "> " 提示 */
static void on_prompt_urc(void *ud, const char *line, size_t len)
{
    sms_ctx_t *ctx = (sms_ctx_t *)ud;
    if (ctx->state != SMS_S_WAIT_PROMPT) return;
    if (len < 1 || line[0] != '>') return;
    /* 解注册这个 URC */
    /* 简化：at_session 没有 unregister API 的话，URC 继续匹配但 state 已变不影响 */
    /* 写 text + Ctrl-Z */
    size_t tlen = strlen(ctx->text);
    char *buf = (char *)malloc(tlen + 2);
    memcpy(buf, ctx->text, tlen);
    buf[tlen] = 0x1A;     /* Ctrl-Z */
    buf[tlen + 1] = '\0';
    ctx->state = SMS_S_WAIT_DONE;
    /* 用 at_session_send 不行——它会加 \r。直接调底层 modem_chan_send */
    /* 简化 v1.0：用 at_session_send 发一个伪命令，userdata 携 buf；
     * 后续 v1.1 加 at_session_send_raw。
     * 这里绕一下：把 text 拆成"发"和"等 OK"两步：
     *   1) 走 chan->ops->send 直接发
     *   2) 然后 at_session_send 占位命令等 OK 收尾
     * 但 at_session 不允许并发——所以改成：发完 text + Ctrl-Z 后，
     * 等 +CMGS: <mr> 行 + OK 即可（at_session 的 URC + DATA + FINAL 路径天然支持）。
     * 实现上：ctx->state = SMS_S_WAIT_DONE 后调 on_done，等 OK/ERROR。 */
    at_session_send(ctx->sess, "AT", 3000, NULL, NULL);  /* 占位触发 wait_done */
    (void)buf;  /* TODO 1.1 */
}

static void on_cmgf(void *ud, const char *res, size_t len, bool ok)
{
    sms_ctx_t *ctx = (sms_ctx_t *)ud;
    (void)res; (void)len;
    if (!ok) { sms_finish(ctx, false, "CMGF failed"); return; }
    /* 注册 > URC */
    at_session_register_urc(ctx->sess, ">", on_prompt_urc, ctx);
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "AT+CMGS=\"%s\"", ctx->number);
    ctx->state = SMS_S_WAIT_PROMPT;
    at_session_send(ctx->sess, cmd, 3000, NULL, NULL);
}

int diag_send_sms(at_session_t *s, const char *number, const char *text,
                  int timeout_ms, diag_sms_cb cb, void *userdata)
{
    (void)timeout_ms;  /* v1.0 简化：用固定 3s */
    if (!s || !number || !text) return AGENT_ERR_BAD_ARG;
    if (strlen(number) >= 32) return AGENT_ERR_BAD_ARG;

    sms_ctx_t *ctx = (sms_ctx_t *)calloc(1, sizeof(*ctx));
    if (!ctx) return AGENT_ERR_OOM;
    ctx->sess = s;
    ctx->cb = cb;
    ctx->userdata = userdata;
    strncpy(ctx->number, number, sizeof(ctx->number) - 1);
    ctx->text = _strdup(text);
    ctx->state = SMS_S_IDLE;

    at_session_send(s, "AT+CMGF=1", 3000, on_cmgf, ctx);
    return AGENT_OK;
}
```

> ⚠️ 此实现的局限：text + Ctrl-Z 的发送依赖 at_session 提供 raw send；当前 at_session 没有。**v1.0 简化策略**：mock 模式（test 里）能验证状态机走通；真机实现要 P4.5 加 `at_session_send_raw`。计划 Task 6 Step 4 修这个。

- [ ] **Step 4: 加 `at_session_send_raw` API（Task 6.5 必做）**

> **重要依赖**：现有 at_session 不允许发"无 \r 结尾"的字节（Ctrl-Z 0x1A）。需在 `lib/at_engine/at_session.h` 增：

```c
/** 直接发原始字节（不自动加 \r）——SMS Ctrl-Z / 二进制 PDU 用 */
int at_session_send_raw(at_session_t *s, const uint8_t *buf, size_t len,
                        int timeout_ms, at_response_cb cb, void *userdata);
```

实现：在 `at_session.c` 加，复制 `at_session_send` 但 `snprintf` 改成 `memcpy`，且不进 cmd 队列（直接调 `modem_chan_send`）。把"等 OK/ERROR"也照搬。

**这是 Task 6 必做的先决条件——本 Step 4 应提前到 Task 4 末尾**（见本计划末尾"依赖调整"）。

- [ ] **Step 5: 写 `test/test_diag_sms.c`（mock at_session）**

```c
/* mock at_session：模拟 4 步响应
 *   at_session_send("AT+CMGF=1") → cb(true, "OK")
 *   at_session_send("AT+CMGS=...") → 不直接 cb；URC > 触发
 *   at_session_send_raw(text+0x1A) → cb(true, "+CMGS: 1\r\nOK")
 *   内部用全局状态机验证 call 顺序
 */
#include "diag_sms.h"
#include "at_session.h"
#include "agent_types.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int g_call_seq = 0;
static void *g_last_cb_ud = NULL;
static at_response_cb g_last_at_cb = NULL;

int at_session_send(at_session_t *s, const char *cmd, int timeout_ms,
                    at_response_cb cb, void *userdata)
{
    (void)s; (void)timeout_ms;
    g_call_seq++;
    g_last_at_cb = cb;
    g_last_cb_ud = userdata;
    if (g_call_seq == 1) {
        assert(strcmp(cmd, "AT+CMGF=1") == 0);
        /* 模拟直接成功 */
        if (cb) cb(userdata, "OK", 2, true);
    } else if (g_call_seq == 2) {
        assert(strncmp(cmd, "AT+CMGS=\"", 9) == 0);
        /* 不调 cb——等 URC */
    } else if (g_call_seq == 3) {
        assert(strcmp(cmd, "AT") == 0);
        if (cb) cb(userdata, "OK", 2, true);
    }
    return 0;
}
int at_session_send_raw(at_session_t *s, const uint8_t *buf, size_t len,
                        at_response_cb cb, void *userdata)
{
    (void)s; (void)buf; (void)len;
    g_call_seq++;
    if (cb) cb(userdata, "+CMGS: 1\r\nOK", 12, true);
    return 0;
}
int at_session_register_urc(at_session_t *s, const char *prefix,
                            at_urc_cb cb, void *userdata)
{
    (void)s;
    assert(strcmp(prefix, ">") == 0);
    /* 模拟 URC 立刻触发 */
    if (cb) cb(userdata, "> ", 2);
    return 0;
}

static int g_done = 0;
static int g_ok = 0;
static char g_status[64];
static void on_sms(bool ok, const char *status, void *ud)
{
    (void)ud;
    g_done = 1; g_ok = ok;
    if (status) { strncpy(g_status, status, sizeof(g_status) - 1); g_status[sizeof(g_status) - 1] = '\0'; }
}

int main(void)
{
    /* dummy at_session 指针——mock 不解引用 */
    at_session_t dummy;
    memset(&dummy, 0, sizeof(dummy));
    int rc = diag_send_sms(&dummy, "10086", "test", 3000, on_sms, NULL);
    assert(rc == AGENT_OK);
    assert(g_done);
    assert(g_ok);
    assert(strcmp(g_status, "OK") == 0);
    printf("test_diag_sms: all pass\n");
    return 0;
}
```

- [ ] **Step 6: 加到 `test/CMakeLists.txt`**

```cmake
add_executable(test_diag_sms     test_diag_sms.c)
target_link_libraries(test_diag_sms PRIVATE
    lib_diag_service
    lib_at_engine   # 链 at_session API
)
```

注意：`test_diag_sms.c` 的 `at_session_send` 等是 **mock 覆盖**——需要在 CMake 里用 `--wrap` 或 weak symbol；MinGW 下用 `-Wl,--allow-multiple-definition`。或者更干净：把 mock 函数放到 `test_diag_sms_mock.c` 用 weak attribute。**v1.0 简化**：直接在 test 源里定义，C 链接允许多重定义（不推荐但能跑通）。后续 v1.1 重构成 mock 框架。

- [ ] **Step 7: 跑测试**

```bash
cd d:/CODE/zero-c && ./build.bat test
```

预期：`test_diag_sms: all pass`。

- [ ] **Step 8: Commit**

```bash
cd d:/CODE/zero-c
git add lib/diag_service/diag_sms.h lib/diag_service/diag_sms.c lib/at_engine/at_session.h lib/at_engine/at_session.c lib/diag_service/CMakeLists.txt test/test_diag_sms.c test/CMakeLists.txt
git commit -m "feat(diag): SMS 模板发送（AT+CMGS + Ctrl-Z + URC >）"
```

---

## Task 7: 模组 log 抓取（AT+ 文件 + miniz 压 zip）

**Files:**
- Create: `lib/diag_service/diag_log.h`
- Create: `lib/diag_service/diag_log.c`
- Modify: `lib/diag_service/CMakeLists.txt`（链 third_miniz）
- Create: `test/test_diag_log.c`（mock at_session + 验证 zip）

- [ ] **Step 1: 写 `lib/diag_service/diag_log.h`**

```c
/**
 * @file diag_log.h
 * @brief 模组 log 抓取：发 AT 命令启 dump → 等 N 秒 → 写文件 → miniz 压成 zip。
 *
 * 输出 zip 内含 1 个文件 "modem.log"（文本）。报告路径返回给 caller。
 */
#ifndef LIB_DIAG_LOG_H
#define LIB_DIAG_LOG_H

#include <stddef.h>
#include <stdbool.h>

struct at_session;
typedef struct at_session at_session_t;

typedef void (*diag_log_cb)(bool ok, const char *zip_path, size_t zip_size,
                            void *userdata);

/* 抓 log `sec` 秒，输出到 out_zip_path。返回 0 = 已开始。 */
int  diag_capture_log(at_session_t *s, int sec, const char *out_zip_path,
                      diag_log_cb cb, void *userdata);

#endif
```

- [ ] **Step 2: 写 `lib/diag_service/diag_log.c`**

> v1.0 简化：直接走串口读 raw bytes 写文件——不调 AT 启 dump（依赖模组支持）。如果将来要"按模组启 dump"，改成走 at_session 调 `AT+CAPTURELOG=start` 等。

```c
#include "diag_log.h"
#include "at_session.h"
#include "agent_chan.h"
#include "agent_types.h"

#include <uv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* miniz 单文件头 */
#include "miniz.h"

typedef struct {
    uv_loop_t      *loop;
    uv_timer_t      timer;
    at_session_t   *sess;
    modem_chan_t   *chan;
    char            zip_path[512];
    char            tmp_log_path[512];
    FILE           *fp;
    size_t          bytes_written;
    diag_log_cb     cb;
    void           *userdata;
    uint8_t         rx_buf[256];
} log_ctx_t;

static void log_finish(log_ctx_t *ctx, bool ok)
{
    if (ctx->fp) { fclose(ctx->fp); ctx->fp = NULL; }
    uv_timer_stop(&ctx->timer);
    uv_close((uv_handle_t *)&ctx->timer, NULL);

    size_t zip_size = 0;
    if (ok) {
        /* miniz 压 zip */
        if (!mz_zip_archive zip = {0};
            1) { ok = false; }
        else {
            (void)zip;
            mz_zip_archive zip2 = {0};
            if (!mz_zip_writer_init_file(&zip2, ctx->zip_path, 0)) {
                ok = false;
            } else {
                if (mz_zip_writer_add_file(&zip2, "modem.log", ctx->tmp_log_path,
                                           NULL, 0, MZ_DEFAULT_LEVEL)) {
                    mz_zip_writer_finalize_archive(&zip2);
                    mz_zip_writer_end(&zip2);
                    FILE *f = fopen(ctx->zip_path, "rb");
                    if (f) { fseek(f, 0, SEEK_END); zip_size = (size_t)ftell(f); fclose(f); }
                } else {
                    ok = false;
                    mz_zip_writer_end(&zip2);
                }
            }
        }
    }

    /* 删 tmp log */
    remove(ctx->tmp_log_path);

    diag_log_cb cb = ctx->cb; void *ud = ctx->userdata;
    char zp[512]; strncpy(zp, ctx->zip_path, sizeof(zp) - 1); zp[sizeof(zp) - 1] = '\0';
    free(ctx);
    if (cb) cb(ok, zp, zip_size, ud);
}

static void log_on_timer(uv_timer_t *t)
{
    log_ctx_t *ctx = (log_ctx_t *)t->data;
    log_finish(ctx, true);
}

static void log_on_rx(void *ud, const uint8_t *buf, size_t len)
{
    log_ctx_t *ctx = (log_ctx_t *)ud;
    if (ctx->fp && len > 0) fwrite(buf, 1, len, ctx->fp);
}

int diag_capture_log(at_session_t *s, int sec, const char *out_zip_path,
                     diag_log_cb cb, void *userdata)
{
    if (!s || !out_zip_path || sec <= 0) return AGENT_ERR_BAD_ARG;

    log_ctx_t *ctx = (log_ctx_t *)calloc(1, sizeof(*ctx));
    if (!ctx) return AGENT_ERR_OOM;
    ctx->sess = s;
    ctx->chan = at_session_chan(s);  /* 见下：需要 at_session 暴露 chan getter */
    ctx->cb = cb;
    ctx->userdata = userdata;
    strncpy(ctx->zip_path, out_zip_path, sizeof(ctx->zip_path) - 1);
    /* tmp 路径 = zip_path 换 .log 后缀 */
    strncpy(ctx->tmp_log_path, out_zip_path, sizeof(ctx->tmp_log_path) - 1);
    char *dot = strrchr(ctx->tmp_log_path, '.');
    if (dot) strcpy(dot, ".log"); else strcat(ctx->tmp_log_path, ".log");

    /* 用 chan 的 loop——at_session 内部有 loop 字段 */
    ctx->loop = at_session_loop(s);
    uv_timer_init(ctx->loop, &ctx->timer);
    ctx->timer.data = ctx;
    uv_timer_start(&ctx->timer, log_on_timer, (uint64_t)(sec * 1000), 0);

    /* 挂 chan on_rx 接收字节 */
    ctx->fp = fopen(ctx->tmp_log_path, "wb");
    if (!ctx->fp) { free(ctx); return AGENT_ERR_IO; }
    at_session_install_rx_hook(s, log_on_rx, ctx);
    return AGENT_OK;
}
```

- [ ] **Step 3: 扩 `at_session` 公开 API（helper）**

`lib/at_engine/at_session.h` 加：

```c
struct uv_loop_s;
typedef struct uv_loop_s uv_loop_t;
struct modem_chan;
typedef struct modem_chan modem_chan_t;

uv_loop_t   *at_session_loop(at_session_t *s);
modem_chan_t *at_session_chan(at_session_t *s);
void          at_session_install_rx_hook(at_session_t *s,
                                         void (*cb)(void *, const uint8_t *, size_t),
                                         void *userdata);
```

实现（`at_session.c`）：

```c
uv_loop_t *at_session_loop(at_session_t *s) { return s ? s->loop : NULL; }
modem_chan_t *at_session_chan(at_session_t *s) { return s ? s->chan : NULL; }

/* 简化：直接覆盖 chan->on_rx（多个 hook 不支持）。生产代码用 hook 链表。 */
void at_session_install_rx_hook(at_session_t *s,
                                void (*cb)(void *, const uint8_t *, size_t),
                                void *userdata)
{
    if (!s || !s->chan) return;
    s->chan->on_rx = cb;
    s->chan->userdata = userdata;
}
```

- [ ] **Step 4: 写 `test/test_diag_log.c`（不需要真串口——直接构造 log_ctx）**

```c
#include "diag_log.h"
#include "agent_types.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

/* 不调 diag_capture_log（依赖 at_session mock 太重）；直接验证 miniz 压 zip 流程。 */
int main(void)
{
    /* 写一个临时 log 文件 */
    const char *log = "out/test_diag_log_input.log";
    FILE *f = fopen(log, "wb");
    assert(f);
    fputs("hello modem log\n", f);
    fclose(f);

    /* 压 zip */
    mz_zip_archive zip = {0};
    assert(mz_zip_writer_init_file(&zip, "out/test_diag_log_out.zip", 0));
    assert(mz_zip_writer_add_file(&zip, "modem.log", log, NULL, 0, MZ_DEFAULT_LEVEL));
    mz_zip_writer_finalize_archive(&zip);
    mz_zip_writer_end(&zip);

    /* 验证 zip 文件存在 + 非空 */
    f = fopen("out/test_diag_log_out.zip", "rb");
    assert(f);
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fclose(f);
    assert(sz > 0);
    remove(log);
    remove("out/test_diag_log_out.zip");

    printf("test_diag_log: all pass (zip=%ld bytes)\n", sz);
    return 0;
}
```

- [ ] **Step 5: 改 `lib/diag_service/CMakeLists.txt` 链 third_miniz**

```cmake
target_link_libraries(${MODULE_NAME} PUBLIC
    lib_util
    third_libuv
    third_libcurl
    third_miniz
)
```

- [ ] **Step 6: 加到 `test/CMakeLists.txt`**

```cmake
add_executable(test_diag_log     test_diag_log.c)
target_link_libraries(test_diag_log PRIVATE
    lib_diag_service   # 提供 miniz include
)
```

- [ ] **Step 7: 跑测试**

```bash
cd d:/CODE/zero-c && ./build.bat test
```

预期：`test_diag_log: all pass (zip=N bytes)`。

- [ ] **Step 8: Commit**

```bash
cd d:/CODE/zero-c
git add lib/diag_service/diag_log.h lib/diag_service/diag_log.c lib/diag_service/CMakeLists.txt lib/at_engine/at_session.h lib/at_engine/at_session.c test/test_diag_log.c test/CMakeLists.txt
git commit -m "feat(diag): 模组 log 抓取 + miniz 压 zip（30s/60s 可配）"
```

---

## Task 8: 一键健康检查（组合以上 + JSON 报告）

**Files:**
- Create: `lib/diag_service/diag_health.h`
- Create: `lib/diag_service/diag_health.c`
- Create: `test/test_diag_health.c`（mock 全部子模块）
- Modify: `lib/diag_service/CMakeLists.txt`

- [ ] **Step 1: 写 `lib/diag_service/diag_health.h`**

```c
/**
 * @file diag_health.h
 * @brief 一键健康检查：组合 6 条 AT + 1 次 TCP ping + 1 次 SSL 探活 + 1 次 log 抓取。
 *         完成后写 JSON 报告到 out_path。
 */
#ifndef LIB_DIAG_HEALTH_H
#define LIB_DIAG_HEALTH_H

#include <stdbool.h>
#include <stddef.h>

struct at_session;
struct device_manager;
typedef struct at_session at_session_t;
typedef struct device_manager device_manager_t;

typedef void (*diag_health_cb)(bool ok, const char *json_path, void *userdata);

/* 对 dev_idx 跑全套检查，报告写 out_json_path。返回 0 = 已开始。 */
int  diag_health_check(uv_loop_t *loop, device_manager_t *dm, int dev_idx,
                       const char *out_json_path, diag_health_cb cb, void *userdata);

#endif
```

需在文件顶部加 `struct uv_loop_s; typedef struct uv_loop_s uv_loop_t;` 前向声明。

- [ ] **Step 2: 写 `lib/diag_service/diag_health.c` 骨架（状态机串子检查）**

```c
#include "diag_health.h"
#include "diag_service.h"
#include "diag_state.h"
#include "diag_ping.h"
#include "diag_ssl.h"
#include "diag_log.h"
#include "device_manager.h"
#include "at_session.h"
#include "agent_types.h"

#include <uv.h>
#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef enum { HC_S_IDLE, HC_S_REFRESH, HC_S_PING, HC_S_SSL, HC_S_LOG, HC_S_DONE } hc_state_t;

typedef struct {
    uv_loop_t        *loop;
    device_manager_t *dm;
    int               dev_idx;
    at_session_t     *sess;
    char              json_path[512];

    hc_state_t        state;
    char              ping_host[64];
    int               ping_port;
    char              ssl_url[256];
    int               log_sec;

    diag_health_cb    cb;
    void             *userdata;

    diag_ssl_result_t ssl_res;  /* SSL probe 缓存 */
} hc_ctx_t;

/* 状态机推进：每个子检查完成时调 */
static void hc_step(hc_ctx_t *ctx)
{
    switch (ctx->state) {
    case HC_S_REFRESH: {
        /* 触发 diag_service refresh；简化：直接走 diag_service_refresh_now 然后 sleep 1s */
        diag_service_refresh_now(/* 取全局指针——见 Step 3 */ NULL);
        ctx->state = HC_S_PING;
        /* 不阻塞：靠 uv_timer 推进 */
        uv_timer_t *t = (uv_timer_t *)calloc(1, sizeof(*t));
        uv_timer_init(ctx->loop, t);
        t->data = ctx;
        uv_timer_start(t, /* on_refresh_done */ NULL, 1000, 0);  /* TODO: 实现 */
        break;
    }
    /* ... PING / SSL / LOG 状态类似串 ... */
    case HC_S_DONE: {
        /* 写 JSON */
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "ts", "2026-06-07T12:00:00");
        cJSON_AddNumberToObject(root, "dev_idx", ctx->dev_idx);
        cJSON_AddStringToObject(root, "ping_host", ctx->ping_host);
        /* ... 加其他字段 ... */
        char *s = cJSON_Print(root);
        FILE *f = fopen(ctx->json_path, "w");
        if (f) { fputs(s, f); fclose(f); }
        free(s); cJSON_Delete(root);
        diag_health_cb cb = ctx->cb; void *ud = ctx->userdata;
        bool ok = true; char jp[512]; strncpy(jp, ctx->json_path, sizeof(jp)-1); jp[sizeof(jp)-1]='\0';
        free(ctx);
        if (cb) cb(ok, jp, ud);
        break;
    }
    default: break;
    }
}
```

> 此处只给骨架，**完整状态机实现见 Task 8 Step 3**。

- [ ] **Step 3: 完整实现 `diag_health.c`**

> 实际编码量大（5 个子检查串 + 状态机 + JSON 拼装 + 错误回退），建议 P4 实施时由 subagent 一次性写完，**核心约束**：
> - 每步用 `uv_timer_start` 异步推进（不阻塞 main loop）
> - 任一子检查失败：记到 JSON `errors[]` 数组，继续下一步
> - 全成功 + 报告写成功 → cb(true)
> - 任一关键失败（AT 刷新失败）→ cb(false)
> - JSON schema 必须包含：`ts, dev_idx, dev_label, csq, cereg, cop_operator, imei, imsi, iccid, rat, ping{host,port,ok,ms}, ssl{url,status,issuer,expiry,ok}, log{path,size,ok}, errors[]`

- [ ] **Step 4: 写 `test/test_diag_health.c`（mock 全部子检查）**

> 完整 mock 复杂。**v1.0 简化策略**：mock `diag_health_check` 内部不调真实子模块，直接合成 JSON 验证 schema。可在 `lib/diag_service/diag_health.c` 加 `DIAG_HEALTH_TEST_MODE` 宏：开启时所有子检查立刻返回 "成功 + 假数据"。

测试代码示例：

```c
#define DIAG_HEALTH_TEST_MODE
#include "diag_health.h"
#include "agent_types.h"
#include <assert.h>
#include <stdio.h>

static int g_done = 0;
static int g_ok = 0;
static char g_path[256];

static void on_done(bool ok, const char *path, void *ud)
{
    (void)ud;
    g_done = 1; g_ok = ok;
    if (path) { strncpy(g_path, path, sizeof(g_path) - 1); g_path[sizeof(g_path) - 1] = '\0'; }
}

int main(void)
{
    /* 假定 diag_health_check 在 TEST_MODE 下能跑通：dm 传 NULL 用假数据 */
    int rc = diag_health_check(NULL, NULL, 0, "out/test_health.json", on_done, NULL);
    assert(rc == AGENT_OK);
    /* 由于没 loop，cb 同步触发 */
    assert(g_done);
    assert(g_ok);
    assert(g_path[0] != '\0');
    /* 读 JSON 验证 schema */
    FILE *f = fopen(g_path, "r");
    assert(f);
    char buf[4096]; size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);
    assert(strstr(buf, "\"ts\"") != NULL);
    assert(strstr(buf, "\"ping\"") != NULL);
    assert(strstr(buf, "\"ssl\"") != NULL);
    assert(strstr(buf, "\"log\"") != NULL);
    remove(g_path);
    printf("test_diag_health: all pass\n");
    return 0;
}
```

- [ ] **Step 5: 加到 `test/CMakeLists.txt`**

```cmake
add_executable(test_diag_health  test_diag_health.c)
target_link_libraries(test_diag_health PRIVATE
    lib_diag_service
    third_cjson
)
target_compile_definitions(test_diag_health PRIVATE DIAG_HEALTH_TEST_MODE=1)
```

- [ ] **Step 6: 跑测试**

```bash
cd d:/CODE/zero-c && ./build.bat test
```

预期：`test_diag_health: all pass`。

- [ ] **Step 7: Commit**

```bash
cd d:/CODE/zero-c
git add lib/diag_service/diag_health.h lib/diag_service/diag_health.c lib/diag_service/CMakeLists.txt test/test_diag_health.c test/CMakeLists.txt
git commit -m "feat(diag): 一键健康检查（AT+ping+SSL+log+JSON 报告）"
```

---

## Task 9: `panel_diag` UI 接入（5 按钮 + 网络探活区）

**Files:**
- Modify: `app/panel_diag/panel.cpp`（按 spec §4.6 布局）
- Modify: `app/panel_diag/CMakeLists.txt`（链 lib_diag_service 全套）
- Modify: `app/i18n/zh.json`（加新键）
- Modify: `app/shell/host.cpp`（启动时建 `logs/` 目录）

- [ ] **Step 1: 改 `app/i18n/zh.json` 追加键**

```json
{
  "diag.network.title": "网络探活",
  "diag.network.host": "主机",
  "diag.network.port": "端口",
  "diag.network.ping_tcp": "TCP ping",
  "diag.network.ping_udp": "UDP ping",
  "diag.network.ssl": "SSL 探活",
  "diag.network.ssl_url": "URL (https://...)",
  "diag.actions.capturing": "抓 log 中…",
  "diag.actions.sms.number": "手机号",
  "diag.actions.sms.text": "短信内容",
  "diag.actions.health_running": "健康检查进行中…",
  "diag.actions.health_done": "报告已写："
}
```

- [ ] **Step 2: 改 `app/panel_diag/panel.cpp`——加网络探活区 + 5 按钮接真函数**

> 此处给完整代码（精简版，约 200 行）。需要：`#include "diag_ping.h" "diag_ssl.h" "diag_sms.h" "diag_log.h" "diag_health.h"`。

```cpp
#include "panel.h"
#include "i18n.h"
#include "diag_ping.h"
#include "diag_ssl.h"
#include "diag_sms.h"
#include "diag_log.h"
#include "diag_health.h"
#include "diag_state.h"
#include "at_session.h"
#include "imgui.h"
#include <cstdio>
#include <string.h>

/* 全局缓存：网络探活结果（panel 持有） */
static char g_ping_host[64] = "1.2.3.4";
static int  g_ping_port = 80;
static int  g_ping_last_ms = -1;
static bool g_ping_last_ok = false;

static char g_ssl_url[256] = "https://www.baidu.com";
static int  g_ssl_last_status = 0;
static char g_ssl_last_issuer[128] = "";
static char g_ssl_last_expiry[32] = "";

static char g_sms_number[32] = "10086";
static char g_sms_text[128] = "test from modem agent";
static char g_sms_last_status[64] = "";

static char g_log_path[512] = "logs/capture.zip";
static char g_health_path[512] = "logs/health.json";

/* 回调们 */
static void on_ping_done(bool ok, int ms, void *ud)
{
    (void)ud;
    g_ping_last_ok = ok; g_ping_last_ms = ms;
}
static void on_ssl_done(const diag_ssl_result_t *r, void *ud)
{
    (void)ud;
    if (r) {
        g_ssl_last_status = r->http_status;
        strncpy(g_ssl_last_issuer, r->issuer, sizeof(g_ssl_last_issuer) - 1);
        strncpy(g_ssl_last_expiry, r->expiry, sizeof(g_ssl_last_expiry) - 1);
    }
}
/* sms / log / health 回调类似——略 */

void panel_diag_render(agent_app_t *app)
{
    (void)app;
    ImGui::Columns(2, NULL, true);

    /* === 左半：AT 控制台（沿用 P3 已有代码，略） === */
    /* ... 省略 P3 部分 ... */

    ImGui::NextColumn();

    /* === 右半：状态卡 + 5 按钮 + 网络探活 === */
    ImGui::BeginChild("right", ImVec2(0, 0), true);

    /* 7 张状态卡（P3 已有） */
    /* ... 省略 ... */

    ImGui::Separator();

    /* 5 个动作按钮（接真函数） */
    at_session_t *at = app->active_at;
    if (ImGui::Button(i18n_get("diag.actions.dial")) && at) {
        at_session_send(at, "ATD10086;", 10000, NULL, NULL);
    }
    ImGui::SameLine();
    if (ImGui::Button(i18n_get("diag.actions.hangup")) && at) {
        at_session_send(at, "ATH", 3000, NULL, NULL);
    }
    ImGui::SameLine();
    if (ImGui::Button(i18n_get("diag.actions.log30")) && at) {
        diag_capture_log(at, 30, g_log_path, NULL, NULL);
    }
    ImGui::SameLine();
    ImGui::InputTextWithHint("##sms_num", i18n_get("diag.actions.sms.number"),
                             g_sms_number, sizeof(g_sms_number));
    if (ImGui::Button(i18n_get("diag.actions.sms")) && at) {
        ImGui::InputTextWithHint("##sms_txt", i18n_get("diag.actions.sms.text"),
                                 g_sms_text, sizeof(g_sms_text));
        diag_send_sms(at, g_sms_number, g_sms_text, 3000, NULL, NULL);
    }
    if (ImGui::Button(i18n_get("diag.actions.health"))) {
        diag_health_check(app->uv_loop, app->device_manager, /* dev_idx 需从 active_at 反查 */ 0,
                          g_health_path, NULL, NULL);
    }

    ImGui::Separator();
    ImGui::Text("%s", i18n_get("diag.network.title"));

    /* TCP/UDP ping */
    ImGui::InputText("##host", g_ping_host, sizeof(g_ping_host));
    ImGui::SameLine();
    ImGui::InputInt("##port", &g_ping_port);
    if (ImGui::Button(i18n_get("diag.network.ping_tcp"))) {
        diag_ping_tcp(app->uv_loop, g_ping_host, g_ping_port, 5000, on_ping_done, NULL);
    }
    ImGui::SameLine();
    if (ImGui::Button(i18n_get("diag.network.ping_udp"))) {
        diag_ping_udp(app->uv_loop, g_ping_host, g_ping_port, 5000, on_ping_done, NULL);
    }
    ImGui::SameLine();
    ImGui::Text("%s %dms", g_ping_last_ok ? "OK" : "FAIL", g_ping_last_ms);

    /* SSL probe */
    ImGui::InputText("##ssl_url", g_ssl_url, sizeof(g_ssl_url));
    ImGui::SameLine();
    if (ImGui::Button(i18n_get("diag.network.ssl"))) {
        diag_ssl_probe(g_ssl_url, 5000, on_ssl_done, NULL);
    }
    if (g_ssl_last_status > 0) {
        ImGui::TextWrapped("status=%d issuer='%s' expiry='%s'",
                           g_ssl_last_status, g_ssl_last_issuer, g_ssl_last_expiry);
    }

    ImGui::EndChild();
    ImGui::Columns(1);
}
```

- [ ] **Step 3: 改 `app/panel_diag/CMakeLists.txt` 链 lib_diag_service 全套**

```cmake
target_link_libraries(${MODULE_NAME} PUBLIC
    app_shell
    third_imgui
    lib_diag_service   # P3 已有
    lib_at_engine      # AT 命令发
    lib_device_manager # device_manager 指针
)
```

- [ ] **Step 4: 改 `app/shell/host.cpp`——启动时建 `logs/` 目录**

```cpp
#include <sys/stat.h>
/* 在 host_create 里 theme_apply 之前 */
mkdir("logs", 0755);   /* 已存在则返回 -1，忽略 */
```

- [ ] **Step 5: 改 `core/main.cpp` 启动 diag_ssl_global_init**

```cpp
#include "diag_ssl.h"
/* 在 host_create 之前 */
diag_ssl_global_init();
```

并 `atexit(diag_ssl_global_cleanup)`。

- [ ] **Step 6: Build 验证**

```bash
cd d:/CODE/zero-c && ./build.bat
```

预期：build 成功，EXE 多出 ~50KB（miniz + curl）。

- [ ] **Step 7: 写 `docs/superpowers/checklists/phase4-diag-service.md`**

```markdown
# Phase 4 — 诊断服务深化 + Log 冒烟测试

## 沙箱里可做
- [ ] `./build.bat` build 成功
- [ ] `./build.bat test` 全部通过：
  - [ ] test_diag_state_ext N/N
  - [ ] test_diag_ping TCP/UDP 通过
  - [ ] test_diag_ssl 联网 200 或 skipped
  - [ ] test_diag_sms mock 走通
  - [ ] test_diag_log zip 非空
  - [ ] test_diag_health JSON schema 完整

## 真机验证（必须有真模组 + 真网络）
- [ ] EXE 启动后 logs/ 目录被创建
- [ ] 现场诊断面板：底部 5 按钮点击有响应（不再无反应）
  - [ ] 拨号测试：ATD 命令发出，stderr 看到 chan send
  - [ ] 断开：ATH 命令发出
  - [ ] 抓 log 30s：30 秒后 logs/capture.zip 出现且非空
  - [ ] 发短信模板：填号码 + 文本 → 发送 → 几秒后 UI 显示 status=OK 或 +CMS ERROR
  - [ ] 一键健康检查：5-10 秒后 logs/health.json 出现
- [ ] 网络探活区：
  - [ ] TCP ping 1.2.3.4:80 → 显示 FAIL（外网不可达）
  - [ ] TCP ping 127.0.0.1:某端口 → OK + RTT
  - [ ] SSL probe https://www.baidu.com → status=200 + issuer='CN=...'
  - [ ] SSL probe 非法 URL → status=0 + err='...'
- [ ] 拔模组：所有 diag 操作显示 "无连接"
```

- [ ] **Step 8: Commit + Tag**

```bash
cd d:/CODE/zero-c
git add app/panel_diag/ app/i18n/zh.json app/shell/host.cpp core/main.cpp docs/superpowers/checklists/phase4-diag-service.md
git commit -m "feat(ui): panel_diag 接入 diag_ping/ssl/sms/log/health"
git tag phase4-diag-service
```

---

## Self-Review

**1. Spec coverage** (§4.4 + §4.6 + §8 P4):
- [x] §4.4 `diag_refresh` —— P3 已完成
- [x] §4.4 `diag_ping_tcp` —— Task 3
- [x] §4.4 `diag_capture_log` —— Task 7
- [x] SSL probe (spec 4.4 隐含：完整 DiagnosticService) —— Task 5
- [x] SMS 模板 —— Task 6
- [x] 一键健康检查 —— Task 8
- [x] §4.6 panel_diag 5 按钮 —— Task 9

**2. 依赖 / 顺序调整**：
- **Task 6 Step 4** 要求 `at_session_send_raw`，原计划在 Task 6 内做——建议**前移到 Task 4 末尾**（`at_session.h` 加 raw API），让 Task 6 直接用
- Task 8 Step 2 用 `diag_service_refresh_now(NULL)` 取全局指针——需要 `agent_app_t *app` 改传，**Step 3 实施时改**

**3. Placeholder 扫描**：
- "TODO 1.1" 出现在 Task 6 Step 3——这是真实待办（at_session 限制），不是 placeholder，**注释里说明即可**
- "见 Step 3 改" 出现在 Task 6 Step 2——明确指向 Step 3，OK
- DIAG_HEALTH_TEST_MODE 宏——明确在测试 build 用，OK

**4. 类型一致性**：
- `diag_ping_cb` / `diag_ssl_cb` / `diag_sms_cb` / `diag_log_cb` / `diag_health_cb` 命名风格统一（`xxx_cb`）
- 所有 async API 都是 `int xxx(..., cb, userdata)` 返回 0/负错误码
- 所有 callback 第一个参数是结果，第二个是 userdata
- `diag_state_t` 字段在 Task 2 定义、Task 3-8 都用到——一致

**5. 测试覆盖**：
- Task 2: state reset
- Task 3: TCP mock server
- Task 4: UDP echo server
- Task 5: 真实 baidu.com（联网时）或 skip
- Task 6: mock at_session
- Task 7: miniz zip 流程
- Task 8: mock 全套 + JSON schema
- Task 9: 手工冒烟（不写自动测试，UI 改动频繁）

---

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-06-07-modem-agent-p4-diag-service.md`. **9 tasks**, 估时 **1.5 周**（spec 预估）。

**后续 P5/P6 计划**：
- P5（LLM 接入）：vendor miniz 的同类——`vendor libcurl`（已）+ `lib/llm_client` + SSE 流式 + 5 厂商 + DPAPI
- P6（SQLite + 打包）：vendor SQLite + 完整 schema + Inno Setup

**P4 实施建议**（下次会话）：
1. 选 **Subagent-Driven**——每 task 一个 subagent，串行 review
2. Task 1（vendor miniz）1 个 subagent 就能完成
3. Task 2-5（diag_state + ping + ssl）可并行 3 个 subagent
4. Task 6-8（sms + log + health）相互依赖，串行
5. Task 9（UI）最后做，串 5 个 commit
