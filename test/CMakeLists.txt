# ============================================================
#  test — Test executable (only compiled when BUILD_TEST=ON)
# ============================================================
# This module compiles into TEST.exe placed at the project root.
# Use it for unit tests, integration tests, or sandbox experiments.
#
# TEMPLATE INSTRUCTIONS:
#   - MY_SOURCES:   .c files to compile
#   - MY_HEADERS:   test-specific headers
#   - MY_INC_DIRS:  extra include directories
#   - MY_DEPS:      libraries to link (project libs, third-party, etc.)
# ============================================================

# ---- Source files ----
set(TEST_SOURCES
    test_main.c
    # TODO: Add test source files, e.g.:
    # test_http.c
    # test_utils.c
)

# ---- Header files ----
set(TEST_HEADERS
    test_main.h
    # TODO: Add test headers
)

# ---- Test executable target ----
add_executable(TEST
    ${TEST_SOURCES}
    ${TEST_HEADERS}
)

# ---- Output location: out/TEST/ ----
set(TEST_DIST_DIR "${PROJ_OUT_DIR}/TEST")
set_target_properties(TEST PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TEST_DIST_DIR}"
    OUTPUT_NAME "TEST"
)

# ---- Include directories ----
target_include_directories(TEST PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    # Third-party library headers
    ${CMAKE_SOURCE_DIR}/third_party/mbedtls-4.1.0/include
    ${CMAKE_SOURCE_DIR}/third_party/mbedtls-4.1.0/tf-psa-crypto/include
    ${CMAKE_SOURCE_DIR}/third_party/mbedtls-4.1.0/tf-psa-crypto/drivers/builtin/include
    ${CMAKE_SOURCE_DIR}/third_party/libcurl/include
)

# ---- Link libraries ----
target_link_libraries(TEST PRIVATE
    # mbedtls
    mbedtls
    mbedx509
    tfpsacrypto

    # libcurl
    libcurl_shared

    # System libs (Windows)
    ws2_32
    crypt32
    bcrypt
)

# ---- Compiler flags ----
target_compile_options(TEST PRIVATE
    -Wall -Wextra -g
)

# ---- Compile definitions ----
target_compile_definitions(TEST PRIVATE
    -DTEST_MODE
    # TODO: Additional test defines
)

# ---- Post-build: copy runtime DLLs to dist directory ----
add_custom_command(TARGET TEST POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:mbedtls>"
        "${TEST_DIST_DIR}/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:mbedx509>"
        "${TEST_DIST_DIR}/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:tfpsacrypto>"
        "${TEST_DIST_DIR}/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:libcurl_shared>"
        "${TEST_DIST_DIR}/"
    COMMENT "Copying runtime DLLs to TEST dist directory..."
)

# ============================================================
#  New per-file unit tests (one executable per test file)
# ============================================================

# test_strbuf — strbuf + errstr unit tests
add_executable(test_strbuf test_strbuf.c)
target_link_libraries(test_strbuf PRIVATE
    lib_util
)
target_include_directories(test_strbuf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/lib/util
    ${CMAKE_SOURCE_DIR}/include
)
set_target_properties(test_strbuf PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TEST_DIST_DIR}"
    OUTPUT_NAME "test_strbuf"
)

# test_json_roundtrip — json_reader + json_writer roundtrip unit tests
add_executable(test_json_roundtrip test_json_roundtrip.c)
target_link_libraries(test_json_roundtrip PRIVATE
    lib_util
)
target_include_directories(test_json_roundtrip PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/lib/util
    ${CMAKE_SOURCE_DIR}/include
)
set_target_properties(test_json_roundtrip PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TEST_DIST_DIR}"
    OUTPUT_NAME "test_json_roundtrip"
)

# test_i18n — i18n_get zh-CN JSON loader 单元测试
add_executable(test_i18n test_i18n.c)
target_link_libraries(test_i18n PRIVATE
    app_shell
    lib_util
    third_cjson
)
target_include_directories(test_i18n PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/util
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/app/shell
)
set_target_properties(test_i18n PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TEST_DIST_DIR}"
    OUTPUT_NAME "test_i18n"
)

# test_ringbuf — ringbuf 字节环形缓冲单元测试
add_executable(test_ringbuf test_ringbuf.c)
target_link_libraries(test_ringbuf PRIVATE
    lib_util
)
target_include_directories(test_ringbuf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/lib/util
    ${CMAKE_SOURCE_DIR}/include
)
set_target_properties(test_ringbuf PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TEST_DIST_DIR}"
    OUTPUT_NAME "test_ringbuf"
)

# test_ncm_enumerate — NCM/RNDIS 网卡枚举集成测试
add_executable(test_ncm_enumerate test_ncm_enumerate.c)
target_link_libraries(test_ncm_enumerate PRIVATE
    midware_usb_ncm
)
target_include_directories(test_ncm_enumerate PRIVATE
    ${CMAKE_SOURCE_DIR}/midware/usb_ncm
    ${CMAKE_SOURCE_DIR}/include
)
set_target_properties(test_ncm_enumerate PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TEST_DIST_DIR}"
    OUTPUT_NAME "test_ncm_enumerate"
)

# test_at_parser — AT 行解析器单元测试
add_executable(test_at_parser test_at_parser.c)
target_link_libraries(test_at_parser PRIVATE lib_at_engine)
target_include_directories(test_at_parser PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/at_engine
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib/util
)
set_target_properties(test_at_parser PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TEST_DIST_DIR}"
    OUTPUT_NAME "test_at_parser"
)

# test_at_session — AT 会话（队列/状态机/URC）单元测试
add_executable(test_at_session test_at_session.c)
target_link_libraries(test_at_session PRIVATE
    lib_at_engine
    third_libuv
    # libuv 链接 dbghelp（SymGetOptions/MiniDumpWriteDump 等）
    dbghelp
)
target_include_directories(test_at_session PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/at_engine
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib/util
    ${CMAKE_SOURCE_DIR}/midware/serial
)
set_target_properties(test_at_session PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TEST_DIST_DIR}"
    OUTPUT_NAME "test_at_session"
)

# test_diag_state — diag_state 字段读写测试
add_executable(test_diag_state test_diag_state.c)
target_link_libraries(test_diag_state PRIVATE lib_diag_service)
target_include_directories(test_diag_state PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/diag_service
    ${CMAKE_SOURCE_DIR}/include
)
set_target_properties(test_diag_state PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TEST_DIST_DIR}"
    OUTPUT_NAME "test_diag_state"
)

# test_diag_state_ext — P4 扩展字段（ping/ssl/sms/log/health）测试
add_executable(test_diag_state_ext test_diag_state_ext.c)
target_link_libraries(test_diag_state_ext PRIVATE lib_diag_service)
target_include_directories(test_diag_state_ext PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/diag_service
    ${CMAKE_SOURCE_DIR}/include
)
set_target_properties(test_diag_state_ext PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TEST_DIST_DIR}"
    OUTPUT_NAME "test_diag_state_ext"
)

# test_diag_ping — P4 TCP/UDP ping 测试（mock 远端 + 真实 libuv loop）
add_executable(test_diag_ping test_diag_ping.c)
target_link_libraries(test_diag_ping PRIVATE
    lib_diag_service
    third_libuv
    ws2_32
    # libuv 链接 dbghelp（SymGetOptions/MiniDumpWriteDump 等）
    dbghelp
)
target_include_directories(test_diag_ping PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/diag_service
    ${CMAKE_SOURCE_DIR}/include
)
set_target_properties(test_diag_ping PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TEST_DIST_DIR}"
    OUTPUT_NAME "test_diag_ping"
)

# test_diag_ssl — P4 SSL 探活（libcurl 同步 GET + certinfo 解析）
add_executable(test_diag_ssl test_diag_ssl.c)
target_link_libraries(test_diag_ssl PRIVATE
    lib_diag_service
    libcurl_shared
)
target_include_directories(test_diag_ssl PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/diag_service
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party/libcurl/include
)
set_target_properties(test_diag_ssl PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TEST_DIST_DIR}"
    OUTPUT_NAME "test_diag_ssl"
)
# 运行时把 libcurl.dll 复制到 TEST 目录
add_custom_command(TARGET test_diag_ssl POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:libcurl_shared>"
        "${TEST_DIST_DIR}/"
    COMMENT "Copying libcurl.dll to TEST dist directory for test_diag_ssl..."
)

# test_diag_sms — P4 SMS 发送（mock at_session + 验证 4 步状态机）
# 关键：不链 lib_at_engine；mock_at_session.c 提供 weak 符号覆盖。
add_executable(test_diag_sms
    test_diag_sms.c
    mock_at_session.c
)
target_link_libraries(test_diag_sms PRIVATE
    lib_diag_service
)
target_include_directories(test_diag_sms PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/diag_service
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib/at_engine
)
set_target_properties(test_diag_sms PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TEST_DIST_DIR}"
    OUTPUT_NAME "test_diag_sms"
)

# test_diag_log — P4 模组 log 抓取（只测 miniz 压 zip 流程，不依赖 at_session）
add_executable(test_diag_log test_diag_log.c)
target_link_libraries(test_diag_log PRIVATE
    lib_diag_service       # 提供 miniz include
    third_miniz            # MinGW/Clang 静态库不传递依赖，显式链
)
target_include_directories(test_diag_log PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/diag_service
    ${CMAKE_SOURCE_DIR}/include
)
set_target_properties(test_diag_log PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TEST_DIST_DIR}"
    OUTPUT_NAME "test_diag_log"
)

# test_diag_health — P4 一键健康检查（TEST_MODE：所有子检查走假数据）
add_executable(test_diag_health test_diag_health.c)
target_link_libraries(test_diag_health PRIVATE
    lib_diag_service
    third_cjson
    # libuv 链接 dbghelp（SymGetOptions/MiniDumpWriteDump 等）
    dbghelp
)
target_include_directories(test_diag_health PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/lib/diag_service
    ${CMAKE_SOURCE_DIR}/include
)
target_compile_definitions(test_diag_health PRIVATE
    DIAG_HEALTH_TEST_MODE=1
)
set_target_properties(test_diag_health PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TEST_DIST_DIR}"
    OUTPUT_NAME "test_diag_health"
)

# test_llm_dpapi — P5 DPAPI 加密/解密 roundtrip（ASCII + 中文）
add_executable(test_llm_dpapi      test_llm_dpapi.c)
target_link_libraries(test_llm_dpapi PRIVATE
    lib_llm_client
    crypt32
)
set_target_properties(test_llm_dpapi PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TEST_DIST_DIR}"
    OUTPUT_NAME "test_llm_dpapi"
)

# test_llm_sse — P5 SSE 字节流解析器单元测试（5 case）
add_executable(test_llm_sse        test_llm_sse.c)
target_link_libraries(test_llm_sse PRIVATE lib_llm_client)
target_include_directories(test_llm_sse PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/lib/llm_client
    ${CMAKE_SOURCE_DIR}/include
)
set_target_properties(test_llm_sse PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TEST_DIST_DIR}"
    OUTPUT_NAME "test_llm_sse"
)

# test_llm_tool — P5 tool_call JSON 解析器单元测试（2 case）
add_executable(test_llm_tool      test_llm_tool.c)
target_link_libraries(test_llm_tool PRIVATE lib_llm_client)
target_include_directories(test_llm_tool PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/lib/llm_client
    ${CMAKE_SOURCE_DIR}/include
)
set_target_properties(test_llm_tool PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TEST_DIST_DIR}"
    OUTPUT_NAME "test_llm_tool"
)

# test_llm_client_mock — P5 LLM 客户端集成测试（mock HTTP server + SSE）
add_executable(test_llm_client_mock test_llm_client_mock.c)
target_link_libraries(test_llm_client_mock PRIVATE
    lib_llm_client
    libcurl_shared
    ws2_32
    crypt32
)
target_include_directories(test_llm_client_mock PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/lib/llm_client
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party/libcurl/include
)
target_compile_definitions(test_llm_client_mock PRIVATE _CRT_SECURE_NO_WARNINGS)
set_target_properties(test_llm_client_mock PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TEST_DIST_DIR}"
    OUTPUT_NAME "test_llm_client_mock"
)
# 运行时把 libcurl.dll 复制到 TEST 目录
add_custom_command(TARGET test_llm_client_mock POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:libcurl_shared>"
        "${TEST_DIST_DIR}/"
    COMMENT "Copying libcurl.dll to TEST dist directory for test_llm_client_mock..."
)
