
######################## mbedtls start ########################
# mbedtls功能裁剪选项（官方CMake支持的参数）
option(ENABLE_PROGRAMS "Build Mbed TLS programs." OFF)                     # mbedtls示例程序
option(ENABLE_TESTING "Build Mbed TLS tests." OFF)                         # TEST
option(GEN_FILES "Generate the auto-generated files as needed" ON)         # 是否生成自动化的源文件

set(USE_STATIC_MBEDTLS_LIBRARY OFF CACHE BOOL "Build Mbed TLS static library." FORCE)
set(USE_SHARED_MBEDTLS_LIBRARY ON CACHE BOOL "Build Mbed TLS shared library." FORCE)
set(LINK_WITH_PTHREAD OFF CACHE BOOL "Explicitly link Mbed TLS library to pthread." FORCE)
set(LINK_WITH_TRUSTED_STORAGE OFF CACHE BOOL "Explicitly link Mbed TLS library to trusted_storage." FORCE)

#添加子目录
add_subdirectory(mbedtls-4.1.0)  #mbedtls-4.1.0

######################## mbedtls   end ########################

######################## libcurl start ########################
#libcurl功能裁剪
set(BUILD_CURL_EXE OFF CACHE BOOL "Don't build curl.exe")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libcurl")
set(CURL_USE_SCHANNEL ON CACHE BOOL "Use Windows TLS")
set(CURL_DISABLE_LDAP ON CACHE BOOL "")
set(CURL_DISABLE_LDAPS ON CACHE BOOL "")
option(CURL_USE_LIBPSL "Use libpsl" OFF)
option(CURL_WITH_HTTP "Build with HTTP support" ON)                         # HTTP
option(CURL_WITH_FTP "Build with FTP support" ON)                           # FTP
option(CURL_WITH_MBEDTLS "Build with SSL support" ON)                       # mbedtls
option(BUILD_TESTING "Build curl test programs" OFF)                        # TEST
# 只编译动态库
set(BUILD_SHARED_LIBS ON CACHE BOOL "Build curl shared" FORCE)
set(BUILD_CURL_EXE OFF CACHE BOOL "Build curl executable" FORCE)
set(BUILD_STATIC_LIBS OFF CACHE BOOL "Build curl static" FORCE)

#添加子目录
add_subdirectory(libcurl)

######################## libcurl   end ########################

######################## imgui start ########################
add_subdirectory(imgui)
######################## imgui   end ########################

######################## cJSON start ########################
set(CJSON_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/cjson/cJSON.c
)
add_library(third_cjson STATIC ${CJSON_SOURCES})
target_include_directories(third_cjson PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/cjson
)
######################## cJSON   end ########################

######################## libuv start ########################
# libuv vendored under third_party/libuv (cloned in Task 1)
# All sources are Windows-only (libuv/src/win/*); guarded by WIN32.
if (WIN32)
set(LIBUV_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/fs-poll.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/idna.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/inet.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/random.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/strscpy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/threadpool.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/timer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/uv-common.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/uv-data-getter-setters.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/version.c
    # Windows-specific sources
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/async.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/core.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/detect-wakeup.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/dl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/error.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/fs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/fs-event.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/getaddrinfo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/getnameinfo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/handle.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/loop-watcher.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/pipe.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/poll.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/process.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/process-stdio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/signal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/snprintf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/stream.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/tcp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/thread.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/tty.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/udp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/util.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/winapi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/winsock.c
)
add_library(third_libuv STATIC ${LIBUV_SOURCES})
target_include_directories(third_libuv PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/include
    ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src
)
target_compile_definitions(third_libuv PRIVATE
    WIN32_LEAN_AND_MEAN
    _WIN32_WINNT=0x0600
)
target_link_libraries(third_libuv PUBLIC
    ws2_32
    psapi
    userenv
    iphlpapi
)
endif()  # WIN32
######################## libuv   end ########################

######################## miniz start ########################
# miniz vendored under third_party/miniz (单文件 zip 库，约 5000 行)
# 用途：diag_log 把 30s 抓的 log 文本压成 zip
set(MINIZ_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/miniz/miniz.c
)
add_library(third_miniz STATIC ${MINIZ_SOURCES})
target_include_directories(third_miniz PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/miniz
)
target_compile_definitions(third_miniz PRIVATE
    # 注意：miniz 头里用的是 MINIZ_NO_ARCHIVE_WRITING_APIS（带 S，复数），
    # MINIZ_NO_STDIO 等。这些宏一旦定义即关闭对应功能；不定义 = 开启。
    # 之前误写成 =0 反而触发了 #ifndef 的 false 分支，导致 zip 写 API 被关。
    # 实际：什么都不定义就是 zip 写 + stdio 全开，正好是我们要的。
)
######################## miniz   end ########################

# ============================================================
#  Example: OpenSSL — choose your approach
# ============================================================
# OpenSSL uses Perl-based config. Approach B (direct sources)
# is complex due to asm files. Consider Approach A with a
# pre-configured build, or use a pre-built binary.
#
# Option: Use pre-built OpenSSL binaries
# add_library(third_ssl STATIC IMPORTED)
# set_target_properties(third_ssl PROPERTIES
#     IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/openssl/lib/libssl.a
# )
# add_library(third_crypto STATIC IMPORTED)
# set_target_properties(third_crypto PROPERTIES
#     IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/openssl/lib/libcrypto.a
# )
# target_include_directories(third_ssl   INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/openssl/include)
# target_include_directories(third_crypto INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/openssl/include)
#
# TODO: Add your third-party integrations here.
