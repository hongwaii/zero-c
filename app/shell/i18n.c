/**
 * @file i18n.c
 */
#include "i18n.h"
#include "json_reader.h"
#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 当前加载的语言根节点；i18n_shutdown 时 delete */
static struct cJSON *g_root = NULL;
static agent_lang_t  g_lang = AGENT_LANG_ZH_CN;

/**
 * @brief 根据语言枚举返回对应 JSON 文件路径。
 */
static const char *path_for(agent_lang_t l)
{
    switch (l) {
        case AGENT_LANG_EN_US: return "app/i18n/en.json";
        case AGENT_LANG_ZH_CN:
        default:               return "app/i18n/zh.json";
    }
}

/**
 * @brief 加载语言文件并替换全局根节点。失败时 g_root 保持 NULL。
 * @return 0 成功，负数见 agent_errstr。
 */
int i18n_init(agent_lang_t lang)
{
    i18n_shutdown();
    int rc = json_load_file(path_for(lang), &g_root);
    if (rc != AGENT_OK) {
        fprintf(stderr, "i18n: 加载 %s 失败 (err=%d)\n", path_for(lang), rc);
        return rc;
    }
    g_lang = lang;
    return AGENT_OK;
}

/** @brief 释放当前加载的根节点。 */
void i18n_shutdown(void)
{
    if (g_root) { cJSON_Delete(g_root); g_root = NULL; }
    g_lang = AGENT_LANG_ZH_CN;
}

/** @brief 返回当前语言。 */
agent_lang_t i18n_current(void) { return g_lang; }

/**
 * @brief 按 key 取字符串，找不到或类型不匹配时返回 key 本身。
 * @note 返回值由 i18n 内部持有，调用方不应 free。
 */
const char *i18n_get(const char *key)
{
    if (!g_root || !key) return key;
    struct cJSON *v = cJSON_GetObjectItemCaseSensitive(g_root, key);
    if (cJSON_IsString(v) && v->valuestring) return v->valuestring;
    return key;
}
