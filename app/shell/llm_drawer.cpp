/**
 * @file llm_drawer.cpp
 * @brief LLM 抽屉的 mock 实现：P1 仅展示 + 假流式回复，P5 接真模型。
 */
#include "llm_drawer.h"
#include "i18n.h"
#include "imgui.h"
#include <cstdio>
#include <cstring>

/**
 * @brief 抽屉渲染：标题 + disclaimer + 假对话区 + 输入框 + 发送按钮。
 *
 * 假流式效果：点发送后用 snprintf 生成一条 mock 回复，append 到对话区。
 * 真实接入在 P5（libcurl 调 OpenAI 兼容 + SSE 流式）。
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
    ImGui::InputTextWithHint("##llm_in", i18n_get("llm.drawer.placeholder"),
                              input, sizeof(input));
    ImGui::SameLine();
    if (ImGui::Button(i18n_get("llm.drawer.send")) && input[0] != '\0') {
        /* 假流式：把用户输入 + 一段固定回话塞进 output。 */
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
