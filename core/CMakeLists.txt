# ============================================================
#  core — Main executable
# ============================================================
# This module contains the main() entry point and application core.
#
# HOW TO ADD FILES:
#   - Add .c files to CORE_SOURCES
#   - Add private headers to CORE_HEADERS (optional, for IDE listing)
#   - Add public headers in include/ (they're already on the include path)
#   - Add extra include dirs to target_include_directories if needed
# ============================================================

# ---- Source files ----
set(CORE_SOURCES
    main.c
    # TODO: Add your source files here, e.g.:
    # app.c
    # config.c
    # utils.c
)

# ---- Header files (optional, for IDE file-tree display) ----
set(CORE_HEADERS
    # TODO: Add private headers here, e.g.:
    # app.h
    # config.h
    # utils.h
)

# ---- Executable target ----
add_executable(${PRODUCT_NAME}
    ${CORE_SOURCES}
    ${CORE_HEADERS}
)

# ---- Output location: project root (so APP-1.1.0.xxx.exe lands there) ----
set_target_properties(${PRODUCT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJ_OUT_DIR}"
    OUTPUT_NAME "${FULL_TAG}"
)

# ---- Include paths (public headers) ----
target_include_directories(${PRODUCT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    # TODO: Add module-specific include paths, e.g.:
    # ${CMAKE_SOURCE_DIR}/midware/http
)

# ---- Link libraries ----
target_link_libraries(${PRODUCT_NAME} PRIVATE
    # Project libs (from lib/)
    # my_static_lib
    # my_shared_lib

    # Middleware modules (from midware/)
    # midware_http

    # Third-party libs (from third_party/)
    # third_libcurl
    # third_libuv
    # third_openssl

    # System libs (from tools/llvm-mingw)
    # ws2_32
    # psapi
)

# ---- Compiler flags (optional) ----
target_compile_options(${PRODUCT_NAME} PRIVATE
    -Wall -Wextra
    # -Werror
)

# ---- Compile definitions (optional) ----
target_compile_definitions(${PRODUCT_NAME} PRIVATE
    # -DAPP_DEBUG
)
