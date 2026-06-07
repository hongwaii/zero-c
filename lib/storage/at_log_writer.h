/**
 * @file at_log_writer.h
 * @brief AT 收发日志写入：高频写 at_log 表 + 失败重试 + file 降级。
 *
 * v1.0 简化策略：
 *   - 同步写库（不开线程）——理由：SQLite WAL 单写者，多线程收益小且增加复杂度。
 *   - 失败重试 3 次，每次间隔 50ms（spec §6：lock / busy / disk full 重试）。
 *   - 重试全部失败后降级到 `logs/at_log_fallback.log` 追加模式（不阻塞 AT 主流程）。
 *   - 降级成功也返 AGENT_OK——不算错误（已兜底）。
 *
 * dir 取值："TX"（下发）/ "RX"（设备响应）/ "URC"（设备主动上报）。
 */
#ifndef LIB_STORAGE_AT_LOG_WRITER_H
#define LIB_STORAGE_AT_LOG_WRITER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 at_log_writer（v1.0 简化版：空操作，预留后续异步 worker 启动位）。
 * @return AGENT_OK 总是
 */
int  at_log_writer_init(void);

/**
 * @brief 关闭 at_log_writer（v1.0 简化版：空操作，预留后续 flush + 线程 join 位）。
 */
void at_log_writer_close(void);

/**
 * @brief 写入一条 AT 收发记录到 at_log 表。
 * @param dev_id  设备 ID（device.id，如 "MDM-1"）；不能为 NULL
 * @param dir     方向："TX" / "RX" / "URC"；不能为 NULL
 * @param raw     原始字节（UTF-8 / ASCII 字符串）；不能为 NULL
 * @return AGENT_OK 写库成功或已降级到 file；
 *         AGENT_ERR_BAD_ARG 任一参数为 NULL；
 *         其他错误码见 storage_get_db / sqlite3 自身
 *
 * 重试策略：3 次，每次 50ms。非 IO 错（如 BAD_ARG）不重试，直接返错。
 */
int  at_log_writer_add(const char *dev_id, const char *dir, const char *raw);

/**
 * @brief 强制 flush（v1.0 简化版：空操作，sync write 无需 flush）。
 *
 * 预留位：未来若改异步 worker 队列实现，UI 关闭前调此函数确保积压日志全部落库。
 */
void at_log_writer_flush(void);

#ifdef __cplusplus
}
#endif

#endif /* LIB_STORAGE_AT_LOG_WRITER_H */
