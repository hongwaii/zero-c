/**
 * @file seed_llm_providers.cpp
 * @brief 启动时调一次，把 5 个 LLM provider 预置到 app->providers。
 *
 * P1 期间 key 是空字符串（明文），由用户在 UI 里填；
 * P6 改用 Windows DPAPI 加密保存到 config/llm_providers.json。
 */
#include "panel_settings.h"
#include <cstdlib>
#include <cstring>

/**
 * @brief 分配并初始化一个 provider 节点。
 */
static agent_llm_provider_t *mk(const char *name, const char *url, const char *model)
{
    agent_llm_provider_t *p = (agent_llm_provider_t *)std::calloc(1, sizeof(*p));
    if (!p) return NULL;
    std::strncpy(p->name,          name,  sizeof(p->name)          - 1);
    std::strncpy(p->base_url,      url,   sizeof(p->base_url)      - 1);
    std::strncpy(p->default_model, model, sizeof(p->default_model) - 1);
    p->api_key[0] = '\0';
    p->next = NULL;
    return p;
}

/**
 * @brief 建一个 5 节点的 LLM provider 链表，挂到 app->providers。
 */
void panel_settings_seed_defaults(agent_app_t *app)
{
    if (!app) return;
    agent_llm_provider_t *head = NULL, *tail = NULL;

    head = tail = mk("DeepSeek", "https://api.deepseek.com/v1",                 "deepseek-chat");
    if (!tail) return;
    tail->next = mk("Qwen",     "https://dashscope.aliyuncs.com/compatible-mode/v1", "qwen-plus");
    if (!tail->next) return; tail = tail->next;
    tail->next = mk("GLM",      "https://open.bigmodel.cn/api/paas/v4",          "glm-4-plus");
    if (!tail->next) return; tail = tail->next;
    tail->next = mk("Kimi",     "https://api.moonshot.cn/v1",                   "moonshot-v1-8k");
    if (!tail->next) return; tail = tail->next;
    tail->next = mk("MiniMax",  "<user-supplied endpoint>",                      "MiniMax-Text-01");
    if (!tail->next) return; tail = tail->next;

    app->providers = head;
}
