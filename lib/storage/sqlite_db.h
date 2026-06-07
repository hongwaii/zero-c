/**
 * @file sqlite_db.h
 * @brief SQLite 包装：WAL 模式、schema 初始化、错误重试。
 *
 * 异步写：at_log 走 writer 队列；diag / llm 同步写（小流量）。
 * 错误重试：lock/busy 重试 3 次，每次 50ms；其他错返回错误码。
 *
 * 4 张表：device / diag_snapshot / at_log / llm_chat。
 * 索引：diag_snapshot(ts DESC) / at_log(device_id, ts DESC)。
 */
#ifndef LIB_STORAGE_SQLITE_DB_H
#define LIB_STORAGE_SQLITE_DB_H

#include <stdbool.h>
#include "agent_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 打开 SQLite 数据库 + 启用 WAL + 初始化 4 张表 schema。
 * @param db_path  数据库文件路径（不存在则自动创建）
 * @return AGENT_OK 成功；AGENT_ERR_IO 打开失败；其他错误见 sqlite3_open 返回值
 *
 * 幂等：重复调用不会重复打开；首次成功后 g_db 缓存为单例。
 * stub 模式下始终返回 AGENT_ERR_IO（sqlite3_open stub 返回 ERROR）。
 */
int  storage_init(const char *db_path);

/** @brief 关闭数据库并清空单例句柄；幂等。 */
void storage_close(void);

/**
 * @brief 取底层 sqlite3* 句柄（给 writer 模块直接 prepared stmt 复用）。
 * @return 非 NULL = 已初始化；NULL = 未初始化
 */
void *storage_get_db(void);

#ifdef __cplusplus
}
#endif

#endif /* LIB_STORAGE_SQLITE_DB_H */
