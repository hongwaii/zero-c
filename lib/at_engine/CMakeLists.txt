# ============================================================
#  lib/at_engine — AT 命令行解析 + 会话管理（命令队列 / URC 路由）
# ============================================================
set(MODULE_NAME lib_at_engine)

add_library(${MODULE_NAME} STATIC
    at_session.c
    at_parser.c
)

target_include_directories(${MODULE_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib/util
    ${CMAKE_SOURCE_DIR}/midware/serial
)

target_link_libraries(${MODULE_NAME} PUBLIC
    lib_util
    midware_serial
    third_libuv
    # P6: at_session 收发字节时调 at_log_writer_add 落 at_log 表
    lib_storage
)
