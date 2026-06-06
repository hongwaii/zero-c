/**
 * @file panel_settings.cpp
 * @brief 设置 panel：主题单选 + 5 个 LLM provider 列表（name/url/model + key 输入）。
 *
 * P1 期间 in-memory only，保存按钮只弹 printf；
 * P6 改用 json_save_file_atomic 写 config/llm_providers.json + DPAPI 加密 key。
 */
#include "panel_settings.h"
#include "i18n.h"
#include "imgui.h"
#include "version.h"
#include <cstdio>

/**
 * @brief 渲染设置 panel：主题 + LLM provider 列表 + 版本号。
 */
void panel_settings_render(agent_app_t *app)
{
    ImGui::Text("%s", i18n_get("settings.title"));
    ImGui::Separator();

    /* ---- 主题 ---- */
    ImGui::Text("%s", i18n_get("settings.theme"));
    ImGui::SameLine();
    if (ImGui::RadioButton(i18n_get("settings.theme.engineering"),
                           app->theme == AGENT_THEME_ENGINEERING_BLUE))
        app->theme = AGENT_THEME_ENGINEERING_BLUE;
    ImGui::SameLine();
    /* 浅色主题 v1.1 才支持；显示但不可点 */
    ImGui::RadioButton(i18n_get("settings.theme.light"), false);

    ImGui::Text("%s", i18n_get("settings.language"));
    ImGui::SameLine();
    ImGui::RadioButton("中文", app->lang == AGENT_LANG_ZH_CN);
    ImGui::SameLine();
    ImGui::RadioButton("English (v1.1)", false);

    ImGui::Separator();

    /* ---- LLM 接入 ---- */
    ImGui::Text("%s", i18n_get("settings.llm.title"));
    ImGui::Separator();

    int idx = 0;
    for (agent_llm_provider_t *p = app->providers; p; p = p->next, idx++) {
        ImGui::PushID(idx);

        ImGui::Text("%s", i18n_get("settings.llm.provider"));
        ImGui::SameLine(120);
        ImGui::Text("%s", p->name);

        ImGui::Text("%s", i18n_get("settings.llm.baseurl"));
        ImGui::SameLine(120);
        ImGui::Text("%s", p->base_url);

        ImGui::Text("%s", i18n_get("settings.llm.model"));
        ImGui::SameLine(120);
        ImGui::Text("%s", p->default_model);

        /* API key 用 Password flag 隐藏输入字符 */
        ImGui::InputTextWithHint("##key", i18n_get("settings.llm.apikey"),
                                 p->api_key, sizeof(p->api_key),
                                 ImGuiInputTextFlags_Password);

        ImGui::Separator();
        ImGui::PopID();
    }

    if (ImGui::Button(i18n_get("settings.llm.save"))) {
        /* P1：仅打印一行；P6：写 JSON + DPAPI 加密 */
        std::printf("[settings] LLM provider 配置已暂存（in-memory，P6 落盘）。\n");
    }

    ImGui::Separator();
    ImGui::Text("%s: %s", i18n_get("settings.version"), APP_VERSION_STRING);
}
