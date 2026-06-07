# ============================================================
#  lib/storage — SQLite 持久化（device / diag / at_log / llm_chat 四表）
# ============================================================
# 静态库 lib_storage；链 third_sqlite（vendor amalgamation）。
#
# 当前包含：
#   - sqlite_db            打开 / 关闭 / WAL / 4 表 schema
#   - at_log_writer        高频异步写 + 重试降级 (Task 3)
#   - diag_snapshot_writer diag 同步写 (Task 4)
#   - llm_chat_writer      LLM 同步写 (Task 4)
#   - config_io            app.json / devices.json 加载 / 保存 (Task 6)
#
# 后续 task 追加：
#   - report_html          自包含 HTML 报告 (Task 5)
# ============================================================
set(MODULE_NAME lib_storage)

add_library(${MODULE_NAME} STATIC
    sqlite_db.c
    at_log_writer.c
    diag_snapshot_writer.c
    llm_chat_writer.c
    config_io.c
    # report_html 在后续 task 追加
)

target_include_directories(${MODULE_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    # diag_snapshot_writer.h 需要 diag_state_t
    ${CMAKE_SOURCE_DIR}/lib/diag_service
)

target_link_libraries(${MODULE_NAME} PUBLIC
    third_sqlite
    # config_io 用 lib/util/json_reader + json_writer；lib_util 传递 third_cjson
    lib_util
)
