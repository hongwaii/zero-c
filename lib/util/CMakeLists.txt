# ============================================================
#  lib/util — string + json + i18n helpers (no UI deps)
# ============================================================
set(MODULE_NAME lib_util)

set(MY_SOURCES
    strbuf.c
    errstr.c
    json_reader.c
    json_writer.c
    ringbuf.c
)

add_library(${MODULE_NAME} STATIC ${MY_SOURCES})

target_include_directories(${MODULE_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(${MODULE_NAME} PUBLIC
    third_cjson
)
