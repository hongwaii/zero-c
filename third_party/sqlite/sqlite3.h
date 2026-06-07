/*
 * sqlite3.h —— SQLite 3.46 stub (network 不可达时的占位实现)
 *
 * 本头文件只声明了 v1.0 持久化层会用到的最小 API 子集。
 * 真实 SQLite amalgamation 应从 https://www.sqlite.org/2024/sqlite-amalgamation-3460100.zip
 * 下载后替换本文件。
 *
 * stub 模式下：
 *   - sqlite3_open 始终返回 SQLITE_ERROR 并把 *ppDb 置为 NULL
 *   - 所有 prepare/step/finalize 同样返回 SQLITE_ERROR
 *   - 任何 P6 存储测试在 stub 模式下都会失败 —— 这是预期行为
 *   - UI 层用 mock 验证即可，不影响其它模块编译
 */

#ifndef SQLITE3_H
#define SQLITE3_H

#include <stddef.h>

/* ---- 不透明类型 ---- */
typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;

/* ---- 状态码（与官方一致） ---- */
#define SQLITE_OK         0
#define SQLITE_ERROR      1
#define SQLITE_BUSY       5
#define SQLITE_ROW      100
#define SQLITE_DONE     101

/* ---- 关键 API（只声明需要的；其他功能按需追加） ---- */
int  sqlite3_open(const char *filename, sqlite3 **ppDb);
int  sqlite3_close(sqlite3 *db);
int  sqlite3_exec(sqlite3 *db, const char *sql,
                  int (*cb)(void *, int, char **, char **),
                  void *arg, char **errmsg);
int  sqlite3_prepare_v2(sqlite3 *db, const char *zSql, int nByte,
                        sqlite3_stmt **ppStmt, const char **pzTail);
int  sqlite3_step(sqlite3_stmt *pStmt);
int  sqlite3_finalize(sqlite3_stmt *pStmt);
int  sqlite3_bind_text(sqlite3_stmt *pStmt, int index, const char *value,
                       int n, void (*dt)(void *));
int  sqlite3_bind_int(sqlite3_stmt *pStmt, int index, int value);
int  sqlite3_bind_int64(sqlite3_stmt *pStmt, int index, long long value);
const unsigned char *sqlite3_column_text(sqlite3_stmt *pStmt, int iCol);
int                  sqlite3_column_int (sqlite3_stmt *pStmt, int iCol);
long long            sqlite3_column_int64(sqlite3_stmt *pStmt, int iCol);
int  sqlite3_errmsg(sqlite3 *db, char *buf, int buf_size);
const char *sqlite3_errmsg_str(sqlite3 *db);
int  sqlite3_errcode(sqlite3 *db);

/* 释放 sqlite3_exec / sqlite3_mprintf 等返回的字符串 */
void sqlite3_free(void *ptr);

#endif /* SQLITE3_H */
