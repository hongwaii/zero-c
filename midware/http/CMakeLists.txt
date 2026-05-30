# ============================================================
#  midware/http — HTTP client/server middleware module
# ============================================================
# This module is self-contained. It exposes a public header
# and may depend on lib/ or third_party/ libraries.
#
# TEMPLATE INSTRUCTIONS:
#   - MY_SOURCES:   .c files to compile
#   - MY_HEADERS:   public .h files (exposed to dependents via INTERFACE)
#   - MY_PRIV_INC:  private include dirs (not exposed)
#   - MY_DEPS:      libraries this module links to
# ============================================================

# ---- Module name (used as CMake target) ----
set(MODULE_NAME midware_http)

# ---- Source files ----
set(MY_SOURCES
    http.c
    # TODO: Add source files
    # http_client.c
    # http_server.c
    # http_parser.c
)

# ---- Public headers (exposed to consumers) ----
set(MY_HEADERS
    http.h
    # TODO: Add public headers
)

# ---- Library target (static by default; change to SHARED for .dll) ----
add_library(${MODULE_NAME} STATIC
    ${MY_SOURCES}
    ${MY_HEADERS}
)

# ---- Expose public headers to dependents ----
target_include_directories(${MODULE_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

# ---- Private include directories ----
target_include_directories(${MODULE_NAME} PRIVATE
    # TODO: Add private/internal include paths, e.g.:
    # ${CMAKE_CURRENT_SOURCE_DIR}/internal
)

# ---- Dependencies (other project libs / third-party libs) ----
target_link_libraries(${MODULE_NAME} PUBLIC
    # TODO: Link to libraries this module needs, e.g.:
    # my_static_lib
    # third_libcurl
    # ws2_32
)

# ---- Output directory (default: build tree; adjust if needed) ----
# set_target_properties(${MODULE_NAME} PROPERTIES
#     ARCHIVE_OUTPUT_DIRECTORY "${PROJ_OUT_DIR}/out/lib"
# )

# ---- Compile definitions ----
target_compile_definitions(${MODULE_NAME} PRIVATE
    # TODO: Add module-specific defines, e.g.:
    # -DHTTP_USE_TLS
)
