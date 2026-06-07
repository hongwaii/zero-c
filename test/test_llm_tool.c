#include "llm_tool.h"
#include "agent_types.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int test_simple_tool(void)
{
    const char *json =
        "{\"choices\":[{\"message\":{\"tool_calls\":["
        "{\"id\":\"call_1\",\"type\":\"function\","
        "\"function\":{\"name\":\"send_at\",\"arguments\":\"{\\\"cmd\\\":\\\"AT+CSQ\\\"}\"}}"
        "]}}]}";
    llm_tool_call_t t = {0};
    bool ok = llm_extract_tool_call(json, strlen(json), &t);
    assert(ok);
    assert(strcmp(t.id, "call_1") == 0);
    assert(strcmp(t.name, "send_at") == 0);
    assert(strstr(t.arguments, "AT+CSQ") != NULL);
    return 0;
}

static int test_no_tool(void)
{
    const char *json = "{\"choices\":[{\"message\":{\"content\":\"hello\"}}]}";
    llm_tool_call_t t = {0};
    bool ok = llm_extract_tool_call(json, strlen(json), &t);
    assert(!ok);
    return 0;
}

int main(void)
{
    test_simple_tool();
    test_no_tool();
    printf("test_llm_tool: 2/2 pass\n");
    return 0;
}
