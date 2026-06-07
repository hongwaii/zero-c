/**
 * @file test_llm_chat_writer.c
 * @brief llm_chat_writer 单元测试。
 *
 * 用例：
 *  1. stub 模式（storage_init 返 IO 错）→ skip
 *  2. 真 SQLite 模式：插 user / assistant / tool 三种 role
 *  3. NULL 参数走 "" 路径不崩
 */
#include "llm_chat_writer.h"
#include "sqlite_db.h"
#include "agent_types.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

int main(void)
{
    /* ---- 准备：临时 DB ---- */
    const char *path = "out/test_llmchat.db";
    remove(path);

    int rc = storage_init(path);
    if (rc != AGENT_OK) {
        /* stub 模式：DB 未打开，直接 skip */
        printf("test_llm_chat_writer: skip (sqlite stub mode)\n");
        remove(path);
        return 0;
    }

    /* ---- init / close 占位 ---- */
    assert(llm_chat_writer_init() == AGENT_OK);
    llm_chat_writer_close();

    /* ---- 用例 1：user 角色 ---- */
    rc = llm_chat_writer_add("DeepSeek", "deepseek-chat", "user", "hello", NULL);
    assert(rc == AGENT_OK);

    /* ---- 用例 2：assistant 角色（带 tool_calls JSON） ---- */
    const char *tool_calls =
        "[{\"name\":\"ping_host\",\"args\":{\"host\":\"8.8.8.8\"}}]";
    rc = llm_chat_writer_add("DeepSeek", "deepseek-chat", "assistant",
                             "I'll ping 8.8.8.8 for you", tool_calls);
    assert(rc == AGENT_OK);

    /* ---- 用例 3：tool 角色（回执） ---- */
    rc = llm_chat_writer_add("DeepSeek", "deepseek-chat", "tool",
                             "ping 8.8.8.8: 32ms OK", NULL);
    assert(rc == AGENT_OK);

    /* ---- 用例 4：NULL 全部走 "" 路径 ---- */
    rc = llm_chat_writer_add(NULL, NULL, NULL, NULL, NULL);
    assert(rc == AGENT_OK);

    /* ---- 收尾 ---- */
    storage_close();
    remove(path);
    printf("test_llm_chat_writer: 4/4 pass\n");
    return 0;
}
