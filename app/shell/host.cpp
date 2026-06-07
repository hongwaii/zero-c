/**
 * @file host.cpp
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

#include "theme.h"
#include "i18n.h"

#include <uv.h>

/* 主题与 i18n：theme_apply 在 ImGui 上下文创建后立即调用；theme_load_fonts
 * 在 ImGui backend init 之后、第一次 NewFrame 之前；i18n_init 紧随其后。 */

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

struct host_ctx {
    HWND               hwnd;
    ID3D11Device      *device;
    ID3D11DeviceContext*ctx;
    IDXGISwapChain    *swap_chain;
    ID3D11RenderTargetView*rtv;
    bool               quit;
    int                width;
    int                height;
    uv_loop_t         *uv_loop;  /* 弱引用，host 拥有 */
};

static struct host_ctx *g_active_ctx = NULL;

static LRESULT CALLBACK host_wnd_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return 1;
    switch (msg) {
        case WM_SIZE:
            if (g_active_ctx && g_active_ctx->device && wParam != SIZE_MINIMIZED) {
                g_active_ctx->width  = LOWORD(lParam);
                g_active_ctx->height = HIWORD(lParam);
                if (g_active_ctx->rtv) { g_active_ctx->rtv->Release(); g_active_ctx->rtv = NULL; }
                g_active_ctx->swap_chain->ResizeBuffers(
                    0,
                    (UINT)g_active_ctx->width, (UINT)g_active_ctx->height,
                    DXGI_FORMAT_UNKNOWN, 0);
                ID3D11Texture2D *bb = NULL;
                g_active_ctx->swap_chain->GetBuffer(
                    0, __uuidof(ID3D11Texture2D), (void **)&bb);
                g_active_ctx->device->CreateRenderTargetView(
                    (ID3D11Resource *)bb, NULL, &g_active_ctx->rtv);
                bb->Release();
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
    hr = c->swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&bb);
    if (FAILED(hr)) return false;
    hr = c->device->CreateRenderTargetView((ID3D11Resource *)bb, NULL, &c->rtv);
    bb->Release();
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

    /* libuv loop（device_manager / 串口 / 未来定时器都要在这上面跑） */
    c->uv_loop = (uv_loop_t *)malloc(sizeof(uv_loop_t));
    if (!c->uv_loop) {
        fprintf(stderr, "host_create: malloc uv_loop 失败\n");
        DestroyWindow(c->hwnd);
        free(c);
        return -5;
    }
    if (uv_loop_init(c->uv_loop) != 0) {
        fprintf(stderr, "host_create: uv_loop_init 失败\n");
        free(c->uv_loop);
        DestroyWindow(c->hwnd);
        free(c);
        return -1;
    }

    /* ImGui bootstrap */
    ImGui::CreateContext();
    ImGui_ImplWin32_Init(c->hwnd);
    ImGui_ImplDX11_Init(c->device, c->ctx);

    /* 主题：上下文创建后立即应用，否则字体加载前的首帧会用默认色。 */
    theme_apply(AGENT_THEME_ENGINEERING_BLUE);
    /* 字体：在 backend init 之后、第一次 NewFrame 之前。 */
    theme_load_fonts();
    /* i18n：UI 字符串源。 */
    i18n_init(AGENT_LANG_ZH_CN);

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
        /* libuv 非阻塞 tick：让 device_manager 的扫描 timer、
         * 串口 HAL 的读回调能跑。不阻塞主消息循环。 */
        if (c->uv_loop) uv_run(c->uv_loop, UV_RUN_NOWAIT);
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        tick(ud);
        ImGui::Render();
        const float clear[4] = {0.06f, 0.07f, 0.09f, 1.0f};
        c->ctx->ClearRenderTargetView(c->rtv, clear);
        c->ctx->OMSetRenderTargets(1, &c->rtv, NULL);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        c->swap_chain->Present(1, 0);
    }
    return 0;
}

void host_destroy(host_ctx_t *c)
{
    if (!c) return;
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    /* 释放 i18n 加载的字符串树。 */
    i18n_shutdown();
    ImGui::DestroyContext();
    if (c->rtv) c->rtv->Release();
    if (c->swap_chain) c->swap_chain->Release();
    if (c->ctx) c->ctx->Release();
    if (c->device) c->device->Release();
    if (c->hwnd) DestroyWindow(c->hwnd);
    /* libuv 清理（顺序：先关 handle，再 close loop） */
    if (c->uv_loop) {
        /* 注意：P2 阶段 device_manager 还没 stop，会导致 uv_loop_close 返回非 0。
         * 进程退出时 OS 回收所有资源；P3 接入 device_manager_stop 后再修此处。 */
        if (uv_loop_close(c->uv_loop) != 0) {
            fprintf(stderr, "host_destroy: uv_loop_close 非零（残留 handle），P3 修复\n");
        }
        free(c->uv_loop);
        c->uv_loop = NULL;
    }
    g_active_ctx = NULL;
    free(c);
}

void host_request_quit(host_ctx_t *c) { if (c) c->quit = true; }
