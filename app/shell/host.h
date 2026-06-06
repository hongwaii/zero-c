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
