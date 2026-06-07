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

/* 默认串口波特率，定义在 core/main.cpp（跨模块 extern 共享）。
 * 在这里直接写：用户改 radio 立即生效（不重启），下次 device_manager
 * 扫描重建 dev 时会用新 baud。已存在的 dev 不更新，P3 简化。 */
extern int g_default_baud;

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

    /* ---- 串口默认波特率 ----
     * P3 简化：UI 只暴露 9600 和 115200 两档——用户真模组 9600，原写死 115200
     * 导致 AT 解不开；如需 19200/38400/57600 等再补。改了立即生效（写入
     * g_default_baud 全局），但**已存在**的 dev 不重建 chan_uri，P3 简化：
     * 改 baud 后让用户重插模组或等扫描重建。P4 再加 zh.json 持久化。 */
    ImGui::Text("%s", i18n_get("settings.serial.default_baud"));
    ImGui::SameLine();
    /* 0=9600, 1=115200；初始按 g_default_baud 推断选中 */
    static int s_baud_choice = -1;  /* -1 表示首次进入，按 g_default_baud 推断 */
    if (s_baud_choice < 0) {
        s_baud_choice = (g_default_baud == 115200) ? 1 : 0;
    }
    if (ImGui::RadioButton("9600", s_baud_choice == 0)) {
        s_baud_choice = 0;
        g_default_baud = 9600;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("115200", s_baud_choice == 1)) {
        s_baud_choice = 1;
        g_default_baud = 115200;
    }

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
