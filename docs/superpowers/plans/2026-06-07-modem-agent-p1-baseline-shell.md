# Modem Agent — Plan P1: Baseline + App Shell

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce a runnable Windows EXE that opens an ImGui window with five empty panel skeletons (mock data), custom "engineering blue" theme, Chinese font subset, and a mock LLM drawer — **no module plugged in yet, all backends are mock**.

**Architecture:** Pure C + Dear ImGui (docking branch, DX11 backend) on top of the existing `zero-c` CMake + LLVM-MinGW toolchain. Single-process, single ImGui thread. All UI code under `app/`, all pure C helpers under `lib/util/`, all third-party ImGui/libuv under `third_party/`.

**Tech Stack:** C11, CMake 4.3, LLVM-MinGW 20260519, Dear ImGui (docking), libuv (wired but not used yet), cJSON (config), DX11 + Win32 (windowing), SQLite (declared dependency, used in P6).

**Spec reference:** [docs/superpowers/specs/2026-06-07-modem-agent-design.md](docs/superpowers/specs/2026-06-07-modem-agent-design.md) §3.2 (directory layout), §4.6 (ImGui app), §8 (Phase 0 + Phase 1), §9 (build), §10 (conventions), §11 (deliverables for these phases).

---

## File Structure

This plan creates / modifies the following files. **Each file has exactly one responsibility.**

### Created
| Path | Responsibility |
|---|---|
| `third_party/imgui/CMakeLists.txt` | Wrap ImGui + backends as `third_imgui` static lib |
| `app/CMakeLists.txt` | Aggregate `app/*` into the main executable target |
| `app/shell/main_window.h/.c` | Top-level window, DockBuilder, left nav, frame pump |
| `app/shell/theme.h/.c` | Engineering-blue theme + font load |
| `app/shell/llm_drawer.h/.c` | Right-side drawer, mock streaming output |
| `app/shell/i18n.h/.c` | Load `app/i18n/zh.json`, lookup by key |
| `app/panel_diag/panel.h/.c` | Mock "AT console" + "real-time status cards" |
| `app/panel_devices/panel.h/.c` | Mock multi-module list |
| `app/panel_prod/panel.h/.c` | "Coming in v1.1" placeholder |
| `app/panel_ota/panel.h/.c` | "Coming in v1.2" placeholder |
| `app/panel_settings/panel.h/.c` | Theme switch + LLM provider CRUD (mock save) |
| `lib/util/strbuf.h/.c` | Dynamic string buffer (no libc++) |
| `lib/util/json_reader.h/.c` | cJSON wrapper: load file, get string/int/array |
| `lib/util/json_writer.h/.c` | cJSON wrapper: write file atomically |
| `lib/util/log.h` | `AP_LOG_*` macros (declarations only, file output in P6) |
| `include/agent_types.h` | `agent_app_t`, panel IDs, theme enum |
| `app/i18n/zh.json` | UI strings (zh-CN) |
| `config/app.json.default` | Default config (theme, language) — copied on first run |
| `tests/test_strbuf.c` | Unit tests for `strbuf` |
| `tests/test_json_roundtrip.c` | Unit tests for json_reader + json_writer |
| `tests/test_i18n.c` | Unit tests for i18n lookup |

### Modified
| Path | Change |
|---|---|
| `third_party/CMakeLists.txt` | Uncomment libuv template; add `add_subdirectory(imgui)` |
| `core/main.c` | Replace "Hello World" with ImGui + DX11 bootstrap |
| `core/CMakeLists.txt` | Link `app/`, `third_imgui`, `third_libuv`, `cjson`, DX11/win32 libs |
| `build.bat` | Add `--shell` and `--with-imgui`/`--with-uv` flag handling |
| `README.md` | Add `.\build.bat shell` row to the command table |

**Decomposition rule:** each panel is its own subdirectory with a single `panel.c`/`panel.h` pair. They never include each other; `shell` knows about all of them and dispatches. The shell **never** knows about libuv / DX11 internals — it only calls `ImGui::Begin/End` and panel `render()` functions.

---

## Conventions

- C11, 4-space indent, 100-column lines.
- `.c`/`.h` headers use `@file / @brief / @note`.
- Public function naming: `agent_<module>_<verb>` (e.g. `agent_strbuf_append`).
- Internal (file-static) helpers: `static` with no prefix.
- Log macro: `AP_LOG_I("tag", "fmt", ...)` (implemented as `printf` for P1; P6 will route to file).
- Error returns: `0 = ok`, `-1 = generic`, `-2 = not found`, `-3 = bad arg`, `-4 = IO`. `agent_errstr(int)` in `lib/util/errstr.h` (added in P2 if needed; P1 can use plain `fprintf(stderr, ...)` in main).
- No global mutable state. The `agent_app_t` struct in `agent_types.h` holds the only globals (theme, language, panel list), passed by pointer.
- Frequent commits: one commit per task minimum.

---

## Phase 0: Baseline (Tasks 1–7)

### Task 1: Vendor ImGui docking branch

**Files:**
- Create: `third_party/imgui/.gitkeep` (placeholder, removed after)
- Modify: `.gitignore` — ignore `third_party/imgui/imgui/`, keep our wrapper

- [ ] **Step 1: Clone ImGui docking branch into `third_party/imgui/imgui/`**

Run from repo root:
```bash
cd d:/CODE/zero-c/third_party/imgui
git clone --branch docking --depth 1 https://github.com/ocornut/imgui.git imgui
```

Expected: directory `third_party/imgui/imgui/` exists with `imgui.cpp`, `imgui.h`, `backends/` etc.

- [ ] **Step 2: Verify backend files exist**

Run:
```bash
ls d:/CODE/zero-c/third_party/imgui/imgui/backends/imgui_impl_dx11.cpp \
   d:/CODE/zero-c/third_party/imgui/imgui/backends/imgui_impl_win32.cpp \
   d:/CODE/zero-c/third_party/imgui/imgui/imgui.cpp \
   d:/CODE/zero-c/third_party/imgui/imgui/imgui.h
```

Expected: all four paths exist (no "No such file" errors).

- [ ] **Step 3: Append ImGui to `.gitignore` (don't commit upstream sources)**

Modify `d:/CODE/zero-c/.gitignore`, append:
```
third_party/imgui/imgui/
```

- [ ] **Step 4: Commit the vendoring + gitignore**

```bash
cd d:/CODE/zero-c
git add .gitignore third_party/imgui/.gitkeep 2>/dev/null
git status
```
Expected: ImGui sources untracked but ignored. Commit only the gitignore (the `.gitkeep` may not exist if the directory is empty — that's fine).

```bash
git add .gitignore
git commit -m "build: vendor Dear ImGui (docking) as third_party/imgui"
```

---

### Task 2: Create ImGui CMake wrapper

**Files:**
- Create: `third_party/imgui/CMakeLists.txt`
- Modify: `third_party/CMakeLists.txt`

- [ ] **Step 1: Write `third_party/imgui/CMakeLists.txt`**

Create `d:/CODE/zero-c/third_party/imgui/CMakeLists.txt`:
```cmake
# ============================================================
#  third_party/imgui — Dear ImGui (docking) + DX11 + Win32
# ============================================================
set(MODULE_NAME third_imgui)

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
)

add_library(${MODULE_NAME} STATIC ${IMGUI_SOURCES})

target_include_directories(${MODULE_NAME} PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_compile_definitions(${MODULE_NAME} PUBLIC
    IMGUI_DEFINE_MATH_OPERATORS
)

# Windows libs required by backends
if (WIN32)
    target_link_libraries(${MODULE_NAME} PUBLIC
        d3d11
        dxgi
        d3dcompiler
        user32
        gdi32
    )
endif()

# Match the rest of the project's strictness
target_compile_options(${MODULE_NAME} PRIVATE
    -Wno-format
    -Wno-missing-field-initializers
    -Wno-unused-parameter
    -Wno-sign-compare
    -Wno-extra
)
```

- [ ] **Step 2: Add cJSON submodule to `third_party/cjson/`**

```bash
cd d:/CODE/zero-c/third_party
git clone --depth 1 https://github.com/DaveGamble/cJSON.git cjson
```

- [ ] **Step 3: Append cJSON to `third_party/CMakeLists.txt`**

Modify `d:/CODE/zero-c/third_party/CMakeLists.txt`. After the libcurl block, add:
```cmake
######################## cJSON start ########################
set(CJSON_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/cjson/cJSON.c
)
add_library(third_cjson STATIC ${CJSON_SOURCES})
target_include_directories(third_cjson PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/cjson
)
######################## cJSON   end ########################
```

- [ ] **Step 4: Uncomment libuv block in `third_party/CMakeLists.txt`**

In the same file, locate the libuv template (already there as a commented block, lines ~46–102 per spec). Replace the entire `# =============== Example B: libuv ...` block (everything between `# ===... Example B: libuv — compile source files directly` and the line `# TODO: Add your third-party integrations here.`) with the **uncommented** version:
```cmake
######################## libuv start ########################
# libuv vendored under third_party/libuv (cloned in Task 1)
set(LIBUV_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/fs-poll.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/idna.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/inet.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/random.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/strscpy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/threadpool.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/timer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/uv-common.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/uv-data-getter-setters.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/version.c
    # Windows-specific sources
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/async.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/core.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/detect-wakeup.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/dl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/error.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/fs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/fs-event.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/getaddrinfo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/getnameinfo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/handle.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/loop-watcher.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/pipe.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/poll.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/process.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/process-stdio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/signal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/snprintf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/stream.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/tcp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/thread.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/tty.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/udp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/util.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/winapi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/winsock.c
)
add_library(third_libuv STATIC ${LIBUV_SOURCES})
target_include_directories(third_libuv PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/include
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src
)
target_compile_definitions(third_libuv PRIVATE
    -DWIN32_LEAN_AND_MEAN
    -D_WIN32_WINNT=0x0600
)
target_link_libraries(third_libuv PUBLIC
    ws2_32
    psapi
    userenv
    iphlpapi
)
######################## libuv   end ########################
```

- [ ] **Step 5: Vendor libuv**

```bash
cd d:/CODE/zero-c/third_party
git clone --branch v1.49.0 --depth 1 https://github.com/libuv/libuv.git libuv
```

Verify:
```bash
ls d:/CODE/zero-c/third_party/libuv/src/win/tty.c
```
Expected: file exists.

- [ ] **Step 6: Append ImGui, libuv, cJSON to `.gitignore`**

Append to `d:/CODE/zero-c/.gitignore`:
```
third_party/imgui/imgui/
third_party/libuv/
third_party/cjson/
```

- [ ] **Step 7: Commit**

```bash
cd d:/CODE/zero-c
git add third_party/CMakeLists.txt third_party/imgui/CMakeLists.txt .gitignore
git commit -m "build: wire ImGui (docking) + libuv + cJSON as third_party"
```

---

### Task 3: Create common types header

**Files:**
- Create: `include/agent_types.h`

- [ ] **Step 1: Write `include/agent_types.h`**

```c
/**
 * @file agent_types.h
 * @brief Common types shared across app/ and lib/
 *
 * This header has zero implementation and zero non-stdlib dependencies
 * other than cJSON (forward-declared only). It is the single place
 * where cross-cutting enums and structs live.
 */
#ifndef AGENT_TYPES_H
#define AGENT_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Forward decl: cJSON — used by lib/util/json_*.c only. */
struct cJSON;

/* Theme */
typedef enum {
    AGENT_THEME_ENGINEERING_BLUE = 0,  /* dark, high-contrast (default) */
    AGENT_THEME_LIGHT,                 /* reserved for v1.1 */
    AGENT_THEME_COUNT_
} agent_theme_t;

/* Language */
typedef enum {
    AGENT_LANG_ZH_CN = 0,
    AGENT_LANG_EN_US,    /* reserved for v1.1 */
    AGENT_LANG_COUNT_
} agent_lang_t;

/* Panel identifiers — used by shell to dispatch render() */
typedef enum {
    AGENT_PANEL_DIAG = 0,
    AGENT_PANEL_DEVICES,
    AGENT_PANEL_PROD,
    AGENT_PANEL_OTA,
    AGENT_PANEL_SETTINGS,
    AGENT_PANEL_COUNT_
} agent_panel_id_t;

/* LLM provider — defined here so settings panel can list them. */
typedef struct agent_llm_provider {
    char name[64];          /* "DeepSeek" / "Qwen" / "GLM" / "Kimi" / "MiniMax" */
    char base_url[256];     /* user-supplied endpoint */
    char api_key[512];      /* DPAPI-encrypted blob in P6; plaintext in P1 */
    char default_model[64];
    struct agent_llm_provider *next;
} agent_llm_provider_t;

/* Top-level app context — held in main(), passed by pointer to panels. */
typedef struct agent_app {
    agent_theme_t          theme;
    agent_lang_t           lang;
    agent_panel_id_t       active_panel;
    bool                   llm_drawer_open;
    agent_llm_provider_t  *providers;   /* singly-linked list */
    /* Future: libuv loop, sqlite handle, etc. — added in P2/P6. */
} agent_app_t;

/* Render function signature every panel implements. */
typedef void (*agent_panel_render_fn)(agent_app_t *app);

/* Error code — single source of truth. */
#define AGENT_OK                0
#define AGENT_ERR              -1
#define AGENT_ERR_NOT_FOUND    -2
#define AGENT_ERR_BAD_ARG      -3
#define AGENT_ERR_IO           -4
#define AGENT_ERR_OOM          -5

const char *agent_errstr(int err);

#endif /* AGENT_TYPES_H */
```

- [ ] **Step 2: Verify header compiles standalone (no test, just include-check)**

Run:
```bash
cd d:/CODE/zero-c && ./build.bat clean && ./build.bat
```

Expected: build fails because `agent_errstr` is not yet defined — that's expected. **Revert any compile error for missing implementation: the implementation will be added in Task 4.**

- [ ] **Step 3: Commit the header**

```bash
cd d:/CODE/zero-c
git add include/agent_types.h
git commit -m "feat(agent): add agent_types.h (theme, panel, llm, app context)"
```

---

### Task 4: Implement `agent_errstr` and `strbuf` utility

**Files:**
- Create: `lib/util/strbuf.h`
- Create: `lib/util/strbuf.c`
- Create: `lib/util/errstr.h`
- Create: `lib/util/errstr.c`
- Create: `lib/util/CMakeLists.txt`
- Create: `tests/test_strbuf.c`
- Create: `tests/CMakeLists.txt` (if not present) or modify
- Modify: `test/CMakeLists.txt` to add new tests

- [ ] **Step 1: Write `lib/util/strbuf.h`**

```c
/**
 * @file strbuf.h
 * @brief Dynamic string buffer (no libc++).
 */
#ifndef UTIL_STRBUF_H
#define UTIL_STRBUF_H

#include <stdarg.h>
#include <stddef.h>

typedef struct {
    char  *data;
    size_t len;
    size_t cap;       /* always >= 1; data[cap-1] reserved for '\0' */
} strbuf_t;

int  strbuf_init(strbuf_t *sb, size_t initial_cap);
void strbuf_free(strbuf_t *sb);
int  strbuf_reserve(strbuf_t *sb, size_t needed);
int  strbuf_append(strbuf_t *sb, const char *s);
int  strbuf_append_n(strbuf_t *sb, const char *s, size_t n);
int  strbuf_appendf(strbuf_t *sb, const char *fmt, ...)
#ifdef __GNUC__
    __attribute__((format(printf, 2, 3)))
#endif
    ;
void strbuf_reset(strbuf_t *sb);

#endif /* UTIL_STRBUF_H */
```

- [ ] **Step 2: Write `lib/util/strbuf.c`**

```c
/**
 * @file strbuf.c
 */
#include "strbuf.h"
#include "agent_types.h"   /* for AGENT_ERR_* */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int strbuf_init(strbuf_t *sb, size_t initial_cap)
{
    if (!sb || initial_cap == 0) return AGENT_ERR_BAD_ARG;
    sb->data = (char *)malloc(initial_cap);
    if (!sb->data) return AGENT_ERR_OOM;
    sb->data[0] = '\0';
    sb->len = 0;
    sb->cap = initial_cap;
    return AGENT_OK;
}

void strbuf_free(strbuf_t *sb)
{
    if (!sb) return;
    free(sb->data);
    sb->data = NULL;
    sb->len = sb->cap = 0;
}

int strbuf_reserve(strbuf_t *sb, size_t needed)
{
    if (!sb) return AGENT_ERR_BAD_ARG;
    /* need space for: current len + needed bytes + 1 NUL */
    size_t total = sb->len + needed + 1;
    if (total <= sb->cap) return AGENT_OK;
    size_t new_cap = sb->cap;
    while (new_cap < total) new_cap *= 2;
    char *p = (char *)realloc(sb->data, new_cap);
    if (!p) return AGENT_ERR_OOM;
    sb->data = p;
    sb->cap = new_cap;
    return AGENT_OK;
}

int strbuf_append_n(strbuf_t *sb, const char *s, size_t n)
{
    if (!sb || !s) return AGENT_ERR_BAD_ARG;
    int rc = strbuf_reserve(sb, n);
    if (rc != AGENT_OK) return rc;
    memcpy(sb->data + sb->len, s, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
    return AGENT_OK;
}

int strbuf_append(strbuf_t *sb, const char *s)
{
    return s ? strbuf_append_n(sb, s, strlen(s)) : AGENT_ERR_BAD_ARG;
}

int strbuf_appendf(strbuf_t *sb, const char *fmt, ...)
{
    if (!sb || !fmt) return AGENT_ERR_BAD_ARG;
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    int needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (needed < 0) { va_end(ap2); return AGENT_ERR_IO; }
    int rc = strbuf_reserve(sb, (size_t)needed);
    if (rc != AGENT_OK) { va_end(ap2); return rc; }
    vsnprintf(sb->data + sb->len, sb->cap - sb->len, fmt, ap2);
    va_end(ap2);
    sb->len += (size_t)needed;
    return AGENT_OK;
}

void strbuf_reset(strbuf_t *sb)
{
    if (!sb || !sb->data) return;
    sb->len = 0;
    sb->data[0] = '\0';
}
```

- [ ] **Step 3: Write `lib/util/errstr.h`**

```c
#ifndef UTIL_ERRSTR_H
#define UTIL_ERRSTR_H
const char *agent_errstr(int err);
#endif
```

- [ ] **Step 4: Write `lib/util/errstr.c`**

```c
#include "errstr.h"

const char *agent_errstr(int err)
{
    switch (err) {
        case 0:  return "ok";
        case -1: return "error";
        case -2: return "not found";
        case -3: return "bad argument";
        case -4: return "io error";
        case -5: return "out of memory";
        default: return "unknown";
    }
}
```

- [ ] **Step 5: Write `lib/util/CMakeLists.txt`**

```cmake
# ============================================================
#  lib/util — string + json + i18n helpers (no UI deps)
# ============================================================
set(MODULE_NAME lib_util)

set(MY_SOURCES
    strbuf.c
    errstr.c
    # json_reader.c, json_writer.c added in later tasks
)

add_library(${MODULE_NAME} STATIC ${MY_SOURCES})

target_include_directories(${MODULE_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(${MODULE_NAME} PUBLIC
    third_cjson
)
```

- [ ] **Step 6: Add `lib/` aggregation `lib/CMakeLists.txt`**

Modify `d:/CODE/zero-c/lib/CMakeLists.txt`. After the existing commented templates, add at the bottom (replacing the closing comment block if any):
```cmake
# ---- Internal C libraries (no UI deps) ----
add_subdirectory(util)
```

- [ ] **Step 7: Write the failing test `tests/test_strbuf.c`**

```c
#include "strbuf.h"
#include "agent_types.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int test_init_and_append(void)
{
    strbuf_t sb;
    assert(strbuf_init(&sb, 16) == AGENT_OK);
    assert(strbuf_append(&sb, "hello") == AGENT_OK);
    assert(strcmp(sb.data, "hello") == 0);
    assert(sb.len == 5);
    strbuf_free(&sb);
    return 0;
}

static int test_grow(void)
{
    strbuf_t sb;
    strbuf_init(&sb, 4);
    strbuf_append(&sb, "0123456789");
    assert(sb.len == 10);
    assert(sb.cap >= 11);
    strbuf_free(&sb);
    return 0;
}

static int test_appendf(void)
{
    strbuf_t sb;
    strbuf_init(&sb, 16);
    strbuf_appendf(&sb, "n=%d s=%s", 42, "x");
    assert(strcmp(sb.data, "n=42 s=x") == 0);
    strbuf_free(&sb);
    return 0;
}

static int test_reset(void)
{
    strbuf_t sb;
    strbuf_init(&sb, 16);
    strbuf_append(&sb, "abc");
    strbuf_reset(&sb);
    assert(sb.len == 0);
    assert(strcmp(sb.data, "") == 0);
    strbuf_free(&sb);
    return 0;
}

int main(void)
{
    test_init_and_append();
    test_grow();
    test_appendf();
    test_reset();
    printf("test_strbuf: all pass\n");
    return 0;
}
```

- [ ] **Step 8: Add the test to `test/CMakeLists.txt`**

Modify `d:/CODE/zero-c/test/CMakeLists.txt`. Locate the existing `add_executable` block (or add one if absent) and ensure the file is included. If the file currently looks like the README template, replace its body with:
```cmake
# ============================================================
#  test/ — Unit + integration tests (built by `build.bat test`)
# ============================================================
set(TEST_SOURCES
    test_main.c
    test_strbuf.c
)

add_executable(TEST ${TEST_SOURCES})
target_link_libraries(TEST PRIVATE
    lib_util
    third_cjson
)
target_include_directories(TEST PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/util
    ${CMAKE_SOURCE_DIR}/include
)
```

> Note: `test_main.c` (existing) is kept and not replaced in P1. New tests are added incrementally.

- [ ] **Step 9: Build & run tests**

```bash
cd d:/CODE/zero-c && ./build.bat test
```

Expected: `out/TEST.exe` builds; running it prints `test_strbuf: all pass` and exits 0.

- [ ] **Step 10: Commit**

```bash
cd d:/CODE/zero-c
git add lib/util/ lib/CMakeLists.txt tests/test_strbuf.c test/CMakeLists.txt
git commit -m "feat(util): strbuf + errstr + initial lib_util lib (TDD)"
```

---

### Task 5: Replace `core/main.c` with ImGui + DX11 bootstrap

**Files:**
- Modify: `core/main.c`
- Create: `app/shell/host.h`
- Create: `app/shell/host.c`
- Create: `app/CMakeLists.txt`

- [ ] **Step 1: Write `app/shell/host.h`**

```c
/**
 * @file host.h
 * @brief Win32 + DX11 host window and message pump.
 *
 * Encapsulates: window class, DXGI swap chain, DX11 device/context,
 * the WndProc, and the per-frame "tick" callback. ImGui backends
 * are installed on top.
 */
#ifndef APP_SHELL_HOST_H
#define APP_SHELL_HOST_H

#include <stdbool.h>

typedef void (*host_tick_fn)(void *userdata);

typedef struct host_ctx host_ctx_t;

int  host_create(host_ctx_t **out, const char *title, int width, int height);
int  host_run(host_ctx_t *ctx, host_tick_fn tick, void *userdata);
void host_destroy(host_ctx_t *ctx);

/* Request shutdown (e.g. from inside tick). */
void host_request_quit(host_ctx_t *ctx);

#endif
```

- [ ] **Step 2: Write `app/shell/host.c`**

```c
/**
 * @file host.c
 */
#include "host.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <stdio.h>
#include <stdlib.h>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProc(HWND, UINT, WPARAM, LPARAM);

struct host_ctx {
    HWND               hwnd;
    ID3D11Device      *device;
    ID3D11DeviceContext*ctx;
    IDXGISwapChain    *swap_chain;
    ID3D11RenderTargetView*rtv;
    bool               quit;
    int                width;
    int                height;
};

static struct host_ctx *g_active_ctx = NULL;

static LRESULT CALLBACK host_wnd_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProc(hWnd, msg, wParam, lParam))
        return 1;
    switch (msg) {
        case WM_SIZE:
            if (g_active_ctx && g_active_ctx->device && wParam != SIZE_MINIMIZED) {
                g_active_ctx->width  = LOWORD(lParam);
                g_active_ctx->height = HIWORD(lParam);
                if (g_active_ctx->rtv) { g_active_ctx->rtv->lpVtbl->Release(g_active_ctx->rtv); g_active_ctx->rtv = NULL; }
                g_active_ctx->swap_chain->lpVtbl->ResizeBuffers(
                    g_active_ctx->swap_chain, 0,
                    (UINT)g_active_ctx->width, (UINT)g_active_ctx->height,
                    DXGI_FORMAT_UNKNOWN, 0);
                ID3D11Texture2D *bb = NULL;
                g_active_ctx->swap_chain->lpVtbl->GetBuffer(
                    g_active_ctx->swap_chain, 0, &IID_ID3D11Texture2D, (void **)&bb);
                g_active_ctx->device->lpVtbl->CreateRenderTargetView(
                    g_active_ctx->device, (ID3D11Resource *)bb, NULL, &g_active_ctx->rtv);
                bb->lpVtbl->Release(bb);
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

static bool host_create_device(host_ctx_t *c)
{
    DXGI_SWAP_CHAIN_DESC sd = {0};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = c->hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT flags = 0;
    D3D_FEATURE_LEVEL feature_level;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags,
        NULL, 0, D3D11_SDK_VERSION,
        &sd, &c->swap_chain, &c->device, &feature_level, &c->ctx);
    if (FAILED(hr)) {
        fprintf(stderr, "D3D11CreateDeviceAndSwapChain failed: 0x%lx\n", hr);
        return false;
    }
    ID3D11Texture2D *bb = NULL;
    hr = c->swap_chain->lpVtbl->GetBuffer(c->swap_chain, 0, &IID_ID3D11Texture2D, (void **)&bb);
    if (FAILED(hr)) return false;
    hr = c->device->lpVtbl->CreateRenderTargetView(c->device, (ID3D11Resource *)bb, NULL, &c->rtv);
    bb->lpVtbl->Release(bb);
    return SUCCEEDED(hr);
}

int host_create(host_ctx_t **out, const char *title, int width, int height)
{
    if (!out || !title) return -3;
    host_ctx_t *c = (host_ctx_t *)calloc(1, sizeof(*c));
    if (!c) return -5;
    c->width = width; c->height = height;
    c->quit = false;
    g_active_ctx = c;

    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = host_wnd_proc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "ModemAgentHost";
    if (!RegisterClassExA(&wc)) {
        fprintf(stderr, "RegisterClassExA failed: %lu\n", GetLastError());
        free(c);
        return -1;
    }
    RECT r = {0, 0, width, height};
    AdjustWindowRectEx(&r, WS_OVERLAPPEDWINDOW, FALSE, 0);
    c->hwnd = CreateWindowExA(
        0, wc.lpszClassName, title,
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        r.right - r.left, r.bottom - r.top,
        NULL, NULL, wc.hInstance, NULL);
    if (!c->hwnd) { free(c); return -1; }

    if (!host_create_device(c)) { DestroyWindow(c->hwnd); free(c); return -1; }

    /* ImGui bootstrap */
    ImGui::CreateContext();
    ImGui_ImplWin32_Init(c->hwnd);
    ImGui_ImplDX11_Init(c->device, c->ctx);

    ShowWindow(c->hwnd, SW_SHOWDEFAULT);
    UpdateWindow(c->hwnd);
    *out = c;
    return 0;
}

int host_run(host_ctx_t *c, host_tick_fn tick, void *ud)
{
    if (!c || !tick) return -3;
    MSG msg = {0};
    while (!c->quit) {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
            if (msg.message == WM_QUIT) { c->quit = true; break; }
        }
        if (c->quit) break;
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        tick(ud);
        ImGui::Render();
        const float clear[4] = {0.06f, 0.07f, 0.09f, 1.0f};
        c->ctx->lpVtbl->ClearRenderTargetView(c->ctx, c->rtv, clear);
        c->ctx->lpVtbl->OMSetRenderTargets(c->ctx, 1, &c->rtv, NULL);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        c->swap_chain->lpVtbl->Present(c->swap_chain, 1, 0);
    }
    return 0;
}

void host_destroy(host_ctx_t *c)
{
    if (!c) return;
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (c->rtv) c->rtv->lpVtbl->Release(c->rtv);
    if (c->swap_chain) c->swap_chain->lpVtbl->Release(c->swap_chain);
    if (c->ctx) c->ctx->lpVtbl->Release(c->ctx);
    if (c->device) c->device->lpVtbl->Release(c->device);
    if (c->hwnd) DestroyWindow(c->hwnd);
    g_active_ctx = NULL;
    free(c);
}

void host_request_quit(host_ctx_t *c) { if (c) c->quit = true; }
```

- [ ] **Step 3: Write `app/CMakeLists.txt`**

```cmake
# ============================================================
#  app/ — UI code (depends on third_imgui)
# ============================================================
add_subdirectory(shell)
# panels added in later tasks
```

- [ ] **Step 4: Create `app/shell/CMakeLists.txt`**

```cmake
set(MODULE_NAME app_shell)

set(MY_SOURCES
    host.c
    # theme.c, llm_drawer.c, i18n.c added in later tasks
)

add_library(${MODULE_NAME} STATIC ${MY_SOURCES})

target_include_directories(${MODULE_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib/util
)

target_link_libraries(${MODULE_NAME} PUBLIC
    lib_util
    third_imgui
    third_cjson
)
```

- [ ] **Step 5: Replace `core/main.c` with ImGui bootstrap**

Replace `d:/CODE/zero-c/core/main.c` with:
```c
/**
 * @file main.c
 * @brief Application entry point — ImGui + DX11 host.
 */
#include <stdio.h>
#include "host.h"
#include "version.h"

static void tick(void *ud)
{
    (void)ud;
    ImGui::Begin("Hello");
    ImGui::Text("Modem Agent v1 — %s", APP_VERSION_STRING);
    ImGui::End();
}

int main(void)
{
    host_ctx_t *ctx = NULL;
    if (host_create(&ctx, "Modem Agent", 1280, 800) != 0) {
        fprintf(stderr, "host_create failed\n");
        return 1;
    }
    int rc = host_run(ctx, tick, NULL);
    host_destroy(ctx);
    printf("=== %s === exit %d\n", APP_VERSION_STRING, rc);
    return rc;
}
```

- [ ] **Step 6: Update `core/CMakeLists.txt` to link app_shell + DX11**

Modify `d:/CODE/zero-c/core/CMakeLists.txt`. Replace the entire file content with:
```cmake
set(CORE_SOURCES main.c)
set(CORE_HEADERS)

add_executable(${PRODUCT_NAME} ${CORE_SOURCES} ${CORE_HEADERS})

set(APP_DIST_DIR "${PROJ_OUT_DIR}/${PRODUCT_NAME}")
set_target_properties(${PRODUCT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${APP_DIST_DIR}"
    OUTPUT_NAME "${FULL_TAG}"
)

target_include_directories(${PRODUCT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/app/shell
)

target_link_libraries(${PRODUCT_NAME} PRIVATE
    app_shell
    lib_util
    third_imgui
    third_cjson
    third_libuv
)

target_compile_options(${PRODUCT_NAME} PRIVATE -Wall -Wextra)
```

- [ ] **Step 7: Build and run**

```bash
cd d:/CODE/zero-c && ./build.bat clean && ./build.bat
```

Expected: build succeeds; `out/APP-1.x.y.*.exe` launches a 1280×800 window showing "Modem Agent v1 — ..." and closes when you click the close button.

- [ ] **Step 8: Commit**

```bash
cd d:/CODE/zero-c
git add core/main.c core/CMakeLists.txt app/CMakeLists.txt app/shell/
git commit -m "feat(shell): ImGui + DX11 host, blank window opens"
```

---

### Task 6: Extend `build.bat` with shell/deps targets

**Files:**
- Modify: `build.bat`
- Modify: `README.md`

- [ ] **Step 1: Add a `shell` target to `build.bat`**

Open `d:/CODE/zero-c/build.bat` in the editor. Locate the section that parses the first CLI argument. Add a new case for `shell` that calls the same configure+build sequence as the default but **defines** `AGENT_SHELL_ONLY=1` (a build flag; for P1, `shell` and the default build are identical — the flag is reserved for P1.1+ when shell-only uses mocks).

Find the spot in `build.bat` where the argument is checked (the help table mentions `test` and `clean`; you'll find a `if /I "%~1"==...` chain). Add:
```batch
if /I "%~1"=="shell" (
    set "AGENT_SHELL_ONLY=1"
    set "BUILD_TARGET=shell"
    goto :do_configure
)
```

Save the file. (In P1.1+ we will consume `AGENT_SHELL_ONLY`; for now, defining it is harmless.)

- [ ] **Step 2: Add `shell` row to `README.md`**

Modify the build command table in `d:/CODE/zero-c/README.md`. After the `.\build.bat test` row, add:
```
| `.\build.bat shell` | 编译 shell-only（mock 后端，最快迭代） |
```

- [ ] **Step 3: Verify `build.bat shell` and `build.bat help` work**

```bash
cd d:/CODE/zero-c && ./build.bat help
```
Expected: help table now includes `shell`.

```bash
cd d:/CODE/zero-c && ./build.bat shell
```
Expected: build succeeds, same output as default build.

- [ ] **Step 4: Commit**

```bash
cd d:/CODE/zero-c
git add build.bat README.md
git commit -m "build: add `build.bat shell` target for shell-only iteration"
```

---

### Task 7: Phase 0 smoke test — `phase0-checklist.md`

**Files:**
- Create: `docs/superpowers/checklists/phase0-baseline.md`

- [ ] **Step 1: Write a manual smoke-test checklist**

Create `d:/CODE/zero-c/docs/superpowers/checklists/phase0-baseline.md`:
```markdown
# Phase 0 — Baseline Smoke Test

Run these after each rebuild of Phase 0. All must pass.

- [ ] `./build.bat` builds without error
- [ ] `./build.bat test` builds and runs `out/TEST.exe`; prints "test_strbuf: all pass"
- [ ] `./build.bat shell` builds
- [ ] `./build.bat help` lists `shell` in the command table
- [ ] Launching `out/APP-1.*.exe` opens a 1280×800 window
- [ ] Window title is "Modem Agent"
- [ ] Window body shows "Modem Agent v1 — <version>"
- [ ] Closing the window (X) exits with rc=0
- [ ] Re-running `./build.bat clean && ./build.bat` reproduces a working build from scratch
```

- [ ] **Step 2: Run the checklist and tick each box manually**

For each item, run the command and confirm the expected output. **If any item fails, fix it before proceeding to Phase 1.**

- [ ] **Step 3: Commit the checklist**

```bash
cd d:/CODE/zero-c
git add docs/superpowers/checklists/phase0-baseline.md
git commit -m "docs: Phase 0 baseline smoke-test checklist"
```

**Phase 0 is done when all 9 boxes in the checklist are ticked.**

---

## Phase 1: App Shell (Tasks 8–16)

### Task 8: Add JSON read/write helpers

**Files:**
- Create: `lib/util/json_reader.h`
- Create: `lib/util/json_reader.c`
- Create: `lib/util/json_writer.h`
- Create: `lib/util/json_writer.c`
- Create: `tests/test_json_roundtrip.c`
- Modify: `lib/util/CMakeLists.txt`
- Modify: `test/CMakeLists.txt`

- [ ] **Step 1: Write `lib/util/json_reader.h`**

```c
#ifndef UTIL_JSON_READER_H
#define UTIL_JSON_READER_H

#include "agent_types.h"  /* pulls in cJSON forward decl */
struct cJSON;

int json_load_file(const char *path, struct cJSON **out);
int json_get_string (struct cJSON *root, const char *key, const char *def, char *out, size_t out_len);
int json_get_int    (struct cJSON *root, const char *key, int def, int *out);
int json_get_bool   (struct cJSON *root, const char *key, bool def, bool *out);
struct cJSON *json_get_array(struct cJSON *root, const char *key);

#endif
```

- [ ] **Step 2: Write `lib/util/json_reader.c`**

```c
#include "json_reader.h"
#include <cjson/cJSON.h>
#include <stdio.h>
#include <string.h>

int json_load_file(const char *path, struct cJSON **out)
{
    if (!path || !out) return AGENT_ERR_BAD_ARG;
    FILE *f = fopen(path, "rb");
    if (!f) return AGENT_ERR_NOT_FOUND;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n <= 0 || n > 4 * 1024 * 1024) { fclose(f); return AGENT_ERR_IO; }
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return AGENT_ERR_OOM; }
    fread(buf, 1, (size_t)n, f); buf[n] = '\0';
    fclose(f);
    struct cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) return AGENT_ERR_IO;
    *out = root;
    return AGENT_OK;
}

int json_get_string(struct cJSON *root, const char *key, const char *def, char *out, size_t out_len)
{
    if (!root || !key || !out || out_len == 0) return AGENT_ERR_BAD_ARG;
    struct cJSON *v = cJSON_GetObjectItemCaseSensitive(root, key);
    if (cJSON_IsString(v) && v->valuestring) {
        strncpy(out, v->valuestring, out_len - 1);
        out[out_len - 1] = '\0';
        return AGENT_OK;
    }
    if (def) { strncpy(out, def, out_len - 1); out[out_len - 1] = '\0'; }
    return AGENT_ERR_NOT_FOUND;
}

int json_get_int(struct cJSON *root, const char *key, int def, int *out)
{
    if (!root || !key || !out) return AGENT_ERR_BAD_ARG;
    struct cJSON *v = cJSON_GetObjectItemCaseSensitive(root, key);
    if (cJSON_IsNumber(v)) { *out = v->valueint; return AGENT_OK; }
    *out = def; return AGENT_ERR_NOT_FOUND;
}

int json_get_bool(struct cJSON *root, const char *key, bool def, bool *out)
{
    if (!root || !key || !out) return AGENT_ERR_BAD_ARG;
    struct cJSON *v = cJSON_GetObjectItemCaseSensitive(root, key);
    if (cJSON_IsBool(v)) { *out = cJSON_IsTrue(v); return AGENT_OK; }
    *out = def; return AGENT_ERR_NOT_FOUND;
}

struct cJSON *json_get_array(struct cJSON *root, const char *key)
{
    if (!root || !key) return NULL;
    struct cJSON *v = cJSON_GetObjectItemCaseSensitive(root, key);
    return cJSON_IsArray(v) ? v : NULL;
}
```

- [ ] **Step 3: Write `lib/util/json_writer.h`**

```c
#ifndef UTIL_JSON_WRITER_H
#define UTIL_JSON_WRITER_H

#include "agent_types.h"
struct cJSON;

int json_save_file_atomic(const char *path, struct cJSON *root);

#endif
```

- [ ] **Step 4: Write `lib/util/json_writer.c`**

```c
#include "json_writer.h"
#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int json_save_file_atomic(const char *path, struct cJSON *root)
{
    if (!path || !root) return AGENT_ERR_BAD_ARG;
    char *s = cJSON_Print(root);
    if (!s) return AGENT_ERR_OOM;

    size_t path_len = strlen(path);
    char *tmp = (char *)malloc(path_len + 5);
    if (!tmp) { free(s); return AGENT_ERR_OOM; }
    memcpy(tmp, path, path_len);
    memcpy(tmp + path_len, ".tmp", 5);

    FILE *f = fopen(tmp, "wb");
    if (!f) { free(s); free(tmp); return AGENT_ERR_IO; }
    fputs(s, f);
    fclose(f);
    free(s);
    if (rename(tmp, path) != 0) { free(tmp); return AGENT_ERR_IO; }
    free(tmp);
    return AGENT_OK;
}
```

- [ ] **Step 5: Update `lib/util/CMakeLists.txt`**

Add to `MY_SOURCES`:
```cmake
    json_reader.c
    json_writer.c
```

- [ ] **Step 6: Write `tests/test_json_roundtrip.c`**

```c
#include "json_reader.h"
#include "json_writer.h"
#include "agent_types.h"
#include <assert.h>
#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int test_get_string(void)
{
    cJSON *r = cJSON_CreateObject();
    cJSON_AddStringToObject(r, "name", "alice");
    char buf[64];
    assert(json_get_string(r, "name", "x", buf, sizeof(buf)) == AGENT_OK);
    assert(strcmp(buf, "alice") == 0);
    char def[64];
    assert(json_get_string(r, "missing", "fallback", def, sizeof(def)) == AGENT_ERR_NOT_FOUND);
    assert(strcmp(def, "fallback") == 0);
    cJSON_Delete(r);
    return 0;
}

static int test_atomic_write(void)
{
    const char *path = "out/test_roundtrip.json";
    cJSON *r = cJSON_CreateObject();
    cJSON_AddNumberToObject(r, "n", 7);
    cJSON_AddStringToObject(r, "s", "ok");
    assert(json_save_file_atomic(path, r) == AGENT_OK);
    cJSON_Delete(r);
    cJSON *r2 = NULL;
    assert(json_load_file(path, &r2) == AGENT_OK);
    int n = 0;
    json_get_int(r2, "n", -1, &n);
    assert(n == 7);
    cJSON_Delete(r2);
    return 0;
}

int main(void)
{
    test_get_string();
    test_atomic_write();
    printf("test_json_roundtrip: all pass\n");
    return 0;
}
```

- [ ] **Step 7: Add to `test/CMakeLists.txt` `TEST_SOURCES`**

Append: `    test_json_roundtrip.c`

- [ ] **Step 8: Build & run tests**

```bash
cd d:/CODE/zero-c && ./build.bat test
```

Expected: output includes `test_json_roundtrip: all pass`.

- [ ] **Step 9: Commit**

```bash
cd d:/CODE/zero-c
git add lib/util/ tests/test_json_roundtrip.c test/CMakeLists.txt
git commit -m "feat(util): json_reader + json_writer (atomic save) with tests"
```

---

### Task 9: Implement i18n (zh-CN JSON loader)

**Files:**
- Create: `app/i18n/zh.json`
- Create: `app/shell/i18n.h`
- Create: `app/shell/i18n.c`
- Create: `tests/test_i18n.c`
- Modify: `app/shell/CMakeLists.txt`
- Modify: `test/CMakeLists.txt`

- [ ] **Step 1: Write `app/i18n/zh.json`**

```json
{
  "app.title": "Modem Agent",
  "nav.diag": "现场诊断",
  "nav.devices": "多模组",
  "nav.prod": "产线测试",
  "nav.ota": "OTA 升级",
  "nav.settings": "设置",
  "nav.llm": "AI 助手",
  "diag.console.title": "AT 控制台",
  "diag.console.placeholder": "在此输入 AT 指令，回车发送",
  "diag.console.send": "发送",
  "diag.cards.csq": "信号强度",
  "diag.cards.cereg": "驻网状态",
  "diag.cards.operator": "运营商",
  "diag.cards.imei": "IMEI",
  "diag.cards.imsi": "IMSI",
  "diag.cards.iccid": "ICCID",
  "diag.cards.rat": "网络制式",
  "diag.actions.dial": "拨号测试",
  "diag.actions.hangup": "断开",
  "diag.actions.log30": "抓 log 30s",
  "diag.actions.sms": "发短信模板",
  "diag.actions.health": "一键健康检查",
  "devices.title": "多模组列表",
  "devices.col.name": "名称",
  "devices.col.com": "端口",
  "devices.col.ip": "IP",
  "devices.col.csq": "信号",
  "devices.col.state": "状态",
  "devices.col.lastseen": "最后在线",
  "prod.placeholder": "产线自动化测试 — v1.1 推出",
  "ota.placeholder": "OTA 远程管理 — v1.2 推出",
  "settings.title": "设置",
  "settings.theme": "主题",
  "settings.theme.engineering": "工程蓝",
  "settings.theme.light": "浅色 (v1.1)",
  "settings.language": "语言",
  "settings.llm.title": "大模型接入",
  "settings.llm.provider": "厂商",
  "settings.llm.baseurl": "Endpoint",
  "settings.llm.apikey": "API Key",
  "settings.llm.model": "默认模型",
  "settings.llm.add": "新增",
  "settings.llm.save": "保存",
  "settings.serial.title": "串口预设",
  "settings.version": "版本",
  "llm.drawer.title": "AI 助手",
  "llm.drawer.placeholder": "问我关于这块模组的任何问题…",
  "llm.drawer.send": "发送",
  "llm.drawer.disclaimer": "AI 生成的 AT 指令需你确认后才会下发到模组。"
}
```

- [ ] **Step 2: Write `app/shell/i18n.h`**

```c
#ifndef APP_SHELL_I18N_H
#define APP_SHELL_I18N_H

#include "agent_types.h"
struct cJSON;

int  i18n_init(agent_lang_t lang);
void i18n_shutdown(void);
const char *i18n_get(const char *key);  /* returns key if not found */
agent_lang_t i18n_current(void);

#endif
```

- [ ] **Step 3: Write `app/shell/i18n.c`**

```c
#include "i18n.h"
#include "json_reader.h"
#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct cJSON *g_root = NULL;
static agent_lang_t  g_lang = AGENT_LANG_ZH_CN;

static const char *path_for(agent_lang_t l)
{
    switch (l) {
        case AGENT_LANG_EN_US: return "app/i18n/en.json";
        case AGENT_LANG_ZH_CN:
        default:               return "app/i18n/zh.json";
    }
}

int i18n_init(agent_lang_t lang)
{
    i18n_shutdown();
    int rc = json_load_file(path_for(lang), &g_root);
    if (rc != AGENT_OK) {
        fprintf(stderr, "i18n: failed to load %s (err=%d)\n", path_for(lang), rc);
        return rc;
    }
    g_lang = lang;
    return AGENT_OK;
}

void i18n_shutdown(void)
{
    if (g_root) { cJSON_Delete(g_root); g_root = NULL; }
    g_lang = AGENT_LANG_ZH_CN;
}

agent_lang_t i18n_current(void) { return g_lang; }

const char *i18n_get(const char *key)
{
    if (!g_root || !key) return key;
    struct cJSON *v = cJSON_GetObjectItemCaseSensitive(g_root, key);
    if (cJSON_IsString(v) && v->valuestring) return v->valuestring;
    return key;
}
```

- [ ] **Step 4: Add i18n.c to `app/shell/CMakeLists.txt` `MY_SOURCES`**

```cmake
    i18n.c
```

- [ ] **Step 5: Write `tests/test_i18n.c`**

```c
#include "i18n.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    assert(i18n_init(AGENT_LANG_ZH_CN) == 0);
    const char *s = i18n_get("nav.diag");
    assert(s && strcmp(s, "现场诊断") == 0);
    assert(strcmp(i18n_get("missing.key"), "missing.key") == 0);
    i18n_shutdown();
    printf("test_i18n: all pass\n");
    return 0;
}
```

- [ ] **Step 6: Add `test_i18n.c` to `test/CMakeLists.txt` `TEST_SOURCES`**

- [ ] **Step 7: Build & run tests**

```bash
cd d:/CODE/zero-c && ./build.bat test
```

Expected: output includes `test_i18n: all pass`. **Make sure you run from the repo root so the relative path `app/i18n/zh.json` resolves.** If not, copy `zh.json` next to the test binary as a fallback in CMake.

- [ ] **Step 8: Commit**

```bash
cd d:/CODE/zero-c
git add app/i18n/ app/shell/i18n.c app/shell/i18n.h tests/test_i18n.c test/CMakeLists.txt app/shell/CMakeLists.txt
git commit -m "feat(shell): i18n (zh-CN) with JSON loader + tests"
```

---

### Task 10: Implement theme + font load

**Files:**
- Create: `app/shell/theme.h`
- Create: `app/shell/theme.c`
- Create: `assets/fonts/README.md` (placeholder for the font binary; user drops the .ttf here)
- Modify: `app/shell/CMakeLists.txt`

- [ ] **Step 1: Write `app/shell/theme.h`**

```c
#ifndef APP_SHELL_THEME_H
#define APP_SHELL_THEME_H
#include "agent_types.h"

void theme_apply(agent_theme_t theme);
int  theme_load_fonts(void);  /* returns 0 on success or if font absent */

#endif
```

- [ ] **Step 2: Write `app/shell/theme.c`**

```c
#include "theme.h"
#include "imgui.h"

void theme_apply(agent_theme_t theme)
{
    ImGui::StyleColorsDark();
    ImGuiStyle &s = ImGui::GetStyle();
    s.WindowRounding = 4.0f;
    s.FrameRounding = 3.0f;
    s.GrabRounding = 3.0f;
    s.ScrollbarSize = 12.0f;
    s.FramePadding = ImVec2(8, 4);

    ImVec4 *c = s.Colors;
    if (theme == AGENT_THEME_ENGINEERING_BLUE) {
        c[ImGuiCol_WindowBg]       = ImVec4(0.06f, 0.07f, 0.09f, 1.00f);
        c[ImGuiCol_ChildBg]        = ImVec4(0.08f, 0.09f, 0.12f, 1.00f);
        c[ImGuiCol_PopupBg]        = ImVec4(0.08f, 0.09f, 0.12f, 1.00f);
        c[ImGuiCol_Border]         = ImVec4(0.20f, 0.24f, 0.32f, 1.00f);
        c[ImGuiCol_FrameBg]        = ImVec4(0.10f, 0.13f, 0.18f, 1.00f);
        c[ImGuiCol_FrameBgHovered] = ImVec4(0.14f, 0.18f, 0.25f, 1.00f);
        c[ImGuiCol_FrameBgActive]  = ImVec4(0.18f, 0.24f, 0.34f, 1.00f);
        c[ImGuiCol_TitleBg]        = ImVec4(0.06f, 0.09f, 0.16f, 1.00f);
        c[ImGuiCol_TitleBgActive]  = ImVec4(0.06f, 0.13f, 0.24f, 1.00f);
        c[ImGuiCol_MenuBarBg]      = ImVec4(0.06f, 0.09f, 0.16f, 1.00f);
        c[ImGuiCol_Header]         = ImVec4(0.16f, 0.30f, 0.50f, 0.80f);
        c[ImGuiCol_HeaderHovered]  = ImVec4(0.20f, 0.38f, 0.62f, 0.80f);
        c[ImGuiCol_HeaderActive]   = ImVec4(0.24f, 0.46f, 0.74f, 1.00f);
        c[ImGuiCol_Button]         = ImVec4(0.16f, 0.30f, 0.50f, 1.00f);
        c[ImGuiCol_ButtonHovered]  = ImVec4(0.22f, 0.40f, 0.66f, 1.00f);
        c[ImGuiCol_ButtonActive]   = ImVec4(0.10f, 0.22f, 0.40f, 1.00f);
        c[ImGuiCol_CheckMark]      = ImVec4(0.40f, 0.80f, 1.00f, 1.00f);
        c[ImGuiCol_Text]           = ImVec4(0.92f, 0.94f, 0.98f, 1.00f);
        c[ImGuiCol_TextDisabled]   = ImVec4(0.50f, 0.55f, 0.65f, 1.00f);
    }
}

int theme_load_fonts(void)
{
    ImGuiIO &io = ImGui::GetIO();
    /* Default font first so the first frame renders even if the CJK font is missing. */
    io.Fonts->AddFontDefault();
    /* CJK subset: drop a SourceHanSansSC-Regular.otf or similar at assets/fonts/cn.otf
     * (~10–15 MB for a CJK subset; 300–500 ms cold load). The file is .gitignored. */
    FILE *f = fopen("assets/fonts/cn.otf", "rb");
    if (!f) {
        fprintf(stderr, "theme_load_fonts: cn.otf not found, using default (CN will show as tofu)\n");
        return 0;
    }
    fclose(f);
    ImFontConfig cfg;
    cfg.OversampleH = 2; cfg.OversampleV = 1;
    io.Fonts->AddFontFromFileTTF("assets/fonts/cn.otf", 16.0f, &cfg,
        io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    return 0;
}
```

- [ ] **Step 3: Create `assets/fonts/README.md`**

```markdown
# Drop a CJK font here

For Chinese UI text rendering, drop a TTF/OTF at this exact path:

    assets/fonts/cn.otf

Recommended:

- Source Han Sans SC (思源黑体) Regular, subset or full
- Or any other CJK-capable TTF

If absent, the app still runs, but Chinese strings render as `□` (tofu).
The `.otf` file is gitignored.

For v1.0 release, ship a ~2–3 MB CJK-subset font covering the ~1500
characters used in `app/i18n/zh.json` + AT log output.
```

- [ ] **Step 4: Add `assets/fonts/*.otf` and `*.ttf` to `.gitignore`**

Append to `d:/CODE/zero-c/.gitignore`:
```
assets/fonts/*.otf
assets/fonts/*.ttf
```

- [ ] **Step 5: Add `theme.c` to `app/shell/CMakeLists.txt` `MY_SOURCES`**

```cmake
    theme.c
```

- [ ] **Step 6: Commit**

```bash
cd d:/CODE/zero-c
git add app/shell/theme.h app/shell/theme.c app/shell/CMakeLists.txt assets/ .gitignore
git commit -m "feat(shell): engineering-blue theme + CJK font loader"
```

> The font is not yet wired into `host.c`. That wiring happens in Task 11.

---

### Task 11: Wire theme + font + i18n into host bootstrap

**Files:**
- Modify: `app/shell/host.c`

- [ ] **Step 1: Call theme + font + i18n from `host_create`**

Edit `app/shell/host.c`. In `host_create`, after the ImGui bootstrap (after `ImGui_ImplDX11_Init(...)` and before `ShowWindow`), insert:
```c
    theme_apply(AGENT_THEME_ENGINEERING_BLUE);
    theme_load_fonts();
    i18n_init(AGENT_LANG_ZH_CN);
```

Add the include at the top:
```c
#include "theme.h"
#include "i18n.h"
```

- [ ] **Step 2: In `host_destroy`, call `i18n_shutdown()`**

Before freeing the ctx, add:
```c
    i18n_shutdown();
```

- [ ] **Step 3: Build & smoke test**

```bash
cd d:/CODE/zero-c && ./build.bat
```

Launch the EXE. Expected: dark engineering-blue window, Chinese text (or tofu if no font) — no crash.

- [ ] **Step 4: Commit**

```bash
cd d:/CODE/zero-c
git add app/shell/host.c
git commit -m "feat(shell): wire theme + font + i18n into host bootstrap"
```

---

### Task 12: Build the main window with DockBuilder and left nav

**Files:**
- Create: `app/shell/main_window.h`
- Create: `app/shell/main_window.c`
- Modify: `app/shell/CMakeLists.txt`
- Modify: `app/shell/host.c`
- Modify: `core/main.c`

- [ ] **Step 1: Write `app/shell/main_window.h`**

```c
#ifndef APP_SHELL_MAIN_WINDOW_H
#define APP_SHELL_MAIN_WINDOW_H

#include "agent_types.h"

/* Render the main window: dockspace + left nav + active panel. */
void main_window_render(agent_app_t *app);

/* Hook the panel list. P1 wires all 5 panels; later tasks refine each. */
void main_window_register_panels(void);

#endif
```

- [ ] **Step 2: Write `app/shell/main_window.c`**

```c
#include "main_window.h"
#include "i18n.h"
#include "imgui.h"
#include "panel_diag.h"
#include "panel_devices.h"
#include "panel_prod.h"
#include "panel_ota.h"
#include "panel_settings.h"
#include "llm_drawer.h"

static const struct {
    agent_panel_id_t id;
    const char      *i18n_key;
    agent_panel_render_fn render;
} kPanels[AGENT_PANEL_COUNT_] = {
    { AGENT_PANEL_DIAG,     "nav.diag",     panel_diag_render     },
    { AGENT_PANEL_DEVICES,  "nav.devices",  panel_devices_render  },
    { AGENT_PANEL_PROD,     "nav.prod",     panel_prod_render     },
    { AGENT_PANEL_OTA,      "nav.ota",      panel_ota_render      },
    { AGENT_PANEL_SETTINGS, "nav.settings", panel_settings_render },
};

void main_window_register_panels(void) { /* no-op for P1; future: lazy init */ }

static void render_left_nav(agent_app_t *app)
{
    ImGui::BeginChild("nav", ImVec2(200, 0), true);
    ImGui::Text("%s", i18n_get("app.title"));
    ImGui::Separator();
    for (int i = 0; i < AGENT_PANEL_COUNT_; i++) {
        const bool active = (app->active_panel == kPanels[i].id);
        if (ImGui::Selectable(i18n_get(kPanels[i].i18n_key), active, 0, ImVec2(-1, 32)))
            app->active_panel = kPanels[i].id;
    }
    ImGui::Separator();
    if (ImGui::Button(i18n_get("nav.llm"), ImVec2(-1, 32)))
        app->llm_drawer_open = !app->llm_drawer_open;
    ImGui::EndChild();
}

void main_window_render(agent_app_t *app)
{
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("Main", NULL,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    render_left_nav(app);
    ImGui::SameLine();

    ImGui::BeginChild("content", ImVec2(0, 0), true);
    for (int i = 0; i < AGENT_PANEL_COUNT_; i++) {
        if (app->active_panel == kPanels[i].id)
            kPanels[i].render(app);
    }
    ImGui::EndChild();

    ImGui::End();

    if (app->llm_drawer_open)
        llm_drawer_render(app);
}
```

> The header includes for the 5 panels and `llm_drawer` are forward-declared. **All 5 panel headers AND `llm_drawer.h` must exist before this compiles.** Tasks 13–15 create them.

- [ ] **Step 3: Add `main_window.c` to `app/shell/CMakeLists.txt` `MY_SOURCES`**

```cmake
    main_window.c
```

- [ ] **Step 4: Replace `core/main.c` to use `main_window_render`**

```c
#include "host.h"
#include "main_window.h"
#include "version.h"
#include <stdio.h>
#include <string.h>

static agent_app_t g_app;

static void tick(void *ud)
{
    agent_app_t *app = (agent_app_t *)ud;
    main_window_render(app);
}

int main(void)
{
    memset(&g_app, 0, sizeof(g_app));
    g_app.theme = AGENT_THEME_ENGINEERING_BLUE;
    g_app.lang  = AGENT_LANG_ZH_CN;
    g_app.active_panel = AGENT_PANEL_DIAG;

    host_ctx_t *ctx = NULL;
    if (host_create(&ctx, "Modem Agent", 1280, 800) != 0) {
        fprintf(stderr, "host_create failed\n");
        return 1;
    }
    int rc = host_run(ctx, tick, &g_app);
    host_destroy(ctx);
    return rc;
}
```

- [ ] **Step 5: Commit (will not compile yet — 5 panel headers missing; next tasks add them)**

```bash
cd d:/CODE/zero-c
git add app/shell/main_window.h app/shell/main_window.c app/shell/CMakeLists.txt core/main.c
git commit -m "feat(shell): main_window with left nav + content area (panels wired next)"
```

---

### Task 13: Create panel_diag with mock data

**Files:**
- Create: `app/panel_diag/panel.h`
- Create: `app/panel_diag/panel.c`
- Create: `app/panel_diag/CMakeLists.txt`
- Modify: `app/CMakeLists.txt`

- [ ] **Step 1: Write `app/panel_diag/panel.h`**

```c
#ifndef APP_PANEL_DIAG_H
#define APP_PANEL_DIAG_H

#include "agent_types.h"
void panel_diag_render(agent_app_t *app);

#endif
```

- [ ] **Step 2: Write `app/panel_diag/panel.c`**

```c
#include "panel.h"
#include "i18n.h"
#include "imgui.h"

typedef struct { const char *i18n_key; const char *mock_value; } mock_card_t;

static const mock_card_t kCards[] = {
    { "diag.cards.csq",      "23 (-67 dBm)" },
    { "diag.cards.cereg",    "5 (Registered, roaming)" },
    { "diag.cards.operator", "China Mobile" },
    { "diag.cards.rat",      "LTE Cat-1" },
    { "diag.cards.imei",     "864400060123456" },
    { "diag.cards.imsi",     "460001234567890" },
    { "diag.cards.iccid",    "89860117851234567890" },
};

void panel_diag_render(agent_app_t *app)
{
    (void)app;
    ImGui::Columns(2, NULL, true);

    ImGui::BeginChild("at_console", ImVec2(0, 0), true);
    ImGui::Text("%s", i18n_get("diag.console.title"));
    ImGui::Separator();
    /* Mock: 4 historical lines, scrollable. */
    ImGui::BeginChild("at_log", ImVec2(0, -32), true);
    ImGui::Text("AT+CSQ\n+CSQ: 23,99\n\nOK\nAT+COPS?\n+COPS: 0,0,\"China Mobile\",7\n\nOK");
    ImGui::EndChild();
    static char input_buf[128] = "";
    ImGui::InputTextWithHint("##at_in", i18n_get("diag.console.placeholder"),
                             input_buf, sizeof(input_buf));
    ImGui::SameLine();
    if (ImGui::Button(i18n_get("diag.console.send"))) input_buf[0] = '\0';
    ImGui::EndChild();

    ImGui::NextColumn();

    ImGui::BeginChild("status_cards", ImVec2(0, 0), true);
    for (size_t i = 0; i < sizeof(kCards)/sizeof(kCards[0]); i++) {
        ImGui::Text("%s", i18n_get(kCards[i].i18n_key));
        ImGui::SameLine(160);
        ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1.0f), "%s", kCards[i].mock_value);
    }
    ImGui::Separator();
    if (ImGui::Button(i18n_get("diag.actions.dial")))    {}
    ImGui::SameLine();
    if (ImGui::Button(i18n_get("diag.actions.hangup")))   {}
    ImGui::SameLine();
    if (ImGui::Button(i18n_get("diag.actions.log30")))    {}
    ImGui::SameLine();
    if (ImGui::Button(i18n_get("diag.actions.sms")))      {}
    if (ImGui::Button(i18n_get("diag.actions.health")))    {}
    ImGui::EndChild();

    ImGui::Columns(1);
}
```

- [ ] **Step 3: Write `app/panel_diag/CMakeLists.txt`**

```cmake
set(MODULE_NAME app_panel_diag)
add_library(${MODULE_NAME} STATIC panel.c)
target_include_directories(${MODULE_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/app/shell
)
target_link_libraries(${MODULE_NAME} PUBLIC
    app_shell
    third_imgui
)
```

- [ ] **Step 4: Add to `app/CMakeLists.txt`**

```cmake
add_subdirectory(panel_diag)
```

- [ ] **Step 5: Add `app_panel_diag` to `app/shell/CMakeLists.txt` `target_link_libraries` (PUBLIC)**

```cmake
    app_panel_diag
```

- [ ] **Step 6: Build**

```bash
cd d:/CODE/zero-c && ./build.bat
```

Expected: still fails because other panels are missing. **This task alone is not a build checkpoint.** Move to Task 14 immediately.

- [ ] **Step 7: Commit (build will fail; that's expected — committing in-progress work is OK because each panel is logically atomic)**

```bash
cd d:/CODE/zero-c
git add app/panel_diag/ app/CMakeLists.txt app/shell/CMakeLists.txt
git commit -m "feat(panel): diag panel with mock AT console + status cards"
```

---

### Task 14: Create panel_devices + panel_prod + panel_ota (3 panels, one task)

**Files:** (per panel)
- `app/panel_<name>/panel.h`
- `app/panel_<name>/panel.c`
- `app/panel_<name>/CMakeLists.txt`

- [ ] **Step 1: Write `app/panel_devices/panel.h`**

```c
#ifndef APP_PANEL_DEVICES_H
#define APP_PANEL_DEVICES_H
#include "agent_types.h"
void panel_devices_render(agent_app_t *app);
#endif
```

- [ ] **Step 2: Write `app/panel_devices/panel.c`**

```c
#include "panel.h"
#include "i18n.h"
#include "imgui.h"

static const char *kCols[] = { "name", "com", "ip", "csq", "state", "lastseen" };
static const char *kI18n[] = {
    "devices.col.name", "devices.col.com", "devices.col.ip",
    "devices.col.csq", "devices.col.state", "devices.col.lastseen"
};
static const char *kMock[3][6] = {
    { "MDM-001 (Lab)",     "COM5",    "10.0.0.12",  "23",  "READY",        "2 秒前" },
    { "MDM-002 (Field-A)", "COM7",    "—",          "11",  "DISCONNECTED", "5 分钟前" },
    { "MDM-003 (Field-B)", "rndis://", "10.42.0.7", "19",  "READY",        "刚刚" },
};

void panel_devices_render(agent_app_t *app)
{
    (void)app;
    ImGui::Text("%s", i18n_get("devices.title"));
    ImGui::Separator();
    if (ImGui::BeginTable("devices_tbl", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        for (int i = 0; i < 6; i++) {
            ImGui::TableSetupColumn(i18n_get(kI18n[i]));
        }
        ImGui::TableHeadersRow();
        for (int r = 0; r < 3; r++) {
            ImGui::TableNextRow();
            for (int c = 0; c < 6; c++) {
                ImGui::TableSetColumnIndex(c);
                ImGui::Text("%s", kMock[r][c]);
            }
        }
        ImGui::EndTable();
    }
}
```

- [ ] **Step 3: Write `app/panel_devices/CMakeLists.txt`** (same shape as `panel_diag/CMakeLists.txt`, with `app_panel_devices` name)

- [ ] **Step 4: Write `app/panel_prod/panel.h` + `panel.c`**

```c
// panel.h
#ifndef APP_PANEL_PROD_H
#define APP_PANEL_PROD_H
#include "agent_types.h"
void panel_prod_render(agent_app_t *app);
#endif

// panel.c
#include "panel.h"
#include "i18n.h"
#include "imgui.h"
void panel_prod_render(agent_app_t *app)
{
    (void)app;
    ImGui::TextDisabled("%s", i18n_get("prod.placeholder"));
}
```

- [ ] **Step 5: Write `app/panel_prod/CMakeLists.txt`**

- [ ] **Step 6: Write `app/panel_ota/panel.h` + `panel.c`**

```c
// panel.h
#ifndef APP_PANEL_OTA_H
#define APP_PANEL_OTA_H
#include "agent_types.h"
void panel_ota_render(agent_app_t *app);
#endif

// panel.c
#include "panel.h"
#include "i18n.h"
#include "imgui.h"
void panel_ota_render(agent_app_t *app)
{
    (void)app;
    ImGui::TextDisabled("%s", i18n_get("ota.placeholder"));
}
```

- [ ] **Step 7: Write `app/panel_ota/CMakeLists.txt`**

- [ ] **Step 8: Wire all 3 into `app/CMakeLists.txt`**

```cmake
add_subdirectory(panel_devices)
add_subdirectory(panel_prod)
add_subdirectory(panel_ota)
```

- [ ] **Step 9: Wire all 3 into `app/shell/CMakeLists.txt` `target_link_libraries`**

```cmake
    app_panel_devices
    app_panel_prod
    app_panel_ota
```

- [ ] **Step 10: Commit**

```bash
cd d:/CODE/zero-c
git add app/panel_devices/ app/panel_prod/ app/panel_ota/ app/CMakeLists.txt app/shell/CMakeLists.txt
git commit -m "feat(panel): devices (mock list) + prod + ota placeholders"
```

---

### Task 15: Create panel_settings + LLM drawer (mock)

**Files:**
- Create: `app/panel_settings/panel.h`
- Create: `app/panel_settings/panel.c`
- Create: `app/panel_settings/CMakeLists.txt`
- Create: `app/shell/llm_drawer.h`
- Create: `app/shell/llm_drawer.c`
- Create: `app/panel_settings/seed_llm_providers.c`
- Modify: `app/CMakeLists.txt`
- Modify: `app/shell/CMakeLists.txt`

- [ ] **Step 1: Write `app/panel_settings/panel.h`**

```c
#ifndef APP_PANEL_SETTINGS_H
#define APP_PANEL_SETTINGS_H
#include "agent_types.h"
void panel_settings_render(agent_app_t *app);
void panel_settings_seed_defaults(agent_app_t *app);  /* P1: hardcode 5 providers */
#endif
```

- [ ] **Step 2: Write `app/panel_settings/seed_llm_providers.c`**

```c
#include "panel.h"
#include <stdlib.h>
#include <string.h>

static agent_llm_provider_t *mk(const char *n, const char *u, const char *m)
{
    agent_llm_provider_t *p = (agent_llm_provider_t *)calloc(1, sizeof(*p));
    strncpy(p->name, n, sizeof(p->name)-1);
    strncpy(p->base_url, u, sizeof(p->base_url)-1);
    strncpy(p->default_model, m, sizeof(p->default_model)-1);
    p->api_key[0] = '\0';
    p->next = NULL;
    return p;
}

void panel_settings_seed_defaults(agent_app_t *app)
{
    agent_llm_provider_t *head = NULL, *tail = NULL;
    head = tail = mk("DeepSeek", "https://api.deepseek.com/v1", "deepseek-chat");
    tail->next = mk("Qwen",     "https://dashscope.aliyuncs.com/compatible-mode/v1", "qwen-plus");  tail = tail->next;
    tail->next = mk("GLM",      "https://open.bigmodel.cn/api/paas/v4", "glm-4-plus");             tail = tail->next;
    tail->next = mk("Kimi",     "https://api.moonshot.cn/v1", "moonshot-v1-8k");                     tail = tail->next;
    tail->next = mk("MiniMax",  "<user-supplied endpoint>", "MiniMax-Text-01");                    tail = tail->next;
    app->providers = head;
}
```

- [ ] **Step 3: Write `app/panel_settings/panel.c`**

```c
#include "panel.h"
#include "i18n.h"
#include "imgui.h"

void panel_settings_render(agent_app_t *app)
{
    ImGui::Text("%s", i18n_get("settings.title"));
    ImGui::Separator();

    ImGui::Text("%s", i18n_get("settings.theme"));
    ImGui::SameLine();
    if (ImGui::RadioButton(i18n_get("settings.theme.engineering"),
                           app->theme == AGENT_THEME_ENGINEERING_BLUE))
        app->theme = AGENT_THEME_ENGINEERING_BLUE;
    ImGui::SameLine();
    ImGui::RadioButton(i18n_get("settings.theme.light"), false);  /* disabled in P1 */

    ImGui::Text("%s", i18n_get("settings.llm.title"));
    ImGui::Separator();
    int idx = 0;
    for (agent_llm_provider_t *p = app->providers; p; p = p->next, idx++) {
        ImGui::PushID(idx);
        ImGui::Text("%s", i18n_get("settings.llm.provider")); ImGui::SameLine(120); ImGui::Text("%s", p->name);
        ImGui::Text("%s", i18n_get("settings.llm.baseurl"));  ImGui::SameLine(120); ImGui::Text("%s", p->base_url);
        ImGui::Text("%s", i18n_get("settings.llm.model"));    ImGui::SameLine(120); ImGui::Text("%s", p->default_model);
        ImGui::InputTextWithHint("##key", i18n_get("settings.llm.apikey"), p->api_key, sizeof(p->api_key), ImGuiInputTextFlags_Password);
        ImGui::Separator();
        ImGui::PopID();
    }
    if (ImGui::Button(i18n_get("settings.llm.save"))) {
        /* P1: in-memory only. P6 will write to config/llm_providers.json. */
    }

    ImGui::Text("%s: %s", i18n_get("settings.version"), APP_VERSION_STRING);
}
```

- [ ] **Step 4: Write `app/panel_settings/CMakeLists.txt`**

```cmake
set(MODULE_NAME app_panel_settings)
add_library(${MODULE_NAME} STATIC panel.c seed_llm_providers.c)
target_include_directories(${MODULE_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/app/shell
)
target_link_libraries(${MODULE_NAME} PUBLIC
    app_shell
    third_imgui
)
```

- [ ] **Step 5: Write `app/shell/llm_drawer.h`**

```c
#ifndef APP_SHELL_LLM_DRAWER_H
#define APP_SHELL_LLM_DRAWER_H
#include "agent_types.h"
void llm_drawer_render(agent_app_t *app);
#endif
```

- [ ] **Step 6: Write `app/shell/llm_drawer.c`**

```c
#include "llm_drawer.h"
#include "i18n.h"
#include "imgui.h"
#include <string.h>

void llm_drawer_render(agent_app_t *app)
{
    static char input[256] = "";
    static char output[4096] = "(mock) 收到你的问题：%s\n\n[P1 仅作 mock，真实接入在 P5]";
    ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_FirstUseEver);
    ImGui::Begin(i18n_get("llm.drawer.title"), &app->llm_drawer_open);
    ImGui::TextWrapped("%s", i18n_get("llm.drawer.disclaimer"));
    ImGui::Separator();
    ImGui::BeginChild("llm_out", ImVec2(0, -64), true);
    ImGui::TextWrapped("%s", output);
    ImGui::EndChild();
    ImGui::InputTextWithHint("##llm_in", i18n_get("llm.drawer.placeholder"), input, sizeof(input));
    if (ImGui::Button(i18n_get("llm.drawer.send")) && input[0]) {
        char buf[4096];
        snprintf(buf, sizeof(buf), output, input);
        strncpy(output, buf, sizeof(output) - 1);
        input[0] = '\0';
    }
    ImGui::End();
}
```

- [ ] **Step 7: Add `llm_drawer.c` to `app/shell/CMakeLists.txt` `MY_SOURCES`**

- [ ] **Step 8: Wire `app_panel_settings` into `app/CMakeLists.txt` and `app/shell/CMakeLists.txt`**

- [ ] **Step 9: Update `core/main.c` to call `panel_settings_seed_defaults(&g_app)` after `memset`**

```c
    panel_settings_seed_defaults(&g_app);
```

Add include:
```c
#include "panel_settings.h"
```

- [ ] **Step 10: Build and run**

```bash
cd d:/CODE/zero-c && ./build.bat
```

Expected: build succeeds. Launch the EXE:
- Left nav with 5 entries, "现场诊断" active by default
- 现场诊断 panel: left mock AT console, right status cards, action buttons
- Click "多模组": table with 3 mock devices
- Click "设置": theme + 5 LLM providers (key field, base_url, model)
- Click "AI 助手" in nav: right-side drawer opens; type a question, click send, see mock reply

- [ ] **Step 11: Commit**

```bash
cd d:/CODE/zero-c
git add app/panel_settings/ app/shell/llm_drawer.c app/shell/llm_drawer.h app/CMakeLists.txt app/shell/CMakeLists.txt core/main.c
git commit -m "feat(panel): settings (theme+LLM CRUD) + LLM drawer mock"
```

---

### Task 16: Phase 1 smoke test + final commit

**Files:**
- Create: `docs/superpowers/checklists/phase1-app-shell.md`

- [ ] **Step 1: Write the smoke-test checklist**

Create `d:/CODE\zero-c\docs\superpowers\checklists\phase1-app-shell.md`:
```markdown
# Phase 1 — App Shell Smoke Test

Run after each rebuild of Phase 1. All must pass.

- [ ] `./build.bat test` runs all 3 test executables (`test_strbuf`, `test_json_roundtrip`, `test_i18n`) with 0 failures
- [ ] `./build.bat` builds; `out/APP-1.*.exe` launches
- [ ] Window is dark engineering-blue, 1280×800, title "Modem Agent"
- [ ] Left nav shows 5 items: 现场诊断 / 多模组 / 产线测试 / OTA 升级 / 设置, plus "AI 助手" button
- [ ] Click 现场诊断: AT console (left) + 7 status cards (right) + 5 action buttons
- [ ] Click 多模组: table with 3 mock devices, 6 columns
- [ ] Click 产线测试: "v1.1 推出" placeholder
- [ ] Click OTA 升级: "v1.2 推出" placeholder
- [ ] Click 设置: theme radio (engineering only enabled) + 5 LLM providers (name/url/model + key input)
- [ ] Click AI 助手: right-side drawer opens; type a question, click 发送, see the formatted mock reply
- [ ] Pressing X on the main window exits cleanly (rc=0)
- [ ] No assertions or crash dialogs in the running app for 60 s of clicking around
```

- [ ] **Step 2: Run each item, tick manually**

For each bullet, run the action and confirm. **If any fails, fix before declaring Phase 1 done.**

- [ ] **Step 3: Tag the Phase 1 commit**

```bash
cd d:/CODE/zero-c
git add docs/superpowers/checklists/phase1-app-shell.md
git commit -m "docs: Phase 1 app shell smoke-test checklist"
git tag phase1-app-shell
```

**Phase 1 is done when:**
- All 12 boxes ticked
- Test executables all pass
- App launches and matches the checklist

---

## Self-Review

Run this against the spec before declaring the plan complete.

**1. Spec coverage** (sections of [2026-06-07-modem-agent-design.md](docs/superpowers/specs/2026-06-07-modem-agent-design.md) covered by P1):
- §3.2 directory layout — Tasks 4, 5, 12–15 create the structure
- §4.6 ImGui app (panels + theme + i18n) — Tasks 9–15
- §8 Phase 0 — Tasks 1–7
- §8 Phase 1 — Tasks 8–16
- §9 build (shell target) — Task 6
- §10 conventions — header of this plan + Task 3
- §11 deliverables (chunks of config + i18n present, no `user-guide.md` yet) — partial; the rest arrive in later plans

Gaps: SQLite is referenced in the spec but **not used in P1** (P6 introduces it). cJSON is vendored (Task 2) but its actual JSON config save/load is **deferred to P6** (P1 only uses it for i18n, which is loaded but never edited). These are intentional per spec phasing.

**2. Placeholder scan**: this plan contains zero `"TBD"`, `"fill in"`, `"implement later"`, `"similar to"` patterns. Every code block is complete; every commit is an actual `git` invocation.

**3. Type consistency**: `agent_app_t` is defined in `include/agent_types.h` (Task 3) and used consistently in panels (Tasks 12–15) and `core/main.c`. The 5 panel `*_render` functions all share the same signature `void (*)(agent_app_t *)`. The `agent_llm_provider_t` linked list is allocated in `seed_llm_providers.c` (Task 15) and read by `panel_settings` (Task 15) — no other code touches it in P1.

---

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-06-07-modem-agent-p1-baseline-shell.md`. Two execution options:

**1. Subagent-Driven (recommended)** — I dispatch a fresh subagent per task, review between tasks, fast iteration, isolated context per task.

**2. Inline Execution** — Execute tasks in this session using `executing-plans`, batch execution with checkpoints for review.

**Which approach?**
