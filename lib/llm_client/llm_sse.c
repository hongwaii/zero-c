/**
 * @file llm_sse.c
 * @brief SSE 字节流解析器实现。
 *
 * 状态机：
 *   LINE_START: 行首，决定是 data: / [DONE] / : 注释 / 空
 *   IN_DATA: data: 后累积字节
 *   AFTER_LF: 收到 \n 后看下一个字符
 *
 * 行边界：\n（CR 单独出现视为数据）。事件边界：\n\n。
 */
#include "llm_sse.h"
#include <stdlib.h>
#include <string.h>

#define LINE_BUF_MAX 4096
#define EVENT_PAYLOAD_MAX 8192

struct sse_parser {
    sse_event_cb cb;
    void        *userdata;
    char         line_buf[LINE_BUF_MAX];
    size_t       line_len;
    char         payload_buf[EVENT_PAYLOAD_MAX];
    size_t       payload_len;
    bool         in_data;     /* 当前 line 是不是 data: 行 */
};

sse_parser_t *sse_parser_create(sse_event_cb cb, void *ud)
{
    if (!cb) return NULL;
    sse_parser_t *p = (sse_parser_t *)calloc(1, sizeof(*p));
    if (!p) return NULL;
    p->cb = cb;
    p->userdata = ud;
    return p;
}

void sse_parser_destroy(sse_parser_t *p) { free(p); }

/* 内部：当前 line 收尾（遇到 \n 时调） */
static void on_line(sse_parser_t *p)
{
    /* 去掉尾部 \r */
    if (p->line_len > 0 && p->line_buf[p->line_len - 1] == '\r')
        p->line_buf[--p->line_len] = '\0';

    if (p->line_len == 0) {
        /* 空行 = 事件边界 */
        if (p->payload_len > 0) {
            /* 检 [DONE] */
            sse_event_t ev = {0};
            if (p->payload_len == 6 && memcmp(p->payload_buf, "[DONE]", 6) == 0) {
                ev.type = SSE_EV_DONE;
            } else {
                ev.type = SSE_EV_DATA;
                ev.payload = p->payload_buf;
                ev.payload_len = p->payload_len;
            }
            p->cb(&ev, p->userdata);
            p->payload_len = 0;
        }
    } else if (p->line_buf[0] == ':') {
        /* 注释——忽略 */
    } else if (strncmp(p->line_buf, "data: ", 6) == 0 || strcmp(p->line_buf, "data:") == 0) {
        /* data 行 */
        const char *data = (p->line_buf[0] == 'd') ? p->line_buf + 6 : p->line_buf + 5;
        if (p->payload_len > 0 && p->payload_len < EVENT_PAYLOAD_MAX) {
            p->payload_buf[p->payload_len++] = '\n';
        }
        size_t dlen = strlen(data);
        if (p->payload_len + dlen < EVENT_PAYLOAD_MAX) {
            memcpy(p->payload_buf + p->payload_len, data, dlen);
            p->payload_len += dlen;
        }
    }
    /* 其他字段（event:, id:, retry:）—— v1.0 忽略 */
    p->line_len = 0;
}

size_t sse_parser_feed(sse_parser_t *p, const char *buf, size_t len)
{
    size_t consumed = 0;
    while (consumed < len) {
        char c = buf[consumed++];
        if (c == '\n') {
            on_line(p);
        } else if (p->line_len < LINE_BUF_MAX - 1) {
            p->line_buf[p->line_len++] = c;
        }
    }
    return consumed;
}

void sse_parser_finalize(sse_parser_t *p)
{
    /* 处理残余（无 \n 收尾的行）—— v1.0 简化：丢弃 */
    p->line_len = 0;
    p->payload_len = 0;
}
