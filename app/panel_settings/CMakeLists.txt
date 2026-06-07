set(MODULE_NAME app_panel_settings)
add_library(${MODULE_NAME} STATIC
    panel.cpp
    seed_llm_providers.cpp
)
target_include_directories(${MODULE_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/app/shell
)
target_link_libraries(${MODULE_NAME} PUBLIC
    app_shell
    lib_device_manager       # 新加：panel 改 g_default_baud 需要链接 C 库解析符号
    third_imgui
)
