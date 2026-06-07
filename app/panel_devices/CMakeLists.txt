set(MODULE_NAME app_panel_devices)
add_library(${MODULE_NAME} STATIC panel.cpp)
target_include_directories(${MODULE_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/app/shell
    ${CMAKE_SOURCE_DIR}/lib/device_manager
    ${CMAKE_SOURCE_DIR}/midware/serial   # 新加：panel.cpp 调 device_manager_connect_dev
)
target_link_libraries(${MODULE_NAME} PUBLIC
    app_shell
    lib_device_manager
    midware_serial       # 新加：device_manager.h 前向声明了 serial_chan
    lib_at_engine         # 新加：device_manager.h 前向声明了 at_session
    third_imgui
)
