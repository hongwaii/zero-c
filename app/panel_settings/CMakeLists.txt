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
    lib_llm_client           # P5: panel_settings_save_to_config 调 llm_provider_config_save
    lib_storage              # P6: "导出报告"按钮调 report_html_export + storage_get_db 判 DB 状态
)
