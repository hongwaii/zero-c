/**
 * @file theme.cpp
 * @brief 工程蓝主题（深色高对比）+ CJK 字体加载。
 *
 * CJK 字体路径固定 `assets/fonts/cn.otf`。如果文件不存在则降级到 ImGui 默认
 * 字体（仅 ASCII 可显示），不报错——首屏必须有字可看。
 */
#include "theme.h"
#include "imgui.h"
#include <cstdio>

/**
 * @brief 设定全局 StyleColorsDark 后逐项覆盖为目标主题色。
 */
void theme_apply(agent_theme_t theme)
{
    ImGui::StyleColorsDark();
    ImGuiStyle &s = ImGui::GetStyle();
    s.WindowRounding = 4.0f;
    s.FrameRounding = 3.0f;
    s.GrabRounding = 3.0f;
    s.ScrollbarSize = 12.0f;
    s.FramePadding = ImVec2(8, 4);

    ImVec4 *c = s.Colors;
    if (theme == AGENT_THEME_ENGINEERING_BLUE) {
        /* 工程蓝：深底高对比，标题与强调用蓝调 */
        c[ImGuiCol_WindowBg]       = ImVec4(0.06f, 0.07f, 0.09f, 1.00f);
        c[ImGuiCol_ChildBg]        = ImVec4(0.08f, 0.09f, 0.12f, 1.00f);
        c[ImGuiCol_PopupBg]        = ImVec4(0.08f, 0.09f, 0.12f, 1.00f);
        c[ImGuiCol_Border]         = ImVec4(0.20f, 0.24f, 0.32f, 1.00f);
        c[ImGuiCol_FrameBg]        = ImVec4(0.10f, 0.13f, 0.18f, 1.00f);
        c[ImGuiCol_FrameBgHovered] = ImVec4(0.14f, 0.18f, 0.25f, 1.00f);
        c[ImGuiCol_FrameBgActive]  = ImVec4(0.18f, 0.24f, 0.34f, 1.00f);
        c[ImGuiCol_TitleBg]        = ImVec4(0.06f, 0.09f, 0.16f, 1.00f);
        c[ImGuiCol_TitleBgActive]  = ImVec4(0.06f, 0.13f, 0.24f, 1.00f);
        c[ImGuiCol_MenuBarBg]      = ImVec4(0.06f, 0.09f, 0.16f, 1.00f);
        c[ImGuiCol_Header]         = ImVec4(0.16f, 0.30f, 0.50f, 0.80f);
        c[ImGuiCol_HeaderHovered]  = ImVec4(0.20f, 0.38f, 0.62f, 0.80f);
        c[ImGuiCol_HeaderActive]   = ImVec4(0.24f, 0.46f, 0.74f, 1.00f);
        c[ImGuiCol_Button]         = ImVec4(0.16f, 0.30f, 0.50f, 1.00f);
        c[ImGuiCol_ButtonHovered]  = ImVec4(0.22f, 0.40f, 0.66f, 1.00f);
        c[ImGuiCol_ButtonActive]   = ImVec4(0.10f, 0.22f, 0.40f, 1.00f);
        c[ImGuiCol_CheckMark]      = ImVec4(0.40f, 0.80f, 1.00f, 1.00f);
        c[ImGuiCol_Text]           = ImVec4(0.92f, 0.94f, 0.98f, 1.00f);
        c[ImGuiCol_TextDisabled]   = ImVec4(0.50f, 0.55f, 0.65f, 1.00f);
    }
    /* 浅色主题 (AGENT_THEME_LIGHT) 留到 v1.1，本期仅支持工程蓝。 */
}

/**
 * @brief 加载默认字体；如果 assets/fonts/cn.otf 存在则追加 CJK 子集。
 *
 * 路径相对于 cwd，因此 ./build.bat (cwd=仓库根) 能解析。
 * 字体缺失时仅打印提示，不返回错误——首屏仍可用默认 ASCII 字体。
 */
int theme_load_fonts(void)
{
    ImGuiIO &io = ImGui::GetIO();
    /* 第一步：始终注册默认字体，避免第一帧渲染时 fonts atlas 为空。 */
    io.Fonts->AddFontDefault();

    /* 第二步：尝试加载 CJK 字体（思源黑体 / 其它含 CJK 字符的 TTF/OTF）。 */
    FILE *f = fopen("assets/fonts/cn.otf", "rb");
    if (!f) {
        /* 字体缺失非致命——ASCII 仍可显示，中文会显示为方块。 */
        fprintf(stderr, "theme_load_fonts: 未找到 assets/fonts/cn.otf，"
                        "中文会显示为方块（见 assets/fonts/README.md）。\n");
        return 0;
    }
    fclose(f);

    ImFontConfig cfg;
    cfg.OversampleH = 2;
    cfg.OversampleV = 1;
    io.Fonts->AddFontFromFileTTF("assets/fonts/cn.otf", 16.0f, &cfg,
        io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    return 0;
}
