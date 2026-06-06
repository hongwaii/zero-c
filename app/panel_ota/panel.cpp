/**
 * @file panel_ota.cpp
 * @brief OTA 升级 panel：P1 占位，v1.2 推出。
 *
 * 留作架构占位（spec §4.4 OtaService 是 stub），UI 仅显示提示。
 */
#include "panel_ota.h"
#include "i18n.h"
#include "imgui.h"

void panel_ota_render(agent_app_t *app)
{
    (void)app;
    ImGui::TextDisabled("%s", i18n_get("ota.placeholder"));
}
