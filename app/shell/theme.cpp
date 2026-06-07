/**
 * @file theme.cpp
 * @brief 工程蓝主题（深色高对比）+ CJK 字体三级 fallback 加载。
 *
 * 字体加载策略：
 *   1) assets/fonts/cn.otf（用户/未来 P6 打包自带的 CJK 字体）
 *   2) Windows 系统已装的中文字体（微软雅黑等），用 GetFontData 读进内存
 *   3) 都失败：用 ImGui 默认字体（中文会显示为方块）+ stderr 警告
 *
 * 每次启动都尝试前两步，无需用户做任何配置。
 */
#include "theme.h"
#include "imgui.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

/**
 * @brief 应用主题配色与控件样式（圆角、padding 等）。
 * （原样保留，无改动。）
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

/* === CJK 字体加载：三级 fallback 实现 === */

/** 候选 CJK 字体名（按优先级）。中文 Windows 系统通常至少装其中一个。 */
static const wchar_t *kCjkFontCandidates[] = {
    L"Microsoft YaHei UI",
    L"Microsoft YaHei",
    L"\x5B5D\x8F6F\x96C5\x9ED1",  /* L"微软雅黑" */
    L"SimSun",
    L"NSimSun",
    L"SimHei",
    L"DengXian",
    L"Source Han Sans SC",
    L"Noto Sans CJK SC",
    L"Noto Sans SC",
};

/** 候选字体数量。 */
static const int kCjkFontCandidateCount =
    sizeof(kCjkFontCandidates) / sizeof(kCjkFontCandidates[0]);

/**
 * @brief 把候选字体名转成 ANSI 字符串用于 stderr 输出。
 * 简化：宽字符里只输出可打印 ASCII 部分，中文名用省略号。
 */
static void wname_to_ansi(const wchar_t *wname, char *out, size_t out_len)
{
    /* 简化：宽字符名直接 WideCharToMultiByte 转换。 */
    WideCharToMultiByte(CP_UTF8, 0, wname, -1, out, (int)out_len, NULL, NULL);
}

/**
 * @brief 用 Windows GDI 加载指定字体名的 TTF 数据到堆 buffer。
 * @param face_name 字体名（宽字符串）。
 * @param out_size  输出：buffer 字节数。
 * @return 成功返回 malloc 出来的 buffer（调用方 free），失败返回 NULL。
 */
static void *load_system_font_ttf(const wchar_t *face_name, size_t *out_size)
{
    HDC hdc = ::CreateCompatibleDC(NULL);
    if (!hdc) return NULL;

    LOGFONTW lf = {0};
    lf.lfHeight         = 16;  /* 16px 取数据，渲染时再设实际大小 */
    lf.lfWeight         = FW_NORMAL;
    lf.lfCharSet        = DEFAULT_CHARSET;
    lf.lfOutPrecision   = OUT_TT_PRECIS;
    lf.lfClipPrecision  = CLIP_DEFAULT_PRECIS;
    lf.lfQuality        = PROOF_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcsncpy(lf.lfFaceName, face_name, LF_FACESIZE - 1);
    lf.lfFaceName[LF_FACESIZE - 1] = L'\0';

    HFONT hfont = ::CreateFontIndirectW(&lf);
    if (!hfont) {
        ::DeleteDC(hdc);
        return NULL;
    }
    HFONT hfont_old = (HFONT)::SelectObject(hdc, hfont);

    /* 第一次 GetFontData 拿大小（最后一个参数传 NULL）。 */
    DWORD size = ::GetFontData(hdc, 0, 0, NULL, 0);
    if (size == GDI_ERROR || size == 0) {
        ::SelectObject(hdc, hfont_old);
        ::DeleteObject(hfont);
        ::DeleteDC(hdc);
        return NULL;
    }

    /* 分配 buffer 并读 TTF 数据。 */
    void *buf = std::malloc(size);
    if (!buf) {
        ::SelectObject(hdc, hfont_old);
        ::DeleteObject(hfont);
        ::DeleteDC(hdc);
        return NULL;
    }
    DWORD got = ::GetFontData(hdc, 0, 0, buf, size);
    if (got == GDI_ERROR || got != size) {
        std::free(buf);
        ::SelectObject(hdc, hfont_old);
        ::DeleteObject(hfont);
        ::DeleteDC(hdc);
        return NULL;
    }

    ::SelectObject(hdc, hfont_old);
    ::DeleteObject(hfont);
    ::DeleteDC(hdc);

    *out_size = (size_t)size;
    return buf;
}

/**
 * @brief 三级 fallback：文件 → 系统字体 → 默认 + 警告。
 * @return 0 成功（含 fallback 情况），-1 默认字体也没注册（极不应该）。
 */
int theme_load_fonts(void)
{
    ImGuiIO &io = ImGui::GetIO();

    /* 第一步：始终注册默认字体（避免 fonts atlas 为空导致首帧渲染失败）。 */
    io.Fonts->AddFontDefault();

    /* 第二步：尝试 assets/fonts/cn.otf（用户自放 / 未来 P6 打包）。 */
    {
        FILE *f = std::fopen("assets/fonts/cn.otf", "rb");
        if (f) {
            std::fclose(f);
            ImFontConfig cfg;
            cfg.OversampleH = 2;
            cfg.OversampleV = 1;
            io.Fonts->AddFontFromFileTTF("assets/fonts/cn.otf", 16.0f, &cfg,
                io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
            std::fprintf(stderr, "theme_load_fonts: 已加载 assets/fonts/cn.otf\n");
            return 0;
        }
    }

    /* 第三步：枚举 Windows 系统字体，挨个试候选 CJK 字体。 */
    for (int i = 0; i < kCjkFontCandidateCount; i++) {
        size_t size = 0;
        void *ttf = load_system_font_ttf(kCjkFontCandidates[i], &size);
        if (ttf) {
            ImFontConfig cfg;
            cfg.OversampleH = 2;
            cfg.OversampleV = 1;
            io.Fonts->AddFontFromMemoryTTF(ttf, (int)size, 16.0f, &cfg,
                io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
            char aname[128];
            wname_to_ansi(kCjkFontCandidates[i], aname, sizeof(aname));
            std::fprintf(stderr,
                "theme_load_fonts: assets/fonts/cn.otf 缺失，已用系统字体 '%s' "
                "(%zu bytes) 提供中文渲染。\n", aname, size);
            return 0;
        }
    }

    /* 第四步：都失败 → 默认字体 + 警告。 */
    std::fprintf(stderr,
        "theme_load_fonts: 既无 assets/fonts/cn.otf 也无系统 CJK 字体，"
        "中文将显示为方块。\n");
    return 0;
}
