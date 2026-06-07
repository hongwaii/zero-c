/**
 * @file report_html.h
 * @brief 读 sqlite 生成 self-contained HTML 报告（CSS 内嵌，零外部依赖）。
 *
 * 设计要点：
 *  - 零外部依赖：所有 CSS 嵌入 <style> 块，HTML 单文件可双击在浏览器打开。
 *  - 暗黑 GitHub 风格：参考 gh-dark 配色（背景 #0d1117、文字 #c9d1d9）。
 *  - 内容：device 表（设备清单）+ at_log 最近 50 条（按 id DESC 倒序）。
 *  - 安全：所有数据库取出的字符串先 HTML escape（<, >, &, "）再写入。
 *  - 错误：DB 未初始化 / fopen 失败返 AGENT_ERR_IO；非 NULL 参数校验。
 *  - stub 模式：storage_get_db() 返 NULL，report_html_export 直接返 AGENT_ERR_IO；
 *    UI 调用方需在导出前自己判定。
 *
 * v1.0 简化：只输出 device + at_log 两表。diag_snapshot / llm_chat 留给 v1.1。
 */
#ifndef LIB_STORAGE_REPORT_HTML_H
#define LIB_STORAGE_REPORT_HTML_H

#include "agent_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 把当前数据库的 device + at_log 导出到 self-contained HTML 文件。
 * @param out_html_path  输出的 .html 文件路径；不能为 NULL
 * @return AGENT_OK 成功；AGENT_ERR_BAD_ARG 参数为 NULL；AGENT_ERR_IO DB 未初始化 / 文件打开失败
 *
 * 行为：
 *  - 已 export 的旧文件会被覆盖（"w" 模式）。
 *  - 文件 IO 失败但 DB 句柄存在时，仍可能写出部分内容（best-effort）。
 *  - 即使表为空，HTML 仍生成（标题 + 两个空表 + "no rows" 隐含）。
 *
 * 典型调用方：panel_settings "导出报告" 按钮 → 写到 logs/report.html。
 */
int  report_html_export(const char *out_html_path);

#ifdef __cplusplus
}
#endif

#endif /* LIB_STORAGE_REPORT_HTML_H */
