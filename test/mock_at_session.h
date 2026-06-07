/**
 * @file mock_at_session.h
 * @brief at_session 函数的 mock 桩（weak symbol 实现）。
 *
 * 用法：
 *   1) test_diag_xxx.c 包含此头；
 *   2) CMake 里把 mock_at_session.c 加进该测试 exe 的源文件；
 *   3) mock 默认行为见下面 SETUP_MOCK 宏，可选切换。
 *
 * 弱符号机制：
 *   - GCC/Clang 支持 __attribute__((weak))，允许多个 TU 提供同名符号，
 *     链接器取"强符号"优先；没有强符号则取 weak。
 *   - 我们的 mock_at_session.c 提供的是 weak 版本——正常 link
 *     lib_at_engine 时，at_session.c 里的同名函数是强符号，自动覆盖 mock。
 *   - test exe 只 link lib_diag_service（不 link lib_at_engine）时，
 *     强符号缺席，weak 的 mock 生效——达成"测试覆盖"目的。
 */
#ifndef TEST_MOCK_AT_SESSION_H
#define TEST_MOCK_AT_SESSION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "at_session.h"   /* 复用 at_response_cb / at_urc_cb 类型 */

/* ---------- mock 全局状态（默认实现简单按 call 顺序模拟响应） ---------- */

/* mock 行为模式：决定 default 模拟怎样回包 */
typedef enum {
    MOCK_MODE_SIMPLE = 0,   /* 默认：CMGF 返 OK；CMGS 不 cb（等 URC）；raw 返 OK */
    MOCK_MODE_ERROR,         /* CMGF 返 ERROR */
    MOCK_MODE_URC_NEVER,     /* URC 永不触发（用来测超时——v1.1） */
} mock_mode_t;

void mock_at_set_mode(mock_mode_t m);
mock_mode_t mock_at_get_mode(void);

/* 调用次数（测试断言用） */
int mock_at_send_count(void);
int mock_at_send_raw_count(void);
int mock_at_register_urc_count(void);

/* 最近一次 send / send_raw 收到的 cmd / raw bytes 拷贝（截断） */
void mock_at_last_cmd(char *out, size_t out_size);
void mock_at_last_raw(uint8_t *out, size_t *out_len);

#endif /* TEST_MOCK_AT_SESSION_H */
