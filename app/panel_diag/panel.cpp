/**
 * @file panel_diag.cpp
 * @brief 现场诊断 panel——本任务仅占位，Task 13 填 mock 数据。
 */
#include "panel_diag.h"
#include "i18n.h"
#include "imgui.h"

void panel_diag_render(agent_app_t *app)
{
    (void)app;
    ImGui::Text("%s (P1 占位，Task 13 填内容)", i18n_get("diag.console.title"));
}
