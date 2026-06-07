set(MODULE_NAME app_panel_devices)
add_library(${MODULE_NAME} STATIC panel.cpp)
target_include_directories(${MODULE_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/app/shell
)
target_link_libraries(${MODULE_NAME} PUBLIC
    app_shell
    lib_device_manager   # 新加：panel.cpp 直接读 device_manager_t 结构体
    third_imgui
)
