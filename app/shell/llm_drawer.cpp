/**
 * @file llm_drawer.cpp
 * @brief 右侧 LLM 抽屉：disclaimer + provider 状态 + 流式对话区 + 输入框。
 *
 * P1-P4：纯 mock 流式回复。
 * P5 Task 7：接 lib_llm_client —— llm_chat_stream 真发 OpenAI 兼容 chat completion；
 *           token 实时追加到全局 g_response（worker 线程写），主循环 snapshot 复制渲染。
 */
#include "llm_drawer.h"
#include "llm_client.h"
#include "provider_config.h"
#include "i18n.h"
#include "imgui.h"
#include "agent_types.h"
#include <cstdio>
#include <cstring>
#include <string>

/* 全局当前响应累积（流式 token 不断追加，worker 线程写） */
static std::string g_response;

/* 全局 busy 标志 + 错误缓冲（on_done 写，main loop 读） */
static bool g_busy = false;
static char g_err[128] = "";

/* on_token 回调：worker 线程直接调 → 追加到 g_response */
static void on_token(const char *t, size_t n, void *ud)
{
    (void)ud;
    g_response.append(t, n);
}

/* on_done 回调：worker 线程直接调 → 标记完成 + 可选设置错误 */
static void on_done(bool ok, const char *err, void *ud)
{
    (void)ud;
    g_busy = false;
    if (!ok && err) {
        std::strncpy(g_err, err, sizeof(g_err) - 1);
        g_err[sizeof(g_err) - 1] = '\0';
    } else {
        g_err[0] = '\0';
    }
}

/* 选第一个有 api_key 的 provider */
static const agent_llm_provider_t *pick_provider(agent_app_t *app)
{
    for (const agent_llm_provider_t *p = app->providers; p; p = p->next) {
        if (p->api_key[0] != '\0') return p;
    }
    return NULL;
}

/* 统计已配置的 LLM provider 数量（api_key 非空） */
static int count_configured_providers(agent_app_t *app)
{
    int n = 0;
    for (const agent_llm_provider_t *p = app->providers; p; p = p->next) {
        if (p->api_key[0] != '\0') n++;
    }
    return n;
}

/**
 * @brief 渲染抽屉：disclaimer + provider 状态 + 对话区 + 输入框 + 发送按钮。
 */
void llm_drawer_render(agent_app_t *app)
{
    static char input[256] = "";
    static std::string history;  /* 累积 user + ai 轮次（main loop 单线程写） */

    ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_FirstUseEver);
    ImGui::Begin(i18n_get("llm.drawer.title"), &app->llm_drawer_open);

    ImGui::TextWrapped("%s", i18n_get("llm.drawer.disclaimer"));
    ImGui::Separator();

    /* provider 状态摘要 */
    int total = 0, configured = 0;
    for (const agent_llm_provider_t *p = app->providers; p; p = p->next) total++;
    configured = count_configured_providers(app);
    ImGui::Text("LLM providers: %d / %d configured", configured, total);

    ImGui::BeginChild("llm_out", ImVec2(0, -64), true);
    ImGui::TextWrapped("%s", history.c_str());
    /* 流式中：snapshot 复制 g_response（避免直接 c_str() 读到 worker 正在写的串） */
    if (g_busy && g_response.size() > 0) {
        static std::string snapshot;
        snapshot = g_response;
        ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1.0f), "%s", snapshot.c_str());
    } else if (!g_busy && g_response.size() > 0) {
        /* 完成态：把 g_response 拼到 history 然后清空（让下次提问从干净状态开始） */
        history += g_response;
        history += "\n";
        g_response.clear();
    }
    ImGui::EndChild();

    ImGui::InputTextWithHint("##llm_in", i18n_get("llm.drawer.placeholder"),
                              input, sizeof(input));
    ImGui::SameLine();
    if (ImGui::Button(i18n_get("llm.drawer.send")) && !g_busy) {
        if (input[0] != '\0') {
            const agent_llm_provider_t *p = pick_provider(app);
            if (!p) {
                std::snprintf(g_err, sizeof(g_err),
                              "no provider has api_key — set in settings");
            } else {
                char prefix[1024];
                std::snprintf(prefix, sizeof(prefix), "[user] %s\n[ai] ", input);
                history += prefix;
                g_response.clear();
                g_busy = true;
                g_err[0] = '\0';

                /* 拼 OpenAI 风格 messages JSON（v1.0 简化：只发当次 input） */
                char msgs[2048];
                std::snprintf(msgs, sizeof(msgs),
                              "[{\"role\":\"user\",\"content\":\"%s\"}]", input);
                llm_chat_stream(p, NULL, msgs, on_token, on_done, NULL);
                input[0] = '\0';
            }
        }
    }
    if (g_err[0]) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", g_err);
    }
    ImGui::End();
}
