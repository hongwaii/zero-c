/**
 * @file test_ringbuf.c
 * @brief ringbuf 单元测试。
 *
 * 4 个测试函数 / 12 个 CHECK：
 *   - test_basic_write_read: 基础写读 + count / free_space 校验
 *   - test_wrap_around:     head/tail 回卷后内容顺序正确
 *   - test_overflow_overwrites_oldest: 写满后继续写会覆盖最旧
 *   - test_peek_doesnt_consume: peek 不修改 count
 *
 * 使用 runtime CHECK 宏（不用 assert），失败不 abort，方便单测报告聚合。
 */
#include "ringbuf.h"
#include "agent_types.h"
#include <stdio.h>
#include <string.h>

static int g_checks = 0;
static int g_failed = 0;

/* 失败仅打印文件/行号并累加计数，不终止进程。 */
#define CHECK(cond, msg) do {                                            \
        g_checks++;                                                      \
        if (!(cond)) {                                                   \
            g_failed++;                                                  \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg);\
        }                                                                \
    } while (0)

/**
 * @brief 基础：写 5 字节 "hello"，全部读出，内容一致。
 */
static void test_basic_write_read(void)
{
    ringbuf_t rb;
    CHECK(ringbuf_init(&rb, 16) == AGENT_OK, "init cap=16");
    const uint8_t in[] = "hello";
    CHECK(ringbuf_write(&rb, in, 5) == 5, "write 5");
    CHECK(ringbuf_available(&rb) == 5, "available=5");
    CHECK(ringbuf_free_space(&rb) == 11, "free=11");

    uint8_t out[8] = {0};
    CHECK(ringbuf_read(&rb, out, 8) == 5, "read 5 of 8");
    CHECK(memcmp(out, "hello", 5) == 0, "content == hello");
    CHECK(ringbuf_available(&rb) == 0, "available=0 after drain");
    ringbuf_free(&rb);
}

/**
 * @brief 写 6、读 4、再写 6 触发回卷，验证最终内容顺序仍为写入顺序。
 */
static void test_wrap_around(void)
{
    ringbuf_t rb;
    ringbuf_init(&rb, 8);
    const uint8_t a[] = "abcdef";
    ringbuf_write(&rb, a, 6);
    uint8_t b[4];
    ringbuf_read(&rb, b, 4);  /* 消费 "abcd"，tail 移到 4 */

    const uint8_t c[] = "123456";
    CHECK(ringbuf_write(&rb, c, 6) == 6, "write 6 after read triggers wrap");
    uint8_t out[8] = {0};
    CHECK(ringbuf_read(&rb, out, 8) == 8, "read 8 (6 buffered)");
    CHECK(memcmp(out, "ef123456", 8) == 0, "wrapped content == ef123456");
    ringbuf_free(&rb);
}

/**
 * @brief 容量 4 时写入 5 字节 "ABCDE"，最旧 1 字节应被覆盖。
 * 读出 4 字节应为 "BCDE"。
 */
static void test_overflow_overwrites_oldest(void)
{
    ringbuf_t rb;
    ringbuf_init(&rb, 4);
    const uint8_t a[] = "ABCDE";
    ringbuf_write(&rb, a, 5);  /* cap=4, 写入 5 -> "A" 被覆盖 */
    uint8_t out[4] = {0};
    CHECK(ringbuf_read(&rb, out, 4) == 4, "read 4 (full buffer)");
    CHECK(memcmp(out, "BCDE", 4) == 0, "BCDE remains after overflow");
    ringbuf_free(&rb);
}

/**
 * @brief peek 不消耗：peek 后 count 仍为 3。
 */
static void test_peek_doesnt_consume(void)
{
    ringbuf_t rb;
    ringbuf_init(&rb, 8);
    const uint8_t a[] = "xyz";
    ringbuf_write(&rb, a, 3);
    uint8_t out[4] = {0};
    CHECK(ringbuf_peek(&rb, out, 4) == 3, "peek 3 (asked for 4)");
    CHECK(memcmp(out, "xyz", 3) == 0, "peeked content == xyz");
    CHECK(ringbuf_available(&rb) == 3, "still 3 after peek (no consume)");
    ringbuf_free(&rb);
}

int main(void)
{
    test_basic_write_read();
    test_wrap_around();
    test_overflow_overwrites_oldest();
    test_peek_doesnt_consume();
    printf("test_ringbuf: %d/%d pass\n", g_checks - g_failed, g_checks);
    return g_failed == 0 ? 0 : 1;
}
