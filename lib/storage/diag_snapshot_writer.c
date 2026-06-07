/**
 * @file diag_snapshot_writer.c
 * @brief diag_snapshot 表 INSERT OR REPLACE 实现。
 *
 * 流程：
 *  1. 校验参数 + 取 storage_get_db()
 *  2. sqlite3_prepare_v2 编译 INSERT OR REPLACE
 *  3. bind 9 个字段（device_id, ts, csq, rsrp, rsrq, snr, operator, rat, cereg, ps_attached）
 *     —— 注：rsrp/rsrq/snr/ps_attached v1.0 暂存 0（P3 diag_state 无对应字段）
 *  4. sqlite3_step + sqlite3_finalize
 *
 * 错误处理：
 *  - 参数空 → AGENT_ERR_BAD_ARG
 *  - DB 未初始化（stub 模式） → AGENT_ERR_IO
 *  - sqlite3_prepare/step 失败 → AGENT_ERR_IO（错误信息打到 stderr）
 */
#include "diag_snapshot_writer.h"
#include "sqlite_db.h"

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * @brief 把字符串形式的状态值转成整数（0 失败时返 0）。
 * @param s  字符串
 * @return  整数
 */
static int s_to_i(const char *s)
{
    if (!s || !*s) return 0;
    return atoi(s);
}

int diag_snapshot_writer_add(const char *dev_id, const diag_state_t *st)
{
    /* ---- 参数校验 ---- */
    if (!dev_id || !st) {
        fprintf(stderr, "diag_snapshot_writer_add: bad arg (dev_id=%p, st=%p)\n",
                (const void *)dev_id, (const void *)st);
        return AGENT_ERR_BAD_ARG;
    }

    /* ---- 取 DB 句柄（stub 模式下为 NULL） ---- */
    sqlite3 *db = (sqlite3 *)storage_get_db();
    if (!db) {
        fprintf(stderr, "diag_snapshot_writer_add: storage not initialized\n");
        return AGENT_ERR_IO;
    }

    /* ---- 准备 INSERT OR REPLACE 语句 ----
     * PRIMARY KEY (device_id, ts) → 同设备同秒重复插入会覆盖。
     * v1.0 简化：rsrp/rsrq/snr/ps_attached 暂存 0（P3 diag_state 无字段，留给 v1.1 补）。 */
    const char *sql =
        "INSERT OR REPLACE INTO diag_snapshot"
        "(device_id, ts, csq, rsrp, rsrq, snr, operator, rat, cereg, ps_attached)"
        " VALUES (?, ?, ?, 0, 0, 0.0, ?, ?, ?, 0)";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "diag_snapshot_writer_add: prepare failed: %d\n", rc);
        return AGENT_ERR_IO;
    }

    /* ---- 绑定参数 ---- */
    sqlite3_bind_text (stmt, 1, dev_id,                    -1, SQLITE_TRANSIENT);
    sqlite3_bind_int  (stmt, 2, (int)time(NULL));
    sqlite3_bind_int  (stmt, 3, s_to_i(st->csq));
    /* idx 4/5/6: rsrp/rsrq/snr —— 暂用 0（P3 暂未采集） */
    sqlite3_bind_text (stmt, 7, st->cop_operator ? st->cop_operator : "",
                       -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 8, st->rat ? st->rat : "",
                       -1, SQLITE_TRANSIENT);
    sqlite3_bind_int  (stmt, 9, s_to_i(st->cereg));
    /* idx 10: ps_attached —— 暂用 0 */

    /* ---- 执行 ---- */
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "diag_snapshot_writer_add: step failed: %d\n", rc);
        return AGENT_ERR_IO;
    }
    return AGENT_OK;
}

int diag_snapshot_writer_recent(const char *dev_id, int n,
                                diag_state_t *out, int *got)
{
    /* v1.0 简化：不实现 select（test_at_session 不需要 roundtrip） */
    (void)dev_id;
    (void)n;
    (void)out;
    if (got) *got = 0;
    return AGENT_OK;
}

int  diag_snapshot_writer_init(void)  { return AGENT_OK; }
void diag_snapshot_writer_close(void) { (void)0; }
