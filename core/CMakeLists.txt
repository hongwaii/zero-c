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

# Static-link C++ runtime to avoid DLL dependencies on libc++.dll / libunwind.dll.
# The final EXE link is what actually resolves the import libraries, so the static
# link flags MUST be applied to this target (not just the static-lib app_shell).
# ImGui + DX11 backend are C++ targets, so without this LLVM-MinGW produces an EXE
# that requires libc++.dll and libunwind.dll to be present next to the EXE at runtime.
target_link_options(${PRODUCT_NAME} PRIVATE
    -static
    -static-libgcc
    -static-libstdc++
    -Wl,-Bstatic
    -l:libc++.a
    -l:libunwind.a
    -Wl,-Bdynamic
    # Make the EXE a Windows GUI subsystem app so no console window is opened
    # by default. printf/fprintf to stdout/stderr are silently no-op in this
    # mode unless agent_maybe_open_debug_console() is called at startup.
    -mwindows
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
        kernel32
    )
endif()

target_compile_options(${PRODUCT_NAME} PRIVATE -Wall -Wextra)

# ---- Copy LLVM-MinGW runtime DLLs next to the EXE (safety net) ----
# Even though we static-link libc++/libunwind at the final EXE link step above,
# some transitive dependency (a third-party .lib) might still be dynamic. This
# step guarantees the EXE finds its runtime DLLs on Windows.
add_custom_command(TARGET ${PRODUCT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/tools/llvm-mingw-20260519-ucrt-x86_64/bin/libc++.dll"
        "${CMAKE_SOURCE_DIR}/tools/llvm-mingw-20260519-ucrt-x86_64/bin/libunwind.dll"
        $<TARGET_FILE_DIR:${PRODUCT_NAME}>
    COMMENT "Copying libc++ / libunwind runtime DLLs to $<TARGET_FILE_DIR:${PRODUCT_NAME}>"
)
