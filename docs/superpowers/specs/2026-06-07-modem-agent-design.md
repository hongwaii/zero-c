# Modem Agent — Design Spec

| | |
|---|---|
| **Date** | 2026-06-07 |
| **Status** | Draft (pending user review) |
| **Owner** | huanghongwei |
| **Working dir** | `d:\CODE\zero-c` |
| **Target v1.0 GA** | ~9 weeks from kickoff |

## 1. Background & Goals

### 1.1 Context

`zero-c` is a pure-C Windows development framework built on CMake + LLVM-MinGW. It currently contains:

- a self-contained toolchain (LLVM-MinGW, CMake as zip),
- placeholder `core/main.c` and `midware/http/`,
- integrated `mbedtls-4.1.0` and `libcurl`,
- a `libuv` template (not yet wired in),
- a test runner (`test/test_main.c`) that exercises mbedtls SHA-256 and libcurl HTTPS.

The user is an embedded software engineer at a cellular module vendor, working across NBIoT / LTE / RedCap / NR modules, 3GPP AT command sets, SMS, network registration, TCPIP, SSL, lwIP, and dial-up. The goal is to build a Windows desktop "agent" that unifies field-diagnostic, production-test, OTA, and multi-module management workflows for these modules, and to embed LLM assistance via mainstream Chinese providers (DeepSeek, Qwen, GLM, Kimi, MiniMax).

### 1.2 Goals

A single Windows EXE that:

1. Talks to one or more cellular modules concurrently (COM serial and/or USB-NCM/RNDIS).
2. Provides a clean ImGui-based UI with five major panels: 现场诊断 / 产线测试 / OTA / 多模组 / 设置.
3. Embeds a field-diagnostic toolset in v1.0 (AT console, real-time status, TCP/SSL probe, SMS, modem-log capture).
4. Plugs in any OpenAI-compatible LLM provider via user-configured base URL + API key + model.
5. Persists config in JSON, reports/history in SQLite.
6. Stays pure C throughout, on top of the existing `zero-c` toolchain.

### 1.3 Non-Goals (v1.0)

- Production-test case scheduler (architecture reserved, panel is a placeholder).
- OTA delta upgrade with signature verification (architecture reserved, panel is a placeholder).
- CMUX (TS 27.010) multiplexing.
- Linux/macOS port.
- Code signing of the binary.

## 2. Key Decisions (Locked)

| Dimension | Decision | Rationale |
|---|---|---|
| Form factor | Single Windows 10/11 x64 EXE | Matches `zero-c` toolchain; matches user environment. |
| Language / UI | Pure C + Dear ImGui (DX11 backend) | Keeps single-language codebase; ideal aesthetic for engineering tools. |
| Async model | libuv, single loop, all I/O | Already templated in repo; unifies serial, sockets, timers, file I/O. |
| TLS / HTTP | mbedtls + libcurl (already integrated) | No new dependencies. |
| Module channels | COM serial (`uv_tty_t`) + USB-NCM/RNDIS | Covers 2026 NBIoT/LTE/RedCap/NR field practice. |
| Persistence | JSON config + SQLite history | Standard split, each tool for its strength. |
| LLM | OpenAI-compatible protocol, all 5 Chinese providers | Single client code, providers interchangeable. |
| v1.0 hero | AT engine + 现场诊断 + LLM 助手 | Foundation for the other three directions. |
| Module concurrency | Multi-module architecture from v1.0 (UI may focus on 1 active) | Avoids v1.1 rewrite. |
| Build order | ImGui shell with mock data first, then real backends | Fastest path to "product feel" for iteration. |

## 3. Architecture

### 3.1 Layered model

```
┌────────────────────────────────────────────────────────┐
│  ImGui 表现层 (UI)                                       │
│  - 主窗口 / 主题 / 导航                                    │
│  - 5 大面板: 现场诊断 / 产线测试 / OTA / 多模组 / 设置      │
│  - LLM 对话抽屉（右侧滑出）                                 │
└───────────────▲──────────────┬─────────────────────────┘
                │ 状态查询      │ 用户事件
┌───────────────┴──────────────▼─────────────────────────┐
│  业务/服务层 (Controller + Service)                      │
│  - DeviceManager: N 模组的生命周期                        │
│  - AtEngine: 单模组 AT 会话（命令队列/URC/超时/重试）        │
│  - DiagnosticService / ProductionService / OtaService    │
│  - LlmService: OpenAI 兼容客户端                           │
│  - StorageService: JSON 配置 + SQLite                    │
└───────────────▲──────────────┬─────────────────────────┘
                │ uv_async      │ uv_tcp/uv_pipe/timer
┌───────────────┴──────────────▼─────────────────────────┐
│  libuv 事件循环 (单 loop)                                 │
│  + Windows HAL: COM 串口 / USB-NCM socket / 文件 I/O       │
│  + 工作线程池: SQLite / LLM HTTP / 重活                   │
└────────────────────────────────────────────────────────┘
```

**Boundaries**

- ImGui main thread renders only; all I/O returns to it via `uv_async`.
- Each module = one `modem_dev_t` with its own `at_session_t`, URC parser, log buffer. `DeviceManager` owns N of them.
- The service layer does not know whether a module is on COM or USB-NCM — both go through the unified `modem_chan_t` abstraction.

### 3.2 Directory layout (extends existing `zero-c`)

```
zero-c/
├── core/                       # main.c：启动 ImGui + libuv loop
├── include/                    # 公共头（version.h, agent_types.h...）
├── lib/                        # 内部 C 库 (无 UI 依赖, 可单元测试)
│   ├── at_engine/              # AT 解析器、URC 路由、状态机
│   ├── device_manager/         # 多模组 session 池
│   ├── llm_client/             # OpenAI 兼容 HTTP 客户端
│   ├── storage/                # JSON 配置 + SQLite 包装
│   ├── log/                    # 滚动日志、AP_LOG 宏
│   └── util/                   # strbuf / ringbuf / utf8 / time
├── midware/
│   ├── http/                   # libcurl 包装（已有）
│   ├── serial/                 # uv_tty 串口通道 + AT 帧检测
│   ├── usb_ncm/                # RNDIS/NCM 网卡发现与探活
│   └── imgui_backend/          # DX11/Win32 平台层、字体、主题
├── app/                        # 5 大面板的 UI 代码
│   ├── shell/                  # 主窗口 + 主题 + 左侧导航
│   ├── panel_diag/             # 现场诊断 (v1.0 主菜)
│   ├── panel_prod/             # 产线测试 (v1.0 占位)
│   ├── panel_ota/              # OTA (v1.0 占位)
│   ├── panel_devices/          # 多模组列表 (v1.0 半成品)
│   └── panel_settings/         # 设置（API key、串口、主题）
├── third_party/                # mbedtls/curl/libuv/imgui/sqlite/cJSON
└── test/                       # 单元 + 集成
```

`core/` holds only `main()`. All business lives in `lib/` (UI-free, unit-testable) or `app/` (ImGui-bound). This lets `build.bat test` exercise business logic without spinning up a window.

## 4. Core Modules

### 4.1 Module Channel HAL

Public header `include/agent_chan.h`:

```c
typedef struct modem_chan modem_chan_t;
typedef struct {
    int  (*open) (modem_chan_t *self, const char *uri);
    int  (*send) (modem_chan_t *self, const uint8_t *buf, size_t len);
    void (*close)(modem_chan_t *self);
} modem_chan_ops_t;
```

- `midware/serial/`: `uv_tty_t` for COM. URI: `com://COM5?baud=115200&parity=N`.
- `midware/usb_ncm/`: IP Helper enumerates RNDIS / CDC-ECM / NCM NICs (matched by description string), uses `uv_udp_t` + ICMP for liveness. URI: `rndis://{FriendlyName}`. Module also monitors link up/down and IP readiness.
- Both share a **ring buffer** + **AT frame detector** (CR/LF delimiter, `+++` escape, `--EOF--` prompt). AT semantic parsing is not done at this layer.

### 4.2 AT Engine (`lib/at_engine`)

```c
typedef enum { AT_S_IDLE, AT_S_WAIT_REPLY, AT_S_CMUX, AT_S_ERROR } at_state_t;

typedef struct {
    at_state_t     state;
    uv_timer_t     cmd_timer;
    ringbuf_t      rx_ring;
    strbuf_t       cur_line;
    list_t         cmd_q;
    list_head_t    urc_handlers;
    at_parser_t    parser;
} at_session_t;
```

- **FIFO command queue** — never concurrent; preserves request/response pairing.
- **URC (Unsolicited Result Code) channel** — `+CMTI: ...` `+CEREG: 5` etc. dispatched to registered handlers, never pollutes pending-command replies.
- **Error recovery** — 3 consecutive `ERROR` resets session to IDLE and fires `on_session_reset`; the upper layer decides whether to reopen.
- **CMUX** is v2.0 scope.

### 4.3 Device Manager (`lib/device_manager`)

```c
typedef struct modem_dev {
    char            id[64];          // "MDM-1736-0001"
    char            label[64];
    char            chan_uri[256];   // com://COM5 / rndis://xxx
    at_session_t   *at;
    diag_state_t    diag;
    sqlite_id_t     db_id;
    dev_state_t     state;           // DISCONNECTED / CONNECTING / READY / ERROR
} modem_dev_t;
```

- Backing array, v1.0 capacity = 16.
- Hot-plug detection: `uv_timer_t` scans COM ports + RNDIS NICs every 2 s, diffs, and `uv_async` notifies UI.
- Each device holds an independent AT session — a second module never disturbs the first.
- Persistence: device list + label + chan_uri in JSON; test records and historical logs in SQLite.

### 4.4 Services

| Service | v1.0 status | Public API sketch |
|---|---|---|
| `DiagnosticService` | **Complete** | `diag_refresh(dev_id)` → CSQ/CEREG/COPS/CIMI/IMEI/ICCID; `diag_ping_tcp(dev_id, host, port)`; `diag_capture_log(dev_id, sec, out_path)` |
| `ProductionService` | **Stub** | Interface + mock; case scheduler + report export deferred to v1.1 |
| `OtaService` | **Stub** | Interface + mock; delta upgrade + signature deferred to v1.2 |
| `LlmService` | **Complete** | `llm_chat(messages[], on_token_cb)` OpenAI-compatible with SSE streaming; function-calling plumbing reserved |

### 4.5 LLM Client (`lib/llm_client`)

- HTTP via libcurl + mbedtls (already integrated), streaming via `CURLOPT_WRITEFUNCTION`.
- Provider config in `config/llm_providers.json`:

```json
{
  "providers": [
    { "name": "DeepSeek", "base_url": "https://api.deepseek.com/v1", "api_key": "<DPAPI>", "default_model": "deepseek-chat" },
    { "name": "Qwen",     "base_url": "https://dashscope.aliyuncs.com/compatible-mode/v1", "api_key": "<DPAPI>", "default_model": "qwen-plus" },
    { "name": "GLM",      "base_url": "https://open.bigmodel.cn/api/paas/v4", "api_key": "<DPAPI>", "default_model": "glm-4-plus" },
    { "name": "Kimi",     "base_url": "https://api.moonshot.cn/v1", "api_key": "<DPAPI>", "default_model": "moonshot-v1-8k" },
    { "name": "MiniMax",  "base_url": "<user-supplied endpoint>", "api_key": "<DPAPI>", "default_model": "MiniMax-Text-01" }
  ]
}
```

- API keys are encrypted via Windows DPAPI; never written to disk in plaintext.
- v1.0 must at minimum parse the `tool_calls` field and surface "AI wants to run `AT+...` → confirm" prompt; real tool execution is v1.1.

### 4.6 ImGui App (`app/`)

- `app/shell/`: main window = `DockBuilder` two-column; 200px left nav with 5 buttons + "🤖 AI 助手" button (right-side LLM drawer).
- `panel_diag`: top half "AT console" (input + history), bottom half "real-time status cards" (CSQ / cell / operator / RAT / IMEI / IMSI / ICCID), footer actions (dial, hangup, capture log 30 s, send SMS template).
- `panel_devices`: list of all detected modules (name, COM, IP, signal, state, last seen). Double-click to make a device the active one.
- `panel_prod` / `panel_ota`: v1.0 shows "coming in v1.x" + roadmap mock.
- `panel_settings`: LLM providers, serial profiles, theme, log path, version.
- Theme: custom "engineering blue" (dark + high contrast). UI strings live in `i18n/zh.json` for future English.

## 5. Threading & Data Flow

### 5.1 Threads

```
main thread ─ ImGui @ 60 FPS, calls uv_run(UV_RUN_NOWAIT) per frame
libuv loop  ─ same thread as main; owns uv_tty / uv_udp / uv_timer / uv_async
SQLite      ─ dedicated worker thread (via uv_queue_work or own queue)
curl HTTP   ─ dedicated worker thread (LLM and outbound HTTP)
heavy work  ─ worker thread pool (OTA diff, log zip, etc.)
log flush   ─ dedicated pipe to a file writer thread
```

**Invariants**

- The main thread never calls blocking I/O (no `sqlite3_open`, no synchronous curl, no file read).
- Main → worker: `uv_queue_work` or `uv_async` + self-managed queue.
- Worker → main: `uv_async_send` with a small payload (< 64 B ID + handle); large payloads use a shared pointer + ref-count.
- libuv in Windows covers COM/UDP/timer/pipe natively; we do not write raw IOCP.

### 5.2 UI event flow — "click Refresh CSQ"

```
button click
  └─ panel_diag::on_refresh_click()
       └─ diag_service_refresh(dev_id)            // returns synchronously
            └─ uv_queue_work → at_session_send("AT+CSQ")
                 └─ parser sees "+CSQ: 23,99" + URC "+CEREG: 5"
                      └─ uv_async → main thread
                           └─ diag_state updated → panel_diag redraws next frame
```

UI never blocks. Services **push** updates; UI subscribes (`diag_subscribe(dev_id, &on_state, this)`) and unsubscribes on panel teardown.

### 5.3 SQLite schema (`data/agent.db`, WAL mode)

```sql
CREATE TABLE device (
    id            TEXT PRIMARY KEY,
    label         TEXT,
    chan_uri      TEXT,
    module_model  TEXT,
    first_seen    INTEGER,
    last_seen     INTEGER
);

CREATE TABLE diag_snapshot (
    device_id     TEXT,
    ts            INTEGER,
    csq           INTEGER,
    rsrp          INTEGER,
    rsrq          INTEGER,
    snr           REAL,
    operator      TEXT,
    rat           TEXT,            -- NB-IoT / LTE Cat-1 / RedCap / NR
    cereg         INTEGER,
    ps_attached   INTEGER,
    PRIMARY KEY (device_id, ts),
    FOREIGN KEY (device_id) REFERENCES device(id)
);
CREATE INDEX idx_diag_ts ON diag_snapshot(ts DESC);

CREATE TABLE at_log (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id     TEXT,
    ts            INTEGER,
    dir           TEXT,            -- "TX" / "RX" / "URC"
    raw           TEXT
);
CREATE INDEX idx_atlog_dev_ts ON at_log(device_id, ts DESC);

CREATE TABLE llm_chat (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    ts            INTEGER,
    provider      TEXT,
    model         TEXT,
    role          TEXT,            -- user / assistant / tool
    content       TEXT,
    tool_calls    TEXT             -- JSON
);

-- Reserved for v1.1 / v1.2; declared now to avoid schema migration later.
-- CREATE TABLE test_run ...
-- CREATE TABLE ota_job ...
```

### 5.4 Config files

```
config/
├── app.json              # theme / language / startup
├── devices.json          # module list + label
├── llm_providers.json    # LLM providers (keys DPAPI-encrypted)
├── serial_profiles.json  # baud-rate / flow-control presets
└── diag_presets.json     # user-defined diagnostic sequences
```

Reads: at startup and on `uv_async` notification of external change. Writes: atomic (write `.tmp` + rename).

## 6. Error Handling

| Layer | Error | Handling |
|---|---|---|
| HAL | COM unplugged / NIC down | Mark `chan_error`, session → `ERROR`. **No auto-reconnect** — UI surfaces a "Reconnect" button (avoids misjudgment on production lines). |
| AT engine | 3 consecutive ERROR | Session reset; fire `on_session_reset`; UI marks yellow "reset". |
| LLM | 401 / network timeout | Toast "key invalid" or "retry"; chat history preserved, user can resend. |
| SQLite | lock / disk full | Worker retries 3× → degrade to file log → toast "DB unavailable, UI in read-only mode". |
| ImGui | DX11 device lost | `ImGui_ImplDX11_InvalidateDeviceObjects` + rebuild; one-line notice. |

**Production-line mode toggle (future consideration):** in this mode, COM unplug events auto-reconnect. v1.0 may opt to ship without it; revisit if the user requests.

## 7. Testing Strategy

- `lib/*` C libraries: unit-tested via `build.bat test` using CMocka or pure assert (zero external deps).
  - AT parser: 3GPP canonical cases + URC edges (e.g. `+CEREG: 5,"002F","001A6708",7`).
  - LlmClient: local mock HTTP server in test harness.
  - Storage: temporary DB.
- `midware/*` integration: simulated serial port (com0com or libuv `uv_pipe_t` pseudo-COM).
- `app/*` has no unit tests; UI changes are frequent. A small set of golden screenshots provides a smoke check.
- Coverage gate: v1.0 core libs (`at_engine`, `device_manager`, `llm_client`) `>= 70%` line coverage.

## 8. Implementation Phases

> Each phase ends with a runnable EXE; backends progress from mock → real.

| Phase | Weeks | Deliverable | Acceptance |
|---|---|---|---|
| **0 — Baseline** | 0.5 | Wire ImGui (docking) + DX11 + libuv into build; verify "blank window opens and closes" | `zero-c.exe` launches an empty ImGui window |
| **1 — App shell** ★ | 1.5 | Main window + docks + left nav; all 5 panels as **empty skeletons with mock data**; theme "engineering blue"; CN font subset; LLM drawer (mock streaming); settings page writes `app.json` | All 5 panels + LLM drawer + theme switch + settings persist work **without a module plugged in** |
| **2 — HAL + libuv loop** | 1.5 | `midware/serial` (uv_tty), `midware/usb_ncm` (IP Helper), unified `modem_chan_t`, hot-plug scan | Plug a COM module → device list auto-updates (link layer only, no AT) |
| **3 — AT engine + DeviceManager** ★ v1.0 core | 2 | `lib/at_engine`, `lib/device_manager`, real CSQ/CEREG/COPS/CIMI/IMEI/ICCID; AT console persists to SQLite | Two modules plugged in run independently |
| **4 — Diag service + log capture** | 1.5 | TCP/UDP ping over NCM, SSL probe (https://www.baidu.com + cert check), SMS template, modem log dump + zip | "Health check" wizard produces a complete report |
| **5 — LLM real + tool plumbing** | 1 | `lib/llm_client` OpenAI-compatible + SSE; verify all 5 providers; "AI asks to run AT…" confirm dialog | "Show me the IMEI" works through Qwen |
| **6 — Persistence + reports + packaging** | 1 | Full SQLite schema; HTML / PDF report export; Inno Setup packaging → `dist/agent-setup-x.y.z.exe` | Copy to a clean Windows machine → install → use |
| **7 — Stub panels light up** | v1.1 / v1.2 | Production scheduler, OTA delta+signature | Out of scope for v1.0 spec |

**Total: ~9 weeks to v1.0 GA.**

## 9. Build System

`build.bat` extensions:

| Command | Action |
|---|---|
| `.\build.bat` | default build (v1.0 complete) |
| `.\build.bat test` | run library unit tests (already supported) |
| `.\build.bat shell` | **shell-only build** (mocks) — fastest iteration during Phase 1 |
| `.\build.bat deps` | re-extract / verify tools/ zips |
| `.\build.bat clean` | already supported |
| `.\build.bat package` | Inno Setup → `dist/agent-setup-x.y.z.exe` |

`third_party/CMakeLists.txt` additions:

- Dear ImGui (docking branch, DX11 + Win32 backends)
- libuv (uncomment the template)
- SQLite (single `.c` + `.h`, zero deps)
- cJSON (single file)
- miniz (log zip, optional)

**Not added** to keep the binary small: stb_image, tinygltf, etc.

## 10. Coding Conventions

- C11, 4-space indent, 100-column lines.
- `.c` / `.h` headers follow `@file / @brief / @note` three-section convention.
- Functions returning `int` use `0 = ok`, negative = error, positive = warning; errors formatted via `agent_errstr(int)`.
- Logging: `AP_LOG_<LVL>(tag, fmt, ...)` — `tag` is module name (`"at"`, `"uv"`, `"llm"`); output includes ts, tid, tag, msg.
- No global mutable state; every service is `*_create()` / `*_destroy()` and passed by pointer.
- Avoid macro pollution: prefer `static inline` functions except for `AP_LOG` and `RUN_TEST`.

## 11. Deliverables (v1.0 GA)

- `zero-c.exe` (main binary with ImGui backend)
- `agent.db` (auto-created on first run)
- `config/*.json` (defaults seeded on first run)
- `logs/` (auto-created on first run)
- `agent-setup-x.y.z.exe` (Inno Setup wrapper)
- `docs/`:
  - `user-guide.md` (features + screenshots)
  - `at-engine.md` (3GPP parsing rules, URC catalog)
  - `llm-providers.md` (base URL / model quick reference)
- `CHANGELOG.md`

## 12. Risks & Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| libuv's `uv_tty_t` on Windows supports only COM (not generic USB-CDC that mimics other devices) | Some modules don't appear as COM | v1.0 also supports `uv_pipe_t` over WinUSB if libuv 1.49+ supports it; otherwise fall back to native `CreateFile` |
| ImGui CN font cold start is 300–500 ms (CJK glyph set) | UX | Background-thread font load + splash screen, **or** CJK subset (~1500 chars actually used) |
| Multi-module + LLM streaming → main thread FPS jitter | UX | LLM streaming uses `uv_async`, max 1 push/frame; lists use virtual scroll |
| mbedtls + libcurl synchronous blocks | Blocks main thread | Required: `CURLOPT_OPENSOCKETFUNCTION` to a libuv socket (curl-impersonate-style); v1.0 may use thread-pool fallback, optimize in v1.1 |
| AT commands differ between vendors outside 3GPP | Parsing failure | AT engine accepts a **module profile** (vendor/model → private URC list) loaded from JSON; hot-reloadable |
| Inno Setup adds installer weight | Larger download | Optional — Phase 6 packaging is opt-in |
