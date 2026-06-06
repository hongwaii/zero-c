/**
 * @file i18n.h
 * @brief 多语言字符串加载（zh-CN / en-US 占位）。
 *
 * 当前 P1 只实现 zh-CN 加载。en-US 在 v1.1 再加。
 * i18n_get() 找不到 key 时返回 key 本身（不返回 NULL，方便调用方）。
 */
#ifndef APP_SHELL_I18N_H
#define APP_SHELL_I18N_H

#include "agent_types.h"
struct cJSON;

#ifdef __cplusplus
extern "C" {
#endif

int  i18n_init(agent_lang_t lang);
void i18n_shutdown(void);
const char *i18n_get(const char *key);  /* 找不到时返回 key 字符串 */
agent_lang_t i18n_current(void);

#ifdef __cplusplus
}
#endif

#endif
