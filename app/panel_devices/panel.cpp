/**
 * @file panel_devices.cpp
 * @brief 多模组列表 panel：6 列表格 + 3 行 mock 模组数据。
 *
 * P1 期间数据硬编码；P3 接真 device_manager 后这里订阅多模组状态。
 */
#include "panel_devices.h"
#include "i18n.h"
#include "imgui.h"

/* 6 列的 i18n key 列表 */
static const char *kColI18n[] = {
    "devices.col.name",
    "devices.col.com",
    "devices.col.ip",
    "devices.col.csq",
    "devices.col.state",
    "devices.col.lastseen",
};

/* 3 行 mock 模组 */
static const char *kRows[3][6] = {
    { "MDM-001 (Lab)",     "COM5",     "10.0.0.12",  "23",  "READY",        "2 秒前" },
    { "MDM-002 (Field-A)", "COM7",     "-",          "11",  "DISCONNECTED", "5 分钟前" },
    { "MDM-003 (Field-B)", "rndis://", "10.42.0.7",  "19",  "READY",        "刚刚" },
};

/**
 * @brief 渲染多模组列表：标题 + 6 列表格。
 */
void panel_devices_render(agent_app_t *app)
{
    (void)app;
    ImGui::Text("%s", i18n_get("devices.title"));
    ImGui::Separator();
    if (ImGui::BeginTable("devices_tbl", 6,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        /* 表头 */
        for (int i = 0; i < 6; i++) {
            ImGui::TableSetupColumn(i18n_get(kColI18n[i]));
        }
        ImGui::TableHeadersRow();
        /* 3 行数据 */
        for (int r = 0; r < 3; r++) {
            ImGui::TableNextRow();
            for (int c = 0; c < 6; c++) {
                ImGui::TableSetColumnIndex(c);
                ImGui::Text("%s", kRows[r][c]);
            }
        }
        ImGui::EndTable();
    }
}
