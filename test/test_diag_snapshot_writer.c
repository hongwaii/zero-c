/**
 * @file test_diag_snapshot_writer.c
 * @brief diag_snapshot_writer 单元测试。
 *
 * 用例：
 *  1. stub 模式（storage_init 返 IO 错）→ skip
 *  2. 真 SQLite 模式：插入 1 行 diag_snapshot + 校验返回值
 *  3. recent() 占位返回 AGENT_OK，*got=0
 *  4. 空参数 → AGENT_ERR_BAD_ARG
 */
#include "diag_snapshot_writer.h"
#include "diag_state.h"
#include "sqlite_db.h"
#include "agent_types.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <windows.h>  /* Sleep() */

int main(void)
{
    /* ---- 准备：临时 DB ---- */
    const char *path = "out/test_diag.db";
    remove(path);

    int rc = storage_init(path);
    if (rc != AGENT_OK) {
        /* stub 模式：DB 未打开，直接 skip */
        printf("test_diag_snapshot_writer: skip (sqlite stub mode)\n");
        remove(path);
        return 0;
    }

    /* ---- init / close 占位 ---- */
    assert(diag_snapshot_writer_init() == AGENT_OK);
    diag_snapshot_writer_close();

    /* ---- 用例 1：正常插入 ---- */
    diag_state_t st = {0};
    strcpy(st.csq, "23");
    strcpy(st.cereg, "5");
    strcpy(st.cop_operator, "China Mobile");
    strcpy(st.rat, "LTE");
    rc = diag_snapshot_writer_add("MDM-1", &st);
    assert(rc == AGENT_OK);

    /* ---- 用例 2：再插一行（不同 ts，应共存） ---- */
    Sleep(1100);  /* 让 ts 跨秒 → PK 不冲突 */
    strcpy(st.csq, "18");
    strcpy(st.cereg, "1");
    rc = diag_snapshot_writer_add("MDM-1", &st);
    assert(rc == AGENT_OK);

    /* ---- 用例 3：空参数校验 ---- */
    rc = diag_snapshot_writer_add(NULL, &st);
    assert(rc == AGENT_ERR_BAD_ARG);
    rc = diag_snapshot_writer_add("MDM-1", NULL);
    assert(rc == AGENT_ERR_BAD_ARG);

    /* ---- 用例 4：recent() 占位（v1.0 简化） ---- */
    diag_state_t out[5] = {0};
    int got = -1;
    rc = diag_snapshot_writer_recent("MDM-1", 5, out, &got);
    assert(rc == AGENT_OK);
    assert(got == 0);

    /* ---- 收尾 ---- */
    storage_close();
    remove(path);
    printf("test_diag_snapshot_writer: 4/4 pass\n");
    return 0;
}
