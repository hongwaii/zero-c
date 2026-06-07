/**
 * @file panel_devices.cpp
 * @brief 多模组列表 panel——订阅 device_manager 实时设备列表。
 *
 * 每帧从 app->device_manager 读 devs[] 渲染表格；dev 数变化时 ImGui 自动重绘。
 */
#include "panel_devices.h"
#include "i18n.h"
#include "imgui.h"
#include "device_manager.h"
#include <cstring>

/* 6 列的 i18n key */
static const char *kColI18n[] = {
    "devices.col.name",
    "devices.col.com",
    "devices.col.ip",
    "devices.col.csq",
    "devices.col.state",
    "devices.col.lastseen",
};

/**
 * @brief 渲染多模组列表：标题 + 6 列表格（数据来自 device_manager）。
 */
void panel_devices_render(agent_app_t *app)
{
    ImGui::Text("%s", i18n_get("devices.title"));
    ImGui::Separator();

    device_manager_t *m = (device_manager_t *)app->device_manager;
    int count = (m != NULL) ? m->dev_count : 0;

    if (count == 0) {
        ImGui::TextDisabled("暂未发现模组——插上 COM 或 USB-NCM 模组等待 2 秒");
        return;
    }

    if (ImGui::BeginTable("devices_tbl", 6,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        for (int i = 0; i < 6; i++) {
            ImGui::TableSetupColumn(i18n_get(kColI18n[i]));
        }
        ImGui::TableHeadersRow();

        for (int r = 0; r < count; r++) {
            const modem_dev_t *d = &m->devs[r];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("%s", d->label);
            ImGui::TableSetColumnIndex(1); ImGui::Text("%s", d->chan_uri);
            ImGui::TableSetColumnIndex(2); ImGui::Text("%s", d->ipv4[0] ? d->ipv4 : "-");
            ImGui::TableSetColumnIndex(3); ImGui::Text("%d", d->csq);
            ImGui::TableSetColumnIndex(4);
            const char *state_str = (d->state == DEV_STATE_READY) ? "READY"
                                    : (d->state == DEV_STATE_ERROR) ? "ERROR" : "DISCONNECTED";
            ImGui::Text("%s", state_str);
            ImGui::TableSetColumnIndex(5); ImGui::Text("-");
        }
        ImGui::EndTable();
    }
}
