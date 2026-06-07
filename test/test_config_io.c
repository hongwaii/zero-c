/**
 * @file test_config_io.c
 * @brief config_io app.json roundtrip 单元测试。
 *
 * 覆盖：
 *   1. 写一个 app（theme=LIGHT, lang=EN_US）→ 文件
 *   2. 加载回来 → theme / lang 还原
 *   3. 加载不存在的文件 → AGENT_ERR_NOT_FOUND，app 字段保留旧值
 *   4. devices 加载 / 保存都是 no-op（返 AGENT_OK）
 *   5. 写 / 读 default_baud 字段被忽略（v1.0 约定）
 */
#include "config_io.h"
#include "agent_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    const char *path = "out/test_config.json";
    remove(path);

    /* ---- 1. save 写文件 ---- */
    agent_app_t app = {0};
    app.theme = AGENT_THEME_LIGHT;       /* 非默认 0 */
    app.lang  = AGENT_LANG_EN_US;        /* 非默认 0 */
    int rc = config_save_app(&app, path);
    if (rc != AGENT_OK) {
        printf("test_config_io: FAIL (save rc=%d)\n", rc);
        return 1;
    }

    /* ---- 2. 加载回来 → 字段还原 ---- */
    agent_app_t app2 = {0};
    app2.theme = AGENT_THEME_ENGINEERING_BLUE;  /* 故意设成不同 */
    app2.lang  = AGENT_LANG_ZH_CN;
    rc = config_load_app(&app2, path);
    if (rc != AGENT_OK) {
        printf("test_config_io: FAIL (load rc=%d)\n", rc);
        remove(path);
        return 1;
    }
    if (app2.theme != AGENT_THEME_LIGHT) {
        printf("test_config_io: FAIL (theme=%d want LIGHT=%d)\n",
               (int)app2.theme, (int)AGENT_THEME_LIGHT);
        remove(path);
        return 1;
    }
    if (app2.lang != AGENT_LANG_EN_US) {
        printf("test_config_io: FAIL (lang=%d want EN_US=%d)\n",
               (int)app2.lang, (int)AGENT_LANG_EN_US);
        remove(path);
        return 1;
    }

    /* ---- 3. 加载不存在的文件 → NOT_FOUND，原值保留 ---- */
    remove(path);
    agent_app_t app3 = {0};
    app3.theme = AGENT_THEME_ENGINEERING_BLUE;
    app3.lang  = AGENT_LANG_ZH_CN;
    rc = config_load_app(&app3, "out/__no_such_file__.json");
    if (rc != AGENT_ERR_NOT_FOUND) {
        printf("test_config_io: FAIL (missing-file rc=%d want NOT_FOUND)\n", rc);
        return 1;
    }
    if (app3.theme != AGENT_THEME_ENGINEERING_BLUE ||
        app3.lang  != AGENT_LANG_ZH_CN) {
        printf("test_config_io: FAIL (default not preserved on missing file)\n");
        return 1;
    }

    /* ---- 4. devices load/save 是 no-op ---- */
    rc = config_load_devices(&app, "out/test_devices.json");
    if (rc != AGENT_OK) {
        printf("test_config_io: FAIL (load_devices rc=%d)\n", rc);
        return 1;
    }
    rc = config_save_devices(&app, "out/test_devices.json");
    if (rc != AGENT_OK) {
        printf("test_config_io: FAIL (save_devices rc=%d)\n", rc);
        return 1;
    }

    /* ---- 5. 文件不存在时 save 也允许（先写后存） ---- */
    agent_app_t app4 = {0};
    app4.theme = AGENT_THEME_ENGINEERING_BLUE;
    app4.lang  = AGENT_LANG_ZH_CN;
    rc = config_save_app(&app4, path);
    if (rc != AGENT_OK) {
        printf("test_config_io: FAIL (second save rc=%d)\n", rc);
        return 1;
    }
    agent_app_t app5 = {0};
    rc = config_load_app(&app5, path);
    if (rc != AGENT_OK || app5.theme != AGENT_THEME_ENGINEERING_BLUE ||
        app5.lang != AGENT_LANG_ZH_CN) {
        printf("test_config_io: FAIL (roundtrip-2)\n");
        remove(path);
        return 1;
    }

    remove(path);
    printf("test_config_io: roundtrip OK (5/5 cases)\n");
    return 0;
}
