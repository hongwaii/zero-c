/**
 * @file at_session.c
 * @brief AT 会话：P3-T1 仅 stub（编译过），P3-T2 填真实现。
 *
 * 当前所有 API 都返回 AGENT_ERR_BAD_ARG 或 NULL，调用方可以 link 通过，
 * 但实际使用会立即失败——这是有意的，P3-T2 才会填真实现。
 */
#include "at_session.h"
#include "agent_types.h"

at_session_t *at_session_create(uv_loop_t *loop, struct modem_chan *chan)
{
    (void)loop; (void)chan;
    return NULL;  /* P3-T2 填真实现 */
}

int at_session_open(at_session_t *s)
{
    (void)s;
    return AGENT_ERR_BAD_ARG;
}

void at_session_close(at_session_t *s)
{
    (void)s;
}

/* at_session_send 与 at_session_register_urc 用 variadic 在 stub 里吞参；
 * 真实签名在头文件，调用方编译期能拿到正确类型检查。 */
int at_session_send(at_session_t *s, const char *cmd, int timeout_ms,
                    at_response_cb cb, void *userdata)
{
    (void)s; (void)cmd; (void)timeout_ms; (void)cb; (void)userdata;
    return AGENT_ERR_BAD_ARG;
}

int at_session_register_urc(at_session_t *s, const char *prefix,
                            at_urc_cb cb, void *userdata)
{
    (void)s; (void)prefix; (void)cb; (void)userdata;
    return AGENT_ERR_BAD_ARG;
}
