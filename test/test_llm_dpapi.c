#include "llm_dpapi.h"
#include "agent_types.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int test_roundtrip(void)
{
    const char *plain = "sk-test-key-12345-abcde";
    size_t plen = strlen(plain);
    char hex[256];
    int rc = llm_dpapi_encrypt_hex(plain, plen, hex, sizeof(hex));
    assert(rc == AGENT_OK);
    /* hex 长度 = plen * 2（不含 \0） */
    assert(strlen(hex) == plen * 2);
    /* 解密回原文 */
    char back[64];
    rc = llm_dpapi_decrypt_hex(hex, strlen(hex), back, sizeof(back));
    assert(rc == AGENT_OK);
    assert(strcmp(back, plain) == 0);
    return 0;
}

static int test_chinese_roundtrip(void)
{
    const char *plain = "中文测试 key 中文";
    size_t plen = strlen(plain);
    char hex[256];
    int rc = llm_dpapi_encrypt_hex(plain, plen, hex, sizeof(hex));
    assert(rc == AGENT_OK);
    char back[64];
    rc = llm_dpapi_decrypt_hex(hex, strlen(hex), back, sizeof(back));
    assert(rc == AGENT_OK);
    assert(strcmp(back, plain) == 0);
    return 0;
}

int main(void)
{
    test_roundtrip();
    test_chinese_roundtrip();
    printf("test_llm_dpapi: 2/2 pass\n");
    return 0;
}
