/**
 * @file panel_prod.cpp
 * @brief 产线测试 panel——P1 仅占位。
 */
#include "panel_prod.h"
#include "i18n.h"
#include "imgui.h"

void panel_prod_render(agent_app_t *app)
{
    (void)app;
    ImGui::TextDisabled("%s", i18n_get("prod.placeholder"));
}
