/**
 * @file main_window.cpp
 * @brief 主窗口实现：左 200px 导航条 + 右侧内容区 + DockBuilder 布局。
 */
#include "main_window.h"
#include "i18n.h"
#include "imgui.h"
#include "panel_diag.h"
#include "panel_devices.h"
#include "panel_prod.h"
#include "panel_ota.h"
#include "panel_settings.h"
#include "llm_drawer.h"

/**
 * @brief panel 注册表：id + i18n key + render 回调。
 *
 * 5 个 panel 按 spec §4.6 顺序固定——P1 期间顺序就是导航顺序。
 * 未来可换 dynamic registration，本期保持静态。
 */
static const struct {
    agent_panel_id_t        id;
    const char             *i18n_key;
    agent_panel_render_fn   render;
} kPanels[AGENT_PANEL_COUNT_] = {
    { AGENT_PANEL_DIAG,     "nav.diag",     panel_diag_render     },
    { AGENT_PANEL_DEVICES,  "nav.devices",  panel_devices_render  },
    { AGENT_PANEL_PROD,     "nav.prod",     panel_prod_render     },
    { AGENT_PANEL_OTA,      "nav.ota",      panel_ota_render      },
    { AGENT_PANEL_SETTINGS, "nav.settings", panel_settings_render },
};

void main_window_register_panels(void)
{
    /* P1 空操作；未来 panel 走 lazy init 时这里分配资源。 */
}

/**
 * @brief 渲染左侧导航栏：app 标题 + 5 个 panel 按钮 + "AI 助手" 按钮。
 *
 * 用 ImGui::Button + PushStyleColor 替代 Selectable —— 后者在 ImGui 1.92.9
 * 对 UTF-8 CJK 字符串渲染有问题，前者稳定。Active 态用蓝色背景，inactive 用默认。
 */
static void render_left_nav(agent_app_t *app)
{
    ImGui::BeginChild("nav", ImVec2(220, 0), true);
    ImGui::Text("%s", i18n_get("app.title"));
    ImGui::Separator();

    for (int i = 0; i < AGENT_PANEL_COUNT_; i++) {
        const bool active = (app->active_panel == kPanels[i].id);
        const char *label = i18n_get(kPanels[i].i18n_key);

        /* 颜色：active 用蓝底白字（视觉上等价于 Selectable 的"选中"态）；
         * inactive 用普通按钮色。 */
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button,
                ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
            ImGui::PushStyleColor(ImGuiCol_Text,
                ImGui::GetStyleColorVec4(ImGuiCol_Text));
        } else {
            /* 让 inactive 看起来不像"按钮"——背景透明 / 文本默认色 */
            ImGui::PushStyleColor(ImGuiCol_Button,
                ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                ImGui::GetStyleColorVec4(ImGuiCol_Header));
            /* Text 色用默认，不 push */
            ImGui::PushStyleColor(ImGuiCol_Text,
                ImGui::GetStyleColorVec4(ImGuiCol_Text));
        }

        if (ImGui::Button(label, ImVec2(-1, 32))) {
            app->active_panel = kPanels[i].id;
        }

        ImGui::PopStyleColor(4);
    }

    ImGui::Separator();
    if (ImGui::Button(i18n_get("nav.llm"), ImVec2(-1, 32)))
        app->llm_drawer_open = !app->llm_drawer_open;
    ImGui::EndChild();
}

/**
 * @brief 渲染主窗口：铺满全屏，左 nav + 右 content（dispatch 当前 panel）。
 */
void main_window_render(agent_app_t *app)
{
    /* 全屏无边框窗口作为容器 */
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("Main", NULL,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    render_left_nav(app);
    ImGui::SameLine();

    ImGui::BeginChild("content", ImVec2(0, 0), true);
    for (int i = 0; i < AGENT_PANEL_COUNT_; i++) {
        if (app->active_panel == kPanels[i].id)
            kPanels[i].render(app);
    }
    ImGui::EndChild();

    ImGui::End();

    /* LLM 抽屉独立窗口 */
    if (app->llm_drawer_open)
        llm_drawer_render(app);
}
