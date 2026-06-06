/**
 * @file panel_diag.cpp
 * @brief 现场诊断 panel（mock）：左 AT 控制台 + 右状态卡 + 底部动作按钮。
 *
 * 本 task 是 mock 实现，所有数据硬编码；P3 接真 AT 引擎后这里订阅 diag state。
 * 布局：ImGui::Columns(2) 拆左右两半。
 */
#include "panel_diag.h"
#include "i18n.h"
#include "imgui.h"

/* 状态卡数据：i18n key → 假显示值。P3 替换为 diag_state 订阅。 */
typedef struct {
    const char *i18n_key;     /* "diag.cards.csq" 等 */
    const char *mock_value;   /* mock 字符串 */
} diag_card_t;

static const diag_card_t kDiagCards[] = {
    { "diag.cards.csq",      "23 (-67 dBm)" },
    { "diag.cards.cereg",    "5 (Registered, roaming)" },
    { "diag.cards.operator", "China Mobile" },
    { "diag.cards.rat",      "LTE Cat-1" },
    { "diag.cards.imei",     "864400060123456" },
    { "diag.cards.imsi",     "460001234567890" },
    { "diag.cards.iccid",    "89860117851234567890" },
};

/**
 * @brief 渲染 panel_diag 主体。两列布局：左 AT 控制台，右状态卡。
 * @param app 全局 app context（本 mock 不使用，保留接口与未来真实现一致）。
 */
void panel_diag_render(agent_app_t *app)
{
    (void)app;

    ImGui::Columns(2, NULL, true);

    /* ---- 左半：AT 控制台 ---- */
    ImGui::BeginChild("at_console", ImVec2(0, 0), true);
    ImGui::Text("%s", i18n_get("diag.console.title"));
    ImGui::Separator();

    /* 历史区：可滚动，P3 接真引擎后由 at_log 推送 */
    ImGui::BeginChild("at_log", ImVec2(0, -32), true);
    ImGui::Text("AT+CSQ\n+CSQ: 23,99\n\nOK\nAT+COPS?\n+COPS: 0,0,\"China Mobile\",7\n\nOK");
    ImGui::EndChild();

    /* 输入框 + 发送按钮 */
    static char input_buf[128] = "";
    ImGui::InputTextWithHint("##at_in", i18n_get("diag.console.placeholder"),
                             input_buf, sizeof(input_buf));
    ImGui::SameLine();
    if (ImGui::Button(i18n_get("diag.console.send"))) {
        /* mock：点发送后清空输入框；P3 接真引擎时改为 at_session_send */
        input_buf[0] = '\0';
    }
    ImGui::EndChild();

    ImGui::NextColumn();

    /* ---- 右半：状态卡 + 动作按钮 ---- */
    ImGui::BeginChild("status_cards", ImVec2(0, 0), true);

    for (size_t i = 0; i < sizeof(kDiagCards) / sizeof(kDiagCards[0]); i++) {
        ImGui::Text("%s", i18n_get(kDiagCards[i].i18n_key));
        ImGui::SameLine(160);
        ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1.0f), "%s", kDiagCards[i].mock_value);
    }
    ImGui::Separator();

    /* 动作按钮：拨号、断开、抓 log 30s、发短信模板、一键健康检查 */
    if (ImGui::Button(i18n_get("diag.actions.dial")))    { /* P3 接真拨号 */ }
    ImGui::SameLine();
    if (ImGui::Button(i18n_get("diag.actions.hangup")))   { /* P3 接真断开 */ }
    ImGui::SameLine();
    if (ImGui::Button(i18n_get("diag.actions.log30")))    { /* P4 接真抓 log */ }
    ImGui::SameLine();
    if (ImGui::Button(i18n_get("diag.actions.sms")))      { /* P4 接真发短信 */ }
    if (ImGui::Button(i18n_get("diag.actions.health")))    { /* P4 接一键健康检查 */ }

    ImGui::EndChild();

    ImGui::Columns(1);
}
