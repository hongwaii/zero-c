set(MODULE_NAME app_shell)

set(MY_SOURCES
    host.cpp
    debug_console.cpp
    i18n.c
    theme.cpp
    main_window.cpp
    llm_drawer.cpp
)

add_library(${MODULE_NAME} STATIC ${MY_SOURCES})

# host.cpp uses C++ ImGui headers; force the linker to use C++ rules
set_target_properties(${MODULE_NAME} PROPERTIES
    LINKER_LANGUAGE CXX
)

target_include_directories(${MODULE_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib/util
)

target_link_libraries(${MODULE_NAME} PUBLIC
    lib_util
    third_imgui
    third_cjson
    app_panel_diag
    app_panel_devices
    app_panel_prod
    app_panel_ota
    app_panel_settings
)

# Static-link C++ runtime flags are applied at the final EXE target
# (see core/CMakeLists.txt). The static-lib link step here does not need them
# because the final link is what actually resolves the import libraries.
