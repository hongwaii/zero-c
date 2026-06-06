/**
 * @file llm_drawer.cpp
 * @brief 右侧 LLM 抽屉：disclaimer + 假对话区 + 输入框 + provider 状态摘要。
 *
 * P1：纯 mock 流式回复；P5：libcurl 调 OpenAI 兼容 + SSE 流式。
 */
#include "llm_drawer.h"
#include "i18n.h"
#include "imgui.h"
#include <cstdio>
#include <cstring>

/**
 * @brief 统计已配置的 LLM provider 数量（api_key 非空）。
 */
static int count_configured_providers(agent_app_t *app)
{
    int n = 0;
    for (agent_llm_provider_t *p = app->providers; p; p = p->next) {
        if (p->api_key[0] != '\0') n++;
    }
    return n;
}

/**
 * @brief 渲染抽屉：disclaimer + 当前 provider 数 + 对话区 + 输入框 + 发送按钮。
 */
void llm_drawer_render(agent_app_t *app)
{
    static char input[256] = "";
    static char output[4096] = "";
    if (output[0] == '\0') {
        std::snprintf(output, sizeof(output), "%s", i18n_get("llm.drawer.disclaimer"));
    }

    ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_FirstUseEver);
    ImGui::Begin(i18n_get("llm.drawer.title"), &app->llm_drawer_open);

    ImGui::TextWrapped("%s", output);
    ImGui::Separator();

    /* provider 状态摘要：5 个里填了几个 key */
    int total = 0, configured = 0;
    for (agent_llm_provider_t *p = app->providers; p; p = p->next) total++;
    configured = count_configured_providers(app);
    ImGui::Text("LLM providers: %d / %d configured", configured, total);

    ImGui::BeginChild("llm_out", ImVec2(0, -64), true);
    ImGui::TextWrapped("%s", output);
    ImGui::EndChild();

    ImGui::InputTextWithHint("##llm_in", i18n_get("llm.drawer.placeholder"),
                              input, sizeof(input));
    ImGui::SameLine();
    if (ImGui::Button(i18n_get("llm.drawer.send")) && input[0] != '\0') {
        char buf[4096];
        std::snprintf(buf, sizeof(buf),
            "%s\n\nUser: %s\nAI: (mock) 已收到你的问题：%s\n",
            output, input, input);
        std::strncpy(output, buf, sizeof(output) - 1);
        output[sizeof(output) - 1] = '\0';
        input[0] = '\0';
    }
    ImGui::End();
}
