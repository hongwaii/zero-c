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

/* libuv loop 前向 typedef：保持 host.h 不引 <uv.h>。 */
struct uv_loop_s;
typedef struct uv_loop_s uv_loop_t;

typedef void (*host_tick_fn)(void *userdata);

typedef struct host_ctx host_ctx_t;

int  host_create(host_ctx_t **out, const char *title, int width, int height);
int  host_run(host_ctx_t *ctx, host_tick_fn tick, void *userdata);
void host_destroy(host_ctx_t *ctx);

/* Request shutdown (e.g. from inside tick). */
void host_request_quit(host_ctx_t *ctx);

/**
 * @brief 取 host 内部的 libuv loop 指针。
 *
 * core/main.cpp 等"知道 host_ctx_t 但看不到 struct 定义"的地方
 * 拿 uv_loop 必须走这个 getter，不能直接访问 struct 字段。
 */
uv_loop_t *host_get_uv_loop(host_ctx_t *ctx);

#endif
