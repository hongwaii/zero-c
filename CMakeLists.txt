# ============================================================
#  Windows-C Build Framework — Root CMakeLists.txt
# ============================================================
cmake_minimum_required(VERSION 3.15)
project(${PRODUCT_NAME} C CXX)

# ---- C standard ----
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ---- Global paths (set by build.bat via -D) ----
if(NOT DEFINED PROJ_ROOT_DIR)
    set(PROJ_ROOT_DIR ${CMAKE_SOURCE_DIR})
endif()
if(NOT DEFINED PROJ_OUT_DIR)
    set(PROJ_OUT_DIR ${CMAKE_SOURCE_DIR})
endif()

# ---- Default build-time values (overridden by build.bat -D flags) ----
if(NOT DEFINED PRODUCT_NAME)
    set(PRODUCT_NAME "MyApp")
endif()
if(NOT DEFINED PRODUCT_VERSION)
    set(PRODUCT_VERSION "0.0.0")
endif()
if(NOT DEFINED VERSION_MAJOR)
    set(VERSION_MAJOR 0)
endif()
if(NOT DEFINED VERSION_MINOR)
    set(VERSION_MINOR 0)
endif()
if(NOT DEFINED VERSION_PATCH)
    set(VERSION_PATCH 0)
endif()
if(NOT DEFINED BUILD_TIME)
    set(BUILD_TIME "0000000000")
endif()
if(NOT DEFINED BUILD_TYPE)
    set(BUILD_TYPE "alpha")
endif()
if(NOT DEFINED FULL_TAG)
    set(FULL_TAG "${PRODUCT_NAME}-${PRODUCT_VERSION}.${BUILD_TIME}_${BUILD_TYPE}")
endif()
if(NOT DEFINED GIT_HASH)
    set(GIT_HASH "00000000")
endif()

# ---- Generate version.h from template ----
configure_file(
    ${CMAKE_SOURCE_DIR}/version.h.in
    ${CMAKE_SOURCE_DIR}/include/version.h
    @ONLY
)

# ---- Global include directories ----
include_directories(${CMAKE_SOURCE_DIR}/include)

# ---- Add subdirectories (order matters: libs before dependents) ----

# 1) Third-party libraries (built first — other modules may depend on them)
add_subdirectory(third_party)

# 2) Project libraries (static / dynamic)
add_subdirectory(lib)

# 3) Middleware modules (may depend on lib + third_party)
add_subdirectory(midware)

# 4) App modules (UI panels, may depend on lib + midware + third_party)
add_subdirectory(app)

# 5) Core executable (depends on everything above)
add_subdirectory(core)

# 5) Test executable (only when BUILD_TEST=ON)
if(BUILD_TEST)
    add_subdirectory(test)
endif()

# ---- Print build configuration summary ----
message(STATUS "")
message(STATUS "============================================")
message(STATUS "  Build Configuration")
message(STATUS "============================================")
message(STATUS "  Product      : ${FULL_TAG}")
message(STATUS "  Version      : ${PRODUCT_VERSION}")
message(STATUS "  Build time   : ${BUILD_TIME}")
message(STATUS "  Build type   : ${BUILD_TYPE}")
message(STATUS "  Git hash     : ${GIT_HASH}")
message(STATUS "  Build test   : ${BUILD_TEST}")
message(STATUS "  Output dir   : ${PROJ_OUT_DIR}")
message(STATUS "============================================")
message(STATUS "")
