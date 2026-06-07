/**
 * @file at_session.c
 * @brief AT 会话实现：命令队列、状态机、URC 路由。
 *
 * 数据流：
 *   用户 send → 入队 → 状态机推进 → 调 chan->send
 *   chan->on_rx 收字节 → at_parser_feed → 根据行类型：
 *     - FINAL_OK / FINAL_ERROR → 触发当前命令的完成回调
 *     - URC + 前缀匹配 → 调对应 urc_cb
 *     - DATA → 累积到当前命令的 result 缓冲
 *
 * P3 简化：不支持 CMUX、不支持并发命令、单条 FIFO。
 */
#include "at_session.h"
#include "at_parser.h"
#include "agent_chan.h"
#include "agent_types.h"
#include "strbuf.h"

#define WIN32_LEAN_AND_MEAN
#include <uv.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CMD_QUEUE_MAX      32
#define RESULT_BUF_CAP     1024
#define URC_HANDLERS_MAX   16

/* URC 订阅项：按行首 prefix 匹配 */
typedef struct {
    char        prefix[32];
    at_urc_cb   cb;
    void       *userdata;
} urc_handler_t;

/* 命令队列项 */
typedef struct cmd_item {
    char            cmd[128];
    int             timeout_ms;
    at_response_cb  cb;
    void           *userdata;
    strbuf_t        result;
    bool            in_flight;
} cmd_item_t;

struct at_session {
    uv_loop_t         *loop;
    struct modem_chan *chan;
    at_line_type_t     last_line_type;

    cmd_item_t         queue[CMD_QUEUE_MAX];
    int                q_head;
    int                q_tail;
    int                q_count;

    bool               in_flight;
    uv_timer_t         cmd_timer;

    urc_handler_t      urc_handlers[URC_HANDLERS_MAX];
    int                urc_count;
};

/* ---------- 队列工具 ---------- */

/**
 * @brief 取得队首（正在 in_flight 或下一条待发）项。
 */
static cmd_item_t *queue_front(at_session_t *s)
{
    return (s->q_count > 0) ? &s->queue[s->q_head] : NULL;
}

/**
 * @brief 弹出队首：释放其 result 缓冲，复位 in_flight 标志。
 *
 * 注意：必须在回调已被取出之后调用本函数——complete_current() 负责顺序。
 */
static void queue_pop_front(at_session_t *s)
{
    if (s->q_count == 0) return;
    cmd_item_t *front = &s->queue[s->q_head];
    strbuf_free(&front->result);
    s->q_head = (s->q_head + 1) % CMD_QUEUE_MAX;
    s->q_count--;
    s->in_flight = false;
}

/**
 * @brief 触发队首命令的完成回调并出队。
 *
 * 先调 cb 再 pop——result 的内存由 strbuf 拥有，pop 时 strbuf_free
 * 会把 buffer 释放掉。回调必须在释放前读 result.data。
 */
static void complete_current(at_session_t *s, bool ok)
{
    cmd_item_t *front = queue_front(s);
    if (!front) return;
    at_response_cb cb = front->cb;
    void *ud = front->userdata;
    const char *res = front->result.data;
    size_t res_len = front->result.len;
    /* 注意：先调 cb 再 pop。cb 拿到 res/res_len 后必须立即拷走，
     * 因为 pop 会 strbuf_free 掉 result 的 buffer。 */
    if (cb) cb(ud, res, res_len, ok);
    queue_pop_front(s);
}

/**
 * @brief 前向声明：命令超时回调（定义在 try_send_next 之后）。
 *
 * 之前 try_send_next 给 uv_timer_start 传 NULL，timer 静默无效；
 * 改成传 on_cmd_timeout 后必须先前向声明——否则编译器在 try_send_next
 * 引用 on_cmd_timeout 时报"undeclared identifier"。
 */
static void on_cmd_timeout(uv_timer_t *handle);

/**
 * @brief 若空闲且队列非空，发下一条命令并启动超时定时器。
 */
static void try_send_next(at_session_t *s)
{
    if (s->in_flight) return;
    cmd_item_t *front = queue_front(s);
    if (!front) return;
    char buf[256];
    int n = snprintf(buf, sizeof(buf), "%s\r", front->cmd);
    if (n < 0 || n >= (int)sizeof(buf)) return;
    if (modem_chan_send(s->chan, (const uint8_t *)buf, (size_t)n) != 0) {
        fprintf(stderr, "at_session: chan send failed\n");
        complete_current(s, false);
        return;
    }
    s->in_flight = true;
    /* 必须传 on_cmd_timeout——NULL 会让 libuv 静默无效，timer 永远不 fire，
     * 导致某条 AT 卡住后整个队列冻死，UI 看不到新数据。 */
    uv_timer_start(&s->cmd_timer, on_cmd_timeout, front->timeout_ms, 0);
}

/**
 * @brief 命令超时回调：把当前命令当失败完成，再尝试发下一条。
 */
static void on_cmd_timeout(uv_timer_t *handle)
{
    at_session_t *s = (at_session_t *)handle->data;
    fprintf(stderr, "at_session: command timeout\n");
    complete_current(s, false);
    try_send_next(s);
}

/* ---------- URC 路由 ---------- */

/**
 * @brief 把一行 URC 派发给匹配的处理器。
 * @return true 表示派发成功（找到匹配），false 表示没匹配上。
 */
static bool dispatch_urc(at_session_t *s, const char *line, size_t len)
{
    for (int i = 0; i < s->urc_count; i++) {
        if (strncmp(line, s->urc_handlers[i].prefix, strlen(s->urc_handlers[i].prefix)) == 0) {
            s->urc_handlers[i].cb(s->urc_handlers[i].userdata, line, len);
            return true;
        }
    }
    return false;
}

/**
 * @brief 把 data 行累积到当前在飞命令的 result 缓冲。
 */
static void accumulate_data(at_session_t *s, const char *line, size_t len)
{
    cmd_item_t *front = queue_front(s);
    if (!front || !s->in_flight) return;
    if (front->result.len > 0) {
        strbuf_append(&front->result, "\r\n");
    }
    strbuf_append_n(&front->result, line, len);
}

/* ---------- 收字节回调（由 modem_chan 调） ---------- */

/**
 * @brief 收到一批字节：逐字节喂给 at_parser，根据解析出的行类型推进状态机。
 */
static void on_chan_rx(void *userdata, const uint8_t *buf, size_t len)
{
    at_session_t *s = (at_session_t *)userdata;
    if (!s || !buf || len == 0) return;

    for (size_t i = 0; i < len; i++) {
        at_line_t line;
        if (at_parser_feed(buf[i], &line)) {
            switch (line.type) {
            case AT_LINE_FINAL_OK:
                complete_current(s, true);
                try_send_next(s);
                break;
            case AT_LINE_FINAL_ERROR:
                complete_current(s, false);
                try_send_next(s);
                break;
            case AT_LINE_URC:
                /* URC 优先尝试按订阅前缀派发；没匹配上则当作 data 行
                 * 累积到当前在飞命令的 result（如 AT+CSQ 的响应 +CSQ: 23,99）。 */
                if (!dispatch_urc(s, line.line, line.len)) {
                    accumulate_data(s, line.line, line.len);
                }
                break;
            case AT_LINE_DATA:
                accumulate_data(s, line.line, line.len);
                break;
            default:
                break;
            }
        }
    }
}

/* ---------- 公共 API ---------- */

/**
 * @brief 创建会话；绑到 chan 的 on_rx 回调上。
 */
at_session_t *at_session_create(uv_loop_t *loop, struct modem_chan *chan)
{
    if (!loop || !chan) return NULL;
    at_session_t *s = (at_session_t *)calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->loop = loop;
    s->chan = chan;
    chan->on_rx = on_chan_rx;
    chan->userdata = s;
    return s;
}

/**
 * @brief 启动会话：初始化超时定时器、parser。
 */
int at_session_open(at_session_t *s)
{
    if (!s) return AGENT_ERR_BAD_ARG;
    uv_timer_init(s->loop, &s->cmd_timer);
    s->cmd_timer.data = s;
    at_parser_init();
    return 0;
}

/**
 * @brief 关闭会话：停定时器、释放所有挂起命令的 result、与 chan 解绑。
 */
void at_session_close(at_session_t *s)
{
    if (!s) return;
    uv_timer_stop(&s->cmd_timer);
    uv_close((uv_handle_t *)&s->cmd_timer, NULL);
    for (int i = 0; i < s->q_count; i++) {
        cmd_item_t *it = &s->queue[(s->q_head + i) % CMD_QUEUE_MAX];
        strbuf_free(&it->result);
    }
    s->q_head = s->q_tail = s->q_count = 0;
    s->in_flight = false;
    if (s->chan) {
        s->chan->on_rx = NULL;
        s->chan->userdata = NULL;
        s->chan = NULL;
    }
}

/**
 * @brief 排队一条 AT 命令。timeout_ms<=0 时使用默认 3000ms。
 */
int at_session_send(at_session_t *s, const char *cmd, int timeout_ms,
                    at_response_cb cb, void *userdata)
{
    if (!s || !cmd) return AGENT_ERR_BAD_ARG;
    if (s->q_count >= CMD_QUEUE_MAX) return AGENT_ERR_OOM;
    cmd_item_t *it = &s->queue[s->q_tail];
    strncpy(it->cmd, cmd, sizeof(it->cmd) - 1);
    it->cmd[sizeof(it->cmd) - 1] = '\0';
    it->timeout_ms = (timeout_ms > 0) ? timeout_ms : 3000;
    it->cb = cb;
    it->userdata = userdata;
    it->in_flight = false;
    strbuf_init(&it->result, RESULT_BUF_CAP);
    s->q_tail = (s->q_tail + 1) % CMD_QUEUE_MAX;
    s->q_count++;
    try_send_next(s);
    return 0;
}

/**
 * @brief 注册一个 URC 处理器（按行首 prefix 匹配）。
 */
int at_session_register_urc(at_session_t *s, const char *prefix,
                            at_urc_cb cb, void *userdata)
{
    if (!s || !prefix || !cb) return AGENT_ERR_BAD_ARG;
    if (s->urc_count >= URC_HANDLERS_MAX) return AGENT_ERR_OOM;
    urc_handler_t *h = &s->urc_handlers[s->urc_count++];
    strncpy(h->prefix, prefix, sizeof(h->prefix) - 1);
    h->prefix[sizeof(h->prefix) - 1] = '\0';
    h->cb = cb;
    h->userdata = userdata;
    return 0;
}
