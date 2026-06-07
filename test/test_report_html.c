/**
 * @file test_report_html.c
 * @brief report_html_export 单元测试。
 *
 * 用例：
 *  1. stub 模式（storage_init 返 IO 错）→ skip
 *  2. 真 SQLite 模式：调用 report_html_export → 检查文件非空 + 含关键字段
 *     （标题、Devices 表、AT Log 表）
 *  3. NULL 参数 → AGENT_ERR_BAD_ARG
 *  4. 反复调用覆盖旧文件无副作用
 */
#include "report_html.h"
#include "sqlite_db.h"
#include "agent_types.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#define HTML_PATH "out/test_report.html"
#define DB_PATH   "out/test_report.db"

int main(void)
{
    /* ---- 用例 3（先测，无需 DB）---- */
    assert(report_html_export(NULL) == AGENT_ERR_BAD_ARG);
    printf("test_report_html: null arg check OK\n");

    /* ---- stub 模式：DB 未打开，直接 skip ---- */
    remove(DB_PATH);
    remove(HTML_PATH);
    int rc = storage_init(DB_PATH);
    if (rc != AGENT_OK) {
        printf("test_report_html: skip (sqlite stub mode)\n");
        remove(DB_PATH);
        return 0;
    }

    /* ---- 真 SQLite 模式 ---- */
    /* 1) 空库导出：HTML 应包含标题 + 两个表 + "(no rows)" 类空提示 */
    remove(HTML_PATH);
    rc = report_html_export(HTML_PATH);
    assert(rc == AGENT_OK);
    FILE *fp = fopen(HTML_PATH, "r");
    assert(fp != NULL);
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fclose(fp);
    assert(sz > 100);  /* 至少要含 <html> + <h1> + 两段 <table> 标签 */
    printf("test_report_html: empty-db export OK (%ld bytes)\n", sz);

    /* 2) 关键字段校验：标题 + Devices + AT Log 表头 */
    fp = fopen(HTML_PATH, "r");
    assert(fp != NULL);
    static char buf[16384];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    buf[n] = '\0';
    fclose(fp);
    assert(strstr(buf, "Modem Agent Report") != NULL);
    assert(strstr(buf, "<h2>Devices</h2>") != NULL);
    assert(strstr(buf, "<h2>AT Log (recent 50)</h2>") != NULL);
    assert(strstr(buf, "<table>") != NULL);
    /* 空库时也应有 "No devices recorded." 之类 */
    assert(strstr(buf, "No devices recorded.") != NULL);
    assert(strstr(buf, "No AT log records.") != NULL);
    printf("test_report_html: key-fields check OK\n");

    /* 3) 覆盖调用无副作用 */
    rc = report_html_export(HTML_PATH);
    assert(rc == AGENT_OK);
    fp = fopen(HTML_PATH, "r");
    assert(fp != NULL);
    fseek(fp, 0, SEEK_END);
    sz = ftell(fp);
    fclose(fp);
    assert(sz > 100);
    printf("test_report_html: overwrite OK (%ld bytes)\n", sz);

    /* ---- 收尾 ---- */
    storage_close();
    remove(HTML_PATH);
    remove(DB_PATH);
    printf("test_report_html: 4/4 pass\n");
    return 0;
}
