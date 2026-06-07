/**
 * @file provider_config.c
 * @brief 5 厂商 seed + JSON 加载 / 保存 + DPAPI 加密落盘。
 */
#include "provider_config.h"
#include "llm_dpapi.h"
#include "json_reader.h"
#include "json_writer.h"
#include "agent_types.h"

#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 5 厂商默认 seed（spec §4.5） */
static const struct { const char *name, *url, *model; } kSeed[5] = {
    { "DeepSeek", "https://api.deepseek.com/v1",                    "deepseek-chat"   },
    { "Qwen",     "https://dashscope.aliyuncs.com/compatible-mode/v1", "qwen-plus"    },
    { "GLM",      "https://open.bigmodel.cn/api/paas/v4",            "glm-4-plus"     },
    { "Kimi",     "https://api.moonshot.cn/v1",                      "moonshot-v1-8k" },
    { "MiniMax",  "<user-supplied endpoint>",                         "MiniMax-Text-01" },
};

/* 释放 app->providers 链表（每次 load 前调，避免泄漏） */
static void free_providers(agent_app_t *app)
{
    agent_llm_provider_t *p = app->providers;
    while (p) { agent_llm_provider_t *n = p->next; free(p); p = n; }
    app->providers = NULL;
}

int llm_seed_defaults(agent_app_t *app)
{
    if (!app) return AGENT_ERR_BAD_ARG;
    free_providers(app);
    agent_llm_provider_t *head = NULL, *tail = NULL;
    for (int i = 0; i < 5; i++) {
        agent_llm_provider_t *p = (agent_llm_provider_t *)calloc(1, sizeof(*p));
        if (!p) { free_providers(app); return AGENT_ERR_OOM; }
        strncpy(p->name, kSeed[i].name, sizeof(p->name) - 1);
        strncpy(p->base_url, kSeed[i].url, sizeof(p->base_url) - 1);
        strncpy(p->default_model, kSeed[i].model, sizeof(p->default_model) - 1);
        p->api_key[0] = '\0';
        if (!head) head = p; else tail->next = p;
        tail = p;
    }
    app->providers = head;
    return AGENT_OK;
}

int llm_provider_config_load(agent_app_t *app, const char *path)
{
    if (!app || !path) return AGENT_ERR_BAD_ARG;
    free_providers(app);
    cJSON *root = NULL;
    int rc = json_load_file(path, &root);
    if (rc != AGENT_OK) {
        /* 文件不存在（或读失败）→ seed + save 兜底 */
        int sr = llm_seed_defaults(app);
        if (sr != AGENT_OK) return sr;
        return llm_provider_config_save(app, path);
    }
    cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, "providers");
    if (!cJSON_IsArray(arr)) { cJSON_Delete(root); return AGENT_ERR_IO; }
    agent_llm_provider_t *head = NULL, *tail = NULL;
    cJSON *item;
    cJSON_ArrayForEach(item, arr) {
        agent_llm_provider_t *p = (agent_llm_provider_t *)calloc(1, sizeof(*p));
        if (!p) {
            /* 失败回滚：释放已建节点 */
            while (head) { agent_llm_provider_t *n = head->next; free(head); head = n; }
            cJSON_Delete(root);
            return AGENT_ERR_OOM;
        }
        cJSON *n  = cJSON_GetObjectItemCaseSensitive(item, "name");
        cJSON *u  = cJSON_GetObjectItemCaseSensitive(item, "base_url");
        cJSON *m  = cJSON_GetObjectItemCaseSensitive(item, "default_model");
        cJSON *k  = cJSON_GetObjectItemCaseSensitive(item, "api_key");
        if (cJSON_IsString(n) && n->valuestring)
            strncpy(p->name, n->valuestring, sizeof(p->name) - 1);
        if (cJSON_IsString(u) && u->valuestring)
            strncpy(p->base_url, u->valuestring, sizeof(p->base_url) - 1);
        if (cJSON_IsString(m) && m->valuestring)
            strncpy(p->default_model, m->valuestring, sizeof(p->default_model) - 1);
        if (cJSON_IsString(k) && k->valuestring && k->valuestring[0]) {
            /* DPAPI 解密 hex 串 */
            char plain[512];
            if (llm_dpapi_decrypt_hex(k->valuestring, strlen(k->valuestring),
                                      plain, sizeof(plain)) == AGENT_OK) {
                strncpy(p->api_key, plain, sizeof(p->api_key) - 1);
            }
            /* 解密失败保留空 key（不让明文残留） */
        }
        if (!head) head = p; else tail->next = p;
        tail = p;
    }
    cJSON_Delete(root);
    app->providers = head;
    return AGENT_OK;
}

int llm_provider_config_save(const agent_app_t *app, const char *path)
{
    if (!app || !path) return AGENT_ERR_BAD_ARG;
    cJSON *root = cJSON_CreateObject();
    if (!root) return AGENT_ERR_OOM;
    cJSON *arr = cJSON_AddArrayToObject(root, "providers");
    if (!arr) { cJSON_Delete(root); return AGENT_ERR_OOM; }
    for (agent_llm_provider_t *p = app->providers; p; p = p->next) {
        cJSON *item = cJSON_CreateObject();
        if (!item) { cJSON_Delete(root); return AGENT_ERR_OOM; }
        cJSON_AddStringToObject(item, "name",          p->name);
        cJSON_AddStringToObject(item, "base_url",      p->base_url);
        cJSON_AddStringToObject(item, "default_model", p->default_model);
        if (p->api_key[0]) {
            char hex[1024];
            if (llm_dpapi_encrypt_hex(p->api_key, strlen(p->api_key),
                                      hex, sizeof(hex)) == AGENT_OK) {
                cJSON_AddStringToObject(item, "api_key", hex);
            } else {
                /* 加密失败也不写明文——只留空串 */
                cJSON_AddStringToObject(item, "api_key", "");
            }
        } else {
            cJSON_AddStringToObject(item, "api_key", "");
        }
        cJSON_AddItemToArray(arr, item);
    }
    int rc = json_save_file_atomic(path, root);
    cJSON_Delete(root);
    return rc;
}
