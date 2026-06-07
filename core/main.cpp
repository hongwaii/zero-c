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
     * 注意：host_ctx 是 opaque struct，loop 指针的获取需要 host.h 暴露
     * getter（待 P2-T8 在 host.h 加 host_get_uv_loop()，这里改用该 getter）。
     * 现在用 NULL 占位——init 内部走 NULL 分支，扫描不会启动；P2-T8 拿到
     * getter 后会接上真正的 host loop，扫描 timer 才生效。 */
    static device_manager_t g_devmgr;
    (void)device_manager_init(&g_devmgr, NULL);
    device_manager_set_callback(&g_devmgr, NULL, NULL);  /* P2-T8 接入 panel_devices */
    /* device_manager_start(&g_devmgr); */  /* 等 P2-T8 拿到真 loop 再 start */
    /* g_app.uv_loop = host_get_uv_loop(ctx); */  /* P2-T8 接入 panels */

    main_window_register_panels();

    int rc = host_run(ctx, tick, &g_app);
    host_destroy(ctx);
    printf("=== %s === exit %d\n", APP_VERSION_STRING, rc);
    return rc;
}
