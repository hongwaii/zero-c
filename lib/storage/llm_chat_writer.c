/**
 * @file llm_chat_writer.c
 * @brief llm_chat 表 INSERT 实现。
 *
 * 流程：
 *  1. 取 storage_get_db()（stub 模式下为 NULL → 返 AGENT_ERR_IO）
 *  2. sqlite3_prepare_v2 编译 INSERT
 *  3. bind 6 个字段（ts / provider / model / role / content / tool_calls）
 *     —— NULL 参数用 "" 替代（schema 列允许空但写时显式空串便于 SELECT）
 *  4. sqlite3_step + sqlite3_finalize
 *
 * 错误处理：
 *  - DB 未初始化（stub 模式） → AGENT_ERR_IO
 *  - sqlite3_prepare/step 失败 → AGENT_ERR_IO（错误信息打到 stderr）
 */
#include "llm_chat_writer.h"
#include "sqlite_db.h"

#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/**
 * @brief 把 NULL 字符串转成 ""，便于 SQLite 列存显式空串。
 */
static const char *nz(const char *s) { return s ? s : ""; }

int llm_chat_writer_add(const char *provider, const char *model,
                        const char *role,    const char *content,
                        const char *tool_calls_json)
{
    /* ---- 取 DB 句柄（stub 模式下为 NULL） ---- */
    sqlite3 *db = (sqlite3 *)storage_get_db();
    if (!db) {
        fprintf(stderr, "llm_chat_writer_add: storage not initialized\n");
        return AGENT_ERR_IO;
    }

    /* ---- 准备 INSERT 语句 ---- */
    const char *sql =
        "INSERT INTO llm_chat(ts, provider, model, role, content, tool_calls)"
        " VALUES (?, ?, ?, ?, ?, ?)";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "llm_chat_writer_add: prepare failed: %d\n", rc);
        return AGENT_ERR_IO;
    }

    /* ---- 绑定参数 ---- */
    sqlite3_bind_int (stmt, 1, (int)time(NULL));
    sqlite3_bind_text(stmt, 2, nz(provider),         -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, nz(model),            -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, nz(role),             -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, nz(content),          -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, nz(tool_calls_json),  -1, SQLITE_TRANSIENT);

    /* ---- 执行 ---- */
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "llm_chat_writer_add: step failed: %d\n", rc);
        return AGENT_ERR_IO;
    }
    return AGENT_OK;
}

int  llm_chat_writer_init(void)  { return AGENT_OK; }
void llm_chat_writer_close(void) { (void)0; }
