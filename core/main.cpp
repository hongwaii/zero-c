/**
 * @file main.cpp
 * @brief Application entry point — ImGui + DX11 host.
 */
#include <stdio.h>
#include "imgui.h"
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
