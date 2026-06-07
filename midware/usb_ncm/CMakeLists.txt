set(MODULE_NAME midware_usb_ncm)
add_library(${MODULE_NAME} STATIC ncm_chan.c)
target_include_directories(${MODULE_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)
# Windows IP Helper + WinSock
target_link_libraries(${MODULE_NAME} PUBLIC iphlpapi ws2_32)
