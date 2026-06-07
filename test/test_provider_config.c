/**
 * @file test_provider_config.c
 * @brief P5 Task 5 roundtrip：seed → save（DPAPI 加密）→ load（DPAPI 解密）→ 校验。
 */
#include "provider_config.h"
#include "llm_dpapi.h"
#include "agent_types.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(void)
{
    agent_app_t app = {0};
    /* 1) seed 5 厂商 */
    int rc = llm_seed_defaults(&app);
    assert(rc == AGENT_OK);
    assert(app.providers != NULL);

    /* 数节点数：应为 5 */
    int cnt = 0;
    for (agent_llm_provider_t *q = app.providers; q; q = q->next) cnt++;
    assert(cnt == 5);

    /* 2) 给第一个 provider 设 api_key（明文）；其余保持空 */
    agent_llm_provider_t *p = app.providers;
    strcpy(p->api_key, "sk-test-key");

    const char *path = "out/test_providers.json";

    /* 3) save（api_key 走 DPAPI 加密落盘） */
    rc = llm_provider_config_save(&app, path);
    assert(rc == AGENT_OK);

    /* 4) load 到新 app（DPAPI 解密） */
    agent_app_t app2 = {0};
    rc = llm_provider_config_load(&app2, path);
    assert(rc == AGENT_OK);
    assert(app2.providers != NULL);

    /* 5) 校验：第一个 provider 的 api_key 已解密回明文 */
    assert(strcmp(app2.providers->api_key, "sk-test-key") == 0);
    /* 其余 4 个 api_key 仍为空 */
    agent_llm_provider_t *q = app2.providers->next;
    for (int i = 0; i < 4 && q; i++, q = q->next) {
        assert(q->api_key[0] == '\0');
    }

    /* 6) 清理：两个 app 各自释放，删测试文件 */
    agent_llm_provider_t *cur, *n;
    for (cur = app.providers; cur; cur = n) { n = cur->next; free(cur); }
    for (cur = app2.providers; cur; cur = n) { n = cur->next; free(cur); }
    remove(path);

    printf("test_provider_config: roundtrip OK\n");
    return 0;
}
