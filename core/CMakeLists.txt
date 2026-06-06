set(CORE_SOURCES main.cpp)
set(CORE_HEADERS)

add_executable(${PRODUCT_NAME} ${CORE_SOURCES} ${CORE_HEADERS})

# main.cpp uses C++ ImGui API; force the linker to use C++ rules so symbols
# from third_imgui and app_shell (both C++) are resolved correctly.
set(APP_DIST_DIR "${PROJ_OUT_DIR}/${PRODUCT_NAME}")
set_target_properties(${PRODUCT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${APP_DIST_DIR}"
    OUTPUT_NAME "${FULL_TAG}"
    LINKER_LANGUAGE CXX
)

target_include_directories(${PRODUCT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/app/shell
)

target_link_libraries(${PRODUCT_NAME} PRIVATE
    app_shell
    lib_util
    third_imgui
    third_cjson
    third_libuv
)

# Win32 libraries needed by app_shell / ImGui backends
if (WIN32)
    target_link_libraries(${PRODUCT_NAME} PRIVATE
        d3d11
        dxgi
        d3dcompiler
        dwmapi
        user32
        gdi32
    )
endif()

target_compile_options(${PRODUCT_NAME} PRIVATE -Wall -Wextra)
