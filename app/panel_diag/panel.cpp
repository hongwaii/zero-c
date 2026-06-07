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
#include "diag_ping.h"
#include "diag_ssl.h"
#include "diag_sms.h"
#include "diag_log.h"
#include "diag_health.h"
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

/* 推一行到 AT 控制台历史（环形 100 行）——先于回调们定义，因回调里要调 */
static void hist_push(const char *line)
{
    if (!line) return;
    int idx = (g_hist.head + g_hist.count) % 100;
    strncpy(g_hist.lines[idx], line, 255);
    g_hist.lines[idx][255] = '\0';
    if (g_hist.count < 100) g_hist.count++;
    else g_hist.head = (g_hist.head + 1) % 100;
}

/* === P4 新增：网络探活 + 5 按钮结果缓存 ===
 * 全部 static——panel 私有；多 panel 时再考虑挪到 diag_state。 */

/* TCP/UDP ping 结果 */
static char  g_ping_host[64] = "1.2.3.4";
static int   g_ping_port    = 80;
static int   g_ping_last_ms = -1;
static bool  g_ping_last_ok = false;

/* SSL 探活结果 */
static char  g_ssl_url[256]        = "https://www.baidu.com";
static int   g_ssl_last_status     = 0;
static char  g_ssl_last_issuer[128] = "";
static char  g_ssl_last_expiry[32]  = "";

/* SMS 输入 + 结果 */
static char  g_sms_number[32]      = "10086";
static char  g_sms_text[128]       = "test from modem agent";
static char  g_sms_last_status[64] = "";

/* 抓 log / 健康检查输出路径 */
static char  g_log_path[512]       = "logs/capture.zip";
static char  g_health_path[512]    = "logs/health.json";
static bool  g_capturing           = false;
static bool  g_health_running      = false;

/* 回调们——把 diag_* async 结果写回全局缓存，UI 下一帧渲染 */
/* ping 完成回调：ok=true 网络通；ms 是 RTT 毫秒 */
static void on_ping_done(bool ok, int ms, void *ud)
{
    (void)ud;
    g_ping_last_ok = ok;
    g_ping_last_ms = ms;
}

/* SSL 探活完成回调：把 issuer/expiry 拷到 panel 静态缓冲 */
static void on_ssl_done(const diag_ssl_result_t *r, void *ud)
{
    (void)ud;
    if (!r) return;
    g_ssl_last_status = r->http_status;
    strncpy(g_ssl_last_issuer, r->issuer, sizeof(g_ssl_last_issuer) - 1);
    g_ssl_last_issuer[sizeof(g_ssl_last_issuer) - 1] = '\0';
    strncpy(g_ssl_last_expiry, r->expiry, sizeof(g_ssl_last_expiry) - 1);
    g_ssl_last_expiry[sizeof(g_ssl_last_expiry) - 1] = '\0';
}

/* SMS 发送完成回调：把 OK / ERROR 字符串填到 UI 状态行 */
static void on_sms_done(bool ok, const char *status, void *ud)
{
    (void)ud;
    if (status) {
        strncpy(g_sms_last_status, status, sizeof(g_sms_last_status) - 1);
        g_sms_last_status[sizeof(g_sms_last_status) - 1] = '\0';
    } else {
        snprintf(g_sms_last_status, sizeof(g_sms_last_status),
                 ok ? "OK" : "ERROR");
    }
}

/* 抓 log 完成回调：仅复位"抓取中"标记；路径已在 g_log_path 写死 */
static void on_log_done(bool ok, const char *zip_path, size_t zip_size, void *ud)
{
    (void)zip_size; (void)ud;
    g_capturing = false;
    if (ok && zip_path) {
        /* 把 zip 路径再 echo 一次进 AT 控制台历史，便于复制 */
        char buf[600];
        snprintf(buf, sizeof(buf), "< log capture saved: %s", zip_path);
        hist_push(buf);
    } else {
        hist_push("< log capture FAILED");
    }
}

/* 健康检查完成回调：把报告路径 echo 到控制台历史 */
static void on_health_done(bool ok, const char *json_path, void *ud)
{
    (void)ud;
    g_health_running = false;
    if (json_path) {
        char buf[600];
        snprintf(buf, sizeof(buf),
                 ok ? "< health report saved: %s" : "< health report FAILED: %s",
                 json_path);
        hist_push(buf);
    }
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

    /* 输入框 + 发送：支持回车直接发（ImGuiInputTextFlags_EnterReturnsTrue） */
    static char input_buf[128] = "";
    /* 抽出 do_send_at lambda——回车和按钮共用同一段发命令逻辑，避免代码重复 */
    auto do_send_at = [&]() {
        if (input_buf[0] != '\0') {
            char echo[160];
            std::snprintf(echo, sizeof(echo), "> %s", input_buf);
            hist_push(echo);
            at_session_t *at = (at_session_t *)app->active_at;
            if (at) {
                at_session_send(at, input_buf, 3000, on_at_done, NULL);
            } else {
                hist_push("(无连接：先在多模组面板点连接)");
            }
            input_buf[0] = '\0';
        }
    };
    if (ImGui::InputTextWithHint("##at_in", i18n_get("diag.console.placeholder"),
                                 input_buf, sizeof(input_buf),
                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
        do_send_at();
    }
    ImGui::SameLine();
    if (ImGui::Button(i18n_get("diag.console.send"))) {
        do_send_at();
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

    /* 找到 READY 设备就**永远**渲染 7 张卡——空字段显示 "-"——
     * 之前 st->valid 永远 false（refresh_now 异步且 strbuf 复用），导致连上
     * COM 口后 UI 仍显示"未连接"，去掉 valid 检查。 */
    if (!st) {
        /* 真的没找到 READY 设备 */
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
        if (st->last_update[0] != '\0') {
            ImGui::TextDisabled("上次刷新：%s", st->last_update);
        }
    }
    ImGui::Separator();

    /* === P4: 5 个动作按钮（接真函数，保留 P3 行为） === */
    at_session_t *at = (at_session_t *)app->active_at;

    /* 拨号测试 → ATD10086; */
    if (ImGui::Button(i18n_get("diag.actions.dial"))) {
        if (at) at_session_send(at, "ATD10086;", 10000, NULL, NULL);
        else hist_push("(无连接：先在多模组面板点连接)");
    }
    ImGui::SameLine();
    /* 断开 → ATH */
    if (ImGui::Button(i18n_get("diag.actions.hangup"))) {
        if (at) at_session_send(at, "ATH", 3000, NULL, NULL);
        else hist_push("(无连接：先在多模组面板点连接)");
    }
    ImGui::SameLine();
    /* 抓 log 30s → 触发 diag_capture_log 异步；UI 显示"抓 log 中…"
     * 注：at_session 期间 at_session 自身 cmd 收发停摆（设计意图）。 */
    if (g_capturing) {
        ImGui::Button(i18n_get("diag.actions.capturing"));  /* 灰色禁用提示 */
    } else if (ImGui::Button(i18n_get("diag.actions.log30"))) {
        if (at) {
            g_capturing = true;
            int rc = diag_capture_log(at, 30, g_log_path, on_log_done, NULL);
            if (rc != 0) {
                g_capturing = false;
                hist_push("< diag_capture_log 启动失败");
            }
        } else {
            hist_push("(无连接：先在多模组面板点连接)");
        }
    }

    /* 发短信模板：号码 + 文本 → diag_send_sms */
    ImGui::InputTextWithHint("##sms_num", i18n_get("diag.actions.sms.number"),
                             g_sms_number, sizeof(g_sms_number));
    ImGui::SameLine();
    if (ImGui::Button(i18n_get("diag.actions.sms"))) {
        if (at) {
            diag_send_sms(at, g_sms_number, g_sms_text, 3000, on_sms_done, NULL);
        } else {
            hist_push("(无连接：先在多模组面板点连接)");
        }
    }
    ImGui::InputTextWithHint("##sms_txt", i18n_get("diag.actions.sms.text"),
                             g_sms_text, sizeof(g_sms_text));
    if (g_sms_last_status[0] != '\0') {
        ImGui::SameLine();
        ImGui::TextDisabled("status: %s", g_sms_last_status);
    }

    /* 刷新诊断（P3 行为保留）+ 一键健康检查（P4 新增） */
    if (ImGui::Button(i18n_get("diag.actions.health"))) {
        if (g_health_running) {
            /* 已在跑——忽略；UI 提示 */
        } else if (at && app->diag_service && app->device_manager) {
            /* dev_idx：从 active_at 反查 device_manager */
            device_manager_t *m = (device_manager_t *)app->device_manager;
            int dev_idx = 0;
            for (int i = 0; i < m->dev_count; i++) {
                if (m->devs[i].at == at) { dev_idx = i; break; }
            }
            g_health_running = true;
            int rc = diag_health_check(app->uv_loop, app->device_manager, dev_idx,
                                       g_health_path, on_health_done, NULL);
            if (rc != 0) {
                g_health_running = false;
                hist_push("< diag_health_check 启动失败");
            }
        } else {
            /* 无活动设备——只走 P3 行为：触发单次 AT 刷新 */
            if (app->diag_service) {
                diag_service_refresh_now(app->diag_service);
            } else {
                hist_push("(无连接：先在多模组面板点连接)");
            }
        }
    }
    if (g_health_running) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", i18n_get("diag.actions.health_running"));
    }

    ImGui::Separator();

    /* === P4: 网络探活区 === */
    ImGui::Text("%s", i18n_get("diag.network.title"));

    /* TCP/UDP ping：主机 + 端口 + 两个按钮 + 上次结果 */
    ImGui::InputText(i18n_get("diag.network.host"), g_ping_host, sizeof(g_ping_host));
    ImGui::SameLine();
    ImGui::InputInt(i18n_get("diag.network.port"), &g_ping_port);
    if (ImGui::Button(i18n_get("diag.network.ping_tcp"))) {
        if (app->uv_loop) {
            diag_ping_tcp(app->uv_loop, g_ping_host, g_ping_port,
                          5000, on_ping_done, NULL);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(i18n_get("diag.network.ping_udp"))) {
        if (app->uv_loop) {
            diag_ping_udp(app->uv_loop, g_ping_host, g_ping_port,
                          5000, on_ping_done, NULL);
        }
    }
    ImGui::SameLine();
    if (g_ping_last_ms < 0) {
        ImGui::TextDisabled("(- ms)");
    } else {
        ImGui::TextColored(
            g_ping_last_ok ? ImVec4(0.4f, 0.85f, 1.0f, 1.0f) : ImVec4(1.0f, 0.5f, 0.5f, 1.0f),
            "%s %dms", g_ping_last_ok ? "OK" : "FAIL", g_ping_last_ms);
    }

    /* SSL 探活：URL 输入 + 按钮 + 证书信息展示 */
    ImGui::InputText(i18n_get("diag.network.ssl_url"), g_ssl_url, sizeof(g_ssl_url));
    ImGui::SameLine();
    if (ImGui::Button(i18n_get("diag.network.ssl"))) {
        diag_ssl_probe(g_ssl_url, 5000, on_ssl_done, NULL);
    }
    if (g_ssl_last_status > 0) {
        ImGui::TextWrapped("status=%d issuer='%s' expiry='%s'",
                           g_ssl_last_status, g_ssl_last_issuer, g_ssl_last_expiry);
    }

    ImGui::EndChild();

    ImGui::Columns(1);
}
