# ============================================================
#  test — Test executable (only compiled when BUILD_TEST=ON)
# ============================================================
# This module compiles into TEST.exe placed at the project root.
# Use it for unit tests, integration tests, or sandbox experiments.
#
# TEMPLATE INSTRUCTIONS:
#   - MY_SOURCES:   .c files to compile
#   - MY_HEADERS:   test-specific headers
#   - MY_INC_DIRS:  extra include directories
#   - MY_DEPS:      libraries to link (project libs, third-party, etc.)
# ============================================================

# ---- Source files ----
set(TEST_SOURCES
    test_main.c
    # TODO: Add test source files, e.g.:
    # test_http.c
    # test_utils.c
)

# ---- Header files ----
set(TEST_HEADERS
    test_main.h
    # TODO: Add test headers
)

# ---- Test executable target ----
add_executable(TEST
    ${TEST_SOURCES}
    ${TEST_HEADERS}
)

# ---- Output to project root as TEST.exe ----
set_target_properties(TEST PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJ_OUT_DIR}"
    OUTPUT_NAME "TEST"
)

# ---- Include directories ----
target_include_directories(TEST PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    # TODO: Add module include paths for testing, e.g.:
    # ${CMAKE_SOURCE_DIR}/midware/http
)

# ---- Link libraries ----
target_link_libraries(TEST PRIVATE
    # TODO: Link the modules you want to test, e.g.:
    # midware_http
    # my_static_lib
)

# ---- Compiler flags ----
target_compile_options(TEST PRIVATE
    -Wall -Wextra -g
)

# ---- Compile definitions ----
target_compile_definitions(TEST PRIVATE
    -DTEST_MODE
    # TODO: Additional test defines
)
