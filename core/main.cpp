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

    main_window_register_panels();

    int rc = host_run(ctx, tick, &g_app);
    host_destroy(ctx);
    printf("=== %s === exit %d\n", APP_VERSION_STRING, rc);
    return rc;
}
