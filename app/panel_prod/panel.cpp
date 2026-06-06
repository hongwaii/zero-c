/**
 * @file panel_prod.cpp
 * @brief 产线测试 panel：P1 占位，v1.1 推出。
 *
 * 留作架构占位（spec §4.4 ProductionService 是 stub），UI 仅显示提示。
 */
#include "panel_prod.h"
#include "i18n.h"
#include "imgui.h"

void panel_prod_render(agent_app_t *app)
{
    (void)app;
    ImGui::TextDisabled("%s", i18n_get("prod.placeholder"));
}
