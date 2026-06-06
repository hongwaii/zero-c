/**
 * @file theme.h
 * @brief 主题（颜色 + 样式）+ 字体加载。
 *
 * theme_apply() 必须在 ImGui 上下文创建后立即调用（host_create 里）。
 * theme_load_fonts() 在 ImGui backend init 后、第一次 NewFrame() 前调用。
 */
#ifndef APP_SHELL_THEME_H
#define APP_SHELL_THEME_H

#include "agent_types.h"

/**
 * @brief 应用主题配色与控件样式（圆角、padding 等）。
 */
void theme_apply(agent_theme_t theme);

/**
 * @brief 加载字体：默认字体 + 可选 CJK 子集。
 * @return 0 成功（或 CJK 字体未提供也视作成功），负数见 agent_errstr。
 */
int theme_load_fonts(void);

#endif
