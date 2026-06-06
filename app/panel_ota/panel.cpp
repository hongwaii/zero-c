/**
 * @file panel_ota.cpp
 * @brief OTA 升级 panel——P1 仅占位。
 */
#include "panel_ota.h"
#include "i18n.h"
#include "imgui.h"

void panel_ota_render(agent_app_t *app)
{
    (void)app;
    ImGui::TextDisabled("%s", i18n_get("ota.placeholder"));
}
