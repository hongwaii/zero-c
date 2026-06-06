# ============================================================
#  third_party/imgui — Dear ImGui (docking) + DX11 + Win32
# ============================================================
set(MODULE_NAME third_imgui)

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
)

add_library(${MODULE_NAME} STATIC ${IMGUI_SOURCES})

target_include_directories(${MODULE_NAME} PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_compile_definitions(${MODULE_NAME} PUBLIC
    IMGUI_DEFINE_MATH_OPERATORS
)

# Windows libs required by backends
if (WIN32)
    target_link_libraries(${MODULE_NAME} PUBLIC
        d3d11
        dxgi
        d3dcompiler
        user32
        gdi32
    )
endif()

# Match the rest of the project's strictness
target_compile_options(${MODULE_NAME} PRIVATE
    -Wno-format
    -Wno-missing-field-initializers
    -Wno-unused-parameter
    -Wno-sign-compare
    -Wno-extra
)
