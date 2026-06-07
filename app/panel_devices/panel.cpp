/**
 * @file panel_devices.cpp
 * @brief 多模组列表 panel——每行带"连接/断开"按钮。
 */
#include "panel_devices.h"
#include "i18n.h"
#include "imgui.h"
/* device_manager.h / agent_types.h 是 C 头；本文件是 C++，需 extern "C"
 * 包裹避免符号 mangling——否则 device_manager_connect_dev 等函数会按 C++
 * 规则 mangle，链接时找不到 C 端符号。 */
extern "C" {
#include "device_manager.h"
#include "agent_types.h"
}
#include <cstdio>
#include <cstring>

/* 6 列的 i18n key（与 zh.json 对应） */
static const char *kColI18n[] = {
    "devices.col.name",
    "devices.col.com",
    "devices.col.ip",
    "devices.col.csq",
    "devices.col.state",
    "devices.col.lastseen",
};

/**
 * @brief 渲染多模组列表：标题 + 7 列表格（第 7 列为连接/断开按钮）。
 *
 * 数据来自 device_manager；按钮按 dev 状态切换：READY 显示"断开"，
 * ERROR 显示"重试"，DISCONNECTED 显示"连接"。点按调用
 * device_manager_connect_dev / disconnect_dev，状态变化由 device_manager
 * 自己推到 m->devs[]，下帧重绘即生效。错误码用 agent_errstr() 翻译。
 */
void panel_devices_render(agent_app_t *app)
{
    ImGui::Text("%s", i18n_get("devices.title"));
    ImGui::Separator();

    device_manager_t *m = (device_manager_t *)app->device_manager;
    int count = (m != NULL) ? m->dev_count : 0;

    if (count == 0) {
        ImGui::TextDisabled("暂未发现模组——插上 COM 或 USB-NCM 模组等待 0.5 秒");
        return;
    }

    /* 7 列：6 个原列 + "操作"列 */
    if (ImGui::BeginTable("devices_tbl", 7,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        for (int i = 0; i < 6; i++) {
            ImGui::TableSetupColumn(i18n_get(kColI18n[i]));
        }
        ImGui::TableSetupColumn("操作");
        ImGui::TableHeadersRow();

        for (int r = 0; r < count; r++) {
            modem_dev_t *d = &m->devs[r];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("%s", d->label);
            ImGui::TableSetColumnIndex(1); ImGui::Text("%s", d->chan_uri);
            ImGui::TableSetColumnIndex(2); ImGui::Text("%s", d->ipv4[0] ? d->ipv4 : "-");
            ImGui::TableSetColumnIndex(3); ImGui::Text("%d", d->csq);
            ImGui::TableSetColumnIndex(4);
            const char *state_str = (d->state == DEV_STATE_READY) ? "READY"
                                    : (d->state == DEV_STATE_ERROR) ? "ERROR" : "DISCONNECTED";
            ImGui::Text("%s", state_str);
            ImGui::TableSetColumnIndex(5); ImGui::Text("%s", d->diag_last_update[0] ? d->diag_last_update : "-");

            /* 第 7 列：连接/断开按钮——按 dev 状态切换；ID 用 r*100+7
             * 避免与 panel_diag / panel_settings 里的 widget ID 冲突 */
            ImGui::TableSetColumnIndex(6);
            ImGui::PushID(r * 100 + 7);
            if (d->state == DEV_STATE_READY) {
                if (ImGui::Button("断开")) {
                    device_manager_disconnect_dev(m, r);
                }
            } else {
                if (ImGui::Button(d->state == DEV_STATE_ERROR ? "重试" : "连接")) {
                    int rc = device_manager_connect_dev(m, r);
                    if (rc != 0) {
                        fprintf(stderr, "panel_devices: connect dev %d failed rc=%d (%s)\n",
                                r, rc, agent_errstr(rc));
                    }
                }
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}
