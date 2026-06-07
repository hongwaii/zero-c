/**
 * @file test_at_parser.c
 * @brief AT 行解析器单元测试。
 *
 * 覆盖：OK / ERROR / +CME ERROR / +CSQ 响应 / URC / 空行 / LF-only。
 * 8 个测试函数，共 13 个 CHECK。
 */
#include "at_parser.h"

#include <stdio.h>
#include <string.h>

/* 简单的 CHECK 宏：累计检查数与失败数 */
static int g_checks = 0, g_failed = 0;
#define CHECK(c, m) do { \
    g_checks++; \
    if (!(c)) { \
        g_failed++; \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, m); \
    } \
} while (0)

/**
 * @brief 把字符串喂给 parser，把每行结果累积到 lines 数组。
 * @return 解析出的行数
 */
static size_t feed_string(const char *s, at_line_t *lines, size_t max)
{
    at_parser_init();
    size_t n = 0;
    for (const char *p = s; *p; p++) {
        at_line_t line;
        if (at_parser_feed((uint8_t)*p, &line)) {
            if (n < max) lines[n++] = line;
        }
    }
    return n;
}

/* OK\r\n → 一行 "OK"，类型 FINAL_OK */
static int test_simple_ok(void)
{
    at_line_t lines[8];
    size_t n = feed_string("OK\r\n", lines, 8);
    CHECK(n == 1, "1 line emitted");
    CHECK(strcmp(lines[0].line, "OK") == 0, "line == OK");
    CHECK(lines[0].type == AT_LINE_FINAL_OK, "type == FINAL_OK");
    return 0;
}

/* AT\r\nOK\r\n → 两行：echo "AT" + final "OK"。
 * 注：at_parser 不知道什么是 echo，原始字符流里只要是 \r\n 收尾就当一行。
 * 真正的 echo 抑制由 at_session（前置状态机）处理，P3-T2 负责。 */
static int test_at_echo(void)
{
    at_line_t lines[8];
    size_t n = feed_string("AT\r\nOK\r\n", lines, 8);
    CHECK(n == 2, "2 lines (echo + final)");
    CHECK(strcmp(lines[0].line, "AT") == 0, "first == AT");
    CHECK(strcmp(lines[1].line, "OK") == 0, "second == OK");
    CHECK(lines[1].type == AT_LINE_FINAL_OK, "FINAL_OK");
    return 0;
}

/* 纯 ERROR 行 */
static int test_error(void)
{
    at_line_t lines[8];
    size_t n = feed_string("ERROR\r\n", lines, 8);
    CHECK(n == 1, "1 line");
    CHECK(lines[0].type == AT_LINE_FINAL_ERROR, "FINAL_ERROR");
    return 0;
}

/* +CME ERROR: ... 也算 FINAL_ERROR */
static int test_cme_error(void)
{
    at_line_t lines[8];
    size_t n = feed_string("+CME ERROR: SIM not inserted\r\n", lines, 8);
    CHECK(n == 1, "1 line");
    CHECK(lines[0].type == AT_LINE_FINAL_ERROR, "FINAL_ERROR");
    return 0;
}

/* 多行响应：+CSQ: 23,99 后跟空行 + OK；空行被跳过。
 * 注：at_parser 看到行首 '+' 就归 URC。真正的 data/URC 区分由
 * at_session 拿到当前 pending 命令的预期响应后做前缀匹配——P3-T2 负责。 */
static int test_csq_response(void)
{
    at_line_t lines[8];
    size_t n = feed_string("+CSQ: 23,99\r\n\r\nOK\r\n", lines, 8);
    CHECK(n == 2, "2 lines (CSQ + OK, empty line skipped)");
    CHECK(strcmp(lines[0].line, "+CSQ: 23,99") == 0, "first == +CSQ: 23,99");
    CHECK(lines[0].type == AT_LINE_URC, "URC (parser 默认归类)");
    CHECK(strcmp(lines[1].line, "OK") == 0, "OK");
    CHECK(lines[1].type == AT_LINE_FINAL_OK, "FINAL_OK");
    return 0;
}

/* URC 行（无前缀 final / data） */
static int test_urc(void)
{
    at_line_t lines[8];
    size_t n = feed_string("+CEREG: 5\r\n", lines, 8);
    CHECK(n == 1, "1 line");
    CHECK(strcmp(lines[0].line, "+CEREG: 5") == 0, "line");
    CHECK(lines[0].type == AT_LINE_URC, "URC");
    return 0;
}

/* URC：+CMTI 带引号 */
static int test_cmti_urc(void)
{
    at_line_t lines[8];
    size_t n = feed_string("+CMTI: \"SM\",5\r\n", lines, 8);
    CHECK(n == 1, "1 line");
    CHECK(lines[0].type == AT_LINE_URC, "URC");
    return 0;
}

/* LF-only 收尾（mod 端有时候只发 \n） */
static int test_lf_only(void)
{
    at_line_t lines[8];
    size_t n = feed_string("OK\n", lines, 8);
    CHECK(n == 1, "1 line (LF only)");
    CHECK(lines[0].type == AT_LINE_FINAL_OK, "FINAL_OK");
    return 0;
}

/* 多余空行被全部忽略 */
static int test_empty_lines_skipped(void)
{
    at_line_t lines[8];
    size_t n = feed_string("\r\n\r\nOK\r\n\r\n", lines, 8);
    CHECK(n == 1, "1 line (empty skipped)");
    CHECK(lines[0].type == AT_LINE_FINAL_OK, "FINAL_OK");
    return 0;
}

int main(void)
{
    test_simple_ok();
    test_at_echo();
    test_error();
    test_cme_error();
    test_csq_response();
    test_urc();
    test_cmti_urc();
    test_lf_only();
    test_empty_lines_skipped();
    printf("test_at_parser: %d/%d pass\n", g_checks - g_failed, g_checks);
    return g_failed == 0 ? 0 : 1;
}
