set(MODULE_NAME lib_device_manager)
add_library(${MODULE_NAME} STATIC device_manager.c)
target_include_directories(${MODULE_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/midware/usb_ncm
    ${CMAKE_SOURCE_DIR}/midware/serial
    ${CMAKE_SOURCE_DIR}/lib/at_engine
)
target_link_libraries(${MODULE_NAME} PUBLIC
    lib_util
    midware_usb_ncm
    midware_serial
    lib_at_engine
    third_libuv
)
