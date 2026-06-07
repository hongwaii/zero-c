/**
 * @file panel_diag.cpp
 * @brief 现场诊断 panel：7 张状态卡实时订阅 diag_state + AT 控制台走真通道。
 *
 * 数据流：
 *   1. 每帧扫描 device_manager 找第一个 READY 设备
 *   2. 用它的 at_session 拉过的 diag_state 渲染 7 张卡
 *   3. AT 控制台输入 → at_session.send → on_at_done 把响应 push 到历史
 *
 * P3 简化：同时只支持一个活动设备（active_at）；P3.5 再做"选设备看诊断"。
 */
#include "panel_diag.h"
#include "i18n.h"
#include "imgui.h"
#include "agent_types.h"
/* C 头必须 extern "C" 包裹（C++ TU 里 C 头不要包） */
extern "C" {
#include "diag_state.h"
#include "diag_service.h"
#include "device_manager.h"
#include "at_session.h"
}

#include <cstring>
#include <cstdio>
#include <ctime>

/* 7 张状态卡：i18n key + diag_state 字段偏移 */
typedef struct {
    const char  *i18n_key;
    size_t       offset;   /* offsetof(diag_state_t, field) */
} diag_card_t;

#define DIAG_FIELD(name) offsetof(diag_state_t, name)

static const diag_card_t kDiagCards[] = {
    { "diag.cards.csq",      DIAG_FIELD(csq) },
    { "diag.cards.cereg",    DIAG_FIELD(cereg) },
    { "diag.cards.operator", DIAG_FIELD(cop_operator) },
    { "diag.cards.rat",      DIAG_FIELD(rat) },
    { "diag.cards.imei",     DIAG_FIELD(imei) },
    { "diag.cards.imsi",     DIAG_FIELD(imsi) },
    { "diag.cards.iccid",    DIAG_FIELD(iccid) },
};

/* AT 控制台历史（环形 100 行） */
typedef struct {
    char lines[100][256];
    int  head;
    int  count;
} console_history_t;

static console_history_t g_hist = {0};

static void hist_push(const char *line)
{
    if (!line) return;
    int idx = (g_hist.head + g_hist.count) % 100;
    strncpy(g_hist.lines[idx], line, 255);
    g_hist.lines[idx][255] = '\0';
    if (g_hist.count < 100) g_hist.count++;
    else g_hist.head = (g_hist.head + 1) % 100;
}

/* AT 命令完成回调：把 result 推入历史 */
static void on_at_done(void *ud, const char *res, size_t len, bool ok)
{
    (void)ud;
    if (ok) {
        if (len > 0) {
            char buf[512];
            snprintf(buf, sizeof(buf), "< %s", res);
            hist_push(buf);
        }
        hist_push("< OK");
    } else {
        hist_push("< ERROR");
    }
}

void panel_diag_render(agent_app_t *app)
{
    ImGui::Columns(2, NULL, true);

    /* ---- 左半：AT 控制台 ---- */
    ImGui::BeginChild("at_console", ImVec2(0, 0), true);
    ImGui::Text("%s", i18n_get("diag.console.title"));
    ImGui::Separator();

    /* 历史区（环形 100 行） */
    ImGui::BeginChild("at_log", ImVec2(0, -32), true);
    for (int i = 0; i < g_hist.count; i++) {
        int idx = (g_hist.head + i) % 100;
        ImGui::Text("%s", g_hist.lines[idx]);
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();

    /* 输入框 + 发送 */
    static char input_buf[128] = "";
    ImGui::InputTextWithHint("##at_in", i18n_get("diag.console.placeholder"),
                             input_buf, sizeof(input_buf));
    ImGui::SameLine();
    if (ImGui::Button(i18n_get("diag.console.send"))) {
        if (input_buf[0] != '\0') {
            char echo[160];
            snprintf(echo, sizeof(echo), "> %s", input_buf);
            hist_push(echo);
            at_session_t *at = (at_session_t *)app->active_at;
            if (at) {
                at_session_send(at, input_buf, 3000, on_at_done, NULL);
            } else {
                hist_push("(无连接：先在多模组面板点连接)");
            }
            input_buf[0] = '\0';
        }
    }
    ImGui::EndChild();

    ImGui::NextColumn();

    /* ---- 右半：状态卡 + 动作按钮 ---- */
    ImGui::BeginChild("status_cards", ImVec2(0, 0), true);

    /* 找第一个 READY 设备，更新 app->active_at */
    diag_state_t *st = NULL;
    if (app->diag_service && app->device_manager) {
        device_manager_t *m = (device_manager_t *)app->device_manager;
        app->active_at = NULL;
        for (int i = 0; i < m->dev_count; i++) {
            if (m->devs[i].state == DEV_STATE_READY) {
                st = (diag_state_t *)diag_service_get_state(app->diag_service, i);
                app->active_at = m->devs[i].at;
                break;
            }
        }
    }

    if (!st || !st->valid) {
        ImGui::TextDisabled("未连接模组（先在多模组面板点连接）");
    } else {
        for (size_t i = 0; i < sizeof(kDiagCards) / sizeof(kDiagCards[0]); i++) {
            const char *label = i18n_get(kDiagCards[i].i18n_key);
            const char *value = (const char *)((char *)st + kDiagCards[i].offset);
            ImGui::Text("%s", label);
            ImGui::SameLine(160);
            if (value[0] == '\0') {
                ImGui::TextDisabled("-");
            } else {
                ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1.0f), "%s", value);
            }
        }
        ImGui::TextDisabled("上次刷新：%s", st->last_update);
    }
    ImGui::Separator();

    /* 动作按钮：P3 仅"刷新诊断"——拨号/抓 log 留给 P4 */
    if (ImGui::Button(i18n_get("diag.actions.health"))) {
        if (app->diag_service) {
            diag_service_refresh_now(app->diag_service);
        }
    }

    ImGui::EndChild();

    ImGui::Columns(1);
}
