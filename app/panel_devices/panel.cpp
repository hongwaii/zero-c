/**
 * @file panel_devices.cpp
 * @brief 多模组列表 panel——本任务仅占位，Task 14 填 mock 列表。
 */
#include "panel_devices.h"
#include "i18n.h"
#include "imgui.h"

void panel_devices_render(agent_app_t *app)
{
    (void)app;
    ImGui::Text("%s (P1 占位，Task 14 填内容)", i18n_get("devices.title"));
}
