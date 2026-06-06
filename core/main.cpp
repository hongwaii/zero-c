/**
 * @file main.cpp
 * @brief Application entry point — ImGui + DX11 host.
 */
#include <stdio.h>
#include "imgui.h"
#include "host.h"
#include "version.h"
#include "debug_console.h"

static void tick(void *ud)
{
    (void)ud;
    ImGui::Begin("Hello");
    /* ASCII only here; CJK font is added in P1 Task 10. */
    ImGui::Text("Modem Agent v1 - %s", APP_VERSION_STRING);
    ImGui::End();
}

int main(void)
{
    /* Opt-in debug console (no-op unless AGENT_DEBUG_CONSOLE=1 or --debug-console). */
    agent_maybe_open_debug_console(__argc, __argv);

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
