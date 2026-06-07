/**
 * @file main.cpp
 * @brief 应用入口：ImGui + DX11 host + 主窗口。
 */
#include <stdio.h>
#include "imgui.h"
#include "host.h"
#include "main_window.h"
#include "debug_console.h"
#include "version.h"
#include "agent_types.h"
#include "panel_settings.h"

/* device_manager.h 是 C 头；本文件是 C++，需 extern "C" 包裹避免符号 mangling。
 * 用 extern "C" 块包 include 不会影响 device_manager_t 内的 uv_loop_t *
 * 字段类型（仍按 C 约定解释为指向 C struct 的指针），仅保证函数符号是 C
 * 链接。 */
extern "C" {
#include "device_manager.h"
}

static agent_app_t g_app;

static void tick(void *ud)
{
    agent_app_t *app = (agent_app_t *)ud;
    main_window_render(app);
}

int main(void)
{
    agent_maybe_open_debug_console(__argc, __argv);

    /* 默认值：工程蓝 + zh-CN + 现场诊断 panel */
    memset(&g_app, 0, sizeof(g_app));
    g_app.theme        = AGENT_THEME_ENGINEERING_BLUE;
    g_app.lang         = AGENT_LANG_ZH_CN;
    g_app.active_panel = AGENT_PANEL_DIAG;
    g_app.llm_drawer_open = false;
    g_app.providers    = NULL;
    panel_settings_seed_defaults(&g_app);

    host_ctx_t *ctx = NULL;
    if (host_create(&ctx, "Modem Agent", 1280, 800) != 0) {
        fprintf(stderr, "host_create failed\n");
        return 1;
    }

    /* 启动 device_manager（用 host 的 uv_loop）。
     * host_ctx 是 opaque struct，loop 指针走 host_get_uv_loop() 取，
     * 不直接戳 struct 字段。device_manager_init 接受 NULL loop 时
     * 不崩但不会真正起 timer；这里把 host 的 loop 传进去，start 之后
     * 扫描 timer 才会真的在 host 的 uv_run(NOWAIT) 上 tick。
     * 注：plan 里提的 g_app.device_manager 字段目前不在 agent_app_t
     * 内——P2-T8 接入 panel_devices 时再补该字段并赋值。 */
    static device_manager_t g_devmgr;
    device_manager_init(&g_devmgr, host_get_uv_loop(ctx));
    device_manager_set_callback(&g_devmgr, NULL, NULL);  /* P2-T8 接入 panel_devices */
    device_manager_start(&g_devmgr);
    g_app.uv_loop = host_get_uv_loop(ctx);

    main_window_register_panels();

    int rc = host_run(ctx, tick, &g_app);
    host_destroy(ctx);
    printf("=== %s === exit %d\n", APP_VERSION_STRING, rc);
    return rc;
}
