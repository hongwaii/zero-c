/**
 * @file diag_health.h
 * @brief 一键健康检查：组合 6 条 AT 刷新 + 1 次 TCP ping + 1 次 SSL 探活
 *        + 1 次 log 抓取，串联成状态机，最终落盘一份 JSON 报告。
 *
 * 状态机（HC_S_*）：
 *   IDLE → REFRESH → PING → SSL → LOG → DONE
 *
 * 约束：
 *   - 每步用 uv_timer 异步推进，不阻塞 main loop
 *   - 任一子检查失败：把描述塞到 JSON 的 errors[] 数组，继续下一步
 *   - 全部走完 → cJSON_Print 写文件 → 调 cb
 *   - 全成功 + 报告写成功 → cb(true)；AT 刷新失败（最关键）→ cb(false)
 *
 * 异步 API：返回 AGENT_OK 表示已开始；完成时在 main loop 上调 cb。
 */
#ifndef LIB_DIAG_HEALTH_H
#define LIB_DIAG_HEALTH_H

#include <stdbool.h>
#include <stddef.h>

/* 前向声明：避免拉 libuv 头进本公开头 */
struct uv_loop_s;
typedef struct uv_loop_s uv_loop_t;

struct at_session;
typedef struct at_session at_session_t;

struct device_manager;
typedef struct device_manager device_manager_t;

/**
 * @brief 一键健康检查完成回调（main loop 上触发）。
 * @param ok         true = JSON 报告已写成功（即使子检查有错也 ok=true，
 *                   只要报告本身落盘；只有 AT 刷新失败才 false）
 * @param json_path  报告文件路径（caller 提供的 out_json_path）
 * @param userdata   注册时传入
 */
typedef void (*diag_health_cb)(bool ok, const char *json_path, void *userdata);

/**
 * @brief 启动一次一键健康检查。
 *
 * @param loop           libuv main loop（异步推进用）
 * @param dm             device_manager（取 READY 设备 + at_session）
 * @param dev_idx        设备索引（>= 0）
 * @param out_json_path  JSON 报告输出路径（caller 保证父目录存在）
 * @param cb             完成回调（必须非 NULL）
 * @param userdata       透传给 cb
 * @return AGENT_OK / AGENT_ERR_BAD_ARG / AGENT_ERR_OOM
 *
 * 报告 schema（强制）：
 *   {
 *     "ts":           "2026-06-07T12:00:00",
 *     "dev_idx":      0,
 *     "dev_label":    "COM3",
 *     "csq":          "23",
 *     "cereg":        "5",
 *     "cop_operator": "China Mobile",
 *     "imei":         "...",
 *     "imsi":         "...",
 *     "iccid":        "...",
 *     "rat":          "LTE",
 *     "ping":  { "host": "...", "port": 80, "ok": true,  "ms": 23 },
 *     "ssl":   { "url":  "...", "status": 200, "issuer": "...", "expiry": "...", "ok": true },
 *     "log":   { "path": "...", "size": 1234, "ok": true },
 *     "errors": [ "ping: timeout", ... ]
 *   }
 *
 * TEST_MODE 说明：
 *   - #define DIAG_HEALTH_TEST_MODE 1 时所有子检查走假数据路径，
 *     5 秒内同步返回（实际 < 100ms）
 *   - 这让 test 能在无模组/无网络环境下跑通
 */
int  diag_health_check(uv_loop_t *loop, device_manager_t *dm, int dev_idx,
                       const char *out_json_path, diag_health_cb cb, void *userdata);

#endif /* LIB_DIAG_HEALTH_H */
