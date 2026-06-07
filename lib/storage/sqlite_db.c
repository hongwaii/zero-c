/**
 * @file sqlite_db.c
 * @brief SQLite 单例句柄 + WAL 模式 + 4 表 schema 初始化。
 *
 * 设计要点：
 *  - 单例：g_db 在进程内只持有一份；storage_init 幂等。
 *  - WAL：journal_mode=WAL + synchronous=NORMAL，平衡性能与崩溃安全
 *    （WAL 单写者多读者，UI 读不被 at_session 写阻塞）。
 *  - 4 表：device（设备清单）/ diag_snapshot（诊断快照，PK(device_id,ts)）
 *         / at_log（AT 收发，AUTINCREMENT）/ llm_chat（LLM 角色消息，AUTINCREMENT）。
 *  - 2 索引：idx_diag_ts / idx_atlog_dev_ts 加速"最近 N 条"查询。
 *  - stub 模式：所有 exec 仍会调，但 sqlite3_exec stub 返回 ERROR；
 *    exec_or_log 把错误打到 stderr，storage_init 仍返 AGENT_OK（DDL 失败不回滚 init）。
 */
#include "sqlite_db.h"

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- 全局单例：进程级共享 DB 句柄 ---- */
static sqlite3 *g_db = NULL;

/**
 * @brief 执行单条 SQL；失败时把 errmsg + SQL 打到 stderr。
 * @return sqlite3_exec 返回码（SQLITE_OK 表示成功）
 */
static int exec_or_log(const char *sql)
{
    char *errmsg = NULL;
    int rc = sqlite3_exec(g_db, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "storage: exec failed: %s\nSQL: %s\n",
                errmsg ? errmsg : "(null)", sql);
        if (errmsg) sqlite3_free(errmsg);
    }
    return rc;
}

int storage_init(const char *db_path)
{
    /* 幂等：已打开则不重复 */
    if (g_db) return AGENT_OK;
    if (!db_path) return AGENT_ERR_BAD_ARG;

    int rc = sqlite3_open(db_path, &g_db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "storage: sqlite3_open(%s) failed: %d\n", db_path, rc);
        g_db = NULL;
        return AGENT_ERR_IO;
    }

    /* ---- WAL 模式 + 性能调优 ---- */
    exec_or_log("PRAGMA journal_mode=WAL;");
    exec_or_log("PRAGMA synchronous=NORMAL;");

    /* ---- device：设备清单（id 来自 device_manager 派发） ---- */
    exec_or_log(
        "CREATE TABLE IF NOT EXISTS device ("
        " id TEXT PRIMARY KEY, label TEXT, chan_uri TEXT,"
        " module_model TEXT, first_seen INTEGER, last_seen INTEGER);");

    /* ---- diag_snapshot：诊断快照（PK device_id+ts，自动去重） ---- */
    exec_or_log(
        "CREATE TABLE IF NOT EXISTS diag_snapshot ("
        " device_id TEXT, ts INTEGER, csq INTEGER, rsrp INTEGER,"
        " rsrq INTEGER, snr REAL, operator TEXT, rat TEXT,"
        " cereg INTEGER, ps_attached INTEGER,"
        " PRIMARY KEY (device_id, ts));");
    exec_or_log("CREATE INDEX IF NOT EXISTS idx_diag_ts "
                "ON diag_snapshot(ts DESC);");

    /* ---- at_log：AT 收发流水（每条 AT 一行；高频） ---- */
    exec_or_log(
        "CREATE TABLE IF NOT EXISTS at_log ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " device_id TEXT, ts INTEGER, dir TEXT, raw TEXT);");
    exec_or_log("CREATE INDEX IF NOT EXISTS idx_atlog_dev_ts "
                "ON at_log(device_id, ts DESC);");

    /* ---- llm_chat：LLM 对话（user / assistant / tool 三种 role） ---- */
    exec_or_log(
        "CREATE TABLE IF NOT EXISTS llm_chat ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " ts INTEGER, provider TEXT, model TEXT,"
        " role TEXT, content TEXT, tool_calls TEXT);");

    return AGENT_OK;
}

void storage_close(void)
{
    if (g_db) {
        sqlite3_close(g_db);
        g_db = NULL;
    }
}

void *storage_get_db(void)
{
    return g_db;
}
