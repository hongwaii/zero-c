set(MODULE_NAME midware_serial)
add_library(${MODULE_NAME} STATIC serial_chan.c)
target_include_directories(${MODULE_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib/util
)
target_link_libraries(${MODULE_NAME} PUBLIC
    lib_util
    third_libuv
)
