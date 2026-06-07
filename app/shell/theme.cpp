/**
 * @file theme.cpp
 * @brief 工程蓝主题 + CJK 字体三级 fallback（文件路径 API，避开 OTF/CFF 解析坑）。
 *
 * 字体加载策略：
 *   1) assets/fonts/cn.otf（或 .ttf）—— 用户/未来 P6 打包自带
 *   2) C:\Windows\Fonts\ 下的已知中文字体文件路径
 *   3) ImGui 默认字体（中文显示为方块）
 *
 * 为什么走文件路径而不是 GetFontData：
 *   我们的 ImGui 1.92.9 没有 AddFontFromMemoryOTF，
 *   而 YaHei/SimSun 等 Windows 系统字体是 OTF/CFF 或 TTC。
 *   走 GetFontData + AddFontFromMemoryTTF 会拿到 CFF 数据让 TTF parser 静默失败，
 *   产生一个"0 glyph 的空壳"字体，把 io.FontDefault 设上去就全空白。
 *   文件路径 API 内部能自动检测 TTF/OTF/TTC 格式，最稳。
 *
 * TTC 文件用 cfg.FontNo 选择集合内的 face。
 */
#include "theme.h"
#include "imgui.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

/* theme_apply 完全保持原样 —— 略，见 commit 0c1c18e */

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
}

/* === CJK 字体：走文件路径，避开 OTF 解析坑 === */

/**
 * @brief 候选字体文件路径。TTC 文件 FontNo 指定集合内 face。
 * 按优先级排：第一项最优先。Windows 10/11 简体中文系统上 msyh.ttc 几乎肯定有。
 */
struct cjk_font_candidate {
    const char *path;       /* UTF-8 路径 */
    int         font_no;    /* TTC 内 face 索引；单 TTF/OTF 用 0 */
};

static const cjk_font_candidate kCjkFontCandidates[] = {
    /* 用户/未来 P6 打包字体 */
    { "assets/fonts/cn.otf",  0 },
    { "assets/fonts/cn.ttf",  0 },
    /* Win10/11 默认中文 UI 字体（TTC, 含 regular/bold/light） */
    { "C:/Windows/Fonts/msyh.ttc",     0 },  /* Microsoft YaHei Regular */
    { "C:/Windows/Fonts/msyhbd.ttc",   0 },  /* Microsoft YaHei Bold */
    { "C:/Windows/Fonts/msyhl.ttc",    0 },  /* Microsoft YaHei Light */
    /* 单文件 TrueType */
    { "C:/Windows/Fonts/simhei.ttf",   0 },  /* 黑体 */
    { "C:/Windows/Fonts/simsun.ttc",   0 },  /* 宋体（TTC） */
    { "C:/Windows/Fonts/simfang.ttf",  0 },  /* 仿宋 */
    { "C:/Windows/Fonts/Deng.ttf",     0 },  /* 等线 */
    { "C:/Windows/Fonts/Dengb.ttf",    0 },  /* 等线粗 */
    /* 用户主动装过的开源字体 */
    { "C:/Windows/Fonts/SourceHanSansSC-Regular.otf", 0 },
    { "C:/Windows/Fonts/NotoSansCJKsc-Regular.otf",   0 },
    { "C:/Windows/Fonts/NotoSansSC-Regular.otf",      0 },
};

static const int kCjkFontCandidateCount =
    sizeof(kCjkFontCandidates) / sizeof(kCjkFontCandidates[0]);

/**
 * @brief 检查文件是否存在。
 */
static bool file_exists(const char *path)
{
    DWORD attr = ::GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES) &&
           !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

/**
 * @brief 三级 fallback：用户字体 → 系统字体（文件路径 API）→ 默认。
 *
 * 关键：每级捕获 ImFont* 返回值，结尾显式设 io.FontDefault。
 * ImGui **不会**自动跨 Fonts[] 数组搜索 glyph——必须显式指定当前用哪个字体。
 *
 * @return 0 成功（含 fallback 情况）。
 */
int theme_load_fonts(void)
{
    ImGuiIO &io = ImGui::GetIO();

    /* 第一步：默认字体保底（保证 fonts atlas 非空，渲染管线不会崩）。 */
    ImFont *default_font = io.Fonts->AddFontDefault();
    ImFont *cjk_font = NULL;

    /* 遍历候选文件路径，找到第一个存在的、且 ImGui 成功加载的。 */
    for (int i = 0; i < kCjkFontCandidateCount; i++) {
        const char *path = kCjkFontCandidates[i].path;
        if (!file_exists(path)) {
            continue;  /* 文件不存在直接试下一个 */
        }

        ImFontConfig cfg;
        cfg.OversampleH = 2;
        cfg.OversampleV = 1;
        cfg.FontNo = kCjkFontCandidates[i].font_no;  /* TTC 用，TTF/OTF 忽略 */

        cjk_font = io.Fonts->AddFontFromFileTTF(
            path, 16.0f, &cfg,
            io.Fonts->GetGlyphRangesChineseSimplifiedCommon());

        if (cjk_font) {
            std::fprintf(stderr,
                "theme_load_fonts: 已加载 '%s' (FontNo=%d) 作为 CJK 字体。\n",
                path, cfg.FontNo);
            break;  /* 找到能用的就停 */
        }
        /* 加载失败（路径存在但解析失败）继续试下一个 */
    }

    /* 结尾：显式设 io.FontDefault——决定 ImGui 用哪个字体渲染所有文本。
     *  CJK 字体本身也含 ASCII glyph，所以英文/中文都正常。 */
    if (cjk_font) {
        io.FontDefault = cjk_font;
    } else {
        io.FontDefault = default_font;
        std::fprintf(stderr,
            "theme_load_fonts: 没找到任何可用 CJK 字体，"
            "中文将显示为方块（ASCII 不受影响）。\n");
    }

    return 0;
}
