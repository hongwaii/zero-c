/**
 * @file at_log_writer.c
 * @brief AT 收发日志写入：同步写库 + 3x 重试 + file 降级。
 *
 * 写入流程：
 *   1) prepare "INSERT INTO at_log(device_id, ts, dir, raw) VALUES (?, ?, ?, ?)"
 *   2) bind_text 三次 + bind_int(ts)
 *   3) step + finalize
 *   4) 若 step != SQLITE_DONE → 重试（最多 3 次，间隔 50ms）
 *   5) 重试全部失败 → 追加到 logs/at_log_fallback.log（兜底）
 *
 * v1.0 简化：不开 worker 线程，不维护队列。理由：SQLite WAL 模式下
 * 单写者瓶颈在磁盘 I/O，线程切换无收益；v1.1 如确需异步再改。
 */
#include "at_log_writer.h"
#include "sqlite_db.h"
#include "agent_types.h"

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Windows 平台下 Sleep 来自 windows.h；其他平台用 usleep 兜底 */
#ifdef _WIN32
#include <windows.h>
#define SLEEP_MS(ms) Sleep((DWORD)(ms))
#else
#include <unistd.h>
#define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

#define RETRY_MAX 3   /* 重试次数（含首次） */
#define RETRY_MS  50  /* 重试间隔 */

#define FALLBACK_PATH "logs/at_log_fallback.log"

/**
 * @brief 单次尝试 insert。
 * @return AGENT_OK 成功；AGENT_ERR_IO 失败（DB 句柄为空 / prepare 失败 / step 失败）
 */
static int try_insert(const char *dev_id, const char *dir, const char *raw)
{
    sqlite3 *db = (sqlite3 *)storage_get_db();
    if (!db) return AGENT_ERR_IO;

    const char *sql = "INSERT INTO at_log(device_id, ts, dir, raw) VALUES (?, ?, ?, ?)";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "at_log: prepare failed: %d\n", rc);
        return AGENT_ERR_IO;
    }

    /* 4 个绑定：device_id(ts=1), ts(2), dir(3), raw(4) */
    sqlite3_bind_text(stmt, 1, dev_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 2, (int)time(NULL));
    sqlite3_bind_text(stmt, 3, dir,    -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, raw,    -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? AGENT_OK : AGENT_ERR_IO;
}

/**
 * @brief 降级写：追加一行到 logs/at_log_fallback.log。
 *
 * 格式：ts<TAB>dev_id<TAB>dir<TAB>raw\n
 * 文件打不开直接静默返 AGENT_OK（兜底已尽力）。
 */
static void fallback_to_file(const char *dev_id, const char *dir, const char *raw)
{
    FILE *f = fopen(FALLBACK_PATH, "a");
    if (!f) {
        fprintf(stderr, "at_log: fallback fopen(%s) failed (truly lost)\n", FALLBACK_PATH);
        return;
    }
    fprintf(f, "%ld\t%s\t%s\t%s\n", (long)time(NULL), dev_id, dir, raw);
    fclose(f);
    fprintf(stderr, "at_log: db write failed 3x, fallback to %s\n", FALLBACK_PATH);
}

int at_log_writer_add(const char *dev_id, const char *dir, const char *raw)
{
    if (!dev_id || !dir || !raw) return AGENT_ERR_BAD_ARG;

    /* 重试 3 次（含首次），每次失败后 Sleep 50ms */
    for (int i = 0; i < RETRY_MAX; i++) {
        int rc = try_insert(dev_id, dir, raw);
        if (rc == AGENT_OK) return AGENT_OK;
        if (rc != AGENT_ERR_IO) return rc;  /* 非 IO 错不重试 */
        if (i < RETRY_MAX - 1) SLEEP_MS(RETRY_MS);
    }

    /* 3 次全失败 → 降级到文件 */
    fallback_to_file(dev_id, dir, raw);
    return AGENT_OK;  /* 降级成功不算错误——已兜底 */
}

int  at_log_writer_init(void) { return AGENT_OK; }
void at_log_writer_close(void) {}
void at_log_writer_flush(void) {}
