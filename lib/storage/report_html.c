/**
 * @file report_html.c
 * @brief 生成 self-contained HTML 报告（设备清单 + AT 收发最近 50 条）。
 *
 * 实现要点：
 *  - html_escape：转义 <, >, &, " 四个字符，防止注入/破坏标签。
 *  - write_css：内嵌 <style> 块，无任何外部资源（单文件可移植）。
 *  - write_devices / write_at_log：分别查询对应表，遍历结果写 <tr><td>。
 *    数值字段（last_seen / ts）走 fprintf %d；字符串字段走 html_escape。
 *  - report_html_export：参数校验 → 取 DB 句柄 → fopen → 写头/CSS/两表/尾 → fclose。
 *
 * stub 模式：storage_get_db() 返 NULL → 直接返 AGENT_ERR_IO，不 fopen。
 */
#include "report_html.h"
#include "sqlite_db.h"

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ---- HTML escape：写 s 到 f，转义 <, >, &, "（' 单引号不在属性内可不转） ---- */
static void html_escape(FILE *f, const char *s)
{
    if (!s) return;
    for (; *s; s++) {
        switch (*s) {
            case '<':  fputs("&lt;",   f); break;
            case '>':  fputs("&gt;",   f); break;
            case '&':  fputs("&amp;",  f); break;
            case '"':  fputs("&quot;", f); break;
            default:   fputc(*s, f);
        }
    }
}

/* ---- 嵌入 CSS：gh-dark 风格 ---- */
static void write_css(FILE *f)
{
    fputs(
        "<style>"
        "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;"
             "background:#0d1117;color:#c9d1d9;padding:24px;margin:0;}"
        "h1{color:#58a6ff;margin:0 0 8px 0;}"
        "h2{color:#79c0ff;border-bottom:1px solid #30363d;padding-bottom:4px;"
            "margin-top:32px;}"
        ".meta{color:#8b949e;margin:0 0 16px 0;}"
        "table{border-collapse:collapse;width:100%;margin-bottom:24px;}"
        "th,td{border:1px solid #30363d;padding:6px 10px;text-align:left;"
              "vertical-align:top;}"
        "th{background:#161b22;color:#c9d1d9;font-weight:600;}"
        "tr:nth-child(even){background:#161b22;}"
        "code{background:#161b22;padding:2px 4px;border-radius:3px;"
              "font-family:Consolas,Monaco,monospace;font-size:0.9em;}"
        ".empty{color:#8b949e;font-style:italic;padding:12px 0;}"
        "</style>", f);
}

/* ---- device 表 ---- */
static void write_devices(FILE *f, sqlite3 *db)
{
    fputs("<h2>Devices</h2>", f);
    sqlite3_stmt *s = NULL;
    int rc = sqlite3_prepare_v2(db,
        "SELECT id, label, chan_uri, last_seen FROM device ORDER BY last_seen DESC",
        -1, &s, NULL);
    if (rc != SQLITE_OK) {
        fputs("<p class=\"empty\">(query failed)</p>", f);
        return;
    }
    fputs("<table><tr><th>ID</th><th>Label</th><th>URI</th><th>Last Seen</th></tr>", f);
    int rows = 0;
    while (sqlite3_step(s) == SQLITE_ROW) {
        fputs("<tr>", f);
        fputs("<td><code>", f);
        html_escape(f, (const char *)sqlite3_column_text(s, 0));
        fputs("</code></td>", f);
        fputs("<td>", f);
        html_escape(f, (const char *)sqlite3_column_text(s, 1));
        fputs("</td>", f);
        fputs("<td><code>", f);
        html_escape(f, (const char *)sqlite3_column_text(s, 2));
        fputs("</code></td>", f);
        fprintf(f, "<td>%d</td>", sqlite3_column_int(s, 3));
        fputs("</tr>", f);
        rows++;
    }
    sqlite3_finalize(s);
    fputs("</table>", f);
    if (rows == 0) {
        fputs("<p class=\"empty\">No devices recorded.</p>", f);
    }
}

/* ---- at_log 表：最近 50 条（按 id DESC 倒序） ---- */
static void write_at_log(FILE *f, sqlite3 *db)
{
    fputs("<h2>AT Log (recent 50)</h2>", f);
    sqlite3_stmt *s = NULL;
    int rc = sqlite3_prepare_v2(db,
        "SELECT ts, device_id, dir, raw FROM at_log ORDER BY id DESC LIMIT 50",
        -1, &s, NULL);
    if (rc != SQLITE_OK) {
        fputs("<p class=\"empty\">(query failed)</p>", f);
        return;
    }
    fputs("<table><tr><th>Time</th><th>Device</th><th>Dir</th><th>Raw</th></tr>", f);
    int rows = 0;
    while (sqlite3_step(s) == SQLITE_ROW) {
        fputs("<tr>", f);
        fprintf(f, "<td>%d</td>", sqlite3_column_int(s, 0));
        fputs("<td><code>", f);
        html_escape(f, (const char *)sqlite3_column_text(s, 1));
        fputs("</code></td>", f);
        fputs("<td>", f);
        html_escape(f, (const char *)sqlite3_column_text(s, 2));
        fputs("</td>", f);
        fputs("<td><code>", f);
        html_escape(f, (const char *)sqlite3_column_text(s, 3));
        fputs("</code></td>", f);
        fputs("</tr>", f);
        rows++;
    }
    sqlite3_finalize(s);
    fputs("</table>", f);
    if (rows == 0) {
        fputs("<p class=\"empty\">No AT log records.</p>", f);
    }
}

int report_html_export(const char *out_html_path)
{
    if (!out_html_path) return AGENT_ERR_BAD_ARG;

    sqlite3 *db = (sqlite3 *)storage_get_db();
    if (!db) return AGENT_ERR_IO;  /* stub 模式 / 未初始化 */

    FILE *f = fopen(out_html_path, "w");
    if (!f) {
        fprintf(stderr, "report_html: fopen(%s) failed\n", out_html_path);
        return AGENT_ERR_IO;
    }

    /* ---- HTML 头 + meta ---- */
    time_t now = time(NULL);
    fprintf(f, "<!DOCTYPE html>\n");
    fprintf(f, "<html lang=\"en\"><head><meta charset=\"utf-8\">");
    fprintf(f, "<title>Modem Agent Report</title>");
    write_css(f);
    fprintf(f, "</head><body>");
    fprintf(f, "<h1>Modem Agent Report</h1>");
    fprintf(f, "<p class=\"meta\">Generated: %s</p>", ctime(&now));

    /* ---- 两个表 ---- */
    write_devices(f, db);
    write_at_log(f, db);

    /* ---- 收尾 ---- */
    fputs("</body></html>\n", f);
    fclose(f);
    return AGENT_OK;
}
