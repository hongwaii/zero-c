# ============================================================
#  lib — Project libraries (static .a / dynamic .dll)
# ============================================================
# This directory holds your own library code.
#
# Each library is a separate target. Choose STATIC (.a) or SHARED (.dll).
#
# HOW TO ADD A STATIC LIBRARY:
#   add_library(my_static_lib STATATIC
#       my_lib.c
#       my_lib.h
#   )
#   target_include_directories(my_static_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
#
# HOW TO ADD A SHARED LIBRARY:
#   add_library(my_shared_lib SHARED
#       my_lib.c
#       my_lib.h
#   )
#   target_include_directories(my_shared_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
#   target_compile_definitions(my_shared_lib PRIVATE MY_LIB_BUILDING_DLL)
#
# HOW TO LINK AN EXTERNAL LIBRARY (placed under lib/):
#   add_library(ext_lib STATIC IMPORTED)
#   set_target_properties(ext_lib PROPERTIES
#       IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/ext_lib/libext.a
#   )
#   target_include_directories(ext_lib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/ext_lib/include)
#
# HOW TO LINK A TOOLCHAIN LIBRARY (from tools/llvm-mingw-*):
#   find_library(WS2_32_LIB ws2_32 PATHS ${PROJ_ROOT_DIR}/tools/llvm-mingw-*/x86_64-w64-mingw32/lib)
#   target_link_libraries(my_target PRIVATE ${WS2_32_LIB})
# ============================================================

# ============================================================
#  Example: Static library
# ============================================================
# Uncomment and customize:
#
# add_library(my_static_lib STATIC
#     my_static_lib.c
#     my_static_lib.h
# )
# target_include_directories(my_static_lib PUBLIC
#     ${CMAKE_CURRENT_SOURCE_DIR}
# )
# set_target_properties(my_static_lib PROPERTIES
#     ARCHIVE_OUTPUT_DIRECTORY "${PROJ_OUT_DIR}/lib"
# )
#
# ============================================================
#  Example: Shared library (.dll)
# ============================================================
# Uncomment and customize:
#
# add_library(my_shared_lib SHARED
#     my_shared_lib.c
#     my_shared_lib.h
# )
# target_include_directories(my_shared_lib PUBLIC
#     ${CMAKE_CURRENT_SOURCE_DIR}
# )
# target_compile_definitions(my_shared_lib PRIVATE
#     MY_SHARED_LIB_BUILDING_DLL
# )
# set_target_properties(my_shared_lib PROPERTIES
#     LIBRARY_OUTPUT_DIRECTORY "${PROJ_OUT_DIR}"
#     RUNTIME_OUTPUT_DIRECTORY "${PROJ_OUT_DIR}"
#     ARCHIVE_OUTPUT_DIRECTORY "${PROJ_OUT_DIR}/lib"
# )
#
# ============================================================
#  Example: External pre-built library placed under lib/
# ============================================================
# Uncomment and customize:
#
# add_library(ext_some_lib STATIC IMPORTED)
# set_target_properties(ext_some_lib PROPERTIES
#     IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/ext_some/lib/libsome.a
# )
# target_include_directories(ext_some_lib INTERFACE
#     ${CMAKE_CURRENT_SOURCE_DIR}/ext_some/include
# )
#
# ============================================================
#  Example: Toolchain system library
# ============================================================
# Uncomment and customize:
#
# file(GLOB MINGW_LIB_DIR "${PROJ_ROOT_DIR}/tools/llvm-mingw-*/x86_64-w64-mingw32/lib")
# find_library(SYSTEM_WS2_32 ws2_32 PATHS ${MINGW_LIB_DIR} NO_DEFAULT_PATH)
# if(SYSTEM_WS2_32)
#     message(STATUS "Found ws2_32: ${SYSTEM_WS2_32}")
# endif()

# ---- Internal C libraries (no UI deps) ----
add_subdirectory(util)
add_subdirectory(device_manager)
