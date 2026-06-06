/**
 * @file panel_settings.cpp
 * @brief 设置 panel——本任务仅占位，Task 15 填主题切换 + LLM provider CRUD。
 */
#include "panel_settings.h"
#include "imgui.h"

void panel_settings_render(agent_app_t *app)
{
    (void)app;
    ImGui::Text("%s (P1 占位，Task 15 填内容)", "Settings");
}
