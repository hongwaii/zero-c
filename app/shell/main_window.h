/**
 * @file main_window.h
 * @brief 主窗口：DockBuilder + 左侧导航 + 内容区（dispatch 到各 panel）。
 */
#ifndef APP_SHELL_MAIN_WINDOW_H
#define APP_SHELL_MAIN_WINDOW_H

#include "agent_types.h"

/**
 * @brief 渲染主窗口（每帧调用）。
 * @param app 全局 app context（持有当前激活的 panel 编号等）。
 */
void main_window_render(agent_app_t *app);

/**
 * @brief 注册 panel 列表（P1 时为空操作，未来 lazy init）。
 */
void main_window_register_panels(void);

#endif
