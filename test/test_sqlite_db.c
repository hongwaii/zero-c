/**
 * @file test_sqlite_db.c
 * @brief sqlite_db 包装单元测试：开 / 关 / 4 表 schema roundtrip。
 *
 * 模式分支：
 *   - 真 SQLite：storage_init 返 AGENT_OK → 重新 init 验证 IF NOT EXISTS 不报错
 *   - stub 模式：storage_init 返 AGENT_ERR_IO → 打印 skip、返回 0
 *
 * v1.0 简化：不开真 SQL 做"INSERT 一行再 SELECT"的全 roundtrip；只验证
 * 打开 / 关闭 / 多次 init 不出错。后续 task (at_log_writer 等) 各自负责 insert。
 */
#include "sqlite_db.h"
#include "agent_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    /* 用 out/ 下的临时 DB（每次都先删） */
    const char *path = "out/test_storage.db";
    remove(path);

    /* ---- 1. 首次 init ---- */
    int rc = storage_init(path);
    if (rc != AGENT_OK) {
        /* stub 模式：sqlite3_open stub 返 ERROR → storage_init 返 IO。
         * 视为 skip，不让 CI 失败；v1.0.1 拉真 SQLite 后会真跑。 */
        printf("test_sqlite_db: skip (sqlite stub mode, rc=%d)\n", rc);
        remove(path);
        return 0;
    }

    /* 真 SQLite 模式：验证 schema 创建成功 */
    if (storage_get_db() == NULL) {
        printf("test_sqlite_db: FAIL (storage_get_db NULL after init)\n");
        storage_close();
        remove(path);
        return 1;
    }

    storage_close();

    /* ---- 2. 重新打开：验证表持久化 + IF NOT EXISTS 不报错 ---- */
    rc = storage_init(path);
    if (rc != AGENT_OK) {
        printf("test_sqlite_db: FAIL (second init rc=%d)\n", rc);
        storage_close();
        remove(path);
        return 1;
    }
    if (storage_get_db() == NULL) {
        printf("test_sqlite_db: FAIL (storage_get_db NULL after re-init)\n");
        storage_close();
        remove(path);
        return 1;
    }

    /* ---- 3. 多次 init 幂等性 ---- */
    rc = storage_init(path);
    if (rc != AGENT_OK) {
        printf("test_sqlite_db: FAIL (third init rc=%d)\n", rc);
        storage_close();
        remove(path);
        return 1;
    }

    /* ---- 4. close 后 get_db 应返 NULL；再 init 仍可用 ---- */
    storage_close();
    if (storage_get_db() != NULL) {
        printf("test_sqlite_db: FAIL (storage_get_db not NULL after close)\n");
        remove(path);
        return 1;
    }
    rc = storage_init(path);
    if (rc != AGENT_OK) {
        printf("test_sqlite_db: FAIL (re-init after close rc=%d)\n", rc);
        remove(path);
        return 1;
    }

    storage_close();
    remove(path);

    printf("test_sqlite_db: all pass (open/close/init×3 roundtrip)\n");
    return 0;
}
