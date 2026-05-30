
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


# add_library(third_libcurl ALIAS libcurl)
# # (The target name depends on what the upstream CMakeLists defines.
# #  Common names: libcurl, CURL::libcurl, or just curl.)
#
# ============================================================
#  Example B: libuv — compile source files directly
# ============================================================
# Uncomment to enable:
#
# set(LIBUV_SOURCES
#     libuv/src/fs-poll.c
#     libuv/src/idna.c
#     libuv/src/inet.c
#     libuv/src/random.c
#     libuv/src/strscpy.c
#     libuv/src/threadpool.c
#     libuv/src/timer.c
#     libuv/src/uv-common.c
#     libuv/src/uv-data-getter-setters.c
#     libuv/src/version.c
#     # Windows-specific sources
#     libuv/src/win/async.c
#     libuv/src/win/core.c
#     libuv/src/win/detect-wakeup.c
#     libuv/src/win/dl.c
#     libuv/src/win/error.c
#     libuv/src/win/fs.c
#     libuv/src/win/fs-event.c
#     libuv/src/win/getaddrinfo.c
#     libuv/src/win/getnameinfo.c
#     libuv/src/win/handle.c
#     libuv/src/win/loop-watcher.c
#     libuv/src/win/pipe.c
#     libuv/src/win/poll.c
#     libuv/src/win/process.c
#     libuv/src/win/process-stdio.c
#     libuv/src/win/signal.c
#     libuv/src/win/snprintf.c
#     libuv/src/win/stream.c
#     libuv/src/win/tcp.c
#     libuv/src/win/thread.c
#     libuv/src/win/tty.c
#     libuv/src/win/udp.c
#     libuv/src/win/util.c
#     libuv/src/win/winapi.c
#     libuv/src/win/winsock.c
# )
# add_library(third_libuv STATIC ${LIBUV_SOURCES})
# target_include_directories(third_libuv PUBLIC
#     ${CMAKE_CURRENT_SOURCE_DIR}/libuv/include
#     ${CMAKE_CURRENT_SOURCE_DIR}/libuv/src
# )
# target_compile_definitions(third_libuv PRIVATE
#     -DWIN32_LEAN_AND_MEAN
#     -D_WIN32_WINNT=0x0600
# )
# target_link_libraries(third_libuv PUBLIC
#     ws2_32
#     psapi
#     userenv
#     iphlpapi
# )
#
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
