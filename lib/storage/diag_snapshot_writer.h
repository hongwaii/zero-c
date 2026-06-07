/**
 * @file diag_snapshot_writer.h
 * @brief diag_snapshot 表写入：单台模组诊断状态 → SQLite 持久化。
 *
 * 写入策略：同步小流量（每分钟一次或刷新后一次，量小）。
 * 主键：PRIMARY KEY (device_id, ts)——同设备同秒刷新会被 REPLACE 覆盖。
 * 字段：csq/rsrp/rsrq/snr/operator/rat/cereg/ps_attached。
 * stub 模式：storage_get_db() 返 NULL → 直接返 AGENT_ERR_IO。
 */
#ifndef LIB_STORAGE_DIAG_SNAPSHOT_WRITER_H
#define LIB_STORAGE_DIAG_SNAPSHOT_WRITER_H

#include "agent_types.h"
#include "diag_state.h"  /* diag_state_t from lib/diag_service */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化（占位：当前版本不申请资源，留作 v1.1 异步队列扩展）。
 * @return AGENT_OK
 */
int  diag_snapshot_writer_init(void);

/**
 * @brief 关闭（占位）。
 */
void diag_snapshot_writer_close(void);

/**
 * @brief 把当前 diag_state_t 写入一行 diag_snapshot。
 * @param dev_id  设备 ID（device_manager 派发）
 * @param st      诊断状态（csq/cereg/operator/rat 等）
 * @return AGENT_OK 成功；AGENT_ERR_BAD_ARG 参数为空；AGENT_ERR_IO DB 未初始化或写失败
 *
 * INSERT OR REPLACE：同 (device_id, ts) 重复插入会覆盖前一行的指标。
 * 字符串字段（csq/cereg）用 atoi 转 int 存；operator/rat 原样存。
 */
int  diag_snapshot_writer_add(const char *dev_id, const diag_state_t *st);

/**
 * @brief 取最近 N 条 diag_snapshot（按 ts DESC）。
 * @param dev_id  设备 ID
 * @param n       最多取几条
 * @param out     输出数组（调用方分配，至少 n 个 diag_state_t）
 * @param got     实际取到的条数
 * @return AGENT_OK 成功；AGENT_ERR_NOT_IMPL 当前版本未实现（v1.0 简化）
 *
 * v1.0 简化：不实现 select（test_at_session 不需要）。v1.1 补 roundtrip。
 */
int  diag_snapshot_writer_recent(const char *dev_id, int n,
                                 diag_state_t *out, int *got);

#ifdef __cplusplus
}
#endif

#endif /* LIB_STORAGE_DIAG_SNAPSHOT_WRITER_H */
