/**
 * @file config_io.h
 * @brief app.json / devices.json 加载 / 保存。
 *
 * 字段：app = { theme, lang }（v1.0 不存 default_baud，靠全局）；
 *       devices = { 数组，每项 {id,label,custom_baud} }（v1.0 加载/保存留空，
 *       devices 当前全靠 device_manager 扫描）。
 *
 * 用 lib/util/json_reader / json_writer（P1 已实现）。
 */
#ifndef LIB_STORAGE_CONFIG_IO_H
#define LIB_STORAGE_CONFIG_IO_H

#include "agent_types.h"

/**
 * @brief 从 path 加载 app.json，填充 app->theme / app->lang。
 *
 * 文件不存在不视为错误（返 AGENT_ERR_NOT_FOUND，但 app 已填缺省值），
 * 解析失败才返 AGENT_ERR_IO。
 *
 * @param app  输出，回填 theme / lang（其它字段不动）。
 * @param path JSON 文件路径。
 * @return AGENT_OK 成功，AGENT_ERR_NOT_FOUND 文件不存在，AGENT_ERR_IO 解析失败。
 */
int  config_load_app  (agent_app_t *app, const char *path);

/**
 * @brief 把 app->theme / app->lang 原子写到 path。
 *
 * @param app  输入。
 * @param path 目标文件路径。
 * @return AGENT_OK 成功，AGENT_ERR_IO 写入失败。
 */
int  config_save_app  (const agent_app_t *app, const char *path);

/**
 * @brief v1.0 简化：no-op（devices 列表由 device_manager 扫描得到，不落盘）。
 *
 * 接口保留以便 v1.1 接入。
 */
int  config_load_devices(agent_app_t *app, const char *path);

/**
 * @brief v1.0 简化：no-op。
 */
int  config_save_devices(const agent_app_t *app, const char *path);

#endif /* LIB_STORAGE_CONFIG_IO_H */
