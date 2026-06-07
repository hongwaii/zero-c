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

/* device_manager.h / diag_service.h 是 C 头；本文件是 C++，需 extern "C"
 * 包裹避免符号 mangling。用 extern "C" 块包 include 不会影响内部
 * uv_loop_t * 字段类型（仍按 C 约定解释为指向 C struct 的指针），仅保证
 * 函数符号是 C 链接。 */
extern "C" {
#include "device_manager.h"
#include "diag_service.h"
}

static agent_app_t g_app;

/**
 * @brief 默认串口波特率。
 *
 * 真机用户模组是 9600 波特，原先在 device_manager.c 写死 115200，
 * 导致 AT 命令在 9600 速率下解不出，超时返回 < ERROR。
 * P3：硬编码 9600，settings panel 允许用户改 radio 立刻生效。
 * P3.5：挪到 agent_app_t 字段。
 * P4：从 zh.json 的 settings.serial.default_baud 读，覆盖此处。
 *
 * 跨模块共享（device_manager.c / panel_settings 都要读写），用 extern
 * 链接——简单不优雅但够用。
 */
int g_default_baud = 9600;  /* 用户模组是 9600 波特 */

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
    /* 默认波特率真机用户模组是 9600；P4 起从 zh.json 读 settings.serial.default_baud */
    fprintf(stderr, "main: default_baud = %d\n", g_default_baud);
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
     * g_app.device_manager 在 device_manager_start 之后注入，panel_devices
     * 每帧从这里读 devs[] 实时渲染。 */
    static device_manager_t g_devmgr;
    device_manager_init(&g_devmgr, host_get_uv_loop(ctx));
    device_manager_set_callback(&g_devmgr, NULL, NULL);  /* P2-T8 接入 panel_devices */
    device_manager_start(&g_devmgr);
    /* 把 device_manager 注入 app——panel_devices 每帧从这里读 dev 列表 */
    g_app.device_manager = &g_devmgr;
    g_app.uv_loop = host_get_uv_loop(ctx);

    /* 诊断服务：挂在 device_manager 上，UI 触发 refresh。
     * P3 简化：本 task 不在启动时主动 refresh——由 panel_diag 的"刷新"按钮
     * 调 diag_service_refresh_now() 拉一次（或者后续在 dev_change 回调里
     * 调）。这里只把对象建出来注入 g_app，让 panel_diag 后续能找到。 */
    static diag_service_t *g_diag = NULL;
    g_diag = diag_service_create(host_get_uv_loop(ctx), &g_devmgr);
    g_app.diag_service = g_diag;

    main_window_register_panels();

    int rc = host_run(ctx, tick, &g_app);
    host_destroy(ctx);
    printf("=== %s === exit %d\n", APP_VERSION_STRING, rc);
    return rc;
}
