/**
 * @file llm_sse.h
 * @brief SSE (Server-Sent Events) 字节流解析器：CR/LF 边界 + data: 前缀 + [DONE] 哨兵。
 *
 * 设计：libcurl CURLOPT_WRITEFUNCTION 每次给一段字节（不一定按行对齐），
 *       解析器内部维护"未完成行"缓冲；见到 \n\n 收一条事件，data 行去掉
 *       "data: " 前缀后拼到 payload；遇 "[DONE]" 收尾。
 */
#ifndef LIB_LLM_SSE_H
#define LIB_LLM_SSE_H

#include <stdbool.h>
#include <stddef.h>

typedef struct sse_parser sse_parser_t;

typedef enum {
    SSE_EV_DATA,      /* 正常事件：payload 是 data 行内容（多个 data: 行用 \n 拼） */
    SSE_EV_DONE,      /* 哨兵 [DONE] */
    SSE_EV_COMMENT,   /* 以 : 开头的注释行（OpenAI 心跳用）—— v1.0 忽略 */
} sse_event_type_t;

typedef struct {
    sse_event_type_t type;
    const char      *payload;   /* DATA 有效；DONE/COMMENT 为 NULL */
    size_t           payload_len;
} sse_event_t;

/* 用户回调：返回 false 中断解析（如连接已断） */
typedef bool (*sse_event_cb)(const sse_event_t *ev, void *userdata);

sse_parser_t *sse_parser_create(sse_event_cb cb, void *userdata);
void          sse_parser_destroy(sse_parser_t *p);

/* 喂一段字节，返回"已消费"字节数（= len，正常情况全消费） */
size_t        sse_parser_feed(sse_parser_t *p, const char *buf, size_t len);

/* 收尾：处理残余字节（连接断开时调） */
void          sse_parser_finalize(sse_parser_t *p);

#endif
