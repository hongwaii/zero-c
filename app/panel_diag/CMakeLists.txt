set(MODULE_NAME app_panel_diag)
add_library(${MODULE_NAME} STATIC panel.cpp)
target_include_directories(${MODULE_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/app/shell
    ${CMAKE_SOURCE_DIR}/lib/diag_service   # 新加
    ${CMAKE_SOURCE_DIR}/lib/device_manager
)
target_link_libraries(${MODULE_NAME} PUBLIC
    app_shell
    lib_diag_service     # 新加
    lib_at_engine         # 新加
    lib_device_manager   # 新加
    third_imgui
)
