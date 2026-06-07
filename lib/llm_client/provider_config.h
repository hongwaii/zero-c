/**
 * @file provider_config.h
 * @brief 5 厂商 seed + 加载/保存 config/llm_providers.json。
 *
 * 落盘前 DPAPI 加密 api_key（hex 编码）；加载时 DPAPI 解密。
 * 若 config 文件不存在，调 seed_defaults 写 5 厂商模板。
 */
#ifndef LIB_LLM_PROVIDER_CONFIG_H
#define LIB_LLM_PROVIDER_CONFIG_H

#include "agent_types.h"  /* agent_llm_provider_t */

/* 5 厂商默认 seed。base_url / default_model 见 spec §4.5。 */
int  llm_seed_defaults(agent_app_t *app);  /* 灌到 app->providers */

/* 从 path 加载到 app->providers（清空已有）。
 * 加载时对每个 provider 的 api_key hex 串做 DPAPI 解密。
 * 若 path 不存在，自动 seed_defaults 并 save。返回 0=ok。 */
int  llm_provider_config_load(agent_app_t *app, const char *path);

/* 把 app->providers 加密后写到 path（原子写）。返回 0=ok。 */
int  llm_provider_config_save(const agent_app_t *app, const char *path);

#endif
