# ============================================================
#  lib/llm_client — LLM 客户端 + SSE 解析 + DPAPI 加密 + tool_call 解析
# ============================================================
set(MODULE_NAME lib_llm_client)

add_library(${MODULE_NAME} STATIC
    llm_sse.c
    llm_dpapi.c
    # llm_client.c / llm_tool.c / provider_config.c 在后续 task 追加
)

target_include_directories(${MODULE_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(${MODULE_NAME} PUBLIC
    libcurl_shared
)

if (WIN32)
    target_link_libraries(${MODULE_NAME} PUBLIC crypt32)
endif()
