set(MODULE_NAME app_shell)

set(MY_SOURCES
    host.cpp
    # theme.c, llm_drawer.c, i18n.c added in later tasks
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
)
