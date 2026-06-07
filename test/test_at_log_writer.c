/**
 * @file test_at_log_writer.c
 * @brief at_log_writer 单元测试：插入 10 条 → 校验都成功（真 SQLite）或 skip（stub）。
 *
 * stub 模式：storage_init 返 AGENT_ERR_IO，测试打印 "skip" 后返 0（不算失败）。
 * 真 SQLite 模式：10 条全部 insert 成功，assert(rc == AGENT_OK)。
 */
#include "at_log_writer.h"
#include "sqlite_db.h"
#include "agent_types.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    const char *path = "out/test_atlog.db";
    /* 清掉上轮残留 */
    remove(path);

    int rc = storage_init(path);
    if (rc != AGENT_OK) {
        /* stub 模式：直接 pass（不算 fail） */
        printf("test_at_log_writer: skip (sqlite stub mode)\n");
        remove(path);
        return 0;
    }

    at_log_writer_init();

    /* 插 10 条混合 TX/RX/URC */
    const char *raws[10] = {
        "AT+CSQ\r\n",       "+CSQ: 23,99\r\n",
        "AT+CREG?\r\n",     "+CREG: 0,5\r\n",
        "+CTZV: 25/06/07,08:00:00\r\n",  /* URC */
        "AT+CEREG?\r\n",    "+CEREG: 0,5\r\n",
        "AT+COPS?\r\n",     "+COPS: 0,0,\"China Mobile\",7\r\n",
        "+CESQ: 99,99,255,255,23,89\r\n" /* URC */
    };
    const char *dirs[10] = {
        "TX", "RX", "TX", "RX", "URC", "TX", "RX", "TX", "RX", "URC"
    };
    for (int i = 0; i < 10; i++) {
        rc = at_log_writer_add("MDM-1", dirs[i], raws[i]);
        assert(rc == AGENT_OK);
    }

    at_log_writer_close();
    storage_close();
    remove(path);
    printf("test_at_log_writer: 10/10 pass\n");
    return 0;
}
