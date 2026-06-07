/*
 * sqlite3.c —— SQLite 3.46 stub
 *
 * 沙箱无网 / 网络不稳定时使用。真实实现应从 sqlite.org 拉 amalgamation 后覆盖本文件。
 * 所有函数一律返回 SQLITE_ERROR / SQLITE_OK / NULL —— 不做实际工作。
 */

#include "sqlite3.h"

/* 全局 stub 错误码，UI 调试时能看出"没接真 SQLite"。 */
static int g_last_errcode = SQLITE_ERROR;

/* sqlite3_open：始终失败，*ppDb = NULL。 */
int sqlite3_open(const char *filename, sqlite3 **ppDb)
{
    (void)filename;
    if (ppDb) *ppDb = NULL;
    g_last_errcode = SQLITE_ERROR;
    return SQLITE_ERROR;
}

/* sqlite3_close：NULL 句柄视为成功。 */
int sqlite3_close(sqlite3 *db)
{
    (void)db;
    return SQLITE_OK;
}

/* sqlite3_exec：拒绝所有 SQL。 */
int sqlite3_exec(sqlite3 *db, const char *sql,
                 int (*cb)(void *, int, char **, char **),
                 void *arg, char **errmsg)
{
    (void)db; (void)sql; (void)cb; (void)arg;
    if (errmsg) *errmsg = NULL;
    g_last_errcode = SQLITE_ERROR;
    return SQLITE_ERROR;
}

/* sqlite3_prepare_v2：永远拿不到 stmt。 */
int sqlite3_prepare_v2(sqlite3 *db, const char *zSql, int nByte,
                       sqlite3_stmt **ppStmt, const char **pzTail)
{
    (void)db; (void)zSql; (void)nByte;
    if (ppStmt) *ppStmt = NULL;
    if (pzTail) *pzTail = NULL;
    g_last_errcode = SQLITE_ERROR;
    return SQLITE_ERROR;
}

int sqlite3_step(sqlite3_stmt *pStmt)
{
    (void)pStmt;
    g_last_errcode = SQLITE_ERROR;
    return SQLITE_ERROR;
}

int sqlite3_finalize(sqlite3_stmt *pStmt)
{
    (void)pStmt;
    return SQLITE_OK;
}

int sqlite3_bind_text(sqlite3_stmt *pStmt, int index, const char *value,
                      int n, void (*dt)(void *))
{
    (void)pStmt; (void)index; (void)value; (void)n; (void)dt;
    return SQLITE_ERROR;
}

int sqlite3_bind_int(sqlite3_stmt *pStmt, int index, int value)
{
    (void)pStmt; (void)index; (void)value;
    return SQLITE_ERROR;
}

int sqlite3_bind_int64(sqlite3_stmt *pStmt, int index, long long value)
{
    (void)pStmt; (void)index; (void)value;
    return SQLITE_ERROR;
}

const unsigned char *sqlite3_column_text(sqlite3_stmt *pStmt, int iCol)
{
    (void)pStmt; (void)iCol;
    return NULL;
}

int sqlite3_column_int(sqlite3_stmt *pStmt, int iCol)
{
    (void)pStmt; (void)iCol;
    return 0;
}

long long sqlite3_column_int64(sqlite3_stmt *pStmt, int iCol)
{
    (void)pStmt; (void)iCol;
    return 0LL;
}

int sqlite3_errmsg(sqlite3 *db, char *buf, int buf_size)
{
    const char *msg = "sqlite3 stub: not implemented (vendor real amalgamation)";
    int n = 0;
    (void)db;
    if (!buf || buf_size <= 0) return 0;
    while (msg[n] && n < buf_size - 1) { buf[n] = msg[n]; n++; }
    buf[n] = '\0';
    return n;
}

const char *sqlite3_errmsg_str(sqlite3 *db)
{
    (void)db;
    return "sqlite3 stub: not implemented (vendor real amalgamation)";
}

int sqlite3_errcode(sqlite3 *db)
{
    (void)db;
    return g_last_errcode;
}

/* sqlite3_free：释放 sqlite3_exec / sqlite3_mprintf 等返回的字符串。
 * stub 模式下 sqlite3_exec 不分配字符串（errmsg=NULL），但调用方仍可能
 * 防御性 free 一个 NULL；为安全起见提供实现。 */
void sqlite3_free(void *ptr)
{
    (void)ptr;
    /* stub 模式不分配 → 无操作；真 amalgamation 此处调 free() */
}
