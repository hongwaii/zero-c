/**
 * @file config_io.c
 * @brief app.json / devices.json 加载 / 保存实现。
 *
 * 策略：
 *   - app.json 只持久化 theme + lang；default_baud 是全局常量，不存 app 结构。
 *   - devices.json v1.0 不写（设备列表由 device_manager 扫描），但接口留好。
 *   - 加载时若文件不存在返 AGENT_ERR_NOT_FOUND（不视为错误——调用方可继续用缺省值）。
 *   - 写入用 lib/util/json_writer 的原子写（先写 .tmp 再 rename）。
 */
#include "config_io.h"
#include "json_reader.h"
#include "json_writer.h"

#include <cJSON.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief 从 path 加载 app.json，填充 app->theme / app->lang。
 */
int config_load_app(agent_app_t *app, const char *path)
{
    if (!app || !path) return AGENT_ERR_BAD_ARG;

    /* 缺省值先填好——文件不存在/字段缺失时保留这些。 */
    int theme = (int)app->theme;
    int lang  = (int)app->lang;

    cJSON *root = NULL;
    int rc = json_load_file(path, &root);
    if (rc == AGENT_ERR_NOT_FOUND) {
        /* 文件不存在不算错——首次启动常见；保留 app 现有值（缺省），返 NOT_FOUND 让调用方知晓。 */
        return AGENT_ERR_NOT_FOUND;
    }
    if (rc != AGENT_OK) {
        /* 解析失败 / OOM / IO 错 */
        return rc;
    }

    /* 取字段。找不到/类型不匹配时 json_get_* 写 def、返 NOT_FOUND；我们忽略其返回值
     * （用缺省即可）。但若用户文件里 theme=0 而我们 theme=1，仍以文件为准——这里
     * 先把 def 填成 app 原值再调，覆盖语义正确。 */
    json_get_int(root, "theme", (int)app->theme, &theme);
    json_get_int(root, "lang",  (int)app->lang,  &lang);

    /* default_baud 读取后丢弃（v1.0 不持久化） */
    int dummy_baud = 0;
    json_get_int(root, "default_baud", 0, &dummy_baud);
    (void)dummy_baud;

    app->theme = (agent_theme_t)theme;
    app->lang  = (agent_lang_t)lang;

    cJSON_Delete(root);
    return AGENT_OK;
}

/**
 * @brief 把 app->theme / app->lang 原子写到 path。
 */
int config_save_app(const agent_app_t *app, const char *path)
{
    if (!app || !path) return AGENT_ERR_BAD_ARG;

    cJSON *root = cJSON_CreateObject();
    if (!root) return AGENT_ERR_OOM;

    /* v1.0：只写 theme + lang；default_baud 留空。 */
    cJSON_AddNumberToObject(root, "theme", (double)(int)app->theme);
    cJSON_AddNumberToObject(root, "lang",  (double)(int)app->lang);

    int rc = json_save_file_atomic(path, root);
    cJSON_Delete(root);
    return rc;
}

/**
 * @brief v1.0 简化：no-op。
 *
 * 设备列表当前全靠 device_manager 实时扫描（spec §5.4：v1.0 不落盘设备元数据）。
 * 接口保留以便 v1.1 接入（届时会写 {id,label,custom_baud}[] 数组）。
 */
int config_load_devices(agent_app_t *app, const char *path)
{
    (void)app;
    (void)path;
    return AGENT_OK;
}

/**
 * @brief v1.0 简化：no-op。
 */
int config_save_devices(const agent_app_t *app, const char *path)
{
    (void)app;
    (void)path;
    return AGENT_OK;
}
