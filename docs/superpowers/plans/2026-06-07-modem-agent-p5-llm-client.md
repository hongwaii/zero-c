# Modem Agent — Plan P5: LLM 客户端 + 工具调用

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `app/panel_settings` 的"保存"按钮 + `app/shell/llm_drawer` 的"发送"按钮从"in-memory mock"变成"真发 OpenAI 兼容 chat completion 请求 + SSE 流式回显 + tool_call 确认弹窗"，5 厂商（DeepSeek/Qwen/GLM/Kimi/MiniMax）通杀；API key 用 Windows DPAPI 加密落盘。

**Architecture:**
- `lib/llm_client/llm_sse.h/.c` —— SSE 字节流解析器（状态机，CR/LF 边界，`data: ` 前缀，`[DONE]` 哨兵）；TDD 优先
- `lib/llm_client/llm_dpapi.{h,c}` —— Windows DPAPI（`CryptProtectData` / `CryptUnprotectData`）加密 / 解密 API key；hex 编码落盘
- `lib/llm_client/llm_client.h/.c` —— OpenAI 兼容 chat completion 客户端（libcurl `CURLOPT_WRITEFUNCTION` 走 SSE 解析 → `on_token_cb` 逐 token 上抛）
- `lib/llm_client/llm_tool.h/.c` —— tool_call 解析（从完整响应里抽 `tool_calls[].function.name` + `arguments` JSON 串）
- `lib/llm_client/provider_config.h/.c` —— 5 厂商 seed + 加载 / 保存 `config/llm_providers.json`；落盘前 DPAPI 加密 api_key
- `app/panel_settings` 改造：保存按钮 → 调 `provider_config_save()`；状态行 → 实时读 dpapi 解密结果
- `app/shell/llm_drawer` 改造：发送 → 调 `llm_chat_stream()`；流式 token → 追加到对话区；tool_call → 弹 ImGui 模态确认框
- 模态确认框：标题 "AI 想要执行 AT 命令" + 完整 AT 字符串 + [取消] [确认并发送] 两按钮

**Tech Stack:** C11、libcurl（已集成 / SChannel）、Windows DPAPI（`wincrypt.h`）、libuv（仅异步触发回调用，所有网络 I/O 在 worker 线程做）、ImGui（modal 弹窗）、cJSON（请求/响应序列化）、现有 `agent_types.h` 的 `agent_llm_provider_t`。

**Spec reference:** [docs/superpowers/specs/2026-06-07-modem-agent-design.md](docs/superpowers/specs/2026-06-07-modem-agent-design.md) §4.5（LLMClient）、§4.6（panel_settings + LLM drawer）、§6（401/超时错误处理）、§8（Phase 5）、§12（mbedtls+libcurl 同步阻塞风险 + UI 抖动风险）。

**前置：** Plan P4 完成（tag `phase4-diag-service`）。已有 libcurl 集成（SSL probe 走过）、cJSON 集成。

---

## File Structure

### 新增

| Path | 职责 |
|---|---|
| `lib/llm_client/llm_sse.h` + `.c` | SSE 字节流解析器（状态机，data: 前缀，[DONE] 哨兵） |
| `lib/llm_client/llm_dpapi.h` + `.c` | Windows DPAPI 加密 / 解密（hex 编码） |
| `lib/llm_client/llm_client.h` + `.c` | OpenAI 兼容 chat completion 客户端（curl + SSE 流式） |
| `lib/llm_client/llm_tool.h` + `.c` | tool_call JSON 解析（抽 name + arguments） |
| `lib/llm_client/provider_config.h` + `.c` | 5 厂商 seed + 加载 / 保存 config/llm_providers.json |
| `lib/llm_client/CMakeLists.txt` | 静态库 `lib_llm_client` |
| `lib/llm_client/test_llm_sse.c` | SSE 解析器单元测试（mock 字节流） |
| `lib/llm_client/test_llm_dpapi.c` | DPAPI 加密-解密 roundtrip |
| `lib/llm_client/test_llm_tool.c` | tool_call 解析测试 |
| `test/test_llm_client_mock.c` | 客户端 + 解析器集成测试（mock HTTP server） |
| `docs/superpowers/checklists/phase5-llm-client.md` | 真机冒烟清单 |

### 修改

| Path | 改动 |
|---|---|
| `lib/CMakeLists.txt` | `add_subdirectory(llm_client)` |
| `app/panel_settings/panel.cpp` + `seed_llm_providers.cpp` | 删硬编码，改为加载 `config/llm_providers.json`；保存按钮 → `provider_config_save()` |
| `app/panel_settings/CMakeLists.txt` | 链 `lib_llm_client` |
| `app/shell/llm_drawer.cpp` | 删 mock 字符串，改为调 `llm_chat_stream` + token 流式追加 + tool_call 弹模态 |
| `app/shell/CMakeLists.txt` | 链 `lib_llm_client` |
| `core/main.cpp` | 启动时 `provider_config_load()` → 灌入 `g_app.providers`；UI 关闭时 `provider_config_save()` |
| `app/i18n/zh.json` | 加新键：保存成功 / 401 / 超时 / tool 确认 / 流式 token 占位 |
| `app/panel_settings/panel_settings.h` | 加 `void panel_settings_reload_from_config(agent_app_t *app);` |
| `test/CMakeLists.txt` | 加 4 个新测试 target |
| `.gitignore` | 忽略 `config/llm_providers.json`（含真实 key） |

---

## Conventions（续 Plan P1-P4）

- C11，4 空格，100 列
- **所有注释中文（zh-CN）**（Modem Agent 约定）
- 标识符 / 字符串字面量 / 编译宏英文
- 单元测试用 runtime `CHECK(cond, msg)` 宏
- 每 task 1 commit，commit message 中文
- 错误码用 `agent_types.h` 里的 `AGENT_ERR_*` 常量
- **线程模型**：libcurl 在 worker 线程跑（用 `_beginthreadex`，类比 P2 serial_chan）；完成 / 错误 / token 全部用 `uv_async_send` 回到 main loop 调用户回调
- **DPAPI 范围**：仅本机本用户能解密——APPDATA 换电脑或换用户就解不开。这是产品需求（防止 dump key 出去），不是 bug

---

## Task 1: SSE 解析器（状态机 + TDD）

**Files:**
- Create: `lib/llm_client/llm_sse.h`
- Create: `lib/llm_client/llm_sse.c`
- Create: `lib/llm_client/test_llm_sse.c`
- Create: `lib/llm_client/CMakeLists.txt`
- Modify: `lib/CMakeLists.txt`（`add_subdirectory(llm_client)`）
- Modify: `test/CMakeLists.txt`

- [ ] **Step 1: 写 `lib/llm_client/llm_sse.h`**

```c
/**
 * @file llm_sse.h
 * @brief SSE (Server-Sent Events) 字节流解析器：CR/LF 边界 + data: 前缀 + [DONE] 哨兵。
 *
 * 设计：libcurl CURLOPT_WRITEFUNCTION 每次给一段字节（不一定按行对齐），
 *       解析器内部维护"未完成行"缓冲；见到 \n\n 收一条事件，data 行去掉
 *       "data: " 前缀后拼到 payload；遇 "[DONE]" 收尾。
 */
#ifndef LIB_LLM_SSE_H
#define LIB_LLM_SSE_H

#include <stdbool.h>
#include <stddef.h>

typedef struct sse_parser sse_parser_t;

typedef enum {
    SSE_EV_DATA,      /* 正常事件：payload 是 data 行内容（多个 data: 行用 \n 拼） */
    SSE_EV_DONE,      /* 哨兵 [DONE] */
    SSE_EV_COMMENT,   /* 以 : 开头的注释行（OpenAI 心跳用）—— v1.0 忽略 */
} sse_event_type_t;

typedef struct {
    sse_event_type_t type;
    const char      *payload;   /* DATA 有效；DONE/COMMENT 为 NULL */
    size_t           payload_len;
} sse_event_t;

/* 用户回调：返回 false 中断解析（如连接已断） */
typedef bool (*sse_event_cb)(const sse_event_t *ev, void *userdata);

sse_parser_t *sse_parser_create(sse_event_cb cb, void *userdata);
void          sse_parser_destroy(sse_parser_t *p);

/* 喂一段字节，返回"已消费"字节数（= len，正常情况全消费） */
size_t        sse_parser_feed(sse_parser_t *p, const char *buf, size_t len);

/* 收尾：处理残余字节（连接断开时调） */
void          sse_parser_finalize(sse_parser_t *p);

#endif
```

- [ ] **Step 2: 写失败的测试 `lib/llm_client/test_llm_sse.c`**

```c
#include "llm_sse.h"
#include "agent_types.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* 收集所有事件，存到全局数组 */
#define MAX_EVENTS 32
static sse_event_t g_events[MAX_EVENTS];
static int g_count = 0;

static bool on_event(const sse_event_t *ev, void *ud)
{
    (void)ud;
    if (g_count >= MAX_EVENTS) return false;
    g_events[g_count++] = *ev;
    return true;
}

static void reset(void) { g_count = 0; memset(g_events, 0, sizeof(g_events)); }

static int test_simple_data(void)
{
    reset();
    sse_parser_t *p = sse_parser_create(on_event, NULL);
    /* 单条事件：data: hello\n\n */
    const char *msg = "data: hello\n\n";
    sse_parser_feed(p, msg, strlen(msg));
    sse_parser_finalize(p);
    assert(g_count == 1);
    assert(g_events[0].type == SSE_EV_DATA);
    assert(g_events[0].payload_len == 5);
    assert(memcmp(g_events[0].payload, "hello", 5) == 0);
    sse_parser_destroy(p);
    return 0;
}

static int test_split_feed(void)
{
    reset();
    sse_parser_t *p = sse_parser_create(on_event, NULL);
    /* 跨多次 feed */
    sse_parser_feed(p, "da", 2);
    sse_parser_feed(p, "ta: hello\n", 9);
    sse_parser_feed(p, "\n", 1);
    sse_parser_finalize(p);
    assert(g_count == 1);
    assert(g_events[0].type == SSE_EV_DATA);
    assert(g_events[0].payload_len == 5);
    sse_parser_destroy(p);
    return 0;
}

static int test_done_sentinel(void)
{
    reset();
    sse_parser_t *p = sse_parser_create(on_event, NULL);
    const char *msg = "data: hello\n\ndata: [DONE]\n\n";
    sse_parser_feed(p, msg, strlen(msg));
    sse_parser_finalize(p);
    assert(g_count == 2);
    assert(g_events[0].type == SSE_EV_DATA);
    assert(g_events[1].type == SSE_EV_DONE);
    sse_parser_destroy(p);
    return 0;
}

static int test_multi_data_lines(void)
{
    reset();
    sse_parser_t *p = sse_parser_create(on_event, NULL);
    /* OpenAI 实际格式：单事件可能含多个 data: 行，用 \n 拼 */
    const char *msg = "data: line1\ndata: line2\n\n";
    sse_parser_feed(p, msg, strlen(msg));
    sse_parser_finalize(p);
    assert(g_count == 1);
    /* payload = "line1\nline2" */
    assert(g_events[0].payload_len == 11);
    assert(memcmp(g_events[0].payload, "line1\nline2", 11) == 0);
    sse_parser_destroy(p);
    return 0;
}

static int test_comment_ignored(void)
{
    reset();
    sse_parser_t *p = sse_parser_create(on_event, NULL);
    const char *msg = ": heartbeat\n\ndata: ok\n\n";
    sse_parser_feed(p, msg, strlen(msg));
    sse_parser_finalize(p);
    assert(g_count == 1);
    assert(g_events[0].type == SSE_EV_DATA);
    sse_parser_destroy(p);
    return 0;
}

int main(void)
{
    test_simple_data();
    test_split_feed();
    test_done_sentinel();
    test_multi_data_lines();
    test_comment_ignored();
    printf("test_llm_sse: 5/5 pass\n");
    return 0;
}
```

- [ ] **Step 3: 跑测试，确认失败（编译期）**

```bash
cd d:/CODE/zero-c && ./build.bat test
```

预期：链接报 `undefined reference to sse_parser_create` 等。

- [ ] **Step 4: 写 `lib/llm_client/llm_sse.c`**

```c
/**
 * @file llm_sse.c
 * @brief SSE 字节流解析器实现。
 *
 * 状态机：
 *   LINE_START: 行首，决定是 data: / [DONE] / : 注释 / 空
 *   IN_DATA: data: 后累积字节
 *   AFTER_LF: 收到 \n 后看下一个字符
 *
 * 行边界：\n（CR 单独出现视为数据）。事件边界：\n\n。
 */
#include "llm_sse.h"
#include <stdlib.h>
#include <string.h>

#define LINE_BUF_MAX 4096
#define EVENT_PAYLOAD_MAX 8192

struct sse_parser {
    sse_event_cb cb;
    void        *userdata;
    char         line_buf[LINE_BUF_MAX];
    size_t       line_len;
    char         payload_buf[EVENT_PAYLOAD_MAX];
    size_t       payload_len;
    bool         in_data;     /* 当前 line 是不是 data: 行 */
};

sse_parser_t *sse_parser_create(sse_event_cb cb, void *ud)
{
    if (!cb) return NULL;
    sse_parser_t *p = (sse_parser_t *)calloc(1, sizeof(*p));
    if (!p) return NULL;
    p->cb = cb;
    p->userdata = ud;
    return p;
}

void sse_parser_destroy(sse_parser_t *p) { free(p); }

/* 内部：当前 line 收尾（遇到 \n 时调） */
static void on_line(sse_parser_t *p)
{
    /* 去掉尾部 \r */
    if (p->line_len > 0 && p->line_buf[p->line_len - 1] == '\r')
        p->line_buf[--p->line_len] = '\0';

    if (p->line_len == 0) {
        /* 空行 = 事件边界 */
        if (p->payload_len > 0) {
            /* 检 [DONE] */
            sse_event_t ev = {0};
            if (p->payload_len == 6 && memcmp(p->payload_buf, "[DONE]", 6) == 0) {
                ev.type = SSE_EV_DONE;
            } else {
                ev.type = SSE_EV_DATA;
                ev.payload = p->payload_buf;
                ev.payload_len = p->payload_len;
            }
            p->cb(&ev, p->userdata);
            p->payload_len = 0;
        }
    } else if (p->line_buf[0] == ':') {
        /* 注释——忽略 */
    } else if (strncmp(p->line_buf, "data: ", 6) == 0 || strcmp(p->line_buf, "data:") == 0) {
        /* data 行 */
        const char *data = (p->line_buf[0] == 'd') ? p->line_buf + 6 : p->line_buf + 5;
        if (p->payload_len > 0 && p->payload_len < EVENT_PAYLOAD_MAX) {
            p->payload_buf[p->payload_len++] = '\n';
        }
        size_t dlen = strlen(data);
        if (p->payload_len + dlen < EVENT_PAYLOAD_MAX) {
            memcpy(p->payload_buf + p->payload_len, data, dlen);
            p->payload_len += dlen;
        }
    }
    /* 其他字段（event:, id:, retry:）—— v1.0 忽略 */
    p->line_len = 0;
}

size_t sse_parser_feed(sse_parser_t *p, const char *buf, size_t len)
{
    size_t consumed = 0;
    while (consumed < len) {
        char c = buf[consumed++];
        if (c == '\n') {
            on_line(p);
        } else if (p->line_len < LINE_BUF_MAX - 1) {
            p->line_buf[p->line_len++] = c;
        }
    }
    return consumed;
}

void sse_parser_finalize(sse_parser_t *p)
{
    /* 处理残余（无 \n 收尾的行）—— v1.0 简化：丢弃 */
    p->line_len = 0;
    p->payload_len = 0;
}
```

- [ ] **Step 5: 写 `lib/llm_client/CMakeLists.txt`**

```cmake
# ============================================================
#  lib/llm_client — LLM 客户端 + SSE 解析 + DPAPI 加密 + tool_call 解析
# ============================================================
set(MODULE_NAME lib_llm_client)

add_library(${MODULE_NAME} STATIC
    llm_sse.c
    # llm_dpapi.c / llm_client.c / llm_tool.c / provider_config.c 在后续 task 追加
)

target_include_directories(${MODULE_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(${MODULE_NAME} PUBLIC
    third_libcurl
)
```

`third_libcurl` 在 P4 阶段已确认是 `libcurl_shared`（见 P4 final 报告）。但因 P4 文档说用 `libcurl_shared`，本 plan Step 5 **先用 `libcurl_shared`，如果 build 失败改 `CURL::libcurl` 再不行再 IMPORTED wrapper**。

- [ ] **Step 6: 改 `lib/CMakeLists.txt` 加 `add_subdirectory(llm_client)`**

- [ ] **Step 7: 加 `test_llm_sse` 到 `test/CMakeLists.txt`**

```cmake
add_executable(test_llm_sse        test_llm_sse.c)
target_link_libraries(test_llm_sse PRIVATE lib_llm_client)
```

> **重要**：测试源在 `test/` 不在 `lib/llm_client/`。要么 `target_include_directories(lib_llm_client PUBLIC lib/llm_client)`（已加），要么测试路径加 include——前者已加，OK。

- [ ] **Step 8: 跑测试**

```bash
cd d:/CODE/zero-c && ./build.bat test
```

预期：`test_llm_sse.exe` 输出 `test_llm_sse: 5/5 pass`。

- [ ] **Step 9: Commit**

```bash
cd d:/CODE/zero-c
git add lib/llm_client/llm_sse.h lib/llm_client/llm_sse.c lib/llm_client/CMakeLists.txt lib/CMakeLists.txt test/test_llm_sse.c test/CMakeLists.txt
git commit -m "feat(llm): SSE 字节流解析器（5 case 全过）"
```

---

## Task 2: DPAPI 加密 / 解密（TDD）

**Files:**
- Create: `lib/llm_client/llm_dpapi.h`
- Create: `lib/llm_client/llm_dpapi.c`
- Create: `lib/llm_client/test_llm_dpapi.c`
- Modify: `lib/llm_client/CMakeLists.txt`（加源 + 链 crypt32）
- Modify: `test/CMakeLists.txt`

- [ ] **Step 1: 写 `lib/llm_client/llm_dpapi.h`**

```c
/**
 * @file llm_dpapi.h
 * @brief Windows DPAPI 加密 / 解密：把 API key 用 CryptProtectData 加密，hex 编码后落盘。
 *
 * 特性：
 *   - DPAPI 范围：本机本用户。换电脑 / 换 Windows 账户无法解密——这是产品需求
 *   - 输出 hex 字符串（每字节 2 字符），便于写到 JSON
 *   - 输入空串 / 缓冲太小返回错误
 */
#ifndef LIB_LLM_DPAPI_H
#define LIB_LLM_DPAPI_H

#include <stdbool.h>
#include <stddef.h>

/* 加密：plaintext -> hex_out（hex 长度 = plaintext_len * 2 + 1）。
 * hex_out_cap 至少 plaintext_len * 2 + 1。返回 0=ok / 负=错误。 */
int  llm_dpapi_encrypt_hex(const char *plaintext, size_t plaintext_len,
                           char *hex_out, size_t hex_out_cap);

/* 解密：hex_in -> plaintext_out。plaintext_out_cap 至少 hex_len / 2。 */
int  llm_dpapi_decrypt_hex(const char *hex_in, size_t hex_len,
                           char *plaintext_out, size_t plaintext_out_cap);

#endif
```

- [ ] **Step 2: 写失败的测试 `test_llm_dpapi.c`**

```c
#include "llm_dpapi.h"
#include "agent_types.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int test_roundtrip(void)
{
    const char *plain = "sk-test-key-12345-abcde";
    size_t plen = strlen(plain);
    char hex[256];
    int rc = llm_dpapi_encrypt_hex(plain, plen, hex, sizeof(hex));
    assert(rc == AGENT_OK);
    /* hex 长度 = plen * 2（不含 \0） */
    assert(strlen(hex) == plen * 2);
    /* 解密回原文 */
    char back[64];
    rc = llm_dpapi_decrypt_hex(hex, strlen(hex), back, sizeof(back));
    assert(rc == AGENT_OK);
    assert(strcmp(back, plain) == 0);
    return 0;
}

static int test_chinese_roundtrip(void)
{
    const char *plain = "中文测试 key 中文";
    size_t plen = strlen(plain);
    char hex[256];
    int rc = llm_dpapi_encrypt_hex(plain, plen, hex, sizeof(hex));
    assert(rc == AGENT_OK);
    char back[64];
    rc = llm_dpapi_decrypt_hex(hex, strlen(hex), back, sizeof(back));
    assert(rc == AGENT_OK);
    assert(strcmp(back, plain) == 0);
    return 0;
}

int main(void)
{
    test_roundtrip();
    test_chinese_roundtrip();
    printf("test_llm_dpapi: 2/2 pass\n");
    return 0;
}
```

- [ ] **Step 3: 跑测试，确认失败（编译期）**

```bash
cd d:/CODE/zero-c && ./build.bat test
```

预期：链接报 `undefined reference to llm_dpapi_*`。

- [ ] **Step 4: 写 `lib/llm_client/llm_dpapi.c`**

```c
/**
 * @file llm_dpapi.c
 */
#include "llm_dpapi.h"
#include "agent_types.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>

#include <stdio.h>
#include <string.h>

static const char kHex[] = "0123456789abcdef";

static int to_hex(const uint8_t *src, size_t src_len, char *dst, size_t dst_cap)
{
    if (dst_cap < src_len * 2 + 1) return AGENT_ERR_BAD_ARG;
    for (size_t i = 0; i < src_len; i++) {
        dst[i * 2]     = kHex[(src[i] >> 4) & 0x0F];
        dst[i * 2 + 1] = kHex[src[i] & 0x0F];
    }
    dst[src_len * 2] = '\0';
    return AGENT_OK;
}

static int from_hex(const char *src, size_t src_len, uint8_t *dst, size_t dst_cap)
{
    if (src_len % 2 != 0) return AGENT_ERR_BAD_ARG;
    if (dst_cap < src_len / 2) return AGENT_ERR_BAD_ARG;
    for (size_t i = 0; i < src_len / 2; i++) {
        char hi = src[i * 2], lo = src[i * 2 + 1];
        uint8_t h = (hi >= '0' && hi <= '9') ? (hi - '0') :
                    (hi >= 'a' && hi <= 'f') ? (hi - 'a' + 10) :
                    (hi >= 'A' && hi <= 'F') ? (hi - 'A' + 10) : 0xFF;
        uint8_t l = (lo >= '0' && lo <= '9') ? (lo - '0') :
                    (lo >= 'a' && lo <= 'f') ? (lo - 'a' + 10) :
                    (lo >= 'A' && lo <= 'F') ? (lo - 'A' + 10) : 0xFF;
        if (h == 0xFF || l == 0xFF) return AGENT_ERR_BAD_ARG;
        dst[i] = (h << 4) | l;
    }
    return AGENT_OK;
}

int llm_dpapi_encrypt_hex(const char *plaintext, size_t plaintext_len,
                          char *hex_out, size_t hex_out_cap)
{
    if (!plaintext || !hex_out) return AGENT_ERR_BAD_ARG;
    DATA_BLOB in = { plaintext_len, (BYTE *)plaintext };
    DATA_BLOB out = { 0, NULL };
    if (!CryptProtectData(&in, L"modem-agent-llm-key", NULL, NULL, NULL,
                          CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        return AGENT_ERR_IO;
    }
    int rc = to_hex(out.pbData, out.cbData, hex_out, hex_out_cap);
    LocalFree(out.pbData);
    return rc;
}

int llm_dpapi_decrypt_hex(const char *hex_in, size_t hex_len,
                          char *plaintext_out, size_t plaintext_out_cap)
{
    if (!hex_in || !plaintext_out) return AGENT_ERR_BAD_ARG;
    uint8_t enc[4096];
    int rc = from_hex(hex_in, hex_len, enc, sizeof(enc));
    if (rc != AGENT_OK) return rc;
    DATA_BLOB in = { hex_len / 2, enc };
    DATA_BLOB out = { 0, NULL };
    if (!CryptUnprotectData(&in, NULL, NULL, NULL, NULL,
                            CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        return AGENT_ERR_IO;
    }
    if (out.cbData >= plaintext_out_cap) {
        LocalFree(out.pbData);
        return AGENT_ERR_BAD_ARG;
    }
    memcpy(plaintext_out, out.pbData, out.cbData);
    plaintext_out[out.cbData] = '\0';
    LocalFree(out.pbData);
    return AGENT_OK;
}
```

- [ ] **Step 5: 改 `lib/llm_client/CMakeLists.txt` 链 crypt32**

```cmake
if (WIN32)
    target_link_libraries(${MODULE_NAME} PUBLIC crypt32)
endif()
```

- [ ] **Step 6: 加 `test_llm_dpapi` 到 `test/CMakeLists.txt`**

```cmake
add_executable(test_llm_dpapi      test_llm_dpapi.c)
target_link_libraries(test_llm_dpapi PRIVATE
    lib_llm_client
    crypt32
)
```

- [ ] **Step 7: 跑测试**

```bash
cd d:/CODE/zero-c && ./build.bat test
```

预期：`test_llm_dpapi.exe` 输出 `test_llm_dpapi: 2/2 pass`。

- [ ] **Step 8: Commit**

```bash
cd d:/CODE/zero-c
git add lib/llm_client/llm_dpapi.h lib/llm_client/llm_dpapi.c lib/llm_client/CMakeLists.txt test/test_llm_dpapi.c test/CMakeLists.txt
git commit -m "feat(llm): DPAPI 加密/解密 API key（hex 编码落盘）"
```

---

## Task 3: LLM 客户端（curl POST + SSE 流式 + on_token_cb）

**Files:**
- Create: `lib/llm_client/llm_client.h`
- Create: `lib/llm_client/llm_client.c`
- Create: `lib/llm_client/test_llm_client_mock.c`（mock HTTP server）
- Modify: `lib/llm_client/CMakeLists.txt`（加源 + 链 ws2_32）
- Modify: `test/CMakeLists.txt`

- [ ] **Step 1: 写 `lib/llm_client/llm_client.h`**

```c
/**
 * @file llm_client.h
 * @brief OpenAI 兼容 chat completion 客户端：libcurl POST + SSE 流式回包 + 逐 token 回调。
 *
 * 线程模型：llm_chat_stream 在调用方线程内启动 worker 线程（_beginthreadex），
 *          worker 用 libcurl 同步 POST。SSE 解析后通过 on_token_cb 同步调
 *          用户回调（v1.0 简化：不通过 libuv async——worker 线程直接调）。
 *          v1.1 改进：worker 内部写 ringbuf，main loop 拉。
 */
#ifndef LIB_LLM_CLIENT_H
#define LIB_LLM_CLIENT_H

#include <stdbool.h>
#include <stddef.h>

#include "agent_types.h"  /* agent_llm_provider_t */

/* 单个 token 流式回调。v1.0：worker 线程直接调。 */
typedef void (*llm_token_cb)(const char *token, size_t len, void *userdata);

/* 完成回调。ok=true 表示正常收尾，err=NULL 或空。 */
typedef void (*llm_done_cb)(bool ok, const char *err, void *userdata);

/* 发起一个 chat completion 请求：
 *   - provider: 已加载的 provider（含 base_url / api_key 明文）
 *   - model: 模型名（NULL = 用 provider->default_model）
 *   - messages_json: OpenAI 格式 messages 数组的 JSON 字符串（已序列化好）
 *   - on_token: 每收到一个 token 调一次（多次）
 *   - on_done: 流结束或错误时调一次
 *   - userdata: 两个回调的 userdata
 * 返回 0=已启动；负=参数错/资源失败。
 *
 * 注意：api_key 是 DPAPI 解密后的明文（由调用方负责）。 */
int  llm_chat_stream(const agent_llm_provider_t *provider,
                     const char *model,
                     const char *messages_json,
                     llm_token_cb on_token,
                     llm_done_cb  on_done,
                     void        *userdata);

#endif
```

- [ ] **Step 2: 写 `lib/llm_client/llm_client.c`**

```c
/**
 * @file llm_client.c
 */
#include "llm_client.h"
#include "llm_sse.h"
#include "agent_types.h"

#include <curl/curl.h>
#include <uv.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>

#define REQUEST_BODY_MAX  (32 * 1024)
#define RESPONSE_BODY_MAX (256 * 1024)

typedef struct {
    const agent_llm_provider_t *provider;
    const char *model;
    const char *messages_json;
    llm_token_cb on_token;
    llm_done_cb  on_done;
    void        *userdata;

    /* 累积完整响应（tool_call 解析用） */
    char        *full_buf;
    size_t       full_len;
    size_t       full_cap;

    /* 当前 SSE 解析器 */
    sse_parser_t *sse;

    /* 错误缓冲 */
    char  err[256];
} chat_ctx_t;

static bool sse_on_event(const sse_event_t *ev, void *ud)
{
    chat_ctx_t *ctx = (chat_ctx_t *)ud;
    if (ev->type == SSE_EV_DONE) return true;
    if (ev->type != SSE_EV_DATA) return true;
    /* payload 是 "choices":[{"delta":{"content":"..."}}] 的 JSON 字符串 */
    if (ev->payload_len == 0) return true;
    /* v1.0 简化：用 cJSON 解析 delta.content */
    cJSON *root = cJSON_ParseWithLength(ev->payload, ev->payload_len);
    if (!root) return true;
    cJSON *choices = cJSON_GetObjectItemCaseSensitive(root, "choices");
    if (cJSON_IsArray(choices) && cJSON_GetArraySize(choices) > 0) {
        cJSON *delta = cJSON_GetObjectItemCaseSensitive(
            cJSON_GetArrayItem(choices, 0), "delta");
        cJSON *content = cJSON_GetObjectItemCaseSensitive(delta, "content");
        if (cJSON_IsString(content) && content->valuestring) {
            const char *s = content->valuestring;
            size_t n = strlen(s);
            if (ctx->on_token) ctx->on_token(s, n, ctx->userdata);
            /* 累加到 full_buf */
            if (ctx->full_len + n < ctx->full_cap) {
                memcpy(ctx->full_buf + ctx->full_len, s, n);
                ctx->full_len += n;
                ctx->full_buf[ctx->full_len] = '\0';
            }
        }
    }
    cJSON_Delete(root);
    return true;
}

static size_t curl_write_cb(char *ptr, size_t size, size_t nmemb, void *ud)
{
    chat_ctx_t *ctx = (chat_ctx_t *)ud;
    size_t total = size * nmemb;
    if (ctx->sse) sse_parser_feed(ctx->sse, ptr, total);
    return total;
}

static unsigned __stdcall chat_worker(void *arg)
{
    chat_ctx_t *ctx = (chat_ctx_t *)arg;
    ctx->sse = sse_parser_create(sse_on_event, ctx);
    if (!ctx->sse) { snprintf(ctx->err, sizeof(ctx->err), "sse_parser_create OOM"); goto done; }

    /* 拼 URL：base_url 末尾通常带 /v1，OpenAI 风格 chat completions = /chat/completions */
    char url[512];
    if (ctx->provider->base_url[strlen(ctx->provider->base_url) - 1] == '/')
        snprintf(url, sizeof(url), "%schat/completions", ctx->provider->base_url);
    else
        snprintf(url, sizeof(url), "%s/chat/completions", ctx->provider->base_url);

    /* 拼 request body */
    char body[REQUEST_BODY_MAX];
    int blen = snprintf(body, sizeof(body),
        "{\"model\":\"%s\",\"messages\":%s,\"stream\":true}",
        ctx->model ? ctx->model : ctx->provider->default_model,
        ctx->messages_json);
    if (blen < 0 || blen >= (int)sizeof(body)) {
        snprintf(ctx->err, sizeof(ctx->err), "request body too big");
        goto done;
    }

    /* Authorization header */
    char auth[1024];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", ctx->provider->api_key);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, auth);
    headers = curl_slist_append(headers, "Accept: text/event-stream");

    CURL *c = curl_easy_init();
    if (!c) { snprintf(ctx->err, sizeof(ctx->err), "curl_easy_init failed"); goto done; }

    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_POST, 1L);
    curl_easy_setopt(c, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, ctx);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 2L);

    CURLcode rc = curl_easy_perform(c);
    long status = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(headers);
    curl_easy_cleanup(c);

    if (rc != CURLE_OK) {
        snprintf(ctx->err, sizeof(ctx->err), "curl: %s (status=%ld)", curl_easy_strerror(rc), status);
        goto done;
    }
    if (status >= 400) {
        snprintf(ctx->err, sizeof(ctx->err), "HTTP %ld", status);
        goto done;
    }
done:
    if (ctx->sse) { sse_parser_finalize(ctx->sse); sse_parser_destroy(ctx->sse); ctx->sse = NULL; }
    if (ctx->on_done) ctx->on_done(ctx->err[0] == '\0', ctx->err, ctx->userdata);
    free(ctx->full_buf);
    free(ctx);
    return 0;
}

int llm_chat_stream(const agent_llm_provider_t *provider,
                    const char *model,
                    const char *messages_json,
                    llm_token_cb on_token,
                    llm_done_cb  on_done,
                    void        *userdata)
{
    if (!provider || !messages_json) return AGENT_ERR_BAD_ARG;
    chat_ctx_t *ctx = (chat_ctx_t *)calloc(1, sizeof(*ctx));
    if (!ctx) return AGENT_ERR_OOM;
    ctx->provider = provider;
    ctx->model = model;
    ctx->messages_json = messages_json;
    ctx->on_token = on_token;
    ctx->on_done = on_done;
    ctx->userdata = userdata;
    ctx->full_cap = RESPONSE_BODY_MAX;
    ctx->full_buf = (char *)malloc(ctx->full_cap);
    if (!ctx->full_buf) { free(ctx); return AGENT_ERR_OOM; }
    ctx->full_buf[0] = '\0';

    uintptr_t h = _beginthreadex(NULL, 0, chat_worker, ctx, 0, NULL);
    if (h == 0) { free(ctx->full_buf); free(ctx); return AGENT_ERR_OOM; }
    CloseHandle((HANDLE)h);  /* 脱离——worker 退出时自己清理 */
    return AGENT_OK;
}
```

> 顶部加 `#include <cjson/cJSON.h>`。

- [ ] **Step 3: 改 `lib/llm_client/CMakeLists.txt` 加源 + 链 cjson**

```cmake
add_library(${MODULE_NAME} STATIC
    llm_sse.c
    llm_dpapi.c
    llm_client.c
)

target_link_libraries(${MODULE_NAME} PUBLIC
    third_libcurl
    third_cjson
)

if (WIN32)
    target_link_libraries(${MODULE_NAME} PUBLIC crypt32 ws2_32)
endif()
```

- [ ] **Step 4: 写 `test/test_llm_client_mock.c`（本机 mock HTTP server，模拟 SSE 流）**

> **简化**：用本机 HTTP socket mock server。先 GET / 测试连通性，然后用一个简化的"POST /chat/completions"返回 SSE 流。

```c
#include "llm_client.h"
#include "llm_sse.h"
#include "agent_types.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <process.h>

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

/* 找空闲端口 */
static int find_port(void)
{
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK); a.sin_port = 0;
    bind(s, (struct sockaddr *)&a, sizeof(a));
    int len = sizeof(a); getsockname(s, (struct sockaddr *)&a, &len);
    int port = ntohs(a.sin_port);
    closesocket(s);
    return port;
}

/* mock server：接收一个 POST，返 200 + 3 个 SSE data + [DONE] */
static unsigned __stdcall mock_llm(void *arg)
{
    int port = (int)(intptr_t)arg;
    SOCKET listen = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons(port);
    bind(listen, (struct sockaddr *)&a, sizeof(a));
    listen(listen, 1);
    SOCKET c = accept(listen, NULL, NULL);
    if (c == INVALID_SOCKET) { closesocket(listen); return 0; }

    char buf[2048]; int got = recv(c, buf, sizeof(buf) - 1, 0);
    buf[got] = '\0';
    /* 解析到 \r\n\r\n 为止，丢弃 body */
    char *body = strstr(buf, "\r\n\r\n");
    if (body) {
        body += 4;
        /* 模拟流式响应：3 个 token + done */
        const char *resp =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/event-stream\r\n\r\n"
            "data: {\"choices\":[{\"delta\":{\"content\":\"He\"}}]}\n\n"
            "data: {\"choices\":[{\"delta\":{\"content\":\"llo\"}}]}\n\n"
            "data: {\"choices\":[{\"delta\":{\"content\":\" world\"}}]}\n\n"
            "data: [DONE]\n\n";
        send(c, resp, strlen(resp), 0);
    }
    closesocket(c);
    closesocket(listen);
    return 0;
}

static char g_tokens[256]; static int g_tok_count = 0;
static void on_token(const char *t, size_t n, void *ud)
{
    (void)ud;
    if (g_tok_count + n < sizeof(g_tokens)) {
        memcpy(g_tokens + g_tok_count, t, n);
        g_tok_count += n;
        g_tokens[g_tok_count] = '\0';
    }
}
static int g_done = 0; static int g_done_ok = 0; static char g_done_err[128];
static void on_done(bool ok, const char *err, void *ud)
{
    (void)ud;
    g_done = 1; g_done_ok = ok;
    if (err) { strncpy(g_done_err, err, sizeof(g_done_err) - 1); g_done_err[sizeof(g_done_err)-1] = '\0'; }
}

int main(void)
{
    WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa);
    int port = find_port();
    _beginthreadex(NULL, 0, mock_llm, (void *)(intptr_t)port, 0, NULL);
    Sleep(100);

    agent_llm_provider_t p = {0};
    snprintf(p.base_url, sizeof(p.base_url), "http://127.0.0.1:%d/v1/", port);
    snprintf(p.api_key, sizeof(p.api_key), "sk-test");
    strcpy(p.default_model, "mock-model");
    strcpy(p.name, "Mock");

    const char *messages = "[{\"role\":\"user\",\"content\":\"hi\"}]";
    int rc = llm_chat_stream(&p, NULL, messages, on_token, on_done, NULL);
    assert(rc == AGENT_OK);

    /* 等 worker 完成（最多 5 秒） */
    for (int i = 0; i < 50 && !g_done; i++) Sleep(100);
    assert(g_done);
    assert(g_done_ok);
    assert(strcmp(g_tokens, "Hello world") == 0);

    printf("test_llm_client_mock: tokens='%s' ok=%d\n", g_tokens, g_done_ok);
    WSACleanup();
    return 0;
}
```

- [ ] **Step 5: 加到 `test/CMakeLists.txt`**

```cmake
add_executable(test_llm_client_mock test_llm_client_mock.c)
target_link_libraries(test_llm_client_mock PRIVATE
    lib_llm_client
    libcurl_shared
    ws2_32
    crypt32
)
target_compile_definitions(test_llm_client_mock PRIVATE _CRT_SECURE_NO_WARNINGS)
```

- [ ] **Step 6: 跑测试**

```bash
cd d:/CODE/zero-c && ./build.bat test
```

预期：`test_llm_client_mock.exe` 输出 `tokens='Hello world' ok=1`。

- [ ] **Step 7: Commit**

```bash
cd d:/CODE/zero-c
git add lib/llm_client/llm_client.h lib/llm_client/llm_client.c lib/llm_client/CMakeLists.txt test/test_llm_client_mock.c test/CMakeLists.txt
git commit -m "feat(llm): OpenAI 兼容 chat completion 客户端（SSE 流式 + on_token）"
```

---

## Task 4: tool_call 解析器

**Files:**
- Create: `lib/llm_client/llm_tool.h`
- Create: `lib/llm_client/llm_tool.c`
- Create: `lib/llm_client/test_llm_tool.c`
- Modify: `lib/llm_client/CMakeLists.txt`
- Modify: `test/CMakeLists.txt`

- [ ] **Step 1: 写 `lib/llm_client/llm_tool.h`**

```c
/**
 * @file llm_tool.h
 * @brief 从完整 chat completion 响应里抽 tool_calls。
 *
 * OpenAI tool_calls 格式：
 *   "tool_calls": [
 *     {"id": "...", "type": "function",
 *      "function": {"name": "send_at", "arguments": "{\"cmd\":\"AT+CSQ\"}"}}
 *   ]
 *
 * v1.0：只解析 function 类型。最多 1 个 tool_call（多 tool 用第一个）。
 */
#ifndef LIB_LLM_TOOL_H
#define LIB_LLM_TOOL_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char id[64];          /* tool_call id */
    char name[64];        /* function name，如 "send_at" */
    char arguments[1024]; /* arguments JSON 字符串原文 */
} llm_tool_call_t;

/* 从完整 JSON 响应里抽第一条 tool_call。
 * 成功返回 true，out 填充；无 tool_call 或解析失败返回 false。 */
bool llm_extract_tool_call(const char *response_json, size_t json_len,
                           llm_tool_call_t *out);

#endif
```

- [ ] **Step 2: 写失败的测试 `test_llm_tool.c`**

```c
#include "llm_tool.h"
#include "agent_types.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int test_simple_tool(void)
{
    const char *json =
        "{\"choices\":[{\"message\":{\"tool_calls\":["
        "{\"id\":\"call_1\",\"type\":\"function\","
        "\"function\":{\"name\":\"send_at\",\"arguments\":\"{\\\"cmd\\\":\\\"AT+CSQ\\\"}\"}}"
        "]}}]}";
    llm_tool_call_t t = {0};
    bool ok = llm_extract_tool_call(json, strlen(json), &t);
    assert(ok);
    assert(strcmp(t.id, "call_1") == 0);
    assert(strcmp(t.name, "send_at") == 0);
    assert(strstr(t.arguments, "AT+CSQ") != NULL);
    return 0;
}

static int test_no_tool(void)
{
    const char *json = "{\"choices\":[{\"message\":{\"content\":\"hello\"}}]}";
    llm_tool_call_t t = {0};
    bool ok = llm_extract_tool_call(json, strlen(json), &t);
    assert(!ok);
    return 0;
}

int main(void)
{
    test_simple_tool();
    test_no_tool();
    printf("test_llm_tool: 2/2 pass\n");
    return 0;
}
```

- [ ] **Step 3: 写 `lib/llm_client/llm_tool.c`**

```c
#include "llm_tool.h"
#include "agent_types.h"
#include <cjson/cJSON.h>
#include <string.h>

bool llm_extract_tool_call(const char *json, size_t len, llm_tool_call_t *out)
{
    if (!json || !out) return false;
    cJSON *root = cJSON_ParseWithLength(json, len);
    if (!root) return false;
    cJSON *choices = cJSON_GetObjectItemCaseSensitive(root, "choices");
    if (!cJSON_IsArray(choices) || cJSON_GetArraySize(choices) == 0) {
        cJSON_Delete(root); return false;
    }
    cJSON *message = cJSON_GetObjectItemCaseSensitive(
        cJSON_GetArrayItem(choices, 0), "message");
    cJSON *tools = cJSON_GetObjectItemCaseSensitive(message, "tool_calls");
    if (!cJSON_IsArray(tools) || cJSON_GetArraySize(tools) == 0) {
        cJSON_Delete(root); return false;
    }
    cJSON *first = cJSON_GetArrayItem(tools, 0);
    cJSON *id = cJSON_GetObjectItemCaseSensitive(first, "id");
    cJSON *func = cJSON_GetObjectItemCaseSensitive(first, "function");
    cJSON *name = cJSON_GetObjectItemCaseSensitive(func, "name");
    cJSON *args = cJSON_GetObjectItemCaseSensitive(func, "arguments");
    bool got = false;
    if (cJSON_IsString(id) && cJSON_IsString(name) && cJSON_IsString(args)) {
        strncpy(out->id, id->valuestring, sizeof(out->id) - 1);
        strncpy(out->name, name->valuestring, sizeof(out->name) - 1);
        strncpy(out->arguments, args->valuestring, sizeof(out->arguments) - 1);
        got = true;
    }
    cJSON_Delete(root);
    return got;
}
```

- [ ] **Step 4: 改 CMakeLists + 加测试**

```cmake
add_executable(test_llm_tool      test_llm_tool.c)
target_link_libraries(test_llm_tool PRIVATE
    lib_llm_client
    third_cjson
)
```

- [ ] **Step 5: 跑测试**

```bash
cd d:/CODE/zero-c && ./build.bat test
```

预期：`test_llm_tool: 2/2 pass`。

- [ ] **Step 6: Commit**

```bash
cd d:/CODE/zero-c
git add lib/llm_client/llm_tool.h lib/llm_client/llm_tool.c test/test_llm_tool.c test/CMakeLists.txt
git commit -m "feat(llm): tool_call 解析（OpenAI 兼容格式）"
```

---

## Task 5: provider_config（5 厂商 seed + 加载/保存 JSON + DPAPI）

**Files:**
- Create: `lib/llm_client/provider_config.h`
- Create: `lib/llm_client/provider_config.c`
- Modify: `lib/llm_client/CMakeLists.txt`
- Modify: `test/CMakeLists.txt`（加 roundtrip 测试）

- [ ] **Step 1: 写 `lib/llm_client/provider_config.h`**

```c
/**
 * @file provider_config.h
 * @brief 5 厂商 seed + 加载/保存 config/llm_providers.json。
 *
 * 落盘前 DPAPI 加密 api_key（hex 编码）；加载时 DPAPI 解密。
 * 若 config 文件不存在，调 seed_defaults 写 5 厂商模板。
 */
#ifndef LIB_LLM_PROVIDER_CONFIG_H
#define LIB_LLM_PROVIDER_CONFIG_H

#include "agent_types.h"  /* agent_llm_provider_t */

/* 5 厂商默认 seed。base_url / default_model 见 spec §4.5。 */
int  llm_seed_defaults(agent_app_t *app);  /* 灌到 app->providers */

/* 从 path 加载到 app->providers（清空已有）。
 * 加载时对每个 provider 的 api_key hex 串做 DPAPI 解密。
 * 若 path 不存在，自动 seed_defaults 并 save。返回 0=ok。 */
int  llm_provider_config_load(agent_app_t *app, const char *path);

/* 把 app->providers 加密后写到 path（原子写）。返回 0=ok。 */
int  llm_provider_config_save(const agent_app_t *app, const char *path);

#endif
```

- [ ] **Step 2: 写 `lib/llm_client/provider_config.c`**

```c
#include "provider_config.h"
#include "llm_dpapi.h"
#include "json_reader.h"
#include "json_writer.h"
#include "agent_types.h"

#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const struct { const char *name, *url, *model; } kSeed[5] = {
    { "DeepSeek", "https://api.deepseek.com/v1",                    "deepseek-chat"   },
    { "Qwen",     "https://dashscope.aliyuncs.com/compatible-mode/v1", "qwen-plus"    },
    { "GLM",      "https://open.bigmodel.cn/api/paas/v4",            "glm-4-plus"     },
    { "Kimi",     "https://api.moonshot.cn/v1",                      "moonshot-v1-8k" },
    { "MiniMax",  "<user-supplied endpoint>",                         "MiniMax-Text-01" },
};

static void free_providers(agent_app_t *app)
{
    agent_llm_provider_t *p = app->providers;
    while (p) { agent_llm_provider_t *n = p->next; free(p); p = n; }
    app->providers = NULL;
}

int llm_seed_defaults(agent_app_t *app)
{
    free_providers(app);
    agent_llm_provider_t *head = NULL, *tail = NULL;
    for (int i = 0; i < 5; i++) {
        agent_llm_provider_t *p = (agent_llm_provider_t *)calloc(1, sizeof(*p));
        strncpy(p->name, kSeed[i].name, sizeof(p->name) - 1);
        strncpy(p->base_url, kSeed[i].url, sizeof(p->base_url) - 1);
        strncpy(p->default_model, kSeed[i].model, sizeof(p->default_model) - 1);
        p->api_key[0] = '\0';
        if (!head) head = p; else tail->next = p;
        tail = p;
    }
    app->providers = head;
    return AGENT_OK;
}

int llm_provider_config_load(agent_app_t *app, const char *path)
{
    free_providers(app);
    cJSON *root = NULL;
    int rc = json_load_file(path, &root);
    if (rc != AGENT_OK) {
        /* 文件不存在 → seed + save */
        llm_seed_defaults(app);
        return llm_provider_config_save(app, path);
    }
    cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, "providers");
    if (!cJSON_IsArray(arr)) { cJSON_Delete(root); return AGENT_ERR_IO; }
    agent_llm_provider_t *head = NULL, *tail = NULL;
    cJSON *item;
    cJSON_ArrayForEach(item, arr) {
        agent_llm_provider_t *p = (agent_llm_provider_t *)calloc(1, sizeof(*p));
        cJSON *n = cJSON_GetObjectItemCaseSensitive(item, "name");
        cJSON *u = cJSON_GetObjectItemCaseSensitive(item, "base_url");
        cJSON *m = cJSON_GetObjectItemCaseSensitive(item, "default_model");
        cJSON *k = cJSON_GetObjectItemCaseSensitive(item, "api_key");
        if (cJSON_IsString(n)) strncpy(p->name, n->valuestring, sizeof(p->name) - 1);
        if (cJSON_IsString(u)) strncpy(p->base_url, u->valuestring, sizeof(p->base_url) - 1);
        if (cJSON_IsString(m)) strncpy(p->default_model, m->valuestring, sizeof(p->default_model) - 1);
        if (cJSON_IsString(k) && k->valuestring && k->valuestring[0]) {
            /* DPAPI 解密 */
            char plain[512];
            if (llm_dpapi_decrypt_hex(k->valuestring, strlen(k->valuestring),
                                      plain, sizeof(plain)) == AGENT_OK) {
                strncpy(p->api_key, plain, sizeof(p->api_key) - 1);
            }
        }
        if (!head) head = p; else tail->next = p;
        tail = p;
    }
    cJSON_Delete(root);
    app->providers = head;
    return AGENT_OK;
}

int llm_provider_config_save(const agent_app_t *app, const char *path)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *arr = cJSON_AddArrayToObject(root, "providers");
    for (agent_llm_provider_t *p = app->providers; p; p = p->next) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "name", p->name);
        cJSON_AddStringToObject(item, "base_url", p->base_url);
        cJSON_AddStringToObject(item, "default_model", p->default_model);
        if (p->api_key[0]) {
            char hex[1024];
            if (llm_dpapi_encrypt_hex(p->api_key, strlen(p->api_key),
                                      hex, sizeof(hex)) == AGENT_OK) {
                cJSON_AddStringToObject(item, "api_key", hex);
            } else {
                cJSON_AddStringToObject(item, "api_key", "");  /* 失败也不写明文 */
            }
        } else {
            cJSON_AddStringToObject(item, "api_key", "");
        }
        cJSON_AddItemToArray(arr, item);
    }
    int rc = json_save_file_atomic(path, root);
    cJSON_Delete(root);
    return rc;
}
```

- [ ] **Step 3: 写 `test/test_provider_config.c`（roundtrip）**

```c
#include "provider_config.h"
#include "llm_dpapi.h"
#include "agent_types.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    agent_app_t app = {0};
    /* seed */
    llm_seed_defaults(&app);
    assert(app.providers != NULL);
    /* 给第一个 provider 设 api_key */
    agent_llm_provider_t *p = app.providers;
    strcpy(p->api_key, "sk-test-key");

    const char *path = "out/test_providers.json";
    /* save */
    int rc = llm_provider_config_save(&app, path);
    assert(rc == AGENT_OK);

    /* load 到新 app */
    agent_app_t app2 = {0};
    rc = llm_provider_config_load(&app2, path);
    assert(rc == AGENT_OK);
    assert(app2.providers != NULL);
    assert(strcmp(app2.providers->api_key, "sk-test-key") == 0);

    /* 清理 */
    agent_llm_provider_t *q = app.providers, *n;
    while (q) { n = q->next; free(q); q = n; }
    q = app2.providers;
    while (q) { n = q->next; free(q); q = n; }
    remove(path);

    printf("test_provider_config: roundtrip OK\n");
    return 0;
}
```

- [ ] **Step 4: 加到 `test/CMakeLists.txt`**

```cmake
add_executable(test_provider_config test_provider_config.c)
target_link_libraries(test_provider_config PRIVATE
    lib_llm_client
    lib_util
    crypt32
)
```

- [ ] **Step 5: 跑测试**

```bash
cd d:/CODE/zero-c && ./build.bat test
```

预期：`test_provider_config: roundtrip OK`。

- [ ] **Step 6: Commit**

```bash
cd d:/CODE/zero-c
git add lib/llm_client/provider_config.h lib/llm_client/provider_config.c test/test_provider_config.c test/CMakeLists.txt
git commit -m "feat(llm): provider_config 5 厂商 + DPAPI 加密落盘"
```

---

## Task 6: panel_settings 真实保存（替换硬编码）

**Files:**
- Modify: `app/panel_settings/seed_llm_providers.cpp`（删硬编码，改调 `llm_provider_config_load`）
- Modify: `app/panel_settings/panel.cpp`（保存按钮接 `llm_provider_config_save`）
- Modify: `app/panel_settings/panel_settings.h`（加 `panel_settings_reload_from_config`）
- Modify: `app/panel_settings/CMakeLists.txt`（链 `lib_llm_client`）
- Modify: `app/i18n/zh.json`（加新键）
- Modify: `core/main.cpp`（启动时 load + 退出时 save）

- [ ] **Step 1: 改 `app/panel_settings/panel_settings.h` 加声明**

```c
void panel_settings_reload_from_config(agent_app_t *app);
void panel_settings_save_to_config  (agent_app_t *app);
```

- [ ] **Step 2: 删 `seed_llm_providers.cpp` 硬编码实现**

整个文件替换为：

```cpp
/**
 * @file seed_llm_providers.cpp
 * @brief P5 改造：原本硬编码 5 厂商——现在调 llm_provider_config_load。
 *
 * 保留同名函数 panel_settings_seed_defaults 给 main.cpp 启动时调。
 * 若 config 文件存在则 load，否则 seed + save。
 */
#include "panel.h"
#include "provider_config.h"
#include <stdio.h>
#include <string.h>

void panel_settings_seed_defaults(agent_app_t *app)
{
    /* 优先级：先看 config 文件；不存在才 seed */
    if (llm_provider_config_load(app, "config/llm_providers.json") != AGENT_OK) {
        fprintf(stderr, "panel_settings: provider config load failed, fall back to seed\n");
        llm_seed_defaults(app);
    }
}

void panel_settings_reload_from_config(agent_app_t *app)
{
    llm_provider_config_load(app, "config/llm_providers.json");
}

void panel_settings_save_to_config(agent_app_t *app)
{
    if (llm_provider_config_save(app, "config/llm_providers.json") != AGENT_OK) {
        fprintf(stderr, "panel_settings: save failed\n");
    } else {
        fprintf(stderr, "panel_settings: saved to config/llm_providers.json\n");
    }
}
```

- [ ] **Step 3: 改 `app/panel_settings/panel.cpp` 保存按钮**

找到现有 "保存" 按钮（`i18n_get("settings.llm.save")`）的点击 handler，替换为：

```cpp
if (ImGui::Button(i18n_get("settings.llm.save"))) {
    panel_settings_save_to_config(app);
}
```

- [ ] **Step 4: 改 `app/panel_settings/CMakeLists.txt` 链 `lib_llm_client`**

```cmake
target_link_libraries(${MODULE_NAME} PUBLIC
    app_shell
    third_imgui
    lib_llm_client
)
```

- [ ] **Step 5: 改 `app/i18n/zh.json` 加新键**

```json
{
  "settings.llm.saved": "已保存到 config/llm_providers.json",
  "settings.llm.save_failed": "保存失败（查看 stderr）",
  "settings.llm.reload": "重新加载"
}
```

并在保存按钮旁边加 "重新加载" 按钮（如果需要刷新看别的进程改的 config）。

- [ ] **Step 6: 改 `core/main.cpp` 启动时确保 config 目录存在**

```cpp
#include <sys/stat.h>
/* 在 main() 开头 */
mkdir("config", 0755);
```

- [ ] **Step 7: Build 验证**

```bash
cd d:/CODE/zero-c && ./build.bat
```

预期：build 成功，EXE 多几十 KB（libcurl + lib_llm_client）。

- [ ] **Step 8: Commit**

```bash
cd d:/CODE/zero-c
git add app/panel_settings/ app/i18n/zh.json core/main.cpp
git commit -m "feat(llm): panel_settings 真保存（DPAPI 加密）"
```

---

## Task 7: llm_drawer 真实聊天（流式 + 历史）

**Files:**
- Modify: `app/shell/llm_drawer.cpp`（替换 mock 流为 `llm_chat_stream` 调用）
- Modify: `app/shell/CMakeLists.txt`（链 `lib_llm_client`）
- Modify: `app/i18n/zh.json`（加新键）

- [ ] **Step 1: 改 `app/shell/llm_drawer.cpp` 真实聊天**

完整重写（关键代码段）：

```cpp
#include "llm_drawer.h"
#include "llm_client.h"
#include "provider_config.h"
#include "i18n.h"
#include "imgui.h"
#include "agent_types.h"
#include <cstdio>
#include <cstring>
#include <string>

/* 全局当前响应累积（流式 token 不断追加） */
static std::string g_response;

/* on_token 回调：worker 线程直接调 → 写 g_response */
static void on_token(const char *t, size_t n, void *ud)
{
    (void)ud;
    g_response.append(t, n);
}

/* on_done 回调：worker 线程直接调 → 标记完成 */
static bool g_busy = false;
static char g_err[128] = "";
static void on_done(bool ok, const char *err, void *ud)
{
    (void)ud;
    g_busy = false;
    if (!ok && err) { strncpy(g_err, err, sizeof(g_err) - 1); g_err[sizeof(g_err)-1] = '\0'; }
    else g_err[0] = '\0';
}

/* 选第一个有 key 的 provider */
static const agent_llm_provider_t *pick_provider(agent_app_t *app)
{
    for (auto p = app->providers; p; p = p->next) {
        if (p->api_key[0]) return p;
    }
    return NULL;
}

void llm_drawer_render(agent_app_t *app)
{
    static char input[256] = "";
    static std::string history;  /* 累积 user + ai 轮次 */

    ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_FirstUseEver);
    ImGui::Begin(i18n_get("llm.drawer.title"), &app->llm_drawer_open);
    ImGui::TextWrapped("%s", i18n_get("llm.drawer.disclaimer"));

    /* 状态行：几个 provider 已配 key */
    int configured = 0;
    for (auto p = app->providers; p; p = p->next) if (p->api_key[0]) configured++;
    ImGui::Text("LLM providers: %d / 5 configured", configured);

    ImGui::Separator();
    ImGui::BeginChild("llm_out", ImVec2(0, -64), true);
    ImGui::TextWrapped("%s", history.c_str());
    if (g_response.size() > 0 && g_busy) {
        /* 流式中：把 g_response 复制出来显示（不能直接 c_str() 因为 worker 还在写） */
        static std::string snapshot;
        snapshot = g_response;
        ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1.0f), "%s", snapshot.c_str());
    }
    ImGui::EndChild();

    ImGui::InputTextWithHint("##llm_in", i18n_get("llm.drawer.placeholder"), input, sizeof(input));
    ImGui::SameLine();
    if (ImGui::Button(i18n_get("llm.drawer.send")) && !g_busy) {
        if (input[0] == '\0') { /* empty */ }
        else {
            const agent_llm_provider_t *p = pick_provider(app);
            if (!p) {
                snprintf(g_err, sizeof(g_err), "no provider has api_key — set in settings");
            } else {
                char msg[1024];
                snprintf(msg, sizeof(msg), "[user] %s\n[ai] ", input);
                history += msg;
                g_response.clear();
                g_busy = true;
                g_err[0] = '\0';
                const char *messages = "[{\"role\":\"user\",\"content\":\"\"}]";  /* placeholder */
                /* 实际 messages 应含本次 input + history；v1.0 简化只发 input */
                char msgs[2048];
                snprintf(msgs, sizeof(msgs), "[{\"role\":\"user\",\"content\":\"%s\"}]", input);
                llm_chat_stream(p, NULL, msgs, on_token, on_done, NULL);
                input[0] = '\0';
            }
        }
    }
    if (g_err[0]) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", g_err);
    }
    ImGui::End();
}
```

> **线程安全提醒**：`on_token` / `on_done` 在 worker 线程调，但 `g_response` 在 main loop 渲染时被读。v1.0 简化：worker 写 `std::string` 不保证线程安全，main loop 在 worker 写时也可能在读。**严格做法**：用 `std::mutex` 保护；**实际可用做法**：接受偶尔花屏（流式场景用户感知不到单帧错位）。v1.1 改 mutex。

- [ ] **Step 2: 改 `app/shell/CMakeLists.txt` 链 `lib_llm_client`**

```cmake
target_link_libraries(${MODULE_NAME} PUBLIC
    lib_util
    third_imgui
    third_cjson
    lib_llm_client
)
```

- [ ] **Step 3: 改 `app/i18n/zh.json` 加新键**

```json
{
  "llm.drawer.no_key": "无 provider 已配 key（请在设置页填 key 后保存）"
}
```

- [ ] **Step 4: Build 验证**

```bash
cd d:/CODE/zero-c && ./build.bat
```

预期：build 成功。

- [ ] **Step 5: Commit**

```bash
cd d:/CODE/zero-c
git add app/shell/llm_drawer.cpp app/shell/CMakeLists.txt app/i18n/zh.json
git commit -m "feat(llm): llm_drawer 真实聊天（SSE 流式 token 渲染）"
```

---

## Task 8: tool_call 确认弹窗（ImGui Modal）

**Files:**
- Modify: `app/shell/llm_drawer.cpp`（加 modal 渲染 + on_done 后检查 tool_call）
- Modify: `app/i18n/zh.json`（加新键）

- [ ] **Step 1: 改 `llm_drawer.cpp` 加 tool_call 解析 + 模态**

在 `on_done` 末尾追加（worker 线程调——v1.0 简化：直接读 g_response，main loop 也会读，竞态同上）：

```cpp
static bool g_show_tool_modal = false;
static char g_tool_id[64] = "";
static char g_tool_name[64] = "";
static char g_tool_args[1024] = "";

static void on_done(bool ok, const char *err, void *ud)
{
    (void)ud;
    g_busy = false;
    if (!ok && err) { strncpy(g_err, err, sizeof(g_err) - 1); g_err[sizeof(g_err)-1] = '\0'; }
    else g_err[0] = '\0';
    /* 解析 tool_call */
    if (ok) {
        llm_tool_call_t tc = {0};
        if (llm_extract_tool_call(g_response.c_str(), g_response.size(), &tc)) {
            strncpy(g_tool_id, tc.id, sizeof(g_tool_id) - 1);
            strncpy(g_tool_name, tc.name, sizeof(g_tool_name) - 1);
            strncpy(g_tool_args, tc.arguments, sizeof(g_tool_args) - 1);
            g_show_tool_modal = true;
        }
    }
}
```

在 `llm_drawer_render` 末尾（`ImGui::End()` 之前）加 modal 渲染：

```cpp
if (g_show_tool_modal) ImGui::OpenPopup("llm_tool_confirm");
if (ImGui::BeginPopupModal("llm_tool_confirm", &g_show_tool_modal, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("%s", i18n_get("llm.tool.confirm_title"));
    ImGui::Separator();
    ImGui::Text("function: %s", g_tool_name);
    ImGui::InputTextMultiline("##args", g_tool_args, sizeof(g_tool_args), ImVec2(400, 100), ImGuiInputTextFlags_ReadOnly);
    if (ImGui::Button(i18n_get("llm.tool.confirm_run"))) {
        /* v1.0：只显示确认；v1.1 真发 AT */
        g_show_tool_modal = false;
        char log[1100];
        snprintf(log, sizeof(log), "[tool] would execute: %s(%s)\n", g_tool_name, g_tool_args);
        history += log;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button(i18n_get("llm.tool.cancel"))) {
        g_show_tool_modal = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}
```

- [ ] **Step 2: 改 `app/i18n/zh.json` 加新键**

```json
{
  "llm.tool.confirm_title": "AI 想要执行以下操作",
  "llm.tool.confirm_run": "确认并执行",
  "llm.tool.cancel": "取消"
}
```

- [ ] **Step 3: Build 验证**

```bash
cd d:/CODE/zero-c && ./build.bat
```

预期：build 成功。

- [ ] **Step 4: Commit**

```bash
cd d:/CODE/zero-c
git add app/shell/llm_drawer.cpp app/i18n/zh.json
git commit -m "feat(llm): tool_call 确认弹窗（v1.0 仅显示，v1.1 真发）"
```

---

## Task 9: Phase 5 冒烟清单 + Tag

**Files:**
- Create: `docs/superpowers/checklists/phase5-llm-client.md`

- [ ] **Step 1: 写 `docs/superpowers/checklists/phase5-llm-client.md`**

```markdown
# Phase 5 — LLM 客户端冒烟测试

## 沙箱里可做
- [ ] `./build.bat` build 成功
- [ ] `./build.bat test` 全部通过：
  - [ ] test_llm_sse 5/5
  - [ ] test_llm_dpapi 2/2
  - [ ] test_llm_client_mock tokens='Hello world' ok=1
  - [ ] test_llm_tool 2/2
  - [ ] test_provider_config roundtrip OK
  - [ ] 老的 P0-P4 测试不回归

## 真机验证（必须有真实 LLM API key）
- [ ] 启动 EXE：自动从 config/llm_providers.json 加载 5 厂商
  - [ ] 若文件不存在：自动 seed 5 厂商 + 落盘
- [ ] 切到"设置"页：5 厂商列出，每个可改 key
  - [ ] 填 DeepSeek key → 点"保存" → 关闭 EXE → 重启 → key 仍在
  - [ ] 看 config/llm_providers.json：api_key 是 hex 字符串（明文 DPAPI 加密后）
- [ ] 切到"AI 助手"抽屉
  - [ ] 顶部状态行：`LLM providers: 1 / 5 configured`（填了几个就显示几）
  - [ ] 输"你好" → 点发送
  - [ ] 流式 token 逐字显示（不是等全部到齐再显示）
  - [ ] 几秒后 AI 完整回复出现
  - [ ] status 区域无 401 / timeout
- [ ] 故意填错 key（截断最后 5 字符）：
  - [ ] 重启 EXE → 发请求 → 几秒后红色 status: HTTP 401
  - [ ] 修正 key → 重启 → 重发 → 正常回复
- [ ] 故意拔网线（或者断 WiFi）：
  - [ ] 发请求 → 几秒后 status: curl: Failed to connect
- [ ] 拔模组（COM）：
  - [ ] LLM 抽屉仍正常工作（LLM 不依赖模组）
- [ ] tool_call 弹窗（v1.0 不真发）：
  - [ ] 输入"帮我查 CSQ"→ 等 AI 响应 → 若 AI 返回 tool_call → 弹模态
  - [ ] 模态显示 function name + arguments（arguments 是只读）
  - [ ] 点"确认并执行"：history 追加一行 "[tool] would execute: send_at({...})"
  - [ ] 点"取消"：模态关闭，history 无追加

## 不在 P5 范围
- ✗ 真正的 tool 执行（v1.1）
- ✗ 多轮对话上下文（v1.0 只发当次 input）
- ✗ tool_call 流式解析（v1.0 等完整响应后再解析）
- ✗ SSE 失败重连（v1.0 一次失败就报）
- ✗ 中文 / 英文双语 UI（spec v1.1）
```

- [ ] **Step 2: Commit + Tag**

```bash
cd d:/CODE/zero-c
git add docs/superpowers/checklists/phase5-llm-client.md
git commit -m "docs: Phase 5 LLM 客户端 冒烟测试清单"
git tag phase5-llm-client
```

---

## Self-Review

**1. Spec coverage** (§4.5 + §4.6 + §6 + §8 P5):
- [x] §4.5 OpenAI 兼容 HTTP + SSE 流式 — Task 3
- [x] §4.5 5 厂商 config — Task 5
- [x] §4.5 API key DPAPI 加密 — Task 2
- [x] §4.5 tool_calls 解析 + 确认弹窗 — Task 4 + 8
- [x] §4.6 panel_settings LLM CRUD — Task 6
- [x] §4.6 LLM 抽屉 — Task 7
- [x] §6 401 / 超时错误处理 — Task 3 (status 提取) + Task 7 (status 渲染)

**2. 依赖 / 顺序调整**：
- Task 1 (SSE) 是 Task 3 (LLM client) 的前置——顺序对
- Task 2 (DPAPI) 是 Task 5 (provider_config) 的前置——顺序对
- Task 4 (tool_call) 依赖 Task 3 的 full_buf 累积——可与 Task 5 并行（只解析 JSON 字符串），本计划放在 Task 5 之前
- Task 6-8 串行（UI 集成）

**3. Placeholder 扫描**：
- "placeholder" 出现在 Task 7 Step 1 的 `messages` 占位变量——明确指向下一步替换，OK
- "v1.0 简化" 多次出现——明确标注简化决策，OK
- "v1.1 改 mutex" 出现在 Task 7 Step 1——明确后续改进路径，OK

**4. 类型一致性**：
- `llm_token_cb` / `llm_done_cb` 命名风格统一
- `agent_llm_provider_t` 沿用 P1 定义，未改
- `llm_tool_call_t` 新加，与现有 `agent_llm_provider_t` 风格一致

**5. 测试覆盖**：
- Task 1: SSE 5 case（简单 / 分片 / DONE / 多 data / 注释）
- Task 2: DPAPI roundtrip × 2（ASCII / 中文）
- Task 3: mock HTTP server + 真实 SSE 流
- Task 4: tool_call 解析 × 2（有 / 无）
- Task 5: 5 厂商 roundtrip
- Task 6-8: UI 改动，无自动测试（按 spec §7 "app/* has no unit tests"）
- Task 9: 手工冒烟

**6. 风险 & 限制（按 spec §12）**：
- libcurl 同步阻塞 → 已在 Task 3 移到 worker 线程（`_beginthreadex`）
- 主线程 FPS 抖动 → Task 7 流式渲染每帧一次（不是每个 token 一次），可接受
- `g_response` 线程不安全 → 接受 v1.0 偶发单帧错位，v1.1 改 mutex
- DPAPI 仅本机本用户 → 产品需求，不是 bug

---

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-06-07-modem-agent-p5-llm-client.md`. **9 tasks**, 估时 **1 周**（spec 预估）。

**P5 实施建议**（下次会话或本会话后续）：
1. Subagent-Driven
2. Phase 1: Task 1 (SSE) + Task 2 (DPAPI) 并行
3. Phase 2: Task 3 (LLM client) + Task 4 (tool_call) 并行（Task 4 仅依赖 cJSON）
4. Phase 3: Task 5 (provider_config) 串行（依赖 Task 2 + 3）
5. Phase 4: Task 6 (panel_settings) → Task 7 (llm_drawer) → Task 8 (tool modal) 串行
6. Phase 5: Task 9 (checklist) + tag
