/**
 * @file panel_settings.h
 * @brief 设置 panel：主题切换 + 5 个 LLM provider CRUD（in-memory，P6 落盘）。
 */
#ifndef APP_PANEL_SETTINGS_H
#define APP_PANEL_SETTINGS_H
#include "agent_types.h"

/**
 * @brief 渲染设置 panel：主题 + 语言 + LLM provider 列表 + 版本号。
 */
void panel_settings_render(agent_app_t *app);

/**
 * @brief 在启动时调一次，把 5 个 LLM provider 链到 app->providers。
 * @param app 全局 app context（持有 LLM provider 链表）。
 */
void panel_settings_seed_defaults(agent_app_t *app);
#endif
