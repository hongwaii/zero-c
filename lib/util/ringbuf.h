/**
 * @file ringbuf.h
 * @brief 单生产者-单消费者字节 ring buffer（HAL 与上层之间传输字节流用）。
 *
 * 不是线程安全的——仅在同一个 libuv 回调线程内用。生产者写、消费者读。
 * 满了继续写会覆盖最旧数据（环形特性）；读空返回 0。
 */
#ifndef UTIL_RINGBUF_H
#define UTIL_RINGBUF_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief 字节环形缓冲结构体。
 *
 * 头尾指针用模运算推进；count 记录当前已用字节数，方便空/满判断。
 * buf 由 init 内部 malloc 分配，free 释放。
 */
typedef struct {
    uint8_t *buf;       /* 堆分配，长度 = cap */
    size_t   cap;       /* 容量（字节） */
    size_t   head;      /* 下一个写位置（生产者写入此处） */
    size_t   tail;      /* 下一个读位置（消费者从此处读） */
    size_t   count;     /* 当前已用字节数 */
} ringbuf_t;

/**
 * @brief 初始化环形缓冲。
 * @param rb  缓冲指针（非 NULL）
 * @param cap 容量（字节，> 0）
 * @return AGENT_OK / AGENT_ERR_BAD_ARG / AGENT_ERR_OOM
 */
int ringbuf_init(ringbuf_t *rb, size_t cap);

/**
 * @brief 释放环形缓冲内部堆内存，调用方之后不得再使用。
 * @param rb 缓冲指针（NULL 安全）
 */
void ringbuf_free(ringbuf_t *rb);

/**
 * @brief 写入 n 字节；环形满了则覆盖最旧数据（head 推进同时 tail 也前移）。
 * @param rb   缓冲指针
 * @param data 输入数据指针
 * @param n    要写入字节数
 * @return 实际写入字节数（参数非法时为 0）
 */
size_t ringbuf_write(ringbuf_t *rb, const uint8_t *data, size_t n);

/**
 * @brief 读出最多 n 字节。
 * @param rb  缓冲指针
 * @param out 接收缓冲区
 * @param n   最多读出字节数
 * @return 实际读出字节数；空或参数非法返回 0
 */
size_t ringbuf_read(ringbuf_t *rb, uint8_t *out, size_t n);

/**
 * @brief 偷看（不消费）最多 n 字节。
 * @param rb  缓冲指针
 * @param out 接收缓冲区
 * @param n   最多偷看字节数
 * @return 实际复制到 out 的字节数
 */
size_t ringbuf_peek(const ringbuf_t *rb, uint8_t *out, size_t n);

/**
 * @brief 当前已用字节数。
 * @param rb 缓冲指针（NULL 返回 0）
 */
size_t ringbuf_available(const ringbuf_t *rb);

/**
 * @brief 当前剩余空间。
 * @param rb 缓冲指针（NULL 返回 0）
 */
size_t ringbuf_free_space(const ringbuf_t *rb);

#endif /* UTIL_RINGBUF_H */
