/**
 * @file at_parser.c
 * @brief AT 行解析器实现。
 *
 * 状态：积累字符 → 遇 \r 或 \n 收尾 → 判类型 → 输出。
 * 连续 \r\n 合并：见到 \r\n 立即收尾；如果连续 \n\n 第二次空行忽略。
 * 裸 \r 兜底：若 \r 后跟普通字符而非 \n，把当前行先收尾，再把新字符作为下一行首字节。
 */
#include "at_parser.h"

#include <string.h>

/* 内部：行缓冲。本解析器只支持单条在拼行（at_session 会依次喂）。 */
static char     g_line[256];
static size_t   g_len = 0;
static bool     g_prev_was_cr = false;  /* 处理 \r\n 合并 */

/**
 * @brief 初始化解析器：清空行缓冲与 \r 标记。
 */
void at_parser_init(void)
{
    g_len = 0;
    g_prev_was_cr = false;
}

/**
 * @brief 判断字符串是否是 final error 类（ERROR / +CME ERROR / NO CARRIER 等）。
 */
static bool is_final_error(const char *s)
{
    if (strcmp(s, "ERROR") == 0) return true;
    if (strncmp(s, "+CME ERROR", 10) == 0) return true;
    if (strncmp(s, "+CMS ERROR", 10) == 0) return true;
    if (strcmp(s, "NO CARRIER") == 0) return true;
    if (strcmp(s, "NO DIALTONE") == 0) return true;
    if (strcmp(s, "BUSY") == 0) return true;
    if (strcmp(s, "NO ANSWER") == 0) return true;
    return false;
}

/**
 * @brief 喂一个字节。返回 true 表示拼成了一行（out 已被填充）。
 */
bool at_parser_feed(uint8_t b, at_line_t *out)
{
    if (b == '\r') {
        g_prev_was_cr = true;
        return false;
    }
    if (b == '\n') {
        g_prev_was_cr = false;
        if (g_len == 0) {
            return false;
        }
        g_line[g_len] = '\0';
        out->len = g_len;
        memcpy(out->line, g_line, g_len + 1);
        if (strcmp(out->line, "OK") == 0) {
            out->type = AT_LINE_FINAL_OK;
        } else if (is_final_error(out->line)) {
            out->type = AT_LINE_FINAL_ERROR;
        } else if (out->line[0] == '+') {
            out->type = AT_LINE_URC;
        } else {
            out->type = AT_LINE_DATA;
        }
        g_len = 0;
        return true;
    }
    /* 普通字节：如果之前是裸 \r（跟的不是 \n），先把那行收尾 */
    if (g_prev_was_cr && g_len > 0) {
        g_line[g_len] = '\0';
        out->len = g_len;
        memcpy(out->line, g_line, g_len + 1);
        if (strcmp(out->line, "OK") == 0) {
            out->type = AT_LINE_FINAL_OK;
        } else if (is_final_error(out->line)) {
            out->type = AT_LINE_FINAL_ERROR;
        } else if (out->line[0] == '+') {
            out->type = AT_LINE_URC;
        } else {
            out->type = AT_LINE_DATA;
        }
        g_len = 0;
        g_prev_was_cr = false;
        /* fall-through：当前字节作为新行首字节 */
    }
    if (g_len < sizeof(g_line) - 1) {
        g_line[g_len++] = (char)b;
    } else {
        g_len = 0;
    }
    return false;
}
