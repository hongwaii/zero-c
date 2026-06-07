/**
 * @file ringbuf.c
 * @brief 单生产者-单消费者字节环形缓冲实现。
 *
 * 设计要点：
 *   - head 永远指向下一个写位置；tail 永远指向下一个读位置。
 *   - count 跟踪已用字节数；通过 (head == tail && count == 0) 判空、
 *     (head == tail && count == cap) 判满，避免额外状态位。
 *   - 写满后再写：head 正常推进，count 维持 cap，tail 同步前移以丢弃最旧 1 字节。
 *   - peek 不修改 head/tail/count，只读不消费。
 *
 * 复杂度：所有操作 O(n) 字节复制，n 为请求长度（实现里按字节循环）。
 */
#include "ringbuf.h"
#include "agent_types.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief 分配 cap 字节的环形缓冲。
 * @return AGENT_OK 成功；AGENT_ERR_BAD_ARG 参数非法；AGENT_ERR_OOM 分配失败。
 */
int ringbuf_init(ringbuf_t *rb, size_t cap)
{
    if (!rb || cap == 0) return AGENT_ERR_BAD_ARG;
    rb->buf = (uint8_t *)malloc(cap);
    if (!rb->buf) return AGENT_ERR_OOM;
    rb->cap = cap;
    rb->head = rb->tail = rb->count = 0;
    return AGENT_OK;
}

/**
 * @brief 释放缓冲，调用方负责不再使用。
 * @note NULL 安全。
 */
void ringbuf_free(ringbuf_t *rb)
{
    if (!rb) return;
    free(rb->buf);
    rb->buf = NULL;
    rb->cap = rb->head = rb->tail = rb->count = 0;
}

/**
 * @brief 写入 n 字节；环形满了则覆盖最旧数据（head 推进同时 tail 也前移）。
 * @return 实际写入字节数（参数非法时为 0）。
 */
size_t ringbuf_write(ringbuf_t *rb, const uint8_t *data, size_t n)
{
    if (!rb || !data || n == 0) return 0;
    size_t written = 0;
    for (size_t i = 0; i < n; i++) {
        rb->buf[rb->head] = data[i];
        rb->head = (rb->head + 1) % rb->cap;
        if (rb->count < rb->cap) {
            /* 未满：count 累加 */
            rb->count++;
        } else {
            /* 已满：tail 跟着前移，丢弃最旧 1 字节 */
            rb->tail = (rb->tail + 1) % rb->cap;
        }
        written++;
    }
    return written;
}

/**
 * @brief 读出最多 n 字节。
 * @return 实际读出字节数；空或参数非法返回 0。
 */
size_t ringbuf_read(ringbuf_t *rb, uint8_t *out, size_t n)
{
    if (!rb || !out || n == 0) return 0;
    size_t to_read = (n < rb->count) ? n : rb->count;
    for (size_t i = 0; i < to_read; i++) {
        out[i] = rb->buf[rb->tail];
        rb->tail = (rb->tail + 1) % rb->cap;
    }
    rb->count -= to_read;
    return to_read;
}

/**
 * @brief 偷看（不消费）最多 n 字节。
 * @note 不修改 head/tail/count，可重复调用。
 */
size_t ringbuf_peek(const ringbuf_t *rb, uint8_t *out, size_t n)
{
    if (!rb || !out || n == 0) return 0;
    size_t to_read = (n < rb->count) ? n : rb->count;
    size_t idx = rb->tail;
    for (size_t i = 0; i < to_read; i++) {
        out[i] = rb->buf[idx];
        idx = (idx + 1) % rb->cap;
    }
    return to_read;
}

/**
 * @brief 当前已用字节数。
 * @note NULL 安全（返回 0）。
 */
size_t ringbuf_available(const ringbuf_t *rb)
{
    return rb ? rb->count : 0;
}

/**
 * @brief 当前剩余空间（cap - count）。
 * @note NULL 安全（返回 0）。
 */
size_t ringbuf_free_space(const ringbuf_t *rb)
{
    return rb ? (rb->cap - rb->count) : 0;
}
