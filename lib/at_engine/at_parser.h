/**
 * @file at_parser.h
 * @brief AT 命令行解析器：CR/LF 边界 + final/URC/data 分类。
 *
 * 把字节流拆成行；每行判定是 final response / URC / data。
 * 不负责命令队列和超时——那是 at_session 的事。
 */
#ifndef LIB_AT_ENGINE_AT_PARSER_H
#define LIB_AT_ENGINE_AT_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 行类型：解析器对单行的分类结果 */
typedef enum {
    AT_LINE_INCOMPLETE,    /* 还在拼行（行未结束） */
    AT_LINE_FINAL_OK,      /* "OK"：命令成功结束标记 */
    AT_LINE_FINAL_ERROR,   /* "ERROR" / "+CME ERROR: ..." / "NO CARRIER" 等 */
    AT_LINE_URC,           /* "+CMTI: ..." / "+CEREG: 5" 等（行首 +） */
    AT_LINE_DATA,          /* 普通数据行（如 "+CSQ: 23,99"），归 pending 命令 */
} at_line_type_t;

/* 解析器吐出的"完整一行"——out 参数由调用方持有 */
typedef struct {
    char  line[256];       /* 不含 \r\n，NUL 结尾 */
    size_t len;
    at_line_type_t type;
} at_line_t;

/**
 * @brief 初始化解析器（仅置零行缓冲与 \r 标记）。
 */
void at_parser_init(void);

/**
 * @brief 喂一个字节，返回"是否拼成一行"以及（如果成行）行的内容与类型。
 *
 * 内部维持一个待拼行缓冲。调用方在收到新字节时反复调本函数，
 * 直到返回 true 拿到一行为止。
 */
bool at_parser_feed(uint8_t b, at_line_t *out);

#endif /* LIB_AT_ENGINE_AT_PARSER_H */
