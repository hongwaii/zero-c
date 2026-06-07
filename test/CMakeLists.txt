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
