set(MODULE_NAME lib_diag_service)
add_library(${MODULE_NAME} STATIC
    diag_state.c
    diag_service.c
    diag_ping.c
    diag_ssl.c
    diag_sms.c
    diag_log.c
    diag_health.c
)
target_include_directories(${MODULE_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib/util
    ${CMAKE_SOURCE_DIR}/lib/at_engine
    ${CMAKE_SOURCE_DIR}/lib/device_manager
    # libcurl 头（certinfo API 需要）
    ${CMAKE_SOURCE_DIR}/third_party/libcurl/include
)
target_link_libraries(${MODULE_NAME} PUBLIC
    lib_util
    lib_at_engine
    lib_device_manager
    third_libuv
    # SSL probe 用 —— 实际 target 名是 libcurl_shared
    libcurl_shared
    # diag_log 压 zip 用
    third_miniz
    # diag_health JSON 报告用
    third_cjson
)

# diag_health TEST_MODE：BUILD_TEST=ON 时让 diag_health.c 走假数据路径
# （test 可不依赖真模组/真网络跑通）
if (BUILD_TEST)
    set_source_files_properties(diag_health.c PROPERTIES
        COMPILE_DEFINITIONS "DIAG_HEALTH_TEST_MODE=1"
    )
endif()
