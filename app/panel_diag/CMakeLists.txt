set(MODULE_NAME app_panel_diag)
add_library(${MODULE_NAME} STATIC panel.cpp)
target_include_directories(${MODULE_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/app/shell
    ${CMAKE_SOURCE_DIR}/lib/diag_service   # P3 加；P4 提供 diag_ping/ssl/sms/log/health 头
    ${CMAKE_SOURCE_DIR}/lib/device_manager
    ${CMAKE_SOURCE_DIR}/lib/at_engine      # P4 加；at_session.h 直 include
)
target_link_libraries(${MODULE_NAME} PUBLIC
    app_shell
    lib_diag_service     # P3 加
    lib_at_engine        # P3 加；P4 显式用 at_session 触发 ATD/ATH
    lib_device_manager   # P3 加；P4 用来反查 dev_idx
    third_imgui
)
