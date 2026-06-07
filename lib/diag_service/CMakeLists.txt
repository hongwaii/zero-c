set(MODULE_NAME lib_diag_service)
add_library(${MODULE_NAME} STATIC
    diag_state.c
    diag_service.c
)
target_include_directories(${MODULE_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib/util
    ${CMAKE_SOURCE_DIR}/lib/at_engine
    ${CMAKE_SOURCE_DIR}/lib/device_manager
)
target_link_libraries(${MODULE_NAME} PUBLIC
    lib_util
    lib_at_engine
    lib_device_manager
    third_libuv
)
