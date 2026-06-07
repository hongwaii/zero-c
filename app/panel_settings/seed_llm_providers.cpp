/**
 * @file seed_llm_providers.cpp
 * @brief P5 改造：原本硬编码 5 厂商——现在调 llm_provider_config_load。
 *
 * 保留同名函数 panel_settings_seed_defaults 给 main.cpp 启动时调。
 * 若 config 文件存在则 load（自动 DPAPI 解密 api_key），否则 seed + save。
 */
#include "panel_settings.h"
#include "provider_config.h"
#include <cstdio>
#include <cstring>

/* config 文件相对路径：相对 EXE cwd 解析。
 * main.cpp 在启动时 _mkdir("config") 保证目录存在。 */
static const char *kProviderConfigPath = "config/llm_providers.json";

void panel_settings_seed_defaults(agent_app_t *app)
{
    /* 优先级：先看 config 文件；不存在才 seed。 */
    if (llm_provider_config_load(app, kProviderConfigPath) != AGENT_OK) {
        std::fprintf(stderr, "panel_settings: provider config load failed, fall back to seed\n");
        llm_seed_defaults(app);
    }
}

void panel_settings_reload_from_config(agent_app_t *app)
{
    llm_provider_config_load(app, kProviderConfigPath);
}

void panel_settings_save_to_config(agent_app_t *app)
{
    if (llm_provider_config_save(app, kProviderConfigPath) != AGENT_OK) {
        std::fprintf(stderr, "panel_settings: save failed to %s\n", kProviderConfigPath);
    } else {
        std::fprintf(stderr, "panel_settings: saved to %s\n", kProviderConfigPath);
    }
}
